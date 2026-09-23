// SWEEP-W2 guarantee-verification probe — redaction-proof family (L5/L6/L7/L8 + F1 + T2).
// NOT a lane artifact. Written 2026-09-20 by the W2 verification wave.
//
// Independence vs TestSep13LeadRedactionProof / TestRedactionProof:
//   * Own fixture: /Rotate 90 + offset-origin MediaBox [0 200 612 1042], own
//     secrets (W2SecretQuartz0 content / W2AnnotCobalt9 annotation), own
//     viewer-mark math derived from the ISO 32000-1 §14.11.2.1 rotate-90
//     display law (viewer = (y - y0, x)) applied by THIS probe, not copied
//     from any lane test.
//   * FULL production path: RedactOperation::run with produceProof=true —
//     the pack files the UI would ship are exercised, not just the in-process
//     Result.
//   * SAVED artifacts judged through read paths NO lane test uses:
//       - poppler pdftotext (external binary, zero shared code)
//       - qpdf --qdf --object-streams=disable inflate + byte grep
//       - independent QCryptographicHash SHA-256 of the output bytes checked
//         against the pack JSON's outputSha256 binding.
//   * Both proof directions: honest FAIL over a surviving secret, honest PASS
//     over a really-excised artifact (a false FAIL is also dishonest).
#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QCryptographicHash>

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

constexpr double kX0 = 0.0, kY0 = 200.0, kW = 612.0, kH = 842.0;

bool copyFile(const QString& in, const QString& out) {
    QFile::remove(out);
    return QFile::copy(in, out);
}

// Offset-origin + /Rotate 90 page. Display law for /Rotate 90:
// viewer point = (userY - kY0, userX - kX0).
//  - "W2SecretQuartz0 content line" baseline user (100, 940), 12 pt Helvetica:
//    dy = 740, dx = 100 → viewer baseline (740, 100); glyph column viewer
//    x ∈ [728, 744], y ∈ [100, ~700].
//  - "KeepThisVisible public info" baseline user (100, 880) → viewer
//    baseline (680, 100) — clear of the secret column (max 692 < 726).
//  - FreeText annotation "W2AnnotCobalt9", raw /Rect [100, 700, 150x40]:
//    user x ∈ [100, 250], y ∈ [700, 740] → viewer x ∈ [500, 540],
//    y ∈ [100, 250].
QString makeRotatedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto& page = doc.GetPages().CreatePage(PoDoFo::Rect(kX0, kY0, kW, kH));
        page.GetDictionary().AddKey(PoDoFo::PdfName("Rotate"),
                                    PoDoFo::PdfObject(int64_t(90)));
        auto draw = [&](const char* text, double y) {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)(text, 100.0, y);
            painter.FinishDrawing();
        };
        draw("W2SecretQuartz0 content line", 940.0);
        draw("KeepThisVisible public info", 880.0);
        auto& annot = page.GetAnnotations().CreateAnnot(
            PoDoFo::PdfAnnotationType::FreeText,
            PoDoFo::Rect(100.0, 700.0, 150.0, 40.0));
        annot.SetContents(PoDoFo::PdfString("W2AnnotCobalt9"));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception& e) {
        qWarning() << "makeRotatedPdf failed:" << e.what();
        return {};
    }
}

QRectF contentViewerMark() {
    // viewer x = [940-200-12, 940-200+12] = [728, 752]; viewer y = [88, 760]
    return QRectF(726.0, 88.0, 28.0, 680.0);
}
QRectF annotViewerMark() { return QRectF(500.0, 100.0, 40.0, 150.0); }

int byteHits(const QString& pdfPath, const char* needle) {
    QFile f(pdfPath);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    const QByteArray all = f.readAll();
    int hits = all.count(needle);
    QByteArray hex;
    for (const char* p = needle; *p; ++p)
        hex += QByteArray::number(uchar(*p), 16).right(2);
    hits += all.count(hex);
    return hits;
}

QString runTool(const QString& program, const QStringList& args) {
    // Tool location comes from the environment (W2_PDFTOTEXT / W2_QPDF, set by
    // the verification runner) with a PATH fallback — the UCRT64 login shell
    // PATH does not carry the poppler/qpdf install dirs.
    static const QHash<QString, QString> envKeys = {
        { QStringLiteral("pdftotext"), QStringLiteral("W2_PDFTOTEXT") },
        { QStringLiteral("qpdf"), QStringLiteral("W2_QPDF") },
    };
    QString exe = program;
    if (envKeys.contains(program)) {
        const QString fromEnv =
            QString::fromLocal8Bit(qgetenv(envKeys.value(program).toLocal8Bit()));
        if (!fromEnv.isEmpty())
            exe = fromEnv;
    }
    QProcess p;
    p.start(exe, args);
    // Not QVERIFY2 — returns QString (early return only legal in void fns);
    // a tool that never starts/finishes yields "" and downstream asserts fail.
    if (!p.waitForStarted(10000) || !p.waitForFinished(60000)) {
        qWarning() << "tool failed to run:" << exe;
        return {};
    }
    return QString::fromUtf8(p.readAllStandardOutput()) +
           QString::fromUtf8(p.readAllStandardError());
}

