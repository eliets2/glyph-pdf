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
};

// One sandbox = one quickjs-ng JSRuntime + JSContext, fresh per run unit
// (one calculate cascade or one format evaluation).
//
// Sandbox contract (design doc §3 — non-negotiable, enforced HERE):
//   * JS_SetMemoryLimit + JS_SetMaxStackSize set from SandboxLimits.
//   * JS_SetInterruptHandler with a monotonic per-evaluation deadline.
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
    // Calculate read. Values are the fields' current /V strings.
    void setFieldValues(const QVariantMap& nameToValue);
    void updateFieldValue(const QString& name, const QString& value);

    // Resets the event object and log sinks, then evaluates `script`.
    // `fieldName`/`eventKind` only shape the event object (honest attribution).
    // `deadlineMs` is this evaluation's hard deadline (the interrupt handler
    // aborts execution when it fires). Returns the classified outcome.
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
    SandboxLimits m_limits;
    std::unique_ptr<Impl> m_impl;
    QVariantMap m_fieldValues;
};

// Canonical string for a computed number when it is written back to /V:
// integral values without a decimal tail, otherwise 15 significant digits
// (keeps 0.1+0.2 at "0.3", never binary dust; goldens pin this).
QString canonicalNumberString(double v);

} // namespace gp::formjs
