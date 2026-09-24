// SPDX-License-Identifier: Apache-2.0
// R26 (batch-presets P1) - named batch presets over the Batch mode.
//
// Contract under test (docs/research/batch-presets-implementation-plan.md,
// P1 scope of the R26 lane):
//   * Versioned JSON schema, FAIL-CLOSED: golden round-trip byte-stable;
//     unknown newer schemaVersion refused with the handshake message (never
//     misparsed); one negative case per validation class V1-V9 with the
//     diagnostic naming the JSON path.
//   * Store: file-per-preset JSON under a test-rooted directory (settings
//     isolation); CRUD honest; broken preset files are disclosed, never
//     silently hidden.
//   * Save/load/delete/rename in the Batch UI without driving a native modal
//     (objectName'd seams; the confirm path is exercised as the post-confirm
//     action).
//   * Transactional run: a preset-driven run produces the SAME artifacts as
//     the equivalent manual configuration (compared through an INDEPENDENT
//     read path - QPdfDocument open + page count + extracted text, not file
//     bytes, which carry timestamps); it flows through the same mapped
//     pipeline + G12 exactly-once accounting; a failed input leaves the
//     original byte-identical with NO output and NO temp residue.
//   * Capability honesty: a preset step whose capability is unavailable is
//     DISPLAYED with the registry's whyNot at selection time and REFUSED at
//     run time (files staged failed - never a silent skip, never a fake
//     success).
//
// STRUCTURE NOTE: the test class carries DECLARATIONS ONLY and every method
// is defined after the class - moc's in-class parser is not exercised on the
// heavy fixture code below.
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchPresets --output-on-failure
#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPdfDocument>
#include <QStandardPaths>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "core/BatchPreset.h"
#include "core/Capability.h"
#include "modes/BatchMode.h"
#include "mocks/MockPdfEditorEngine.h"

using namespace gp;

// -- Fixture: PoDoFo painter pages with text (TestBatchOpsCoverage idiom) ------

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

// The plan's golden "web-optimize" work order: compress -> strip metadata ->
// verify PDF/A-2b.
static QByteArray goldenWebOptimizeJson() {
    return QByteArray(
        "{\n"
        "    \"glyphpreset\": {\n"
        "        \"schemaVersion\": 1,\n"
        "        \"kind\": \"batch-preset\",\n"
        "        \"minAppVersion\": \"1.0.0\"\n"
        "    },\n"
        "    \"id\": \"web-optimize\",\n"
        "    \"name\": \"Web optimize\",\n"
        "    \"description\": \"Compress for web, strip metadata, verify PDF/A-2b\",\n"
        "    \"created\": \"2026-09-09T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-09T00:00:00.000Z\",\n"
        "    \"authorApp\": \"GlyphPDF 1.4.0\",\n"
        "    \"steps\": [\n"
        "        { \"op\": \"compress\", \"label\": \"Compress images to 150 DPI\",\n"
        "          \"params\": { \"quality\": 60, \"targetDpi\": 150 } },\n"
        "        { \"op\": \"strip-metadata\", \"label\": \"Remove metadata\",\n"
        "          \"params\": { \"sanitize\": true, \"clearInfoDict\": true } },\n"
        "        { \"op\": \"pdfa-check\", \"label\": \"Verify PDF/A-2b\",\n"
        "          \"params\": { \"level\": \"2b\" } }\n"
        "    ],\n"
        "    \"output\": { \"naming\": \"{basename}_web.pdf\", \"onConflict\": \"ask\" },\n"
        "    \"onFileFailure\": \"continue\"\n"
        "}\n");
}

static QByteArray compressPresetJson(const QString& id) {
    return QStringLiteral(
        "{\n"
        "    \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "    \"id\": \"%1\",\n"
        "    \"name\": \"Compress 60\",\n"
        "    \"created\": \"2026-09-09T00:00:00.000Z\",\n"
        "    \"modified\": \"2026-09-09T00:00:00.000Z\",\n"
        "    \"steps\": [ { \"op\": \"compress\", \"params\": "
        "{ \"quality\": 60, \"targetDpi\": 150 } } ]\n"
        "}\n").arg(id).toUtf8();
}

static AppContext makeCtx() {
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(new MockPdfEditorEngine, [](auto*) {});
    return ctx;
}

// The watermark text edit carries the unique placeholder "CONFIDENTIAL"
// (the TestBatchOpsCoverage lookup idiom).
static QLineEdit* watermarkTextEdit(BatchMode& bm) {
    const auto edits = bm.findChildren<QLineEdit*>();
    for (QLineEdit* e : edits)
        if (e->placeholderText() == QStringLiteral("CONFIDENTIAL"))
            return e;
    return nullptr;
}

namespace {
struct CompletionSnapshot {
    bool fired = false;
    int  success = -1;
    int  fail = -1;
    int  remaining = -1;
};
} // namespace

// -- Test class: DECLARATIONS ONLY (bodies defined after the class) ------------

class TestBatchPresets : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void goldenRoundTripByteStable();
    void versionHandshakeRefusesNewerSchema();
    void negativeValidationMatrix();
    void namingTokensResolveAndSanitize();
    void storeCrudRenameDeleteAndBrokenDisclosure();
    void saveAsPresetFromConfiguredRunAndRefusals();
    void presetRunMatchesManualConfiguration();
    void unavailableCapabilityStepDisclosesAndRefuses();
    void zeroOpPresetRefusesToLoadAndRun();
    void minAppVersionBlocksOlderApp();
    void deleteAndRenameHonestyThroughUiSeams();
    void transactionalFailureLeavesOriginalUntouched();
    void firstSaveOnCleanProfileCreatesStoreRoot();

