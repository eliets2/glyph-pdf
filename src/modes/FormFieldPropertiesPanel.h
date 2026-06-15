// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QRectF>

class QLineEdit;
class QCheckBox;
class QLabel;
class QToolButton;
class QDoubleSpinBox;

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

    void setFieldRect(const QRectF& rect);
    QRectF fieldRect() const;

signals:
    void propertiesApplied(const QString& fieldName);
    void geometryCommitted(const QRectF& newRect);

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

    QDoubleSpinBox* m_spinX = nullptr;
    QDoubleSpinBox* m_spinY = nullptr;
    QDoubleSpinBox* m_spinW = nullptr;
    QDoubleSpinBox* m_spinH = nullptr;
};

} // namespace gp
