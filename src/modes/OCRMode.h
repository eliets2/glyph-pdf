// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QRectF>
#include <QWidget>

#include "engines/ocr/OcrPipeline.h"       // MergedOcrWord, PageOcrResult
#include "docmodel/SemanticDocument.h"       // SemanticDocument
#include "pdfws_djot/LuaDjotCodec.h"         // documentToDjot (encode only)

class QComboBox;
class QCheckBox;
class QMenu;
class QToolButton;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QFrame;
class QVBoxLayout;

namespace gp {

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

signals:
    void ocrRequested();
    void reviewAccepted();
    void reviewRejected();
    /// Emitted when the user requests re-OCR of a specific region.
    void reOcrRegionRequested(QRectF regionBbox);
    /// R07: emitted on every state transition so hosts/tests can observe the
    /// lifecycle without polling widgets.
    void reviewStateChanged(gp::OCRMode::ReviewState state);

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

private:
    void buildToolbar(QVBoxLayout* col);
    void buildInfoStrip(QVBoxLayout* col);
    void buildPanes(QVBoxLayout* col);

    /// R07: single state-transition helper — updates m_reviewState, the
    /// lifecycle message and the user-visible controls in ONE place so every
    /// completion path restores the correct buttons (UI thread only).
    void transitionTo(ReviewState state, const QString& message);

    /// Build confidence-colored HTML for the scan pane from current word results.
    /// Green (#22c55e): confidence ≥ 90.  Yellow (#eab308): 70-89.  Red (#ef4444): < 70.
    void updateConfidenceOverlay();

    /// Update info strip (avg confidence, low-confidence word count) from m_currentWords.
    void updateInfoStrip();

    // Current OCR state
    QList<MergedOcrWord> m_currentWords;

    // R07: explicit lifecycle state + last user-visible lifecycle message.
    ReviewState m_reviewState = ReviewState::Idle;
    QString m_lastLifecycleMessage;

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
    QPlainTextEdit* m_textEdit         = nullptr;
    QFrame*         m_zoomPane         = nullptr;
    QLabel*         m_zoomBig          = nullptr;
    QLabel*         m_zoomMeta         = nullptr;
};

} // namespace gp
