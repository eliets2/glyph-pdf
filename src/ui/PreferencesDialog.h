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
class QSettings;
class QVariant;

namespace gp {

class UpdateChecker;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    // R24(a): settings write guard. Refuses to persist a machine-managed key
    // (the policy wins at load time, so a user edit must not silently diverge
    // from what the app will actually do). Unmanaged keys persist normally.
    static void persistSetting(QSettings& store, const QString& key,
                               const QVariant& value);

private slots:
    void saveSettings();
    void onCheckNow();
    void onUpdateResult(const QString& msg);

    // AI tab slots
    void onAiTestKey();
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
    QLineEdit*   _ollamaEndpointEdit = nullptr;
    QPushButton* _aiTestBtn       = nullptr;
    QLabel*      _aiStatusLabel   = nullptr;

    // Security tab widgets (R19a): signing configuration consumed by the
    // SecurityController before every sign/certify/timestamp dispatch.
    QLineEdit*   _tsaUrlEdit      = nullptr;
    QComboBox*   _padesLevelCombo = nullptr;
};

} // namespace gp
