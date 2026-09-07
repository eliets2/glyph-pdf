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
#include <QApplication>
#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
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

// ── Modal driver: auto-answers RedactResultPresenter::present() ─────────────
// present() runs nested QMessageBox::exec() loops. A repeating 1ms timer fires
// inside those loops, clicks the requested button by text, and records every
// box text it saw. A stall fallback default-clicks any unexpected box so a
// contract change can never hang the run — the test fails on its assertions
// instead. stop() after each present() keeps consecutive drivers independent.
class ModalDriver {
public:
    ModalDriver(QObject* owner, QStringList buttons) : m_remaining(std::move(buttons)) {
        m_timer.setParent(owner);
        m_timer.setInterval(1);
        QObject::connect(&m_timer, &QTimer::timeout, owner, [this]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            if (m_remaining.isEmpty() || ++m_stall > 3000) {
                m_boxTexts << box->text() + QStringLiteral(" [default-clicked]");
                m_stall = 0;
                if (auto* d = box->defaultButton()) { d->click(); return; }
                if (!box->buttons().isEmpty()) box->buttons().first()->click();
                return;
            }
            m_boxTexts << box->text();
            for (QAbstractButton* b : box->buttons()) {
                const QString t = b->text();
                const int idx = m_remaining.indexOf(t);
                const int idxMnemonic =
                    t.startsWith(QLatin1Char('&')) ? m_remaining.indexOf(t.mid(1)) : -1;
                if (idx >= 0 || idxMnemonic >= 0) {
                    m_remaining.removeAt(idx >= 0 ? idx : idxMnemonic);
                    m_stall = 0;
                    b->click();
                    return;
                }
            }
        });
        m_timer.start();
    }
    void stop() { m_timer.stop(); }
    QStringList boxTexts() const { return m_boxTexts; }

private:
    QStringList m_remaining;
    QStringList m_boxTexts;
    int m_stall = 0;
    QTimer m_timer;
};

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

    // ── D05: the SANITIZED copy gets the same safe-replacement boundary ─────
    // sanitizeCommittedFile (the transaction's Sanitizing stage AND the
    // presenter's Retry-sanitize recovery) must sanitize into an
    // operation-owned candidate and commit it through the checked QSaveFile
    // boundary — never write/delete the destination directly.
    void sanitizeStageFaultLeavesPreExistingSanitizedDestinationIntact();
    void sanitizeCommitFaultLeavesExistingSanitizedDestinationByteIdentical();
    void sanitizeReplacesExistingSanitizedDestinationOnSuccess();

    // ── D01-family retry recovery, driven through the SHARED presenter ──────
    // The presenter's Retry-sanitize is the real user recovery path; these
    // tests drive the actual modal flow (button clicks) at that boundary.
    void retryThroughPresenterSucceedsFromCommittedRedactedFile();
    void retryThroughPresenterFailsAgainKeepsRedactedArtifactAndMarks();
    // D01: the partial result must preserve the INTENDED sanitize destination
    // (separately from the committed one) so Retry targets the requested path.
    void partialResultCarriesIntendedSanitizeDestination();

    // ── V01/V02 bounded pins: persistence + marks/history of the redaction flow
    // A cancel requested during Sanitizing must NOT discard the already-
    // committed artifacts (they are reported honestly instead), and the marks
    // decision contract must keep marks recoverable on every failure/cancel.
    void cancelDuringSanitizingStageKeepsCommittedArtifacts();
    void presenterMarkDecisionsPinRecoveryContract();

    // ── SafeSave primitives (R01 extraction) ───────────────────────────────
    void safeSaveCandidatePathsAreUnique();
    void safeSaveCommitReplacesDestinationAndFaultLeavesItIntact();

    // ── Pre-mutation dialog + shared result presenter ──────────────────────
    void dialogPresentsSummaryDefaultsAndRefusesSourceDestination();
    void presenterTextIsExplicitForEveryOutcome();

    // ── §9.8 P1: optional overlay text on the burn-in boxes ────────────────
    // Legal/FOIA users expect to see WHY something was redacted printed on the
    // box. Overlay is burn-in paint only — excision semantics stay untouched;
    // an empty overlay must preserve the current behavior exactly.
    void overlayTextIsPrintedOnBurnedInBoxes();
    void emptyOverlayTextPreservesCurrentBehavior();
    void overlaySkippedWhenBoxTooSmall();

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
    // redacted file (D05: through candidate + validation + checked commit).
    // The injected fault was a one-shot condition of the first attempt — the
    // real Retry runs without it, so clear the seam first.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    QString retryErr;
    QVERIFY2(RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &retryErr),
             qPrintable(retryErr));
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData());
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));
    QVERIFY(!cat.HasKey("Metadata"));
}

