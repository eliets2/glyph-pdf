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
//   * signing/tsaUrl + signing/padesLevel are ENFORCED app-wide this build:
//     SecurityController::readSigningConfig (the ONE production reader of the
//     signing settings) applies the policy over the stored user values before
//     every sign/certify/timestamp dispatch.
//   * The remaining allowlist keys are RECOGNIZED: Preferences locks the
//     widget to the policy value and refuses to persist user edits, but their
//     app-wide consumption seams (startup update check, AI chat panel, OCR
//     download gate) are wired in a follow-up — enforcementNote() says
//     "pending" for them, and the UI shows that wording. Never claim more
//     enforcement than exists.
class PolicyController : public QObject {
    Q_OBJECT
public:
    enum class State { NoPolicy, Loaded, Invalid };

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
    // W1-05/F1 trust-model honesty: the policy file is machine-TRUSTED — no
    // origin, ownership or integrity verification exists, so whoever can
    // write its location can set these overrides. One canonical sentence
    // shared by every policy surface (statusLine, Preferences, support
    // bundle).
    static QString trustModelNote();
    // True for keys enforced app-wide in THIS build (see class comment).
    static bool isEnforcedKey(const QString& settingsKey);
    // Per-key enforcement wording: names the wiring point; contains "pending"
    // for recognized-but-not-yet-enforced keys (never empty for known keys).
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
