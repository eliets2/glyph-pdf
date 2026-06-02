// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <memory>
#include "engines/ai/IAiProvider.h"

class QCheckBox;
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QSpinBox;
class CredentialManager;

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

    // AI tab widgets
    QComboBox*   _aiProviderCombo = nullptr;
    QLineEdit*   _aiKeyEdit       = nullptr;
    QPushButton* _aiTestBtn       = nullptr;
    QPushButton* _aiSaveBtn       = nullptr;
    QPushButton* _aiDeleteBtn     = nullptr;
    QLabel*      _aiStatusLabel   = nullptr;

    std::unique_ptr<CredentialManager> _credentialManager;
};

} // namespace gp
