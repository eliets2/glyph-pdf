/**
 * TestPagesMode — headless tests for PagesMode wiring.
 *
 * Tests:
 *   testPageRangeParser      — parsePageRange() with expression "1-3,5,7-9"
 *   testSplitAtPage          — executeSplit(): 5-page doc split at page 3 → 2 parts
 *   testSplitEveryNPages     — executeSplit(): 6-page doc split every 2 → 3 parts
 *   testReorderPages         — legacy sequential reorderPages() logic (simulation)
 *   testAtomicReorder        — AR-8 D5: reorderAllPages() called ONCE for whole permutation
 *   testMovePermutationConsolidation — §9.9 P0: single move = one permutation command
 *   testGridMovePermutation  — §9.9 P0: drag result → engine permutation math
 *   splitRangeExpressionProducesOnePreviewEntryPerSegment — §9.9 P1: range
 *                              expression "1-3,4-6,7" → one output part per
 *                              comma-separated segment (preview path)
 *   testPageRangeSegments_*   — §9.9 P1: parsePageRangeSegments() per-segment
 *                              groups (multi/single/invalid/overlap-clamp)
 *   splitRangeSegmentsWriteOneFilePerSegment — §9.9 P1: seam-derived groups
 *                              write one file per segment (sentinel content)
 *   internalMovePushesAtomicPermutation — U06: a real InternalMove sequence commits
 *                              exactly one atomic command (snapshot captured before
 *                              the drop copy is inserted) and no spurious reload command
 *   keyboardMoveUsesDragCommandPath  — U06 (a): Ctrl+Shift+Up/Down == equivalent drag
 *   selectionAndCurrentPageRestoredAfterUndo — U06 (b): undo restores order+selection+current
 *   selectionCountLabelReflectsSelection — U06 (c): "N pages selected · pages X-Y" label
 *   pageItemLabelsStayReadable — U06: page labels use the theme-token foreground
 *   gridContextMenuReusesExistingCommands — U06: context menu = same commands, no destructive entries
 *
 * All tests run with QT_QPA_PLATFORM=offscreen (no display required).
 * The PdfEditorEngine is mocked; real disk I/O uses QTemporaryDir.
 *
 * Run:
 *   QT_QPA_PLATFORM=offscreen ctest -R TestPagesMode --output-on-failure
 */

#include <QtTest/QtTest>
#include <QLabel>
#include <QApplication>
#include <QLineEdit>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QFile>
#include <QList>
#include <QStringList>
#include <QSharedPointer>
#include <QUndoStack>
#include <QMimeData>
#include <QListWidget>
#include <QMenu>
#include <QAction>
#include <algorithm>

#include "util/GpTheme.h"

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "modes/PagesMode.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"
#include "commands/ReorderPermutationCommand.h"
#include "shell/controllers/PagesController.h"

// ─────────────────────────────────────────────────────────────────────────────
// Extended mock that tracks reorderPages calls and simulates a multi-page doc.
// ─────────────────────────────────────────────────────────────────────────────

class PagesMock final : public MockPdfEditorEngine {
public:
    // Simulated page count (set by test before use)
    int m_pageCount = 0;

    // Track reorder calls: each entry is {from, to}
    struct ReorderCall { int from; int to; };
    QList<ReorderCall> m_reorderCalls;

    // Track extractPageAsBytes calls
    int m_extractCallCount = 0;

    // Track insertPageFromBytes calls
    int m_insertCallCount = 0;

    // Track deletePage calls
    int m_deleteCallCount = 0;

    // extractPageAsBytes: return non-empty bytes for valid page indices.
    QByteArray extractPageAsBytes(const QString& /*path*/, int pageIndex) override {
        ++m_extractCallCount;
        if (pageIndex >= 0 && pageIndex < m_pageCount) {
            // Return minimal page data bytes (sentinel value with index embedded)
            return QByteArray("page_") + QByteArray::number(pageIndex);
        }
        return {};
    }

    // insertPageFromBytes: always succeeds; write the bytes to the output file
    // so the test can verify page count by counting sentinel insertions.
    bool insertPageFromBytes(const QString& path, int /*atIndex*/,
                             const QByteArray& pageData) override {
        ++m_insertCallCount;
        // Append the sentinel to the output file for inspection
        QFile f(path);
        if (f.open(QIODevice::Append)) {
            f.write(pageData);
            f.write("\n");
            f.close();
        }
        return true;
    }

    bool deletePage(const QString& /*path*/, int /*pageIndex*/) override {
        ++m_deleteCallCount;
        return true;
    }

    bool reorderPages(const QString& /*path*/, int from, int to) override {
        m_reorderCalls.append({from, to});
        return true;
    }

