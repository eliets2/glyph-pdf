// SPDX-License-Identifier: Apache-2.0
#include "PreferencesDialog.h"
#include "core/UpdateChecker.h"
#include "core/PolicyController.h"
#include "core/SupportBundle.h"
#include "core/NetworkTouchpoints.h"
#include "GpMainWindow.h"
#include "core/AppContext.h"
#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "engines/ai/OllamaProvider.h"
#include <QFutureWatcher>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
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

    // R24(a): machine policy is loaded ONCE per process (default:
    // %PROGRAMDATA%/GlyphPDF/policy.json, GLYPHPDF_POLICY_PATH overrides the
    // location for tests). Managed keys win over the stored user values at
    // load time — the override below is always VISIBLE (disabled widget with
    // the policy value + "managed by policy" wording + status line), never
    // silent.
    auto& policy = gp::PolicyController::instance();
    policy.ensureLoaded();

    // Marks a field as machine-managed: disabled, badged, tooltip names the
    // enforcement wiring; the row label gets the visible "(managed by
    // policy)" suffix.
    auto markManaged = [](QFormLayout* form, QWidget* field) {
        field->setProperty("managedByPolicy", true);
        field->setEnabled(false);
        if (auto* lbl = qobject_cast<QLabel*>(form->labelForField(field)))
            lbl->setText(lbl->text() + QStringLiteral(" (managed by policy)"));
    };

    auto* outer = new QVBoxLayout(this);

    auto* tabs = new QTabWidget;
    outer->addWidget(tabs, 1);

    // ────────────────────────────────────────────────────────────────────
    // TAB 1 — General (existing settings)
    // ────────────────────────────────────────────────────────────────────
    auto* generalTab = new QWidget;
    auto* col = new QVBoxLayout(generalTab);

    // ── R24(a): machine policy status — ALWAYS visible, even when absent
    // ("No machine policy found (checked …)") so the disclosure itself is
    // never hidden.
    {
        auto* policyGroup = new QGroupBox(tr("Machine policy"));
        policyGroup->setObjectName(QStringLiteral("policyStatusGroup"));
        auto* policyLay = new QVBoxLayout(policyGroup);

        auto* statusLabel = new QLabel(policy.statusLine(), this);
        statusLabel->setObjectName(QStringLiteral("policyStatusLabel"));
        statusLabel->setWordWrap(true);
        statusLabel->setProperty("mono", true);
        statusLabel->setStyleSheet("font-size:8pt; color:#888;");
        policyLay->addWidget(statusLabel);

        const QStringList managed = policy.managedKeys();
        QString keyText;
        for (const QString& k : managed)
            keyText += k + QStringLiteral(" — ")
                       + gp::PolicyController::enforcementNote(k)
                       + QStringLiteral("\n");
        auto* keysLabel = new QLabel(keyText.trimmed(), this);
        keysLabel->setObjectName(QStringLiteral("policyKeysLabel"));
        keysLabel->setWordWrap(true);
        keysLabel->setVisible(!managed.isEmpty());
        policyLay->addWidget(keysLabel);

        col->addWidget(policyGroup);
    }

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

    // ── R24(b): Diagnostics — the redacted support bundle export. ────────
    {
        auto* diagGroup = new QGroupBox(tr("Diagnostics"));
        auto* diagLay = new QVBoxLayout(diagGroup);

        auto* bundleBtn = new QPushButton(tr("Export support bundle…"));
        bundleBtn->setObjectName(QStringLiteral("exportSupportBundleBtn"));
        bundleBtn->setAccessibleName(tr("Export a redacted support bundle"));
        // STUB (fail-first): the real builder is wired before the commit.
        connect(bundleBtn, &QPushButton::clicked, this,
                &PreferencesDialog::onExportSupportBundle);
        diagLay->addWidget(bundleBtn);

        auto* bundleNote = new QLabel(
            tr("Writes a support-bundle JSON file you can attach to a bug "
               "report: app version, build info, enabled capabilities, "
               "machine-policy state, feature toggles, and network-feature "
               "on/off states. Never includes document names, paths, PDF "
               "content, document metadata, URLs or network history."), this);
        bundleNote->setObjectName(QStringLiteral("bundleDisclosureLabel"));
        bundleNote->setWordWrap(true);
        bundleNote->setStyleSheet("color:#888; font-size:8pt;");
        diagLay->addWidget(bundleNote);

        col->addWidget(diagGroup);
    }

    // Updates group
    auto* updateGroup = new QGroupBox(tr("Updates"));
    auto* updateLay = new QVBoxLayout(updateGroup);

    // AR-8 D6: audit recommendation is default OFF (privacy preference).
    // A first-run transparency notice was landed in v1.3.1 (GpMainWindow::initUpdateChecker).
    // The default here matches the in-session preference: opt-in rather than opt-out.
    _autoUpdate = new QCheckBox(tr("Check for updates on startup"));
    _autoUpdate->setObjectName(QStringLiteral("autoUpdateCheck"));
    _autoUpdate->setChecked(settings.value("update/checkOnStartup", false).toBool());
    _autoUpdate->setAccessibleName(tr("Automatically check for updates when the application starts"));
    if (policy.isManaged(QStringLiteral("update/checkOnStartup"))) {
        // R24(a): policy value shown, widget locked, override disclosed.
        _autoUpdate->setChecked(
            policy.policyValue(QStringLiteral("update/checkOnStartup")).toBool());
        _autoUpdate->setEnabled(false);
        _autoUpdate->setProperty("managedByPolicy", true);
        _autoUpdate->setText(tr("Check for updates on startup (managed by policy)"));
        _autoUpdate->setToolTip(
            gp::PolicyController::enforcementNote(QStringLiteral("update/checkOnStartup")));
    }
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
    if (policy.isManaged(QStringLiteral("update/channel"))) {
        const QString policyChannel =
            policy.policyValue(QStringLiteral("update/channel")).toString();
        for (int i = 0; i < _updateChannel->count(); ++i) {
            if (_updateChannel->itemData(i).toString() == policyChannel)
                _updateChannel->setCurrentIndex(i);
        }
        _updateChannel->setEnabled(false);
        _updateChannel->setProperty("managedByPolicy", true);
        _updateChannel->setToolTip(
            gp::PolicyController::enforcementNote(QStringLiteral("update/channel")));
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
    _updateStatus->setStyleSheet("font-size:8pt; color:#888;");
    checkRow->addWidget(_updateStatus, 1);
    updateLay->addLayout(checkRow);

    auto* versionLabel = new QLabel(tr("Current version: %1").arg(UpdateChecker::currentVersion()));
    versionLabel->setProperty("mono", true);
    versionLabel->setStyleSheet("font-size:8pt; color:#666;");
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
        auto* lockNote = new QLabel(QStringLiteral("<span style='color:#888;font-size:8pt;'>%1</span>").arg(lockReason));
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
    ocrNote->setStyleSheet(QStringLiteral("color:#888; font-size:8pt;"));
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
    _ollamaEndpointEdit = new QLineEdit;
    _ollamaEndpointEdit->setPlaceholderText(tr("http://localhost:11434"));
    _ollamaEndpointEdit->setAccessibleName(tr("Ollama endpoint URL"));
    _ollamaEndpointEdit->setText(QSettings().value(QStringLiteral("ai/ollamaEndpoint"),
                                                    QStringLiteral("http://localhost:11434")).toString());
    if (policy.isManaged(QStringLiteral("ai/ollamaEndpoint"))) {
        _ollamaEndpointEdit->setText(
            policy.policyValue(QStringLiteral("ai/ollamaEndpoint")).toString());
        _ollamaEndpointEdit->setEnabled(false);
        _ollamaEndpointEdit->setProperty("managedByPolicy", true);
    }
    aiForm->addRow(tr("Ollama endpoint:"), _ollamaEndpointEdit);
    if (policy.isManaged(QStringLiteral("ai/ollamaEndpoint"))) {
        // Label suffix after the row exists (labelForField needs it built).
        if (auto* lbl = qobject_cast<QLabel*>(aiForm->labelForField(_ollamaEndpointEdit)))
            lbl->setText(lbl->text() + QStringLiteral(" (managed by policy)"));
        _ollamaEndpointEdit->setToolTip(
            gp::PolicyController::enforcementNote(QStringLiteral("ai/ollamaEndpoint")));
    }

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
    _aiStatusLabel->setStyleSheet("font-size:8pt; color:#888;");
    aiCol->addWidget(_aiStatusLabel);

    // Privacy note
    auto* aiNote = new QLabel(tr("AI runs entirely on your machine via a local Ollama server. "
                                 "No document content is sent to any external server. "
                                 "Start Ollama at the endpoint above before using AI Chat."));
    aiNote->setWordWrap(true);
    aiNote->setStyleSheet("color:#888; font-size:8pt;");
    aiCol->addWidget(aiNote);

    aiCol->addStretch();

    // Save endpoint+model on Save button (handled in saveSettings via object name lookup)
    connect(_aiTestBtn, &QPushButton::clicked, this, &PreferencesDialog::onAiTestKey);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int){ refreshAiStatus(); });

    tabs->addTab(aiTab, tr("AI"));
    refreshAiStatus();

    // ────────────────────────────────────────────────────────────────────
    // TAB 4 — Security (R19a): signing configuration.
    // The ONE production surface configuring PAdES level + TSA URL; the
    // SecurityController consumes signing/tsaUrl + signing/padesLevel BEFORE
    // every sign/certify/timestamp dispatch (PP05's missing callers).
    // ────────────────────────────────────────────────────────────────────
    auto* secTab = new QWidget;
    auto* secCol = new QVBoxLayout(secTab);

    auto* signGroup = new QGroupBox(tr("Digital Signatures (PAdES)"));
    auto* signForm  = new QFormLayout(signGroup);

    // RFC 3161 timestamp authority URL. Empty = no timestamping: levels above
    // B-B are refused honestly by the controller instead of silently
    // downgrading (the engine would skip the B-T token fetch when this is
    // empty and still report Success — the settings surface must not make
    // that state reachable, so the controller refuses it up front).
    _tsaUrlEdit = new QLineEdit;
    _tsaUrlEdit->setObjectName(QStringLiteral("tsaUrlEdit"));
    _tsaUrlEdit->setPlaceholderText(tr("https://timestamp.example.com (leave empty for B-B signatures only)"));
    _tsaUrlEdit->setAccessibleName(tr("Timestamp authority (TSA) URL"));
    _tsaUrlEdit->setText(QSettings().value(QStringLiteral("signing/tsaUrl"),
                                           QString()).toString());
    if (policy.isManaged(QStringLiteral("signing/tsaUrl"))) {
        _tsaUrlEdit->setText(
            policy.policyValue(QStringLiteral("signing/tsaUrl")).toString());
        _tsaUrlEdit->setToolTip(
            gp::PolicyController::enforcementNote(QStringLiteral("signing/tsaUrl")));
    }
    signForm->addRow(tr("TSA URL:"), _tsaUrlEdit);
    if (policy.isManaged(QStringLiteral("signing/tsaUrl")))
        markManaged(signForm, _tsaUrlEdit);

    // PAdES conformance level (ETSI EN 319 132-1). Default B-B: the honest
    // floor — it is the only level that needs no timestamp authority.
    _padesLevelCombo = new QComboBox;
    _padesLevelCombo->setObjectName(QStringLiteral("padesLevelCombo"));
    _padesLevelCombo->setAccessibleName(tr("PAdES conformance level"));
    _padesLevelCombo->addItem(tr("B-B   — basic signature (no timestamp)"), QStringLiteral("B-B"));
    _padesLevelCombo->addItem(tr("B-T   — signature timestamp token (needs a TSA URL)"), QStringLiteral("B-T"));
    _padesLevelCombo->addItem(tr("B-LT  — long-term validation (DSS; needs a TSA URL)"), QStringLiteral("B-LT"));
    _padesLevelCombo->addItem(tr("B-LTA — archival (DSS + archive timestamp; needs a TSA URL)"), QStringLiteral("B-LTA"));
    {
        const QString savedLevel = QSettings().value(QStringLiteral("signing/padesLevel"),
                                                     QStringLiteral("B-B")).toString();
        for (int i = 0; i < _padesLevelCombo->count(); ++i) {
            if (_padesLevelCombo->itemData(i).toString() == savedLevel) {
                _padesLevelCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    if (policy.isManaged(QStringLiteral("signing/padesLevel"))) {
        const QString policyLevel =
            policy.policyValue(QStringLiteral("signing/padesLevel")).toString();
        for (int i = 0; i < _padesLevelCombo->count(); ++i) {
            if (_padesLevelCombo->itemData(i).toString() == policyLevel)
                _padesLevelCombo->setCurrentIndex(i);
        }
        _padesLevelCombo->setToolTip(
            gp::PolicyController::enforcementNote(QStringLiteral("signing/padesLevel")));
    }
    signForm->addRow(tr("PAdES level:"), _padesLevelCombo);
    if (policy.isManaged(QStringLiteral("signing/padesLevel")))
        markManaged(signForm, _padesLevelCombo);

    auto* signNote = new QLabel(
        tr("Levels above B-B embed RFC 3161 timestamp and long-term-validation "
           "data and require a reachable TSA URL. If the TSA is missing or "
           "unreachable, signing refuses or discloses the attained level — it "
           "never silently produces a lower level."));
    signNote->setWordWrap(true);
    signNote->setStyleSheet(QStringLiteral("color:#888; font-size:8pt;"));
    signForm->addRow(QString{}, signNote);

    secCol->addWidget(signGroup);
    secCol->addStretch();
    tabs->addTab(secTab, tr("Security"));

    // ────────────────────────────────────────────────────────────────────
    // TAB 5 — Network (R24(c)): read-only disclosure of EVERY network
    // touchpoint with the consent setting that governs it. Displaying this
    // page performs no network requests; the enumeration is a pure
    // QSettings read (core/NetworkTouchpoints.h honesty contract).
    // ────────────────────────────────────────────────────────────────────
    {
        auto* netTab = new QWidget;
        auto* netCol = new QVBoxLayout(netTab);

        auto* netIntro = new QLabel(
            tr("Every network touchpoint in GlyphPDF, with the consent "
               "setting that governs it. Displaying this page performs no "
               "network requests."), this);
        netIntro->setObjectName(QStringLiteral("networkPageIntro"));
        netIntro->setWordWrap(true);
        netCol->addWidget(netIntro);

        const auto touchpoints = gp::NetworkTouchpoints::enumerate(settings);
        for (const auto& tp : touchpoints) {
            const QString state =
                tp.enabled ? tr("Enabled") : tr("Disabled");
            QString text = QStringLiteral("<b>%1</b> — %2 (%3)")
                               .arg(tp.label.toHtmlEscaped(), state,
                                    tp.invocation.toHtmlEscaped())
                               + QStringLiteral(
                                     "<br><span style='color:#888; "
                                     "font-size:8pt;'>%1</span>")
                                     .arg(tp.disclosure.toHtmlEscaped());
            if (!tp.consentKey.isEmpty())
                text += QStringLiteral(
                            "<br><span style='color:#888; font-size:8pt;'>"
                            "%1</span>")
                            .arg(tr("Consent setting: %1")
                                     .arg(tp.consentKey.toHtmlEscaped()));
            auto* row = new QLabel(text, this);
            row->setObjectName(QStringLiteral("networkRow_") + tp.id);
            row->setWordWrap(true);
            netCol->addWidget(row);
        }

        netCol->addStretch();
        tabs->addTab(netTab, tr("Network"));
    }

    // ── Footer — QDialogButtonBox for platform-consistent button order ────────
    // AR-8 D6: use QDialogButtonBox so Cancel/OK ordering follows the platform
    // style guide (Windows: OK left / Cancel right; macOS: Cancel left / OK right).
    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    btnBox->setAccessibleName(tr("Dialog buttons"));

    // Use the Save role button as the "Save" action.
    auto* saveBtn = btnBox->button(QDialogButtonBox::Save);
    if (saveBtn) {
        saveBtn->setAccessibleName(tr("Save preferences"));
        saveBtn->setDefault(true);
    }
    auto* cancelBtn = btnBox->button(QDialogButtonBox::Cancel);
    if (cancelBtn) cancelBtn->setAccessibleName(tr("Cancel without saving"));

    connect(btnBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::saveSettings);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(btnBox);

    // ── Check Now connection ─────────────────────────────────────────
    connect(_checkNowBtn, &QPushButton::clicked, this, &PreferencesDialog::onCheckNow);
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::persistSetting(QSettings& store, const QString& key,
                                       const QVariant& value)
{
    // R24(a): the machine policy wins at load time, so a managed key is
    // never persisted from the UI — a user edit must not silently diverge
    // from what the app will actually do. The override is disclosed in the
    // dialog (disabled widget with the policy value + status line), never
    // silently applied.
    auto& policy = gp::PolicyController::instance();
    policy.ensureLoaded();
    if (policy.isManaged(key))
        return;
    store.setValue(key, value);
}

void PreferencesDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ui/language", _langCombo->currentData().toString());
    settings.setValue("ui/theme", _themeCombo->currentData().toString());
    // R24(a): policy-governed keys go through the write guard — a managed
    // key is never persisted from the UI (the widget above is already locked
    // and badged; this keeps the stored value from silently diverging).
    persistSetting(settings, "update/checkOnStartup", _autoUpdate->isChecked());
    persistSetting(settings, "update/channel", _updateChannel->currentData().toString());
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
    if (_ollamaEndpointEdit) {
        const QString endpoint = _ollamaEndpointEdit->text().trimmed();
        if (!endpoint.isEmpty())
            persistSetting(settings, "ai/ollamaEndpoint", endpoint);
    }
    if (auto* modelEdit = findChild<QLineEdit*>("ollamaModelEdit")) {
        const QString model = modelEdit->text().trimmed();
        if (!model.isEmpty())
            settings.setValue("ai/ollamaModel", model);
    }

    // R19a: signing configuration — the SecurityController consumes these
    // before every sign/certify/timestamp dispatch. The TSA URL is stored
    // trimmed; explicitly clearing it is allowed (it honestly restricts
    // signing to B-B, which the controller enforces).
    if (_tsaUrlEdit)
        persistSetting(settings, QStringLiteral("signing/tsaUrl"),
                       _tsaUrlEdit->text().trimmed());
    if (_padesLevelCombo)
        persistSetting(settings, QStringLiteral("signing/padesLevel"),
                       _padesLevelCombo->currentData().toString());

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

void PreferencesDialog::onExportSupportBundle()
{
    // R24(b): the user picks the destination; the bundle is REDACTED BY
    // CONSTRUCTION (see SupportBundle.h) — counts only for recents and
    // documents, no paths, no PDF content, no URLs, no network history.
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose a destination folder for support-bundle.json"),
        QString(), QFileDialog::ShowDirsOnly);
    if (dir.isEmpty())
        return;

    gp::SupportBundleInput in;
    // Counts only — the open-document COUNT, never its identity.
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin && mainWin->appContext()
        && mainWin->appContext()->document
        && !mainWin->appContext()->document->path().isEmpty())
        in.openDocumentCount = 1;
    if (mainWin && mainWin->appContext())
        in.capabilities = mainWin->appContext()->capabilities.get();

    QSettings user;
    const QJsonObject bundle = gp::SupportBundle::buildFromSettings(user, in);
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path = dir + QStringLiteral("/support-bundle-")
                         + UpdateChecker::currentVersion()
                         + QStringLiteral("-") + stamp
                         + QStringLiteral(".json");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export failed"),
                             tr("Could not write %1").arg(path));
        return;
    }
    f.write(gp::SupportBundle::serialize(bundle));
    f.close();

    QMessageBox::information(
        this, tr("Support bundle exported"),
        tr("Written to %1\n\nThe bundle contains no PDF content, no document "
           "metadata, no file paths, no URL values and no network history — "
           "counts and on/off states only.")
            .arg(path));
}

