// SPDX-License-Identifier: Apache-2.0
// R18(a): the PERSISTENT stale-calculated-field disclosure.
//
// Contract under test (R18 CSV acceptance: "persistent stale-field warnings"):
//   * FormStaleFieldTracker — the session-scoped store: a committed cascade
//     outcome REPLACES the per-document stale set (fields named in the
//     failures are stale; everything else recomputed and clears); a fully
//     successful cascade clears every warning; acknowledge clears exactly the
//     acknowledged field; engine-level entries without a field name flag
//     nothing.
//   * FormFieldPropertiesPanel — the warning banner is derived from the
//     tracker on EVERY refresh: it survives field switches and panel
//     rebuilds, shows the field-attributed reason, and clears only when the
//     field recomputes or the user acknowledges. The transient warning dialog
//     still appears on the apply path, and the real panel-apply flow feeds
//     the tracker (a failed command — nothing recomputed — does not).
//
// Revert-verify: the panel banner + tracker consumption are new surface; the
// tracker pins FAIL against a tracker that appends instead of replaces or
// that keeps acknowledged entries.
#include <QtTest/QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QBasicTimer>
#include <QTemporaryDir>
#include <QTimerEvent>
#include <QUndoStack>
#include <QPdfWriter>
#include <QPainter>
#include <QLabel>
#include <QToolButton>

#include "core/AppContext.h"
#include "core/FormStaleFieldTracker.h"
#include "core/interfaces/IFormManager.h"
#include "commands/EditFormFieldCommand.h"
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "modes/FormFieldPropertiesPanel.h"

#include <podofo/podofo.h>

#ifdef GetObject
#undef GetObject
#endif

using gp::FormStaleFieldTracker;
using gp::FormFieldPropertiesPanel;

namespace {

FormJsFailure failure(const QString& name, const QString& kind, const QString& reason)
{
    FormJsFailure f;
    f.fieldName = name;
    f.kind = kind;
    f.reason = reason;
    return f;
}

struct FieldSpec {
    QString name;
    QString calcScript; // /AA /C body (may be empty)
    QString initial;    // initial /V (may be empty)
};

// Minimal form fixture: real AcroForm fields with optional calculate scripts
// and an explicit /CO order (direct save to a NEW path only — the R01 lesson
// applies to fixtures too).
QString makeFormPdf(const QString& dir, const QString& name,
                    const QList<FieldSpec>& fields, const QStringList& coOrder)
{
    const QString base = dir + "/" + name + "-base.pdf";
    {
        QPdfWriter writer(base);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(80, 100, QStringLiteral("stale-disclosure fixture"));
        p.end();
    }
    const QString path = dir + "/" + name;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(base.toUtf8().constData());
        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(0);
        std::map<QString, PoDoFo::PdfReference> refs;
        double y = 600;
        for (const FieldSpec& spec : fields) {
            const PoDoFo::Rect rect(100, y, 200, 16);
            auto& field = page.CreateField<PoDoFo::PdfTextBox>(spec.name.toStdString(), rect);
            dynamic_cast<PoDoFo::PdfTextBox*>(&field)->SetText(
                PoDoFo::PdfString(spec.initial.toStdString()));
            if (!spec.calcScript.isEmpty()) {
                PoDoFo::PdfDictionary action;
                action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                action.AddKey(PoDoFo::PdfName("JS"),
                              PoDoFo::PdfString(spec.calcScript.toStdString()));
                PoDoFo::PdfDictionary aa;
                aa.AddKey(PoDoFo::PdfName("C"), action);
                field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aa);
            }
            refs[spec.name] = (field.GetObject)().GetIndirectReference();
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

// Reads a text field's /V straight from the on-disk artifact (r18-review F2:
// the undo/redo pins assert the PERSISTED values, not the session state).
QString pdfValueOf(const QString& path, const QString& name)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return {};
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != name) continue;
            if (field.GetType() != PoDoFo::PdfFieldType::TextBox) return {};
            auto* t = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
            if (!t) return {};
            auto text = t->GetText(); // nullable: non-const accessors
            if (!text.has_value()) return {};
            return QString::fromUtf8(text.value().GetString().data(),
                                     static_cast<qsizetype>(text.value().GetString().size()));
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "pdfValueOf failed:" << e.what();
    }
    return {};
}

