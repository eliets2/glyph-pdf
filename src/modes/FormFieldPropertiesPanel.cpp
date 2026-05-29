#include "FormFieldPropertiesPanel.h"
#include "commands/EditFormFieldCommand.h"
#include "core/AppContext.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

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

    auto* cmd = new EditFormFieldCommand(
        m_ctx->forms.get(),
        m_ctx->document.get(),
        m_fieldName,
        newProps
    );
    m_ctx->undoStack->push(cmd);

    const QString applied = newProps.name.isEmpty() ? m_fieldName : newProps.name;
    m_fieldName = applied;
    emit propertiesApplied(applied);
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
