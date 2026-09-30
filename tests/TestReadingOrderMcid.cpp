// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 #3: MCID-level reading-order extraction.
//
// The reading-order checker used to walk ONLY the struct-element level of the
// structure tree: a bare MCID integer under /K (the marked-content reference
// that actually carries the text) was skipped (PdfAValidationPanel.cpp
// "!node->IsDictionary() — skip"). A document whose PARAGRAPHS are correctly
// ordered at the element level but whose MARKED-CONTENT spans paint their
// lines interleaved was therefore reported clean — the text-level inversion
// was invisible to the check.
//
// These pins drive gp::analyzeReadingOrder() with PoDoFo-built tagged fixtures
// whose page content stream carries /P <</MCID n>> BDC … EMC spans (standard
// Helvetica, absolute Tm placement). After the fix the analysis extends INTO
// marked content: each struct-referenced MCID gets its text position from the
// content stream, and text-level inversions are reported with page+MCID
// provenance.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "modes/PdfAValidationPanel.h"

#include <cstring>

namespace {

using PoDoFo::PdfDictionary;
using PoDoFo::PdfName;
using PoDoFo::PdfObject;

// One interleaved-paragraphs page: 8 marked-content spans drawn LINE-INTERLEAVED
// (P1 line 1, P2 line 1, P1 line 2, P2 line 2, …) with absolute Tm placement at
// descending y. The struct tree groups them into two paragraphs:
//   P1 = MCIDs [0,2,4,6]   P2 = MCIDs [1,3,5,7]
// so the structural MCID sequence (0,2,4,6,1,3,5,7) diverges from the visual
// top-down order (0,1,2,…,7) by up to 3 slots — above kReadingOrderSlotTolerance.
// The paragraph struct elements deliberately carry NO /A /BBox: the element
// level of the checker must stay clean here (it is the marked-content level
// that must catch the inversion).
bool makeInterleavedTaggedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        // Font resource /F1 = Helvetica so the Tj strings are plain ASCII.
        PdfDictionary fonts;
        auto& f1 = doc.GetObjects().CreateDictionaryObject();
        f1.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
        f1.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Type1")));
        f1.GetDictionary().AddKey("BaseFont", PdfObject(PdfName("Helvetica")));
        fonts.AddKey(PdfName("F1"), PdfObject(f1.GetIndirectReference()));
        page.GetResources().GetDictionary().AddKey("Font", PdfObject(fonts));

        const char* p1[4] = { "P1 line 1", "P1 line 2", "P1 line 3", "P1 line 4" };
        const char* p2[4] = { "P2 line 1", "P2 line 2", "P2 line 3", "P2 line 4" };
        // Draw order: P1[i], P2[i] alternating; y descends 700,685,670,…
        QByteArray content = "BT\n/F1 10 Tf\n";
        int y = 700;
        for (int i = 0; i < 4; ++i) {
            content += "/P <</MCID " + QByteArray::number(2 * i) +
                       ">> BDC\n1 0 0 1 60 " + QByteArray::number(y) +
                       " Tm\n(" + p1[i] + ") Tj\nEMC\n";
            content += "/P <</MCID " + QByteArray::number(2 * i + 1) +
                       ">> BDC\n1 0 0 1 60 " + QByteArray::number(y - 7) +
                       " Tm\n(" + p2[i] + ") Tj\nEMC\n";
            y -= 14;
        }
        content += "ET\n";
        auto& contents = page.GetOrCreateContents();
        auto& stream = contents.CreateStreamForAppending(
            PoDoFo::PdfStreamAppendFlags::None);
        stream.SetData(PoDoFo::bufferview(content.constData(),
                                          static_cast<size_t>(content.size())));

        // Structure tree: two paragraph elements whose /K arrays reference the
        // MCIDs (bare integers — the exact shape the checker used to skip).
        auto& cat = doc.GetCatalog().GetDictionary();
        PdfDictionary markInfo;
        markInfo.AddKey("Marked", PdfObject(true));
        cat.AddKey("MarkInfo", PdfObject(markInfo));

