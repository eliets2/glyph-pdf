#include "engines/PdfEditorEngine.h"
#include "engines/BackendRouter.h"
#include "engines/PatternRedactor.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "engines/qpdf/QpdfBackend.h"
#include "engines/SignatureManager.h"
#include "core/ErrorInfo.h"
#include "core/TempFileManager.h"
#include <QMutexLocker>
#include <QRecursiveMutex>
#include <QSettings>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>

// Collect the full OpenSSL error stack into a human-readable string.
static QString opensslErrorString() {
    QString result;
    unsigned long err;
    while ((err = ERR_get_error()) != 0) {
        if (!result.isEmpty()) result += QLatin1String("; ");
        result += QString::fromLatin1(ERR_reason_error_string(err));
    }
    return result.isEmpty() ? QStringLiteral("Unknown OpenSSL error") : result;
}

class PdfEditorEngine::Private {
public:
    std::unique_ptr<PoDoFoBackend> backend;
    mutable QRecursiveMutex mutex;
    mutable ErrorInfo lastErr;

    void clearErr() const { lastErr = ErrorInfo{}; }

    void setErr(ErrorInfo::Severity sev, const QString& msg,
                const QString& tech = {}, ErrorInfo::SuggestedActions acts = ErrorInfo::None) const
    {
        lastErr.severity = sev;
        lastErr.userMessage = msg;
        lastErr.technicalDetails = tech;
        lastErr.suggestedActions = acts;
        lastErr.timestamp = QDateTime::currentDateTime();
    }

    bool noBackend(const char* method) const {
        setErr(ErrorInfo::Error,
               QObject::tr("No document is open for editing."),
               QStringLiteral("Method: %1 — backend is null.").arg(QLatin1String(method)));
        return false;
    }
};

PdfEditorEngine::PdfEditorEngine() : d(std::make_unique<Private>()) {}

PdfEditorEngine::~PdfEditorEngine() = default;

bool PdfEditorEngine::loadDocumentForEditing(const QString &filePath)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();

    // D5: Warn for PDFs > 500 MB
    QFileInfo fi(filePath);
    if (fi.exists() && fi.size() > 500LL * 1024 * 1024) {
        qWarning() << "PdfEditorEngine: large file" << fi.size() / (1024*1024) << "MB:" << filePath;
        d->setErr(ErrorInfo::Warning,
                  QObject::tr("This is a very large file (%1 MB). Loading and editing may be slow "
                              "and use significant memory. Consider splitting the document first.")
                      .arg(fi.size() / (1024 * 1024)),
                  QStringLiteral("File size: %1 bytes").arg(fi.size()));
        // Continue loading — this is just a warning
    }

    auto* docBackend = BackendRouter::documentBackendFor(filePath);
    if (!docBackend) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Could not find a suitable backend for this file."),
                  QStringLiteral("BackendRouter::documentBackendFor returned null for: %1").arg(filePath),
                  ErrorInfo::Retry);
        return false;
    }

    auto podofoBackend = std::unique_ptr<PoDoFoBackend>(dynamic_cast<PoDoFoBackend*>(docBackend));
    if (!podofoBackend) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("The editing backend is not available."),
                  QStringLiteral("dynamic_cast<PoDoFoBackend*> failed for: %1").arg(filePath));
        return false;
    }

    if (podofoBackend->loadDocument(filePath)) {
        d->backend = std::move(podofoBackend);
        return true;
    }

    // PoDoFo load failed — attempt structural repair via qpdf
    qWarning() << "PoDoFo failed to load" << filePath << "- attempting structural repair using qpdf...";

    QString tempPath = TempFileManager::instance().createTempFile(".pdf");

    if (!tempPath.isEmpty()) {
        if (QpdfBackend::repair(filePath, tempPath)) {
            qWarning() << "qpdf successfully repaired" << filePath << "- loading repaired copy...";

            if (podofoBackend->loadDocument(tempPath)) {
                podofoBackend->setCurrentFile(filePath);
                d->backend = std::move(podofoBackend);
                TempFileManager::instance().untrack(tempPath);
                QFile::remove(tempPath);

                d->setErr(ErrorInfo::Warning,
                          QObject::tr("This document was damaged and has been automatically repaired. "
                                      "Some content may have been lost. Save to a new file to preserve the repair."),
                          QStringLiteral("qpdf --replace-input succeeded; PoDoFo loaded repaired copy."));
                return true;
            }
        }
        TempFileManager::instance().untrack(tempPath);
        QFile::remove(tempPath);
    }

    d->setErr(ErrorInfo::Error,
              QObject::tr("Unable to open this PDF. The file may be corrupted, password-protected, "
                          "or not a valid PDF document."),
              QStringLiteral("PoDoFo load failed, qpdf repair also failed for: %1").arg(filePath),
              ErrorInfo::Retry);
    d->lastErr.sourceFile = filePath;
    return false;
}

