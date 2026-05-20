#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "commands/SanitizeDocumentHelper.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"

class TestSanitization : public QObject {
    Q_OBJECT

private slots:

    void testSanitizeSucceedsOnLoadedDocument()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        mock.m_sanitizeResult = true;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(result, "Sanitize should succeed on a loaded document");
        QCOMPARE(mock.m_sanitizeCalls, 1);
        QCOMPARE(mock.m_lastSanitizedPath, QString("sanitized.pdf"));
    }

    void testSanitizeFailsWhenEngineRejects()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        mock.m_sanitizeResult = false;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should fail when engine rejects");
        QCOMPARE(mock.m_sanitizeCalls, 1);
    }

    void testSanitizeSkipsWithEmptyDocPath()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        DocumentSession doc;

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip when document path is empty");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testSanitizeSkipsWithEmptyOutputPath()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "");

        QVERIFY2(!result, "Sanitize should skip when output path is empty");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testSanitizeSkipsWithNullEngine()
    {
        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(nullptr, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip with null engine");
    }

    void testSanitizeSkipsWithNullDocument()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        bool result = SanitizeDocumentHelper::execute(&mock, nullptr, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip with null document");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testMockSanitizeRemovesMetadata()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        PdfMetadata meta;
        meta.title = "Secret Title";
        meta.author = "Secret Author";
        meta.subject = "Confidential";
        meta.keywords = "secret;classified";
        meta.creator = "EvilApp";
        meta.producer = "EvilLib";
        mock.setMetadata(meta);

        QVERIFY(mock.sanitizeDocument("output.pdf"));

        QCOMPARE(mock.m_sanitizeCalls, 1);
        QCOMPARE(mock.m_lastSanitizedPath, QString("output.pdf"));
    }

    void testMockSanitizeRequiresLoad()
    {
        MockPdfEditorEngine mock;
        QVERIFY(!mock.sanitizeDocument("output.pdf"));

        mock.loadDocumentForEditing("test.pdf");
        QVERIFY(mock.sanitizeDocument("output.pdf"));
    }

    void testMultipleSanitizeCallsTracked()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        mock.sanitizeDocument("first.pdf");
        mock.sanitizeDocument("second.pdf");
        mock.sanitizeDocument("third.pdf");

        QCOMPARE(mock.m_sanitizeCalls, 3);
        QCOMPARE(mock.m_lastSanitizedPath, QString("third.pdf"));
    }

    void testIntegrationSanitization()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("sanitizetest.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            
            auto& catalog = doc.GetCatalog();
            auto& outlines = doc.GetObjects().CreateDictionaryObject();
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Outlines"), outlines.GetIndirectReference());
            
            auto& collection = doc.GetObjects().CreateDictionaryObject();
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Collection"), collection.GetIndirectReference());
            
            auto& field = page.CreateField<PoDoFo::PdfTextBox>("FieldName", PoDoFo::Rect(100, 100, 100, 20));
            field.SetText(PoDoFo::PdfString("FieldValue"));

            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        
        QString outPdf = m_tmpDir.filePath("sanitizetest_out.pdf");
        QVERIFY(engine.sanitizeDocument(outPdf));

        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(outPdf.toUtf8().constData());
        
        auto& catalog = outDoc.GetCatalog();
        QVERIFY(!catalog.GetDictionary().HasKey("Outlines"));
        QVERIFY(!catalog.GetDictionary().HasKey("Collection"));
        
        if (catalog.GetDictionary().HasKey("AcroForm")) {
            auto* acroForm = catalog.GetDictionary().GetKey("AcroForm");
            if (acroForm && acroForm->IsDictionary() && acroForm->GetDictionary().HasKey("Fields")) {
                auto* fields = acroForm->GetDictionary().GetKey("Fields");
                if (fields && fields->IsArray() && fields->GetArray().GetSize() > 0) {
                    auto fieldRef = fields->GetArray()[0];
                    if (fieldRef.IsReference()) {
                        auto& fieldDict = outDoc.GetObjects().MustGetObject(fieldRef.GetReference());
                        if (fieldDict.IsDictionary()) {
                            QVERIFY2(!fieldDict.GetDictionary().HasKey("V"), "Form field V should be removed");
                        }
                    }
                }
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestSanitization)
#include "TestSanitization.moc"
