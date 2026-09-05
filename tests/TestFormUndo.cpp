// SPDX-License-Identifier: Apache-2.0
// R02 (audit F09, P1) regression suite — form edits must capture the COMPLETE
// field state and undo must restore it exactly, transactionally.
//
// F09 (pre-fix): EditFormFieldCommand remembered only the original field name.
// Undo wrote an empty tooltip and required=false and skipped restoring an
// empty original value; redo skipped intentionally setting an empty value.
// The suite below fails against that command; after the fix every test pins
// exact snapshot semantics (value vs default vs absent vs explicitly empty).
//
// Documented meaning (R02 step 2): the properties panel's "Default" row edits
// the field's CURRENT value — PDF /V — because it is applied through
// fillForm's SetText. The PDF /DV (default value) key is captured and
// restored losslessly by undo but is NOT what the panel edits.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <QUndoStack>
#include <QSignalSpy>
#include <podofo/podofo.h>
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "commands/EditFormFieldCommand.h"

class TestFormUndo : public QObject {
    Q_OBJECT
private:
    // Minimal valid single-page PDF (same fixture pattern as TestFormBuilder).
    static QString createTestPdf(const QString& dir, const QString& name) {
        const QString path = dir + "/" + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write(
            "%PDF-1.4\n"
            "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
            "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
            "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
            "xref\n0 4\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "trailer<</Size 4/Root 1 0 R>>\n"
            "startxref\n183\n%%EOF\n");
        return path;
    }

    static QByteArray sha256Of(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&f);
        return hash.result();
    }

    static bool pdfLoadsOk(const QString& path) {
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            return doc.GetPages().GetCount() >= 1;
        } catch (const PoDoFo::PdfError&) {
            return false;
        }
    }

    // On-disk view of one text field's supported state.
    struct DiskState {
        bool found = false;
        bool tuPresent = false;
        QString tu;
        bool requiredBit = false;
        bool vPresent = false;
        QString v;
        bool dvPresent = false;
        QString dv;
    };

    static DiskState readState(const QString& pdfPath, const QString& fieldName) {
        DiskState s;
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdfPath.toUtf8().constData());
            auto* acroForm = doc.GetAcroForm();
            if (!acroForm) return s;
            for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
                auto& field = acroForm->GetFieldAt(i);
                if (QString::fromStdString(field.GetFullName()) != fieldName) continue;
                s.found = true;
                auto* t = dynamic_cast<PoDoFo::PdfTextBox*>(&field);
                if (t) {
                    auto v = t->GetText(); // nullable: non-const accessors
                    s.vPresent = v.has_value();
                    if (s.vPresent)
                        s.v = QString::fromUtf8(v.value().GetString().data(),
                                                static_cast<int>(v.value().GetString().size()));
                }
                const PoDoFo::PdfDictionary& d = (field.GetObject)().GetDictionary();
                if (const PoDoFo::PdfObject* tu = d.FindKey("TU"); tu && tu->IsString()) {
                    s.tuPresent = true;
                    s.tu = QString::fromUtf8(tu->GetString().GetString().data(),
                                             static_cast<int>(tu->GetString().GetString().size()));
                }
                if (const PoDoFo::PdfObject* ff = d.FindKey("Ff"); ff && ff->IsNumber())
                    s.requiredBit = (ff->GetNumber() & 2) != 0;
                if (const PoDoFo::PdfObject* dv = d.FindKey("DV"); dv && dv->IsString()) {
                    s.dvPresent = true;
                    s.dv = QString::fromUtf8(dv->GetString().GetString().data(),
                                             static_cast<int>(dv->GetString().GetString().size()));
                }
                break; // duplicates: first occurrence wins (documented policy)
            }
        } catch (const PoDoFo::PdfError&) {
            // leave found == false
        }
        return s;
    }

    // Give the field a nonempty tooltip, required=true and a nonempty value.
    void seedField(FormManager& fm, const QString& pdf, const QString& name,
                   const QString& tooltip, const QString& value) {
        QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 100, 144, 24), name, pdf));
        QVariantMap data;
        data[name] = value;
        QVERIFY(fm.fillForm(pdf, data, pdf, /*lockFields=*/false));
        QVERIFY(fm.setFieldMetadata(pdf, name, tooltip, true, pdf));
    }

