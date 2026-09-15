// SPDX-License-Identifier: Apache-2.0
// R24(c) — network behavior made explicit. ONE audit surface enumerating
// every network touchpoint at runtime (Ollama local chat, RFC 3161 TSA
// timestamping, OCSP during validation, update check, OCR traineddata
// download), each with enabled/disabled state derived from the EXISTING code
// paths and a pointer to the consent setting that governs it. Read-only:
// enumeration performs no network access, and the Preferences "Network" page
// renders it with pointers to the consent settings (the OCSP gap — no
// consent switch exists — is disclosed, not papered over).
#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>

#include "core/NetworkTouchpoints.h"
#include "core/SupportBundle.h"
#include "ui/PreferencesDialog.h"

using gp::NetworkTouchpoints;
using gp::PreferencesDialog;

namespace {

const QStringList kExpectedIds = {
    QStringLiteral("ollama"),
    QStringLiteral("tsa"),
    QStringLiteral("ocsp"),
    QStringLiteral("update-check"),
    QStringLiteral("ocr-traineddata"),
};

QString seededIniPath(const QTemporaryDir& dir)
{
    return dir.filePath(QStringLiteral("network-settings.ini"));
}

} // namespace

class TestNetworkDisclosure : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestNetworkDisclosure"));
        QVERIFY(m_dir.isValid());
    }

    // ── Pin 1: every enumerated source appears ───────────────────────────
    void allEnumeratedSourcesAppear()
    {
        const QString ini = seededIniPath(m_dir);
        QSettings store(ini, QSettings::IniFormat);
        store.clear();
        store.setValue(QStringLiteral("signing/tsaUrl"),
                       QStringLiteral("https://tsa.example/rfc3161"));
        store.setValue(QStringLiteral("ocr/allowNetworkDownload"), true);
        store.setValue(QStringLiteral("update/checkOnStartup"), true);
        store.sync();

        const auto list = NetworkTouchpoints::enumerate(store);
        QStringList ids;
        for (const auto& tp : list)
            ids << tp.id;
        QCOMPARE(ids.size(), kExpectedIds.size());
        for (const QString& expected : kExpectedIds)
            QVERIFY2(ids.contains(expected),
                     qPrintable(QStringLiteral("missing touchpoint: %1")
                                    .arg(expected)));

        // Every entry carries its honest metadata.
        for (const auto& tp : list) {
            QVERIFY(!tp.label.isEmpty());
            QVERIFY(!tp.invocation.isEmpty());
            QVERIFY(!tp.disclosure.isEmpty());
        }
    }

    // ── Pin 2: enabled states derive from the EXISTING settings ──────────
    void enabledStatesTrackSettings()
    {
        const QString ini = seededIniPath(m_dir);
        QSettings store(ini, QSettings::IniFormat);
        store.clear();
        store.sync();

        // Defaults: no TSA URL, no OCR download, no startup update check.
        auto states = [](QSettings& s) {
            QHash<QString, bool> out;
            for (const auto& tp : NetworkTouchpoints::enumerate(s))
                out.insert(tp.id, tp.enabled);
            return out;
        };

        const auto defaults = states(store);
        QCOMPARE(defaults.value(QStringLiteral("tsa")), false);
        QCOMPARE(defaults.value(QStringLiteral("ocr-traineddata")), false);
        // The startup leg of the update check follows its consent toggle.
        QCOMPARE(defaults.value(QStringLiteral("update-check")), false);
        // Feature-present, on-demand: disclosed as available when invoked.
        QCOMPARE(defaults.value(QStringLiteral("ollama")), true);
        QCOMPARE(defaults.value(QStringLiteral("ocsp")), true);

        store.setValue(QStringLiteral("signing/tsaUrl"),
                       QStringLiteral("https://tsa.example/rfc3161"));
        store.setValue(QStringLiteral("ocr/allowNetworkDownload"), true);
        store.setValue(QStringLiteral("update/checkOnStartup"), true);
        store.sync();
        const auto enabled = states(store);
        QCOMPARE(enabled.value(QStringLiteral("tsa")), true);
        QCOMPARE(enabled.value(QStringLiteral("ocr-traineddata")), true);
        QCOMPARE(enabled.value(QStringLiteral("update-check")), true);
    }

    // ── Pin 3: the OCSP consent gap is disclosed, not hidden ─────────────
    void ocspDisclosesMissingConsentSwitch()
    {
        QSettings store(seededIniPath(m_dir), QSettings::IniFormat);
        for (const auto& tp : NetworkTouchpoints::enumerate(store)) {
            if (tp.id != QLatin1String("ocsp"))
                continue;
            // No consent setting exists for OCSP — say so instead of
            // pretending a switch governs it.
            QVERIFY(tp.consentKey.isEmpty());
            QVERIFY(tp.disclosure.contains(QStringLiteral("automatic"),
                                           Qt::CaseInsensitive));
            QVERIFY(tp.disclosure.contains(QStringLiteral("consent"),
                                           Qt::CaseInsensitive)
                    || tp.disclosure.contains(QStringLiteral("no Preferences"),
                                              Qt::CaseInsensitive));
            return;
        }
        QFAIL("ocsp touchpoint missing");
    }

    // ── Pin 4: the support bundle carries the same enumeration ──────────
    void supportBundleNetworkSectionMatchesEnumeration()
    {
        const QString ini = seededIniPath(m_dir);
        QSettings store(ini, QSettings::IniFormat);
        store.clear();
        store.setValue(QStringLiteral("signing/tsaUrl"),
                       QStringLiteral("https://tsa.example/rfc3161"));
        store.sync();

        const QJsonObject bundle = gp::SupportBundle::buildFromSettings(store);
        const QJsonArray touchpoints =
            bundle.value(QStringLiteral("network"))
                .toObject()
                .value(QStringLiteral("touchpoints"))
                .toArray();
        QCOMPARE(touchpoints.size(), kExpectedIds.size());
        for (const QString& id : kExpectedIds) {
            bool found = false;
            for (const QJsonValue& v : touchpoints) {
                if (v.toObject().value(QStringLiteral("id")).toString() == id) {
                    found = true;
                    // The bundle shape is on/off states + invocation — no
                    // destinations, no history.
                    QVERIFY(v.toObject().contains(QStringLiteral("enabled")));
                    QVERIFY(!v.toObject().contains(QStringLiteral("url")));
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("bundle missing: %1").arg(id)));
        }
    }

    // ── Pin 5: the Preferences "Network" page renders every touchpoint ───
    void preferencesNetworkPageDisclosesAllTouchpoints()
    {
        PreferencesDialog dlg;
        for (const QString& id : kExpectedIds) {
            auto* row = dlg.findChild<QLabel*>(
                QStringLiteral("networkRow_") + id);
            QVERIFY2(row, qPrintable(QStringLiteral("missing network row: %1")
                                         .arg(id)));
            QVERIFY(row->text().contains(QStringLiteral("Enabled"))
                    || row->text().contains(QStringLiteral("Disabled")));
        }
        // The page states that displaying it performs no network requests.
        auto* intro = dlg.findChild<QLabel*>(QStringLiteral("networkPageIntro"));
        QVERIFY(intro);
        QVERIFY(intro->text().contains(QStringLiteral("no network"),
                                        Qt::CaseInsensitive));
    }
};

#include "TestNetworkDisclosure.moc"

QTEST_MAIN(TestNetworkDisclosure)
