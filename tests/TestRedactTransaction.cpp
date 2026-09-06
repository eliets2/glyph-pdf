// SPDX-License-Identifier: Apache-2.0
// U05 — Make redaction output and partial failure explicit.
//
// One transactional redaction operation (gp::RedactOperation) behind BOTH entry
// paths, built on the R01 safe-save primitives (gp::SafeSave, extracted from
// FormManager.cpp). This file pins the transaction contract:
//
//   * The source PDF's bytes (SHA-256) are identical after EVERY terminal state
//     that is not a successful commit — candidate-save failure, validation
//     failure, commit failure (via the fault seam, exercising QSaveFile's
//     cancel path), cancel between pages, engine failure after one page, and
//     sanitize failure.
//   * Success is proven against the COMMITTED artifact with an independent
//     extractor (PdfiumBackend): the secret is gone from the output while
//     non-marked text survives (guards against a vacuous empty-extraction pass).
//   * A pre-existing destination is replaced atomically on success and left
//     byte-identical on a failed commit.
//   * The ER-2 signed-file refusal fires at Preflight, before any write.
//   * Sanitize failure yields PartialRedactedOnly (labeled partial state, never
//     a generic success banner), with a working Retry-sanitize seam.
//   * The pre-mutation dialog carries the summary ("N marks on M pages"),
//     destination defaults, and refuses a destination equal to the source.
//   * The shared result-presenter text is explicit for every outcome — the
//     partial-failure wording never claims success.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <QPushButton>
#include <QLabel>
#include <podofo/podofo.h>

#include "engines/PdfEditorEngine.h"
#include "engines/RedactOperation.h"
#include "engines/SafeSave.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/SignatureManager.h"
#include "modes/RedactApplyDialog.h"
#include "core/AnnotationTypes.h"

// Windows headers (pulled in transitively by the pdfium/OpenSSL headers) define
// `#define DrawText DrawTextW`, which would rewrite the PoDoFo painter calls below.
#ifdef DrawText
#undef DrawText
#endif

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kP12Path  = kFixtureDir + "/test_signer.p12";
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";
static const QString kCaPath   = kFixtureDir + "/test_ca.pem";
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf) || !QFileInfo::exists(kCaPath)) { \
            QSKIP("Signing fixtures missing — skipping signed-redaction guard test. " \
                  "Run cmake -P tests/fixtures/signing/generate_fixtures.cmake to create them."); \
        } \
    } while(0)

using namespace gp;

namespace {

QByteArray sha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

QString pageText(const QString& pdfPath, int page) {
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return QString();
    return backend.extractText(page);
}

QString errText(const RedactResult& r) {
    return QStringLiteral("outcome=%1 stage=%2 error=%3")
        .arg(int(r.outcome)).arg(r.failedStage, r.error);
}

} // namespace