void PreferencesDialog::onCheckNow()
{
    _checkNowBtn->setEnabled(false);
    _updateStatus->setText(tr("Checking..."));

    auto* checker = new UpdateChecker(this);

    // Honor the channel currently selected in the combo (even if not yet saved).
    const QString channel = _updateChannel ? _updateChannel->currentData().toString()
                                           : QStringLiteral("stable");
    if (channel == QLatin1String("beta"))
        checker->setManifestUrl(UpdateChecker::manifestUrlForChannel(channel));

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
    // R24(a): a managed endpoint is honored here (the widget is locked to
    // the policy value; effectiveValue keeps that true even if the field
    // were edited programmatically).
    auto& policy = gp::PolicyController::instance();
    policy.ensureLoaded();
    const QString endpoint = policy
        .effectiveValue(QStringLiteral("ai/ollamaEndpoint"),
                        _ollamaEndpointEdit
                            ? _ollamaEndpointEdit->text().trimmed()
                            : QStringLiteral("http://localhost:11434"))
        .toString();
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
            _aiStatusLabel->setStyleSheet("color:#4ec96d; font-size:8pt;");
        } else {
            _aiStatusLabel->setText(tr("✗ %1").arg(r.errorMsg));
            _aiStatusLabel->setStyleSheet("color:#cc3333; font-size:8pt;");
        }
    });
    watcher->setFuture(prov->chat(ping, opts));
}

void PreferencesDialog::refreshAiStatus()
{
    if (!_aiStatusLabel) return;
    // R24(a): a managed endpoint is disclosed here too (policy value wins).
    auto& policy = gp::PolicyController::instance();
    policy.ensureLoaded();
    const QString endpoint = policy
        .effectiveValue(QStringLiteral("ai/ollamaEndpoint"),
                        QSettings().value("ai/ollamaEndpoint",
                                          QStringLiteral("http://localhost:11434")))
        .toString();
    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();
    _aiStatusLabel->setText(tr("Ollama endpoint: %1  ·  model: %2").arg(endpoint, model));
    _aiStatusLabel->setStyleSheet("color:#888; font-size:8pt;");
}

} // namespace gp
