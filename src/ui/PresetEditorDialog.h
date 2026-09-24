// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/BatchPreset.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QWidget;

namespace gp {

class CapabilityRegistry;

// ── R26-P2 U7 (plan §4.9): the multi-step preset editor ──────────────────────
// The "multi-step editor" residual: name/description; an ordered step list
// with add (op palette — UnavailableBuild ops are not offered), remove and
// reorder (up/down); per-step parameter forms built from the SAME widget
// families the batch panels use (clamped DPI spin, quality slider, PDF/A
// level combo, watermark text/opacity, redact pattern list, Bates fields).
// UnavailableRuntime steps stay in the preset — selectable, visibly
// disclosed with the capability's whyNot as the row tooltip — disclosed at
// design time and blocked at pre-flight (the landed capability rule).
//
// Save writes through BatchPresetCodec::validate: the editor cannot produce
// an invalid preset (the clamped widgets make it impossible; an impossible
// state is still refused honestly with the diagnostic instead of saving).
// The STORE write belongs to the caller (PresetManagerDialog) — the editor
// only hands back the assembled preset.
class PresetEditorDialog : public QDialog {
    Q_OBJECT
public:
    // `capabilities` may be null (tests, headless): gated ops then answer
    // honestly UnavailableRuntime — a step whose availability cannot be
    // probed must never claim availability.
    explicit PresetEditorDialog(const CapabilityRegistry* capabilities,
                                QWidget* parent = nullptr);

    // Edit an existing preset (the working copy is internal; the caller's
    // store is untouched until it applies the saved result).
    void loadPreset(const BatchPreset& preset);

    // The assembled preset — meaningful after a successful savePreset().
    BatchPreset preset() const { return m_preset; }

    // The Save action (the dialog's Save button runs this): re-assembles,
    // validates through the codec, and accept()s when valid; otherwise shows
    // the diagnostic and stays open. Returns whether the dialog closed with
    // a valid preset.
    bool savePreset();

    // Test seams (offscreen, no native modality): drive the step-list
    // mechanics and read back the refusal without exec().
    void addStepForTest(const QString& op) { addStep(op); }
    void removeCurrentStepForTest() { removeCurrentStep(); }
    bool moveCurrentStepForTest(int delta) { return moveCurrentStep(delta); }
    QString saveErrorForTest() const
        { return errorLabel_ ? errorLabel_->text() : QString(); }

private:
    void addStep(const QString& op);
    void removeCurrentStep();
    bool moveCurrentStep(int delta);
    void refreshStepList();
    void rebuildParamForm();
    void applyParamFormToStep(BatchPresetStep* step);

    const CapabilityRegistry* capabilities_;
    QList<BatchPresetStep> steps_;
    int m_currentFormRow = -1;   // the step the visible form edits

    QLineEdit*  nameEdit_        = nullptr;
    QLineEdit*  descriptionEdit_ = nullptr;
    QListWidget* stepsList_      = nullptr;
    QComboBox*  addOpCombo_      = nullptr;
    QPushButton* addBtn_         = nullptr;
    QPushButton* removeBtn_      = nullptr;
    QPushButton* upBtn_          = nullptr;
    QPushButton* downBtn_        = nullptr;
    QScrollArea* paramScroll_    = nullptr;
    QWidget*    paramForm_       = nullptr;
    QLabel*     paramNote_       = nullptr;
    QLabel*     errorLabel_      = nullptr;
    BatchPreset m_preset;
};

} // namespace gp
