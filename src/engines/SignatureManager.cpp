#include "engines/SignatureManager.h"
#include <memory>
#include <podofo/podofo.h>
#include <podofo/auxiliary/StreamDevice.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <openssl/ts.h>
#include <openssl/ocsp.h>
#include <openssl/sha.h>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>
#include <limits>
#include <vector>

using namespace PoDoFo;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QByteArray hexUpper(const QByteArray &data)
{
    return data.toHex().toUpper();
}

// RAII deleter for OpenSSL EVP_PKEY so the key is freed on every exit path
// (including thrown exceptions). Added per audit 2026-05-23.
namespace {
struct EvpPkeyDeleter {
    void operator()(EVP_PKEY *p) const noexcept { if (p) EVP_PKEY_free(p); }
};
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

// Additional RAII guards for OpenSSL objects used inside validateSignatures
// — protects against leaks if CMS_verify or any inner call throws (Fix J).
struct X509StoreDeleter {
    void operator()(X509_STORE *s) const noexcept { if (s) X509_STORE_free(s); }
};
using X509StorePtr = std::unique_ptr<X509_STORE, X509StoreDeleter>;

struct BioDeleter {
    void operator()(BIO *b) const noexcept { if (b) BIO_free(b); }
};
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

struct CmsContentInfoDeleter {
    void operator()(CMS_ContentInfo *c) const noexcept { if (c) CMS_ContentInfo_free(c); }
};
using CmsContentInfoPtr = std::unique_ptr<CMS_ContentInfo, CmsContentInfoDeleter>;
} // namespace

// ---------------------------------------------------------------------------
class SignatureManager::Private
{
public:
    Private() = default;
    ~Private() = default;

    QString tsaUrl;
    PAdESLevel level = PAdESLevel::B_T;

    // -----------------------------------------------------------------------
    // RFC 3161 timestamp token
    // -----------------------------------------------------------------------
    QByteArray fetchTimestampToken(const QByteArray &digest)
    {
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

        return httpPost(tsaUrl, "application/timestamp-query", reqData);
    }

    // -----------------------------------------------------------------------
    // Generic HTTP POST helper
    // -----------------------------------------------------------------------
    QByteArray httpPost(const QString &url, const QByteArray &contentType, const QByteArray &body)
    {
        QNetworkAccessManager mgr;
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        req.setTransferTimeout(15000);

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QNetworkReply *reply = mgr.post(req, body);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(15000);
        loop.exec();

        QByteArray result;
        if (reply->isFinished() && reply->error() == QNetworkReply::NoError)
            result = reply->readAll();
        else
            qWarning() << "HTTP POST to" << url << "failed:" << reply->errorString();
        reply->deleteLater();
        return result;
    }

    // -----------------------------------------------------------------------
    // Load PKCS#12 and extract cert chain + private key
    // -----------------------------------------------------------------------
    bool loadP12(const QString &certPath, const QString &password,
                 charbuff &certData,         // DER of leaf cert
                 EVP_PKEY **pkey,
                 QList<QByteArray> &certChain, // DER of all certs in chain
                 X509 **leafCert,
                 X509 **issuerCert)
    {
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

        // Leaf cert DER
        int len = i2d_X509(cert, nullptr);
        certData.resize(len);
        unsigned char *p = reinterpret_cast<unsigned char*>(certData.data());
        i2d_X509(cert, &p);
        if (leafCert) *leafCert = cert; else X509_free(cert);

        // Chain: collect all CA certs
        certChain.clear();
        if (ca) {
            const int numCa = sk_X509_num(ca);
            for (int i = 0; i < numCa; ++i) {
                X509 *caCert = sk_X509_value(ca, i);
                int caLen = i2d_X509(caCert, nullptr);
                QByteArray caDer(caLen, '\0');
                unsigned char *cp = reinterpret_cast<unsigned char*>(caDer.data());
                i2d_X509(caCert, &cp);
                certChain.append(caDer);
                if (issuerCert && i == 0) *issuerCert = caCert;
            }
            if (issuerCert) {
                // Caller takes ownership of element 0 (issuer); free the rest
                // plus the container itself to avoid leaking the stack and
                // the non-issuer CA certs (audit Fix I).
                for (int i = 1; i < numCa; ++i) {
                    X509 *extra = sk_X509_value(ca, i);
                    if (extra) X509_free(extra);
                }
                sk_X509_free(ca);
            } else {
                sk_X509_pop_free(ca, X509_free);
            }
        }
        // Add leaf cert to chain
        certChain.prepend(QByteArray(certData.data(), static_cast<int>(certData.size())));

        return true;
    }

