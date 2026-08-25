// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QPair>
#include <QImage>
#include <QRectF>
#include <QWidget>

#include "engines/ocr/OcrPipeline.h"       // MergedOcrWord, PageOcrResult
#include "docmodel/SemanticDocument.h"       // SemanticDocument
#include "pdfws_djot/LuaDjotCodec.h"         // documentToDjot (encode only)
#include "modes/OcrVerifyDialog.h"           // B3: Verify Text dialog

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
/// B1 (FineReader verify spec): low-confidence words (< 70, the same cutoff
/// as the red scan-pane overlay) are highlighted with a light-blue background
/// inside the editable text pane — FineReader's "uncertain characters"
/// treatment. A checkable toggle in the text-pane header shows/hides it.
class OCRMode : public QWidget {
    Q_OBJECT
public:
    explicit OCRMode(QWidget* parent = nullptr);

    /// Number of words flagged uncertain (confidence < 70) in the loaded results.
    int lowConfidenceWordCount() const { return m_lowConfWords.size(); }

    /// Number of ExtraSelection highlights currently applied to the text pane.
    /// Mirrors the toggle state: 0 when highlighting is disabled.
    int uncertainHighlightCount() const;

    /// The editable recognized-text pane (for tests and sibling-pane sync).
    QPlainTextEdit* textPane() const { return m_textEdit; }

    // ── B7: status-strip stats ──────────────────────────────────────────
    /// Set the page indicator ("PAGE x OF y"); y<=0 shows the em-dash state.
    void setPageProgress(int current, int total);
    /// Mark a word as human-verified (drives the VERIFIED % cell).
    void markWordVerified(int wordIndex);
    /// Fraction of loaded words marked verified, in percent (0 if none loaded).
    int verifiedPercent() const;

    /// B12: explicit "this page is done" state (Ctrl+T / toolbar toggle).
    void setPageVerified(bool verified);
    bool isPageVerified() const { return m_pageVerified; }

    // ── B10: per-language user dictionary ───────────────────────────────
    /// Path of the user dictionary file for a language code.
    static QString userDictionaryPath(const QString &langCode);
    /// Load the user dictionary for a language (empty if none yet).
    static QStringList loadUserDictionary(const QString &langCode);
    /// Append a word to the user dictionary for a language.
    static bool addUserDictionaryWord(const QString &langCode, const QString &word);

    /// B9: ranked correction candidates for `word` from `vocabulary`
    /// (Damerau-Levenshtein ≤ 2; no external spell engine —
    /// upgrade path: swap for Hunspell suggest() behind this seam).
    static QStringList suggestCorrections(const QString &word,
                                          const QStringList &vocabulary);

    /// B6: provide the page raster so the zoom pane can show a real magnified
    /// crop of the selected word. Optional — without it the zoom pane falls
    /// back to showing the recognized string in large type.
    void setPageImage(const QImage &pageImage);

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
    /// B5 sync: emitted when the selected word changes (text-pane caret move
    /// or scan-pane word click). Index refers to m_currentWords order.
    void wordSelected(int wordIndex);

private slots:
    void onRunOcr();
    void onAcceptResults();
    void onRejectResults();
    void onImagePaneContextMenu(const QPoint &pos);
    void onReOcrRegion();
    /// B5: caret moved in the text pane — resolve to a word and sync panes.
    void onTextCursorMoved();
    /// B5: an ocrword:<i> anchor was clicked in the scan pane.
    void onScanWordLinkActivated(const QString &link);
    /// B2: jump to the next/previous low-confidence word (Alt+Down / Alt+Up).
    void gotoNextUncertain();
    void gotoPrevUncertain();
    /// B3: open the Verify Text dialog (Ctrl+F7) over the low-conf index.
    void openVerifyDialog();
    /// B4: apply a confirmed correction from the Verify dialog.
    void onVerifyConfirm(int wordIndex, const QString &correctedText);
    /// B4: skip — just advance the selection, no state change.
    void onVerifySkip(int wordIndex);

private:
    void buildToolbar(QVBoxLayout* col);
    void buildInfoStrip(QVBoxLayout* col);
    void buildPanes(QVBoxLayout* col);

    /// Build confidence-colored HTML for the scan pane from current word results.
    /// Green (#22c55e): confidence ≥ 90.  Yellow (#eab308): 70-89.  Red (#ef4444): < 70.
    void updateConfidenceOverlay();

    /// Update info strip (avg confidence, low-confidence word count) from m_currentWords.
    void updateInfoStrip();

    /// B1: record character offsets of each word while populating the text pane,
    /// then (re)apply the uncertain-word ExtraSelection highlights.
    void rebuildTextWordIndex();
    void applyUncertainHighlights();

    /// B5: select word i in all panes (text caret, scan outline, zoom crop).
    void syncWordTo(int wordIndex);

    // Current OCR state
    QList<MergedOcrWord> m_currentWords;

    // B5: currently synchronized word (-1 = none).
    int  m_selectedWord   = -1;
    bool m_syncing        = false;   // guards against caret<->selection loops

    // B1: char-range (start, length) of every word in the text pane, in order,
    // parallel to m_currentWords; plus indices of the <70 subset (B2 nav index).
    QList<QPair<int, int>> m_wordRanges;
    QList<int>             m_lowConfWords;

    // B2: current position within m_lowConfWords (-1 = not started).
    int m_uncertainCursor = -1;
    bool m_uncertainEnabled = true;
    QToolButton* m_btnUncertainToggle = nullptr;
    QToolButton* m_btnPrevUncertain   = nullptr;
    QToolButton* m_btnNextUncertain   = nullptr;

    // B7: verification progress + page indicator state.
    QList<bool> m_verifiedWords;
    int m_pageCurrent = 0;
    int m_pageTotal   = 0;

    // B12: explicit page-level verified state.
    bool m_pageVerified = false;
    QToolButton* m_btnPageVerified = nullptr;

    // B10: language code whose user dictionary gates flagging.
    QString m_dictLang = QStringLiteral("EN");

    // B6: optional page raster backing the zoom pane's magnified crop.
    QImage m_pageImage;
    void renderZoomCrop(int wordIndex);

    // B3: floating Verify Text dialog (owned, created lazily).
    OcrVerifyDialog* m_verifyDialog = nullptr;

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
    QLabel* m_lblLanguage   = nullptr;
    QLabel* m_lblAvgConf    = nullptr;
    QLabel* m_lblLowWords   = nullptr;
    QLabel* m_lblVerified   = nullptr;
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
