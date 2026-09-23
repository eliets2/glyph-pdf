// SPDX-License-Identifier: Apache-2.0
// T1-2 — Redaction Proof Mode: machine-verifiable redaction evidence.
//
// Every test in this file judges SAVED ARTIFACTS (committed files + byte
// digests), never in-memory state. The pins:
//
//   * After a real RedactOperation run with produceProof, the proof PASSES on
//     a correctly excised document, the manifest carries per-excision location
//     / method / before→after page-stream digests / operator+run counts, and
//     the pack files exist on disk (JSON + readable TXT with the no-overclaim
//     disclaimer).
//   * Removed-text survivors ANYWHERE fail the proof loudly and locate the
//     survivor: the same string on an unmarked line (the classic missed spot),
//     planted in the info dictionary, planted in XMP, planted in an annotation
//     string, left inside an embedded-file attachment, and a second %%EOF
//     (incremental-update remnant) making the raw sweep unable to rule out
//     superseded bytes.
//   * A surface that cannot be swept fails the proof (unreadable output).
//   * Honest no-op recording: a mark over no text is recorded as
//     verified-no-text-in-region, never claimed as a removal.
//   * The UI seam carries the toggle: dialog default ON, plan() and the shared
//     plan→request conversion transport it, presenter wording is honest in
//     both directions, and produceProof=false keeps the legacy behavior
//     (TestRedactTransaction's 38 pins stay untouched).
//
// Mutation negative control (evidence-2026-09-08/t2-revert-verify-BROKEN-SWEEP-
// PREFIX.txt, 6 failed / 14 passed): with the sweep's string detection disabled
// end to end — the shared byte matcher containsAny forced false (raw bytes,
// object strings, decoded streams, XMP, embedded files) plus the extraction
// and info-dictionary matches — the anchors below FAIL: missed spot, info
// dictionary, XMP, annotation, attachment, extra needle. A blind sweep cannot
// bless a leaky file. Recorded honestly: breaking ONLY the decoded-stream +
// object-string surfaces changes no verdict
// (t2-weak-mutation-objstr-decstr-NOT-ANCHORED.txt, 20/20 pass) — every planted
// fixture is also visible to an independent surface (raw bytes for uncompressed
// objects, the dedicated metadata/attachment/revision surfaces), so those two
// layers are defense-in-depth rather than sole evidence; PoDoFo-written files
// carry no object streams that could isolate them.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <podofo/podofo.h>

#include "core/RedactionProof.h"
#include "engines/RedactOperation.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "modes/RedactApplyDialog.h"

#ifdef DrawText
#undef DrawText
#endif
#ifdef GetObject
#undef GetObject
#endif

using namespace gp;
using namespace gp::RedactionProof;

namespace {

constexpr double kA4Height = 842.0;

// Secret line lives at PDF y=700 on the page; the mark rect is the same line
// in viewer (top-down) coordinates.
QRectF secretMark()
{
    return QRectF(90.0, kA4Height - 700.0 - 18.0, 280.0, 24.0);
}

QByteArray fileBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// Two-page source document with known strings exactly where the tests need
// them. Everything outside the marked line is benign and must SURVIVE.
//   page 1: "TopSecretAlpha bare secrets"  (y=700 — the marked line)
//           "KeepThisVisible public info"  (y=650)
//   page 2: "SecondSecretBravo hidden note" (y=700)
//           "KeepPageTwo public record"     (y=650)
//   extraLine1: optional extra page-1 line (the missed-spot fixture).
QString makeSourcePdf(const QString& path, const char* extraLine1 = nullptr,
                      double extraY1 = 620.0)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        doc.GetMetadata().SetTitle(PoDoFo::PdfString("QuarterlyReport"));

        auto drawLine = [&doc, &font](PoDoFo::PdfPage& page,
                                      const char* text, double y) {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)(text, 100.0, y);
            painter.FinishDrawing();
        };
        auto& page1 = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        drawLine(page1, "TopSecretAlpha bare secrets", 700.0);
        drawLine(page1, "KeepThisVisible public info", 650.0);
        if (extraLine1)
            drawLine(page1, extraLine1, extraY1);

        auto& page2 = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        drawLine(page2, "SecondSecretBravo hidden note", 700.0);
        drawLine(page2, "KeepPageTwo public record", 650.0);

        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "makeSourcePdf failed:" << e.what();
        return QString();
    }
    return path;
}

// W2B-1 (sweep-w2b-verify 2026-09-20): a rotation-blind companion to
// makeSourcePdf — Letter pages, page 1 plain, page 2 /Rotate 270 — with the
// same secret/public line pair at USER y=700/650 on both (/Rotate never
// enters the content stream, so both pages carry identical user-space text;
// only the view transform differs).
QString makeRotatedSecretPdf(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);

        auto drawLine = [&font](PoDoFo::PdfPage& page,
                                const char* text, double y) {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)(text, 100.0, y);
            painter.FinishDrawing();
        };
        auto& page1 = doc.GetPages().CreatePage(PoDoFo::Rect(0, 0, 612, 792));
        page1.SetRotation(0);
        drawLine(page1, "RotateZeroSecret bare secrets", 700.0);
        drawLine(page1, "KeepThisVisible public info", 650.0);

        auto& page2 = doc.GetPages().CreatePage(PoDoFo::Rect(0, 0, 612, 792));
        page2.SetRotation(270);
        drawLine(page2, "RotateTwoSeventySecret hidden note", 700.0);
        drawLine(page2, "KeepPageTwo public record", 650.0);

        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "makeRotatedSecretPdf failed:" << e.what();
        return QString();
    }
    return path;
}

