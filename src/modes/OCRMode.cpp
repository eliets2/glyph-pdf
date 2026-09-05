// SPDX-License-Identifier: Apache-2.0
#include "OCRMode.h"
#include "engines/ocr/RapidOcrEngine.h"
#include "core/OcrTypes.h"
#include "util/GpTheme.h"
#include "util/Badge.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "pdfws_djot/LuaDjotCodec.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QSettings>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gp {

// Shared QSettings key for the user's selected OCR language code (e.g. "EN").
// StatusBar reads the same key to display the real selected language.
static const char* kOcrLanguageKey = "ocr/language";

// Shared QSettings key for the Auto-Rotate toggle (page-level 0/90/180/270
// orientation detection before OCR). EditController::runOcr and BatchMode read
// the same key — OCRMode here only persists the preference.
static const char* kOcrOrientDetectKey = "ocr/orientDetect";
// §9.4: the preprocessing checkboxes are persisted prefs — the pipeline reads
// them (via EditController/BatchMode) instead of ignoring them.
static const char* kOcrPreprocessDeskewKey   = "ocr/preprocessDeskew";
static const char* kOcrPreprocessBinarizeKey = "ocr/preprocessBinarize";
static const char* kOcrPreprocessDenoiseKey  = "ocr/preprocessDenoise";

// Empty-state shown in the scan/confidence pane before any OCR has run.
static const char* kOcrEmptyStateHtml =
    "<span style='color:#8a8a8a;font-size:13px;'>No OCR results yet.<br><br>"
    "Open a scanned PDF and run OCR to review the recognized text and "
    "per-word confidence here.</span>";

// ── helpers ─────────────────────────────────────────────────────────────────

static QFrame* makeStrip(const char* role, int h) {
    auto* f = new QFrame;
    f->setProperty("role", role);
    f->setFixedHeight(h);
    return f;
}

static QLabel* monoLab(const QString& s) {
    auto* l = new QLabel(s);
    l->setProperty("mono", true);
    return l;
}

static QLabel* infoLab(const QString& s) {
    auto* l = new QLabel(s);
    l->setProperty("role", "infoStrip");
    return l;
}

// ── construction ────────────────────────────────────────────────────────────

OCRMode::OCRMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    buildToolbar(col);
    buildInfoStrip(col);
    buildPanes(col);
}

// ── toolbar ─────────────────────────────────────────────────────────────────

