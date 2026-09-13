// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

namespace gp::formjs {

// Honest error classification for one evaluation. Every failure mode a PDF
// script can produce is named; the runner turns these into user-facing,
// field-attributed messages (never silent, never a wrong value).
enum class JsErrorKind {
    None,       // evaluation succeeded
    Syntax,     // the script text does not parse
    Exception,  // the script threw at run time
    Timeout,    // the interrupt-handler deadline fired (design doc §3.1 CPU)
    Memory,     // the 16 MiB allocation cap (or 1 MiB stack) was refused
};

struct JsEvalResult {
    bool ok = false;
    JsErrorKind kind = JsErrorKind::None;
    QString message;        // honest reason when !ok; empty when ok
    bool rc = true;         // event.rc after the run (scripts may reject)
    bool hasValue = false;  // event.value was set during the run
    QString value;          // event.value as canonical text (number or string)
    QStringList logs;       // console.println / app.alert sink for this run
    QStringList blocked;    // blocked-verb entries (doc.submitForm, app.launchURL, ...)
};

struct SandboxLimits {
    // Design doc §3.1 defaults — never persisted into the document.
    qsizetype memoryLimitBytes = 16 * 1024 * 1024; // JS_SetMemoryLimit
    qsizetype stackLimitBytes  = 1024 * 1024;      // JS_SetMaxStackSize
    int eventDeadlineMs = 250;                     // per single event
    int cascadeDeadlineMs = 1000;                  // per calculate cascade
    // C++-side host allocation caps (R05/JS-01). The JS heap cap does NOT
    // bound host memory: every string crossing the C↔C++ boundary is
    // re-materialized on the host (JS_ToCStringLen → QString::fromUtf8 →
    // toUtf8 → QJsonDocument each copy it), so a getter returning a
    // near-cap string is amplified several-fold. These caps refuse the
    // transfer honestly instead. 4 MiB is orders of magnitude above any
    // honest form-event payload (a value, a few log lines).
    qsizetype maxScriptBytes   = 4 * 1024 * 1024; // authored script text
    qsizetype maxTransferBytes = 4 * 1024 * 1024; // any JS→C++ string
};

// One sandbox = one quickjs-ng JSRuntime + JSContext, fresh per run unit
// (one calculate cascade or one format evaluation).
//
// Sandbox contract (design doc §3 — non-negotiable, enforced HERE):
//   * JS_SetMemoryLimit + JS_SetMaxStackSize set from SandboxLimits.
//   * JS_SetInterruptHandler with ONE absolute whole-operation deadline:
//     every call into the engine — setup, the authored script, exception
//     property reads, string/number coercion inside shim glue, toJSON hooks,
//     result collection — executes potentially hostile code, so the entire
//     operation (setup → eval → result extraction) runs under a single
//     deadline owned by the C++ caller (R05/JS-01: the per-eval-only deadline
//     with -1 gaps was reproducibly bypassable).
//   * ZERO host I/O: no quickjs-libc module is ever initialized — the engine
//     core exposes no stdin/stdout/filesystem/network primitives at all; the
//     only globals are the ECMAScript intrinsics plus the AF shim (AFormShim)
//     whose "host objects" are pure JS reading a host-provided value map.
//   * Egress verbs (doc.submitForm/doc.mailDoc/doc.exportData, app.launchURL/
//     app.mailme/app.execDialog/app.media) are hard no-ops that record an
//     audit entry in JsEvalResult::blocked.
//   * Every evaluation's exception is captured and classified.
class FormJsSandbox {
public:
    explicit FormJsSandbox(const SandboxLimits& limits = {});
    ~FormJsSandbox();
    FormJsSandbox(const FormJsSandbox&) = delete;
    FormJsSandbox& operator=(const FormJsSandbox&) = delete;

    bool isValid() const;

    // Installs the AF shim + host glue (once, before any runEvent). Fails
    // only if the shim source itself does not parse — a build-time defect.
    bool installShim(QString* error);

    // Sets/updates the field-value snapshot the shim's getField()/AFSimple_
    // Calculate read. Values are the fields' current /V strings. The snapshot
    // install is an engine entry (a hostile script can have replaced
    // `globalThis.__gpFieldValues` with a looping setter), so it runs under
    // its own whole-operation deadline; false = the install failed (`error`
    // carries the honest reason — the host-side map is still updated so a
    // later successful install sees the latest values).
    bool setFieldValues(const QVariantMap& nameToValue, QString* error = nullptr);
    bool updateFieldValue(const QString& name, const QString& value,
                          QString* error = nullptr);

    // Resets the event object and log sinks, then evaluates `script`.
    // `fieldName`/`eventKind` only shape the event object (honest attribution).
    // `deadlineMs` is the WHOLE OPERATION's hard deadline — one absolute
    // budget spanning the setup eval, the script eval, exception inspection,
    // the end-event result collection and every coercion/getter/toJSON it
    // triggers (the interrupt handler aborts the engine wherever it is when
    // the budget fires). Returns the classified outcome; on Timeout the
    // caller keeps the field's committed value (transaction policy).
    JsEvalResult runEvent(const QString& script,
                          const QString& fieldName,
                          const QString& eventKind,
                          const QString& currentValue,
                          int deadlineMs);

    // Evaluates a snippet in the shim's context and returns its string result
    // (used by tests and by the clock seam; not for authored scripts).
    bool evalHelper(const QString& code, QString* result, QString* error);

    const SandboxLimits& limits() const { return m_limits; }

private:
    struct Impl;
    // Shared body of setFieldValues/updateFieldValue — installs the host-side
    // field map into the engine under a whole-operation deadline (the
    // assignment can hit a hostile looping setter installed by an earlier
    // script) and reports honest failure instead of silently continuing.
    bool installFieldSnapshot(QString* error);
    SandboxLimits m_limits;
    std::unique_ptr<Impl> m_impl;
    QVariantMap m_fieldValues;
};

// Canonical string for a computed number when it is written back to /V:
// integral values without a decimal tail, otherwise 15 significant digits
// (keeps 0.1+0.2 at "0.3", never binary dust; goldens pin this).
QString canonicalNumberString(double v);

} // namespace gp::formjs
