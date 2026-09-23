// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/interfaces/IPdfDocument.h"
#include "core/interfaces/IPdfWriter.h"
#include "core/interfaces/IPdfEditorEngine.h" // For PdfMetadata and PdfImageInfo
#include <QStringList>
#include <QList>
#include <QRectF>
#include <QPointF>
#include <memory>
#include <vector>

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

    // R2-1 D2: true iff the loaded document has at least one PDF signature field.
    bool hasPdfSignatures() const;

    // G1 (audit REDACTION-RESEARCH-2026-09-21 §2.5): true iff the loaded
    // document carries legacy XFA form data (/AcroForm /XFA or catalog /XFA).
    bool hasXfaDocument() const;

    // G2 (audit §2.4): the named reason the last applyRedactions aborted its
    // content surgery (empty when it did not abort). Surfaced in the
    // engine-level failure message so refusals name WHY.
    QString lastRedactionAbortReason() const;

    // WP-R09b (WHOLE-ARCHITECTURE-REVIEW A05): sticky until the next
    // successful load or in-place commit — the last refusal of an in-place
    // commit was an EXTERNAL source-version conflict (the file changed on
    // disk since it was loaded), not an ordinary I/O failure.
    bool lastCommitRefusedForExternalConflict() const;

    // WP-R09b: capture/refresh the external source-version baseline of
    // `path` (the recovery-destination priming entry for the shell).
    void primeExternalBaseline(const QString &path);

    // ER-3: number of CMS recipient envelopes in /Encrypt → /Recipients.
    int recipientCount() const;

    // Document information/structure
    QString currentFile() const;
    void setCurrentFile(const QString &path);
    QStringList getEmbeddedFiles();
    QByteArray extractEmbeddedFile(const QString &name);
    QStringList getLayers();

    // Page editing operations
    bool rotatePage(const QString &path, int pageIndex, int degrees);
    QByteArray extractPageAsBytes(const QString &path, int pageIndex);
    bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData);
    bool deletePage(const QString &path, int pageIndex);
    // G08 (QUALITY-GATE-2026-09-09): one committed step — swap the page at
    // `pageIndex` for the `pageData` copy inside a single transaction.
    bool restorePageFromBytes(const QString &path, int pageIndex, const QByteArray &pageData);
    bool insertBlankPage(const QString &path, int atIndex);

    // Content editing
    bool editTextInline(int pageIndex, const QRectF &rect, const QString &newText,
                        const QString &fontFamily = "", int fontSize = 0,
                        const QColor &color = Qt::black, bool bold = false,
                        bool italic = false, int alignment = 0,
                        double opacity = 1.0, double letterSpacing = 0.0,
                        double lineSpacing = 1.0);
    bool deleteObjectAt(int pageIndex, const QPointF &pos);
    bool applyRedactions(int pageIndex, const QList<QRectF> &rects);
    // PGR-37 (D2 delta review 2026-09-23): the SAME excision surgery in RAW
    // USER space — rects taken verbatim as content-stream coordinates (the
    // space the surgery itself operates in), with NO viewer transform.
    // Consumer: the PatternRedactor-fed paths (applyPatternRedactions[Multi]),
    // whose producer emits raw user-space char boxes (FPDFText_GetCharBox).
    // applyRedactions (viewer space, the L8 viewerToUser law) remains the
    // contract for VIEWER-produced marks; feeding PDFium-derived rects
    // through that transform transposes the excision on /Rotate 90/270 pages
    // and shifts it by the MediaBox origin — a silent redaction false success.
    bool applyRedactionsUserSpace(int pageIndex, const QList<QRectF> &userRects);

    // T2-2 (ITextReplacer): Find & Replace — excise every matched region,
    // cover it white, draw the replacement at the match origin in the match's
    // font size (standard-14 Helvetica). See ITextReplacer for the contract.
    bool replaceTextRegions(const QList<TextReplacementSpec>& specs,
                            QList<double>* drawnWidthsOut = nullptr);

    // Page Geometry & Operations
    bool cropPage(const QString &path, int pageIndex, const QRectF &cropRect);
    QRectF pageCropBox(const QString &path, int pageIndex, bool *ok);
    // G07 (QUALITY-GATE-2026-09-09): origin-aware effective-CropBox snapshot
    // (explicit / inherited / absent — see IPdfEditorEngine for the origin
    // codes) and the restoration of absent/inherited semantics after a crop.
    bool pageCropBoxInfo(const QString &path, int pageIndex,
                         QRectF *outBox, int *outOrigin);
    bool removePageCropBox(const QString &path, int pageIndex);
    // GUI-held-handle residual: drops the resident document when it is loaded
    // from `path` (alias-tolerant), closing the parser device that would block
    // an external same-path SafeSave replacement (form import). The next
    // resolveDocument(path) lazily re-loads from disk.
    void releaseResidentFile(const QString &path);
    bool resizePage(const QString &path, int pageIndex, const QSizeF &size);
    bool reorderPages(const QString &path, int fromIndex, int toIndex);
    bool reorderAllPages(const QString &path, const QList<int> &permutation);

    // Content Injection
    bool addHeaderFooter(const QString &path, const HeaderFooterOptions &options);
    bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options);
    // §9.9 P1: cross-document Bates continuity. Stamps exactly like the
    // two-argument overload; on success `*lastNumberOut` (when non-null)
    // receives the LAST Bates number used on the final stamped page, so a
    // batch caller can stamp document N+1 starting at *lastNumberOut + 1.
    // If nothing was stamped (empty/out-of-range page range) it reports
    // options.startNumber - 1, keeping `next = last + 1` always safe.
    bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options, int *lastNumberOut);

    // Annotation Export
    bool embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations);

    // T2-9 (IOutlineEditor): outline read/write — see IOutlineEditor.
    QList<OutlineEntry> getOutline(const QString& path);
    bool replaceOutline(const QString& path, const QList<OutlineEntry>& entries);

    // Annotation Import (M6-P4 D4) — reads annotation dictionaries back into
    // AnnotationItem, restoring djotSource from the /PieceInfo /GlyphPDF sidecar
    // when present, else deriving a trivial djotSource from /Contents. Enables
    // the perfect GlyphPDF→PDF→GlyphPDF rich-text roundtrip.
    QList<AnnotationItem> extractAnnotations(const QString &inputPath);

    // §9.1 P0: clickable link annotations on one page (URI + internal GoTo).
    static QList<PdfLinkInfo> extractLinks(const QString &inputPath, int pageIndex);

    // Specialized conversion/security operations
    bool linearizeDocument(const QString &outputPath);
    bool exportPdfA(const QString &outputPath, int conformanceLevel);
    bool encryptDocument(const QString &userPassword, const QString &ownerPassword,
                         const DocumentPermissions& perms);
    bool removeEncryption(const QString &ownerPassword);
    bool sanitizeDocument(const QString &outputPath);

    // Image operations
    QList<PdfImageInfo> listImages(int pageIndex);
    bool moveImage(int pageIndex, const QString &xobjectName, double dx, double dy);
    bool resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight);
    bool rotateImage(int pageIndex, const QString &xobjectName, double degrees);
    bool replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath);
    bool deleteImage(int pageIndex, const QString &xobjectName);
    // Byte-exact content edits via gp::content (ContentSpans.h); both refuse,
    // leaving the document untouched, when the edit could restyle the image.
    bool setImageZOrder(int pageIndex, const QString &xobjectName, bool bringToFront);
    bool setImageOpacity(int pageIndex, const QString &xobjectName, double opacity);

    // Watermarking (Session 13)
    bool addTextWatermark(const TextWatermarkOptions &options);
    bool addImageWatermark(const ImageWatermarkOptions &options);

    // Optimization (Session 13)
    OptimizeEstimate estimateOptimization(const OptimizeOptions &options);
    bool optimizeDocument(const QString &outputPath, const OptimizeOptions &options);

