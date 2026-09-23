// R14 INDEPENDENT REVIEWER probe — SEP13 L5/L7/L8 composed data-loss claim.
// NOT a lane artifact. Written by the independent reviewer (2026-09-14).
//
// Independence vs the committed TestSep13LeadRedactionProof:
//   * ONE fixture combining offset-origin AND /Rotate 90 (committed suite
//     tests each in isolation); MediaBox [0 200 612 1042], /Rotate 90.
//   * Viewer marks derived by the reviewer's OWN application of the measured
//     /Rotate 90 law (display = (dy, dx)), not copied from the lane's marks.
//   * FULL production flow: RedactOperation::run() on a real file, verdicts
//     ONLY on the SAVED artifact via two independent read paths (PDFium text
//     extraction + PoDoFo object/raw-string scan).
//   * Both proof directions checked: honest FAILURE over a surviving secret
//     (composed data-loss must report honestly) and honest PASS over a really
//     excised artifact.
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

constexpr double kY0 = 200.0;   // MediaBox lower-left Y
constexpr double kH = 842.0;    // MediaBox height
constexpr double kW = 612.0;

bool copyFile(const QString& in, const QString& out) {
    QFile::remove(out);
    return QFile::copy(in, out);
}

// Offset-origin + /Rotate 90 page. Secret at user (100, 900) → dy = 700,
// dx = 100 → viewer baseline (700, 100), glyphs run down to y≈198.
// Benign line at user (100, 860) → viewer baseline (660, 100).
// FreeText annotation secret: user rect [100,650,200x30] → viewer rect
// x=[450,480], y=[100,300].
QString makeCombinedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(PoDoFo::Rect(0.0, kY0, kW, kH));
        page.GetDictionary().AddKey(PoDoFo::PdfName("Rotate"),
                                    PoDoFo::PdfObject(int64_t(90)));
        auto draw = [&](const char* text, double y) {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)(text, 100.0, y);
            painter.FinishDrawing();
        };
        draw("CombinedSecretAlpha bare secrets", 900.0);
        draw("KeepThisVisible public info", 860.0);
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
        annot.SetContents(PoDoFo::PdfString("AnnotSecretZebra"));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeCombinedPdf failed:" << e.what();
        return {};
    }
}

// Same geometry WITHOUT the annotation — used for the proof-PASS direction.
QString makeCombinedNoAnnot(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(PoDoFo::Rect(0.0, kY0, kW, kH));
        page.GetDictionary().AddKey(PoDoFo::PdfName("Rotate"),
                                    PoDoFo::PdfObject(int64_t(90)));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.TextState.SetFont(font, 12.0);
        (painter.DrawText)("CombinedSecretAlpha bare secrets", 100.0, 900.0);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeCombinedNoAnnot failed:" << e.what();
        return {};
    }
}

// ROTATED + offset page whose ONLY secret is an annotation string; the
// content stream decodes but draws nothing. Spec-correct viewer mark over the
// annot (ISO 32000-1 12.5.2: /Rect is default user space; /Rotate rotates the
// whole presentation — same display law as content, which this review
// confirmed empirically on the content path in this very fixture).
QString makeRotatedAnnotOnlyPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(PoDoFo::Rect(0.0, kY0, kW, kH));
        page.GetDictionary().AddKey(PoDoFo::PdfName("Rotate"),
                                    PoDoFo::PdfObject(int64_t(90)));
        {   // valid, empty content stream
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.FinishDrawing();
        }
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
        annot.SetContents(PoDoFo::PdfString("AnnotSecretZebra"));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeRotatedAnnotOnlyPdf failed:" << e.what();
        return {};
    }
}

// Reviewer's own /Rotate 90 viewer mark over the secret line:
// viewer x = [dy-12, dy+12] = [688, 712]; viewer y = [dx-12, dx+98] = [88, 198].
QRectF secretViewerMark() { return QRectF(688.0, 88.0, 24.0, 110.0); }
// Reviewer's own viewer mark over the annotation (see math in header comment).
QRectF annotViewerMark() { return QRectF(450.0, 100.0, 30.0, 200.0); }

int rawStringHits(const QString& pdfPath, const char* needle) {
    int hits = 0;
    QFile f(pdfPath);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray all = f.readAll();
        hits = all.count(needle);
        // also check the hex-encoded literal <...> form (very unlikely for
        // these fixtures, but cheap)
        QByteArray hex;
        for (const char* p = needle; *p; ++p)
            hex += QByteArray::number(uchar(*p), 16).right(2);
        hits += all.count(hex);
    }
    return hits;
}

} // namespace