// ── D05: safe replacement at the sanitize boundary ──────────────────────────
// The sanitize pass is a SECOND output of the same transaction: replacing an
// existing sanitized destination must keep the original bytes intact on every
// failure and atomically replace them on success. (The redacted artifact and
// the source document must survive every one of these failures untouched.)

// Candidate-stage failure (Fault::Sanitize fires before any engine write):
// the pre-existing sanitized destination is never touched.
void TestRedactTransaction::sanitizeStageFaultLeavesPreExistingSanitizedDestinationIntact() {
    const QString src = createPdf("sfadeault.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("sfault_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfault_redacted_sanitized.pdf");
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::PartialRedactedOnly);
    // Every artifact boundary holds: redacted committed, sanitized destination
    // byte-identical, source byte-identical.
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(sha256(sanitized), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

// Commit-stage failure (injected at the exact QSaveFile::cancelWriting seam):
// the pre-existing sanitized destination stays byte-identical (SHA-256).
void TestRedactTransaction::sanitizeCommitFaultLeavesExistingSanitizedDestinationByteIdentical() {
    const QString src = createPdf("sfcommit.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("sfcommit_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfcommit_redacted_sanitized.pdf");

    // First run completes and produces the real sanitized copy.
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Re-seed the destination with known bytes: it is now a PRE-EXISTING file
    // that a second sanitize pass must replace atomically or not at all.
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION-BYTES";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.resize(0); d.write(preExisting);
    }
    const QByteArray preSha = sha256(sanitized);
    QVERIFY(!preSha.isEmpty());

    // The commit fault fires inside SafeSave::commitFileToDestination AFTER
    // the bounded copy — the exact path a real commit failure takes.
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::FailBeforeCommit);
    QString err;
    const bool ok = RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &err);
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!ok, "injected sanitize-commit fault must fail the sanitize step");
    QVERIFY2(err.contains(QLatin1String("commit")), qPrintable(err));

    // The failed replacement left the existing destination byte-identical,
    // and the committed redacted artifact (the sanitize input) is intact.
    QCOMPARE(sha256(sanitized), preSha);
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(sha256(src), srcSha);
}

// Success: an existing sanitized destination is replaced by the new sanitized
// content — a valid PDF with the hidden data stripped, not a corrupt or stale file.
void TestRedactTransaction::sanitizeReplacesExistingSanitizedDestinationOnSuccess() {
    const QString src = createPdf("sfreplace.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("sfreplace_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfreplace_redacted_sanitized.pdf");

    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Stale content in the destination, then a full re-sanitize over it.
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.resize(0);
        d.write("STALE-SANITIZED-CONTENT");
    }
    QString err;
    QVERIFY2(RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &err),
             qPrintable(err));

    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts
    QVERIFY2(QFile(sanitized).size() > 0, "replaced sanitized output must not be empty");
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));  // hidden data stripped by the new pass
    QVERIFY(!cat.HasKey("Metadata"));
    QByteArray replacedBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        replacedBytes = d.readAll();
    }
    QVERIFY2(!replacedBytes.contains("STALE-SANITIZED-CONTENT"),
             "stale destination bytes must be gone after the atomic replacement");
}

