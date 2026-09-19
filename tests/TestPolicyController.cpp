// SPDX-License-Identifier: Apache-2.0
// R24(a) — machine-level policy that overrides user preferences, with the
// override VISIBLE in the UI (visible-in-UI is the point: never silently).
//
// Pins:
//   1. Policy load + precedence: a seeded %PROGRAMDATA%-shaped policy.json
//      makes PolicyController::effectiveValue return the POLICY value for a
//      managed key while unmanaged keys pass the user value through.
//   2. Consumption seam: SecurityController::readSigningConfig (the ONE
//      production reader of signing/tsaUrl + signing/padesLevel) returns the
//      policy values under a seeded policy, user values without one.
//   3. UI managed-state disclosure: PreferencesDialog under a seeded policy
//      renders the managed row with the policy value, DISABLED, badged
//      "Managed by policy", plus the honest status line (path + key count).
//   4. Save guard: persistSetting refuses to write a managed key (the user
//      edit cannot silently diverge from what the app will do).
//   5. Tampered/invalid policy.json is IGNORED honestly (state=Invalid,
//      status line discloses, zero managed keys — user prefs still work) and
//      a missing policy file is an honest NoPolicy, not an error.
//   6. Bounded allowlist: keys outside the audited allowlist are ignored AND
//      disclosed (unrecognizedKeys) — the policy surface stays auditable.
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "core/PolicyController.h"
#include "shell/controllers/SecurityController.h"
#include "ui/PreferencesDialog.h"

using gp::PolicyController;
using gp::PreferencesDialog;
using gp::SecurityController;

namespace {

bool writePolicyFile(const QString& path, const QByteArray& contents)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(contents) == contents.size();
}

QJsonObject makePolicyObject()
{
    QJsonObject settings;
    settings.insert(QStringLiteral("signing/tsaUrl"),
                    QStringLiteral("https://tsa.corp.internal.example/rfc3161"));
    settings.insert(QStringLiteral("signing/padesLevel"), QStringLiteral("B-B"));
    settings.insert(QStringLiteral("update/checkOnStartup"), false);
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("settings"), settings);
    return root;
}

} // namespace

class TestPolicyController : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    // Loads `root` as the machine policy via an EXPLICIT load(path) — the
    // production default path is never touched. (The env-var seam itself is
    // pinned separately below: defaultPolicyPathHonorsEnvSeam.)
    bool loadPolicy(const QJsonObject& root)
    {
        const QString path = m_dir.filePath(QStringLiteral("policy.json"));
        if (!writePolicyFile(path, QJsonDocument(root).toJson()))
            return false;
        return PolicyController::instance().load(path);
    }

