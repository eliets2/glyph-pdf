// SPDX-License-Identifier: Apache-2.0
// TestBatchPresetsP2.cpp — batch-presets P2 (docs/research/batch-presets-p2-plan-
// 2026-09-21.md): the seven P1 residuals. P1 goldens and their pins stay in
// TestBatchPresets (byte-stable by design — kSchemaVersion stays 1); this suite
// pins the P2 semantics on top:
//   * U1 bates op + run-ordered continuity (the ordered lane)
//   * U2 onConflict "rename" through the W1-01 containment guard
//   * U3 onFileFailure "stop" (truthful not-run reporting)
//   * U4 batch-scoped failure abort (keeps committed files)
//   * U5 per-step measured-bytes report (JSON + CSV)
//   * U6 import/export as validated atomic copies
//   * U7 manager + multi-step editor dialogs
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchPresetsP2 --output-on-failure
#include <QtTest/QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPdfDocument>
#include <QSemaphore>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "core/BatchPreset.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "mocks/MockPdfEditorEngine.h"
#include "modes/BatchMode.h"

// PdfiumBackend's Windows headers #define DrawText -> DrawTextW, mangling
// PoDoFo's PdfPainter::DrawText below (the TestAutoBookmarks idiom).
#ifdef DrawText
#undef DrawText
#endif

using namespace gp;

// -- Fixtures (TestBatchPresets idioms) ----------------------------------------

static QString createTextPdf(const QString& dir, const QString& name,
                             const QStringList& pageTexts) {
    const QString path = dir + "/" + name;
    try {
        PoDoFo::PdfMemDocument doc;
        for (const QString& text : pageTexts) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(text.toUtf8().constData(), 50.0, 700.0);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createTextPdf failed:" << e.what();
        return {};
    }
    return path;
}

static bool writeStoreFile(const QString& dir, const QString& stem, const QByteArray& json) {
    QDir().mkpath(dir);
    QFile f(dir + "/" + stem + QStringLiteral(".glyphpreset.json"));
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(json) == json.size();
}

static AppContext makeCtx() {
    AppContext ctx;
    // The preset chain constructs its own PdfEditorEngine instances; the
    // context only satisfies the run gate.
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(new MockPdfEditorEngine, [](auto*) {});
    return ctx;
}

// A single-step bates preset with the given params JSON object body.
static QByteArray batesPresetJson(const QString& id, const QString& paramsJson) {
    return QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"%1\",\n"
        "    \"name\": \"Bates Runner\",\n"
        "    \"created\": \"2026-09-21T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-21T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"bates\", \"params\": %2 } ]\n"
        "}\n").arg(id, paramsJson).toUtf8();
}

// Extract all text of a document through the independent PDFium read path —
// the SAME seam the Bates cross-document tests use (never the writer).
static QString pageText(const QString& pdfPath, int page) {
    PdfiumBackend reader;
    if (!reader.loadDocument(pdfPath))
        return {};
    QString text;
    for (const auto& run : reader.extractPageTextRuns(page))
        text += run.text;
    return text;
}

// -- Test class -----------------------------------------------------------------

class TestBatchPresetsP2 : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    // ── U1: bates op + run-ordered continuity ────────────────────────────────
    void batesStepSchemaAdmittedAndRangeChecked();
    void batesOrderedLaneContinuity();
    void batesExplicitStartNumberPinsRunStart();
    void batesFaultAtFileTwoKeepsFileOneCommitted();
    void batesLaneCancelAtBoundaryIsTruthful();
    void batesLaneDisclosesOrderedRun();

private:
    std::unique_ptr<QTemporaryDir> m_storeDir;
    std::unique_ptr<QTemporaryDir> m_runDir;

    static void runAndWait(BatchMode& bm);
    QLineEdit* presetOutEdit(BatchMode& bm) const;
};

void TestBatchPresetsP2::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationVersion(QStringLiteral("1.4.0"));
}

void TestBatchPresetsP2::init() {
    m_storeDir = std::make_unique<QTemporaryDir>();
    m_runDir = std::make_unique<QTemporaryDir>();
    BatchMode::setPresetStoreDirForTest(m_storeDir->path());
}