// Plants an embedded file (file specification + name tree) carrying `payload`
// into the document — the exact surface the sweep must cover per contract.
bool attachFileWithPayload(PoDoFo::PdfMemDocument& doc,
                           const char* name, const QByteArray& payload)
{
    try {
        auto& payloadObj = doc.GetObjects().CreateDictionaryObject();
        payloadObj.GetOrCreateStream().SetData(
            PoDoFo::bufferview(payload.constData(), size_t(payload.size())));

        auto& efDict = doc.GetObjects().CreateDictionaryObject();
        efDict.GetDictionary().AddKey(PoDoFo::PdfName("F"),
                                      payloadObj.GetIndirectReference());

        auto& filespec = doc.GetObjects().CreateDictionaryObject();
        filespec.GetDictionary().AddKey(PoDoFo::PdfName("Type"),
                                        PoDoFo::PdfObject(PoDoFo::PdfName("Filespec")));
        filespec.GetDictionary().AddKey(PoDoFo::PdfName("F"),
                                        PoDoFo::PdfObject(PoDoFo::PdfString(name)));
        filespec.GetDictionary().AddKey(PoDoFo::PdfName("EF"),
                                        efDict.GetIndirectReference());

        PoDoFo::PdfArray nameArr;
        nameArr.Add(PoDoFo::PdfObject(PoDoFo::PdfString(name)));
        nameArr.Add(filespec.GetIndirectReference());

        auto& nameNode = doc.GetObjects().CreateDictionaryObject();
        nameNode.GetDictionary().AddKey(PoDoFo::PdfName("Names"), nameArr);

        auto& namesRoot = doc.GetObjects().CreateDictionaryObject();
        namesRoot.GetDictionary().AddKey(PoDoFo::PdfName("EmbeddedFiles"),
                                         nameNode.GetIndirectReference());
        doc.GetCatalog().GetDictionary().AddKey(PoDoFo::PdfName("Names"),
                                                namesRoot.GetIndirectReference());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "attachFileWithPayload failed:" << e.what();
        return false;
    }
}

// Tamper helpers — each produces a NEW file, leaving the committed output
// untouched (tests compare verdicts across untampered and tampered copies).

bool tamperInfoTitle(const QString& inPath, const QString& outPath, const QString& title)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inPath.toUtf8().constData());
        doc.GetMetadata().SetTitle(PoDoFo::PdfString(title.toUtf8().constData()));
        doc.Save(outPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "tamperInfoTitle failed:" << e.what();
        return false;
    }
}

bool tamperXmp(const QString& inPath, const QString& outPath, const QString& secret)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inPath.toUtf8().constData());
        const std::string xmp =
            "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>"
            "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" "
            "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:RDF>"
            "<rdf:Description xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title><rdf:Alt><rdf:li>" + secret.toStdString() +
            "</rdf:li></rdf:Alt></dc:title></rdf:Description></rdf:RDF></x:xmpmeta>";
        doc.GetCatalog().SetMetadataStreamValue(xmp);
        // NoMetadataUpdate is REQUIRED here (soak-followup 2026-09-23): a
        // default-options Save stamps /Info/ModDate with the current time and,
        // when that stamp actually differs from the stored one (PDF dates have
        // second granularity — a second-boundary crossing between the redaction
        // save and this save makes it differ, so the flake is load-sensitive),
        // PoDoFo 1.1.0 re-synchronizes the /Metadata packet from its metadata
        // store — silently DISCARDING the plant above. The proof then rightly
        // passes over a file that carries no secret and the slot failed ~1-in-4
        // under load / 10x in the 48h soak. NoMetadataUpdate is PoDoFo's
        // documented option for manual XMP manipulation. The reload below
        // enforces the plant-survived contract so any future PoDoFo behavior
        // change fails HERE, not as a confusing proof PASS.
        doc.Save(outPath.toUtf8().constData(),
                 PoDoFo::PdfSaveOptions::NoMetadataUpdate);
        PoDoFo::PdfMemDocument check;
        check.Load(outPath.toUtf8().constData());
        if (check.GetCatalog().GetMetadataStreamValue().find(
                secret.toStdString()) == std::string::npos) {
            qWarning() << "tamperXmp: planted XMP did not survive the save";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        qWarning() << "tamperXmp failed:" << e.what();
        return false;
    }
}

bool tamperAnnotation(const QString& inPath, const QString& outPath, const QString& secret)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inPath.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(40.0, 100.0, 200.0, 30.0));
        annot.SetContents(PoDoFo::PdfString(secret.toUtf8().constData()));
        doc.Save(outPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning() << "tamperAnnotation failed:" << e.what();
        return false;
    }
}

