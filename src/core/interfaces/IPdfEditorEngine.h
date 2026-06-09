// SPDX-License-Identifier: Apache-2.0
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

// Forward declarations to avoid circular includes
#include <QImage>
#include <QList>
struct PageOcrResult;  // defined in engines/ocr/OcrPipeline.h

// ── MRC compression mode ───────────────────────────────────────────────────
// Controls the JBIG2 foreground + JPEG2000 background encoding aggressiveness
// in the Mixed Raster Content PDF/A-2b export pipeline (M7-P3).
enum class MrcMode {
    Off,         ///< No MRC — use standard image embedding
    Lossless,    ///< JBIG2 lossless foreground + JPEG2000 at 10:1 background
    Balanced,    ///< JBIG2 lossless foreground + JPEG2000 at 30:1 (default)
    Aggressive   ///< JBIG2 lossless foreground + JPEG2000 at 50:1
};

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
    // Page range to stamp, 1-based inclusive. firstPage <= 0 means "from the
    // first page"; lastPage <= 0 means "through the last page". Defaults stamp
    // every page (backward-compatible with the prior all-pages behavior).
    int firstPage = 0;
    int lastPage = 0;
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

    /// Export current document as a PDF/A-2b MRC sandwich:
    ///   - JPEG2000 background XObject per page
    ///   - JBIG2 foreground mask XObject (1bpp) per page
    ///   - Invisible text layer (3 Tr) from OCR word boxes
    /// Runs veraPDF validation gate after assembly (QSKIP if CLI unavailable).
    /// \param pageImages   Pre-rendered page images (from PDFium renderer).
    /// \param pageResults  OCR results per page (from OcrPipeline); may be empty.
    /// \param mode         MRC compression mode (default Balanced = 30:1 background).
    /// \returns true if export + veraPDF validation passed.
    virtual bool exportMrcPdfA(const QString& outputPath,
                               const QList<QImage>& pageImages,
                               const QList<struct PageOcrResult>& pageResults,
                               MrcMode mode = MrcMode::Balanced) = 0;
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
    /// Extract the decoded bytes of the embedded file named `name` (as listed
    /// by getEmbeddedFiles). Returns an empty array if absent or on error.
    virtual QByteArray extractEmbeddedFile(const QString &name) = 0;
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

    // R2-1 D2: incremental-update save that preserves /ByteRange integrity.
    // Must be called instead of saveDocument() whenever the loaded document
    // has PDF signatures (see hasPdfSignatures()). On a document without
    // signatures the implementation may fall back to a full save.
    virtual bool writeUpdate(const QString &outputPath) = 0;

    // R2-1 D2: return true iff the currently-loaded document contains at least
    // one PDF signature field (/Sig widget).  Used by HomeController::onSave to
    // select the correct save path without coupling the controller to the backend.
    virtual bool hasPdfSignatures() const = 0;

    // ER-3: Return the number of CMS recipient envelopes in the /Encrypt
    // dictionary's /Recipients array.  Returns 0 if the document is not
    // encrypted or has no public-key recipients.  The caller checks > 1 to
    // detect the multi-recipient case before allowing in-place re-encryption.
    virtual int recipientCount() const = 0;

protected:
    IPdfEditorEngine() = default;
    IPdfEditorEngine(const IPdfEditorEngine&) = delete;
    IPdfEditorEngine& operator=(const IPdfEditorEngine&) = delete;
};
