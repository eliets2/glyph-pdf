// SPDX-License-Identifier: Apache-2.0
// SEP13 leads 5 + 7 + 8 — redaction-proof coordinate/attribution leads.
//
// PARITY-GLM-REVIEW-2026-09-13 leads exercised here (RedactionProof.cpp,
// RedactOperation.cpp):
//   lead 5 (~693/~813): runIntersects maps viewer marks with MediaBox HEIGHT
//     only — page /Rotate and the MediaBox lower-left ORIGIN are ignored, so
//     on rotated / offset-origin pages NO source run is attributed,
//     removedStrings stays empty, and the entry is certified
//     VerifiedNoTextInRegion — a false certification over text that WAS in
//     the region (and in these fixtures still IS in the output).
//   lead 7 (~819): attribution derives removedStrings ONLY from PDFium page-
//     content runs; text that lives only in an annotation string is never
//     attributed and therefore never swept for → false PASS.
//   lead 8 (RedactOperation.cpp ~189/~222): the burn-in overlay label flips
//     the viewer rect with pageHeight = GetMediaBox().Height, ignoring the
//     MediaBox lower-left origin — the label (and the black box painted by
//     the same flip in PoDoFoBackend::applyRedactions) is shifted DOWN by the
//     origin offset on offset-origin pages.
//
// Probes assert the CORRECT contract and are expected to FAIL on candidate
// 83be3c2. Diagnostics log the measured PDFium/PoDoFo coordinate semantics so
// the confirmation document cites measured numbers, not assumptions.
#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "core/RedactionProof.h"
#include "engines/RedactOperation.h"
#include "engines/pdfium/PdfiumBackend.h"

#ifdef DrawText
#undef DrawText
#endif

using namespace gp;
using namespace gp::RedactionProof;

namespace {

constexpr double kOffY = 200.0;   // MediaBox lower-left Y of the offset fixture
constexpr double kOffH = 842.0;   // MediaBox HEIGHT of the offset fixture
constexpr double kOffW = 612.0;

bool copyFile(const QString& in, const QString& out) {
    QFile::remove(out);
    return QFile::copy(in, out);
}

// Offset-origin page: MediaBox [0 200 612 1042]. Secret line at user y=900
// (viewer top-down y = 1042-900 = 142), benign line at user y=860.
QString makeOffsetOriginPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::Rect(0.0, kOffY, kOffW, kOffH));
        auto draw = [&](const char* text, double y) {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)(text, 100.0, y);
            painter.FinishDrawing();
        };
        draw("TopSecretAlpha bare secrets", 900.0);
        draw("KeepThisVisible public info", 860.0);
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeOffsetOriginPdf failed:" << e.what();
        return {};
    }
}

// Rotated page: A4 MediaBox [0 0 595 842] with /Rotate 90. Secret at user
// (100, 300). Measured PDFium display convention (render probe, redactfix
// lane 2026-09-14): user offset (dx, dy) from the MediaBox origin displays at
// (dy, dx) — the page renders rotated CLOCKWISE, so the line's viewer
// position is baseline (300, 100) with the glyphs running DOWN from there
// (viewer band x≈[288,312] = user y 288..312 incl. ascent, y≈[100,198] =
// user x 100..198 glyph extent).
QString makeRotatedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::Rect(0.0, 0.0, 595.0, 842.0));
        // No SetRotate on this PoDoFo API — set the page dict key directly.
        page.GetDictionary().AddKey(PoDoFo::PdfName("Rotate"),
                                    PoDoFo::PdfObject(int64_t(90)));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.TextState.SetFont(font, 12.0);
        (painter.DrawText)("RotatedSecret line", 100.0, 300.0);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeRotatedPdf failed:" << e.what();
        return {};
    }
}

// Standard A4 page (control — the geometry existing pins already cover).
QString makeStandardPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::Rect(0.0, 0.0, 612.0, 792.0));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.TextState.SetFont(font, 12.0);
        (painter.DrawText)("TopSecretAlpha bare secrets", 100.0, 700.0);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeStandardPdf failed:" << e.what();
        return {};
    }
}