bool PdfEditorEngine::saveDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("saveDocument");

    QSettings settings;
    bool linearizeRequested = settings.value("export/linearizeOnSave", false).toBool();

    if (linearizeRequested) {
        QString tempPath = TempFileManager::instance().createTempFile(".pdf");

        if (tempPath.isEmpty()) {
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Could not create a temporary file for saving."),
                      QStringLiteral("TempFileManager::createTempFile failed"),
                      ErrorInfo::Retry);
            return false;
        }

        if (!d->backend->saveDocument(tempPath)) {
            TempFileManager::instance().untrack(tempPath);
            QFile::remove(tempPath);
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Failed to save the document. The file may be read-only or the disk may be full."),
                      QStringLiteral("PoDoFoBackend::saveDocument failed for temp path"),
                      ErrorInfo::Retry);
            return false;
        }

        SignatureManager sigMgr;
        auto sigs = sigMgr.validateSignatures(tempPath);
        bool hasSignatures = !sigs.isEmpty();

        bool success = false;
        if (!hasSignatures) {
            success = QpdfBackend::linearize(tempPath, outputPath);
            if (!success) {
                d->setErr(ErrorInfo::Warning,
                          QObject::tr("The document was saved but linearization failed. "
                                      "The PDF will work but may load more slowly in web viewers."),
                          QStringLiteral("QpdfBackend::linearize failed; fallback to direct copy."));
                // Fallback: copy unlinearized
                if (QFile::exists(outputPath)) QFile::remove(outputPath);
                success = QFile::copy(tempPath, outputPath);
            }
        } else {
            if (QFile::exists(outputPath)) QFile::remove(outputPath);
            success = QFile::copy(tempPath, outputPath);
        }

        TempFileManager::instance().untrack(tempPath);
        QFile::remove(tempPath);
        if (!success) {
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Failed to write the output file. Check that the destination is writable."),
                      QStringLiteral("Output path: %1").arg(outputPath),
                      ErrorInfo::Retry);
        }
        return success;
    }

    bool ok = d->backend->saveDocument(outputPath);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to save the document. The file may be read-only or the disk may be full."),
                  QStringLiteral("PoDoFoBackend::saveDocument failed for: %1").arg(outputPath),
                  ErrorInfo::Retry);
    }
    return ok;
}

bool PdfEditorEngine::editTextInline(int pageIndex, const QRectF &rect, const QString &newText,
                                     const QString &fontFamily, int fontSize,
                                     const QColor &color, bool bold,
                                     bool italic, int alignment)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("editTextInline");
    bool ok = d->backend->editTextInline(pageIndex, rect, newText, fontFamily, fontSize, color, bold, italic, alignment);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to edit text on page %1. The text region may be part of an image or scanned content.").arg(pageIndex + 1),
                  QStringLiteral("editTextInline failed — page %1").arg(pageIndex));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::deleteObjectAt(int pageIndex, const QPointF &pos)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("deleteObjectAt");
    bool ok = d->backend->deleteObjectAt(pageIndex, pos);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Could not delete the object at the specified position on page %1.").arg(pageIndex + 1),
                  QStringLiteral("deleteObjectAt page %1, pos (%2, %3)").arg(pageIndex).arg(pos.x()).arg(pos.y()));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::linearizeDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("linearizeDocument");
    bool ok = d->backend->linearizeDocument(outputPath);
    if (!ok)
        d->setErr(ErrorInfo::Warning,
                  QObject::tr("Linearization (web optimization) failed. The document was saved without it."),
                  QStringLiteral("linearizeDocument failed for: %1").arg(outputPath));
    return ok;
}

bool PdfEditorEngine::exportPdfA(const QString &outputPath, int conformanceLevel)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("exportPdfA");
    bool ok = d->backend->exportPdfA(outputPath, conformanceLevel);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("PDF/A export failed. The document may contain features incompatible with the selected conformance level."),
                  QStringLiteral("exportPdfA level=%1, path=%2").arg(conformanceLevel).arg(outputPath),
                  ErrorInfo::Retry);
    return ok;
}

