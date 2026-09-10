// SPDX-License-Identifier: Apache-2.0
#include "engines/formjs/FormJsRunner.h"
#include "engines/formjs/FormJsSandbox.h"

#include <QDebug>
#include <QElapsedTimer>

#include <podofo/podofo.h>

#include <map>
#include <set>
#include <string>

// wingdi.h defines GetObject as an object-like macro in UNICODE builds; it
// collides with PoDoFo::PdfField::GetObject (same guard as TestFormSafety).
#ifdef GetObject
#undef GetObject
#endif

namespace gp::formjs {

namespace {

bool& executionEnabledFlag()
{
#ifdef HAS_QUICKJS
    static bool s_enabled = true; // engine linked → execution is the default
#else
    static bool s_enabled = false; // no engine → the disclosure state stands
#endif
    return s_enabled;
}

QString pdfErrorText(const PoDoFo::PdfError& e)
{
    return QStringLiteral("PoDoFo error: %1").arg(QString::fromLatin1(e.what()));
}

PoDoFo::PdfDictionary& fieldDict(PoDoFo::PdfField& f) { return f.GetDictionary(); }
const PoDoFo::PdfDictionary& fieldDict(const PoDoFo::PdfField& f) { return (f.GetObject)().GetDictionary(); }

QString stringOf(const PoDoFo::PdfString& s)
{
    return QString::fromUtf8(s.GetString().data(), static_cast<qsizetype>(s.GetString().size()));
}

// Reads the field's current value the same way captureFieldSnapshot does —
// one documented meaning of "value" across engine and UI.
QString readValue(const PoDoFo::PdfField& field)
{
    const PoDoFo::PdfDictionary& dict = fieldDict(field);
    switch (field.GetType()) {
        case PoDoFo::PdfFieldType::TextBox: {
            // const accessor path: read the /V key directly (GetText is
            // non-const in PoDoFo 1.1).
            if (const PoDoFo::PdfObject* v = dict.FindKey("V"); v && v->IsString())
                return stringOf(v->GetString());
            return {};
        }
        case PoDoFo::PdfFieldType::CheckBox:
            if (auto* c = dynamic_cast<const PoDoFo::PdfCheckBox*>(&field))
                return c->IsChecked() ? QStringLiteral("Yes") : QStringLiteral("Off");
            return {};
        default: {
            if (const PoDoFo::PdfObject* v = dict.FindKey("V"); v && v->IsString())
                return stringOf(v->GetString());
            return {};
        }
    }
}

// Extracts the JavaScript source of /AA <key> when it is a runnable
// string action. Returns false (with `why` set) for structural problems —
// those surface as honest per-field failures, never as silent no-ops.
bool extractActionScript(const PoDoFo::PdfField& field, char actionKey,
                         QString* script, QString* why)
{
    const PoDoFo::PdfDictionary& dict = fieldDict(field);
    const PoDoFo::PdfObject* aa = dict.FindKey("AA");
    if (!aa || !aa->IsDictionary()) {
        if (why) *why = QStringLiteral("field has no /AA dictionary");
        return false;
    }
    const PoDoFo::PdfObject* action = aa->GetDictionary().FindKey(PoDoFo::PdfName(std::string(1, actionKey)));
    if (!action || !action->IsDictionary()) {
        if (why) *why = QStringLiteral("/AA has no /%1 action").arg(QChar(actionKey));
        return false;
    }
    const PoDoFo::PdfObject* s = action->GetDictionary().FindKey("S");
    if (!s || !s->IsName()
        || QLatin1String(s->GetName().GetString().data(), qsizetype(s->GetName().GetString().size()))
               != QLatin1String("JavaScript")) {
        if (why) *why = QStringLiteral("/AA /%1 /S is not JavaScript").arg(QChar(actionKey));
        return false;
    }
    const PoDoFo::PdfObject* js = action->GetDictionary().FindKey("JS");
    if (!js) {
        if (why) *why = QStringLiteral("/AA /%1 has no /JS text").arg(QChar(actionKey));
        return false;
    }
    if (js->IsString()) {
        if (script) *script = stringOf(js->GetString());
        return true;
    }
    if (why) *why = QStringLiteral("/JS is a stream — not supported in Phase 1");
    return false;
}

const char* kindString(JsErrorKind kind)
{
    switch (kind) {
        case JsErrorKind::Syntax: return "syntax";
        case JsErrorKind::Timeout: return "timeout";
        case JsErrorKind::Memory: return "memory";
        case JsErrorKind::Exception: return "exception";
        case JsErrorKind::None: break;
    }
    return "exception";
}

} // namespace

// ── Kill-switch ──────────────────────────────────────────────────────────────

void FormJsRunner::setExecutionGloballyEnabled(bool on)
{
#ifdef HAS_QUICKJS
    executionEnabledFlag() = on;
#else
    Q_UNUSED(on); // no engine linked: the disabled state is the only truth
#endif
}

bool FormJsRunner::executionGloballyEnabled()
{
    return executionEnabledFlag();
}

// ── Value helpers ────────────────────────────────────────────────────────────

QVariantMap FormJsRunner::collectFieldValues(PoDoFo::PdfMemDocument& doc)
{
    QVariantMap values;
    try {
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return values;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            values.insert(QString::fromStdString(field.GetFullName()), readValue(field));
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "FormJsRunner::collectFieldValues:" << e.what();
    }
    return values;
}

bool FormJsRunner::writeFieldValue(PoDoFo::PdfMemDocument& doc, const QString& name,
                                   const QString& value)
{
    try {
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return false;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != name) continue;
            if (field.GetType() != PoDoFo::PdfFieldType::TextBox) return false;
            auto* textBox = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
            if (!textBox) return false;
            textBox->SetText(PoDoFo::PdfString(value.toStdString()));
            return true;
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "FormJsRunner::writeFieldValue:" << e.what();
    }
    return false;
}

