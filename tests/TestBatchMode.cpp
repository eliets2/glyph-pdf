/**
 * TestBatchMode — D5 headless tests for BatchMode wiring.
 *
 * All tests run with QT_QPA_PLATFORM=offscreen (no display required).
 * Engines are mocked; real disk I/O uses QTemporaryDir.
 *
 * Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchMode --output-on-failure
 */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardItemModel>

#include "core/AppContext.h"
#include "core/interfaces/IConversionEngine.h"
#include "modes/BatchMode.h"
#include "mocks/MockPdfEditorEngine.h"

// ── Minimal mock IConversionEngine ─────────────────────────────────────────────

class MockConversionEngine : public IConversionEngine {
public:
    bool convertTo(const QString& pdfPath, const QString& outputPath,
                   TargetFormat /*format*/, const QVariantMap& /*options*/ = {}) override
    {
        ++m_callCount;
        m_lastInput = pdfPath;
        m_lastOutput = outputPath;

        if (m_shouldFail) return false;

        // Write a sentinel output file so the caller can verify it was created
        QFile f(outputPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("mock_converted");
            f.close();
        }
        return true;
    }

    int     m_callCount  = 0;
    QString m_lastInput;
    QString m_lastOutput;
    bool    m_shouldFail = false;
};

// ── Minimal valid single-page PDF (same fixture used across the test suite) ───

static QString createMinimalPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    // Byte-accurate xref — same layout as TestFormBuilder fixture
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    f.close();
    return path;
}

// ── Test class ─────────────────────────────────────────────────────────────────

class TestBatchMode : public QObject {
    Q_OBJECT

private:
    // Build a minimal AppContext wired with mock engines
    static AppContext makeCtx(MockPdfEditorEngine* editor, MockConversionEngine* conv) {
        AppContext ctx;
        ctx.pdfEditor  = std::shared_ptr<IPdfEditorEngine>(editor, [](auto*){});
        ctx.conversion = std::shared_ptr<IConversionEngine>(conv, [](auto*){});
        return ctx;
    }

private slots:

    // ── T1: File list population ──────────────────────────────────────────────
    // Programmatically add 3 file paths → verify m_filesToProcess has 3 entries.
    void testFileListPopulation() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString p1 = createMinimalPdf(tmp.path(), "a.pdf");
        const QString p2 = createMinimalPdf(tmp.path(), "b.pdf");
        const QString p3 = createMinimalPdf(tmp.path(), "c.pdf");
        QVERIFY(QFile::exists(p1));
        QVERIFY(QFile::exists(p2));
        QVERIFY(QFile::exists(p3));

        gp::BatchMode bm;
        QCOMPARE(bm.fileCount(), 0);