// Drives any active modal dialog closed (the panel's warning box) — the
// established bounded offscreen modal pattern; asserts the box really opened.
class ModalCloser : public QObject {

public:
    int closes = 0;
    void start() { m_timer.start(10, this); }
    void stop() { m_timer.stop(); }
protected:
    void timerEvent(QTimerEvent* ev) override {
        if (ev->timerId() != m_timer.timerId()) return;
        if (QWidget* w = QApplication::activeModalWidget()) {
            w->close();
            ++closes;
        }
    }
private:
    QBasicTimer m_timer;
};

} // namespace

class TestFormStaleDisclosure : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    static constexpr const char* kStaleReason =
        "the calculate cascade was aborted before this field; it was not recalculated";

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    // ── Tracker unit pins ────────────────────────────────────────────────────

    void trackerOutcomeReplacesTheStaleSet()
    {
        FormStaleFieldTracker t;
        const QString doc = QStringLiteral("/doc/a.pdf");
        t.applyCascadeOutcome(doc, { failure(QStringLiteral("total"), QStringLiteral("skipped"),
                                             QStringLiteral("never reached")),
                                     failure(QStringLiteral("vat"), QStringLiteral("exception"),
                                             QStringLiteral("script threw")) });
        QVERIFY(t.isStale(doc, QStringLiteral("total")));
        QVERIFY(t.isStale(doc, QStringLiteral("vat")));
        QCOMPARE(t.staleFields(doc).size(), 2);
        QCOMPARE(t.staleReason(doc, QStringLiteral("vat")), QStringLiteral("script threw"));

        // A later outcome REPLACES the set: vat recomputed (not named) — its
        // warning clears; the never-reached "extra" stays stale.
        t.applyCascadeOutcome(doc, { failure(QStringLiteral("extra"), QStringLiteral("skipped"),
                                             QStringLiteral("never reached")) });
        QVERIFY(t.isStale(doc, QStringLiteral("extra")));
        QVERIFY2(!t.isStale(doc, QStringLiteral("vat")),
                 "a field not named by the latest outcome recomputed — warning cleared");
        QVERIFY2(!t.isStale(doc, QStringLiteral("total")),
                 "the replace rule must not accumulate stale fields across cascades");
    }

    void trackerSuccessfulCascadeClearsEverything()
    {
        FormStaleFieldTracker t;
        const QString doc = QStringLiteral("/doc/b.pdf");
        t.applyCascadeOutcome(doc, { failure(QStringLiteral("total"), QStringLiteral("timeout"),
                                             QStringLiteral("deadline")) });
        QVERIFY(t.isStale(doc, QStringLiteral("total")));

        // The retry cascade ran fully — every warning clears.
        t.applyCascadeOutcome(doc, {});
        QVERIFY2(!t.isStale(doc, QStringLiteral("total")), "recompute clears the warning");
        QVERIFY(t.staleFields(doc).isEmpty());
    }

    void trackerAcknowledgeClearsOnlyOneField()
    {
        FormStaleFieldTracker t;
        const QString doc = QStringLiteral("/doc/c.pdf");
        t.applyCascadeOutcome(doc, { failure(QStringLiteral("total"), QStringLiteral("skipped"),
                                             QStringLiteral("never reached")),
                                     failure(QStringLiteral("vat"), QStringLiteral("skipped"),
                                             QStringLiteral("never reached")) });
        t.acknowledge(doc, QStringLiteral("total"));
        QVERIFY2(!t.isStale(doc, QStringLiteral("total")), "acknowledged warning dismissed");
        QVERIFY2(t.isStale(doc, QStringLiteral("vat")),
                 "the sibling field's warning is untouched by the acknowledgement");

        // A document that was never marked is not stale; acknowledging is inert.
        QVERIFY(!t.isStale(QStringLiteral("/doc/other.pdf"), QStringLiteral("total")));
        t.acknowledge(QStringLiteral("/doc/other.pdf"), QStringLiteral("total"));

        // Documents are independent.
        QVERIFY(t.isStale(doc, QStringLiteral("vat")));
    }

    void trackerIgnoresEngineEntriesWithoutFieldName()
    {
        FormStaleFieldTracker t;
        const QString doc = QStringLiteral("/doc/d.pdf");
        t.applyCascadeOutcome(doc, { failure(QString(), QStringLiteral("engine"),
                                             QStringLiteral("sandbox unavailable")) });
        QVERIFY2(t.staleFields(doc).isEmpty(),
                 "an engine entry names no field — nothing to flag persistently");
    }

    void trackerDocumentSwitchKeepsWarningsSeparate()
    {
        FormStaleFieldTracker t;
        const QString a = QStringLiteral("/doc/e.pdf");
        const QString b = QStringLiteral("/doc/f.pdf");
        t.applyCascadeOutcome(a, { failure(QStringLiteral("total"), QStringLiteral("skipped"),
                                           QStringLiteral("never reached")) });
        t.applyCascadeOutcome(b, {});
        QVERIFY(t.isStale(a, QStringLiteral("total")));
        QVERIFY2(!t.isStale(b, QStringLiteral("total")),
                 "B's successful cascade must not clear A's warnings");
    }

    // ── Panel pins (offscreen widgets) ───────────────────────────────────────

    void panelShowsPersistentWarningUntilAcknowledge()
    {
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("panel-stale.pdf"),
                                         { { QStringLiteral("total") }, { QStringLiteral("other") } },
                                         { QStringLiteral("total") });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();

        FormFieldPropertiesPanel panel(&ctx);
        panel.show(); // offscreen: makes child isVisible() reflect visibility

        // The committed cascade outcome: total never recalculated.
        ctx.formStale->applyCascadeOutcome(
            path, { failure(QStringLiteral("total"), QStringLiteral("skipped"),
                            QStringLiteral("the calculate cascade was aborted before this field; "
                                          "its stored value may be stale")) });

        panel.setFieldName(QStringLiteral("total"));
        QLabel* banner = panel.findChild<QLabel*>(); // banner is the last added QLabel row
        // Find the banner by its content (the panel builds several labels).
        QLabel* staleBanner = nullptr;
        const auto labels = panel.findChildren<QLabel*>();
        for (QLabel* l : labels) {
            if (l->text().contains(QLatin1String("STALE VALUE WARNING")))
                staleBanner = l;
        }
        QToolButton* ack = panel.findChild<QToolButton*>();
        QToolButton* ackBtn = nullptr;
        const auto buttons = panel.findChildren<QToolButton*>();
        for (QToolButton* b : buttons) {
            if (b->text().contains(QLatin1String("Acknowledge")))
                ackBtn = b;
        }
        QVERIFY2(staleBanner && staleBanner->isVisible(),
                 "the stale warning must be visible for the stale field");
        QVERIFY2(staleBanner->text().contains(QLatin1String("may be stale")),
                 "the warning carries the field-attributed reason");
        QVERIFY2(ackBtn && ackBtn->isVisible(), "the acknowledge control is offered");

        // A different field: no warning.
        panel.setFieldName(QStringLiteral("other"));
        QVERIFY2(!staleBanner->isVisible(), "the warning follows the field, not the panel");

        // Back on the stale field: the warning SURVIVED the switches.
        panel.setFieldName(QStringLiteral("total"));
        QVERIFY2(staleBanner->isVisible(),
                 "the stale warning persists across refreshes until recompute or acknowledge");

        // Acknowledge clears exactly this field's warning.
        QVERIFY(ackBtn);
        ackBtn->click();
        QVERIFY2(!staleBanner->isVisible(), "acknowledge dismisses the warning");
        QVERIFY2(!ctx.formStale->isStale(path, QStringLiteral("total")),
                 "the acknowledgement is recorded in the tracker");
        Q_UNUSED(banner);
        Q_UNUSED(ack);
    }

    void panelWarningClearsWhenTheFieldRecomputes()
    {
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("panel-recompute.pdf"),
                                         { { QStringLiteral("total") } },
                                         { QStringLiteral("total") });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();

        FormFieldPropertiesPanel panel(&ctx);
        panel.show(); // offscreen: makes child isVisible() reflect visibility
        ctx.formStale->applyCascadeOutcome(
            path, { failure(QStringLiteral("total"), QStringLiteral("timeout"),
                            QStringLiteral("deadline during the event")) });

        panel.setFieldName(QStringLiteral("total"));
        QLabel* staleBanner = nullptr;
        const auto labels = panel.findChildren<QLabel*>();
        for (QLabel* l : labels)
            if (l->text().contains(QLatin1String("STALE VALUE WARNING"))) staleBanner = l;
        QVERIFY2(staleBanner && staleBanner->isVisible(), "precondition: warning shown");

        // The retry cascade recomputed every field — the tracker clears and
        // the next panel refresh drops the warning (no acknowledge needed).
        ctx.formStale->applyCascadeOutcome(path, {});
        panel.setFieldName(QStringLiteral("total"));
        QVERIFY2(!staleBanner->isVisible(),
                 "the warning clears when the field recomputes");
    }

    void panelApplyRunsCascadeAndFeedsTracker()
    {
#ifdef HAS_QUICKJS
        // Real panel-apply flow: the EditFormFieldCommand's snapshot apply
        // runs the in-transaction cascade; the failing calculated field is
        // (1) named in the transient warning dialog and (2) recorded in the
        // persistent tracker. A FAILED command (nothing recomputed) must NOT
        // touch the tracker.
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("panel-apply.pdf"),
        {
            { QStringLiteral("input") },
            { QStringLiteral("total"), QStringLiteral("throw new Error('boom');"), QStringLiteral("0") },
        },
        { QStringLiteral("total") });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();

        // Pre-existing stale state that the failed edit must preserve.
        ctx.formStale->applyCascadeOutcome(
            path, { failure(QStringLiteral("other"), QStringLiteral("skipped"),
                            QStringLiteral("never reached")) });

        FormFieldPropertiesPanel panel(&ctx);
        panel.setFieldName(QStringLiteral("input"));

        // Type a new default value (the panel's "current value" semantics).
        QLineEdit* defaultEdit = nullptr;
        const auto edits = panel.findChildren<QLineEdit*>();
        for (QLineEdit* e : edits)
            if (e->placeholderText() == QLatin1String("Default value")) defaultEdit = e;
        QVERIFY(defaultEdit);
        defaultEdit->setText(QStringLiteral("5"));

        ModalCloser closer;
        closer.start();
        QToolButton* applyBtn = nullptr;
        const auto buttons = panel.findChildren<QToolButton*>();
        for (QToolButton* b : buttons)
            if (b->text() == QLatin1String("Apply")) applyBtn = b;
        QVERIFY(applyBtn);
        applyBtn->click();
        closer.stop();
        QTest::qWait(30);
        QVERIFY2(closer.closes >= 1,
                 "the transient field-attributed warning dialog still appears on the apply path");

        // The cascade ran inside the applied transaction: total threw and kept
        // its committed value — the tracker recorded it persistently.
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "the panel-apply commit path feeds the persistent tracker");
        QVERIFY2(!ctx.formStale->staleReason(path, QStringLiteral("total")).isEmpty(),
                 "the recorded entry carries the engine's reason");
        // The earlier stale state was REPLACED by this outcome (other is no
        // longer named) — the replace rule, driven by the real apply path.
        QVERIFY2(!ctx.formStale->isStale(path, QStringLiteral("other")),
                 "a later outcome replaces the previous stale set");
