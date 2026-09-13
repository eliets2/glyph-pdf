// SPDX-License-Identifier: Apache-2.0
#include "engines/formjs/FormJsSandbox.h"
#include "engines/formjs/AFormShim.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

#ifdef HAS_QUICKJS
#include <quickjs.h>
#endif

namespace gp::formjs {

// quickjs-ng object files must never carry host-I/O primitives into our link;
// the engine core (libqjs) does not reference them — only the opt-in
// quickjs-libc module does, and it is never initialized here. The behavioral
// pin lives in TestFormJsCalc (no readFile/fetch/Socket/print globals).

#ifdef HAS_QUICKJS

namespace {

// R05/JS-01: host-side egress accounting. The JS heap cap bounds what the
// ENGINE can allocate, but every string crossing into C++ is re-materialized
// on the host several times (the JS_ToCStringLen buffer, the QString, the
// toUtf8 copies, the QJsonDocument parse). A getter returning a near-cap
// string is therefore amplified several-fold in HOST memory, which the JS
// heap cap never sees. The guard refuses such a transfer before the host
// copy is made, and the operation fails with an honest Memory classification.
struct EgressGuard {
    qsizetype capBytes = 4 * 1024 * 1024;
    bool violated = false;
    QString reason;

    bool allows(size_t bytes, const char* what)
    {
        if (qsizetype(bytes) <= capBytes) return true;
        if (!violated) {
            violated = true;
            reason = QStringLiteral("the script produced a %1-byte %2, past the %3-byte host transfer cap; it was refused")
                         .arg(qint64(bytes)).arg(QString::fromLatin1(what)).arg(capBytes);
        }
        return false;
    }
};

// Converts one JS value to text under the host transfer cap. The conversion
// itself executes no script (string/number/bool only — hostile coercion must
// run inside an engine entry, where the whole-operation deadline is active),
// but the string a hostile getter materialized inside the engine can be near
// the heap cap, so the host-side copy is guarded.
QString jsValueToQString(JSContext* ctx, JSValue v, EgressGuard* guard)
{
    if (JS_IsString(v)) {
        size_t len = 0;
        const char* s = JS_ToCStringLen(ctx, &len, v);
        if (!s) return {};
        if (!guard->allows(len, "string result")) {
            JS_FreeCString(ctx, s);
            return {};
        }
        QString out = QString::fromUtf8(s, static_cast<qsizetype>(len));
        JS_FreeCString(ctx, s);
        return out;
    }
    if (JS_IsNumber(v)) {
        double d = 0;
        if (JS_ToFloat64(ctx, &d, v) == 0) return canonicalNumberString(d);
    }
    if (JS_IsBool(v)) return JS_ToBool(ctx, v) ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

// One honest description of a captured exception, with stack when present.
// `name` receives the error class ("SyntaxError", "RangeError", "Error", ...)
// used for honest classification; `message` the human text. Property reads
// here can hit hostile getters on a thrown object — the caller must hold the
// whole-operation deadline active.
void describeException(JSContext* ctx, JSValue exc, QString* name, QString* message,
                       EgressGuard* guard)
{
    QString text;
    QString errName;
    if (JS_IsError(exc)) {
        JSValue nameVal = JS_GetPropertyStr(ctx, exc, "name");
        errName = jsValueToQString(ctx, nameVal, guard);
        JS_FreeValue(ctx, nameVal);
        JSValue msg = JS_GetPropertyStr(ctx, exc, "message");
        text = jsValueToQString(ctx, msg, guard);
        JS_FreeValue(ctx, msg);
        if (text.isEmpty()) text = QStringLiteral("script error");
        JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
        if (!JS_IsUndefined(stack) && !JS_IsException(stack)) {
            const QString st = jsValueToQString(ctx, stack, guard);
            // First stack line only — enough to attribute the failure frame.
            const int nl = st.indexOf(QLatin1Char('\n'));
            if (nl > 0) text += QStringLiteral(" (") + st.left(nl).trimmed() + QLatin1Char(')');
            else if (!st.trimmed().isEmpty()) text += QStringLiteral(" (") + st.trimmed() + QLatin1Char(')');
        }
        JS_FreeValue(ctx, stack);
    } else {
        const QString asStr = jsValueToQString(ctx, exc, guard);
        text = asStr.isEmpty() ? QStringLiteral("non-Error exception") : asStr;
    }
    if (name) *name = errName;
    if (message) *message = text;
}

} // namespace

struct FormJsSandbox::Impl {
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    QElapsedTimer opClock;
    qint64 deadlineMs = -1;        // absolute, on opClock — active for the WHOLE operation
    bool interrupted = false;
    const char* phase = "the event setup"; // what the engine is running right now
    const char* interruptedPhase = nullptr; // what the engine was running when the budget fired
    EgressGuard egress;