        auto& root = doc.GetObjects().CreateDictionaryObject();
        root.GetDictionary().AddKey("Type", PdfObject(PdfName("StructTreeRoot")));
        root.GetDictionary().AddKey("Pg",
            PdfObject(page.GetObject().GetIndirectReference()));
        cat.AddKey("StructTreeRoot", root.GetIndirectReference());

        auto makeParagraph = [&](const char* const* lines) {
            auto& el = doc.GetObjects().CreateDictionaryObject();
            el.GetDictionary().AddKey("Type", PdfObject(PdfName("StructElem")));
            el.GetDictionary().AddKey("S", PdfObject(PdfName("P")));
            el.GetDictionary().AddKey("Pg",
                PdfObject(page.GetObject().GetIndirectReference()));
            PoDoFo::PdfArray mcids;
            for (int i = 0; i < 4; ++i)
                mcids.Add(PdfObject(static_cast<int64_t>(lines == p1
                    ? 2 * i            // P1: MCIDs 0,2,4,6
                    : 2 * i + 1)));    // P2: MCIDs 1,3,5,7
            el.GetDictionary().AddKey("K", PdfObject(mcids));
            return el.GetIndirectReference();
        };
        PoDoFo::PdfArray kids;
        kids.Add(makeParagraph(p1));
        kids.Add(makeParagraph(p2));
        root.GetDictionary().AddKey("K", PdfObject(kids));

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "makeInterleavedTaggedPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

// The SAME two paragraphs, but each drawn contiguously top-down and grouped
// correspondingly (P1 = MCIDs [0,1,2,3] at y 700..658, P2 = [4,5,6,7] at
// y 644..602). Structural order == content-stream order == visual order: the
// no-false-positive control (the July audit's walk-up-fix class).
bool makeOrderedTaggedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        PdfDictionary fonts;
        auto& f1 = doc.GetObjects().CreateDictionaryObject();
        f1.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
        f1.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Type1")));
        f1.GetDictionary().AddKey("BaseFont", PdfObject(PdfName("Helvetica")));
        fonts.AddKey(PdfName("F1"), PdfObject(f1.GetIndirectReference()));
        page.GetResources().GetDictionary().AddKey("Font", PdfObject(fonts));

        const char* p1[4] = { "P1 line 1", "P1 line 2", "P1 line 3", "P1 line 4" };
        const char* p2[4] = { "P2 line 1", "P2 line 2", "P2 line 3", "P2 line 4" };
        QByteArray content = "BT\n/F1 10 Tf\n";
        int y = 700;
        int mcid = 0;
        for (int i = 0; i < 4; ++i, ++mcid) {
            content += "/P <</MCID " + QByteArray::number(mcid) +
                       ">> BDC\n1 0 0 1 60 " + QByteArray::number(y) +
                       " Tm\n(" + p1[i] + ") Tj\nEMC\n";
            y -= 14;
        }
        for (int i = 0; i < 4; ++i, ++mcid) {
            content += "/P <</MCID " + QByteArray::number(mcid) +
                       ">> BDC\n1 0 0 1 60 " + QByteArray::number(y) +
                       " Tm\n(" + p2[i] + ") Tj\nEMC\n";
            y -= 14;
        }
        content += "ET\n";
        auto& contents = page.GetOrCreateContents();
        auto& stream = contents.CreateStreamForAppending(
            PoDoFo::PdfStreamAppendFlags::None);
        stream.SetData(PoDoFo::bufferview(content.constData(),
                                          static_cast<size_t>(content.size())));

        auto& cat = doc.GetCatalog().GetDictionary();
        PdfDictionary markInfo;
        markInfo.AddKey("Marked", PdfObject(true));
        cat.AddKey("MarkInfo", PdfObject(markInfo));

        auto& root = doc.GetObjects().CreateDictionaryObject();
        root.GetDictionary().AddKey("Type", PdfObject(PdfName("StructTreeRoot")));
        root.GetDictionary().AddKey("Pg",
            PdfObject(page.GetObject().GetIndirectReference()));
        cat.AddKey("StructTreeRoot", root.GetIndirectReference());

