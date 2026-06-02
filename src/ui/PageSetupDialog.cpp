// SPDX-License-Identifier: Apache-2.0
#include "PageSetupDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace gp {

PageSetupDialog::PageSetupDialog(const QString& docPath, QWidget* parent)
    : QDialog(parent), m_docPath(docPath)
{
    setWindowTitle(tr("Page Setup"));
    setMinimumWidth(400);
    setAccessibleName(tr("Page Setup"));

    auto* col = new QVBoxLayout(this);

    // ── Paper size ──────────────────────────────────────────────────────
    auto* paperGroup = new QGroupBox(tr("Paper"));
    auto* paperForm = new QFormLayout(paperGroup);

    m_paperSize = new QComboBox;
    m_paperSize->setAccessibleName(tr("Paper size"));
    m_paperSize->addItem(tr("A4 (210 x 297 mm)"),   static_cast<int>(QPageSize::A4));
    m_paperSize->addItem(tr("Letter (8.5 x 11 in)"), static_cast<int>(QPageSize::Letter));
    m_paperSize->addItem(tr("Legal (8.5 x 14 in)"),  static_cast<int>(QPageSize::Legal));
    m_paperSize->addItem(tr("A3 (297 x 420 mm)"),    static_cast<int>(QPageSize::A3));
    m_paperSize->addItem(tr("A5 (148 x 210 mm)"),    static_cast<int>(QPageSize::A5));
    m_paperSize->addItem(tr("B5 (176 x 250 mm)"),    static_cast<int>(QPageSize::B5));
    m_paperSize->addItem(tr("Tabloid (11 x 17 in)"), static_cast<int>(QPageSize::Tabloid));
    paperForm->addRow(tr("Size:"), m_paperSize);

    auto* orientRow = new QHBoxLayout;
    m_portrait = new QRadioButton(tr("Portrait"));
    m_portrait->setAccessibleName(tr("Portrait orientation"));
    m_landscape = new QRadioButton(tr("Landscape"));
    m_landscape->setAccessibleName(tr("Landscape orientation"));
    m_portrait->setChecked(true);
    orientRow->addWidget(m_portrait);
    orientRow->addWidget(m_landscape);
    orientRow->addStretch();
    paperForm->addRow(tr("Orientation:"), orientRow);

    col->addWidget(paperGroup);

    // ── Margins ─────────────────────────────────────────────────────────
    auto* marginGroup = new QGroupBox(tr("Margins (mm)"));
    auto* marginForm = new QFormLayout(marginGroup);

    auto makeMargin = [this](const QString& name) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(0.0, 100.0);
        sb->setValue(10.0);
        sb->setSuffix(tr(" mm"));
        sb->setDecimals(1);
        sb->setAccessibleName(name);
        return sb;
    };
    m_marginTop    = makeMargin(tr("Top margin"));
    m_marginBottom = makeMargin(tr("Bottom margin"));
    m_marginLeft   = makeMargin(tr("Left margin"));
    m_marginRight  = makeMargin(tr("Right margin"));

    marginForm->addRow(tr("Top:"),    m_marginTop);
    marginForm->addRow(tr("Bottom:"), m_marginBottom);
    marginForm->addRow(tr("Left:"),   m_marginLeft);
    marginForm->addRow(tr("Right:"),  m_marginRight);

    col->addWidget(marginGroup);

    // ── Scaling ─────────────────────────────────────────────────────────
    auto* scaleGroup = new QGroupBox(tr("Scaling"));
    auto* scaleLay = new QHBoxLayout(scaleGroup);

    m_scaleMode = new QComboBox;
    m_scaleMode->setAccessibleName(tr("Scaling mode"));
    m_scaleMode->addItem(tr("Fit to Page"));
    m_scaleMode->addItem(tr("Actual Size"));
    m_scaleMode->addItem(tr("Custom"));
    scaleLay->addWidget(m_scaleMode);

    m_scalePercent = new QSpinBox;
    m_scalePercent->setRange(10, 400);
    m_scalePercent->setValue(100);
    m_scalePercent->setSuffix(tr("%"));
    m_scalePercent->setAccessibleName(tr("Custom scale percentage"));
    m_scalePercent->setEnabled(false);
    scaleLay->addWidget(m_scalePercent);

    connect(m_scaleMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int idx) { m_scalePercent->setEnabled(idx == 2); });

    col->addWidget(scaleGroup);

    col->addStretch();

    // ── Buttons ─────────────────────────────────────────────────────────
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setAccessibleName(tr("Cancel page setup"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* ok = new QPushButton(tr("OK"));
    ok->setDefault(true);
    ok->setAccessibleName(tr("Apply page setup"));
    connect(ok, &QPushButton::clicked, this, &PageSetupDialog::onAccept);
    foot->addWidget(cancel);
    foot->addWidget(ok);
    col->addLayout(foot);

    // ── Load saved settings ─────────────────────────────────────────────
    Settings s = loadForDocument(docPath);
    for (int i = 0; i < m_paperSize->count(); ++i) {
        if (m_paperSize->itemData(i).toInt() == static_cast<int>(s.pageSize)) {
            m_paperSize->setCurrentIndex(i);
            break;
        }
    }
    if (s.orientation == QPageLayout::Landscape)
        m_landscape->setChecked(true);
    else
        m_portrait->setChecked(true);

    m_marginTop->setValue(s.marginTop);
    m_marginBottom->setValue(s.marginBottom);
    m_marginLeft->setValue(s.marginLeft);
    m_marginRight->setValue(s.marginRight);
    m_scaleMode->setCurrentIndex(s.scaleMode);
    m_scalePercent->setValue(s.scalePercent);
}

PageSetupDialog::Settings PageSetupDialog::settings() const {
    Settings s;
    s.pageSize     = static_cast<QPageSize::PageSizeId>(m_paperSize->currentData().toInt());
    s.orientation  = m_landscape->isChecked() ? QPageLayout::Landscape : QPageLayout::Portrait;
    s.marginTop    = m_marginTop->value();
    s.marginBottom = m_marginBottom->value();
    s.marginLeft   = m_marginLeft->value();
    s.marginRight  = m_marginRight->value();
    s.scaleMode    = m_scaleMode->currentIndex();
    s.scalePercent = m_scalePercent->value();
    return s;
}

PageSetupDialog::Settings PageSetupDialog::loadForDocument(const QString& docPath) {
    QSettings qset;
    qset.beginGroup("pageSetup/" + QString::number(qHash(docPath)));
    Settings s;
    s.pageSize     = static_cast<QPageSize::PageSizeId>(qset.value("pageSize", static_cast<int>(QPageSize::A4)).toInt());
    s.orientation  = static_cast<QPageLayout::Orientation>(qset.value("orientation", 0).toInt());
    s.marginTop    = qset.value("marginTop",    10.0).toDouble();
    s.marginBottom = qset.value("marginBottom", 10.0).toDouble();
    s.marginLeft   = qset.value("marginLeft",   10.0).toDouble();
    s.marginRight  = qset.value("marginRight",  10.0).toDouble();
    s.scaleMode    = qset.value("scaleMode",    0).toInt();
    s.scalePercent = qset.value("scalePercent", 100).toInt();
    qset.endGroup();
    return s;
}

void PageSetupDialog::saveForDocument(const QString& docPath, const Settings& s) {
    QSettings qset;
    qset.beginGroup("pageSetup/" + QString::number(qHash(docPath)));
    qset.setValue("pageSize",     static_cast<int>(s.pageSize));
    qset.setValue("orientation",  static_cast<int>(s.orientation));
    qset.setValue("marginTop",    s.marginTop);
    qset.setValue("marginBottom", s.marginBottom);
    qset.setValue("marginLeft",   s.marginLeft);
    qset.setValue("marginRight",  s.marginRight);
    qset.setValue("scaleMode",    s.scaleMode);
    qset.setValue("scalePercent", s.scalePercent);
    qset.endGroup();
}

void PageSetupDialog::onAccept() {
    saveForDocument(m_docPath, settings());
    accept();
}

} // namespace gp
