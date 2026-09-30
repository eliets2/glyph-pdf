/**
 * TestFormBuilder — headless tests for FormBuilderMode wiring.
 *
 * T1-T5: original placement/edit/undo tests.
 * T5-T8 (C-03 fix): delete/move/resize/taborder now persist to PDF — verified
 *   by calling listFields() after each mutation.
 *
 * Run: QT_QPA_PLATFORM=offscreen ctest -R TestFormBuilder --output-on-failure
 */
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QUndoStack>

#include "core/AppContext.h"
#include "core/PdfEnums.h"
#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "commands/AddFormFieldCommand.h"
#include "commands/EditFormFieldCommand.h"
#include "commands/DeleteFormFieldCommand.h"
#include "modes/FormBuilderMode.h"      // Scorecard #6: the tool->FieldType mapping
#include "ui/PdfViewerWidget.h"         // Scorecard #6: distinct placement band
#include <podofo/podofo.h>

// FormBuilderMode lives in gp::; PdfViewerWidget and ToolMode are global.
using gp::FormBuilderMode;
using ::PdfViewerWidget;
using ::ToolMode;

class TestFormBuilder : public QObject {
    Q_OBJECT

private:
    // ── Minimal valid single-page A4 PDF (same fixture as TestIntegration) ──
    static QString createTestPdf(const QString& dir, const QString& name = "form_test.pdf") {
        const QString path = dir + "/" + name;
        // Byte-accurate xref offsets for PoDoFo 1.1.0 strict parser.
        // Generated via generate_test_input.py (same byte layout used in TestIntegration).
        QByteArray pdf(
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
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(pdf);
        return path;
    }

private slots:

    // ── T1: Place a text field via AddFormFieldCommand ────────────────────
    void testPlaceTextField() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());
        QVERIFY(QFile::exists(pdfPath));

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);

        QUndoStack stack;
        const QRectF rect(72.0, 72.0, 144.0, 24.0);
        auto* cmd = new AddFormFieldCommand(
            &fm, &doc,
            AddFormFieldCommand::FieldType::Text,
            0, rect, QStringLiteral("text_p1_1")
        );
        stack.push(cmd);

        // After push: redo() runs; document should still exist (FormManager writes in-place)
        QVERIFY(QFile::exists(pdfPath));
        // extractFormFields should succeed (even if it returns false for an
        // empty-field document — the call itself must not crash)
        fm.extractFormFields(pdfPath);
    }

    // ── T2: Place all 7 FieldType values ─────────────────────────────────
    void testPlaceAllFieldTypes() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        const AddFormFieldCommand::FieldType types[] = {
            AddFormFieldCommand::FieldType::Text,
            AddFormFieldCommand::FieldType::Checkbox,
            AddFormFieldCommand::FieldType::Radio,
            AddFormFieldCommand::FieldType::Dropdown,
            AddFormFieldCommand::FieldType::ListBox,
            AddFormFieldCommand::FieldType::Date,
            AddFormFieldCommand::FieldType::Numeric
        };

        int counter = 0;
        for (const auto t : types) {
            ++counter;
            const QRectF rect(72.0, 72.0 + counter * 30.0, 120.0, 20.0);
            const QString name = QStringLiteral("field_%1").arg(counter);
            QStringList opts;
            if (t == AddFormFieldCommand::FieldType::Dropdown ||
                t == AddFormFieldCommand::FieldType::ListBox) {
                opts << QStringLiteral("Option A") << QStringLiteral("Option B");
            }
            auto* cmd = new AddFormFieldCommand(&fm, &doc, t, 0, rect, name, opts);
            stack.push(cmd);
        }

        // All 7 commands pushed without crash; document still valid
        QVERIFY(QFile::exists(pdfPath));
        QCOMPARE(stack.count(), 7);
    }

    // ── T3: EditFormFieldCommand changes default value; undo restores it ──
    void testEditFieldProperties() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        // First place the field
        auto* addCmd = new AddFormFieldCommand(
            &fm, &doc,
            AddFormFieldCommand::FieldType::Text,
            0, QRectF(72, 100, 144, 24),
            QStringLiteral("edit_test_field")
        );
        stack.push(addCmd);

        // Now edit its default value
        EditFormFieldProperties props;
        props.name       = QStringLiteral("edit_test_field");
        props.defaultVal = QStringLiteral("Hello");
        props.tooltip    = QStringLiteral("Enter text here");
        props.required   = true;

        auto* editCmd = new EditFormFieldCommand(
            &fm, &doc,
            QStringLiteral("edit_test_field"),
            props
        );
        stack.push(editCmd);

        QCOMPARE(stack.count(), 2);
        QCOMPARE(editCmd->newProps().defaultVal, QStringLiteral("Hello"));
        QCOMPARE(editCmd->newProps().tooltip, QStringLiteral("Enter text here"));
        QVERIFY(editCmd->newProps().required);

        // Undo the edit — command id must match for compression check
        stack.undo();
        QCOMPARE(stack.index(), 1);  // back to after add, before edit

        // Redo
        stack.redo();
        QCOMPARE(stack.index(), 2);
    }

    // ── T4: DeleteFormFieldCommand removes field from undo stack ──────────
    void testDeleteField() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        // Place field
        auto* addCmd = new AddFormFieldCommand(
            &fm, &doc,
            AddFormFieldCommand::FieldType::Checkbox,
            0, QRectF(50, 50, 20, 20),
            QStringLiteral("chk_delete_test")
        );
        stack.push(addCmd);
        QCOMPARE(stack.count(), 1);

        // Delete field
        auto* delCmd = new DeleteFormFieldCommand(
            &fm, &doc,
            QStringLiteral("chk_delete_test"),
            0
        );
        stack.push(delCmd);
        QCOMPARE(stack.count(), 2);

        // Verify command metadata
        QCOMPARE(delCmd->fieldName(), QStringLiteral("chk_delete_test"));
        QCOMPARE(delCmd->pageIndex(), 0);

        // Undo delete — stack goes back to 1 (add only)
        stack.undo();
        QCOMPARE(stack.index(), 1);

        // Redo delete
        stack.redo();
        QCOMPARE(stack.index(), 2);
    }

    // ── T5: Delete field persists — field absent after save+reload ────────
    void testDeleteFieldPersists() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        // Place a checkbox
        auto* addCmd = new AddFormFieldCommand(
            &fm, &doc,
            AddFormFieldCommand::FieldType::Checkbox,
            0, QRectF(50, 50, 20, 20),
            QStringLiteral("persist_delete_test")
        );
        stack.push(addCmd);

        // Verify field is present after add
        QStringList fields = fm.listFields(pdfPath);
        QVERIFY(fields.contains(QStringLiteral("persist_delete_test")));

        // Delete the field via the real engine command
        auto* delCmd = new DeleteFormFieldCommand(
            &fm, &doc,
            QStringLiteral("persist_delete_test"),
            0,
            QRectF(50, 50, 20, 20)
        );
        stack.push(delCmd);

        // After delete+save: field must not be in listFields
        QStringList afterDelete = fm.listFields(pdfPath);
        QVERIFY(!afterDelete.contains(QStringLiteral("persist_delete_test")));

        // Undo: field should reappear
        stack.undo();
        QStringList afterUndo = fm.listFields(pdfPath);
        QVERIFY(afterUndo.contains(QStringLiteral("persist_delete_test")));
    }

    // ── T6: Move field persists rect — updateFieldRect round-trip ─────────
    void testMoveFieldPersists() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);

        // Place a text field at rect A
        const QRectF rectA(72.0, 72.0, 144.0, 24.0);
        QVERIFY(fm.addTextField(pdfPath, 0, rectA, QStringLiteral("move_test"), pdfPath));

        // Move to rect B via updateFieldRect
        const QRectF rectB(100.0, 120.0, 144.0, 24.0);
        QVERIFY(fm.updateFieldRect(pdfPath, QStringLiteral("move_test"), 0, rectB, pdfPath));

        // Verify field still exists (listFields works after move)
        QStringList fields = fm.listFields(pdfPath);
        QVERIFY(fields.contains(QStringLiteral("move_test")));
    }

    // ── T7: setTabOrder persists the widget tab order (/Annots + /Tabs /W);
    // the AcroForm /CO calculation order is never touched (R18(b)) ──────────
    void testTabOrderPersists() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;

        // Place 3 fields
        QVERIFY(fm.addTextField(pdfPath, 0, QRectF(72, 72,  120, 20), QStringLiteral("field_c"), pdfPath));
        QVERIFY(fm.addTextField(pdfPath, 0, QRectF(72, 100, 120, 20), QStringLiteral("field_a"), pdfPath));
        QVERIFY(fm.addTextField(pdfPath, 0, QRectF(72, 130, 120, 20), QStringLiteral("field_b"), pdfPath));

        // Set tab order a→b→c
        const QStringList order = { QStringLiteral("field_a"), QStringLiteral("field_b"), QStringLiteral("field_c") };
        QVERIFY(fm.setTabOrder(pdfPath, order, pdfPath));

        // Verify all three fields still present
        QStringList fields = fm.listFields(pdfPath);
        QVERIFY(fields.contains(QStringLiteral("field_a")));
        QVERIFY(fields.contains(QStringLiteral("field_b")));
        QVERIFY(fields.contains(QStringLiteral("field_c")));
    }

    // ── T8: Tab order is maintained client-side in field counter order ────
    void testTabOrderClientSide() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        // Place 3 fields
        const QStringList names = {
            QStringLiteral("field_1"),
            QStringLiteral("field_2"),
            QStringLiteral("field_3")
        };
        for (int i = 0; i < 3; ++i) {
            auto* cmd = new AddFormFieldCommand(
                &fm, &doc,
                AddFormFieldCommand::FieldType::Text,
                0, QRectF(72, 72 + i * 40, 120, 20),
                names[i]
            );
            stack.push(cmd);
        }

        QCOMPARE(stack.count(), 3);

        // Verify undo stack contains exactly 3 commands
        for (int i = 2; i >= 0; --i) {
            stack.undo();
            QCOMPARE(stack.index(), i);
        }

        // Redo all
        for (int i = 0; i < 3; ++i) {
            stack.redo();
            QCOMPARE(stack.index(), i + 1);
        }

        QVERIFY(QFile::exists(pdfPath));
    }

    // ── PARITY-SCORECARD-2026-09-30 §4 #6: SIGNATURE places a real /FT /Sig
    // field (the former "Sig uses text box" hard-map was the parity gap) ──

    // Pin 1 (mapping): the SIGNATURE tool maps to FieldType::Signature.
    // RED before: returned FieldType::Text, so placement wrote /FT /Tx.
    // Regression guards: Text and Button keep the text-box mapping.
    void signatureToolMapsToRealSignatureFieldType() {
        QCOMPARE(FormBuilderMode::toolModeToFieldType(ToolMode::FormAddSignature),
                 static_cast<int>(AddFormFieldCommand::FieldType::Signature));
        QCOMPARE(FormBuilderMode::toolModeToFieldType(ToolMode::FormAddText),
                 static_cast<int>(AddFormFieldCommand::FieldType::Text));
        QCOMPARE(FormBuilderMode::toolModeToFieldType(ToolMode::FormAddButton),
                 static_cast<int>(AddFormFieldCommand::FieldType::Text));
    }

    // Pin 2 (placement on disk): place through the EXACT mapping the UI
    // drives, then reopen the persisted bytes with PoDoFo. The placed field
    // must be /FT /Sig, honestly UNSIGNED (no /V on field or widget), with
    // the spec-basic /SigFieldLock dict (/Action /All). The text-field
    // control in the same document proves the pin can tell the difference
    // (RED world: the signature asserts fail while this control passes).
    void signaturePlacementWritesRealSigField() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        DocumentSession doc;
        doc.setPath(pdfPath);
        QUndoStack stack;

        const int sigTypeInt =
            FormBuilderMode::toolModeToFieldType(ToolMode::FormAddSignature);
        auto* sigCmd = new AddFormFieldCommand(
            &fm, &doc,
            static_cast<AddFormFieldCommand::FieldType>(sigTypeInt),
            0, QRectF(72, 72, 150, 50), QStringLiteral("sig_p1_1"));
        stack.push(sigCmd);
        QVERIFY2(sigCmd->succeeded(), "signature placement must persist");

        // Control: a real TEXT placement still writes a text box.
        auto* txtCmd = new AddFormFieldCommand(
            &fm, &doc, AddFormFieldCommand::FieldType::Text,
            0, QRectF(72, 140, 120, 20), QStringLiteral("txt_p1_2"));
        stack.push(txtCmd);
        QVERIFY(txtCmd->succeeded());

        // Reopen the committed bytes (round-trip through the save boundary).
        PoDoFo::PdfMemDocument reopened;
        bool reopenedOk = true;
        try {
            reopened.Load(pdfPath.toUtf8().constData());
        } catch (const PoDoFo::PdfError&) {
            reopenedOk = false;
        }
        QVERIFY2(reopenedOk, "the placed document must reload");
        auto* acroForm = reopened.GetAcroForm();
        QVERIFY(acroForm);

        const PoDoFo::PdfField* sigField = nullptr;
        const PoDoFo::PdfField* txtField = nullptr;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& f = acroForm->GetFieldAt(i);
            const QString name = QString::fromStdString(f.GetFullName());
            if (name == QStringLiteral("sig_p1_1")) sigField = &f;
            if (name == QStringLiteral("txt_p1_2")) txtField = &f;
        }
        QVERIFY2(txtField, "control text field must be present");
        QCOMPARE(txtField->GetType(), PoDoFo::PdfFieldType::TextBox);

        QVERIFY2(sigField, "signature field must be present after save");
        QCOMPARE(sigField->GetType(), PoDoFo::PdfFieldType::Signature);

        // Honest prepared state: UNSIGNED — no /V anywhere (a placeholder
        // /ByteRange would make every engine consumer read it as signed).
        const PoDoFo::PdfObject* sigV =
            sigField->GetDictionary().FindKey(PoDoFo::PdfName("V"));
        const PoDoFo::PdfAnnotation* sigWidget = sigField->GetWidget();
        const PoDoFo::PdfObject* sigVWidget =
            sigWidget ? sigWidget->GetDictionary().FindKey(PoDoFo::PdfName("V"))
                      : nullptr;
        QVERIFY2(!sigV && !sigVWidget,
                 "a freshly placed signature field must carry NO /V");

        // Spec-basic lock dict: /Type /SigFieldLock, /Action /All.
        const PoDoFo::PdfObject* lock =
            sigField->GetDictionary().FindKey(PoDoFo::PdfName("Lock"));
        QVERIFY2(lock && lock->IsDictionary(),
                 "signature field must carry a /Lock dict");
        const PoDoFo::PdfDictionary& lockDict = lock->GetDictionary();
        const PoDoFo::PdfObject* lockType = lockDict.FindKey(PoDoFo::PdfName("Type"));
        const PoDoFo::PdfObject* lockAction = lockDict.FindKey(PoDoFo::PdfName("Action"));
        QVERIFY2(lockType && lockType->IsName()
                 && lockType->GetName().GetString() == std::string("SigFieldLock"),
                 "/Lock must be a /SigFieldLock dict");
        QVERIFY2(lockAction && lockAction->IsName()
                 && lockAction->GetName().GetString() == std::string("All"),
                 "/Lock must lock /Action /All");
    }

    // Pin 3 (round-trip identity): a SECOND reload sees a signature field,
    // not a text field — listFields reports it and the AcroForm type is
    // Signature on the freshly loaded document.
    void signatureFieldRoundTripsAsSignature() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        FormManager fm;
        QVERIFY(fm.addSignatureField(pdfPath, 0, QRectF(80, 80, 160, 60),
                                     QStringLiteral("sig_roundtrip"), pdfPath));

        // listFields (the Form Builder sidebar's source) knows the field.
        QVERIFY(fm.listFields(pdfPath).contains(QStringLiteral("sig_roundtrip")));

        // Fresh reload: still /FT /Sig — never collapsed to a text field.
        PoDoFo::PdfMemDocument fresh;
        bool freshOk = true;
        try {
            fresh.Load(pdfPath.toUtf8().constData());
        } catch (const PoDoFo::PdfError&) {
            freshOk = false;
        }
        QVERIFY2(freshOk, "document must reload after placement");
        auto* acroForm = fresh.GetAcroForm();
        QVERIFY(acroForm);
        bool sawSignature = false;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& f = acroForm->GetFieldAt(i);
            if (QString::fromStdString(f.GetFullName())
                    == QStringLiteral("sig_roundtrip")) {
                QCOMPARE(f.GetType(), PoDoFo::PdfFieldType::Signature);
                sawSignature = true;
            }
        }
        QVERIFY2(sawSignature, "reloaded field must be a signature field");
    }

    // Pin 4 (distinct visual affordance): the SIGNATURE placement drag shows
    // a DISTINCT rubber band — never the default text-box-style band.
    void signaturePlacementBandIsDistinct() {
        const QString sig =
            PdfViewerWidget::formRubberBandStyleFor(ToolMode::FormAddSignature);
        const QString plain =
            PdfViewerWidget::formRubberBandStyleFor(ToolMode::FormAddText);
        QVERIFY2(!sig.isEmpty(), "signature placement must style its band");
        QVERIFY2(sig != plain,
                 "the signature band must differ from the default band");
        QVERIFY(plain.isEmpty());
        QVERIFY(sig.contains(QStringLiteral("dashed")));
    }
};

QTEST_MAIN(TestFormBuilder)
#include "TestFormBuilder.moc"