class TestRedactTransaction : public QObject {
    Q_OBJECT
private slots:
    void init() {
        // Isolate the static fault seam between tests (mirrors FormManager's seam).
        RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    }
    void initTestCase() {
        qRegisterMetaType<gp::RedactResult>("gp::RedactResult");
        qRegisterMetaType<gp::RedactStage>("gp::RedactStage");
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── Success ────────────────────────────────────────────────────────────
    void successCommitsRedactedOutputSourceByteIdentical();

    // ── Failure / cancel keeps the source byte-identical ───────────────────
    void candidateSaveFailureLeavesSourceAndDestinationIntact();
    void validationFailureLeavesSourceAndDestinationIntact();
    void commitFailureLeavesPreExistingDestinationByteIdentical();
    void engineFailureAfterOnePageFailsAtRedacting();
    void cancelBetweenPagesWritesNothing();
    void sanitizeFailureYieldsPartialRedactedOnly();

    // ── ER-2 signed-file refusal at Preflight ──────────────────────────────
    void signedDocumentIsRefusedInPreflight();

    // ── Destination semantics ──────────────────────────────────────────────
    void existingDestinationIsReplacedOnSuccess();

    // ── SafeSave primitives (R01 extraction) ───────────────────────────────
    void safeSaveCandidatePathsAreUnique();
    void safeSaveCommitReplacesDestinationAndFaultLeavesItIntact();

    // ── Pre-mutation dialog + shared result presenter ──────────────────────
    void dialogPresentsSummaryDefaultsAndRefusesSourceDestination();
    void presenterTextIsExplicitForEveryOutcome();

private:
    QTemporaryDir m_tmpDir;

    QString createPdf(const QString& name, int pageCount,
                      const QString& secretPrefix = QStringLiteral("TOPSECRET_DATA"),
                      bool risky = false);
    RedactRequest makeRequest(const QString& src, const QString& dest,
                              const QList<int>& pages, bool sanitize,
                              const QString& sanitizedDest = QString());
    RedactResult runOp(RedactOperation* op);
};

// Draws `pageCount` pages; each page i carries "<secretPrefix>_PAGE<i>" at
// (50,700) and "PUBLIC_KEEP_TEXT" at (50,650). When `risky`, adds OpenAction
// JS + catalog XMP so the sanitize pass has hidden data to strip (the
// TestRedactSanitizeBundle fixture idiom).
QString TestRedactTransaction::createPdf(const QString& name, int pageCount,
                                         const QString& secretPrefix, bool risky) {
    const QString path = m_tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int p = 0; p < pageCount; ++p) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            const QByteArray secret = QStringLiteral("%1_PAGE%2").arg(secretPrefix).arg(p).toUtf8();
            // Parenthesized: the Win32 macro `#define DrawText DrawTextW`
            // (transitively included via Qt/Windows headers) must not rewrite
            // the PoDoFo painter call.
            (painter.DrawText)(secret.constData(), 50, 700);
            (painter.DrawText)("PUBLIC_KEEP_TEXT", 50, 650);
            painter.FinishDrawing();
        }
        if (risky) {
            auto& cat = doc.GetCatalog().GetDictionary();
            PoDoFo::PdfDictionary oa;
            oa.AddKey("S", PoDoFo::PdfName("JavaScript"));
            oa.AddKey("JS", PoDoFo::PdfString("app.alert(1);"));
            cat.AddKey("OpenAction", PoDoFo::PdfObject(oa));
            cat.AddKey("Metadata", PoDoFo::PdfObject(PoDoFo::PdfString("<x:xmpmeta/>")));
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "fixture creation failed:" << e.what();
        return QString(); // callers QVERIFY2 on non-empty
    }
    return path;
}

RedactRequest TestRedactTransaction::makeRequest(const QString& src, const QString& dest,
                                                 const QList<int>& pages, bool sanitize,
                                                 const QString& sanitizedDest) {
    RedactRequest req;
    req.sourcePath = src;
    req.destinationPath = dest;
    for (int p : pages)
        // Marks use the viewer/top-down coordinate convention the engine applies
        // (PoDoFoBackend converts with pageHeight - y - height). Text drawn at
        // PDF (50,700) sits at ~142 from the top, so this band covers it.
        req.redactionsByPage[p].append(QRectF(40, 130, 300, 30));
    req.sanitize = sanitize;
    req.sanitizedDestinationPath = sanitizedDest;
    return req;
}

RedactResult TestRedactTransaction::runOp(RedactOperation* op) {
    RedactResult captured;
    connect(op, &RedactOperation::finished, op,
            [&captured](const RedactResult& r) { captured = r; });
    op->run(); // synchronous execution on the test thread (worker entry point)
    return captured;
}

