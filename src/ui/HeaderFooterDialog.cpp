// SPDX-License-Identifier: Apache-2.0
#include "HeaderFooterDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFontDatabase>

namespace gp {

HeaderFooterDialog::HeaderFooterDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Header / Footer"));

    _textEdit = new QLineEdit(this);
    _textEdit->setPlaceholderText(tr("e.g. Page {page} of {total}"));

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
    _positionCombo->setCurrentIndex(4); // Bottom Center

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("Text (use {page}, {total}, {date}):"), _textEdit);
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

HeaderFooterOptions HeaderFooterDialog::options() const {
    HeaderFooterOptions opt;
    opt.textTemplate = _textEdit->text();
    opt.fontFamily = _fontCombo->currentText();
    opt.fontSize = _sizeSpin->value();
    opt.position = static_cast<HeaderFooterOptions::Position>(_positionCombo->currentData().toInt());
    return opt;
}

} // namespace gp
