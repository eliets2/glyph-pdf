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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPdfDocument>
#include <QSemaphore>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "core/BatchPreset.h"
#include "core/Capability.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "mocks/MockPdfEditorEngine.h"
#include "modes/BatchMode.h"
#include "ui/PresetEditorDialog.h"
#include "ui/PresetManagerDialog.h"

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

    // ── U2: onConflict "rename" + unattended ingest degrade ──────────────────
    void renamePolicySchemaAndRoundTrip();
    void renameConflictCreatesStem2KeepsOriginal();
    void renameSkipsOccupiedChainToFirstFree();
    void renameRaceBetweenStagingAndCommitRetries();
    void renameBoundedExhaustionFailsHonestly();
    void renameCandidatesStayContained();
    void hotFolderAutoRunDegradesAskToRename();

    // ── U3: onFileFailure "stop" (truthful not-run reporting) ─────────────────
    void stopPolicySchemaAdmittedAndRoundTrip();
    void stopAtFileTwoStopsAndReportsRemaining();
    void continueDefaultRunsAllFiles();

    // ── U4: batch-scoped failure abort (keeps committed files) ────────────────
    void batchAbortBeforeStartWhenOutputDirMissing();
    void batchAbortBeforeStartTouchesNothing();
    void batchScopedMidRunAbortsOrderedLaneAndDrainsRemainder();
    void batchAbortUnattendedNotesHotFolderStaysArmed();

    // ── U5: per-step measured-bytes report (JSON + CSV) ───────────────────────
    void measuredBytesTwoStepChainHandComputed();
    void runReportJsonShapePinned();
    void runReportCsvShapePinned();

    // ── U6: import/export as validated atomic copies ──────────────────────────
    void importExportRoundTripByteIdentical();
    void importRefusesIdStemMismatchKeepsStore();
    void importRefusesExistingIdUnlessReplaced();

    // ── U7: manager + multi-step editor dialogs ───────────────────────────────
    void managerDisclosesPresetsAndBrokenFiles();
    void managerDuplicateRenameDeleteFlows();
    void managerImportExportReplaceFlows();
    void editorClampsWidgetsAndSavesValidPreset();
    void editorUnavailableRuntimeStepDisclosed();

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

// ── U2 pins ────────────────────────────────────────────────────────────────────

// The last v1 conflict enum value joins the implemented set: it parses,
// round-trips, and an UNKNOWN value is still refused (fail-closed intact).
void TestBatchPresetsP2::renamePolicySchemaAndRoundTrip() {
    BatchPreset p;
    QString err;

    const QByteArray json = QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"renamer\",\n"
        "    \"name\": \"Renamer\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": {} } ],\n"
        "    \"output\": { \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8();
    QVERIFY2(BatchPresetCodec::parse(json, &p, &err),
             qPrintable(QStringLiteral("rename policy refused: %1").arg(err)));
    QCOMPARE(p.onConflict, QStringLiteral("rename"));

    // Round-trip keeps the policy (serialize emits the non-default value).
    const QByteArray once = BatchPresetCodec::serialize(p);
    BatchPreset p2;
    QVERIFY2(BatchPresetCodec::parse(once, &p2, &err), qPrintable(err));
    QCOMPARE(p2.onConflict, QStringLiteral("rename"));

    // Unknown value — the v1 vocabulary, named verbatim in the diagnostic.
    err.clear();
    QVERIFY(!BatchPresetCodec::parse(
        QByteArray(once).replace("\"rename\"", "\"clobber\""), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("ask, overwrite, rename")), qPrintable(err));

    // The pure naming rule: stem-2.pdf, stem-3.pdf …; an already-renamed
    // name continues the chain; malformed inputs give an empty result.
    QCOMPARE(BatchPresetSchema::renameCandidate(QStringLiteral("report.pdf"), 2),
             QStringLiteral("report-2.pdf"));
    QCOMPARE(BatchPresetSchema::renameCandidate(QStringLiteral("report-2.pdf"), 3),
             QStringLiteral("report-3.pdf"));
    QVERIFY(BatchPresetSchema::renameCandidate(QStringLiteral("report.pdf"), 1).isEmpty());
    QVERIFY(BatchPresetSchema::renameCandidate(QStringLiteral("report.txt"), 2).isEmpty());
    QVERIFY(BatchPresetSchema::renameCandidate(QStringLiteral(".pdf"), 2).isEmpty());
}

// Pre-existing output + rename policy → stem-2 is created, the original is
// byte-identical, and the result REPORTS the renamed path.
void TestBatchPresetsP2::renameConflictCreatesStem2KeepsOriginal() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("doc.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("renamer"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"renamer\",\n"
        "    \"name\": \"Renamer\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ],\n"
        "    \"output\": { \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8()));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    // The FIRST-choice output already exists, with content that must survive.
    const QString existing = outDir + QStringLiteral("/doc_renamer.pdf");
    const QByteArray original = QByteArray("PRECIOUS EXISTING OUTPUT - not a real pdf");
    {
        QFile f(existing);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(original);
    }

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("renamer")));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 1);
    // The renamed output exists; the original is byte-identical.
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/doc_renamer-2.pdf")));
    QFile originalAfter(existing);
    QVERIFY(originalAfter.open(QIODevice::ReadOnly));
    QCOMPARE(originalAfter.readAll(), original);
    // The result reports the FINAL (renamed) path.
    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().outputPath,
             outDir + QStringLiteral("/doc_renamer-2.pdf"));
    // Exactly two pdfs in the out dir (the original + the renamed output).
    QCOMPARE(QDir(outDir).entryList(QStringList() << QStringLiteral("*.pdf"),
                                    QDir::Files).size(), 2);
}

// An occupied stem-2 advances to the first free name.
void TestBatchPresetsP2::renameSkipsOccupiedChainToFirstFree() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("doc.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("renamer"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"renamer\",\n"
        "    \"name\": \"Renamer\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ],\n"
        "    \"output\": { \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8()));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    for (const QString& n : { QStringLiteral("/doc_renamer.pdf"),
                              QStringLiteral("/doc_renamer-2.pdf") }) {
        QFile f(outDir + n);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("occupied");
    }

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("renamer")));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 1);
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/doc_renamer-3.pdf")));
    QCOMPARE(bm.runResultsForTest().first().outputPath,
             outDir + QStringLiteral("/doc_renamer-3.pdf"));
    // The two occupied files keep their placeholder content (never touched).
    QFile a(outDir + QStringLiteral("/doc_renamer.pdf"));
    QVERIFY(a.open(QIODevice::ReadOnly));
    QCOMPARE(a.readAll(), QByteArray("occupied"));
}

