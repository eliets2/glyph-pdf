/**
 * TestBatchMode — D5 headless tests for BatchMode wiring.
 *
 * All tests run with QT_QPA_PLATFORM=offscreen (no display required).
 * Engines are mocked; real disk I/O uses QTemporaryDir.
 *
 * Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchMode --output-on-failure
 */
#include <QtTest/QtTest>
#include <atomic>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

#include "core/AppContext.h"
#include "core/interfaces/IConversionEngine.h"
#include "modes/BatchMode.h"
#include "engines/PatternRedactor.h" // §9.12 P1: preset keys cross-check
#include "mocks/MockPdfEditorEngine.h"

// ── Minimal mock IConversionEngine ─────────────────────────────────────────────

class MockConversionEngine : public IConversionEngine {
public:
    bool convertTo(const QString& pdfPath, const QString& outputPath,
                   TargetFormat /*format*/, const QVariantMap& /*options*/ = {}) override
    {
        ++m_callCount; // atomic: incremented from parallel QtConcurrent workers
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

    std::atomic<int> m_callCount{0};
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
        QCOMPARE(conv->m_callCount.load(), 3);
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
            std::atomic<int> calls{0};
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
        QCOMPARE(conv->calls.load(), 3);
        // 2 succeeded, 1 failed
        QCOMPARE(bm.successCount(), 2);
        QCOMPARE(bm.failCount(), 1);
        // ErrorLog captured the failure
        QVERIFY(bm.errorLogCount() > 0);
    }

    // ── T5b: Editor ops use a per-file engine (true parallelism) ─────────────
    // §9.12 P0: Compress/Watermark/ExportPdfA/Redact must each get a FRESH
    // per-file PdfEditorEngine instead of sharing one stateful engine behind a
    // single mutex (which secretly serialized 5 of 7 "parallel" ops). This test
    // drives the Compress op through the real engine and verifies the output
    // file is produced.
    void testCompressOpProducesOutput() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString src = createMinimalPdf(tmp.path(), "compress_src.pdf");
        QVERIFY(QFile::exists(src));

        // Real engine (not the mock) — the per-file engine path constructs a
        // PdfEditorEngine internally, so the AppContext engine is unused here.
        auto* editor = new MockPdfEditorEngine;
        AppContext ctx;
        ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(editor, [](auto*){});

        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(1); // OpCompress