void OCRMode::buildToolbar(QVBoxLayout* col)
{
    auto* tb = makeStrip("modeToolbar", Theme::ToolbarH);
    auto* row = new QHBoxLayout(tb);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(6);

    row->addWidget(monoLab(tr("OCR")));

    // ── Language selector ───────────────────────────────────────────────
    m_langCombo = new QComboBox;
    m_langCombo->setObjectName("ocrLangCombo");
    for (const auto& l : ocrLanguages()) {
        m_langCombo->addItem(QStringLiteral("%1 · %2")
                                 .arg(QLatin1String(l.uiCode), QCoreApplication::translate("OCRMode", l.displayName)));
    }
    m_langCombo->setProperty("variant", "ghost");

    // Restore the previously-selected OCR language (code before " · ") and
    // persist any change, so the StatusBar OCR cell reflects a real choice.
    {
        QSettings settings;
        const QString savedCode = settings.value(kOcrLanguageKey, "EN").toString();
        for (int i = 0; i < m_langCombo->count(); ++i) {
            if (m_langCombo->itemText(i).section(QStringLiteral(" · "), 0, 0) == savedCode) {
                m_langCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    connect(m_langCombo, &QComboBox::currentTextChanged, this, [](const QString& text) {
        QSettings settings;
        settings.setValue(kOcrLanguageKey, text.section(QStringLiteral(" · "), 0, 0));
    });

    row->addWidget(m_langCombo);

    // ── Engine selector ─────────────────────────────────────────────────
    m_engineCombo = new QComboBox;
    m_engineCombo->setObjectName("ocrEngineCombo");
    m_engineCombo->addItem("Tesseract 5");
#ifdef HAS_RAPIDOCR
    m_engineCombo->addItem("RapidOCR (PP-OCRv5)");
    // Runtime gate: disable the selector only while the engine is still a Mock.
    // RapidOcrEngine now runs the real PP-OCRv5 pipeline, so it stays enabled.
    if (RapidOcrEngine().isMockImplementation()) {
        auto* model = qobject_cast<QStandardItemModel*>(m_engineCombo->model());
        if (model) {
            auto* item = model->item(1);
            if (item) {
                item->setEnabled(false);
                item->setToolTip(tr("Available in a future release"));
            }
        }
    }
#endif
    m_engineCombo->setCurrentIndex(0); // Default to first non-mock engine (Tesseract)
    m_engineCombo->setProperty("variant", "ghost");
    row->addWidget(m_engineCombo);

    // ── Strategy selector ───────────────────────────────────────────────
    m_strategyCombo = new QComboBox;
    m_strategyCombo->setObjectName("ocrStrategyCombo");
    m_strategyCombo->addItem(tr("Primary Only"));
    m_strategyCombo->addItem(tr("Confidence Weighted"));
    m_strategyCombo->addItem(tr("ROVER Vote"));
    m_strategyCombo->setProperty("variant", "ghost");
    row->addWidget(m_strategyCombo);

    // ── Preprocessing toggles ───────────────────────────────────────────
    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedWidth(1); sep1->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep1);

    // Defaults match OcrPreprocessOptions' long-standing pipeline behavior
    // (deskew/binarize/denoise all on) so wiring the prefs is behavior-neutral
    // until the user changes something — previously the Denoise checkbox
    // showed "off" while the pipeline denoised anyway.
    m_chkDeskew = new QCheckBox(tr("Deskew"));
    m_chkDeskew->setObjectName("ocrChkDeskew");
    m_chkDeskew->setChecked(QSettings().value(kOcrPreprocessDeskewKey, true).toBool());
    m_chkDeskew->setStyleSheet("color:#c0c0c0; spacing:4px;");
    connect(m_chkDeskew, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue(kOcrPreprocessDeskewKey, checked);
    });
    row->addWidget(m_chkDeskew);

    m_chkBinarize = new QCheckBox(tr("Binarize"));
    m_chkBinarize->setObjectName("ocrChkBinarize");
    m_chkBinarize->setChecked(QSettings().value(kOcrPreprocessBinarizeKey, true).toBool());
    m_chkBinarize->setStyleSheet("color:#c0c0c0; spacing:4px;");
    connect(m_chkBinarize, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue(kOcrPreprocessBinarizeKey, checked);
    });
    row->addWidget(m_chkBinarize);

    m_chkDenoise = new QCheckBox(tr("Denoise"));
    m_chkDenoise->setObjectName("ocrChkDenoise");
    m_chkDenoise->setChecked(QSettings().value(kOcrPreprocessDenoiseKey, true).toBool());
    m_chkDenoise->setStyleSheet("color:#c0c0c0; spacing:4px;");
    connect(m_chkDenoise, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue(kOcrPreprocessDenoiseKey, checked);
    });
    row->addWidget(m_chkDenoise);

    // §9.4 P0: Auto-Rotate — the only preprocessing toggle here that is wired
    // through to the pipeline (via the shared QSettings key, which the
    // EditController/BatchMode OCR paths read). Default off: zero behavior
    // change unless the user asks for it.
    m_chkOrientDetect = new QCheckBox(tr("Auto-Rotate"));
    m_chkOrientDetect->setObjectName("ocrChkOrientDetect");
    m_chkOrientDetect->setChecked(QSettings().value(kOcrOrientDetectKey, false).toBool());
    m_chkOrientDetect->setStyleSheet("color:#c0c0c0; spacing:4px;");
    connect(m_chkOrientDetect, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue(kOcrOrientDetectKey, checked);
    });
    row->addWidget(m_chkOrientDetect);

    row->addStretch(1);

    // ── Run / Review actions ────────────────────────────────────────────
    m_btnRun = new QToolButton;
    m_btnRun->setObjectName("ocrBtnRun");
    m_btnRun->setText(tr("Run OCR"));
    m_btnRun->setProperty("variant", "accent");
    m_btnRun->setAccessibleName(tr("Run OCR"));
    m_btnRun->setAccessibleDescription(tr("Run optical character recognition on the current page"));
    connect(m_btnRun, &QToolButton::clicked, this, &OCRMode::onRunOcr);
    row->addWidget(m_btnRun);

    m_btnAccept = new QToolButton;
    m_btnAccept->setObjectName("ocrBtnAccept");
    m_btnAccept->setText(tr("✓ Accept"));
    m_btnAccept->setProperty("variant", "ghost");
    m_btnAccept->setEnabled(false);
    m_btnAccept->setAccessibleName(tr("Accept OCR results"));
    m_btnAccept->setAccessibleDescription(tr("Keep the recognised text and confidence overlay for this page"));
    connect(m_btnAccept, &QToolButton::clicked, this, &OCRMode::onAcceptResults);
    row->addWidget(m_btnAccept);

    m_btnReject = new QToolButton;
    m_btnReject->setObjectName("ocrBtnReject");
    m_btnReject->setText(tr("✗ Reject"));
    m_btnReject->setProperty("variant", "ghost");
    m_btnReject->setEnabled(false);
    m_btnReject->setAccessibleName(tr("Reject OCR results"));
    m_btnReject->setAccessibleDescription(tr("Discard the recognised text and clear the OCR overlay"));
    connect(m_btnReject, &QToolButton::clicked, this, &OCRMode::onRejectResults);
    row->addWidget(m_btnReject);

    auto* sep2 = new QFrame; sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1); sep2->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep2);

    auto* exit = new QToolButton;
    exit->setText(tr("Exit OCR"));
    exit->setProperty("variant", "ghost");
    connect(exit, &QToolButton::clicked, this, [this]() {
        // Walk up to the top-level window and request close. The host shell
        // intercepts this and returns to the previous mode page in v1.0.0.
        QWidget* w = this;
        while (w && w->parentWidget()) w = w->parentWidget();
        if (w) w->close();
    });
    row->addWidget(exit);

    col->addWidget(tb);
}

