// SPDX-License-Identifier: Apache-2.0
// R24(b) — redacted support bundle (Preferences → "Export support bundle…").
//
// Redaction-by-construction pin: a bundle is built from a state SEEDED WITH
// SECRETS (recents paths carrying usernames, fake PDF text in a recents-like
// metadata key, a private TSA URL, a user endpoint URL) and the serialized
// bundle bytes must contain NONE of them — while still carrying the
// legitimate, non-sensitive content (version, counts, toggles, capability
// disclosure, policy state).
//
// Also pins: counts-only recents/documents (never names/paths), the
// settings allowlist (feature toggles in, URLs excluded AND the exclusion
// disclosed), the policy section (managed key NAMES + enforcement wording,
// never policy values), capability disclosure without probe `detail`
// (detail can carry paths), and the path-redaction helper itself.
#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>

#include "core/Capability.h"
#include "core/PolicyController.h"
#include "core/SupportBundle.h"
#include "ui/PreferencesDialog.h"

using gp::PolicyController;
using gp::PreferencesDialog;

namespace {

bool writePolicyFile(const QString& path, const QByteArray& contents)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(contents) == contents.size();
}

// Seeds a hermetic settings store (a plain ini under the test's temp dir —
// NOT a QTemporaryFile: its Windows temp-file semantics left QSettings
// writes invisible in the fail-first harness) with sensitive content.
void seedStateWithSecrets(const QString& iniPath, QStringList& secrets)
{
    QSettings store(iniPath, QSettings::IniFormat);
    store.clear();

    const QString recentA =
        QStringLiteral("C:/Users/alice-secrets/2026/contract-acme.pdf");
    const QString recentB =
        QStringLiteral("/home/bob-hidden/priv/quitting-letter.pdf");
    store.setValue(QStringLiteral("recentFiles"), QStringList{recentA, recentB});
    store.setValue(QStringLiteral("ocr/engine"), QStringLiteral("tesseract"));
    store.setValue(QStringLiteral("update/checkOnStartup"), true);
    store.setValue(QStringLiteral("signing/padesLevel"), QStringLiteral("B-B"));
    // A private TSA URL (machine-specific, excluded from the bundle).
    store.setValue(QStringLiteral("signing/tsaUrl"),
                   QStringLiteral("https://tsa-user-secret.example/token"));
    secrets.clear();
    secrets << QStringLiteral("alice-secrets")
            << QStringLiteral("bob-hidden")
            << QStringLiteral("tsa-user-secret");

    // Fake PDF text in a recents-shaped metadata key (as a richer recents
    // store might hold one day) — content must never leak into a bundle.
    store.setValue(QStringLiteral("recentFilesMeta/title0"),
                   QStringLiteral("ACME-SECRET-CLAUSE BT /F1 24 Tf "
                                  "(confidential termination clause) Tj ET"));
    secrets << QStringLiteral("ACME-SECRET-CLAUSE")
            << QStringLiteral("confidential termination clause");

    store.sync();
    // The fixture must prove the seeds reached the store the bundle reads.
    QCOMPARE(QSettings(iniPath, QSettings::IniFormat)
                 .value(QStringLiteral("recentFiles")).toStringList().size(),
             2);
}

QJsonObject policyObjectWithSecretTsa()
{
    QJsonObject settings;
    settings.insert(QStringLiteral("signing/tsaUrl"),
                    QStringLiteral("https://corp-pending-tsa.internal.example/rfc3161"));
    settings.insert(QStringLiteral("update/checkOnStartup"), false);
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("settings"), settings);
    return root;
}

} // namespace

