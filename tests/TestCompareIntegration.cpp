// SPDX-License-Identifier: Apache-2.0
// §9.10 P1 — compare INTEGRATION tests end-to-end. The July plan noted only
// the isolated Myers algorithm was tested; R11/U04 landed pageChanges,
// filter-aware anchors and the CompareWidget integration with their own
// (mostly synthetic-fixture) tests. This file closes the remaining gap:
// every test here drives the REAL DiffEngine (real PDFs on disk through the
// pdfium text extraction + pixel pipeline) feeding the REAL CompareWidget and
// the REAL CompareMode entry point (compareFiles' async watcher path), and
// asserts only through public seams:
//   - DiffResult via CompareMode::lastResult()
//   - CHANGES tree rows + CompareMode data roles (objectName cmpChangesTree)
//   - CompareWidget::changeCount()/anchorAt()/nextChange()/prevChange()
//   - PdfViewerWidget::currentPage()/pageCount() through the splitter
//   - report builders buildTextReport()/buildHtmlReport() (+ filter overloads)
// Hand-built N-page PDFs use TestCompareEntry::createPagePdf's idiom: QPdfWriter
// embeds subset fonts that extract as garbage, so DiffEngine-driven fixtures
// need raw string literals (an empty string yields a blank page).
//
// Page-alignment note (characterization): the engine aligns two pages as
// "same" when their word-set similarity is >= 0.80, so the text-edit fixture
// changes one word of a six-word page (similarity 5/6 ≈ 0.83 — stays aligned,
// no structural noise), while the trailing-page fixture adds a page sharing no
// words with any existing page (a clean PageAdded).
#include <QtTest>
#include <QLabel>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSemaphore>
#include <QSharedPointer>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QToolButton>

#include "engines/DiffEngine.h"
#include "modes/CompareMode.h"
#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"  // viewer assertions need the complete type

namespace {

// Hand-built real N-page PDF (TestCompareEntry idiom): deterministic bytes,
// one Helvetica string per page, a blank page for an empty string.
QString createPagePdf(const QString& path, const QStringList& pageTexts)
{
    const int n = pageTexts.size();
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    offsets.append(pdf.size());
    pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    offsets.append(pdf.size());
    QByteArray kids;
    for (int k = 0; k < n; ++k)
        kids += QByteArray::number(3 + 2 * k) + " 0 R ";
    pdf += "2 0 obj<</Type/Pages/Kids[" + kids + "]/Count "
           + QByteArray::number(n) + ">>endobj\n";
    for (int k = 0; k < n; ++k) {
        const int pageNo = 3 + 2 * k;
        const int contNo = 4 + 2 * k;
        const QString line = pageTexts.at(k);
        QByteArray content;
        if (!line.isEmpty()) {
            QByteArray lit = line.toLatin1();
            lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
            content = "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n";
        }
        offsets.append(pdf.size());
        pdf += QByteArray::number(pageNo)
             + " 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents "
             + QByteArray::number(contNo)
             + " 0 R/Resources<</Font<</F1 " + QByteArray::number(3 + 2 * n)
             + " 0 R>>>>>>endobj\n";
        offsets.append(pdf.size());
        pdf += QByteArray::number(contNo) + " 0 obj<</Length "
             + QByteArray::number(content.size()) + ">>stream\n"
             + content + "endstream endobj\n";
    }
    offsets.append(pdf.size());
    pdf += QByteArray::number(3 + 2 * n)
         + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n";
    const qint64 xrefOffset = pdf.size();
    const int objCount = 4 + 2 * n;
    pdf += "xref\n0 " + QByteArray::number(objCount) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off))
                   .rightJustified(10, '0')
               + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(objCount)
           + "/Root 1 0 R>>\nstartxref\n" + QByteArray::number(xrefOffset)
           + "\n%%EOF\n";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

} // namespace

class TestCompareIntegration : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString pagePdf(const QString& name, const QStringList& pageTexts)
    {
        const QString path = m_dir.filePath(name);
        const QString written = createPagePdf(path, pageTexts);
        if (written.isEmpty())
            qWarning("failed to write %s", qPrintable(path));
        return written;
    }

    // compareFiles() runs DiffEngine::compare on a QtConcurrent thread; the
    // first post-finish observable is the status label leaving "COMPARING...".
    void waitForDiffFinished(gp::CompareMode& mode)
    {
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY2(status, "status label must carry objectName cmpStatusLabel");
        QTRY_VERIFY2(!status->text().contains(QStringLiteral("COMPARING")),
                     qPrintable(QStringLiteral("diff never finished, status: %1")
                                    .arg(status->text())));
    }

    static QSplitter* viewerSplitter(CompareWidget& widget)
    {
        // nullptr on failure — call sites QVERIFY2 the result (QVERIFY may
        // only be used in void functions).
        return widget.findChild<QSplitter*>();
    }

