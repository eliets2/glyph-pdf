// SPDX-License-Identifier: Apache-2.0
// formjs Phase-D adversarial suite (review lane feat/formjs-review).
//
// Threat model under test (docs/audit/FORMJS-THREAT-MODEL-2026-09-24.md —
// written as attack stories): every document is hostile. The document author
// controls field names, field values, and every /AA script. This suite attacks
// the surface the W1/JS-01 work did not cover, as an adversary would:
//
//   ISOLATION    — the engine global surface is EXACTLY the documented shim
//                  set; no host filesystem/network/process capability is
//                  reachable through any chain (Function constructor, indirect
//                  eval, async continuations, prototype walks).
//   LIMITS       — every budget attacked: recursion/stack, gradual allocation
//                  under the cap, regex backtracking, stacked budgets across a
//                  cascade, audit-sink flood.
//   RE-ENTRANCY  — the cascade cannot be spun: self-referential /CO, dangling
//                  /CO, a hostile setter on the snapshot global, and the
//                  characterized (deferred F-1) cross-event tamper window.
//   CRASH SAFETY — a malformed-script battery terminates honestly classified,
//                  never crashes or wedges the runtime.
//   HOST API /
//   POLICY       — the raw hostile text a script can place in failure reasons
//                  (the premise of PGR-35), and the kill-switch over all four
//                  engine entries.
//
// Revert directions: reverting the whole-operation deadline (R05/JS-01) hangs
// the limit and re-entrancy tests (harness timeout fails them); adding any
// shim global without updating the surface pin fails
// shimSurfaceIsExactlyTheDocumentedSet.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <functional>
#include <map>

#include <podofo/podofo.h>
#include "core/AppContext.h"
#include "core/FormStaleFieldTracker.h"
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "engines/formjs/FormJsSandbox.h"
#include "engines/formjs/FormJsRunner.h"
#include "modes/FormFieldPropertiesPanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QPdfWriter>
#include <QPainter>
#include <QUndoStack>

#ifdef GetObject
#undef GetObject
#endif

using gp::formjs::FormJsSandbox;
using gp::formjs::FormJsRunner;
using gp::formjs::CascadeReport;
using gp::formjs::FieldJsFailure;
using gp::formjs::JsEvalResult;

namespace {

// Runs `code` in a fresh shim-installed sandbox and returns the classified
// result. Every hostile probe in this suite goes through here (or through the
// cascade) — never through a hand-rolled engine.
JsEvalResult runHostile(const QString& code, const QString& value = {},
                        int deadlineMs = 150)
{
    FormJsSandbox s;
    QString err;
    if (!s.installShim(&err))
        qFatal("runHostile: shim install failed: %s", err.toUtf8().constData());
    return s.runEvent(code, QStringLiteral("probe"), QStringLiteral("Calculate"),
                      value, deadlineMs);
}

// The deadline the suite grants a single hostile evaluation: generous enough
// for engine jitter, far under any human-observable stall.
constexpr int kHostileDeadlineMs = 150;

} // namespace

