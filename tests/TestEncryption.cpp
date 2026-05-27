#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

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
        QVERIFY2(data.contains("/Encrypt"), "Saved PDF must have an /Encrypt dictionary");
        QVERIFY2(data.contains("/V 5"), "Encryption must use V=5 (AES-256)");
        QVERIFY2(data.contains("/R 6"), "Encryption revision must be R=6 (AES-256 R6)");
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

        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QVERIFY2(data.contains("/Encrypt"), "Saved PDF must have an /Encrypt dictionary");
        QVERIFY2(data.contains("/V 5"), "Encryption must use V=5 (AES-256)");
        QVERIFY2(data.contains("/R 6"), "Encryption revision must be R=6 (AES-256 R6)");
    }

    void testCertificateEncryption() {
        // Generate a self-signed certificate programmatically
        
        EVP_PKEY* pkey = EVP_PKEY_new();
        BIGNUM* bn = BN_new();
        BN_set_word(bn, RSA_F4);
        RSA* rsa = RSA_new();
        RSA_generate_key_ex(rsa, 2048, bn, nullptr);
        EVP_PKEY_assign_RSA(pkey, rsa);
        BN_free(bn);

        X509* x509 = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
        X509_gmtime_adj(X509_get_notBefore(x509), 0);
        X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
        X509_set_pubkey(x509, pkey);

        X509_NAME* name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"Test", -1, -1, 0);
        X509_set_issuer_name(x509, name);
        X509_sign(x509, pkey, EVP_sha256());

        QString certPath = tmpPath("test_cert.pem");
        FILE* f = fopen(certPath.toUtf8().constData(), "wb");
        QVERIFY(f != nullptr);
        PEM_write_X509(f, x509);
        fclose(f);

        X509_free(x509);
        EVP_PKEY_free(pkey);

        QString pdf = tmpPath("to_encrypt_cert.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QString output = tmpPath("encrypted_cert.pdf");
        bool ok = engine.encryptWithCertificate(pdf, output, { certPath });
        if (!ok) {
            qDebug() << "Error User Message:" << engine.lastError().userMessage;
            qDebug() << "Error Technical Details:" << engine.lastError().technicalDetails;
        }
        QVERIFY(ok);

        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        
        try {
            PoDoFo::PdfMemDocument checkDoc;
            checkDoc.Load(output.toUtf8().constData());
            auto& trailer = checkDoc.GetTrailer();
            QVERIFY(trailer.GetDictionary().HasKey("Encrypt"));
            auto* encryptObjRef = trailer.GetDictionary().GetKey("Encrypt");
            QVERIFY(encryptObjRef->IsReference());
            auto& encryptDictObj = checkDoc.GetObjects().MustGetObject(encryptObjRef->GetReference());
            
            QVERIFY(encryptDictObj.GetDictionary().HasKey("Filter"));
            qDebug() << "Actual Filter Name:" << std::string(encryptDictObj.GetDictionary().GetKey("Filter")->GetName().GetString()).c_str();
            QCOMPARE(std::string(encryptDictObj.GetDictionary().GetKey("Filter")->GetName().GetString()), std::string("PubSec"));
            
            QVERIFY(encryptDictObj.GetDictionary().HasKey("SubFilter"));
            QCOMPARE(std::string(encryptDictObj.GetDictionary().GetKey("SubFilter")->GetName().GetString()), std::string("adbe.pkcs7.s5"));
            
            QVERIFY(encryptDictObj.GetDictionary().HasKey("Recipients"));
        } catch (const std::exception& e) {
            QFAIL(QString("PoDoFo read exception: %1").arg(e.what()).toUtf8().constData());
        }
    }
};

QTEST_GUILESS_MAIN(TestEncryption)
#include "TestEncryption.moc"