private:
    std::unique_ptr<QTemporaryDir> m_storeDir;
    std::unique_ptr<QTemporaryDir> m_runDir;

    QSharedPointer<CompletionSnapshot> captureCompletion(BatchMode& bm);
    static void runAndWait(BatchMode& bm);
};

// -- Bodies --------------------------------------------------------------------

void TestBatchPresets::initTestCase() {
    // Settings isolation (TestBatchOcrLanguage idiom): redirect every
    // QStandardPaths location; the preset store root is re-pointed per test
    // in init(). The minAppVersion gate compares against the RUNNING app
    // version - pin it so the gate is deterministic.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationVersion(QStringLiteral("1.4.0"));
}

void TestBatchPresets::init() {
    m_storeDir = std::make_unique<QTemporaryDir>();
    m_runDir = std::make_unique<QTemporaryDir>();
    BatchMode::setPresetStoreDirForTest(m_storeDir->path());
}

QSharedPointer<CompletionSnapshot> TestBatchPresets::captureCompletion(BatchMode& bm) {
    auto snap = QSharedPointer<CompletionSnapshot>::create();
    QObject::connect(&bm, &BatchMode::batchFinished, &bm, [snap, &bm] {
        snap->fired = true;
        snap->success = bm.successCount();
        snap->fail = bm.failCount();
        snap->remaining = bm.remainingCount();
    }, Qt::DirectConnection);
    return snap;
}

void TestBatchPresets::runAndWait(BatchMode& bm) {
    bool finished = false;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&finished] { finished = true; }, Qt::DirectConnection);
    bm.onRunBatch();
    int waited = 0;
    while (!finished && waited < 30000) {
        QTest::qWait(50);
        waited += 50;
    }
    QVERIFY2(finished, "Batch did not reach batchFinished within 30 seconds");
}

void TestBatchPresets::goldenRoundTripByteStable() {
    const QByteArray json = goldenWebOptimizeJson();
    BatchPreset p;
    QString err;
    QVERIFY2(BatchPresetCodec::parse(json, &p, &err),
             qPrintable(QStringLiteral("golden fixture refused: %1").arg(err)));
    QCOMPARE(p.id, QStringLiteral("web-optimize"));
    QCOMPARE(p.name, QStringLiteral("Web optimize"));
    QCOMPARE(p.minAppVersion, QStringLiteral("1.0.0"));
    QCOMPARE(p.steps.size(), 3);
    QCOMPARE(p.steps.at(0).op, QStringLiteral("compress"));
    QCOMPARE(p.steps.at(0).params.value(QStringLiteral("quality")).toInt(), 60);
    QCOMPARE(p.steps.at(2).op, QStringLiteral("pdfa-check"));
    QCOMPARE(p.onConflict, QStringLiteral("ask"));
    QCOMPARE(p.onFileFailure, QStringLiteral("continue"));

    // parse -> serialize -> parse is a FIXED POINT (byte-stable round trip).
    const QByteArray once = BatchPresetCodec::serialize(p);
    BatchPreset p2;
    QString err2;
    QVERIFY2(BatchPresetCodec::parse(once, &p2, &err2),
             qPrintable(QStringLiteral("re-parse refused: %1").arg(err2)));
    QVERIFY(p == p2);
    QCOMPARE(BatchPresetCodec::serialize(p2), once);
}

void TestBatchPresets::versionHandshakeRefusesNewerSchema() {
    // schemaVersion 2 - refused with the handshake message, never misparsed
    // (fail-closed forward-compat policy, plan 2.4).
    QJsonObject root = QJsonDocument::fromJson(goldenWebOptimizeJson()).object();
    QJsonObject env = root.value(QStringLiteral("glyphpreset")).toObject();
    env.insert(QStringLiteral("schemaVersion"), 2);
    root.insert(QStringLiteral("glyphpreset"), env);
    BatchPreset p;
    QString err;
    QVERIFY(!BatchPresetCodec::parse(QJsonDocument(root).toJson(), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("schema v2")), qPrintable(err));
    QVERIFY2(err.contains(QStringLiteral("v1")), qPrintable(err));
    QVERIFY2(err.contains(QStringLiteral("Update GlyphPDF")), qPrintable(err));

    // kind mismatch - a JSON file that is not a batch preset is refused.
    QJsonObject wrongKind = QJsonDocument::fromJson(goldenWebOptimizeJson()).object();
    QJsonObject env2 = wrongKind.value(QStringLiteral("glyphpreset")).toObject();
    env2.insert(QStringLiteral("kind"), QStringLiteral("watermark-preset"));
    wrongKind.insert(QStringLiteral("glyphpreset"), env2);
    QVERIFY(!BatchPresetCodec::parse(QJsonDocument(wrongKind).toJson(), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("kind")), qPrintable(err));

    // Unknown ENVELOPE key on a v1 file - V5 fail-closed (a newer app's extra
    // field is never tolerated-and-ignored).
    QJsonObject unknownEnv = QJsonDocument::fromJson(goldenWebOptimizeJson()).object();
    QJsonObject env3 = unknownEnv.value(QStringLiteral("glyphpreset")).toObject();
    env3.insert(QStringLiteral("newerKey"), true);
    unknownEnv.insert(QStringLiteral("glyphpreset"), env3);
    QVERIFY(!BatchPresetCodec::parse(QJsonDocument(unknownEnv).toJson(), &p, &err));
    QVERIFY2(err.contains(QStringLiteral("glyphpreset.newerKey")), qPrintable(err));
}

