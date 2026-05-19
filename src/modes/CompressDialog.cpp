#include "CompressDialog.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

static QToolButton* presetCard(const QString& name, const QString& spec,
                               const QString& desc, bool active) {
    auto* b = new QToolButton;
    b->setText(QString("%1\n\n%2\n%3").arg(name, spec, desc));
    b->setCheckable(true);
    b->setChecked(active);
    b->setMinimumHeight(110);
    b->setStyleSheet(active ?
        "QToolButton{background:rgba(255,140,66,0.13);border:1px solid #ff8c42;color:#dfe1e5;text-align:left;padding:14px;}" :
        "QToolButton{background:#1a1b1e;border:1px solid #4a4d52;color:#dfe1e5;text-align:left;padding:14px;}"
        "QToolButton:hover{border-color:#71747a;}");
    return b;
}

CompressDialog::CompressDialog(QWidget* parent) : QDialog(parent) {
    setProperty("role", "modal");
    setWindowTitle("Compress");
    setFixedSize(540, 500);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    auto* head = new QFrame; head->setProperty("role","modalHead");
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(12,0,12,0);
    auto* t = new QLabel("OPTIMIZE DOCUMENT"); t->setStyleSheet("font-weight:600;letter-spacing:1px;");
    auto* sub = new QLabel("· Annual-Report-2025.pdf"); sub->setProperty("mono",true);
    hr->addWidget(t); hr->addWidget(sub); hr->addStretch(1);
    col->addWidget(head);

    auto* body = new QFrame;
    auto* bl = new QVBoxLayout(body); bl->setContentsMargins(16,14,16,14); bl->setSpacing(12);

    auto* presetGroup = new QButtonGroup(this);
    presetGroup->setExclusive(true);
    auto* grid = new QGridLayout; grid->setSpacing(8);
    auto* pScreen   = presetCard("SCREEN",   "72 DPI · ~2.1 MB · ↓82%",  "On-screen viewing only", false);
    auto* pEbook    = presetCard("EBOOK",    "150 DPI · ~5.8 MB · ↓52%", "E-readers and email",    true);
    auto* pPrinter  = presetCard("PRINTER",  "300 DPI · ~8.4 MB · ↓31%", "Desktop printing",       false);
    auto* pPrepress = presetCard("PREPRESS", "300 DPI + ICC · ~9.1 MB", "Professional print",     false);
    presetGroup->addButton(pScreen,   0);
    presetGroup->addButton(pEbook,    1);
    presetGroup->addButton(pPrinter,  2);
    presetGroup->addButton(pPrepress, 3);
    grid->addWidget(pScreen,   0, 0);
    grid->addWidget(pEbook,    0, 1);
    grid->addWidget(pPrinter,  1, 0);
    grid->addWidget(pPrepress, 1, 1);
    bl->addLayout(grid);

    auto* sizeRow = new QFrame;
    sizeRow->setStyleSheet("background:#1a1b1e; border:1px solid #393b40; padding:12px;");
    auto* sl = new QVBoxLayout(sizeRow);

    // Original (full bar, muted)
    auto* origRow = new QHBoxLayout;
    auto* origLab = new QLabel("ORIGINAL"); origLab->setProperty("mono", true); origLab->setFixedWidth(70);
    origRow->addWidget(origLab);
    auto* origBar = new QProgressBar;
    origBar->setRange(0, 100);
    origBar->setValue(100);
    origBar->setTextVisible(false);
    origBar->setFixedHeight(10);
    origBar->setStyleSheet("QProgressBar{background:#34363b;border:1px solid #4a4d52;}"
                           "QProgressBar::chunk{background:#71747a;}");
    origRow->addWidget(origBar, 1);
    auto* origVal = new QLabel("12.1 MB"); origVal->setProperty("mono", true); origVal->setFixedWidth(70); origVal->setAlignment(Qt::AlignRight);
    origRow->addWidget(origVal);
    sl->addLayout(origRow);

    // Estimated (~48%, accent)
    auto* estRow = new QHBoxLayout;
    auto* estLab = new QLabel("ESTIMATED"); estLab->setProperty("mono", true); estLab->setFixedWidth(70);
    estRow->addWidget(estLab);
    auto* estBar = new QProgressBar;
    estBar->setRange(0, 100);
    estBar->setValue(48);
    estBar->setTextVisible(false);
    estBar->setFixedHeight(10);
    estBar->setStyleSheet("QProgressBar{background:#34363b;border:1px solid #4a4d52;}"
                          "QProgressBar::chunk{background:#ff8c42;}");
    estRow->addWidget(estBar, 1);
    auto* estVal = new QLabel("5.8 MB"); estVal->setProperty("mono", true); estVal->setStyleSheet("color:#ff8c42;"); estVal->setFixedWidth(70); estVal->setAlignment(Qt::AlignRight);
    estRow->addWidget(estVal);
    sl->addLayout(estRow);

    auto* badge = new QHBoxLayout;
    badge->addWidget(new Badge("↓ 52% REDUCTION", Badge::Warn));
    badge->addStretch(1);
    sl->addLayout(badge);
    bl->addWidget(sizeRow);
    bl->addStretch(1);
    col->addWidget(body, 1);

    auto* foot = new QFrame; foot->setProperty("role","modalFoot");
    auto* fr = new QHBoxLayout(foot); fr->setContentsMargins(12,8,12,8);
    fr->addStretch(1);
    auto* cancel = new QPushButton("Cancel"); cancel->setFlat(true);
    auto* go = new QPushButton("COMPRESS DOCUMENT");
    go->setStyleSheet("background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;font-weight:700;padding:8px 20px;letter-spacing:0.6px;");
    fr->addWidget(cancel); fr->addWidget(go);
    col->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(go,     &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace gp
