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
//
// R08 regression tests (F04): reviewed OCR words are authoritative.
// The editable pane used to be populated but never read back, so Accept
// exported the cached ORIGINAL words; the displayed page could also differ
// from the cached page being saved. These tests cover the review session
// (identity/revision/page/image), word-based correction with stable IDs,
// deleted words, Unicode, similar words on two pages, source-change
// rejection, save cancellation, and verify the saved text layer by
// extracting it through PDFium (PdfiumBackend::extractText).
#include <QtTest>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QToolButton>

#include "modes/OCRMode.h"
#include "modes/OcrReviewSession.h"
#include "shell/controllers/EditController.h"
#include "engines/ocr/OcrPipeline.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"

using gp::OCRMode;
using gp::EditController;
using gp::OcrReviewedWord;
using gp::OcrReviewSession;

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

QImage makeScanPage(int w = 400, int h = 300)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));
    return img;
}

// A minimal review session as EditController would cache it after a run:
// source identity + revision proxy + the page image the words belong to.
OcrReviewSession makeSession(int page, const QStringList& texts,
                             const QString& path = QStringLiteral("C:/scans/inv.pdf"))
{
    OcrReviewSession s;
    s.generation = 1;
    s.sourcePath = path;
    s.sourcePage = page;
    s.sourcePageCount = 3;
    s.pageImage = makeScanPage();
    int id = 0;
    for (const QString& t : texts) {
        OcrReviewedWord w;
        w.stableId = id;
        w.originalText = t;
        w.reviewedText = t;
        w.deleted = false;
        w.boundingBox = QRectF(20 + 10 * id, 30 + 18 * id, 60, 14);
        w.confidence = 90;
        w.sourceEngine = QStringLiteral("Tesseract");
        s.words.append(w);
        ++id;
    }
    return s;
}

bool extractContains(const QString& pdfPath, int page, const QString& needle)
{
    PdfiumBackend pdfium;
    if (!pdfium.loadDocument(pdfPath)) return false;
    if (page >= pdfium.pageCount()) return false;
    return pdfium.extractText(page).contains(needle);
}