void TestBatchPresets::negativeValidationMatrix() {
    BatchPreset p;
    QString err;

    const auto mustRefuse = [&p, &err](const QString& json,
                                       const QString& pathNeedle,
                                       const QString& what) {
        err.clear();
        const bool ok = BatchPresetCodec::parse(json.toUtf8(), &p, &err);
        QVERIFY2(!ok, qPrintable(QStringLiteral("%1: parse unexpectedly SUCCEEDED").arg(what)));
        QVERIFY2(err.contains(pathNeedle),
                 qPrintable(QStringLiteral("%1: diagnostic '%2' does not name the path %3")
                                .arg(what, err, pathNeedle)));
    };

    const QString base = QString::fromUtf8(compressPresetJson("some-id"));
    const QString head = QStringLiteral(
        "{\"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" }, "
        "\"id\": \"x\", \"name\": \"X\", "
        "\"created\": \"2026-09-09T00:00:00.000Z\", "
        "\"modified\": \"2026-09-09T00:00:00.000Z\", ");
    const auto doc = [](const QString& stepsAndTail) {
        return QStringLiteral("{\"glyphpreset\": { \"schemaVersion\": 1, "
                              "\"kind\": \"batch-preset\" }, \"id\": \"x\", \"name\": \"X\", "
                              "\"created\": \"2026-09-09T00:00:00.000Z\", "
                              "\"modified\": \"2026-09-09T00:00:00.000Z\", \"steps\": ") +
               stepsAndTail + QStringLiteral("}");
    };

    // V1 unknown op - names the op path.
    mustRefuse(QString(base).replace(QStringLiteral("\"op\": \"compress\""),
                            QStringLiteral("\"op\": \"ocr-skip\"")),
               QStringLiteral("steps[0].op"), "V1 unknown op");

    // V2 unknown param key inside a known op.
    mustRefuse(QString(base).replace(QStringLiteral("\"quality\": 60"),
                            QStringLiteral("\"quality\": 60, \"bitrate\": 9")),
               QStringLiteral("steps[0].params.bitrate"), "V2 unknown param");

    // V3 wrong type (string where int belongs).
    mustRefuse(QString(base).replace(QStringLiteral("\"quality\": 60"),
                            QStringLiteral("\"quality\": \"high\"")),
               QStringLiteral("steps[0].params.quality"), "V3 wrong type");

    // V4 out of range (dpi outside the BatchMode clamp seams).
    mustRefuse(QString(base).replace(QStringLiteral("\"targetDpi\": 150"),
                            QStringLiteral("\"targetDpi\": 900")),
               QStringLiteral("steps[0].params.targetDpi"), "V4 dpi range");
    mustRefuse(QString(base).replace(QStringLiteral("\"targetDpi\": 150"),
                            QStringLiteral("\"targetDpi\": 12")),
               QStringLiteral("steps[0].params.targetDpi"), "V4 dpi low");
    // V4 unknown PDF/A level.
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"pdfa-export\", "
                                   "\"params\": { \"level\": \"4b\" } } ]")),
               QStringLiteral("steps[0].params.level"), "V4 pdfa level");
    // V4 invalid regex in redact patterns.
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"redact\", "
                                   "\"params\": { \"patterns\": [ \"[unclosed\" ] } } ]")),
               QStringLiteral("steps[0].params.patterns"), "V4 bad regex");
    // V4 unknown named-pattern key.
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"redact\", "
                                   "\"params\": { \"presets\": [ \"no-such-key\" ] } } ]")),
               QStringLiteral("steps[0].params.presets"), "V4 unknown preset key");
    // V4 redact with nothing effective would redact nothing.
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"redact\", "
                                   "\"params\": { \"presets\": [], \"patterns\": [] } } ]")),
               QStringLiteral("steps[0].params"), "V4 empty redact");

    // V5 unknown keys at every level.
    mustRefuse(QString(base).replace(QStringLiteral("\"id\": \"some-id\""),
                            QStringLiteral("\"id\": \"some-id\", \"surprise\": 1")),
               QStringLiteral("surprise"), "V5 unknown top-level key");
    mustRefuse(QString(base).replace(QStringLiteral("\"op\": \"compress\","),
                            QStringLiteral("\"op\": \"compress\", \"when\": \"later\",")),
               QStringLiteral("steps[0].when"), "V5 unknown step key");
    mustRefuse(QString(base).replace(QStringLiteral("\"steps\":"),
                            QStringLiteral("\"output\": { \"naming\": \"{basename}.pdf\", "
                                           "\"bogus\": 1 }, \"steps\":")),
               QStringLiteral("output.bogus"), "V5 unknown output key");

    // V7 naming tokens.
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"compress\", \"params\": {} } ], "
                                   "\"output\": { \"naming\": "
                                   "\"{basename}_{publisher}.pdf\" }")),
               QStringLiteral("output.naming"), "V7 unknown token");
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"compress\", \"params\": {} } ], "
                                   "\"output\": { \"naming\": \"{basename}.png\" }")),
               QStringLiteral("output.naming"), "V7 non-pdf naming");

    // V8 structure.
    mustRefuse(doc(QStringLiteral("[]")),
               QStringLiteral("steps"), "V8 empty steps");
    {
        QStringList stepItems;
        for (int i = 0; i < 17; ++i)
            stepItems << QStringLiteral("{ \"op\": \"compress\", \"params\": {} }");
        mustRefuse(doc(QStringLiteral("[ ") + stepItems.join(QStringLiteral(", ")) +
                         QStringLiteral(" ]")),
                   QStringLiteral("maximum"), "V8 too many steps");
    }
    // V8 id must equal the filename stem on load.
    {
        const QString dir = m_storeDir->path();
        QVERIFY(writeStoreFile(dir, QStringLiteral("different-stem"),
                               compressPresetJson("some-id")));
        BatchPreset loaded;
        QString loadErr;
        QVERIFY(!BatchPresetCodec::loadFile(
            dir + QStringLiteral("/different-stem.glyphpreset.json"), &loaded, &loadErr));
        QVERIFY2(loadErr.contains(QStringLiteral("stem")), qPrintable(loadErr));
    }

    // V9 resource cap: refuse BEFORE parsing.
    mustRefuse(QString(300 * 1024, QLatin1Char(' ')),
               QStringLiteral("256 KiB"), "V9 oversize file");

    // Not-implemented-but-schema-legal values are refused with an explicit
    // build-capability diagnostic - never silently reinterpreted.
    // R26-P2: onConflict "rename" is now IMPLEMENTED (P2 plan §4.3) - it
    // loads and validates, and an unknown conflict value is still refused
    // (fail-closed did not loosen).
    {
        BatchPreset renamed;
        QString okErr;
        QVERIFY2(BatchPresetCodec::parse(
            doc(QStringLiteral("[ { \"op\": \"compress\", \"params\": {} } ], "
                               "\"output\": { \"onConflict\": \"rename\" }")).toUtf8(),
            &renamed, &okErr),
            qPrintable(QStringLiteral("rename conflict policy refused: %1").arg(okErr)));
        QCOMPARE(renamed.onConflict, QStringLiteral("rename"));
    }
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"compress\", \"params\": {} } ], "
                                   "\"output\": { \"onConflict\": \"overwriteee\" }")),
               QStringLiteral("output.onConflict"), "unknown onConflict value");
    mustRefuse(doc(QStringLiteral("[ { \"op\": \"compress\", \"params\": {} } ], "
                                   "\"onFileFailure\": \"stop\"")),
               QStringLiteral("onFileFailure"), "onFileFailure stop refusal");
}