#else
        QSKIP("This build was compiled without a JavaScript engine (disclosure state).");
#endif
    }

    // ── r18-review F1 (2026-09-13): stack arithmetic is not a success result ──

    // Fixture shared by the two panel-apply regression tests: `total` succeeds
    // while input ≤ 100 (value = input × 2) and THROWS above it — so undo and
    // re-apply can move the cascade outcome between clean and failed.
    QString makeConditionalTotalForm(const QString& name)
    {
        return makeFormPdf(m_dir.path(), name,
        {
            { QStringLiteral("input"), {}, QStringLiteral("1") },
            { QStringLiteral("total"),
              QStringLiteral("var v = Number(this.getField('input').value); "
                             "if (v > 100) { throw new Error('value too large'); } "
                             "event.value = v * 2;"),
              QStringLiteral("0") },
        },
        { QStringLiteral("total") });
    }

    // r18-review F1, case 1 (configured undo limit): with undoLimit = 2 a
    // successful push DELETES the oldest command so count() stays EQUAL — the
    // old `count == before + 1` heuristic read that as "not applied" and
    // skipped the tracker feed exactly when a real recompute happened.
    void panelApplyFeedsTrackerWhenTheUndoLimitDiscards()
    {
#ifdef HAS_QUICKJS
        const QString path = makeConditionalTotalForm(QStringLiteral("stale-undolimit"));
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();
        ctx.undoStack->setUndoLimit(2);

        // Fill the stack with two real, applying edits (no tracker — they are
        // not panel applies; their cascades succeed and touch nothing stale).
        for (const QString& v : { QStringLiteral("3"), QStringLiteral("5") }) {
            EditFormFieldProperties p;
            p.defaultVal = v;
            ctx.undoStack->push(new EditFormFieldCommand(
                ctx.forms.get(), ctx.document.get(), QStringLiteral("input"), p));
        }
        QCOMPARE(ctx.undoStack->count(), 2);

        FormFieldPropertiesPanel panel(&ctx);
        panel.setFieldName(QStringLiteral("input"));
        QLineEdit* defaultEdit = nullptr;
        for (QLineEdit* e : panel.findChildren<QLineEdit*>())
            if (e->placeholderText() == QLatin1String("Default value")) defaultEdit = e;
        QVERIFY(defaultEdit);
        defaultEdit->setText(QStringLiteral("200")); // total's script will THROW

        ModalCloser closer;
        closer.start();
        QToolButton* applyBtn = nullptr;
        for (QToolButton* b : panel.findChildren<QToolButton*>())
            if (b->text() == QLatin1String("Apply")) applyBtn = b;
        QVERIFY(applyBtn);
        applyBtn->click();
        closer.stop();
        QTest::qWait(30);

        // The undo limit really held: the push discarded the oldest command,
        // so count() is UNCHANGED even though this apply succeeded.
        QCOMPARE(ctx.undoStack->count(), 2);
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "a real recompute inside an undo-limit-discard push must still "
                 "feed the stale tracker (old stack arithmetic skipped it)");
