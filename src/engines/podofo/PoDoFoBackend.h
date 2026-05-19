#pragma once
#include "core/interfaces/IPdfDocument.h"
#include "core/interfaces/IPdfWriter.h"
#include "core/interfaces/IPdfEditorEngine.h" // For PdfMetadata and PdfImageInfo
#include <QStringList>
#include <QList>
#include <QRectF>
#include <QPointF>
#include <memory>

class PoDoFoBackend final : public IPdfDocument, public IPdfWriter {
public:
    PoDoFoBackend();
    ~PoDoFoBackend() override;

    // IPdfDocument interface
    bool loadDocument(const QString &path) override;
    bool saveDocument(const QString &path) override;
    int pageCount() const override;
    PdfMetadata metadata() const override;
    bool setMetadata(const PdfMetadata &metadata) override;

    // IPdfWriter interface
    bool writeDocument(const QString &path) override;
    bool writeUpdate(const QString &path) override;

    // Document information/structure
    QString currentFile() const;
    void setCurrentFile(const QString &path);
    QStringList getEmbeddedFiles();
    QStringList getLayers();

    // Page editing operations
    bool rotatePage(const QString &path, int pageIndex, int degrees);
    QByteArray extractPageAsBytes(const QString &path, int pageIndex);
    bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData);
    bool deletePage(const QString &path, int pageIndex);
    bool insertBlankPage(const QString &path, int atIndex);

    // Content editing
    bool editTextInline(int pageIndex, const QRectF &rect, const QString &newText);
    bool deleteObjectAt(int pageIndex, const QPointF &pos);
    bool applyRedactions(int pageIndex, const QList<QRectF> &rects);

    // Specialized conversion/security operations
    bool linearizeDocument(const QString &outputPath);
    bool exportPdfA(const QString &outputPath, int conformanceLevel);
    bool encryptDocument(const QString &userPassword, const QString &ownerPassword,
                         bool canPrint, bool canCopy, bool canModify);
    bool sanitizeDocument(const QString &outputPath);

    // Image operations
    QList<PdfImageInfo> listImages(int pageIndex);
    bool moveImage(int pageIndex, const QString &xobjectName, double dx, double dy);
    bool resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight);
    bool rotateImage(int pageIndex, const QString &xobjectName, double degrees);
    bool replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath);
    bool deleteImage(int pageIndex, const QString &xobjectName);

private:
    class Private;
    std::unique_ptr<Private> d;
};