// The pre-check/commit race: a file appears at the pinned path between the
// staging resolution and the commit — the commit re-checks, retries the
// rename chain (bounded), and reports the FINAL name.
void TestBatchPresetsP2::renameRaceBetweenStagingAndCommitRetries() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("doc.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("renamer"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"renamer\",\n"
        "    \"name\": \"Renamer\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ],\n"
        "    \"output\": { \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8()));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("renamer")));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1 });

    // The race: at commit time the pinned destination is BUSY — occupied by
    // another writer between staging and commit (deterministic seam).
    bm.setPresetRaceHookForTest([&outDir](const QString& dest) {
        if (dest == outDir + QStringLiteral("/doc_renamer.pdf")) {
            QFile f(outDir + QStringLiteral("/doc_renamer.pdf"));
            if (f.open(QIODevice::WriteOnly))
                f.write("RIVAL WRITER");
        }
    });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 1);
    // The commit retried to stem-2 and reported it; the rival bytes stand.
    QCOMPARE(bm.runResultsForTest().first().outputPath,
             outDir + QStringLiteral("/doc_renamer-2.pdf"));
    QFile rival(outDir + QStringLiteral("/doc_renamer.pdf"));
    QVERIFY(rival.open(QIODevice::ReadOnly));
    QCOMPARE(rival.readAll(), QByteArray("RIVAL WRITER"));
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/doc_renamer-2.pdf")));
}

// Exhaustion of the bounded rename chain is an honest file-scoped failure —
// no candidate is overwritten, nothing is silent.
void TestBatchPresetsP2::renameBoundedExhaustionFailsHonestly() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("doc.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("renamer"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"renamer\",\n"
        "    \"name\": \"Renamer\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ],\n"
        "    \"output\": { \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8()));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    // Occupy the whole chain: the pinned name + every candidate stem-2…999.
    const auto occupied = QDir(outDir).entryInfoList(QStringList() << QStringLiteral("*"),
                                                     QDir::Files);
    for (const QFileInfo& fi : occupied)
        QFile::remove(fi.absoluteFilePath());
    {
        QFile f(outDir + QStringLiteral("/doc_renamer.pdf"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("occupied");
    }
    for (int i = 2; i <= 999; ++i) {
        QFile f(outDir + QStringLiteral("/doc_renamer-%1.pdf").arg(i));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("occupied");
    }

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("renamer")));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.failCount(), 1);
    // The failure names the exhaustion — never a silent overwrite of any of
    // the 999 occupied files (spot-check the boundaries).
    QCOMPARE(bm.errorLogCount(), 1);
    QVERIFY2(bm.errorDetailForTest(0).contains(QStringLiteral("999")),
             qPrintable(bm.errorDetailForTest(0)));
    QFile first(outDir + QStringLiteral("/doc_renamer.pdf"));
    QVERIFY(first.open(QIODevice::ReadOnly));
    QCOMPARE(first.readAll(), QByteArray("occupied"));
    QFile last(outDir + QStringLiteral("/doc_renamer-999.pdf"));
    QVERIFY(last.open(QIODevice::ReadOnly));
    QCOMPARE(last.readAll(), QByteArray("occupied"));
}

// Adversary pin: the rename chain cannot escape the W1-01 containment — a
// hostile naming template is refused BEFORE rename is ever consulted, and
// every rename candidate re-checks through resolveNaming.
void TestBatchPresetsP2::renameCandidatesStayContained() {
    // Hostile template + rename policy: refused at parse (the containment
    // gate runs for the naming template regardless of the conflict policy).
    BatchPreset p;
    QString err;
    QVERIFY(!BatchPresetCodec::parse(QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"evil\",\n"
        "    \"name\": \"Evil\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": {} } ],\n"
        "    \"output\": { \"naming\": \"../evil_{n}.pdf\", \"onConflict\": \"rename\" }\n"
        "}\n").toUtf8(), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("output.naming")), qPrintable(err));

    // The naming rule never introduces separators, dot segments or
    // non-.pdf tails, for ANY attempt, from a benign stem — and every
    // candidate passes the shared containment gate verbatim.
    const QDate d(2026, 9, 23);
    for (int attempt = 2; attempt <= 999; ++attempt) {
        const QString candidate =
            BatchPresetSchema::renameCandidate(QStringLiteral("report_web-optimize.pdf"),
                                               attempt);
        QCOMPARE(candidate, QStringLiteral("report_web-optimize-%1.pdf").arg(attempt));
        QVERIFY(!candidate.contains(QLatin1Char('/')));
        QVERIFY(!candidate.contains(QLatin1Char('\\')));
        QVERIFY(!candidate.contains(QLatin1Char(':')));
        QVERIFY(!candidate.contains(QStringLiteral("..")));
        QString checkName;
        QString checkErr;
        QVERIFY2(BatchPresetSchema::resolveNaming(candidate, {}, {}, 1, d,
                                                  &checkName, &checkErr),
                 qPrintable(checkErr));
        QCOMPARE(checkName, candidate);
    }
    // ... and a hostile (already separator-carrying) input is rejected.
    QVERIFY(BatchPresetSchema::renameCandidate(QStringLiteral("sub/dir/evil.pdf"), 2)
                .isEmpty());
}

// The hot-folder auto-run is unattended: an "ask" preset degrades to
// "rename" (logged), and the conflicting output is renamed — never a modal
// overwrite prompt on the watcher path, never a silent overwrite.
void TestBatchPresetsP2::hotFolderAutoRunDegradesAskToRename() {
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("degrade"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"degrade\",\n"
        "    \"name\": \"Degrade\",\n"
        "    \"created\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-23T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("degrade")));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    presetOutEdit(bm)->setText(outDir);
    // The conflicting output already exists (the drop will be renamed).
    const QString existing = outDir + QStringLiteral("/dropped_degrade.pdf");
    {
        QFile f(existing);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("EXISTING OUTPUT");
    }

    // Arm the hot folder (seeds the empty dir) with auto-run ON, then DROP a
    // new pdf into it — the ingest seam drives the watcher's work.
    const QString hotDir = m_runDir->filePath(QStringLiteral("hot"));
    QDir().mkpath(hotDir);
    bm.armHotFolderForTest(hotDir);
    const QString dropped = createTextPdf(hotDir, QStringLiteral("dropped.pdf"),
                                          { QStringLiteral("fresh drop") });
    QVERIFY(!dropped.isEmpty());

    // The ingest seam ingests + auto-runs (onRunBatch is invoked BY the
    // ingest path, not by the test) — wait for the batch to finish.
    bool finished = false;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&finished] { finished = true; }, Qt::DirectConnection);
    bm.runHotFolderIngestForTest();
    int waited = 0;
    while (!finished && waited < 60000) {
        QTest::qWait(50);
        waited += 50;
    }
    QVERIFY2(finished, "ingest auto-run did not reach batchFinished within 60 seconds");
    QCOMPARE(bm.successCount(), 1);
    // The conflicting output was RENAMED, not overwritten, not skipped.
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/dropped_degrade-2.pdf")));
    QFile existingFile(existing);
    QVERIFY(existingFile.open(QIODevice::ReadOnly));
    QCOMPARE(existingFile.readAll(), QByteArray("EXISTING OUTPUT"));
    // The degrade is disclosed in the log.
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("renamed instead of asking")), qPrintable(log));
}