void TestBatchPresetsP2::runAndWait(BatchMode& bm) {
    bool finished = false;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&finished] { finished = true; }, Qt::DirectConnection);
    bm.onRunBatch();
    int waited = 0;
    while (!finished && waited < 60000) {
        QTest::qWait(50);
        waited += 50;
    }
    QVERIFY2(finished, "Batch did not reach batchFinished within 60 seconds");
}

QLineEdit* TestBatchPresetsP2::presetOutEdit(BatchMode& bm) const {
    return bm.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
}

// ── U1 pins ────────────────────────────────────────────────────────────────────

// The v1 grammar already names `bates` (plan §2.2 param table); the P1 build
// refused it as not-implemented. P2 admits it — with the FULL range checks —
// while kSchemaVersion stays 1 (zero schema churn).
void TestBatchPresetsP2::batesStepSchemaAdmittedAndRangeChecked() {
    BatchPreset p;
    QString err;

    // The v1 param table: prefix/suffix strings <= 32, startNumber
    // 1-2,147,483,000, digitCount 1-12, six position values.
    QVERIFY2(BatchPresetCodec::parse(
        batesPresetJson(QStringLiteral("bates-full"),
                        QStringLiteral("{ \"prefix\": \"ABC-\", \"suffix\": \"-X\", "
                                       "\"startNumber\": 2147483000, \"digitCount\": 12, "
                                       "\"position\": \"top-center\" }")),
        &p, &err), qPrintable(QStringLiteral("bates preset refused: %1").arg(err)));
    QCOMPARE(p.steps.size(), 1);
    QCOMPARE(p.steps.first().op, QStringLiteral("bates"));

    // Defaults parse.
    QVERIFY2(BatchPresetCodec::parse(
        batesPresetJson(QStringLiteral("bates-default"), QStringLiteral("{}")), &p, &err),
        qPrintable(err));

    // Unknown param key inside a known op — still fail-closed (V2).
    QVERIFY(!BatchPresetCodec::parse(
        batesPresetJson(QStringLiteral("bates-unknown"),
                        QStringLiteral("{ \"fontFamily\": \"Times\" }")), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("steps[0].params.fontFamily")), qPrintable(err));

    // V4 range checks.
    const auto mustRefuse = [&p, &err](const QString& params, const QString& needle) {
        err.clear();
        QVERIFY2(!BatchPresetCodec::parse(
            batesPresetJson(QStringLiteral("bates-bad"), params), &p, &err),
            qPrintable(QStringLiteral("bates params %1 unexpectedly accepted").arg(params)));
        QVERIFY2(err.contains(needle), qPrintable(err));
    };
    mustRefuse(QStringLiteral("{ \"startNumber\": 0 }"), QStringLiteral("startNumber"));
    mustRefuse(QStringLiteral("{ \"startNumber\": 2147483001 }"), QStringLiteral("startNumber"));
    mustRefuse(QStringLiteral("{ \"digitCount\": 0 }"), QStringLiteral("digitCount"));
    mustRefuse(QStringLiteral("{ \"digitCount\": 13 }"), QStringLiteral("digitCount"));
    mustRefuse(QStringLiteral("{ \"position\": \"middle\" }"), QStringLiteral("position"));
    mustRefuse(QStringLiteral("{ \"prefix\": \"\", \"suffix\": \"1234567890123456789012345678901234\" }"),
               QStringLiteral("suffix"));   // 34 chars > 32
}

