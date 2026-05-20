#include "OCRMode.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

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

    row->addWidget(monoLab("OCR"));

    // ── Language selector ───────────────────────────────────────────────
    m_langCombo = new QComboBox;
    m_langCombo->setObjectName("ocrLangCombo");
    m_langCombo->addItems({"EN · English", "DE · Deutsch", "FR · Français",
                           "ES · Español", "IT · Italiano", "PT · Português",
                           "RU · Русский", "ZH · 中文 (简)", "JA · 日本語",
                           "KO · 한국어", "AR · العربية", "NL · Nederlands"});
    m_langCombo->setProperty("variant", "ghost");
    row->addWidget(m_langCombo);

    // ── Engine selector ─────────────────────────────────────────────────
    m_engineCombo = new QComboBox;
    m_engineCombo->setObjectName("ocrEngineCombo");
    m_engineCombo->addItem("Tesseract 5");
#ifdef HAS_RAPIDOCR
    m_engineCombo->addItem("RapidOCR (PP-OCRv5)");
#endif
    m_engineCombo->setProperty("variant", "ghost");
    row->addWidget(m_engineCombo);

    // ── Strategy selector ───────────────────────────────────────────────
    m_strategyCombo = new QComboBox;
    m_strategyCombo->setObjectName("ocrStrategyCombo");
    m_strategyCombo->addItem("Primary Only");
    m_strategyCombo->addItem("Confidence Weighted");
    m_strategyCombo->addItem("ROVER Vote");
    m_strategyCombo->setProperty("variant", "ghost");
    row->addWidget(m_strategyCombo);

    // ── Preprocessing toggles ───────────────────────────────────────────
    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedWidth(1); sep1->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep1);

    m_chkDeskew = new QCheckBox("Deskew");
    m_chkDeskew->setObjectName("ocrChkDeskew");
    m_chkDeskew->setChecked(true);
    m_chkDeskew->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDeskew);

    m_chkBinarize = new QCheckBox("Binarize");
    m_chkBinarize->setObjectName("ocrChkBinarize");
    m_chkBinarize->setChecked(true);
    m_chkBinarize->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkBinarize);

    m_chkDenoise = new QCheckBox("Denoise");
    m_chkDenoise->setObjectName("ocrChkDenoise");
    m_chkDenoise->setChecked(false);
    m_chkDenoise->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDenoise);

    row->addStretch(1);

    // ── Run / Review actions ────────────────────────────────────────────
    m_btnRun = new QToolButton;
    m_btnRun->setObjectName("ocrBtnRun");
    m_btnRun->setText("Run OCR");
    m_btnRun->setProperty("variant", "accent");
    connect(m_btnRun, &QToolButton::clicked, this, &OCRMode::onRunOcr);
    row->addWidget(m_btnRun);

    m_btnAccept = new QToolButton;
    m_btnAccept->setObjectName("ocrBtnAccept");
    m_btnAccept->setText("✓ Accept");
    m_btnAccept->setProperty("variant", "ghost");
    m_btnAccept->setEnabled(false);
    connect(m_btnAccept, &QToolButton::clicked, this, &OCRMode::onAcceptResults);
    row->addWidget(m_btnAccept);

    m_btnReject = new QToolButton;
    m_btnReject->setObjectName("ocrBtnReject");
    m_btnReject->setText("✗ Reject");
    m_btnReject->setProperty("variant", "ghost");
    m_btnReject->setEnabled(false);
    connect(m_btnReject, &QToolButton::clicked, this, &OCRMode::reviewRejected);
    row->addWidget(m_btnReject);

    auto* sep2 = new QFrame; sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1); sep2->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep2);

    auto* exit = new QToolButton;
    exit->setText("Exit OCR");
    exit->setProperty("variant", "ghost");
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

    m_lblPage    = infoLab("PAGE 01 OF 12");
    m_lblAvgConf = infoLab("AVG CONFIDENCE —");
    m_lblLowWords= infoLab("LOW-CONFIDENCE WORDS —");
    m_lblEngine  = infoLab("ENGINE: Tesseract 5");

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
    impHeadRow->addWidget(monoLab("IMAGE · SCAN"));
    impHeadRow->addStretch(1);
    impHeadRow->addWidget(monoLab("4× PIXELS"));
    impLay->addWidget(impHead);

    auto* scanArea = new QFrame;
    auto* scanLay = new QVBoxLayout(scanArea);
    scanLay->setContentsMargins(20,20,20,20);

    auto* scanPaper = new QFrame;
    scanPaper->setStyleSheet("background:#f4f1ea; border:1px solid #000; min-width:380px;");
    auto* scanInner = new QVBoxLayout(scanPaper);
    scanInner->setContentsMargins(24,24,24,24);

    auto* sw1 = new QLabel("<b style='color:#1a1a1a;font-size:18px'>Performance Overview</b>");
    sw1->setStyleSheet("color:#1a1a1a;"); sw1->setTextFormat(Qt::RichText);

    // Confidence-colored word overlays:
    //   Green (#22c55e) = high (≥80), Yellow (#eab308) = medium (50-79), Red (#ef4444) = low (<50)
    auto* sw2 = new QLabel(
        "Consolidated revenue reached "
        "<span style='background:#22c55e33;padding:1px;outline:1px solid #22c55e99'>$2,418M</span>"
        ", an increase of "
        "<span style='background:#22c55e33;outline:1px solid #22c55e99'>14.2%</span>"
        " year-over-year, driven by "
        "<span style='background:#eab30833;outline:1px solid #eab30899'>Industrial Automation</span>"
        " demand.");
    sw2->setWordWrap(true); sw2->setStyleSheet("color:#1a1a1a;"); sw2->setTextFormat(Qt::RichText);

    auto* sw3 = new QLabel(
        "Operating margin expanded by "
        "<span style='background:#ef444433;outline:1px solid #ef444499'>180bps</span>"
        " to "
        "<span style='background:#eab30833;outline:1px solid #eab30899'>22.4%</span>"
        ".");
    sw3->setWordWrap(true); sw3->setStyleSheet("color:#1a1a1a;"); sw3->setTextFormat(Qt::RichText);

    scanInner->addWidget(sw1);
    scanInner->addSpacing(10);
    scanInner->addWidget(sw2);
    scanInner->addSpacing(6);
    scanInner->addWidget(sw3);
    scanInner->addStretch(1);

    scanLay->addWidget(scanPaper);
    scanLay->addStretch(1);
    impLay->addWidget(scanArea, 1);
    split->addWidget(m_imagePane);

    // ── Text pane (editable) ────────────────────────────────────────────
    auto* textPane = new QFrame;
    auto* txtLay = new QVBoxLayout(textPane);
    txtLay->setContentsMargins(0,0,0,0); txtLay->setSpacing(0);

    auto* txtHead = makeStrip("modeToolbar", 24);
    auto* txtHeadRow = new QHBoxLayout(txtHead);
    txtHeadRow->setContentsMargins(12,0,12,0);
    txtHeadRow->addWidget(monoLab("RECOGNIZED · EDITABLE"));
    txtHeadRow->addStretch(1);
    txtHeadRow->addWidget(monoLab("UTF-8"));
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
    zHeadRow->addWidget(monoLab("ZOOM · 4×"));
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

    legendLay->addWidget(monoLab("CONFIDENCE"));

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

    makeLegendRow("#22c55e", "HIGH (≥ 80%)");
    makeLegendRow("#eab308", "MEDIUM (50-79%)");
    makeLegendRow("#ef4444", "LOW (< 50%)");

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

    // Update info strip from selected controls
    m_lblEngine->setText(QString("ENGINE: %1 · %2")
        .arg(m_engineCombo->currentText(),
             m_strategyCombo->currentText()));
    m_lblAvgConf->setText("AVG CONFIDENCE 87.3%");
    m_lblLowWords->setText("14 LOW-CONFIDENCE WORDS");

    emit ocrRequested();
}

void OCRMode::onAcceptResults()
{
    m_btnAccept->setEnabled(false);
    m_btnReject->setEnabled(false);
    emit reviewAccepted();
}

void OCRMode::updateConfidenceOverlay()
{
    // In a real implementation this would iterate MergedOcrWord results
    // and paint colored rectangles over the image pane at each bounding box.
    // Green ≥80, Yellow 50-79, Red <50.
}

} // namespace gp
