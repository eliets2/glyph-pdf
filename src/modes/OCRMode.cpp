#include "OCRMode.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

static QFrame* makeStrip(const char* role, int h) {
    auto* f = new QFrame;
    f->setProperty("role", role);
    f->setFixedHeight(h);
    return f;
}

OCRMode::OCRMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // --- toolbar
    auto* tb = makeStrip("modeToolbar", Theme::ToolbarH);
    auto* tbRow = new QHBoxLayout(tb);
    tbRow->setContentsMargins(10, 0, 10, 0);
    tbRow->setSpacing(6);
    auto monoLab = [](const QString& s) { auto* l = new QLabel(s); l->setProperty("mono", true); return l; };
    tbRow->addWidget(monoLab("OCR"));
    auto* lang = new QToolButton; lang->setText("EN · English ▾"); lang->setProperty("variant","ghost"); tbRow->addWidget(lang);
    auto* eng  = new QToolButton; eng->setText("Tesseract 5 ▾");   eng->setProperty("variant","ghost"); tbRow->addWidget(eng);
    auto* rng  = new QToolButton; rng->setText("All Pages ▾");     rng->setProperty("variant","ghost"); tbRow->addWidget(rng);
    auto* run  = new QToolButton; run->setText("Run OCR");          run->setProperty("variant","accent"); tbRow->addWidget(run);
    auto* verify = new QToolButton; verify->setText("Verify Text…"); verify->setProperty("variant","ghost"); tbRow->addWidget(verify);
    tbRow->addStretch(1);
    auto* exit = new QToolButton; exit->setText("Exit OCR"); exit->setProperty("variant","ghost"); tbRow->addWidget(exit);
    col->addWidget(tb);

    // --- info strip
    auto* info = makeStrip("infoStrip", Theme::InfoStripH);
    auto* infoRow = new QHBoxLayout(info);
    infoRow->setContentsMargins(12, 0, 12, 0);
    infoRow->setSpacing(14);
    auto info1 = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("role","infoStrip"); return l; };
    infoRow->addWidget(info1("PAGE 04 OF 12"));
    infoRow->addWidget(info1("AVG CONFIDENCE 87.3%"));
    infoRow->addWidget(info1("14 LOW-CONFIDENCE WORDS"));
    infoRow->addWidget(info1("2 UNCERTAIN"));
    infoRow->addStretch(1);
    col->addWidget(info);

    // --- 4-pane splitter
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    auto* pages = new QListWidget;
    pages->setFixedWidth(180);
    for (int i = 1; i <= 12; ++i)
        pages->addItem(QString("Page %1   ●").arg(i, 3, 10, QChar('0')));
    split->addWidget(pages);

    auto* imagePane = new QFrame;
    auto* impLay = new QVBoxLayout(imagePane);
    impLay->setContentsMargins(0,0,0,0); impLay->setSpacing(0);
    auto* impHead = makeStrip("modeToolbar", 24);
    auto* impHeadRow = new QHBoxLayout(impHead); impHeadRow->setContentsMargins(12,0,12,0);
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
    auto* sw1 = new QLabel("<b style='color:#1a1a1a;font-size:18px'>Performance Overview</b>"); sw1->setStyleSheet("color:#1a1a1a;");
    auto* sw2 = new QLabel("Consolidated revenue reached <span style='background:#ff8c4222;padding:1px;outline:1px dashed #ff8c42'>$2,418M</span>, an increase of 14.2% year-over-year, driven by <span style='background:#ff8c4218;outline:1px solid #ff8c4288'>Industrial Automation</span> demand.");
    sw2->setWordWrap(true); sw2->setStyleSheet("color:#1a1a1a;");
    auto* sw3 = new QLabel("Operating margin expanded by <span style='background:#c8442b22;outline:1px solid #c8442b99'>180bps</span> to <span style='background:#ff8c4218;outline:1px solid #ff8c4288'>22.4%</span>.");
    sw3->setWordWrap(true); sw3->setStyleSheet("color:#1a1a1a;");
    sw1->setTextFormat(Qt::RichText);
    sw2->setTextFormat(Qt::RichText);
    sw3->setTextFormat(Qt::RichText);
    scanInner->addWidget(sw1);
    scanInner->addSpacing(10);
    scanInner->addWidget(sw2);
    scanInner->addSpacing(6);
    scanInner->addWidget(sw3);
    scanInner->addStretch(1);
    scanLay->addWidget(scanPaper);
    scanLay->addStretch(1);
    impLay->addWidget(scanArea, 1);
    split->addWidget(imagePane);

    auto* textPane = new QFrame;
    auto* txtLay = new QVBoxLayout(textPane); txtLay->setContentsMargins(0,0,0,0); txtLay->setSpacing(0);
    auto* txtHead = makeStrip("modeToolbar", 24);
    auto* txtHeadRow = new QHBoxLayout(txtHead); txtHeadRow->setContentsMargins(12,0,12,0);
    txtHeadRow->addWidget(monoLab("RECOGNIZED · EDITABLE"));
    txtHeadRow->addStretch(1);
    txtHeadRow->addWidget(monoLab("UTF-8"));
    txtLay->addWidget(txtHead);
    auto* txt = new QPlainTextEdit;
    txt->setPlainText("Performance Overview\n\nConsolidated revenue reached $2,418M, an increase of 14.2% year-over-year, driven by Industrial Automation demand.\n\nOperating margin expanded by 180bps to 22.4%.");
    txtLay->addWidget(txt, 1);
    split->addWidget(textPane);

    auto* zoomPane = new QFrame;
    zoomPane->setFixedWidth(200);
    auto* zLay = new QVBoxLayout(zoomPane); zLay->setContentsMargins(0,0,0,0); zLay->setSpacing(0);
    auto* zHead = makeStrip("modeToolbar", 24);
    auto* zHeadRow = new QHBoxLayout(zHead); zHeadRow->setContentsMargins(12,0,12,0);
    zHeadRow->addWidget(monoLab("ZOOM · 4×")); zHeadRow->addStretch(1);
    zLay->addWidget(zHead);
    auto* zBig = new QLabel("$2,418M");
    zBig->setAlignment(Qt::AlignCenter);
    zBig->setStyleSheet("background:#e8e6df; color:#1a1a1a; font-family:Manrope; font-size:42px; font-weight:600; padding:16px; margin:24px 16px; border:1px solid #000;");
    zLay->addWidget(zBig);
    auto* zMeta = new QLabel("WORD #13\nCONFIDENCE 64%\nBBOX 318, 442 · 84×22\nBASELINE 462 pt");
    zMeta->setProperty("mono", true);
    zMeta->setStyleSheet("padding:8px 12px;");
    zMeta->setAlignment(Qt::AlignLeft);
    zLay->addWidget(zMeta);
    zLay->addStretch(1);
    split->addWidget(zoomPane);

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    split->setStretchFactor(3, 0);
    col->addWidget(split, 1);
}

} // namespace gp