class TestSupportBundle : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestSupportBundle"));
        QCoreApplication::setApplicationVersion(QStringLiteral("9.9.9"));
        QVERIFY(m_dir.isValid());
        // HERMETICITY (SWEEP-QUALITY-NEW P2): buildFromSettings() consults the
        // policy singleton (ensureLoaded). Without this env override the suite
        // would read whatever policy.json exists on the RUNNER machine
        // (GenericDataLocation/GlyphPDF/policy.json), making the bundle's
        // policy section machine-dependent. Point the env test seam at a
        // controlled absent file: every test sees the deterministic NoPolicy
        // state unless it loads an explicit fixture path itself.
        qputenv("GLYPHPDF_POLICY_PATH",
                m_dir.filePath(QStringLiteral("no-policy-here.json")).toUtf8());
    }

    void init()
    {
        PolicyController::instance().resetForTesting();
        // W1-05 structural close: the bundle pins load a policy fixture this
        // standard-user process wrote — run under the disclosed
        // assume-trusted seam (the gate itself is pinned in TestPolicyWiring).
        qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED", "1");
    }

    void cleanup()
    {
        PolicyController::instance().resetForTesting();
    }

    // ── THE redaction-by-construction pin ────────────────────────────────
    void bundleBytesContainNoSeededSecrets()
    {
        const QString iniPath =
            m_dir.filePath(QStringLiteral("seeded-settings.ini"));
        QStringList secrets;
        seedStateWithSecrets(iniPath, secrets);
        QSettings store(iniPath, QSettings::IniFormat);

        gp::SupportBundleInput in;
        in.openDocumentCount = 1;
        QJsonObject bundle = gp::SupportBundle::buildFromSettings(store, in);
        const QByteArray bytes = gp::SupportBundle::serialize(bundle);
        const QString text = QString::fromUtf8(bytes);

        // NONE of the seeded secrets may appear anywhere in the bytes.
        for (const QString& secret : secrets)
            QVERIFY2(!text.contains(secret),
                     qPrintable(QStringLiteral("bundle leaked: %1").arg(secret)));

        // The bundle still carries the legitimate content.
        QVERIFY(text.contains(QStringLiteral("9.9.9")));          // app version
        QCOMPARE(bundle.value(QStringLiteral("type")).toString(),
                 QStringLiteral("glyphpdf-support-bundle"));
        // Recents: COUNT only, never names/paths.
        QCOMPARE(bundle.value(QStringLiteral("recents"))
                     .toObject()
                     .value(QStringLiteral("count"))
                     .toInt(),
                 2);
        // Documents: count only.
        QCOMPARE(bundle.value(QStringLiteral("documents"))
                     .toObject()
                     .value(QStringLiteral("openCount"))
                     .toInt(),
                 1);
        // Feature toggles survive (they are what support needs).
        QCOMPARE(bundle.value(QStringLiteral("settings"))
                     .toObject()
                     .value(QStringLiteral("update/checkOnStartup"))
                     .toBool(),
                 true);
        QCOMPARE(bundle.value(QStringLiteral("settings"))
                     .toObject()
                     .value(QStringLiteral("ocr/engine"))
                     .toString(),
                 QStringLiteral("tesseract"));
    }

    void urlSettingsExcludedAndDisclosureCarried()
    {
        const QString iniPath =
            m_dir.filePath(QStringLiteral("seeded-settings.ini"));
        QStringList secrets;
        seedStateWithSecrets(iniPath, secrets);
        QSettings store(iniPath, QSettings::IniFormat);
        QJsonObject bundle = gp::SupportBundle::buildFromSettings(store);

        const QJsonObject settings =
            bundle.value(QStringLiteral("settings")).toObject();
        // URL-valued settings are NOT in the allowlist section...
        QVERIFY(!settings.contains(QStringLiteral("signing/tsaUrl")));
        QVERIFY(!settings.contains(QStringLiteral("ai/ollamaEndpoint")));
        // ...and their EXCLUSION is disclosed, not silent.
        const QJsonObject excluded =
            settings.value(QStringLiteral("excludedByRedaction")).toObject();
        QVERIFY(excluded.contains(QStringLiteral("signing/tsaUrl")));
        QVERIFY(excluded.contains(QStringLiteral("ai/ollamaEndpoint")));
        QVERIFY(QString::fromUtf8(QJsonDocument(bundle).toJson())
                    .contains(QStringLiteral("excluded"),
                              Qt::CaseInsensitive));
    }

    void policySectionCarriesNamesNotValues()
    {
        const QString iniPath =
            m_dir.filePath(QStringLiteral("seeded-settings.ini"));
        QStringList secrets;
        seedStateWithSecrets(iniPath, secrets);

        const QString policyPath = m_dir.filePath(QStringLiteral("policy.json"));
        QVERIFY(writePolicyFile(policyPath,
                                QJsonDocument(policyObjectWithSecretTsa()).toJson()));
        QVERIFY(PolicyController::instance().load(policyPath));

        QSettings store(iniPath, QSettings::IniFormat);
        QJsonObject bundle = gp::SupportBundle::buildFromSettings(store);
        const QString text =
            QString::fromUtf8(gp::SupportBundle::serialize(bundle));

        // Policy STATE is in (honest disclosure)...
        QCOMPARE(bundle.value(QStringLiteral("policy"))
                     .toObject()
                     .value(QStringLiteral("state"))
                     .toString(),
                 QStringLiteral("loaded"));
        QVERIFY(bundle.value(QStringLiteral("policy"))
                    .toObject()
                    .contains(QStringLiteral("managedKeys")));
        // ...but the policy VALUES are not (an internal TSA hostname is the
        // admin's business, not the support bundle's).
        QVERIFY(!text.contains(QStringLiteral("corp-pending-tsa")));
    }

    void capabilitiesDisclosedWithoutDetail()
    {
        const QString iniPath =
            m_dir.filePath(QStringLiteral("seeded-settings.ini"));
        QStringList secrets;
        seedStateWithSecrets(iniPath, secrets);
        QSettings store(iniPath, QSettings::IniFormat);

        gp::CapabilityRegistry registry;
        registry.registerEngineProbes();
        gp::SupportBundleInput in;
        in.capabilities = &registry;
        QJsonObject bundle = gp::SupportBundle::buildFromSettings(store, in);

        const QJsonArray caps =
            bundle.value(QStringLiteral("capabilities")).toArray();
        QVERIFY(caps.size() >= 3);
        for (const QJsonValue& v : caps) {
            const QJsonObject entry = v.toObject();
            QVERIFY(!entry.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!entry.value(QStringLiteral("status")).toString().isEmpty());
            // The probe `detail` is EXCLUDED: it can carry absolute paths.
            QVERIFY(!entry.contains(QStringLiteral("detail")));
        }
    }

    void redactPathStringStripsUsernames()
    {
        using gp::SupportBundle;
        QCOMPARE(SupportBundle::redactPathString(
                     QStringLiteral("C:/Users/alice-secrets/2026/file.pdf")),
                 QStringLiteral("C:/Users/<redacted>/2026/file.pdf"));
        QCOMPARE(SupportBundle::redactPathString(
                     QStringLiteral("C:\\Users\\bob\\private\\x.pdf")),
                 QStringLiteral("C:\\Users\\<redacted>\\private\\x.pdf"));
        QCOMPARE(SupportBundle::redactPathString(
                     QStringLiteral("/home/carol/docs/y.pdf")),
                 QStringLiteral("/home/<redacted>/docs/y.pdf"));
        // Non-path text passes through untouched.
        QCOMPARE(SupportBundle::redactPathString(QStringLiteral("B-LT")),
                 QStringLiteral("B-LT"));
    }

    void defenseInDepthScrubCatchesInjectedPath()
    {
        // Even if a future section accidentally echoes a path-shaped value,
        // the final scrub pass strips the username segment.
        const QString iniPath =
            m_dir.filePath(QStringLiteral("seeded-settings.ini"));
        QStringList secrets;
        seedStateWithSecrets(iniPath, secrets);
        QSettings store(iniPath, QSettings::IniFormat);
        store.setValue(QStringLiteral("ui/language"),
                       QStringLiteral("C:/Users/zoe-admin/leaked"));
        store.sync();
        QJsonObject bundle = gp::SupportBundle::buildFromSettings(store);
        const QString text =
            QString::fromUtf8(gp::SupportBundle::serialize(bundle));
        QVERIFY(!text.contains(QStringLiteral("zoe-admin")));
        QVERIFY(text.contains(QStringLiteral("<redacted>")));
    }

    // ── The UI entry point exists with its honest disclosure ────────────
    void preferencesDialogCarriesExportActionAndDisclosure()
    {
        PreferencesDialog dlg;
        auto* btn = dlg.findChild<QPushButton*>(
            QStringLiteral("exportSupportBundleBtn"));
        QVERIFY(btn);
        auto* note = dlg.findChild<QLabel*>(
            QStringLiteral("bundleDisclosureLabel"));
        QVERIFY(note);
        // The disclosure names what is NEVER included.
        QVERIFY(note->text().contains(QStringLiteral("Never"),
                                       Qt::CaseInsensitive));
        QVERIFY(note->text().contains(QStringLiteral("PDF"),
                                       Qt::CaseInsensitive));
    }
};

#include "TestSupportBundle.moc"

QTEST_MAIN(TestSupportBundle)