// ── D01-family retry: the presenter's Retry-sanitize at the real boundary ───
// Drives the actual modal flow: a genuine PartialRedactedOnly result from a
// full operation run, the presenter's "Retry Sanitize" button, then the
// artifacts on disk. The retry must re-run ONLY the Sanitizing stage from the
// COMMITTED redacted file (D05 boundary) into the intended destination.
void TestRedactTransaction::retryThroughPresenterSucceedsFromCommittedRedactedFile() {
    const QString src = createPdf("retryok.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("retryok_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("retryok_redacted_sanitized.pdf");
    // Pre-existing stale content at the sanitized destination: the successful
    // retry must atomically REPLACE it (D05), never merge or corrupt.
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("STALE-BYTES");
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);
    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);
    QByteArray staleBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        staleBytes = d.readAll();
    }
    QVERIFY2(!staleBytes.isEmpty(), "precondition: stale bytes present before the retry");
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);

    ModalDriver driver(this, {QStringLiteral("Retry Sanitize"), QStringLiteral("OK")});
    RedactResult recovered;
    const auto decision = RedactResultPresenter::present(nullptr, partial, &recovered);
    driver.stop();

    // A completed retry means the marks' effect is fully saved: ClearMarks.
    QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);

    // D01: the recovery must be reflected in the effective result — the flow
    // is Completed, and the banner built from it names BOTH artifacts and
    // never repeats the failed-sanitization wording.
    QCOMPARE(recovered.outcome, RedactOutcome::Completed);
    QCOMPARE(recovered.sanitizedDestination, sanitized);
    QCOMPARE(recovered.destination, partial.destination);
    const QString banner = RedactResultPresenter::bannerText(recovered);
    QVERIFY2(banner.contains(QFileInfo(dest).fileName()), qPrintable(banner));
    QVERIFY2(banner.contains(QFileInfo(sanitized).fileName()), qPrintable(banner));
    QVERIFY2(!banner.contains(QLatin1String("FAILED")),
             qPrintable(QStringLiteral("banner after a successful retry must not keep the "
                                       "failure wording: %1").arg(banner)));

    // The sanitized copy EXISTS at the intended destination and is the NEW
    // sanitized content (pre-fix: the presenter passed an empty path, the
    // retry failed, and this file still held the stale bytes).
    QVERIFY2(QFileInfo::exists(sanitized),
             "retry through the presenter must produce the sanitized copy");
    QByteArray newBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        newBytes = d.readAll();
    }
    QVERIFY2(!newBytes.contains("STALE-BYTES") && newBytes != staleBytes,
             "the retry must replace the stale destination bytes with new content");
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));   // hidden data stripped
    QVERIFY(!cat.HasKey("Metadata"));

    // The presenter announced the committed path ("Sanitization Complete").
    const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
    QVERIFY2(boxes.contains(sanitized), qPrintable(boxes));

    // Every other artifact boundary survives the recovery.
    QVERIFY(QFileInfo::exists(partial.destination));       // redacted artifact intact
    QCOMPARE(sha256(src), srcSha);                          // source untouched
}

// Retry that fails AGAIN must fail honestly and keep the state recoverable:
// the redacted artifact stays, the pre-existing sanitized destination is not
// damaged, and the marks decision RETAINS the marks (a clean retry remains
// possible) — the pre-fix flow cleared them.
void TestRedactTransaction::retryThroughPresenterFailsAgainKeepsRedactedArtifactAndMarks() {
    const QString src = createPdf("retryfail.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("retryfail_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("retryfail_redacted_sanitized.pdf");
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }
    const QByteArray preSha = sha256(sanitized);

    // The fault stays armed: the retry fails AGAIN, deterministically.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);
    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);

    ModalDriver driver(this, {QStringLiteral("Retry Sanitize"), QStringLiteral("OK")});
    const auto decision = RedactResultPresenter::present(nullptr, partial);
    driver.stop();

    // Recoverable state: marks retained for a clean retry.
    QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);

    // The honest failure names the redacted file (the user's recoverable copy).
    const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
    QVERIFY2(boxes.contains(partial.destination), qPrintable(boxes));

    // No artifact was damaged by the failed recovery.
    QVERIFY(QFileInfo::exists(partial.destination));  // redacted artifact kept
    QCOMPARE(sha256(sanitized), preSha);               // pre-existing dest untouched
    QCOMPARE(sha256(src), srcSha);                    // source untouched
}

