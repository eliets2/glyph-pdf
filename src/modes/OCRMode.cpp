// SPDX-License-Identifier: Apache-2.0
#include "OCRMode.h"
#include "engines/ocr/RapidOcrEngine.h"
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
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gp {

// Shared QSettings key for the user's selected OCR language code (e.g. "EN").
// StatusBar reads the same key to display the real selected language.
static const char* kOcrLanguageKey = "ocr/language";

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
    m_langCombo->addItems({"EN · English", "DE · Deutsch", "FR · Français",
                           "ES · Español", "IT · Italiano", "PT · Português",
                           "RU · Русский", "ZH · 中文 (简)", "JA · 日本語",
                           "KO · 한국어", "AR · العربية", "NL · Nederlands"});
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

    m_chkDeskew = new QCheckBox(tr("Deskew"));
    m_chkDeskew->setObjectName("ocrChkDeskew");
    m_chkDeskew->setChecked(true);
    m_chkDeskew->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDeskew);

    m_chkBinarize = new QCheckBox(tr("Binarize"));
    m_chkBinarize->setObjectName("ocrChkBinarize");
    m_chkBinarize->setChecked(true);
    m_chkBinarize->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkBinarize);

    m_chkDenoise = new QCheckBox(tr("Denoise"));
    m_chkDenoise->setObjectName("ocrChkDenoise");
    m_chkDenoise->setChecked(false);
    m_chkDenoise->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDenoise);

    row->addStretch(1);

    // ── Run / Review actions ────────────────────────────────────────────
    m_btnRun = new QToolButton;
    m_btnRun->setObjectName("ocrBtnRun");
    m_btnRun->setText(tr("Run OCR"));
    m_btnRun->setProperty("variant", "accent");
    connect(m_btnRun, &QToolButton::clicked, this, &OCRMode::onRunOcr);
    row->addWidget(m_btnRun);

    m_btnAccept = new QToolButton;
    m_btnAccept->setObjectName("ocrBtnAccept");
    m_btnAccept->setText(tr("✓ Accept"));
    m_btnAccept->setProperty("variant", "ghost");
    m_btnAccept->setEnabled(false);
    connect(m_btnAccept, &QToolButton::clicked, this, &OCRMode::onAcceptResults);
    row->addWidget(m_btnAccept);

    m_btnReject = new QToolButton;
    m_btnReject->setObjectName("ocrBtnReject");
    m_btnReject->setText(tr("✗ Reject"));
    m_btnReject->setProperty("variant", "ghost");
    m_btnReject->setEnabled(false);
    connect(m_btnReject, &QToolButton::clicked, this, &OCRMode::reviewRejected);
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

    m_lblPage    = infoLab(tr("PAGE 01 OF 12"));
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
    m_pageList = new QListWidget;
    m_pageList->setObjectName("ocrPageList");
    m_pageList->setFixedWidth(180);
    for (int i = 1; i <= 12; ++i)
        m_pageList->addItem(QString("Page %1   ●").arg(i, 3, 10, QChar('0')));
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

    // Seed with static demo content until real OCR runs
    m_scanContentLabel->setText(
        "<b style='font-size:18px'>Performance Overview</b><br><br>"
        "Consolidated revenue reached "
        "<span style='background:#22c55e33;outline:1px solid #22c55e99'>$2,418M</span>"
        ", an increase of "
        "<span style='background:#22c55e33;outline:1px solid #22c55e99'>14.2%</span>"
        " year-over-year, driven by "
        "<span style='background:#eab30833;outline:1px solid #eab30899'>Industrial Automation</span>"
        " demand.<br><br>"
        "Operating margin expanded by "
        "<span style='background:#ef444433;outline:1px solid #ef444499'>180bps</span>"
        " to "
        "<span style='background:#eab30833;outline:1px solid #eab30899'>22.4%</span>"
        ".");

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidget(m_scanContentLabel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background:#2a2a2a;");

    impLay->addWidget(scrollArea, 1);
    split->addWidget(m_imagePane);

    // ── Text pane (editable) ────────────────────────────────────────────
    auto* textPane = new QFrame;
    auto* txtLay = new QVBoxLayout(textPane);
    txtLay->setContentsMargins(0,0,0,0); txtLay->setSpacing(0);

    auto* txtHead = makeStrip("modeToolbar", 24);
    auto* txtHeadRow = new QHBoxLayout(txtHead);
    txtHeadRow->setContentsMargins(12,0,12,0);
    txtHeadRow->addWidget(monoLab(tr("RECOGNIZED · EDITABLE")));
    txtHeadRow->addStretch(1);
    txtHeadRow->addWidget(monoLab(tr("UTF-8")));
    txtLay->addWidget(txtHead);

    m_textEdit = new QPlainTextEdit;
    m_textEdit->setObjectName("ocrTextEdit");
    m_textEdit->setPlainText(
        "Performance Overview\n\n"
        "Consolidated revenue reached $2,418M, an increase of 14.2% year-over-year, "
        "driven by Industrial Automation demand.\n\n"
        "Operating margin expanded by 180bps to 22.4%.");
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

    m_zoomBig = new QLabel("$2,418M");
    m_zoomBig->setAlignment(Qt::AlignCenter);
    m_zoomBig->setStyleSheet(
        "background:#e8e6df; color:#1a1a1a; font-family:Manrope; "
        "font-size:42px; font-weight:600; padding:16px; margin:24px 16px; "
        "border:1px solid #000;");
    zLay->addWidget(m_zoomBig);

    m_zoomMeta = new QLabel(
        "WORD #13\nCONFIDENCE 64%\nBBOX 318, 442 · 84×22\nBASELINE 462 pt\nSOURCE: ROVER");
    m_zoomMeta->setProperty("mono", true);
    m_zoomMeta->setStyleSheet("padding:8px 12px;");
    m_zoomMeta->setAlignment(Qt::AlignLeft);
    zLay->addWidget(m_zoomMeta);

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

void OCRMode::onRunOcr()
{
    // Enable review buttons after OCR run
    m_btnAccept->setEnabled(true);
    m_btnReject->setEnabled(true);

    // Update engine label
    m_lblEngine->setText(tr("ENGINE: %1 · %2")
        .arg(m_engineCombo->currentText(),
             m_strategyCombo->currentText()));

    // Confidence stats will be updated by setOcrResults() when results arrive.
    // If no results set yet, clear the stats to avoid stale values.
    if (m_currentWords.isEmpty()) {
        m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
        m_lblLowWords->setText(tr("LOW-CONFIDENCE WORDS —"));
    }

    emit ocrRequested();
}

void OCRMode::onAcceptResults()
{
    m_btnAccept->setEnabled(false);
    m_btnReject->setEnabled(false);
    emit reviewAccepted();
}

// ── Context menu (right-click on scan pane) ──────────────────────────────────

void OCRMode::onImagePaneContextMenu(const QPoint &pos)
{
    // For the rich-text label, we use a fixed "current page" region
    // as the re-OCR target.  Future work: map pos to individual LayoutRegion bboxes.
    m_contextRegionBbox = QRectF();  // empty = whole current page

    QMenu menu(this);

    QAction *reOcrAction = menu.addAction(tr("Re-OCR this region"));
    connect(reOcrAction, &QAction::triggered, this, &OCRMode::onReOcrRegion);

    menu.addSeparator();

    // Per-region accept / reject workflow
    QAction *acceptRegion = menu.addAction(tr("Accept this region"));
    connect(acceptRegion, &QAction::triggered, this, [this]() {
        // Accept: mark region as reviewed (future: remove yellow/red overlay for this region)
        // For now: enable accept/reject buttons so user can confirm the full page
        m_btnAccept->setEnabled(true);
        m_btnReject->setEnabled(true);
    });

    QAction *rejectRegion = menu.addAction(tr("Reject this region"));
    connect(rejectRegion, &QAction::triggered, this, [this]() {
        // Reject: mark region as needing re-work
        m_btnReject->setEnabled(true);
        emit reviewRejected();
    });

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
    updateConfidenceOverlay();
    updateInfoStrip();

    // Populate plain-text editor with the recognized text
    if (m_textEdit) {
        QStringList lines;
        for (const auto &w : words)
            lines.append(w.text);
        m_textEdit->setPlainText(lines.join(QStringLiteral(" ")));
    }

    // Enable review buttons
    if (m_btnAccept) m_btnAccept->setEnabled(true);
    if (m_btnReject) m_btnReject->setEnabled(true);
}

// ── updateConfidenceOverlay ───────────────────────────────────────────────────

void OCRMode::updateConfidenceOverlay()
{
    if (!m_scanContentLabel) return;

    if (m_currentWords.isEmpty()) {
        // No results yet — keep the demo content
        return;
    }

    // Build a rich-text paragraph with per-word confidence coloring.
    // Thresholds per M5-P2 D6 spec:
    //   green (#22c55e): confidence ≥ 90
    //   yellow (#eab308): confidence 70–89
    //   red (#ef4444): confidence < 70
    QString html;
    html.reserve(m_currentWords.size() * 80);

    for (const auto &w : m_currentWords) {
        const int conf = w.confidence;
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

        // Escape HTML special chars in the word text
        QString escaped = w.text.toHtmlEscaped();

        html += QStringLiteral(
            "<span style='background:%1;outline:1px solid %2;padding:1px;margin:1px;' "
            "title='%3% | %4 | %5'>%6</span> ")
            .arg(bgColor, borderColor)
            .arg(conf)
            .arg(w.sourceEngine)
            .arg(w.boundingBox.x(), 0, 'f', 0)
            .arg(escaped);
    }

    m_scanContentLabel->setText(html);
}

// ── updateInfoStrip ───────────────────────────────────────────────────────────

void OCRMode::updateInfoStrip()
{
    if (m_currentWords.isEmpty()) {
        m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
        m_lblLowWords->setText(tr("LOW-CONFIDENCE WORDS —"));
        return;
    }

    double totalConf = 0.0;
    int lowCount     = 0;
    for (const auto &w : m_currentWords) {
        totalConf += w.confidence;
        if (w.confidence < 70) ++lowCount;
    }
    const double avgConf = totalConf / m_currentWords.size();

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

    // 3. Enable the review buttons (accept/reject) — per-region workflow
    if (m_btnAccept) m_btnAccept->setEnabled(true);
    if (m_btnReject) m_btnReject->setEnabled(true);
}

} // namespace gp