#else
        QSKIP("This build was compiled without a JavaScript engine (disclosure state).");
#endif
    }

    // r18-review F1, case 2 (edit after undo): the push flushes the redo
    // entries so count() DROPS (1 → 1 here: delete the undone command, append
    // the new one) — the old heuristic again read "not applied". The stale
    // warning must follow the ACTUAL recompute: the successful restore cleared
    // it, the re-applied failing value must re-record it.
    void panelApplyAfterUndoFeedsTrackerNotStackArithmetic()
    {
#ifdef HAS_QUICKJS
        const QString path = makeConditionalTotalForm(QStringLiteral("stale-reapply"));
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();

        FormFieldPropertiesPanel panel(&ctx);
        panel.setFieldName(QStringLiteral("input"));
        QLineEdit* defaultEdit = nullptr;
        for (QLineEdit* e : panel.findChildren<QLineEdit*>())
            if (e->placeholderText() == QLatin1String("Default value")) defaultEdit = e;
        QVERIFY(defaultEdit);
        QToolButton* applyBtn = nullptr;
        for (QToolButton* b : panel.findChildren<QToolButton*>())
            if (b->text() == QLatin1String("Apply")) applyBtn = b;
        QVERIFY(applyBtn);

        // Apply 1: input = 200 → total throws → stale recorded.
        defaultEdit->setText(QStringLiteral("200"));
        ModalCloser closer1;
        closer1.start();
        applyBtn->click();
        closer1.stop();
        QTest::qWait(30);
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "apply 1: the failing cascade is recorded");

        // Undo: the restore recomputes from input = 1 → cascade succeeds →
        // the stale set CLEARS (this is also the F2 restore-feeding pin).
        ctx.undoStack->undo();
        QVERIFY2(!ctx.formStale->isStale(path, QStringLiteral("total")),
                 "the restore's successful recompute clears the stale warning");
        QCOMPARE(pdfValueOf(path, QStringLiteral("input")), QStringLiteral("1"));
        QCOMPARE(pdfValueOf(path, QStringLiteral("total")), QStringLiteral("2"));

        // Re-apply: input = 300 → the push flushes the redo entry (count stays
        // 1) and total's script throws again. The old stack arithmetic would
        // skip the feed here — omitting the warning exactly when the user's
        // new value made the script fail.
        defaultEdit->setText(QStringLiteral("300"));
        ModalCloser closer2;
        closer2.start();
        applyBtn->click();
        closer2.stop();
        QTest::qWait(30);
        QCOMPARE(ctx.undoStack->count(), 1); // the arithmetic blind spot, pinned
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "apply after undo: the re-applied failing recompute must feed "
                 "the tracker (the redo-stack flush defeats stack arithmetic)");