QString sha256OfFile(const QString& path) {
    QFile f(path);
    // NOTE: not QVERIFY2 — this function returns QString (early return is
    // only legal in void functions). An unopenable file yields "" and the
    // caller's comparison fails.
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

class W2ProbeRedactProof : public QObject {
    Q_OBJECT

private slots:
    // Direction 1 (excision + honest PASS + pack bindings): the production
    // redaction over BOTH marks on the rotated page must excise both secrets
    // from the SAVED artifact (external read paths agree), keep the benign
    // line, and the produced proof pack must PASS with an outputSha256 equal
    // to an INDEPENDENT hash of the committed bytes.
    void productionRedactionExcisesAndPackPassesOnRotatedPage() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeRotatedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString dest = tmp.filePath("dest.pdf");

        RedactRequest rq;
        rq.sourcePath = src;
        rq.destinationPath = dest;
        rq.redactionsByPage[0] = { contentViewerMark(), annotViewerMark() };
        rq.overlayText = QStringLiteral("W2");
        rq.produceProof = true;

        RedactOperation op(rq);
        RedactResult res;
        QObject::connect(&op, &RedactOperation::finished,
                         [&res](const RedactResult& r) { res = r; });
        op.run();
        qInfo() << "outcome =" << int(res.outcome) << res.error;
        QCOMPARE(int(res.outcome), int(RedactOutcome::Completed));
        QVERIFY(QFile::exists(dest));
        QVERIFY(res.proofRan);
        qInfo() << "proofPassed =" << res.proofPassed << res.proofFailures;

        // ── external read path A: poppler pdftotext on the SAVED artifact ──
        const QString text = runTool(QStringLiteral("pdftotext"),
                                     QStringList{dest, QStringLiteral("-")});
        QVERIFY2(!text.contains(QLatin1String("W2SecretQuartz0")),
                 "poppler text extraction must not see the content secret");
        QVERIFY2(!text.contains(QLatin1String("W2AnnotCobalt9")),
                 "poppler text extraction must not see the annotation secret");
        QVERIFY2(text.contains(QLatin1String("KeepThisVisible")),
                 "the benign line must survive");

        // ── external read path B: qpdf QDF inflate + byte grep ──
        // Invariant: an independent parser (qpdf) can inflate the saved
        // artifact and NEITHER secret survives in the fully inflated bytes.
        // (The benign line is NOT asserted here: content-stream text may be
        // split across TJ kerning arrays, so ASCII containment is not a valid
        // survival invariant — poppler's decode-level check above covers it.)
        const QString qdf = tmp.filePath("inflated.pdf");
        QFile::remove(qdf);
        runTool(QStringLiteral("qpdf"),
                QStringList{QStringLiteral("--qdf"),
                            QStringLiteral("--object-streams=disable"),
                            dest, qdf});
        QVERIFY2(QFile::exists(qdf), "qpdf must accept the saved artifact");
        QCOMPARE(byteHits(qdf, "W2SecretQuartz0"), 0);
        QCOMPARE(byteHits(qdf, "W2AnnotCobalt9"), 0);

        // ── PoDoFo re-walk: the annotation object must be gone ──
        {
            PoDoFo::PdfMemDocument d;
            d.Load(dest.toUtf8().constData());
            QCOMPARE(int(d.GetPages().GetPageAt(0).GetAnnotations().GetCount()), 0);
        }

        // ── pack bindings: JSON parses, verdict honest, SHA-256 matches an
        //    independent hash of the committed bytes ──
        QVERIFY(!res.proofJsonPath.isEmpty());
        QVERIFY(QFile::exists(res.proofJsonPath));
        QFile jf(res.proofJsonPath);
        QVERIFY(jf.open(QIODevice::ReadOnly));
        QJsonDocument pack = QJsonDocument::fromJson(jf.readAll());
        QVERIFY2(!pack.isNull(), "proof pack must be valid JSON");
        const QJsonObject top = pack.object();
        QCOMPARE(top.value("format").toString(),
                 QLatin1String("glyphpdf-redaction-proof/1"));
        QVERIFY2(top.value("verdict").toString() == QLatin1String("PASS"),
                 "pack verdict must be PASS when the excision really removed "
                 "both secrets");
        QVERIFY2(!top.value("disclaimer").toString().isEmpty(),
                 "the pack must carry the honest-evidence disclaimer");
        QVERIFY2(res.proofPassed,
                 "with both secrets really excised the pack must PASS");
        const QString myHash = sha256OfFile(dest);
        QCOMPARE(top.value("files").toObject().value("output_sha256").toString(),
                 myHash);

        // ── in-process verify agrees with the pack (second path over the
        //    saved artifact) ──
        Request verifyReq;
        verifyReq.sourcePath = src;
        verifyReq.outputPath = dest;
        verifyReq.redactionsByPage[0] = rq.redactionsByPage[0];
        const Result proof = verify(verifyReq);
        QVERIFY(proof.proofRan);
        QVERIFY2(proof.proofPassed,
                 "independent verify over the saved artifact must PASS");
        bool contentAttributed = false, annotAttributed = false;
        for (const auto& e : proof.entries) {
            for (const auto& s : e.removedStrings) {
                contentAttributed |= s.contains(QLatin1String("W2SecretQuartz0"));
                annotAttributed |= s.contains(QLatin1String("W2AnnotCobalt9"));
            }
        }
        QVERIFY2(contentAttributed, "content secret must be attributed (L5 transform)");
        QVERIFY2(annotAttributed, "annotation secret must be attributed (L7/F1 raw /Rect)");
    }

    // Direction 2 (honest FAIL): untouched output + viewer marks → the proof
    // must attribute BOTH secrets and FAIL loudly — never certify
    // VerifiedNoTextInRegion over survivors on a rotated page (the L5+L7×L5
    // composed false-PASS class).
    void proofMustFailHonestlyOverSurvivingRotatedSecrets() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeRotatedPdf(tmp.filePath("src.pdf"));
        QVERIFY(!src.isEmpty());
        const QString out = tmp.filePath("out.pdf");
        QVERIFY(copyFile(src, out)); // output = untouched source, secrets live

        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        req.redactionsByPage[0] = { contentViewerMark(), annotViewerMark() };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        qInfo() << "honest-fail: passed =" << proof.proofPassed
                << "failures =" << proof.failureReasons;
        for (const auto& e : proof.entries)
            qInfo() << "entry page" << e.pageIndex << "removed =" << e.removedStrings;
        bool contentAttributed = false, annotAttributed = false;
        for (const auto& e : proof.entries) {
            for (const auto& s : e.removedStrings) {
                contentAttributed |= s.contains(QLatin1String("W2SecretQuartz0"));
                annotAttributed |= s.contains(QLatin1String("W2AnnotCobalt9"));
            }
        }
        QVERIFY2(contentAttributed,
                 "content secret must be attributed through this probe's own "
                 "rotate-90 viewer mark (L5)");
        QVERIFY2(annotAttributed,
                 "annotation-only secret must be attributed from a viewer mark "
                 "on the rotated page (L7/F1 raw /Rect)");
        QVERIFY2(!proof.proofPassed,
                 "the proof must FAIL while the secrets survive in the output");

        // A surface-level survivor must be NAMED (raw bytes at minimum).
        bool rawSurvivorNamed = false;
        for (const auto& s : proof.surfaces) {
            if (s.surface == Surface::RawBytes &&
                s.verdict == SurfaceVerdict::Survivor)
                rawSurvivorNamed = true;
        }
        QVERIFY2(rawSurvivorNamed,
                 "the raw-bytes surface must report the surviving secrets");
    }

    // Unrotated + unoffset control page: separates "transform broken in
    // general" from "/Rotate handling" — the trivial identity case must hold.
    void plainPageControl() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = tmp.filePath("plain.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::Rect(0.0, 0.0, 612.0, 792.0));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(font, 12.0);
            (painter.DrawText)("W2SecretQuartz0 content line", 100.0, 700.0);
            painter.FinishDrawing();
            doc.Save(src.toUtf8().constData());
        }
        const QString out = tmp.filePath("plain-out.pdf");
        QVERIFY(copyFile(src, out));
        Request req;
        req.sourcePath = src;
        req.outputPath = out;
        // VIEWER coordinates (top-left origin, y-down): user baseline
        // (100, 700) on a 612x792 page displays at viewer y = 792-700 = 92,
        // glyph box roughly [80, 94]. (First draft wrongly reused user space.)
        req.redactionsByPage[0] = { QRectF(88.0, 78.0, 400.0, 26.0) };
        const Result proof = verify(req);
        QVERIFY(proof.proofRan);
        QVERIFY2(!proof.entries.first().removedStrings.isEmpty(),
                 "control: attribution must work on a plain page");
        QVERIFY2(!proof.proofPassed,
                 "control: honest failure while the secret survives");
    }
};

#include "W2ProbeRedactProof.moc"
QTEST_MAIN(W2ProbeRedactProof)
