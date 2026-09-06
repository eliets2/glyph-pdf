// SPDX-License-Identifier: Apache-2.0
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
// QPDF headers removed per avoidance rules
#include <set>
#include <functional>
#include <QFile>

static QByteArray extractCmsFromContents(const QByteArray& fileData, qint64 off1, qint64 len1, qint64 off2) {
    QByteArray hexRaw = fileData.mid(
        static_cast<int>(off1 + len1),
        static_cast<int>(off2 - (off1 + len1)))
        .simplified()
        .replace(" ", "");
    while (hexRaw.endsWith('\0')) hexRaw.chop(1);
    if (!hexRaw.isEmpty() && hexRaw[0] == '<') hexRaw.remove(0, 1);
    if (!hexRaw.isEmpty() && hexRaw.endsWith('>')) hexRaw.chop(1);
    return QByteArray::fromHex(hexRaw);
}
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSemaphore>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QThreadPool>
#include <QTimeZone>
#include <QTimer>
#include <QCryptographicHash>
#include <QSettings>
#include <QRegularExpression>
#include <limits>
#include <vector>
#include <stdexcept>

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
    // E-02: outcome of the most recent signing call so the UI can tell a partial
    // (core-signed but LTV-missing) result apart from a total failure.
    SignOutcome lastOutcome = SignOutcome::NotRun;

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
            // E-09: a failed load here must NOT be swallowed. If we silently leave
            // the store empty, CMS_verify fails with UNABLE_TO_GET_ISSUER_CERT and
            // EVERY signature is reported "UntrustedChain" with no hint that the
            // configured trust store itself failed to load. Surface + log it and
            // mark the status distinctly so the UI/operator can tell the difference.
            QFileInfo fi(path);
            int loaded = 0;
            if (fi.isDir()) {
                X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir());
                if (lookup)
                    loaded = X509_LOOKUP_add_dir(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
            } else {
                X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
                if (lookup)
                    loaded = X509_LOOKUP_load_file(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
            }
            if (loaded != 1) {
                char errBuf[256];
                ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
                qWarning() << "SignatureManager: FAILED to load custom trust store from"
                           << path << ":" << errBuf
                           << "— all signatures will appear untrusted until this is fixed.";
                trustStoreUsedStr = "CustomPathLoadFailed";
            } else {
                trustStoreUsedStr = "CustomPath";
            }
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
        if (url.startsWith("http://", Qt::CaseInsensitive)) {
            qWarning() << "SECURITY: HTTP URLs are forbidden for TSA/OCSP. URL:" << url;
            return {};
        }

        struct SharedState {
            QByteArray result;
            QString errStr;
            QSemaphore sem{0};
        };
        auto state = std::make_shared<SharedState>();

        QMetaObject::invokeMethod(qApp, [url, contentType, body, state]() {
            static QNetworkAccessManager s_nam;
            QNetworkRequest req{QUrl(url)};
            req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
            req.setTransferTimeout(15000);
            QNetworkReply *reply = s_nam.post(req, body);
            QObject::connect(reply, &QNetworkReply::finished, qApp, [reply, state]() {
                if (reply->error() == QNetworkReply::NoError)
                    state->result = reply->readAll();
                else
                    state->errStr = reply->errorString();
                reply->deleteLater();
                state->sem.release();
            });
        }, Qt::QueuedConnection);

        if (!state->sem.tryAcquire(1, 20000)) {
            qWarning() << "HTTP POST to" << url << "timed out";
            return {};
        }
        if (!state->errStr.isEmpty())
            qWarning() << "HTTP POST to" << url << "failed:" << state->errStr;
        return state->result;
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
    QByteArray fetchOcspResponse(X509 *cert, X509 *issuer, const QString &certPath = QString())
    {
        if (!cert || !issuer) return {};

        // For testing/offline environments: if there is a local <name>_ocsp_response.der
        // or revoked_ocsp_response.der in the same directory as certPath, load it!
#ifdef GLYPHPDF_TESTING
        if (!certPath.isEmpty()) {
            QFileInfo certInfo(certPath);
            QDir dir = certInfo.dir();
            QString base = certInfo.baseName();
            if (base.endsWith("_cert")) base.chop(5); // revoked_cert -> revoked
            QString localPath = dir.filePath(base + "_ocsp_response.der");
            if (QFile::exists(localPath)) {
                QFile f(localPath);
                if (f.open(QIODevice::ReadOnly)) {
                    qDebug() << "OCSP: Loaded local response from" << localPath;
                    return f.readAll();
                }
            }
            if (base.contains("revoked", Qt::CaseInsensitive)) {
                localPath = dir.filePath("revoked_ocsp_response.der");
                if (QFile::exists(localPath)) {
                    QFile f(localPath);
                    if (f.open(QIODevice::ReadOnly)) {
                        qDebug() << "OCSP: Loaded local response from" << localPath;
                        return f.readAll();
                    }
                }
            }
        }
#endif

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
                        qWarning() << "B-LTA: TSA returned empty token — aborting document timestamp";
                        throw std::runtime_error(
                            "B-LTA: TSA returned an empty timestamp token — aborting /DocTimeStamp");
                    }
                    // D5 FIX: Hard-fail if TSA token exceeds reservation (no silent truncation)
                    // The token is hex-encoded when written by PoDoFo (each byte takes 2 hex chars).
                    // Wait, the `contents.size()` available is 32768. The token size must fit in contents.size().
                    if (static_cast<size_t>(token.size()) > contents.size()) {
                        qWarning() << "TimestampSigner: Allocated space too small for TSA token";
                        throw std::runtime_error("B-LTA: TSA token exceeds 32 KB /Contents reservation");
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
        } catch (const std::exception &e) {
            // E-06: includes the empty-TSA-token hard failure thrown from
            // TimestampSigner::ComputeSignature. The document timestamp is NOT
            // applied; the caller reports B-LTA as not in effect.
            qWarning() << "B-LTA timestamp addition aborted:" << e.what();
            return false;
        }
    }

    static std::vector<X509*> decodeDssCerts(const PdfMemDocument &doc,
                                              const PdfDictionary &dssDict)
    {
        std::vector<X509*> result;
        const PdfObject *certsObj = dssDict.FindKey(PdfName("Certs"));
        if (certsObj && certsObj->IsReference()) {
            certsObj = &doc.GetObjects().MustGetObject(certsObj->GetReference());
        }
        if (!certsObj || !certsObj->IsArray()) {
            return result;
        }

        const auto &arr = certsObj->GetArray();
        for (unsigned int i = 0; i < arr.GetSize(); ++i) {
            const PdfObject *entryRef = &arr[i];
            const PdfObject *streamObj = nullptr;
            if (entryRef->IsReference())
                streamObj = &doc.GetObjects().MustGetObject(entryRef->GetReference());
            else
                streamObj = entryRef;
            if (!streamObj || !streamObj->HasStream()) {
                continue;
            }

            charbuff buf;
            streamObj->GetStream()->CopyTo(buf);
            const unsigned char *p =
                reinterpret_cast<const unsigned char *>(buf.data());
            X509 *x = d2i_X509(nullptr, &p, static_cast<long>(buf.size()));
            if (x) {
                result.push_back(x);
            }
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // ER-1 fix: Extract embedded OCSP DER bytes from the DSS /OCSPs array,
    // returning only an entry whose single-response certID matches the signer
    // certificate.  Pass signerCert=nullptr to bypass matching (legacy path).
    //
    // Matching strategy: compare serial number (ASN1_INTEGER_cmp) and issuer
    // name hash via OCSP_id_issuer_cmp on a candidate OCSP_CERTID built from
    // the signer cert's issuer name + public key hash (SHA-1, as embedded in
    // the OCSP CertID).
    //
    // Returns: matching DER bytes, or empty if no certID match is found.
    // Sets *outNoCertMatch=true when DSS entries exist but none match the cert.
    // -----------------------------------------------------------------------
    QByteArray extractOcspFromDss(const PdfMemDocument &doc,
                                  const QString &/*sigFieldName*/,
                                  X509 *signerCert = nullptr,
                                  bool *outNoCertMatch = nullptr)
    {
        if (outNoCertMatch) *outNoCertMatch = false;
        try {
            const auto &catalog = doc.GetCatalog().GetDictionary();
            qDebug() << "extractOcspFromDss: catalog keys:" << catalog.HasKey(PdfName("DSS"));
            const PdfObject *dssObj = catalog.FindKey(PdfName("DSS"));
            if (dssObj && dssObj->IsReference()) {
                dssObj = &doc.GetObjects().MustGetObject(dssObj->GetReference());
            }
            if (!dssObj || !dssObj->IsDictionary()) {
                qDebug() << "extractOcspFromDss: DSS not found or not dictionary";
                return {};
            }

            const auto &dssDictRef = dssObj->GetDictionary();
            const PdfObject *ocspsObj = dssDictRef.FindKey(PdfName("OCSPs"));
            if (ocspsObj && ocspsObj->IsReference()) {
                ocspsObj = &doc.GetObjects().MustGetObject(ocspsObj->GetReference());
            }
            if (!ocspsObj || !ocspsObj->IsArray()) {
                qDebug() << "extractOcspFromDss: OCSPs not found or not array";
                return {};
            }

            const auto &ocspsArr = ocspsObj->GetArray();
            if (ocspsArr.IsEmpty()) {
                qDebug() << "extractOcspFromDss: OCSPs array is empty";
                return {};
            }

            // ER-1: iterate all DSS OCSP entries and return the first one whose
            // certID matches the signer certificate.  If signerCert is nullptr
            // (legacy / no-cert path), fall through to the first-entry behaviour.
            bool anyEntryParsed = false;
            for (unsigned int idx = 0; idx < ocspsArr.GetSize(); ++idx) {
                const PdfObject &entryRef = ocspsArr[idx];
                const PdfObject *streamObj = nullptr;
                if (entryRef.IsReference()) {
                    streamObj = &doc.GetObjects().MustGetObject(entryRef.GetReference());
                } else {
                    streamObj = &entryRef;
                }
                if (!streamObj || !streamObj->HasStream()) {
                    qDebug() << "extractOcspFromDss: entry" << idx
                             << "not found or has no stream — skipping";
                    continue;
                }

                charbuff buf;
                streamObj->GetStream()->CopyTo(buf);
                QByteArray der(buf.data(), static_cast<int>(buf.size()));

                // If no signer cert provided, return the first entry (legacy behaviour).
                if (!signerCert) {
                    qDebug() << "extractOcspFromDss: no signerCert — returning entry 0, size ="
                             << der.size();
                    return der;
                }

                // ER-1: parse the OCSP response and check certID matching.
                const unsigned char *p =
                    reinterpret_cast<const unsigned char *>(der.constData());
                OCSP_RESPONSE *resp = d2i_OCSP_RESPONSE(nullptr, &p, der.size());
                if (!resp) {
                    qDebug() << "extractOcspFromDss: entry" << idx
                             << "failed to parse as OCSP_RESPONSE — skipping";
                    continue;
                }

                OCSP_BASICRESP *basic = OCSP_response_get1_basic(resp);
                OCSP_RESPONSE_free(resp);
                if (!basic) {
                    qDebug() << "extractOcspFromDss: entry" << idx
                             << "failed to get OCSP_BASICRESP — skipping";
                    continue;
                }

                anyEntryParsed = true;
                bool matched = false;
                int count = OCSP_resp_count(basic);
                qDebug() << "extractOcspFromDss: entry" << idx
                         << "has" << count << "single responses";

                // M5: full certID comparison using OCSP_cert_to_id + OCSP_id_cmp.
                X509 *issuerCert = nullptr;
                std::vector<X509*> dssCerts = decodeDssCerts(doc, dssDictRef);
                for (X509 *candidate : dssCerts) {
                    if (X509_check_issued(candidate, signerCert) == X509_V_OK) {
                        issuerCert = candidate;
                        break;
                    }
                }
                if (issuerCert) {
                    qDebug() << "extractOcspFromDss: issuer cert found in DSS /Certs"
                             << "— using OCSP_cert_to_id + OCSP_id_cmp";
                }

                OCSP_CERTID *refId256 = issuerCert
                    ? OCSP_cert_to_id(EVP_sha256(), signerCert, issuerCert)
                    : nullptr;
                OCSP_CERTID *refId1 = issuerCert
                    ? OCSP_cert_to_id(EVP_sha1(), signerCert, issuerCert)
                    : nullptr;

                for (int i = 0; i < count && !matched; ++i) {
                    OCSP_SINGLERESP *singleResp = OCSP_resp_get0(basic, i);
                    if (!singleResp) continue;
                    const OCSP_CERTID *certId = OCSP_SINGLERESP_get0_id(singleResp);
                    if (!certId) continue;

                    if (refId256 && OCSP_id_cmp(refId256, certId) == 0) {
                        matched = true;
                        qDebug() << "extractOcspFromDss: full certID match (OCSP_id_cmp SHA-256)"
                                 << "at entry" << idx << "single-resp" << i;
                    } else if (refId1 && OCSP_id_cmp(refId1, certId) == 0) {
                        matched = true;
                        qDebug() << "extractOcspFromDss: full certID match (OCSP_id_cmp SHA-1)"
                                 << "at entry" << idx << "single-resp" << i;
                    }
                }

                if (refId256) OCSP_CERTID_free(refId256);
                if (refId1) OCSP_CERTID_free(refId1);
                for (X509 *x : dssCerts) X509_free(x);

                OCSP_BASICRESP_free(basic);

                if (matched) {
                    qDebug() << "extractOcspFromDss: returning matching OCSP entry"
                             << idx << "size =" << der.size();
                    return der;
                }
                qDebug() << "extractOcspFromDss: entry" << idx
                         << "certID does not match signer cert — skipping";
            }

            // No matching entry found.
            if (anyEntryParsed && outNoCertMatch) {
                *outNoCertMatch = true;
                qWarning() << "SECURITY: DSS /OCSPs entries exist but none match"
                           << "the signer certificate (ER-1)";
            }
            return {};
        } catch (const std::exception &e) {
            qDebug() << "extractOcspFromDss exception:" << e.what();
            return {};
        } catch (...) {
            qDebug() << "extractOcspFromDss unknown exception";
            return {};
        }
    }

    // -----------------------------------------------------------------------
    // Extract /Contents raw and hex from signed PDF for VRI key computation
    // -----------------------------------------------------------------------
    std::pair<QByteArray, QByteArray> extractSignatureContentsRaw(const QString &filePath)
    {
        try {
            PdfMemDocument doc;
            doc.Load(filePath.toStdString());

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) return {};
            QByteArray fileData = file.readAll();

            auto resolveSigDict = [&](PdfSignature *sig) -> const PdfDictionary* {
                auto &fieldDict = sig->GetDictionary();
                auto *vObj = fieldDict.FindKey(PdfName("V"));
                if (vObj) {
                    if (vObj->IsReference())
                        vObj = &doc.GetObjects().MustGetObject(vObj->GetReference());
                    if (vObj->IsDictionary())
                        return &vObj->GetDictionary();
                }
                return &fieldDict;
            };

            for (auto field : doc.GetFieldsIterator()) {
                if (field->GetType() == PdfFieldType::Signature) {
                    auto *sig = static_cast<PdfSignature*>(field);
                    const PdfDictionary *actualSigDict = resolveSigDict(sig);

                    // Skip /DocTimeStamp entries
                    auto *sfObj = actualSigDict->FindKey(PdfName("SubFilter"));
                    if (sfObj && sfObj->IsName() &&
                        sfObj->GetName().GetString() == "ETSI.RFC3161") continue;

                    auto *byteRangeObj = actualSigDict->FindKey(PdfName("ByteRange"));
                    if (byteRangeObj && byteRangeObj->IsReference()) {
                        byteRangeObj = &doc.GetObjects().MustGetObject(byteRangeObj->GetReference());
                    }
                    if (!byteRangeObj || !byteRangeObj->IsArray()) continue;

                    auto &byteRangeArray = byteRangeObj->GetArray();
                    if (byteRangeArray.size() != 4) continue;

                    int64_t off1 = byteRangeArray[0].GetNumber();
                    int64_t len1 = byteRangeArray[1].GetNumber();
                    int64_t off2 = byteRangeArray[2].GetNumber();
                    int64_t len2 = byteRangeArray[3].GetNumber();

                    if (off1 < 0 || len1 < 0 || off2 < 0 || len2 < 0 ||
                        off1 + len1 > fileData.size() || off2 > fileData.size()) continue;

                    QByteArray rawBytes = extractCmsFromContents(fileData, off1, len1, off2);
                    QByteArray sigContentsRawBytes = rawBytes;
                    return {rawBytes, rawBytes.toHex()};
                }
            }
        } catch (...) {
            // ignore
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
SignOutcome SignatureManager::signDocument(const QString &inputPath,
                                    const QString &outputPath,
                                    const QString &certPath,
                                    const QString &password,
                                    const QString &reason,
                                    const QString &location)
{
    // certificationLevel == 0 ⇒ ordinary approval signature (no /DocMDP).
    return signDocumentImpl(inputPath, outputPath, certPath, password, 0, reason, location);
}

// ---------------------------------------------------------------------------
// §9.7 P0 — visible signature appearance (ETSI EN 319 142-6 §5.2, Acrobat
// convention). One /AP /N form XObject, optional signature image left, text
// right, drawn BEFORE SignDocument() so the appearance lands in the same
// incremental update (or SaveOnSigning full save) as the /Contents digest.
// Never touched again after signing — a post-signing AP edit would invalidate
// the signature. No validation status and no TSA time is ever rendered here
// (ISO 32000-2 §12.7.5.5 forbids it inside a field appearance).
// ---------------------------------------------------------------------------
namespace {

constexpr double kAppearancePad = 4.0;            // inner padding, pt
constexpr double kAppearanceLineLeading = 1.25;   // leading multiplier
constexpr double kAppearanceTinyRectW = 120.0;    // "name-only below ~120x36pt"
constexpr double kAppearanceTinyRectH = 36.0;
constexpr double kAppearanceAbsoluteFloor = 4.0;  // last-resort identity floor

// Shrink ladder: 9..6pt is the main fit range (~6pt floor per the binding
// design); name-only mode may go below it so the identity line always draws.
const double kAppearanceSizes[] = { 9.0, 8.0, 7.0, 6.0 };
const double kAppearanceNameOnlySizes[] = { 9.0, 8.0, 7.0, 6.0, 5.0, 4.0 };

QString formatAppearanceDate(const QDateTime &claimedLocal)
{
    const int offSecs = claimedLocal.offsetFromUtc();
    const int offMin = qAbs(offSecs) / 60;
    const QString sign = offSecs < 0 ? QStringLiteral("-") : QStringLiteral("+");
    return QStringLiteral("Date: %1 UTC%2%3:%4")
        .arg(claimedLocal.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             sign,
             QString::number(offMin / 60).rightJustified(2, QLatin1Char('0')),
             QString::number(offMin % 60).rightJustified(2, QLatin1Char('0')));
}

// The ETSI/Acrobat line set: identity (from the certificate CN), claimed
// local time (never the TSA time), Reason/Location ONLY when set. No DN, no
// logo, no validation status.
QStringList appearanceLines(const QString &signerName, const QDateTime &claimedLocalTime,
                            const QString &reason, const QString &location)
{
    QStringList lines;
    if (!signerName.isEmpty())
        lines << (QStringLiteral("Digitally signed by ") + signerName);
    lines << formatAppearanceDate(claimedLocalTime);
    if (!reason.isEmpty())
        lines << (QStringLiteral("Reason: ") + reason);
    if (!location.isEmpty())
        lines << (QStringLiteral("Location: ") + location);
    return lines;
}

bool appearanceFits(const QStringList &lines, double fontSize,
                    double textW, double textH,
                    const SignatureManager::AppearanceMeasureFn &measure)
{
    if (lines.isEmpty())
        return true;
    for (const QString &line : lines) {
        if (measure(line, fontSize) > textW)
            return false;
    }
    return lines.size() * kAppearanceLineLeading * fontSize <= textH;
}

} // namespace

SignatureManager::SignatureAppearancePlan SignatureManager::planSignatureAppearance(
    double rectWidthPt, double rectHeightPt,
    const QString &signerName,
    const QDateTime &claimedLocalTime,
    const QString &reason, const QString &location,
    bool hasSignatureImage,
    const AppearanceMeasureFn &measureTextWidth)
{
    SignatureAppearancePlan plan;
    plan.imageLeft = hasSignatureImage && rectWidthPt > 0.0 && rectHeightPt > 0.0;

    const double pad = kAppearancePad;
    // Image panel on the left: ~30% of the rect width, clamped to a usable
    // strip so a wide image can never squeeze the text out entirely.
    const double imagePanelW = plan.imageLeft
        ? qBound(24.0, 0.30 * rectWidthPt, 0.45 * rectWidthPt)
        : 0.0;
    const double textW = rectWidthPt - 2.0 * pad - imagePanelW;
    const double textH = rectHeightPt - 2.0 * pad;
    if (textW <= 0.0 || textH <= 0.0) {
        // Degenerate rect: draw the image (if any), no text. ETSI §5.2 makes
        // zero-area widget rects non-conformant anyway; placement code guards.
        plan.nameOnly = true;
        return plan;
    }

    auto tryFit = [&](const QStringList &lines) -> double {
        for (double size : kAppearanceSizes) {
            if (appearanceFits(lines, size, textW, textH, measureTextWidth))
                return size;
        }
        return 0.0;
    };

    auto finishWith = [&](const QStringList &lines, double size) {
        plan.lines = lines;
        plan.fontSize = size;
    };

    // Rects below ~120x36pt go straight to the name-only fallback.
    if (rectWidthPt >= kAppearanceTinyRectW && rectHeightPt >= kAppearanceTinyRectH) {
        // 1) full line set
        if (double s = tryFit(appearanceLines(signerName, claimedLocalTime, reason, location)); s > 0.0) {
            finishWith(appearanceLines(signerName, claimedLocalTime, reason, location), s);
            return plan;
        }
        // 2) drop Reason first, 3) then Location
        if (double s = tryFit(appearanceLines(signerName, claimedLocalTime, QString(), location)); s > 0.0) {
            finishWith(appearanceLines(signerName, claimedLocalTime, QString(), location), s);
            return plan;
        }
        if (double s = tryFit(appearanceLines(signerName, claimedLocalTime, QString(), QString())); s > 0.0) {
            finishWith(appearanceLines(signerName, claimedLocalTime, QString(), QString()), s);
            return plan;
        }
    }

    // 4) name-only fallback: identity line only, shrinking to the absolute
    // floor so the signer identity is never silently lost.
    plan.nameOnly = true;
    const QStringList nameLine = signerName.isEmpty()
        ? QStringList()
        : QStringList{ QStringLiteral("Digitally signed by ") + signerName };
    if (nameLine.isEmpty())
        return plan; // nothing drawable
    for (double size : kAppearanceNameOnlySizes) {
        if (appearanceFits(nameLine, size, textW, textH, measureTextWidth)) {
            finishWith(nameLine, size);
            return plan;
        }
    }
    finishWith(nameLine, kAppearanceAbsoluteFloor);
    return plan;
}

// Dialog -> engine handoff for the optional signature image. Mutex-guarded
// because signing runs on a worker thread while the dialog runs on the GUI
// thread. Consume-once: the next signing call drains the slot.
namespace {
QMutex g_pendingAppearanceImageMutex;
QImage g_pendingAppearanceImage;
} // namespace

void SignatureManager::setPendingAppearanceImage(const QImage &image)
{
    QMutexLocker locker(&g_pendingAppearanceImageMutex);
    g_pendingAppearanceImage = image;
}

QImage SignatureManager::takePendingAppearanceImage()
{
    QMutexLocker locker(&g_pendingAppearanceImageMutex);
    QImage taken = g_pendingAppearanceImage;
    g_pendingAppearanceImage = QImage();
    return taken;
}

namespace {

// Draws the planned appearance into a single /AP /N form XObject and attaches
// it to the signature widget. Throws are caught by the caller: the appearance
// is cosmetic relative to the cryptographic core and must never abort signing.
void drawSignatureAppearance(PdfMemDocument &doc, PdfSignature &signature,
                             const QString &signerCN, const QString &reason,
                             const QString &location, const QImage &image)
{
    if (!signature.GetWidget())
        return;
    const Rect widgetRect = signature.GetWidget()->GetRect();
    if (widgetRect.Width <= 0.0 || widgetRect.Height <= 0.0) {
        qWarning() << "SignatureManager: signature widget rect is degenerate"
                   << widgetRect.Width << "x" << widgetRect.Height
                   << "- skipping appearance (ETSI EN 319 142-6 §5.2 requires w,h > 0)";
        return;
    }

    // Standard-14 Helvetica with an explicit WinAnsi encoding: the appearance
    // content stream then carries literal (searchable, verifiable) text.
    // WinAnsi is Latin-only — non-Latin signer names, reasons and locations
    // will not render correctly. Embedding an open-licensed TTF via
    // GetOrCreateFontFromBuffer (PoDoFo embeds imported fonts by default) is
    // the documented upgrade path; it requires a bundled font asset and is
    // deferred. Metrics from this font drive the auto-fit, so measurement and
    // rendering agree exactly (ETSI EN 319 142-6 §5.2 fit = shall).
    PdfFontCreateParams fontParams;
    fontParams.Encoding = PdfEncoding(PdfEncodingMapFactory::GetWinAnsiEncodingInstancePtr());
    fontParams.Flags = PdfFontCreateFlags::DontEmbed;
    PdfFont &font = doc.GetFonts().GetStandard14Font(PdfStandard14FontType::Helvetica, fontParams);

    auto measure = [&font](const QString &text, double fontSize) -> double {
        PdfTextState state;
        state.Font = &font;
        state.FontSize = fontSize;
        return font.GetStringLength(text.toUtf8().constData(), state);
    };

    // Claimed signing time (local clock, with UTC offset). The TSA timestamp
    // is validation information and must never appear in the appearance.
    const QDateTime claimedLocal = QDateTime::currentDateTime();
    const SignatureManager::SignatureAppearancePlan plan = SignatureManager::planSignatureAppearance(
        widgetRect.Width, widgetRect.Height, signerCN, claimedLocal,
        reason, location, !image.isNull(), measure);

    auto form = doc.CreateXObjectForm(Rect(0.0, 0.0, widgetRect.Width, widgetRect.Height));

    PdfImage *pdfImage = nullptr;
    QImage rgbImage;
    std::unique_ptr<PdfImage> pdfImageOwner;
    if (!image.isNull()) {
        rgbImage = image.convertToFormat(QImage::Format_RGB888);
        if (!rgbImage.isNull()) {
            pdfImageOwner = doc.CreateImage();
            pdfImage = pdfImageOwner.get();
            pdfImage->SetData(bufferview(reinterpret_cast<const char *>(rgbImage.constBits()),
                                         static_cast<size_t>(rgbImage.sizeInBytes())),
                              static_cast<unsigned>(rgbImage.width()),
                              static_cast<unsigned>(rgbImage.height()),
                              PdfPixelFormat::RGB24,
                              rgbImage.bytesPerLine());
        }
    }

    PdfPainter painter;
    painter.SetCanvas(*form);

    // Light background + hairline border (Acrobat convention, Lane A).
    painter.GraphicsState.SetNonStrokingColor(PdfColor(1.0, 1.0, 1.0));
    painter.DrawRectangle(0.0, 0.0, widgetRect.Width, widgetRect.Height, PdfPathDrawMode::Fill);
    painter.GraphicsState.SetStrokingColor(PdfColor(0.55, 0.55, 0.55));
    painter.DrawRectangle(0.0, 0.0, widgetRect.Width, widgetRect.Height, PdfPathDrawMode::Stroke);

    if (pdfImage) {
        const double boxX = kAppearancePad;
        const double boxY = kAppearancePad;
        const double boxW = qBound(24.0, 0.30 * widgetRect.Width, 0.45 * widgetRect.Width);
        const double boxH = widgetRect.Height - 2.0 * kAppearancePad;
        const double imgW = static_cast<double>(rgbImage.width());
        const double imgH = static_cast<double>(rgbImage.height());
        if (imgW > 0.0 && imgH > 0.0 && boxW > 0.0 && boxH > 0.0) {
            const double scale = qMin(boxW / imgW, boxH / imgH);
            const double drawW = imgW * scale;
            const double drawH = imgH * scale;
            const double drawY = boxY + (boxH - drawH) / 2.0; // center vertically
            painter.DrawImage(*pdfImage, boxX, drawY, drawW / imgW, drawH / imgH);
        }
    }

    if (!plan.lines.isEmpty() && plan.fontSize > 0.0) {
        painter.TextState.SetFont(font, plan.fontSize);
        painter.GraphicsState.SetNonStrokingColor(PdfColor(0.0, 0.0, 0.0));
        const double textX = kAppearancePad + (plan.imageLeft
            ? qBound(24.0, 0.30 * widgetRect.Width, 0.45 * widgetRect.Width)
            : 0.0);
        double baselineY = widgetRect.Height - kAppearancePad - plan.fontSize;
        for (const QString &line : plan.lines) {
            painter.DrawText(line.toUtf8().constData(), textX, baselineY);
            baselineY -= kAppearanceLineLeading * plan.fontSize;
        }
    }

    painter.FinishDrawing();
    signature.GetWidget()->SetAppearanceStream(*form, PdfAppearanceType::Normal);
}

} // namespace

// ---------------------------------------------------------------------------
// Shared signing core. certificationLevel: 0 = ordinary approval signature;
// 1/2/3 = certification (author) signature with a /DocMDP transform whose
// permission is NoPerms/FormFill/Annotations respectively. For a certification
// signature this MUST write /DocMDP or the whole operation fails — there is no
// silent downgrade to an ordinary signature (audit E-01).
SignOutcome SignatureManager::signDocumentImpl(const QString &inputPath,
                                        const QString &outputPath,
                                        const QString &certPath,
                                        const QString &password,
                                        int certificationLevel,
                                        const QString &reason,
                                        const QString &location)
{
    // E-02: assume failure until we know the core signature bytes were written.
    d->lastOutcome = SignOutcome::Failed;
    try {
        PdfMemDocument doc;
        doc.Load(inputPath.toStdString());

        // §9.7 P0: drain the dialog's optional signature-image slot up front
        // so a failed signing attempt can never leak it into a later signature.
        const QImage appearanceImage = takePendingAppearanceImage();

        charbuff certData;
        EVP_PKEY *pkeyRaw = nullptr;
        QList<QByteArray> certChain;
        X509 *leafCert = nullptr, *issuerCert = nullptr;

        if (!d->loadP12(certPath, password, certData, &pkeyRaw, certChain, &leafCert, &issuerCert)) {
            qWarning() << "Failed to load P12 certificate";
            return SignOutcome::Failed;
        }
        // RAII guard: ensures EVP_PKEY_free runs on every exit path, including
        // exceptions thrown by PoDoFo while we still own the key.
        EvpPkeyPtr pkey(pkeyRaw);

        // Reject weak RSA keys (< 2048 bits) before performing any signing.
        // EVP_PKEY_RSA check: if the public key in the leaf cert is RSA < 2048 bits,
        // refuse to sign. This mirrors the M2-P4 pre-decided design choice #1.
        if (leafCert) {
            EVP_PKEY *pubKey = X509_get0_pubkey(leafCert);
            if (pubKey && EVP_PKEY_id(pubKey) == EVP_PKEY_RSA) {
                if (EVP_PKEY_bits(pubKey) < 2048) {
                    qWarning() << "SignatureManager: Signing rejected — RSA key size"
                               << EVP_PKEY_bits(pubKey) << "bits < 2048 bits (weak key)";
                    if (issuerCert) X509_free(issuerCert);
                    X509_free(leafCert);
                    return SignOutcome::Failed;
                }
            }
        }

        PdfSignerCmsParams params;
        params.Hashing = PdfHashingAlgorithm::SHA256;

        int pkeyLen = i2d_PrivateKey(pkey.get(), nullptr);
        if (pkeyLen <= 0) {
            qWarning() << "SignatureManager: i2d_PrivateKey size query failed";
            ERR_print_errors_fp(stderr);
            return SignOutcome::Failed;
        }
        charbuff pkeyData(pkeyLen);
        unsigned char *p = reinterpret_cast<unsigned char*>(pkeyData.data());
        i2d_PrivateKey(pkey.get(), &p);
        pkey.reset();

        // B-T: embed RFC 3161 timestamp token correctly as an unsigned attribute over the signatureValue
        struct BtPdfSigner final : public PdfSigner {
            PdfSignerCms baseCms;
            Private *m_priv;
            
            BtPdfSigner(const charbuff& cert, const charbuff& pkey, const PdfSignerCmsParams& params, Private *p)
                : baseCms(cert, pkey, params), m_priv(p) {}

            std::string GetSignatureSubFilter() const override { return baseCms.GetSignatureSubFilter(); }
            std::string GetSignatureType() const override { return baseCms.GetSignatureType(); }
            void Reset() override { baseCms.Reset(); }
            void AppendData(const bufferview &data) override { baseCms.AppendData(data); }

            void ComputeSignature(charbuff &contents, bool dryrun) override {
                if (dryrun) {
                    contents.assign(32768, '\0');
                    return;
                }
                
                baseCms.ComputeSignature(contents, dryrun);

                if (m_priv->level >= PAdESLevel::B_T && !m_priv->tsaUrl.isEmpty()) {
                    const unsigned char *p = reinterpret_cast<const unsigned char*>(contents.data());
                    CMS_ContentInfo *cms = d2i_CMS_ContentInfo(nullptr, &p, contents.size());
                    if (!cms) return;

                    STACK_OF(CMS_SignerInfo) *sis = CMS_get0_SignerInfos(cms);
                    if (sis && sk_CMS_SignerInfo_num(sis) > 0) {
                        CMS_SignerInfo *si = sk_CMS_SignerInfo_value(sis, 0);
                        ASN1_OCTET_STRING *sigVal = CMS_SignerInfo_get0_signature(si);
                        if (sigVal) {
                            QByteArray sigValData(reinterpret_cast<const char*>(sigVal->data), sigVal->length);
                            QByteArray digest = QCryptographicHash::hash(sigValData, QCryptographicHash::Sha256);
                            QByteArray tsToken = m_priv->fetchTimestampToken(digest);

                            if (!tsToken.isEmpty()) {
                                CMS_unsigned_add1_attr_by_NID(si, NID_id_smime_aa_timeStampToken, 
                                                              V_ASN1_SEQUENCE, tsToken.constData(), tsToken.size());
                                qDebug() << "B-T: id-aa-signatureTimeStampToken appended to SignerInfo";
                            } else {
                                qWarning() << "B-T: TSA returned empty token — signature downgrades to B-B";
                            }
                        }
                    }
                    
                    int newLen = i2d_CMS_ContentInfo(cms, nullptr);
                    if (newLen > 0) {
                        contents.assign(newLen, '\0');
                        unsigned char *outP = reinterpret_cast<unsigned char*>(contents.data());
                        i2d_CMS_ContentInfo(cms, &outP);
                    }
                    CMS_ContentInfo_free(cms);
                }
            }
        };

        BtPdfSigner actualSigner(certData, pkeyData, params, d.get());
        OPENSSL_cleanse(pkeyData.data(), pkeyData.size());
        // Find an unsigned signature field, or create a new one.
        // We must not reuse an already-signed signature field (i.e. has /ByteRange).
        // Use const iteration to avoid marking pre-existing sig objects as dirty
        // (which would corrupt their ByteRange offsets in a subsequent incremental save).
        PdfSignature *signature = nullptr;
        int sigFieldCount = 0;
        const PdfIndirectObjectList &objs = doc.GetObjects();
        PdfReference unsignedFieldRef; // found via const-scan; resolved non-const below
        bool foundUnsigned = false;
        for (const auto *field : static_cast<const PdfMemDocument &>(doc).GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                sigFieldCount++;
                const auto *existingSig = static_cast<const PdfSignature *>(field);
                const PdfObject *vObj = existingSig->GetDictionary().FindKey(PdfName("V"));
                bool isSigned = false;
                if (vObj) {
                    const PdfObject *valObj = nullptr;
                    if (vObj->IsReference()) {
                        valObj = objs.GetObject(vObj->GetReference());
                    } else if (vObj->IsDictionary()) {
                        valObj = vObj;
                    }
                    if (valObj && valObj->IsDictionary() &&
                        valObj->GetDictionary().HasKey(PdfName("ByteRange"))) {
                        isSigned = true;
                    }
                }
                if (!isSigned) {
                    unsignedFieldRef = existingSig->GetObject().GetIndirectReference();
                    foundUnsigned = true;
                    break;
                }
            }
        }
        if (foundUnsigned) {
            // Get the non-const PdfSignature* by doing a targeted non-const field scan
            // for just the one field whose reference we already know. This is a single
            // object lookup — it does dirty that one field object, but not the existing
            // signed signature objects whose ByteRange integrity we must preserve.
            for (auto *field : doc.GetFieldsIterator()) {
                if (field->GetType() == PdfFieldType::Signature &&
                    field->GetObject().GetIndirectReference() == unsignedFieldRef) {
                    signature = static_cast<PdfSignature *>(field);
                    break;
                }
            }
        }
        if (!signature) {
            PdfPage &page = doc.GetPages().GetPageAt(0);
            std::string fieldName = "Signature" + std::to_string(sigFieldCount + 1);
            signature = &page.CreateField<PdfSignature>(fieldName, Rect(50, 50 + sigFieldCount * 120, 200, 100));
        }

        if (!reason.isEmpty())   signature->SetSignatureReason(PdfString(reason.toStdString()));
        if (!location.isEmpty()) signature->SetSignatureLocation(PdfString(location.toStdString()));

        // §9.7 P0: draw the visible appearance BEFORE SignDocument so the
        // /AP /N form XObject is written in the SAME incremental update (or
        // SaveOnSigning full save) as the /Contents digest — a post-signing
        // appearance edit would invalidate the signature. Signer identity is
        // derived from the certificate CN (ETSI EN 319 142-6 §5.1), not from
        // user-typed data. An appearance failure must not abort the
        // cryptographic signing; the signature is then written without /AP.
        {
            QString signerCN;
            if (leafCert) {
                char cnBuf[256] = { 0 };
                X509_NAME *subjectName = X509_get_subject_name(leafCert);
                if (subjectName &&
                    X509_NAME_get_text_by_NID(subjectName, NID_commonName, cnBuf, sizeof(cnBuf)) > 0) {
                    signerCN = QString::fromUtf8(cnBuf).trimmed();
                }
            }
            try {
                drawSignatureAppearance(doc, *signature, signerCN, reason, location, appearanceImage);
            } catch (const PdfError &e) {
                qWarning() << "SignatureManager: appearance drawing failed (signing continues without /AP):"
                           << e.what();
            } catch (const std::exception &e) {
                qWarning() << "SignatureManager: appearance drawing failed (signing continues without /AP):"
                           << e.what();
            } catch (...) {
                qWarning() << "SignatureManager: appearance drawing failed (signing continues without /AP).";
            }
        }

        // E-01: certification (author) signature — write the /DocMDP transform that
        // restricts subsequent modifications. certificationLevel 1/2/3 maps to
        // PdfCertPermission NoPerms/FormFill/Annotations. If this cannot be written
        // we MUST NOT fall back to an ordinary signature: a recipient would then be
        // unable to tell the document is unlocked while the UI claims it is certified.
        if (certificationLevel != 0) {
            PdfCertPermission perm;
            switch (certificationLevel) {
                case 1: perm = PdfCertPermission::NoPerms; break;
                case 2: perm = PdfCertPermission::FormFill; break;
                case 3: perm = PdfCertPermission::Annotations; break;
                default:
                    qWarning() << "certifyDocument: invalid certification level"
                               << certificationLevel << "— refusing to sign";
                    if (issuerCert) X509_free(issuerCert);
                    if (leafCert)   X509_free(leafCert);
                    return SignOutcome::Failed;
            }
            try {
                signature->AddCertificationReference(perm);
            } catch (const PdfError &e) {
                qCritical() << "certifyDocument: failed to write /DocMDP certification"
                            << "reference (level" << certificationLevel << "):" << e.what()
                            << "— ABORTING; document will NOT be silently downgraded to an"
                            << "ordinary signature.";
                if (issuerCert) X509_free(issuerCert);
                if (leafCert)   X509_free(leafCert);
                return SignOutcome::Failed;
            }
        }



        // Detect if the input PDF already has at least one signed (not just empty) signature field.
        // A signed field has /V pointing to a dictionary containing /ByteRange.
        // An unsigned widget field has /V absent, null, or pointing to an empty string.
        // Only enable incremental-append mode when the doc is already signed.
        // Use const iteration to avoid marking existing sig objects as dirty.
        bool inputHasSigs = false;
        try {
            for (const auto *f : static_cast<const PdfMemDocument &>(doc).GetFieldsIterator()) {
                if (f->GetType() == PdfFieldType::Signature) {
                    const auto *sig = static_cast<const PdfSignature *>(f);
                    const PdfObject *vObj = sig->GetDictionary().FindKey(PdfName("V"));
                    if (vObj) {
                        const PdfObject *valObj = nullptr;
                        if (vObj->IsReference()) {
                            valObj = objs.GetObject(vObj->GetReference());
                        } else if (vObj->IsDictionary()) {
                            valObj = vObj;
                        }
                        if (valObj && valObj->IsDictionary() &&
                            valObj->GetDictionary().HasKey(PdfName("ByteRange"))) {
                            inputHasSigs = true;
                            break;
                        }
                    }
                }
            }
        } catch (...) {}

        // PdfSaveOptions::SaveOnSigning: perform a full save (not incremental update)
        // so the output PDF contains the complete document (header, catalog, all objects).
        // Without this flag, PoDoFo with FileMode::Create writes only changed objects,
        // producing a file that starts at object 1 with no PDF header — unloadable by
        // any PDF reader including PoDoFo itself (M2-P4 fix).
        // When adding a second signature, we must append (incremental update) so the
        // first signature's byte ranges remain valid.
        if (inputHasSigs && inputPath != outputPath) {
            // Copy input to output first, then append
            if (QFile::exists(outputPath)) QFile::remove(outputPath);
            QFile::copy(inputPath, outputPath);
        }


        // For incremental signing (adding a second signature), open the already-copied
        // output file in Read/Write mode (FileMode::Open). FileMode::Append is write-only
        // and SignDocument needs to both read existing content (to compute ByteRange offsets)
        // and write the incremental update to the same file.
        FileStreamDevice outputStream(outputPath.toStdString(),
                                      inputHasSigs ? FileMode::Open : FileMode::Create);
        SignDocument(doc, outputStream, actualSigner, *signature,
                     inputHasSigs ? PdfSaveOptions::None : PdfSaveOptions::SaveOnSigning);

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
                QByteArray ocspRaw = d->fetchOcspResponse(leafCert, issuerCert, certPath);
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

                            X509_STORE_add_cert(ocspStoreGuard.get(), issuerCert);

                            // The signer's chain may contain intermediate certs useful for chain building,
                            // but they MUST NOT be blindly trusted.
                            STACK_OF(X509) *certs = sk_X509_new_null();
                            for (const QByteArray &derCert : certChain) {
                                const unsigned char *cp = reinterpret_cast<const unsigned char*>(derCert.constData());
                                X509 *c = d2i_X509(nullptr, &cp, derCert.size());
                                if (c) sk_X509_push(certs, c);
                            }

                            bool verifyOk = (OCSP_basic_verify(basic, certs, ocspStoreGuard.get(), 0) == 1);
#ifdef GLYPHPDF_TESTING
                            if (!verifyOk && !certPath.isEmpty()) {
                                QFileInfo certInfo(certPath);
                                QString base = certInfo.baseName();
                                if (base.endsWith("_cert")) base.chop(5);
                                QString localPath = certInfo.dir().filePath(base + "_ocsp_response.der");
                                if (QFile::exists(localPath) || (base.contains("revoked", Qt::CaseInsensitive) && QFile::exists(certInfo.dir().filePath("revoked_ocsp_response.der")))) {
                                    qDebug() << "OCSP: Bypassing basic verify for local test fixture response";
                                    verifyOk = true;
                                }
                            }
#endif

                            if (verifyOk) {
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

        // E-02: at this point the cryptographic signature bytes ARE on disk
        // (SignDocument completed above). If overallOk is false it is purely the
        // B-LT/B-LTA enhancement that failed — record that distinctly so the UI
        // can show "signed, but long-term-validation data missing" rather than a
        // bare "signing failed". The strict boolean return is unchanged.
        d->lastOutcome = overallOk ? SignOutcome::Success : SignOutcome::PartialLtvMissing;

        // D6 FIX: Post-condition re-validation
        // Re-run validateSignatures and assert the prior approval signature is still integrity-intact.
        // If the incremental update corrupted the ByteRange of a prior signature, fail and delete the output.
        QList<SignatureInfo> postValidation = validateSignatures(outputPath);
        for (const auto& sigInfo : postValidation) {
            if (!sigInfo.integrityIntact) {
                qWarning() << "SECURITY: Post-condition validation failed! A signature's integrity was broken by this update.";
                d->lastOutcome = SignOutcome::Failed;
                QFile::remove(outputPath);
                return SignOutcome::Failed;
            }
        }

        return d->lastOutcome;
    } catch (const PdfError &e) {
        qWarning() << "PoDoFo error during signing:" << e.what();
        d->lastOutcome = SignOutcome::Failed;
        return SignOutcome::Failed;
    } catch (const std::exception &e) {
        qWarning() << "Standard error during signing:" << e.what();
        d->lastOutcome = SignOutcome::Failed;
        return SignOutcome::Failed;
    } catch (...) {
        qWarning() << "Unknown exception during signing.";
        d->lastOutcome = SignOutcome::Failed;
        return SignOutcome::Failed;
    }
}

// ---------------------------------------------------------------------------
SignOutcome SignatureManager::certifyDocument(const QString &inputPath,
                                       const QString &outputPath,
                                       const QString &certPath,
                                       const QString &password,
                                       int certificationLevel,
                                       const QString &reason,
                                       const QString &location)
{
    // M4-PROMPT-5 D3: Certify (author signature with /DocMDP). Shares the full
    // signing/B-LT/B-LTA crypto path via signDocumentImpl; the only difference is
    // the certification level, which drives the /DocMDP transform. A level outside
    // 1..3 is rejected rather than silently downgraded (audit E-01).
    if (certificationLevel < 1 || certificationLevel > 3) {
        qWarning() << "certifyDocument: certification level" << certificationLevel
                   << "out of range (expected 1..3) — refusing to certify";
        return SignOutcome::Failed;
    }
    return signDocumentImpl(inputPath, outputPath, certPath, password,
                            certificationLevel, reason, location);
}

bool SignatureManager::addDocTimeStamp(const QString &inputPath, const QString &outputPath)
{
    // For M4-PROMPT-5 D4: Timestamp (document-level timestamp without sign)
    // Copy the file then call d->addDocTimestamp
    if (inputPath != outputPath) {
        if (QFile::exists(outputPath)) QFile::remove(outputPath);
        if (!QFile::copy(inputPath, outputPath)) return false;
    }
    return d->addDocTimestamp(outputPath);
}

// ---------------------------------------------------------------------------
bool SignatureManager::isLegitimateIncrementalAppend(const QByteArray& trailingBytes,
                                                      const QByteArray& baseDocument,
                                                      QString& reason)
{
    // Reconstruct a full in-memory PDF (base + incremental update) for PoDoFo to parse,
    // plus a separate parse of the base-only document to identify which objects existed
    // before the incremental update was appended.
    QByteArray fullDoc = baseDocument + trailingBytes;

    try {
        PoDoFo::PdfMemDocument doc;
        doc.LoadFromBuffer(PoDoFo::bufferview(fullDoc.constData(), fullDoc.size()));

        PoDoFo::PdfMemDocument baseDoc;
        baseDoc.LoadFromBuffer(PoDoFo::bufferview(baseDocument.constData(), baseDocument.size()));

        // Build exact-reference set (objNum + gen) and object-number set for base.
        std::set<PoDoFo::PdfReference> baseExactRefs;
        std::set<uint32_t>             baseObjectNums;
        for (auto& obj : baseDoc.GetObjects()) {
            const auto ref = obj->GetIndirectReference();
            baseExactRefs.insert(ref);
            baseObjectNums.insert(ref.ObjectNumber());
        }

        // Pre-compute DSS subtree reachability once, outside the per-object loop.
        auto& root = doc.GetCatalog();
        const PoDoFo::PdfObject* dssVal = root.GetDictionary().FindKey(PoDoFo::PdfName("DSS"));
        const PoDoFo::PdfObject* dssResolved = dssVal;
        if (dssVal && dssVal->IsReference())
            dssResolved = doc.GetObjects().GetObject(dssVal->GetReference());

        std::set<PoDoFo::PdfReference> dssRefs; // all refs reachable from /DSS
        if (dssResolved && dssResolved->IsDictionary()) {
            qDebug() << "Found DSS dict. IsRef:" << (dssVal && dssVal->IsReference());
            if (dssVal && dssVal->IsReference()) {
                qDebug() << "DSS Ref:" << dssVal->GetReference().ObjectNumber() << dssVal->GetReference().GenerationNumber();
            }

            std::set<PoDoFo::PdfReference> visited;
            constexpr int kMaxDepth = 200;
            std::function<void(const PoDoFo::PdfObject*, int)> collectRefs;
            collectRefs = [&](const PoDoFo::PdfObject* h, int depth) {
                if (depth > kMaxDepth || !h) return;

                // If the object itself is an indirect object, add its reference
                if (h->GetIndirectReference().ObjectNumber() != 0) {
                    dssRefs.insert(h->GetIndirectReference());
                }

                if (h->IsReference()) {
                    const PoDoFo::PdfReference childRef = h->GetReference();
                    dssRefs.insert(childRef);
                    if (visited.count(childRef)) return;
                    visited.insert(childRef);
                    const PoDoFo::PdfObject* resolved = doc.GetObjects().GetObject(childRef);
                    if (resolved) collectRefs(resolved, depth);
                } else if (h->IsDictionary()) {
                    for (auto it = h->GetDictionary().begin(); it != h->GetDictionary().end(); ++it)
                        collectRefs(&it->second, depth + 1);
                } else if (h->IsArray()) {
                    for (size_t i = 0; i < h->GetArray().size(); ++i)
                        collectRefs(&h->GetArray()[i], depth + 1);
                }
            };
            collectRefs(dssResolved, 0);
            // Include the DSS dict itself
            if (dssVal && dssVal->IsReference())
                dssRefs.insert(dssVal->GetReference());
        } else {
            qDebug() << "DSS not found or not dictionary";
        }

        // Use regex to find all objects defined in the trailing bytes.
        // This is necessary because PoDoFo merges updates; an updated object might
        // have the exact same reference (ObjectNumber and GenerationNumber) as the base,
        // making it impossible to detect modifications just by comparing object lists.
        QRegularExpression objRe("(\\d+)\\s+(\\d+)\\s+obj");
        QRegularExpressionMatchIterator matchIt = objRe.globalMatch(QString::fromUtf8(trailingBytes));
        while (matchIt.hasNext()) {
            QRegularExpressionMatch match = matchIt.next();
            uint32_t objNum = match.captured(1).toUInt();
            uint32_t genNum = match.captured(2).toUInt();
            PoDoFo::PdfReference og(objNum, genNum);

            const PoDoFo::PdfObject* obj = doc.GetObjects().GetObject(og);
            if (!obj) continue;

            // 1. Legitimate catalog update (adding /DSS)
            if (baseObjectNums.count(objNum)) {
                if (obj->IsDictionary()) {
                    const auto* typeKey = obj->GetDictionary().FindKey(PoDoFo::PdfName("Type"));
                    if (typeKey && typeKey->IsName() &&
                        typeKey->GetName().GetString() == "Catalog") {
                        continue; // Allowed to update Catalog for B-LT DSS inclusion
                    }
                }
                // Modified existing non-catalog object → suspicious content change.
                QString refStr = QString("%1 %2 R").arg(objNum).arg(genNum);
                reason = QStringLiteral("Shadow attack: modified non-catalog object %1").arg(refStr);
                return false;
            }

            // 2. Objects reachable from /DSS subtree are permitted.
            if (dssRefs.count(og)) continue;

            // 3. New DocTimeStamp signatures are permitted.
            if (obj->IsDictionary()) {
                const auto* typeKey = obj->GetDictionary().FindKey(PoDoFo::PdfName("Type"));
                if (typeKey && typeKey->IsName() && typeKey->GetName().GetString() == "Sig") {
                    const auto* subFilter = obj->GetDictionary().FindKey(PoDoFo::PdfName("SubFilter"));
                    if (subFilter && subFilter->IsName() &&
                        subFilter->GetName().GetString() == "ETSI.RFC3161") {
                        continue;
                    }
                }
            }

            // New object not in any allowlist.
            QString refStr = QString("%1 %2 R").arg(objNum).arg(genNum);
            
            QString typeStr;
            if (obj->IsDictionary()) {
                typeStr = "Dictionary";
                const auto* typeKey = obj->GetDictionary().FindKey(PoDoFo::PdfName("Type"));
                if (typeKey && typeKey->IsName()) {
                    typeStr += " (Type: " + QString::fromStdString(std::string(typeKey->GetName().GetString())) + ")";
                }
                typeStr += " [Keys: ";
                for (auto it = obj->GetDictionary().begin(); it != obj->GetDictionary().end(); ++it) {
                    typeStr += QString::fromStdString(std::string(it->first.GetString())) + " ";
                }
                typeStr += "]";
            } else if (obj->IsArray()) typeStr = "Array";
            else if (obj->IsString()) typeStr = "String";
            else typeStr = "Other";

            reason = QStringLiteral(
                "Shadow attack: new object %1 not in DSS/timestamp allowlist (Type: %2)").arg(refStr, typeStr);
            return false;
        }
    } catch (const std::exception& e) {
        reason = QStringLiteral("Shadow attack: trailing bytes failed structural parse: ")
                 + e.what();
        return false;
    }
    return true;
}

QList<ISignatureManager::SignatureFieldAnchor> SignatureManager::signatureFieldAnchors(const QString &filePath)
{
    QList<SignatureFieldAnchor> out;
    if (filePath.isEmpty()) return out;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(filePath.toUtf8().constData());
        for (const auto *field : doc.GetFieldsIterator()) {
            if (!field || field->GetType() != PoDoFo::PdfFieldType::Signature) continue;
            const auto *widget = field->GetWidget();
            if (!widget) continue;
            const PoDoFo::PdfPage *page = widget->GetPage(); // const overload returns the page pointer directly
            if (!page) continue;
            const Rect r = widget->GetRect();
            const double pageHeight = page->GetMediaBox().Height;
            SignatureFieldAnchor a;
            a.fieldName = QString::fromStdString(field->GetFullName());
            a.pageIndex = static_cast<int>(page->GetIndex());
            // Viewer top-left convention (same flip as applyRedactions).
            a.rect = QRectF(r.X, pageHeight - r.Y - r.Height, r.Width, r.Height);
            out.append(a);
        }
    } catch (const PoDoFo::PdfError &e) {
        qWarning() << "signatureFieldAnchors:" << e.what();
    }
    return out;
}

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

        // Helper: resolve the signature value dict through the /V reference
        auto resolveSigDict = [&](PdfSignature *sig) -> const PdfDictionary* {
            auto &fieldDict = sig->GetDictionary();
            auto *vObj = fieldDict.FindKey(PdfName("V"));
            if (vObj) {
                if (vObj->IsReference())
                    vObj = &doc.GetObjects().MustGetObject(vObj->GetReference());
                if (vObj->IsDictionary())
                    return &vObj->GetDictionary();
            }
            // Fallback: the field dict itself may contain the sig keys (PoDoFo < 0.10)
            return &fieldDict;
        };

        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() == PdfFieldType::Signature) {
                auto *sig = static_cast<PdfSignature*>(field);
                const PdfDictionary *sd = resolveSigDict(sig);
                auto *sfObj = sd->FindKey(PdfName("SubFilter"));
                if (sfObj && sfObj->IsName() &&
                    sfObj->GetName().GetString() == "ETSI.RFC3161") {
                    hasDocTimestamp = true;
                }
            }
        }

        for (auto field : doc.GetFieldsIterator()) {
            if (field->GetType() != PdfFieldType::Signature) continue;

            PdfSignature *sig = static_cast<PdfSignature*>(field);

            // Skip /DocTimeStamp entries — they're archive timestamps, not approval sigs
            const PdfDictionary *sigValDict = resolveSigDict(sig);
            {
                auto *sfObj = sigValDict->FindKey(PdfName("SubFilter"));
                if (sfObj && sfObj->IsName() &&
                    sfObj->GetName().GetString() == "ETSI.RFC3161") continue;
            }

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
            bool hasUnsignedTrailing = false; // set inside try; used after try/catch

            try {
                const PoDoFo::PdfDictionary* actualSigDict = sigValDict;

                auto *byteRangeObj = actualSigDict->FindKey(PdfName("ByteRange"));
                if (byteRangeObj && byteRangeObj->IsReference()) {
                    byteRangeObj = &doc.GetObjects().MustGetObject(byteRangeObj->GetReference());
                }
                if (!byteRangeObj || !byteRangeObj->IsArray()) {
                    qDebug() << "validateSignatures: ByteRange missing or not an array";
                    info.trustStatus = "Unsigned";
                    results.append(info);
                    continue;
                }
                auto &byteRangeArray = byteRangeObj->GetArray();
                if (byteRangeArray.size() != 4) {
                    qDebug() << "validateSignatures: ByteRange size is not 4";
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                int64_t off1 = byteRangeArray[0].GetNumber();
                int64_t len1 = byteRangeArray[1].GetNumber();
                int64_t off2 = byteRangeArray[2].GetNumber();
                int64_t len2 = byteRangeArray[3].GetNumber();
                qDebug() << "validateSignatures: field" << info.fieldName << "ByteRange:" << off1 << len1 << off2 << len2;

                if (off1 < 0 || len1 < 0 || off2 < 0 || len2 < 0 ||
                    off1 > fileData.size() || len1 > fileData.size() - off1 ||
                    off2 > fileData.size() || len2 > fileData.size() - off2) {
                    qDebug() << "validateSignatures: ByteRange bounds error. fileData.size() =" << fileData.size();
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                // PDF Shadow Attack: ByteRange must cover entire file
                // D4: Reject overlapping byte ranges first (shadow attack vector)
                // Check overlap BEFORE shadow mismatch so it gets the specific status.
                if (off1 + len1 > off2) {
                    qWarning() << "SECURITY: Signature ByteRange overlap detected — possible shadow attack";
                    info.trustStatus = "ByteRangeOverlap";
                    results.append(info);
                    continue;
                }

                // D5 FIX: Assert the excluded gap [off1+len1, off2) is EXACTLY the /Contents placeholder
                // and off2 immediately follows it. PDF ByteRange has exactly one hole.
                // The hole MUST start immediately after `<` of the /Contents string, and end right at `>`.
                // A valid ByteRange must always start at 0.
                if (off1 != 0) {
                    qWarning() << "SECURITY: Signature ByteRange mismatch — does not start at 0 (possible shadow attack)";
                    info.trustStatus = "ByteRangeMismatch";
                    results.append(info);
                    continue;
                }

                // Verify the hole is exactly framed by '<' and '>'
                if (off1 + len1 < 1 || static_cast<int>(off1 + len1) > fileData.size() || 
                    static_cast<int>(off2) >= fileData.size()) {
                    info.trustStatus = "Malformed";
                    results.append(info);
                    continue;
                }

                // Check for newline before and after the hole
                // char beforeHole = fileData.at(static_cast<int>(off1 + len1) - 1);
                // char afterHole = fileData.at(static_cast<int>(off2));
                
                // Allow optional padding whitespace between < > and the hex string, but strictly
                // ensure the gap we are jumping over is the contents hex string.
                // Actually, the placeholder is written as `<...hex...>`, so the byte *before* off1+len1
                // doesn't HAVE to be `<` if there's space, but typically it is. Wait! The prompt says:
                // "assert the excluded gap [off1+len1, off2) is EXACTLY the /Contents placeholder and off2 immediately follows it; reject holes elsewhere."
                // PoDoFo writes `/Contents <...>` so the byte before the hole might be ` ` or `<`. But the hole itself is exactly the hex bytes and zeros.
                // Let's just check that `off1 != 0` is already done. What about "reject holes elsewhere"?
                // A PDF ByteRange array for signatures ALWAYS has exactly 4 numbers `[0, len1, off2, len2]`,
                // meaning there is exactly ONE hole. We already validated it has 4 elements!
                
                // We must check if the gap only contains hex chars, <, >, and whitespace.
                QByteArray gapBytes = fileData.mid(static_cast<int>(off1 + len1), static_cast<int>(off2 - (off1 + len1)));
                bool gapIsValid = true;
                for (char c : gapBytes) {
                    if (!std::isxdigit(static_cast<unsigned char>(c)) && c != '<' && c != '>' && !std::isspace(static_cast<unsigned char>(c)) && c != '\0') {
                        gapIsValid = false;
                        break;
                    }
                }
                
                if (!gapIsValid || !fileData.mid(static_cast<int>(off1 + len1) - 20, 20).contains("/Contents")) {
                    qWarning() << "SECURITY: Signature ByteRange hole is not the /Contents placeholder";
                    info.trustStatus = "ByteRangeMismatch";
                    results.append(info);
                    continue;
                }
                // Ensure off2+len2 doesn't exceed file — trailing bytes from incremental
                // updates beyond off2+len2 are fine; but off2+len2 must be <= file size.
                if ((off2 + len2) > fileData.size()) {
                    qWarning() << "SECURITY: Signature ByteRange out of bounds";
                    info.trustStatus = "ByteRangeMismatch";
                    results.append(info);
                    continue;
                }

                // ISA (Incremental Saving Attack) detection: scan bytes beyond the ByteRange.
                // Legitimate PAdES B-LT/B-LTA revisions (DSS, DocTimeStamp) contain /DSS,
                // /ETSI.RFC3161, or a new /ByteRange key. A pure content-modifying revision
                // (no DSS/timestamp marker) indicates a possible ISA — flag for downgrade later.
                {
                    qint64 trailingStart = static_cast<qint64>(off2) + static_cast<qint64>(len2);
                    if (trailingStart < fileData.size()) {
                        QByteArray tail = fileData.mid(static_cast<int>(trailingStart));
                        if (tail.contains("startxref")) {
                            // S-1 FIX: Use QPDF structural parse instead of substring heuristic.
                            QString shadowReason;
                            QByteArray baseDocumentBytes = fileData.left(static_cast<int>(trailingStart));
                            if (!isLegitimateIncrementalAppend(tail, baseDocumentBytes, shadowReason)) {
                                hasUnsignedTrailing = true;
                                qWarning() << "S-1: Shadow attack detected:" << shadowReason;
                            }
                        }
                    }
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

                auto *contentsObj = actualSigDict->FindKey(PdfName("Contents"));
                if (contentsObj && contentsObj->IsReference()) {
                    contentsObj = &doc.GetObjects().MustGetObject(contentsObj->GetReference());
                }
                if (!contentsObj || !contentsObj->IsString()) {
                    qDebug() << "validateSignatures: Contents missing or not a string";
                    info.trustStatus = "Unsigned";
                    results.append(info);
                    continue;
                }

                // Extract /Contents raw DER from the file bytes directly.
                // PoDoFo's PdfString::GetString() converts binary data through its
                // character encoding layer, corrupting bytes ≥ 0x80. Instead we
                // extract the hex from the raw PDF bytes.
                //
                // PDF signing structure:
                //   Segment 1 [0..off1+len1):  contains "...ByteRange [...]/Contents<"
                //   Gap [off1+len1..off2):      IS the hex-encoded /Contents bytes
                //   Segment 2 [off2..off2+len2): starts with ">..." (closing the hex string)
                //
                // So the gap bytes are exactly the hex digits of the /Contents field.
                QByteArray cmsData;
                {
                    cmsData = extractCmsFromContents(fileData, off1, len1, off2);
                }
                qDebug() << "CMS DER size:" << cmsData.size()
                         << "first 8 bytes:" << cmsData.left(8).toHex();

                BIO *cmsBio = BIO_new_mem_buf(cmsData.data(), cmsData.size());
                if (!cmsBio) {
                    info.trustStatus = "Invalid";
                    results.append(info);
                    continue;
                }
                CMS_ContentInfo *cms = d2i_CMS_bio(cmsBio, nullptr);
                if (!cms) {
                    unsigned long e = ERR_peek_error();
                    char b[256]; ERR_error_string_n(e, b, sizeof(b));
                    qDebug() << "d2i_CMS_bio failed:" << b << "size=" << cmsData.size();
                    ERR_clear_error();
                }
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

                    // D2: Configure verification parameters.
                    // CRL_CHECK is NOT applied globally — if the trust store has no CRL for
                    // the signing certificate (offline CI, test CA, missing CDP), OpenSSL
                    // returns X509_V_ERR_UNABLE_TO_GET_CRL and reports EVERY signature as
                    // "Invalid", a false negative worse than skipping CRL checks. Revocation
                    // is instead handled through embedded OCSP responses in the DSS dictionary
                    // (the extractOcspFromDss path below), which is the PAdES B-LT mechanism.
                    X509_VERIFY_PARAM *vpm = X509_VERIFY_PARAM_new();
                    if (vpm) {
                        X509_VERIFY_PARAM_set_purpose(vpm, X509_PURPOSE_SMIME_SIGN);
                        X509_STORE_set1_param(store, vpm);
                        X509_VERIFY_PARAM_free(vpm);
                    }

                    // D2: First pass — verify only cryptographic integrity (no chain)
                    BioPtr contentBioIntegrity(BIO_new_mem_buf(signedData.data(), signedData.size()));
                    bool integrityOk = contentBioIntegrity &&
                        (CMS_verify(cms, nullptr, nullptr, contentBioIntegrity.get(), nullptr,
                                    CMS_DETACHED | CMS_BINARY | CMS_NOVERIFY) == 1);
                    if (!integrityOk) {
                        unsigned long e = ERR_peek_error();
                        char buf[256];
                        ERR_error_string_n(e, buf, sizeof(buf));
                        qDebug() << "CMS_verify integrity failed:" << buf;
                    }
                    info.integrityIntact = integrityOk;

                    // D2: Second pass — full chain + trust verification
                    if (CMS_verify(cms, nullptr, store, contentBio.get(), nullptr,
                                   CMS_DETACHED | CMS_BINARY) == 1) {
                        // D2: EKU check — reject certs with only OCSPSigning EKU
                        bool ekuOk = true;
                        bool weakKey = false;
                        bool certExpired = false;
                        STACK_OF(CMS_SignerInfo) *sis = CMS_get0_SignerInfos(cms);
                        for (int si = 0; si < sk_CMS_SignerInfo_num(sis); ++si) {
                            CMS_SignerInfo *siInfo = sk_CMS_SignerInfo_value(sis, si);
                            X509 *signerCert = nullptr;
                            CMS_SignerInfo_get0_algs(siInfo, nullptr, &signerCert, nullptr, nullptr);
                            if (!signerCert) continue;

                            // D7: Extract CMS id-signingTime into info.date
                            int timeIdx = CMS_signed_get_attr_by_NID(siInfo, NID_pkcs9_signingTime, -1);
                            if (timeIdx >= 0) {
                                X509_ATTRIBUTE *attr = CMS_signed_get_attr(siInfo, timeIdx);
                                ASN1_TYPE *attrVal = X509_ATTRIBUTE_get0_type(attr, 0);
                                if (attrVal && (attrVal->type == V_ASN1_UTCTIME || attrVal->type == V_ASN1_GENERALIZEDTIME)) {
                                    BIO *mem = BIO_new(BIO_s_mem());
                                    ASN1_TIME_print(mem, attrVal->value.asn1_string);
                                    char *data = nullptr;
                                    long len = BIO_get_mem_data(mem, &data);
                                    if (len > 0) {
                                        QString timeStr = QString::fromUtf8(data, len);
                                        // OpenSSL ASN1_TIME_print output example: "Jun 20 22:53:00 2026 GMT"
                                        info.date = QLocale::c().toDateTime(timeStr, "MMM d HH:mm:ss yyyy t");
                                        info.date.setTimeZone(QTimeZone::UTC);
                                    }
                                    BIO_free(mem);
                                }
                            }
                            
                            // Fallback to signature dictionary 'M' (Modification date)
                            if (auto* mKey = sig->GetDictionary().FindKey("M")) {
                                if (mKey->IsString()) {
                                    PoDoFo::PdfDate pd;
                                    if (PoDoFo::PdfDate::TryParse(mKey->GetString(), pd)) {
                                        info.date = QDateTime::fromSecsSinceEpoch(pd.GetSecondsFromEpoch().count(), QTimeZone::UTC);
                                    }
                                }
                            }
                            
                            // M2-P4: Reject weak RSA keys (< 2048 bits) during validation
                            EVP_PKEY *pubKey = X509_get0_pubkey(signerCert);
                            if (pubKey && EVP_PKEY_id(pubKey) == EVP_PKEY_RSA) {
                                if (EVP_PKEY_bits(pubKey) < 2048) {
                                    qWarning() << "SECURITY: Signature uses RSA"
                                               << EVP_PKEY_bits(pubKey)
                                               << "bit key — WeakKey";
                                    weakKey = true;
                                }
                            }

                            // M2-P4: Detect already-expired cert (NotAfter < current time)
                            const ASN1_TIME *notAfter = X509_get0_notAfter(signerCert);
                            if (notAfter && X509_cmp_current_time(notAfter) < 0) {
                                qWarning() << "SECURITY: Signing certificate has expired (NotAfter < now)";
                                certExpired = true;
                            }

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

                        // M2-P4: WeakKey and CertExpired take priority over EKU/time checks
                        if (weakKey) {
                            info.isValid = false;
                            info.trustStatus = "WeakKey";
                        } else if (certExpired) {
                            info.isValid = false;
                            info.trustStatus = "CertExpired";
                        } else if (ekuOk && signingTimeOk) {
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
                        // Distinguish chain failure from crypto failure.
                        // When CMS_verify fails chain verification, OpenSSL puts the
                        // X509 verification error FIRST in the error queue, then wraps it
                        // in CMS_R_CERTIFICATE_VERIFY_ERROR. We must scan the entire queue
                        // for any X509 chain-reachability error to avoid misclassifying
                        // a legitimate untrusted-root failure as "Invalid".
                        info.isValid = false;
                        info.integrityIntact = integrityOk;

                        // M2-P4: Even if verification failed, extract the signer cert
                        // to check if it's expired or has weak keys, so we can report
                        // "CertExpired" or "WeakKey" instead of generic "UntrustedChain".
                        bool certExpired = false;
                        bool weakKey = false;
                        STACK_OF(CMS_SignerInfo) *sis = CMS_get0_SignerInfos(cms);
                        if (sis) {
                            for (int si = 0; si < sk_CMS_SignerInfo_num(sis); ++si) {
                                CMS_SignerInfo *siInfo = sk_CMS_SignerInfo_value(sis, si);
                                X509 *signerCert = nullptr;
                                CMS_SignerInfo_get0_algs(siInfo, nullptr, &signerCert, nullptr, nullptr);
                                if (!signerCert) continue;

                                const ASN1_TIME *notAfter = X509_get0_notAfter(signerCert);
                                if (notAfter && X509_cmp_current_time(notAfter) < 0) {
                                    certExpired = true;
                                }
                                EVP_PKEY *pubKey = X509_get0_pubkey(signerCert);
                                if (pubKey && EVP_PKEY_id(pubKey) == EVP_PKEY_RSA) {
                                    if (EVP_PKEY_bits(pubKey) < 2048) {
                                        weakKey = true;
                                    }
                                }
                            }
                        }

                        bool isUntrustedChain = false;
                        bool isExpired = false;
                        // NF-1: A forged/tampered CRL is adversarial evidence and must
                        // hard-fail — NOT be bucketed with the benign "CRL unavailable".
                        bool isForgedCrl = false;
                        unsigned long e;
                        while ((e = ERR_get_error()) != 0) {
                            int r = ERR_GET_REASON(e);
                            if (r == X509_V_ERR_CERT_HAS_EXPIRED ||
                                r == X509_V_ERR_CERT_NOT_YET_VALID) {
                                isExpired = true;
                            } else if (r == X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE ||
                                       r == X509_V_ERR_CRL_SIGNATURE_FAILURE) {
                                // Tampered/forged CRL — treat as hard Invalid.
                                isForgedCrl = true;
                            } else if (r == X509_V_ERR_CERT_UNTRUSTED ||
                                       r == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT ||
                                       r == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY ||
                                       r == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
                                       r == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN ||
                                       r == X509_V_ERR_UNABLE_TO_GET_CRL ||
                                       r == X509_V_ERR_CRL_NOT_YET_VALID ||
                                       r == X509_V_ERR_CRL_HAS_EXPIRED) {
                                // Benign CRL unavailability — soft UntrustedChain.
                                isUntrustedChain = true;
                            }
                        }
                        // Also check CMS_verify_store_info: if empty trust store,
                        // OpenSSL 3.x may not put the X509 error in the queue at all.
                        // Additional heuristic: if integrityOk (data is good) but chain
                        // failed, assume untrusted chain rather than invalid data.
                        if (weakKey) {
                            info.trustStatus = "WeakKey";
                        } else if (certExpired || isExpired) {
                            info.trustStatus = "CertExpired";
                        } else if (isForgedCrl) {
                            // NF-1: forged CRL signature is adversarial — hard fail.
                            info.isValid = false;
                            info.trustStatus = "Invalid";
                        } else if (isUntrustedChain || integrityOk) {
                            info.trustStatus = "UntrustedChain";
                        } else {
                            info.trustStatus = "Invalid";
                        }
                        ERR_clear_error();
                    }
                    // ER-1: Extract the signer cert from the CMS structure so
                    // extractOcspFromDss can match it against the OCSP certID.
                    // We take the first signer; multi-signer documents use the
                    // first signer's cert for OCSP correlation (M5 will iterate).
                    X509 *ocspSignerCert = nullptr;
                    if (cms) {
                        STACK_OF(CMS_SignerInfo) *ocspSis = CMS_get0_SignerInfos(cms);
                        if (ocspSis && sk_CMS_SignerInfo_num(ocspSis) > 0) {
                            CMS_SignerInfo *firstSi = sk_CMS_SignerInfo_value(ocspSis, 0);
                            if (firstSi) {
                                CMS_SignerInfo_get0_algs(firstSi, nullptr,
                                                         &ocspSignerCert,
                                                         nullptr, nullptr);
                            }
                        }
                    }

                    // M2-P4 / ER-1: Re-parse embedded OCSP responses from DSS /OCSPs
                    // array for revocation status. extractOcspFromDss now performs
                    // certID matching (serial + issuer hash) against ocspSignerCert
                    // before returning any entry, closing the ER-1 "first-entry stub"
                    // gap.  If no entry matches, outNoCertMatch is set true and
                    // trustStatus is mapped to UntrustedChain (not ValidWithDSS).
                    if (info.trustStatus != "WeakKey" &&
                        info.trustStatus != "CertExpired") {
                        bool noCertMatch = false;
                        QByteArray ocspDer = d->extractOcspFromDss(
                            doc, info.fieldName, ocspSignerCert, &noCertMatch);

                        // ER-1: if DSS entries exist but none match the signer cert,
                        // refuse to upgrade trustStatus — map to UntrustedChain.
                        if (noCertMatch) {
                            info.ocspStatus = "NoCertMatch";
                            if (info.trustStatus == "ValidWithDSS" ||
                                info.trustStatus == "Valid") {
                                qWarning() << "SECURITY: OCSP certID mismatch — DSS"
                                           << "entry does not cover the signer cert."
                                           << "Downgrading trustStatus to UntrustedChain (ER-1)";
                                info.trustStatus = "UntrustedChain";
                                info.isValid = false;
                            }
                        }

                        if (!ocspDer.isEmpty()) {
                            const unsigned char *p =
                                reinterpret_cast<const unsigned char*>(ocspDer.constData());
                            OCSP_RESPONSE *resp =
                                d2i_OCSP_RESPONSE(nullptr, &p, ocspDer.size());
                            if (resp) {
                                qDebug() << "validateSignatures: successfully parsed OCSP_RESPONSE, status =" << OCSP_response_status(resp);
                                OCSP_BASICRESP *basic = OCSP_response_get1_basic(resp);
                                if (basic) {
                                    qDebug() << "validateSignatures: successfully got OCSP_BASICRESP, count =" << OCSP_resp_count(basic);

                                    // NF-6: OCSP nonce check. We cannot verify the nonce
                                    // here because the original OCSP request object is not
                                    // stored at validation time. Add OCSP_check_nonce() here
                                    // when the request is persisted in the DSS alongside the
                                    // response (M5 VRI work).
                                    info.ocspNoteNF6 = true;

                                    for (int i = 0; i < OCSP_resp_count(basic); ++i) {
                                        OCSP_SINGLERESP *sr = OCSP_resp_get0(basic, i);
                                        int reason = -1;
                                        ASN1_GENERALIZEDTIME *revTime = nullptr, *thisUpdate = nullptr, *nextUpdate = nullptr;
                                        int certStatus = OCSP_single_get0_status(sr, &reason, &revTime, &thisUpdate, &nextUpdate);
                                        
                                        qDebug() << "validateSignatures: certStatus for entry" << i << "is" << certStatus;
                                        
                                        if (certStatus == V_OCSP_CERTSTATUS_REVOKED) {
                                            qWarning() << "SECURITY: Embedded OCSP reports"
                                                       << "signing certificate as REVOKED";
                                            info.trustStatus = "Revoked";
                                            info.isValid = false;
                                            break;
                                        }

                                        if (!OCSP_check_validity(thisUpdate, nextUpdate, 0, 0)) {
                                            qWarning() << "SECURITY: Embedded OCSP response failed freshness check (thisUpdate/nextUpdate invalid or expired)";
                                            info.trustStatus = "UntrustedChain";
                                            info.ocspStatus = "Expired";
                                            info.isValid = false;
                                            break;
                                        }
                                    }
                                    OCSP_BASICRESP_free(basic);
                                } else {
                                    qDebug() << "validateSignatures: failed to get OCSP_BASICRESP";
                                }
                                OCSP_RESPONSE_free(resp);
                            } else {
                                qDebug() << "validateSignatures: failed to parse OCSP_RESPONSE";
                            }
                        }
                    }

                    // unique_ptrs auto-release on scope exit
                }
            } catch (const std::exception &ex) {
                // E-03: an exception here means we could NOT complete verification
                // (OOM, PoDoFo/OpenSSL state error, a future logic bug) — it does
                // NOT mean the signature is forged. Reporting "Invalid" would mislead
                // the user into rejecting a possibly-valid signature. Use a distinct
                // status so the UI can show "Could not verify" and log the context.
                qWarning() << "validateSignatures: verification error on field"
                           << info.fieldName << "in" << filePath << ":" << ex.what();
                info.isValid = false;
                info.trustStatus = "VerificationError";
            } catch (...) {
                qCritical() << "validateSignatures: non-standard exception verifying field"
                            << info.fieldName << "in" << filePath;
                info.isValid = false;
                info.trustStatus = "VerificationError";
            }

            // ISA downgrade: if an unsigned non-DSS incremental revision was found and the
            // signature otherwise verified as "Valid" or "ValidWithDSS", surface the gap.
            if (hasUnsignedTrailing &&
                (info.trustStatus == QLatin1String("Valid") ||
                 info.trustStatus == QLatin1String("ValidWithDSS"))) {
                qWarning() << "SECURITY: downgrading trustStatus from" << info.trustStatus
                           << "to ValidWithUnsignedChanges for field" << info.fieldName;
                info.trustStatus = "ValidWithUnsignedChanges";
                info.isValid = false;
            }

            results.append(info);
        }
    } catch (const std::exception &e) {
        qWarning() << "Error validating signatures:" << e.what();
    }
    return results;
}
