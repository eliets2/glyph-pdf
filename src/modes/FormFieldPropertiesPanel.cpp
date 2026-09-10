// SPDX-License-Identifier: Apache-2.0
#include "FormFieldPropertiesPanel.h"
#include "commands/EditFormFieldCommand.h"
#include "core/AppContext.h"
#include "core/interfaces/IFormManager.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

namespace gp {

FormFieldPropertiesPanel::FormFieldPropertiesPanel(const AppContext* ctx, QWidget* parent)
    : QWidget(parent)
    , m_ctx(ctx)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(4, 4, 4, 4);
    col->setSpacing(6);

    auto* title = new QLabel(tr("Field Properties"));
    title->setProperty("mono", true);
    col->addWidget(title);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("field_name"));
    connect(m_nameEdit, &QLineEdit::textChanged, this, &FormFieldPropertiesPanel::onNameChanged);
    form->addRow(tr("Name:"), m_nameEdit);

    m_nameStatus = new QLabel;
    m_nameStatus->setStyleSheet("QLabel { color: #c00; font-size: 10px; }");
    m_nameStatus->setVisible(false);
    form->addRow(QString(), m_nameStatus);

    m_tooltipEdit = new QLineEdit;
    m_tooltipEdit->setPlaceholderText(tr("Tooltip text"));
    form->addRow(tr("Tooltip:"), m_tooltipEdit);

    m_requiredCheck = new QCheckBox(tr("Required"));
    form->addRow(QString(), m_requiredCheck);

    m_defaultEdit = new QLineEdit;
    m_defaultEdit->setPlaceholderText(tr("Default value"));
    form->addRow(tr("Default:"), m_defaultEdit);

    // Phase-1 form-JS (U08 idiom): a calculated field is NAMED before the user
    // wonders why its value changes, and a format script's effect is shown as
    // a clearly-labeled display preview (presentation only, /V is untouched).
    m_scriptBadge = new QLabel;
    m_scriptBadge->setStyleSheet("QLabel { color: #06c; font-size: 10px; }");
    m_scriptBadge->setWordWrap(true);
    m_scriptBadge->setVisible(false);
    form->addRow(QString(), m_scriptBadge);

    m_displayPreview = new QLabel;
    m_displayPreview->setStyleSheet("QLabel { color: #666; font-size: 10px; }");
    m_displayPreview->setWordWrap(true);
    m_displayPreview->setVisible(false);
    form->addRow(QString(), m_displayPreview);

    m_placeholderEdit = new QLineEdit;
    m_placeholderEdit->setPlaceholderText(tr("Placeholder text"));
    form->addRow(tr("Placeholder:"), m_placeholderEdit);

    m_regexEdit = new QLineEdit;
    m_regexEdit->setPlaceholderText(tr("Validation regex (optional)"));
    connect(m_regexEdit, &QLineEdit::textChanged, this, &FormFieldPropertiesPanel::onRegexChanged);
    form->addRow(tr("Regex:"), m_regexEdit);

    m_regexStatus = new QLabel;
    m_regexStatus->setStyleSheet("QLabel { color: #c00; font-size: 10px; }");
    m_regexStatus->setVisible(false);
    form->addRow(QString(), m_regexStatus);

    auto addSpin = [this](QDoubleSpinBox*& spin) {
        spin = new QDoubleSpinBox;
        spin->setRange(0, 9999);
        spin->setDecimals(1);
    };
    addSpin(m_spinX);
    addSpin(m_spinY);
    addSpin(m_spinW);
    addSpin(m_spinH);
    
    auto* geomLayout = new QHBoxLayout;
    geomLayout->addWidget(new QLabel(tr("X:"))); geomLayout->addWidget(m_spinX);
    geomLayout->addWidget(new QLabel(tr("Y:"))); geomLayout->addWidget(m_spinY);
    geomLayout->addWidget(new QLabel(tr("W:"))); geomLayout->addWidget(m_spinW);
    geomLayout->addWidget(new QLabel(tr("H:"))); geomLayout->addWidget(m_spinH);
    form->addRow(tr("Geometry:"), geomLayout);

    col->addLayout(form);
    col->addStretch(1);

    m_applyBtn = new QToolButton;
    m_applyBtn->setText(tr("Apply"));
    m_applyBtn->setProperty("variant", "primary");
    m_applyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_applyBtn, &QToolButton::clicked, this, &FormFieldPropertiesPanel::onApplyClicked);
    col->addWidget(m_applyBtn);
}

void FormFieldPropertiesPanel::setFieldName(const QString& name)
{
    m_fieldName = name;
    m_nameEdit->setText(name);
    m_tooltipEdit->clear();
    m_requiredCheck->setChecked(false);
    m_defaultEdit->clear();
    m_placeholderEdit->clear();
    m_regexEdit->clear();
    m_nameStatus->setVisible(false);
    m_regexStatus->setVisible(false);
    refreshScriptState();
}

void FormFieldPropertiesPanel::clearFields()
{
    m_fieldName.clear();
    m_nameEdit->clear();
    m_tooltipEdit->clear();
    m_requiredCheck->setChecked(false);
    m_defaultEdit->clear();
    m_placeholderEdit->clear();
    m_regexEdit->clear();
    m_nameStatus->setVisible(false);
    m_regexStatus->setVisible(false);
    m_scriptBadge->setVisible(false);
    m_displayPreview->setVisible(false);
}

