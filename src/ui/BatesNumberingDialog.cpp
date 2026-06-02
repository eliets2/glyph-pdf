// SPDX-License-Identifier: Apache-2.0
#include "BatesNumberingDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>

#include <algorithm>
#include <iterator>
#include <utility>

namespace gp {

namespace {
// Built-in Bates presets. Index 0 ("Custom") leaves the fields untouched.
struct BatesPreset {
    const char* name;
    const char* prefix;
    const char* suffix;
    int digits;
    HeaderFooterOptions::Position position;
};
const BatesPreset kPresets[] = {
    { "Custom",            "",        "",  6, HeaderFooterOptions::Position::BottomRight },
    { "Legal Production",  "PROD",    "",  6, HeaderFooterOptions::Position::BottomRight },
    { "Exhibit",           "EX-",     "",  4, HeaderFooterOptions::Position::BottomCenter },
    { "Confidential",      "CONF-",   "",  6, HeaderFooterOptions::Position::BottomRight },
    { "Sequential Plain",  "",        "",  4, HeaderFooterOptions::Position::BottomRight },
};
} // namespace

BatesNumberingDialog::BatesNumberingDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Bates Numbering"));

    // ── Preset selector ──────────────────────────────────────────────────
    _presetCombo = new QComboBox(this);
    for (const auto& p : kPresets) _presetCombo->addItem(tr(p.name));

    _prefixEdit = new QLineEdit(this);
    _suffixEdit = new QLineEdit(this);

    _startSpin = new QSpinBox(this);
    _startSpin->setRange(1, 999999);
    _startSpin->setValue(1);

    _digitsSpin = new QSpinBox(this);
    _digitsSpin->setRange(3, 10);
    _digitsSpin->setValue(6);

    _fontCombo = new QComboBox(this);
    _fontCombo->addItems(QFontDatabase::families());

    _sizeSpin = new QSpinBox(this);
    _sizeSpin->setRange(6, 72);
    _sizeSpin->setValue(12);

    _positionCombo = new QComboBox(this);
    _positionCombo->addItem(tr("Top Left"), static_cast<int>(HeaderFooterOptions::Position::TopLeft));
    _positionCombo->addItem(tr("Top Center"), static_cast<int>(HeaderFooterOptions::Position::TopCenter));
    _positionCombo->addItem(tr("Top Right"), static_cast<int>(HeaderFooterOptions::Position::TopRight));
    _positionCombo->addItem(tr("Bottom Left"), static_cast<int>(HeaderFooterOptions::Position::BottomLeft));
    _positionCombo->addItem(tr("Bottom Center"), static_cast<int>(HeaderFooterOptions::Position::BottomCenter));
    _positionCombo->addItem(tr("Bottom Right"), static_cast<int>(HeaderFooterOptions::Position::BottomRight));
    _positionCombo->setCurrentIndex(5); // Bottom Right

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("Preset:"), _presetCombo);
    form->addRow(tr("Prefix:"), _prefixEdit);
    form->addRow(tr("Suffix:"), _suffixEdit);
    form->addRow(tr("Start Number:"), _startSpin);
    form->addRow(tr("Digits:"), _digitsSpin);
    form->addRow(tr("Font:"), _fontCombo);
    form->addRow(tr("Size:"), _sizeSpin);
    form->addRow(tr("Position:"), _positionCombo);

    // ── Page range selection ─────────────────────────────────────────────
    _rangeAllRadio = new QRadioButton(tr("All pages"), this);
    _rangeAllRadio->setChecked(true);
    _rangeCustomRadio = new QRadioButton(tr("Pages"), this);
    auto* rangeGroup = new QButtonGroup(this);
    rangeGroup->addButton(_rangeAllRadio);
    rangeGroup->addButton(_rangeCustomRadio);

    _fromSpin = new QSpinBox(this);
    _fromSpin->setRange(1, 999999);
    _fromSpin->setValue(1);
    _toSpin = new QSpinBox(this);
    _toSpin->setRange(1, 999999);
    _toSpin->setValue(1);

    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(_rangeAllRadio);
    rangeRow->addSpacing(8);
    rangeRow->addWidget(_rangeCustomRadio);
    rangeRow->addWidget(_fromSpin);
    rangeRow->addWidget(new QLabel(tr("to"), this));
    rangeRow->addWidget(_toSpin);
    rangeRow->addStretch(1);
    form->addRow(tr("Range:"), rangeRow);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    // Wiring
    connect(_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatesNumberingDialog::applyPreset);
    connect(_rangeAllRadio, &QRadioButton::toggled, this, [this]() { updateRangeEnabled(); });
    connect(_rangeCustomRadio, &QRadioButton::toggled, this, [this]() { updateRangeEnabled(); });

    applyPreset(0);          // initialize fields from the "Custom" preset baseline
    updateRangeEnabled();    // disable the from/to spins until "Pages" is chosen
}

void BatesNumberingDialog::setPageCount(int pageCount) {
    _pageCount = pageCount;
    if (pageCount > 0) {
        _fromSpin->setMaximum(pageCount);
        _toSpin->setMaximum(pageCount);
        _toSpin->setValue(pageCount);  // default custom range spans the whole doc
    }
}

void BatesNumberingDialog::applyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kPresets))) return;
    if (index == 0) return;  // "Custom": leave the user's fields as-is
    const BatesPreset& p = kPresets[index];
    _prefixEdit->setText(QString::fromUtf8(p.prefix));
    _suffixEdit->setText(QString::fromUtf8(p.suffix));
    _digitsSpin->setValue(p.digits);
    int posIdx = _positionCombo->findData(static_cast<int>(p.position));
    if (posIdx >= 0) _positionCombo->setCurrentIndex(posIdx);
}

void BatesNumberingDialog::updateRangeEnabled() {
    const bool custom = _rangeCustomRadio->isChecked();
    _fromSpin->setEnabled(custom);
    _toSpin->setEnabled(custom);
}

BatesNumberingOptions BatesNumberingDialog::options() const {
    BatesNumberingOptions opt;
    opt.prefix = _prefixEdit->text();
    opt.suffix = _suffixEdit->text();
    opt.startNumber = _startSpin->value();
    opt.digitCount = _digitsSpin->value();
    opt.fontFamily = _fontCombo->currentText();
    opt.fontSize = _sizeSpin->value();
    opt.position = static_cast<HeaderFooterOptions::Position>(_positionCombo->currentData().toInt());

    if (_rangeCustomRadio->isChecked()) {
        // Normalize so from <= to; backend treats these as 1-based inclusive.
        int from = _fromSpin->value();
        int to = _toSpin->value();
        if (from > to) std::swap(from, to);
        opt.firstPage = from;
        opt.lastPage = to;
    } else {
        opt.firstPage = 0;  // all pages
        opt.lastPage = 0;
    }
    return opt;
}

} // namespace gp