// ── U3 pins ────────────────────────────────────────────────────────────────────

// The second v1 failure-policy value joins the implemented set: it parses,
// round-trips, and an UNKNOWN value is still refused (fail-closed intact).
// The default "continue" stays omitted from serialization (goldens stable).
void TestBatchPresetsP2::stopPolicySchemaAdmittedAndRoundTrip() {
    BatchPreset p;
    QString err;

    const QByteArray json = QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"stopper\",\n"
        "    \"name\": \"Stopper\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": {} } ],\n"
        "    \"onFileFailure\": \"stop\"\n"
        "}\n").toUtf8();
    QVERIFY2(BatchPresetCodec::parse(json, &p, &err),
             qPrintable(QStringLiteral("stop policy refused: %1").arg(err)));
    QCOMPARE(p.onFileFailure, QStringLiteral("stop"));

    // Round-trip keeps the policy (serialize emits the non-default value).
    const QByteArray once = BatchPresetCodec::serialize(p);
    QVERIFY2(QString::fromUtf8(once).contains(QStringLiteral("\"onFileFailure\"")),
             qPrintable(QStringLiteral("serialize dropped the stop policy: %1")
                            .arg(QString::fromUtf8(once.left(400)))));
    BatchPreset p2;
    QVERIFY2(BatchPresetCodec::parse(once, &p2, &err), qPrintable(err));
    QCOMPARE(p2.onFileFailure, QStringLiteral("stop"));

    // The DEFAULT preset serializes without the key (byte-stable goldens).
    err.clear();
    QVERIFY2(BatchPresetCodec::parse(
        QByteArray(once).replace("\"stop\"", "\"continue\""), &p, &err), qPrintable(err));
    QVERIFY(!QString::fromUtf8(BatchPresetCodec::serialize(p))
                 .contains(QStringLiteral("onFileFailure")));

    // Unknown value — the v1 vocabulary, named verbatim in the diagnostic.
    err.clear();
    QVERIFY(!BatchPresetCodec::parse(
        QByteArray(json).replace("\"stop\"", "\"halt\""), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("continue, stop")), qPrintable(err));
}

// N2: with onFileFailure "stop" a file-scoped failure at file 2 halts the
// queue AT THAT FILE BOUNDARY — files 3-4 are reported as not-run with the
// policy reason (skipped bucket, U08 truthfulness), never silently dropped.
// The preset here is NON-bates: stop-policy runs take the ordered lane too —
// with the mapped pipeline, WHICH files get skipped would be pool-scheduling
// luck, not a stated invariant.
void TestBatchPresetsP2::stopAtFileTwoStopsAndReportsRemaining() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f3 = createTextPdf(fx, QStringLiteral("c.pdf"), { QStringLiteral("gamma") });
    const QString f4 = createTextPdf(fx, QStringLiteral("d.pdf"), { QStringLiteral("delta") });
    QVERIFY(!f1.isEmpty() && !f3.isEmpty() && !f4.isEmpty());
    const QString bad = fx + QStringLiteral("/bad.pdf");
    {
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf at all");
    }

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("stop-run"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"stop-run\",\n"
        "    \"name\": \"Stop Run\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ],\n"
        "    \"onFileFailure\": \"stop\"\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("stop-run")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1, bad, f3, f4 });

    runAndWait(bm);
    // File 1 succeeded, file 2 failed, files 3-4 not run (skipped bucket).
    QCOMPARE(bm.successCount(), 1);
    QCOMPARE(bm.failCount(), 1);
    QCOMPARE(bm.skipCount(), 2);
    QCOMPARE(bm.remainingCount(), 0);

    // Only file 1's output exists — the not-run files produced nothing.
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    const QStringList outs = QDir(outDir).entryList(QStringList() << QStringLiteral("*.pdf"),
                                                    QDir::Files);
    QCOMPARE(outs.size(), 1);
    QCOMPARE(outs.first(), QStringLiteral("a_stop-run.pdf"));

    // Truthful per-file records, in list order: ok, failed, skipped, skipped;
    // the skip reason names the policy AND the file that stopped the run.
    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 4);
    QCOMPARE(results.at(0).success, true);
    QCOMPARE(results.at(1).success, false);
    QVERIFY2(results.at(1).errorMessage.contains(QStringLiteral("Failed to open PDF")),
             qPrintable(results.at(1).errorMessage));
    QVERIFY(results.at(2).skipped);
    QVERIFY2(results.at(2).skipReason.contains(
                 QStringLiteral("run stopped by onFileFailure=stop after bad.pdf")),
             qPrintable(results.at(2).skipReason));
    QVERIFY(results.at(3).skipped);
    QVERIFY2(results.at(3).skipReason.contains(
                 QStringLiteral("run stopped by onFileFailure=stop after bad.pdf")),
             qPrintable(results.at(3).skipReason));

    // The skip bucket reached the error log too (never a silent drop), and
    // the pre-flight/run log disclosed the policy.
    QCOMPARE(bm.errorLogCount(), 3);
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("onFileFailure=stop")), qPrintable(log.left(1500)));
}

// The "continue" default is unchanged: the same run shape with the default
// policy processes every file (failures are file-scoped, no stops).
void TestBatchPresetsP2::continueDefaultRunsAllFiles() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f3 = createTextPdf(fx, QStringLiteral("c.pdf"), { QStringLiteral("gamma") });
    const QString f4 = createTextPdf(fx, QStringLiteral("d.pdf"), { QStringLiteral("delta") });
    QVERIFY(!f1.isEmpty() && !f3.isEmpty() && !f4.isEmpty());
    const QString bad = fx + QStringLiteral("/bad.pdf");
    {
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf at all");
    }

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("cont-run"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"cont-run\",\n"
        "    \"name\": \"Continue Run\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("cont-run")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ f1, bad, f3, f4 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 3);
    QCOMPARE(bm.failCount(), 1);
    QCOMPARE(bm.skipCount(), 0);
    // c and d ran: their outputs exist (the stop run above produced none).
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/c_cont-run.pdf")));
    QVERIFY(QFileInfo::exists(outDir + QStringLiteral("/d_cont-run.pdf")));
}

// ── U4 pins ────────────────────────────────────────────────────────────────────