// ── info strip ──────────────────────────────────────────────────────────────

void OCRMode::buildInfoStrip(QVBoxLayout* col)
{
    auto* info = makeStrip("infoStrip", Theme::InfoStripH);
    auto* row = new QHBoxLayout(info);
    row->setContentsMargins(12, 0, 12, 0);
    row->setSpacing(14);

    m_lblPage    = infoLab(tr("PAGE — OF —"));
    m_lblAvgConf = infoLab(tr("AVG CONFIDENCE —"));
    m_lblLowWords= infoLab(tr("LOW-CONFIDENCE WORDS —"));
    m_lblEngine  = infoLab(tr("ENGINE: Tesseract 5"));

    row->addWidget(m_lblPage);
    row->addWidget(m_lblAvgConf);
    row->addWidget(m_lblLowWords);
    row->addWidget(m_lblEngine);
    row->addStretch(1);

    col->addWidget(info);
}

// ── 4-pane splitter ─────────────────────────────────────────────────────────

void OCRMode::buildPanes(QVBoxLayout* col)
{
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    // ── Page list ───────────────────────────────────────────────────────
    // Populated by setOcrResults() from real document pages; starts empty.
    m_pageList = new QListWidget;
    m_pageList->setObjectName("ocrPageList");
    m_pageList->setFixedWidth(180);
    split->addWidget(m_pageList);

    // ── Image / scan pane ───────────────────────────────────────────────
    m_imagePane = new QFrame;
    auto* impLay = new QVBoxLayout(m_imagePane);
    impLay->setContentsMargins(0,0,0,0); impLay->setSpacing(0);

    auto* impHead = makeStrip("modeToolbar", 24);
    auto* impHeadRow = new QHBoxLayout(impHead);
    impHeadRow->setContentsMargins(12,0,12,0);
    impHeadRow->addWidget(monoLab(tr("IMAGE · SCAN")));
    impHeadRow->addStretch(1);
    impHeadRow->addWidget(monoLab(tr("4× PIXELS")));
    impLay->addWidget(impHead);

    // ── Confidence overlay: scrollable paper with per-word colored spans ──────
    // m_scanContentLabel is updated by updateConfidenceOverlay() each time
    // OCR results arrive.  Right-click opens the per-region context menu.
    m_scanContentLabel = new QLabel;
    m_scanContentLabel->setObjectName("ocrScanContent");
    m_scanContentLabel->setTextFormat(Qt::RichText);
    m_scanContentLabel->setWordWrap(true);
    m_scanContentLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scanContentLabel->setStyleSheet(
        "background:#f4f1ea; color:#1a1a1a; padding:24px; "
        "border:1px solid #000; min-width:380px;");
    m_scanContentLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_scanContentLabel, &QLabel::customContextMenuRequested,
            this, &OCRMode::onImagePaneContextMenu);
    // R08: each recognized word is a link; clicking selects its stable record
    // (the first supported correction interaction is word-based).
    connect(m_scanContentLabel, &QLabel::linkActivated,
            this, &OCRMode::onWordLinkActivated);

    // Empty state until a document is OCR'd (replaced by updateConfidenceOverlay).
    m_scanContentLabel->setText(kOcrEmptyStateHtml);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidget(m_scanContentLabel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background:#2a2a2a;");

    impLay->addWidget(scrollArea, 1);
    split->addWidget(m_imagePane);

    // ── Text pane (plain-text page view: PREVIEW only) ──────────────────
    // R08 (F04): this pane used to be labelled editable but was never read
    // back — Accept exported the cached original words. It is now honestly
    // labelled a preview; word corrections (word inspector) are authoritative.
    auto* textPane = new QFrame;
    auto* txtLay = new QVBoxLayout(textPane);
    txtLay->setContentsMargins(0,0,0,0); txtLay->setSpacing(0);

    auto* txtHead = makeStrip("modeToolbar", 24);
    auto* txtHeadRow = new QHBoxLayout(txtHead);
    txtHeadRow->setContentsMargins(12,0,12,0);
    txtHeadRow->addWidget(monoLab(tr("RECOGNIZED · PREVIEW")));
    txtHeadRow->addStretch(1);
    txtHeadRow->addWidget(monoLab(tr("word corrections are saved, not this text")));
    txtLay->addWidget(txtHead);

    m_textEdit = new QPlainTextEdit;
    m_textEdit->setObjectName("ocrTextEdit");
    m_textEdit->setPlaceholderText(
        tr("Recognized text appears here after you run OCR on a document."));
    txtLay->addWidget(m_textEdit, 1);
    split->addWidget(textPane);

    // ── Zoom / metadata pane ────────────────────────────────────────────
    m_zoomPane = new QFrame;
    m_zoomPane->setFixedWidth(200);
    auto* zLay = new QVBoxLayout(m_zoomPane);
    zLay->setContentsMargins(0,0,0,0); zLay->setSpacing(0);

    auto* zHead = makeStrip("modeToolbar", 24);
    auto* zHeadRow = new QHBoxLayout(zHead);
    zHeadRow->setContentsMargins(12,0,12,0);
    zHeadRow->addWidget(monoLab(tr("ZOOM · 4×")));
    zHeadRow->addStretch(1);
    zLay->addWidget(zHead);

    m_zoomBig = new QLabel(QStringLiteral("\xE2\x80\x94"));  // em dash — no selection
    m_zoomBig->setAlignment(Qt::AlignCenter);
    m_zoomBig->setStyleSheet(
        "background:#e8e6df; color:#1a1a1a; font-family:Manrope; "
        "font-size:42px; font-weight:600; padding:16px; margin:24px 16px; "
        "border:1px solid #000;");
    zLay->addWidget(m_zoomBig);

    m_zoomMeta = new QLabel(tr("No word selected"));
    m_zoomMeta->setProperty("mono", true);
    m_zoomMeta->setStyleSheet("padding:8px 12px;");
    m_zoomMeta->setAlignment(Qt::AlignLeft);
    zLay->addWidget(m_zoomMeta);

    // ── R08: word inspector — the first supported correction interaction ──
    // Editing the selected word updates its stable record (reviewed text);
    // the source box never moves, so export coordinates stay truthful.
    auto* wordRow = new QHBoxLayout;
    wordRow->setContentsMargins(12, 0, 12, 0);
    m_wordEdit = new QLineEdit;
    m_wordEdit->setObjectName("ocrWordEdit");
    m_wordEdit->setPlaceholderText(tr("Correct the selected word"));
    m_wordEdit->setEnabled(false);
    m_wordEdit->setToolTip(tr(
        "Edits the selected word record; the original text box is kept. "
        "Clear the text to remove the word from the saved text layer."));
    connect(m_wordEdit, &QLineEdit::returnPressed, this, [this]() {
        applyWordCorrection(m_selectedWordId, m_wordEdit->text());
    });
    wordRow->addWidget(m_wordEdit, 1);
    m_btnDeleteWord = new QToolButton;
    m_btnDeleteWord->setObjectName("ocrBtnDeleteWord");
    m_btnDeleteWord->setText(tr("✕"));
    m_btnDeleteWord->setToolTip(tr("Remove the selected word from the saved text layer"));
    m_btnDeleteWord->setEnabled(false);
    connect(m_btnDeleteWord, &QToolButton::clicked, this, [this]() {
        markWordDeleted(m_selectedWordId);
    });
    wordRow->addWidget(m_btnDeleteWord);
    zLay->addLayout(wordRow);

    // ── Confidence legend ───────────────────────────────────────────────
    auto* legend = new QFrame;
    auto* legendLay = new QVBoxLayout(legend);
    legendLay->setContentsMargins(12, 8, 12, 8);
    legendLay->setSpacing(4);

    legendLay->addWidget(monoLab(tr("CONFIDENCE")));

    auto makeLegendRow = [&](const QString &color, const QString &label) {
        auto* row = new QHBoxLayout;
        auto* swatch = new QFrame;
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QString("background:%1; border:1px solid %1; border-radius:2px;").arg(color));
        row->addWidget(swatch);
        row->addWidget(monoLab(label));
        row->addStretch(1);
        legendLay->addLayout(row);
    };

    makeLegendRow("#22c55e", tr("HIGH (≥ 80%)"));
    makeLegendRow("#eab308", tr("MEDIUM (50-79%)"));
    makeLegendRow("#ef4444", tr("LOW (< 50%)"));

    zLay->addWidget(legend);
    zLay->addStretch(1);
    split->addWidget(m_zoomPane);

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    split->setStretchFactor(3, 0);
    col->addWidget(split, 1);
}

