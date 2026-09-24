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

QTEST_MAIN(TestBatchPresetsP2)
#include "TestBatchPresetsP2.moc"