// The ordered lane: a bates-bearing preset runs sequentially in LIST ORDER and
// file n+1 continues at file n's lastNumberOut + 1 (N1 — continuity as a loop
// invariant). Black-box: the stamps are read back through the independent
// PDFium text path; white-box: the per-file step records carry first/last.
void TestBatchPresetsP2::batesOrderedLaneContinuity() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f2 = createTextPdf(fx, QStringLiteral("b.pdf"),
                                     { QStringLiteral("beta one"), QStringLiteral("beta two") });
    const QString f3 = createTextPdf(fx, QStringLiteral("c.pdf"), { QStringLiteral("gamma") });
    QVERIFY(!f1.isEmpty() && !f2.isEmpty() && !f3.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("bates-run"),
                           batesPresetJson(QStringLiteral("bates-run"),
                                           QStringLiteral("{ \"digitCount\": 3 }"))));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7 /* OpPresetPipeline */);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("bates-run")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1, f2, f3 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 3);
    QCOMPARE(bm.failCount(), 0);
    QCOMPARE(bm.remainingCount(), 0);

    // Black-box: file a (1 page) gets 001; file b (2 pages) gets 002, 003;
    // file c gets 004. List order, cross-file continuity, in-page advance.
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    QVERIFY2(pageText(outDir + QStringLiteral("/a_bates-run.pdf"), 0)
                 .contains(QStringLiteral("001")),
             qPrintable(pageText(outDir + QStringLiteral("/a_bates-run.pdf"), 0)));
    const QString b1 = pageText(outDir + QStringLiteral("/b_bates-run.pdf"), 0);
    const QString b2 = pageText(outDir + QStringLiteral("/b_bates-run.pdf"), 1);
    QVERIFY2(b1.contains(QStringLiteral("002")), qPrintable(b1));
    QVERIFY2(b2.contains(QStringLiteral("003")), qPrintable(b2));
    QVERIFY2(pageText(outDir + QStringLiteral("/c_bates-run.pdf"), 0)
                 .contains(QStringLiteral("004")),
             qPrintable(pageText(outDir + QStringLiteral("/c_bates-run.pdf"), 0)));

    // White-box: the per-file step records carry first/last bates numbers.
    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 3);
    QCOMPARE(results.at(0).steps.size(), 1);
    QCOMPARE(results.at(0).steps.first().firstBates, 1);
    QCOMPARE(results.at(0).steps.first().lastBates, 1);
    QCOMPARE(results.at(1).steps.first().firstBates, 2);
    QCOMPARE(results.at(1).steps.first().lastBates, 3);
    QCOMPARE(results.at(2).steps.first().firstBates, 4);
    QCOMPARE(results.at(2).steps.first().lastBates, 4);
}

// An explicit startNumber pins where the run's sequence STARTS (the first file
// of the run); subsequent files continue per §3.5 — and the report discloses
// the pinned start instead of silently re-basing.
void TestBatchPresetsP2::batesExplicitStartNumberPinsRunStart() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f2 = createTextPdf(fx, QStringLiteral("b.pdf"), { QStringLiteral("beta") });
    QVERIFY(!f1.isEmpty() && !f2.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("bates-100"),
                           batesPresetJson(QStringLiteral("bates-100"),
                                           QStringLiteral("{ \"startNumber\": 100, "
                                                          "\"digitCount\": 3 }"))));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("bates-100")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1, f2 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 2);

    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    QVERIFY(pageText(outDir + QStringLiteral("/a_bates-100.pdf"), 0).contains(QStringLiteral("100")));
    QVERIFY(pageText(outDir + QStringLiteral("/b_bates-100.pdf"), 0).contains(QStringLiteral("101")));

    const auto results = bm.runResultsForTest();
    QCOMPARE(results.at(0).steps.first().firstBates, 100);
    QCOMPARE(results.at(1).steps.first().firstBates, 101);
}

// A bates fault at file 2 (corrupt input): file 1 stays committed WITH its
// numbers, the failed file consumed NOTHING (file 3 continues from file 1's
// last + 1 — a discarded candidate never burns numbers), no temp residue, and
// the failed accounting is honest.
void TestBatchPresetsP2::batesFaultAtFileTwoKeepsFileOneCommitted() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f3 = createTextPdf(fx, QStringLiteral("c.pdf"), { QStringLiteral("gamma") });
    QVERIFY(!f1.isEmpty() && !f3.isEmpty());
    const QString bad = fx + QStringLiteral("/bad.pdf");
    {
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf at all");
    }
    const QByteArray f1Original = QFile(f1).readAll();

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("bates-run"),
                           batesPresetJson(QStringLiteral("bates-run"),
                                           QStringLiteral("{ \"digitCount\": 3 }"))));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("bates-run")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1, bad, f3 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 2);   // files 1 and 3
    QCOMPARE(bm.failCount(), 1);      // file 2 (corrupt)
    QCOMPARE(bm.remainingCount(), 0);

    // File 1's input is byte-identical; its committed output carries 001.
    QCOMPARE(QFile(f1).readAll(), f1Original);
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    QVERIFY(pageText(outDir + QStringLiteral("/a_bates-run.pdf"), 0).contains(QStringLiteral("001")));
    // File 3 continues from file 1's last number + 1 — the FAILED file never
    // burned a number.
    QVERIFY2(pageText(outDir + QStringLiteral("/c_bates-run.pdf"), 0)
                 .contains(QStringLiteral("002")),
             qPrintable(pageText(outDir + QStringLiteral("/c_bates-run.pdf"), 0)));
    // The corrupt file produced no output.
    QVERIFY(!QFileInfo::exists(outDir + QStringLiteral("/bad_bates-run.pdf")));

    // No temp residue: exactly the two committed outputs in the out dir.
    QCOMPARE(QDir(outDir).entryList(QStringList() << QStringLiteral("*.pdf"),
                                    QDir::Files).size(), 2);

    // Step records: file 2 failed with detail; file 3 continues at 2.
    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 3);
    QCOMPARE(results.at(1).success, false);
    QVERIFY(!results.at(1).steps.isEmpty());
    QCOMPARE(int(results.at(1).steps.first().status), int(BatchStepResult::Status::Failed));
    QVERIFY(!results.at(1).steps.first().detail.isEmpty());
    QCOMPARE(results.at(2).steps.first().firstBates, 2);
}

