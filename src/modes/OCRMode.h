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

    /// Load a completed OCR result into the mode for review.
    /// Call this after the OCR pipeline produces results.
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

private slots:
    void onRunOcr();
    void onAcceptResults();
    void onImagePaneContextMenu(const QPoint &pos);
    void onReOcrRegion();

private:
    void buildToolbar(QVBoxLayout* col);
    void buildInfoStrip(QVBoxLayout* col);
    void buildPanes(QVBoxLayout* col);

    /// Build confidence-colored HTML for the scan pane from current word results.
    /// Green (#22c55e): confidence ≥ 90.  Yellow (#eab308): 70-89.  Red (#ef4444): < 70.
    void updateConfidenceOverlay();

    /// Update info strip (avg confidence, low-confidence word count) from m_currentWords.
    void updateInfoStrip();

    // Current OCR state
    QList<MergedOcrWord> m_currentWords;

    // Last right-clicked region bbox (used by onReOcrRegion)
    QRectF m_contextRegionBbox;

    // Toolbar controls
    QComboBox*   m_engineCombo   = nullptr;
    QComboBox*   m_strategyCombo = nullptr;
    QComboBox*   m_langCombo     = nullptr;
    QCheckBox*   m_chkDeskew     = nullptr;
    QCheckBox*   m_chkBinarize   = nullptr;
    QCheckBox*   m_chkDenoise    = nullptr;
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
