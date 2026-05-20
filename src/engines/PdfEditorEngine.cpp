#include "engines/PdfEditorEngine.h"
#include "engines/BackendRouter.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "engines/qpdf/QpdfBackend.h"
#include "engines/SignatureManager.h"
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

class PdfEditorEngine::Private {
public:
    std::unique_ptr<PoDoFoBackend> backend;
    mutable QRecursiveMutex mutex;
};

PdfEditorEngine::PdfEditorEngine() : d(std::make_unique<Private>()) {}

PdfEditorEngine::~PdfEditorEngine() = default;

bool PdfEditorEngine::loadDocumentForEditing(const QString &filePath)
{
    QMutexLocker locker(&d->mutex);
    auto* docBackend = BackendRouter::documentBackendFor(filePath);
    if (!docBackend) return false;

    auto podofoBackend = std::unique_ptr<PoDoFoBackend>(dynamic_cast<PoDoFoBackend*>(docBackend));
    if (!podofoBackend) return false;

    if (podofoBackend->loadDocument(filePath)) {
        d->backend = std::move(podofoBackend);
        return true;
    }

    // PoDoFo load failed, attempt repair-on-open using QpdfBackend
    qWarning() << "PoDoFo failed to load" << filePath << "- attempting structural repair using qpdf...";
    
    QString tempPath;
    {
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(false);
        if (tempFile.open()) {
            tempPath = tempFile.fileName();
        }
    }

    if (!tempPath.isEmpty()) {
        if (QpdfBackend::repair(filePath, tempPath)) {
            qWarning() << "qpdf successfully repaired" << filePath << "- loading repaired copy...";
            
            // Retry loading repaired copy in PoDoFo
            if (podofoBackend->loadDocument(tempPath)) {
                podofoBackend->setCurrentFile(filePath); // Restore active session path to original
                d->backend = std::move(podofoBackend);
                QFile::remove(tempPath);
                
                // Warn the user through standard log levels
                qWarning() << "WARNING: The document was damaged and has been repaired on load.";
                return true;
            }
        }
        QFile::remove(tempPath);
    }

    return false;
}

bool PdfEditorEngine::saveDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;

    QSettings settings;
    bool linearizeRequested = settings.value("export/linearizeOnSave", false).toBool();

    if (linearizeRequested) {
        QString tempPath;
        {
            QTemporaryFile tempFile;
            tempFile.setAutoRemove(false);
            if (tempFile.open()) {
                tempPath = tempFile.fileName();
            }
        }

        if (tempPath.isEmpty()) return false;

        // Save PoDoFo's work to the temp file first
        if (!d->backend->saveDocument(tempPath)) {
            QFile::remove(tempPath);
            return false;
        }

        // Validate signatures on the saved temp document
        SignatureManager sigMgr;
        auto sigs = sigMgr.validateSignatures(tempPath);
        bool hasSignatures = !sigs.isEmpty();

        bool success = false;
        if (!hasSignatures) {
            // Document has no digital signatures, safe to post-process/linearize
            success = QpdfBackend::linearize(tempPath, outputPath);
        } else {
            // Digital signatures present: skip QPDF post-processing, use direct copy
            if (QFile::exists(outputPath)) {
                QFile::remove(outputPath);
            }
            success = QFile::copy(tempPath, outputPath);
        }

        QFile::remove(tempPath);
        return success;
    }

    return d->backend->saveDocument(outputPath);
}

bool PdfEditorEngine::editTextInline(int pageIndex, const QRectF &rect, const QString &newText)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->editTextInline(pageIndex, rect, newText);
}

bool PdfEditorEngine::deleteObjectAt(int pageIndex, const QPointF &pos)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->deleteObjectAt(pageIndex, pos);
}

bool PdfEditorEngine::linearizeDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->linearizeDocument(outputPath);
}

bool PdfEditorEngine::exportPdfA(const QString &outputPath, int conformanceLevel)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->exportPdfA(outputPath, conformanceLevel);
}

bool PdfEditorEngine::encryptDocument(const QString &userPassword, const QString &ownerPassword, bool canPrint, bool canCopy, bool canModify)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->encryptDocument(userPassword, ownerPassword, canPrint, canCopy, canModify);
}