// Cancellation on the ordered lane is honored at FILE boundaries — never
// mid-chain: the file in flight (file 1) completes honestly, the cancel lands
// in the boundary window before file 2, files 2-3 are never attempted, and the
// U08 buckets add up (success + failed + skipped + remaining == file count).
void TestBatchPresetsP2::batesLaneCancelAtBoundaryIsTruthful() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    QStringList files;
    for (const QString& n : { QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c") }) {
        const QString p = createTextPdf(fx, n + QStringLiteral(".pdf"),
                                        { QStringLiteral("page ") + n });
        QVERIFY(!p.isEmpty());
        files << p;
    }

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("bates-run"),
                           batesPresetJson(QStringLiteral("bates-run"),
                                           QStringLiteral("{ \"digitCount\": 3 }"))));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("bates-run")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest(files);

    // The boundary hook parks the WORKER in the boundary window before file 2
    // until the GUI thread has cancelled — cancel lands BETWEEN files, never
    // mid-chain.
    QSemaphore boundaryReached;
    QSemaphore releaseBoundary;
    bm.setPresetBoundaryHookForTest([&](int fileIndex) {
        if (fileIndex == 1) {
            boundaryReached.release();
            releaseBoundary.acquire();   // bounded by the test's 60s window
        }
    });

    bool finished = false;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&finished] { finished = true; }, Qt::DirectConnection);
    bm.onRunBatch();
    bool cancelIssued = false;
    int waited = 0;
    while (!finished && waited < 60000) {
        QTest::qWait(50);
        waited += 50;
        if (!cancelIssued && boundaryReached.available() > 0) {
            boundaryReached.acquire();
            bm.onCancelBatch();          // GUI-thread entry (the button path)
            releaseBoundary.release();
            cancelIssued = true;
        }
    }
    QVERIFY2(finished, "Batch did not reach batchFinished within 60 seconds");
    QVERIFY2(cancelIssued, "the ordered lane never reached the file-2 boundary");

    QCOMPARE(bm.successCount(), 1);
    QCOMPARE(bm.failCount(), 0);
    // Files 2-3 were never attempted — the remaining bucket says so.
    QCOMPARE(bm.remainingCount(), 2);
    QCOMPARE(bm.successCount() + bm.failCount() + bm.skipCount() + bm.remainingCount(), 3);
    // Exactly one output exists (file 1's).
    QCOMPARE(QDir(m_runDir->filePath(QStringLiteral("out")))
                 .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files).size(), 1);
}

// The parallelism cost is disclosed BEFORE/AT the run: a bates-bearing
// preset's run log says the files are processed in order (the ordered lane).
void TestBatchPresetsP2::batesLaneDisclosesOrderedRun() {
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("bates-run"),
                           batesPresetJson(QStringLiteral("bates-run"),
                                           QStringLiteral("{ \"digitCount\": 3 }"))));
    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("bates-run")));

    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1 });

    runAndWait(bm);
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("in order")),
             qPrintable(QStringLiteral("ordered-lane disclosure missing from log: %1")
                            .arg(log.left(600))));
}

QTEST_MAIN(TestBatchPresetsP2)
#include "TestBatchPresetsP2.moc"
