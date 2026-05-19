#include <QtTest/QtTest>
#include "commands/SanitizeDocumentHelper.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"

class TestPdfEditorInterface : public QObject {
    Q_OBJECT
private slots:
    void testMetadataRoundTrip() {
        MockPdfEditorEngine mock;
        PdfMetadata meta;
        meta.title = "Test";
        meta.author = "Author";
        QVERIFY(mock.setMetadata(meta));

        PdfMetadata out;
        QVERIFY(mock.getMetadata(out));
        QCOMPARE(out.title, "Test");
        QCOMPARE(out.author, "Author");
    }

    void testEncryptRequiresLoad() {
        MockPdfEditorEngine mock;
        QVERIFY(!mock.encryptDocument("user", "owner", true, true, true)); // not loaded
        mock.m_loaded = true;
        QVERIFY(mock.encryptDocument("user", "owner", true, true, true));  // loaded
    }

    void testSanitizeCommandSavesOutputWhenSanitizeSucceeds() {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        DocumentSession doc;
        doc.setPath("input.pdf");
        QSignalSpy reloadSpy(&doc, &DocumentSession::reloadRequested);

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY(result);
        QCOMPARE(mock.m_sanitizeCalls, 1);
        QCOMPARE(mock.m_lastSanitizedPath, QString("sanitized.pdf"));
    }

    void testSanitizeCommandDoesNotSaveWhenSanitizeFails() {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        mock.m_sanitizeResult = false;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY(!result);
        QCOMPARE(mock.m_sanitizeCalls, 1);
    }
};

QTEST_GUILESS_MAIN(TestPdfEditorInterface)
#include "TestPdfEditorInterface.moc"