bool PdfEditorEngine::encryptDocument(const QString &userPassword, const QString &ownerPassword, const DocumentPermissions& perms) {
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("encryptDocument");
    bool ok = d->backend->encryptDocument(userPassword, ownerPassword, perms);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Encryption failed. The document may already be encrypted or contain signatures that prevent modification."),
                  QStringLiteral("encryptDocument failed"),
                  ErrorInfo::Retry);
    return ok;
}

bool PdfEditorEngine::removeEncryption(const QString &ownerPassword) {
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("removeEncryption");
    bool ok = d->backend->removeEncryption(ownerPassword);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to remove encryption (incorrect password?)"),
                  QStringLiteral("removeEncryption failed"),
                  ErrorInfo::Retry);
    return ok;
}

// OpenSSL RAII Deleters
struct BIO_Deleter { void operator()(BIO* b) const { BIO_free(b); } };
struct CMS_Deleter { void operator()(CMS_ContentInfo* c) const { CMS_ContentInfo_free(c); } };
struct X509_Deleter { void operator()(X509* c) const { X509_free(c); } };
struct STACK_X509_Deleter { void operator()(STACK_OF(X509)* s) const { sk_X509_pop_free(s, X509_free); } };

using BioPtr = std::unique_ptr<BIO, BIO_Deleter>;
using CmsPtr = std::unique_ptr<CMS_ContentInfo, CMS_Deleter>;
using X509Ptr = std::unique_ptr<X509, X509_Deleter>;
using SkX509Ptr = std::unique_ptr<STACK_OF(X509), STACK_X509_Deleter>;

// PoDoFo Hack to support PubSec
#define private public
#define protected public
#include <podofo/podofo.h>
#undef private
#undef protected
#include <podofo/main/PdfMemDocument.h>
#include <podofo/main/PdfEncryptSession.h>
#include <openssl/rand.h>

class PdfEncryptPubSec : public PoDoFo::PdfEncrypt {
    std::unique_ptr<PoDoFo::PdfEncrypt> m_delegate;
    std::vector<unsigned char> m_fek;
    std::vector<QByteArray> m_recipientsCMS;

public:
    PdfEncryptPubSec(const unsigned char* fek, const std::vector<QByteArray>& recipients) 
        : PoDoFo::PdfEncrypt()
    {
        m_delegate = PoDoFo::PdfEncrypt::Create("dummy", "dummy", PoDoFo::PdfPermissions::Default, PoDoFo::PdfEncryptionAlgorithm::AESV3R6);
        
        m_fek.assign(fek, fek + 32);
        m_recipientsCMS = recipients;

        m_Algorithm = PoDoFo::PdfEncryptionAlgorithm::AESV3R6;
        m_KeyLength = PoDoFo::PdfKeyLength::L256;
        m_pValue = PoDoFo::PdfPermissions::Default;
        m_rValue = 6;
        m_EncryptMetadata = true;
        m_initialized = false;
    }

    void GenerateEncryptionKey(const std::string_view&, PoDoFo::PdfAuthResult, PODOFO_CRYPT_CTX*,
        unsigned char[48], unsigned char[48], unsigned char encryptionKey[32]) override 
    {
        memcpy(encryptionKey, m_fek.data(), 32);
    }

    void CreateEncryptionDictionary(PoDoFo::PdfDictionary& dict) const override {
        dict.AddKey(PoDoFo::PdfName("Filter"), PoDoFo::PdfName("PubSec"));
        dict.AddKey(PoDoFo::PdfName("SubFilter"), PoDoFo::PdfName("adbe.pkcs7.s5"));
        dict.AddKey(PoDoFo::PdfName("V"), static_cast<int64_t>(5));
        dict.AddKey(PoDoFo::PdfName("Length"), static_cast<int64_t>(256));
        
        PoDoFo::PdfArray recips;
        for (const auto& r : m_recipientsCMS) {
            recips.Add(PoDoFo::PdfString::FromRaw(std::string_view(r.constData(), r.size()), true));
        }
        dict.AddKey(PoDoFo::PdfName("Recipients"), recips);
    }

    void Encrypt(const char* inStr, size_t inLen, PoDoFo::PdfEncryptContext& context,
        const PoDoFo::PdfReference& objref, char* outStr, size_t outLen) const override {
        m_delegate->Encrypt(inStr, inLen, context, objref, outStr, outLen);
    }
    