void TestBatchPresets::namingTokensResolveAndSanitize() {
    QString name;
    QString err;
    const QDate d(2026, 9, 15);

    // Default template.
    QVERIFY(BatchPresetSchema::resolveNaming({}, QStringLiteral("report"),
                                             QStringLiteral("web-optimize"), 3, d,
                                             &name, &err));
    QCOMPARE(name, QStringLiteral("report_web-optimize.pdf"));

    // Full token set.
    QVERIFY(BatchPresetSchema::resolveNaming(
        QStringLiteral("{n}_{basename}_{preset}_{date}.pdf"),
        QStringLiteral("a b"), QStringLiteral("p1"), 12, d, &name, &err));
    QCOMPARE(name, QStringLiteral("12_a b_p1_2026-09-15.pdf"));

    // Path separators are stripped from replacement values - no smuggling.
    QVERIFY(BatchPresetSchema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                             QStringLiteral("../evil/dir"), QStringLiteral("p"),
                                             1, d, &name, &err));
    QVERIFY2(!name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')),
             qPrintable(name));

    // Unknown token refused (V7).
    err.clear();
    QVERIFY(!BatchPresetSchema::resolveNaming(QStringLiteral("{basename}_{folder}.pdf"),
                                              QStringLiteral("a"), QStringLiteral("p"),
                                              1, d, &name, &err));
    QVERIFY(err.contains(QStringLiteral("{folder}")));

    QCOMPARE(BatchPresetSchema::compareVersions(QStringLiteral("1.4.0"),
                                                QStringLiteral("1.4")), 0);
    QCOMPARE(BatchPresetSchema::compareVersions(QStringLiteral("1.4.0"),
                                                QStringLiteral("1.5")), -1);
    QCOMPARE(BatchPresetSchema::compareVersions(QStringLiteral("2.0.0"),
                                                QStringLiteral("1.9.9")), 1);
    QCOMPARE(BatchPresetSchema::compareVersions(QStringLiteral("1.4.1"),
                                                QStringLiteral("1.4")), 1);
}