    // AR-8 D5: track atomic permutation calls.
    QList<QList<int>> m_reorderAllCalls;

    bool reorderAllPages(const QString& /*path*/, const QList<int>& permutation) override {
        m_reorderAllCalls.append(permutation);
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: write a minimal stub PDF (delegates to PagesMode::writeMinimalPdf via
// the test-only public static exposed in the header).
// ─────────────────────────────────────────────────────────────────────────────

static bool writeStubPdf(const QString& path) {
    // Use the same logic as PagesMode::writeMinimalPdf by calling it via a
    // PagesMode instance (it is a public static method).
    return gp::PagesMode::writeMinimalPdf(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// U06 harness: real PagesMode + PagesMock context with N pages loaded. Returns
// the thumbnail grid (objectName "pagesGrid") once the async page-count query
// has populated it, or nullptr on failure.
// ─────────────────────────────────────────────────────────────────────────────
static QListWidget* setupPagesHarness(int pageCount, const QString& stem,
                                      QTemporaryDir& tmpDir,
                                      std::shared_ptr<PagesMock>& mock,
                                      std::shared_ptr<DocumentSession>& session,
                                      std::shared_ptr<QUndoStack>& undoStack,
                                      AppContext& ctx,
                                      gp::PagesMode& mode) {
    if (!tmpDir.isValid()) return nullptr;
    const QString srcPath = tmpDir.path() + "/" + stem;
    if (!writeStubPdf(srcPath)) return nullptr;

    mock = std::make_shared<PagesMock>();
    mock->m_pageCount = pageCount;
    mock->m_loaded    = true;

    session = std::make_shared<DocumentSession>();
    session->setPath(srcPath);
    undoStack = std::make_shared<QUndoStack>();

    ctx.pdfEditor = mock;
    ctx.document  = session;
    ctx.undoStack = undoStack;

    mode.setAppContext(&ctx);

    // The thumbnail grid is the one QListWidget in IconMode (the page list —
    // the preview and reorder lists use ListMode). Located by property, not by
    // a test-only objectName, so pre-fix runs fail on real behavior.
    QListWidget* grid = nullptr;
    for (QListWidget* lw : mode.findChildren<QListWidget*>()) {
        if (lw->viewMode() == QListView::IconMode) { grid = lw; break; }
    }
    if (!grid) return nullptr;
    // Wait for the async page-count query to populate the grid (QTRY_* macros
    // expand to a bare return, so they cannot be used in this QListWidget* fn).
    for (int i = 0; i < 100 && grid->count() != pageCount; ++i)
        QTest::qWait(50);
    if (grid->count() != pageCount) return nullptr;
    return grid;
}

// Sorted rows of the grid's current selection.
static QList<int> selectedRows(QListWidget* grid) {
    QList<int> rows;
    if (!grid || !grid->selectionModel()) return rows;
    for (const QModelIndex& idx : grid->selectionModel()->selectedIndexes())
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end());
    return rows;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test class
// ─────────────────────────────────────────────────────────────────────────────

class TestPagesMode : public QObject {
    Q_OBJECT

private slots:
    // §9.8+§9.9 P0: the local-first differentiator must be a single shared
    // seam and must actually be displayed on the page-management surface.
    void localFirstClaimSeamAndPanelLabel();

    // ── testPageRangeParser ────────────────────────────────────────────────
    void testPageRangeParser() {
        // "1-3,5,7-9" with 10 pages → 0-based [0,1,2,4,6,7,8]
        const QList<int> result = gp::PagesMode::parsePageRange("1-3,5,7-9", 10);
        QCOMPARE(result.size(), 7);
        QCOMPARE(result[0], 0);
        QCOMPARE(result[1], 1);
        QCOMPARE(result[2], 2);
        QCOMPARE(result[3], 4);
        QCOMPARE(result[4], 6);
        QCOMPARE(result[5], 7);
        QCOMPARE(result[6], 8);
    }

    void testPageRangeParser_single() {
        // "2" with 5 pages → [1]
        const QList<int> result = gp::PagesMode::parsePageRange("2", 5);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0], 1);
    }

    void testPageRangeParser_empty() {
        // Empty expression → []
        const QList<int> result = gp::PagesMode::parsePageRange("", 5);
        QVERIFY(result.isEmpty());
    }

    void testPageRangeParser_outOfRange() {
        // "1-100" with 5 pages → [0,1,2,3,4] (clamped)
        const QList<int> result = gp::PagesMode::parsePageRange("1-100", 5);
        QCOMPARE(result.size(), 5);
        QCOMPARE(result[0], 0);
        QCOMPARE(result[4], 4);
    }

    // ── testSplitAtPage ────────────────────────────────────────────────────
    void testSplitAtPage() {
        // 5-page doc, split at page 3 → 2 output files:
        //   part1: pages [0,1,2], part2: pages [3,4]
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a stub source PDF (content doesn't matter for mock)
        const QString srcPath = tmpDir.path() + "/source.pdf";
        QVERIFY(writeStubPdf(srcPath));

        // Set up context
        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 5;
        mock->m_loaded    = true;

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);

        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = mock;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        // Construct PagesMode and inject context
        gp::PagesMode mode;
        mode.setAppContext(&ctx);

        // AR-7 D2: refreshPageList now runs the page-count binary-search on a worker
        // thread (QtConcurrent). Wait for it to complete before resetting counters so
        // the extract calls from the background query are not mixed with the executeSplit
        // calls we are counting below.
        QTest::qWait(200);

        // Reset counters after setAppContext (which calls refreshPageList and performs
        // a binary-search via extractPageAsBytes to determine page count).
        mock->m_extractCallCount = 0;
        mock->m_insertCallCount  = 0;
        mock->m_deleteCallCount  = 0;

        // Build split groups manually: [[0,1,2], [3,4]]
        const QList<QList<int>> groups { {0, 1, 2}, {3, 4} };

        const QStringList produced = mode.executeSplit(
            srcPath, groups, tmpDir.path(), "{stem}_part{n}.pdf");

        QCOMPARE(produced.size(), 2);

        // Verify part1 was written
        const QString part1 = tmpDir.path() + "/source_part1.pdf";
        const QString part2 = tmpDir.path() + "/source_part2.pdf";
        QVERIFY(produced.contains(part1));
        QVERIFY(produced.contains(part2));

        // Verify extract was called for all 5 pages (3+2) — only counting executeSplit calls
        QCOMPARE(mock->m_extractCallCount, 5);

        // Verify insert was called for all 5 pages
        QCOMPARE(mock->m_insertCallCount, 5);

        // Verify delete was called once per output part (removing the stub page)
        QCOMPARE(mock->m_deleteCallCount, 2);
    }

    // ── testSplitEveryNPages ───────────────────────────────────────────────
    void testSplitEveryNPages() {
        // 6-page doc, split every 2 → 3 output files:
        //   part1: [0,1], part2: [2,3], part3: [4,5]
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString srcPath = tmpDir.path() + "/doc.pdf";
        QVERIFY(writeStubPdf(srcPath));

        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 6;
        mock->m_loaded    = true;

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);

        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = mock;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        gp::PagesMode mode;
        mode.setAppContext(&ctx);

        // AR-7 D2: wait for the async page-count query before resetting counters.
        QTest::qWait(200);

        // Reset counters after setAppContext (refreshPageList uses extractPageAsBytes)
        mock->m_extractCallCount = 0;
        mock->m_insertCallCount  = 0;
        mock->m_deleteCallCount  = 0;

        // Groups: split every 2 from 6-page doc
        const QList<QList<int>> groups { {0,1}, {2,3}, {4,5} };

        const QStringList produced = mode.executeSplit(
            srcPath, groups, tmpDir.path(), "{stem}_part{n}.pdf");

        QCOMPARE(produced.size(), 3);
        QVERIFY(produced.contains(tmpDir.path() + "/doc_part1.pdf"));
        QVERIFY(produced.contains(tmpDir.path() + "/doc_part2.pdf"));
        QVERIFY(produced.contains(tmpDir.path() + "/doc_part3.pdf"));

        // 6 extracts total (2 per part × 3 parts) — only counting executeSplit calls
        QCOMPARE(mock->m_extractCallCount, 6);
        // 6 inserts
        QCOMPARE(mock->m_insertCallCount, 6);
        // 3 deletes (one stub page per output file)
        QCOMPARE(mock->m_deleteCallCount, 3);
    }

    // ── testReorderPages ──────────────────────────────────────────────────
    void testReorderPages() {
        // Verify the parsePageRange round-trip for a reorder scenario.
        // 4 pages, desired new order: [3,0,1,2] (0-based).
        // We test that PagesMode::onApplyReorder generates the right reorderPages
        // calls by checking mock->m_reorderCalls after the apply.
        //
        // Since onApplyReorder is driven by the QListWidget drag state (not a
        // public API seam), we test the underlying logic by simulating it directly:
        // verify that the sequential-move algorithm produces the correct engine calls.
        //
        // Desired order: page 3 first, then 0, 1, 2.
        // Original:      [0, 1, 2, 3]
        // After move page@currentPos(3) → targetPos(0):  [3,0,1,2]  ← 1 call: reorderPages(3,0)
        // That produces [3,0,1,2] in one step — matches desired.

        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString srcPath = tmpDir.path() + "/reo.pdf";
        QVERIFY(writeStubPdf(srcPath));

        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 4;
        mock->m_loaded    = true;

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);

        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = mock;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        // Simulate the reorder algorithm (mirrors PagesMode::onApplyReorder logic)
        const QList<int> desiredOrder = {3, 0, 1, 2};
        QList<int> currentOrder = {0, 1, 2, 3};

        for (int targetPos = 0; targetPos < desiredOrder.size(); ++targetPos) {
            const int wantOriginal = desiredOrder[targetPos];
            int currentPos = currentOrder.indexOf(wantOriginal);
            if (currentPos < 0 || currentPos == targetPos) continue;

            mock->reorderPages(srcPath, currentPos, targetPos);
            currentOrder.move(currentPos, targetPos);
        }

        // Verify at least one reorder call was made
        QVERIFY(!mock->m_reorderCalls.isEmpty());

        // Verify the live order after all calls matches the desired order
        QCOMPARE(currentOrder, desiredOrder);

        // Verify first call moves original page 3 (at position 3) to position 0
        QCOMPARE(mock->m_reorderCalls[0].from, 3);
        QCOMPARE(mock->m_reorderCalls[0].to,   0);
    }

    // ── testAtomicReorder (AR-8 D5) ──────────────────────────────────────
    // Verify that ReorderPermutationCommand calls reorderAllPages() EXACTLY ONCE
    // (not N separate reorderPages() calls), and that undo() calls reorderAllPages()
    // with the correct inverse permutation.
    void testAtomicReorder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString srcPath = tmpDir.path() + "/atomic.pdf";
        QVERIFY(writeStubPdf(srcPath));

        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 4;
        mock->m_loaded    = true;

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);

        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = mock;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        // Desired permutation: [2, 0, 3, 1]
        // Inverse must be:     [1, 3, 0, 2] (inverse[perm[i]] = i)
        const QList<int> desired = {2, 0, 3, 1};
        const QList<int> expectedInverse = {1, 3, 0, 2};

