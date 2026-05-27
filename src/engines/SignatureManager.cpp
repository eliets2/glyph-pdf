#include "engines/SignatureManager.h"
#include <memory>

// Windows CryptoAPI must come BEFORE OpenSSL to prevent wincrypt.h from
// defining OCSP_REQUEST, OCSP_RESPONSE, X509_NAME etc. as macros that
// stomp OpenSSL's typedefs.  We include it first, then #undef the
// conflicting names so OpenSSL's headers define them correctly.
#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wincrypt.h>
   // wincrypt.h defines these as macros; OpenSSL needs them as its own types
#  undef X509_NAME
#  undef X509_CERT_PAIR
#  undef X509_EXTENSIONS
#  undef PKCS7_ISSUER_AND_SERIAL
#  undef PKCS7_SIGNER_INFO
#  undef OCSP_REQUEST
#  undef OCSP_RESPONSE
#endif

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
#include <QSettings>
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
    X509_STORE *testTrustStore = nullptr;

    // -----------------------------------------------------------------------
    // Populate X509_STORE with trust roots
    // -----------------------------------------------------------------------
    X509_STORE* getTrustStore(QString &trustStoreUsedStr, X509StorePtr &guard)
    {
        if (testTrustStore) {
            trustStoreUsedStr = "TestStore";
            X509_STORE_up_ref(testTrustStore);
            guard.reset(testTrustStore);
            return testTrustStore;
        }

        X509_STORE *store = X509_STORE_new();
        guard.reset(store);

        QSettings settings;
        QString path = settings.value("signing/trustStorePath").toString();
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            if (fi.isDir()) {
                X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir());
                if (lookup) X509_LOOKUP_add_dir(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
            } else {
                X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
                if (lookup) X509_LOOKUP_load_file(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
            }
            trustStoreUsedStr = "CustomPath";
        } else {
#ifdef Q_OS_WIN
            HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
            if (hStore) {
                PCCERT_CONTEXT pContext = nullptr;
                while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != nullptr) {
                    const unsigned char* pbCertEncoded = pContext->pbCertEncoded;
                    X509* x509 = d2i_X509(nullptr, &pbCertEncoded, pContext->cbCertEncoded);
                    if (x509) {
                        X509_STORE_add_cert(store, x509);
                        X509_free(x509);
                    }
                }
                CertCloseStore(hStore, 0);
            }
            trustStoreUsedStr = "WindowsSystemStore";
#else
            X509_STORE_set_default_paths(store);
            trustStoreUsedStr = "SystemDefault";
#endif
        }
        return store;
    }

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
        if (!bio) return false;
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
        if (len <= 0) { X509_free(cert); if (ca) sk_X509_pop_free(ca, X509_free); return false; }
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
                if (caLen > 0) {
                    QByteArray caDer(caLen, '\0');
                    unsigned char *cp = reinterpret_cast<unsigned char*>(caDer.data());
                    i2d_X509(caCert, &cp);
                    certChain.append(caDer);
                    if (issuerCert && i == 0) *issuerCert = caCert;
                }
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
        OCSP_REQUEST *ocspReq = OCSP_REQUEST_new();
        if (!ocspReq) return {};

        OCSP_CERTID *certId = OCSP_cert_to_id(EVP_sha256(), cert, issuer);
        if (!certId) { OCSP_REQUEST_free(ocspReq); return {}; }

        OCSP_request_add0_id(ocspReq, certId);
        OCSP_request_add1_nonce(ocspReq, nullptr, -1);

        // Serialize request
        unsigned char *derBuf = nullptr;
        int derLen = i2d_OCSP_REQUEST(ocspReq, &derBuf);
        OCSP_REQUEST_free(ocspReq);
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
                            const QByteArray &sigContentsRaw)
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
            if (!sigContentsRaw.isEmpty()) {
                // SHA-1 of the raw /Contents octets
                QByteArray sha1 = QCryptographicHash::hash(sigContentsRaw, QCryptographicHash::Sha1);
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
                        // 32 KB accommodates multi-cert TSA chains; bump if cert chain exceeds
                        contents.assign(32768, '\0');
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
    // Extract /Contents raw and hex from signed PDF for VRI key computation
    // -----------------------------------------------------------------------
    std::pair<QByteArray, QByteArray> extractSignatureContentsRaw(const QString &filePath)
    {
        PdfMemDocument doc;
        doc.Load(filePath.toStdString());
        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                auto *sig = static_cast<PdfSignature*>(field);
                auto *contentsObj = sig->GetDictionary().FindKey(PdfName("Contents"));
                if (contentsObj && contentsObj->IsString()) {
                    auto str = contentsObj->GetString().GetString();
                    QByteArray rawBytes(str.data(), static_cast<int>(str.size()));
                    return {rawBytes, rawBytes.toHex()};
                }
            }
        }
        return {};
    }
};