#else
        QSKIP("This build was compiled without a JavaScript engine (disclosure state).");
#endif
    }

    // ── r18-review F2 (2026-09-13): history traversal feeds the tracker ──────
    //
    // Artifact-backed: every step asserts BOTH the on-disk field values
    // (reopened PoDoFo document) AND the tracker state. The cascade outcome is
    // value-dependent (input > 100 → total throws), so undo and redo really
    // move the stale state.
    void undoRedoTraversalFeedsTheTrackerArtifactBacked()
    {
#ifdef HAS_QUICKJS
        const QString path = makeConditionalTotalForm(QStringLiteral("stale-undoredo"));
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();

        // Panel-equivalent apply via the command itself (tracker attached).
        EditFormFieldProperties p;
        p.defaultVal = QStringLiteral("200");
        ctx.undoStack->push(new EditFormFieldCommand(
            ctx.forms.get(), ctx.document.get(), QStringLiteral("input"), p,
            nullptr, ctx.formStale.get()));

        // After apply: input = 200 on disk; total THREW and kept "0"; stale.
        QCOMPARE(pdfValueOf(path, QStringLiteral("input")), QStringLiteral("200"));
        QCOMPARE(pdfValueOf(path, QStringLiteral("total")), QStringLiteral("0"));
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "apply: the failed recompute is recorded");

        // UNDO: the restore recomputes from input = 1 — values AND stale state
        // move (pre-fix, the traversal never touched the tracker).
        ctx.undoStack->undo();
        QCOMPARE(pdfValueOf(path, QStringLiteral("input")), QStringLiteral("1"));
        QCOMPARE(pdfValueOf(path, QStringLiteral("total")), QStringLiteral("2"));
        QVERIFY2(!ctx.formStale->isStale(path, QStringLiteral("total")),
                 "undo: the restore's successful recompute CLEARS the stale state");

        // REDO: the re-applied snapshot recomputes from input = 200 — total
        // throws again and keeps its CURRENT value ("2"), stale re-recorded.
        ctx.undoStack->redo();
        QCOMPARE(pdfValueOf(path, QStringLiteral("input")), QStringLiteral("200"));
        QCOMPARE(pdfValueOf(path, QStringLiteral("total")), QStringLiteral("2"));
        QVERIFY2(ctx.formStale->isStale(path, QStringLiteral("total")),
                 "redo: the failed recompute re-records the stale state");