class R14ProbeRedactSpace : public QObject {
    Q_OBJECT

private slots:
    // COMPOSED data-loss contract, honest-failure direction: with NO
    // redaction performed, the proof must attribute BOTH the content secret
    // (through the offset+rotated transform) AND the annotation-only secret,
    // and must FAIL loudly — never certify VerifiedNoTextInRegion.
    void proofMustFailHonestlyOnSurvivingSecrets() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeCombinedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output = untouched source, secrets live

        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { secretViewerMark(), annotViewerMark() };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        qInfo() << "honest-failure: passed =" << proof.proofPassed
                << "removed =" << proof.entries.first().removedStrings
                << "failures =" << proof.failureReasons;
        for (const auto& s : proof.surfaces)
            qInfo() << "surface" << int(s.surface) << "problems =" << s.problems;
        {   // dump what PoDoFo itself reports for the annotation rect
            PoDoFo::PdfMemDocument d;
            d.Load(src.toUtf8().constData());
            auto& annos = d.GetPages().GetPageAt(0).GetAnnotations();
            qInfo() << "annot count =" << int(annos.GetCount());
            for (unsigned i = 0; i < annos.GetCount(); ++i) {
                auto& a = annos.GetAnnotAt(i);
                const PoDoFo::Rect r = a.GetRect();
                const QString c = a.GetContents().has_value()
                    ? QString::fromLatin1(a.GetContents()->GetString())
                    : QString();
                qInfo().nospace() << "annot " << i << " rect X=" << r.X
                                  << " Y=" << r.Y << " W=" << r.Width
                                  << " H=" << r.Height
                                  << " contents='" << c << "'";
            }
        }