private slots:
    // THE F09 demonstration: edit tooltip+required+value, undo, reopen, and
    // compare — the old values must come back EXACTLY.
    void undoRestoresTooltipRequiredAndValue();
    // Redo must apply an explicitly empty value (pre-fix skipped it).
    void redoAppliesExplicitlyEmptyValue();
    // Undo must restore a nonempty tooltip after the edit cleared it.
    void undoRestoresClearedTooltip();
    // Contract pin: the panel's "Default" row edits the CURRENT value (/V).
    void panelDefaultRowEditsCurrentValue();
    // /DV is not the panel's target; undo and redo must never clobber it.
    void dvPreservedThroughEditUndo();
    // Unrelated fields keep their metadata through a neighbor's edit cycle.
    void secondFieldUntouchedByNeighborEdit();
    // ── Tests below need the R02 snapshot API (captureFieldSnapshot /
    //    applyFieldSnapshot + EditFormFieldCommand::succeeded) ──
    // Missing fields resolve explicitly: the command fails and writes nothing.
    void missingFieldFailsExplicitly();
    // A failed save must not partially persist metadata or advance success state.
    void failedSaveIsAtomicNoPartialPersist();
    // Absence of /V is distinct from an explicitly empty /V: undo restores
    // absence exactly.
    void absentValueStaysAbsentThroughUndo();
    // Redo after undo re-applies the stored new snapshot — it must never
    // recapture the edited state as the original.
    void redoDoesNotRecaptureEditedState();
};

void TestFormUndo::undoRestoresTooltipRequiredAndValue() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "f09.pdf");
    QVERIFY(!pdf.isEmpty());

    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip A"), QStringLiteral("orig value"));
    QCOMPARE(readState(pdf, QStringLiteral("f1")).tu, QStringLiteral("tip A"));
    QCOMPARE(readState(pdf, QStringLiteral("f1")).requiredBit, true);
    QCOMPARE(readState(pdf, QStringLiteral("f1")).v, QStringLiteral("orig value"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip B");
    props.required = false;
    props.defaultVal = QStringLiteral("new value");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);
    stack.undo();

    const DiskState after = readState(pdf, QStringLiteral("f1"));
    QVERIFY(after.found);
    QCOMPARE(after.tuPresent, true);
    QCOMPARE(after.tu, QStringLiteral("tip A"));      // pre-fix: written EMPTY
    QCOMPARE(after.requiredBit, true);                // pre-fix: cleared to false
    QCOMPARE(after.vPresent, true);
    QCOMPARE(after.v, QStringLiteral("orig value"));  // pre-fix: skipped (empty capture)

    stack.redo();
    const DiskState redone = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(redone.tu, QStringLiteral("tip B"));
    QCOMPARE(redone.requiredBit, false);
    QCOMPARE(redone.v, QStringLiteral("new value"));
}

void TestFormUndo::redoAppliesExplicitlyEmptyValue() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "empty.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip"), QStringLiteral("x"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip");
    props.required = true;
    props.defaultVal = QString(); // explicitly clear the value
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);

    const DiskState cleared = readState(pdf, QStringLiteral("f1"));
    QVERIFY(cleared.vPresent);                        // pre-fix: redo skipped the empty write
    QCOMPARE(cleared.v, QString());

    stack.undo();
    const DiskState restored = readState(pdf, QStringLiteral("f1"));
    QVERIFY(restored.vPresent);
    QCOMPARE(restored.v, QStringLiteral("x"));
}

void TestFormUndo::undoRestoresClearedTooltip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "tip.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("keep me"), QStringLiteral("v"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QString();     // user cleared the tooltip
    props.required = false;
    props.defaultVal = QStringLiteral("v");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);
    const DiskState cleared = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(cleared.tuPresent, false);               // tooltip removed by the edit

    stack.undo();
    const DiskState restored = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(restored.tuPresent, true);
    QCOMPARE(restored.tu, QStringLiteral("keep me")); // pre-fix: undo wrote EMPTY tooltip
}

void TestFormUndo::panelDefaultRowEditsCurrentValue() {
    // The properties panel has always applied "Default" through fillForm's
    // SetText, i.e. it targets /V (the current value), not /DV. Pin that
    // documented meaning so engine and UI cannot drift apart.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "vv.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip"), QStringLiteral("old"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip");
    props.required = false;
    props.defaultVal = QStringLiteral("written value");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);

    const DiskState s = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(s.v, QStringLiteral("written value"));   // /V changed
}