bool FormJsRunner::fieldHasActionScript(const PoDoFo::PdfField& field, char actionKey)
{
    QString script, why;
    return extractActionScript(field, actionKey, &script, &why);
}

bool FormJsRunner::hasCalculateEntries(PoDoFo::PdfMemDocument& doc)
{
    try {
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return false;
        const PoDoFo::PdfObject* co = acroForm->GetDictionary().FindKey("CO");
        return co && co->IsArray() && co->GetArray().GetSize() > 0;
    } catch (const PoDoFo::PdfError&) {
        return false;
    }
}

// ── Calculate cascade ────────────────────────────────────────────────────────

CascadeReport FormJsRunner::runCalculateCascade(PoDoFo::PdfMemDocument& doc,
                                                int eventDeadlineMs,
                                                int cascadeDeadlineMs)
{
    CascadeReport report;
    if (!executionEnabledFlag())
        return report; // disabled: the caller's disclosure state, nothing runs

    try {
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return report;
        const PoDoFo::PdfObject* co = acroForm->GetDictionary().FindKey("CO");
        if (!co || !co->IsArray()) return report;

        // name → field table (first occurrence wins on duplicate full names,
        // the same policy every other FormManager mutation documents).
        std::map<std::string, PoDoFo::PdfField*> byRef;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            byRef[(field.GetObject)().GetIndirectReference().ToString()] = &field;
        }

        // Resolve the /CO order. Cyclic/malformed entries terminate: a
        // reference resolved twice is skipped (pdf.js `_isCalculating`
        // equivalent) and the loop is hard-capped — a corrupt /CO can never
        // spin the cascade.
        std::set<std::string> calculatedOnce;
        const size_t coSize = co->GetArray().GetSize();
        size_t guard = coSize * 2 + 8;

        FormJsSandbox sandbox;
        if (!sandbox.isValid()) {
            FieldJsFailure f;
            f.kind = QStringLiteral("engine");
            f.reason = QStringLiteral("quickjs runtime is unavailable in this build");
            report.failures.append(f);
            report.engineAborted = true;
            return report;
        }
        QString shimError;
        if (!sandbox.installShim(&shimError)) {
            FieldJsFailure f;
            f.kind = QStringLiteral("engine");
            f.reason = shimError;
            report.failures.append(f);
            report.engineAborted = true;
            return report;
        }
        sandbox.setFieldValues(collectFieldValues(doc));

        QElapsedTimer cascadeClock;
        cascadeClock.start();

        for (const auto& item : co->GetArray()) {
            if (guard-- == 0) break;
            if (!item.IsReference()) continue;
            const auto it = byRef.find(item.GetReference().ToString());
            if (it == byRef.end()) continue; // dangling /CO ref — nothing to run
            PoDoFo::PdfField* field = it->second;
            const QString name = QString::fromStdString(field->GetFullName());
            if (!calculatedOnce.insert((field->GetObject)().GetIndirectReference().ToString()).second)
                continue; // cycle: this field already ran once in this cascade

            QString script;
            QString why;
            if (!extractActionScript(*field, 'C', &script, &why)) {
                // A /CO entry without a runnable calculate action is ordinary
                // (setTabOrder also appends non-calculated fields) — skip.
                Q_UNUSED(why);
                continue;
            }
            ++report.attempted;

            // Cascade deadline: each event gets the remaining cascade budget,
            // capped by the per-event ceiling (design doc §3.1).
            const int remaining = cascadeDeadlineMs - int(cascadeClock.elapsed());
            if (remaining <= 0) {
                FieldJsFailure f;
                f.fieldName = name;
                f.kind = QStringLiteral("timeout");
                f.reason = QStringLiteral("the calculate cascade exceeded its %1 ms budget before this field")
                               .arg(cascadeDeadlineMs);
                report.failures.append(f);
                report.engineAborted = true;
                break;
            }
            const int deadline = qMin(eventDeadlineMs, remaining);

            JsEvalResult r = sandbox.runEvent(script, name, QStringLiteral("Calculate"),
                                              readValue(*field), deadline);
            report.logs += r.logs;
            report.blockedActions += r.blocked;

            if (!r.ok) {
                FieldJsFailure f;
                f.fieldName = name;
                f.kind = QLatin1String(kindString(r.kind));
                f.reason = r.message;
                report.failures.append(f);
                if (r.kind == JsErrorKind::Timeout || r.kind == JsErrorKind::Memory) {
                    // Poisoned budget/engine: abort the remaining cascade.
                    report.engineAborted = true;
                    break;
                }
                continue; // syntax/exception: other fields still calculate
            }
            // rc=false: the script rejected the event — keep the committed value.
            if (!r.rc) {
                FieldJsFailure f;
                f.fieldName = name;
                f.kind = QStringLiteral("rejected");
                f.reason = QStringLiteral("the script set event.rc = false; the value was not recalculated");
                report.failures.append(f);
                continue;
            }
            if (!r.hasValue)
                continue; // script left event.value unset — value stays as-is

            // Write event.value to /V (TextBox only — the only type a
            // calculated field can honestly hold in Phase 1) and refresh the
            // snapshot so later /CO entries compute on the new values.
            if (field->GetType() != PoDoFo::PdfFieldType::TextBox) {
                FieldJsFailure f;
                f.fieldName = name;
                f.kind = QStringLiteral("engine");
                f.reason = QStringLiteral("calculated field type cannot hold a computed value in Phase 1");
                report.failures.append(f);
                continue;
            }
            const bool written = writeFieldValue(doc, name, r.value);
            if (!written) {
                FieldJsFailure f;
                f.fieldName = name;
                f.kind = QStringLiteral("engine");
                f.reason = QStringLiteral("could not write the computed value to /V");
                report.failures.append(f);
                continue;
            }
            sandbox.updateFieldValue(name, r.value);
            ++report.calculated;
        }
    } catch (const PoDoFo::PdfError& e) {
        FieldJsFailure f;
        f.kind = QStringLiteral("engine");
        f.reason = pdfErrorText(e);
        report.failures.append(f);
        report.engineAborted = true;
    }
    return report;
}