// N3, staging class: the resolved output directory is ABSENT — the run aborts
// BEFORE any worker starts (deterministic on BOTH lanes; this preset is
// non-bates, i.e. the mapped lane), the cause is reported once, and every
// runnable file is listed as not-run with the reason. Nothing is attempted,
// nothing is faked.
void TestBatchPresetsP2::batchAbortBeforeStartWhenOutputDirMissing() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f2 = createTextPdf(fx, QStringLiteral("b.pdf"), { QStringLiteral("beta") });
    QVERIFY(!f1.isEmpty() && !f2.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("probe-run"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"probe-run\",\n"
        "    \"name\": \"Probe Run\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("probe-run")));
    // The out dir is deliberately NEVER created (the precondition below pins it).
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1, f2 });
    QVERIFY2(!QFileInfo::exists(outDir), "precondition: the output dir must not exist");

    runAndWait(bm);
    // Nothing ran: no success, no failure — every runnable file is not-run.
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.failCount(), 0);
    QCOMPARE(bm.skipCount(), 2);
    QCOMPARE(bm.remainingCount(), 0);

    // Truthful per-file records: skipped with the before-start reason.
    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 2);
    for (const auto& r : results) {
        QVERIFY2(r.skipped, qPrintable(r.inputPath));
        QVERIFY2(r.skipReason.contains(QStringLiteral("run aborted before start")),
                 qPrintable(r.skipReason));
    }

    // The batch-scoped cause is reported ONCE (never per-file noise).
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("Run aborted before start")),
             qPrintable(log.left(1500)));
    QCOMPARE(log.count(QStringLiteral("Run aborted before start")), 1);
    // The skip entries reached the error log too (never a silent drop).
    QCOMPARE(bm.errorLogCount(), 2);
    // No output was produced and the missing dir was never conjured.
    QVERIFY(!QFileInfo::exists(outDir));
}

// The before-start abort touches NOTHING: the out path is occupied by a
// regular FILE (the "committed data" stand-in — also the not-a-directory
// probe arm); after the abort it is byte-identical.
void TestBatchPresetsP2::batchAbortBeforeStartTouchesNothing() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    QVERIFY(!f1.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("probe-run"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"probe-run\",\n"
        "    \"name\": \"Probe Run\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("probe-run")));
    // A FILE sits at the configured out-dir path — the directory is not a
    // directory (the unwritable/absent class, deterministically).
    const QString outPath = m_runDir->filePath(QStringLiteral("out"));
    const QByteArray committed = QByteArray("PRECIOUS COMMITTED DATA - not a real pdf");
    {
        QFile f(outPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(committed);
    }
    presetOutEdit(bm)->setText(outPath);
    bm.addFilesForTest({ f1 });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.failCount(), 0);
    QCOMPARE(bm.skipCount(), 1);

    // The abort removed, replaced or wrote nothing: the file is identical
    // and still a FILE (the run never conjured a directory over it).
    QVERIFY(!QFileInfo(outPath).isDir());
    QFile after(outPath);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), committed);
    // No outputs or probe residue at the run root (the fixture lives in
    // fixtures/, so any *.pdf here would be run debris).
    QCOMPARE(QDir(m_runDir->path())
                 .entryList(QStringList() << QStringLiteral("*.pdf")
                                          << QStringLiteral(".glyph-write-probe-*"),
                            QDir::Files)
                 .size(), 0);
}

// N3, mid-run class (ordered lane): a bates run loses its output directory
// right before file 2's commit (the U2 race seam models the environment
// strike). File 2's failure is classified batch-scoped (its destination
// directory is gone — content-independent), the run aborts at that file
// boundary, the remainder is drained as not-run with the cause, and the
// already-committed file is NEVER rolled back or re-marked.
void TestBatchPresetsP2::batchScopedMidRunAbortsOrderedLaneAndDrainsRemainder() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString f1 = createTextPdf(fx, QStringLiteral("a.pdf"), { QStringLiteral("alpha") });
    const QString f2 = createTextPdf(fx, QStringLiteral("b.pdf"), { QStringLiteral("beta") });
    const QString f3 = createTextPdf(fx, QStringLiteral("c.pdf"), { QStringLiteral("gamma") });
    QVERIFY(!f1.isEmpty() && !f2.isEmpty() && !f3.isEmpty());

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
    const QString outDir = m_runDir->filePath(QStringLiteral("out"));
    presetOutEdit(bm)->setText(outDir);
    bm.addFilesForTest({ f1, f2, f3 });

    // The environment strike: the output directory vanishes right before
    // file 2's commit (file 1 has already committed — the strike destroys
    // its output, not the runner).
    bm.setPresetRaceHookForTest([&outDir](const QString& dest) {
        if (dest == outDir + QStringLiteral("/b_bates-run.pdf"))
            QDir(outDir).removeRecursively();
    });

    runAndWait(bm);
    // File 1 committed (as reported), file 2 failed, file 3 not-run.
    QCOMPARE(bm.successCount(), 1);
    QCOMPARE(bm.failCount(), 1);
    QCOMPARE(bm.skipCount(), 1);
    QCOMPARE(bm.remainingCount(), 0);
    QCOMPARE(bm.successCount() + bm.failCount() + bm.skipCount() + bm.remainingCount(), 3);

    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 3);
    // Committed work is never rolled back: file 1 stays a success with its
    // recorded output path, even though the strike later destroyed the file.
    QCOMPARE(results.at(0).success, true);
    QCOMPARE(results.at(0).outputPath, outDir + QStringLiteral("/a_bates-run.pdf"));
    // File 2: failed AND classified batch-scoped (no error-text matching —
    // the classification is the new flag, fed by the directory-existence
    // content-independence test).
    QCOMPARE(results.at(1).success, false);
    QVERIFY2(results.at(1).batchScoped, qPrintable(results.at(1).errorMessage));
    QVERIFY(!results.at(1).errorMessage.isEmpty());
    // File 3: not-run, with the abort reason naming the stopper file.
    QVERIFY(results.at(2).skipped);
    QVERIFY2(results.at(2).skipReason.contains(QStringLiteral("run aborted after b.pdf")),
             qPrintable(results.at(2).skipReason));

    // ONE batch-scoped cause line at completion (never per-file spam), and it
    // names the file whose failure aborted the run.
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QCOMPARE(log.count(QStringLiteral("Run aborted:")), 1);
    QVERIFY2(log.contains(QStringLiteral("after b.pdf")), qPrintable(log.right(800)));

    // The aborted run left no residue: the strike destroyed the dir and the
    // runner did not silently re-create it or write outputs elsewhere.
    QVERIFY(!QFileInfo::exists(outDir));
    // File 2's failure + file 3's skip reached the error log (never silent).
    QCOMPARE(bm.errorLogCount(), 2);
}