// ── Success: the committed artifact is redacted, the source is untouched ────
void TestRedactTransaction::successCommitsRedactedOutputSourceByteIdentical() {
    const QString src = createPdf("success.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    QVERIFY(!srcSha.isEmpty());
    const QString dest = m_tmpDir.filePath("success_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("success_redacted_sanitized.pdf");

    RedactOperation op(makeRequest(src, dest, {0, 1}, /*sanitize=*/true, sanitized));
    QSignalSpy stages(&op, &RedactOperation::stageChanged);

    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));
    QCOMPARE(r.destination, dest);
    QCOMPARE(r.sanitizedDestination, sanitized);
    QCOMPARE(r.pagesTotal, 2);
    QCOMPARE(r.pagesProcessed, 2);
    QVERIFY(r.error.isEmpty());

    // Stage machine walked every stage in order, ending at Done. Redacting
    // legitimately emits per-page progress (stageChanged is the progress
    // contract), so collapse consecutive same-stage emissions first.
    QList<int> seen;
    for (const auto& call : stages) {
        const int s = static_cast<int>(call.at(0).value<RedactStage>());
        if (seen.isEmpty() || seen.last() != s) seen << s;
    }
    const QList<int> expected{int(RedactStage::Preflight), int(RedactStage::Redacting),
                              int(RedactStage::SavingCandidate), int(RedactStage::Validating),
                              int(RedactStage::Committing), int(RedactStage::Sanitizing),
                              int(RedactStage::Done)};
    QStringList seenStr;
    for (int s : seen) seenStr << QString::number(s);
    QVERIFY2(seen == expected,
             qPrintable(QStringLiteral("stage order mismatch, seen=[%1] expected=[0,1,2,3,4,5,6]")
                            .arg(seenStr.join(QLatin1Char(',')))));

    // Independent extractor: the secret is GONE from both committed pages,
    // the non-marked text SURVIVES (proves extraction is not vacuously empty).
    for (int p = 0; p < 2; ++p) {
        const QString text = pageText(dest, p);
        QVERIFY2(!text.isEmpty(), "committed output must carry extractable text");
        QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
                 qPrintable(QStringLiteral("secret survived on page %1: %2").arg(p).arg(text)));
        QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")),
                 qPrintable(QStringLiteral("public text lost on page %1").arg(p)));
    }

    // The sanitized artifact exists and is loadable.
    QVERIFY(QFileInfo::exists(sanitized));
    PoDoFo::PdfMemDocument sanitizedDoc;
    sanitizedDoc.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts

    // The source is byte-identical.
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::candidateSaveFailureLeavesSourceAndDestinationIntact() {
    const QString src = createPdf("cand.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cand_redacted.pdf");
    const QByteArray preExisting = "PRE-EXISTING-DESTINATION";
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::CandidateSave);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("SavingCandidate"));
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!r.destination.isEmpty() == false);
    // Destination byte-identical, source byte-identical.
    QCOMPARE(sha256(dest), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::validationFailureLeavesSourceAndDestinationIntact() {
    const QString src = createPdf("val.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("val_redacted.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Validation);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Validating"));
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest)); // no output was committed
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::commitFailureLeavesPreExistingDestinationByteIdentical() {
    const QString src = createPdf("commit.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("commit_redacted.pdf");
    const QByteArray preExisting = "PRE-EXISTING-DESTINATION-BYTES";
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    // The Commit fault fires inside SafeSave::commitFileToDestination AFTER the
    // bounded copy, exercising QSaveFile::cancelWriting — the exact path a real
    // commit failure (open handle, full disk) takes.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Commit);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Committing"));
    QVERIFY2(r.error.contains(QLatin1String("commit")), qPrintable(r.error));
    QCOMPARE(sha256(dest), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::engineFailureAfterOnePageFailsAtRedacting() {
    const QString src = createPdf("engfail.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("engfail_redacted.pdf");

    // Fault::Redact fails the SECOND page's applyRedactions — the plan's
    // "engine failure after one page" acceptance case. The live session must
    // show nothing (the operation mutates only its disposable private engine),
    // the source must be byte-identical, and no destination may appear.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Redact);
    RedactOperation op(makeRequest(src, dest, {0, 1, 2}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Redacting"));
    QCOMPARE(r.pagesProcessed, 1); // exactly one page applied before the failure
    QCOMPARE(r.pagesTotal, 3);
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::cancelBetweenPagesWritesNothing() {
    const QString src = createPdf("cancel.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cancel_redacted.pdf");

    RedactOperation op(makeRequest(src, dest, {0, 1, 2}, false));
    // Cancel at the first page boundary (after 1 page applied in memory).
    op.setPageBoundaryHook([&op](int pagesDone) {
        if (pagesDone == 1) op.cancel();
    });
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Canceled);
    QCOMPARE(r.pagesProcessed, 1);
    QVERIFY(r.destination.isEmpty());
    QVERIFY(!QFileInfo::exists(dest)); // no output written
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::sanitizeFailureYieldsPartialRedactedOnly() {
    // The source carries hidden data (OpenAction JS + XMP) so the sanitize
    // stage has real work to fail.
    const QString src = createPdf("partial.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("partial_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("partial_redacted_sanitized.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);

    // The labeled partial state — NOT Failed, NOT Completed.
    QCOMPARE(r.outcome, RedactOutcome::PartialRedactedOnly);
    QCOMPARE(r.failedStage, QStringLiteral("Sanitizing"));
    QVERIFY2(r.error.contains(QLatin1String("sanitiz"), Qt::CaseInsensitive), qPrintable(r.error));

    // The redacted artifact IS committed and valid; the sanitized copy is not.
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(r.destination, dest);
    QVERIFY(!QFileInfo::exists(sanitized));
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));

    // Source still byte-identical.
    QCOMPARE(sha256(src), srcSha);

    // Retry-sanitize seam: re-runs ONLY the Sanitizing stage over the COMMITTED
    // redacted file. The hidden data must be stripped from the result.
    QString retryErr;
    QVERIFY2(RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &retryErr),
             qPrintable(retryErr));
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData());
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));
    QVERIFY(!cat.HasKey("Metadata"));
}

void TestRedactTransaction::signedDocumentIsRefusedInPreflight() {
    REQUIRE_FIXTURES();

    QString signedPdf = m_tmpDir.filePath("signed_for_redact.pdf");
    SignatureManager mgr;
    QVERIFY2(mgr.signDocument(kInputPdf, signedPdf, kP12Path, kP12Pass,
                              "RedactTransactionTest", "") == SignOutcome::Success,
             "signDocument should succeed with valid P12");
    const QByteArray srcSha = sha256(signedPdf);

    const QString dest = m_tmpDir.filePath("signed_redacted.pdf");
    RedactOperation op(makeRequest(signedPdf, dest, {0}, false));
    const RedactResult r = runOp(&op);

    // ER-2: refuse at Preflight, before any write.
    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Preflight"));
    QVERIFY2(r.error.contains(QLatin1String("signed"), Qt::CaseInsensitive),
             qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest));
    QCOMPARE(sha256(signedPdf), srcSha);
}

void TestRedactTransaction::existingDestinationIsReplacedOnSuccess() {
    const QString src = createPdf("replace.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("replace_redacted.pdf");
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("STALE-OLD-CONTENT");
    }

    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(!text.contains(QLatin1String("STALE-OLD-CONTENT")), "old bytes must be gone");
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));
}