    void Decrypt(const char* inStr, size_t inLen, PoDoFo::PdfEncryptContext& context,
        const PoDoFo::PdfReference& objref, char* outStr, size_t& outLen) const override {
        m_delegate->Decrypt(inStr, inLen, context, objref, outStr, outLen);
    }

    std::unique_ptr<PoDoFo::InputStream> CreateEncryptionInputStream(PoDoFo::InputStream& inputStream, size_t inputLen,
        PoDoFo::PdfEncryptContext& context, const PoDoFo::PdfReference& objref) const override {
        return m_delegate->CreateEncryptionInputStream(inputStream, inputLen, context, objref);
    }

    std::unique_ptr<PoDoFo::OutputStream> CreateEncryptionOutputStream(PoDoFo::OutputStream& outputStream,
        PoDoFo::PdfEncryptContext& context, const PoDoFo::PdfReference& objref) const override {
        return m_delegate->CreateEncryptionOutputStream(outputStream, context, objref);
    }

    size_t CalculateStreamLength(size_t length) const override {
        return m_delegate->CalculateStreamLength(length);
    }

    size_t CalculateStreamOffset() const override {
        return m_delegate->CalculateStreamOffset();
    }
    
    PoDoFo::PdfAuthResult Authenticate(const std::string_view&, const std::string_view&,
        PODOFO_CRYPT_CTX*, unsigned char[32]) const override {
        return PoDoFo::PdfAuthResult::Owner; 
    }
};

bool PdfEditorEngine::encryptWithCertificate(const QString &inputPath,
                                              const QString &outputPath,
                                              const QStringList &certPaths)
{
    d->clearErr();

    if (certPaths.isEmpty()) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("No recipient certificates were provided for encryption."),
                  QStringLiteral("certPaths is empty"));
        return false;
    }

    SkX509Ptr recipients(sk_X509_new_null());
    if (!recipients) {
        d->setErr(ErrorInfo::Critical,
                  QObject::tr("Failed to initialize the encryption subsystem."),
                  QStringLiteral("sk_X509_new_null returned null - OpenSSL: %1").arg(opensslErrorString()),
                  ErrorInfo::ExportLog);
        return false;
    }

    for (const QString &cp : certPaths) {
        QFile f(cp);
        if (!f.open(QIODevice::ReadOnly)) {
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Cannot read certificate file: %1").arg(QFileInfo(cp).fileName()),
                      QStringLiteral("QFile::open failed for: %1").arg(cp),
                      ErrorInfo::Retry);
            return false;
        }
        QByteArray data = f.readAll();
        const unsigned char *p = reinterpret_cast<const unsigned char*>(data.constData());
        X509 *cert = d2i_X509(nullptr, &p, data.size());
        if (!cert) {
            BioPtr bio(BIO_new_mem_buf(data.data(), data.size()));
            cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
        }
        if (!cert) {
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Invalid certificate: %1. The file must be a valid X.509 certificate in PEM or DER format.").arg(QFileInfo(cp).fileName()),
                      QStringLiteral("d2i_X509 and PEM_read_bio_X509 both failed for: %1 - OpenSSL: %2").arg(cp, opensslErrorString()),
                      ErrorInfo::Retry);
            return false;
        }
        sk_X509_push(recipients.get(), cert);
    }

    unsigned char sessionKey[24];
    if (RAND_bytes(sessionKey, 20) != 1) {
        d->setErr(ErrorInfo::Critical,
                  QObject::tr("Failed to generate secure random seed."),
                  QStringLiteral("RAND_bytes failed - OpenSSL: %1").arg(opensslErrorString()),
                  ErrorInfo::ExportLog);
        return false;
    }

    uint32_t perms = static_cast<uint32_t>(PoDoFo::PdfPermissions::Default);
    sessionKey[20] = (unsigned char)(perms & 0xFF);
    sessionKey[21] = (unsigned char)((perms >> 8) & 0xFF);
    sessionKey[22] = (unsigned char)((perms >> 16) & 0xFF);
    sessionKey[23] = (unsigned char)((perms >> 24) & 0xFF);

    unsigned char fek[32];
    SHA256(sessionKey, 24, fek);

    std::vector<QByteArray> envelopes;
    for (int i = 0; i < sk_X509_num(recipients.get()); ++i) {
        X509* cert = sk_X509_value(recipients.get(), i);
        STACK_OF(X509)* singleRecip = sk_X509_new_null();
        sk_X509_push(singleRecip, cert);
        
        BioPtr inBio(BIO_new_mem_buf(sessionKey, 24));
        CmsPtr cms(CMS_encrypt(singleRecip, inBio.get(), EVP_aes_256_cbc(), CMS_BINARY));
        
        if (!cms) {
            sk_X509_free(singleRecip);
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Certificate-based encryption failed. The certificate may be expired, revoked, or incompatible."),
                      QStringLiteral("CMS_encrypt failed - OpenSSL: %1").arg(opensslErrorString()),
                      ErrorInfo::Retry | ErrorInfo::ExportLog);
            return false;
        }
        
        BioPtr outBio(BIO_new(BIO_s_mem()));
        if (!i2d_CMS_bio(outBio.get(), cms.get())) {
            sk_X509_free(singleRecip);
            d->setErr(ErrorInfo::Error,
                      QObject::tr("Failed to encode the encrypted output."),
                      QStringLiteral("i2d_CMS_bio failed - OpenSSL: %1").arg(opensslErrorString()),
                      ErrorInfo::ExportLog);
            return false;
        }
        
        char *buf = nullptr;
        long len = BIO_get_mem_data(outBio.get(), &buf);
        envelopes.push_back(QByteArray(buf, static_cast<int>(len)));
        
        sk_X509_free(singleRecip);
    }

    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inputPath.toUtf8().constData(), "");
        doc.SetEncrypt(std::make_unique<PdfEncryptPubSec>(fek, envelopes));
        doc.Save(outputPath.toUtf8().constData(), PoDoFo::PdfSaveOptions::None);
    } catch (const PoDoFo::PdfError &e) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to apply public-key encryption to the PDF."),
                  QStringLiteral("PoDoFo exception: %1").arg(e.what()),
                  ErrorInfo::ExportLog);
        return false;
    }
    return true;
}

