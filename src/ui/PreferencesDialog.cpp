// SPDX-License-Identifier: Apache-2.0
#include "PreferencesDialog.h"
#include "core/UpdateChecker.h"
#include "GpMainWindow.h"
#include "core/AppContext.h"
#include "engines/AutosaveManager.h"
#include "engines/ai/OllamaProvider.h"
#include <QFutureWatcher>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSpinBox>

namespace gp {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setMinimumWidth(460);
    setProperty("role", "modal");
    setAccessibleName(tr("Preferences"));
    setAccessibleDescription(tr("Application settings for language, theme, AI keys, and more"));

    auto* outer = new QVBoxLayout(this);

    auto* tabs = new QTabWidget;
    outer->addWidget(tabs, 1);

    // ────────────────────────────────────────────────────────────────────
    // TAB 1 — General (existing settings)
    // ────────────────────────────────────────────────────────────────────
    auto* generalTab = new QWidget;
    auto* col = new QVBoxLayout(generalTab);

    auto* genGroup = new QGroupBox(tr("General"));
    auto* form = new QFormLayout(genGroup);

    _langCombo = new QComboBox;
    _langCombo->setAccessibleName(tr("Interface language"));
    _langCombo->addItem(QStringLiteral("English"),       QStringLiteral("en"));
    _langCombo->addItem(QStringLiteral("\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9"),  QStringLiteral("ar"));
    _langCombo->addItem(QStringLiteral("Fran\xc3\xa7""ais"),    QStringLiteral("fr"));
    _langCombo->addItem(QStringLiteral("Deutsch"),        QStringLiteral("de"));

    QSettings settings;
    QString savedLang = settings.value("ui/language", "en").toString();
    for (int i = 0; i < _langCombo->count(); ++i) {
        if (_langCombo->itemData(i).toString() == savedLang) {
            _langCombo->setCurrentIndex(i);
            break;
        }
    }
    form->addRow(tr("Language:"), _langCombo);

    _themeCombo = new QComboBox;
    _themeCombo->setAccessibleName(tr("Color theme"));
    _themeCombo->addItem(tr("Dark"),           QStringLiteral("dark"));
    _themeCombo->addItem(tr("Light"),          QStringLiteral("light"));
    _themeCombo->addItem(tr("High Contrast"),  QStringLiteral("highcontrast"));
    _themeCombo->addItem(tr("System"),         QStringLiteral("system"));

    QString savedTheme = settings.value("ui/theme", "dark").toString();
    for (int i = 0; i < _themeCombo->count(); ++i) {
        if (_themeCombo->itemData(i).toString() == savedTheme) {
            _themeCombo->setCurrentIndex(i);
            break;
        }
    }
    form->addRow(tr("Theme:"), _themeCombo);

    _autosaveIntervalSpin = new QSpinBox(this);
    _autosaveIntervalSpin->setRange(1, 30);
    _autosaveIntervalSpin->setSuffix(tr(" minutes"));
    int currentInterval = settings.value("autosave/intervalSeconds", 300).toInt() / 60;
    _autosaveIntervalSpin->setValue(currentInterval);
    form->addRow(tr("Autosave Interval:"), _autosaveIntervalSpin);

    _autoPrune = new QCheckBox(tr("Auto-prune missing recent files on startup"));
    _autoPrune->setChecked(settings.value("recent/autoPrune", false).toBool());
    _autoPrune->setAccessibleName(tr("Remove missing recent files from the list when the application starts"));
    form->addRow(QString{}, _autoPrune);

    col->addWidget(genGroup);

    // Updates group
    auto* updateGroup = new QGroupBox(tr("Updates"));
    auto* updateLay = new QVBoxLayout(updateGroup);

    _autoUpdate = new QCheckBox(tr("Check for updates on startup"));
    _autoUpdate->setChecked(settings.value("update/checkOnStartup", true).toBool());
    _autoUpdate->setAccessibleName(tr("Automatically check for updates when the application starts"));
    updateLay->addWidget(_autoUpdate);

    auto* channelRow = new QHBoxLayout;
    channelRow->addWidget(new QLabel(tr("Channel:")));
    _updateChannel = new QComboBox;
    _updateChannel->addItem(tr("Stable"), QStringLiteral("stable"));
    _updateChannel->addItem(tr("Beta"),   QStringLiteral("beta"));
    QString savedChannel = settings.value("update/channel", "stable").toString();
    for (int i = 0; i < _updateChannel->count(); ++i) {
        if (_updateChannel->itemData(i).toString() == savedChannel) {
            _updateChannel->setCurrentIndex(i);
            break;
        }
    }
    channelRow->addWidget(_updateChannel);
    channelRow->addStretch(1);
    updateLay->addLayout(channelRow);

