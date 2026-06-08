// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <memory>

class QCheckBox;
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace gp {

class UpdateChecker;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

private slots:
    void saveSettings();
    void onCheckNow();
    void onUpdateResult(const QString& msg);

    // AI tab slots
    void onAiTestKey();
    void onAiSaveKey();
    void onAiDeleteKey();
    void refreshAiStatus();

private:
    QComboBox*   _langCombo   = nullptr;
    QComboBox*   _themeCombo  = nullptr;
    QSpinBox*    _autosaveIntervalSpin = nullptr;
    QCheckBox*   _autoUpdate  = nullptr;
    QCheckBox*   _autoPrune   = nullptr;
    QComboBox*   _updateChannel = nullptr;
    QPushButton* _checkNowBtn = nullptr;
    QLabel*      _updateStatus = nullptr;
    QComboBox*   _ocrEngineCombo = nullptr;

    // AI tab widgets
    QLineEdit*   _aiKeyEdit       = nullptr;  // repurposed: Ollama endpoint
    QPushButton* _aiTestBtn       = nullptr;
    QPushButton* _aiSaveBtn       = nullptr;
    QPushButton* _aiDeleteBtn     = nullptr;
    QLabel*      _aiStatusLabel   = nullptr;
};

} // namespace gp