bool PdfEditorEngine::sanitizeDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("sanitizeDocument");
    bool ok = d->backend->sanitizeDocument(outputPath);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Document sanitization failed."),
                  QStringLiteral("sanitizeDocument path=%1").arg(outputPath), ErrorInfo::Retry);
    return ok;
}

bool PdfEditorEngine::getMetadata(PdfMetadata &outMetadata)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("getMetadata");
    outMetadata = d->backend->metadata();
    return true;
}

bool PdfEditorEngine::setMetadata(const PdfMetadata &metadata)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("setMetadata");
    bool ok = d->backend->setMetadata(metadata);
    if (!ok)
        d->setErr(ErrorInfo::Warning,
                  QObject::tr("Could not update the document metadata."),
                  QStringLiteral("setMetadata failed"));
    return ok;
}

QString PdfEditorEngine::currentFile() const
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return QString();
    return d->backend->currentFile();
}

QStringList PdfEditorEngine::getEmbeddedFiles()
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("getEmbeddedFiles"); return {}; }
    return d->backend->getEmbeddedFiles();
}

QByteArray PdfEditorEngine::extractEmbeddedFile(const QString &name)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("extractEmbeddedFile"); return {}; }
    return d->backend->extractEmbeddedFile(name);
}

QStringList PdfEditorEngine::getLayers()
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("getLayers"); return {}; }
    return d->backend->getLayers();
}

bool PdfEditorEngine::rotatePage(const QString &path, int pageIndex, int degrees)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("rotatePage");
    bool ok = d->backend->rotatePage(path, pageIndex, degrees);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to rotate page %1.").arg(pageIndex + 1),
                  QStringLiteral("rotatePage page=%1, degrees=%2").arg(pageIndex).arg(degrees));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

QByteArray PdfEditorEngine::extractPageAsBytes(const QString &path, int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("extractPageAsBytes"); return {}; }
    QByteArray result = d->backend->extractPageAsBytes(path, pageIndex);
    if (result.isEmpty()) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to extract page %1.").arg(pageIndex + 1),
                  QStringLiteral("extractPageAsBytes returned empty — page=%1").arg(pageIndex));
        d->lastErr.sourcePage = pageIndex;
    }
    return result;
}

bool PdfEditorEngine::insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("insertPageFromBytes");
    bool ok = d->backend->insertPageFromBytes(path, atIndex, pageData);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to insert page at position %1.").arg(atIndex + 1),
                  QStringLiteral("insertPageFromBytes at=%1").arg(atIndex));
    return ok;
}

