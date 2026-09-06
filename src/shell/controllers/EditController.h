// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QRectF>
#include <QList>
#include <QRegularExpression>
#include <memory>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "engines/ocr/OcrPipeline.h" // PageOcrResult / MergedOcrWord (§9.4 Accept seam)
#include "modes/OcrReviewSession.h"  // R08: review session + reviewed word records

struct AppContext;
class EditToolBar;
class IOcrEngine;

namespace gp {

class MainWindow;

class EditController : public QObject, public IToolController {
    Q_OBJECT
public:
    EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // Search / replace slots wired from FindBar
    void onSearchRequested(const QString &text, bool forward, bool matchCase,
                           bool wholeWords, bool useRegex, int scope);
    void onReplaceRequested(const QString &searchText, const QString &replaceText,
                            bool matchCase, bool wholeWords, bool useRegex);
    void onReplaceAllRequested(const QString &searchText, const QString &replaceText,
                               bool matchCase, bool wholeWords, bool useRegex);
    void onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords);

    // §9.15 test seam: build the page-text matcher for the document-text search
    // path. Returns an inactive pattern when the flags are all off (the caller
    // then falls back to QPdfSearchModel's fast case-insensitive substring
    // scan). Shared by onSearchRequested and onRedactAllRequested so the two
    // callers cannot drift apart.
    struct PageTextPattern {
        QRegularExpression rx;   // invalid when the pattern itself is bad
        bool active = false;     // true when any of matchCase/wholeWords/useRegex is set
    };
    static PageTextPattern pageTextPattern(const QString &text, bool matchCase,
                                           bool wholeWords, bool useRegex);

    // ── R07 (F11): OCR lifecycle classification seams ────────────────────────
    // Terminal verdict of one dispatched OCR job. Every completion — success,
    // empty result, validation failure, worker failure, cancellation — maps to
    // exactly one verdict, and each verdict drives a distinct recovery path.
    enum class OcrJobVerdict {
        Deliver,   // results are fresh for the current document+page: deliver
        Stale,     // superseded/cancelled (newer job, page/doc switch, closed editor)
        Failed     // worker error (missing data, render/engine failure)
    };
    Q_ENUM(OcrJobVerdict)

    /// Pure seam: why a dispatch cannot start (empty string == dispatchable).
    /// Covers the "no document" exit so the panel can recover instead of being
    /// left with Run disabled forever.
    static QString ocrDispatchBlocker(const QString& filePath, int page);

    /// Pure seam: classify one job completion. jobGeneration != currentGeneration
    /// means a newer request superseded it; an empty currentSourcePath means the
    /// viewer/editor was gone. The human-readable recovery message is written to
    /// messageOut when non-null.
    static OcrJobVerdict classifyOcrJobCompletion(
        qint64 jobGeneration, qint64 currentGeneration,
        const QString& jobSourcePath, int jobPage,
        const QString& currentSourcePath, int currentPage,
        const QString& workerError, QString* messageOut);

    // ── R08 (F04): reviewed-word authority seams ─────────────────────────────
    /// Pure seam: may this review session still be saved against the live
    /// viewer? Rejects stale sessions after a source change (different
    /// document) or a revision change (page count differs). Writes a
    /// human-readable reason to reasonOut when non-null.
    static bool ocrSessionIsExportable(const OcrReviewSession& session,
                                       const QString& currentSourcePath,
                                       int currentPageCount,
                                       QString* reasonOut = nullptr);

    /// Pure seam: merge the panel's reviewed records into the session and
    /// build the per-page export payload. The payload's pageIndex is the
    /// SESSION's reviewed page (never the currently displayed page). Records
    /// must align 1:1 with the session words (same count, same stable IDs) —
    /// otherwise a stale-interaction error is written to errorOut. An empty
    /// record list reviews the session unedited.
    static PageOcrResult buildReviewedPageOcrResult(const OcrReviewSession& session,
                                                    const QList<OcrReviewedWord>& reviewedWords,
                                                    QString* errorOut = nullptr);

public slots:
    // Run OCR on the viewer's current page (engine chosen per Preferences). Public so
    // the OCR Verify screen's Run button can drive the same real pipeline as the ribbon.
    void runOcr();

    // §9.4 P0: persist the accepted OCR results as a searchable MRC PDF/A copy
    // (called directly from MainWindow, but kept as a slot for consistency with
    // the other EditController entry points wired to the OCR Verify screen).
    // R08: the host passes the review panel's reviewed records; the reviewed
    // words are authoritative for the export.
    void onOcrAcceptRequested(const QList<OcrReviewedWord>& reviewedWords);
    void onOcrAcceptRequested();   // legacy entry: no panel records (unedited review)

