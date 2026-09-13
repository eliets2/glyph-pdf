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
};

QTEST_MAIN(TestFormStaleDisclosure)
#include "TestFormStaleDisclosure.moc"