bool PdfEditorEngine::encryptWithCertificate(const QString &inputPath,
                                              const QString &outputPath,
                                              const QStringList &certPaths)
{
    if (certPaths.isEmpty()) {
        qWarning() << "encryptWithCertificate: no recipient certificates supplied";
        return false;
    }

    // Load each recipient certificate
    STACK_OF(X509) *recipients = sk_X509_new_null();
    if (!recipients) return false;

    for (const QString &cp : certPaths) {
        QFile f(cp);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "encryptWithCertificate: cannot open cert" << cp;
            sk_X509_pop_free(recipients, X509_free);
            return false;
        }
        QByteArray data = f.readAll();
        const unsigned char *p = reinterpret_cast<const unsigned char*>(data.constData());
        X509 *cert = nullptr;
        // Try DER first, then PEM
        cert = d2i_X509(nullptr, &p, data.size());
        if (!cert) {
            BIO *bio = BIO_new_mem_buf(data.data(), data.size());
            cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
        }
        if (!cert) {
            qWarning() << "encryptWithCertificate: failed to parse cert" << cp;
            sk_X509_pop_free(recipients, X509_free);
            return false;
        }
        sk_X509_push(recipients, cert);
    }

    // Read input PDF
    QFile inFile(inputPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        sk_X509_pop_free(recipients, X509_free);
        return false;
    }
    QByteArray pdfData = inFile.readAll();
    inFile.close();

    // CMS_encrypt wraps plaintext in an EnvelopedData structure
    // AES-256-CBC per-recipient RSA-wrapped session key
    BIO *inBio = BIO_new_mem_buf(pdfData.data(), pdfData.size());
    CMS_ContentInfo *cms = CMS_encrypt(recipients, inBio,
                                       EVP_aes_256_cbc(),
                                       CMS_BINARY | CMS_STREAM);
    BIO_free(inBio);
    sk_X509_pop_free(recipients, X509_free);

    if (!cms) {
        qWarning() << "encryptWithCertificate: CMS_encrypt failed:"
                   << ERR_reason_error_string(ERR_get_error());
        return false;
    }

    // Write DER-encoded CMS envelope
    BIO *outBio = BIO_new(BIO_s_mem());
    if (!i2d_CMS_bio_stream(outBio, cms, inBio, CMS_BINARY)) {
        qWarning() << "encryptWithCertificate: i2d_CMS_bio_stream failed";
        CMS_ContentInfo_free(cms);
        BIO_free(outBio);
        return false;
    }
    CMS_ContentInfo_free(cms);

    char *buf = nullptr;
    long len = BIO_get_mem_data(outBio, &buf);
    QFile outFile(outputPath);
    bool ok = false;
    if (outFile.open(QIODevice::WriteOnly)) {
        ok = (outFile.write(buf, len) == len);
        outFile.close();
    }
    BIO_free(outBio);

    if (!ok) qWarning() << "encryptWithCertificate: failed to write output" << outputPath;
    return ok;
}

bool PdfEditorEngine::sanitizeDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->sanitizeDocument(outputPath);
}

bool PdfEditorEngine::getMetadata(PdfMetadata &outMetadata)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    outMetadata = d->backend->metadata();
    return true;
}

bool PdfEditorEngine::setMetadata(const PdfMetadata &metadata)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->setMetadata(metadata);
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
    if (!d->backend) return QStringList();
    return d->backend->getEmbeddedFiles();
}

QStringList PdfEditorEngine::getLayers()
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return QStringList();
    return d->backend->getLayers();
}

bool PdfEditorEngine::rotatePage(const QString &path, int pageIndex, int degrees)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->rotatePage(path, pageIndex, degrees);
}

QByteArray PdfEditorEngine::extractPageAsBytes(const QString &path, int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return QByteArray();
    return d->backend->extractPageAsBytes(path, pageIndex);
}

bool PdfEditorEngine::insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->insertPageFromBytes(path, atIndex, pageData);
}

bool PdfEditorEngine::deletePage(const QString &path, int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->deletePage(path, pageIndex);
}

bool PdfEditorEngine::insertBlankPage(const QString &path, int atIndex)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->insertBlankPage(path, atIndex);
}

QList<PdfImageInfo> PdfEditorEngine::listImages(int pageIndex)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return QList<PdfImageInfo>();
    return d->backend->listImages(pageIndex);
}

bool PdfEditorEngine::moveImage(int pageIndex, const QString &xobjectName, double dx, double dy)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->moveImage(pageIndex, xobjectName, dx, dy);
}

bool PdfEditorEngine::resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->resizeImage(pageIndex, xobjectName, newWidth, newHeight);
}

bool PdfEditorEngine::rotateImage(int pageIndex, const QString &xobjectName, double degrees)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->rotateImage(pageIndex, xobjectName, degrees);
}

bool PdfEditorEngine::replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->replaceImage(pageIndex, xobjectName, newImagePath);
}

bool PdfEditorEngine::deleteImage(int pageIndex, const QString &xobjectName)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->deleteImage(pageIndex, xobjectName);
}

bool PdfEditorEngine::applyRedactions(int pageIndex, const QList<QRectF> &rects)
{
    QMutexLocker locker(&d->mutex);
    if (!d->backend) return false;
    return d->backend->applyRedactions(pageIndex, rects);
}