void TestFormUndo::dvPreservedThroughEditUndo() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "dv.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip"), QStringLiteral("old"));

    // Give the field a /DV default value (test-side preparation; the panel
    // has no control for it). Save to a NEW path — never same-file writes.
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        bool done = false;
        for (unsigned i = 0; i < acroForm->GetFieldCount() && !done; ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != QLatin1String("f1")) continue;
            field.GetDictionary().AddKey(PoDoFo::PdfName("DV"),
                                         PoDoFo::PdfString("template default"));
            done = true;
        }
        QVERIFY(done);
        doc.Save(tmp.filePath("dv-prepared.pdf").toUtf8().constData());
    }
    const QString prepared = tmp.filePath("dv-prepared.pdf");
    QCOMPARE(readState(prepared, QStringLiteral("f1")).dv, QStringLiteral("template default"));

    DocumentSession doc;
    doc.setPath(prepared);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip 2");
    props.required = false;
    props.defaultVal = QStringLiteral("new value");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);
    QCOMPARE(readState(prepared, QStringLiteral("f1")).dv, QStringLiteral("template default"));

    stack.undo();
    QCOMPARE(readState(prepared, QStringLiteral("f1")).dv, QStringLiteral("template default"));
    QCOMPARE(readState(prepared, QStringLiteral("f1")).v, QStringLiteral("old"));
}

void TestFormUndo::secondFieldUntouchedByNeighborEdit() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "two.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("fa"), QStringLiteral("tip A"), QStringLiteral("va"));
    seedField(fm, pdf, QStringLiteral("fb"), QStringLiteral("tip B"), QStringLiteral("vb"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip A2");
    props.required = false;
    props.defaultVal = QStringLiteral("va2");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("fa"), props);
    stack.push(cmd);
    stack.undo();

    const DiskState b = readState(pdf, QStringLiteral("fb"));
    QCOMPARE(b.tu, QStringLiteral("tip B"));
    QCOMPARE(b.requiredBit, true);
    QCOMPARE(b.v, QStringLiteral("vb"));
}

// ── R02 snapshot API tests ──────────────────────────────────────────────────

void TestFormUndo::missingFieldFailsExplicitly() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "ghost.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("real"), QStringLiteral("tip"), QStringLiteral("v"));

    const QByteArray shaBefore = sha256Of(pdf);
    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;
    QSignalSpy reloadSpy(&doc, &DocumentSession::reloadRequested);

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("new tip");
    props.defaultVal = QStringLiteral("new value");

    // Command-state probe via direct redo (no stack ownership involved —
    // Qt 6.11 push() deletes commands that mark themselves obsolete, so
    // post-push reads would be use-after-free).
    {
        EditFormFieldCommand probe(&fm, &doc, QStringLiteral("ghost"), props);
        probe.redo();
        QVERIFY2(!probe.succeeded(), "editing a missing field must report failure");
        QVERIFY2(probe.isObsolete(), "missing-field command must be marked obsolete");
    }

    // Stack-level: pushing the failing command must not create an undo entry.
    {
        auto* pushed = new EditFormFieldCommand(&fm, &doc, QStringLiteral("ghost"), props);
        stack.push(pushed);
    }
    QCOMPARE(stack.count(), 0);
    QCOMPARE(stack.index(), 0);
    QCOMPARE(reloadSpy.count(), 0);
    QCOMPARE(sha256Of(pdf), shaBefore);            // document byte-identical
    QVERIFY(pdfLoadsOk(pdf));
}

