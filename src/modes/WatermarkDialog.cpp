// SPDX-License-Identifier: Apache-2.0
#include "WatermarkDialog.h"
#include "util/GpTheme.h"
#include "util/Slider.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QButtonGroup>

namespace gp {

WatermarkDialog::WatermarkDialog(const AppContext* ctx, QWidget* parent)
    : QDialog(parent), _ctx(ctx)
{
    setProperty("role", "modal");
    setWindowTitle(tr("Watermark"));
    setFixedSize(580, 520);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Header ──
    auto* head = new QFrame;
    head->setProperty("role", "modalHead");
    auto* hr = new QHBoxLayout(head);
    hr->setContentsMargins(12, 0, 12, 0);
    auto* t = new QLabel(tr("ADD WATERMARK"));
    t->setStyleSheet("font-weight:600;letter-spacing:1px;");
    hr->addWidget(t);
    hr->addStretch(1);
    col->addWidget(head);

    // ── Body: preview left | controls right ──
    auto* body = new QFrame;
    auto* br = new QHBoxLayout(body);
    br->setContentsMargins(0, 0, 0, 0);
    br->setSpacing(0);

    // Preview pane
    auto* preview = new QFrame;
    preview->setFixedWidth(200);
    preview->setStyleSheet("background:#1a1b1e; border-right:1px solid #393b40;");
    auto* pl = new QVBoxLayout(preview);
    pl->setContentsMargins(20, 20, 20, 20);
    auto* page = new QFrame;
    page->setStyleSheet("background:#f4f1ea; border:1px solid #000; min-height:200px;");
    auto* pli = new QVBoxLayout(page);
    pli->setAlignment(Qt::AlignCenter);
    _previewLabel = new QLabel("CONFIDENTIAL");
    _previewLabel->setStyleSheet(
        "color:rgba(60,60,60,0.4); font-size:22px; font-weight:800; letter-spacing:1px;");
    _previewLabel->setAlignment(Qt::AlignCenter);
    pli->addWidget(_previewLabel);
    pl->addWidget(page);
    br->addWidget(preview);

    // Controls pane
    auto* ctrl = new QFrame;
    auto* cl = new QVBoxLayout(ctrl);
    cl->setContentsMargins(14, 10, 14, 10);
    cl->setSpacing(6);

    _tabs = new QTabWidget;
    auto* textTab = new QWidget;
    auto* imageTab = new QWidget;
    buildTextTab(textTab);
    buildImageTab(imageTab);
    _tabs->addTab(textTab, tr("Text"));
    _tabs->addTab(imageTab, tr("Image"));
    cl->addWidget(_tabs);

    // Position grid (shared by both tabs)
    cl->addWidget(new QLabel(tr("POSITION")));
    _posGroup = new QButtonGroup(this);
    _posGroup->setExclusive(true);
    auto* pos = new QGridLayout;
    pos->setSpacing(2);
    const QStringList labels = {"TL", "TC", "TR", "ML", "C", "MR", "BL", "BC", "BR"};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            auto* btn = new QToolButton;
            btn->setText(labels[r * 3 + c]);
            btn->setFixedSize(32, 26);
            btn->setCheckable(true);
            btn->setProperty("variant", "pill");
            int id = r * 3 + c;
            _posGroup->addButton(btn, id);
            pos->addWidget(btn, r, c);
            if (id == 4) btn->setChecked(true);
        }
    }
    cl->addLayout(pos);

    // Page range
    auto* rangeBox = new QWidget;
    buildPageRange(rangeBox);
    cl->addWidget(rangeBox);
    cl->addStretch(1);

    br->addWidget(ctrl, 1);
    col->addWidget(body, 1);

    // ── Footer ──
    auto* foot = new QFrame;
    foot->setProperty("role", "modalFoot");
    auto* fr = new QHBoxLayout(foot);
    fr->setContentsMargins(12, 8, 12, 8);
    fr->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* apply = new QPushButton(tr("APPLY WATERMARK"));
    apply->setStyleSheet(
        "background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;"
        "font-weight:700;padding:8px 20px;");
    fr->addWidget(cancel);
    fr->addWidget(apply);
    col->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply, &QPushButton::clicked, this, &WatermarkDialog::onApply);
    connect(_tabs, &QTabWidget::currentChanged, this, &WatermarkDialog::updatePreview);
    updatePreview();
}