// ── SafeSave primitives (extracted R01) ─────────────────────────────────────
void TestRedactTransaction::safeSaveCandidatePathsAreUnique() {
    QString a, b, err;
    QVERIFY(SafeSave::makeUniqueCandidate(&a, &err));
    QVERIFY(SafeSave::makeUniqueCandidate(&b, &err));
    QVERIFY(!a.isEmpty() && !b.isEmpty());
    QVERIFY2(a != b, "candidate paths must be unique");
    QVERIFY2(a.endsWith(QLatin1String(".pdf")), qPrintable(a));
}

void TestRedactTransaction::safeSaveCommitReplacesDestinationAndFaultLeavesItIntact() {
    const QString candidate = m_tmpDir.filePath("ss_candidate.pdf");
    const QByteArray contentA = "CANDIDATE-BYTES-A";
    {
        QFile f(candidate); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(contentA);
    }
    const QString dest = m_tmpDir.filePath("ss_dest.pdf");

    QString err;
    QVERIFY2(SafeSave::commitFileToDestination(candidate, dest, &err), qPrintable(err));
    QCOMPARE(QFile(dest).size(), qint64(contentA.size()));

    // Replace an existing destination (overwrite semantics).
    const QByteArray contentB = "CANDIDATE-BYTES-B-VERY-DIFFERENT";
    {
        QFile f(candidate); QVERIFY(f.open(QIODevice::WriteOnly)); f.resize(0); f.write(contentB);
    }
    QVERIFY2(SafeSave::commitFileToDestination(candidate, dest, &err), qPrintable(err));
    QCOMPARE(QFile(dest).size(), qint64(contentB.size()));

    // Injected commit failure: the destination stays byte-identical.
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const bool ok = SafeSave::commitFileToDestination(candidate, dest, &err);
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!ok, "injected commit fault must fail the commit");
    QVERIFY2(!err.isEmpty(), "injected commit fault must report an error");
    QCOMPARE(QFile(dest).size(), qint64(contentB.size()));
}