void TestFormUndo::failedSaveIsAtomicNoPartialPersist() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "atomic.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip A"), QStringLiteral("va"));
    const QByteArray shaBefore = sha256Of(pdf);

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;
    QSignalSpy reloadSpy(&doc, &DocumentSession::reloadRequested);

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip B");
    props.required = false;
    props.defaultVal = QStringLiteral("vb");
    FormManager::setSaveFaultForTesting(FormManager::SaveFault::Commit);

    // Command-state probe via direct redo.
    {
        EditFormFieldCommand probe(&fm, &doc, QStringLiteral("f1"), props);
        probe.redo(); // the single transactional save fails at commit
        QVERIFY2(!probe.succeeded(), "failed edit must report failure");
        QVERIFY2(probe.isObsolete(), "failed edit command must be marked obsolete");
    }

    // Stack-level: the failing command must not create an undo entry.
    {
        auto* pushed = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
        stack.push(pushed);
    }
    FormManager::setSaveFaultForTesting(FormManager::SaveFault::None);

    QCOMPARE(stack.count(), 0);
    QCOMPARE(stack.index(), 0);
    QCOMPARE(reloadSpy.count(), 0);                // no success-looking signal
    QCOMPARE(sha256Of(pdf), shaBefore);            // nothing persisted at all

    // The value and the metadata must BOTH still carry the old state — one
    // transactional save means no partial completion.
    const DiskState s = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(s.tu, QStringLiteral("tip A"));
    QCOMPARE(s.requiredBit, true);
    QCOMPARE(s.v, QStringLiteral("va"));

    // With the seam reset, the same edit succeeds and undo restores everything.
    auto* cmd2 = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd2);
    QVERIFY(cmd2->succeeded());
    const DiskState edited = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(edited.tu, QStringLiteral("tip B"));
    QCOMPARE(edited.v, QStringLiteral("vb"));
    stack.undo();
    const DiskState restored = readState(pdf, QStringLiteral("f1"));
    QCOMPARE(restored.tu, QStringLiteral("tip A"));
    QCOMPARE(restored.requiredBit, true);
    QCOMPARE(restored.v, QStringLiteral("va"));
}

void TestFormUndo::absentValueStaysAbsentThroughUndo() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "absent.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip"), QStringLiteral("x"));

    // Remove /V entirely (test-side preparation, saved to a NEW path).
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        bool done = false;
        for (unsigned i = 0; i < acroForm->GetFieldCount() && !done; ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != QLatin1String("f1")) continue;
            (field.GetObject)().GetDictionary().RemoveKey("V");
            done = true;
        }
        QVERIFY(done);
        doc.Save(tmp.filePath("absent-prepared.pdf").toUtf8().constData());
    }
    const QString prepared = tmp.filePath("absent-prepared.pdf");
    const DiskState seeded = readState(prepared, QStringLiteral("f1"));
    QCOMPARE(seeded.vPresent, false);              // /V truly absent

    DocumentSession doc;
    doc.setPath(prepared);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip");
    props.required = true;
    props.defaultVal = QString();                  // explicitly empty value
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);
    const DiskState cleared = readState(prepared, QStringLiteral("f1"));
    QVERIFY(cleared.vPresent);                     // explicit empty: /V present but ""
    QCOMPARE(cleared.v, QString());

    stack.undo();
    const DiskState restored = readState(prepared, QStringLiteral("f1"));
    QVERIFY2(!restored.vPresent,
             "undo must restore the ABSENCE of /V, not an empty string");
}

void TestFormUndo::redoDoesNotRecaptureEditedState() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "recap.pdf");
    FormManager fm;
    seedField(fm, pdf, QStringLiteral("f1"), QStringLiteral("tip A"), QStringLiteral("va"));

    DocumentSession doc;
    doc.setPath(pdf);
    QUndoStack stack;

    EditFormFieldProperties props;
    props.tooltip = QStringLiteral("tip B");
    props.required = false;
    props.defaultVal = QStringLiteral("vb");
    auto* cmd = new EditFormFieldCommand(&fm, &doc, QStringLiteral("f1"), props);
    stack.push(cmd);
    QCOMPARE(readState(pdf, QStringLiteral("f1")).v, QStringLiteral("vb"));

    for (int cycle = 0; cycle < 3; ++cycle) {
        stack.undo();
        const DiskState oldState = readState(pdf, QStringLiteral("f1"));
        QCOMPARE(oldState.v, QStringLiteral("va"));
        QCOMPARE(oldState.tu, QStringLiteral("tip A"));
        QCOMPARE(oldState.requiredBit, true);

        stack.redo();
        const DiskState newState = readState(pdf, QStringLiteral("f1"));
        QCOMPARE(newState.v, QStringLiteral("vb"));
        QCOMPARE(newState.tu, QStringLiteral("tip B"));
        QCOMPARE(newState.requiredBit, false);
    }
}

QTEST_MAIN(TestFormUndo)
#include "TestFormUndo.moc"