// ── slots ───────────────────────────────────────────────────────────────────

// R07: ONE state-transition helper. Every completion path lands here, so the
// user-visible controls are restored consistently on the UI thread:
//   - Running/Saving disable everything,
//   - Idle/ReviewReady restore Run (review only when words exist),
//   - RecoverableError restores Run for retry but NEVER re-enables review from
//     stale content — only a fresh setOcrResults() may enable Accept again
//     ("a stale completion must not re-enable another document's save").
void OCRMode::transitionTo(ReviewState state, const QString& message)
{
    m_reviewState = state;
    m_lastLifecycleMessage = message;

    const bool hasWords = !m_reviewWords.isEmpty();
    auto setRun = [this](bool enabled, const QString& text) {
        if (!m_btnRun) return;
        m_btnRun->setEnabled(enabled);
        m_btnRun->setText(text);
    };
    auto setReview = [this](bool enabled) {
        if (m_btnAccept) m_btnAccept->setEnabled(enabled);
        if (m_btnReject) m_btnReject->setEnabled(enabled);
    };

    switch (state) {
    case ReviewState::Idle:
        setRun(true, tr("Run OCR"));
        setReview(false);
        break;
    case ReviewState::Running:
        setRun(false, tr("Running…"));
        setReview(false);
        break;
    case ReviewState::ReviewReady:
        setRun(true, tr("Run OCR"));
        setReview(hasWords);
        break;
    case ReviewState::Saving:
        setRun(false, tr("Run OCR"));
        setReview(false);
        break;
    case ReviewState::RecoverableError:
        setRun(true, tr("Run OCR"));
        setReview(false);
        break;
    }
    emit reviewStateChanged(state);
}

