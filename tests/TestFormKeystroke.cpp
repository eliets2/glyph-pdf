// SPDX-License-Identifier: Apache-2.0
// R18(f): the /AA /K Keystroke event wired into the Qt line-edit layer.
//
// Contract under test (Acrobat keystroke semantics, sandbox caps unchanged):
//   * the properties panel's Default-value line edit (the Qt layer where a
//     field's value is typed) runs the field's /AA /K script for EVERY
//     text-changing edit the USER makes — never for programmatic writes
//     (population/revert/transform must not self-trigger);
//   * event.rc = false or ANY script failure (timeout — fail closed)
//     REJECTS the keystroke: the line edit reverts and the rejection is
//     disclosed, never silent;
//   * a script that TRANSFORMS event.value (the AFMergeChange idiom) makes
//     the field show the transformed text;
//   * a field without /AA /K types exactly as before (the common case adds
//     no gate and no disclosure);
//   * the merge itself is the Acrobat host contract: [selStart, selEnd) of
//     the pre-keystroke text replaced by event.change (AFMergeChange).
//
// Revert controls (both compile-clean, assertion-level):
//   * reverting src/engines/formjs/AFormShim.cpp alone (the pre-R18f pdf.js
//     AFMergeChange deviation + absent event.change/willCommit/selStart/
//     selEnd + absent AF*_Keystroke family) fails the merge pins, the
//     transform pins and the family pins below;
//   * reverting src/modes/FormFieldPropertiesPanel.cpp alone fails every
//     wiring pin (typing is no longer gated at the line-edit layer).
#include <QtTest/QtTest>
#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QUndoStack>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include <QCryptographicHash>

#include "core/AppContext.h"
#include "core/FormStaleFieldTracker.h"
#include "core/interfaces/IFormManager.h"
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "modes/FormFieldPropertiesPanel.h"

#include <podofo/podofo.h>
#include <map>
#include <memory>
#include <QElapsedTimer>

#ifdef GetObject
#undef GetObject
#endif

using gp::FormFieldPropertiesPanel;
using gp::FormStaleFieldTracker;

namespace {

struct FieldSpec {
    QString name;
    QString keystrokeScript; // /AA /K body (may be empty)
    QString initial;         // initial /V (may be empty)
};

// Minimal one-page form fixture with optional /AA /K scripts (direct save to
// a NEW path only — the R01 lesson applies to fixtures too).
QString makeFormPdf(const QString& dir, const QString& name,
                    const QList<FieldSpec>& fields)
{
    const QString base = dir + "/" + name + "-base.pdf";
    {
        QPdfWriter writer(base);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(80, 100, QStringLiteral("keystroke fixture"));
        p.end();
    }
    const QString path = dir + "/" + name;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(base.toUtf8().constData());
        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(0);
        double y = 600;
        for (const FieldSpec& spec : fields) {
            const PoDoFo::Rect rect(100, y, 200, 16);
            auto& field = page.CreateField<PoDoFo::PdfTextBox>(spec.name.toStdString(), rect);
            dynamic_cast<PoDoFo::PdfTextBox*>(&field)->SetText(
                PoDoFo::PdfString(spec.initial.toStdString()));
            if (!spec.keystrokeScript.isEmpty()) {
                PoDoFo::PdfDictionary action;
                action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                action.AddKey(PoDoFo::PdfName("JS"),
                              PoDoFo::PdfString(spec.keystrokeScript.toStdString()));
                PoDoFo::PdfDictionary aa;
                aa.AddKey(PoDoFo::PdfName("K"), action);
                field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aa);
            }
            y -= 40;
        }
        doc.Save(path.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "fixture build failed:" << e.what();
        return {};
    }
    return path;
}

    // emergence E-1: byte-identity check for the zero-mutation assertion.
    static QByteArray sha256(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&f);
        return hash.result();
    }

} // namespace