void FormFieldPropertiesPanel::refreshScriptState()
{
    if (!m_scriptBadge || !m_displayPreview) return;
    const QString path = (m_ctx && m_ctx->document) ? m_ctx->document->path() : QString();
    if (m_fieldName.isEmpty() || path.isEmpty() || !m_ctx || !m_ctx->forms) {
        m_scriptBadge->setVisible(false);
        m_displayPreview->setVisible(false);
        return;
    }

    const bool calculated = m_ctx->forms->fieldHasCalculateScript(path, m_fieldName);
    if (calculated) {
        m_scriptBadge->setText(tr("Calculated field — the value is recomputed from the "
                                  "document's calculation order when values are committed "
                                  "or saved. Typing here will not stick."));
    } else {
        m_scriptBadge->setVisible(false);
    }

    const bool formatted = m_ctx->forms->fieldHasFormatScript(path, m_fieldName);
    if (formatted) {
        FormJsFailure failure;
        const QString display = m_ctx->forms->formatFieldValue(path, m_fieldName, &failure);
        if (!failure.kind.isEmpty()) {
            // Honest: a failed format script is reported, not silently hidden.
            m_displayPreview->setText(tr("Format script failed (%1): %2 — the stored value is "
                                         "shown unchanged.").arg(failure.kind, failure.reason));
        } else {
            m_displayPreview->setText(tr("Display preview (format script — presentation only, "
                                         "the stored value is unchanged): %1").arg(display));
        }
        m_displayPreview->setVisible(true);
    } else {
        m_displayPreview->setVisible(false);
    }
    if (calculated) m_scriptBadge->setVisible(true);
}

void FormFieldPropertiesPanel::onApplyClicked()
{
    // Validate before pushing command
    validateName();
    validateRegex();

    if (m_nameStatus->isVisible() || m_regexStatus->isVisible()) return;
    if (m_fieldName.isEmpty()) return;
    if (!m_ctx || !m_ctx->undoStack || !m_ctx->forms || !m_ctx->document) return;

    EditFormFieldProperties newProps;
    newProps.name        = m_nameEdit->text().trimmed();
    newProps.tooltip     = m_tooltipEdit->text();
    newProps.required    = m_requiredCheck->isChecked();
    newProps.defaultVal  = m_defaultEdit->text();
    newProps.placeholder = m_placeholderEdit->text();
    newProps.validRegex  = m_regexEdit->text();

    QList<FormJsFailure> jsFailures;
    auto* cmd = new EditFormFieldCommand(
        m_ctx->forms.get(),
        m_ctx->document.get(),
        m_fieldName,
        newProps,
        &jsFailures
    );
    m_ctx->undoStack->push(cmd);

    const QString applied = newProps.name.isEmpty() ? m_fieldName : newProps.name;
    m_fieldName = applied;
    emit propertiesApplied(applied);
    emit geometryCommitted(fieldRect());

    // Phase-1 form-JS honesty contract: the edit persisted, but calculated
    // fields whose scripts failed are named — never a silent wrong value.
    if (!jsFailures.isEmpty()) {
        QStringList lines;
        for (const FormJsFailure& f : jsFailures)
            lines << tr("• %1 — %2 (%3)").arg(f.fieldName, f.reason, f.kind);
        QMessageBox::warning(this, tr("Field saved, calculation failed"),
            tr("The field was saved, but %n calculated field(s) failed and kept "
               "their previous value:\n\n%1", "", jsFailures.size()).arg(lines.join('\n')));
    }
    refreshScriptState();
}

void FormFieldPropertiesPanel::setFieldRect(const QRectF& rect)
{
    if (m_spinX) m_spinX->setValue(rect.x());
    if (m_spinY) m_spinY->setValue(rect.y());
    if (m_spinW) m_spinW->setValue(rect.width());
    if (m_spinH) m_spinH->setValue(rect.height());
}

QRectF FormFieldPropertiesPanel::fieldRect() const
{
    if (!m_spinX || !m_spinY || !m_spinW || !m_spinH) return QRectF();
    return QRectF(m_spinX->value(), m_spinY->value(), m_spinW->value(), m_spinH->value());
}

void FormFieldPropertiesPanel::onNameChanged(const QString& /*text*/)
{
    validateName();
}

void FormFieldPropertiesPanel::onRegexChanged(const QString& /*text*/)
{
    validateRegex();
}

void FormFieldPropertiesPanel::validateName()
{
    if (!m_nameStatus) return;
    const QString n = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    if (n.isEmpty()) {
        m_nameStatus->setText(tr("Name cannot be empty"));
        m_nameStatus->setVisible(true);
        if (m_nameEdit)
            m_nameEdit->setStyleSheet("QLineEdit { border: 1px solid #c00; }");
    } else {
        m_nameStatus->setVisible(false);
        if (m_nameEdit)
            m_nameEdit->setStyleSheet(QString());
    }
}

void FormFieldPropertiesPanel::validateRegex()
{
    if (!m_regexStatus || !m_regexEdit) return;
    const QString pattern = m_regexEdit->text();
    if (pattern.isEmpty()) {
        m_regexStatus->setVisible(false);
        m_regexEdit->setStyleSheet(QString());
        return;
    }
    const QRegularExpression re(pattern);
    if (!re.isValid()) {
        m_regexStatus->setText(tr("Invalid regex: %1").arg(re.errorString()));
        m_regexStatus->setVisible(true);
        m_regexEdit->setStyleSheet("QLineEdit { border: 1px solid #c00; }");
    } else {
        m_regexStatus->setVisible(false);
        m_regexEdit->setStyleSheet(QString());
    }
}

} // namespace gp