    static int interruptHandler(JSRuntime* /*rt*/, void* opaque)
    {
        auto* self = static_cast<Impl*>(opaque);
        if (self->deadlineMs < 0) return 0;
        if (self->opClock.elapsed() > self->deadlineMs) {
            if (!self->interrupted) self->interruptedPhase = self->phase;
            self->interrupted = true;
            return 1; // abort the running evaluation
        }
        return 0;
    }

    // R05/JS-01: ONE absolute whole-operation deadline per public entry. It
    // begins at the FIRST engine entry of the operation (the setup eval, the
    // snapshot install, the shim install …) and stays active across every
    // later entry — the authored script, exception property reads, the
    // end-event collection, and every getter/coercion/toJSON hook those
    // trigger — until endOperation(). There are no `-1` gaps for script-
    // reachable code to hide in (the per-eval-only deadline with gaps was
    // reproducibly bypassable nine ways).
    void beginOperation(int budgetMs, const char* whatPhase)
    {
        opClock.start();
        deadlineMs = qMax(budgetMs, 0);
        interrupted = false;
        phase = whatPhase;
        interruptedPhase = nullptr;
        egress.violated = false;
        egress.reason.clear();
    }
    void endOperation() { deadlineMs = -1; }

    ~Impl()
    {
        if (ctx) JS_FreeContext(ctx);
        if (rt) JS_FreeRuntime(rt);
    }
};

FormJsSandbox::FormJsSandbox(const SandboxLimits& limits)
    : m_limits(limits), m_impl(std::make_unique<Impl>())
{
    m_impl->rt = JS_NewRuntime();
    if (!m_impl->rt) return;
    m_impl->ctx = JS_NewContext(m_impl->rt);
    if (!m_impl->ctx) {
        JS_FreeRuntime(m_impl->rt);
        m_impl->rt = nullptr;
        return;
    }
    // §3.1 Memory: hard allocation ceiling + interpreter stack cap.
    JS_SetMemoryLimit(m_impl->rt, static_cast<size_t>(m_limits.memoryLimitBytes));
    JS_SetMaxStackSize(m_impl->rt, static_cast<size_t>(m_limits.stackLimitBytes));
    // §3.1 CPU: the interrupt handler enforces the whole-operation deadline.
    JS_SetInterruptHandler(m_impl->rt, &Impl::interruptHandler, m_impl.get());
    // Host-side transfer cap from the same limits (R05/JS-01).
    m_impl->egress.capBytes = m_limits.maxTransferBytes;
    // NOTE: no js_init_module / quickjs-libc registration — zero host I/O.
}

FormJsSandbox::~FormJsSandbox() = default;

bool FormJsSandbox::isValid() const
{
    return m_impl && m_impl->ctx != nullptr;
}

bool FormJsSandbox::installShim(QString* error)
{
    if (!isValid()) {
        if (error) *error = QStringLiteral("quickjs runtime is unavailable in this build");
        return false;
    }
    const QByteArray src(aformShimSource());
    m_impl->beginOperation(m_limits.eventDeadlineMs, "the shim installation");
    JSValue v = JS_Eval(m_impl->ctx, src.constData(), size_t(src.size()),
                        QStringLiteral("<aform-shim>").toUtf8().constData(),
                        JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString name, msg;
        describeException(m_impl->ctx, exc, &name, &msg, &m_impl->egress);
        JS_FreeValue(m_impl->ctx, exc);
        if (error) {
            *error = m_impl->interrupted
                ? QStringLiteral("AF shim installation was aborted at the %1 ms deadline")
                      .arg(m_limits.eventDeadlineMs)
                : QStringLiteral("AF shim failed to install: %1: %2").arg(name, msg);
        }
        m_impl->endOperation();
        return false;
    }
    JS_FreeValue(m_impl->ctx, v);
    m_impl->endOperation();
    return true;
}

bool FormJsSandbox::installFieldSnapshot(QString* error)
{
    if (!isValid()) {
        if (error) *error = QStringLiteral("quickjs runtime is unavailable in this build");
        return false;
    }
    QJsonObject obj;
    for (auto it = m_fieldValues.constBegin(); it != m_fieldValues.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue(it.value().toString()));
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (qsizetype(json.size()) > m_limits.maxTransferBytes) {
        if (error)
            *error = QStringLiteral("the field-value snapshot (%1 bytes) exceeds the %2-byte host transfer cap")
                         .arg(json.size()).arg(m_limits.maxTransferBytes);
        return false;
    }
    // JSON is a syntactic subset of JS, so the document embeds directly. The
    // ASSIGNMENT is an engine entry: a hostile script can have replaced
    // `globalThis.__gpFieldValues` with a looping setter (reproduced bypass) —
    // it runs under the whole-operation deadline like everything else.
    const QString code = QStringLiteral("globalThis.__gpFieldValues = %1;")
                             .arg(QString::fromUtf8(json));
    const QByteArray utf8 = code.toUtf8();
    m_impl->beginOperation(m_limits.eventDeadlineMs, "the field snapshot refresh");
    JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                        QStringLiteral("<field-values>").toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(v);
    if (!ok) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString name, msg;
        describeException(m_impl->ctx, exc, &name, &msg, &m_impl->egress);
        JS_FreeValue(m_impl->ctx, exc);
        if (error) {
            *error = m_impl->interrupted
                ? QStringLiteral("the field snapshot refresh was aborted at the %1 ms deadline")
                      .arg(m_limits.eventDeadlineMs)
                : QStringLiteral("the field snapshot could not be installed: %1")
                      .arg(name.isEmpty() ? msg : name + QLatin1String(": ") + msg);
        }
    }
    m_impl->endOperation();
    JS_FreeValue(m_impl->ctx, v);
    return ok;
}

bool FormJsSandbox::setFieldValues(const QVariantMap& nameToValue, QString* error)
{
    m_fieldValues = nameToValue;
    return installFieldSnapshot(error);
}

bool FormJsSandbox::updateFieldValue(const QString& name, const QString& value, QString* error)
{
    m_fieldValues.insert(name, value);
    return installFieldSnapshot(error);
}

JsEvalResult FormJsSandbox::runEvent(const QString& script,
                                     const QString& fieldName,
                                     const QString& eventKind,
                                     const QString& currentValue,
                                     int deadlineMs)
{
    JsEvalResult out;
    if (!isValid()) {
        out.kind = JsErrorKind::Exception;
        out.message = QStringLiteral("quickjs runtime is unavailable in this build");
        return out;
    }

    // Host-side input cap: the authored script text is re-encoded (toUtf8)
    // before it ever reaches the engine; refuse absurd sizes here rather than
    // copying them. (Numbers are the host's own honest bookkeeping.)
    if (qsizetype(script.size()) > m_limits.maxScriptBytes) {
        out.kind = JsErrorKind::Memory;
        out.message = QStringLiteral("the script text (%1 bytes) exceeds the %2-byte maximum")
                          .arg(script.size()).arg(m_limits.maxScriptBytes);
        return out;
    }

    // R05/JS-01: ONE absolute whole-operation deadline, set ONCE before the
    // first engine entry and active across EVERY entry below — the setup eval
    // (whose helper may be hostile code from an earlier event), the authored
    // script, exception property reads, the end-event collection, and every
    // getter / coercion (toString/valueOf) / toJSON hook those trigger. The
    // caller's `deadlineMs` is the cascade-aware budget for the WHOLE
    // operation, not just the script eval.
    const qint64 opDeadline = qMax(deadlineMs, 0);
    m_impl->beginOperation(int(opDeadline), "the event setup");

    // 1. Reset the event + sinks.
    QJsonObject setup;
    setup.insert(QStringLiteral("name"), fieldName);
    setup.insert(QStringLiteral("value"), currentValue);
    setup.insert(QStringLiteral("eventKind"), eventKind);
    const QString begin = QStringLiteral("globalThis.__gpBeginEvent(%1);")
                              .arg(QString::fromUtf8(QJsonDocument(setup).toJson(QJsonDocument::Compact)));
    {
        const QByteArray utf8 = begin.toUtf8();
        JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                            QStringLiteral("<begin-event>").toUtf8().constData(),
                            JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(v)) {
            JSValue exc = JS_GetException(m_impl->ctx);
            QString excName, excMsg;
            describeException(m_impl->ctx, exc, &excName, &excMsg, &m_impl->egress);
            JS_FreeValue(m_impl->ctx, exc);
            if (m_impl->interrupted) {
                out.kind = JsErrorKind::Timeout;
                out.message = QStringLiteral("execution was aborted after exceeding the %1 ms deadline during the event setup")
                                  .arg(opDeadline);
            } else {
                out.kind = JsErrorKind::Exception;
                out.message = QStringLiteral("internal: event setup failed");
            }
            m_impl->endOperation();
            return out;
        }
        JS_FreeValue(m_impl->ctx, v);
    }