// Page whose ONLY secret lives in an annotation string. The page carries an
// EMPTY-but-DECODEABLE content stream (a painter pass with no draw calls) so
// the proof's stream decode succeeds and nothing blocks a clean verdict.
QString makeAnnotationOnlyPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::Rect(0.0, 0.0, 612.0, 792.0));
        {   // Valid, empty content stream — decodes fine, yields 0 runs.
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.FinishDrawing();
        }
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
        annot.SetContents(PoDoFo::PdfString("AnnotationSecret"));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeAnnotationOnlyPdf failed:" << e.what();
        return {};
    }
}

// MIXED page: a decodable content stream (benign line) PLUS a secret that
// lives ONLY in an annotation string. If attribution is content-only, a mark
// over the ANNOTATION alone attributes nothing while the stream still
// decodes — the proof can certify a clean PASS over the surviving secret.
QString makeMixedAnnotationPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::Rect(0.0, 0.0, 612.0, 792.0));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.TextState.SetFont(font, 12.0);
        // Content placed LOW on the page: the Height-only flip maps the
        // annotation mark to a band around user y 640..690, so with content
        // at baseline 300 NOTHING is attributed — the exact shape in which
        // annotation blindness can certify a clean PASS.
        (painter.DrawText)("KeepThisVisible public info", 100.0, 300.0);
        painter.FinishDrawing();
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
        annot.SetContents(PoDoFo::PdfString("AnnotationSecret"));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeMixedAnnotationPdf failed:" << e.what();
        return {};
    }
}

void dumpRuns(const QString& label, PdfiumBackend& backend, int page) {
    const auto runs = backend.extractPageTextRuns(page);
    qInfo() << label << "page" << page << "runs:" << runs.size();
    for (const auto& r : runs)
        qInfo().nospace() << "  run \"" << r.text << "\" rect=(" << r.rect.x()
                          << "," << r.rect.y() << "," << r.rect.width()
                          << "x" << r.rect.height() << ")";
}

} // namespace