// The unattended (hot-folder) abort discloses that the watcher STAYS ARMED —
// a before-start abort in the ingest auto-run must never look like the hot
// folder stopped watching.
void TestBatchPresetsP2::batchAbortUnattendedNotesHotFolderStaysArmed() {
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("probe-run"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"probe-run\",\n"
        "    \"name\": \"Probe Run\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("probe-run")));
    // The out dir does not exist — the ingest auto-run will abort before start.
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));

    const QString hotDir = m_runDir->filePath(QStringLiteral("hot"));
    QDir().mkpath(hotDir);
    bm.armHotFolderForTest(hotDir);
    const QString dropped = createTextPdf(hotDir, QStringLiteral("dropped.pdf"),
                                          { QStringLiteral("fresh drop") });
    QVERIFY(!dropped.isEmpty());

    bool finished = false;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&finished] { finished = true; }, Qt::DirectConnection);
    bm.runHotFolderIngestForTest();
    int waited = 0;
    while (!finished && waited < 60000) {
        QTest::qWait(50);
        waited += 50;
    }
    QVERIFY2(finished, "ingest auto-run did not reach batchFinished within 60 seconds");
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.skipCount(), 1);

    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("Run aborted before start")), qPrintable(log.left(1500)));
    QVERIFY2(log.contains(QStringLiteral("hot folder stays armed")),
             qPrintable(log.right(1200)));
}

// ── U5 pins ────────────────────────────────────────────────────────────────────

// §3/§4.8: the chain instruments every step with MEASURED on-disk byte counts
// (no estimates): step 0 enters from the input file, each later step enters
// from the previous candidate, and the last candidate's size is the committed
// output's size (the commit copies it byte for byte). Two-step golden run:
// compress → watermark, sizes hand-checked against the files on disk.
void TestBatchPresetsP2::measuredBytesTwoStepChainHandComputed() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString src = createTextPdf(fx, QStringLiteral("doc.pdf"),
                                      { QStringLiteral("alpha") });
    QVERIFY(!src.isEmpty());
    const qint64 inputBytes = QFileInfo(src).size();
    QVERIFY(inputBytes > 0);

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("two-step"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"two-step\",\n"
        "    \"name\": \"Two Step\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [\n"
        "        { \"op\": \"compress\", \"params\": { \"quality\": 60 } },\n"
        "        { \"op\": \"watermark\", \"params\": { \"text\": \"DRAFT\", \"opacity\": 30 } }\n"
        "    ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("two-step")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString out = m_runDir->filePath(QStringLiteral("out/doc_two-step.pdf"));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ src });

    runAndWait(bm);
    QCOMPARE(bm.successCount(), 1);
    QVERIFY(QFileInfo::exists(out));

    const auto results = bm.runResultsForTest();
    QCOMPARE(results.size(), 1);
    const auto& steps = results.first().steps;
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps.at(0).op, QStringLiteral("compress"));
    QCOMPARE(steps.at(1).op, QStringLiteral("watermark"));
    QCOMPARE(int(steps.at(0).status), int(BatchStepResult::Status::Ok));
    QCOMPARE(int(steps.at(1).status), int(BatchStepResult::Status::Ok));

    // The hand-computed byte table: every count is a file on disk.
    QCOMPARE(steps.at(0).bytesIn, inputBytes);                  // step 0 enters from the input
    QVERIFY(steps.at(0).bytesOut > 0);                          // candidate 0 was produced
    QCOMPARE(steps.at(1).bytesIn, steps.at(0).bytesOut);        // chain-link continuity
    QCOMPARE(steps.at(1).bytesOut, QFileInfo(out).size());      // last link == committed output
    QVERIFY(steps.at(0).durationMs >= 0);
    QVERIFY(steps.at(1).durationMs >= 0);
}

