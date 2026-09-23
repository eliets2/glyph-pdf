// SPDX-License-Identifier: Apache-2.0
//
// SEP13:1 — CID /W out-of-bounds heap write + DoS (PARITY-GLM-REVIEW
// 2026-09-13 finding #1, confirmed at the cb92aa0 baseline).
//
// exportPdfA's ensureCompleteCidSets() derived each embedded composite font's
// /CIDSet from its own /W array with NO bounds validation: /W numbers come
// straight from the (possibly hostile) document as unvalidated int64_t.
//   * /W [-5 [500 500 500]] collected cids {-5,-4,-3}, sized the bitmap from
//     maxCid only, then evaluated bits[static_cast<size_t>(-5)/8] — a wild
//     OOB heap write (address ~2^61 bytes past a 1-byte buffer: non-canonical
//     on x86-64, a guaranteed fault) with an additional UB negative shift.
//   * /W [0 4000000000 500] inserted ~4e9 set entries and allocated a giant
//     bitmap — OOM/hang (DoS).
// Both are reachable through Export → PDF/A on any opened crafted PDF.
//
// The fix clamps collection AND indexing to the ISO 32000-1 CID domain
// (0..65535): out-of-domain entries are dropped with an honest qWarning, and
// the range-form loop bounds are clamped — which IS the span cap, so a
// hostile declared span can neither loop nor allocate beyond the 65536-bit
// domain.
//
// Fixtures are hand-built raw PDFs whose Type0 descendant font carries a
// hostile /W literal; the font is UNUSED by the page content, mirroring how
// the object walk finds it. Every test drives the REAL engine export and
// verifies the CIDSet actually written to the SAVED artifact.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include <podofo/podofo.h>

#include "engines/PdfEditorEngine.h"

namespace {

// Hand-built PDF with a Type0 font whose descendant /W is attacker-controlled
// (`wArray` is spliced into the descendant dictionary verbatim). Xref offsets
// are computed programmatically, so the fixture stays byte-exact without
// hand-maintained offsets.
QString createType0WPdf(const QString& dir, const QString& name,
                        const QByteArray& wArray)
{
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};

    const QByteArray content =
        "BT /F1 12 Tf 72 720 Td (SEP13 fixture page) Tj ET\n";
    const QByteArray objs[] = {
        // 1: catalog
        "<< /Type /Catalog /Pages 2 0 R >>",
        // 2: pages
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        // 3: page (references the Type0 font through /Resources — a real
        // document reaches its fonts this way, and PoDoFo's writer only
        // persists REACHABLE objects)
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        "/Resources << /Font << /F1 4 0 R >> >> /Contents 7 0 R >>",
        // 4: Type0 font
        "<< /Type /Font /Subtype /Type0 /BaseFont /SEP13+Test "
        "/Encoding /Identity-H /DescendantFonts [5 0 R] >>",
        // 5: descendant CIDFontType2 with the hostile /W
        "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /SEP13+Test "
        "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) "
        "/Supplement 0 >> /FontDescriptor 6 0 R /DW 500 /W [" +
            wArray + "] >>",
        // 6: FontDescriptor — NO /CIDSet: the exporter must derive one
        "<< /Type /FontDescriptor /FontName /SEP13+Test /Flags 4 "
        "/FontBBox [0 0 1000 1000] /ItalicAngle 0 /Ascent 800 "
        "/Descent -200 /CapHeight 700 /StemV 80 >>",
        // 7: page content
        "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" +
            content + "endstream",
    };

    QByteArray out = "%PDF-1.7\n";
    QList<int> offsets;
    for (int i = 0; i < 7; ++i) {
        offsets.append(out.size());
        out += QByteArray::number(i + 1) + " 0 obj\n" + objs[i] + "\nendobj\n";
    }
    const int xrefPos = out.size();
    out += "xref\n0 8\n0000000000 65535 f \n";
    for (int off : offsets)
        out += QString("%1 00000 n \n").arg(off, 10, 10, QChar('0')).toLatin1();
    out += "trailer\n<< /Size 8 /Root 1 0 R >>\nstartxref\n" +
           QByteArray::number(xrefPos) + "\n%%EOF\n";
    f.write(out);
    f.close();
    return path;
}

