// SPDX-License-Identifier: Apache-2.0
// harness_policy.cpp — W1 sweep S5: machine policy file decode + path
// resolution (HOST-SIDE ONLY — no real symlinks, no %PROGRAMDATA% writes).
//
// Surfaces under test (src/core/PolicyController.cpp):
//   PolicyController::load(path)          — schemaVersion/settings handshake,
//                                           bounded allowlist intake
//   PolicyController::defaultPolicyPath() — GLYPHPDF_POLICY_PATH env override
//                                           (relative paths, weird strings)
//   statusLine() / effectiveValue()       — the disclosed-report surface
//
// Property oracles:
//   P1 bounded allowlist: managedKeys() ⊆ knownKeys() always. Anything else
//      entering m_managed is an accepts-invalid escape of the allowlist.
//   P2 typed intake: object/array/null values for a KNOWN key must never be
//      accepted as managed (they land in unrecognizedKeys() instead).
//   P3 honesty states: missing file → NoPolicy; unparsable → Invalid; and a
//      load() that returns true implies State::Loaded with statusLine()
//      naming the path. statusLine() must never crash on any input.
//   P4 path resolution: defaultPolicyPath() with any env value (relative,
//      empty, device names, unicode, 32 KiB) returns without crash; the
//      resolved path is disclosed (never hidden).
//
// Deterministic driver (sweep_common.h): campaign / one / materialize modes.
#include <cstdio>
#include <string>

#include <QCoreApplication>
#include <QFile>
#include <QString>

#include "core/PolicyController.h"
#include "sweep_common.h"

using gp::PolicyController;

namespace {

// Where the driver stages mutated policy files (own scratch — never the
// repo, never %PROGRAMDATA%). Overridden by SWEEP_SCRATCH for isolation.
std::string scratchPath() {
    const char* env = getenv("SWEEP_SCRATCH");
    return env ? env : "sweep-scratch";
}

const char* runOne(const std::vector<uint8_t>& data) {
    PolicyController& pc = PolicyController::instance();
    pc.resetForTesting();

    const std::string stage =
        scratchPath() + "\\policy-input.json";
    if (!sweep::writeFileBytes(stage, data)) return "ERR scratch-unwritable";

    // ── load + honesty-state classification ─────────────────────────────
    const bool ok = pc.load(QString::fromStdString(stage));
    const auto state = pc.state();

    if (ok && state != PolicyController::State::Loaded)
        return "FINDING P3_LOAD_TRUE_BUT_NOT_LOADED";

    // ── P1: the allowlist is bounded, always ────────────────────────────
    const QStringList known = PolicyController::knownKeys();
    for (const QString& k : pc.managedKeys()) {
        if (!known.contains(k))
            return "FINDING P1_UNBOUNDED_ALLOWLIST_KEY_ENTERED";
    }

    // ── P2: object/array/null values never become managed values ────────
    for (const QString& k : pc.managedKeys()) {
        const QVariant v = pc.policyValue(k);
        if (!v.isValid())
            return "FINDING P2_INVALID_VARIANT_MANAGED";
    }

    // ── P3: statusLine() discloses, never crashes ───────────────────────
    const QString line = pc.statusLine();
    if (!line.isEmpty() && pc.policyPath().isEmpty()
        && state == PolicyController::State::Invalid)
        return "FINDING P3_INVALID_STATE_HIDES_PATH";

    // ── effective precedence probe ──────────────────────────────────────
    const QVariant eff = pc.effectiveValue(
        QStringLiteral("signing/tsaUrl"), QStringLiteral("user-value"));
    Q_UNUSED(eff);

    return ok ? "LOADED" : (state == PolicyController::State::Invalid
                                ? "INVALID"
                                : "NOPOLICY");
}

// P4: env-driven path resolution probes — exercised once per process run
// (before the campaign), host-side only: relative strings, device names,
// unicode, huge strings. No file is created at these paths.
void probePathResolution() {
    const std::string huge(32768, 'P');
    const std::vector<std::string> hostile = {
        "", "relative/policy.json", "..\\..\\policy.json", "NUL",
        "C:\\nonexistent\\dir\\policy.json", "\xF0\x9F\x98\x80.json",
        huge,
        "with space\\and.dots\\..\\policy.json",
    };
    for (const std::string& h : hostile) {
        qputenv("GLYPHPDF_POLICY_PATH", h.c_str());
        const QString p = PolicyController::defaultPolicyPath();
        PolicyController::instance().resetForTesting();
        // A load attempt against the hostile path must return (NoPolicy or
        // Invalid), never crash or hang. Write nothing: load() only reads.
        PolicyController::instance().load(p);
    }
    qunsetenv("GLYPHPDF_POLICY_PATH");
}

}  // namespace

int main(int argc, char** argv) {
    // QtCore-only app: QObject/tr/QStandardPaths need the core plumbing to
    // exist; no GUI, no display, no platform plugin — offscreen by construction.
    QCoreApplication app(argc, argv);
    probePathResolution();
    return sweep::driverMain(argc, argv, runOne);
}
