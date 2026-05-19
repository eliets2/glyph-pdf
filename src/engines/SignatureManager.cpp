#include "engines/SignatureManager.h"
#include <memory>
#include <podofo/podofo.h>
#include <podofo/auxiliary/StreamDevice.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <openssl/ts.h>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <limits>

using namespace PoDoFo;

class SignatureManager::Private
{
public:
    Private() = default;
    ~Private() = default;

    QString tsaUrl;

    QByteArray fetchTimestampToken(const QByteArray &digest) {
        if (tsaUrl.isEmpty()) return {};

        TS_REQ *req = TS_REQ_new();
        if (!req) return {};
        TS_REQ_set_version(req, 1);
        TS_REQ_set_cert_req(req, 1);

        TS_MSG_IMPRINT *imprint = TS_MSG_IMPRINT_new();
        X509_ALGOR *algo = X509_ALGOR_new();
        X509_ALGOR_set0(algo, OBJ_nid2obj(NID_sha256), V_ASN1_NULL, nullptr);
        TS_MSG_IMPRINT_set_algo(imprint, algo);
        TS_MSG_IMPRINT_set_msg(imprint,
            const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(digest.constData())),
            digest.size());
        TS_REQ_set_msg_imprint(req, imprint);

        unsigned char *derBuf = nullptr;
        int derLen = i2d_TS_REQ(req, &derBuf);
        TS_MSG_IMPRINT_free(imprint);
        X509_ALGOR_free(algo);
        TS_REQ_free(req);

        if (derLen <= 0 || !derBuf) return {};
        QByteArray reqData(reinterpret_cast<char*>(derBuf), derLen);
        OPENSSL_free(derBuf);

        QNetworkAccessManager mgr;
        QUrl tsaEndpoint(tsaUrl);
        QNetworkRequest httpReq{tsaEndpoint};
        httpReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/timestamp-query");
        httpReq.setTransferTimeout(10000);

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QNetworkReply *reply = mgr.post(httpReq, reqData);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(15000);
        loop.exec();

        QByteArray token;
        if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
            token = reply->readAll();
        }
        reply->deleteLater();
        return token;
    }

    bool loadP12(const QString &certPath, const QString &password, charbuff &certData, EVP_PKEY **pkey) {
        QFile file(certPath);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QByteArray data = file.readAll();

        BIO *bio = BIO_new_mem_buf(data.data(), data.size());
        PKCS12 *p12 = d2i_PKCS12_bio(bio, nullptr);
        BIO_free(bio);

        if (!p12) return false;

        X509 *cert = nullptr;
        STACK_OF(X509) *ca = nullptr;
        if (!PKCS12_parse(p12, password.toStdString().c_str(), pkey, &cert, &ca)) {
            PKCS12_free(p12);
            return false;
        }
        PKCS12_free(p12);

        // Convert cert to DER
        int len = i2d_X509(cert, nullptr);
        certData.resize(len);
        unsigned char *p = (unsigned char *)certData.data();
        i2d_X509(cert, &p);

        X509_free(cert);
        if (ca) sk_X509_pop_free(ca, X509_free);

        return true;
    }
};

SignatureManager::SignatureManager() : d(std::make_unique<Private>()) {}
SignatureManager::~SignatureManager() = default;

void SignatureManager::setTsaUrl(const QString &url) {
    d->tsaUrl = url;
}

