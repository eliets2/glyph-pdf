// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>

class QLabel;
class QPushButton;

namespace gp {

// ── R24 wiring closure: OCSP network consent ──────────────────────────────
// The network-disclosure lane (R24(c)) found that OCSP — unlike TSA/Ollama —
// had NO consent switch: the responder URL embedded in the signing
// certificate's AIA extension was contacted automatically whenever signing at
// PAdES B-LT/B-LTA built the DSS. This surface closes that gap following the
// send-for-signing plan's consent design (§3.4/§5.4, decision D1a):
//
//   * per-document consent dialog at the first OCSP-needing dispatch
//     (Allow once / Allow for this document / Deny);
//   * remember-for-document (session-scoped — restarting the app asks again);
//   * a global never-network switch: signing/ocspNetworkPolicy = "never"
//     refuses OCSP egress up front WITHOUT any dialog (the fail-closed floor;
//     unknown values refuse too, like padesLevelFromSetting maps unknown to
//     the B-B floor). Default is "ask".
//
// ENFORCEMENT SHAPE — a UI/controller gate driving existing engine seams:
// the engine's OCSP code is untouched. SecurityController::runSigning (the
// ONE production dispatch that can reach the engine's DSS/OCSP build, i.e.
// level >= B_LT) calls OcspConsent::obtain() BEFORE dispatching; a denied
// decision REFUSES the signing attempt up front with an honest whyNot that
// names the consent state and the way out (choose B-T/B-B, or grant
// consent) — never a silent downgrade (R19(b) discipline). The plan's
// "degrade with the existing SignOutcome machinery" path remains what the
// user sees when an IN-FLIGHT OCSP fetch fails after consent was granted.
inline constexpr char OcspNetworkPolicyKey[] = "signing/ocspNetworkPolicy";

// The per-document decision. NotAsked = no decision obtained yet.
enum class OcspConsentDecision {
    NotAsked,
    AllowedOnce,      // this dispatch only — not remembered
    AllowedDocument,  // remembered for this document (session-scoped)
    Denied,           // this dispatch only — the next attempt asks again
};

// The consent dialog itself: names exactly what will fire, what is sent,
// and what is never sent (document content). Buttons carry objectNames so
// offscreen tests can drive each branch.
class OcspConsentDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcspConsentDialog(QWidget* parent = nullptr);

    QPushButton* allowOnceButton() const { return m_allowOnce; }
    QPushButton* allowDocumentButton() const { return m_allowDocument; }
    QPushButton* denyButton() const { return m_deny; }
    QLabel* disclosureLabel() const { return m_disclosure; }

private:
    QLabel*      m_disclosure   = nullptr;
    QPushButton* m_allowOnce    = nullptr;
    QPushButton* m_allowDocument = nullptr;
    QPushButton* m_deny          = nullptr;
};

// The decision entry point used by the controller, plus the session-scoped
// remember store. Statics + resetForTesting mirror the PolicyController test
// seam so consent pins are order-independent.
class OcspConsent {
public:
    // The controller's ONE call before any OCSP-needing dispatch:
    //   * global switch "never" (or unknown) → Denied, NO dialog;
    //   * a remembered decision for this document → that decision, NO dialog;
    //   * otherwise the per-document dialog runs (exec) and its answer is
    //     returned (Allow-for-this-document is remembered).
    static OcspConsentDecision obtain(QWidget* parent,
                                      const QString& documentPath);

    // True only for AllowedOnce / AllowedDocument — a decision that lets the
    // dispatch contact the responder.
    static bool egressAllowed(OcspConsentDecision decision);

    // The honest refusal text for a denied decision: names the consent
    // switch (key + value) for the global never-network state, or the
    // declined per-document choice, and states the way out (B-T/B-B need no
    // OCSP; granting consent unblocks B-LT/B-LTA). No signature is attempted.
    static QString refusalReason(OcspConsentDecision decision);

    // Test seam: clears the per-document remember store (the global switch
    // lives in QSettings and is isolated by the tests' org/app names).
    static void resetForTesting();

private:
    OcspConsent() = default;
};

} // namespace gp