    // 2. Evaluate the authored script under the same deadline.
    const QByteArray utf8 = script.toUtf8();
    const QByteArray fname = QStringLiteral("<%1:%2>").arg(eventKind, fieldName).toUtf8();
    m_impl->phase = "the script";
    bool scriptFailed = false;
    JsErrorKind scriptKind = JsErrorKind::None;
    QString excName, excMsg;
    {
        JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                            fname.constData(), JS_EVAL_TYPE_GLOBAL);
        scriptFailed = JS_IsException(v);
        if (scriptFailed) {
            JSValue exc = JS_GetException(m_impl->ctx);
            // Exception property reads (name/message/stack) can be hostile
            // getters on a thrown object — still under the same budget.
            describeException(m_impl->ctx, exc, &excName, &excMsg, &m_impl->egress);
            if (!m_impl->interrupted) {
                // Honest classification from the error CLASS (the message text
                // of a SyntaxError does not carry the "SyntaxError" name):
                //   SyntaxError                                  → Syntax
                //   InternalError "out of memory"                → Memory (cap hit)
                //   RangeError "Maximum call stack size exceeded" → Memory (stack cap)
                //   NULL/undefined sentinel + runtime at its ceiling → Memory —
                //     when the allocation cap fires, the engine cannot even build
                //     an Error object, so it throws the empty sentinel; the
                //     runtime's usage against the configured cap is the evidence.
                //   everything else                              → Exception
                scriptKind = JsErrorKind::Exception;
                if (excName == QLatin1String("SyntaxError")) {
                    scriptKind = JsErrorKind::Syntax;
                } else if ((excName == QLatin1String("InternalError")
                            && excMsg.contains(QLatin1String("out of memory"), Qt::CaseInsensitive))
                           || (excName == QLatin1String("InternalError")
                               && excMsg.contains(QLatin1String("stack overflow"), Qt::CaseInsensitive))
                           || (excName == QLatin1String("RangeError")
                               && excMsg.contains(QLatin1String("call stack"), Qt::CaseInsensitive))) {
                    scriptKind = JsErrorKind::Memory;
                } else if ((JS_IsNull(exc) || JS_IsUndefined(exc))) {
                    JSMemoryUsage mu;
                    JS_ComputeMemoryUsage(m_impl->rt, &mu);
                    if (mu.malloc_size >= size_t(m_limits.memoryLimitBytes * 0.9)) {
                        scriptKind = JsErrorKind::Memory;
                        excMsg = QStringLiteral("the sandbox refused allocation at the %1 MiB memory cap")
                                     .arg(int(m_limits.memoryLimitBytes / (1024 * 1024)));
                        excName = QStringLiteral("MemoryLimit");
                    }
                }
            }
            JS_FreeValue(m_impl->ctx, exc);
        } else {
            JS_FreeValue(m_impl->ctx, v);
        }
    }

    // 3. Read the event snapshot back (even on failure — logs/blocked audit
    //    entries accumulated before a failure are still honest evidence; a
    //    lapsed budget lets loop-free helpers finish while any loop in them
    //    aborts at the first interrupt poll). The end-event helper may be
    //    hostile-replaced code; every getter, coercion (String →
    //    toString/valueOf) and JSON.stringify toJSON hook it fires runs under
    //    the SAME budget.
    m_impl->phase = "the event result collection";
    bool snapshotOk = false;
    {
        const char* endCode = "globalThis.__gpEndEvent()";
        JSValue ev = JS_Eval(m_impl->ctx, endCode, strlen(endCode),
                             QStringLiteral("<end-event>").toUtf8().constData(),
                             JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(ev)) {
            const QString json = jsValueToQString(m_impl->ctx, ev, &m_impl->egress);
            JS_FreeValue(m_impl->ctx, ev);
            if (!m_impl->egress.violated) {
                const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
                const QJsonObject o = doc.object();
                for (const auto& l : o.value(QLatin1String("logs")).toArray())
                    out.logs << l.toString();
                for (const auto& b : o.value(QLatin1String("blocked")).toArray())
                    out.blocked << b.toString();
                snapshotOk = true;
                if (!scriptFailed) {
                    out.rc = o.value(QLatin1String("rc")).toBool(true);
                    out.hasValue = o.value(QLatin1String("hasValue")).toBool();
                    // Numbers arrive as JSON numbers — canonicalize through the
                    // same /V formatting the write path uses (QJsonValue::
                    // toString() is deliberately empty for non-strings).
                    const QJsonValue jsonValue = o.value(QLatin1String("value"));
                    out.value = jsonValue.isDouble() ? canonicalNumberString(jsonValue.toDouble())
                                                     : jsonValue.toString();
                }
            }
        } else {
            JSValue exc = JS_GetException(m_impl->ctx);
            JS_FreeValue(m_impl->ctx, exc);
        }
    }
    m_impl->endOperation();

    // 4. ONE honest classification for the whole operation. A timeout in ANY
    //    phase — or the user's value preservation (the caller keeps the
    //    committed /V whenever ok is false) — is decided here, exactly once.
    if (m_impl->interrupted) {
        out.ok = false;
        out.kind = JsErrorKind::Timeout;
        out.message = QStringLiteral("execution was aborted after exceeding the %1 ms deadline while %2 was running")
                          .arg(opDeadline)
                          .arg(QLatin1String(m_impl->interruptedPhase ? m_impl->interruptedPhase : m_impl->phase));
        return out;
    }
    if (m_impl->egress.violated) {
        out.ok = false;
        out.kind = JsErrorKind::Memory;
        out.message = m_impl->egress.reason;
        return out;
    }
    if (scriptFailed) {
        out.ok = false;
        out.kind = scriptKind;
        out.message = excName.isEmpty() ? excMsg : excName + QLatin1String(": ") + excMsg;
        return out;
    }
    if (!snapshotOk) {
        out.ok = false;
        out.kind = JsErrorKind::Exception;
        out.message = QStringLiteral("the event result could not be collected (the end-event helper failed)");
        return out;
    }
    out.ok = true;
    out.message.clear();
    return out;
}

