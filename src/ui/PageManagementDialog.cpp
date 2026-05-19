#include "ui/PageManagementDialog.h"
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

PageManagementDialog::PageManagementDialog(int totalPages, QWidget *parent)
    : QDialog(parent), m_totalPages(totalPages)
{
    setWindowTitle(tr("Page Management"));
    setMinimumWidth(350);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Operation selection
    QGroupBox *opGroup = new QGroupBox(tr("Operation"), this);
    QFormLayout *formLayout = new QFormLayout(opGroup);

    operationCombo = new QComboBox(this);
    operationCombo->addItem(tr("Extract Pages"), static_cast<int>(Operation::ExtractPages));
    operationCombo->addItem(tr("Delete Pages"), static_cast<int>(Operation::DeletePages));
    operationCombo->addItem(tr("Rotate Pages"), static_cast<int>(Operation::RotatePages));
    operationCombo->addItem(tr("Insert Blank Page"), static_cast<int>(Operation::InsertBlankPage));
    formLayout->addRow(tr("Action:"), operationCombo);

    fromPageSpin = new QSpinBox(this);
    fromPageSpin->setRange(1, m_totalPages);
    fromPageSpin->setValue(1);
    formLayout->addRow(tr("From Page:"), fromPageSpin);

    toPageSpin = new QSpinBox(this);
    toPageSpin->setRange(1, m_totalPages);
    toPageSpin->setValue(m_totalPages);
    formLayout->addRow(tr("To Page:"), toPageSpin);

    rotationLabel = new QLabel(tr("Rotation:"), this);
    rotationCombo = new QComboBox(this);
    rotationCombo->addItem(tr("90° Clockwise"), 90);
    rotationCombo->addItem(tr("180°"), 180);
    rotationCombo->addItem(tr("90° Counter-clockwise"), 270);
    formLayout->addRow(rotationLabel, rotationCombo);
    rotationLabel->hide();
    rotationCombo->hide();

    mainLayout->addWidget(opGroup);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    okButton = new QPushButton(tr("Apply"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(operationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PageManagementDialog::onOperationChanged);
}

void PageManagementDialog::onOperationChanged(int index)
{
    Operation op = static_cast<Operation>(operationCombo->itemData(index).toInt());
    bool showRotation = (op == Operation::RotatePages);
    rotationLabel->setVisible(showRotation);
    rotationCombo->setVisible(showRotation);

    bool showRange = (op != Operation::InsertBlankPage);
    fromPageSpin->setVisible(showRange);
    toPageSpin->setVisible(showRange);
}

PageManagementDialog::Operation PageManagementDialog::selectedOperation() const
{
    return static_cast<Operation>(operationCombo->currentData().toInt());
}

int PageManagementDialog::fromPage() const { return fromPageSpin->value(); }
int PageManagementDialog::toPage() const { return toPageSpin->value(); }
int PageManagementDialog::rotationAngle() const { return rotationCombo->currentData().toInt(); }