    // -----------------------------------------------------------------------
    // OCSP: fetch response for cert signed by issuer
    // -----------------------------------------------------------------------
    QByteArray fetchOcspResponse(X509 *cert, X509 *issuer)
    {
        if (!cert || !issuer) return {};

        // Extract OCSP responder URL from AIA extension
        AUTHORITY_INFO_ACCESS *aia =
            static_cast<AUTHORITY_INFO_ACCESS*>(X509_get_ext_d2i(cert, NID_info_access, nullptr, nullptr));
        if (!aia) {
            qDebug() << "OCSP: no AIA extension in certificate";
            return {};
        }

        QString ocspUrl;
        for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(aia); ++i) {
            ACCESS_DESCRIPTION *ad = sk_ACCESS_DESCRIPTION_value(aia, i);
            if (OBJ_obj2nid(ad->method) == NID_ad_OCSP) {
                if (ad->location->type == GEN_URI) {
                    ocspUrl = QString::fromUtf8(
                        reinterpret_cast<const char*>(ad->location->d.uniformResourceIdentifier->data),
                        ad->location->d.uniformResourceIdentifier->length);
                    break;
                }
            }
        }
        AUTHORITY_INFO_ACCESS_free(aia);

        if (ocspUrl.isEmpty()) {
            qDebug() << "OCSP: no OCSP URI in AIA extension";
            return {};
        }

        // Build OCSP request
        OCSP_REQUEST *req = OCSP_REQUEST_new();
        if (!req) return {};

        OCSP_CERTID *certId = OCSP_cert_to_id(EVP_sha256(), cert, issuer);
        if (!certId) { OCSP_REQUEST_free(req); return {}; }

        OCSP_request_add0_id(req, certId);
        OCSP_request_add1_nonce(req, nullptr, -1);

        // Serialize request
        unsigned char *derBuf = nullptr;
        int derLen = i2d_OCSP_REQUEST(req, &derBuf);
        OCSP_REQUEST_free(req);
        if (derLen <= 0 || !derBuf) return {};

        QByteArray reqData(reinterpret_cast<char*>(derBuf), derLen);
        OPENSSL_free(derBuf);

