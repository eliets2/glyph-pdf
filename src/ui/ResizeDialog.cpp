#include "ResizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>

namespace gp {

ResizeDialog::ResizeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Resize Page"));
    
    _presetCombo = new QComboBox(this);
    _presetCombo->addItem(tr("Custom"), QSizeF(0, 0));
    _presetCombo->addItem(tr("A4"), QSizeF(595.28, 841.89));
    _presetCombo->addItem(tr("Letter"), QSizeF(612.0, 792.0));
    _presetCombo->addItem(tr("Legal"), QSizeF(612.0, 1008.0));

    _widthSpin = new QDoubleSpinBox(this);
    _widthSpin->setRange(1.0, 10000.0);
    _widthSpin->setDecimals(2);

    _heightSpin = new QDoubleSpinBox(this);
    _heightSpin->setRange(1.0, 10000.0);
    _heightSpin->setDecimals(2);

    _unitCombo = new QComboBox(this);
    _unitCombo->addItem(tr("Points"));

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow(tr("Preset:"), _presetCombo);
    
    QHBoxLayout* wLayout = new QHBoxLayout;
    wLayout->addWidget(_widthSpin);
    wLayout->addWidget(new QLabel("x"));
    wLayout->addWidget(_heightSpin);
    wLayout->addWidget(_unitCombo);
    formLayout->addRow(tr("Size:"), wLayout);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResizeDialog::updateSpinsFromPreset);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttons);
    
    _presetCombo->setCurrentIndex(1); // Default A4
}

void ResizeDialog::updateSpinsFromPreset() {
    QSizeF sz = _presetCombo->currentData().toSizeF();
    if (sz.width() > 0 && sz.height() > 0) {
        _widthSpin->setValue(sz.width());
        _heightSpin->setValue(sz.height());
    }
}

QSizeF ResizeDialog::selectedSize() const {
    return QSizeF(_widthSpin->value(), _heightSpin->value());
}

} // namespace gp
