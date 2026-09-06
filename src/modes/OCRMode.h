// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QRectF>
#include <QWidget>

#include "engines/ocr/OcrPipeline.h"       // MergedOcrWord, PageOcrResult
#include "modes/OcrReviewSession.h"         // OcrReviewedWord (R08 review records)
#include "docmodel/SemanticDocument.h"       // SemanticDocument
#include "pdfws_djot/LuaDjotCodec.h"         // documentToDjot (encode only)

class QComboBox;
class QCheckBox;
class QMenu;
class QToolButton;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QFrame;
class QVBoxLayout;
class QStackedLayout;

namespace gp {

class OcrScanCanvas;
class OcrWordMagnifier;

/// OCR Verification Mode: top toolbar + info strip + 4-pane splitter.
/// M5-P2 additions:
///   - Per-word confidence overlay (green ≥90 / yellow 70-89 / red <70)
///   - Right-click region → "Re-OCR this region"
///   - "Review before save" per-region accept/reject
class OCRMode : public QWidget {
    Q_OBJECT
public:
    explicit OCRMode(QWidget* parent = nullptr);

    /// R07: explicit lifecycle state of the review panel. Every terminal OCR
    /// outcome (success, empty result, validation failure, worker failure,
    /// cancellation) drives the panel into one of these states, so Run/Accept
    /// are never left stuck.
    enum class ReviewState {
        Idle,               // no results; Run available
        Running,            // a job was dispatched; all actions disabled
        ReviewReady,        // words delivered; review + save possible
        Saving,             // Accept pressed; awaiting save outcome
        RecoverableError    // failed/cancelled; Run restored for retry
    };
    Q_ENUM(ReviewState)

    ReviewState reviewState() const { return m_reviewState; }

    /// R07: last lifecycle message shown for the current state (failure,
    /// cancellation, or save outcome). Empty until something is reported.
    const QString& lastLifecycleMessage() const { return m_lastLifecycleMessage; }

    // ── R08 (F04): reviewed words are authoritative ──────────────────────────
    /// The reviewed records for the words currently displayed: stable IDs,
    /// original text, per-word reviewed text, deleted flags, and the ORIGINAL
    /// source boxes. These travel with acceptance into the searchable-PDF
    /// export; the plain-text page view is only a preview.
    const QList<OcrReviewedWord>& reviewedWords() const { return m_reviewWords; }

    /// Word-based correction (the first supported correction interaction):
    /// updates the record's reviewed text WITHOUT changing its source box.
    /// Empty/whitespace text marks the word removed. Returns false for
    /// unknown stable IDs (stale interaction).
    bool applyWordCorrection(int stableId, const QString& text);

    /// Mark a word deleted (kept in the record so the overlay can show it as
    /// struck-through until a fresh recognition replaces it). Returns false
    /// for unknown stable IDs.
    bool markWordDeleted(int stableId);

    /// Select a word from the scan pane's word link (href "word:<id>"):
    /// highlights it in both panes and loads it into the correction field.
    void activateWordLink(const QString& link);

    /// Load a completed OCR result into the mode for review.
    /// Call this after the OCR pipeline produces results.
    /// This is the success/empty-result completion entry: non-empty words move
    /// the panel to ReviewReady; an empty list completes the run as Idle.
    void setOcrResults(const QList<MergedOcrWord> &words);

    /// Load an OcrDjotMapper-produced SemanticDocument into the review pane.
    ///
    /// The scan pane renders a simple inline-HTML preview (block structure visible).
    /// The text pane (m_textEdit) is populated with the Djot source text for
    /// Djot-aware edit-in-place (same pattern as M6-P4 annotation editor).
    ///
    /// Per-region accept/reject from setOcrResults() is preserved:
    /// the accept/reject buttons remain active after this call.
    ///
    /// djotLibPath: path to the vendored djot/ directory (passed to LuaDjotCodec).
    ///              May be empty — in that case the encode-only C++ emitter is used
    ///              (it does not require the Lua runtime for the encode direction).
    void setSemanticDocument(const docmodel::SemanticDocument &doc,
                             const QString &djotLibPath = QString());

    // ── U03: the source-and-correction loop ─────────────────────────────────
    /// Deliver the FULL review session (source identity/revision, the rendered
    /// page image the words were recognized on, and the words with stable IDs).
    /// The session image is handed to the scan canvas and the word magnifier
    /// via QImage implicit sharing — one buffer, no re-render, no per-widget
    /// copy. The review records come from session.words (authoritative).
    void setReviewSession(const gp::OcrReviewSession& session);

    /// THE one selection funnel: word links, canvas clicks and the uncertain-
    /// word navigation all land here, so scan-pane highlight, correction
    /// field, magnifier crop and review state can never disagree.
    void selectWord(int stableId);

    /// Wrap-around iterator over the uncertain words (see isUncertain) starting
    /// after (forward) or before (!forward) fromId. Returns the stableId, or
    /// -1 when no record qualifies. Pure seam — testable without a GUI.
    int nextUncertainWord(int fromId, bool forward) const;

    /// The review session currently loaded (invalid when none).
    const gp::OcrReviewSession& reviewSession() const { return m_session; }

    /// Uncertain = still needs human eyes: LOW confidence band, not removed.
    /// A correction does NOT turn the model's confidence estimate into a high
    /// one — a corrected low-confidence word stays in the navigation (the
    /// source-engine provenance is separate from review status).
    static bool isUncertain(const OcrReviewedWord& w);

signals:
    void ocrRequested();
    void reviewAccepted();
    void reviewRejected();
    /// Emitted when the user requests re-OCR of a specific region.
    void reOcrRegionRequested(QRectF regionBbox);
    /// R07: emitted on every state transition so hosts/tests can observe the
    /// lifecycle without polling widgets.
    void reviewStateChanged(gp::OCRMode::ReviewState state);
    /// U03: emitted by selectWord() — the single selection funnel — so hosts
    /// can observe which stable word record is under inspection.
    void selectedWordChanged(int stableId);

public slots:
    // Lifecycle entries. Run/Accept/Reject stay enabled only in the states that
    // permit them; the slots below complete the lifecycle on every exit.
    void onRunOcr();
    void onAcceptResults();
    void onRejectResults();

