// SPDX-License-Identifier: Apache-2.0
// R24 wiring closure — the FOUR "enforcement pending" policy keys wired at
// their REAL enforcement points (pin the OBSERVABLE behavior, not the
// effectiveValue seam):
//
//   update/checkOnStartup   → MainWindow::startupUpdateCheckEnabled()
//                             (consulted by MainWindow::initUpdateChecker —
//                             the startup check does not even start)
//   update/channel          → MainWindow::startupUpdateChannel()
//                             (decides the manifest URL the updater uses)
//   ai/ollamaEndpoint       → OllamaProvider::resolveEndpoint() — the ONE
//                             endpoint gate feeding isReady()/chat(); a
//                             policy-managed EMPTY endpoint disables AI chat
//                             (probe unavailable + whyNot naming the policy)
//   ocr/allowNetworkDownload→ OcrEngine::initialize()'s download gate — the
//                             engine REFUSES the model load (no download
//                             attempted) when the policy says no, even if
//                             the user preference says yes
//
// Plus the disclosure half: the NetworkTouchpoints enumeration (the R24
// network page) derives the update-check / ocr-traineddata states from the
// EFFECTIVE values — under a policy the page must not show the user's raw
// preference.
//
// Idioms reused from TestPolicyController: GLYPHPDF_POLICY_PATH env seam +
// PolicyController::resetForTesting() in init/cleanup; QSettings isolated
// via the GlyphPDFTests org.
#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFuture>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/NetworkTouchpoints.h"
#include "core/PolicyController.h"
#include "core/UpdateChecker.h"
#include "engines/OcrEngine.h"
#include "engines/ai/OllamaProvider.h"
#include "shell/controllers/SecurityController.h" // emergence E-5: TSA refusal wording
#include "GpMainWindow.h"

using gp::MainWindow;
using gp::NetworkTouchpoints;
using gp::OllamaProvider;
using gp::PolicyController;
using gp::UpdateChecker;

namespace {

bool writePolicyFile(const QString& path, const QByteArray& contents)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(contents) == contents.size();
}

QJsonObject makePolicyRoot(const QJsonObject& settings)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("settings"), settings);
    return root;
}

// Captured Qt warnings — the OCR refusal is observable through its honest
// qWarning ("OCR download disabled"); a download ATTEMPT logs different
// text ("Timed out/Failed downloading"), so the pin distinguishes
// "refused before any attempt" from "attempted and failed".
QStringList g_warnings;
void warningCapturingHandler(QtMsgType type, const QMessageLogContext&,
                             const QString& msg)
{
    if (type == QtWarningMsg)
        g_warnings << msg;
}

} // namespace

class TestPolicyWiring : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    bool loadPolicy(const QJsonObject& settings)
    {
        const QString path = m_dir.filePath(QStringLiteral("policy.json"));
        if (!writePolicyFile(path, QJsonDocument(makePolicyRoot(settings)).toJson()))
            return false;
        return PolicyController::instance().load(path);
    }

    static void setUserPref(const QString& key, const QVariant& value)
    {
        QSettings store;
        store.setValue(key, value);
        store.sync();
    }