    auto* checkRow = new QHBoxLayout;
    _checkNowBtn = new QPushButton(tr("Check Now"));
    _checkNowBtn->setAccessibleName(tr("Check for updates now"));
    checkRow->addWidget(_checkNowBtn);
    _updateStatus = new QLabel;
    _updateStatus->setProperty("mono", true);
    _updateStatus->setStyleSheet("font-size:10px; color:#888;");
    checkRow->addWidget(_updateStatus, 1);
    updateLay->addLayout(checkRow);

    auto* versionLabel = new QLabel(tr("Current version: %1").arg(UpdateChecker::currentVersion()));
    versionLabel->setProperty("mono", true);
    versionLabel->setStyleSheet("font-size:10px; color:#666;");
    updateLay->addWidget(versionLabel);

    col->addWidget(updateGroup);

    auto* note = new QLabel(tr("Language changes take effect on restart."));
    note->setWordWrap(true);
    col->addWidget(note);

    col->addStretch();
    tabs->addTab(generalTab, tr("General"));

    // ────────────────────────────────────────────────────────────────────
    // TAB 2 — AI
    // ────────────────────────────────────────────────────────────────────
    auto* aiTab = new QWidget;
    auto* aiCol = new QVBoxLayout(aiTab);

    auto* aiGroup = new QGroupBox(tr("AI Provider — Ollama (local)"));
    auto* aiForm = new QFormLayout(aiGroup);

    // Ollama endpoint
    _aiKeyEdit = new QLineEdit;
    _aiKeyEdit->setPlaceholderText(tr("http://localhost:11434"));
    _aiKeyEdit->setAccessibleName(tr("Ollama endpoint URL"));
    _aiKeyEdit->setText(QSettings().value("ai/ollamaEndpoint",
                                          QStringLiteral("http://localhost:11434")).toString());
    aiForm->addRow(tr("Ollama endpoint:"), _aiKeyEdit);

    // Ollama model
    auto* ollamaModelEdit = new QLineEdit;
    ollamaModelEdit->setObjectName("ollamaModelEdit");
    ollamaModelEdit->setPlaceholderText(tr("llama3"));
    ollamaModelEdit->setAccessibleName(tr("Ollama model name"));
    ollamaModelEdit->setText(QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString());
    aiForm->addRow(tr("Model:"), ollamaModelEdit);

    aiCol->addWidget(aiGroup);

    // Action buttons row
    auto* aiBtnRow = new QHBoxLayout;
    _aiTestBtn   = new QPushButton(tr("Test connection"));
    aiBtnRow->addWidget(_aiTestBtn);
    aiBtnRow->addStretch(1);
    aiCol->addLayout(aiBtnRow);

    // Status label
    _aiStatusLabel = new QLabel;
    _aiStatusLabel->setProperty("mono", true);
    _aiStatusLabel->setStyleSheet("font-size:10px; color:#888;");
    aiCol->addWidget(_aiStatusLabel);

    // Privacy note
    auto* aiNote = new QLabel(tr("AI runs entirely on your machine via a local Ollama server. "
                                 "No document content is sent to any external server. "
                                 "Start Ollama at the endpoint above before using AI Chat."));
    aiNote->setWordWrap(true);
    aiNote->setStyleSheet("color:#888; font-size:10px;");
    aiCol->addWidget(aiNote);

    aiCol->addStretch();

