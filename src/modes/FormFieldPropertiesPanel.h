#pragma once
#include <QWidget>

class QLineEdit;
class QCheckBox;
class QLabel;
class QToolButton;

struct AppContext;

namespace gp {

/// Right-sidebar panel shown when a form field is selected in FormBuilderMode.
/// Exposes editable properties and pushes EditFormFieldCommand on Apply.
class FormFieldPropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit FormFieldPropertiesPanel(const AppContext* ctx, QWidget* parent = nullptr);

    /// Populate all fields with the data for the named field.
    void setFieldName(const QString& name);

    /// Clear all fields (deselected state).
    void clearFields();

signals:
    void propertiesApplied(const QString& fieldName);

private slots:
    void onApplyClicked();
    void onNameChanged(const QString& text);
    void onRegexChanged(const QString& text);

private:
    void validateName();
    void validateRegex();

    const AppContext* m_ctx       = nullptr;
    QString           m_fieldName;   // original name before edits

    QLineEdit*  m_nameEdit        = nullptr;
    QLineEdit*  m_tooltipEdit     = nullptr;
    QCheckBox*  m_requiredCheck   = nullptr;
    QLineEdit*  m_defaultEdit     = nullptr;
    QLineEdit*  m_placeholderEdit = nullptr;
    QLineEdit*  m_regexEdit       = nullptr;
    QLabel*     m_regexStatus     = nullptr;
    QLabel*     m_nameStatus      = nullptr;
    QToolButton* m_applyBtn       = nullptr;
};

} // namespace gp