void OCRMode::onRunOcr()
{
    // R2: do NOT pre-enable Accept/Reject here. They must stay disabled until
    // setOcrResults() actually delivers recognised words — otherwise the user
    // can "accept" a result that does not exist yet. Show a processing state
    // instead and let the completion paths restore the controls.
    transitionTo(ReviewState::Running, QString());

    // Update engine label
    m_lblEngine->setText(tr("ENGINE: %1 · %2")
        .arg(m_engineCombo->currentText(),
             m_strategyCombo->currentText()));

    // Confidence stats will be updated by setOcrResults() when results arrive.
    // Clear the stats now to avoid showing stale values while OCR runs.
    m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
    m_lblLowWords->setText(tr("LOW-CONFIDENCE WORDS —"));

    emit ocrRequested();
}

void OCRMode::onAcceptResults()
{
    if (m_reviewState != ReviewState::ReviewReady) return;  // nothing reviewable
    // R07: Accept moves the panel to Saving BEFORE the save dialog opens; the
    // save outcome (success/cancel/failure) always arrives via
    // notifySaveFinished() and restores the review controls.
    transitionTo(ReviewState::Saving, QString());
    emit reviewAccepted();
}

void OCRMode::onRejectResults()
{
    // Reject clears the current OCR overlay/results so the page returns to its
    // pre-OCR state; the host is notified to drop any pending applied text.
    m_currentWords.clear();
    m_reviewWords.clear();
    m_selectedWordId = -1;
    updateWordInspector();
    updateConfidenceOverlay();
    updateInfoStrip();
    if (m_textEdit) m_textEdit->clear();
    transitionTo(ReviewState::Idle, tr("OCR results rejected."));
    emit reviewRejected();
}

// ── R07 completion paths (wired from EditController via the host) ────────────

void OCRMode::notifyOcrFailed(const QString& message)
{
    // Worker/validation failure (missing language data, ONNX models, render or
    // engine failure): recoverable — Run is restored for retry.
    transitionTo(ReviewState::RecoverableError, message);
}

void OCRMode::notifyOcrCanceled(const QString& message)
{
    // Cancellation: the job was abandoned (page/document changed or editor
    // closed before results could apply). Same recovery contract as failure.
    transitionTo(ReviewState::RecoverableError, message);
}

void OCRMode::notifySaveFinished(bool saved, bool canceled, const QString& message)
{
    // A cancelled or failed save retains the review edits (m_currentWords is
    // untouched) so the user can retry; success also returns to a reviewable
    // state (the result can be saved again to another destination).
    Q_UNUSED(saved);
    Q_UNUSED(canceled);
    transitionTo(m_reviewWords.isEmpty() ? ReviewState::Idle : ReviewState::ReviewReady,
                 message);
}

// ── Context menu (right-click on scan pane) ──────────────────────────────────

void OCRMode::onImagePaneContextMenu(const QPoint &pos)
{
    // For the rich-text label, we use a fixed "current page" region
    // as the re-OCR target.  Future work: map pos to individual LayoutRegion bboxes.
    m_contextRegionBbox = QRectF();  // empty = whole current page

    QMenu menu(this);

    // U03 honesty note: until region operations exist, these actions act on the
    // WHOLE page — the labels say so instead of implying a bounded region.
    QAction *reOcrAction = menu.addAction(tr("Re-OCR entire page"));
    connect(reOcrAction, &QAction::triggered, this, &OCRMode::onReOcrRegion);

    menu.addSeparator();

    // Per-region accept / reject workflow
    QAction *acceptRegion = menu.addAction(tr("Accept entire page"));
    connect(acceptRegion, &QAction::triggered, this, &OCRMode::onAcceptResults);

    QAction *rejectRegion = menu.addAction(tr("Reject entire page"));
    connect(rejectRegion, &QAction::triggered, this, &OCRMode::onRejectResults);

    QAction *scopeNote = menu.addAction(tr("Regional actions act on the whole page until region OCR ships"));
    scopeNote->setEnabled(false);

    menu.exec(m_scanContentLabel->mapToGlobal(pos));
}

void OCRMode::onReOcrRegion()
{
    emit reOcrRegionRequested(m_contextRegionBbox);
}

// ── setOcrResults ─────────────────────────────────────────────────────────────