        bm.addFilesForTest({p1, p2, p3});
        QCOMPARE(bm.fileCount(), 3);
    }

    // ── T2: Duplicate detection ───────────────────────────────────────────────
    // Adding the same file twice should not duplicate it.
    void testDuplicateFileIgnored() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString p1 = createMinimalPdf(tmp.path(), "dup.pdf");
        QVERIFY(QFile::exists(p1));

        gp::BatchMode bm;
        bm.addFilesForTest({p1, p1});
        QCOMPARE(bm.fileCount(), 1);

        bm.addFilesForTest({p1});
        QCOMPARE(bm.fileCount(), 1);
    }

    // ── T3: Batch convert — 3 files → 3 output files created ─────────────────
    // Configure Convert operation, wire mock engines, trigger run.
    // Verify ConversionEngine::convertTo called once per file.
    void testBatchConvert() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create 3 source PDFs
        QStringList inputs;
        for (int i = 1; i <= 3; ++i)
            inputs << createMinimalPdf(tmp.path(), QString("src%1.pdf").arg(i));
        for (const auto& p : inputs)
            QVERIFY(QFile::exists(p));

        auto* editor = new MockPdfEditorEngine;
        auto* conv   = new MockConversionEngine;
        AppContext ctx = makeCtx(editor, conv);

        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest(inputs);
        QCOMPARE(bm.fileCount(), 3);

        // Run the batch asynchronously
        bm.onRunBatch();

        // Pump event loop until QFutureWatcher signals finished
        int waited = 0;
        while (bm.isBatchRunning() && waited < 5000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not complete within 5 seconds");

        // convertTo called exactly once per input file
        QCOMPARE(conv->m_callCount, 3);
        // All succeeded
        QCOMPARE(bm.successCount(), 3);
        QCOMPARE(bm.failCount(), 0);
        QCOMPARE(bm.errorLogCount(), 0);
    }

    // ── T4: Continue-on-failure — 2 valid + 1 bad file ────────────────────────
    // 2 succeed, 1 fails, ErrorLog captures failure, batch doesn't abort.
    void testBatchWithOneBadFile() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString good1 = createMinimalPdf(tmp.path(), "good1.pdf");
        const QString good2 = createMinimalPdf(tmp.path(), "good2.pdf");
        // "corrupt" file: exists on disk but mock will fail on it
        const QString bad   = createMinimalPdf(tmp.path(), "bad.pdf");
        QVERIFY(QFile::exists(good1));
        QVERIFY(QFile::exists(good2));
        QVERIFY(QFile::exists(bad));

        auto* editor = new MockPdfEditorEngine;
        // ConversionEngine that fails specifically on "bad.pdf"
        class SelFailConv : public IConversionEngine {
        public:
            bool convertTo(const QString& pdfPath, const QString& outputPath,
                           TargetFormat, const QVariantMap& = {}) override {
                ++calls;
                if (pdfPath.endsWith("bad.pdf")) return false;
                QFile f(outputPath);
                if (f.open(QIODevice::WriteOnly)) { f.write("ok"); f.close(); }
                return true;
            }
            int calls = 0;
        };
        auto* conv = new SelFailConv;
        AppContext ctx;
        ctx.pdfEditor  = std::shared_ptr<IPdfEditorEngine>(editor, [](auto*){});
        ctx.conversion = std::shared_ptr<IConversionEngine>(conv, [](auto*){});

        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({good1, bad, good2}); // bad is in the middle
        QCOMPARE(bm.fileCount(), 3);

        bm.onRunBatch();
        int waited = 0;
        while (bm.isBatchRunning() && waited < 5000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not complete within 5 seconds");

        // All 3 files processed (no abort on failure)
        QCOMPARE(conv->calls, 3);
        // 2 succeeded, 1 failed
        QCOMPARE(bm.successCount(), 2);
        QCOMPARE(bm.failCount(), 1);
        // ErrorLog captured the failure
        QVERIFY(bm.errorLogCount() > 0);
    }

    // ── T5: Cancel — batch stops before all files processed ──────────────────
    // Start batch with 6 files using a slow mock, cancel, verify not all completed.
    void testCancelBatch() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QStringList inputs;
        for (int i = 0; i < 6; ++i)
            inputs << createMinimalPdf(tmp.path(), QString("file%1.pdf").arg(i));
        for (const auto& p : inputs) QVERIFY(QFile::exists(p));

        auto* editor = new MockPdfEditorEngine;
        // Slow mock: introduces a small delay so cancel has time to take effect
        class SlowConv : public IConversionEngine {
        public:
            bool convertTo(const QString&, const QString& outputPath,
                           TargetFormat, const QVariantMap& = {}) override {
                QThread::msleep(80); // 80ms per file — give cancel a window
                ++calls;
                QFile f(outputPath);
                if (f.open(QIODevice::WriteOnly)) { f.write("ok"); f.close(); }
                return true;
            }
            int calls = 0;
        };
        auto* conv = new SlowConv;
        AppContext ctx;
        ctx.pdfEditor  = std::shared_ptr<IPdfEditorEngine>(editor, [](auto*){});
        ctx.conversion = std::shared_ptr<IConversionEngine>(conv, [](auto*){});

        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest(inputs);

        bm.onRunBatch();
        QTest::qWait(100); // let at most 1-2 files start
        bm.onCancelBatch();

        int waited = 0;
        while (bm.isBatchRunning() && waited < 5000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not terminate after cancel");

        // Fewer than all 6 files should have been processed
        int processed = bm.successCount() + bm.failCount();
        QVERIFY2(processed < 6,
            qPrintable(QString("Expected cancel to stop before all 6 files; got %1").arg(processed)));
    }
};

QTEST_MAIN(TestBatchMode)
#include "TestBatchMode.moc"