    /// R07: worker/validation failure completion (missing language data, ONNX
    /// models, render failure, engine failure). Restores Run for retry; never
    /// re-enables review from stale content.
    void notifyOcrFailed(const QString& message);
    /// R07: cancellation completion — the job was abandoned because the page or
    /// document changed (or the editor closed) before results could apply.
    void notifyOcrCanceled(const QString& message);
    /// R07: save outcome completion. A cancelled save (canceled=true) retains
    /// the review edits and re-enables Accept; a failed save retains data for
    /// retry; a successful save returns to a reviewable state.
    void notifySaveFinished(bool saved, bool canceled, const QString& message);

private slots:
    void onImagePaneContextMenu(const QPoint &pos);
    void onReOcrRegion();
    /// Scan-pane word link activation (QLabel::linkActivated) → select word.
    void onWordLinkActivated(const QString& link);

private:
    void buildToolbar(QVBoxLayout* col);
    void buildInfoStrip(QVBoxLayout* col);
    void buildPanes(QVBoxLayout* col);

    /// R07: single state-transition helper — updates m_reviewState, the
    /// lifecycle message and the user-visible controls in ONE place so every
    /// completion path restores the correct buttons (UI thread only).
    void transitionTo(ReviewState state, const QString& message);

    /// R08: refresh the zoom pane for the currently selected word.
    void updateWordInspector();

    // ── U03 helpers ─────────────────────────────────────────────────────────
    /// Show the source-image canvas when a session image is loaded, else the
    /// rich-text word list (accessibility/text fallback).
    void updateScanPaneView();
    /// The zoom header shows the magnifier's COMPUTED magnification ("×N.N"),
    /// never a static multiplier claim.
    void updateZoomHeader();
    /// Next/prev-uncertain buttons follow the same lifecycle discipline as
    /// Accept/Reject: enabled only in ReviewReady when something is uncertain.
    void updateNavigationButtons();

    /// Build confidence-colored HTML for the scan pane from current word results.
    /// Bands and colors come from THE one classifier (modes/OcrConfidence.h):
    /// OcrConfidence::bandFor + bandColor — green ≥ 90, yellow 70–89, red < 70.
    void updateConfidenceOverlay();

    /// Update info strip (avg confidence, low-confidence word count) from m_currentWords.
    void updateInfoStrip();

    // Current OCR state
    QList<MergedOcrWord> m_currentWords;

    // R07: explicit lifecycle state + last user-visible lifecycle message.
    ReviewState m_reviewState = ReviewState::Idle;
    QString m_lastLifecycleMessage;

    // R08: reviewed word records (stable ids + per-word reviewed text + the
    // immutable source boxes). Authoritative for the export; m_currentWords is
    // the raw recognition delivery and the plain-text pane is only a preview.
    QList<OcrReviewedWord> m_reviewWords;
    int m_selectedWordId = -1;

    // U03: the review session of the delivered OCR run (source identity +
    // revision + the page image the words belong to). One copy lives here;
    // the scan canvas and the word magnifier share its image implicitly.
    OcrReviewSession m_session;

    // Last right-clicked region bbox (used by onReOcrRegion)
    QRectF m_contextRegionBbox;

    // Toolbar controls
    QComboBox*   m_engineCombo   = nullptr;
    QComboBox*   m_strategyCombo = nullptr;
    QComboBox*   m_langCombo     = nullptr;
    QCheckBox*   m_chkDeskew     = nullptr;
    QCheckBox*   m_chkBinarize   = nullptr;
    QCheckBox*   m_chkDenoise    = nullptr;
    QCheckBox*   m_chkOrientDetect = nullptr;  // §9.4: persisted Auto-Rotate pref
    QToolButton* m_btnRun        = nullptr;
    QToolButton* m_btnAccept     = nullptr;
    QToolButton* m_btnReject     = nullptr;

    // Info strip labels
    QLabel* m_lblPage       = nullptr;
    QLabel* m_lblAvgConf    = nullptr;
    QLabel* m_lblLowWords   = nullptr;
    QLabel* m_lblEngine     = nullptr;

    // Panes
    QListWidget*    m_pageList         = nullptr;
    QFrame*         m_imagePane        = nullptr;
    QLabel*         m_scanContentLabel = nullptr;  // rich-text confidence overlay
    OcrScanCanvas*  m_scanCanvas       = nullptr;  // U03: real source image + word boxes
    QStackedLayout* m_scanStack        = nullptr;  // canvas <-> text fallback
    QPlainTextEdit* m_textEdit         = nullptr;
    QFrame*         m_zoomPane         = nullptr;
    QLabel*         m_zoomHeader       = nullptr;  // U03: computed "ZOOM · ×N.N"
    OcrWordMagnifier* m_magnifier      = nullptr;  // U03: magnified source crop
    QLabel*         m_zoomMeta         = nullptr;
    // R08: word inspector — the first supported correction interaction.
    QLineEdit*      m_wordEdit         = nullptr;
    QToolButton*    m_btnDeleteWord    = nullptr;
    // U03: uncertain-word navigation (wrap-around, next/prev).
    QToolButton*    m_btnNextUncertain = nullptr;
    QToolButton*    m_btnPrevUncertain = nullptr;
};

} // namespace gp