void OCRMode::setOcrResults(const QList<MergedOcrWord> &words)
{
    m_currentWords = words;

    // R08: build the reviewed word records — stable IDs are the delivery
    // order, reviewed text starts as the original text, source boxes are the
    // recognized boxes and are never changed by review edits.
    m_reviewWords.clear();
    m_reviewWords.reserve(words.size());
    for (int i = 0; i < words.size(); ++i) {
        OcrReviewedWord rec;
        rec.stableId      = i;
        rec.originalText  = words[i].text;
        rec.reviewedText  = words[i].text;
        rec.deleted       = false;
        rec.boundingBox   = words[i].boundingBox;
        rec.confidence    = words[i].confidence;
        rec.sourceEngine  = words[i].sourceEngine;
        m_reviewWords.append(rec);
    }
    m_selectedWordId = -1;

    updateWordInspector();
    updateConfidenceOverlay();
    updateInfoStrip();

    // Populate the plain-text PREVIEW pane with the recognized text (preview
    // only — corrections in the word inspector are what gets saved).
    if (m_textEdit) {
        QStringList lines;
        for (const auto &w : words)
            lines.append(w.text);
        m_textEdit->setPlainText(lines.join(QStringLiteral(" ")));
    }

    // R07: one completion path each. A non-empty delivery is the success path
    // (ReviewReady); an empty recognition completes the run as Idle — retryable,
    // with nothing to review — instead of silently leaving stale stats.
    if (words.isEmpty())
        transitionTo(ReviewState::Idle,
                     tr("OCR complete — no text recognized on this page."));
    else
        transitionTo(ReviewState::ReviewReady, QString());
}

// ── R08: word-based review (reviewed words are authoritative) ────────────────

bool OCRMode::applyWordCorrection(int stableId, const QString& text)
{
    if (stableId < 0 || stableId >= m_reviewWords.size()) return false;
    OcrReviewedWord& rec = m_reviewWords[stableId];
    if (text.trimmed().isEmpty()) {
        // Clearing the text removes the word from the saved text layer.
        rec.deleted = true;
        rec.reviewedText.clear();
    } else {
        rec.deleted = false;
        rec.reviewedText = text;
    }
    // The source box (rec.boundingBox) is deliberately untouched.
    updateConfidenceOverlay();
    updateWordInspector();
    return true;
}

bool OCRMode::markWordDeleted(int stableId)
{
    if (stableId < 0 || stableId >= m_reviewWords.size()) return false;
    OcrReviewedWord& rec = m_reviewWords[stableId];
    rec.deleted = true;
    rec.reviewedText.clear();
    updateConfidenceOverlay();
    updateWordInspector();
    return true;
}

void OCRMode::activateWordLink(const QString& link)
{
    // Links are emitted as "word:<stableId>" by the scan-pane overlay.
    if (!link.startsWith(QStringLiteral("word:"))) return;
    bool ok = false;
    const int id = QStringView(link).mid(5).toInt(&ok);
    if (!ok || id < 0 || id >= m_reviewWords.size()) return;
    m_selectedWordId = id;
    updateConfidenceOverlay();   // re-highlight the selected word
    updateWordInspector();
}

void OCRMode::onWordLinkActivated(const QString& link)
{
    activateWordLink(link);
}

void OCRMode::updateWordInspector()
{
    const bool valid = m_selectedWordId >= 0 && m_selectedWordId < m_reviewWords.size();
    if (m_wordEdit) {
        m_wordEdit->setEnabled(valid);
        m_wordEdit->setText(valid ? m_reviewWords[m_selectedWordId].reviewedText
                                  : QString());
    }
    if (m_btnDeleteWord) m_btnDeleteWord->setEnabled(valid);
    if (m_zoomBig) {
        m_zoomBig->setText(valid ? m_reviewWords[m_selectedWordId].reviewedText
                                 : QStringLiteral("\xE2\x80\x94"));
    }
    if (m_zoomMeta) {
        if (!valid) {
            m_zoomMeta->setText(tr("No word selected"));
        } else {
            const OcrReviewedWord& rec = m_reviewWords[m_selectedWordId];
            QString state;
            if (rec.deleted || rec.reviewedText.trimmed().isEmpty())
                state = tr("removed");
            else if (rec.reviewedText != rec.originalText)
                state = tr("corrected");
            else
                state = tr("unreviewed");
            m_zoomMeta->setText(tr("word %1 · %2% · %3 · %4")
                .arg(rec.stableId)
                .arg(rec.confidence)
                .arg(rec.sourceEngine, state));
        }
    }
}

// ── updateConfidenceOverlay ───────────────────────────────────────────────────