private:
    class Private;
    std::unique_ptr<Private> d;

    // WP-R02 (WHOLE-ARCHITECTURE-REVIEW A01): the commit step every RESIDENT
    // mutator uses. Identical to writeUpdate() except for the failure
    // semantics: a mutator's refused commit is a transaction rollback — the
    // resident document is restored to the pre-mutation baseline (earlier
    // accepted edits preserved) or the disk bytes (fresh-load lineage, the
    // G06 rule). writeUpdate() (the user-Save route) keeps the resident work
    // retryable instead. Returns false on refusal; the resident state is
    // resolved as described.
    bool commitMutation(const QString &path);

    // Shared body of writeUpdate()/commitMutation(): `mutationTransaction`
    // selects the rollback-on-failure semantics.
    bool commitMutationImpl(const QString &path, bool mutationTransaction);

    // PGR-37: the redaction surgery body shared by applyRedactions (viewer
    // marks mapped via PageSpace::viewerToUser) and applyRedactionsUserSpace
    // (PDFium-derived rects taken verbatim). d->mutex held; pageIndex
    // validated; abort reason cleared. `userRects` are RAW USER space; the
    // PoDoFo Rect conversion happens inside (this header stays PoDoFo-free).
    bool applyRedactionsMappedLocked(int pageIndex, const QList<QRectF> &userRects);
};
