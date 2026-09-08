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
 *   realSplitExecutionWritesSegmentFiles — N09 P1: REAL end-to-end split on a
 *                              real 5-page PDF through a real PdfEditorEngine
 *                              (resident source): 3 segments → 3 valid files
 *                              with the right pages, source untouched
 *   splitPreviewDerivesDistinctNamesWithoutNumberingToken — N09 P1: a pattern
 *                              without {n} must derive (and preview) distinct
 *                              final names; execution writes exactly those
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
 * The reorder/form tests mock the PdfEditorEngine; the N09 real-split test
 * drives a REAL PdfEditorEngine over real PDF bytes (real file reads).
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
#include <QMessageBox>
#include <QTimer>
#include <functional>
#include <memory>
#include <QUndoStack>
#include <QMimeData>
#include <QListWidget>
#include <QMenu>
#include <QAction>
#include <QSet>
#include <QPair>
#include <QPdfDocument>
#include <algorithm>

#include "util/GpTheme.h"

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "modes/PagesMode.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include "engines/podofo/PdfPageOps.h"
#include "mocks/MockPdfEditorEngine.h"
#include "commands/ReorderPermutationCommand.h"
#include "shell/controllers/PagesController.h"

// ─────────────────────────────────────────────────────────────────────────────
// N09: hand-built REAL N-page PDF (TestCompareIntegration idiom) — byte-exact
// xref, one distinct Helvetica literal "P<k>" per page, so page identity can
// be verified through the real library after a real round-trip.
// ─────────────────────────────────────────────────────────────────────────────
static QString createPagePdf(const QString& path, const QStringList& pageTexts)
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

// Read a whole file back (real file read, not a stub).
static QByteArray readFileBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

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
// N09: destination-engine mock that fails at load — models a destination
// engine that cannot open the candidate, pinning failure atomicity: no part
// may commit and existing outputs must stay byte-identical.
// ─────────────────────────────────────────────────────────────────────────────

class FailingLoadDestEngine final : public MockPdfEditorEngine {
public:
    bool loadDocumentForEditing(const QString&) override { return false; }
};

// ─────────────────────────────────────────────────────────────────────────────
// N09 harness: a REAL multi-page PDF loaded into a REAL PdfEditorEngine (the
// production resident-source state) driving a real PagesMode. Member order
// matters: the mode is destroyed before the context/engine/temp dir it points
// at.
// ─────────────────────────────────────────────────────────────────────────────
struct RealSplitHarness {
    QTemporaryDir tmpDir;
    std::shared_ptr<PdfEditorEngine> engine;
    std::shared_ptr<DocumentSession> session;
    std::shared_ptr<QUndoStack> undoStack;
    AppContext ctx;
    gp::PagesMode mode;
    QString srcPath;
    QListWidget* grid = nullptr;
};

static bool setupRealSplitHarness(const QStringList& pageTexts, const QString& stem,
                                  RealSplitHarness& h)
{
    if (!h.tmpDir.isValid()) return false;
    h.srcPath = h.tmpDir.path() + "/" + stem;
    if (createPagePdf(h.srcPath, pageTexts).isEmpty()) return false;

    h.engine = std::make_shared<PdfEditorEngine>();
    if (!h.engine->loadDocumentForEditing(h.srcPath)) return false;

    h.session = std::make_shared<DocumentSession>();
    h.session->setPath(h.srcPath);
    h.undoStack = std::make_shared<QUndoStack>();
    h.ctx.pdfEditor = h.engine;
    h.ctx.document  = h.session;
    h.ctx.undoStack = h.undoStack;

    h.mode.setAppContext(&h.ctx);

    for (QListWidget* lw : h.mode.findChildren<QListWidget*>())
        if (lw->viewMode() == QListView::IconMode) { h.grid = lw; break; }
    if (!h.grid) return false;
    // The production computeSplitGroups() reads the grid count, populated by
    // the async page-count query against the real engine.
    for (int i = 0; i < 200 && h.grid->count() != pageTexts.size(); ++i)
        QTest::qWait(50);
    return h.grid->count() == pageTexts.size();
}

