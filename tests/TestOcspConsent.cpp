// SPDX-License-Identifier: Apache-2.0
// R24 wiring closure — the OCSP consent switch, closing the gap the network
// disclosure lane found (OCSP egress fired with no consent surface at all).
//
// The consent design is the send-for-signing plan's D1a: per-document
// consent dialog (allow once / allow for this document / deny) +
// remember-for-document (session-scoped) + a global never-network switch
// (signing/ocspNetworkPolicy = "never") that refuses WITHOUT asking. The
// controller gate (SecurityController::runSigning, level >= B_LT) refuses
// BEFORE dispatch with an honest whyNot naming the consent state — the
// engine's OCSP code is untouched.
//
// Pins:
//   1. Default ("ask") shows the dialog; each button drives its branch
//      (allow-once → AllowedOnce, allow-for-document → AllowedDocument,
//      deny → Denied) — offscreen dialog-driving via activeModalWidget.
//   2. Allow-for-document is remembered for THAT document only (session
//      store): the same path skips the dialog, a different path asks again.
//   3. Deny is never remembered — the next attempt asks again.
//   4. The global never-network switch refuses without any dialog (and an
//      UNKNOWN value refuses too — fail-closed, like the B-B floor).
//   5. refusalReason names the consent switch (key + value) and the way out.
//   6. The Preferences Security tab carries the switch (honest values,
//      persisted through the policy write guard).
#include <QtTest/QtTest>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include "ui/OcspConsentDialog.h"
#include "ui/PreferencesDialog.h"

using gp::OcspConsent;
using gp::OcspConsentDecision;
using gp::OcspConsentDialog;
using gp::PreferencesDialog;

namespace {

// Drives the consent dialog the moment it becomes the active modal widget:
// clicks the button with `objectName`, exactly once.
void scheduleDialogClick(const QString& objectName)
{
    QTimer::singleShot(0, [objectName]() {
        if (auto* dlg = qobject_cast<OcspConsentDialog*>(
                QApplication::activeModalWidget())) {
            if (auto* btn = dlg->findChild<QPushButton*>(objectName)) {
                btn->click();
                return;
            }
        }
        QFAIL("OCSP consent dialog did not appear (or button missing)");
    });
}

} // namespace