void OCRMode::updateConfidenceOverlay()
{
    if (!m_scanContentLabel) return;

    if (m_reviewWords.isEmpty()) {
        // No results yet — show the empty state, not stale content.
        m_scanContentLabel->setText(kOcrEmptyStateHtml);
        return;
    }

    // Build a rich-text paragraph with per-word confidence coloring.
    // Thresholds per M5-P2 D6 spec:
    //   green (#22c55e): confidence ≥ 90
    //   yellow (#eab308): confidence 70–89
    //   red (#ef4444): confidence < 70
    // R08: each word is a link carrying its stable ID; corrected words show
    // their reviewed text with a marker, removed words are struck through.
    QString html;
    html.reserve(m_reviewWords.size() * 110);

    for (const auto &rec : m_reviewWords) {
        const int conf = rec.confidence;
        QString bgColor, borderColor;
        if (conf >= 90) {
            bgColor     = QStringLiteral("#22c55e33");
            borderColor = QStringLiteral("#22c55e99");
        } else if (conf >= 70) {
            bgColor     = QStringLiteral("#eab30833");
            borderColor = QStringLiteral("#eab30899");
        } else {
            bgColor     = QStringLiteral("#ef444433");
            borderColor = QStringLiteral("#ef444499");
        }

        const bool removed = rec.deleted || rec.reviewedText.trimmed().isEmpty();
        const bool corrected = !removed && rec.reviewedText != rec.originalText;
        const QString shown = removed ? rec.originalText
                                      : (rec.reviewedText.isEmpty() ? rec.originalText
                                                                    : rec.reviewedText);
        QString style = QStringLiteral("background:%1;outline:1px solid %2;padding:1px;margin:1px;")
                            .arg(bgColor, borderColor);
        if (m_selectedWordId == rec.stableId) {
            style += QStringLiteral("border:2px solid #2563eb;");
        }
        if (corrected) {
            style += QStringLiteral("outline:2px solid #2563eb99;");
        }
        QString extra;
        if (removed) {
            style += QStringLiteral("text-decoration:line-through;color:#777;");
            extra = tr(" | removed");
        } else if (corrected) {
            extra = tr(" | corrected to: %1").arg(rec.reviewedText);
        }

        // Escape HTML special chars in the word text
        QString escaped = shown.toHtmlEscaped();

        html += QStringLiteral(
            "<a href='word:%1' style='%2' "
            "title='%3% | %4 | %5%6'>%7</a> ")
            .arg(rec.stableId)
            .arg(style)
            .arg(conf)
            .arg(rec.sourceEngine)
            .arg(rec.boundingBox.x(), 0, 'f', 0)
            .arg(extra)
            .arg(escaped);
    }

    m_scanContentLabel->setText(html);
}

// ── updateInfoStrip ───────────────────────────────────────────────────────────

void OCRMode::updateInfoStrip()
{
    // R08: review stats come from the reviewed records; removed words no longer
    // count toward the page text (confidence itself stays the engine estimate —
    // corrections do not turn a model estimate into 100%).
    int counted = 0;
    double totalConf = 0.0;
    int lowCount     = 0;
    for (const auto &rec : m_reviewWords) {
        if (rec.deleted || rec.reviewedText.trimmed().isEmpty()) continue;
        ++counted;
        totalConf += rec.confidence;
        if (rec.confidence < 70) ++lowCount;
    }

    if (counted == 0) {
        m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
        m_lblLowWords->setText(tr("LOW-CONFIDENCE WORDS —"));
        return;
    }

    const double avgConf = totalConf / counted;

    m_lblAvgConf->setText(
        tr("AVG CONFIDENCE %1%").arg(static_cast<int>(std::round(avgConf))));
    m_lblLowWords->setText(
        tr("LOW-CONFIDENCE WORDS %1").arg(lowCount));
}

// ── setSemanticDocument — Djot-aware review UI ────────────────────────────────