// ===========================================================================
SignatureManager::SignatureManager() : d(std::make_unique<Private>()) {}
SignatureManager::~SignatureManager() = default;

void SignatureManager::setTsaUrl(const QString &url) { d->tsaUrl = url; }
void SignatureManager::setSignatureLevel(PAdESLevel level) { d->level = level; }
void SignatureManager::setTrustStoreForTest(X509_STORE *store) { d->testTrustStore = store; }

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
        if (pkeyLen <= 0) {
            qWarning() << "SignatureManager: i2d_PrivateKey size query failed";
            ERR_print_errors_fp(stderr);
            return false;
        }
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
            auto [sigContentsRaw, sigContentsHexUnused] = d->extractSignatureContentsRaw(outputPath);

            // Fetch and verify OCSP for leaf cert before embedding in DSS
            QList<QByteArray> ocsps;
            if (leafCert && issuerCert) {
                QByteArray ocspRaw = d->fetchOcspResponse(leafCert, issuerCert);
                if (!ocspRaw.isEmpty()) {
                    // D3: Verify OCSP response with OCSP_basic_verify before embedding
                    const unsigned char *ocspPtr = reinterpret_cast<const unsigned char*>(ocspRaw.constData());
                    OCSP_RESPONSE *resp = d2i_OCSP_RESPONSE(nullptr, &ocspPtr, ocspRaw.size());
                    if (!resp) {
                        qWarning() << "OCSP: failed to parse response — not embedding in DSS";
                    } else if (OCSP_response_status(resp) != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
                        qWarning() << "OCSP: response status not SUCCESSFUL — not embedding in DSS";
                        OCSP_RESPONSE_free(resp);
                    } else {
                        OCSP_BASICRESP *basic = OCSP_response_get1_basic(resp);
                        if (!basic) {
                            qWarning() << "OCSP: failed to extract basic response — not embedding in DSS";
                            OCSP_RESPONSE_free(resp);
                        } else {
                            // Build a temporary store for OCSP verification
                            X509StorePtr ocspStoreGuard(X509_STORE_new());
                            QString unusedStr;
                            d->getTrustStore(unusedStr, ocspStoreGuard);

                            // Build the cert chain stack for OCSP signer verification
                            STACK_OF(X509) *certs = sk_X509_new_null();
                            for (const QByteArray &derCert : certChain) {
                                const unsigned char *cp = reinterpret_cast<const unsigned char*>(derCert.constData());
                                X509 *c = d2i_X509(nullptr, &cp, derCert.size());
                                if (c) sk_X509_push(certs, c);
                            }

                            if (OCSP_basic_verify(basic, certs, ocspStoreGuard.get(), 0) == 1) {
                                ocsps.append(ocspRaw);
                                qDebug() << "OCSP: response verified and embedded in DSS";
                            } else {
                                char errBuf[256];
                                ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
                                qWarning() << "OCSP: OCSP_basic_verify failed —" << errBuf
                                           << "— not embedding in DSS; signature degrades to B-T";
                            }
                            sk_X509_pop_free(certs, X509_free);
                            OCSP_BASICRESP_free(basic);
                            OCSP_RESPONSE_free(resp);
                        }
                    }
                }
            }

            bool dssOk = d->buildDssDictionary(outputPath, certChain, ocsps, {}, sigContentsRaw);
            if (!dssOk) {
                overallOk = false;
                qWarning() << "B-LT: DSS dictionary build failed — signature bytes written but long-term-validation"
                           << "data is INCOMPLETE.";
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

                // D4: Reject overlapping byte ranges (PDF shadow attack vector)
                if (off1 + len1 > off2) {
                    qWarning() << "SECURITY: Signature ByteRange overlap detected — possible shadow attack";
                    info.trustStatus = "ByteRangeOverlap";
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
                if (!cmsBio) {
                    info.trustStatus = "Invalid";
                    results.append(info);
                    continue;
                }
                CMS_ContentInfo *cms = d2i_CMS_bio(cmsBio, nullptr);
                BIO_free(cmsBio);

                if (cms) {
                    CmsContentInfoPtr cmsGuard(cms);
                    BioPtr contentBio(BIO_new_mem_buf(signedData.data(), signedData.size()));
                    if (!contentBio) {
                        info.trustStatus = "Invalid";
                        results.append(info);
                        continue;
                    }

                    // D2: Build a real trust store from system roots or configured path
                    X509StorePtr storeGuard(nullptr);
                    X509_STORE *store = d->getTrustStore(info.trustStoreUsed, storeGuard);

                    // D2: Configure strict verification parameters
                    X509_VERIFY_PARAM *vpm = X509_VERIFY_PARAM_new();
                    if (vpm) {
                        X509_VERIFY_PARAM_set_flags(vpm, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
                        X509_VERIFY_PARAM_set_purpose(vpm, X509_PURPOSE_SMIME_SIGN);
                        X509_STORE_set1_param(store, vpm);
                        X509_VERIFY_PARAM_free(vpm);
                    }

                    // D2: First pass — verify only cryptographic integrity (no chain)
                    BioPtr contentBioIntegrity(BIO_new_mem_buf(signedData.data(), signedData.size()));
                    bool integrityOk = contentBioIntegrity &&
                        (CMS_verify(cms, nullptr, nullptr, contentBioIntegrity.get(), nullptr,
                                    CMS_DETACHED | CMS_BINARY | CMS_NOSIGS) == 1);
                    info.integrityIntact = integrityOk;

                    // D2: Second pass — full chain + trust verification
                    if (CMS_verify(cms, nullptr, store, contentBio.get(), nullptr,
                                   CMS_DETACHED | CMS_BINARY) == 1) {
                        // D2: EKU check — reject certs with only OCSPSigning EKU
                        bool ekuOk = true;
                        STACK_OF(CMS_SignerInfo) *sis = CMS_get0_SignerInfos(cms);
                        for (int si = 0; si < sk_CMS_SignerInfo_num(sis); ++si) {
                            CMS_SignerInfo *siInfo = sk_CMS_SignerInfo_value(sis, si);
                            X509 *signerCert = nullptr;
                            CMS_SignerInfo_get0_algs(siInfo, nullptr, &signerCert, nullptr, nullptr);
                            if (!signerCert) continue;
                            EXTENDED_KEY_USAGE *eku = static_cast<EXTENDED_KEY_USAGE*>(
                                X509_get_ext_d2i(signerCert, NID_ext_key_usage, nullptr, nullptr));
                            if (eku) {
                                bool hasOcspOnly = true;
                                for (int ei = 0; ei < sk_ASN1_OBJECT_num(eku); ++ei) {
                                    int nid = OBJ_obj2nid(sk_ASN1_OBJECT_value(eku, ei));
                                    if (nid != NID_OCSP_sign)
                                        hasOcspOnly = false;
                                }
                                if (hasOcspOnly) { ekuOk = false; }
                                EXTENDED_KEY_USAGE_free(eku);
                            }
                        }

                        // D2: Signing-time check — verify within cert validity window
                        bool signingTimeOk = true;
                        for (int si = 0; si < sk_CMS_SignerInfo_num(sis); ++si) {
                            CMS_SignerInfo *siInfo = sk_CMS_SignerInfo_value(sis, si);
                            X509 *signerCert = nullptr;
                            CMS_SignerInfo_get0_algs(siInfo, nullptr, &signerCert, nullptr, nullptr);
                            if (!signerCert) continue;
                            int attrIdx = CMS_signed_get_attr_by_NID(siInfo, NID_pkcs9_signingTime, -1);
                            if (attrIdx >= 0) {
                                X509_ATTRIBUTE *attr = CMS_signed_get_attr(siInfo, attrIdx);
                                if (attr) {
                                    ASN1_TYPE *attrType = X509_ATTRIBUTE_get0_type(attr, 0);
                                    if (attrType) {
                                        ASN1_TIME *sigTime = nullptr;
                                        if (attrType->type == V_ASN1_UTCTIME)
                                            sigTime = attrType->value.utctime;
                                        else if (attrType->type == V_ASN1_GENERALIZEDTIME)
                                            sigTime = attrType->value.generalizedtime;
                                        if (sigTime) {
                                            const ASN1_TIME *notBefore = X509_get0_notBefore(signerCert);
                                            const ASN1_TIME *notAfter  = X509_get0_notAfter(signerCert);
                                            if (ASN1_TIME_compare(sigTime, notBefore) < 0 ||
                                                ASN1_TIME_compare(sigTime, notAfter)  > 0) {
                                                signingTimeOk = false;
                                                qWarning() << "SECURITY: Signing time outside certificate validity window";
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (ekuOk && signingTimeOk) {
                            info.isValid = true;
                            info.trustStatus = hasDss ? "ValidWithDSS" : "Valid";
                        } else if (!ekuOk) {
                            info.isValid = false;
                            info.trustStatus = "InvalidEKU";
                        } else {
                            info.isValid = false;
                            info.trustStatus = "SigningTimeOutOfRange";
                        }
                    } else {
                        // Distinguish chain failure from crypto failure
                        unsigned long err = ERR_peek_last_error();
                        info.isValid = false;
                        info.integrityIntact = integrityOk;
                        if (ERR_GET_REASON(err) == X509_V_ERR_CERT_UNTRUSTED ||
                            ERR_GET_REASON(err) == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT ||
                            ERR_GET_REASON(err) == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY ||
                            ERR_GET_REASON(err) == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
                            ERR_GET_REASON(err) == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) {
                            info.trustStatus = "UntrustedChain";
                        } else {
                            info.trustStatus = "Invalid";
                        }
                        ERR_clear_error();
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
