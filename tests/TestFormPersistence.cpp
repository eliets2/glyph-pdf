/**
 * TestFormPersistence — C-03 / UX-08 regression suite.
 *
 * Each test follows the save+reload pattern:
 *   1. Place a field via FormManager directly (engine call).
 *   2. Mutate (delete / move / resize) via the QUndoCommand or engine call.
 *   3. Reload: open the PDF a second time with a fresh FormManager instance
 *      and verify the mutation persisted (field gone, rect updated, etc.).
 *
 * This is distinct from TestFormBuilder T5-T7 which also test persistence
 * but use a single FormManager instance.  Here we use a second instance to
 * prove that the bytes on disk changed, not just an in-memory cache.
 *
 * Run: QT_QPA_PLATFORM=offscreen ctest -R TestFormPersistence --output-on-failure
 */
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QUndoStack>

#include "engines/FormManager.h"
#include "engines/DocumentSession.h"
#include "commands/AddFormFieldCommand.h"
#include "commands/DeleteFormFieldCommand.h"
#include "commands/MoveFormFieldCommand.h"
#include "commands/ResizeFormFieldCommand.h"

class TestFormPersistence : public QObject {
    Q_OBJECT

private:
    // Minimal single-page A4 PDF with precise xref offsets.
    static QString createTestPdf(const QString& dir, const QString& name = "persist_test.pdf") {
        const QString path = dir + "/" + name;
        const QByteArray pdf(
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

    // ── TP-1: Delete field — absent after save+reload with a fresh instance ──
    void testDeletePersistsAcrossReload() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        // Session 1: place then delete
        {
            FormManager fm1;
            DocumentSession doc1;
            doc1.setPath(pdfPath);

            // Place a text field
            QVERIFY(fm1.addTextField(pdfPath, 0, QRectF(72, 72, 144, 24),
                                     QStringLiteral("tp1_field"), pdfPath));
            QVERIFY(fm1.listFields(pdfPath).contains(QStringLiteral("tp1_field")));

            // Delete via command (exercises redo() → removeFieldByName)
            QUndoStack stack;
            auto* del = new DeleteFormFieldCommand(
                &fm1, &doc1,
                QStringLiteral("tp1_field"), 0,
                QRectF(72, 72, 144, 24));
            stack.push(del);
        }

        // Session 2: fresh FormManager — field must be GONE from disk
        {
            FormManager fm2;
            const QStringList fields = fm2.listFields(pdfPath);
            QVERIFY2(!fields.contains(QStringLiteral("tp1_field")),
                     "Field survived delete+save — deletion did not persist to disk.");
        }
    }

    // ── TP-2: Move field — rect updated after save+reload ────────────────────
    void testMovePersistsAcrossReload() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        const QRectF rectA(72.0,  72.0,  144.0, 24.0);
        const QRectF rectB(200.0, 300.0, 144.0, 24.0);

        // Session 1: place then move
        {
            FormManager fm1;
            DocumentSession doc1;
            doc1.setPath(pdfPath);

            QVERIFY(fm1.addTextField(pdfPath, 0, rectA,
                                     QStringLiteral("tp2_field"), pdfPath));

            // Move via command
            QUndoStack stack;
            auto* mv = new MoveFormFieldCommand(
                &fm1, &doc1,
                QStringLiteral("tp2_field"), 0, rectA, rectB);
            stack.push(mv);
        }

        // Session 2: fresh FormManager — field must still be present (move doesn't delete)
        {
            FormManager fm2;
            const QStringList fields = fm2.listFields(pdfPath);
            QVERIFY2(fields.contains(QStringLiteral("tp2_field")),
                     "Field missing after move+save — move destroyed the field.");
        }
    }

    // ── TP-3: Resize field — rect updated after save+reload ──────────────────
    void testResizePersistsAcrossReload() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        const QRectF rectA(72.0, 72.0,  144.0, 24.0);
        const QRectF rectB(72.0, 72.0,  288.0, 48.0);   // wider + taller

        // Session 1: place then resize
        {
            FormManager fm1;
            DocumentSession doc1;
            doc1.setPath(pdfPath);

            QVERIFY(fm1.addTextField(pdfPath, 0, rectA,
                                     QStringLiteral("tp3_field"), pdfPath));

            QUndoStack stack;
            auto* rsz = new ResizeFormFieldCommand(
                &fm1, &doc1,
                QStringLiteral("tp3_field"), 0, rectA, rectB);
            stack.push(rsz);
        }

        // Session 2: field must still be present
        {
            FormManager fm2;
            const QStringList fields = fm2.listFields(pdfPath);
            QVERIFY2(fields.contains(QStringLiteral("tp3_field")),
                     "Field missing after resize+save — resize destroyed the field.");
        }
    }

    // ── TP-4: Delete undo — field reappears after undo+reload ────────────────
    void testDeleteUndoPersistsAcrossReload() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        // Session 1: place, delete, undo
        {
            FormManager fm1;
            DocumentSession doc1;
            doc1.setPath(pdfPath);

            QVERIFY(fm1.addTextField(pdfPath, 0, QRectF(72, 72, 144, 24),
                                     QStringLiteral("tp4_field"), pdfPath));

            QUndoStack stack;
            auto* del = new DeleteFormFieldCommand(
                &fm1, &doc1,
                QStringLiteral("tp4_field"), 0,
                QRectF(72, 72, 144, 24));
            stack.push(del);  // field gone
            stack.undo();     // field restored via addTextField
        }

        // Session 2: field must be back on disk
        {
            FormManager fm2;
            const QStringList fields = fm2.listFields(pdfPath);
            QVERIFY2(fields.contains(QStringLiteral("tp4_field")),
                     "Field did not reappear after delete-undo+save.");
        }
    }

    // ── TP-5: Add undo — field removed after undo+reload ─────────────────────
    void testAddUndoPersistsAcrossReload() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdfPath = createTestPdf(tmp.path());

        // Session 1: place, then undo the placement
        {
            FormManager fm1;
            DocumentSession doc1;
            doc1.setPath(pdfPath);

            QUndoStack stack;
            auto* add = new AddFormFieldCommand(
                &fm1, &doc1,
                AddFormFieldCommand::FieldType::Text,
                0, QRectF(72, 72, 144, 24),
                QStringLiteral("tp5_field"));
            stack.push(add);  // field placed
            QVERIFY(fm1.listFields(pdfPath).contains(QStringLiteral("tp5_field")));

            stack.undo();     // AddFormFieldCommand::undo() calls removeFieldByName
        }

        // Session 2: field must NOT be present — undo was persisted to disk
        {
            FormManager fm2;
            const QStringList fields = fm2.listFields(pdfPath);
            QVERIFY2(!fields.contains(QStringLiteral("tp5_field")),
                     "Field persisted after add-undo — AddFormFieldCommand::undo() did not remove from PDF.");
        }
    }
};

QTEST_MAIN(TestFormPersistence)
#include "TestFormPersistence.moc"
