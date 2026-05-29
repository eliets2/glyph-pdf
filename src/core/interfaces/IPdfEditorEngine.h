#pragma once
#include "core/ImageTypes.h"
#include "core/AnnotationTypes.h"
#include "core/ErrorInfo.h"
#include <QString>
#include <QStringList>
#include <QRectF>
#include <QPointF>
#include <QColor>
#include <QRegularExpression>

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

// ── Watermark options ──────────────────────────────────────────────────────

struct TextWatermarkOptions {
    QString text = "CONFIDENTIAL";
    QString fontFamily = "Helvetica";
    int fontSize = 48;
    QColor color = QColor(128, 128, 128);
    double opacity = 0.3;          // 0.0–1.0
    double rotationDeg = -45.0;    // degrees counter-clockwise
    enum Position { Center, Diagonal, TopLeft, TopRight, BottomLeft, BottomRight };
    Position position = Diagonal;
    int pageFrom = -1;             // -1 = all pages
    int pageTo = -1;
    bool skipSigned = true;
};

struct ImageWatermarkOptions {
    QString imagePath;
    double opacity = 0.3;
    double scale = 0.5;           // fraction of page width
    enum Position { Center, TopLeft, TopRight, BottomLeft, BottomRight, Tile };
    Position position = Center;
    int pageFrom = -1;
    int pageTo = -1;
    bool skipSigned = true;
};

// ── Optimization options ───────────────────────────────────────────────────

struct OptimizeOptions {
    bool downsampleImages = true;
    int targetDpi = 150;
    int jpegQuality = 75;          // 0–100
    bool deduplicateImages = true;
    bool subsetFonts = true;
    bool removeUnusedObjects = true;
    bool stripMetadata = false;
};

struct OptimizeEstimate {
    qint64 originalBytes = 0;
    qint64 estimatedBytes = 0;
    int imageCount = 0;
    qint64 imageTotalBytes = 0;
    int fontCount = 0;
    int duplicateImages = 0;
    double reductionPercent = 0.0;
};

struct DocumentPermissions {
    bool print = true;
    bool printHighQuality = true;
    bool copy = true;
    bool modify = false;
    bool annotate = false;
    bool fillForms = false;
    bool accessibility = true;
    bool assemble = false;
};

// ── Interface ──────────────────────────────────────────────────────────────

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
                                  const DocumentPermissions& perms = DocumentPermissions()) = 0;
    virtual bool removeEncryption(const QString &ownerPassword) = 0;
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

    // Pattern-based redaction: find all regex matches across [startPage, endPage] (0-based,
    // inclusive) and excise them from the content stream.
    // Pass startPage = -1 / endPage = -1 to redact all pages.
    virtual bool applyPatternRedactions(const QRegularExpression& pattern,
                                        int startPage, int endPage) = 0;

    // Page Geometry & Operations
    virtual bool cropPage(const QString &path, int pageIndex, const QRectF &cropRect) = 0;
    virtual bool resizePage(const QString &path, int pageIndex, const QSizeF &size) = 0;
    virtual bool reorderPages(const QString &path, int fromIndex, int toIndex) = 0;

    // Content Injection
    virtual bool addHeaderFooter(const QString &path, const HeaderFooterOptions &options) = 0;
    virtual bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options) = 0;

    // Annotation Export
    virtual bool embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations) = 0;

    // Watermarking (Session 13)
    virtual bool addTextWatermark(const TextWatermarkOptions &options) = 0;
    virtual bool addImageWatermark(const ImageWatermarkOptions &options) = 0;

    // Optimization (Session 13)
    virtual OptimizeEstimate estimateOptimization(const OptimizeOptions &options) = 0;
    virtual bool optimizeDocument(const QString &outputPath, const OptimizeOptions &options) = 0;

    // Error reporting (Session 16)
    virtual ErrorInfo lastError() const = 0;
    virtual void clearError() = 0;

protected:
    IPdfEditorEngine() = default;
    IPdfEditorEngine(const IPdfEditorEngine&) = delete;
    IPdfEditorEngine& operator=(const IPdfEditorEngine&) = delete;
};
