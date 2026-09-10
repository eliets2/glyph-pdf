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

QString jsValueToQString(JSContext* ctx, JSValue v)
{
    if (JS_IsString(v)) {
        size_t len = 0;
        const char* s = JS_ToCStringLen(ctx, &len, v);
        if (!s) return {};
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
// used for honest classification; `message` the human text.
void describeException(JSContext* ctx, JSValue exc, QString* name, QString* message)
{
    QString text;
    QString errName;
    if (JS_IsError(exc)) {
        JSValue nameVal = JS_GetPropertyStr(ctx, exc, "name");
        errName = jsValueToQString(ctx, nameVal);
        JS_FreeValue(ctx, nameVal);
        JSValue msg = JS_GetPropertyStr(ctx, exc, "message");
        text = jsValueToQString(ctx, msg);
        JS_FreeValue(ctx, msg);
        if (text.isEmpty()) text = QStringLiteral("script error");
        JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
        if (!JS_IsUndefined(stack) && !JS_IsException(stack)) {
            const QString st = jsValueToQString(ctx, stack);
            // First stack line only — enough to attribute the failure frame.
            const int nl = st.indexOf(QLatin1Char('\n'));
            if (nl > 0) text += QStringLiteral(" (") + st.left(nl).trimmed() + QLatin1Char(')');
            else if (!st.trimmed().isEmpty()) text += QStringLiteral(" (") + st.trimmed() + QLatin1Char(')');
        }
        JS_FreeValue(ctx, stack);
    } else {
        const QString asStr = jsValueToQString(ctx, exc);
        text = asStr.isEmpty() ? QStringLiteral("non-Error exception") : asStr;
    }
    if (name) *name = errName;
    if (message) *message = text;
}

} // namespace

struct FormJsSandbox::Impl {
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    QElapsedTimer evalClock;
    qint64 deadlineMs = -1;   // absolute, on evalClock
    bool interrupted = false;

    static int interruptHandler(JSRuntime* /*rt*/, void* opaque)
    {
        auto* self = static_cast<Impl*>(opaque);
        if (self->deadlineMs < 0) return 0;
        if (self->evalClock.elapsed() > self->deadlineMs) {
            self->interrupted = true;
            return 1; // abort the running evaluation
        }
        return 0;
    }

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
    // §3.1 CPU: the interrupt handler enforces the per-evaluation deadline.
    JS_SetInterruptHandler(m_impl->rt, &Impl::interruptHandler, m_impl.get());
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
    JsEvalResult r;
    JSValue v = JS_Eval(m_impl->ctx, src.constData(), size_t(src.size()),
                        QStringLiteral("<aform-shim>").toUtf8().constData(),
                        JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString name, msg;
        describeException(m_impl->ctx, exc, &name, &msg);
        if (error) *error = QStringLiteral("AF shim failed to install: %1: %2").arg(name, msg);
        JS_FreeValue(m_impl->ctx, exc);
        return false;
    }
    JS_FreeValue(m_impl->ctx, v);
    return true;
}

void FormJsSandbox::setFieldValues(const QVariantMap& nameToValue)
{
    m_fieldValues = nameToValue;
    if (!isValid()) return;
    QJsonObject obj;
    for (auto it = nameToValue.constBegin(); it != nameToValue.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue(it.value().toString()));
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    // JSON is a syntactic subset of JS, so the document embeds directly.
    const QString code = QStringLiteral("globalThis.__gpFieldValues = %1;")
                             .arg(QString::fromUtf8(json));
    JSValue v = JS_Eval(m_impl->ctx, code.toUtf8().constData(), size_t(code.toUtf8().size()),
                        QStringLiteral("<field-values>").toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(m_impl->ctx, v);
}

void FormJsSandbox::updateFieldValue(const QString& name, const QString& value)
{
    m_fieldValues.insert(name, value);
    if (!isValid()) return;
    QJsonObject obj;
    for (auto it = m_fieldValues.constBegin(); it != m_fieldValues.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue(it.value().toString()));
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QString code = QStringLiteral("globalThis.__gpFieldValues = %1;")
                             .arg(QString::fromUtf8(json));
    JSValue v = JS_Eval(m_impl->ctx, code.toUtf8().constData(), size_t(code.toUtf8().size()),
                        QStringLiteral("<field-values>").toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(m_impl->ctx, v);
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
            out.kind = JsErrorKind::Exception;
            out.message = QStringLiteral("internal: event setup failed");
            return out;
        }
        JS_FreeValue(m_impl->ctx, v);
    }

    // 2. Evaluate the authored script under the deadline.
    const QByteArray utf8 = script.toUtf8();
    const QByteArray fname = QStringLiteral("<%1:%2>").arg(eventKind, fieldName).toUtf8();
    m_impl->evalClock.start();
    m_impl->deadlineMs = deadlineMs;
    m_impl->interrupted = false;
    JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                        fname.constData(), JS_EVAL_TYPE_GLOBAL);
    const bool failed = JS_IsException(v);
    if (failed) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString excName, excMsg;
        describeException(m_impl->ctx, exc, &excName, &excMsg);
        out.kind = m_impl->interrupted
            ? JsErrorKind::Timeout
            : JsErrorKind::Exception;
        if (m_impl->interrupted) {
            out.message = QStringLiteral("execution was aborted after exceeding the %1 ms deadline")
                              .arg(deadlineMs);
        } else {
            // Honest classification from the error CLASS (the message text of
            // a SyntaxError does not carry the "SyntaxError" name):
            //   SyntaxError                                  → Syntax
            //   InternalError "out of memory"                → Memory (cap hit)
            //   RangeError "Maximum call stack size exceeded" → Memory (stack cap)
            //   NULL/undefined sentinel + runtime at its ceiling → Memory —
            //     when the allocation cap fires, the engine cannot even build
            //     an Error object, so it throws the empty sentinel; the
            //     runtime's usage against the configured cap is the evidence.
            //   everything else                              → Exception
            if (excName == QLatin1String("SyntaxError")) {
                out.kind = JsErrorKind::Syntax;
            } else if ((excName == QLatin1String("InternalError")
                        && excMsg.contains(QLatin1String("out of memory"), Qt::CaseInsensitive))
                       || (excName == QLatin1String("InternalError")
                           && excMsg.contains(QLatin1String("stack overflow"), Qt::CaseInsensitive))
                       || (excName == QLatin1String("RangeError")
                           && excMsg.contains(QLatin1String("call stack"), Qt::CaseInsensitive))) {
                out.kind = JsErrorKind::Memory;
            } else if ((JS_IsNull(exc) || JS_IsUndefined(exc))) {
                JSMemoryUsage mu;
                JS_ComputeMemoryUsage(m_impl->rt, &mu);
                if (mu.malloc_size >= size_t(m_limits.memoryLimitBytes * 0.9)) {
                    out.kind = JsErrorKind::Memory;
                    excMsg = QStringLiteral("the sandbox refused allocation at the %1 MiB memory cap")
                                 .arg(int(m_limits.memoryLimitBytes / (1024 * 1024)));
                    excName = QStringLiteral("MemoryLimit");
                }
            }
            out.message = excName.isEmpty() ? excMsg : excName + QLatin1String(": ") + excMsg;
        }
        JS_FreeValue(m_impl->ctx, exc);
    } else {
        JS_FreeValue(m_impl->ctx, v);
    }
    m_impl->deadlineMs = -1;

    // 3. Read the event snapshot back (even on failure — logs/blocked audit
    //    entries accumulated before the failure are still honest evidence).
    {
        const char* endCode = "globalThis.__gpEndEvent()";
        JSValue ev = JS_Eval(m_impl->ctx, endCode, strlen(endCode),
                             QStringLiteral("<end-event>").toUtf8().constData(),
                             JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(ev)) {
            const QString json = jsValueToQString(m_impl->ctx, ev);
            JS_FreeValue(m_impl->ctx, ev);
            const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            const QJsonObject o = doc.object();
            for (const auto& l : o.value(QLatin1String("logs")).toArray())
                out.logs << l.toString();
            for (const auto& b : o.value(QLatin1String("blocked")).toArray())
                out.blocked << b.toString();
            if (!failed) {
                out.ok = true;
                out.rc = o.value(QLatin1String("rc")).toBool(true);
                out.hasValue = o.value(QLatin1String("hasValue")).toBool();
                // Numbers arrive as JSON numbers — canonicalize through the
                // same /V formatting the write path uses (QJsonValue::
                // toString() is deliberately empty for non-strings).
                const QJsonValue jsonValue = o.value(QLatin1String("value"));
                out.value = jsonValue.isDouble() ? canonicalNumberString(jsonValue.toDouble())
                                                 : jsonValue.toString();
                out.message.clear();
            }
        } else {
            JSValue exc = JS_GetException(m_impl->ctx);
            JS_FreeValue(m_impl->ctx, exc);
        }
    }
    return out;
}

bool FormJsSandbox::evalHelper(const QString& code, QString* result, QString* error)
{
    if (!isValid()) {
        if (error) *error = QStringLiteral("runtime unavailable");
        return false;
    }
    const QByteArray utf8 = code.toUtf8();
    JSValue v = JS_Eval(m_impl->ctx, utf8.constData(), size_t(utf8.size()),
                        QStringLiteral("<helper>").toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(m_impl->ctx);
        QString name, msg;
        describeException(m_impl->ctx, exc, &name, &msg);
        if (error) *error = name.isEmpty() ? msg : name + QLatin1String(": ") + msg;
        JS_FreeValue(m_impl->ctx, exc);
        return false;
    }
    if (result) *result = jsValueToQString(m_impl->ctx, v);
    JS_FreeValue(m_impl->ctx, v);
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
void FormJsSandbox::setFieldValues(const QVariantMap&) {}
void FormJsSandbox::updateFieldValue(const QString&, const QString&) {}
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
