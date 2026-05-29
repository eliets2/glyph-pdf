#include "BatesNumberingDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>

namespace gp {

BatesNumberingDialog::BatesNumberingDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Bates Numbering"));

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
    form->addRow(tr("Prefix:"), _prefixEdit);
    form->addRow(tr("Suffix:"), _suffixEdit);
    form->addRow(tr("Start Number:"), _startSpin);
    form->addRow(tr("Digits:"), _digitsSpin);
    form->addRow(tr("Font:"), _fontCombo);
    form->addRow(tr("Size:"), _sizeSpin);
    form->addRow(tr("Position:"), _positionCombo);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);
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
    return opt;
}

} // namespace gp