// The JSON export shape (M5 precedent) is pinned: one object per file, one
// step record per chain step with the measured numbers; the exported file's
// bytes are exactly the pure formatter's bytes, and the .csv extension routes
// to the CSV formatter.
void TestBatchPresetsP2::runReportJsonShapePinned() {
    const QString fx = m_runDir->filePath(QStringLiteral("fixtures"));
    QDir().mkpath(fx);
    const QString src = createTextPdf(fx, QStringLiteral("doc.pdf"),
                                      { QStringLiteral("alpha") });
    QVERIFY(!src.isEmpty());

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("two-step"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"two-step\",\n"
        "    \"name\": \"Two Step\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("two-step")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out")));
    const QString out = m_runDir->filePath(QStringLiteral("out/doc_two-step.pdf"));
    presetOutEdit(bm)->setText(m_runDir->filePath(QStringLiteral("out")));
    bm.addFilesForTest({ src });
    runAndWait(bm);
    QCOMPARE(bm.successCount(), 1);

    // Extension routing: the exported file's bytes are exactly the pure
    // formatters' output.
    const QString jsonPath = m_runDir->filePath(QStringLiteral("report.json"));
    QVERIFY(bm.exportRunReportForTest(jsonPath));
    QFile jf(jsonPath);
    QVERIFY(jf.open(QIODevice::ReadOnly));
    const QByteArray jsonBytes = jf.readAll();
    jf.close();
    QCOMPARE(jsonBytes, BatchMode::runReportJson(bm.runResultsForTest()));
    const QString csvPath = m_runDir->filePath(QStringLiteral("report.csv"));
    QVERIFY(bm.exportRunReportForTest(csvPath));
    QFile cf(csvPath);
    QVERIFY(cf.open(QIODevice::ReadOnly));
    QCOMPARE(cf.readAll(), BatchMode::runReportCsv(bm.runResultsForTest()));
    cf.close();

    // JSON shape: one file object, step array with the measured facts.
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    QCOMPARE(parseErr.error, QJsonParseError::NoError);
    QVERIFY(doc.isArray());
    const QJsonArray arr = doc.array();
    QCOMPARE(arr.size(), 1);
    const QJsonObject f = arr.at(0).toObject();
    QCOMPARE(f.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(f.value(QStringLiteral("input")).toString(), src);
    QCOMPARE(f.value(QStringLiteral("output")).toString(), out);
    QCOMPARE(f.value(QStringLiteral("batchScoped")).toBool(), false);
    const QJsonArray steps = f.value(QStringLiteral("steps")).toArray();
    QCOMPARE(steps.size(), 1);
    const QJsonObject s0 = steps.at(0).toObject();
    QCOMPARE(s0.value(QStringLiteral("index")).toInt(), 1);
    QCOMPARE(s0.value(QStringLiteral("op")).toString(), QStringLiteral("compress"));
    QCOMPARE(s0.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(static_cast<qint64>(s0.value(QStringLiteral("bytesIn")).toDouble()),
             QFileInfo(src).size());
    QCOMPARE(static_cast<qint64>(s0.value(QStringLiteral("bytesOut")).toDouble()),
             QFileInfo(out).size());
    QVERIFY(s0.value(QStringLiteral("durationMs")).toDouble() >= 0);
    QCOMPARE(s0.value(QStringLiteral("firstBates")).toInt(), -1);
    QCOMPARE(s0.value(QStringLiteral("lastBates")).toInt(), -1);
}

// The CSV is RFC-4180: CRLF record separators, every field quoted, embedded
// quotes doubled (a hostile detail survives round-trip), one row per chain
// step, and a not-run file rides as its own row.
void TestBatchPresetsP2::runReportCsvShapePinned() {
    QList<BatchFileResult> results;

    BatchFileResult okRun;
    okRun.inputPath  = QStringLiteral("C:/in/a.pdf");
    okRun.outputPath = QStringLiteral("C:/out/a.pdf");
    okRun.success = true;
    BatchStepResult s0;
    s0.stepIndex = 0;
    s0.op = QStringLiteral("compress");
    s0.status = BatchStepResult::Status::Ok;
    s0.bytesIn = 8421;
    s0.bytesOut = 7912;
    s0.durationMs = 34;
    okRun.steps << s0;
    results << okRun;

    BatchFileResult notRun;
    notRun.inputPath = QStringLiteral("C:/in/b.pdf");
    notRun.skipped = true;
    notRun.skipReason = QStringLiteral("run aborted before start: comma, \"quote\"");
    results << notRun;

    const QString text = QString::fromUtf8(BatchMode::runReportCsv(results));

    // Header + one row per step + one row for the not-run file; CRLF records.
    QVERIFY2(text.startsWith(QStringLiteral(
                 "\"File\",\"Output\",\"Status\",\"Step\",\"Op\",\"StepStatus\","
                 "\"BytesIn\",\"BytesOut\",\"DurationMs\",\"FirstBates\","
                 "\"LastBates\",\"Detail\"\r\n")),
             qPrintable(text.left(200)));
    QCOMPARE(text.count(QStringLiteral("\r\n")), 3);
    QVERIFY2(text.endsWith(QStringLiteral("\r\n")), qPrintable(text.right(120)));
    // The measured row, numbers as-is (quoted, unrounded).
    QVERIFY2(text.contains(QStringLiteral(
                 "\"C:/in/a.pdf\",\"C:/out/a.pdf\",\"ok\",\"1\",\"compress\","
                 "\"ok\",\"8421\",\"7912\",\"34\",\"\",\"\",\"\"\r\n")),
             qPrintable(text));
    // The not-run row carries the reason; the embedded comma and quote are
    // RFC-4180 escaped (doubled), the field stays intact.
    QVERIFY2(text.contains(QStringLiteral(
                 "\"C:/in/b.pdf\",\"\",\"skipped\",\"\",\"\",\"\",\"\",\"\",\"\","
                 "\"\",\"\",\"run aborted before start: comma, \"\"quote\"\"\"\r\n")),
             qPrintable(text));
}

// ── U6 pins ────────────────────────────────────────────────────────────────────

static QByteArray readFileBytes(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

// The full share discipline: export is a byte-identical copy of the store
// file; import validates before anything appears; export→delete→import
// round-trips the SAME id and byte-identical file; an existing export target
// is refused without the confirm and replaced with it.
void TestBatchPresetsP2::importExportRoundTripByteIdentical() {
    const QString dirA = m_storeDir->path() + QStringLiteral("/a");
    const QString dirB = m_storeDir->path() + QStringLiteral("/b");
    BatchPresetStore storeA(dirA);
    BatchPreset p;
    p.name = QStringLiteral("Round Trip");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(storeA.save(&p, &err), qPrintable(err));
    const QString id = p.id;
    QVERIFY(!id.isEmpty());
    const QString storePathA =
        dirA + QStringLiteral("/") + id + QStringLiteral(".glyphpreset.json");
    const QByteArray storeBytes = readFileBytes(storePathA);
    QVERIFY(!storeBytes.isEmpty());

    // Export: byte-identical copy (no re-serialization). The share artifact
    // carries the id as its stem — the V8 import rule the receiver enforces.
    const QString exported = m_runDir->filePath(id + QStringLiteral(".glyphpreset.json"));
    QVERIFY2(storeA.exportTo(id, exported, false, &err), qPrintable(err));
    QCOMPARE(readFileBytes(exported), storeBytes);

    // An existing export target is refused without the confirm — never a
    // silent overwrite — and replaced with it.
    {
        QFile f(exported);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"junk\": true}");
    }
    err.clear();
    QVERIFY(!storeA.exportTo(id, exported, false, &err));
    QVERIFY2(err.contains(QStringLiteral("already exists")), qPrintable(err));
    QVERIFY2(storeA.exportTo(id, exported, true, &err), qPrintable(err));
    QCOMPARE(readFileBytes(exported), storeBytes);

    // Import into a fresh store: validated, atomic, same id, byte-identical.
    BatchPresetStore storeB(dirB);
    QString importedId;
    QVERIFY2(storeB.importFrom(exported, false, &err, &importedId), qPrintable(err));
    QCOMPARE(importedId, id);
    QCOMPARE(storeB.list().size(), 1);
    QCOMPARE(storeB.list().first().id, id);
    QCOMPARE(storeB.list().first().name, QStringLiteral("Round Trip"));
    QCOMPARE(readFileBytes(dirB + QStringLiteral("/") + id
                                       + QStringLiteral(".glyphpreset.json")),
             storeBytes);

    // Delete from A, re-import from the shared copy: the preset comes back
    // byte-identical.
    QVERIFY(storeA.remove(id, &err));
    QVERIFY(!QFileInfo::exists(storePathA));
    QVERIFY2(storeA.importFrom(exported, false, &err), qPrintable(err));
    QCOMPARE(readFileBytes(storePathA), storeBytes);
}

// The V8 import rule holds at import time: a valid preset file whose stem
// does not match its id is refused with the diagnostic and the store stays
// unchanged; a missing or corrupt source is refused too.
void TestBatchPresetsP2::importRefusesIdStemMismatchKeepsStore() {
    const QString dirA = m_storeDir->path() + QStringLiteral("/a");
    BatchPresetStore storeA(dirA);
    BatchPreset p;
    p.name = QStringLiteral("Mismatch");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(storeA.save(&p, &err), qPrintable(err));
    const QString id = p.id;   // slug of "Mismatch" — != "other"

    const QString exported = m_runDir->filePath(QStringLiteral("mismatch.glyphpreset.json"));
    QVERIFY2(storeA.exportTo(id, exported, false, &err), qPrintable(err));
    const QString renamedCopy =
        m_runDir->filePath(QStringLiteral("other.glyphpreset.json"));
    QVERIFY(QFile::copy(exported, renamedCopy));

    BatchPresetStore storeB(m_storeDir->path() + QStringLiteral("/b"));
    err.clear();
    QVERIFY(!storeB.importFrom(renamedCopy, false, &err));
    QVERIFY2(!err.isEmpty(), "the V8 id!=stem refusal must carry its diagnostic");
    // Nothing appeared in the store — a rejected import changes nothing.
    QVERIFY(storeB.list().isEmpty());
    QVERIFY(!storeB.contains(id));
    QVERIFY(storeB.brokenFiles().isEmpty());

    // A missing source and a corrupt source are refused with diagnostics.
    err.clear();
    QVERIFY(!storeB.importFrom(m_runDir->filePath(QStringLiteral("missing.glyphpreset.json")),
                               false, &err));
    QVERIFY(!err.isEmpty());
    const QString corrupt = m_runDir->filePath(QStringLiteral("corrupt.glyphpreset.json"));
    {
        QFile f(corrupt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ this is not json ");
    }
    err.clear();
    QVERIFY(!storeB.importFrom(corrupt, false, &err));
    QVERIFY2(!err.isEmpty(), "a corrupt source must be refused with its diagnostic");
    QVERIFY(storeB.list().isEmpty());
}

// An id already in the store is NEVER silently replaced: the import refuses
// with the existing-id diagnostic; the replace path (the post-confirm action)
// performs the replacement atomically.
void TestBatchPresetsP2::importRefusesExistingIdUnlessReplaced() {
    const QString dirA = m_storeDir->path() + QStringLiteral("/a");
    const QString dirB = m_storeDir->path() + QStringLiteral("/b");
    BatchPresetStore storeA(dirA);
    BatchPreset p1;
    p1.name = QStringLiteral("Clash");
    p1.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(storeA.save(&p1, &err), qPrintable(err));
    const QString id = p1.id;
    const QString storePathA =
        dirA + QStringLiteral("/") + id + QStringLiteral(".glyphpreset.json");
    const QByteArray originalBytes = readFileBytes(storePathA);

    // A second store holds a preset with the SAME id and different content
    // (renamed display name → modified stamp + name differ, bytes differ).
    BatchPresetStore storeB(dirB);
    BatchPreset p2;
    p2.name = QStringLiteral("Clash");
    p2.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QVERIFY2(storeB.save(&p2, &err), qPrintable(err));
    QCOMPARE(p2.id, id);
    QVERIFY2(storeB.rename(id, QStringLiteral("Clash Modified"), &err), qPrintable(err));
    const QString exported = m_runDir->filePath(QStringLiteral("clash.glyphpreset.json"));
    QVERIFY2(storeB.exportTo(id, exported, false, &err), qPrintable(err));
    const QByteArray replacementBytes = readFileBytes(exported);
    QVERIFY(replacementBytes != originalBytes);

    // The refusal: nothing changes.
    err.clear();
    QVERIFY(!storeA.importFrom(exported, false, &err));
    QVERIFY2(err.contains(QStringLiteral("already exists")), qPrintable(err));
    QCOMPARE(readFileBytes(storePathA), originalBytes);

    // The post-confirm replace: atomic, complete.
    QVERIFY2(storeA.importFrom(exported, true, &err), qPrintable(err));
    QCOMPARE(readFileBytes(storePathA), replacementBytes);
    BatchPreset after;
    QVERIFY2(storeA.get(id, &after, &err), qPrintable(err));
    QCOMPARE(after.name, QStringLiteral("Clash Modified"));
    QCOMPARE(storeA.list().size(), 1);
}

// ── U7 pins ────────────────────────────────────────────────────────────────────

// The manager discloses everything honestly: presets with step-count badges,
// per-step capability lines in the detail pane, and broken store files with
// their diagnostics — never silently hidden.
void TestBatchPresetsP2::managerDisclosesPresetsAndBrokenFiles() {
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("good"),
                           QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"good\",\n"
        "    \"name\": \"Good Preset\",\n"
        "    \"created\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-24T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": { \"quality\": 60 } } ]\n"
        "}\n").toUtf8()));
    {
        QFile f(m_storeDir->path()
                + QStringLiteral("/broken.glyphpreset.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ this is not valid json ");
    }

    PresetManagerDialog::setStoreRootForTest(m_storeDir->path());
    PresetManagerDialog dlg(nullptr);   // null registry: gated ops unavailable
    auto* list = dlg.findChild<QListWidget*>(QStringLiteral("presetManagerList"));
    QVERIFY(list);
    QCOMPARE(list->count(), 1);
    QVERIFY2(list->item(0)->text().contains(QStringLiteral("Good Preset")),
             qPrintable(list->item(0)->text()));
    QVERIFY2(list->item(0)->text().contains(QStringLiteral("1 step")),
             qPrintable(list->item(0)->text()));

    // The broken file is disclosed with its diagnostic (never hidden).
    auto* broken = dlg.findChild<QLabel*>(QStringLiteral("presetManagerBrokenLabel"));
    QVERIFY(broken);
    QVERIFY2(!broken->isHidden(), "the broken-file disclosure must be visible");
    QVERIFY2(broken->text().contains(QStringLiteral("broken.glyphpreset.json")),
             qPrintable(broken->text()));

    // The detail pane renders the selected preset's per-step capability line.
    list->setCurrentRow(0);
    auto* detail = dlg.findChild<QLabel*>(QStringLiteral("presetManagerDetail"));
    QVERIFY(detail);
    QVERIFY2(detail->text().contains(QStringLiteral("compress")),
             qPrintable(detail->text()));
}

// The toolbar flows through the post-confirm seams: duplicate assigns a fresh
// id and "(copy)" name, rename edits the display name only, delete removes.
void TestBatchPresetsP2::managerDuplicateRenameDeleteFlows() {
    const QString dirA = m_storeDir->path();
    BatchPresetStore store(dirA);
    BatchPreset p;
    p.name = QStringLiteral("Original");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(store.save(&p, &err), qPrintable(err));

    PresetManagerDialog::setStoreRootForTest(dirA);
    PresetManagerDialog dlg(nullptr);
    auto* list = dlg.findChild<QListWidget*>(QStringLiteral("presetManagerList"));
    QVERIFY(list);
    QCOMPARE(list->count(), 1);

    // Duplicate: a fresh id, "(copy)" name, same steps.
    QVERIFY(dlg.duplicatePresetForTest(p.id));
    QCOMPARE(list->count(), 2);
    QString copyId;
    for (const BatchPreset& q : store.list())
        if (q.id != p.id) copyId = q.id;
    QVERIFY(!copyId.isEmpty());
    BatchPreset got;
    QVERIFY2(store.get(copyId, &got, &err), qPrintable(err));
    QCOMPARE(got.name, QStringLiteral("Original (copy)"));
    QCOMPARE(got.steps.size(), 1);

    // Rename edits the display name only — the id stays stable.
    QVERIFY(dlg.renamePresetForTest(copyId, QStringLiteral("Renamed Copy")));
    QVERIFY2(store.get(copyId, &got, &err), qPrintable(err));
    QCOMPARE(got.name, QStringLiteral("Renamed Copy"));
    QCOMPARE(got.id, copyId);

    // Delete removes exactly the selected preset.
    QVERIFY(dlg.deletePresetForTest(copyId));
    QCOMPARE(list->count(), 1);
    QVERIFY(store.contains(p.id));
    QVERIFY(!store.contains(copyId));
}

// Import/export through the manager ride the U6 store mechanics; a rejected
// import leaves the list unchanged; the replace path applies after confirm.
void TestBatchPresetsP2::managerImportExportReplaceFlows() {
    const QString dirA = m_storeDir->path() + QStringLiteral("/a");
    BatchPresetStore storeA(dirA);
    BatchPreset p;
    p.name = QStringLiteral("Shareable");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(storeA.save(&p, &err), qPrintable(err));

    PresetManagerDialog::setStoreRootForTest(dirA);
    PresetManagerDialog dlgA(nullptr);
    const QString exported = m_runDir->filePath(p.id + QStringLiteral(".glyphpreset.json"));
    QVERIFY(dlgA.exportPresetForTest(p.id, exported, false));
    QVERIFY(QFileInfo::exists(exported));

    const QString dirB = m_storeDir->path() + QStringLiteral("/b");
    PresetManagerDialog::setStoreRootForTest(dirB);
    PresetManagerDialog dlgB(nullptr);
    QVERIFY(dlgB.importPresetForTest(exported, false));
    QCOMPARE(dlgB.findChild<QListWidget*>(QStringLiteral("presetManagerList"))
                 ->count(), 1);

    // The existing-id refusal: the list is unchanged (nothing replaced).
    QVERIFY(!dlgB.importPresetForTest(exported, false));
    QCOMPARE(dlgB.findChild<QListWidget*>(QStringLiteral("presetManagerList"))
                 ->count(), 1);

    // The post-confirm replace applies.
    QVERIFY(dlgB.importPresetForTest(exported, true));
    QCOMPARE(dlgB.findChild<QListWidget*>(QStringLiteral("presetManagerList"))
                 ->count(), 1);
    BatchPresetStore storeB(dirB);
    QVERIFY(storeB.contains(p.id));
}

// The editor cannot save an invalid preset: the parameter widgets are
// CLAMPED to the schema ranges (the same families the batch panels use), and
// a save that would be invalid refuses honestly with the diagnostic.
void TestBatchPresetsP2::editorClampsWidgetsAndSavesValidPreset() {
    PresetEditorDialog editor(nullptr);
    auto* name = editor.findChild<QLineEdit*>(QStringLiteral("presetEditorName"));
    QVERIFY(name);
    name->setText(QStringLiteral("Clamped"));

    editor.addStepForTest(QStringLiteral("compress"));
    auto* dpi = editor.findChild<QSpinBox*>(QStringLiteral("param_targetDpi"));
    QVERIFY(dpi);
    dpi->setValue(9999);
    QCOMPARE(dpi->value(), 600);       // the engine's maximum
    dpi->setValue(1);
    QCOMPARE(dpi->value(), 36);        // the engine's minimum
    auto* quality = editor.findChild<QSlider*>(QStringLiteral("param_quality"));
    QVERIFY(quality);
    quality->setValue(999);
    QCOMPARE(quality->value(), 100);

    // A second step: the bates fields are clamped too.
    editor.addStepForTest(QStringLiteral("bates"));
    auto* digits = editor.findChild<QSpinBox*>(QStringLiteral("param_digitCount"));
    QVERIFY(digits);
    digits->setValue(99);
    QCOMPARE(digits->value(), 12);
    auto* start = editor.findChild<QSpinBox*>(QStringLiteral("param_startNumber"));
    QVERIFY(start);
    start->setValue(0);
    QCOMPARE(start->value(), 1);

    // Reorder: bates moves above compress (up from row 1).
    QVERIFY(editor.moveCurrentStepForTest(-1));
    auto* steps = editor.findChild<QListWidget*>(QStringLiteral("presetEditorSteps"));
    QCOMPARE(steps->item(0)->text().contains(QStringLiteral("bates")), true);
    QVERIFY(editor.moveCurrentStepForTest(1));
    QCOMPARE(steps->item(0)->text().contains(QStringLiteral("compress")), true);

    QVERIFY(editor.savePreset());
    const BatchPreset saved = editor.preset();
    QCOMPARE(saved.name, QStringLiteral("Clamped"));
    QCOMPARE(saved.steps.size(), 2);
    QCOMPARE(saved.steps.at(0).op, QStringLiteral("compress"));
    QCOMPARE(saved.steps.at(0).params.value(QStringLiteral("targetDpi")).toInt(), 36);

    // The full authoring path: the editor hands back an EMPTY id — the store
    // assigns (and de-conflicts) it on save; the stored preset validates.
    QString verr;
    PresetManagerDialog::setStoreRootForTest(m_storeDir->path()
                                             + QStringLiteral("/editor"));
    PresetManagerDialog mgr(nullptr);
    QVERIFY(mgr.applyEditedPresetForTest(saved));
    const BatchPresetStore stored(m_storeDir->path() + QStringLiteral("/editor"));
    QCOMPARE(stored.list().size(), 1);
    QCOMPARE(stored.list().first().id, QStringLiteral("clamped"));
    QVERIFY2(BatchPresetCodec::validate(stored.list().first(), &verr),
             qPrintable(verr));

    // An empty name would be invalid — the save refuses with the diagnostic
    // and the dialog stays open.
    PresetEditorDialog invalid(nullptr);
    invalid.addStepForTest(QStringLiteral("compress"));
    QVERIFY(!invalid.savePreset());
    QVERIFY2(!invalid.saveErrorForTest().isEmpty(),
             qPrintable(invalid.saveErrorForTest()));
}

// An UnavailableRuntime step is disclosed IN the editor — visible row badge
// plus the whyNot tooltip — and stays in the preset: disclosed at design
// time, blocked at pre-flight (the landed capability rule). The palette
// never offers UnavailableBuild ops.
void TestBatchPresetsP2::editorUnavailableRuntimeStepDisclosed() {
    gp::CapabilityRegistry registry;   // no probes → gated ops unavailable
    PresetEditorDialog editor(&registry);
    auto* name = editor.findChild<QLineEdit*>(QStringLiteral("presetEditorName"));
    QVERIFY(name);
    name->setText(QStringLiteral("With Check"));

    editor.addStepForTest(QStringLiteral("pdfa-check"));
    auto* steps = editor.findChild<QListWidget*>(QStringLiteral("presetEditorSteps"));
    QVERIFY(steps);
    QCOMPARE(steps->count(), 1);
    QVERIFY2(steps->item(0)->text().contains(QStringLiteral("[unavailable]")),
             qPrintable(steps->item(0)->text()));
    QVERIFY2(!steps->item(0)->toolTip().isEmpty(),
             "the whyNot + alternative must be the row tooltip");

    // The step is KEPT — the disclosure is honest, not a silent drop.
    QVERIFY(editor.savePreset());
    QCOMPARE(editor.preset().steps.size(), 1);
    QCOMPARE(editor.preset().steps.first().op, QStringLiteral("pdfa-check"));
}

QTEST_MAIN(TestBatchPresetsP2)
#include "TestBatchPresetsP2.moc"
