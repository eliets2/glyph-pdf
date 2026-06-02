#pragma once
#include "core/interfaces/IPdfEditorEngine.h"
#include <QMap>
#include <QImage>
#include <QFile>

// Forward declaration — IPdfEditorEngine.h already forward-declares PageOcrResult
// using `struct PageOcrResult;` so we don't need the full OcrPipeline.h here.

class MockPdfEditorEngine : public IPdfEditorEngine {
public:
    bool loadDocumentForEditing(const QString &) override { m_loaded = true; return true; }
    bool saveDocument(const QString &path) override {
        ++m_saveCalls;
        m_lastSavedPath = path;
        if (m_loaded) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write("mock");
                f.close();
            }
            return true;
        }
        return false;
    }
    bool editTextInline(int, const QRectF &, const QString &,
                        const QString & = {}, int = 0, const QColor & = Qt::black,
                        bool = false, bool = false, int = 0) override { return m_loaded; }
    bool deleteObjectAt(int, const QPointF &) override { return m_loaded; }
    bool linearizeDocument(const QString &) override { return m_loaded; }
    bool exportPdfA(const QString &, int) override { return m_loaded; }
    bool exportMrcPdfA(const QString&, const QList<QImage>&,
                       const QList<PageOcrResult>&,
                       MrcMode) override { return m_loaded; }
    bool encryptDocument(const QString &, const QString &, const DocumentPermissions&) override { return m_loaded; }
    bool removeEncryption(const QString &) override { return m_loaded; }
    bool encryptWithCertificate(const QString &, const QString &, const QStringList &) override { return m_loaded; }
    bool sanitizeDocument(const QString &path) override { ++m_sanitizeCalls; m_lastSanitizedPath = path; return m_sanitizeResult && m_loaded; }
    bool getMetadata(PdfMetadata &out) override { out = m_meta; return true; }
    bool setMetadata(const PdfMetadata &meta) override { m_meta = meta; return true; }
    QString currentFile() const override { return m_file; }
    QStringList getEmbeddedFiles() override { return {}; }
    QByteArray extractEmbeddedFile(const QString &name) override { return QByteArray(); }
    QStringList getLayers() override { return {}; }
    
    bool rotatePage(const QString &path, int pageIndex, int degrees) override { return true; }
    QByteArray extractPageAsBytes(const QString &path, int pageIndex) override { return QByteArray(); }
    bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData) override { return true; }
    bool deletePage(const QString &path, int pageIndex) override { return true; }
    bool insertBlankPage(const QString &path, int atIndex) override { return true; }

    QList<PdfImageInfo> listImages(int) override { return {}; }
    bool moveImage(int, const QString &, double, double) override { return true; }
    bool resizeImage(int, const QString &, double, double) override { return true; }
    bool rotateImage(int, const QString &, double) override { return true; }
    bool replaceImage(int, const QString &, const QString &) override { return true; }
    bool deleteImage(int, const QString &) override { return true; }
    bool applyRedactions(int, const QList<QRectF> &) override { return m_loaded; }
    bool applyPatternRedactions(const QRegularExpression&, int, int) override { return m_loaded; }
    bool embedAnnotations(const QString &, const QString &, const QList<AnnotationItem> &) override { return m_loaded; }

    // Page geometry & content injection
    bool cropPage(const QString &, int, const QRectF &) override { return m_loaded; }
    bool resizePage(const QString &, int, const QSizeF &) override { return m_loaded; }
    bool reorderPages(const QString &, int, int) override { return m_loaded; }
    bool addHeaderFooter(const QString &, const HeaderFooterOptions &) override { return m_loaded; }
    bool applyBatesNumbering(const QString &, const BatesNumberingOptions &) override { return m_loaded; }

    // Watermarking & optimization (Session 13)
    bool addTextWatermark(const TextWatermarkOptions &) override { return m_loaded; }
    bool addImageWatermark(const ImageWatermarkOptions &) override { return m_loaded; }
    OptimizeEstimate estimateOptimization(const OptimizeOptions &) override { return OptimizeEstimate{}; }
    bool optimizeDocument(const QString &, const OptimizeOptions &) override { return m_loaded; }

    // Error reporting (Session 16)
    ErrorInfo lastError() const override { return m_lastError; }
    void clearError() override { m_lastError = ErrorInfo{}; }

    // Test helpers
    mutable ErrorInfo m_lastError;
    bool m_loaded = false;
    bool m_sanitizeResult = true;
    int m_sanitizeCalls = 0;
    int m_saveCalls = 0;
    QString m_lastSanitizedPath;
    QString m_lastSavedPath;
    QString m_file;
    PdfMetadata m_meta;
};
