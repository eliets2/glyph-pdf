#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/FormManager.h"
#include "engines/PdfEditorEngine.h"
#include "engines/ConversionManager.h"
#include "engines/SignatureManager.h"

class SmokeTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;
    QString m_testPdf;

    QString tmpPath(const QString &name) const {
        return m_tmpDir.filePath(name);
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temporary directory");

        m_testPdf = tmpPath("smoke_test.pdf");
        try {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(m_testPdf.toUtf8().constData());
        } catch (const std::exception &e) {
            QFAIL(qPrintable(QString("Failed to create test PDF: %1").arg(e.what())));
        }
        QVERIFY2(QFile::exists(m_testPdf), "Test PDF was not created");
    }

    void testFormManagerExtract()
    {
        FormManager fm;
        // A freshly created PDF has no forms, but extraction should not crash
        fm.extractFormFields(m_testPdf);
    }

    void testEditorLoad()
    {
        PdfEditorEngine editor;
        QVERIFY2(editor.loadDocumentForEditing(m_testPdf),
                 "PdfEditorEngine failed to load test PDF");
    }

    void testEditTextInline()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));
        bool ok = editor.editTextInline(0, QRectF(100, 100, 200, 50), "Hello\nWorld");
        QVERIFY2(ok, "editTextInline failed");
    }

    void testDeleteObject()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));
        bool ok = editor.deleteObjectAt(0, QPointF(150, 150));
        QVERIFY2(ok, "deleteObjectAt failed");
    }

    void testRedaction()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));
        bool ok = editor.applyRedactions(0, {QRectF(50, 50, 100, 100)});
        QVERIFY2(ok, "applyRedactions failed");
    }

    void testSaveEdited()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));
        editor.editTextInline(0, QRectF(100, 100, 200, 50), "Test");

        QString edited = tmpPath("edited.pdf");
        editor.saveDocument(edited);
        QVERIFY2(QFile::exists(edited), "Edited PDF was not saved");
    }

    void testLinearize()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));

        QString linearized = tmpPath("linearized.pdf");
        bool ok = editor.linearizeDocument(linearized);
        if (!ok)
            QSKIP("linearizeDocument not available (qpdf not compiled in)");
        QVERIFY(QFile::exists(linearized));
    }

    void testExportPdfA1b()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));

        QString output = tmpPath("pdfa1b.pdf");
        bool ok = editor.exportPdfA(output, 1);
        QVERIFY2(ok, "exportPdfA level 1 failed");
        QVERIFY(QFile::exists(output));
    }

    void testExportPdfA2b()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));

        QString output = tmpPath("pdfa2b.pdf");
        bool ok = editor.exportPdfA(output, 2);
        QVERIFY2(ok, "exportPdfA level 2 failed");
        QVERIFY(QFile::exists(output));
    }

    void testExportPdfA3b()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));

        QString output = tmpPath("pdfa3b.pdf");
        bool ok = editor.exportPdfA(output, 3);
        QVERIFY2(ok, "exportPdfA level 3 failed");
        QVERIFY(QFile::exists(output));
    }

    void testSanitize()
    {
        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(m_testPdf));

        QString sanitized = tmpPath("sanitized.pdf");
        bool ok = editor.sanitizeDocument(sanitized);
        QVERIFY2(ok, "sanitizeDocument failed");
        QVERIFY(QFile::exists(sanitized));
    }

    void testConversionWord()
    {
        ConversionManager conv;
        QString output = tmpPath("converted.docx");
        bool ok = conv.convertTo(m_testPdf, output, IConversionEngine::TargetFormat::Word);
        QVERIFY2(ok, "Conversion to Word failed");
    }

    void testConversionExcel()
    {
        ConversionManager conv;
        QString output = tmpPath("converted.xlsx");
        bool ok = conv.convertTo(m_testPdf, output, IConversionEngine::TargetFormat::Excel);
        QVERIFY2(ok, "Conversion to Excel failed");
    }

    void testSignatureValidation()
    {
        SignatureManager sm;
        auto sigs = sm.validateSignatures(m_testPdf);
        // A freshly created PDF has no signatures
        QCOMPARE(sigs.size(), 0);
    }

    void cleanupTestCase()
    {
        // QTemporaryDir handles cleanup automatically
    }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "smoke_test.moc"
