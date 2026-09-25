// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVariant>

namespace gp {

// ── R24(a): machine-level policy ─────────────────────────────────────────────
// A machine administrator can deploy a policy file that OVERRIDES selected
// user preferences. The override is never silent: every managed setting is
// shown in Preferences with the policy value, a "Managed by policy" badge and
// a disabled widget, plus a status line naming the policy file — the
// visible-in-UI requirement IS the feature.
//
// SWEEP-W1 F1 (trust-model honesty) + W1-05 structural close: on Windows,
// load() now VERIFIES the file's owner is an administrator-tier account
// (Administrators/SYSTEM) before a single key is enforced — a file written
// by anyone else (the squatter who pre-created the %PROGRAMDATA% location
// on a default install) lands in State::UntrustedOwner: ignored, with the
// refusal disclosed wherever overrides render (statusLine() appends
// trustModelNote()) and in the support bundle's policy section. Non-Windows
// platforms keep the machine-trusted behavior (no ownership check), and the
// platform difference is disclosed in trustModelNote(). A disclosed
// GLYPHPDF_POLICY_ASSUME_TRUSTED env seam (test-only wiring fixtures) skips
// the gate — it is no wider than the GLYPHPDF_POLICY_PATH seam.
//
// File location (production): %PROGRAMDATA%\GlyphPDF\policy.json — resolved as
// QStandardPaths::GenericDataLocation + "/GlyphPDF/policy.json". The
// GLYPHPDF_POLICY_PATH environment variable overrides the location (test seam;
// also lets a portable install point elsewhere honestly — the resolved path is
// always disclosed in the status line and in the support bundle).
//
// Schema (schemaVersion 1):
//   {
//     "schemaVersion": 1,
//     "settings": {
//       "signing/tsaUrl": "https://tsa.corp.example/rfc3161",
//       "signing/padesLevel": "B-B",
//       "update/checkOnStartup": false,
//       ...only keys from knownKeys() are accepted...
//     }
//   }
//
// Honesty contract:
//   * Missing file → State::NoPolicy (normal case, disclosed as such).
//   * Unparsable JSON / wrong schemaVersion / "settings" not an object →
//     State::Invalid: the policy is IGNORED, user prefs stay in force, and
//     statusLine() discloses the file and the reason (a status line, not a
//     crash).
//   * Keys outside knownKeys() are IGNORED and DISCLOSED via
//     unrecognizedKeys()/statusLine() — the policy surface stays bounded and
//     auditable (4-8 keys).
//   * Values of a rejected JSON type (object/array/null) are ignored and
//     disclosed like unknown keys.
//
// Enforcement scope (declared per key — never claimed where not wired):
//   ALL allowlist keys are ENFORCED app-wide this build (R24 wiring closure),
//   each pinned at its observable enforcement point by TestPolicyWiring:
//   * signing/tsaUrl + signing/padesLevel → SecurityController::readSigningConfig
//     (the ONE production reader of the signing settings) applies the policy
//     over the user values before every sign/certify/timestamp dispatch;
//   * update/checkOnStartup + update/channel → MainWindow::
//     startupUpdateCheckEnabled()/startupUpdateChannel(), consulted by
//     MainWindow::initUpdateChecker (the startup check honors the policy);
//   * ai/ollamaEndpoint → OllamaProvider::resolveEndpoint (the ONE endpoint
//     gate feeding isReady()/chat()); a policy-managed EMPTY endpoint
//     disables AI chat with an honest whyNot naming the policy;
//   * ocr/allowNetworkDownload → the OcrEngine model-load gate (the shared
//     download gate all OCR callers pass through).
class PolicyController : public QObject {
    Q_OBJECT
public:
    enum class State {
        NoPolicy,
        Loaded,
        Invalid,
        // W1-05 structural close: the file parsed, but its Windows owner is
        // NOT an administrator-tier account (Administrators/SYSTEM) — the
        // planted-squatter posture. The policy is IGNORED (zero managed
        // keys) and the refusal is disclosed via statusLine()/support
        // bundle. Non-Windows platforms keep the machine-trusted behavior
        // (no ownership check) — disclosed in trustModelNote().
        UntrustedOwner
    };

    // Process-wide instance (QSettings-affine reads stay on the GUI thread —
    // same threading rule as CapabilityRegistry).
    static PolicyController& instance();

    // GLYPHPDF_POLICY_PATH wins; else %PROGRAMDATA%/GlyphPDF/policy.json.
    static QString defaultPolicyPath();

    // Loads and validates the policy file. Last load wins (explicit reloads
    // are allowed; ensureLoaded() loads the default path once). Returns true
    // only for State::Loaded — a missing file is not an error (false +
    // State::NoPolicy), an invalid file is not silently swallowed (false +
    // State::Invalid + disclosed statusLine).
    bool load(const QString& path);
    void ensureLoaded();   // one default-path load per process (after a reset)

    State        state() const;
    QString      statusLine() const;   // user-facing honest one-liner
    QString      policyPath() const;   // the path last consulted

    // The bounded allowlist (schema v1): machine-admin stories only.
    static QStringList knownKeys();
    // True for keys enforced app-wide in THIS build (see class comment —
    // this is the whole allowlist since the R24 wiring closure).
    // W1-05/F1 trust-model honesty: the policy file is machine-TRUSTED — no
    // origin, ownership or integrity verification exists, so whoever can
    // write its location can set these overrides. One canonical sentence
    // shared by every policy surface (statusLine, Preferences, support
    // bundle).
    static QString trustModelNote();
    // True for keys enforced app-wide in THIS build (see class comment).
    static bool isEnforcedKey(const QString& settingsKey);
    // Per-key enforcement wording: names the exact wiring point (never empty
    // for known keys; contains "Enforced app-wide" for every allowlist key).
    static QString enforcementNote(const QString& settingsKey);

    bool        isManaged(const QString& settingsKey) const;
    QVariant    policyValue(const QString& settingsKey) const;
    // Precedence in ONE call: the policy value when managed, else userValue.
    QVariant    effectiveValue(const QString& settingsKey,
                               const QVariant& userValue) const;
    QStringList managedKeys() const;      // accepted keys from the loaded file
    QStringList unrecognizedKeys() const; // disclosed, ignored entries

    // Test seam: back to State::NoPolicy with no managed keys so tests are
    // order-independent.
    void resetForTesting();

private:
    explicit PolicyController(QObject* parent = nullptr);

    State                    m_state = State::NoPolicy;
    QString                  m_path;
    QHash<QString, QVariant> m_managed;
    QStringList              m_unrecognized;
    bool                     m_loadedOnce = false;
};

} // namespace gp