    // §9.4 P0 test seam: assemble the per-page OCR payload for exportMrcPdfA.
    static PageOcrResult buildPageOcrResult(int pageIndex, const QList<MergedOcrWord>& words);

    // §9.4 honesty seams: the interactive Accept flow persists a ONE-PAGE MRC
    // PDF/A (runOcr recognises the current page only), so the save dialog and
    // the success status must say so instead of implying a whole-document
    // searchable copy. Single-page documents need no scope note — the one-page
    // copy IS the document.
    static QString ocrSaveDialogTitle(int totalPages, int pageIndex);
    static QString ocrSavedStatus(int totalPages, int pageIndex, const QString& fileName);

signals:
    // Emitted on the GUI thread when an OCR run finishes, carrying the recognised
    // words so the OCR Verify screen can display them for review.
    void ocrResultsReady(const QList<MergedOcrWord>& words);

    // U03: emitted right after ocrResultsReady with the FULL review session —
    // source identity/revision, the rendered page image the words belong to,
    // and the words with stable IDs. The host relays it (via
    // ModeController::deliverOcrReview) to the OCR Verify screen so the scan
    // pane shows the real source image instead of text-only word links.
    void ocrReviewReady(const gp::OcrReviewSession& session);

    // ── R07 (F11): lifecycle completion signals ──────────────────────────────
    // Worker/validation failure (missing language data, ONNX models, render or
    // engine failure, blocked dispatch). The host relays it to the review panel
    // so Run is restored for retry.
    void ocrRunFailed(const QString& message);
    // Cancellation: the job was abandoned (newer request, page/document switch,
    // editor closed) and its results were dropped.
    void ocrRunAbandoned(const QString& message);
    // Save outcome after Accept: every path (success, dialog cancellation,
    // write failure, validation failure) is reported exactly once so the panel
    // can leave the Saving state and re-enable Save/Accept.
    void ocrSaveFinished(bool saved, bool canceled, const QString& message);

private slots:
    void onImageSelected(const QString &name, const QRectF &placement);
    void onImageMoved(const QString &name, double dx, double dy);
    void onImageResized(const QString &name, double newW, double newH);
    void onTextEditRequested(int pageIndex, QPointF pos);
    void onTextFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment);
    void onEraseRequested(int pageIndex, QPointF pos);

private:
    void editPdfText();
    void enterImageEditMode();
    bool copySelectionToClipboard();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _ocrRunning = false;

    // R07: monotonically increasing identity of the dispatched OCR job. The
    // completion callback captures its generation and drops results when a
    // newer request has been issued in the meantime.
    qint64 _ocrJobGeneration = 0;

    // R08: the review session of the most recent delivered OCR run — source
    // identity/revision, the page image the words belong to, and the words
    // with stable IDs. Acceptance validates it against the live viewer, merges
    // the panel's reviewed records, and exports the reviewed text.
    OcrReviewSession m_reviewSession;

    // P4: cache the initialized OCR engine pair across runs. Constructing a fresh
    // OcrEngine/RapidOcrEngine per call rebuilt 3 ONNX sessions (and the Tesseract
    // API) from disk every time, defeating each engine's own init guard. We reuse
    // one Tesseract + one RapidOCR instance, reinitializing only when the language
    // changes. Access is serialized by _ocrRunning (one OCR run at a time), so no
    // additional locking is required.
    std::shared_ptr<IOcrEngine> _ocrTesseract;   // primary (Tesseract 5)
    std::shared_ptr<IOcrEngine> _ocrRapid;        // RapidOCR / PP-OCRv5
    QString _ocrTesseractLang;                    // language the cached Tesseract was init'd with
    QString _ocrRapidLang;                        // language the cached RapidOCR was init'd with

    QString _selectedImageName;
    int _imageEditPage = -1;
    EditToolBar* _textToolBar = nullptr;

    // Text formatting state
    QString _fontFamily = "Helvetica";
    int _fontSize = 12;
    QColor _fontColor = Qt::black;
    bool _fontBold = false;
    bool _fontItalic = false;
    int _fontAlignment = 0;

    // Search state for match navigation
    int _currentMatchIndex = -1;
    int _totalMatches = 0;
};

} // namespace gp