void WatermarkDialog::buildTextTab(QWidget* tab) {
    auto* form = new QFormLayout(tab);
    form->setContentsMargins(4, 8, 4, 4);
    form->setSpacing(6);

    _textInput = new QLineEdit("CONFIDENTIAL");
    form->addRow(tr("Text:"), _textInput);
    connect(_textInput, &QLineEdit::textChanged, this, &WatermarkDialog::updatePreview);

    _fontCombo = new QComboBox;
    _fontCombo->addItems({"Helvetica", "Courier", "Times-Roman"});
    form->addRow(tr("Font:"), _fontCombo);

    _fontSizeSpin = new QSpinBox;
    _fontSizeSpin->setRange(8, 200);
    _fontSizeSpin->setValue(48);
    _fontSizeSpin->setSuffix(" pt");
    form->addRow(tr("Size:"), _fontSizeSpin);

    _colorBtn = new QToolButton;
    _colorBtn->setFixedSize(40, 24);
    _colorBtn->setStyleSheet("background:#808080; border:1px solid #555;");
    _colorBtn->setProperty("selectedColor", QColor(128, 128, 128));
    connect(_colorBtn, &QToolButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(
            _colorBtn->property("selectedColor").value<QColor>(), this, tr("Watermark Color"));
        if (c.isValid()) {
            _colorBtn->setStyleSheet(
                QString("background:%1; border:1px solid #555;").arg(c.name()));
            _colorBtn->setProperty("selectedColor", c);
        }
    });
    form->addRow(tr("Color:"), _colorBtn);

    _opacitySlider = new LabeledSlider(0, 100, 30, "%");
    form->addRow(tr("Opacity:"), _opacitySlider);

    _rotationSpin = new QSpinBox;
    _rotationSpin->setRange(-180, 180);
    _rotationSpin->setValue(-45);
    _rotationSpin->setSuffix(QString::fromUtf8("\xC2\xB0")); // degree sign
    form->addRow(tr("Rotation:"), _rotationSpin);
}

void WatermarkDialog::buildImageTab(QWidget* tab) {
    auto* form = new QFormLayout(tab);
    form->setContentsMargins(4, 8, 4, 4);
    form->setSpacing(6);

    auto* row = new QHBoxLayout;
    _imagePath = new QLineEdit;
    _imagePath->setPlaceholderText(tr("Select an image..."));
    _imagePath->setReadOnly(true);
    auto* browse = new QPushButton(tr("Browse..."));
    connect(browse, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, tr("Select Watermark Image"), {},
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!f.isEmpty()) _imagePath->setText(f);
    });
    row->addWidget(_imagePath, 1);
    row->addWidget(browse);
    form->addRow(tr("Image:"), row);

    _imgOpacitySlider = new LabeledSlider(0, 100, 30, "%");
    form->addRow(tr("Opacity:"), _imgOpacitySlider);

    _imgScaleSlider = new LabeledSlider(10, 100, 50, "%");
    form->addRow(tr("Scale:"), _imgScaleSlider);
}

void WatermarkDialog::buildPageRange(QWidget* container) {
    auto* rl = new QHBoxLayout(container);
    rl->setContentsMargins(0, 4, 0, 0);
    rl->setSpacing(6);

    _allPages = new QRadioButton(tr("All Pages"));
    _allPages->setChecked(true);
    _customPages = new QRadioButton(tr("Custom:"));

    _pageFrom = new QSpinBox;
    _pageFrom->setRange(1, 9999);
    _pageFrom->setValue(1);
    _pageFrom->setEnabled(false);
    _pageTo = new QSpinBox;
    _pageTo->setRange(1, 9999);
    _pageTo->setValue(1);
    _pageTo->setEnabled(false);

    connect(_customPages, &QRadioButton::toggled, _pageFrom, &QSpinBox::setEnabled);
    connect(_customPages, &QRadioButton::toggled, _pageTo, &QSpinBox::setEnabled);

    rl->addWidget(_allPages);
    rl->addWidget(_customPages);
    rl->addWidget(_pageFrom);
    rl->addWidget(new QLabel(QString::fromUtf8("\xe2\x80\x93"))); // en-dash
    rl->addWidget(_pageTo);
    rl->addStretch(1);
}