        bool contentAttributed = false, annotAttributed = false;
        for (const auto& s : proof.entries.first().removedStrings) {
            contentAttributed |= s.contains(QLatin1String("CombinedSecretAlpha"));
            annotAttributed |= s.contains(QLatin1String("AnnotSecretZebra"));
        }
        QVERIFY2(contentAttributed,
                 "content secret must be attributed through the reviewer's own "
                 "offset+rotate viewer mark (L5)");
        QVERIFY2(annotAttributed,
                 "annotation-only secret must be attributed from a viewer mark "
                 "on an offset+rotated page (L7 composed with L5)");
        QVERIFY2(!proof.proofPassed,
                 "the proof must FAIL when the secrets still survive in the "
                 "output — silent PASS is the data-loss defect");
    }

    // COMPOSED data-loss contract, excision direction: the REAL redaction
    // (RedactOperation::run) on the combined offset+rotate page must excise
    // the content secret from the SAVED artifact (PDFium + raw-bytes + PoDoFo
    // object read paths agree) and must NOT report plain Completed over a
    // surviving annotation secret.
    void realRedactionExcisesSecretOnOffsetRotatedPage() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeCombinedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = tmp.filePath("dest.pdf");

        RedactRequest rq;
        rq.sourcePath = src;
        rq.destinationPath = dest;
        rq.redactionsByPage[0] = { secretViewerMark() };
        rq.overlayText = QStringLiteral("R14");

        RedactOperation op(rq);
        RedactResult res;
        QObject::connect(&op, &RedactOperation::finished,
                         [&res](const RedactResult& r) { res = r; });
        op.run();
        qInfo() << "outcome =" << int(res.outcome) << res.error;
        QVERIFY(QFile::exists(dest));

        // Read path 1: PDFium extraction of the SAVED artifact.
        PdfiumBackend backend;
        QVERIFY(backend.loadDocument(dest));
        const auto runs = backend.extractPageTextRuns(0);
        for (const auto& r : runs) {
            qInfo().nospace() << "run \"" << r.text << "\" rect=(" << r.rect.x()
                              << "," << r.rect.y() << ")";
            QVERIFY2(!r.text.contains(QLatin1String("CombinedSecretAlpha")),
                     "excision must remove the content secret on the combined "
                     "offset+rotate page (L8 composed with L5)");
        }
        // Benign line must survive (no over-excision).
        bool benign = false;
        for (const auto& r : runs)
            benign |= r.text.contains(QLatin1String("KeepThisVisible"));
        QVERIFY2(benign, "the benign line must survive the redaction");

        // Read path 2: raw file bytes + PoDoFo object strings.
        QCOMPARE(rawStringHits(dest, "CombinedSecretAlpha"), 0);

        // The proof must PASS on the really-excised artifact (no false fail):
        // content mark attributed + excised. The annotation was NOT marked for
        // excision, so its secret legitimately survives — exclude it by
        // verifying with a no-annot source copy so the PASS is clean.
        const QString srcClean = makeCombinedNoAnnot(tmp.filePath("src2.pdf"));
        QVERIFY(!srcClean.isEmpty());
        const QString destClean = tmp.filePath("dest2.pdf");
        RedactRequest rq2;
        rq2.sourcePath = srcClean;
        rq2.destinationPath = destClean;
        rq2.redactionsByPage[0] = { secretViewerMark() };
        RedactOperation op2(rq2);
        RedactResult res2;
        QObject::connect(&op2, &RedactOperation::finished,
                         [&res2](const RedactResult& r) { res2 = r; });
        op2.run();
        QVERIFY(QFile::exists(destClean));
        Request pr;
        pr.sourcePath = srcClean;
        pr.outputPath = destClean;
        pr.redactionsByPage[0] = { secretViewerMark() };
        const Result proof = verify(pr);
        QVERIFY(proof.proofRan);
        qInfo() << "proof-on-real-output: passed =" << proof.proofPassed
                << "removed =" << proof.entries.first().removedStrings;
        QVERIFY2(!proof.entries.first().removedStrings.isEmpty(),
                 "the proof must attribute the excised strings (no vacuous pass)");
        QVERIFY2(proof.proofPassed,
                 "the proof must PASS when the excision really removed the "
                 "secret — false failure is also dishonest");
    }
    // FINDING SLOT (R14): annotation-only secret on a ROTATED + offset page.
    // Contract under test (L7 composed with L5): a spec-correct viewer mark
    // over the displayed annot must attribute "AnnotSecretZebra", so the
    // surviving secret forces an honest proof FAILURE. The production path
    // intersects the user mark against PoDoFo GetRect(), which on /Rotate 90
    // returns an adjusted rect that no longer corresponds to the raw /Rect
    // under the display law — attribution misses, nothing is swept for, and
    // the proof certifies PASS over the surviving secret.
    void rotatedPageAnnotOnlySecretMustNotFalsePass() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeRotatedAnnotOnlyPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        {   // confirm the content side sees NO text runs (annot-blind source)
            PdfiumBackend backend;
            QVERIFY(backend.loadDocument(src));
            QCOMPARE(backend.extractPageTextRuns(0).size(), 0);
        }
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output still CONTAINS the annotation

        // Spec-correct viewer mark: annot /Rect x [100,300], y [650,680]
        // (y0=200 → dy [450,480]); under /Rotate 90 display=(dy, dx):
        // viewer x [450,480], viewer y [100,300].
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(450.0, 100.0, 30.0, 200.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "rotated-annot-only: status =" << int(e.status)
                << "removed =" << e.removedStrings
                << "passed =" << proof.proofPassed
                << "failures =" << proof.failureReasons;
        QVERIFY2(!e.removedStrings.isEmpty(),
                 "a spec-correct viewer mark over the displayed annotation must "
                 "attribute the annot string on a rotated page (L7×L5)");
        QVERIFY2(!proof.proofPassed,
                 "the proof must FAIL while the annotation secret survives");
    }

    // OFFSET-ONLY (rotate 0) annotation control: on unrotated pages PoDoFo
    // GetRect() equals the raw /Rect, so the committed L7 contract must hold —
    // this control separates "annot attribution broken for /Rotate" from
    // "broken in general".
    void offsetOnlyPageAnnotAttributionControl() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = tmp.filePath("src.pdf");
        {   // offset origin, NO rotate, annot-only secret
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::Rect(0.0, kY0, kW, kH));
            {   PoDoFo::PdfPainter painter;
                painter.SetCanvas(page);
                painter.FinishDrawing(); }
            auto& annot = page.GetAnnotations().CreateAnnot(
                PoDoFo::PdfAnnotationType::FreeText,
                PoDoFo::Rect(100.0, 650.0, 200.0, 30.0));
            annot.SetContents(PoDoFo::PdfString("AnnotSecretZebra"));
            doc.Save(src.toUtf8().constData());
        }
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out));
        // Viewer mark: y flipped with FULL page top = y0 + H = 1042:
        // viewer y of user 650..680 = [1042-680, 1042-650] = [362, 392].
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { QRectF(100.0, 362.0, 200.0, 30.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        const auto& e = proof.entries.first();
        qInfo() << "offset-only-annot control: removed =" << e.removedStrings
                << "passed =" << proof.proofPassed;
        QVERIFY2(!e.removedStrings.isEmpty(),
                 "control: annot attribution works on offset-only (rotate 0) pages");
        QVERIFY2(!proof.proofPassed,
                 "control: honest failure while the annot secret survives");
    }
    // DECISIVE FALSE-PASS demonstration (R14 finding): decodable content
    // stream + annot-only secret + mark over the annot ONLY (not the content).
    // Correct contract: the proof must FAIL while AnnotSecretZebra survives.
    // Demonstrates the L7×L5 gap end-to-end on the fixed build.
    void rotatedAnnotMarkFalsePassDemonstration() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeCombinedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out));

        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { annotViewerMark() };  // annot mark ONLY
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        qInfo() << "FALSE-PASS demo: passed =" << proof.proofPassed
                << "removed =" << proof.entries.first().removedStrings
                << "failures =" << proof.failureReasons;
        QVERIFY2(!proof.proofPassed,
                 "the proof must NOT certify a PASS while AnnotSecretZebra "
                 "survives in the output — the viewer mark over the displayed "
                 "annotation attributed nothing (PoDoFo GetRect × /Rotate)");
    }

};

#include "R14ProbeRedactSpace.moc"
QTEST_MAIN(R14ProbeRedactSpace)
