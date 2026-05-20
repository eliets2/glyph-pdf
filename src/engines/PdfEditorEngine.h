#pragma once

#include <QString>
#include <QList>
#include <memory>
#include "core/interfaces/IPdfEditorEngine.h"

class PdfEditorEngine final : public IPdfEditorEngine
{
public:
    PdfEditorEngine();
    ~PdfEditorEngine() override;

    // Core routing integration
    bool loadDocumentForEditing(const QString &filePath) override;
    bool saveDocument(const QString &outputPath) override;
    
    // Structural DOM manipulation
    bool editTextInline(int pageIndex, const QRectF &rect, const QString &newText,
                        const QString &fontFamily = "", int fontSize = 0,
                        const QColor &color = Qt::black, bool bold = false,
                        bool italic = false, int alignment = 0) override;
    bool deleteObjectAt(int pageIndex, const QPointF &pos) override;
    
    // QPDF/Structural tasks
    bool linearizeDocument(const QString &outputPath) override;
    bool exportPdfA(const QString &outputPath, int conformanceLevel) override;
    bool encryptDocument(const QString &userPassword, const QString &ownerPassword, bool canPrint, bool canCopy, bool canModify) override;
    bool encryptWithCertificate(const QString &inputPath, const QString &outputPath, const QStringList &certPaths) override;
    bool sanitizeDocument(const QString &outputPath) override;

    // Metadata
    bool getMetadata(PdfMetadata &outMetadata) override;
    bool setMetadata(const PdfMetadata &metadata) override;

    QString currentFile() const override;
    QStringList getEmbeddedFiles() override;
    QStringList getLayers() override;

    // Page editing operations
    bool rotatePage(const QString &path, int pageIndex, int degrees) override;
    QByteArray extractPageAsBytes(const QString &path, int pageIndex) override;
    bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData) override;
    bool deletePage(const QString &path, int pageIndex) override;
    bool insertBlankPage(const QString &path, int atIndex) override;

    // Page Geometry & Operations
    bool cropPage(const QString &path, int pageIndex, const QRectF &cropRect) override;
    bool resizePage(const QString &path, int pageIndex, const QSizeF &size) override;
    bool reorderPages(const QString &path, int fromIndex, int toIndex) override;

    // Content Injection
    bool addHeaderFooter(const QString &path, const HeaderFooterOptions &options) override;
    bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options) override;

    // Image operations
    QList<PdfImageInfo> listImages(int pageIndex) override;
    bool moveImage(int pageIndex, const QString &xobjectName, double dx, double dy) override;
    bool resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight) override;
    bool rotateImage(int pageIndex, const QString &xobjectName, double degrees) override;
    bool replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath) override;
    bool deleteImage(int pageIndex, const QString &xobjectName) override;
    bool applyRedactions(int pageIndex, const QList<QRectF> &rects) override;
    bool embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