// ── V01/V02 bounded pins: persistence + marks/history of the redaction flow ─
// The redaction flow keeps marks as viewer annotations (no undo-stack
// commands), so its history contract is the marks decision: marks survive
// every cancel/failure and are cleared exactly once, when the output is
// committed AND kept. Pinned through the shared presenter's real modal flow.
void TestRedactTransaction::cancelDuringSanitizingStageKeepsCommittedArtifacts() {
    const QString src = createPdf("cancelsan.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cancelsan_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("cancelsan_redacted_sanitized.pdf");

    // Cancel requested exactly when the Sanitizing stage begins — AFTER the
    // redacted file is committed. The contract: once committed, cancellation
    // is no longer honored; the artifacts exist and are honestly reported
    // (Completed), never silently discarded.
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    QObject::connect(&op, &RedactOperation::stageChanged, &op,
                     [&op](RedactStage stage, int, int) {
                         if (stage == RedactStage::Sanitizing) op.cancel();
                     });
    const RedactResult r = runOp(&op);

    QVERIFY2(r.outcome == RedactOutcome::Completed,
             qPrintable(QStringLiteral("cancel-after-commit must not discard committed "
                                       "artifacts: %1").arg(errText(r))));
    QCOMPARE(r.destination, dest);
    QCOMPARE(r.sanitizedDestination, sanitized);
    QVERIFY(QFileInfo::exists(dest));
    QVERIFY(QFileInfo::exists(sanitized));
    QCOMPARE(sha256(src), srcSha);  // the original is still never written
}

void TestRedactTransaction::presenterMarkDecisionsPinRecoveryContract() {
    // Failed / Canceled: the marks are kept (RetainMarks) so the user can
    // retry; the wording never claims an output was written.
    {
        RedactResult failed;
        failed.outcome = RedactOutcome::Failed;
        failed.failedStage = QStringLiteral("Committing");
        failed.error = QStringLiteral("injected commit failure");
        failed.destination = m_tmpDir.filePath("dec_failed_redacted.pdf");
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, failed);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
        QVERIFY2(boxes.contains(QLatin1String("not modified"), Qt::CaseInsensitive),
                 qPrintable(boxes));
    }
    {
        RedactResult canceled;
        canceled.outcome = RedactOutcome::Canceled;
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, canceled);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
        QVERIFY2(boxes.contains(QLatin1String("No output")), qPrintable(boxes));
        QVERIFY2(boxes.contains(QLatin1String("preserved")), qPrintable(boxes));
    }
    // Completed: both artifacts committed — marks may be cleared.
    {
        RedactResult done;
        done.outcome = RedactOutcome::Completed;
        done.destination = m_tmpDir.filePath("dec_done_redacted.pdf");
        done.sanitizedDestination = m_tmpDir.filePath("dec_done_sanitized.pdf");
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, done);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);
    }
    // Partial + "Keep Redacted File": artifact committed and kept — ClearMarks,
    // and the redacted file survives.
    {
        const QString dest = m_tmpDir.filePath("dec_keep_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Keep Redacted File")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);
        QVERIFY(QFileInfo::exists(dest));
    }
    // Partial + "Discard Output" with the confirmation DECLINED: marks kept,
    // the redacted copy is NOT deleted.
    {
        const QString dest = m_tmpDir.filePath("dec_discno_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Discard Output"), QStringLiteral("No")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        QVERIFY2(QFileInfo::exists(dest), "declined discard must not delete the redacted copy");
    }
    // Partial + "Discard Output" CONFIRMED: the redacted copy is deleted (only
    // after the explicit confirmation; the source is never touched), marks kept.
    {
        const QString dest = m_tmpDir.filePath("dec_discyes_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Discard Output"), QStringLiteral("Yes")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        QVERIFY2(!QFileInfo::exists(dest), "confirmed discard deletes only the redacted copy");
    }
}