// ── Format (display-only) ────────────────────────────────────────────────────

QString FormJsRunner::formatForDisplay(PoDoFo::PdfMemDocument& doc,
                                       const PoDoFo::PdfField& field,
                                       FieldJsFailure* failure,
                                       int eventDeadlineMs)
{
    const QString name = QString::fromStdString(field.GetFullName());
    if (failure) *failure = {};
    if (!executionEnabledFlag()) {
        if (failure) {
            failure->fieldName = name;
            failure->kind = QStringLiteral("engine");
            failure->reason = QStringLiteral("form script execution is disabled in this build/session");
        }
        return {};
    }

    QString script;
    QString why;
    if (!extractActionScript(field, 'F', &script, &why)) {
        // No format script is the common, non-failure case (the display layer
        // shows the raw value).
        Q_UNUSED(why);
        return {};
    }

    FormJsSandbox sandbox;
    if (!sandbox.isValid() || !sandbox.installShim(nullptr)) {
        if (failure) {
            failure->fieldName = name;
            failure->kind = QStringLiteral("engine");
            failure->reason = QStringLiteral("quickjs runtime is unavailable in this build");
        }
        return {};
    }
    sandbox.setFieldValues(collectFieldValues(doc));

    const JsEvalResult r = sandbox.runEvent(script, name, QStringLiteral("Format"),
                                            readValue(field), eventDeadlineMs);
    if (!r.ok) {
        if (failure) {
            failure->fieldName = name;
            failure->kind = QLatin1String(kindString(r.kind));
            failure->reason = r.message;
        }
        return {};
    }
    // Display result — /V is never written (formatting is presentation).
    return r.hasValue ? r.value : QString();
}

} // namespace gp::formjs
