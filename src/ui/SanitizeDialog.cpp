// SPDX-License-Identifier: Apache-2.0
#include "SanitizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>

SanitizeDialog::SanitizeDialog(IPdfEditorEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine)
{
    setWindowTitle(tr("Sanitize Document"));
    resize(400, 450);

    auto* mainLayout = new QVBoxLayout(this);

    auto* lblDesc = new QLabel(tr("Sanitizing a document removes hidden data, metadata, and structural information that might contain sensitive information."));
    lblDesc->setWordWrap(true);
    mainLayout->addWidget(lblDesc);

    m_selectiveModeCheckbox = new QCheckBox(tr("Selective remove mode"), this);
    m_selectiveModeCheckbox->setToolTip(tr("Enable this to choose exactly which categories to remove."));
    mainLayout->addWidget(m_selectiveModeCheckbox);

    m_optionsGroup = new QGroupBox(tr("Categories to Remove"), this);
    auto* optionsLayout = new QVBoxLayout(m_optionsGroup);

    m_chkMetadata = new QCheckBox(tr("Document Metadata && Properties"), m_optionsGroup);
    m_chkHiddenText = new QCheckBox(tr("Hidden Text && Structural Tags"), m_optionsGroup);
    m_chkOptionalContent = new QCheckBox(tr("Optional Content (Layers)"), m_optionsGroup);
    m_chkBookmarks = new QCheckBox(tr("Bookmarks && Outlines"), m_optionsGroup);
    m_chkEmbeddedFiles = new QCheckBox(tr("Embedded Files && Portfolios"), m_optionsGroup);
    m_chkFormValues = new QCheckBox(tr("AcroForm Values"), m_optionsGroup);
    m_chkAnnotations = new QCheckBox(tr("Annotation Contents && Rich Media"), m_optionsGroup);
    m_chkDangerousActions = new QCheckBox(tr("Dangerous Actions (JavaScript, Launch, etc.)"), m_optionsGroup);

    optionsLayout->addWidget(m_chkMetadata);
    optionsLayout->addWidget(m_chkHiddenText);
    optionsLayout->addWidget(m_chkOptionalContent);
    optionsLayout->addWidget(m_chkBookmarks);
    optionsLayout->addWidget(m_chkEmbeddedFiles);
    optionsLayout->addWidget(m_chkFormValues);
    optionsLayout->addWidget(m_chkAnnotations);
    optionsLayout->addWidget(m_chkDangerousActions);

    mainLayout->addWidget(m_optionsGroup);

    m_estimateLabel = new QLabel(this);
    m_estimateLabel->setWordWrap(true);
    m_estimateLabel->setStyleSheet("font-weight: bold; margin-top: 10px; margin-bottom: 10px;");
    mainLayout->addWidget(m_estimateLabel);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("Sanitize"));
    mainLayout->addWidget(btnBox);

    // Initial state
    m_selectiveModeCheckbox->setChecked(false);
    toggleSelectiveMode(false);

    connect(m_selectiveModeCheckbox, &QCheckBox::toggled, this, &SanitizeDialog::toggleSelectiveMode);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Update estimate when options change
    connect(m_chkMetadata, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkHiddenText, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkOptionalContent, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkBookmarks, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkEmbeddedFiles, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkFormValues, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkAnnotations, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);
    connect(m_chkDangerousActions, &QCheckBox::toggled, this, &SanitizeDialog::updateEstimate);

    updateEstimate();
}

SanitizeDialog::~SanitizeDialog() = default;

SanitizeOptions SanitizeDialog::getOptions() const
{
    SanitizeOptions opt;
    opt.removeMetadata = m_chkMetadata->isChecked();
    opt.removeHiddenText = m_chkHiddenText->isChecked();
    opt.removeOptionalContent = m_chkOptionalContent->isChecked();
    opt.removeBookmarks = m_chkBookmarks->isChecked();
    opt.removeEmbeddedFiles = m_chkEmbeddedFiles->isChecked();
    opt.removeFormValues = m_chkFormValues->isChecked();
    opt.removeAnnotations = m_chkAnnotations->isChecked();
    opt.removeDangerousActions = m_chkDangerousActions->isChecked();
    return opt;
}

void SanitizeDialog::toggleSelectiveMode(bool enabled)
{
    m_optionsGroup->setEnabled(enabled);
    if (!enabled) {
        // If not selective, we force check all
        m_chkMetadata->setChecked(true);
        m_chkHiddenText->setChecked(true);
        m_chkOptionalContent->setChecked(true);
        m_chkBookmarks->setChecked(true);
        m_chkEmbeddedFiles->setChecked(true);
        m_chkFormValues->setChecked(true);
        m_chkAnnotations->setChecked(true);
        m_chkDangerousActions->setChecked(true);
    }
    updateEstimate();
}

void SanitizeDialog::updateEstimate()
{
    if (!m_engine) return;
    
    SanitizeOptions opt = getOptions();
    SanitizeEstimate est = m_engine->estimateSanitization(opt);
    
    int total = est.totalItems();
    if (total == 0) {
        m_estimateLabel->setText(tr("No removable items found for the selected categories."));
    } else {
        QStringList details;
        if (est.metadataItems > 0) details << tr("%1 metadata items").arg(est.metadataItems);
        if (est.hiddenTextItems > 0) details << tr("%1 structural text items").arg(est.hiddenTextItems);
        if (est.optionalContentItems > 0) details << tr("%1 layers").arg(est.optionalContentItems);
        if (est.bookmarks > 0) details << tr("%1 bookmarks").arg(est.bookmarks);
        if (est.embeddedFiles > 0) details << tr("%1 embedded files").arg(est.embeddedFiles);
        if (est.formValues > 0) details << tr("%1 form values").arg(est.formValues);
        if (est.annotationsModified > 0) details << tr("%1 annotations").arg(est.annotationsModified);
        if (est.dangerousActions > 0) details << tr("%1 dangerous actions").arg(est.dangerousActions);
        
        m_estimateLabel->setText(tr("Summary: %1 items will be removed (%2).")
            .arg(total)
            .arg(details.join(", ")));
    }
}