// The panel's reviewed records mirror the delivered words 1:1 (same count, same
// stable ids) — this helper simulates a panel that reviewed an OLDER delivery.
QList<OcrReviewedWord> panelRecordsFor(const OcrReviewSession& s)
{
    return s.words;
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
// U03: uncertain-word navigation buttons follow the SAME lifecycle discipline
// as Accept/Reject (enabled only in ReviewReady with something to review).
QToolButton* nextUncertainButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnNextUncertain"));
}
QToolButton* prevUncertainButton(const OCRMode& panel)
{
    return panel.findChild<QToolButton*>(QStringLiteral("ocrBtnPrevUncertain"));
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

    // ══════════════════════════════════════════════════════════════════════════
    // R08 — reviewed OCR words are authoritative (F04)
    // ══════════════════════════════════════════════════════════════════════════

    // ── Word-based correction updates the stable record, never the source box ─
    void wordCorrectionUpdatesRecordNotBox()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());

        const QRectF boxBefore = panel.reviewedWords().at(0).boundingBox;
        QVERIFY(panel.applyWordCorrection(0, QStringLiteral("INVOICE #812")));
        const auto records = panel.reviewedWords();
        QCOMPARE(records.at(0).stableId, 0);
        QCOMPARE(records.at(0).reviewedText, QStringLiteral("INVOICE #812"));
        QCOMPARE(records.at(0).originalText, QStringLiteral("invoice"));
        QCOMPARE(records.at(0).boundingBox, boxBefore);   // source box untouched
        QVERIFY(!records.at(0).deleted);
        // The unedited neighbour keeps its own record.
        QCOMPARE(records.at(1).reviewedText, QStringLiteral("total"));

        // Unknown stable IDs are rejected (stale interaction guard).
        QVERIFY(!panel.applyWordCorrection(42, QStringLiteral("ghost")));
        QVERIFY(!panel.markWordDeleted(42));
    }

    // ── The correction field drives the selected word's record ───────────────
    void correctionFieldEditsSelectedWord()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());

        // Click the second word in the scan pane (link activation selects it).
        panel.activateWordLink(QStringLiteral("word:1"));
        auto* edit = panel.findChild<QLineEdit*>(QStringLiteral("ocrWordEdit"));
        QVERIFY(edit);
        QCOMPARE(edit->text(), QStringLiteral("total"));

        edit->setText(QStringLiteral("grand total"));
        emit edit->returnPressed();

        const auto records = panel.reviewedWords();
        QCOMPARE(records.at(1).reviewedText, QStringLiteral("grand total"));
        QCOMPARE(records.at(1).boundingBox, QRectF(10, 40, 40, 14));
        QCOMPARE(records.at(0).reviewedText, QStringLiteral("invoice"));
    }

    // ── A deleted word is excluded from the export payload ────────────────────
    void deletedWordExcludedFromPayload()
    {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        QVERIFY(panel.markWordDeleted(1));

        OcrReviewSession session = makeSession(0, { QStringLiteral("invoice"), QStringLiteral("total") });
        QString error;
        const PageOcrResult payload =
            EditController::buildReviewedPageOcrResult(session, panel.reviewedWords(), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(payload.pageIndex, 0);
        QCOMPARE(payload.words.size(), 1);
        QCOMPARE(payload.words.first().text, QStringLiteral("invoice"));
        QVERIFY(payload.success);
    }

    // ── The reviewed page index (not the displayed page) labels and payload ───
    void reviewedPageIdentityDrivesPayload()
    {
        // The user reviewed page 2 (0-based) and then navigated elsewhere; the
        // payload must still carry the reviewed page index.
        OcrReviewSession session = makeSession(2, { QStringLiteral("invoice") });
        QString error;
        const PageOcrResult payload =
            EditController::buildReviewedPageOcrResult(session, {}, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(payload.pageIndex, 2);                 // payload page identity
        QCOMPARE(payload.words.size(), 1);
        QVERIFY(payload.success);

        // The save label uses the same reviewed page index.
        const QString title = EditController::ocrSaveDialogTitle(40, 2);
        QVERIFY(title.contains(QStringLiteral("3 of 40")));
    }

    // ── Session validity: source change / revision change is rejected ─────────
    void sourceChangeRejectsSession()
    {
        OcrReviewSession session = makeSession(1, { QStringLiteral("invoice") });

        QString reason;
        // Same source, same page count → exportable.
        QVERIFY(EditController::ocrSessionIsExportable(session,
                   QStringLiteral("C:/scans/inv.pdf"), 3, &reason));

        // Source document changed → rejected with a reason.
        QVERIFY(!EditController::ocrSessionIsExportable(session,
                   QStringLiteral("C:/scans/other.pdf"), 3, &reason));
        QVERIFY(reason.contains(QStringLiteral("another document"), Qt::CaseInsensitive));

        // Document revision changed (page inserted/deleted) → rejected.
        QVERIFY(!EditController::ocrSessionIsExportable(session,
                   QStringLiteral("C:/scans/inv.pdf"), 4, &reason));
        QVERIFY(reason.contains(QStringLiteral("changed"), Qt::CaseInsensitive));

        // Invalid/absent session → rejected.
        OcrReviewSession empty;
        QVERIFY(!EditController::ocrSessionIsExportable(empty,
                   QStringLiteral("C:/scans/inv.pdf"), 3, &reason));
    }

    // ── Records from an older delivery are rejected against the session ───────
    void staleReviewRecordsRejected()
    {
        OcrReviewSession session = makeSession(0, { QStringLiteral("invoice"), QStringLiteral("total") });
        QList<OcrReviewedWord> stale = panelRecordsFor(makeSession(0, { QStringLiteral("invoice") }));
        QString error;
        EditController::buildReviewedPageOcrResult(session, stale, &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(error.contains(QStringLiteral("no longer matches"), Qt::CaseInsensitive));
    }

    // ── Export: corrected text reaches the PDFium text layer, old token gone ──
    void exportCorrectedTextExtractsViaPdfium()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("corrected.pdf"));

        // Seed the misspelled recognition, then correct it via the word record.
        QList<MergedOcrWord> words = makeWords();          // {invoice, total}
        words[0].text = QStringLiteral("recievng");        // {recievng, total}
        OCRMode panel;
        panel.setOcrResults(words);
        QVERIFY(panel.applyWordCorrection(0, QStringLiteral("receiving")));

        // The session mirrors the SAME delivery the panel reviewed.
        OcrReviewSession session =
            makeSession(0, { words[0].text, words[1].text });
        QString error;
        const PageOcrResult payload =
            EditController::buildReviewedPageOcrResult(session, panel.reviewedWords(), &error);
        QVERIFY(error.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.exportMrcPdfA(out, { session.pageImage }, { payload }));

        // PDFium extraction: the corrected token appears in the new text layer
        // and the old misspelled token is gone from it.
        QVERIFY(extractContains(out, 0, QStringLiteral("receiving")));
        QVERIFY(extractContains(out, 0, QStringLiteral("total")));
        QVERIFY(!extractContains(out, 0, QStringLiteral("recievng")));
    }

    // ── Export: Unicode corrections survive extraction ────────────────────────
    void exportUnicodeCorrectionExtractsViaPdfium()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("unicode.pdf"));

        QList<MergedOcrWord> words = makeWords();          // {invoice, total}
        words[0].text = QStringLiteral("cafe");            // {cafe, total}
        OCRMode panel;
        panel.setOcrResults(words);
        QVERIFY(panel.applyWordCorrection(0, QStringLiteral("café naïve")));

        OcrReviewSession session =
            makeSession(0, { words[0].text, words[1].text });
        QString error;
        const PageOcrResult payload =
            EditController::buildReviewedPageOcrResult(session, panel.reviewedWords(), &error);
        QVERIFY(error.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.exportMrcPdfA(out, { session.pageImage }, { payload }));

        QVERIFY(extractContains(out, 0, QStringLiteral("café naïve")));
        QVERIFY(extractContains(out, 0, QStringLiteral("total")));
        QVERIFY(!extractContains(out, 0, QStringLiteral("cafe")));
    }

    // ── U03: uncertain-word navigation follows the lifecycle state machine ────
    // A stale/canceled completion must leave the navigation buttons in the
    // same disabled/enabled state as Accept (state-machine coherence).
    void uncertainNavigationFollowsLifecycle()
    {
        OCRMode panel;
        // makeWords() carries a 55%-confidence word → reviewable + uncertain.
        panel.setOcrResults(makeWords());
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(nextUncertainButton(panel)->isEnabled());
        QVERIFY(prevUncertainButton(panel)->isEnabled());

        // A canceled job disables review AND navigation together.
        panel.onRunOcr();
        panel.notifyOcrCanceled(QStringLiteral("OCR finished, but the page changed — results discarded."));
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::RecoverableError);
        QVERIFY(!acceptButton(panel)->isEnabled());
        QVERIFY(!nextUncertainButton(panel)->isEnabled());
        QVERIFY(!prevUncertainButton(panel)->isEnabled());

        // A reviewable but all-certain delivery: review possible, nothing
        // uncertain to jump to — navigation stays disabled.
        QList<MergedOcrWord> highWords = makeWords();
        highWords[1].confidence = 88;
        panel.setOcrResults(highWords);
        QCOMPARE(panel.reviewState(), OCRMode::ReviewState::ReviewReady);
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(!nextUncertainButton(panel)->isEnabled());
        QVERIFY(!prevUncertainButton(panel)->isEnabled());

        // Removing the last uncertain word through review also disables nav.
        QList<MergedOcrWord> mixed = makeWords();   // 92 / 55
        panel.setOcrResults(mixed);
        QVERIFY(nextUncertainButton(panel)->isEnabled());
        QVERIFY(panel.markWordDeleted(1));
        QVERIFY(acceptButton(panel)->isEnabled());
        QVERIFY(!nextUncertainButton(panel)->isEnabled());
    }

    // ── Two pages with similar words stay distinct in the export ──────────────
    void twoPagesSimilarWordsKeptDistinct()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("twopages.pdf"));

        OcrReviewSession session0 = makeSession(0, { QStringLiteral("invoice"), QStringLiteral("total") });
        OcrReviewSession session1 = makeSession(1, { QStringLiteral("invoice"), QStringLiteral("due") });

        QString error;
        const PageOcrResult payload0 = EditController::buildReviewedPageOcrResult(session0, {}, &error);
        QVERIFY(error.isEmpty());
        const PageOcrResult payload1 = EditController::buildReviewedPageOcrResult(session1, {}, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(payload0.pageIndex, 0);
        QCOMPARE(payload1.pageIndex, 1);

        PdfEditorEngine engine;
        QVERIFY(engine.exportMrcPdfA(out, { session0.pageImage, session1.pageImage },
                                     { payload0, payload1 }));

        QVERIFY(extractContains(out, 0, QStringLiteral("total")));
        QVERIFY(!extractContains(out, 0, QStringLiteral("due")));
        QVERIFY(extractContains(out, 1, QStringLiteral("due")));
        QVERIFY(!extractContains(out, 1, QStringLiteral("total")));
        QVERIFY(extractContains(out, 1, QStringLiteral("invoice")));
    }
};

QTEST_MAIN(TestOcrReviewLifecycle)
#include "TestOcrReviewLifecycle.moc"
