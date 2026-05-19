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
