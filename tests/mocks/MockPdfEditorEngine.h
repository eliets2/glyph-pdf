#pragma once
#include "core/interfaces/IPdfEditorEngine.h"
#include <QMap>
#include <QImage>
#include <QFile>
#include <QSemaphore>

class MockPdfEditorEngine : public IPdfEditorEngine {
public:
    // EC02 test barrier: when armed, the save decision parks the caller so a
    // test can switch documents mid-flight deterministically (no sleeps).
    // One-shot per save call: saveDocumentIfCurrent passes the gate, then its
    // saveDocument delegation sees null pointers and does not re-gate.
    void saveGatePass() {
        if (m_saveEntered) { QSemaphore* s = m_saveEntered; m_saveEntered = nullptr; s->release(); }
        if (m_saveHold)    { QSemaphore* s = m_saveHold;    m_saveHold = nullptr;    s->acquire();  }
    }

    bool loadDocumentForEditing(const QString &) override { m_loaded = true; return true; }
    bool saveDocument(const QString &path) override {
        ++m_saveCalls;
        m_lastSavedPath = path;
        saveGatePass();
        if (m_loaded) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(m_saveWritesIdentity
                            ? QByteArray("resident=") + m_file.toUtf8()
                            : QByteArray("mock"));
                f.close();
            }
            return true;
        }
        return false;
    }
    // EC02 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): identity-guarded save — the
    // resident document must still be `expectedCurrentFile` when the save
    // decision is made, so a queued async writer can never serialize document
    // B's bytes into a recovery path captured for document A.
    // NOTE: deliberately no `override` keyword — pre-fix baselines (revert
    // verification) have no such virtual yet; post-fix it implements
    // IPdfDocumentIO::saveDocumentIfCurrent. Signature is pinned by the
    // AutosaveManager call and the interface declaration.
    bool saveDocumentIfCurrent(const QString &expectedCurrentFile, const QString &outputPath) {
        ++m_saveIfCurrentCalls;
        m_lastIfCurrentExpected = expectedCurrentFile;
        saveGatePass();
        if (m_file != expectedCurrentFile) {
            m_lastIfCurrentRefusal = expectedCurrentFile;
            return false;
        }
        return saveDocument(outputPath);
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
    bool rotateImage(int pageIndex, const QString &name, double degrees) override {
        m_lastRotatePage = pageIndex; m_lastRotateName = name; m_lastRotateDegrees = degrees;
        ++m_rotateCalls; return true;
    }
    bool replaceImage(int, const QString &, const QString &) override { return true; }
    // TestHistoryIntegrity (EC03): the fault engines below subclass this mock;
    // deleteImage is counted so a refusal ("no destructive edit without a
    // restorable backup") is observable.
    bool deleteImage(int, const QString &) override { ++m_deleteImageCalls; return true; }
    int m_deleteImageCalls = 0;
    bool applyRedactions(int, const QList<QRectF> &) override { return m_loaded; }
    bool applyMarkRedactions(const QList<AnnotationItem>& marks) override {
        m_lastMarkRedactions = marks;
        return m_loaded;
    }
    bool applyPatternRedactions(const QRegularExpression&, const QList<int>&, const QString&) override { return m_loaded; }
    bool applyPatternRedactionsMulti(const QStringList&, const QList<int>&, const QString&) override { return m_loaded; }
    bool embedAnnotations(const QString &, const QString &, const QList<AnnotationItem> &) override { return m_loaded; }

    // Page geometry & content injection
    bool cropPage(const QString &, int, const QRectF &) override { return m_loaded; }
    // EC05 (2026-09-08 persistence lane): new interface member — deliberately
    // NO `override` keyword. Pre-fix baselines (revert verification) have no
    // such virtual, and this header must compile against them; post-fix this
    // implements IPageEditor::pageCropBox (signature pinned by the interface).
    QRectF pageCropBox(const QString &, int, bool *ok) {
        if (ok) *ok = m_loaded;
        return QRectF(0, 0, 595, 842);
    }
    // G07 (QUALITY-GATE-2026-09-09): origin-aware snapshot + semantics
    // restoration seam. Again NO `override` — compiles as a plain member
    // against pre-fix baselines, implements the interface virtuals post-fix.
    // The restore-fault flags let history tests inject a failing restoration
    // deterministically.
    bool pageCropBoxInfo(const QString &, int, QRectF *outBox, int *outOrigin) {
        if (outBox) *outBox = QRectF(0, 0, 595, 842);
        // literal 1 == IPdfEditorEngine::kCropBoxExplicit (G07 origin codes;
        // written as a literal so this header still compiles against pre-fix
        // baselines during revert verification, where the constant is absent)
        if (outOrigin) *outOrigin = 1;
        return m_loaded && !m_cropSnapshotFails;
    }
    bool removePageCropBox(const QString &, int) {
        ++m_removeCropBoxCalls;
        return m_cropRestoreOk && m_loaded;
    }
    int m_removeCropBoxCalls = 0;
    bool m_cropSnapshotFails = false;
    bool m_cropRestoreOk = true;
    // G08 (QUALITY-GATE-2026-09-09): the single-transaction page-restore seam
    // with fault injection; the base class keeps the two-step
    // insertPageFromBytes/deletePage calls so tests can prove the commands no
    // longer take the intermediate-state path.
    bool restorePageFromBytes(const QString &, int, const QByteArray &) {
        ++m_restorePageCalls;
        return m_pageRestoreOk && m_loaded;
    }
    int m_restorePageCalls = 0;
    bool m_pageRestoreOk = true;
    // GUI-held-handle residual (2026-09-08 persistence lane): shell-side
    // same-path writers release the resident file before replacing it. Again
    // NO `override` — the interface member is new in this repair; pre-fix
    // baselines compile this as a plain member, post-fix it implements
    // IPageEditor::releaseResidentFile.
    void releaseResidentFile(const QString &) {}
    bool resizePage(const QString &, int, const QSizeF &) override { return m_loaded; }
    bool reorderPages(const QString &, int, int) override { return m_loaded; }
    bool reorderAllPages(const QString &, const QList<int> &) override { return m_loaded; }
    bool addHeaderFooter(const QString &, const HeaderFooterOptions &) override { return m_loaded; }
    bool applyBatesNumbering(const QString &, const BatesNumberingOptions &) override { return m_loaded; }
    // §9.9 P1: continuity overload — the mock performs no real stamping, so it
    // only honours the success contract (report goes untouched when null).
    bool applyBatesNumbering(const QString &, const BatesNumberingOptions &, int *lastNumberOut) override {
        if (m_loaded && lastNumberOut) *lastNumberOut = m_mockBatesLastNumber;
        return m_loaded;
    }

    // Watermarking & optimization (Session 13)
    bool addTextWatermark(const TextWatermarkOptions &) override { return m_loaded; }
    bool addImageWatermark(const ImageWatermarkOptions &) override { return m_loaded; }
    OptimizeEstimate estimateOptimization(const OptimizeOptions &) override { return OptimizeEstimate{}; }
    bool optimizeDocument(const QString &, const OptimizeOptions &) override { return m_loaded; }

    // Error reporting (Session 16)
    ErrorInfo lastError() const override { return m_lastError; }
    void clearError() override { m_lastError = ErrorInfo{}; }

    // R2-1 D2
    bool writeUpdate(const QString &path) override {
        ++m_writeUpdateCalls;
        m_lastWriteUpdatePath = path;
        return saveDocument(path);
    }
    // §9.11: expiry marker (promoted onto IPdfDocumentIO)
    bool setExpiryDate(const QString &pdfPath, const QDate &date, const QString &outputPath) override {
        ++m_expiryCalls;
        m_lastExpiryPath = pdfPath;
        m_lastExpiryOut = outputPath;
        m_lastExpiryDate = date;
        return m_loaded && date.isValid();
    }
    // §9.1: link reader (promoted onto IPdfDocumentIO)
    QList<PdfLinkInfo> extractLinks(const QString &pdfPath, int pageIndex) override {
        ++m_linkCalls;
        m_lastLinkPath = pdfPath;
        m_lastLinkPage = pageIndex;
        return m_links;
    }
    bool hasPdfSignatures() const override { return m_hasPdfSignatures; }
    int recipientCount() const override { return 0; }

    // Test helpers
    mutable ErrorInfo m_lastError;
    bool m_loaded = false;
    bool m_sanitizeResult = true;
    bool m_hasPdfSignatures = false;
    // EC02 barrier + identity-bytes hooks
    QSemaphore* m_saveEntered = nullptr;
    QSemaphore* m_saveHold = nullptr;
    bool m_saveWritesIdentity = false;
    int m_saveIfCurrentCalls = 0;
    QString m_lastIfCurrentExpected;
    QString m_lastIfCurrentRefusal;
    int m_sanitizeCalls = 0;
    int m_saveCalls = 0;
    int m_writeUpdateCalls = 0;
    QString m_lastSanitizedPath;
    QString m_lastSavedPath;
    QString m_lastWriteUpdatePath;
    QString m_file;
    PdfMetadata m_meta;
    // §9.2 P0: image-rotate tracking
    int m_rotateCalls = 0;
    int m_lastRotatePage = -1;
    QString m_lastRotateName;
    double m_lastRotateDegrees = 0.0;
    // §9.8 P0: mark-based redaction tracking
    QList<AnnotationItem> m_lastMarkRedactions;
    // §9.9 P1: Bates continuity overload — last number the fake stamping
    // "consumed" (reported through the out-param when the call succeeds).
    int m_mockBatesLastNumber = 0;
    // §9.11: expiry tracking
    int m_expiryCalls = 0;
    QString m_lastExpiryPath;
    QString m_lastExpiryOut;
    QDate m_lastExpiryDate;
    // §9.1: link-reader tracking
    int m_linkCalls = 0;
    int m_lastLinkPage = -1;
    QString m_lastLinkPath;
    QList<PdfLinkInfo> m_links;
};