class TestFormJsAdversarial : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    // ── Cascade fixture ──────────────────────────────────────────────────────
    QString makeFormPdf(const QString& name,
                        const QStringList& fieldNames,
                        const QMap<QString, QString>& calcScripts,
                        const QStringList& coOrder,
                        const QMap<QString, QString>& keystrokeScripts = {},
                        const QMap<QString, QString>& formatScripts = {},
                        const QMap<QString, QString>& initialValues = {})
    {
        const QString base = m_dir.path() + "/" + name + "-base.pdf";
        {
            QPdfWriter writer(base);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setResolution(72);
            QPainter p(&writer);
            p.drawText(80, 100, QStringLiteral("formjs adversarial fixture"));
            p.end();
        }
        const QString path = m_dir.path() + "/" + name;
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(base.toUtf8().constData());
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(0);
            std::map<QString, PoDoFo::PdfReference> refs;
            double y = 640;
            for (const QString& n : fieldNames) {
                const PoDoFo::Rect rect(100, y, 200, 16);
                auto& field = page.CreateField<PoDoFo::PdfTextBox>(n.toStdString(), rect);
                const auto itInit = initialValues.find(n);
                if (itInit != initialValues.end())
                    dynamic_cast<PoDoFo::PdfTextBox*>(&field)->SetText(
                        PoDoFo::PdfString(itInit.value().toStdString()));
                const auto it = calcScripts.find(n);
                const auto itK = keystrokeScripts.find(n);
                const auto itF = formatScripts.find(n);
                if ((it != calcScripts.end() && !it.value().isEmpty())
                    || (itK != keystrokeScripts.end() && !itK.value().isEmpty())
                    || (itF != formatScripts.end() && !itF.value().isEmpty())) {
                    PoDoFo::PdfDictionary aa;
                    if (it != calcScripts.end() && !it.value().isEmpty()) {
                        PoDoFo::PdfDictionary action;
                        action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                        action.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(it.value().toStdString()));
                        aa.AddKey(PoDoFo::PdfName("C"), action);
                    }
                    if (itK != keystrokeScripts.end() && !itK.value().isEmpty()) {
                        PoDoFo::PdfDictionary action;
                        action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                        action.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(itK.value().toStdString()));
                        aa.AddKey(PoDoFo::PdfName("K"), action);
                    }
                    if (itF != formatScripts.end() && !itF.value().isEmpty()) {
                        PoDoFo::PdfDictionary action;
                        action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                        action.AddKey(PoDoFo::PdfName("JS"), PoDoFo::PdfString(itF.value().toStdString()));
                        aa.AddKey(PoDoFo::PdfName("F"), action);
                    }
                    field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aa);
                }
                refs[n] = (field.GetObject)().GetIndirectReference();
                y -= 40;
            }
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) qFatal("fixture: no AcroForm");
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

    static PoDoFo::PdfMemDocument loadDoc(const QString& path)
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        return doc;
    }

    // In-memory /V read (the cascade mutates the in-memory document; a reload
    // from disk would read pre-cascade state).
    static QString docFieldValue(const PoDoFo::PdfMemDocument& doc, const QString& name)
    {
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return {};
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) == name) {
                const PoDoFo::PdfDictionary& dict = (field.GetObject)().GetDictionary();
                if (const PoDoFo::PdfObject* v = dict.FindKey("V"); v && v->IsString())
                    return QString::fromUtf8(v->GetString().GetString().data(),
                                             static_cast<qsizetype>(v->GetString().GetString().size()));
                return {};
            }
        }
        return {};
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // ══ ISOLATION ══════════════════════════════════════════════════════════

    // Attack story: "find anything on the global object that reaches the
    // host." The shim installs a KNOWN set of globals and nothing else — the
    // delta of globalThis own properties before/after installShim must be
    // exactly the documented surface (AFormShim.h). Any future addition must
    // update this pin, so the host API surface cannot grow silently.
    void shimSurfaceIsExactlyTheDocumentedSet()
    {
        FormJsSandbox s;
        QVERIFY(s.isValid());
        auto names = [&s]() {
            QString out;
            if (!s.evalHelper(
                    "Object.getOwnPropertyNames(globalThis).sort().join('\\u0001')",
                    &out, nullptr))
                return QStringList{};
            return out.split(QLatin1Char('\u0001'));
        };
        const QStringList before = names();
        QVERIFY2(before.size() > 10, "could not snapshot the bare global surface");
        QVERIFY(s.installShim(nullptr));
        const QStringList after = names();
        QVERIFY2(after.size() > before.size(), "the shim must install its surface");

        QSet<QString> delta;
        for (const QString& n : after)
            if (!before.contains(n)) delta.insert(n);

        // PGR-39 FIX: the shim body now lives inside an IIFE — every internal
        // helper is a closure binding and the ONLY globalThis additions are
        // the documented surface (AFormShim.h). Any future addition must
        // extend that pin deliberately.
        const QStringList documented{
            QStringLiteral("AFDate_Format"), QStringLiteral("AFDate_FormatEx"),
            QStringLiteral("AFDate_Keystroke"), QStringLiteral("AFDate_KeystrokeEx"),
            QStringLiteral("AFExtractNums"), QStringLiteral("AFMakeArrayFromList"),
            QStringLiteral("AFMakeNumber"), QStringLiteral("AFMergeChange"),
            QStringLiteral("AFNumber_Format"), QStringLiteral("AFNumber_Keystroke"),
            QStringLiteral("AFNumber_Parse"), QStringLiteral("AFPercent_Format"),
            QStringLiteral("AFPercent_Keystroke"), QStringLiteral("AFParseDateEx"),
            QStringLiteral("AFSimple"), QStringLiteral("AFSimple_Calculate"),
            QStringLiteral("AFTime_Format"), QStringLiteral("AFTime_FormatEx"),
            QStringLiteral("AFTime_Keystroke"), QStringLiteral("AFTime_KeystrokeEx"),
            QStringLiteral("__gpBeginEvent"), QStringLiteral("__gpEndEvent"),
            QStringLiteral("__gpBlocked"), QStringLiteral("__gpLog"),
            QStringLiteral("__gpNowProvider"), QStringLiteral("__gpSetFixedNowFromIso"),
            QStringLiteral("app"), QStringLiteral("color"), QStringLiteral("console"),
            QStringLiteral("doc"), QStringLiteral("getField"), QStringLiteral("util"),
        };
        // PGR-39 (fixed): these 9 internals are closure bindings — they must
        // NOT appear on globalThis anymore.
        const QStringList leakedInternals{
            QStringLiteral("__blockedVerb"), QStringLiteral("__createDateActions"),
            QStringLiteral("__createScandData"), QStringLiteral("__mkTargetName"),
            QStringLiteral("__parseDate"), QStringLiteral("__printf"),
            QStringLiteral("__printd"), QStringLiteral("__scand"),
            QStringLiteral("__tryToGuessDate"),
        };
        QSet<QString> expected(documented.cbegin(), documented.cend());
        for (const QString& leaked : leakedInternals)
            QVERIFY2(!delta.contains(leaked),
                     qPrintable(leaked + QStringLiteral(" leaked onto globalThis")));
        QCOMPARE(delta, expected);
    }

    // Attack story: "reach the host through the Function constructor, an
    // indirect eval, or the async-function machinery." All of them compile and
    // run inside the SAME bare engine — the primitives simply do not exist at
    // any scope depth.
    void noHostCapabilityReachableThroughAnyChain()
    {
        JsEvalResult r = runHostile(QStringLiteral(
            "const names = [\"read\", \"readFile\", \"write\", \"writeFile\", \"remove\","
            "               \"os\", \"std\", \"popen\", \"system\", \"exec\", \"fetch\","
            "               \"XMLHttpRequest\", \"Socket\", \"connect\", \"process\","
            "               \"require\", \"module\", \"Worker\", \"importScripts\","
            "               \"setTimeout\", \"setInterval\", \"print\", \"load\","
            "               \"quit\", \"scriptArgs\"];"
            "const missing = names.filter(n => globalThis[n] !== undefined);"
            "const viaCtor = new Function(\"return typeof read\")();"
            "const viaIndirect = (0, eval)(\"typeof writeFile\");"
            // The async-function machinery compiles in the same bare engine:
            // an async body runs synchronously up to its first await, so a
            // host capability WOULD be observable here if one existed.
            "const AsyncFunctionCtor = Object.getPrototypeOf(async function(){}).constructor;"
            "new AsyncFunctionCtor(\"globalThis.__pwned = typeof popen\")();"
            "event.value = missing.length + \"|\" + viaCtor + \"|\" + viaIndirect + \"|\" + globalThis.__pwned;"));
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(r.value, QStringLiteral("0|undefined|undefined|undefined"));
    }

    // Attack story: "schedule the hostile work in a promise job; the host's
    // deadline then measures an empty eval while my loop keeps running." The
    // host NEVER pumps promise jobs — an async continuation does not run after
    // JS_Eval returns, so async is a dead end, not a deadline escape.
    void promiseJobsNeverRunAfterEvalReturns()
    {
        FormJsSandbox s;
        QVERIFY(s.installShim(nullptr));
        const JsEvalResult r = s.runEvent(
            QStringLiteral(
                "globalThis.__asyncRan = false;"
                "Promise.resolve().then(() => { globalThis.__asyncRan = true; for (;;) {} });"
                "(async () => { await 0; globalThis.__asyncRan = true; })();"),
            QStringLiteral("probe"), QStringLiteral("Calculate"), {}, kHostileDeadlineMs);
        QVERIFY2(r.ok, qPrintable(r.message));
        QString asyncRan;
        QVERIFY(s.evalHelper("String(globalThis.__asyncRan)", &asyncRan, nullptr));
        // The probe itself seeded `false`; a value of `true` would mean a job
        // ran (and its spin loop would have tripped the deadline instead).
        QCOMPARE(asyncRan, QStringLiteral("false"));
    }

    // Attack story: "mistake a blocked verb for the real host API, or forge a
    // return value that says the egress went through." Every blocked verb is a
    // pure JS closure, returns false, and only appends to the audit sink.
    void egressVerbsArePureJsClosuresWithHonestReturns()
    {
        JsEvalResult r = runHostile(QStringLiteral(
            "const ret = ["
            "    doc.submitForm(\"\"), doc.mailDoc({}), doc.exportData(\"/tmp/x\"),"
            "    app.launchURL(\"https://x\"), app.mailme({}), app.execDialog({}),"
            "    app.media({})"
            "];"
            "const alerted = app.alert(\"x\");"
            "event.value = (ret.every(v => v === false) && alerted === true);"));
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(r.value, QStringLiteral("true"));
        QCOMPARE(r.blocked.size(), 7);
    }

    // ══ LIMITS ═════════════════════════════════════════════════════════════

    // Infinite recursion must hit the 1 MiB interpreter-stack cap and be
    // classified Memory (an honest reason), within bounded time.
    void deepRecursionClassifiedAndTerminates()
    {
        QElapsedTimer t;
        t.start();
        const JsEvalResult r = runHostile(
            QStringLiteral("function f(n) { return 1 + f(n + 1); } f(0);"));
        QVERIFY2(t.elapsed() < 5000,
                 qPrintable(QStringLiteral("recursion probe ran %1 ms").arg(t.elapsed())));
        QVERIFY2(!r.ok, "infinite recursion must not succeed");
        QVERIFY2(r.kind == gp::formjs::JsErrorKind::Memory,
                 qPrintable(QStringLiteral("kind=%1 msg=%2").arg(int(r.kind)).arg(r.message)));
    }

    // Many SMALL allocations, each far under the cap, exhaust the heap just
    // the same — the cap bounds total engine memory, not per allocation.
    void allocationLoopUnderCapTerminatesClassified()
    {
        QElapsedTimer t;
        t.start();
        const JsEvalResult r = runHostile(
            QStringLiteral("let a = []; for (;;) { a.push(new Array(64).fill('x')); }"));
        QVERIFY2(t.elapsed() < 5000,
                 qPrintable(QStringLiteral("exhaustion probe ran %1 ms").arg(t.elapsed())));
        QVERIFY2(!r.ok, "gradual exhaustion must not succeed");
        QVERIFY2(r.kind == gp::formjs::JsErrorKind::Memory
                 || r.kind == gp::formjs::JsErrorKind::Timeout,
                 qPrintable(QStringLiteral("kind=%1").arg(int(r.kind))));
    }

    // Catastrophic backtracking ("ReDoS") burns CPU inside one native regex
    // call — quickjs-ng's regexp engine polls the interrupt handler, so a
    // FAILING match (the trailing 'b' forces the backtracking wall) aborts at
    // the deadline exactly like an interpreter loop.
    void reDoSPatternAbortsAtDeadline()
    {
        QElapsedTimer t;
        t.start();
        const JsEvalResult r = runHostile(
            QStringLiteral("'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaab'.replace(/(a+)+$/, 'x');"
                           "event.value = 'done';"));
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 2000, qPrintable(QStringLiteral("ReDoS probe ran %1 ms").arg(ms)));
        QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    }

    // PGR-40: native builtins that scan SPARSE arrays element-by-element
    // (indexOf/includes/lastIndexOf/flat/sort/join) never poll the interrupt
    // handler and never allocate per hole, so neither the deadline nor the
    // memory cap can stop them. Measured on the pinned quickjs-ng 0.15.0 with
    // this exact sandbox contract (see docs/audit/evidence-formjs-2026-09-23/qjs-probe.c):
    //   Array(2^31).indexOf  → 44.8 s   includes → 53.2 s   flat → 61.8 s
    // One expression in a Format/Calculate script freezes the UI thread
    // unkillably. Upstream quickjs-ng added interrupt checks to the array
    // builtins after 0.15.0; the MSYS2 package is still 0.15.0-1, so the
    // remediation is the dependency bump (threat model, PGR-40).
    // This pin AUTO-ARMS once the package carries the fix: on a fixed engine
    // the 150 ms deadline fires (Timeout, fast); on 0.15.0 the probe returns
    // ok past the deadline and the pin skips with the finding reference.
    void nativeSparseArrayScansAbideTheDeadline()
    {
        QElapsedTimer t;
        t.start();
        const JsEvalResult r = runHostile(
            QStringLiteral("Array(16777216).includes('x'); event.value = 'done';"), {}, 150);
        const qint64 ms = t.elapsed();
        if (r.ok && ms > 140)
            QSKIP("PGR-40: the pinned quickjs-ng 0.15.0 cannot interrupt native "
                  "sparse-array scans (upstream added array-method interrupt checks "
                  "after 0.15.0; MSYS2 still packages 0.15.0-1). This pin asserts the "
                  "deadline bound automatically once the dependency is bumped. See "
                  "docs/audit/FORMJS-THREAT-MODEL-2026-09-24.md and docs/audit/evidence-formjs-2026-09-23/ (probe + captured output).");
        QVERIFY2(ms < 2500, qPrintable(QStringLiteral("native scan ran %1 ms").arg(ms)));
        QCOMPARE(r.kind, gp::formjs::JsErrorKind::Timeout);
    }

    // Stacked budgets: a /CO full of spinners cannot exceed the cascade budget
    // by more than one per-event overshoot, no matter how many entries it has.
    void cascadeBudgetBoundsHostileDocuments()
    {
        const QString infinite = QStringLiteral("for (;;) {}");
        QStringList fields;
        QMap<QString, QString> scripts;
        QStringList order;
        for (int i = 0; i < 6; ++i) {
            fields << QStringLiteral("spin%1").arg(i);
            scripts.insert(QStringLiteral("spin%1").arg(i), infinite);
            order << QStringLiteral("spin%1").arg(i);
        }
        const QString path = makeFormPdf(QStringLiteral("budget.pdf"), fields, scripts, order);
        QVERIFY(!path.isEmpty());

        QElapsedTimer t;
        t.start();
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 100, 500);
        const qint64 ms = t.elapsed();
        // 500 ms cascade budget + at most one 100 ms event overshoot + slop.
        QVERIFY2(ms < 2500, qPrintable(QStringLiteral("cascade ran %1 ms").arg(ms)));
        QVERIFY(rep.engineAborted);
        QVERIFY2(!rep.failures.isEmpty(), "the hostile cascade must disclose failures");
    }

    // The audit log sink is a host transfer surface too: flooding app.alert
    // with a near-cap string is refused at the transfer cap (honest Memory),
    // and the flood never reaches the host's log list.
    void logFloodRefusedByTransferCap()
    {
        FormJsSandbox s;
        QVERIFY(s.installShim(nullptr));
        const JsEvalResult r = s.runEvent(
            QStringLiteral("app.alert('A'.repeat(5 * 1024 * 1024)); event.value = 1;"),
            QStringLiteral("probe"), QStringLiteral("Calculate"), {}, 1000);
        QVERIFY2(!r.ok, "a >4 MiB log flood must be refused at the host transfer cap");
        QCOMPARE(r.kind, gp::formjs::JsErrorKind::Memory);
        QVERIFY2(r.logs.isEmpty(), "a refused flood must not be delivered to the host");
    }

    // ══ RE-ENTRANCY / CASCADE STRUCTURE ════════════════════════════════════

    // Attack story: "list my field 5000 times in /CO so every save runs my
    // script 5000 times." Each field runs AT MOST ONCE per cascade (the pdf.js
    // _isCalculating pin) and the host stays responsive.
    void selfReferentialMassiveCoTerminatesOnce()
    {
        const QString script = QStringLiteral(
            "event.value = (getField('a') ? Number(getField('a').value) : 0) + 1;");
        const QString path = makeFormPdf(
            QStringLiteral("dupco.pdf"),
            { QStringLiteral("a"), QStringLiteral("b") },
            { { QStringLiteral("b"), script } },
            QStringList{});
        QVERIFY(!path.isEmpty());
        try {
            PoDoFo::PdfMemDocument doc = loadDoc(path);
            auto* acroForm = doc.GetAcroForm();
            PoDoFo::PdfReference bRef;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i)
                if (QString::fromStdString(acroForm->GetFieldAt(i).GetFullName()) == QStringLiteral("b"))
                    bRef = (acroForm->GetFieldAt(i).GetObject)().GetIndirectReference();
            PoDoFo::PdfArray co;
            for (int i = 0; i < 5000; ++i) co.Add(bRef);
            acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), co);
            doc.Save((m_dir.path() + "/dupco-co.pdf").toUtf8().constData());
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(e.what()));
        }

        QElapsedTimer t;
        t.start();
        PoDoFo::PdfMemDocument doc = loadDoc(m_dir.path() + "/dupco-co.pdf");
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QVERIFY2(t.elapsed() < 3000,
                 qPrintable(QStringLiteral("5000-entry /CO ran %1 ms").arg(t.elapsed())));
        QCOMPARE(rep.attempted, 1); // once per cascade — never 5000
    }

    // A dangling /CO reference is skipped; honest fields around it still run.
    void danglingCoRefsAreSkippedAndOthersCalculate()
    {
        const QString script = QStringLiteral(
            "event.value = (getField('a') ? Number(getField('a').value) : 0) + 1;");
        const QString path = makeFormPdf(
            QStringLiteral("dangling.pdf"),
            { QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c") },
            { { QStringLiteral("b"), script }, { QStringLiteral("c"), script } },
            { QStringLiteral("b"), QStringLiteral("c") });
        QVERIFY(!path.isEmpty());
        try {
            PoDoFo::PdfMemDocument doc = loadDoc(path);
            auto* acroForm = doc.GetAcroForm();
            const PoDoFo::PdfObject* coObj = acroForm->GetDictionary().FindKey("CO");
            PoDoFo::PdfArray co(coObj->GetArray());
            co.Add(PoDoFo::PdfReference(999999u, 0)); // dangling
            acroForm->GetDictionary().AddKey(PoDoFo::PdfName("CO"), co);
            doc.Save((m_dir.path() + "/dangling-co.pdf").toUtf8().constData());
        } catch (const PoDoFo::PdfError& e) {
            QFAIL(qPrintable(e.what()));
        }
        PoDoFo::PdfMemDocument doc = loadDoc(m_dir.path() + "/dangling-co.pdf");
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QVERIFY(!rep.engineAborted);
        QCOMPARE(rep.attempted, 2);
        QCOMPARE(docFieldValue(doc, QStringLiteral("b")), QStringLiteral("1"));
        QCOMPARE(docFieldValue(doc, QStringLiteral("c")), QStringLiteral("1"));
    }

    // Attack story: "install a spinning setter on the snapshot global so the
    // host wedges when it refreshes values." The refresh runs under its own
    // budget, fails honestly, and the cascade aborts with the skipped-field
    // disclosure (R05/JS-01) — the field's own committed write still stands.
    void hostileSnapshotSetterAbortsCascadeHonest()
    {
        const QString poisonSetter = QStringLiteral(
            "Object.defineProperty(globalThis, '__gpFieldValues', "
            "{ get() { return {}; }, set(v) { for (;;) {} }, configurable: true }); "
            "event.value = 'poisoned';");
        const QString path = makeFormPdf(
            QStringLiteral("setterstorm.pdf"),
            { QStringLiteral("p"), QStringLiteral("q") },
            { { QStringLiteral("p"), poisonSetter },
              { QStringLiteral("q"), QStringLiteral("event.value = 1;") } },
            { QStringLiteral("p"), QStringLiteral("q") });
        QVERIFY(!path.isEmpty());
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        QElapsedTimer t;
        t.start();
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 200, 1000);
        QVERIFY2(t.elapsed() < 4000,
                 qPrintable(QStringLiteral("setter-storm cascade ran %1 ms").arg(t.elapsed())));
        QVERIFY2(rep.engineAborted, "the wedged snapshot refresh must abort the cascade");
        bool hasRefreshFailure = false;
        for (const auto& f : rep.failures)
            if (f.reason.contains(QLatin1String("snapshot")))
                hasRefreshFailure = true;
        QVERIFY2(hasRefreshFailure, "the refresh failure must be disclosed with its reason");
        QCOMPARE(docFieldValue(doc, QStringLiteral("p")), QStringLiteral("poisoned"));
    }

    // CHARACTERIZATION (deferred finding F-1 — see the threat model): a script
    // that ends WITHOUT a committed write leaves its rewiring of the shared
    // sandbox globals in place, so the NEXT event's inputs can be tampered.
    // This is platform-inherent for shared-context cascades (Acrobat and pdf.js
    // share it); the deferred fix is a fresh runtime per event. This pin makes
    // the window a CONSCIOUS decision: adopting per-event runtimes flips this
    // test deliberately.
    void crossEventTamperWindowIsCharacterized()
    {
        const QString tamper = QStringLiteral(
            "globalThis.__gpFieldValues = { b: '999' }; event.rc = false;");
        const QString reader = QStringLiteral(
            "event.value = getField('b') ? getField('b').value : 'clean';");
        const QString path = makeFormPdf(
            QStringLiteral("tamper.pdf"),
            { QStringLiteral("t"), QStringLiteral("b"), QStringLiteral("r") },
            { { QStringLiteral("t"), tamper }, { QStringLiteral("r"), reader } },
            { QStringLiteral("t"), QStringLiteral("r") });
        QVERIFY(!path.isEmpty());
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        // t: rejected (rc=false) — no write, no refresh. r reads b through the
        // TAMPERED snapshot: the characterized window.
        QCOMPARE(docFieldValue(doc, QStringLiteral("r")), QStringLiteral("999"));
        bool rejected = false;
        for (const auto& f : rep.failures)
            if (f.fieldName == QStringLiteral("t") && f.kind == QStringLiteral("rejected"))
                rejected = true;
        QVERIFY(rejected);
    }

    // ══ CRASH SAFETY — malformed-script battery ════════════════════════════

    // Every probe must (a) terminate quickly, (b) return a classified result,
    // (c) leave the host alive. No assertion on WHICH honest classification a
    // given malformation earns beyond ok==false for the failing ones.
    void malformedScriptBatteryTerminatesClassified()
    {
        const QStringList battery{
            QStringLiteral("({}).x.y.z()"),                                    // property of undefined
            QStringLiteral("'\\ud800' + '\\udfff'"),                           // lone surrogates
            QStringLiteral("eval('eval(\\'eval(\"{\')')"),                     // nested eval syntax junk
            QStringLiteral("throw { toString() { throw 2; } };"),              // hostile non-Error throw
            QStringLiteral("throw new Proxy({}, { get() { return 1; } });"),   // Proxy throw
            QStringLiteral("throw null;"),                                     // null throw
            QStringLiteral("0.1e9999"),                                        // numeric overflow literal
            QStringLiteral("JSON.parse('{\"a\":' + '['.repeat(64) + '}')"),    // JSON parse junk
            QStringLiteral("event.value = { valueOf() { return {}; } };"),     // junk value
            QStringLiteral("(function(){ return arguments.callee; })()()()"),  // call junk
            QStringLiteral("new Array(-1)"),                                   // RangeError
            QStringLiteral("'\\u2028\\u2029'"),                                // line-separator strings
            QStringLiteral("Object.defineProperty(globalThis, 'event', { get() { throw 0; } });"),
        };
        for (int i = 0; i < battery.size(); ++i) {
            QElapsedTimer t;
            t.start();
            const JsEvalResult r = runHostile(battery.at(i));
            const qint64 ms = t.elapsed();
            QVERIFY2(ms < 3000,
                     qPrintable(QStringLiteral("battery[%1] ran %2 ms").arg(i).arg(ms)));
            QVERIFY2(r.kind != gp::formjs::JsErrorKind::None || r.ok,
                     qPrintable(QStringLiteral("battery[%1] unclassified").arg(i)));
        }
    }

    // Hostile field NAMES cross the whole pipeline (PDF name → QVariantMap →
    // JSON embed → engine → read). Markup, quotes, backslashes and Unicode
    // must round-trip byte-exactly — the JSON embed must be an injection-proof
    // channel, not a JS-source concatenation.
    void hostileFieldNamesRoundTripThroughTheEngine()
    {
        const QString nasty = QStringLiteral("a<b>&\"'\\/_x\u00e9\u4e2d\U0001F600");
        FormJsSandbox s;
        QVERIFY(s.installShim(nullptr));
        QVariantMap vals;
        vals.insert(nasty, QStringLiteral("V<>\"'&"));
        QString err;
        QVERIFY2(s.setFieldValues(vals, &err), qPrintable(err));
        // Embed the name as a quoted JS string literal the same way the host
        // does (a JSON array element, brackets stripped).
        const QByteArray nameJson = QJsonDocument(QJsonArray{ nasty }).toJson(QJsonDocument::Compact);
        const QString nameLiteral = QString::fromUtf8(nameJson.mid(1, nameJson.size() - 2));
        QString out;
        QVERIFY2(s.evalHelper(QStringLiteral("getField(%1).valueAsString").arg(nameLiteral),
                              &out, &err),
                 qPrintable(err));
        QCOMPARE(out, QStringLiteral("V<>\"'&"));
    }

    // ══ HOST API / POLICY ══════════════════════════════════════════════════

    // PIN (the premise of PGR-35): a hostile script's exception text reaches
    // the host VERBATIM in the failure reason — markup included. The UI layer
    // must therefore render these strings with a plain text format.
    void scriptTextReachesFailureReasonsVerbatim()
    {
        const JsEvalResult r = runHostile(
            QStringLiteral("throw new Error('<img src=//x><b>pwn</b>');"));
        QVERIFY2(!r.ok, "the hostile throw must fail the event");
        QVERIFY2(r.message.contains(QLatin1String("<img src=//x><b>pwn</b>")),
                 qPrintable("hostile markup must reach the reason verbatim: " + r.message));
    }

    // PGR-35 (the fix): the properties panel surfaces document-derived text —
    // a keystroke script's failure reason and a format script's OUTPUT — so
    // every disclosure label renders with Qt::PlainText. On the pre-fix panel
    // (Qt::AutoText) Qt's mightBeRichText heuristic renders hostile markup:
    // UI spoofing at minimum, local-file/share <img> loads at worst.
    void pgr35PanelDisclosuresRenderAsPlainText()
    {
        const QString path = makeFormPdf(
            QStringLiteral("pgr35.pdf"),
            { QStringLiteral("gate"), QStringLiteral("fmt") },
            {},
            QStringList{},
            { { QStringLiteral("gate"),
                QStringLiteral("throw new Error('<img src=//x><b>pwn</b>');") } },
            { { QStringLiteral("fmt"),
                QStringLiteral("event.value = '<h1>spoof</h1><img src=//y>';") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<gp::FormStaleFieldTracker>();
        gp::FormFieldPropertiesPanel panel(&ctx);
        panel.show(); // offscreen

        // 1. Keystroke rejection path: the script's error message is the
        //    disclosure body — it must stay plain text and keep the payload
        //    as SOURCE (visible to the user, never rendered).
        panel.setFieldName(QStringLiteral("gate"));
        auto* edit = panel.findChild<QLineEdit*>(QStringLiteral("defaultValueEdit"));
        auto* status = panel.findChild<QLabel*>(QStringLiteral("keystrokeStatus"));
        QVERIFY(edit);
        QVERIFY(status);
        QTest::keyClicks(edit, QStringLiteral("x"));
        QVERIFY2(status->isVisible(), "the keystroke rejection must be disclosed");
        QCOMPARE(status->textFormat(), Qt::PlainText);
        QVERIFY2(status->text().contains(QLatin1String("<img src=//x><b>pwn</b>")),
                 qPrintable("payload must survive verbatim as plain text: "
                            + status->text()));

        // 2. Format display preview: the SCRIPT'S OUTPUT is shown verbatim —
        //    the sharpest PGR-35 site.
        panel.setFieldName(QStringLiteral("fmt"));
        auto* preview = panel.findChild<QLabel*>(QStringLiteral("displayPreview"));
        QVERIFY(preview);
        QVERIFY2(preview->isVisible(), "the format preview must be shown");
        QCOMPARE(preview->textFormat(), Qt::PlainText);
        QVERIFY2(preview->text().contains(QLatin1String("<h1>spoof</h1>")),
                 qPrintable("script output must survive verbatim as plain text: "
                            + preview->text()));
    }

    // PGR-36 (the fix): AFSimple_Calculate's operation membership must be an
    // OWN-property test. Pre-fix, `op in actions` walked the prototype chain,
    // so cFunction "toString"/"constructor"/"hasOwnProperty" invoked the
    // inherited Object function and silently wrote garbage to /V instead of
    // the honest TypeError.
    void afSimpleCalculateInheritedOpsRefused()
    {
        for (const QString bad : { QStringLiteral("toString"),
                                   QStringLiteral("constructor"),
                                   QStringLiteral("hasOwnProperty"),
                                   QStringLiteral("valueOf") }) {
            const JsEvalResult r = runHostile(
                QStringLiteral("AFSimple_Calculate('%1', new Array('a'));").arg(bad));
            QVERIFY2(!r.ok, qPrintable(bad + QStringLiteral(" must be refused")));
            QCOMPARE(r.kind, gp::formjs::JsErrorKind::Exception);
            QVERIFY2(r.message.contains(QLatin1String("Invalid function in AFSimple_Calculate")),
                     qPrintable(r.message));
        }
        // Cascade integration: the refused op is a field-attributed failure
        // and the field keeps its committed /V (no garbage write).
        const QString path = makeFormPdf(
            QStringLiteral("pgr36.pdf"),
            { QStringLiteral("a"), QStringLiteral("v") },
            { { QStringLiteral("v"),
                QStringLiteral("AFSimple_Calculate('toString', new Array('a'));") } },
            { QStringLiteral("v") });
        QVERIFY(!path.isEmpty());
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QCOMPARE(docFieldValue(doc, QStringLiteral("v")), QString());
        bool exceptionFailure = false;
        for (const auto& f : rep.failures)
            if (f.fieldName == QStringLiteral("v") && f.kind == QStringLiteral("exception"))
                exceptionFailure = true;
        QVERIFY2(exceptionFailure, "the inherited-op refusal must be disclosed");
    }

    // PGR-37 (the fix): a script that sets event.value to NaN or ±Infinity
    // produced hasValue=true with a JSON-null payload — the cascade then
    // wrote "" and silently WIPED the committed /V. Non-finite numbers are
    // now "no usable value": the committed value stands.
    void nanOrInfinityKeepsTheCommittedValue()
    {
        for (const QString expr : { QStringLiteral("0/0"), QStringLiteral("1/0"),
                                    QStringLiteral("-1/0") }) {
            const JsEvalResult r = runHostile(
                QStringLiteral("event.value = %1;").arg(expr));
            QVERIFY2(r.ok, qPrintable(r.message));
            QVERIFY2(!r.hasValue,
                     qPrintable(expr + QStringLiteral(" must not carry a usable value")));
        }
        // Cascade integration: the committed /V survives the hostile compute.
        const QString path = makeFormPdf(
            QStringLiteral("pgr37.pdf"),
            { QStringLiteral("v") },
            { { QStringLiteral("v"), QStringLiteral("event.value = 0/0;") } },
            { QStringLiteral("v") }, {}, {},
            { { QStringLiteral("v"), QStringLiteral("5") } });
        QVERIFY(!path.isEmpty());
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QCOMPARE(docFieldValue(doc, QStringLiteral("v")), QStringLiteral("5"));
    }

    // PGR-38 (the fix): a field literally named "__proto__" vanished from
    // every script's view — the snapshot was embedded as a JS object LITERAL,
    // where the key "__proto__" invokes the inherited setter (a string value
    // is silently dropped) instead of defining an own property as JSON.parse
    // does. JSON.parse now builds the snapshot; the hostile name round-trips
    // like any other.
    void protoFieldNameSurvivesTheSnapshot()
    {
        FormJsSandbox s;
        QVERIFY(s.installShim(nullptr));
        QVariantMap vals;
        vals.insert(QStringLiteral("__proto__"), QStringLiteral("poison"));
        vals.insert(QStringLiteral("plain"), QStringLiteral("ok"));
        QString err;
        QVERIFY2(s.setFieldValues(vals, &err), qPrintable(err));
        QString ownProto;
        QVERIFY(s.evalHelper(
            QStringLiteral("Object.prototype.hasOwnProperty.call(globalThis.__gpFieldValues, "
                           "'__proto__') ? 'own' : 'missing'"),
            &ownProto, &err));
        QCOMPARE(ownProto, QStringLiteral("own"));
        QString value;
        QVERIFY2(s.evalHelper(QStringLiteral("getField('__proto__').value"), &value, &err),
                 qPrintable(err));
        QCOMPARE(value, QStringLiteral("poison"));
        // The map stays a plain dictionary — the embed must not have changed
        // its prototype either.
        QString protoUnchanged;
        QVERIFY(s.evalHelper(
            QStringLiteral("Object.getPrototypeOf(globalThis.__gpFieldValues) === "
                           "Object.prototype ? 'clean' : 'tampered'"),
            &protoUnchanged, nullptr));
        QCOMPARE(protoUnchanged, QStringLiteral("clean"));
    }

    // PGR-38 cascade integration: a hostile document with a field literally
    // named "__proto__" computes on its real value.
    void protoFieldComputesInTheCascade()
    {
        const QString path = makeFormPdf(
            QStringLiteral("pgr38.pdf"),
            { QStringLiteral("__proto__"), QStringLiteral("reader") },
            { { QStringLiteral("reader"),
                QStringLiteral("event.value = getField('__proto__') ? "
                               "getField('__proto__').value + '!' : 'LOST';") } },
            { QStringLiteral("reader") }, {}, {},
            { { QStringLiteral("__proto__"), QStringLiteral("got") } });
        QVERIFY(!path.isEmpty());
        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QCOMPARE(docFieldValue(doc, QStringLiteral("reader")), QStringLiteral("got!"));
    }

    // The kill-switch is the pre-fix disclosure state: ALL FOUR engine entries
    // (cascade, validate, keystroke, format) refuse to run scripts.
    void killSwitchDisablesAllFourEntries()
    {
        const QString script = QStringLiteral("event.value = 'ran';");
        const QString path = makeFormPdf(
            QStringLiteral("killswitch.pdf"),
            { QStringLiteral("k") },
            { { QStringLiteral("k"), script } },
            { QStringLiteral("k") });
        QVERIFY(!path.isEmpty());

        QVERIFY(FormJsRunner::executionGloballyEnabled());
        FormJsRunner::setExecutionGloballyEnabled(false);
        struct Restore {
            ~Restore() { FormJsRunner::setExecutionGloballyEnabled(true); }
        } restore;

        PoDoFo::PdfMemDocument doc = loadDoc(path);
        const CascadeReport rep = FormJsRunner::runCalculateCascade(doc, 250, 1000);
        QCOMPARE(rep.attempted, 0);
        QCOMPARE(rep.calculated, 0);
        QVERIFY(rep.failures.isEmpty()); // nothing ran, nothing to disclose

        const auto v = FormJsRunner::runValidateEvent(doc, QStringLiteral("k"), QStringLiteral("x"));
        QVERIFY(!v.ran);
        const auto k = FormJsRunner::runKeystrokeEvent(doc, QStringLiteral("k"),
                                                       QStringLiteral("a"), QStringLiteral("b"), 1, 1);
        QVERIFY(!k.ran);

        PoDoFo::PdfMemDocument doc2 = loadDoc(path);
        FieldJsFailure failure;
        const QString display = FormJsRunner::formatForDisplay(
            doc2, doc2.GetAcroForm()->GetFieldAt(0), &failure);
        QVERIFY(display.isEmpty());
        QCOMPARE(failure.kind, QStringLiteral("engine"));
    }
};

QTEST_MAIN(TestFormJsAdversarial)
#include "TestFormJsAdversarial.moc"