        bm.onRunBatch();
        int waited = 0;
        while (bm.isBatchRunning() && waited < 5000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not complete within 5 seconds");

        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 0);
        // The per-file engine wrote a _compressed.pdf next to the source.
        QVERIFY2(QFile::exists(tmp.path() + "/compress_src_compressed.pdf"),
                 "Compress op must produce the output file via the per-file engine");
    }

    // ── §9.12 P1: Compress/Optimize target DPI must be user-configurable ─────
    // The batch Compress panel used to carry a hard "150 DPI" note and the
    // worker literally coded opts.targetDpi = 150 — no way to change it. The
    // panel must expose a target-DPI spin plus named Low/Medium/High presets.
    void compressDpiControlsExistWithDefaults() {
        gp::BatchMode bm;
        auto* spin = bm.findChild<QSpinBox*>(QStringLiteral("batchCompressDpiSpin"));
        QVERIFY2(spin, "Compress panel must expose a target-DPI spin box "
                       "(objectName batchCompressDpiSpin)");
        auto* preset = bm.findChild<QComboBox*>(QStringLiteral("batchCompressDpiPreset"));
        QVERIFY2(preset, "Compress panel must expose named DPI presets "
                         "(objectName batchCompressDpiPreset)");
        QVERIFY2(preset->count() >= 3,
                 "at least the Low/Medium/High DPI presets must be offered");
        // The previous hard-coded 150 stays the default — behavior only widens.
        QCOMPARE(spin->value(), 150);
    }

    // ── §9.12 P1: named PII redaction presets (quick picks) ──────────────────
    // The Redact batch op used to accept only free-form regex. Quick-pick
    // named PII presets (Email / Phone (US) / SSN) must exist, reusing the
    // PatternRedactor::namedPattern keys the interactive Redact mode offers.
    void namedRedactPresetCheckBoxesExist() {
        gp::BatchMode bm;
        auto* email = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_email"));
        auto* phone = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_phone-us"));
        auto* ssn   = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_ssn"));
        QVERIFY2(email, "named preset checkbox 'Email' missing (batchRedactPreset_email)");
        QVERIFY2(phone, "named preset checkbox 'Phone (US)' missing (batchRedactPreset_phone-us)");
        QVERIFY2(ssn,   "named preset checkbox 'SSN' missing (batchRedactPreset_ssn)");
        // Presets are opt-in — nothing is redacted the user did not ask for.
        QVERIFY(!email->isChecked());
        QVERIFY(!phone->isChecked());
        QVERIFY(!ssn->isChecked());
    }

    // ── §9.12 P1: named presets Low/Medium/High → 72/150/300 drive the spin ──
    void namedDpiPresetsDriveTheSpin() {
        gp::BatchMode bm;
        auto* preset = bm.findChild<QComboBox*>(QStringLiteral("batchCompressDpiPreset"));
        auto* spin   = bm.findChild<QSpinBox*>(QStringLiteral("batchCompressDpiSpin"));
        QVERIFY2(preset && spin, "DPI preset combo and spin must both exist");

        // The named presets carry the documented mapping Low/Medium/High →
        // 72/150/300 and selecting one writes it into the spin.
        const QList<int> expectedDpi = { 72, 150, 300 };
        QCOMPARE(preset->count(), expectedDpi.size() + 1); // + the Custom escape hatch
        for (int i = 0; i < expectedDpi.size(); ++i) {
            QCOMPARE(preset->itemData(i).toInt(), expectedDpi.at(i));
            preset->setCurrentIndex(i);
            QCOMPARE(spin->value(), expectedDpi.at(i));
        }

        // A manual spin edit leaves the named presets (combo flips to Custom,
        // item data -1) so the combo never lies about the spin's value.
        spin->setValue(100);
        QCOMPARE(preset->currentData().toInt(), -1);
    }

    // ── §9.12 P1: the DPI clamp seam — the worker's only path to targetDpi ───
    void resolveCompressTargetDpiClampsToSupportedRange() {
        // In-range values pass through unchanged (incl. all three presets).
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(72),  72);
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(150), 150);
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(300), 300);
        // Out-of-range values clamp into [kMinTargetDpi, kMaxTargetDpi].
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(0),     gp::BatchMode::kMinTargetDpi);
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(-100),  gp::BatchMode::kMinTargetDpi);
        QCOMPARE(gp::BatchMode::resolveCompressTargetDpi(10000), gp::BatchMode::kMaxTargetDpi);
        // The named constants pin the documented boundary.
        QCOMPARE(gp::BatchMode::kMinTargetDpi,    36);
        QCOMPARE(gp::BatchMode::kMaxTargetDpi,   600);
        QCOMPARE(gp::BatchMode::kDefaultTargetDpi, 150);
    }

    // ── §9.12 P1: preset keys resolve through PatternRedactor's built-ins ────
    void effectiveRedactPatternsResolvePresetsAndFreeForm() {
        // A named preset contributes the SAME regex body PatternRedactor
        // defines for that key (one source of truth, no re-typed patterns).
        const QRegularExpression ssn = PatternRedactor::namedPattern(QStringLiteral("ssn"));
        QVERIFY(ssn.isValid());
        QCOMPARE(gp::BatchMode::effectiveRedactPatterns({ QStringLiteral("ssn") }, {}),
                 QStringList{ ssn.pattern() });

        // Free-form entries pass through (trimmed).
        QCOMPARE(gp::BatchMode::effectiveRedactPatterns(
                     {}, { QStringLiteral(R"(\d{3}-\d{2}-\d{4})") }),
                 QStringList{ QStringLiteral(R"(\d{3}-\d{2}-\d{4})") });

        // Presets + free-form combined: presets first, unknown keys dropped,
        // duplicates collapsed.
        const QRegularExpression email = PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY(email.isValid());
        const QStringList combined = gp::BatchMode::effectiveRedactPatterns(
            { QStringLiteral("email"), QStringLiteral("no-such-preset-key") },
            { QStringLiteral("a+b"), QStringLiteral(" a+b ") });
        QCOMPARE(combined, (QStringList{ email.pattern(), QStringLiteral("a+b") }));

        // Nothing checked + nothing typed = nothing to redact.
        QVERIFY(gp::BatchMode::effectiveRedactPatterns({}, {}).isEmpty());
    }

    // ── §9.12 P1: the checkboxes feed the seam (widget wiring) ───────────────
    void redactPresetCheckBoxesFeedCheckedKeys() {
        gp::BatchMode bm;
        auto* email = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_email"));
        auto* phone = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_phone-us"));
        auto* ssn   = bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_ssn"));
        QVERIFY2(email && phone && ssn, "named PII preset checkboxes must exist");

        // All opt-in initially.
        QCOMPARE(bm.checkedRedactPresetKeys(), QStringList{});

        email->setChecked(true);
        ssn->setChecked(true);
        QCOMPARE(bm.checkedRedactPresetKeys(),
                 (QStringList{ QStringLiteral("email"), QStringLiteral("ssn") }));

        // And the checked keys resolve to the same regex bodies interactive
        // Redact mode uses — the wiring cannot drift from PatternRedactor.
        const QStringList resolved =
            gp::BatchMode::effectiveRedactPatterns(bm.checkedRedactPresetKeys(), {});
        QCOMPARE(resolved.size(), 2);
        QVERIFY(resolved.contains(PatternRedactor::namedPattern(QStringLiteral("email")).pattern()));
        QVERIFY(resolved.contains(PatternRedactor::namedPattern(QStringLiteral("ssn")).pattern()));
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
                QThread::msleep(250); // 250ms per file — ensures cancel fires before completion even on multi-core
                ++calls;
                QFile f(outputPath);
                if (f.open(QIODevice::WriteOnly)) { f.write("ok"); f.close(); }
                return true;
            }
            std::atomic<int> calls{0};
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