bool FormJsSandbox::evalHelper(const QString& code, QString* result, QString* error)
{
    if (!isValid()) {
        if (error) *error = QStringLiteral("runtime unavailable");
        return false;
    }
    if (qsizetype(code.size()) > m_limits.maxScriptBytes) {
        if (error)
            *error = QStringLiteral("the helper code (%1 bytes) exceeds the %2-byte maximum")
                         .arg(code.size()).arg(m_limits.maxScriptBytes);
        return false;
    }
    const QByteArray utf8 = code.toUtf8();
    m_impl->beginOperation(m_limits.eventDeadlineMs, "the helper evaluation");
    JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                        QStringLiteral("<helper>").toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString name, msg;
        describeException(m_impl->ctx, exc, &name, &msg, &m_impl->egress);
        JS_FreeValue(m_impl->ctx, exc);
        if (error) {
            *error = m_impl->interrupted
                ? QStringLiteral("the helper evaluation was aborted at the %1 ms deadline")
                      .arg(m_limits.eventDeadlineMs)
                : (name.isEmpty() ? msg : name + QLatin1String(": ") + msg);
        }
        m_impl->endOperation();
        return false;
    }
    if (result) *result = jsValueToQString(m_impl->ctx, v, &m_impl->egress);
    JS_FreeValue(m_impl->ctx, v);
    m_impl->endOperation();
    if (m_impl->egress.violated) {
        if (error) *error = m_impl->egress.reason;
        return false;
    }
    return true;
}