        auto makeParagraph = [&](int firstMcid) {
            auto& el = doc.GetObjects().CreateDictionaryObject();
            el.GetDictionary().AddKey("Type", PdfObject(PdfName("StructElem")));
            el.GetDictionary().AddKey("S", PdfObject(PdfName("P")));
            el.GetDictionary().AddKey("Pg",
                PdfObject(page.GetObject().GetIndirectReference()));
            PoDoFo::PdfArray mcids;
            for (int i = 0; i < 4; ++i)
                mcids.Add(PdfObject(static_cast<int64_t>(firstMcid + i)));
            el.GetDictionary().AddKey("K", PdfObject(mcids));
            return el.GetIndirectReference();
        };
        PoDoFo::PdfArray kids;
        kids.Add(makeParagraph(0));
        kids.Add(makeParagraph(4));
        root.GetDictionary().AddKey("K", PdfObject(kids));

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "makeOrderedTaggedPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace

class TestReadingOrderMcid : public QObject {
    Q_OBJECT
private slots:
    // (a)+(b) RED→GREEN: a structurally well-ordered document whose
    // marked-content lines are interleaved must NO LONGER report clean; the
    // report must name the inverted spans with page+MCID provenance.
    void interleavedMarkedContentIsFlaggedWithMcidProvenance();
    // (c) no-false-positive control: contiguous paragraphs in stream order,
    // struct order and visual order stay clean.
    void contiguousMarkedContentStaysClean();
};

void TestReadingOrderMcid::interleavedMarkedContentIsFlaggedWithMcidProvenance() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("interleaved.pdf");
    QVERIFY2(makeInterleavedTaggedPdf(pdf), "fixture build failed");

    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QVERIFY2(r.tagged, "fixture must parse as a tagged PDF");
    QCOMPARE(r.elementCount, 2);   // the two paragraph elements

    // The element level is clean by construction (no /BBox): any reported
    // issue must come from the marked-content level.
    const QString joined = r.issues.join(QStringLiteral("; "));
    QVERIFY2(!r.issues.isEmpty(),
             "interleaved marked-content order must be reported; got a clean "
             "report — text-level order problems are still invisible");
    QVERIFY2(r.issues.size() == 2,
             qPrintable(QStringLiteral("expected exactly the 2 inverted spans "
                                      "(MCID 6, MCID 1), got %1: %2")
                           .arg(r.issues.size()).arg(joined)));
    // MCID provenance: MCID 6 ("P1 line 4") is drawn at structure position 4
    // but paints at visual position 7; MCID 1 ("P2 line 1") at structure
    // position 5 / visual position 2.
    QVERIFY2(joined.contains(QStringLiteral("MCID 6")),
             qPrintable(QStringLiteral("issue must name MCID 6: %1").arg(joined)));
    QVERIFY2(joined.contains(QStringLiteral("MCID 1")),
             qPrintable(QStringLiteral("issue must name MCID 1: %1").arg(joined)));
    // The extracted span text is part of the provenance.
    QVERIFY2(joined.contains(QStringLiteral("P1 line 4")),
             qPrintable(QStringLiteral("issue must quote the span text: %1").arg(joined)));
    QVERIFY2(joined.contains(QStringLiteral("P2 line 1")),
             qPrintable(QStringLiteral("issue must quote the span text: %1").arg(joined)));
    // Page provenance (1-based in the message, 0-based in issuePages).
    QVERIFY2(joined.contains(QStringLiteral("page 1")),
             qPrintable(QStringLiteral("issue must name the page: %1").arg(joined)));
    QCOMPARE(r.issuePages, (QList<int>{ 0, 0 }));
}

void TestReadingOrderMcid::contiguousMarkedContentStaysClean() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("ordered.pdf");
    QVERIFY2(makeOrderedTaggedPdf(pdf), "fixture build failed");

    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QVERIFY2(r.tagged, "fixture must parse as a tagged PDF");
    QVERIFY2(r.issues.isEmpty(),
             qPrintable(QStringLiteral(
                 "correctly-ordered marked content must stay clean; got: %1")
                 .arg(r.issues.join(QStringLiteral("; ")))));
    QCOMPARE(r.issuePages, (QList<int>{}));
}

QTEST_MAIN(TestReadingOrderMcid)
#include "TestReadingOrderMcid.moc"