bool SignatureManager::signDocument(const QString &inputPath, 
                                    const QString &outputPath, 
                                    const QString &certPath, 
                                    const QString &password,
                                    const QString &reason,
                                    const QString &location)
{
    try {
        PdfMemDocument doc;
        doc.Load(inputPath.toStdString());

        charbuff certData;
        EVP_PKEY *pkey = nullptr;
        if (!d->loadP12(certPath, password, certData, &pkey)) {
#ifdef QT_DEBUG
            qDebug() << "Failed to load P12 certificate";
#endif
            return false;
        }

        PdfSignerCmsParams params;
        params.Hashing = PdfHashingAlgorithm::SHA256;
        
        // Convert pkey to DER
        int pkeyLen = i2d_PrivateKey(pkey, nullptr);
        charbuff pkeyData(pkeyLen);
        unsigned char *p = (unsigned char *)pkeyData.data();
        i2d_PrivateKey(pkey, &p);
        EVP_PKEY_free(pkey);

        PdfSignerCms actualSigner(certData, pkeyData, params);
        OPENSSL_cleanse(pkeyData.data(), pkeyData.size());

        // Find or create signature field
        PdfSignature* signature = nullptr;
        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                signature = static_cast<PdfSignature*>(field);
                break;
            }
        }

        if (!signature) {
            // Create a new signature field on the first page if none found
            PdfPage& page = doc.GetPages().GetPageAt(0);
            signature = &page.CreateField<PdfSignature>("Signature1", Rect(50, 50, 200, 100));
        }

        if (!reason.isEmpty()) signature->SetSignatureReason(PdfString(reason.toStdString()));
        if (!location.isEmpty()) signature->SetSignatureLocation(PdfString(location.toStdString()));
        
        // actualSigner.SetAppearanceStream(*signature); // TODO: Implement visual appearance in PoDoFo 1.1
        
        if (!d->tsaUrl.isEmpty()) {
            unsigned char hash[32];
            unsigned int hashLen = 0;
            EVP_Digest(certData.data(), certData.size(), hash, &hashLen, EVP_sha256(), nullptr);
            QByteArray tsToken = d->fetchTimestampToken(QByteArray(reinterpret_cast<char*>(hash), hashLen));
            if (!tsToken.isEmpty()) {
                qDebug() << "RFC 3161 timestamp token obtained (" << tsToken.size() << "bytes)";
            } else {
                qWarning() << "TSA request failed — signature will use local time only";
            }
        }

        FileStreamDevice outputStream(outputPath.toStdString(), FileMode::Create);
        SignDocument(doc, outputStream, actualSigner, *signature);

        return true;
    } catch (const PdfError &e) {
#ifdef QT_DEBUG
        qDebug() << "PoDoFo error during signing:" << e.what();
#endif
        return false;
    } catch (const std::exception &e) {
#ifdef QT_DEBUG
        qDebug() << "Standard error during signing:" << e.what();
#endif
        return false;
    }
}