// CIDSet readback from a SAVED artifact: walk the object tree exactly like
// the exporter does (Type0 → DescendantFonts[0] → FontDescriptor → CIDSet)
// and return the stream bytes. foundFont/foundCidSet report what was seen so
// tests can distinguish "no CIDSet written" from "assert failed".
struct CidSetReadback {
    bool foundFont = false;
    bool foundCidSet = false;
    QByteArray bytes;
};

CidSetReadback readBackCidSet(const QString& pdfPath)
{
    CidSetReadback r;
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdfPath.toUtf8().constData());
    for (auto* objPtr : doc.GetObjects()) {
        if (!objPtr || !objPtr->IsDictionary()) continue;
        const auto* subtype =
            objPtr->GetDictionary().FindKey(PoDoFo::PdfName("Subtype"));
        if (!subtype || !subtype->IsName() ||
            subtype->GetName() != PoDoFo::PdfName("Type0"))
            continue;
        const auto* descendants = objPtr->GetDictionary().FindKey(
            PoDoFo::PdfName("DescendantFonts"));
        if (!descendants || !descendants->IsArray()) continue;
        for (const auto& dref : descendants->GetArray()) {
            PoDoFo::PdfObject* desc = nullptr;
            if (dref.IsReference())
                desc = &doc.GetObjects().MustGetObject(dref.GetReference());
            else if (dref.IsDictionary())
                desc = const_cast<PoDoFo::PdfObject*>(&dref);
            if (!desc || !desc->IsDictionary()) continue;
            const auto* fd = desc->GetDictionary().FindKey(
                PoDoFo::PdfName("FontDescriptor"));
            if (!fd || !fd->IsDictionary()) continue;
            r.foundFont = true;
            const auto* cidSet = fd->GetDictionary().FindKey(
                PoDoFo::PdfName("CIDSet"));
            if (!cidSet) continue;   // honestly dropped / not derived
            PoDoFo::PdfObject* cidSetObj = nullptr;
            if (cidSet->IsReference())
                cidSetObj = &doc.GetObjects().MustGetObject(cidSet->GetReference());
            else if (cidSet->IsDictionary())
                cidSetObj = const_cast<PoDoFo::PdfObject*>(cidSet);
            if (!cidSetObj || !cidSetObj->HasStream()) continue;
            r.foundCidSet = true;
            const PoDoFo::charbuff bits = cidSetObj->MustGetStream().GetCopy();
            r.bytes = QByteArray(bits.data(), static_cast<int>(bits.size()));
            return r;   // one Type0 font per fixture
        }
    }
    return r;
}

} // namespace

class TestPdfACidSetSafety : public QObject {
    Q_OBJECT

    // Export a crafted fixture through the REAL engine and read the CIDSet
    // back from the saved artifact. (Void: QVERIFY/QCOMPARE macros `return;`
    // and must not appear in a value-returning function.)
    void exportAndReadBack(const QString& dir, const QString& name,
                           const QByteArray& wArray,
                           CidSetReadback& readback, bool& exportOk)
    {
        exportOk = false;
        readback = CidSetReadback{};
        const QString src = createType0WPdf(dir, name, wArray);
        QVERIFY2(QFile::exists(src) && QFileInfo(src).size() > 200,
                 "crafted Type0 /W fixture must be written");
        PdfEditorEngine editor;
        QVERIFY2(editor.loadDocumentForEditing(src),
                 "the crafted fixture must load for editing");
        const QString out = dir + "/" + name + "_pdfa.pdf";
        exportOk = editor.exportPdfA(out, 1);   // PDF/A-1b
        if (exportOk) readback = readBackCidSet(out);
    }

private slots:

