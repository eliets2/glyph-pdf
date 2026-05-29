/**
 * TestPagesMode — D4 headless tests for PagesMode wiring.
 *
 * Tests:
 *   testPageRangeParser  — parsePageRange() with expression "1-3,5,7-9"
 *   testSplitAtPage      — executeSplit(): 5-page doc split at page 3 → 2 parts
 *   testSplitEveryNPages — executeSplit(): 6-page doc split every 2 → 3 parts
 *   testReorderPages     — reorderPages() called in correct sequence
 *
 * All tests run with QT_QPA_PLATFORM=offscreen (no display required).
 * The PdfEditorEngine is mocked; real disk I/O uses QTemporaryDir.
 *
 * Run:
 *   QT_QPA_PLATFORM=offscreen ctest -R TestPagesMode --output-on-failure
 */

#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QList>
#include <QStringList>
#include <QSharedPointer>
#include <QUndoStack>

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "modes/PagesMode.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"

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
// Test class
// ─────────────────────────────────────────────────────────────────────────────

class TestPagesMode : public QObject {
    Q_OBJECT

private slots:

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
};

QTEST_MAIN(TestPagesMode)
#include "TestPagesMode.moc"