    // Save endpoint+model on Save button (handled in saveSettings via object name lookup)
    connect(_aiTestBtn, &QPushButton::clicked, this, &PreferencesDialog::onAiTestKey);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int){ refreshAiStatus(); });

    tabs->addTab(aiTab, tr("AI"));
    refreshAiStatus();

    // ── Footer ───────────────────────────────────────────────────────
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setAccessibleName(tr("Cancel without saving"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* ok = new QPushButton(tr("Save"));
    ok->setAccessibleName(tr("Save preferences"));
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &PreferencesDialog::saveSettings);
    foot->addWidget(cancel);
    foot->addWidget(ok);
    outer->addLayout(foot);

    // ── Check Now connection ─────────────────────────────────────────
    connect(_checkNowBtn, &QPushButton::clicked, this, &PreferencesDialog::onCheckNow);
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ui/language", _langCombo->currentData().toString());
    settings.setValue("ui/theme", _themeCombo->currentData().toString());
    settings.setValue("update/checkOnStartup", _autoUpdate->isChecked());
    settings.setValue("update/channel", _updateChannel->currentData().toString());
    settings.setValue("recent/autoPrune", _autoPrune->isChecked());

    int intervalMinutes = _autosaveIntervalSpin->value();
    int intervalSeconds = intervalMinutes * 60;
    settings.setValue("autosave/intervalSeconds", intervalSeconds);

    // AI — Ollama endpoint and model
    if (_aiKeyEdit) {
        const QString endpoint = _aiKeyEdit->text().trimmed();
        if (!endpoint.isEmpty())
            settings.setValue("ai/ollamaEndpoint", endpoint);
    }
    if (auto* modelEdit = findChild<QLineEdit*>("ollamaModelEdit")) {
        const QString model = modelEdit->text().trimmed();
        if (!model.isEmpty())
            settings.setValue("ai/ollamaModel", model);
    }

    // Live apply to AutosaveManager
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin && mainWin->appContext() && mainWin->appContext()->autosave) {
        mainWin->appContext()->autosave->stop();
        mainWin->appContext()->autosave->start(intervalSeconds);
    }

    QMessageBox::information(this, tr("Preferences Saved"),
        tr("Some changes require a restart to take effect."));
    accept();
}

void PreferencesDialog::onCheckNow()
{
    _checkNowBtn->setEnabled(false);
    _updateStatus->setText(tr("Checking..."));

    auto* checker = new UpdateChecker(this);

    connect(checker, &UpdateChecker::updateAvailable, this, [this, checker](const UpdateChecker::UpdateInfo& info) {
        onUpdateResult(tr("Update available: v%1 (%2)").arg(info.version, info.releaseDate));
        checker->deleteLater();
    });
    connect(checker, &UpdateChecker::noUpdateAvailable, this, [this, checker]() {
        onUpdateResult(tr("You are running the latest version."));
        checker->deleteLater();
    });
    connect(checker, &UpdateChecker::checkFailed, this, [this, checker](const QString& reason) {
        onUpdateResult(tr("Check failed: %1").arg(reason));
        checker->deleteLater();
    });

    checker->checkForUpdates();
}

void PreferencesDialog::onUpdateResult(const QString& msg)
{
    _updateStatus->setText(msg);
    _checkNowBtn->setEnabled(true);
}

// ────────────────────────────────────────────────────────────────────
// AI tab slots
// ────────────────────────────────────────────────────────────────────

void PreferencesDialog::onAiTestKey()
{
    const QString endpoint = _aiKeyEdit ? _aiKeyEdit->text().trimmed()
                                        : QStringLiteral("http://localhost:11434");
    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();

    _aiTestBtn->setEnabled(false);
    _aiStatusLabel->setText(tr("Testing Ollama connection…"));

    auto prov = std::make_shared<OllamaProvider>(endpoint);
    QList<AiMessage> ping{{QStringLiteral("user"), QStringLiteral("Hello")}};
    AiOptions opts; opts.maxTokens = 5;

    auto* watcher = new QFutureWatcher<AiResult>(this);
    connect(watcher, &QFutureWatcher<AiResult>::finished, this,
            [this, watcher, prov]() {
        _aiTestBtn->setEnabled(true);
        const AiResult r = watcher->result();
        watcher->deleteLater();
        if (r.ok) {
            _aiStatusLabel->setText(tr("✓ Ollama reachable — AI chat is ready"));
            _aiStatusLabel->setStyleSheet("color:#4ec96d; font-size:10px;");
        } else {
            _aiStatusLabel->setText(tr("✗ %1").arg(r.errorMsg));
            _aiStatusLabel->setStyleSheet("color:#cc3333; font-size:10px;");
        }
    });
    watcher->setFuture(prov->chat(ping, opts));
}

void PreferencesDialog::onAiSaveKey()
{
    // No-op: Ollama requires no API key. Settings saved via saveSettings().
}

void PreferencesDialog::onAiDeleteKey()
{
    // No-op: Ollama requires no API key.
}

void PreferencesDialog::refreshAiStatus()
{
    if (!_aiStatusLabel) return;
    const QString endpoint = QSettings().value("ai/ollamaEndpoint",
                                               QStringLiteral("http://localhost:11434")).toString();
    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();
    _aiStatusLabel->setText(tr("Ollama endpoint: %1  ·  model: %2").arg(endpoint, model));
    _aiStatusLabel->setStyleSheet("color:#888; font-size:10px;");
}

} // namespace gp
