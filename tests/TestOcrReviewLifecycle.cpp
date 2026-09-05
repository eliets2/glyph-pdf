// SPDX-License-Identifier: Apache-2.0
// R07 regression tests: complete the OCR lifecycle on every exit.
//
// Evidence F11: Run was disabled before dispatch and only success restored it —
// missing files/models/languages and worker errors left the OCR review panel
// stuck with Run/Accept/Reject disabled. Accept also disabled the review
// controls before the save dialog, so save cancellation left them stuck too.
//
// These tests drive ONE OCRMode panel instance through every terminal outcome
// and assert both the explicit state and the user-visible controls.
#include <QtTest>
#include <QLineEdit>
#include <QToolButton>

#include "modes/OCRMode.h"
#include "shell/controllers/EditController.h"
#include "engines/ocr/OcrPipeline.h"

using gp::OCRMode;
using gp::EditController;

namespace {

QList<MergedOcrWord> makeWords()
{
    MergedOcrWord a;
    a.text = QStringLiteral("invoice");
    a.boundingBox = QRectF(10, 10, 60, 14);
    a.confidence = 92;
    a.sourceEngine = QStringLiteral("Tesseract");
    MergedOcrWord b;
    b.text = QStringLiteral("total");
    b.boundingBox = QRectF(10, 40, 40, 14);
    b.confidence = 55;
    b.sourceEngine = QStringLiteral("ROVER");
    return { a, b };
}

QToolButton* runButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnRun"));
}
QToolButton* acceptButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnAccept"));
}
QToolButton* rejectButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnReject"));
}

} // namespace

class TestOcrReviewLifecycle : public QObject
{
    Q_OBJECT

private slots:

    // ── Baseline: a fresh panel is idle with only Run available ──────────────
    void freshPanelIsIdleWithRunEnabled()
    {
        OCRMode panel;
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::Idle);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Running state disables everything until a completion arrives ─────────
    void runningDisablesControlsUntilCompletion()
    {
        OCRMode panel;
        panel.onRunOcr();
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::Running);
        QVERIFY(!runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Missing language/model data fails the run, retry succeeds (same panel)
    void missingLanguageThenRetrySucceeds()
    {
        OCRMode panel;
        panel.onRunOcr();
        // EditController reports the missing-data failure (language data, ONNX
        // models) instead of dying silently with Run disabled.
        panel.notifyOcrFailed(
            QStringLiteral("OCR failed: Tesseract language data for 'DEU' is unavailable."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(runButton(panel)->isEnabled());          // retry is possible
        QCOMPARE(runButton(panel)->text(), QStringLiteral("Run OCR"));
        QVERIFY(!acceptButton(panel)->isEnabled());      // nothing to review
        QVERIFY(!rejectButton(panel)->isEnabled());
        QVERIFY(panel.lastLifecycleMessage().contains(QStringLiteral("DEU")));

        // Retry succeeds on the same panel instance.
        panel.setOcrResults(makeWords());
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(rejectButton(panel)->isEnabled());
    }

    // ── No document: dispatch is blocked and the panel is told why ───────────
    void noDocumentBlocksDispatchWithRecovery()
    {
        // Pure seam: empty path / negative page block dispatch with a message.
        QString blocker = EditController::ocrDispatchBlocker(QString(), -1);
        QVERIFY(!blocker.isEmpty());
        QVERIFY(EditController::ocrDispatchBlocker(QStringLiteral("doc.pdf"), 0).isEmpty());

        // The panel recovers to a retryable state when the blocker is reported.
        OCRMode panel;
        panel.onRunOcr();
        panel.notifyOcrFailed(blocker);
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
    }

    // ── Empty recognition completes: idle, retryable, nothing to review ──────
    void emptyRecognitionCompletesWithoutReview()
    {
        OCRMode panel;
        panel.onRunOcr();
        panel.setOcrResults({});
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::Idle);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Worker error: recoverable, Run restored ──────────────────────────────
    void workerErrorRecoversRun()
    {
        OCRMode panel;
        panel.onRunOcr();
        panel.notifyOcrFailed(QStringLiteral("OCR failed: RapidOCR engine could not be initialised."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Cancellation (job abandoned): recoverable, Run restored ──────────────
    void canceledJobRecoversRun()
    {
        OCRMode panel;
        panel.onRunOcr();
        panel.notifyOcrCanceled(QStringLiteral("OCR finished, but the page changed — results discarded."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Job completion classification (pure seam used by the worker callback) ─
    void classifyJobCompletionVerdicts()
    {
        QString message;

        // Success delivers.
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 1,
                   QStringLiteral("a.pdf"), 0, QStringLiteral("a.pdf"), 0,
                   QString(), &message),
                 EditController::OcrJobVerdict::Deliver);

        // Worker error fails with the worker's message.
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 1,
                   QStringLiteral("a.pdf"), 0, QStringLiteral("a.pdf"), 0,
                   QStringLiteral("OCR failed: could not render page."), &message),
                 EditController::OcrJobVerdict::Failed);
        QVERIFY(message.contains(QStringLiteral("could not render page")));

        // A newer request supersedes the stale job.
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 2,
                   QStringLiteral("a.pdf"), 0, QStringLiteral("a.pdf"), 0,
                   QString(), &message),
                 EditController::OcrJobVerdict::Stale);

        // Page changed mid-job → stale/cancelled, never delivered.
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 1,
                   QStringLiteral("a.pdf"), 0, QStringLiteral("a.pdf"), 3,
                   QString(), &message),
                 EditController::OcrJobVerdict::Stale);
        QVERIFY(message.contains(QStringLiteral("page changed")));

        // Document switch mid-job → stale.
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 1,
                   QStringLiteral("a.pdf"), 0, QStringLiteral("b.pdf"), 0,
                   QString(), &message),
                 EditController::OcrJobVerdict::Stale);
        QVERIFY(message.contains(QStringLiteral("document changed")));