namespace {

/// Escape HTML special characters for inline display.
static QString htmlEsc(const std::string& s) {
    return QString::fromStdString(s).toHtmlEscaped();
}

/// Walk a Block and produce simple inline-styled HTML.
static void blockToHtml(const docmodel::Block& block, std::ostringstream& html)
{
    using BT = docmodel::Block::Type;

    // Helper lambda: render a vector of Inline nodes to HTML
    auto inlinesToHtml = [](const std::vector<std::shared_ptr<docmodel::Inline>>& inlines,
                            std::ostringstream& out) {
        for (const auto& inl : inlines) {
            if (!inl) continue;
            switch (inl->getType()) {
            case docmodel::Inline::Type::Text:
                out << htmlEsc(inl->getText()).toStdString();
                break;
            case docmodel::Inline::Type::Strong:
                out << "<b>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</b>";
                break;
            case docmodel::Inline::Type::Emph:
                out << "<i>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</i>";
                break;
            case docmodel::Inline::Type::Code:
                out << "<code style='font-family:monospace;background:#e0e0e0;padding:1px 3px;'>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</code>";
                break;
            }
        }
    };

    switch (block.getType()) {
    case BT::Heading:
        html << "<h1 style='font-size:18px;font-weight:600;margin:8px 0 4px;color:#1a1a1a;'>";
        inlinesToHtml(block.getInlines(), html);
        html << "</h1>";
        break;
    case BT::Paragraph:
        html << "<p style='margin:4px 0;color:#1a1a1a;'>";
        inlinesToHtml(block.getInlines(), html);
        html << "</p>";
        break;
    case BT::Figure:
        html << "<p style='margin:4px 0;color:#555;font-style:italic;'>"
             << "<span style='background:#dbeafe;padding:1px 4px;border-radius:2px;'>"
             << "Figure: </span> ";
        inlinesToHtml(block.getInlines(), html);
        html << "</p>";
        break;
    case BT::List:
        html << "<ul style='margin:4px 0 4px 20px;color:#1a1a1a;'>";
        for (const auto& item : block.getBlocks()) {
            if (!item) continue;
            html << "<li>";
            inlinesToHtml(item->getInlines(), html);
            html << "</li>";
        }
        html << "</ul>";
        break;
    case BT::Table: {
        html << "<table style='border-collapse:collapse;margin:6px 0;width:100%;'>";
        bool firstRow = true;
        for (const auto& row : block.getBlocks()) {
            if (!row) continue;
            html << "<tr>";
            const auto& cells = row->getBlocks();
            if (cells.empty()) {
                // Row with inline content
                const char* cellTag = firstRow ? "th" : "td";
                const char* cellStyle = firstRow
                    ? "border:1px solid #999;padding:3px 6px;background:#e8e8e8;font-weight:600;"
                    : "border:1px solid #ccc;padding:3px 6px;";
                html << "<" << cellTag << " style='" << cellStyle << "'>";
                inlinesToHtml(row->getInlines(), html);
                html << "</" << cellTag << ">";
            } else {
                for (const auto& cell : cells) {
                    if (!cell) continue;
                    const char* cellTag = firstRow ? "th" : "td";
                    const char* cellStyle = firstRow
                        ? "border:1px solid #999;padding:3px 6px;background:#e8e8e8;font-weight:600;"
                        : "border:1px solid #ccc;padding:3px 6px;";
                    html << "<" << cellTag << " style='" << cellStyle << "'>";
                    inlinesToHtml(cell->getInlines(), html);
                    html << "</" << cellTag << ">";
                }
            }
            html << "</tr>";
            firstRow = false;
        }
        html << "</table>";
        break;
    }
    case BT::CodeBlock:
        html << "<pre style='background:#f5f5f5;padding:6px;font-family:monospace;"
                "font-size:11px;margin:4px 0;overflow:auto;'>";
        for (const auto& child : block.getBlocks()) {
            if (!child) continue;
            inlinesToHtml(child->getInlines(), html);
            html << "\n";
        }
        html << "</pre>";
        break;
    case BT::ListItem:
        // Standalone ListItem (shouldn't normally appear outside a List)
        inlinesToHtml(block.getInlines(), html);
        break;
    }
}

/// Walk a SemanticDocument and produce inline-styled HTML preview.
static QString semanticDocToHtml(const docmodel::SemanticDocument& doc)
{
    std::ostringstream html;
    html << "<html><body style='font-family:Manrope,sans-serif;font-size:13px;"
            "background:#f4f1ea;color:#1a1a1a;padding:16px;'>";

    for (const auto& section : doc.getSections()) {
        if (!section) continue;

        // Page header (section-level separator if not the first section)
        if (!section->getTitle().empty()) {
            html << "<h2 style='font-size:14px;font-weight:700;margin:12px 0 4px;"
                    "color:#666;border-top:1px solid #ccc;padding-top:8px;'>"
                 << htmlEsc(section->getTitle()).toStdString()
                 << "</h2>";
        }

        for (const auto& block : section->getBlocks()) {
            if (block) blockToHtml(*block, html);
        }
    }

    html << "</body></html>";
    return QString::fromStdString(html.str());
}

} // anonymous namespace

void OCRMode::setSemanticDocument(const docmodel::SemanticDocument &doc,
                                  const QString &djotLibPath)
{
    // R08: this Djot review path delivers no word records — clear any stale
    // reviewed records so they can never be pulled into an export.
    m_reviewWords.clear();
    m_selectedWordId = -1;
    updateWordInspector();

    // 1. Render SemanticDocument → inline-styled HTML for the scan pane
    if (m_scanContentLabel) {
        const QString html = semanticDocToHtml(doc);
        m_scanContentLabel->setText(html);
    }

    // 2. Populate the Djot text editor for Djot-aware edit-in-place.
    //    LuaDjotCodec::documentToDjot uses the C++ emitter (no Lua required for encode).
    //    The djotLibPath is only needed for djotToDocument (the decode stub) — not here.
    if (m_textEdit) {
        pdfws::LuaDjotCodec codec(djotLibPath.toStdString());
        try {
            std::string djotText = codec.documentToDjot(doc);
            m_textEdit->setPlainText(QString::fromStdString(djotText));
        } catch (const std::exception& e) {
            m_textEdit->setPlainText(
                tr("[Djot encode error: %1]").arg(QString::fromLatin1(e.what())));
        }
    }

    // 3. Enable the review buttons (accept/reject) — per-region workflow.
    //    R07: keep the explicit lifecycle state truthful for this path too.
    if (m_btnAccept) m_btnAccept->setEnabled(true);
    if (m_btnReject) m_btnReject->setEnabled(true);
    m_reviewState = ReviewState::ReviewReady;
    m_lastLifecycleMessage = QString();
    emit reviewStateChanged(m_reviewState);
}

} // namespace gp
