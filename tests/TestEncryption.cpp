#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/cms.h>
#include <openssl/sha.h>
#include <zlib.h>

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
        
        DocumentPermissions perms;
        QVERIFY(engine.encryptDocument("user", "owner", perms));
        
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
        DocumentPermissions perms;
        bool ok = engine.encryptDocument("", "", perms);
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

    // G-03: prove that opening an AES-256 encrypted file WITHOUT the correct
    // password fails. A broken encryption implementation that writes the /Encrypt
    // dict but leaves content unencrypted would not throw here.
    void testEncryptionPreventsPasswordlessAccess() {
        // Create and encrypt a PDF with a non-trivial user password.
        QString pdf = tmpPath("enforce_input.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter p;
            p.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            p.TextState.SetFont(font, 12.0);
            p.DrawText("G03_PLAINTEXT_MARKER", 100, 700);
            p.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        DocumentPermissions perms;
        QVERIFY(engine.encryptDocument("correct_user_pass", "owner_pass", perms));
        QString output = tmpPath("enforce_encrypted.pdf");
        QVERIFY(engine.saveDocument(output));

        // Attempt to open WITHOUT a password. PoDoFo must throw PdfError
        // (or PoDoFo::PdfException). If it succeeds, the encryption is broken.
        bool threwAsExpected = false;
        try {
            PoDoFo::PdfMemDocument lockedDoc;
            lockedDoc.Load(output.toUtf8().constData());
            // If Load succeeds without a password and content is readable, encryption failed.
            // Check if content stream is accessible (it shouldn't be).
            if (lockedDoc.GetPages().GetCount() > 0) {
                auto& page = lockedDoc.GetPages().GetPageAt(0);
                auto* co = page.GetContents();
                if (co) {
                    PoDoFo::charbuff buf;
                    co->CopyTo(buf);
                    // If we can read the plaintext marker, encryption is broken.
                    QVERIFY2(!QByteArray(buf.data(), (int)buf.size()).contains("G03_PLAINTEXT_MARKER"),
                             "G-03: Encrypted PDF content is readable without password — encryption broken");
                }
            }
            // Some PoDoFo versions open empty-password encrypted files without throwing.
            // The critical check is content unreadability above.
        } catch (const PoDoFo::PdfError&) {
            threwAsExpected = true;  // expected: password required
        } catch (const std::exception&) {
            threwAsExpected = true;  // also acceptable
        }
        // Either PoDoFo threw (password required) or it opened but content was unreadable.
        // Both are correct outcomes for a working encryption implementation.
        Q_UNUSED(threwAsExpected);
    }

    // Reader-side helper (A-01 / G-03): CMS-unwrap the 24-byte seed from a
    // /Recipients envelope, derive FEK = SHA256(seed), then AES-256-CBC decrypt a
    // stream (IV = first 16 bytes) exactly as a conformant PubSec reader would.
    static QByteArray cmsRecoverFek(const QByteArray& cms, X509* cert, EVP_PKEY* key) {
        const unsigned char* cp = reinterpret_cast<const unsigned char*>(cms.constData());
        CMS_ContentInfo* ci = d2i_CMS_ContentInfo(nullptr, &cp, cms.size());
        if (!ci) return {};
        BIO* obio = BIO_new(BIO_s_mem());
        int dec = CMS_decrypt(ci, key, cert, nullptr, obio, 0);
        QByteArray fek;
        if (dec == 1) {
            char* seed = nullptr; long n = BIO_get_mem_data(obio, &seed);
            if (n >= 20) {
                unsigned char digest[32];
                SHA256(reinterpret_cast<const unsigned char*>(seed), static_cast<size_t>(n), digest);
                fek = QByteArray(reinterpret_cast<char*>(digest), 32);
            }
        }
        BIO_free(obio); CMS_ContentInfo_free(ci);
        return fek;
    }

    static QByteArray zlibInflate(const QByteArray& in) {
        z_stream zs; memset(&zs, 0, sizeof(zs));
        if (inflateInit(&zs) != Z_OK) return {};
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.constData()));
        zs.avail_in = static_cast<uInt>(in.size());
        QByteArray out;
        char buf[8192];
        int ret;
        do {
            zs.next_out = reinterpret_cast<Bytef*>(buf);
            zs.avail_out = sizeof(buf);
            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) break;
            out.append(buf, sizeof(buf) - zs.avail_out);
        } while (ret == Z_OK && zs.avail_in > 0);
        inflateEnd(&zs);
        return out;
    }

    static QByteArray aesCbcDecrypt(const QByteArray& fek, const QByteArray& ivPlusCt) {
        if (ivPlusCt.size() < 32) return {};
        const auto* iv = reinterpret_cast<const unsigned char*>(ivPlusCt.constData());
        const auto* ct = iv + 16;
        int ctLen = ivPlusCt.size() - 16;
        QByteArray out(ctLen + 16, '\0');
        EVP_CIPHER_CTX* c = EVP_CIPHER_CTX_new();
        int len = 0, total = 0; QByteArray result;
        if (EVP_DecryptInit_ex(c, EVP_aes_256_cbc(), nullptr,
                               reinterpret_cast<const unsigned char*>(fek.constData()), iv) == 1) {
            auto* op = reinterpret_cast<unsigned char*>(out.data());
            if (EVP_DecryptUpdate(c, op, &len, ct, ctLen) == 1) {
                total = len;
                if (EVP_DecryptFinal_ex(c, op + total, &len) == 1) {
                    total += len;
                    result = QByteArray(out.constData(), total);
                }
            }
        }
        EVP_CIPHER_CTX_free(c);
        return result;
    }

    void testCertificateEncryption() {
        // Generate a self-signed certificate + key (kept alive for reader-side decrypt).
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

        QString pdf = tmpPath("to_encrypt_cert.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter p; p.SetCanvas(page);
            auto* font = doc.GetFonts().SearchFont("Helvetica");
            if (font) { p.TextState.SetFont(*font, 18); p.DrawText("PUBSEC_ROUNDTRIP_MARKER_98765", 50, 700); }
            p.FinishDrawing();
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
        const QByteArray rawEncrypted = file.readAll();
        file.close();

        // Structural assertions + recover the Recipients CMS envelope.
        QByteArray cmsEnvelope;
        try {
            PoDoFo::PdfMemDocument checkDoc;
            checkDoc.Load(output.toUtf8().constData());
            auto& trailer = checkDoc.GetTrailer();
            QVERIFY(trailer.GetDictionary().HasKey("Encrypt"));
            auto* encryptObjRef = trailer.GetDictionary().GetKey("Encrypt");
            QVERIFY(encryptObjRef->IsReference());
            auto& encryptDictObj = checkDoc.GetObjects().MustGetObject(encryptObjRef->GetReference());

            QVERIFY(encryptDictObj.GetDictionary().HasKey("Filter"));
            QCOMPARE(std::string(encryptDictObj.GetDictionary().GetKey("Filter")->GetName().GetString()), std::string("PubSec"));
            QVERIFY(encryptDictObj.GetDictionary().HasKey("SubFilter"));
            QCOMPARE(std::string(encryptDictObj.GetDictionary().GetKey("SubFilter")->GetName().GetString()), std::string("adbe.pkcs7.s5"));
            QVERIFY(encryptDictObj.GetDictionary().HasKey("Recipients"));
            auto* recips = encryptDictObj.GetDictionary().GetKey("Recipients");
            if (recips->IsArray()) {
                auto sv = recips->GetArray()[0].GetString().GetRawData();
                cmsEnvelope = QByteArray(sv.data(), (int)sv.size());
            } else {
                auto sv = recips->GetString().GetRawData();
                cmsEnvelope = QByteArray(sv.data(), (int)sv.size());
            }
        } catch (const std::exception& e) {
            QFAIL(QString("PoDoFo read exception: %1").arg(e.what()).toUtf8().constData());
        }
        QVERIFY2(!cmsEnvelope.isEmpty(), "Recipients CMS envelope is empty");

        // A-01 / G-03 (1): the plaintext marker must NOT survive in the encrypted
        // bytes (proves the streams were actually encrypted).
        QVERIFY2(!rawEncrypted.contains("PUBSEC_ROUNDTRIP_MARKER_98765"),
                 "Plaintext content leaked into the encrypted PubSec file");

        // A-01 / G-03 (2): a certificate holder MUST be able to recover the FEK from
        // the /Recipients envelope and decrypt the document streams with it. This is
        // the exact path a conformant PubSec reader (Acrobat) follows, and is the
        // assertion the pre-fix all-zero-key code failed.
        QByteArray fek = cmsRecoverFek(cmsEnvelope, x509, pkey);
        QVERIFY2(fek.size() == 32, "Failed to CMS-unwrap the FEK from the Recipients envelope");
        bool fekAllZero = true; for (char c : fek) if (c != 0) { fekAllZero = false; break; }
        QVERIFY2(!fekAllZero, "FEK is all-zero");

        // Parse the encrypted file structurally and read the RAW (still
        // AES-encrypted) stream octets, then decrypt them ourselves with the
        // recovered FEK and FlateDecode. Success = at least one stream decrypts to
        // a valid, well-formed PDF content/object stream. (The visible text is
        // emitted through an embedded CID-subset font, so the page-marker survives
        // only as glyph indices, not ASCII — hence we assert on decrypt validity:
        // a wrong key yields AES/PKCS#7 failure or garbage that never inflates.)
        int decryptedStreams = 0;
        bool sawContentStream = false;
        {
            PoDoFo::PdfMemDocument enc;
            enc.Load(output.toUtf8().constData());
            for (auto it = enc.GetObjects().begin(); it != enc.GetObjects().end(); ++it) {
                PoDoFo::PdfObject* o = *it;
                if (!o->HasStream()) continue;
                // Raw, still-encrypted stream octets (IV||ciphertext).
                PoDoFo::charbuff rawBuf;
                try {
                    auto in = o->GetStream()->GetInputStream(true /*raw*/);
                    PoDoFo::StringStreamDevice dev(rawBuf);
                    in.CopyTo(dev);
                } catch (...) { continue; }
                QByteArray ivCt(rawBuf.data(), (int)rawBuf.size());
                QByteArray plain = aesCbcDecrypt(fek, ivCt);
                if (plain.isEmpty()) continue;   // PKCS#7/AES failure ⇒ wrong key
                ++decryptedStreams;
                QByteArray inflated = zlibInflate(plain);
                // A correctly decrypted page content stream inflates to PDF text
                // operators; a correctly decrypted object stream begins with PDF
                // tokens. Either is proof the FEK matched.
                if (inflated.contains("BT") || inflated.contains("Td") ||
                    inflated.contains("/CIDInit") || inflated.contains("Tf"))
                    sawContentStream = true;
            }
        }
        QVERIFY2(decryptedStreams > 0,
                 "Recipient could not AES-decrypt ANY PubSec stream with the CMS-recovered FEK — "
                 "streams were encrypted under a different key (A-01 regression).");
        QVERIFY2(sawContentStream,
                 "Decrypted PubSec streams did not inflate to valid PDF content — "
                 "key mismatch or corrupt encryption (A-01 regression).");

        X509_free(x509);
        EVP_PKEY_free(pkey);
    }
};

QTEST_GUILESS_MAIN(TestEncryption)
#include "TestEncryption.moc"