class TestSep13LeadRedactionProof : public QObject {
    Q_OBJECT

private slots:
    // Control pin (must PASS): on a standard-origin page the proof attributes
    // the secret (my mark math + the harness are right).
    void controlStandardPageAttributesText() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeStandardPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output still CONTAINS the secret

        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(90.0, 792.0 - 700.0 - 12.0, 300.0, 24.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.entries.isEmpty(), "one entry expected");
        const auto& e = proof.entries.first();
        qInfo() << "control: status =" << int(e.status) << "removed ="
                << e.removedStrings << "passed =" << proof.proofPassed;
        QVERIFY2(!e.removedStrings.isEmpty(),
                 "control: standard page must attribute the secret line");
    }

    // LEAD 5a CONFIRMATION (expected FAILURE on the candidate): offset-origin
    // MediaBox [0 200 612 1042] — the Height-only flip attributes nothing and
    // certifies "verified-no-text-in-region" over text that is STILL in the
    // output.
    void offsetOriginPageMustNotFalsePass() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeOffsetOriginPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());

        {   // Measure the real coordinate semantics (logged evidence).
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(src));
            dumpRuns("offset-source", backend, 0);
        }

        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output still CONTAINS the secret

        // Viewer mark over the secret line: viewer top = y0 + H = 1042;
        // viewer y of the line = 1042 - 900 = 142.
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(90.0, kOffY + kOffH - 900.0 - 12.0, 300.0, 24.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "offset: status =" << int(e.status) << "removed ="
                << e.removedStrings << "passed =" << proof.proofPassed
                << "failures =" << proof.failureReasons;

        // CORRECT contract: the entry must be Verified (attributed strings)
        // and the proof must FAIL loudly (the output still carries them).
        QVERIFY2(!e.removedStrings.isEmpty(),
                 "SEP13 lead 5 CONFIRMED: on an offset-origin page the proof "
                 "attributed NO text for a mark that covers a text line — "
                 "removedStrings is empty because the flip ignores the MediaBox "
                 "lower-left origin");
        QVERIFY2(!proof.proofPassed,
                 "the proof must FAIL when the attributed strings still survive");
    }

    // LEAD 5b CONFIRMATION (expected FAILURE on the candidate): /Rotate 90 —
    // the flip ignores rotation entirely; the mark maps to the wrong band and
    // nothing is attributed.
    void rotatedPageMustNotFalsePass() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeRotatedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());

        {   PdfiumBackend backend;
            QVERIFY(backend.loadDocument(src));
            dumpRuns("rotated-source", backend, 0);
        }

        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out));

        // Viewer (rotated) mark over the text at its MEASURED display position
        // (baseline (300,100), glyphs run down; see makeRotatedPdf above).
        // Mark x [288,312] covers user y 288..312 (∋ baseline 300); mark
        // y [88,262] covers user x 88..262 (∋ the full 100..198 glyph extent).
        // (The original repro mark (528,88,28,174) was derived from a
        // display mapping no real viewer produces — x from the /Rotate 270
        // law, y from the /Rotate 90 law — and covered empty page under the
        // corrected convention; fixed to the true viewer contract 2026-09-14,
        // assertions unchanged.)
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(288.0, 88.0, 24.0, 174.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "rotated: status =" << int(e.status) << "removed ="
                << e.removedStrings << "passed =" << proof.proofPassed;

        QVERIFY2(!e.removedStrings.isEmpty(),
                 "SEP13 lead 5 CONFIRMED: on a /Rotate 90 page the proof "
                 "attributed NO text for a mark that covers a text line — "
                 "the Height-only flip ignores /Rotate");
        QVERIFY2(!proof.proofPassed,
                 "the proof must FAIL when the attributed strings still survive");
    }

    // LEAD 7 CONFIRMATION (expected FAILURE on the candidate): a secret that
    // lives ONLY in an annotation string is never attributed (attribution is
    // page-content extraction only) and therefore never swept for — the proof
    // certifies a clean PASS over a surviving secret.
    void annotationOnlySecretMustNotFalsePass() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeAnnotationOnlyPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());

        {   PdfiumBackend backend;
            QVERIFY(backend.loadDocument(src));
            const auto runs = backend.extractPageTextRuns(0);
            qInfo() << "annotation-only source runs (must be 0 to prove the"
                    << "attribution source is blind to annotations):" << runs.size();
            QCOMPARE(runs.size(), 0);
        }

        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output still CONTAINS the annotation

        // Viewer mark over the annotation rect (user y 650..680, top 792).
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(90.0, 792.0 - 680.0 - 10.0, 220.0, 50.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "annotation-only: status =" << int(e.status) << "removed ="
                << e.removedStrings << "passed =" << proof.proofPassed
                << "failures =" << proof.failureReasons;

        QVERIFY2(!proof.proofPassed,
                 "SEP13 lead 7 CONFIRMED: the proof certified a PASS while the "
                 "annotation-only secret survives in the output — attribution "
                 "never derives annotation/form-field strings, so nothing was "
                 "swept for them");
    }

    // LEAD 7b DECISIVE PROBE (expected FAILURE on the candidate): same
    // annotation-blindness, but on a page whose content stream DECODES. The
    // mark covers ONLY the annotation; attribution is content-only, so
    // nothing is attributed, the sweep has no target, and the proof can
    // certify VerifiedNoTextInRegion / PASS while "AnnotationSecret" survives
    // verbatim in the output.
    void mixedPageAnnotationSecretMustNotFalsePass() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeMixedAnnotationPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());

        {
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(src));
            const auto runs = backend.extractPageTextRuns(0);
            qInfo() << "mixed-page content runs (annotation text must be absent):"
                    << runs.size();
            bool annotExtracted = false;
            for (const auto& r : runs)
                annotExtracted |= r.text.contains(QLatin1String("AnnotationSecret"));
            QCOMPARE(annotExtracted, false);
        }

        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output still CONTAINS the annotation

        // Viewer mark over the annotation rect (user y 650..680, top 792).
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(90.0, 792.0 - 680.0 - 10.0, 220.0, 50.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "mixed: status =" << int(e.status) << "removed ="
                << e.removedStrings << "passed =" << proof.proofPassed
                << "failures =" << proof.failureReasons;

        // CORRECT contract: the proof must NOT certify a clean PASS — the
        // annotation secret survives in the output and nothing was swept for.
        QVERIFY2(!proof.proofPassed,
                 "SEP13 lead 7 CONFIRMED (mixed page): the proof PASSED while the "
                 "annotation-only secret 'AnnotationSecret' survives in the "
                 "output — attribution is content-only, so the mark over the "
                 "annotation attributed nothing and certified the region clean");
    }

    // LEAD 8 CONFIRMATION (expected FAILURE on the candidate): the burn-in
    // overlay label on the offset page lands ~200pt (the MediaBox origin)
    // BELOW the redaction box position.
    void overlayLabelMustLandInsideMarkOnOffsetPage() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeOffsetOriginPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = tmp.filePath("dest.pdf");

        // Mark over the secret line (viewer coords): y = 1042-900-12 = 130.
        RedactRequest req;
        req.sourcePath = src;
        req.destinationPath = dest;
        req.redactionsByPage[0] = { QRectF(90.0, kOffY + kOffH - 900.0 - 12.0, 300.0, 30.0) };
        req.overlayText = QStringLiteral("LABEL");

        RedactOperation op(req);
        RedactResult res;
        QObject::connect(&op, &RedactOperation::finished,
                         [&res](const RedactResult& r) { res = r; });
        op.run();
        qInfo() << "redact outcome:" << int(res.outcome) << res.error;
        QVERIFY(res.outcome == RedactOutcome::Completed ||
                res.outcome == RedactOutcome::PartialRedactedOnly);

        PdfiumBackend backend;
        QVERIFY(backend.loadDocument(dest));
        dumpRuns("redacted-dest", backend, 0);
        const auto runs = backend.extractPageTextRuns(0);

        // The secret line must be GONE (excision worked)...
        for (const auto& r : runs)
            QVERIFY2(!r.text.contains(QLatin1String("TopSecretAlpha")),
                     "excision must remove the secret line (if this fails the "
                     "coordinate bug reaches the excision itself — CRITICAL)");
        // ...and the overlay label must exist.
        QString labelText;
        double baseline = -1e9;
        for (const auto& r : runs) {
            if (r.text.contains(QLatin1String("LABEL"))) {
                labelText = r.text;
                baseline = r.rect.y();
            }
        }
        QVERIFY2(baseline > -1e8, "overlay label run must exist in the output");
        qInfo() << "overlay label:" << labelText << "baseline (PDF user space) =" << baseline;

        // CORRECT position: inside the mark box [882 .. 912] user-space Y
        // (viewer mark y=130, height=30; viewer top = 1042). The Height-only
        // flip paints at [682 .. 712] — 200pt (the ignored origin) below.
        const double okLo = (kOffY + kOffH) - 130.0 - 30.0 - 6.0;  // 876
        const double okHi = (kOffY + kOffH) - 130.0 + 6.0;         // 918
        QVERIFY2(baseline >= okLo && baseline <= okHi,
                 QStringLiteral("SEP13 lead 8 CONFIRMED: overlay label baseline %1 landed "
                 "outside the mark's user-space band [%2..%3] — shifted by the "
                 "ignored MediaBox origin (%4); the Height-only flip painted "
                 "at ~[682..712]")
                     .arg(baseline, 0, 'f', 1)
                     .arg(okLo, 0, 'f', 1)
                     .arg(okHi, 0, 'f', 1)
                     .arg(kOffY, 0, 'f', 0)
                     .toUtf8()
                     .constData());
    }
};

#include "TestSep13LeadRedactionProof.moc"
QTEST_MAIN(TestSep13LeadRedactionProof)