        // Pre-fix check: reorderAllPages should NOT have been called yet.
        QVERIFY(mock->m_reorderAllCalls.isEmpty());
        // Pre-fix check: reorderPages (the old N-call path) also NOT called.
        QVERIFY(mock->m_reorderCalls.isEmpty());

        // Push the command (redo() is called immediately by QUndoStack::push).
        auto* cmd = new ReorderPermutationCommand(
            mock.get(), session.get(), desired);
        undoStack->push(cmd);

        // PASS check: reorderAllPages called ONCE with the desired permutation.
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(mock->m_reorderAllCalls[0], desired);

        // PASS check: the old N-call path was NOT used.
        QVERIFY(mock->m_reorderCalls.isEmpty());

        // Undo: should call reorderAllPages with the INVERSE permutation.
        undoStack->undo();
        QCOMPARE(mock->m_reorderAllCalls.size(), 2);
        QCOMPARE(mock->m_reorderAllCalls[1], expectedInverse);

        // Redo again: back to desired.
        undoStack->redo();
        QCOMPARE(mock->m_reorderAllCalls.size(), 3);
        QCOMPARE(mock->m_reorderAllCalls[2], desired);
    }

    // ── §9.9 P0: single-swap → atomic permutation consolidation ─────────
    // The legacy ReorderPageCommand path is retired; a drag-drop move must be
    // expressible as one permutation driving ONE reorderAllPages() call.
    void testMovePermutationConsolidation() {
        // buildMovePermutation: moving page 0 to position 2 in a 4-page doc.
        const QList<int> perm = gp::PagesController::buildMovePermutation(4, 0, 2);
        QCOMPARE(perm, QList<int>({1, 2, 0, 3}));

        // Invalid inputs yield an empty permutation (rejected by caller).
        QVERIFY(gp::PagesController::buildMovePermutation(4, -1, 0).isEmpty());
        QVERIFY(gp::PagesController::buildMovePermutation(4, 0, 4).isEmpty());
        QVERIFY(gp::PagesController::buildMovePermutation(0, 0, 0).isEmpty());

        // End-to-end through the command: exactly one reorderAllPages call,
        // zero legacy reorderPages calls, undo restores original order.
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString srcPath = tmpDir.path() + "/consolidated.pdf";
        QVERIFY(writeStubPdf(srcPath));

        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 4;
        mock->m_loaded    = true;
        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);
        auto undoStack = std::make_shared<QUndoStack>();

        auto* cmd = new ReorderPermutationCommand(mock.get(), session.get(), perm);
        undoStack->push(cmd);
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QVERIFY(mock->m_reorderCalls.isEmpty());
        undoStack->undo();
        QCOMPARE(mock->m_reorderAllCalls.size(), 2);
        // Inverse of [1,2,0,3] is [2,0,1,3].
        QCOMPARE(mock->m_reorderAllCalls[1], QList<int>({2, 0, 1, 3}));
    }

    // ── U06: Qt's InternalMove inserts the drop copy BEFORE removing the ──
    // source rows, so a snapshot captured at first removal contains the
    // duplicate and the permutation never computes. The grid must capture the
    // pre-drag order at the first model mutation (rowsAboutToBeInserted) and
    // the drag must land as ONE atomic reorderAllPages() call — with no
    // spurious second command from the reload.
    void internalMovePushesAtomicPermutation() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            4, "drag.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);

        QVERIFY(mock->m_reorderAllCalls.isEmpty()); // pre-condition: load pushed nothing

        // Simulate QAbstractItemView::startDrag for InternalMove: encode row 0,
        // drop it at the end (insert), then remove the source rows.
        const QModelIndex src = grid->model()->index(0, 0);
        QMimeData* md = grid->model()->mimeData({src});
        QVERIFY(md != nullptr);
        QVERIFY(grid->model()->dropMimeData(md, Qt::MoveAction, -1, 0, QModelIndex()));
        QVERIFY(grid->model()->removeRows(0, 1));

        // finishGridReorder runs queued — the drag must commit exactly once.
        QTRY_COMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(mock->m_reorderAllCalls[0], QList<int>({1, 2, 3, 0}));
        QVERIFY(mock->m_reorderCalls.isEmpty()); // legacy N-call path unused

        // The post-command reload must not push a second (spurious) command.
        QTest::qWait(150);
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(undoStack->count(), 1);
    }

    // ── U06 (a): keyboard moves produce the SAME permutation as the ──────
    // equivalent drag and travel through the same atomic command path.
    void keyboardMoveUsesDragCommandPath() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            4, "kbd.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);
        QVERIFY(mock->m_reorderAllCalls.isEmpty());

        // Move the page at row 1 down by one (Ctrl+Shift+Down handler).
        grid->setCurrentRow(1);
        // The equivalent drag: snapshot [0,1,2,3] → new visual order [0,2,1,3].
        const QList<int> expected =
            gp::PagesMode::gridMovePermutation({0, 1, 2, 3}, {0, 2, 1, 3});
        QCOMPARE(expected, QList<int>({0, 2, 1, 3}));

        QVERIFY(QMetaObject::invokeMethod(&mode, "moveSelectedPagesBy", Q_ARG(int, 1)));
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(mock->m_reorderAllCalls[0], expected);
        QVERIFY(mock->m_reorderCalls.isEmpty());   // legacy N-call path unused
        QCOMPARE(undoStack->count(), 1);           // exactly one atomic command

        // After the reload the moved page stays selected at its new row…
        QTRY_COMPARE(grid->currentRow(), 2);
        QCOMPARE(selectedRows(grid), QList<int>({2}));
        // …and the reload pushed no second (spurious) command.
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(undoStack->count(), 1);

        // Nonadjacent selection (plan acceptance): rows {0,2} move down.
        grid->selectionModel()->select(grid->model()->index(0, 0), QItemSelectionModel::Select);
        grid->selectionModel()->select(grid->model()->index(2, 0), QItemSelectionModel::Select);
        const QList<int> expected2 =
            gp::PagesMode::gridMovePermutation({0, 1, 2, 3}, {1, 0, 3, 2});
        QCOMPARE(expected2, QList<int>({1, 0, 3, 2}));
        QVERIFY(QMetaObject::invokeMethod(&mode, "moveSelectedPagesBy", Q_ARG(int, 1)));
        QCOMPARE(mock->m_reorderAllCalls.size(), 2);
        QCOMPARE(mock->m_reorderAllCalls[1], expected2);
        QTRY_VERIFY(grid->count() == 4);           // reload completed
        QTRY_COMPARE(grid->selectionModel()->selectedIndexes().size(), 2);
        QCOMPARE(selectedRows(grid), QList<int>({1, 3})); // pages followed the move
    }

    // ── U06 (b): undo of a grid reorder restores the page order, the ─────
    // selection, and the current page across the undo-triggered reload.
    void selectionAndCurrentPageRestoredAfterUndo() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            4, "undo.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);

        // Select the top two pages and move them down: [2,0,1,3].
        grid->setCurrentRow(0);
        grid->selectionModel()->select(grid->model()->index(1, 0), QItemSelectionModel::Select);
        QVERIFY(QMetaObject::invokeMethod(&mode, "moveSelectedPagesBy", Q_ARG(int, 1)));
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(mock->m_reorderAllCalls[0], QList<int>({2, 0, 1, 3}));

        // Selection and current page followed the move across the reload.
        QTRY_COMPARE(grid->selectionModel()->selectedIndexes().size(), 2);
        QCOMPARE(selectedRows(grid), QList<int>({1, 2}));
        QCOMPARE(grid->currentRow(), 1);

        // UNDO — document reverts via the command's inverse permutation…
        const int callsAfterMove = mock->m_reorderAllCalls.size();
        undoStack->undo();
        QCOMPARE(mock->m_reorderAllCalls.size(), callsAfterMove + 1);
        QCOMPARE(mock->m_reorderAllCalls[callsAfterMove], QList<int>({1, 2, 0, 3}));

        // …and the coalesced reload restores grid, selection, and current page.
        // (Wait on the rows themselves: the pre-undo selection also has size 2.)
        QTRY_COMPARE(selectedRows(grid), QList<int>({0, 1})); // the same two pages
        QCOMPARE(grid->currentRow(), 0);
        for (int i = 0; i < grid->count(); ++i)
            QCOMPARE(grid->item(i)->data(Qt::UserRole).toInt(), i); // original order

        // The undo-triggered reload pushed no extra command.
        QCOMPARE(mock->m_reorderAllCalls.size(), callsAfterMove + 1);
    }

    // ── U06 (c): the PAGE LIST header shows the selected-page count and ──
    // the affected range, live off the grid's selection.
    void selectionCountLabelReflectsSelection() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            4, "label.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);
        auto* label = mode.findChild<QLabel*>(QStringLiteral("pagesSelectionLabel"));
        QVERIFY2(label, "PagesMode must display a pagesSelectionLabel in the PAGE LIST header");

        QCOMPARE(label->text(), QString());          // nothing selected
        grid->setCurrentRow(3);
        QCOMPARE(label->text(), QStringLiteral("1 page selected · page 4"));
        grid->clearSelection();
        grid->selectionModel()->select(grid->model()->index(0, 0), QItemSelectionModel::Select);
        grid->selectionModel()->select(grid->model()->index(2, 0), QItemSelectionModel::Select);
        QCOMPARE(label->text(), QStringLiteral("2 pages selected · pages 1-3"));
        grid->clearSelection();
        QCOMPARE(label->text(), QString());
    }

    // ── U06: page-number labels use the theme-token foreground so they ───
    // stay readable on every theme background.
    void pageItemLabelsStayReadable() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            3, "labels.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);
        for (int i = 0; i < grid->count(); ++i) {
            const QListWidgetItem* it = grid->item(i);
            QCOMPARE(it->text(), QString("Page %1").arg(i + 1));
            QCOMPARE(it->foreground().color(), gp::Theme::fg0());
        }
    }

    // ── U06: thumbnail context actions reuse the grid's existing command ─
    // path and carry no destructive entries.
    void gridContextMenuReusesExistingCommands() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            4, "menu.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);

        QMenu menu;
        QVERIFY(QMetaObject::invokeMethod(&mode, "fillGridContextMenu", Q_ARG(QMenu*, &menu)));
        QStringList texts;
        for (QAction* a : menu.actions())
            if (!a->isSeparator()) texts << a->text();
        QVERIFY(texts.contains(QStringLiteral("Move Up")));
        QVERIFY(texts.contains(QStringLiteral("Move Down")));
        QVERIFY(texts.contains(QStringLiteral("Select All")));
        QVERIFY(texts.contains(QStringLiteral("Clear Selection")));
        for (const QString& t : texts) {
            QVERIFY2(!t.contains(QStringLiteral("Delete"), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("unexpected destructive entry: %1").arg(t)));
            QVERIFY2(!t.contains(QStringLiteral("Merge"), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("unexpected destructive entry: %1").arg(t)));
            QVERIFY2(!t.contains(QStringLiteral("Rotate"), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("unexpected destructive entry: %1").arg(t)));
        }

        // Without a selection the move actions are inert (no incidental clicks).
        grid->clearSelection();
        QMenu emptyMenu;
        QVERIFY(QMetaObject::invokeMethod(&mode, "fillGridContextMenu", Q_ARG(QMenu*, &emptyMenu)));
        for (QAction* a : emptyMenu.actions()) {
            if (a->text() == QStringLiteral("Move Up") ||
                a->text() == QStringLiteral("Move Down"))
                QVERIFY2(!a->isEnabled(),
                         qPrintable(QStringLiteral("%1 must be disabled without a selection")
                                        .arg(a->text())));
        }

        // "Move Up" drives the SAME atomic command path as drag/keyboard.
        grid->setCurrentRow(2);
        QMenu selMenu;
        QVERIFY(QMetaObject::invokeMethod(&mode, "fillGridContextMenu", Q_ARG(QMenu*, &selMenu)));
        QAction* moveUp = nullptr;
        for (QAction* a : selMenu.actions())
            if (a->text() == QStringLiteral("Move Up")) moveUp = a;
        QVERIFY(moveUp);
        QVERIFY(moveUp->isEnabled());
        moveUp->trigger();
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
        QCOMPARE(mock->m_reorderAllCalls[0],
                 gp::PagesMode::gridMovePermutation({0, 1, 2, 3}, {0, 2, 1, 3}));
        QTRY_VERIFY(grid->count() == 4); // reload settled without extra commands
        QCOMPARE(mock->m_reorderAllCalls.size(), 1);
    }

    // ── §9.9 P0: thumbnail-grid drag-and-drop permutation math ──────────
    void testGridMovePermutation() {
        // Drag page 0 to the end of [0,1,2,3]: new visual order [1,2,3,0]
        // → positions in snapshot: [1,2,3,0].
        QCOMPARE(gp::PagesMode::gridMovePermutation({0, 1, 2, 3}, {1, 2, 3, 0}),
                 QList<int>({1, 2, 3, 0}));
        // Swap two pages.
        QCOMPARE(gp::PagesMode::gridMovePermutation({0, 1, 2}, {1, 0, 2}),
                 QList<int>({1, 0, 2}));
        // No net change → empty (caller must not push a command).
        QVERIFY(gp::PagesMode::gridMovePermutation({0, 1, 2}, {0, 1, 2}).isEmpty());
        // Inconsistent input (unknown index / size mismatch) → empty.
        QVERIFY(gp::PagesMode::gridMovePermutation({0, 1, 2}, {0, 5, 2}).isEmpty());
        QVERIFY(gp::PagesMode::gridMovePermutation({0, 1}, {0}).isEmpty());
    }

    // ── §9.9 P1: a range expression "1-3,4-6,7" must produce ONE output ──
    // part per comma-separated segment (single-page segments included), not
    // one merged part for the whole expression. Verified through the real
    // preview slot so the contract covers computeSplitGroups, not just math.
    void splitRangeExpressionProducesOnePreviewEntryPerSegment() {
        std::shared_ptr<PagesMock> mock;
        std::shared_ptr<DocumentSession> session;
        std::shared_ptr<QUndoStack> undoStack;
        AppContext ctx;
        QTemporaryDir tmpDir;
        gp::PagesMode mode;
        QListWidget* grid = setupPagesHarness(
            10, "split.pdf", tmpDir, mock, session, undoStack, ctx, mode);
        QVERIFY(grid);

        // Select "Split by range" and enter a three-segment expression.
        QRadioButton* rangeRadio = nullptr;
        for (QRadioButton* rb : mode.findChildren<QRadioButton*>())
            if (rb->text() == QStringLiteral("Split by range:")) rangeRadio = rb;
        QVERIFY2(rangeRadio, "PagesMode must expose the 'Split by range:' radio");
        rangeRadio->setChecked(true);

        QLineEdit* rangeEdit = nullptr;
        for (QLineEdit* le : mode.findChildren<QLineEdit*>())
            if (le->placeholderText().startsWith(QStringLiteral("e.g. 1-3")))
                rangeEdit = le;
        QVERIFY2(rangeEdit, "PagesMode must expose the range expression edit");
        rangeEdit->setText(QStringLiteral("1-3,4-6,7"));

        // Drive the real preview slot; the preview list is the ListMode list
        // that displays the produced "…_part{n}.pdf" entries.
        QVERIFY(QMetaObject::invokeMethod(&mode, "onPreviewSplit"));
        QListWidget* preview = nullptr;
        for (QListWidget* lw : mode.findChildren<QListWidget*>()) {
            if (lw->viewMode() == QListView::ListMode && lw->count() > 0 &&
                lw->item(0)->text().contains(QStringLiteral("_part"))) {
                preview = lw;
                break;
            }
        }
        QVERIFY2(preview, "preview list must show the produced part files");
        QCOMPARE(preview->count(), 3); // one output per segment: 1-3 | 4-6 | 7
        QVERIFY(preview->item(0)->text().contains(QStringLiteral("split_part1.pdf")));
        QVERIFY(preview->item(1)->text().contains(QStringLiteral("split_part2.pdf")));
        QVERIFY(preview->item(2)->text().contains(QStringLiteral("split_part3.pdf")));
    }

    // ── §9.9 P1: per-segment range parser (pure seam) ─────────────────────
    void testPageRangeSegments_multi() {
        // "1-3,4-6,7" with 10 pages → three groups in segment order.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-3,4-6,7", 10);
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups[0], QList<int>({0, 1, 2}));
        QCOMPARE(groups[1], QList<int>({3, 4, 5}));
        QCOMPARE(groups[2], QList<int>({6}));
    }

    void testPageRangeSegments_singleSegment() {
        // One segment behaves like the old single-output split.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-5", 10);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0], QList<int>({0, 1, 2, 3, 4}));
    }

    void testPageRangeSegments_skipsInvalidAndEmpty() {
        // Invalid segments yield no group; valid neighbours survive.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-3,junk,5", 10);
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0], QList<int>({0, 1, 2}));
        QCOMPARE(groups[1], QList<int>({4}));
        QVERIFY(gp::PagesMode::parsePageRangeSegments("", 10).isEmpty());
        QVERIFY(gp::PagesMode::parsePageRangeSegments("junk", 10).isEmpty());
        QVERIFY(gp::PagesMode::parsePageRangeSegments("1-3", 0).isEmpty());
    }

    void testPageRangeSegments_overlapAndClamp() {
        // Overlapping segments are allowed (per-segment dedupe only) and
        // out-of-range values clamp per segment.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-2,2-4,100", 4);
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups[0], QList<int>({0, 1}));
        QCOMPARE(groups[1], QList<int>({1, 2, 3}));
        QCOMPARE(groups[2], QList<int>({3}));
    }

    // ── §9.9 P1: end-to-end — seam-derived groups write one file per ─────
    // segment through the SAME extract/insert/atomic machinery as before.
    void splitRangeSegmentsWriteOneFilePerSegment() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString srcPath = tmpDir.path() + "/range.pdf";
        QVERIFY(writeStubPdf(srcPath));

        auto mock = std::make_shared<PagesMock>();
        mock->m_pageCount = 10;
        mock->m_loaded    = true;

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);
        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = mock;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        gp::PagesMode mode;
        mode.setAppContext(&ctx);

        // AR-7 D2: let the async page-count query finish, then reset counters.
        QTest::qWait(200);
        mock->m_extractCallCount = 0;
        mock->m_insertCallCount  = 0;
        mock->m_deleteCallCount  = 0;

        // "1-3,4-6,7" → 3 groups → 3 output files.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-3,4-6,7", 10);
        QCOMPARE(groups.size(), 3);

        const QStringList produced = mode.executeSplit(
            srcPath, groups, tmpDir.path(), "{stem}_part{n}.pdf");
        QCOMPARE(produced.size(), 3);

        const QString part1 = tmpDir.path() + "/range_part1.pdf";
        const QString part2 = tmpDir.path() + "/range_part2.pdf";
        const QString part3 = tmpDir.path() + "/range_part3.pdf";
        QCOMPARE(produced, QStringList({part1, part2, part3}));

        // Each part carries exactly its segment's pages (PagesMock appends a
        // "page_<idx>" sentinel per inserted page).
        QFile f1(part1), f2(part2), f3(part3);
        QVERIFY(f1.open(QIODevice::ReadOnly));
        const QByteArray b1 = f1.readAll();
        f1.close();
        QVERIFY(b1.contains("page_0"));
        QVERIFY(b1.contains("page_1"));
        QVERIFY(b1.contains("page_2"));
        QVERIFY(!b1.contains("page_3"));

        QVERIFY(f2.open(QIODevice::ReadOnly));
        const QByteArray b2 = f2.readAll();
        f2.close();
        QVERIFY(b2.contains("page_3"));
        QVERIFY(b2.contains("page_4"));
        QVERIFY(b2.contains("page_5"));
        QVERIFY(!b2.contains("page_6"));

        QVERIFY(f3.open(QIODevice::ReadOnly));
        const QByteArray b3 = f3.readAll();
        f3.close();
        QVERIFY(b3.contains("page_6"));
        QVERIFY(!b3.contains("page_5"));

        // 7 extracts/inserts total (3+3+1), one stub delete per output file.
        QCOMPARE(mock->m_extractCallCount, 7);
        QCOMPARE(mock->m_insertCallCount, 7);
        QCOMPARE(mock->m_deleteCallCount, 3);
    }
};

void TestPagesMode::localFirstClaimSeamAndPanelLabel() {
    const QString claim = gp::PagesMode::localFirstClaim();
    QVERIFY2(claim.contains(QStringLiteral("100% local"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("seam must state 100%% local: %1").arg(claim)));
    QVERIFY2(claim.contains(QStringLiteral("no upload"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("seam must state no-upload: %1").arg(claim)));

    gp::PagesMode mode;
    auto* label = mode.findChild<QLabel*>(QStringLiteral("pagesLocalClaimLabel"));
    QVERIFY2(label, "PagesMode must display the local-first claim label");
    QCOMPARE(label->text(), claim);
}
QTEST_MAIN(TestPagesMode)
#include "TestPagesMode.moc"