    // The finding's first repro: a NEGATIVE CID indexed
    // bits[static_cast<size_t>(cid)/8] — on the pre-fix code a wild OOB heap
    // write at a non-canonical address (~2^61 past the buffer; guaranteed
    // fault on x86-64), plus a UB negative shift. Post-fix the out-of-domain
    // entries are dropped, the in-domain ones are mapped exactly, and the
    // export succeeds with an honest CIDSet.
    void negativeCidIsDroppedAndExportSurvives() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // cids {-5,-4,-3} (out of domain) ∪ {0,1,2} (in domain, from
        // "0 [255 128 100]"). Expected bitmap: 1 byte, bits 0..2 set
        // (0x80|0x40|0x20 = 0xE0).
        bool ok = false;
        CidSetReadback r;
        exportAndReadBack(tmp.path(), QStringLiteral("neg_cid.pdf"),
                          QByteArrayLiteral("-5 [500 500 500] 0 [255 128 100]"),
                          r, ok);
        QVERIFY2(ok, "exportPdfA must succeed on the negative-CID fixture");
        QVERIFY2(r.foundFont, "the exported artifact must retain the Type0 font");
        QVERIFY2(r.foundCidSet, "an in-domain CID population must produce a CIDSet");
        QCOMPARE(r.bytes.size(), 1);
        QCOMPARE(static_cast<unsigned char>(r.bytes.at(0)), 0xE0);
    }

    // The finding's second repro shape (over-cap range): the declared span
    // (0..2000000) exceeds the CID domain 150×. The clamp caps the bitmap at
    // 65536 bits = 8192 bytes — the mutation negative control (clamp
    // disabled) produces a 250001-byte CIDSet and fails this pin
    // deterministically, without relying on OOM timing.
    void overCapRangeIsBoundedToCidDomain() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        bool ok = false;
        CidSetReadback r;
        exportAndReadBack(tmp.path(), QStringLiteral("over_cap.pdf"),
                          QByteArrayLiteral("0 2000000 500"), r, ok);
        QVERIFY2(ok, "exportPdfA must succeed on the over-cap-range fixture");
        QVERIFY2(r.foundFont && r.foundCidSet,
                 "the clamped range must still produce a CIDSet");
        QCOMPARE(r.bytes.size(), 8192);          // (65535/8)+1 — the domain cap
        for (unsigned char b : r.bytes)
            QCOMPARE(int(b), 0xFF);              // every domain CID set
    }

    // The finding's stated magnitude ("0 4000000000 500"; 100M here keeps the
    // hostile span far beyond any legitimate font while the bounded loop
    // stays instant on the fixed code). The span cap is absolute — the loop
    // bounds are clamped to 0..65535 regardless of the declared numbers — so
    // this exercises the identical clamp as overCapRange at DoS scale.
    void hugeRangeCompletesBoundedWithoutOom() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        bool ok = false;
        CidSetReadback r;
        exportAndReadBack(tmp.path(), QStringLiteral("huge_range.pdf"),
                          QByteArrayLiteral("0 100000000 500"), r, ok);
        QVERIFY2(ok, "exportPdfA must succeed on the huge-range fixture");
        QVERIFY2(r.foundFont && r.foundCidSet,
                 "the clamped range must still produce a CIDSet");
        QCOMPARE(r.bytes.size(), 8192);
        for (unsigned char b : r.bytes)
            QCOMPARE(int(b), 0xFF);
    }

    // Honesty: when EVERY /W entry is out of domain, no CIDSet is written
    // (nothing is claimed from nothing) and the export still succeeds — the
    // same skip the exporter applies to an empty /W, now also covering the
    // fully-rejected hostile case.
    void allNegativeWLeavesNoCidSetButExports() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        bool ok = false;
        CidSetReadback r;
        exportAndReadBack(tmp.path(), QStringLiteral("all_neg.pdf"),
                          QByteArrayLiteral("-5 [500 500 500]"), r, ok);
        QVERIFY2(ok, "exportPdfA must succeed even when every CID is out of domain");
        QVERIFY2(r.foundFont, "the exported artifact must retain the Type0 font");
        QVERIFY2(!r.foundCidSet,
                 "an all-out-of-domain /W must not produce a CIDSet "
                 "(honest skip, never a fabricated bitmap)");
    }
};

#include "TestPdfACidSetSafety.moc"
QTEST_MAIN(TestPdfACidSetSafety)