// ── Pre-mutation dialog ─────────────────────────────────────────────────────
void TestRedactTransaction::dialogPresentsSummaryDefaultsAndRefusesSourceDestination() {
    RedactApplyPlan plan;
    plan.sourcePath = m_tmpDir.filePath("doc.pdf");
    plan.destinationPath = m_tmpDir.filePath("doc_redacted.pdf");
    plan.sanitizedDestinationPath = m_tmpDir.filePath("doc_redacted_sanitized.pdf");
    plan.markCount = 3;
    plan.marksPerPage = {{0, 2}, {2, 1}};
    plan.sourcePageCount = 4;
    plan.sanitize = false;

    RedactApplyDialog dlg(plan);
    dlg.show(); // offscreen platform

    // Summary: "N marks on M pages", the source page count, and the
    // "marked for removal" wording (distinct from "applied to saved output").
    const QString summary = dlg.summaryText();
    QVERIFY2(summary.contains(QLatin1String("3 marks on 2 pages")), qPrintable(summary));
    QVERIFY2(summary.contains(QLatin1String("4 pages")), qPrintable(summary));
    QVERIFY2(summary.contains(QLatin1String("marked for removal")), qPrintable(summary));

    // Defaults preserved: <base>_redacted.pdf / <base>_redacted_sanitized.pdf.
    QCOMPARE(dlg.plan().destinationPath, plan.destinationPath);
    QCOMPARE(dlg.plan().sanitizedDestinationPath, plan.sanitizedDestinationPath);
    QCOMPARE(dlg.plan().sanitize, false);
    QCOMPARE(dlg.plan().markCount, 3);

    // The sanitize choice and destination are user-adjustable.
    dlg.setSanitizeChecked(true);
    QCOMPARE(dlg.plan().sanitize, true);
    dlg.setDestinationPath(m_tmpDir.filePath("elsewhere.pdf"));
    QCOMPARE(dlg.plan().destinationPath, m_tmpDir.filePath("elsewhere.pdf"));

    // A destination equal to the SOURCE must be refused (the old controller
    // path saved over the original — U05 forbids that).
    dlg.setDestinationPath(plan.sourcePath);
    auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("redactApplyOkButton"));
    auto* warn = dlg.findChild<QLabel*>(QStringLiteral("redactApplyWarningLabel"));
    QVERIFY(ok);
    QVERIFY2(!ok->isEnabled(), "OK must be disabled when the destination equals the source");
    QVERIFY2(warn && !warn->text().isEmpty(), "a reason must be shown");
}

// ── Shared result presenter text (never a generic banner) ───────────────────
void TestRedactTransaction::presenterTextIsExplicitForEveryOutcome() {
    const QString dest = QStringLiteral("/out/secret_redacted.pdf");
    const QString sanitized = QStringLiteral("/out/secret_redacted_sanitized.pdf");

    // Completed: names BOTH committed artifacts.
    RedactResult done;
    done.outcome = RedactOutcome::Completed;
    done.destination = dest;
    done.sanitizedDestination = sanitized;
    const QString doneText = RedactResultPresenter::bannerText(done);
    QVERIFY2(doneText.contains(QLatin1String("secret_redacted.pdf")), qPrintable(doneText));
    QVERIFY2(doneText.contains(QLatin1String("secret_redacted_sanitized.pdf")), qPrintable(doneText));
    QVERIFY2(!doneText.contains(QLatin1String("failed"), Qt::CaseInsensitive),
             qPrintable(doneText));

    // PartialRedactedOnly: labeled partial state — names the redacted file,
    // states the sanitization failure, and NEVER claims success.
    RedactResult partial;
    partial.outcome = RedactOutcome::PartialRedactedOnly;
    partial.destination = dest;
    partial.failedStage = QStringLiteral("Sanitizing");
    partial.error = QStringLiteral("injected sanitize failure (test seam)");
    const QString partialText = RedactResultPresenter::detailText(partial);
    QVERIFY2(partialText.contains(QLatin1String("secret_redacted.pdf")), qPrintable(partialText));
    QVERIFY2(partialText.contains(QLatin1String("sanitiz"), Qt::CaseInsensitive),
             qPrintable(partialText));
    QVERIFY2(!partialText.contains(QLatin1String("successfully")),
             qPrintable(QStringLiteral("partial text must never claim success: %1")
                            .arg(partialText)));

    // Failed: states the original was not modified.
    RedactResult failed;
    failed.outcome = RedactOutcome::Failed;
    failed.failedStage = QStringLiteral("SavingCandidate");
    failed.error = QStringLiteral("disk full");
    const QString failedText = RedactResultPresenter::detailText(failed);
    QVERIFY2(failedText.contains(QLatin1String("not modified"), Qt::CaseInsensitive),
             qPrintable(failedText));
    QVERIFY2(failedText.contains(QLatin1String("disk full")), qPrintable(failedText));

    // Canceled: no output written, marks preserved.
    RedactResult canceled;
    canceled.outcome = RedactOutcome::Canceled;
    const QString canceledText = RedactResultPresenter::detailText(canceled);
    QVERIFY2(canceledText.contains(QLatin1String("No output")), qPrintable(canceledText));
}

QTEST_MAIN(TestRedactTransaction)
#include "TestRedactTransaction.moc"