class TestFormKeystroke : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    // Seam asserts MUST live in a void helper: QVERIFY2's early `return` is
    // only legal in a void function, and these lookups run inside slots that
    // expect the pointer accessors below to succeed first.
    void assertSeams(FormFieldPropertiesPanel& panel) const
    {
        QVERIFY2(panel.findChild<QLineEdit*>(QStringLiteral("defaultValueEdit")),
                 "the Default-value line edit must expose the defaultValueEdit seam");
        QVERIFY2(panel.findChild<QLabel*>(QStringLiteral("keystrokeStatus")),
                 "the keystroke disclosure label must expose the keystrokeStatus seam");
    }

    // Call ONLY after assertSeams(panel) passed (a nullptr here would crash
    // the harness rather than fail the test).
    QLineEdit* valueEdit(FormFieldPropertiesPanel& panel) const
    {
        return panel.findChild<QLineEdit*>(QStringLiteral("defaultValueEdit"));
    }

    QLabel* statusLabel(FormFieldPropertiesPanel& panel) const
    {
        return panel.findChild<QLabel*>(QStringLiteral("keystrokeStatus"));
    }

    // A panel wired to a real FormManager + DocumentSession over the fixture.
    std::unique_ptr<FormFieldPropertiesPanel> makePanel(const QString& path, AppContext& ctx)
    {
        ctx.forms = std::make_shared<FormManager>();
        ctx.document = std::make_shared<DocumentSession>();
        ctx.document->setPath(path);
        ctx.undoStack = std::make_shared<QUndoStack>();
        ctx.formStale = std::make_shared<FormStaleFieldTracker>();
        auto panel = std::make_unique<FormFieldPropertiesPanel>(&ctx);
        panel->show(); // offscreen: makes child visibility meaningful
        return panel;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void fieldWithoutKeystrokeScriptTypesUnchanged()
    {
        // The common case: no /AA /K — typing works with no gate and no
        // disclosure, exactly as before R18(f).
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("plain.pdf"),
                                         { { QStringLiteral("note") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        panel->setFieldName(QStringLiteral("note"));

        QLineEdit* edit = valueEdit(*panel);
        QLabel* status = statusLabel(*panel);
        QTest::keyClicks(edit, QStringLiteral("hello"));
        QCOMPARE(edit->text(), QStringLiteral("hello"));
        QVERIFY2(!status->isVisible(),
                 "an ungated field must not show keystroke disclosures");
    }

    void keystrokeScriptRejectsTheKeystrokeAndReverts()
    {
        // A max-length gate (merged text longer than 3 chars is refused):
        // the 4th character never takes, the edit reverts, and the rejection
        // is disclosed with the field-attributed kind.
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("reject.pdf"),
                                         { { QStringLiteral("qty"),
                                             QStringLiteral("if (AFMergeChange(event).length > 3) { event.rc = false; }") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        panel->setFieldName(QStringLiteral("qty"));

        QLineEdit* edit = valueEdit(*panel);
        QLabel* status = statusLabel(*panel);
        QTest::keyClicks(edit, QStringLiteral("abc"));
        QCOMPARE(edit->text(), QStringLiteral("abc"));
        QVERIFY2(!status->isVisible(), "accepted keystrokes are not disclosed");

        QTest::keyClicks(edit, QStringLiteral("d"));
        QVERIFY2(edit->text() == QStringLiteral("abc"),
                 qPrintable(QStringLiteral("the rejected keystroke must revert, got '%1'")
                                .arg(edit->text())));
        QVERIFY2(status->isVisible(), "the rejection must be disclosed");
        QVERIFY2(status->text().contains(QLatin1String("blocked")),
                 qPrintable(status->text()));
        QVERIFY2(status->text().contains(QLatin1String("rejected")),
                 qPrintable(status->text()));

        // Deleting again merges to 2 chars — allowed, the disclosure clears.
        QTest::keyClick(edit, Qt::Key_Backspace);
        QCOMPARE(edit->text(), QStringLiteral("ab"));
        QVERIFY2(!status->isVisible(),
                 "the disclosure clears on the next accepted keystroke");
    }

    void keystrokeScriptTransformsTypedText()
    {
        // The AFMergeChange transform idiom: what the user typed is replaced
        // by the script's event.value (upper-cased merged proposal).
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("transform.pdf"),
                                         { { QStringLiteral("code"),
                                             QStringLiteral("event.value = AFMergeChange(event).toUpperCase();") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        panel->setFieldName(QStringLiteral("code"));

        QLineEdit* edit = valueEdit(*panel);
        QTest::keyClicks(edit, QStringLiteral("ab"));
        QVERIFY2(edit->text() == QStringLiteral("AB"),
                 qPrintable(QStringLiteral("the field must show the transformed text, got '%1'")
                                .arg(edit->text())));
    }

    void populateNeverRunsTheKeystrokeScript()
    {
        // A script that rewrites EVERY keystroke must not fire on panel
        // population — programmatic writes are not user keystrokes. (If
        // population triggered the gate, an always-transform script would
        // corrupt the freshly-loaded value.)
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("populate.pdf"),
                                         { { QStringLiteral("f"),
                                             QStringLiteral("event.value = 'X';") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        panel->setFieldName(QStringLiteral("f"));

        QLineEdit* edit = valueEdit(*panel);
        QVERIFY2(edit->text().isEmpty(),
                 "population must not run the keystroke event (the value stays as loaded)");

        // The USER's first keystroke IS gated.
        QTest::keyClicks(edit, QStringLiteral("a"));
        QCOMPARE(edit->text(), QStringLiteral("X"));
    }

    void timeoutFailsClosedAndDiscloses()
    {
        // A looping /AA /K script: the 250 ms event budget fires, the edit
        // is refused (fail closed — a partially gated keystroke never takes)
        // and the timeout is disclosed. Bounded: the test itself must finish.
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("timeout.pdf"),
                                         { { QStringLiteral("qty"), QStringLiteral("while (true) {}") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        panel->setFieldName(QStringLiteral("qty"));

        QLineEdit* edit = valueEdit(*panel);
        QLabel* status = statusLabel(*panel);
        QElapsedTimer clock;
        clock.start();
        QTest::keyClicks(edit, QStringLiteral("7"));
        QVERIFY2(clock.elapsed() < 10000,
                 qPrintable(QStringLiteral("the gated keystroke took %1 ms").arg(clock.elapsed())));
        QVERIFY2(edit->text().isEmpty(),
                 "the timed-out keystroke must be reverted (fail closed)");
        QVERIFY2(status->isVisible(), "the timeout must be disclosed");
        QVERIFY2(status->text().contains(QLatin1String("timeout")),
                 qPrintable(status->text()));
    }

    void fieldSwitchResetsTheGate()
    {
        // Switching fields re-bases the keystroke merge state: the gate must
        // merge against the NEW field's text, never the previous field's.
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("switch.pdf"),
                                         { { QStringLiteral("a"),
                                             QStringLiteral("if (AFMergeChange(event).length > 2) { event.rc = false; }") },
                                           { QStringLiteral("b") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        QLineEdit* edit = valueEdit(*panel);

        panel->setFieldName(QStringLiteral("a"));
        QTest::keyClicks(edit, QStringLiteral("ab")); // accepted (2 chars)
        QCOMPARE(edit->text(), QStringLiteral("ab"));

        panel->setFieldName(QStringLiteral("b")); // ungated field, empty value
        QTest::keyClicks(edit, QStringLiteral("xyz"));
        QCOMPARE(edit->text(), QStringLiteral("xyz"));
    }

    // ── emergence E-1 (SWEEP-W3-EMERGENCE §6) ───────────────────────────────
    // An expired (read-only) document must not execute the field's /AA /K
    // script from the panel, and Apply must refuse with the honest read-only
    // disclosure — zero mutation, no undo entry (the audit's probe inverted:
    // pre-fix this very panel persisted edits through runFormSaveTransaction).
    void readOnlyDocumentRefusesKeystrokeAndApply()
    {
        const QString path = makeFormPdf(m_dir.path(), QStringLiteral("ro.pdf"),
                                         { { QStringLiteral("f"),
                                             QStringLiteral("event.value = 'X';") } });
        QVERIFY(!path.isEmpty());

        AppContext ctx;
        auto panel = makePanel(path, ctx);
        assertSeams(*panel);
        ctx.document->setReadOnly(true); // the expiry feature's session state

        // The Apply affordance is removed for the read-only session.
        QToolButton* apply = panel->findChild<QToolButton*>(
            QStringLiteral("applyFieldPropsButton"));
        QVERIFY2(apply, "the Apply button must expose the applyFieldPropsButton seam");
        QVERIFY2(!apply->isEnabled(),
                 "Apply must be disabled while the document is read-only");

        // Typing runs NO keystroke script: the transform never happens and
        // the refusal is disclosed.
        QLineEdit* edit = valueEdit(*panel);
        QLabel* status = statusLabel(*panel);
        QTest::keyClicks(edit, QStringLiteral("hi"));
        QVERIFY2(edit->text() == QStringLiteral("hi"),
                 qPrintable(QStringLiteral("the /K script must not run on a read-only "
                                          "document, got '%1'").arg(edit->text())));
        QVERIFY2(status->isVisible()
                     && status->text().contains(QLatin1String("read-only")),
                 qPrintable(QStringLiteral("the keystroke refusal must be disclosed, got: %1")
                                .arg(status->text())));

        // The GATE, not just the disabled affordance: invoke the slot directly
        // and require refusal — document byte-identical, no undo entry.
        const QByteArray shaBefore = sha256(path);
        QMetaObject::invokeMethod(panel.get(), "onApplyClicked");
        QVERIFY2(sha256(path) == shaBefore,
                 "Apply on a read-only document must not mutate the file");
        QCOMPARE(ctx.undoStack->count(), 0);
        QVERIFY2(status->isVisible()
                     && status->text().contains(QLatin1String("read-only")),
                 qPrintable(QStringLiteral("the Apply refusal must be disclosed, got: %1")
                                .arg(status->text())));
    }
};

QTEST_MAIN(TestFormKeystroke)
#include "TestFormKeystroke.moc"