void TestBatchPresets::storeCrudRenameDeleteAndBrokenDisclosure() {
    BatchPresetStore store(m_storeDir->path());
    QVERIFY(store.list().isEmpty());
    QVERIFY(store.brokenFiles().isEmpty());

    // Save assigns a slug id + stamps; the file lands as <id>.glyphpreset.json.
    BatchPreset p;
    p.name = QStringLiteral("My Web Optimize!");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(store.save(&p, &err), qPrintable(err));
    QCOMPARE(p.id, QStringLiteral("my-web-optimize"));
    QVERIFY(p.created.isValid() && p.modified.isValid());
    QVERIFY(store.contains(p.id));
    QVERIFY(QFileInfo::exists(store.rootDir()
                              + QStringLiteral("/my-web-optimize.glyphpreset.json")));

    // Get round-trips; list sorts by display name.
    BatchPreset got;
    QVERIFY(store.get(QStringLiteral("my-web-optimize"), &got, &err));
    QCOMPARE(got.name, QStringLiteral("My Web Optimize!"));
    QCOMPARE(got.steps.size(), 1);
    QCOMPARE(got.steps.first().op, QStringLiteral("compress"));

    BatchPreset b;
    b.name = QStringLiteral("Aardvark");
    b.steps.append({ QStringLiteral("watermark"), {}, { { "text", "X" } } });
    QVERIFY(store.save(&b, &err));
    QCOMPARE(store.list().size(), 2);
    QCOMPARE(store.list().first().name, QStringLiteral("Aardvark"));

    // Id collision on create -> de-conflicted id, both files exist.
    BatchPreset dup;
    dup.name = QStringLiteral("My Web Optimize?");
    dup.steps.append({ QStringLiteral("compress"), {}, {} });
    QVERIFY(store.save(&dup, &err));
    QCOMPARE(dup.id, QStringLiteral("my-web-optimize-2"));

    // Rename edits the NAME, never the id or filename.
    QVERIFY2(store.rename(QStringLiteral("my-web-optimize"),
                          QStringLiteral("Renamed Preset"), &err),
             qPrintable(err));
    BatchPreset renamed;
    QVERIFY(store.get(QStringLiteral("my-web-optimize"), &renamed, &err));
    QCOMPARE(renamed.name, QStringLiteral("Renamed Preset"));
    QVERIFY(QFileInfo::exists(store.rootDir()
                              + QStringLiteral("/my-web-optimize.glyphpreset.json")));
    QVERIFY(!QFileInfo::exists(store.rootDir()
                               + QStringLiteral("/renamed-preset.glyphpreset.json")));

    // Rename honesty: an over-long name is refused, store unchanged.
    QString longErr;
    QVERIFY(!store.rename(QStringLiteral("my-web-optimize"),
                          QString(81, QLatin1Char('x')), &longErr));
    QVERIFY(store.get(QStringLiteral("my-web-optimize"), &renamed, &err));
    QCOMPARE(renamed.name, QStringLiteral("Renamed Preset"));

    // Delete; a second delete honestly fails (nothing to delete).
    QVERIFY(store.remove(QStringLiteral("my-web-optimize"), &err));
    QVERIFY(!store.contains(QStringLiteral("my-web-optimize")));
    QString delErr;
    QVERIFY(!store.remove(QStringLiteral("my-web-optimize"), &delErr));
    QVERIFY(!delErr.isEmpty());

    // A hand-corrupted preset file is DISCLOSED (brokenFiles), never silently
    // hidden from the picker's store.
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("corrupt"),
                           QByteArray("{ not valid json")));
    QCOMPARE(store.list().size(), 2);          // corrupt file invisible to list()...
    QCOMPARE(store.brokenFiles().size(), 1);   // ...but disclosed here
    QVERIFY(!store.brokenFiles().first().error.isEmpty());
}

// F2b-D1 (SWEEP-W3 UX audit, 2026-09-20): the FIRST-EVER preset save on a
// clean profile used to fail with an opaque QSaveFile "cannot open for
// writing — The system cannot find the path specified" because the store root
// directory did not exist yet and nothing created it. The store owns its root
// (rootDir()), so the save/rename boundary must create it (mkpath) before the
// QSaveFile write. Pins the clean-profile save-to-fresh-root contract.
void TestBatchPresets::firstSaveOnCleanProfileCreatesStoreRoot() {
    const QString freshRoot =
        m_storeDir->path() + QStringLiteral("/clean-profile/presets");
    BatchPresetStore store(freshRoot);
    QCOMPARE(store.rootDir(), freshRoot);
    QVERIFY2(!QFileInfo::exists(freshRoot),
             "precondition: the store root must not exist yet (clean profile)");

    BatchPreset p;
    p.name = QStringLiteral("First Save");
    p.steps.append({ QStringLiteral("compress"), {}, { { "quality", 60 } } });
    QString err;
    QVERIFY2(store.save(&p, &err),
             qPrintable(QStringLiteral(
                 "first-ever save on a clean profile must succeed: %1").arg(err)));
    QVERIFY(QFileInfo::exists(store.rootDir()
                              + QStringLiteral("/first-save.glyphpreset.json")));
    QVERIFY(store.contains(p.id));

    // The rename entry shares the same root boundary (symmetric contract).
    QVERIFY2(store.rename(p.id, QStringLiteral("First Save Renamed"), &err),
             qPrintable(err));
    BatchPreset renamed;
    QVERIFY(store.get(p.id, &renamed, &err));
    QCOMPARE(renamed.name, QStringLiteral("First Save Renamed"));
}

