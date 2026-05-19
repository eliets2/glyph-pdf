#include "WatermarkDialog.h"
#include "util/GpTheme.h"
#include "util/Slider.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

WatermarkDialog::WatermarkDialog(QWidget* parent) : QDialog(parent) {
    setProperty("role", "modal");
    setWindowTitle("Watermark");
    setFixedSize(560, 480);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    auto* head = new QFrame; head->setProperty("role","modalHead");
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(12,0,12,0);
    auto* t = new QLabel("ADD WATERMARK"); t->setStyleSheet("font-weight:600;letter-spacing:1px;");
    hr->addWidget(t); hr->addStretch(1);
    col->addWidget(head);

    auto* body = new QFrame;
    auto* br = new QHBoxLayout(body); br->setContentsMargins(0,0,0,0); br->setSpacing(0);

    // Preview
    auto* preview = new QFrame; preview->setFixedWidth(200);
    preview->setStyleSheet("background:#1a1b1e; border-right:1px solid #393b40;");
    auto* pl = new QVBoxLayout(preview); pl->setContentsMargins(20,20,20,20);
    auto* page = new QFrame;
    page->setStyleSheet("background:#f4f1ea; border:1px solid #000; min-height:200px;");
    auto* pli = new QVBoxLayout(page);
    pli->setAlignment(Qt::AlignCenter);
    auto* wm = new QLabel("CONFIDENTIAL");
    wm->setStyleSheet("color:rgba(60,60,60,0.4); font-family:Manrope; font-size:22px; font-weight:800; letter-spacing:1px;");
    wm->setAlignment(Qt::AlignCenter);
    pli->addWidget(wm);
    pl->addWidget(page);
    br->addWidget(preview);

    // Controls
    auto* ctrl = new QFrame;
    auto* cl = new QVBoxLayout(ctrl); cl->setContentsMargins(14,14,14,14); cl->setSpacing(8);
    auto* form = new QFormLayout;
    form->addRow("Text",     new QLineEdit("CONFIDENTIAL"));
    form->addRow("Size",     new QLineEdit("48pt"));
    form->addRow("Opacity",  new LabeledSlider(0, 100, 40, "%"));
    form->addRow("Rotation", new QLineEdit("315°"));
    cl->addLayout(form);

    cl->addWidget(new QLabel("POSITION"));
    auto* pos = new QGridLayout; pos->setSpacing(2);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
        auto* btn = new QToolButton; btn->setFixedSize(28, 24);
        btn->setCheckable(true); btn->setAutoExclusive(true);
        if (r == 1 && c == 1) btn->setChecked(true);
        btn->setProperty("variant", "pill");
        pos->addWidget(btn, r, c);
    }
    cl->addLayout(pos);

    auto* pageRange = new QHBoxLayout;
    pageRange->addWidget(new QRadioButton("All Pages"));
    pageRange->addWidget(new QRadioButton("Odd"));
    pageRange->addWidget(new QRadioButton("Even"));
    pageRange->addWidget(new QRadioButton("Custom"));
    cl->addLayout(pageRange);
    cl->addStretch(1);

    br->addWidget(ctrl, 1);
    col->addWidget(body, 1);

    auto* foot = new QFrame; foot->setProperty("role","modalFoot");
    auto* fr = new QHBoxLayout(foot); fr->setContentsMargins(12,8,12,8);
    fr->addStretch(1);
    auto* cancel = new QPushButton("Cancel");
    auto* apply = new QPushButton("APPLY WATERMARK");
    apply->setStyleSheet("background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;font-weight:700;padding:8px 20px;");
    fr->addWidget(cancel); fr->addWidget(apply);
    col->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply,  &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace gp