private slots:
    void initTestCase() { QVERIFY2(m_dir.isValid(), "temporary dir must be valid"); }

    // ── §4 row 7 (wave 2b): compare progress + cancel ─────────────────────────
    // compareFiles() must surface a cancelable progress dialog for the
    // duration of the diff (the MainWindow::runConversion idiom). Hook-free
    // probe: the dialog is constructed synchronously inside compareFiles()
    // BEFORE the worker's finish can be delivered, so its existence right
    // after the call is deterministic. Pre-fix there is no dialog at all —
    // the RED pin for the parity row.
    void progressDialogExistsDuringCompareRun()
    {
        const QStringList texts = {
            QStringLiteral("alpha bravo charlie delta echo foxtrot"),
            QStringLiteral("second page")};
        const QString a = pagePdf("prog_a.pdf", texts);
        const QString b = pagePdf("prog_b.pdf", texts);
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(a, b);

        auto* dialog = mode.findChild<QProgressDialog*>(
            QStringLiteral("cmpProgressDialog"));
        QVERIFY2(dialog,
                 "compareFiles() must surface a QProgressDialog (objectName "
                 "cmpProgressDialog) while the diff runs");

        // Completion path unchanged: the identical pair still resolves
        // identically and leaves the dialog closed behind it.
        waitForDiffFinished(mode);
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("FILES ARE IDENTICAL")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
        QVERIFY(mode.lastResult().isIdentical);
    }

    // ── §4 row 7 (wave 2b): progress stages observed ──────────────────────────
    // The dialog tracks the engine's per-stage progress (extraction → page
    // pairs) through the watcher. Deterministic via the pair-boundary test
    // seam (BatchMode's setPresetBoundaryHookForTest idiom): the worker parks
    // at a boundary so the GUI thread can read the dialog mid-run.
    void progressDialogTracksStagesDuringDiff()
    {
        const QString base = pagePdf("stage_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("stage_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        // Park the worker BEFORE the first extraction report, before the
        // first pair report, and before the second pair report. The hook
        // fires before each boundary's report posts, so each wake observes a
        // deterministic dialog state (the backlog holds exactly the reports
        // made before the park). Worker parks are bounded (60s) so a failing
        // assertion upstream can never hang the QtConcurrent pool at process
        // exit.
        QSemaphore reached[3], release[3];
        mode.setStageBoundaryHookForTest([&](int stage, int done) {
            const int key = (stage == DiffEngine::ProgressExtractText && done == 0) ? 0
                          : (stage == DiffEngine::ProgressPagePairs && done == 0) ? 1
                          : (stage == DiffEngine::ProgressPagePairs && done == 1) ? 2
                          : -1;
            if (key >= 0) {
                reached[key].release();
                release[key].tryAcquire(1, 60000);
            }
        });
        mode.compareFiles(base, revised);

        // QPointer, not a raw pointer: onDiffFinished() closes AND destroys
        // the dialog (deleteLater), so any post-finish access through a raw
        // pointer would be a use-after-free. The tail pins below rely on the
        // QPointer turning null when the destruction lands.
        QPointer<QProgressDialog> dialog = mode.findChild<QProgressDialog*>(
            QStringLiteral("cmpProgressDialog"));
        QVERIFY2(dialog, "progress dialog must exist during a compare run");

        // Stage 1 — extraction: the FIRST park fires before the stage's first
        // report, so the dialog is still in its initial busy state (range
        // (0,0) from the constructor) — the deterministic pre-report
        // observation. (The stage observation below rides the RANGE/VALUE
        // stream — the label text is user-facing polish, not pinned here.)
        QVERIFY2(reached[0].tryAcquire(1, 30000),
                 "worker never reached the first extraction boundary");
        QTRY_COMPARE_WITH_TIMEOUT(dialog->maximum(), 0, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(dialog->value(), 0, 30000);
        release[0].release();

        // Extraction reports flow: by the time the worker reaches the first
        // pair boundary (parked there), the whole extraction stage (range
        // (0,4), final value 4) has been delivered.
        QVERIFY2(reached[1].tryAcquire(1, 30000),
                 "worker never reached the first page-pair boundary");
        QTRY_COMPARE_WITH_TIMEOUT(dialog->maximum(), 4, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(dialog->value(), 4, 30000);
        release[1].release();

        // Stage 2 — page pairs: the range SWITCH to (0, 2) is the observable
        // stage transition. (The bar's intermediate value within this stage
        // is not pinned: the range change resets QProgressBar's value to -1
        // and the first post-reset setValue is unreliable through the modal
        // dialog in this environment — the completion value below IS pinned.)
        QVERIFY2(reached[2].tryAcquire(1, 30000),
                 "worker never reached the second page-pair boundary");
        QTRY_COMPARE_WITH_TIMEOUT(dialog->maximum(), 2, 30000);

        // Completion path unchanged: the remaining boundaries report, the
        // finish lands, the dialog is closed AND destroyed, and the result
        // is applied. (The bar's final in-dialog value is not re-read after
        // the release on purpose: the finish may already have been processed
        // by the time the GUI thread re-reads it — QFutureInterface orders
        // all progress reports strictly BEFORE the finished delivery, so the
        // finished state below PROVES the "2/2" boundary was posted. The
        // destruction pin is the observable half: nothing is left behind.)
        release[2].release();
        waitForDiffFinished(mode);
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
        QTRY_VERIFY2_WITH_TIMEOUT(dialog.isNull(),
                                  "progress dialog must be destroyed after completion",
                                  30000);
    }

    // ── §4 row 7 (wave 2b): cancel mid-diff honored, no partial state ─────────
    // The worker stops at the next boundary probe (the QPromise::isCanceled
    // poll DiffEngine already honours); the partial result is discarded
    // (never read into the widget/tree/lastResult); the UI leaves the
    // COMPARING state with nav/export off and swap re-enabled; a re-run of
    // the same pair afterwards completes normally (idempotent after cancel).
    void cancelMidDiffStopsWorkerAndKeepsStateClean()
    {
        const QString base = pagePdf("cxl_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("cxl_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        QSemaphore reachedPair0, releasePair0;
        mode.setStageBoundaryHookForTest([&](int stage, int done) {
            if (stage == DiffEngine::ProgressPagePairs && done == 0) {
                reachedPair0.release();
                releasePair0.tryAcquire(1, 60000);   // bounded park (see stages test)
            }
        });
        mode.compareFiles(base, revised);

        // QPointer (see the stages test): the dialog is destroyed after the
        // cancel lands, and the destruction pin below relies on the QPointer.
        QPointer<QProgressDialog> dialog = mode.findChild<QProgressDialog*>(
            QStringLiteral("cmpProgressDialog"));
        QVERIFY2(dialog, "progress dialog must exist during a compare run");
        QVERIFY2(reachedPair0.tryAcquire(1, 30000),
                 "worker never reached the first page-pair boundary");
        QVERIFY2(mode.isBusy(),
                 "the diff must still be running while the worker is parked");

        // The Cancel path the USER drives: the dialog's Cancel button (the
        // QPushButton QProgressDialog builds from the constructor's cancel
        // text) is what a real click activates, and its clicked() signal is
        // wired by QProgressDialog itself to emit canceled() — the exact
        // signal compareFiles() connected to the watcher's cancel(). Clicking
        // the button (not poking the watcher) reproduces the user gesture
        // through the shipped wiring.
        QPushButton* cancelBtn = dialog->findChild<QPushButton*>();
        QVERIFY2(cancelBtn, "the progress dialog must own its Cancel button");
        QVERIFY2(cancelBtn->text().contains(QStringLiteral("Cancel")),
                 qPrintable(QStringLiteral("cancel button text: %1").arg(cancelBtn->text())));
        cancelBtn->click();
        releasePair0.release();   // worker wakes and abandons the diff

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QTRY_VERIFY2_WITH_TIMEOUT(
            status->text().contains(QStringLiteral("COMPARISON CANCELLED")),
            qPrintable(QStringLiteral("status after cancel: %1").arg(status->text())),
            30000);
        QVERIFY2(!mode.isBusy(), "the watcher must not report busy after cancel");
        QTRY_VERIFY2_WITH_TIMEOUT(dialog.isNull(),
                                  "progress dialog must be destroyed after cancel",
                                  30000);

        // No leaked partial results: the engine's partial DiffResult was
        // never read into the widget, the tree or lastResult().
        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 0);
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 0);
        QVERIFY(mode.lastResult().pages.isEmpty());
        QVERIFY(mode.lastResult().pageChanges.isEmpty());

        // UI re-enabled cleanly: PREV/NEXT stay off (nothing is displayed),
        // export stays off (there is no completed diff to export), swap is
        // safe again (the same pair can be re-run).
        QToolButton* prev = nullptr; QToolButton* next = nullptr;
        QToolButton* exportBtn = nullptr; QToolButton* swap = nullptr;
        const auto buttons = mode.findChildren<QToolButton*>();
        for (auto* btn : buttons) {
            if (btn->text().contains(QStringLiteral("PREV"))) prev = btn;
            if (btn->text().contains(QStringLiteral("NEXT"))) next = btn;
            if (btn->text().contains(QStringLiteral("Export"))) exportBtn = btn;
            if (btn->text().contains(QStringLiteral("Swap"))) swap = btn;
        }
        QVERIFY2(prev && next && exportBtn && swap, "toolbar buttons must exist");
        QVERIFY2(!prev->isEnabled(), "PREV must stay disabled after cancel");
        QVERIFY2(!next->isEnabled(), "NEXT must stay disabled after cancel");
        QVERIFY2(!exportBtn->isEnabled(), "export must stay disabled after cancel");
        QVERIFY2(swap->isEnabled(), "swap must be re-enabled after cancel");

        // Idempotent re-run after cancel: same pair, no hook → completes.
        mode.setStageBoundaryHookForTest({});
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);
        QVERIFY2(status->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("status after re-run: %1").arg(status->text())));
        QCOMPARE(mode.lastResult().pages.size(), 2);
        QCOMPARE(widget->changeCount(), 1);
    }

    // ── §4 row 7 (findings-tests 2026-10-02): re-entry while running refused ──
    // CompareMode.cpp's guard (`if (m_watcher.isRunning()) return;`) is the
    // row's "re-entry refused" contract — testing-specialist wave-2b §4.3:
    // a deleted guard passed all nine pins because no pin drove a second
    // compareFiles while one ran (the cancel pin only re-runs AFTER
    // completion). This pin parks the first run at its first extraction
    // boundary, fires a second compareFiles with a DIFFERENT pair, and pins
    // the refusal three ways:
    //   1. no second progress dialog is created,
    //   2. the files label still names the FIRST pair (re-entry would have
    //      overwritten it with the second pair's names),
    //   3. the applied result is the FIRST pair's shape — a one-word text
    //      edit over 2 aligned pages (no structural rows). The refused pair
    //      is a page INSERTION (3 pages, one structural row): with the guard
    //      deleted, the second call detaches the watcher from the running
    //      future, starts the insertion pair, and ITS result wins the UI —
    //      red on the lastResult shape, the label and the dialog count.
    // (The first pair must not be byte-identical: the engine's streaming-hash
    // short-circuit resolves identical bytes before the first extraction
    // boundary, and the park would never fire.)
    void reentryWhileRunningIsRefused()
    {
        // First pair: the stages fixture's one-word edit → "1 CHANGES",
        // 2 aligned pages, zero structural rows.
        const QString a = pagePdf("ree_a.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page")});
        const QString b = pagePdf("ree_b.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        // Second (refused) pair: a TRAILING PAGE INSERTION → 3 pages, one
        // structural PageAdded (the trailing-page fixture's shape).
        const QString c = pagePdf("ree_c.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot"),
            QStringLiteral("second page")});
        const QString d = pagePdf("ree_d.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot"),
            QStringLiteral("second page"),
            QStringLiteral("third page")});
        QVERIFY(!a.isEmpty() && !b.isEmpty() && !c.isEmpty() && !d.isEmpty());

        gp::CompareMode mode;
        // Heap-shared semaphores, captured BY VALUE into the hook (the hook
        // outlives this function whenever an upstream assertion fails while
        // the worker is parked — the bounded park then expires into a lambda
        // that still owns its semaphores, never into dangling stack).
        auto reached = QSharedPointer<QSemaphore>::create();
        auto wake = QSharedPointer<QSemaphore>::create();
        mode.setStageBoundaryHookForTest([&](int stage, int done) {
            if (stage == DiffEngine::ProgressExtractText && done == 0) {
                reached->release();
                wake->tryAcquire(1, 60000);   // bounded park (see stages test)
            }
        });
        mode.compareFiles(a, b);
        QVERIFY2(reached->tryAcquire(1, 30000),
                 "worker never reached the first extraction boundary");
        QVERIFY2(mode.isBusy(),
                 "the first diff must still be running while the worker is parked");

        // The REFUSED re-entry: a second compare while one runs.
        mode.compareFiles(c, d);

        // (1) No second progress dialog — a non-refused call constructs one.
        QCOMPARE(mode.findChildren<QProgressDialog*>(
                     QStringLiteral("cmpProgressDialog")).size(), 1);
        // (2) The compared scope still names the FIRST pair (cmpFilesLabel is
        // the U04 testable Old/New surface; re-entry overwrites it).
        auto* filesLabel = mode.findChild<QLabel*>(QStringLiteral("cmpFilesLabel"));
        QVERIFY2(filesLabel, "files label must carry objectName cmpFilesLabel");
        QVERIFY2(filesLabel->text().contains(QStringLiteral("ree_a.pdf"))
                     && filesLabel->text().contains(QStringLiteral("ree_b.pdf")),
                 qPrintable(QStringLiteral("the re-entry must be refused — the compared "
                              "scope still names the FIRST pair, got: %1")
                                .arg(filesLabel->text())));
        QVERIFY2(mode.isBusy(),
                 "the refused re-entry must not have disturbed the running job");

        // (3) The FIRST pair's result is the one applied: the text edit's
        // shape (2 pages, no structural rows) — never the insertion pair's
        // (3 pages, one PageAdded).
        wake->release();
        waitForDiffFinished(mode);
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
        const DiffResult& applied = mode.lastResult();
        QVERIFY2(applied.pages.size() == 2 && applied.pageChanges.isEmpty(),
                 qPrintable(QStringLiteral("the FIRST compare's result must be the one "
                              "applied — a second compareFiles while running must be "
                              "refused (the insertion pair would show 3 pages + 1 "
                              "structural row); got pages=%1 pageChanges=%2")
                                .arg(applied.pages.size())
                                .arg(applied.pageChanges.size())));
    }
    // ── R3-perf (audit finding 5): promise progress posts are throttled ───────
    // Every QPromise progress post is a queued cross-thread delivery to the
    // GUI thread plus a dialog repaint; posting one per page-pair boundary is
    // a repaint storm on a large comparison. The worker's stage-boundary hook
    // (and with it the engine's cancel-probe granularity) stays PER-BOUNDARY
    // — pinned unchanged below — while the promise posts are capped: at most
    // one per kProgressEveryNBoundaries boundaries, with a forced post at
    // every stage change and at every stage completion (a stage's range
    // switch and its final value must never be swallowed — the stage pins
    // above still observe them).
    void promisePostsThrottledPerBoundaryStorm()
    {
        // 6 pages per side, one word of page 0 changed (similarity 5/6 —
        // stays aligned, no structural noise). Extraction runs 6+6 per-page
        // boundaries + its completion, the 6 aligned pairs add 6 boundaries +
        // their completion: 20 hook calls in total.
        QStringList texts;
        for (int i = 0; i < 6; ++i)
            texts << QStringLiteral("page %1 alpha bravo charlie delta echo").arg(i);
        const QString base = pagePdf("throttle_base.pdf", texts);
        QStringList revised = texts;
        revised[0] = QStringLiteral("page 0 alpha bravo CHARLIE delta echo");
        const QString rev = pagePdf("throttle_rev.pdf", revised);
        QVERIFY(!base.isEmpty() && !rev.isEmpty());

        gp::CompareMode mode;
        int hookCalls = 0;
        mode.setStageBoundaryHookForTest([&hookCalls](int, int) { ++hookCalls; });
        mode.compareFiles(base, rev);
        waitForDiffFinished(mode);

        // Cancel-probe granularity UNCHANGED: the hook still fires at every
        // single boundary (pre- and post-throttle).
        QCOMPARE(hookCalls, 20);

        // THE pin (RED pre-fix): promise posts must fall below the boundary
        // count. Post shape with the per-8-boundaries cap: extraction posts
        // at boundaries 0 (stage change), 8 (cap), 12 (completion); pairs at
        // 0 (stage change) and 6 (completion) — exactly 5.
        QVERIFY2(mode.promiseReportCountForTest() < hookCalls,
                 "promise progress posts were not throttled: one queued "
                 "GUI-thread delivery per page-pair boundary");
        QCOMPARE(mode.promiseReportCountForTest(), 5);

        // The run completed honestly (throttling changed nothing else).
        QCOMPARE(mode.lastResult().pages.size(), 6);
    }

    // ── (a) two identical documents → no changes, isIdentical ────────────────

    void identicalDocumentsProduceNoChanges()
    {
        const QStringList texts = {
            QStringLiteral("alpha bravo charlie delta echo foxtrot"),
            QStringLiteral("second page"),
            QStringLiteral("third page")};
        const QString a = pagePdf("ident_a.pdf", texts);
        const QString b = pagePdf("ident_b.pdf", texts);   // deterministic bytes
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        // Real engine, direct: identical bytes short-circuit to isIdentical.
        DiffEngine engine;
        const DiffResult r = engine.compare(a, b);
        QVERIFY2(r.isIdentical, "identical documents must be flagged identical");
        QCOMPARE(r.pages.size(), 0);
        QCOMPARE(r.pageChanges.size(), 0);

        // Real mode, end-to-end through the async compareFiles path.
        gp::CompareMode mode;
        mode.compareFiles(a, b);
        waitForDiffFinished(mode);

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("FILES ARE IDENTICAL")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));

        QVERIFY(mode.lastResult().isIdentical);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY2(widget, "CompareMode must own a CompareWidget");
        QCOMPARE(widget->changeCount(), 0);

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY2(tree, "CHANGES tree must carry objectName cmpChangesTree");
        QCOMPARE(tree->topLevelItemCount(), 0);

        // PREV/NEXT stay disabled when there is nothing to navigate to.
        QToolButton* prev = nullptr;
        QToolButton* next = nullptr;
        const auto buttons = mode.findChildren<QToolButton*>();
        for (auto* btn : buttons) {
            if (btn->text().contains(QStringLiteral("PREV"))) prev = btn;
            if (btn->text().contains(QStringLiteral("NEXT"))) next = btn;
        }
        QVERIFY2(prev && next, "PREV/NEXT buttons must exist");
        QVERIFY2(!prev->isEnabled(), "PREV must stay disabled on identical files");
        QVERIFY2(!next->isEnabled(), "NEXT must stay disabled on identical files");

        // The full report says identical (never a fabricated change list).
        QVERIFY2(mode.buildTextReport().contains(
                     QStringLiteral("The documents are identical.")),
                 qPrintable(QStringLiteral("text report: %1").arg(mode.buildTextReport())));
        QVERIFY2(mode.buildHtmlReport().contains(
                     QStringLiteral("The documents are identical.")),
                 "html report must say the documents are identical");
    }

    // ── (b) text edit on one page → text-diff section + correct anchor ───────

    void textEditOnOnePageYieldsTextSectionAndAnchor()
    {
        // One word of ten changes: the page-alignment word-set similarity is
        // 9/11 ≈ 0.82 >= 0.80, so the page stays ALIGNED (no structural rows)
        // while the token diff still fires. (With only six words the union
        // 5/7 ≈ 0.71 would fall under the threshold and the edited page would
        // legitimately be reported as removed+added — pinned by design in
        // TestDiffEngine's alignment contract, not a defect.)
        const QString base = pagePdf("edit_base.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("third page")});
        const QString revised = pagePdf("edit_rev.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("third page")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Engine-level result: exactly one changed page, token-level only.
        const DiffResult& r = mode.lastResult();
        QVERIFY2(!r.isIdentical, "an edited document is not identical");
        QCOMPARE(r.pageCount1, 3);
        QCOMPARE(r.pageCount2, 3);
        QCOMPARE(r.pageChanges.size(), 0);   // alignment held — no structural rows
        QCOMPARE(r.pages.size(), 3);
        QVERIFY2(r.pages.at(0).textAdded.isEmpty()
                     && r.pages.at(0).textRemoved.isEmpty()
                     && r.pages.at(0).pixelDiffCount == 0,
                 "page 1 is untouched");
        QCOMPARE(r.pages.at(1).textAdded, QStringList{QStringLiteral("gulf")});
        QCOMPARE(r.pages.at(1).textRemoved, QStringList{QStringLiteral("golf")});
        QVERIFY2(r.pages.at(1).pixelDiffCount > 0,
                 "the rendered page with different text must differ in pixels");
        QVERIFY2(r.pages.at(2).textAdded.isEmpty()
                     && r.pages.at(2).textRemoved.isEmpty()
                     && r.pages.at(2).pixelDiffCount == 0,
                 "page 3 is untouched");

        // CHANGES tree: exactly the page-2 row.
        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("p.2"));
        QVERIFY(tree->topLevelItem(0)->data(0, gp::CompareMode::kHasTextRole).toBool());

        // Widget: one change in the shared sequence, anchored on page 2 of
        // BOTH sides (a token change exists on both sides of the page).
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 1);
        const auto anchor = widget->anchorAt(0);
        QCOMPARE(anchor.structuralIndex, -1);
        QCOMPARE(anchor.pageDiffIndex, 1);
        QCOMPARE(anchor.oldPage, 1);
        QCOMPARE(anchor.newPage, 1);

        // The text-diff section shows the added and removed tokens.
        auto* browser = widget->findChild<QTextBrowser*>();
        QVERIFY2(browser, "CompareWidget must own its text diff browser");
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("gulf")),
                 qPrintable(QStringLiteral("text panel: %1").arg(browser->toPlainText())));
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("golf")),
                 qPrintable(QStringLiteral("text panel: %1").arg(browser->toPlainText())));
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("Page 2")),
                 "the text-diff section must name the changed page");

        // Navigating the sequence lands both viewers on the changed page 2
        // (index 1) — a both-sides anchor moves both views.
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "nav label must carry objectName cmpNavLabel");
        widget->nextChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav label: %1").arg(nav->text())));
        auto* split = viewerSplitter(*widget);
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QCOMPARE(left->pageCount(), 3);
        QCOMPARE(right->pageCount(), 3);
        QTRY_COMPARE(left->currentPage(), 1);
        QTRY_COMPARE(right->currentPage(), 1);

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        // After a navigation the toolbar status tracks the shared-sequence
        // position (CHANGE x OF y); the total form ("N CHANGES") is pinned in
        // filteredExportHonorsChangeTypeFilter().
        QVERIFY2(status->text().contains(QStringLiteral("CHANGE 1 OF 1")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
    }

    // ── (c) trailing page added → structural anchor + next/prev + viewers ────

    void trailingPageAddedNavigatesToStructuralAnchor()
    {
        const QString base = pagePdf("add_base.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("second page")});
        const QString extended = pagePdf("add_ext.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});   // shares no words → clean PageAdded
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        // Real engine, direct: the trailing page is a structural PageAdded.
        DiffEngine engine;
        const DiffResult r = engine.compare(base, extended);
        QCOMPARE(r.pageChanges.size(), 1);
        QCOMPARE(r.pageChanges.first().type, DiffResult::PageChangeType::PageAdded);
        QCOMPARE(r.pageChanges.first().oldPage, -1);
        QCOMPARE(r.pageChanges.first().newPage, 2);

        // Real mode, end-to-end.
        gp::CompareMode mode;
        mode.compareFiles(base, extended);
        waitForDiffFinished(mode);

        QCOMPARE(mode.lastResult().pageChanges.size(), 1);
        QCOMPARE(mode.lastResult().pages.size(), 2);   // min(base, ext)

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QTreeWidgetItem* added = tree->topLevelItem(0);
        QVERIFY2(added->text(2).contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("tree row: %1").arg(added->text(2))));
        QVERIFY(added->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool());
        QCOMPARE(added->data(0, gp::CompareMode::kAnchorIndexRole).toInt(), 0);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 1);
        const auto anchor = widget->anchorAt(0);
        QCOMPARE(anchor.structuralIndex, 0);
        QCOMPARE(anchor.pageDiffIndex, -1);
        QCOMPARE(anchor.oldPage, -1);   // missing side, never page-0 sentinel
        QCOMPARE(anchor.newPage, 2);

        // Selecting the structural CHANGES tree row navigates the shared
        // sequence (CompareMode wiring, not just the raw widget).
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY(nav);
        tree->setCurrentItem(added);
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after tree selection: %1").arg(nav->text())));

        // next/prev wrap on a one-change sequence — the change stays selected
        // and the viewers stay on the pages the change names.
        widget->prevChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after prev: %1").arg(nav->text())));
        widget->nextChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after next: %1").arg(nav->text())));

        // Viewers end on the right pages: the revised side follows the change
        // to the added page 3 (index 2); the original side has no page 3 — it
        // stays put (no bogus navigation) and explains itself with the
        // missing-side placeholder.
        auto* split = viewerSplitter(*widget);
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QCOMPARE(left->pageCount(), 2);
        QCOMPARE(right->pageCount(), 3);
        QTRY_COMPARE(right->currentPage(), 2);
        QCOMPARE(left->currentPage(), 0);

        auto* leftPh = widget->findChild<QLabel*>(QStringLiteral("cmpLeftPlaceholder"));
        QVERIFY2(leftPh, "cmpLeftPlaceholder must exist");
        QVERIFY2(leftPh->isVisibleTo(widget),
                 "an added page must show the missing-side placeholder on the original viewer");
        QVERIFY2(leftPh->text().contains(QStringLiteral("added in the revised document")),
                 qPrintable(QStringLiteral("left placeholder: %1").arg(leftPh->text())));
    }

    // ── (d) export report contains the changed page references ───────────────

    void exportReportsContainChangedPageReferences()
    {
        // Real combined scenario: a token edit on page 1 AND a trailing page.
        // (Ten words so the edited page stays word-set aligned: 9/11 ≈ 0.82.)
        const QString base = pagePdf("rep_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("rep_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Sanity: one token page row + one structural added page.
        QCOMPARE(mode.lastResult().pageChanges.size(), 1);
        QCOMPARE(mode.lastResult().pages.size(), 2);
        QVERIFY(mode.lastResult().pages.at(0).pixelDiffCount > 0
                || !mode.lastResult().pages.at(0).textAdded.isEmpty());

        const QString txt = mode.buildTextReport();
        QVERIFY2(txt.contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("structural reference missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("--- Page 1 ---")),
                 qPrintable(QStringLiteral("token page reference missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("+ gulf")),
                 qPrintable(QStringLiteral("added token missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("- golf")),
                 qPrintable(QStringLiteral("removed token missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("2 -> 3")),
                 qPrintable(QStringLiteral("page-count transition missing: %1").arg(txt)));

        const QString html = mode.buildHtmlReport();
        QVERIFY2(html.contains(QStringLiteral("Page 3 added in revised document")),
                 "html report must name the added page");
        QVERIFY2(html.contains(QStringLiteral("gulf")) && html.contains(QStringLiteral("golf")),
                 "html report must carry the edited page's tokens");
        QVERIFY2(html.contains(QStringLiteral("<h2>Page 1</h2>")),
                 "html report must carry a section for the changed page");
    }

    // ── (e) filter on → export honors it ─────────────────────────────────────

    void filteredExportHonorsChangeTypeFilter()
    {
        const QString base = pagePdf("flt_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("flt_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Backward-compatible no-arg overloads: the full (all-on) report.
        QVERIFY2(mode.buildTextReport().contains(QStringLiteral("gulf")),
                 "full text report keeps the token page");
        QVERIFY2(mode.buildTextReport().contains(QStringLiteral("Page 3 added in revised document")),
                 "full text report keeps the structural entry");

        // Pure overload: hide added/removed pages → structural entry drops,
        // the token page row stays.
        CompareChangeFilter noAddRemove;
        noAddRemove.showPageAddRemove = false;
        const QString txt = mode.buildTextReport(noAddRemove);
        QVERIFY2(!txt.contains(QStringLiteral("added in revised document")),
                 qPrintable(QStringLiteral("filtered text report leaked the added page: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("+ gulf")),
                 "token page row stays in scope when only add/rm is gated off");

        const QString html = mode.buildHtmlReport(noAddRemove);
        QVERIFY2(!html.contains(QStringLiteral("added in revised document")),
                 "filtered html report leaked the added page");
        QVERIFY2(html.contains(QStringLiteral("gulf")),
                 "html report keeps the token page in scope");

        // Everything off: the report says no changes match — never "identical".
        CompareChangeFilter none;
        none.showText = none.showMove = none.showPixel = false;
        none.showPageMove = none.showPageAddRemove = false;
        const QString emptyTxt = mode.buildTextReport(none);
        QVERIFY2(emptyTxt.contains(QStringLiteral("No changes match the filter.")),
                 qPrintable(QStringLiteral("empty text report: %1").arg(emptyTxt)));
        QVERIFY2(!emptyTxt.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "an emptied report must never claim the files are identical");
        const QString emptyHtml = mode.buildHtmlReport(none);
        QVERIFY2(emptyHtml.contains(QStringLiteral("No changes match the filter.")),
                 "empty html report must say no changes match");
        QVERIFY2(!emptyHtml.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "empty html report must never claim identical");

        // UI path: the filter toggles ARE the export scope (onExportReport
        // exports currentFilter()). The widget's changeFilter() is the public
        // observable of that state, and the status total tracks it live.
        auto* addRmBtn = mode.findChild<QToolButton*>(QStringLiteral("cmpFilterPageAddRemove"));
        QVERIFY2(addRmBtn, "cmpFilterPageAddRemove toggle must exist");
        addRmBtn->setChecked(false);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QVERIFY2(!widget->changeFilter().showPageAddRemove,
                 "the widget must observe the same filter state as the toggles");
        QCOMPARE(widget->changeCount(), 1);   // only the token page row survives

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("status after add/rm filter: %1").arg(status->text())));

        // What export would now write: no added page, token page still there.
        const QString exported = mode.buildTextReport(widget->changeFilter());
        QVERIFY2(!exported.contains(QStringLiteral("added in revised document")),
                 "export scope must drop the gated structural row");
        QVERIFY2(exported.contains(QStringLiteral("+ gulf")),
                 "export scope must keep the surviving token row");

        // All five toggles off → zero-scope state, honored end-to-end.
        for (const char* name : {"cmpFilterText", "cmpFilterMove", "cmpFilterPixel",
                                 "cmpFilterPageMove", "cmpFilterPageAddRemove"}) {
            auto* b = mode.findChild<QToolButton*>(QString::fromLatin1(name));
            QVERIFY2(b, qPrintable(QStringLiteral("%1 not found").arg(name)));
            b->setChecked(false);
        }
        QCOMPARE(widget->changeCount(), 0);
        QVERIFY2(status->text().contains(QStringLiteral("NO CHANGES MATCH THE FILTER")),
                 qPrintable(QStringLiteral("status fully filtered: %1").arg(status->text())));
        QVERIFY2(mode.buildTextReport(widget->changeFilter())
                     .contains(QStringLiteral("No changes match the filter.")),
                 "zero-scope export says no changes match");

        // Restore the shipped defaults.
        for (const char* name : {"cmpFilterText", "cmpFilterMove", "cmpFilterPixel",
                                 "cmpFilterPageMove", "cmpFilterPageAddRemove"}) {
            auto* b = mode.findChild<QToolButton*>(QString::fromLatin1(name));
            QVERIFY(b);
            b->setChecked(true);
        }
        QCOMPARE(widget->changeCount(), 2);
    }

    // ── R06 (PERF-01): ONE old/new mapping across engine, tree, navigation
    // and exports. The reviewer's insertion fixture: old [alpha, beta, gamma]
    // vs new [alpha, inserted, beta, gamma]. The page-insertion is detected
    // exactly once, the unchanged matched pages produce NO text/pixel/export
    // changes, and the tree, the navigable sequence and both report builders
    // agree on that.
    void middleInsertionAgreesAcrossEngineTreeNavigationAndReports()
    {
        const QString base = pagePdf("r06_base.pdf", {
            QStringLiteral("alpha page"),
            QStringLiteral("beta page"),
            QStringLiteral("gamma page")});
        const QString revised = pagePdf("r06_rev.pdf", {
            QStringLiteral("alpha page"),
            QStringLiteral("inserted page"),
            QStringLiteral("beta page"),
            QStringLiteral("gamma page")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        // Engine level: the mapping and the content rows agree.
        DiffEngine engine;
        const DiffResult direct = engine.compare(base, revised);
        QCOMPARE(direct.pageChanges.size(), 1);   // inserted page appears exactly once
        QCOMPARE(direct.pageChanges.first().type, DiffResult::PageChangeType::PageAdded);
        QCOMPARE(direct.pageChanges.first().newPage, 1);
        QCOMPARE(direct.pages.size(), 3);
        for (const auto& pd : direct.pages) {
            QVERIFY2(pd.textAdded.isEmpty() && pd.textRemoved.isEmpty()
                         && pd.moves.isEmpty() && pd.pixelDiffCount == 0,
                     "unchanged matched pages must produce no content changes");
        }

        // End-to-end through the real async mode.
        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // CHANGES tree: exactly the structural insertion row — no false
        // token rows (pre-fix: two spurious rows, "Beta → Inserted" and
        // "Gamma → Beta").
        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QVERIFY(tree->topLevelItem(0)->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool());

        // Navigation: exactly one change in the shared sequence, anchored on
        // the inserted page (no old side, new side = page index 1).
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 1);
        const auto anchor = widget->anchorAt(0);
        QCOMPARE(anchor.structuralIndex, 0);
        QCOMPARE(anchor.pageDiffIndex, -1);
        QCOMPARE(anchor.oldPage, -1);
        QCOMPARE(anchor.newPage, 1);

        // Exports agree: the insertion is named, and NO word/pixel change
        // sections exist anywhere in either report.
        const QString txt = mode.buildTextReport();
        QVERIFY2(txt.contains(QStringLiteral("Page 2 added in revised document")),
                 qPrintable(QStringLiteral("text report: %1").arg(txt)));
        const QStringList txtLines = txt.split(QLatin1Char('\n'));
        for (const QString& line : txtLines) {
            const QString t = line.trimmed();
            QVERIFY2(!t.startsWith(QLatin1Char('-')) && !t.startsWith(QLatin1Char('+'))
                         && !t.startsWith(QLatin1Char('~')),
                     qPrintable(QStringLiteral(
                                    "text report must not contain false word-change "
                                    "lines; got: %1").arg(t)));
        }
        QVERIFY2(!txt.contains(QStringLiteral("pixels differ")),
                 qPrintable(QStringLiteral("text report: %1").arg(txt)));

        const QString html = mode.buildHtmlReport();
        QVERIFY2(html.contains(QStringLiteral("Page 2 added in revised document")),
                 "html report must name the inserted page at its true position");
        QVERIFY2(!html.contains(QStringLiteral("<table class=\"diff\">")),
                 "html report must not contain false per-page word-diff tables");
    }
};

QTEST_MAIN(TestCompareIntegration)
#include "TestCompareIntegration.moc"