void WatermarkDialog::updatePreview() {
    if (_tabs->currentIndex() == 0) {
        QString txt = _textInput ? _textInput->text() : "CONFIDENTIAL";
        if (txt.isEmpty()) txt = "CONFIDENTIAL";
        _previewLabel->setText(txt);
        _previewLabel->setStyleSheet(
            "color:rgba(60,60,60,0.4); font-size:22px; font-weight:800; letter-spacing:1px;");
    } else {
        _previewLabel->setText("[Image]");
        _previewLabel->setStyleSheet(
            "color:rgba(60,60,60,0.3); font-size:16px; font-style:italic;");
    }
}

void WatermarkDialog::onApply() {
    if (!_ctx || !_ctx->pdfEditor) {
        QMessageBox::warning(this, tr("Error"), tr("PDF editing engine is not available."));
        return;
    }

    // Resolve page range
    int pageFrom = -1, pageTo = -1;
    if (_customPages->isChecked()) {
        pageFrom = _pageFrom->value() - 1; // convert to 0-based
        pageTo = _pageTo->value() - 1;
        if (pageTo < pageFrom) std::swap(pageFrom, pageTo);
    }

    int posId = _posGroup->checkedId();
    bool success = false;

    if (_tabs->currentIndex() == 0) {
        // ── Text watermark ──
        TextWatermarkOptions opts;
        opts.text = _textInput->text();
        if (opts.text.isEmpty()) opts.text = "CONFIDENTIAL";
        opts.fontFamily = _fontCombo->currentText();
        opts.fontSize = _fontSizeSpin->value();
        opts.opacity = _opacitySlider->slider()->value() / 100.0;
        opts.rotationDeg = _rotationSpin->value();
        opts.pageFrom = pageFrom;
        opts.pageTo = pageTo;

        QVariant cv = _colorBtn->property("selectedColor");
        if (cv.isValid()) opts.color = cv.value<QColor>();

        switch (posId) {
            case 0: opts.position = TextWatermarkOptions::TopLeft; break;
            case 1: opts.position = TextWatermarkOptions::Diagonal; break;
            case 2: opts.position = TextWatermarkOptions::TopRight; break;
            case 4: opts.position = TextWatermarkOptions::Center; break;
            case 6: opts.position = TextWatermarkOptions::BottomLeft; break;
            case 8: opts.position = TextWatermarkOptions::BottomRight; break;
            default: opts.position = TextWatermarkOptions::Center; break;
        }

        success = _ctx->pdfEditor->addTextWatermark(opts);
    } else {
        // ── Image watermark ──
        if (_imagePath->text().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("Please select a watermark image."));
            return;
        }

        ImageWatermarkOptions opts;
        opts.imagePath = _imagePath->text();
        opts.opacity = _imgOpacitySlider->slider()->value() / 100.0;
        opts.scale = _imgScaleSlider->slider()->value() / 100.0;
        opts.pageFrom = pageFrom;
        opts.pageTo = pageTo;

        switch (posId) {
            case 0: opts.position = ImageWatermarkOptions::TopLeft; break;
            case 2: opts.position = ImageWatermarkOptions::TopRight; break;
            case 4: opts.position = ImageWatermarkOptions::Center; break;
            case 6: opts.position = ImageWatermarkOptions::BottomLeft; break;
            case 8: opts.position = ImageWatermarkOptions::BottomRight; break;
            default: opts.position = ImageWatermarkOptions::Tile; break;
        }

        success = _ctx->pdfEditor->addImageWatermark(opts);
    }

    if (success) {
        if (_ctx->document) _ctx->document->markReload();
        accept();
    } else {
        QMessageBox::warning(this, tr("Watermark Failed"),
            tr("Could not apply the watermark. The document may be read-only or corrupted."));
    }
}

} // namespace gp