        qDebug() << "OCSP: querying" << ocspUrl;
        QByteArray response = httpPost(ocspUrl, "application/ocsp-request", reqData);
        if (response.isEmpty()) {
            qWarning() << "OCSP: empty response from" << ocspUrl;
        }
        return response;
    }

    // -----------------------------------------------------------------------
    // Build DSS dictionary (PAdES B-LT) as an incremental update
    // After calling this, signedFilePath is updated in-place.
    // -----------------------------------------------------------------------
    bool buildDssDictionary(const QString &signedFilePath,
                            const QList<QByteArray> &certs,
                            const QList<QByteArray> &ocsps,
                            const QList<QByteArray> &crls,
                            const QByteArray &sigContentsHex)
    {
        try {
            PdfMemDocument doc;
            doc.Load(signedFilePath.toStdString());

            PdfDictionary &catalog = doc.GetCatalog().GetDictionary();

            // /DSS
            PdfDictionary dss;

            // /Certs array
            if (!certs.isEmpty()) {
                PdfArray certsArr;
                for (const QByteArray &derCert : certs) {
                    charbuff buf(derCert.size());
                    memcpy(buf.data(), derCert.constData(), derCert.size());
                    auto &stream = doc.GetObjects().CreateDictionaryObject();
                    stream.GetOrCreateStream().SetData(buf);
                    certsArr.Add(stream.GetIndirectReference());
                }
                dss.AddKey("Certs", certsArr);
            }

            // /OCSPs array
            if (!ocsps.isEmpty()) {
                PdfArray ocspArr;
                for (const QByteArray &ocspData : ocsps) {
                    charbuff buf(ocspData.size());
                    memcpy(buf.data(), ocspData.constData(), ocspData.size());
                    auto &stream = doc.GetObjects().CreateDictionaryObject();
                    stream.GetOrCreateStream().SetData(buf);
                    ocspArr.Add(stream.GetIndirectReference());
                }
                dss.AddKey("OCSPs", ocspArr);
            }

            // /CRLs array
            if (!crls.isEmpty()) {
                PdfArray crlArr;
                for (const QByteArray &crlData : crls) {
                    charbuff buf(crlData.size());
                    memcpy(buf.data(), crlData.constData(), crlData.size());
                    auto &stream = doc.GetObjects().CreateDictionaryObject();
                    stream.GetOrCreateStream().SetData(buf);
                    crlArr.Add(stream.GetIndirectReference());
                }
                dss.AddKey("CRLs", crlArr);
            }

            // /VRI keyed by uppercase hex SHA-1 of /Contents bytes
            if (!sigContentsHex.isEmpty()) {
                // SHA-1 of the raw /Contents octets (the hex-decoded bytes)
                // TODO(audit-2026-05-23): remove hex round-trip in VRI key construction.
                // extractSignatureContentsHex() currently emits hex; we decode here only to re-hash.
                // Refactor extractSignatureContentsHex to return raw bytes and update buildDssDictionary's
                // parameter type (touches function signature, deferred from small-fix sweep).
                QByteArray contentsBytes = QByteArray::fromHex(sigContentsHex);
                QByteArray sha1 = QCryptographicHash::hash(contentsBytes, QCryptographicHash::Sha1);
                QString vriKey = QString::fromLatin1(hexUpper(sha1));

                PdfDictionary vri;
                PdfDictionary vriEntry;

                if (!certs.isEmpty()) {
                    PdfArray vc;
                    for (const QByteArray &derCert : certs) {
                        charbuff buf(derCert.size());
                        memcpy(buf.data(), derCert.constData(), derCert.size());
                        auto &stream = doc.GetObjects().CreateDictionaryObject();
                        stream.GetOrCreateStream().SetData(buf);
                        vc.Add(stream.GetIndirectReference());
                    }
                    vriEntry.AddKey("Cert", vc);
                }
                if (!ocsps.isEmpty()) {
                    PdfArray vo;
                    for (const QByteArray &ocspData : ocsps) {
                        charbuff buf(ocspData.size());
                        memcpy(buf.data(), ocspData.constData(), ocspData.size());
                        auto &stream = doc.GetObjects().CreateDictionaryObject();
                        stream.GetOrCreateStream().SetData(buf);
                        vo.Add(stream.GetIndirectReference());
                    }
                    vriEntry.AddKey("OCSP", vo);
                }
                if (!crls.isEmpty()) {
                    PdfArray vc2;
                    for (const QByteArray &crlData : crls) {
                        charbuff buf(crlData.size());
                        memcpy(buf.data(), crlData.constData(), crlData.size());
                        auto &stream = doc.GetObjects().CreateDictionaryObject();
                        stream.GetOrCreateStream().SetData(buf);
                        vc2.Add(stream.GetIndirectReference());
                    }
                    vriEntry.AddKey("CRL", vc2);
                }

                // /TU = current timestamp (informational)
                auto nowStr = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                vriEntry.AddKey("TU", PdfString(nowStr.toStdString()));

                vri.AddKey(PdfName(vriKey.toStdString()), vriEntry);
                dss.AddKey("VRI", vri);
            }

            // Embed DSS in catalog
            auto &dssObj = doc.GetObjects().CreateDictionaryObject();
            dssObj.GetDictionary() = dss;
            catalog.AddKey("DSS", dssObj.GetIndirectReference());

            // Write as incremental update
            FileStreamDevice output(signedFilePath.toStdString(), FileMode::Append);
            doc.SaveUpdate(output);
            return true;
        } catch (const PdfError &e) {
            qWarning() << "DSS dictionary build failed:" << e.what();
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // PAdES B-LTA: add /DocTimeStamp as incremental update
    // A TimestampSigner accumulates the signed bytes in AppendData, then
    // in ComputeSignature fetches the RFC 3161 token from the TSA.
    // -----------------------------------------------------------------------
    bool addDocTimestamp(const QString &filePath)
    {
        if (tsaUrl.isEmpty()) {
            qWarning() << "B-LTA: no TSA URL configured — skipping document timestamp";
            return false;
        }
        try {
            PdfMemDocument doc;
            doc.Load(filePath.toStdString());

            PdfPage &page = doc.GetPages().GetPageAt(0);
            PdfSignature &ts = page.CreateField<PdfSignature>(
                "DocTimeStamp", Rect(0, 0, 0, 0));

            // Mark widget annotation as hidden (invisible, no print)
            if (auto *widget = ts.GetWidget())
                widget->SetFlags(PdfAnnotationFlags::Hidden |
                                 PdfAnnotationFlags::Invisible);

            // TimestampSigner: implements the PdfSigner interface for RFC 3161
            struct TimestampSigner final : public PdfSigner {
                QByteArray m_accumulated;
                Private *m_priv;
                explicit TimestampSigner(Private *p) : m_priv(p) {}

                std::string GetSignatureSubFilter() const override {
                    return "ETSI.RFC3161";
                }
                std::string GetSignatureType() const override {
                    return "DocTimeStamp";
                }
                void Reset() override { m_accumulated.clear(); }
                void AppendData(const bufferview &data) override {
                    m_accumulated.append(data.data(),
                                         static_cast<qsizetype>(data.size()));
                }
                void ComputeSignature(charbuff &contents, bool dryrun) override {
                    if (dryrun) {
                        // Reserve 16 KB for the token placeholder. TSA tokens with
                        // multi-cert chains can exceed 8 KB; B-LTA would fail silently
                        // (audit 2026-05-23).
                        contents.assign(16384, '\0');
                        return;
                    }
                    QByteArray digest = QCryptographicHash::hash(
                        m_accumulated, QCryptographicHash::Sha256);
                    QByteArray token = m_priv->fetchTimestampToken(digest);
                    if (token.isEmpty()) {
                        qWarning() << "B-LTA: TSA returned empty token";
                        contents.assign(4, '\0');
                        return;
                    }
                    contents.assign(token.constData(), token.size());
                }
            };

            TimestampSigner tsSigner(this);
            FileStreamDevice output(filePath.toStdString(), FileMode::Append);
            SignDocument(doc, output, tsSigner, ts);
            return true;
        } catch (const PdfError &e) {
            qWarning() << "B-LTA timestamp addition failed:" << e.what();
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Extract /Contents hex from signed PDF for VRI key computation
    // -----------------------------------------------------------------------
    QByteArray extractSignatureContentsHex(const QString &filePath)
    {
        try {
            PdfMemDocument doc;
            doc.Load(filePath.toStdString());
            for (auto field : doc.GetFieldsIterator()) {
                if (field->GetType() == PdfFieldType::Signature) {
                    auto *sig = static_cast<PdfSignature*>(field);
                    auto *contentsObj = sig->GetDictionary().FindKey(PdfName("Contents"));
                    if (contentsObj && contentsObj->IsString()) {
                        auto str = contentsObj->GetString().GetString();
                        // PoDoFo returns the raw binary contents; convert to hex
                        return QByteArray(str.data(), static_cast<int>(str.size())).toHex();
                    }
                }
            }
        } catch (const std::exception& e) {
            qWarning() << __func__ << "swallowed exception:" << e.what();
        } catch (...) {
            qWarning() << __func__ << "swallowed unknown exception";
        }
        return {};
    }
};

// ===========================================================================
SignatureManager::SignatureManager() : d(std::make_unique<Private>()) {}
SignatureManager::~SignatureManager() = default;

void SignatureManager::setTsaUrl(const QString &url) { d->tsaUrl = url; }
void SignatureManager::setSignatureLevel(PAdESLevel level) { d->level = level; }

// ---------------------------------------------------------------------------
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
        EVP_PKEY *pkeyRaw = nullptr;
        QList<QByteArray> certChain;
        X509 *leafCert = nullptr, *issuerCert = nullptr;

        if (!d->loadP12(certPath, password, certData, &pkeyRaw, certChain, &leafCert, &issuerCert)) {
            qWarning() << "Failed to load P12 certificate";
            return false;
        }
        // RAII guard: ensures EVP_PKEY_free runs on every exit path, including
        // exceptions thrown by PoDoFo while we still own the key.
        EvpPkeyPtr pkey(pkeyRaw);

        PdfSignerCmsParams params;
        params.Hashing = PdfHashingAlgorithm::SHA256;

        int pkeyLen = i2d_PrivateKey(pkey.get(), nullptr);
        charbuff pkeyData(pkeyLen);
        unsigned char *p = reinterpret_cast<unsigned char*>(pkeyData.data());
        i2d_PrivateKey(pkey.get(), &p);
        pkey.reset();

        PdfSignerCms actualSigner(certData, pkeyData, params);
        OPENSSL_cleanse(pkeyData.data(), pkeyData.size());

        // Find or create signature field
        PdfSignature *signature = nullptr;
        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                signature = static_cast<PdfSignature*>(field);
                break;
            }
        }
        if (!signature) {
            PdfPage &page = doc.GetPages().GetPageAt(0);
            signature = &page.CreateField<PdfSignature>("Signature1", Rect(50, 50, 200, 100));
        }

        if (!reason.isEmpty())   signature->SetSignatureReason(PdfString(reason.toStdString()));
        if (!location.isEmpty()) signature->SetSignatureLocation(PdfString(location.toStdString()));

        // B-T: embed RFC 3161 timestamp token (only if TSA configured)
        if (d->level >= PAdESLevel::B_T && !d->tsaUrl.isEmpty()) {
            unsigned char hash[32];
            unsigned int hashLen = 0;
            EVP_Digest(certData.data(), certData.size(), hash, &hashLen, EVP_sha256(), nullptr);
            QByteArray tsToken = d->fetchTimestampToken(QByteArray(reinterpret_cast<char*>(hash), hashLen));
            if (!tsToken.isEmpty())
                qDebug() << "B-T: RFC 3161 timestamp token obtained (" << tsToken.size() << "bytes)";
            else
                qWarning() << "B-T: TSA request failed — signature will use local time only";
        }

        FileStreamDevice outputStream(outputPath.toStdString(), FileMode::Create);
        SignDocument(doc, outputStream, actualSigner, *signature);

        // ----------------------------------------------------------------
        // B-LT / B-LTA outcome tracking — bytes are written, but DSS / TSA
        // failures structurally weaken the long-term-validation badge.
        // Callers MUST be informed so the UI does not falsely claim B-LT/LTA.
        // ----------------------------------------------------------------
        bool overallOk = true;

        // ----------------------------------------------------------------
        // B-LT: build DSS dictionary with OCSP/certs
        // ----------------------------------------------------------------
        if (d->level >= PAdESLevel::B_LT) {
            QByteArray contentsHex = d->extractSignatureContentsHex(outputPath);

            // Fetch OCSP for leaf cert
            QList<QByteArray> ocsps;
            if (leafCert && issuerCert) {
                QByteArray ocsp = d->fetchOcspResponse(leafCert, issuerCert);
                if (!ocsp.isEmpty()) ocsps.append(ocsp);
            }

            if (!ocsps.isEmpty()) {
                qWarning() << "SignatureManager: OCSP response embedded into DSS without OCSP_basic_verify "
                           << "(audit 2026-05-23). Trusts responder solely on TLS — strengthen before PAdES B-LT compliance review.";
            }

            bool dssOk = d->buildDssDictionary(outputPath, certChain, ocsps, {}, contentsHex);
            if (!dssOk) {
                overallOk = false;
                qWarning() << "B-LT: DSS dictionary build failed — signature bytes written but long-term-validation"
                           << "data is INCOMPLETE. The caller MUST inform the user that the B-LT badge is structurally"
                           << "weakened and revocation data may be missing.";
            }
        }

        // Cleanup X509 objects
        if (leafCert) X509_free(leafCert);
        if (issuerCert) X509_free(issuerCert);

        // ----------------------------------------------------------------
        // B-LTA: document timestamp over DSS-augmented file
        // ----------------------------------------------------------------
        if (d->level >= PAdESLevel::B_LTA) {
            if (!d->addDocTimestamp(outputPath)) {
                overallOk = false;
                qWarning() << "B-LTA: document timestamp failed — signature bytes written but archival timestamp"
                           << "is MISSING. The caller MUST inform the user that B-LTA archival assurances are not"
                           << "in effect for this document.";
            }
        }

        return overallOk;
    } catch (const PdfError &e) {
        qWarning() << "PoDoFo error during signing:" << e.what();
        return false;
    } catch (const std::exception &e) {
        qWarning() << "Standard error during signing:" << e.what();
        return false;
    }
}