private slots:
    void initTestCase()
    {
        // Isolate QSettings: never read the user's real prefs, never clobber.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestPolicyController"));
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        PolicyController::instance().resetForTesting();
    }

    void cleanup()
    {
        PolicyController::instance().resetForTesting();
    }

    // ── Pin 1: precedence ────────────────────────────────────────────────
    void effectiveValuePrefersPolicyForManagedKeys()
    {
        QVERIFY(loadPolicy(makePolicyObject()));
        const auto& pc = PolicyController::instance();

        QCOMPARE(pc.state(), PolicyController::State::Loaded);
        QVERIFY(pc.isManaged(QStringLiteral("signing/tsaUrl")));
        QVERIFY(pc.isManaged(QStringLiteral("update/checkOnStartup")));

        // Managed key: policy WINS over the user value.
        QCOMPARE(pc.effectiveValue(QStringLiteral("signing/tsaUrl"),
                                   QStringLiteral("https://user.example/tsa"))
                     .toString(),
                 QStringLiteral("https://tsa.corp.internal.example/rfc3161"));
        // Unmanaged key: the user value passes through untouched.
        QCOMPARE(pc.effectiveValue(QStringLiteral("ui/theme"),
                                   QStringLiteral("light"))
                     .toString(),
                 QStringLiteral("light"));
        // Bool values survive as bools (a policy forcing a toggle OFF).
        QCOMPARE(pc.effectiveValue(QStringLiteral("update/checkOnStartup"), true)
                     .toBool(),
                 false);
    }

    void noPolicyMeansUserPrefsWin()
    {
        // No load() call at all: nothing is managed, everything passes through.
        const auto& pc = PolicyController::instance();
        QCOMPARE(pc.state(), PolicyController::State::NoPolicy);
        QVERIFY(pc.managedKeys().isEmpty());
        QVERIFY(!pc.isManaged(QStringLiteral("signing/tsaUrl")));
        QCOMPARE(pc.effectiveValue(QStringLiteral("signing/tsaUrl"),
                                   QStringLiteral("https://user.example/tsa"))
                     .toString(),
                 QStringLiteral("https://user.example/tsa"));
        // The honest status line names where the app looked.
        QVERIFY(pc.statusLine().contains(QStringLiteral("policy"),
                                         Qt::CaseInsensitive));
    }

    // ── Pin: the default-path env seam (SWEEP-QUALITY-NEW P1) ────────────
    // GLYPHPDF_POLICY_PATH decides the production default path; every
    // ensureLoaded() consumer (support bundle, Preferences status line)
    // rides on it. Pin the seam so it can never silently break.
    void defaultPolicyPathHonorsEnvSeam()
    {
        const QString envPath =
            m_dir.filePath(QStringLiteral("env-seam-policy.json"));
        qputenv("GLYPHPDF_POLICY_PATH", envPath.toUtf8());
        QCOMPARE(PolicyController::defaultPolicyPath(), envPath);

        // Without the variable: the machine default location — assert the
        // SHAPE (never an absolute pin; GenericDataLocation is per-machine).
        qunsetenv("GLYPHPDF_POLICY_PATH");
        const QString fallback = PolicyController::defaultPolicyPath();
        QVERIFY(fallback.endsWith(QStringLiteral("GlyphPDF/policy.json")));
        QVERIFY(fallback != envPath);

        // An EMPTY value means "no override" for the production reader —
        // leave the environment in that neutral state for later slots.
        qputenv("GLYPHPDF_POLICY_PATH", "");
        QCOMPARE(PolicyController::defaultPolicyPath(), fallback);
    }

    // ── Pin 2: the consumption seam ──────────────────────────────────────
    void readSigningConfigHonorsPolicyOverUserPrefs()
    {
        QVERIFY(loadPolicy(makePolicyObject()));

        // User's own (conflicting) preferences in a hermetic settings store.
        QTemporaryFile ini;
        QVERIFY(ini.open());
        ini.close();
        QSettings userStore(ini.fileName(), QSettings::IniFormat);
        userStore.setValue(QStringLiteral("signing/tsaUrl"),
                           QStringLiteral("https://user.example/tsa"));
        userStore.setValue(QStringLiteral("signing/padesLevel"),
                           QStringLiteral("B-LTA"));

        const auto cfg = SecurityController::readSigningConfig(&userStore);
        // Policy wins at load time — the dispatch will use the TSA the
        // machine admin chose, not the user's override.
        QCOMPARE(cfg.tsaUrl,
                 QStringLiteral("https://tsa.corp.internal.example/rfc3161"));
        QCOMPARE(cfg.level, PAdESLevel::B_B);

        // Without a policy the same store reads the user values (the R19a
        // contract is unchanged when no machine policy exists).
        PolicyController::instance().resetForTesting();
        const auto userCfg = SecurityController::readSigningConfig(&userStore);
        QCOMPARE(userCfg.tsaUrl, QStringLiteral("https://user.example/tsa"));
        QCOMPARE(userCfg.level, PAdESLevel::B_LTA);
    }

    // ── Pin 3: the override is VISIBLE in the UI ─────────────────────────
    void preferencesDialogDisclosesManagedRows()
    {
        QVERIFY(loadPolicy(makePolicyObject()));

        // QSettings are isolated via initTestCase (GlyphPDFTests org); the
        // managed rows show the POLICY value regardless of any stored value.
        PreferencesDialog dlg;
        // Managed signing rows: policy value shown, widget disabled, badged.
        auto* tsaEdit = dlg.findChild<QLineEdit*>(QStringLiteral("tsaUrlEdit"));
        QVERIFY(tsaEdit);
        QVERIFY(tsaEdit->property("managedByPolicy").toBool());
        QVERIFY(!tsaEdit->isEnabled());
        QCOMPARE(tsaEdit->text(),
                 QStringLiteral("https://tsa.corp.internal.example/rfc3161"));

        auto* levelCombo =
            dlg.findChild<QComboBox*>(QStringLiteral("padesLevelCombo"));
        QVERIFY(levelCombo);
        QVERIFY(levelCombo->property("managedByPolicy").toBool());
        QVERIFY(!levelCombo->isEnabled());
        QCOMPARE(levelCombo->currentData().toString(), QStringLiteral("B-B"));

        // The startup-check toggle is locked to the policy value too.
        auto* autoUpdate =
            dlg.findChild<QCheckBox*>(QStringLiteral("autoUpdateCheck"));
        QVERIFY(autoUpdate);
        QVERIFY(autoUpdate->property("managedByPolicy").toBool());
        QVERIFY(!autoUpdate->isEnabled());
        QVERIFY(!autoUpdate->isChecked());

        // The status line discloses policy presence, path and key count.
        auto* status = dlg.findChild<QLabel*>(QStringLiteral("policyStatusLabel"));
        QVERIFY(status);
        QVERIFY(status->text().contains(QStringLiteral("policy.json")));
        QVERIFY(status->text().contains(QStringLiteral("3")));

        // The managed-keys summary names every managed key.
        auto* keys = dlg.findChild<QLabel*>(QStringLiteral("policyKeysLabel"));
        QVERIFY(keys);
        QVERIFY(keys->text().contains(QStringLiteral("signing/tsaUrl")));
        QVERIFY(keys->text().contains(QStringLiteral("update/checkOnStartup")));
        // Enforcement honesty: enforced vs pending is stated per key.
        QVERIFY(keys->text().contains(QStringLiteral("pending"),
                                       Qt::CaseInsensitive));
        QVERIFY(keys->text().contains(QStringLiteral("Enforced")));
    }

    // ── Pin 4: the save guard ────────────────────────────────────────────
    void persistSettingRefusesManagedKeys()
    {
        QVERIFY(loadPolicy(makePolicyObject()));

        QTemporaryFile ini;
        QVERIFY(ini.open());
        ini.close();
        QSettings store(ini.fileName(), QSettings::IniFormat);

        // Managed key: the user's value is NOT persisted.
        PreferencesDialog::persistSetting(
            store, QStringLiteral("signing/tsaUrl"),
            QStringLiteral("https://user.example/tsa"));
        // Unmanaged key: persists normally.
        PreferencesDialog::persistSetting(store, QStringLiteral("ui/theme"),
                                          QStringLiteral("light"));
        store.sync();

        QSettings readback(ini.fileName(), QSettings::IniFormat);
        QVERIFY(!readback.contains(QStringLiteral("signing/tsaUrl")));
        QCOMPARE(readback.value(QStringLiteral("ui/theme")).toString(),
                 QStringLiteral("light"));
    }

    // ── Pin 5: tampered / invalid / missing policy is honest ─────────────
    void tamperedPolicyIsIgnoredAndDisclosed()
    {
        // Unparsable JSON: Invalid, nothing managed, disclosed in the line.
        const QString path = m_dir.filePath(QStringLiteral("policy.json"));
        QVERIFY(writePolicyFile(path, QByteArray("{ not json !!!")));
        QVERIFY(!PolicyController::instance().load(path));
        QCOMPARE(PolicyController::instance().state(),
                 PolicyController::State::Invalid);
        QVERIFY(PolicyController::instance().managedKeys().isEmpty());
        QVERIFY(PolicyController::instance().statusLine().contains(
            QStringLiteral("ignored"), Qt::CaseInsensitive));
        // User prefs still work after a tampered policy.
        QCOMPARE(PolicyController::instance().effectiveValue(
                     QStringLiteral("signing/tsaUrl"),
                     QStringLiteral("https://user.example/tsa"))
                     .toString(),
                 QStringLiteral("https://user.example/tsa"));
    }

    void wrongSchemaVersionIsInvalid()
    {
        QJsonObject root = makePolicyObject();
        root.insert(QStringLiteral("schemaVersion"), 99);
        QVERIFY(!loadPolicy(root));
        QCOMPARE(PolicyController::instance().state(),
                 PolicyController::State::Invalid);
        QVERIFY(PolicyController::instance().managedKeys().isEmpty());
    }

    // ── Pin 6: the bounded allowlist ─────────────────────────────────────
    void unknownPolicyKeysAreIgnoredAndDisclosed()
    {
        QJsonObject settings = makePolicyObject()["settings"].toObject();
        settings.insert(QStringLiteral("ui/language"), QStringLiteral("de"));
        settings.insert(QStringLiteral("filesystem/allowAnything"), true);
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        root.insert(QStringLiteral("settings"), settings);
        QVERIFY(loadPolicy(root));

        const auto& pc = PolicyController::instance();
        // Known keys still managed...
        QVERIFY(pc.isManaged(QStringLiteral("signing/tsaUrl")));
        // ...unknown keys are NOT silently accepted...
        QVERIFY(!pc.isManaged(QStringLiteral("ui/language")));
        QVERIFY(!pc.isManaged(QStringLiteral("filesystem/allowAnything")));
        QCOMPARE(pc.effectiveValue(QStringLiteral("ui/language"),
                                   QStringLiteral("en"))
                     .toString(),
                 QStringLiteral("en"));
        // ...and are DISCLOSED, not swallowed.
        QCOMPARE(pc.unrecognizedKeys().size(), 2);
        QVERIFY(pc.unrecognizedKeys().contains(
            QStringLiteral("filesystem/allowAnything")));
        QVERIFY(pc.statusLine().contains(
            QStringLiteral("filesystem/allowAnything")));
    }

    void emptySettingsObjectLoadsClean()
    {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        root.insert(QStringLiteral("settings"), QJsonObject{});
        QVERIFY(loadPolicy(root));
        QCOMPARE(PolicyController::instance().state(),
                 PolicyController::State::Loaded);
        QVERIFY(PolicyController::instance().managedKeys().isEmpty());
        // Zero managed keys is honest too — the line must not claim a count
        // of managed keys that does not exist.
        QVERIFY(!PolicyController::instance().statusLine().contains(
            QStringLiteral("3 key")));
    }

    void allowlistIsBoundedAndSignedKeysEnforced()
    {
        // 4-8 keys max: the audited allowlist stays inside that bound.
        const QStringList known = PolicyController::knownKeys();
        QVERIFY(known.size() >= 4);
        QVERIFY(known.size() <= 8);
        // The keys enforced app-wide THIS build are the signing pair.
        QVERIFY(PolicyController::isEnforcedKey(QStringLiteral("signing/tsaUrl")));
        QVERIFY(PolicyController::isEnforcedKey(QStringLiteral("signing/padesLevel")));
        // A recognized-but-not-yet-enforced key must say so (visible honesty).
        QVERIFY(!PolicyController::enforcementNote(QStringLiteral("update/checkOnStartup"))
                     .isEmpty());
        QVERIFY(PolicyController::enforcementNote(QStringLiteral("update/checkOnStartup"))
                    .contains(QStringLiteral("pending"), Qt::CaseInsensitive));
    }
};

#include "TestPolicyController.moc"

QTEST_MAIN(TestPolicyController)