QString canonicalNumberString(double v)
{
    if (std::isnan(v) || std::isinf(v)) return {};
    if (std::fabs(v) < 1e-15) return QStringLiteral("0");
    if (std::floor(v) == v && std::fabs(v) < 1e15)
        return QString::number(static_cast<qlonglong>(v));
    return QString::number(v, 'g', 15);
}

#else // !HAS_QUICKJS

// No-engine build: every entry point fails honestly with the disclosure
// reason (CapabilityRegistry carries the user-facing copy). Behavior tests
// for the engine are compiled only with HAS_QUICKJS.
struct FormJsSandbox::Impl { int unused = 0; };

FormJsSandbox::FormJsSandbox(const SandboxLimits& limits)
    : m_limits(limits), m_impl(std::make_unique<Impl>()) {}
FormJsSandbox::~FormJsSandbox() = default;
bool FormJsSandbox::isValid() const { return false; }
bool FormJsSandbox::installShim(QString* error)
{
    if (error) *error = QStringLiteral("this build was compiled without a JavaScript engine");
    return false;
}
bool FormJsSandbox::installFieldSnapshot(QString* error)
{
    if (error) *error = QStringLiteral("this build was compiled without a JavaScript engine");
    return false;
}
bool FormJsSandbox::setFieldValues(const QVariantMap&, QString* error)
{
    if (error) *error = QStringLiteral("this build was compiled without a JavaScript engine");
    return false;
}
bool FormJsSandbox::updateFieldValue(const QString&, const QString&, QString* error)
{
    if (error) *error = QStringLiteral("this build was compiled without a JavaScript engine");
    return false;
}
JsEvalResult FormJsSandbox::runEvent(const QString&, const QString&, const QString&, const QString&, int)
{
    JsEvalResult out;
    out.kind = JsErrorKind::Exception;
    out.message = QStringLiteral("this build was compiled without a JavaScript engine");
    return out;
}
bool FormJsSandbox::evalHelper(const QString&, QString*, QString* error)
{
    if (error) *error = QStringLiteral("this build was compiled without a JavaScript engine");
    return false;
}
QString canonicalNumberString(double v) { return {}; }

#endif // HAS_QUICKJS

} // namespace gp::formjs