// D01: PartialRedactedOnly must carry the intended sanitize destination
// separately from the committed one — Retry needs the requested path (the
// pre-fix result lost it, so Retry-sanitize through the presenter always
// failed). A Completed result records the same intended path it committed.
void TestRedactTransaction::partialResultCarriesIntendedSanitizeDestination() {
    const QString src = createPdf("intended.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("intended_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("intended_redacted_sanitized.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);

    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);
    QCOMPARE(partial.intendedSanitizedDestination, sanitized); // the retry target survives
    QVERIFY2(partial.sanitizedDestination.isEmpty(),
             "a partial result must not claim a committed sanitized copy");

    // The completed run records the same intended path it committed.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    const QString dest2 = m_tmpDir.filePath("intended2_redacted.pdf");
    const QString sanitized2 = m_tmpDir.filePath("intended2_redacted_sanitized.pdf");
    RedactOperation op2(makeRequest(src, dest2, {0}, /*sanitize=*/true, sanitized2));
    const RedactResult done = runOp(&op2);
    QCOMPARE(done.outcome, RedactOutcome::Completed);
    QCOMPARE(done.intendedSanitizedDestination, sanitized2);
    QCOMPARE(done.sanitizedDestination, sanitized2);
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

    // The plan's optional overlay text rides the plan struct (§9.8 P1) — empty
    // default, user-adjustable, carried back out of plan().
    QCOMPARE(dlg.plan().overlayText, QString());
    dlg.setOverlayText(QStringLiteral("FOIA-2026-114"));
    QCOMPARE(dlg.plan().overlayText, QStringLiteral("FOIA-2026-114"));

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

// ── §9.8 P1: optional overlay text printed on the burn-in boxes ──────────────
void TestRedactTransaction::overlayTextIsPrintedOnBurnedInBoxes() {
    const QString src = createPdf("overlay.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("overlay_redacted.pdf");

    RedactRequest req = makeRequest(src, dest, {0, 1}, false);
    req.overlayText = QStringLiteral("CLASSIFIED");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // The reason code is PRINTED ON the black boxes — independently
    // extractable on every redacted page — while the excision contract is
    // untouched: the secret is gone, the public line survives.
    for (int p = 0; p < 2; ++p) {
        const QString text = pageText(dest, p);
        QVERIFY2(text.contains(QLatin1String("CLASSIFIED")),
                 qPrintable(QStringLiteral("overlay text missing on page %1: %2").arg(p).arg(text)));
        QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
                 qPrintable(QStringLiteral("secret survived on page %1: %2").arg(p).arg(text)));
        QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")),
                 qPrintable(QStringLiteral("public text lost on page %1").arg(p)));
    }
}

void TestRedactTransaction::emptyOverlayTextPreservesCurrentBehavior() {
    const QString src = createPdf("nooverlay.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("nooverlay_redacted.pdf");

    RedactRequest req = makeRequest(src, dest, {0}, false);
    QVERIFY2(req.overlayText.isEmpty(), "overlayText must default to empty");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Empty overlay = exactly the current behavior: excision happens, no
    // overlay text is added to the page.
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));
}

void TestRedactTransaction::overlaySkippedWhenBoxTooSmall() {
    const QString src = createPdf("smalloverlay.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("smalloverlay_redacted.pdf");

    // An 8pt-tall band that still crosses the secret's baseline (pdf
    // y 699..707 contains y=700 — the engine excises on baseline-span
    // intersection): the box IS burned in, but it is too small to carry 7pt
    // overlay text, so the overlay must be skipped (appearance auto-fit
    // precedent), never squeezed in or drawn outside the box.
    RedactRequest req = makeRequest(src, dest, {0}, false);
    req.redactionsByPage.clear();
    req.redactionsByPage[0].append(QRectF(40, 135, 300, 8));
    req.overlayText = QStringLiteral("CLASSIFIED");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("CLASSIFIED")),
             qPrintable(QStringLiteral("overlay must be skipped on an 8pt-tall box: %1").arg(text)));
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
             qPrintable(QStringLiteral("excision must not depend on overlay fit: %1").arg(text)));
}

QTEST_MAIN(TestRedactTransaction)
#include "TestRedactTransaction.moc"