// Reopen a produced part with an INDEPENDENT reader (pdfium via Qt PDF) and
// verify it carries exactly the expected source pages, in order (source page
// k carries the text "Pk"). Page-text equality subsumes the no-bleed check.
static void verifySplitPart(const QString& partPath, const QList<int>& sourcePages)
{
    QPdfDocument part;
    QVERIFY2(part.load(partPath) == QPdfDocument::Error::None,
             qPrintable(QStringLiteral("%1 must reopen as a valid PDF").arg(partPath)));
    QVERIFY2(part.pageCount() == sourcePages.size(),
             qPrintable(QStringLiteral("%1 must have exactly %2 pages")
                             .arg(partPath).arg(sourcePages.size())));
    for (int k = 0; k < sourcePages.size(); ++k) {
        const QString text = part.getAllText(k).text().trimmed();
        QVERIFY2(text == QStringLiteral("P%1").arg(sourcePages[k]),
                 qPrintable(QStringLiteral("%1 page %2 must carry P%3, got [%4]")
                                .arg(partPath).arg(k).arg(sourcePages[k]).arg(text)));
    }
}

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

private:
    // NCR-01 modal capture state (one completion dialog at a time; this suite
    // is linear). Members — not locals — because the budgeted capture driver
    // re-arms across event-loop turns.
    QString capturedModalText;

    void scheduleModalCapture(int turnsLeft) {
        if (turnsLeft <= 0) return;
        QTimer::singleShot(0, [this, turnsLeft] {
            if (auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                capturedModalText = box->text();
                box->close();
                return;
            }
            scheduleModalCapture(turnsLeft - 1);
        });
    }

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
    // N09: REAL execution — a real 5-page PDF loaded in a real PdfEditorEngine
    // (resident source), groups as the "split at page 3" form computes them
    // ([0,1,2] and [3,4]) → two REAL files with the right pages, source intact.
    void testSplitAtPage() {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1", "P2", "P3", "P4"}, "source.pdf", h));
        const QByteArray srcBytes = readFileBytes(h.srcPath);

        const QList<QList<int>> groups { {0, 1, 2}, {3, 4} };

        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), "{stem}_part{n}.pdf");

        QCOMPARE(produced.size(), 2);
        QCOMPARE(produced, QStringList({h.tmpDir.path() + "/source_part1.pdf",
                                        h.tmpDir.path() + "/source_part2.pdf"}));
        verifySplitPart(produced[0], {0, 1, 2});
        verifySplitPart(produced[1], {3, 4});

        // The loaded source survived: resident, extractable, bytes untouched.
        QCOMPARE(h.engine->currentFile(), h.srcPath);
        QVERIFY(!h.engine->extractPageAsBytes(h.srcPath, 4).isEmpty());
        QCOMPARE(readFileBytes(h.srcPath), srcBytes);
    }

    // ── testSplitEveryNPages ───────────────────────────────────────────────
    // N09: REAL execution — 6-page doc, split-every-2 groups ([0,1],[2,3],[4,5])
    // → three REAL 2-page files.
    void testSplitEveryNPages() {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1", "P2", "P3", "P4", "P5"}, "doc.pdf", h));
        const QByteArray srcBytes = readFileBytes(h.srcPath);

        const QList<QList<int>> groups { {0,1}, {2,3}, {4,5} };

        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), "{stem}_part{n}.pdf");

        QCOMPARE(produced.size(), 3);
        QCOMPARE(produced, QStringList({h.tmpDir.path() + "/doc_part1.pdf",
                                        h.tmpDir.path() + "/doc_part2.pdf",
                                        h.tmpDir.path() + "/doc_part3.pdf"}));
        verifySplitPart(produced[0], {0, 1});
        verifySplitPart(produced[1], {2, 3});
        verifySplitPart(produced[2], {4, 5});

        QCOMPARE(h.engine->currentFile(), h.srcPath);
        QCOMPARE(readFileBytes(h.srcPath), srcBytes);
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
    // segment through the REAL execution path (N09: real 10-page source,
    // real parts, page identity verified per file).
    void splitRangeSegmentsWriteOneFilePerSegment() {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness(
            {"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "P9"},
            "range.pdf", h));
        const QByteArray srcBytes = readFileBytes(h.srcPath);

        // "1-3,4-6,7" → 3 groups → 3 output files.
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments("1-3,4-6,7", 10);
        QCOMPARE(groups.size(), 3);

        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), "{stem}_part{n}.pdf");
        QCOMPARE(produced.size(), 3);

        QCOMPARE(produced, QStringList({h.tmpDir.path() + "/range_part1.pdf",
                                        h.tmpDir.path() + "/range_part2.pdf",
                                        h.tmpDir.path() + "/range_part3.pdf"}));
        verifySplitPart(produced[0], {0, 1, 2});
        verifySplitPart(produced[1], {3, 4, 5});
        verifySplitPart(produced[2], {6});

        QCOMPARE(h.engine->currentFile(), h.srcPath);
        QVERIFY(!h.engine->extractPageAsBytes(h.srcPath, 9).isEmpty());
        QCOMPARE(readFileBytes(h.srcPath), srcBytes);
    }

    // ── N09 P1: a pattern without the {n} token would generate the SAME ──
    // name for every part (silent overwrite of earlier parts). Both the
    // preview and the execution must derive distinct final names — and the
    // preview must show exactly the paths execution writes — and no derived
    // name may ever be the open source file itself.
    void splitPreviewDerivesDistinctNamesWithoutNumberingToken()
    {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1", "P2", "P3", "P4"}, "collide.pdf", h));

        QRadioButton* rangeRadio = nullptr;
        for (QRadioButton* rb : h.mode.findChildren<QRadioButton*>())
            if (rb->text() == QStringLiteral("Split by range:")) rangeRadio = rb;
        QVERIFY(rangeRadio);
        rangeRadio->setChecked(true);

        QLineEdit* rangeEdit = nullptr;
        for (QLineEdit* le : h.mode.findChildren<QLineEdit*>())
            if (le->placeholderText().startsWith(QStringLiteral("e.g. 1-3")))
                rangeEdit = le;
        QVERIFY(rangeEdit);
        rangeEdit->setText(QStringLiteral("1-3,4-6,7"));

        // The naming edit carries the default pattern as its text.
        QLineEdit* namingEdit = nullptr;
        for (QLineEdit* le : h.mode.findChildren<QLineEdit*>())
            if (le->text() == QStringLiteral("{stem}_part{n}.pdf")) namingEdit = le;
        QVERIFY2(namingEdit, "PagesMode must expose the output-name pattern edit");
        namingEdit->setText(QStringLiteral("fixed.pdf")); // no {n}: every part collides

        // Drive the real preview slot.
        QVERIFY(QMetaObject::invokeMethod(&h.mode, "onPreviewSplit"));
        QListWidget* preview = nullptr;
        for (QListWidget* lw : h.mode.findChildren<QListWidget*>()) {
            if (lw->viewMode() == QListView::ListMode && lw->count() > 0 &&
                lw->item(0)->text().contains(QStringLiteral("  ["))) {
                preview = lw;
                break;
            }
        }
        QVERIFY2(preview, "preview list must show the produced part files");
        QCOMPARE(preview->count(), 3);

        QStringList previewed;
        for (int i = 0; i < preview->count(); ++i)
            previewed.append(preview->item(i)->text().section(QStringLiteral("  ["), 0, 0));

        // Distinct names, none of them the source file. The FIRST part keeps
        // the requested name (it collides with nothing); every LATER part —
        // which would silently overwrite it under the old makeOutputName —
        // gets a derived, numbered name.
        QCOMPARE(previewed.size(), QSet<QString>(previewed.cbegin(), previewed.cend()).size());
        QVERIFY2(previewed[0] != h.srcPath,
                 qPrintable(QStringLiteral("output must not be the source: %1").arg(previewed[0])));
        QVERIFY(previewed[0].endsWith(QStringLiteral("fixed.pdf")));
        for (int i = 1; i < previewed.size(); ++i) {
            QVERIFY2(previewed[i] != h.srcPath,
                     qPrintable(QStringLiteral("output must not be the source: %1").arg(previewed[i])));
            QVERIFY2(previewed[i].contains(QStringLiteral("_part")),
                     qPrintable(QStringLiteral("derived name: %1").arg(previewed[i])));
            QVERIFY(previewed[i].endsWith(QStringLiteral(".pdf")));
        }

        // Execution writes exactly the previewed paths (honest preview).
        const QList<QList<int>> groups =
            gp::PagesMode::parsePageRangeSegments(QStringLiteral("1-3,4-6,7"), 5);
        QCOMPARE(groups.size(), 3);
        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), QStringLiteral("fixed.pdf"));
        QCOMPARE(produced, previewed);
        for (const QString& p : produced)
            QVERIFY(QFile::exists(p));
    }

    // ── N09 P1: a destination-engine failure must not commit anything and ──
    // must leave pre-existing output files byte-identical (the split writes
    // into a SafeSave candidate; the destination is only replaced by the
    // atomic commit of a fully validated part).
    void splitDestinationFailurePreservesExistingOutputs()
    {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1", "P2", "P3", "P4"}, "splitfail.pdf", h));

        // Pre-existing outputs the user confirmed to overwrite — they must
        // survive a failed run byte-identical.
        const QString part1 = h.tmpDir.path() + "/splitfail_part1.pdf";
        const QString part2 = h.tmpDir.path() + "/splitfail_part2.pdf";
        for (const QString& p : {part1, part2}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("PRE-EXISTING");
        }

        h.mode.setSplitEngineFactory(
            []() -> std::shared_ptr<IPdfEditorEngine> {
                return std::make_shared<FailingLoadDestEngine>();
            });

        const QList<QList<int>> groups { {0, 1, 2}, {3, 4} };
        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), "{stem}_part{n}.pdf");

        // Nothing committed; nothing clobbered.
        QCOMPARE(produced.size(), 0);
        QCOMPARE(readFileBytes(part1), QByteArray("PRE-EXISTING"));
        QCOMPARE(readFileBytes(part2), QByteArray("PRE-EXISTING"));

        // Source still resident and untouched.
        QCOMPARE(h.engine->currentFile(), h.srcPath);
        QVERIFY(!h.engine->extractPageAsBytes(h.srcPath, 4).isEmpty());
    }

    // ── N09 P1: REAL end-to-end split through the production execution ──
    // path. A real 5-page PDF is loaded into a real PdfEditorEngine (the
    // document is RESIDENT — the production state), the real range form
    // drives computeSplitGroups(), and executeSplit() (the execution the
    // onSplit() production caller runs after its preflight) must produce
    // three REAL files on disk with the right pages in order, while the
    // loaded source document and its on-disk bytes stay untouched. This is
    // the test the mocked split tests could not provide: the real backend's
    // anti-divergence guard (PoDoFoBackend::resolveDocument refuses to mutate
    // a second path while the source is resident) is in the loop.
    void realSplitExecutionWritesSegmentFiles()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Real 5-page PDF: page k carries the distinct literal "Pk".
        const QString srcPath = tmpDir.path() + "/real5.pdf";
        QVERIFY(!createPagePdf(srcPath, {"P0", "P1", "P2", "P3", "P4"}).isEmpty());
        const QByteArray srcBytes = readFileBytes(srcPath);
        QVERIFY(!srcBytes.isEmpty());

        // The production state: the source is loaded (resident) in the editor.
        auto engine = std::make_shared<PdfEditorEngine>();
        QVERIFY(engine->loadDocumentForEditing(srcPath));

        auto session = std::make_shared<DocumentSession>();
        session->setPath(srcPath);
        auto undoStack = std::make_shared<QUndoStack>();

        AppContext ctx;
        ctx.pdfEditor = engine;
        ctx.document  = session;
        ctx.undoStack = undoStack;

        gp::PagesMode mode;
        mode.setAppContext(&ctx);

        // Wait for the async page-count query to populate the real grid —
        // production computeSplitGroups() reads the grid's count.
        QListWidget* grid = nullptr;
        for (QListWidget* lw : mode.findChildren<QListWidget*>())
            if (lw->viewMode() == QListView::IconMode) { grid = lw; break; }
        QVERIFY(grid);
        for (int i = 0; i < 200 && grid->count() != 5; ++i)
            QTest::qWait(50);
        QCOMPARE(grid->count(), 5);

        // Drive the REAL production path: range form → computeSplitGroups →
        // executeSplit (the execution onSplit() calls after its preflight).
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
        rangeEdit->setText(QStringLiteral("1-2,3-4,5"));

        const QList<QList<int>> groups = mode.computeSplitGroups();
        QCOMPARE(groups.size(), 3); // segments {0,1} {2,3} {4}

        const QStringList produced = mode.executeSplit(
            srcPath, groups, tmpDir.path(), QStringLiteral("{stem}_part{n}.pdf"));

        // THE N09 CONTRACT: three real files, one per segment.
        if (produced.size() != 3)
            qWarning("real split produced %lld files: %s",
                     static_cast<long long>(produced.size()),
                     qPrintable(produced.join(QStringLiteral(", "))));
        QCOMPARE(produced.size(), 3);
        QCOMPARE(produced, QStringList({tmpDir.path() + "/real5_part1.pdf",
                                        tmpDir.path() + "/real5_part2.pdf",
                                        tmpDir.path() + "/real5_part3.pdf"}));
        QVERIFY(QFile::exists(tmpDir.path() + "/real5_part1.pdf"));
        QVERIFY(QFile::exists(tmpDir.path() + "/real5_part2.pdf"));
        QVERIFY(QFile::exists(tmpDir.path() + "/real5_part3.pdf"));

        // Each part reopens — through an INDEPENDENT reader (pdfium via Qt
        // PDF) — as a valid PDF carrying exactly its segment's pages in order.
        const QList<QPair<QString, QList<int>>> expected = {
            { QStringLiteral("real5_part1.pdf"), {0, 1} },
            { QStringLiteral("real5_part2.pdf"), {2, 3} },
            { QStringLiteral("real5_part3.pdf"), {4} },
        };
        for (const auto& e : expected)
            verifySplitPart(tmpDir.path() + "/" + e.first, e.second);

        // The open source survived the split: still resident, still
        // extractable, and its on-disk bytes are byte-identical (split is
        // never allowed to write the source).
        QCOMPARE(engine->currentFile(), srcPath);
        QVERIFY(!engine->extractPageAsBytes(srcPath, 4).isEmpty());
        QCOMPARE(readFileBytes(srcPath), srcBytes);
    }

    // ── NCR-01 (TEAM-NEW-COMMITS-REVIEW-2026-09-07, wave 4A): case-only ──
    // source aliases must be caught by the split output preflight. On the
    // case-insensitive Windows filesystem "SPLIT-SOURCE.pdf" IS the open
    // source "split-source.pdf"; the case-sensitive QSet treated them as
    // distinct, so part 1 targeted the OPEN SOURCE (its commit failed on the
    // loaded-file lock) and only part 2 was produced while the caller still
    // reported completion. The contract: two DISTINCT outputs with the right
    // pages, source bytes unchanged.
    void splitCaseOnlySourceAliasYieldsDistinctOutputsAndPreservesSource()
    {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1"}, "split-source.pdf", h));
        const QByteArray srcBytes = readFileBytes(h.srcPath);

        const QList<QList<int>> groups { {0}, {1} };
        const QStringList produced = h.mode.executeSplit(
            h.srcPath, groups, h.tmpDir.path(), QStringLiteral("SPLIT-SOURCE.pdf"));

        QCOMPARE(produced.size(), 2);
        // Both parts exist and are DISTINCT — neither is the source path.
        QVERIFY(produced[0] != produced[1]);
        for (const QString& p : produced) {
            QVERIFY2(!p.endsWith(QStringLiteral("/SPLIT-SOURCE.pdf")),
                     qPrintable(QStringLiteral("output must not alias the source: %1").arg(p)));
            QVERIFY2(QFile::exists(p),
                     qPrintable(QStringLiteral("part must exist: %1").arg(p)));
        }
        verifySplitPart(produced[0], {0});
        verifySplitPart(produced[1], {1});

        // The source file must be byte-identical and still two pages.
        QCOMPARE(readFileBytes(h.srcPath), srcBytes);
        verifySplitPart(h.srcPath, {0, 1});
    }

    // The preview consumes the same derivation as the execution, so it must
    // never promise a case-only alias of the open source.
    void splitPreviewDoesNotPromiseCaseAliasOfSource()
    {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1"}, "split-source.pdf", h));

        QRadioButton* atRadio = nullptr;
        for (QRadioButton* rb : h.mode.findChildren<QRadioButton*>())
            if (rb->text() == QStringLiteral("Split at page:")) atRadio = rb;
        QVERIFY(atRadio);
        atRadio->setChecked(true);

        QLineEdit* namingEdit = nullptr;
        for (QLineEdit* le : h.mode.findChildren<QLineEdit*>())
            if (le->text() == QStringLiteral("{stem}_part{n}.pdf")) namingEdit = le;
        QVERIFY2(namingEdit, "PagesMode must expose the output-name pattern edit");
        namingEdit->setText(QStringLiteral("SPLIT-SOURCE.pdf"));

        QVERIFY(QMetaObject::invokeMethod(&h.mode, "onPreviewSplit"));
        QListWidget* preview = nullptr;
        for (QListWidget* lw : h.mode.findChildren<QListWidget*>()) {
            if (lw->viewMode() == QListView::ListMode && lw->count() > 0 &&
                lw->item(0)->text().contains(QStringLiteral("  ["))) {
                preview = lw;
                break;
            }
        }
        QVERIFY2(preview, "preview list must show the produced part files");
        QCOMPARE(preview->count(), 2);
        for (int i = 0; i < preview->count(); ++i) {
            const QString entry = preview->item(i)->text().section(
                QStringLiteral("  ["), 0, 0);
            QVERIFY2(!entry.endsWith(QStringLiteral("/SPLIT-SOURCE.pdf")),
                     qPrintable(QStringLiteral("preview must not promise the source: %1").arg(entry)));
            QVERIFY2(entry.contains(QStringLiteral("_part")),
                     qPrintable(QStringLiteral("preview must show derived names: %1").arg(entry)));
        }
    }

    // NCR-01 second half: a FORCED failure in one of two parts must name the
    // failed part and must NOT be reported as a complete split. Driven
    // through the real onSplit() completion dialog (the completion message is
    // the reviewed defect surface); the modal driver captures its text and
    // closes it (offscreen: the Qt-internal message box is activeModalWidget).
    void splitForcedPartFailureNamesPartAndReportsPartialCompletion()
    {
        RealSplitHarness h;
        QVERIFY(setupRealSplitHarness({"P0", "P1", "P2", "P3"}, "partial.pdf", h));

        // Part 1 commits through a real engine; part 2's destination engine
        // cannot load the candidate — exactly one forced per-part failure.
        auto calls = std::make_shared<int>(0);
        h.mode.setSplitEngineFactory([calls]() -> std::shared_ptr<IPdfEditorEngine> {
            if ((*calls)++ == 0)
                return std::make_shared<PdfEditorEngine>();
            return std::make_shared<FailingLoadDestEngine>();
        });

        capturedModalText = QString();
        // Budgeted (turn-counted) capture driver: reacts only to the split
        // completion QMessageBox, never to a transient QProgressDialog.
        scheduleModalCapture(50);

        // The range expression "1-2,3-4" → two groups via the real UI state.
        QRadioButton* rangeRadio = nullptr;
        for (QRadioButton* rb : h.mode.findChildren<QRadioButton*>())
            if (rb->text() == QStringLiteral("Split by range:")) rangeRadio = rb;
        QVERIFY(rangeRadio);
        rangeRadio->setChecked(true);
        for (QLineEdit* le : h.mode.findChildren<QLineEdit*>())
            if (le->placeholderText().startsWith(QStringLiteral("e.g. 1-3")))
                le->setText(QStringLiteral("1-2,3-4"));

        QVERIFY(QMetaObject::invokeMethod(&h.mode, "onSplit"));

        // Wait until the completion box was captured (the split runs
        // synchronously on this thread; the box closes from the driver above).
        QTRY_VERIFY_WITH_TIMEOUT(!capturedModalText.isEmpty(), 10000);

        QVERIFY2(QFile::exists(h.tmpDir.path() + QStringLiteral("/partial_part1.pdf")),
                 "the healthy part must still be written (partial completion is real work)");
        QVERIFY2(!QFile::exists(h.tmpDir.path() + QStringLiteral("/partial_part2.pdf")),
                 "the failed part must not leave an output behind");

        // THE NCR-01 contract: the failed part is NAMED and the dialog does
        // not claim an unconditional "Split complete". Pre-fix the caller
        // showed "Split complete. 1 file(s) written" with no part named.
        QVERIFY2(!capturedModalText.contains(QStringLiteral("Split complete")),
                 qPrintable(QStringLiteral("a partial split must not claim completion: %1").arg(capturedModalText)));
        QVERIFY2(capturedModalText.contains(QStringLiteral("part 2")),
                 qPrintable(QStringLiteral("the failed part must be named: %1").arg(capturedModalText)));
        QVERIFY2(capturedModalText.contains(QStringLiteral("partial_part2.pdf")),
                 qPrintable(QStringLiteral("the failed part's path must be named: %1").arg(capturedModalText)));
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
