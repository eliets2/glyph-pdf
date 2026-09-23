// SPDX-License-Identifier: Apache-2.0
// SEP13 lead 3 — CapabilityRegistry::applyToWidget: the Degraded branch never
// reverses a registry-owned disable.
//
// PARITY-GLM-REVIEW-2026-09-13 lead (Capability.cpp ~191): a widget disabled by
// an Unavailable* apply (owned via the capOwnedDisable property) stays disabled
// when the capability later transitions to Degraded — the Degraded branch only
// sets a tooltip ("Disclose but keep the control usable") and never honors the
// registry's own disable claim. The Available branch DOES reverse it (D06), so
// the stuck window is exactly the Degraded phase.
//
// These probes assert the CORRECT contract; the Degraded-transition probe is
// expected to FAIL on the feat/sep13-leads candidate (83be3c2) — that failure
// is the confirmation evidence.
#include <QtTest/QtTest>
#include <QWidget>

#include "core/Capability.h"

using namespace gp;

namespace {

enum class ProbeState { Available, Degraded, UnavailableRuntime };

ProbeState s_state = ProbeState::Available;

Capability capabilityFor(ProbeState state)
{
    Capability c;
    switch (state) {
    case ProbeState::Available:
        c.status = Availability::Available;
        c.detail = QStringLiteral("probe available");
        break;
    case ProbeState::Degraded:
        c.status = Availability::Degraded;
        c.whyNot = QStringLiteral("probe degraded why");
        c.alternative = QStringLiteral("probe degraded alternative");
        c.detail = QStringLiteral("probe degraded detail");
        break;
    case ProbeState::UnavailableRuntime:
        c.status = Availability::UnavailableRuntime;
        c.whyNot = QStringLiteral("probe unavailable why");
        c.alternative = QStringLiteral("probe unavailable alternative");
        break;
    }
    return c;
}

} // namespace

class TestSep13LeadCapability : public QObject {
    Q_OBJECT

private slots:
    void init() { s_state = ProbeState::Available; }

    // LEAD 3 CONFIRMATION (expected FAILURE on the candidate): Unavailable ->
    // invalidate/re-register -> Degraded leaves the widget stuck disabled even
    // though Degraded means "keep the control usable".
    void degradedTransitionMustReverseRegistryOwnedDisable() {
        CapabilityRegistry registry;
        registry.registerProbe(CapId::MrcCompression, [](const QVariant&) {
            return capabilityFor(s_state);
        });

        QWidget w;
        s_state = ProbeState::UnavailableRuntime;
        registry.applyToWidget(&w, CapId::MrcCompression);
        QVERIFY2(!w.isEnabled(), "precondition: UnavailableRuntime must disable the widget");
        QVERIFY2(w.property("capOwnedDisable").toBool(),
                 "precondition: the registry must own the disable it created");

        s_state = ProbeState::Degraded;
        // Production invalidation flow (Preferences change / models installed):
        // the registry MUST re-probe before the next apply, or applyToWidget
        // serves the cached UnavailableRuntime capability and no branch runs.
        registry.invalidate(CapId::MrcCompression);
        registry.applyToWidget(&w, CapId::MrcCompression);

        // CORRECT contract: Degraded discloses but KEEPS THE CONTROL USABLE.
        // Candidate behavior: the Degraded branch never checks capOwnedDisable,
        // so the widget stays disabled with the stale unavailable tooltip.
        QVERIFY2(w.isEnabled(),
                 "SEP13 lead 3 CONFIRMED: Degraded apply left the registry-disabled "
                 "widget stuck disabled (Degraded means 'keep the control usable')");
        QVERIFY2(w.toolTip().isEmpty() || !w.toolTip().isEmpty(),
                 "tooltip state is not the assertion — presence logged above");
        qInfo() << "post-Degraded tooltip:" << w.toolTip();
    }

    // Control pin: the Available branch reverses the owned disable (D06).
    // Must PASS — proves the harness and the ownership property semantics.
    void availableTransitionReversesOwnedDisable() {
        CapabilityRegistry registry;
        registry.registerProbe(CapId::MrcCompression, [](const QVariant&) {
            return capabilityFor(s_state);
        });

        QWidget w;
        s_state = ProbeState::UnavailableRuntime;
        registry.applyToWidget(&w, CapId::MrcCompression);
        QVERIFY(!w.isEnabled());

        s_state = ProbeState::Available;
        registry.invalidate(CapId::MrcCompression);   // production re-probe flow
        registry.applyToWidget(&w, CapId::MrcCompression);
        QVERIFY2(w.isEnabled(), "Available must reverse the registry-owned disable (D06)");
        QVERIFY(w.toolTip().isEmpty());
        QVERIFY(!w.property("capOwnedDisable").toBool());
    }

    // Control pin: Degraded applied to an ENABLED widget keeps it usable.
    void degradedOnEnabledWidgetKeepsItUsable() {
        CapabilityRegistry registry;
        registry.registerProbe(CapId::MrcCompression, [](const QVariant&) {
            return capabilityFor(s_state);
        });

        QWidget w;
        s_state = ProbeState::Degraded;
        registry.applyToWidget(&w, CapId::MrcCompression);
        QVERIFY2(w.isEnabled(), "Degraded on a fresh enabled widget must not disable it");
        QVERIFY(!w.toolTip().isEmpty());
    }
};

QTEST_MAIN(TestSep13LeadCapability)
#include "TestSep13LeadCapability.moc"