void TestBatchPresets::saveAsPresetFromConfiguredRunAndRefusals() {
    BatchMode bm;   // no AppContext: built-in ops stay available

    // Configure Compress (quality 40, DPI 72) and save it as a preset.
    bm.setOperationForTest(1 /* OpCompress */);
    const auto sliders = bm.findChildren<QSlider*>();
    QVERIFY(!sliders.isEmpty());
    sliders.first()->setValue(40);
    auto* dpi = bm.findChild<QSpinBox*>(QStringLiteral("batchCompressDpiSpin"));
    QVERIFY(dpi);
    dpi->setValue(72);

    QString err;
    QVERIFY2(bm.saveConfiguredOpAsPresetForTest(QStringLiteral("Web Optimize"), &err),
             qPrintable(err));
    QCOMPARE(bm.presetIdsForTest().size(), 1);

    // The stored params must match the configured widgets.
    BatchPresetStore store(m_storeDir->path());
    const auto presets = store.list();
    QCOMPARE(presets.size(), 1);
    QCOMPARE(presets.first().id, QStringLiteral("web-optimize"));
    QCOMPARE(presets.first().steps.first().op, QStringLiteral("compress"));
    QCOMPARE(presets.first().steps.first().params.value(QStringLiteral("quality")).toInt(), 40);
    QCOMPARE(presets.first().steps.first().params.value(QStringLiteral("targetDpi")).toInt(), 72);

    QVERIFY(bm.selectPresetForTest(QStringLiteral("web-optimize")));
    QVERIFY(bm.presetStepsDisplayForTest().contains(QStringLiteral("compress")));

    // Load + display a preset carrying a check step with NO registry: the
    // disclosure says the capability cannot be probed (never a fake
    // "available").
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("with-check"),
                           QString::fromUtf8(goldenWebOptimizeJson())
                               .replace(QStringLiteral("web-optimize"),
                                        QStringLiteral("with-check"))
                               .toUtf8()));
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("with-check")));
    const QString display = bm.presetStepsDisplayForTest();
    QVERIFY2(display.contains(QStringLiteral("pdfa-check")), qPrintable(display));
    QVERIFY2(display.contains(QStringLiteral("cannot be probed")), qPrintable(display));

    // Refusal: a Convert configuration cannot be saved as a preset.
    bm.setOperationForTest(0 /* OpConvert */);
    QString convErr;
    QVERIFY(!bm.saveConfiguredOpAsPresetForTest(QStringLiteral("Nope"), &convErr));
    QVERIFY2(convErr.contains(QStringLiteral("Convert")), qPrintable(convErr));
    // An unnamed preset is refused, not silently defaulted.
    bm.setOperationForTest(1);
    QString nameErr;
    QVERIFY(!bm.saveConfiguredOpAsPresetForTest(QStringLiteral("   "), &nameErr));
    QVERIFY(!nameErr.isEmpty());
    // Store unchanged by the refused saves.
    QCOMPARE(store.list().size(), 2);
}

void TestBatchPresets::presetRunMatchesManualConfiguration() {
    // 2-file MIXED fixture: one 1-page and one 2-page document, in their own
    // directory (the manual run writes outputs NEXT TO SOURCES - the default
    // out-dir behavior - so no panel lookup is needed).
    QDir().mkpath(m_runDir->filePath(QStringLiteral("fixtures")));
    const QString f1 = createTextPdf(m_runDir->filePath(QStringLiteral("fixtures")),
                                     QStringLiteral("one.pdf"),
                                     { QStringLiteral("Invoice one") });
    const QString f2 = createTextPdf(m_runDir->filePath(QStringLiteral("fixtures")),
                                     QStringLiteral("two.pdf"),
                                     { QStringLiteral("Page alpha"),
                                       QStringLiteral("Page beta") });
    QVERIFY(!f1.isEmpty() && !f2.isEmpty());

    // MANUAL run: compress quality 50 via the standard panel.
    BatchMode bmManual;
    AppContext ctxManual = makeCtx();
    bmManual.setAppContext(&ctxManual);
    bmManual.setOperationForTest(1 /* OpCompress */);
    bmManual.findChildren<QSlider*>().first()->setValue(50);
    bmManual.addFilesForTest({ f1, f2 });
    runAndWait(bmManual);
    QCOMPARE(bmManual.successCount(), 2);
    QCOMPARE(bmManual.failCount(), 0);
    QCOMPARE(bmManual.remainingCount(), 0);
    const QString manual1 = m_runDir->filePath(QStringLiteral("fixtures/one_compressed.pdf"));
    const QString manual2 = m_runDir->filePath(QStringLiteral("fixtures/two_compressed.pdf"));
    QVERIFY(QFileInfo::exists(manual1) && QFileInfo::exists(manual2));

    // PRESET run: the SAME configuration captured as a preset, run through
    // the preset pipeline.
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out-preset")));
    BatchMode bm;
    AppContext ctx = makeCtx();
    bm.setAppContext(&ctx);
    bm.setOperationForTest(1);
    bm.findChildren<QSlider*>().first()->setValue(50);
    QString err;
    QVERIFY2(bm.saveConfiguredOpAsPresetForTest(QStringLiteral("Web Optimize"), &err),
             qPrintable(err));
    QVERIFY(bm.selectPresetForTest(QStringLiteral("web-optimize")));
    auto* presetOut = bm.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
    QVERIFY(presetOut);
    presetOut->setText(m_runDir->filePath(QStringLiteral("out-preset")));
    bm.addFilesForTest({ f1, f2 });

    // G12 composition: the completion contract holds AT batchFinished.
    const auto snap = captureCompletion(bm);
    runAndWait(bm);
    QVERIFY(snap->fired);
    QCOMPARE(snap->success, 2);
    QCOMPARE(snap->fail, 0);
    QCOMPARE(snap->remaining, 0);

    // Artifact equality via the INDEPENDENT read path: each preset output
    // opens as a PDF with the input's page count and the input's text.
    const QString p1 = m_runDir->filePath(QStringLiteral("out-preset/one_web-optimize.pdf"));
    const QString p2 = m_runDir->filePath(QStringLiteral("out-preset/two_web-optimize.pdf"));
    QVERIFY(QFileInfo::exists(p1));
    QVERIFY(QFileInfo::exists(p2));
    QPdfDocument doc1;
    doc1.load(p1);
    QCOMPARE(doc1.status(), QPdfDocument::Status::Ready);
    QCOMPARE(doc1.pageCount(), 1);
    QVERIFY(doc1.getAllText(0).text().contains(QStringLiteral("Invoice one")));
    QPdfDocument doc2;
    doc2.load(p2);
    QCOMPARE(doc2.status(), QPdfDocument::Status::Ready);
    QCOMPARE(doc2.pageCount(), 2);
    QVERIFY(doc2.getAllText(0).text().contains(QStringLiteral("Page alpha")));
    QVERIFY(doc2.getAllText(1).text().contains(QStringLiteral("Page beta")));

    // SAME semantics as the manual run: identical page counts (byte equality
    // is impossible - PDF timestamps differ run to run).
    QPdfDocument m1;
    m1.load(manual1);
    QCOMPARE(m1.status(), QPdfDocument::Status::Ready);
    QCOMPARE(m1.pageCount(), doc1.pageCount());
    QPdfDocument m2;
    m2.load(manual2);
    QCOMPARE(m2.status(), QPdfDocument::Status::Ready);
    QCOMPARE(m2.pageCount(), doc2.pageCount());

    // NO temp residue in the output directory: exactly the two outputs.
    QCOMPARE(QDir(m_runDir->filePath(QStringLiteral("out-preset")))
                 .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files)
                 .size(), 2);
}