bool tamperAppendSecondEof(const QString& inPath, const QString& outPath)
{
    const QByteArray bytes = fileBytes(inPath);
    if (bytes.isEmpty()) return false;
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray out = bytes;
    out += QByteArrayLiteral("\n%%EOF\n");
    const bool ok = f.write(out) == out.size();
    f.close();
    return ok;
}

QString joinedFailures(const Result& r)
{
    return r.failureReasons.join(QStringLiteral(" || "));
}

QString joinedProofFailures(const RedactResult& r)
{
    return r.proofFailures.join(QStringLiteral(" || "));
}

QJsonObject jsonRoot(const QString& path)
{
    return QJsonDocument::fromJson(fileBytes(path)).object();
}

} // namespace

class TestRedactionProof : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tmpDir.isValid(), "temp dir invalid");
    }

    // ── unit-level primitives ────────────────────────────────────────────

    void survivorEncodingsCoverPdfStringForms()
    {
        const QList<QByteArray> enc = survivorEncodings(QStringLiteral("Secret"));
        QVERIFY(enc.contains(QByteArrayLiteral("Secret")));                 // raw UTF-8
        bool sawUtf16 = false, sawHex = false;
        for (const QByteArray& e : enc) {
            if (e.startsWith(QByteArrayLiteral("\xFE\xFF"))) sawUtf16 = true;  // BOM + UTF-16BE
            if (e == QByteArrayLiteral("536563726574")) sawHex = true;
        }
        QVERIFY2(sawUtf16, "UTF-16BE with BOM (PDF text string form) missing");
        QVERIFY2(sawHex, "hex-string form missing");
    }

    void operatorCounterSkipsStringsAndDicts()
    {
        // (x) Tj, [(a) -2 (b)] TJ, a literal containing the decoy "Tj", a
        // BT/ET block, a dict with hex string, a comment — and ' and ".
        const QByteArray stream = QByteArrayLiteral(
            "BT /F1 12 Tf 100 700 Td (x) Tj ET\n"
            "[(a) -2 (b)] TJ\n"
            "(Tj) Tj\n"
            "% comment Tj must not count\n"
            "<< /X <AABBCC> /Y (TJ) >>\n"
            "14 TL (next)\n"
            "'\n"
            "\"\n");
        // Real glyph-carrying operators: Tj x2, TJ x1 (has strings), ' x1,
        // " x1 -> 5.
        QCOMPARE(countTextOperators(stream), 5);
        QCOMPARE(countTextOperators(QByteArrayLiteral("")), 0);
        QCOMPARE(countTextOperators(QByteArrayLiteral("(unbalanced Tj")), 0);
        // The excision engine's Edact-Ray substitute: numeric-only TJ — the
        // cursor gap survives, the glyphs do not. NOT glyph-carrying.
        QCOMPARE(countTextOperators(QByteArrayLiteral("[ -123.456 ] TJ")), 0);
        QCOMPARE(countTextOperators(QByteArrayLiteral("[ (kept) 12 ] TJ")), 1);
        // Mixed array after an excision: one glyph run left of three.
        QCOMPARE(countTextOperators(QByteArrayLiteral(
            "(a) Tj [ (b) -2 (c) ] TJ [ 42 ] TJ (d) Tj [ 7 ] TJ '")), 4);
    }

    // ── the pass case + manifest mechanics ───────────────────────────────

    void proofPassesAfterRealRedactionWithManifest()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("pass_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("pass_redacted.pdf");

        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(r.proofPassed, qPrintable(QStringLiteral("proof should PASS: %1")
                                               .arg(joinedProofFailures(r))));
        QVERIFY(r.proofJsonPath.endsWith(QStringLiteral("_redaction-proof.json")));
        QVERIFY(QFile::exists(r.proofJsonPath));
        QVERIFY(QFile::exists(r.proofTextPath));

        // Pack content: JSON verdict + manifest mechanics + disclaimer.
        const QJsonObject root = jsonRoot(r.proofJsonPath);
        QCOMPARE(root["verdict"].toString(), QStringLiteral("PASS"));
        QCOMPARE(root["pages_before"].toInt(), 2);
        QCOMPARE(root["pages_after"].toInt(), 2);
        QVERIFY(!root["files"].toObject()["output_sha256"].toString().isEmpty());
        QVERIFY(root["disclaimer"].toString().contains(QStringLiteral("audit stamp")));

        // SEP13 L6 — the pack must claim exactly what PDFium-only extraction
        // plus annotation/form scanning delivers. The old wording claimed
        // "text encoded with non-standard glyph encodings is covered by
        // decode-level text extraction", but decode-level extraction IS the
        // gap: attribution and the extracted-text sweep share it, so a font
        // PDFium cannot decode (subset, no /ToUnicode) is invisible to the
        // verdict. The disclaimer must state that limit, not overclaim.
        const QString disclaimer = root["disclaimer"].toString();
        QVERIFY2(!disclaimer.contains(
                     QStringLiteral("is covered by decode-level text extraction")),
                 "SEP13 L6: the overclaim must stay out of the pack — decode-level "
                 "extraction is the recall LIMIT, not the coverage");
        QVERIFY2(disclaimer.contains(QStringLiteral("NOT covered")),
                 "SEP13 L6: the disclaimer must state the extraction-blind limit "
                 "as a NOT-covered scope, honestly");
        QVERIFY2(disclaimer.contains(QStringLiteral("form-field")),
                 "SEP13 L6: the disclaimer must name annotation/form-field "
                 "strings as part of what attribution actually scans");

        const QJsonArray entries = root["excisions"].toArray();
        QCOMPARE(entries.size(), 1);
        const QJsonObject e0 = entries.at(0).toObject();
        QCOMPARE(e0["page_1based"].toInt(), 1);
        QCOMPARE(e0["method"].toString(), QStringLiteral("excision"));
        const QJsonArray region = e0["region"].toArray();
        QCOMPARE(region.at(0).toDouble(), secretMark().x());
        QCOMPARE(region.at(2).toDouble(), secretMark().width());
        const QJsonArray removed = e0["removed_strings"].toArray();
        bool sawSecret = false;
        for (const auto& v : removed)
            if (v.toString().contains(QStringLiteral("TopSecretAlpha"))) sawSecret = true;
        QVERIFY2(sawSecret, "the attributed removed text must name the secret");
        QVERIFY2(e0["page_stream_sha256_before"].toString()
                     != e0["page_stream_sha256_after"].toString(),
                 "the page content stream digest must change across the excision");
        QVERIFY(e0["page_stream_digestable"].toBool());
        QVERIFY2(e0["text_operators_after"].toInt() < e0["text_operators_before"].toInt(),
                 "text-showing operators must decrease across the excision");
        QVERIFY2(e0["text_runs_after"].toInt() < e0["text_runs_before"].toInt(),
                 "PDFium text runs must decrease across the excision");

        // Every surface reported; each either clean or absent.
        const QJsonArray surfaces = root["surfaces"].toArray();
        QCOMPARE(surfaces.size(), 8); // all eight named surfaces reported
        for (const auto& v : surfaces) {
            const QString verdict = v.toObject()["verdict"].toString();
            QVERIFY2(verdict == QStringLiteral("clean") || verdict == QStringLiteral("absent"),
                     qPrintable(QStringLiteral("surface %1 unexpectedly %2")
                                    .arg(v.toObject()["surface"].toString(), verdict)));
        }

        // TXT pack: human-readable verdict + no-overclaim wording.
        const QString text = QString::fromUtf8(fileBytes(r.proofTextPath));
        QVERIFY(text.contains(QStringLiteral("PASS")));
        QVERIFY(text.contains(QStringLiteral("TopSecretAlpha")));
        QVERIFY(text.contains(QStringLiteral("audit stamp")));
        QVERIFY(text.contains(QStringLiteral("SHA-256")));

        // Independent extractor double-check on the committed bytes.
        PdfiumBackend backend;
        QVERIFY(backend.loadDocument(dest));
        const QString page1 = backend.extractText(0);
        QVERIFY(!page1.contains(QStringLiteral("TopSecretAlpha")));
        QVERIFY(page1.contains(QStringLiteral("KeepThisVisible")));
        QVERIFY(backend.extractText(1).contains(QStringLiteral("KeepPageTwo")));
    }

    // ── G1(c): the pack must name the XFA limitation ────────────────────────
    //
    // Legacy XFA form data re-encodes field values in streams no sweep can
    // attribute; GlyphPDF's answer is refusal (RedactOperation preflight) plus
    // sanitize removal. The disclaimer must SAY so, like the raster-image
    // limitation it already names (audit REDACTION-RESEARCH-2026-09-21 §2.5).
    void packDisclaimerNamesXfaRefusalPolicy()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("xfa_note_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("xfa_note_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);

        const QJsonObject root = jsonRoot(r.proofJsonPath);
        const QString disclaimer = root["disclaimer"].toString();
        QVERIFY2(disclaimer.contains(QStringLiteral("XFA")),
                 "G-01: the pack disclaimer must name the XFA policy");
        QVERIFY2(disclaimer.contains(QStringLiteral("refus")),
                 "G-01: the disclaimer must state the refusal policy for "
                 "XFA-bearing documents");
        const QString text = QString::fromUtf8(fileBytes(r.proofTextPath));
        QVERIFY2(text.contains(QStringLiteral("XFA")),
                 "G-01: the TXT pack must name the XFA policy too");
    }

    // ── G6 (audit §1.6 Plan 1-C): an ABSENT /Contents is "no content", not
    // "undecodable" — annotation-only and scanned pages must not emit UNSWEPT
    // [PageStreams] noise (it trains users to ignore the rows that mark real
    // decode failures). Zero UNSWEPT rows, honest failure over the survivor.
    void annotationOnlyPageCarriesZeroUnsweptRows()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Fixture: a page with NO /Contents at all + an annotation-only secret.
        const QString src = tmp.filePath("annot_only.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            auto& annot = page.GetAnnotations().CreateAnnot(
                PoDoFo::PdfAnnotationType::FreeText,
                PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
            annot.SetContents(PoDoFo::PdfString("AnnotSecretZebra"));
            doc.Save(src.toUtf8().constData());
        }
        {   // fixture sanity: the page truly has no /Contents
            PoDoFo::PdfMemDocument d;
            d.Load(src.toUtf8().constData());
            QVERIFY(d.GetPages().GetPageAt(0).GetContents() == nullptr);
        }
        const QString out = tmp.filePath("annot_only_out.pdf");
        QVERIFY(QFile::copy(src, out));

        // Mark over the annot (unrotated A4: viewer y = 842-680..842-650).
        gp::RedactionProof::Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0].append(QRectF(100.0, 162.0, 200.0, 30.0));
        const gp::RedactionProof::Result proof = gp::RedactionProof::verify(req);
        QVERIFY(proof.proofRan);
        for (const QString& f : proof.failureReasons) {
            QVERIFY2(!f.contains(QStringLiteral("UNSWEPT [PageStreams]")),
                     qPrintable(QStringLiteral("G-06: absent /Contents must not "
                                              "emit UNSWEPT noise: %1").arg(f)));
        }
        bool attributed = false;
        for (const auto& e : proof.entries)
            for (const auto& s : e.removedStrings)
                attributed |= s.contains(QLatin1String("AnnotSecretZebra"));
        QVERIFY2(attributed, "G-06: the annot secret must still be attributed");
        QVERIFY2(!proof.proofPassed,
                 "G-06: the proof must still FAIL honestly over the survivor");
    }

    // G6 companion: corruption cannot slip a secret past the failure net.
    // PoDoFo 1.1's stream decode is LENIENT — a corrupted flate payload comes
    // back from CopyTo as raw bytes rather than throwing — so the ok=false
    // "real decode exception" branch of pageMechanics is effectively
    // unreachable through file corruption, and no UNSWED [PageStreams] row is
    // produced for this fixture (asserted here as the documented reality).
    // What the companion pins is the property that matters: the proof still
    // FAILS honestly over the surviving secret on the corrupted file.
    void corruptStreamStillFailsHonestly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = tmp.filePath("corrupt_stream.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            {   // a real content stream — the corruption target
                PoDoFo::PdfPainter painter;
                painter.SetCanvas(page);
                auto& font = doc.GetFonts().GetStandard14Font(
                    PoDoFo::PdfStandard14FontType::Helvetica);
                painter.TextState.SetFont(font, 12.0);
                (painter.DrawText)("PUBLIC_KEEP_TEXT", 50, 650);
                painter.FinishDrawing();
            }
            auto& annot = page.GetAnnotations().CreateAnnot(
                PoDoFo::PdfAnnotationType::FreeText,
                PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
            annot.SetContents(PoDoFo::PdfString("AnnotSecretZebra"));
            doc.Save(src.toUtf8().constData());
        }
        {   // Corrupt the saved bytes IN PLACE: flip every stream payload
            // (length-preserving, /Length untouched). PoDoFo's Save
            // re-encodes streams itself, so the corruption must happen after
            // the file is written.
            QFile f(src);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QByteArray all = f.readAll();
            f.close();
            QByteArray corrupted = all;
            int flipped = 0;
            int pos = 0;
            while (true) {
                const int s = corrupted.indexOf("stream", pos);
                if (s < 0) break;
                pos = s + 6;
                if (s >= 3 && corrupted.mid(s - 3, 3) == "end")
                    continue; // matched inside "endstream"
                int data = pos;
                while (data < corrupted.size()
                       && (corrupted[data] == '\n' || corrupted[data] == '\r'))
                    ++data;
                const int e = corrupted.indexOf("endstream", data);
                if (e < 0) break;
                for (int i = data; i < e; ++i)
                    corrupted[i] = static_cast<char>(corrupted[i] ^ 0xFF);
                ++flipped;
            }
            QVERIFY2(flipped >= 1, "fixture: expected stream payloads to corrupt");
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(f.write(corrupted), qint64(corrupted.size()));
            f.close();
        }

        gp::RedactionProof::Request req;
        req.sourcePath = src;
        req.outputPath = src; // same corrupted file on both sides
        req.redactionsByPage[0].append(QRectF(100.0, 162.0, 200.0, 30.0));
        const gp::RedactionProof::Result proof = gp::RedactionProof::verify(req);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed,
                 "G-06: a corrupted stream must not slip the secret past the "
                 "failure net — the proof still fails over the survivor");
        bool survivorNamed = false;
        for (const QString& f : proof.failureReasons)
            survivorNamed |= f.contains(QStringLiteral("AnnotSecretZebra"));
        QVERIFY2(survivorNamed,
                 "G-06: the surviving secret must be named in the failures");
    }

    // ── W2B-1: the /Rotate 270 page-shape (the fixture blind spot) ──────────
    //
    // The excision rect for a mark on a /Rotate 270 page must land on the
    // secret in RAW USER space. The pre-fix base fed the law a
    // rotation-normalized (W/H-swapped) MediaBox, so the mark mapped to a
    // TRANSPOSED region that missed the glyphs entirely — and because the
    // proof's own mark→region attribution transposed identically, the proof
    // stayed GREEN while the secret survived (the SEP13 false-success class).
    // The independent PDFium extractor below is what makes the defect visible.
    void rotatedPageSecretIsExcisedAndProofPasses()
    {
        const QString src = makeRotatedSecretPdf(m_tmpDir.filePath("rot_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("rot_redacted.pdf");

        QMap<int, QList<QRectF>> rects;
        // Page 1 (rot 0) control: the user region (90..370, 694..718) in
        // viewer coords: y = 792-718..792-694 = 74..98.
        rects[0].append(QRectF(90.0, 74.0, 280.0, 24.0));
        // Page 2 (rot 270): the SAME user region, hand-computed through the
        // published law table (PageSpaceTransform.h), Letter 612x792:
        //   vx = H-uy1..H-uy0 = 792-718..792-694 = 74..98
        //   vy = W-ux1..W-ux0 = 612-370..612-90 = 242..522
        // — a 24x280 VERTICAL strip (the text line displays rotated).
        rects[1].append(QRectF(74.0, 242.0, 24.0, 280.0));

        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(r.proofPassed, qPrintable(QStringLiteral("rotated-page proof: %1")
                                               .arg(joinedProofFailures(r))));

        // Independent extractor double-check: the rot-270 page's secret must
        // be GONE from the committed bytes (pre-fix base: survived here while
        // the proof above still claimed PASS), the public lines must survive.
        PdfiumBackend backend;
        QVERIFY(backend.loadDocument(dest));
        QVERIFY(!backend.extractText(0).contains(QStringLiteral("RotateZeroSecret")));
        QVERIFY(backend.extractText(0).contains(QStringLiteral("KeepThisVisible")));
        QVERIFY(!backend.extractText(1).contains(QStringLiteral("RotateTwoSeventySecret")));
        QVERIFY(backend.extractText(1).contains(QStringLiteral("KeepPageTwo")));
    }

    void manifestRecordsRegionMethodAndCounts()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("manifest_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("manifest_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[1].append(QRectF(90.0, kA4Height - 700.0 - 18.0, 250.0, 24.0));
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(r.proofPassed, qPrintable(joinedProofFailures(r)));

        const QJsonObject root = jsonRoot(r.proofJsonPath);
        const QJsonObject e0 = root["excisions"].toArray().at(0).toObject();
        QCOMPARE(e0["page_1based"].toInt(), 2);
        QCOMPARE(e0["method"].toString(), QStringLiteral("excision"));
        QVERIFY(e0["text_runs_before"].toInt() >= 1);
        QVERIFY(e0["text_runs_after"].toInt() >= 0);
    }

    void legacyRunWithoutProofStaysProofFree()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("legacy_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("legacy_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = false; // the default — TestRedactTransaction's world
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(!r.proofRan);
        QVERIFY(!r.proofPassed);
        QVERIFY(r.proofJsonPath.isEmpty());
        QVERIFY(r.proofTextPath.isEmpty());
        QVERIFY(!QFile::exists(dest + QStringLiteral("_redaction-proof.json")));
    }

    // ── loud failures: the proof must be able to fail ────────────────────

    void proofFailsOnMissedSpotOnSamePage()
    {
        // THE r/pdf scenario: the same secret survives on an unmarked line.
        const QString src = makeSourcePdf(m_tmpDir.filePath("miss_src.pdf"),
                                          "TopSecretAlpha bare secrets", 620.0);
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("miss_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed); // the file IS committed…
        QVERIFY(r.proofRan);
        QVERIFY2(!r.proofPassed, "a surviving duplicate must FAIL the proof");
        QVERIFY(!r.proofFailures.isEmpty());
        const QString failures = joinedProofFailures(r);
        QVERIFY2(failures.contains(QStringLiteral("extracted-text")),
                 qPrintable(QStringLiteral("failure must name the text layer: %1").arg(failures)));

        // The pack is still written and its JSON verdict flips to FAIL.
        QVERIFY(QFile::exists(r.proofJsonPath));
        QCOMPARE(jsonRoot(r.proofJsonPath)["verdict"].toString(), QStringLiteral("FAIL"));
    }

    void proofFailsOnInfoDictionarySurvivor()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("info_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("info_redacted.pdf");
        const QString tampered = dest + QStringLiteral(".tampered.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());

        QVERIFY(redactAndVerifyPass(src, dest, rects));
        QVERIFY(tamperInfoTitle(dest, tampered,
                                QStringLiteral("Leak of TopSecretAlpha bare secrets")));
        const Result proof = verifyRequest(src, tampered, rects);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed,
                 "a redacted string planted in the info dictionary must FAIL the proof");
        const QString failures = joinedFailures(proof);
        QVERIFY2(failures.contains(QStringLiteral("info-dictionary"))
                     || failures.contains(QStringLiteral("object-strings")),
                 qPrintable(QStringLiteral("failure must name the metadata surface: %1")
                                .arg(failures)));
    }

    void proofFailsOnXmpSurvivor()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("xmp_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("xmp_redacted.pdf");
        const QString tampered = dest + QStringLiteral(".tampered.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());

        QVERIFY(redactAndVerifyPass(src, dest, rects));
        QVERIFY(tamperXmp(dest, tampered, QStringLiteral("TopSecretAlpha bare secrets")));
        const Result proof = verifyRequest(src, tampered, rects);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed, "a redacted string planted in XMP must FAIL the proof");
        const QString failures = joinedFailures(proof);
        QVERIFY2(failures.contains(QStringLiteral("xmp-metadata")),
                 qPrintable(QStringLiteral("failure must name the XMP surface: %1").arg(failures)));
    }

    void proofFailsOnAnnotationStringSurvivor()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("annot_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("annot_redacted.pdf");
        const QString tampered = dest + QStringLiteral(".tampered.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());

        QVERIFY(redactAndVerifyPass(src, dest, rects));
        QVERIFY(tamperAnnotation(dest, tampered, QStringLiteral("TopSecretAlpha bare secrets")));
        const Result proof = verifyRequest(src, tampered, rects);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed, "a redacted string planted in an annotation must FAIL");
        const QString failures = joinedFailures(proof);
        QVERIFY2(failures.contains(QStringLiteral("object-strings"))
                     || failures.contains(QStringLiteral("raw-bytes")),
                 qPrintable(QStringLiteral("failure must name the object-string surface: %1")
                                .arg(failures)));
    }

    void proofFailsOnAttachmentSurvivor()
    {
        // The secret ALSO lives in an embedded file; page-level excision cannot
        // fix that, and the proof must catch it instead of blessing the file.
        const QString src = m_tmpDir.filePath("attach_src.pdf");
        try {
            PoDoFo::PdfMemDocument doc;
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)("TopSecretAlpha bare secrets", 100.0, 700.0);
            painter.FinishDrawing();
            QVERIFY(attachFileWithPayload(doc, "leaked.txt",
                                          QByteArray("TopSecretAlpha bare secrets\n")));
            doc.Save(src.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("attachment fixture failed: %1").arg(e.what())));
        }

        const QString dest = m_tmpDir.filePath("attach_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true; // NO sanitize: the attachment must trip the proof
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(!r.proofPassed, "a survivor inside an embedded file must FAIL the proof");
        QVERIFY2(joinedProofFailures(r).contains(QStringLiteral("embedded-files")),
                 qPrintable(QStringLiteral("failure must name the embedded-file surface: %1")
                                .arg(joinedProofFailures(r))));
    }

    void proofFailsOnIncrementalUpdateRemnant()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("rev_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("rev_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());

        // Committed output passes…
        QVERIFY(redactAndVerifyPass(src, dest, rects));
        // …but a second %%EOF means superseded bytes may exist: unswept → FAIL.
        const QString tampered = dest + QStringLiteral(".twice.pdf");
        QVERIFY(tamperAppendSecondEof(dest, tampered));
        const Result proof = verifyRequest(src, tampered, rects);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed,
                 "an incremental-update remnant must FAIL the proof (cannot rule out "
                 "superseded bytes)");
        QVERIFY2(joinedFailures(proof).contains(QStringLiteral("revision-structure")),
                 qPrintable(QStringLiteral("failure must name the revision surface: %1")
                                .arg(joinedFailures(proof))));
    }

    void proofFailsLoudlyOnUnsweepableOutput()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("junk_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString junk = m_tmpDir.filePath("junk_output.pdf");
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf at all");
        f.close();

        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        const Result proof = verifyRequest(src, junk, rects);
        QVERIFY2(!proof.proofRan, "an unsweepable output must not produce a verdict");
        QVERIFY2(!proof.error.isEmpty(), "the honest error must say what could not be swept");
        QVERIFY(!proof.proofPassed);
    }

    void verifyRejectsIncompleteRequest()
    {
        Request req;
        req.sourcePath = m_tmpDir.filePath("whatever.pdf");
        req.outputPath = m_tmpDir.filePath("whatever2.pdf");
        // No redactionsByPage: nothing to prove — refuse to fabricate a verdict.
        const Result proof = verify(req);
        QVERIFY(!proof.proofRan);
        QVERIFY(!proof.error.isEmpty());
        QVERIFY(!proof.proofPassed);
    }

    void emptyRegionMarkIsRecordedHonestly()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("empty_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("empty_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(QRectF(300.0, 400.0, 100.0, 50.0)); // blank corner — no text
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(r.proofPassed, qPrintable(QStringLiteral("an empty-region mark is not a "
                                                          "survivor: %1").arg(joinedProofFailures(r))));
        const QJsonObject e0 = jsonRoot(r.proofJsonPath)["excisions"].toArray().at(0).toObject();
        QCOMPARE(e0["status"].toString(), QStringLiteral("verified-no-text-in-region"));
    }

    void extraSurvivorStringsAreSwept()
    {
        // Pattern-redaction hook: caller-supplied strings are swept even when
        // geometry attribution cannot derive them.
        const QString src = makeSourcePdf(m_tmpDir.filePath("extra_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("extra_redacted.pdf");
        const QString tampered = dest + QStringLiteral(".tampered.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        QVERIFY(redactAndVerifyPass(src, dest, rects));

        // …an extra needle planted in the info dict of a copy must be found.
        QVERIFY(tamperInfoTitle(dest, tampered, QStringLiteral("PatternHit-Kappa-77")));
        Request req;
        req.sourcePath = src;
        req.outputPath = tampered;
        req.redactionsByPage = rects;
        req.extraSurvivorStrings.append(QStringLiteral("PatternHit-Kappa-77"));
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.proofPassed, "an extra needle found in the output must FAIL");
        QVERIFY(proof.extraStringsSwept.contains(QStringLiteral("PatternHit-Kappa-77")));
    }

    // ── whole-flow with sanitize: pack covers both artifacts ─────────────

    void proofWithSanitizeCoversSanitizedCopy()
    {
        const QString src = makeSourcePdf(m_tmpDir.filePath("san_src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = m_tmpDir.filePath("san_redacted.pdf");
        QMap<int, QList<QRectF>> rects;
        rects[0].append(secretMark());
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.sanitize = true;
        req.sanitizedDestinationPath = dest + QStringLiteral("_sanitized.pdf");
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        QCOMPARE(r.outcome, RedactOutcome::Completed);
        QVERIFY(r.proofRan);
        QVERIFY2(r.proofPassed, qPrintable(joinedProofFailures(r)));
        QVERIFY(QFile::exists(req.sanitizedDestinationPath));

        const QJsonObject files = jsonRoot(r.proofJsonPath)["files"].toObject();
        QVERIFY2(!files["sanitized_sha256"].toString().isEmpty(),
                 "the pack must hash-bind the sanitized copy when one was produced");
    }

    // ── UI seam: dialog default, plan transport, presenter honesty ───────

    void dialogDefaultsProofOnAndTransportsThroughPlan()
    {
        RedactApplyPlan plan;
        plan.sourcePath = m_tmpDir.filePath("ui_src.pdf");
        plan.destinationPath = m_tmpDir.filePath("ui_redacted.pdf");
        plan.sanitizedDestinationPath = m_tmpDir.filePath("ui_redacted_sanitized.pdf");
        plan.markCount = 1;
        plan.marksPerPage[0] = 1;
        plan.sanitize = kDefaultSanitizeOn;

        RedactApplyDialog dlg(plan);
        QVERIFY(dlg.summaryText().contains(QStringLiteral("1 mark")));
        // Default ON — proof mode is the feature.
        RedactApplyPlan chosen = dlg.plan();
        QVERIFY(chosen.produceProof);
        // Explicit opt-out survives plan().
        dlg.setProduceProofChecked(false);
        chosen = dlg.plan();
        QVERIFY(!chosen.produceProof);
        // And the shared conversion transports it (the N04 single seam).
        QMap<int, QList<QRectF>> marks;
        marks[0].append(secretMark());
        const RedactRequest req = redactRequestFromPlan(chosen, marks);
        QVERIFY(!req.produceProof);
        dlg.setProduceProofChecked(true);
        const RedactRequest req2 = redactRequestFromPlan(dlg.plan(), marks);
        QVERIFY(req2.produceProof);
    }

    void presenterWordingIsHonestInBothDirections()
    {
        RedactResult r;
        r.outcome = RedactOutcome::Completed;
        r.destination = QStringLiteral("/tmp/x_redacted.pdf");
        QVERIFY(!RedactResultPresenter::bannerText(r).contains(QStringLiteral("proof")));

        r.proofRan = true;
        r.proofPassed = true;
        r.proofJsonPath = QStringLiteral("/tmp/x_redacted_redaction-proof.json");
        r.proofTextPath = QStringLiteral("/tmp/x_redacted_redaction-proof.txt");
        QVERIFY(RedactResultPresenter::bannerText(r).contains(QStringLiteral("PASSED")));
        const QString passDetail = RedactResultPresenter::detailText(r);
        QVERIFY(passDetail.contains(QStringLiteral("PASSED")));
        QVERIFY(passDetail.contains(QStringLiteral("redaction-proof.txt")));

        r.proofPassed = false;
        r.proofFailures = QStringList{ QStringLiteral("SURVIVOR [raw-bytes] X — y") };
        QVERIFY(RedactResultPresenter::bannerText(r).contains(QStringLiteral("FAILED")));
        const QString failDetail = RedactResultPresenter::detailText(r);
        QVERIFY(failDetail.contains(QStringLiteral("FAILED")));
        QVERIFY(failDetail.contains(QStringLiteral("SURVIVOR [raw-bytes] X — y")));
    }

private:
    QTemporaryDir m_tmpDir;

    // Synchronous capture of the operation's terminal result (the
    // TestRedactTransaction idiom: finished() delivered during run()).
    static RedactResult runOp(RedactOperation* op)
    {
        RedactResult captured;
        QObject::connect(op, &RedactOperation::finished, op,
                         [&captured](const RedactResult& r) { captured = r; });
        op->run();
        return captured;
    }

    // Redact the fixture and require a PASS verdict on the untampered output,
    // so every tamper test proves the verdict FLIPS rather than a perpetually
    // broken sweep passing everything.
    bool redactAndVerifyPass(const QString& src, const QString& dest,
                             const QMap<int, QList<QRectF>>& rects)
    {
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage = rects;
        req.produceProof = true;
        RedactOperation op(req);
        const RedactResult r = runOp(&op);
        if (r.outcome != RedactOutcome::Completed || !r.proofPassed) {
            qWarning() << "baseline redaction did not pass:" << r.error
                       << joinedProofFailures(r);
            return false;
        }
        return true;
    }

    Result verifyRequest(const QString& src, const QString& output,
                         const QMap<int, QList<QRectF>>& rects)
    {
        Request req;
        req.sourcePath = src;
        req.outputPath = output;
        req.redactionsByPage = rects;
        return verify(req);
    }
};

#include "TestRedactionProof.moc"
QTEST_MAIN(TestRedactionProof)