class TestOcspConsent : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestOcspConsent"));
    }

    void init()
    {
        QSettings store;
        store.clear();
        store.sync();
        OcspConsent::resetForTesting();
    }

    void cleanup()
    {
        QSettings store;
        store.clear();
        store.sync();
        OcspConsent::resetForTesting();
    }

    // ── Pin 1: each dialog branch drives its decision ────────────────────
    void dialogBranchesDecide()
    {
        scheduleDialogClick(QStringLiteral("ocspAllowOnceBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedOnce);

        OcspConsent::resetForTesting();
        scheduleDialogClick(QStringLiteral("ocspAllowDocumentBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);

        OcspConsent::resetForTesting();
        scheduleDialogClick(QStringLiteral("ocspDenyBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::Denied);
    }

    void dialogDisclosureNamesWhatFiresAndWhatNeverLeaves()
    {
        OcspConsentDialog dlg;
        auto* disclosure = dlg.disclosureLabel();
        QVERIFY(disclosure);
        // What fires: the certificate's AIA responder, during B-LT/B-LTA.
        QVERIFY(disclosure->text().contains(QStringLiteral("OCSP"),
                                            Qt::CaseInsensitive));
        QVERIFY(disclosure->text().contains(QStringLiteral("AIA")));
        // What is never sent: document content. What is enforced: HTTPS.
        QVERIFY(disclosure->text().contains(QStringLiteral("HTTPS")));
        QVERIFY(disclosure->text().contains(QStringLiteral("never"),
                                            Qt::CaseInsensitive));
        // The three honest choices exist as named buttons.
        QVERIFY(dlg.allowOnceButton());
        QVERIFY(dlg.allowDocumentButton());
        QVERIFY(dlg.denyButton());
    }

    // ── Pin 2: remember-for-document (session-scoped, per path) ──────────
    void allowForDocumentRemembersThatDocumentOnly()
    {
        scheduleDialogClick(QStringLiteral("ocspAllowDocumentBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);

        // Same document: remembered — no dialog (obtain returns directly).
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);

        // A DIFFERENT document asks again (schedule the click for it).
        scheduleDialogClick(QStringLiteral("ocspAllowOnceBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/b.pdf")),
                 OcspConsentDecision::AllowedOnce);

        // …and a.pdf stays remembered.
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);
    }

    // ── Pin 3: deny is never remembered ──────────────────────────────────
    void denyIsNotRemembered()
    {
        scheduleDialogClick(QStringLiteral("ocspDenyBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::Denied);
        // The next attempt for the SAME document asks again (allow-once this
        // time) — a misclicked Deny cannot lock the user out of the session.
        scheduleDialogClick(QStringLiteral("ocspAllowOnceBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedOnce);
    }

    // ── Pin 4: the global never-network switch refuses without a dialog ──
    void globalNeverNetworkRefusesWithoutDialog()
    {
        QSettings store;
        store.setValue(QLatin1String(gp::OcspNetworkPolicyKey),
                       QStringLiteral("never"));
        store.sync();

        // No scheduled click: if a dialog appeared here the test would hang
        // and time out — the switch must refuse up front, silently.
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::Denied);

        // Fail-closed: an unknown value refuses too (the honest floor).
        store.setValue(QLatin1String(gp::OcspNetworkPolicyKey),
                       QStringLiteral("occasionally"));
        store.sync();
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::Denied);

        // "ask" is the only value that leads to the dialog path — and with a
        // remembered decision it does not even need to ask.
        store.setValue(QLatin1String(gp::OcspNetworkPolicyKey),
                       QStringLiteral("ask"));
        store.sync();
        scheduleDialogClick(QStringLiteral("ocspAllowDocumentBtn"));
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);
        QCOMPARE(OcspConsent::obtain(nullptr, QStringLiteral("/docs/a.pdf")),
                 OcspConsentDecision::AllowedDocument);
    }

    // ── Pin 5: the refusal names the consent state and the way out ───────
    void refusalReasonNamesTheConsentSwitch()
    {
        // Global switch: the reason names the KEY and its value.
        QSettings store;
        store.setValue(QLatin1String(gp::OcspNetworkPolicyKey),
                       QStringLiteral("never"));
        store.sync();
        const QString globalReason =
            OcspConsent::refusalReason(OcspConsentDecision::Denied);
        QVERIFY(globalReason.contains(QLatin1String(gp::OcspNetworkPolicyKey)));
        QVERIFY(globalReason.contains(QStringLiteral("never")));
        // The way out is named: B-T/B-B need no OCSP egress; the honest
        // outcome statement: nothing was attempted.
        QVERIFY(globalReason.contains(QStringLiteral("B-T")));
        QVERIFY(globalReason.contains(QStringLiteral("B-B")));
        QVERIFY(globalReason.contains(QStringLiteral("No signature was attempted")));

        // Per-document denial: the reason honestly says consent was declined.
        store.setValue(QLatin1String(gp::OcspNetworkPolicyKey),
                       QStringLiteral("ask"));
        store.sync();
        const QString declined =
            OcspConsent::refusalReason(OcspConsentDecision::Denied);
        QVERIFY(declined.contains(QStringLiteral("consent"),
                                  Qt::CaseInsensitive));
        QVERIFY(declined.contains(QStringLiteral("B-T")));

        // An allowed decision produces no refusal text at all.
        QVERIFY(OcspConsent::refusalReason(
                    OcspConsentDecision::AllowedDocument).isEmpty());
        QVERIFY(OcspConsent::egressAllowed(
                    OcspConsentDecision::AllowedDocument));
        QVERIFY(OcspConsent::egressAllowed(
                    OcspConsentDecision::AllowedOnce));
        QVERIFY(!OcspConsent::egressAllowed(OcspConsentDecision::Denied));
    }

    // ── Pin 6: the Preferences Security tab carries the switch ───────────
    void preferencesRowCarriesTheSwitchAndPersists()
    {
        PreferencesDialog dlg;
        auto* combo = dlg.findChild<QComboBox*>(
            QStringLiteral("ocspNetworkPolicyCombo"));
        QVERIFY2(combo, "missing ocspNetworkPolicyCombo on the Security tab");
        QVERIFY(!combo->property("managedByPolicy").toBool());

        // Exactly the two honest values, with "ask" (the consent default).
        QCOMPARE(combo->count(), 2);
        QStringList values;
        for (int i = 0; i < combo->count(); ++i)
            values << combo->itemData(i).toString();
        QVERIFY(values.contains(QStringLiteral("ask")));
        QVERIFY(values.contains(QStringLiteral("never")));
        QCOMPARE(combo->currentData().toString(), QStringLiteral("ask"));

        // Choosing "never" and Saving persists the switch through the
        // policy write guard (the key is NOT policy-managed).
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == QLatin1String("never"))
                combo->setCurrentIndex(i);
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString ini = dir.filePath(QStringLiteral("prefs.ini"));
        {
            QSettings store(ini, QSettings::IniFormat);
            store.clear();
            dlg.persistSetting(store, QLatin1String(gp::OcspNetworkPolicyKey),
                               combo->currentData().toString());
            store.sync();
        }
        QSettings readback(ini, QSettings::IniFormat);
        QCOMPARE(readback.value(QLatin1String(gp::OcspNetworkPolicyKey))
                     .toString(),
                 QStringLiteral("never"));
    }
};

#include "TestOcspConsent.moc"

QTEST_MAIN(TestOcspConsent)