private slots:
    void initTestCase()
    {
        // Isolate QSettings: never read the user's real prefs, never clobber.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestPolicyWiring"));
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        PolicyController::instance().resetForTesting();
        // W1-05 structural close: these pins exercise the ENFORCEMENT WIRING
        // (policy key → observable enforcement point), not the ownership
        // gate, and their fixtures are necessarily written by this test
        // process (a standard-user-owned file). The disclosed
        // GLYPHPDF_POLICY_ASSUME_TRUSTED seam keeps the policy loadable for
        // the wiring pins; the GATE itself is pinned separately in
        // plantedPolicyFromUserWritablePathIsNotEnforced, which runs WITHOUT
        // the seam.
        qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED", "1");
        g_warnings.clear();
    }

    void cleanup()
    {
        PolicyController::instance().resetForTesting();
        qunsetenv("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        qInstallMessageHandler(nullptr);
        g_warnings.clear();
    }

    // ── update/checkOnStartup at the decision point ──────────────────────
    void startupCheckPolicyOverridesUserPref()
    {
        // User opted IN; the machine policy says NO → the startup check does
        // not run (initUpdateChecker returns before creating any updater).
        setUserPref(QStringLiteral("update/checkOnStartup"), true);
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("update/checkOnStartup"), false}}));
        QCOMPARE(MainWindow::startupUpdateCheckEnabled(), false);

        // And the override works in the honest other direction too: the user
        // never opted in, the machine admin requires the check.
        setUserPref(QStringLiteral("update/checkOnStartup"), false);
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("update/checkOnStartup"), true}}));
        QCOMPARE(MainWindow::startupUpdateCheckEnabled(), true);

        // No policy → the user preference rules (AR-8 D6 contract unchanged).
        PolicyController::instance().resetForTesting();
        setUserPref(QStringLiteral("update/checkOnStartup"), true);
        QCOMPARE(MainWindow::startupUpdateCheckEnabled(), true);
        setUserPref(QStringLiteral("update/checkOnStartup"), false);
        QCOMPARE(MainWindow::startupUpdateCheckEnabled(), false);
    }

    // ── update/channel at the decision point ─────────────────────────────
    void startupChannelPolicyOverridesUserPref()
    {
        setUserPref(QStringLiteral("update/checkOnStartup"), true);
        setUserPref(QStringLiteral("update/channel"), QStringLiteral("beta"));

        // Policy pins the machine to the stable channel — the beta manifest
        // URL is never configured even though the user chose beta.
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("update/channel"), QStringLiteral("stable")}}));
        QCOMPARE(MainWindow::startupUpdateChannel(), QStringLiteral("stable"));

        // The consequence is real: the channels map to DIFFERENT manifests,
        // so a policy-pinned stable channel means the beta manifest (and its
        // earlier betas) is never fetched at startup.
        QVERIFY(UpdateChecker::manifestUrlForChannel(QStringLiteral("beta"))
                != UpdateChecker::manifestUrlForChannel(QStringLiteral("stable")));

        // Policy can put the whole machine on the beta channel.
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("update/channel"), QStringLiteral("beta")}}));
        QCOMPARE(MainWindow::startupUpdateChannel(), QStringLiteral("beta"));

        // No policy → user pref.
        PolicyController::instance().resetForTesting();
        QCOMPARE(MainWindow::startupUpdateChannel(), QStringLiteral("beta"));
    }

    // ── ai/ollamaEndpoint at the endpoint gate ────────────────────────────
    void ollamaEndpointPolicyOverridesUserPref()
    {
        setUserPref(QStringLiteral("ai/ollamaEndpoint"),
                    QStringLiteral("http://localhost:11434"));

        // Policy points the machine at a different (allowed, loopback) port.
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("ai/ollamaEndpoint"),
             QStringLiteral("http://localhost:11499")}}));
        QCOMPARE(OllamaProvider::resolveEndpoint(QString()),
                 QStringLiteral("http://localhost:11499"));

        // No policy → the stored user endpoint (R04 contract unchanged).
        PolicyController::instance().resetForTesting();
        QCOMPARE(OllamaProvider::resolveEndpoint(QString()),
                 QStringLiteral("http://localhost:11434"));
    }

    void ollamaPolicyEmptyEndpointDisablesAiHonestly()
    {
        // The user has a working local endpoint; the admin disables AI chat
        // for the whole machine by managing the key with an EMPTY value.
        setUserPref(QStringLiteral("ai/ollamaEndpoint"),
                    QStringLiteral("http://localhost:11434"));
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("ai/ollamaEndpoint"), QString()}}));

        // The probe reports honestly unavailable — and does NOT silently
        // fall back to the user's endpoint (or any default).
        QCOMPARE(OllamaProvider::resolveEndpoint(QString()), QString());
        OllamaProvider provider;
        QCOMPARE(provider.isReady(), false);

        // chat() refuses with a whyNot that NAMES the policy.
        const gp::AiResult r =
            provider.chat({{QStringLiteral("user"), QStringLiteral("hi")}}).result();
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("policy"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("whyNot must name the policy, got: %1")
                                .arg(r.errorMsg)));
    }

    void ollamaPolicyEndpointStillGuardedByR04()
    {
        // A policy cannot bypass the R04 endpoint guard: a non-loopback
        // cleartext HTTP endpoint falls back to the safe default exactly as
        // the user-pref path always did.
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("ai/ollamaEndpoint"),
             QStringLiteral("http://attacker.example:8080")}}));
        QCOMPARE(OllamaProvider::resolveEndpoint(QString()),
                 QStringLiteral("http://localhost:11434"));
    }

    // ── ocr/allowNetworkDownload at the model-load gate ───────────────────
    void ocrDownloadPolicyGateRefusesEngineLoad()
    {
        // User says YES; the machine policy says NO → the engine refuses the
        // model load without ANY download attempt (the honest refusal warning
        // is logged; no network artifacts appear).
        setUserPref(QStringLiteral("ocr/allowNetworkDownload"), true);
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("ocr/allowNetworkDownload"), false}}));

        const QString dataPath =
            QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/tessdata-policy-wiring-1");
        qInstallMessageHandler(warningCapturingHandler);
        OcrEngine engine;
        const bool loaded = engine.initialize(QStringLiteral("eng"), dataPath);
        QCOMPARE(loaded, false);
        QVERIFY2(g_warnings.contains(QStringLiteral("OCR download disabled")),
                 "expected the honest OCR-download refusal warning");
        // The engine never even attempted the download: no traineddata file
        // was written into the data directory.
        QDir dir(dataPath);
        if (dir.exists())
            QCOMPARE(dir.entryList(QDir::Files).isEmpty(), true);
        QDir(QStandardPaths::writableLocation(
                 QStandardPaths::AppLocalDataLocation))
            .removeRecursively();
    }

    void ocrDownloadUserPrefOnlyStillWorks()
    {
        // No policy → the existing gate behavior is unchanged: user OFF
        // means no download, with the same honest refusal.
        setUserPref(QStringLiteral("ocr/allowNetworkDownload"), false);
        const QString dataPath =
            QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/tessdata-policy-wiring-2");
        qInstallMessageHandler(warningCapturingHandler);
        OcrEngine engine;
        QCOMPARE(engine.initialize(QStringLiteral("eng"), dataPath), false);
        QVERIFY2(g_warnings.contains(QStringLiteral("OCR download disabled")),
                 "expected the honest OCR-download refusal warning");
        QDir(QStandardPaths::writableLocation(
                 QStandardPaths::AppLocalDataLocation))
            .removeRecursively();
    }

    // ── the disclosure half: the network page follows the policy ─────────
    void networkPageRowsFollowPolicy()
    {
        // User opted INTO both network features; the policy forces both OFF.
        QSettings store;
        store.setValue(QStringLiteral("update/checkOnStartup"), true);
        store.setValue(QStringLiteral("ocr/allowNetworkDownload"), true);
        store.sync();
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("update/checkOnStartup"), false},
            {QStringLiteral("ocr/allowNetworkDownload"), false}}));

        QHash<QString, bool> states;
        for (const auto& tp : NetworkTouchpoints::enumerate(store))
            states.insert(tp.id, tp.enabled);
        // The page must not show the user's raw preference under a policy.
        QCOMPARE(states.value(QStringLiteral("update-check")), false);
        QCOMPARE(states.value(QStringLiteral("ocr-traineddata")), false);

        // And the honest other direction: the policy can ENABLE a download
        // the user never consented to — disclosed, not silently assumed away.
        store.setValue(QStringLiteral("ocr/allowNetworkDownload"), false);
        store.sync();
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("ocr/allowNetworkDownload"), true}}));
        for (const auto& tp : NetworkTouchpoints::enumerate(store)) {
            if (tp.id == QLatin1String("ocr-traineddata"))
                QCOMPARE(tp.enabled, true);
        }

        // No policy → the enumeration keeps its user-pref derivation.
        PolicyController::instance().resetForTesting();
        for (const auto& tp : NetworkTouchpoints::enumerate(store)) {
            if (tp.id == QLatin1String("ocr-traineddata"))
                QCOMPARE(tp.enabled, false);
        }
    }

    // ── emergence E-5 (SWEEP-W3-EMERGENCE §5): the TSA refusal names the ────
    // policy. Under a policy-managed EMPTY signing/tsaUrl the old wording
    // sent the user to "Preferences → Security → Signing" — a control the
    // policy DISABLES for exactly that key (the R24 disclosure lives there,
    // not the setter). The refusal must name machine policy; the unmanaged
    // refusal keeps the Preferences advice.
    void tsaRefusalNamesPolicyUnderManagedEmptyTsaUrl()
    {
        // User has a URL configured; the machine policy EMPTIES it (the exact
        // composition the audit flagged: refusal fires while the Preferences
        // control is dead).
        setUserPref(QStringLiteral("signing/tsaUrl"), QStringLiteral("http://tsa.example.org"));
        QVERIFY(loadPolicy(QJsonObject{
            {QStringLiteral("signing/tsaUrl"), QString("")}}));

        const QString refusal = gp::SecurityController::signingPreflightRefusal(
            PAdESLevel::B_T, QString(), /*forTimestamp*/ false);
        QVERIFY2(!refusal.isEmpty(), "a B-T request without a TSA URL must refuse");
        QVERIFY2(refusal.contains(QLatin1String("managed by machine policy")),
                 qPrintable(QStringLiteral("the refusal must name the machine policy, got: %1")
                                .arg(refusal)));
        QVERIFY2(refusal.contains(QLatin1String("signing/tsaUrl")),
                 qPrintable(refusal));
        QVERIFY2(!refusal.contains(QLatin1String("Set the TSA URL under Preferences")),
                 qPrintable(QStringLiteral("the managed refusal must not direct to a dead "
                                          "setter, got: %1").arg(refusal)));

        // Same for the timestamp-only entry point.
        const QString ts = gp::SecurityController::signingPreflightRefusal(
            PAdESLevel::B_B, QString(), /*forTimestamp*/ true);
        QVERIFY2(!ts.isEmpty(), "a timestamp request without a TSA URL must refuse");
        QVERIFY2(ts.contains(QLatin1String("managed by machine policy")),
                 qPrintable(ts));

        // Unmanaged (no policy): the advice stays actionable Preferences.
        PolicyController::instance().resetForTesting();
        const QString plain = gp::SecurityController::signingPreflightRefusal(
            PAdESLevel::B_T, QString(), false);
        QVERIFY2(!plain.isEmpty(), "the unmanaged refusal must still fire");
        QVERIFY2(plain.contains(QLatin1String("Set the TSA URL under Preferences")),
                 qPrintable(plain));
        QVERIFY2(!plain.contains(QLatin1String("machine policy")),
                 qPrintable(plain));
    }

    // ── W1-05 structural close: a user-writable (non-admin-owned) policy
    // file is NOT enforced ─────────────────────────────────────────────────
    // The natural squatter fixture: a schema-valid policy the test process
    // (a standard user) wrote — its default Windows owner IS the creating
    // user, exactly the planted-policy posture the W1-05 structural close
    // denies. The gate must ignore it AND disclose why. Runs WITHOUT the
    // GLYPHPDF_POLICY_ASSUME_TRUSTED seam (restored on exit); the wiring
    // pins above use the seam and are unaffected by the gate.
    void plantedPolicyFromUserWritablePathIsNotEnforced()
    {
#ifdef Q_OS_WIN
        const QString savedSeam =
            qEnvironmentVariable("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        qunsetenv("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        const auto restoreSeam = qScopeGuard([&] {
            if (!savedSeam.isEmpty())
                qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED",
                        savedSeam.toUtf8());
        });

        // A schema-valid policy disabling a managed setting, planted at a
        // path whose ACL is the creating user's (default for a test file).
        const QString path =
            m_dir.filePath(QStringLiteral("planted-policy.json"));
        QJsonObject settings;
        settings.insert(QStringLiteral("update/checkOnStartup"), false);
        QVERIFY(writePolicyFile(
            path, QJsonDocument(makePolicyRoot(settings)).toJson()));

        auto& policy = PolicyController::instance();
        policy.resetForTesting();
        // Not "loaded": a non-admin-owned file never becomes machine policy.
        QVERIFY2(!policy.load(path),
                 "W1-05 REGRESSION: a user-writable planted policy loaded as "
                 "machine policy");
        QCOMPARE(policy.state(), PolicyController::State::UntrustedOwner);
        // Nothing from the planted file reaches any enforcement point...
        QVERIFY2(!policy.isManaged(QStringLiteral("update/checkOnStartup")),
                 "planted key is managed — the squat payload would enforce");
        // ...the user's own preference stays in force...
        QCOMPARE(policy.effectiveValue(QStringLiteral("update/checkOnStartup"),
                                       QVariant(true)),
                 QVariant(true));
        // ...and the refusal is DISCLOSED where policy status renders.
        QVERIFY2(policy.statusLine().contains(QStringLiteral("IGNORED")),
                 qPrintable(QStringLiteral(
                     "the untrusted-owner refusal must be disclosed in the "
                     "status line, got: %1").arg(policy.statusLine())));
#else
        QSKIP("the ownership gate is a Windows surface (Q_OS_WIN)");
#endif
    }
};

#include "TestPolicyWiring.moc"

QTEST_MAIN(TestPolicyWiring)