        // Editor closed before completion → stale (destroyed panels get nothing).
        QCOMPARE(EditController::classifyOcrJobCompletion(1, 1,
                   QStringLiteral("a.pdf"), 0, QString(), -1,
                   QString(), &message),
                 EditController::OcrJobVerdict::Stale);
        QVERIFY(message.contains(QStringLiteral("closed")));
    }

    // ── A stale completion must not re-enable another document's save ────────
    void staleCompletionDoesNotReenableForeignSave()
    {
        OCRMode panel;
        // Review results from document A.
        panel.setOcrResults(makeWords());
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());

        // User starts a new run on document B; that job is abandoned as stale
        // (document switched). The panel must NOT fall back to the document-A
        // review: Accept stays disabled and only Run (retry) is restored.
        panel.onRunOcr();
        panel.notifyOcrCanceled(QStringLiteral("OCR finished, but the document changed — results discarded."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }

    // ── Accept → save cancelled: review edits retained, controls restored ────
    void saveCancellationRetainsReviewAndRestoresControls()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        panel.onAcceptResults();
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::Saving);
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());

        // User cancels the save dialog: nothing was written; review continues.
        panel.notifySaveFinished(false, true, QString());
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(rejectButton(panel)->isEnabled());
        QVERIFY(runButton(panel)->isEnabled());
    }

    // ── Save error: data retained for retry ───────────────────────────────────
    void saveErrorRetainsReviewForRetry()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        panel.onAcceptResults();
        panel.notifySaveFinished(false, false,
            QStringLiteral("Could not write the searchable MRC PDF/A copy. See the application log."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(rejectButton(panel)->isEnabled());
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(panel.lastLifecycleMessage().contains(QStringLiteral("MRC PDF/A")));
    }

    // ── Save success: back to a reviewable state ──────────────────────────────
    void saveSuccessReturnsToReviewReady()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        panel.onAcceptResults();
        panel.notifySaveFinished(true, false, QStringLiteral("Searchable copy saved: doc_ocr.pdf"));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(runButton(panel)->isEnabled());
    }

    // ── Reject clears review and returns to idle ──────────────────────────────
    void rejectReturnsToIdle()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        panel.onRejectResults();
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::Idle);
        QVERIFY(runButton(panel)->isEnabled());
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!rejectButton(panel)->isEnabled());
    }
};

QTEST_MAIN(TestOcrReviewLifecycle)
#include "TestOcrReviewLifecycle.moc"
