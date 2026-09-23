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
    // R18(f): the /AA /K Keystroke event — the Default-value line edit runs
    // the field's keystroke script for EVERY text-changing edit and rejects
    // (reverts) the edit when the script refuses it (Acrobat keystroke
    // semantics; the host merges via AFMergeChange).
    void onDefaultTextChanged(const QString& text);

private:
    void validateName();
    void validateRegex();
    // Phase-1 form-JS: shows the "calculated" badge for fields carrying an
    // /AA /C script and, when an /AA /F format script exists, a DISPLAY-ONLY
    // preview of the formatted value (the stored /V is never rewritten).
    void refreshScriptState();
    // R18(f): writes the value line edit programmatically (populate, revert,
    // script transform) without re-triggering the keystroke event.
    void setValueText(const QString& text);

    const AppContext* m_ctx       = nullptr;
    QString           m_fieldName;   // original name before edits

    QLineEdit*  m_nameEdit        = nullptr;
    QLineEdit*  m_tooltipEdit     = nullptr;
    QCheckBox*  m_requiredCheck   = nullptr;
    QLineEdit*  m_defaultEdit     = nullptr;
    // R18(f): the keystroke gate for m_defaultEdit — the text BEFORE the
    // pending edit (Acrobat's event.value), the programmatic-write guard, and
    // the disclosure label for rejected/blocked/transformed keystrokes.
    QString     m_keystrokeBase;
    bool        m_syncingValueText = false;
    QLabel*     m_keystrokeStatus = nullptr;
    QLineEdit*  m_placeholderEdit = nullptr;
    QLineEdit*  m_regexEdit       = nullptr;
    QLabel*     m_regexStatus     = nullptr;
    QLabel*     m_nameStatus      = nullptr;
    QLabel*     m_scriptBadge     = nullptr;
    QLabel*     m_displayPreview  = nullptr;
    // R18(a): persistent stale-value warning + its acknowledge control.
    QLabel*     m_staleBanner     = nullptr;
    QToolButton* m_staleAckBtn    = nullptr;
    QToolButton* m_applyBtn       = nullptr;

    QDoubleSpinBox* m_spinX = nullptr;
    QDoubleSpinBox* m_spinY = nullptr;
    QDoubleSpinBox* m_spinW = nullptr;
    QDoubleSpinBox* m_spinH = nullptr;
};

} // namespace gp