void TestBatchPresets::unavailableCapabilityStepDisclosesAndRefuses() {
    // Stub registry: the pdfa-check step's veraPDF capability is UNAVAILABLE -
    // the registry's whyNot + alternative must reach BOTH the design-time
    // display and the run-time refusal.
    CapabilityRegistry caps;
    caps.registerProbe(CapId::PdfAValidation, [](const QVariant&) {
        Capability c;
        c.status = Availability::UnavailableRuntime;
        c.whyNot = QStringLiteral("stub: veraPDF validator is missing");
        c.alternative = QStringLiteral("stub: install veraPDF to enable the check");
        return c;
    });
    AppContext ctx = makeCtx();
    ctx.capabilities = std::shared_ptr<CapabilityRegistry>(&caps, [](CapabilityRegistry*) {});

    const QString f1 = createTextPdf(m_runDir->path(), QStringLiteral("doc.pdf"),
                                     { QStringLiteral("hello world") });
    QVERIFY(!f1.isEmpty());
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("web-optimize"),
                           goldenWebOptimizeJson()));

    QDir().mkpath(m_runDir->filePath(QStringLiteral("out-blocked")));
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7 /* OpPresetPipeline */);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("web-optimize")));

    // Design time: the step DISPLAYS the registry's whyNot.
    const QString display = bm.presetStepsDisplayForTest();
    QVERIFY2(display.contains(QStringLiteral("stub: veraPDF validator is missing")),
             qPrintable(display));

    // Run time: EVERY file is staged failed with the same whyNot - no worker
    // runs, no output exists, the summary stays truthful.
    auto* presetOut = bm.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
    QVERIFY(presetOut);
    presetOut->setText(m_runDir->filePath(QStringLiteral("out-blocked")));
    bm.addFilesForTest({ f1 });
    const auto snap = captureCompletion(bm);
    runAndWait(bm);
    QVERIFY(snap->fired);
    QCOMPARE(snap->success, 0);
    QCOMPARE(snap->fail, 1);
    QCOMPARE(snap->remaining, 0);
    QCOMPARE(bm.errorLogCount(), 1);
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("stub: veraPDF validator is missing")),
             qPrintable(log));
    QCOMPARE(QDir(m_runDir->filePath(QStringLiteral("out-blocked")))
                 .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files).size(), 0);

    // Positive control: the same registry, a preset whose steps are all
    // available, runs fine.
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("compress-only"),
                           compressPresetJson(QStringLiteral("compress-only"))));
    BatchMode bm2;
    bm2.setAppContext(&ctx);
    bm2.setOperationForTest(7 /* OpPresetPipeline */);
    bm2.refreshPresetsForTest();
    QVERIFY(bm2.selectPresetForTest(QStringLiteral("compress-only")));
    auto* out2 = bm2.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
    QVERIFY(out2);
    out2->setText(m_runDir->filePath(QStringLiteral("out-blocked")));
    bm2.addFilesForTest({ f1 });
    runAndWait(bm2);
    QCOMPARE(bm2.successCount(), 1);
    QCOMPARE(bm2.failCount(), 0);
}

void TestBatchPresets::zeroOpPresetRefusesToLoadAndRun() {
    // A preset with zero steps cannot be created through the store -
    // validation refuses at every layer (V8).
    BatchPreset p;
    p.id = QStringLiteral("empty");
    p.name = QStringLiteral("Empty");
    p.created = p.modified = QDateTime::currentDateTimeUtc();
    QString err;
    QVERIFY(!BatchPresetCodec::validate(p, &err));
    QVERIFY2(err.contains(QStringLiteral("at least one step")), qPrintable(err));

    BatchPresetStore store(m_storeDir->path());
    QVERIFY(!store.save(&p, &err));
    QVERIFY(!store.contains(QStringLiteral("empty")));

    // A hand-written zero-step file is a broken file: disclosed, and it never
    // reaches the picker, so it can never run.
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("empty"),
                           QByteArray("{ \"glyphpreset\": { \"schemaVersion\": 1, "
                                      "\"kind\": \"batch-preset\" }, "
                                      "\"id\": \"empty\", \"name\": \"Empty\", "
                                      "\"created\": \"2026-09-09T00:00:00.000Z\", "
                                      "\"modified\": \"2026-09-09T00:00:00.000Z\", "
                                      "\"steps\": [] }")));
    QVERIFY(store.list().isEmpty());
    QCOMPARE(store.brokenFiles().size(), 1);

    BatchMode bm;
    bm.refreshPresetsForTest();
    QVERIFY(!bm.selectPresetForTest(QStringLiteral("empty")));   // not offered
    QVERIFY(bm.presetIdsForTest().isEmpty());   // the picker hides broken files
    // Defense in depth at the run gate: an unselected preset refuses.
    QVERIFY(!bm.presetRunBlockerForTest().isEmpty());
}