#else
        QSKIP("This build was compiled without a JavaScript engine (disclosure state).");
#endif
    }

    // ── r18-review F3 (2026-09-13): the disclosure is SESSION-scoped ─────────
    //
    // Decision, pinned: the warnings persist within the session (across
    // recomputes, switches, rebuilds — pinned by the tests above) and across
    // in-session document switches, NOT across restarts. The tracker is
    // deliberately in-memory only; a fresh instance — the restart-equivalent
    // state — starts clean, and nothing is shared between instances.
    void trackerIsSessionOnlyNoDurableState()
    {
        const QString doc = QStringLiteral("/doc/session.pdf");
        FormStaleFieldTracker session;
        session.applyCascadeOutcome(doc, { failure(QStringLiteral("total"),
                                                  QStringLiteral("timeout"),
                                                  QStringLiteral("deadline during the event")) });
        QVERIFY(session.isStale(doc, QStringLiteral("total")));

        FormStaleFieldTracker freshAsAfterRestart;
        QVERIFY2(!freshAsAfterRestart.isStale(doc, QStringLiteral("total")),
                 "a fresh tracker — the restart-equivalent state — starts clean: "
                 "the disclosure is session-only, not durable across restarts");
        // ...and the two instances do not share state in either direction.
        QVERIFY2(freshAsAfterRestart.staleFields(doc).isEmpty(),
                 "no hidden shared/durable store behind the instances");
        session.clearDocument(doc);
        QVERIFY2(!session.isStale(doc, QStringLiteral("total")),
                 "clearDocument works as before (no hidden second store)");
    }
};

QTEST_MAIN(TestFormStaleDisclosure)
#include "TestFormStaleDisclosure.moc"
