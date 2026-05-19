#include <QtTest/QtTest>
#include "commands/SanitizeDocumentHelper.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"

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
};

QTEST_GUILESS_MAIN(TestSanitization)
#include "TestSanitization.moc"