void TestBatchPresets::minAppVersionBlocksOlderApp() {
    // The app reports 1.4.0 (pinned in initTestCase): a preset requiring 99.0
    // loads and displays, but the run gate refuses it with whyNot.
    QJsonObject root = QJsonDocument::fromJson(
        compressPresetJson(QStringLiteral("futurum"))).object();
    QJsonObject env = root.value(QStringLiteral("glyphpreset")).toObject();
    env.insert(QStringLiteral("minAppVersion"), QStringLiteral("99.0.0"));
    root.insert(QStringLiteral("glyphpreset"), env);
    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("futurum"),
                           QJsonDocument(root).toJson()));

    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7 /* OpPresetPipeline */);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("futurum")));   // loads + displays

    const QString f1 = createTextPdf(m_runDir->path(), QStringLiteral("m.pdf"),
                                     { QStringLiteral("m") });
    QVERIFY(!f1.isEmpty());
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out-min")));
    auto* presetOut = bm.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
    QVERIFY(presetOut);
    presetOut->setText(m_runDir->filePath(QStringLiteral("out-min")));
    bm.addFilesForTest({ f1 });
    runAndWait(bm);
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.failCount(), 1);
    const QString log = bm.findChildren<QTextEdit*>().first()->toPlainText();
    QVERIFY2(log.contains(QStringLiteral("99.0.0")), qPrintable(log));
    QCOMPARE(QDir(m_runDir->filePath(QStringLiteral("out-min")))
                 .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files).size(), 0);
}

void TestBatchPresets::deleteAndRenameHonestyThroughUiSeams() {
    BatchMode bm;   // no AppContext needed for the CRUD surface
    bm.setOperationForTest(2 /* OpWatermark */);
    auto* wmText = watermarkTextEdit(bm);
    QVERIFY(wmText);
    wmText->setText(QStringLiteral("TOP SECRET"));
    QString err;
    QVERIFY2(bm.saveConfiguredOpAsPresetForTest(QStringLiteral("Secret Stamper"), &err),
             qPrintable(err));
    const QStringList ids = bm.presetIdsForTest();
    QCOMPARE(ids.size(), 1);
    const QString id = ids.first();

    // Stored params match the configured widgets.
    BatchPresetStore store(m_storeDir->path());
    BatchPreset got;
    QVERIFY(store.get(id, &got, &err));
    QCOMPARE(got.steps.first().op, QStringLiteral("watermark"));
    QCOMPARE(got.steps.first().params.value(QStringLiteral("text")).toString(),
             QStringLiteral("TOP SECRET"));

    // Rename through the seam: name changes, id and file stay stable, the
    // picker refreshes with the new name.
    QVERIFY2(bm.renamePresetForTest(id, QStringLiteral("Renamed Stamper"), &err),
             qPrintable(err));
    QVERIFY(store.get(id, &got, &err));
    QCOMPARE(got.name, QStringLiteral("Renamed Stamper"));
    QVERIFY(bm.presetIdsForTest().contains(id));

    // Over-long rename refused; store unchanged.
    QVERIFY(!bm.renamePresetForTest(id, QString(81, QLatin1Char('y')), &err));
    QVERIFY(store.get(id, &got, &err));
    QCOMPARE(got.name, QStringLiteral("Renamed Stamper"));

    // Delete through the seam (the button path's post-confirm action): the
    // picker drops it, the file is gone, and deleting again honestly reports
    // there is nothing to delete.
    QVERIFY2(bm.deletePresetForTest(id, &err), qPrintable(err));
    QVERIFY(!store.contains(id));
    QVERIFY(bm.presetIdsForTest().isEmpty());
    QString delErr;
    QVERIFY(!bm.deletePresetForTest(id, &delErr));
    QVERIFY(!delErr.isEmpty());
}

void TestBatchPresets::transactionalFailureLeavesOriginalUntouched() {
    // A corrupt input fails inside the chain; the original stays
    // byte-identical, no output is created, the file counts as failed.
    const QString bad = m_runDir->filePath(QStringLiteral("bad.pdf"));
    {
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a pdf at all");
    }
    const QByteArray original = QFile(bad).readAll();

    QVERIFY(writeStoreFile(m_storeDir->path(), QStringLiteral("compress-only"),
                           compressPresetJson(QStringLiteral("compress-only"))));
    AppContext ctx = makeCtx();
    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(7 /* OpPresetPipeline */);
    bm.refreshPresetsForTest();
    QVERIFY(bm.selectPresetForTest(QStringLiteral("compress-only")));
    QDir().mkpath(m_runDir->filePath(QStringLiteral("out-tx")));
    auto* presetOut = bm.findChild<QLineEdit*>(QStringLiteral("batchPresetOutDir"));
    QVERIFY(presetOut);
    presetOut->setText(m_runDir->filePath(QStringLiteral("out-tx")));
    bm.addFilesForTest({ bad });
    runAndWait(bm);
    QCOMPARE(bm.successCount(), 0);
    QCOMPARE(bm.failCount(), 1);
    QCOMPARE(bm.remainingCount(), 0);
    QCOMPARE(bm.errorLogCount(), 1);
    // The original is byte-identical; no output was created.
    QCOMPARE(QFile(bad).readAll(), original);
    QCOMPARE(QDir(m_runDir->filePath(QStringLiteral("out-tx")))
                 .entryList(QStringList() << QStringLiteral("*.pdf"), QDir::Files).size(), 0);
}

QTEST_MAIN(TestBatchPresets)
#include "TestBatchPresets.moc"