bool PdfEditorEngine::deletePage(const QString &path, int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("deletePage");
    bool ok = d->backend->deletePage(path, pageIndex);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to delete page %1.").arg(pageIndex + 1),
                  QStringLiteral("deletePage page=%1").arg(pageIndex));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::insertBlankPage(const QString &path, int atIndex)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("insertBlankPage");
    bool ok = d->backend->insertBlankPage(path, atIndex);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to insert a blank page at position %1.").arg(atIndex + 1),
                  QStringLiteral("insertBlankPage at=%1").arg(atIndex));
    return ok;
}

bool PdfEditorEngine::cropPage(const QString &path, int pageIndex, const QRectF &cropRect)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("cropPage");
    bool ok = d->backend->cropPage(path, pageIndex, cropRect);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to crop page %1.").arg(pageIndex + 1),
                  QStringLiteral("cropPage page=%1").arg(pageIndex));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::resizePage(const QString &path, int pageIndex, const QSizeF &size)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("resizePage");
    bool ok = d->backend->resizePage(path, pageIndex, size);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to resize page %1.").arg(pageIndex + 1),
                  QStringLiteral("resizePage page=%1, w=%2, h=%3").arg(pageIndex).arg(size.width()).arg(size.height()));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::reorderPages(const QString &path, int fromIndex, int toIndex)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("reorderPages");
    bool ok = d->backend->reorderPages(path, fromIndex, toIndex);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to reorder pages."),
                  QStringLiteral("reorderPages from=%1, to=%2").arg(fromIndex).arg(toIndex));
    return ok;
}

bool PdfEditorEngine::addHeaderFooter(const QString &path, const HeaderFooterOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("addHeaderFooter");
    bool ok = d->backend->addHeaderFooter(path, options);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to add headers/footers."),
                  QStringLiteral("addHeaderFooter failed"));
    return ok;
}

bool PdfEditorEngine::applyBatesNumbering(const QString &path, const BatesNumberingOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("applyBatesNumbering");
    bool ok = d->backend->applyBatesNumbering(path, options);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to apply Bates numbering."),
                  QStringLiteral("applyBatesNumbering failed"));
    return ok;
}


QList<PdfImageInfo> PdfEditorEngine::listImages(int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("listImages"); return {}; }
    return d->backend->listImages(pageIndex);
}

bool PdfEditorEngine::moveImage(int pageIndex, const QString &xobjectName, double dx, double dy)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("moveImage");
    bool ok = d->backend->moveImage(pageIndex, xobjectName, dx, dy);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to move the image on page %1.").arg(pageIndex + 1),
                  QStringLiteral("moveImage page=%1, obj=%2").arg(pageIndex).arg(xobjectName));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("resizeImage");
    bool ok = d->backend->resizeImage(pageIndex, xobjectName, newWidth, newHeight);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to resize the image on page %1.").arg(pageIndex + 1),
                  QStringLiteral("resizeImage page=%1, obj=%2").arg(pageIndex).arg(xobjectName));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::rotateImage(int pageIndex, const QString &xobjectName, double degrees)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("rotateImage");
    bool ok = d->backend->rotateImage(pageIndex, xobjectName, degrees);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to rotate the image on page %1.").arg(pageIndex + 1),
                  QStringLiteral("rotateImage page=%1, obj=%2").arg(pageIndex).arg(xobjectName));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("replaceImage");
    bool ok = d->backend->replaceImage(pageIndex, xobjectName, newImagePath);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to replace the image on page %1.").arg(pageIndex + 1),
                  QStringLiteral("replaceImage page=%1, obj=%2, newPath=%3").arg(pageIndex).arg(xobjectName, newImagePath));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::deleteImage(int pageIndex, const QString &xobjectName)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("deleteImage");
    bool ok = d->backend->deleteImage(pageIndex, xobjectName);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to delete the image on page %1.").arg(pageIndex + 1),
                  QStringLiteral("deleteImage page=%1, obj=%2").arg(pageIndex).arg(xobjectName));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::applyRedactions(int pageIndex, const QList<QRectF> &rects)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("applyRedactions");
    bool ok = d->backend->applyRedactions(pageIndex, rects);
    if (!ok) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to apply redactions on page %1.").arg(pageIndex + 1),
                  QStringLiteral("applyRedactions page=%1, rects=%2").arg(pageIndex).arg(rects.size()));
        d->lastErr.sourcePage = pageIndex;
    }
    return ok;
}

