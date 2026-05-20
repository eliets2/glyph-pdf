#pragma once
#include "core/ImageTypes.h"
#include "core/AnnotationTypes.h"
#include <QString>
#include <QStringList>
#include <QRectF>
#include <QPointF>

struct PdfMetadata {
    QString title;
    QString author;
    QString subject;
    QString keywords;
    QString creator;
    QString producer;
};

struct HeaderFooterOptions {
    QString textTemplate;
    QString fontFamily;
    int fontSize = 12;
    enum class Position { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight };
    Position position = Position::BottomCenter;
};

struct BatesNumberingOptions {
    QString prefix;
    QString suffix;
    int startNumber = 1;
    int digitCount = 6;
    QString fontFamily;
    int fontSize = 12;
    HeaderFooterOptions::Position position = HeaderFooterOptions::Position::BottomRight;
};

class IPdfEditorEngine {
public:
    virtual ~IPdfEditorEngine() = default;
    virtual bool loadDocumentForEditing(const QString &filePath) = 0;
    virtual bool saveDocument(const QString &outputPath) = 0;
    virtual bool editTextInline(int pageIndex, const QRectF &rect, const QString &newText,
                                const QString &fontFamily = "", int fontSize = 0,
                                const QColor &color = Qt::black, bool bold = false,
                                bool italic = false, int alignment = 0) = 0;
    virtual bool deleteObjectAt(int pageIndex, const QPointF &pos) = 0;
    virtual bool linearizeDocument(const QString &outputPath) = 0;
    virtual bool exportPdfA(const QString &outputPath, int conformanceLevel) = 0;
    virtual bool encryptDocument(const QString &userPassword, const QString &ownerPassword,
                                  bool canPrint, bool canCopy, bool canModify) = 0;

    /// Certificate-based encryption (PDF /PubSec, AES-256, RSA-wrapped per recipient).
    /// @param inputPath   Source PDF (may be the currently loaded document).
    /// @param outputPath  Destination encrypted PDF.
    /// @param certPaths   DER or PEM X.509 recipient certificates (one per user).
    /// @return true on success.
    virtual bool encryptWithCertificate(const QString &inputPath,
                                        const QString &outputPath,
                                        const QStringList &certPaths) = 0;
    virtual bool sanitizeDocument(const QString &outputPath) = 0;
    virtual bool getMetadata(PdfMetadata &outMetadata) = 0;
    virtual bool setMetadata(const PdfMetadata &metadata) = 0;
    virtual QString currentFile() const = 0;
    virtual QStringList getEmbeddedFiles() = 0;
    virtual QStringList getLayers() = 0;

    virtual bool rotatePage(const QString &path, int pageIndex, int degrees) = 0;
    virtual QByteArray extractPageAsBytes(const QString &path, int pageIndex) = 0;
    virtual bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData) = 0;
    virtual bool deletePage(const QString &path, int pageIndex) = 0;
    virtual bool insertBlankPage(const QString &path, int atIndex) = 0;

    virtual QList<PdfImageInfo> listImages(int pageIndex) = 0;
    virtual bool moveImage(int pageIndex, const QString &xobjectName, double dx, double dy) = 0;
    virtual bool resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight) = 0;
    virtual bool rotateImage(int pageIndex, const QString &xobjectName, double degrees) = 0;
    virtual bool replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath) = 0;
    virtual bool deleteImage(int pageIndex, const QString &xobjectName) = 0;
    virtual bool applyRedactions(int pageIndex, const QList<QRectF> &rects) = 0;
    
    // Page Geometry & Operations
    virtual bool cropPage(const QString &path, int pageIndex, const QRectF &cropRect) = 0;
    virtual bool resizePage(const QString &path, int pageIndex, const QSizeF &size) = 0;
    virtual bool reorderPages(const QString &path, int fromIndex, int toIndex) = 0;
    
    // Content Injection
    virtual bool addHeaderFooter(const QString &path, const HeaderFooterOptions &options) = 0;
    virtual bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options) = 0;
    
    // Annotation Export
    virtual bool embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations) = 0;
protected:
    IPdfEditorEngine() = default;
    IPdfEditorEngine(const IPdfEditorEngine&) = delete;
    IPdfEditorEngine& operator=(const IPdfEditorEngine&) = delete;
};
