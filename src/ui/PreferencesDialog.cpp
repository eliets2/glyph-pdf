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
#include <QCoreApplication>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QStandardPaths>
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
    // TAB 2 — Engines
    // Honest disclosure of active backends per D1 (no fake selectors).
    // ────────────────────────────────────────────────────────────────────
    auto* engTab = new QWidget;
    auto* engCol = new QVBoxLayout(engTab);

    auto makeEngineRow = [&](QFormLayout* flayout, const QString& role,
                             const QString& engineName, const QString& lockReason) {
        auto* lbl = new QLabel(QStringLiteral("<b>%1</b>").arg(engineName));
        lbl->setToolTip(lockReason);
        auto* lockNote = new QLabel(QStringLiteral("<span style='color:#888;font-size:10px;'>%1</span>").arg(lockReason));
        lockNote->setWordWrap(true);
        auto* cell = new QWidget;
        auto* cellLay = new QVBoxLayout(cell);
        cellLay->setContentsMargins(0,0,0,0);
        cellLay->setSpacing(1);
        cellLay->addWidget(lbl);
        cellLay->addWidget(lockNote);
        flayout->addRow(role, cell);
    };

    // ── PDF rendering backend ────────────────────────────────────────
    auto* pdfEngGroup = new QGroupBox(tr("PDF Engine Backends (locked)"));
    auto* pdfEngForm  = new QFormLayout(pdfEngGroup);
#ifdef HAS_PDFIUM
    makeEngineRow(pdfEngForm, tr("Renderer:"), QStringLiteral("PDFium"),
                  tr("Required — tiled rendering and text extraction depend on PDFium."));
#else
    makeEngineRow(pdfEngForm, tr("Renderer:"), QStringLiteral("(unavailable)"),
                  tr("PDFium was not compiled in. Page rendering is disabled."));
#endif
    makeEngineRow(pdfEngForm, tr("Editor / Writer:"), QStringLiteral("PoDoFo"),
                  tr("Required — signing, redaction and form-filling depend on PoDoFo.\n"
                     "Digital signatures (ByteRange) require PoDoFo; switching to qpdf for writing\n"
                     "would invalidate signatures and is permanently blocked."));
    makeEngineRow(pdfEngForm, tr("Linearize / Repair / Inspect:"), QStringLiteral("qpdf"),
#ifdef HAS_QPDF
                  tr("Available — used automatically for repair, linearization and JSON inspection."));
#else
                  tr("qpdf was not compiled in. Linearize/repair features are disabled."));
#endif
    engCol->addWidget(pdfEngGroup);

    // ── OCR engine selector ──────────────────────────────────────────
    // D2: Real selector — all three engines exist and are on-device.
    // RapidOCR / Ensemble require PP-OCRv5 ONNX models to be present;
    // if models are absent the items are disabled with an explanation.
    auto* ocrEngGroup = new QGroupBox(tr("OCR Engine"));
    auto* ocrEngForm  = new QFormLayout(ocrEngGroup);

    _ocrEngineCombo = new QComboBox;
    _ocrEngineCombo->setObjectName(QStringLiteral("ocrEngineCombo"));
    _ocrEngineCombo->setAccessibleName(tr("OCR engine selection"));
    // Default "Automatic" uses the ROVER ensemble when the PP-OCRv5 models are
    // installed, and degrades to Tesseract when they are not. Unlike an explicit
    // ensemble selection, the automatic mode never errors on missing models.
    _ocrEngineCombo->addItem(tr("Automatic  (ROVER ensemble when available)"), QStringLiteral("auto"));
    _ocrEngineCombo->addItem(tr("Tesseract 5  (always available)"),   QStringLiteral("tesseract"));
    _ocrEngineCombo->addItem(tr("RapidOCR / PP-OCRv5  (ONNX)"),       QStringLiteral("rapidocr"));
    _ocrEngineCombo->addItem(tr("Ensemble  (Tesseract + RapidOCR, ROVER merge)"), QStringLiteral("ensemble"));

    // Honest availability check: RapidOCR and Ensemble need the ONNX model files.
    // If the detection model is absent, disable those items so the user is never
    // silently downgraded to Tesseract while the UI claims otherwise (audit §7 Pattern 5).
    {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                + QStringLiteral("/models/ppocrv5");
        const QString nextToExe = QCoreApplication::applicationDirPath()
                                  + QStringLiteral("/models/ppocrv5");
        const QString detModel = QStringLiteral("/PP-OCRv5_mobile_det_infer.onnx");
        const bool onnxAvailable =
#ifdef HAS_RAPIDOCR
            QFile::exists(appData + detModel) || QFile::exists(nextToExe + detModel);
#else
            false;
#endif
        if (!onnxAvailable) {
            const QString tip = tr("PP-OCRv5 ONNX model files not found.\n"
                                   "Place the models/ppocrv5/ directory next to the executable\n"
                                   "or in the application data directory to enable this engine.");
            auto* model = qobject_cast<QStandardItemModel*>(_ocrEngineCombo->model());
            for (int i = 2; i <= 3; ++i) {  // indices 2=rapidocr, 3=ensemble (0=auto, 1=tesseract)
                if (model) {
                    auto* item = model->item(i);
                    if (item) { item->setEnabled(false); item->setToolTip(tip); }
                }
            }
        }
    }

    // Restore saved preference (default: auto — ROVER ensemble when models present,
    // else Tesseract; always safe to select).
    {
        const QString savedEngine = QSettings().value(QStringLiteral("ocr/engine"),
                                                      QStringLiteral("auto")).toString();
        for (int i = 0; i < _ocrEngineCombo->count(); ++i) {
            if (_ocrEngineCombo->itemData(i).toString() == savedEngine) {
                _ocrEngineCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    ocrEngForm->addRow(tr("Engine:"), _ocrEngineCombo);

    auto* ocrNote = new QLabel(
        tr("All OCR engines run entirely on-device. "
           "Ensemble mode runs both engines and merges results via ROVER voting; "
           "it is slower but more accurate on mixed-quality scans."));
    ocrNote->setWordWrap(true);
    ocrNote->setStyleSheet(QStringLiteral("color:#888; font-size:10px;"));
    ocrEngForm->addRow(QString{}, ocrNote);

    engCol->addWidget(ocrEngGroup);
    engCol->addStretch();
    tabs->addTab(engTab, tr("Engines"));

    // ────────────────────────────────────────────────────────────────────
    // TAB 3 — AI
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

    // D2: OCR engine — only persist if the item is actually enabled
    // (guards against saving "rapidocr" when models are absent).
    if (_ocrEngineCombo) {
        auto* model = qobject_cast<QStandardItemModel*>(_ocrEngineCombo->model());
        const int idx = _ocrEngineCombo->currentIndex();
        const bool enabled = !model || !model->item(idx) || model->item(idx)->isEnabled();
        if (enabled) {
            settings.setValue(QStringLiteral("ocr/engine"),
                              _ocrEngineCombo->currentData().toString());
        }
    }

    // D3: AI — Ollama endpoint and model
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