bool PdfEditorEngine::applyPatternRedactions(const QRegularExpression& pattern,
                                              int startPage, int endPage)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("applyPatternRedactions");

    if (!pattern.isValid()) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("The pattern regular expression is invalid."),
                  pattern.errorString());
        return false;
    }

    const QString pdfPath = d->backend->currentFile();
    if (pdfPath.isEmpty()) {
        d->setErr(ErrorInfo::Error,
                  QObject::tr("No document is loaded."),
                  QStringLiteral("applyPatternRedactions: currentFile() is empty"));
        return false;
    }

    // Determine page range
    const int totalPages = static_cast<int>(d->backend->pageCount());
    int firstPage = (startPage < 0) ? 0 : startPage;
    int lastPage  = (endPage   < 0) ? totalPages - 1 : endPage;
    firstPage = qBound(0, firstPage, totalPages - 1);
    lastPage  = qBound(0, lastPage,  totalPages - 1);

    bool anySuccess = false;
    bool anyFailure = false;

    for (int pg = firstPage; pg <= lastPage; ++pg) {
        const QList<QRectF> matches = PatternRedactor::findMatches(pdfPath, pg, pattern);
        if (matches.isEmpty()) continue;

        // Delegate to the existing applyRedactions path which carries the
        // Edact-Ray glyph-advance defense (wired in M2-P1).
        const bool ok = d->backend->applyRedactions(pg, matches);
        if (ok) {
            anySuccess = true;
        } else {
            anyFailure = true;
            qWarning() << "PatternRedactor: applyRedactions failed on page" << pg;
        }
    }

    if (anyFailure) {
        d->setErr(ErrorInfo::Warning,
                  QObject::tr("Pattern redaction partially failed on one or more pages."),
                  QStringLiteral("applyPatternRedactions: pattern=%1 pages=%2-%3")
                      .arg(pattern.pattern()).arg(firstPage).arg(lastPage));
        return false;
    }

    // If no pages had matches at all, that is still success (no-op)
    Q_UNUSED(anySuccess);
    return true;
}

bool PdfEditorEngine::embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("embedAnnotations");
    bool ok = d->backend->embedAnnotations(inputPath, outputPath, annotations);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to embed annotations into the document."),
                  QStringLiteral("embedAnnotations in=%1, out=%2, count=%3").arg(inputPath, outputPath).arg(annotations.size()),
                  ErrorInfo::Retry);
    return ok;
}

// ── Watermarking (Session 13) ─────────────────────────────────────────────

bool PdfEditorEngine::addTextWatermark(const TextWatermarkOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("addTextWatermark");
    bool ok = d->backend->addTextWatermark(options);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to apply the text watermark. The document may be read-only or corrupted."),
                  QStringLiteral("addTextWatermark text=\"%1\"").arg(options.text),
                  ErrorInfo::Retry);
    return ok;
}

bool PdfEditorEngine::addImageWatermark(const ImageWatermarkOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("addImageWatermark");
    bool ok = d->backend->addImageWatermark(options);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Failed to apply the image watermark. Check that the image file exists and is a supported format."),
                  QStringLiteral("addImageWatermark path=\"%1\"").arg(options.imagePath),
                  ErrorInfo::Retry);
    return ok;
}

// ── Optimization (Session 13) ─────────────────────────────────────────────

OptimizeEstimate PdfEditorEngine::estimateOptimization(const OptimizeOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) { d->noBackend("estimateOptimization"); return {}; }
    return d->backend->estimateOptimization(options);
}

bool PdfEditorEngine::optimizeDocument(const QString &outputPath, const OptimizeOptions &options)
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
    if (!d->backend) return d->noBackend("optimizeDocument");
    bool ok = d->backend->optimizeDocument(outputPath, options);
    if (!ok)
        d->setErr(ErrorInfo::Error,
                  QObject::tr("Document optimization failed."),
                  QStringLiteral("optimizeDocument path=%1").arg(outputPath),
                  ErrorInfo::Retry);
    return ok;
}

// ── Error reporting (Session 16) ──────────────────────────────────────────

ErrorInfo PdfEditorEngine::lastError() const
{
    QMutexLocker locker(&d->mutex);
    return d->lastErr;
}

void PdfEditorEngine::clearError()
{
    QMutexLocker locker(&d->mutex);
    d->clearErr();
}
