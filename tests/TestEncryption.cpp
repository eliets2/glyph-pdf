#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestEncryption : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString &name) const {
        return m_tmpDir.filePath(name);
    }

private slots:
    void initTestCase() {
        qDebug() << "INIT TEST CASE";
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void testAES256EncryptionApplied() {
        QString pdf = tmpPath("to_encrypt.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        
        QVERIFY(engine.encryptDocument("user", "owner", true, true, true));
        
        QString output = tmpPath("encrypted.pdf");
        engine.saveDocument(output);
        
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QVERIFY(data.contains("/Encrypt"));
    }

    void testEmptyPasswordHandling() {
        QString pdf = tmpPath("empty_pass.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        
        // Empty password should be gracefully handled, typically by applying standard security with empty password
        bool ok = engine.encryptDocument("", "", true, true, true);
        QVERIFY(ok);
        
        QString output = tmpPath("empty_pass_encrypted.pdf");
        engine.saveDocument(output);
        QVERIFY(QFile::exists(output));
    }
};

QTEST_GUILESS_MAIN(TestEncryption)
#include "TestEncryption.moc"