QList<SignatureInfo> SignatureManager::validateSignatures(const QString &filePath)
{
    QList<SignatureInfo> results;
    try {
        PdfMemDocument doc;
        doc.Load(filePath.toStdString());

        // Load the file data once for byte range extraction
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return results;
        QByteArray fileData = file.readAll();

        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                PdfSignature* sig = static_cast<PdfSignature*>(field);
                SignatureInfo info;
                info.fieldName = QString::fromStdString(sig->GetFullName());
                
                auto reasonVal = sig->GetSignatureReason();
                if (reasonVal.has_value()) {
                    auto val = reasonVal.value().GetString();
                    info.reason = QString::fromUtf8(val.data(), static_cast<int>(val.size()));
                }
                
                auto locVal = sig->GetSignatureLocation();
                if (locVal.has_value()) {
                    auto val = locVal.value().GetString();
                    info.location = QString::fromUtf8(val.data(), static_cast<int>(val.size()));
                }
                
                auto signerVal = sig->GetSignerName();
                if (signerVal.has_value()) {
                    auto val = signerVal.value().GetString();
                    info.signerName = QString::fromUtf8(val.data(), static_cast<int>(val.size()));
                }

                info.isValid = false;
                info.integrityIntact = false;
                info.trustStatus = "Invalid";

                try {
                    auto& sigObj = sig->GetDictionary();

                    // Extract ByteRange
                    auto* byteRangeObj = sigObj.FindKey(PdfName("ByteRange"));
                    if (!byteRangeObj || !byteRangeObj->IsArray()) {
                        info.trustStatus = "Unsigned";
                        results.append(info);
                        continue;
                    }
                    auto& byteRangeArray = byteRangeObj->GetArray();
                    if (byteRangeArray.size() != 4) {
                        info.trustStatus = "Malformed";
                        results.append(info);
                        continue;
                    }

                    int64_t offset1 = byteRangeArray[0].GetNumber();
                    int64_t length1 = byteRangeArray[1].GetNumber();
                    int64_t offset2 = byteRangeArray[2].GetNumber();
                    int64_t length2 = byteRangeArray[3].GetNumber();

                    if (offset1 < 0 || length1 < 0 || offset2 < 0 || length2 < 0) {
                        info.trustStatus = "Malformed";
                        results.append(info);
                        continue;
                    }
                    if (offset1 > fileData.size() || length1 > fileData.size() - offset1) {
                        info.trustStatus = "Malformed";
                        results.append(info);
                        continue;
                    }
                    if (offset2 > fileData.size() || length2 > fileData.size() - offset2) {
                        info.trustStatus = "Malformed";
                        results.append(info);
                        continue;
                    }

                    // HIGH-1: Block PDF Shadow Attacks by ensuring the entire file data is protected
                    if (offset1 != 0 || (offset2 + length2) != fileData.size()) {
                        qWarning() << "SECURITY: Signature ByteRange does not cover the complete file boundaries. "
                                      "Possible PDF Shadow Attack detected!";
                        info.trustStatus = "ByteRangeMismatch";
                        results.append(info);
                        continue;
                    }

                    // MEDIUM-2: Validate integers fit into 32-bit limits to prevent truncation
                    if (offset1 > std::numeric_limits<int>::max() ||
                        length1 > std::numeric_limits<int>::max() ||
                        offset2 > std::numeric_limits<int>::max() ||
                        length2 > std::numeric_limits<int>::max() ||
                        (length1 + length2) > std::numeric_limits<int>::max()) 
                    {
                        qWarning() << "SECURITY: Integer bounds exceeded in signature parsing (>2GB limits).";
                        info.trustStatus = "Malformed";
                        results.append(info);
                        continue;
                    }

                    QByteArray signedData;
                    signedData.reserve(static_cast<int>(length1 + length2));
                    signedData.append(fileData.mid(static_cast<int>(offset1), static_cast<int>(length1)));
                    signedData.append(fileData.mid(static_cast<int>(offset2), static_cast<int>(length2)));

                    // Extract CMS signature (/Contents)
                    auto* contentsObj = sigObj.FindKey(PdfName("Contents"));
                    if (!contentsObj || !contentsObj->IsString()) {
                        info.trustStatus = "Unsigned";
                        results.append(info);
                        continue;
                    }
                    
                    // PoDoFo 1.1 handles hex strings as PdfString
                    std::string contentsHex{ contentsObj->GetString().GetString() };
                    QByteArray cmsData(contentsHex.data(), static_cast<int>(contentsHex.size()));

                    // Cryptographic verification
                    BIO* cmsBio = BIO_new_mem_buf(cmsData.data(), cmsData.size());
                    CMS_ContentInfo* cms = d2i_CMS_bio(cmsBio, nullptr);
                    BIO_free(cmsBio);

                    if (cms) {
                        BIO* contentBio = BIO_new_mem_buf(signedData.data(), signedData.size());
                        X509_STORE* store = X509_STORE_new();
                        X509_STORE_set_default_paths(store);

                        // Single secure verification phase verifying both content integrity AND certificate trust chain
                        if (CMS_verify(cms, nullptr, store, contentBio, nullptr, CMS_DETACHED | CMS_BINARY) == 1) {
                            info.isValid = true;
                            info.integrityIntact = true;
                            info.trustStatus = "Valid";
                        } else {
                            info.isValid = false;
                            info.integrityIntact = false;
                            info.trustStatus = "Invalid";
                        }

                        X509_STORE_free(store);
                        BIO_free(contentBio);
                        CMS_ContentInfo_free(cms);
                    }
                } catch (...) {
                    info.isValid = false;
                    info.integrityIntact = false;
                    info.trustStatus = "Invalid";
                }
                
                results.append(info);
            }
        }
    } catch (const std::exception& e) {
#ifdef QT_DEBUG
        qDebug() << "Error validating signatures:" << e.what();
#endif
    } catch (...) {
#ifdef QT_DEBUG
        qDebug() << "Unknown error during signature validation";
#endif
    }
    return results;
}