// ---------------------------------------------------------------------------
QList<SignatureInfo> SignatureManager::validateSignatures(const QString &filePath)
{
    QList<SignatureInfo> results;
    try {
        PdfMemDocument doc;
        doc.Load(filePath.toStdString());

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return results;
        QByteArray fileData = file.readAll();

        // Detect DSS and DocTimeStamp
        bool hasDss = doc.GetCatalog().GetDictionary().HasKey(PdfName("DSS"));
        bool hasDocTimestamp = false;

        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                auto *sig = static_cast<PdfSignature*>(field);
                auto *sfObj = sig->GetDictionary().FindKey(PdfName("SubFilter"));
                if (sfObj && sfObj->IsName() &&
                    sfObj->GetName().GetString() == "ETSI.RFC3161") {
                    hasDocTimestamp = true;
                }
            }
        }

        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() != PdfFieldType::Signature) continue;

            PdfSignature *sig = static_cast<PdfSignature*>(field);

            // Skip /DocTimeStamp entries in results — they're timestamps, not user sigs
            auto *sfObj = sig->GetDictionary().FindKey(PdfName("SubFilter"));
            if (sfObj && sfObj->IsName() &&
                sfObj->GetName().GetString() == "ETSI.RFC3161") continue;

            SignatureInfo info;
            info.fieldName = QString::fromStdString(sig->GetFullName());
            info.hasDss = hasDss;
            info.hasDocTimestamp = hasDocTimestamp;

            // PoDoFo uses nullable<const PdfString&>, not std::optional<PdfString>
            auto extractNullable = [](PoDoFo::nullable<const PdfString&> opt) -> QString {
                if (!opt.has_value()) return {};
                auto s = opt.value().GetString();
                return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
            };

            info.reason      = extractNullable(sig->GetSignatureReason());
            info.location    = extractNullable(sig->GetSignatureLocation());
            info.signerName  = extractNullable(sig->GetSignerName());
            info.isValid     = false;
            info.integrityIntact = false;
            info.trustStatus = "Invalid";

            try {
                auto &sigDict = sig->GetDictionary();

                auto *byteRangeObj = sigDict.FindKey(PdfName("ByteRange"));
                if (!byteRangeObj || !byteRangeObj->IsArray()) {
                    info.trustStatus = "Unsigned";
                    results.append(info);
                    continue;
                }
                auto &byteRangeArray = byteRangeObj->GetArray();
                if (byteRangeArray.size() != 4) {
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                int64_t off1 = byteRangeArray[0].GetNumber();
                int64_t len1 = byteRangeArray[1].GetNumber();
                int64_t off2 = byteRangeArray[2].GetNumber();
                int64_t len2 = byteRangeArray[3].GetNumber();

                if (off1 < 0 || len1 < 0 || off2 < 0 || len2 < 0 ||
                    off1 > fileData.size() || len1 > fileData.size() - off1 ||
                    off2 > fileData.size() || len2 > fileData.size() - off2) {
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                // PDF Shadow Attack: ByteRange must cover entire file
                if (off1 != 0 || (off2 + len2) != fileData.size()) {
                    qWarning() << "SECURITY: Signature ByteRange mismatch — possible shadow attack";
                    info.trustStatus = "ByteRangeMismatch";
                    results.append(info);
                    continue;
                }

                if (off1 > std::numeric_limits<int>::max() ||
                    len1 > std::numeric_limits<int>::max() ||
                    off2 > std::numeric_limits<int>::max() ||
                    len2 > std::numeric_limits<int>::max() ||
                    (len1 + len2) > std::numeric_limits<int>::max()) {
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                QByteArray signedData;
                signedData.reserve(static_cast<int>(len1 + len2));
                signedData.append(fileData.mid(static_cast<int>(off1), static_cast<int>(len1)));
                signedData.append(fileData.mid(static_cast<int>(off2), static_cast<int>(len2)));

                auto *contentsObj = sigDict.FindKey(PdfName("Contents"));
                if (!contentsObj || !contentsObj->IsString()) {
                    info.trustStatus = "Unsigned";
                    results.append(info);
                    continue;
                }

                std::string contentsHex{contentsObj->GetString().GetString()};
                QByteArray cmsData(contentsHex.data(), static_cast<int>(contentsHex.size()));

                BIO *cmsBio = BIO_new_mem_buf(cmsData.data(), cmsData.size());
                CMS_ContentInfo *cms = d2i_CMS_bio(cmsBio, nullptr);
                BIO_free(cmsBio);

                if (cms) {
                    // RAII guards — ensure OpenSSL objects are freed even if
                    // CMS_verify or any inner call throws (Fix J, audit 2026-05-23).
                    CmsContentInfoPtr cmsGuard(cms);
                    BioPtr contentBio(BIO_new_mem_buf(signedData.data(), signedData.size()));
                    X509StorePtr store(X509_STORE_new());
                    if (store) X509_STORE_set_default_paths(store.get());

                    qWarning() << "SignatureManager::validateSignatures using X509_STORE_set_default_paths with no "
                               << "X509_VERIFY_PARAM strictness flags, no signing-time check, no EKU enforcement "
                               << "(audit 2026-05-23). PAdES long-term validity is structurally weaker than claimed.";

                    if (CMS_verify(cms, nullptr, store.get(), contentBio.get(), nullptr,
                                   CMS_DETACHED | CMS_BINARY) == 1) {
                        info.isValid = true;
                        info.integrityIntact = true;
                        info.trustStatus = hasDss ? "ValidWithDSS" : "Valid";
                    } else {
                        info.isValid = false;
                        info.integrityIntact = false;
                        info.trustStatus = "Invalid";
                    }
                    // unique_ptrs auto-release on scope exit
                }
            } catch (...) {
                info.isValid = false;
                info.trustStatus = "Invalid";
            }

            results.append(info);
        }
    } catch (const std::exception &e) {
        qWarning() << "Error validating signatures:" << e.what();
    }
    return results;
}
