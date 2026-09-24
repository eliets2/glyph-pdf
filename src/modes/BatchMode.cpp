// SPDX-License-Identifier: Apache-2.0
#include "BatchMode.h"
#include "util/GpTheme.h"

#include "core/Capability.h"
#include "core/PolicyController.h"     // emergence E-2: name the download policy in the whyNot
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/IConversionEngine.h"
#include "core/interfaces/IOcrEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"          // R26: the preset candidate chain commits through SafeSave
#include "engines/VeraPdfValidator.h"  // R26: pdfa-check step (the registry's PdfAValidation probe)
#include "engines/ocr/OcrPipeline.h"
#include "engines/podofo/PdfPageOps.h"
#include "engines/pdfium/PdfiumBackend.h" // N3: per-page has-text probe (PDFium text extraction)
#include "engines/PatternRedactor.h" // §9.12 P1: named PII preset keys

// §9.12 P1: the async merge worker appends input-by-input so it can report
// progress, honor cancellation and account per item — boundaries PdfPageOps'
// all-at-once mergeDocuments cannot expose. That loop therefore uses PoDoFo
// directly, mirroring PdfPageOps::mergeDocuments' idiom (see startMergeWorker).
#include <podofo/podofo.h>

#include <QPromise> // §9.12 P1: merge worker reports per-file results/progress

using TargetFormat = IConversionEngine::TargetFormat;

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMap>
#include <QHash>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

namespace gp {

// R26: the preset schema validates targetDpi against ITS OWN copy of the
// engine's clamp range (core must not depend on modes). If the engine range
// ever changes, this pin breaks the build instead of letting the two drift.
static_assert(BatchMode::kMinTargetDpi == 36 && BatchMode::kMaxTargetDpi == 600,
              "BatchPreset schema targetDpi range (36-600) must match "
              "BatchMode's engine clamp constants");

// ── Constructor ───────────────────────────────────────────────────────────────

void BatchMode::setOperationForTest(int index) {
    if (m_opCombo) m_opCombo->setCurrentIndex(index);
}

BatchMode::BatchMode(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Top toolbar ────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setProperty("role", "modeToolbar");
    toolbar->setFixedHeight(36);
    auto* tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(12, 0, 12, 0);
    tbLay->setSpacing(6);
    auto* tbTitle = new QLabel(tr("BATCH PROCESSING"));
    tbTitle->setProperty("mono", true);
    tbLay->addWidget(tbTitle);
    tbLay->addStretch(1);
    mainLayout->addWidget(toolbar);

    // ── Body row ───────────────────────────────────────────────────────────
    auto* bodyHost = new QWidget;
    auto* bodyRow = new QHBoxLayout(bodyHost);
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(0);
    mainLayout->addWidget(bodyHost, 1);

    // Left: file list panel (fixed 280px)
    auto* leftFrame = new QFrame;
    leftFrame->setFixedWidth(280);
    buildFilePanel(leftFrame);
    bodyRow->addWidget(leftFrame);

    // Center+right: operation config + progress
    auto* rightFrame = new QFrame;
    buildOperationPanel(rightFrame);
    bodyRow->addWidget(rightFrame, 1);

    // ── Progress + log pinned at bottom of center panel ────────────────────
    auto* rightLay = qobject_cast<QVBoxLayout*>(rightFrame->layout());
    if (rightLay) {
        auto* progressHost = new QWidget;
        buildProgressPanel(progressHost);
        rightLay->addWidget(progressHost);
    }

    // ── Watcher connections ────────────────────────────────────────────────
    connect(&m_watcher, &QFutureWatcher<BatchFileResult>::progressValueChanged,
            this, &BatchMode::onBatchProgress);
    connect(&m_watcher, &QFutureWatcher<BatchFileResult>::finished,
            this, &BatchMode::onBatchFinished);
}

// ── File panel (D1) ───────────────────────────────────────────────────────────

void BatchMode::buildFilePanel(QWidget* host) {
    auto* vlay = new QVBoxLayout(host);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Header
    auto* hdr = new QFrame;
    hdr->setProperty("role", "modeToolbar");
    hdr->setFixedHeight(28);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(10, 0, 10, 0);
    auto* hdrTitle = new QLabel(tr("INPUT FILES"));
    hdrTitle->setProperty("mono", true);
    hdrLay->addWidget(hdrTitle);
    hdrLay->addStretch(1);
    m_fileCountLabel = new QLabel(tr("0 files"));
    m_fileCountLabel->setProperty("mono", true);
    hdrLay->addWidget(m_fileCountLabel);
    vlay->addWidget(hdr);

    // Drop zone hint + list view
    auto* dropHint = new QLabel(tr("Drop PDF files here or use buttons below"));
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setWordWrap(true);
    dropHint->setStyleSheet("color:#71747a; font-size:10px; padding:8px;");

    m_fileModel = new QStandardItemModel(this);
    m_fileView  = new QListView;
    m_fileView->setModel(m_fileModel);
    m_fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileView->setStyleSheet("font-size:10px;");

    vlay->addWidget(dropHint);
    vlay->addWidget(m_fileView, 1);

    // Button row
    auto* btnFrame = new QFrame;
    auto* btnLay = new QVBoxLayout(btnFrame);
    btnLay->setContentsMargins(8, 6, 8, 6);
    btnLay->setSpacing(4);

    auto* row1 = new QHBoxLayout;
    auto* addFilesBtn = new QPushButton(tr("Add Files…"));
    auto* addFolderBtn = new QPushButton(tr("Add Folder…"));
    row1->addWidget(addFilesBtn);
    row1->addWidget(addFolderBtn);
    btnLay->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    auto* removeBtn = new QPushButton(tr("Remove Selected"));
    auto* clearBtn = new QPushButton(tr("Clear All"));
    row2->addWidget(removeBtn);
    row2->addWidget(clearBtn);
    btnLay->addLayout(row2);

    buildHotFolderSection(btnLay);

    vlay->addWidget(btnFrame);

    connect(addFilesBtn,  &QPushButton::clicked, this, &BatchMode::onAddFiles);
    connect(addFolderBtn, &QPushButton::clicked, this, &BatchMode::onAddFolder);
    connect(removeBtn,    &QPushButton::clicked, this, &BatchMode::onRemoveSelected);
    connect(clearBtn,     &QPushButton::clicked, this, &BatchMode::onClearFiles);
}

// ── Operation panel (D2) ──────────────────────────────────────────────────────

void BatchMode::buildOperationPanel(QWidget* host) {
    auto* vlay = new QVBoxLayout(host);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Header
    auto* hdr = new QFrame;
    hdr->setProperty("role", "modeToolbar");
    hdr->setFixedHeight(28);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(10, 0, 10, 0);
    auto* hdrTitle = new QLabel(tr("OPERATION"));
    hdrTitle->setProperty("mono", true);
    hdrLay->addWidget(hdrTitle);
    hdrLay->addStretch(1);
    vlay->addWidget(hdr);

    // Operation combo
    auto* opRow = new QHBoxLayout;
    opRow->setContentsMargins(12, 8, 12, 4);
    m_opCombo = new QComboBox;
    m_opCombo->addItem(tr("Convert to Format"));     // OpConvert = 0
    m_opCombo->addItem(tr("Compress / Optimize"));   // OpCompress = 1
    m_opCombo->addItem(tr("Add Text Watermark"));    // OpWatermark = 2
    m_opCombo->addItem(tr("Export PDF/A"));          // OpExportPdfA = 3
    m_opCombo->addItem(tr("Merge PDFs"));            // OpMerge = 4
    m_opCombo->addItem(tr("OCR (searchable PDF)"));  // OpOCR = 5
    m_opCombo->addItem(tr("Redact (Search Pattern)")); // OpRedact = 6
    m_opCombo->addItem(tr("Preset Pipeline"));         // OpPresetPipeline = 7 (R26: append-only)
    opRow->addWidget(m_opCombo);
    vlay->addLayout(opRow);

    // Stacked config panels (one per operation)
    m_cfgStack = new QStackedWidget;
    m_cfgStack->setContentsMargins(12, 0, 12, 0);

    // ── Panel 0: Convert ──────────────────────────────────────────────────
    auto* pConvert = new QFrame;
    {
        auto* lay = new QVBoxLayout(pConvert);
        lay->addWidget(new QLabel(tr("Target Format:")));
        m_fmtCombo = new QComboBox;
        m_fmtCombo->addItem(tr("Word (.docx)"),    static_cast<int>(TargetFormat::Word));
        m_fmtCombo->addItem(tr("Excel (.xlsx)"),   static_cast<int>(TargetFormat::Excel));
        m_fmtCombo->addItem(tr("HTML (.html)"),    static_cast<int>(TargetFormat::Html));
        m_fmtCombo->addItem(tr("Image (.png)"),    static_cast<int>(TargetFormat::Image));
        m_fmtCombo->addItem(tr("CSV (.csv)"),      static_cast<int>(TargetFormat::Csv));
        lay->addWidget(m_fmtCombo);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_convertOutDir = new QLineEdit;
        m_convertOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_convertOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_convertOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pConvert);  // index 0

    // ── Panel 1: Compress ─────────────────────────────────────────────────
    auto* pCompress = new QFrame;
    {
        auto* lay = new QVBoxLayout(pCompress);
        lay->addWidget(new QLabel(tr("Image Quality:")));
        auto* sliderRow = new QHBoxLayout;
        m_qualitySlider = new QSlider(Qt::Horizontal);
        m_qualitySlider->setRange(10, 95);
        m_qualitySlider->setValue(75);
        m_qualityLabel = new QLabel(tr("75"));
        m_qualityLabel->setFixedWidth(30);
        sliderRow->addWidget(m_qualitySlider);
        sliderRow->addWidget(m_qualityLabel);
        lay->addLayout(sliderRow);
        connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int v) {
            m_qualityLabel->setText(QString::number(v));
        });

        lay->addWidget(new QLabel(tr("Target DPI (images):")));
        // §9.12 P1: user-configurable target DPI. Previously the value was
        // hard-coded (opts.targetDpi = 150 in the worker, with a static
        // "150 DPI" note here and no way to change it). The spin is the
        // single source of truth; the named presets are quick picks that
        // write into it.
        auto* dpiRow = new QHBoxLayout;
        m_dpiPresetCombo = new QComboBox;
        m_dpiPresetCombo->setObjectName(QStringLiteral("batchCompressDpiPreset"));
        m_dpiPresetCombo->addItem(tr("Low (72 DPI)"),    72);
        m_dpiPresetCombo->addItem(tr("Medium (150 DPI)"), 150);
        m_dpiPresetCombo->addItem(tr("High (300 DPI)"),  300);
        m_dpiPresetCombo->addItem(tr("Custom"),          -1);  // spin-only, set on manual edit
        m_dpiSpin = new QSpinBox;
        m_dpiSpin->setObjectName(QStringLiteral("batchCompressDpiSpin"));
        m_dpiSpin->setRange(kMinTargetDpi, kMaxTargetDpi);
        m_dpiSpin->setValue(kDefaultTargetDpi);
        m_dpiSpin->setSuffix(tr(" DPI"));
        m_dpiSpin->setToolTip(tr("Images are downsampled to this resolution.\n"
                                 "Lower DPI = smaller file, coarser images."));
        dpiRow->addWidget(m_dpiPresetCombo);
        dpiRow->addWidget(m_dpiSpin);
        dpiRow->addStretch(1);
        lay->addLayout(dpiRow);
        // Named preset → spin. The spin write is signal-blocked so the spin's
        // own handler below does not immediately flip the combo to "Custom".
        connect(m_dpiPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            const int dpi = m_dpiPresetCombo->itemData(idx).toInt();
            if (dpi > 0) {
                QSignalBlocker block(m_dpiSpin);
                m_dpiSpin->setValue(dpi);
            }
        });
        // Manual spin edit → no longer on a named preset.
        connect(m_dpiSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            const int presetDpi = m_dpiPresetCombo->currentData().toInt();
            if (presetDpi != v && m_dpiPresetCombo->currentIndex() != 3) {
                QSignalBlocker block(m_dpiPresetCombo);
                m_dpiPresetCombo->setCurrentIndex(3);  // Custom
            }
        });
        // The default DPI is 150 (the previous hard-coded value) — start the
        // combo on the matching named preset so it never disagrees with the
        // spin. (Plain construction leaves index 0 selected without ever
        // firing currentIndexChanged.)
        m_dpiPresetCombo->setCurrentIndex(1);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_compressOutDir = new QLineEdit;
        m_compressOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_compressOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_compressOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pCompress);  // index 1

    // ── Panel 2: Watermark ────────────────────────────────────────────────
    auto* pWatermark = new QFrame;
    {
        auto* lay = new QVBoxLayout(pWatermark);
        lay->addWidget(new QLabel(tr("Watermark Text:")));
        m_wmTextEdit = new QLineEdit;
        m_wmTextEdit->setPlaceholderText(tr("CONFIDENTIAL"));
        lay->addWidget(m_wmTextEdit);

        lay->addWidget(new QLabel(tr("Opacity (%):")));
        m_wmOpacity = new QSpinBox;
        m_wmOpacity->setRange(5, 100);
        m_wmOpacity->setValue(30);
        lay->addWidget(m_wmOpacity);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_wmOutDir = new QLineEdit;
        m_wmOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_wmOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_wmOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pWatermark);  // index 2

    // ── Panel 3: Export PDF/A ─────────────────────────────────────────────
    auto* pPdfA = new QFrame;
    {
        auto* lay = new QVBoxLayout(pPdfA);
        lay->addWidget(new QLabel(tr("Conformance Level:")));
        m_pdfaLevel = new QComboBox;
        m_pdfaLevel->addItem(tr("PDF/A-1B"), 1);
        m_pdfaLevel->addItem(tr("PDF/A-2B"), 2);
        m_pdfaLevel->addItem(tr("PDF/A-2U"), 4);
        m_pdfaLevel->addItem(tr("PDF/A-3B"), 3);
        m_pdfaLevel->addItem(tr("PDF/A-3U"), 5);
        lay->addWidget(m_pdfaLevel);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_pdfaOutDir = new QLineEdit;
        m_pdfaOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_pdfaOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_pdfaOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pPdfA);  // index 3

    // ── Panel 4: Merge ────────────────────────────────────────────────────
    auto* pMerge = new QFrame;
    {
        auto* lay = new QVBoxLayout(pMerge);
        auto* note = new QLabel(tr("All input files are merged, in list order, into a single PDF\n"
                                   "named after the first file (…_merged.pdf)."));
        note->setWordWrap(true);
        note->setStyleSheet("color:#71747a; font-size:10px;");
        lay->addWidget(note);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_mergeOutDir = new QLineEdit;
        m_mergeOutDir->setPlaceholderText(tr("Same folder as first source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_mergeOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_mergeOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pMerge);  // index 4

    // ── Panel 5: OCR ──────────────────────────────────────────────────────
    auto* pOCR = new QFrame;
    {
        auto* lay = new QVBoxLayout(pOCR);
        auto* note = new QLabel(tr("Each file is OCR'd and written as a searchable PDF "
                                   "(image + invisible text layer)."));
        note->setWordWrap(true);
        note->setStyleSheet("color:#71747a; font-size:10px;");
        lay->addWidget(note);

        // §9.12 P0: expose the OCR language in Batch mode — batch OCR was
        // hard-wired to English, a hard functional wall for any non-English
        // document set. Same source of truth as the interactive OCR path
        // (core/OcrTypes.h); the persisted "ocr/language" pref is the default.
        lay->addWidget(new QLabel(tr("Language:")));
        m_ocrLanguage = new QComboBox;
        const QString savedUi = QSettings().value(QStringLiteral("ocr/language"),
                                                  QStringLiteral("EN")).toString();
        int savedIdx = 0;
        for (int i = 0; i < ocrLanguages().size(); ++i) {
            const auto& l = ocrLanguages()[i];
            m_ocrLanguage->addItem(QString::fromUtf8(l.displayName),
                                   QString::fromLatin1(l.uiCode));
            if (savedUi.compare(QLatin1String(l.uiCode), Qt::CaseInsensitive) == 0)
                savedIdx = i;
        }
        m_ocrLanguage->setCurrentIndex(savedIdx);
        lay->addWidget(m_ocrLanguage);

        // N3 (pdf24 §1.3 skip-already-text pattern): the CLI's "-skipFilesWithText
        // / -skipPagesWithText … force OCR" switches mirrored in the GUI. A
        // skipped file is reported truthfully in the batch summary — never as
        // completed OCR work. Force overrides both skips.
        // Q3: the checkboxes READ their state from QSettings on construction,
        // so persistence is the advertised behavior — write back on every
        // change (write-on-change), or the user's choice silently evaporates
        // on the next app start.
        m_ocrSkipFilesWithText = new QCheckBox(tr("Skip files that already contain text"));
        m_ocrSkipFilesWithText->setChecked(
            QSettings().value(QStringLiteral("ocr/skipFilesWithText"), false).toBool());
        connect(m_ocrSkipFilesWithText, &QCheckBox::toggled, this, [](bool on) {
            QSettings().setValue(QStringLiteral("ocr/skipFilesWithText"), on);
        });
        lay->addWidget(m_ocrSkipFilesWithText);
        m_ocrSkipPagesWithText = new QCheckBox(tr("Skip pages that already contain text (keep original page)"));
        m_ocrSkipPagesWithText->setChecked(
            QSettings().value(QStringLiteral("ocr/skipPagesWithText"), false).toBool());
        connect(m_ocrSkipPagesWithText, &QCheckBox::toggled, this, [](bool on) {
            QSettings().setValue(QStringLiteral("ocr/skipPagesWithText"), on);
        });
        lay->addWidget(m_ocrSkipPagesWithText);
        m_ocrForceOcr = new QCheckBox(tr("Force OCR (override skip options)"));
        m_ocrForceOcr->setChecked(
            QSettings().value(QStringLiteral("ocr/forceOcr"), false).toBool());
        connect(m_ocrForceOcr, &QCheckBox::toggled, this, [](bool on) {
            QSettings().setValue(QStringLiteral("ocr/forceOcr"), on);
        });
        lay->addWidget(m_ocrForceOcr);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_ocrOutDir = new QLineEdit;
        m_ocrOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_ocrOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_ocrOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pOCR);  // index 5

    // ── Panel 6: Redact search-pattern ────────────────────────────────────
    auto* pRedact = new QFrame;
    {
        auto* lay = new QVBoxLayout(pRedact);
        // §9.12 P1: named PII quick-pick presets — the same PatternRedactor
        // built-in keys the interactive Redact mode offers ("email",
        // "phone-us", "ssn", …); only the three most common PII cases are
        // surfaced here as one-click checkboxes. Opt-in: a preset redacts
        // only when checked, in ADDITION to any free-form patterns below.
        lay->addWidget(new QLabel(tr("Quick Presets:")));
        auto* presetRow = new QHBoxLayout;
        struct NamedPreset { const char* key; const char* label; };
        const NamedPreset piiPresets[] = {
            { "email",    "Email" },
            { "phone-us", "Phone (US)" },
            { "ssn",      "SSN" },
        };
        for (const NamedPreset& p : piiPresets) {
            auto* chk = new QCheckBox(tr(p.label));
            chk->setObjectName(QStringLiteral("batchRedactPreset_%1").arg(QLatin1String(p.key)));
            chk->setProperty("presetKey", QString::fromLatin1(p.key));
            presetRow->addWidget(chk);
            m_redactPresets.append(chk);
        }
        presetRow->addStretch(1);
        lay->addLayout(presetRow);

        lay->addWidget(new QLabel(tr("Regex Patterns (comma-separated):")));
        m_redactPatterns = new QLineEdit;
        m_redactPatterns->setPlaceholderText(tr(R"(e.g. \d{3}-\d{2}-\d{4}, [\w.]+@[\w.]+)"));
        lay->addWidget(m_redactPatterns);
        auto* note = new QLabel(tr("Matches are excised from the content stream and flattened "
                                   "(not just covered)."));
        note->setWordWrap(true);
        note->setStyleSheet("color:#71747a; font-size:10px;");
        lay->addWidget(note);

        lay->addWidget(new QLabel(tr("Output Folder:")));
        auto* dirRow = new QHBoxLayout;
        m_redactOutDir = new QLineEdit;
        m_redactOutDir->setPlaceholderText(tr("Same folder as source"));
        auto* pickBtn = new QPushButton(tr("…"));
        pickBtn->setFixedWidth(28);
        dirRow->addWidget(m_redactOutDir);
        dirRow->addWidget(pickBtn);
        lay->addLayout(dirRow);
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
            if (!dir.isEmpty()) m_redactOutDir->setText(dir);
        });
        lay->addStretch(1);
    }
    m_cfgStack->addWidget(pRedact);  // index 6

    // ── Panel 7: Preset Pipeline (R26, batch-presets P1) ──────────────────
    auto* pPreset = new QFrame;
    buildPresetPanel(pPreset);
    m_cfgStack->addWidget(pPreset);  // index 7

    // F1 (SWEEP-W3-UI): a QStackedWidget's minimumSizeHint is the MAX over
    // every page — including the currently hidden ones — so the tallest
    // operation panel hardened this page's (and via the mode stack the
    // window's) minimum height past a 768-high viewport. Host the stack in a
    // scroll area: when the viewport is generous every panel still fits and
    // renders exactly as before (no scrollbars, identical geometry); on a
    // short viewport the active panel scrolls instead of inflating the
    // window minimum. The explicit small minimum is what lets the parent
    // layout shrink the area — a scroll area otherwise propagates its
    // widget's minimum.
    auto* cfgScroll = new QScrollArea;
    cfgScroll->setObjectName(QStringLiteral("batchConfigScroll"));
    cfgScroll->setWidgetResizable(true);
    cfgScroll->setFrameShape(QFrame::NoFrame);
    cfgScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cfgScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cfgScroll->setMinimumSize(180, 120);
    cfgScroll->setWidget(m_cfgStack);
    vlay->addWidget(cfgScroll, 1);

    connect(m_opCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatchMode::onOperationChanged);
}

// ── Progress panel (D3 + D4) ──────────────────────────────────────────────────

void BatchMode::buildProgressPanel(QWidget* host) {
    auto* vlay = new QVBoxLayout(host);
    vlay->setContentsMargins(12, 6, 12, 6);
    vlay->setSpacing(4);

    // Status + ETA row
    auto* statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel(tr("IDLE"));
    m_statusLabel->setProperty("mono", true);
    m_etaLabel = new QLabel;
    m_etaLabel->setProperty("mono", true);
    m_etaLabel->setStyleSheet("color:#71747a; font-size:10px;");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch(1);
    statusRow->addWidget(m_etaLabel);
    vlay->addLayout(statusRow);

    // Progress bars
    m_overallProgress = new QProgressBar;
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_overallProgress->setTextVisible(true);
    m_fileProgress = new QProgressBar;
    m_fileProgress->setRange(0, 100);
    m_fileProgress->setValue(0);
    m_fileProgress->setFormat(tr("Per-file: %p%"));
    vlay->addWidget(m_overallProgress);
    vlay->addWidget(m_fileProgress);

    // Button row
    auto* btnRow = new QHBoxLayout;
    m_runBtn = new QToolButton;
    m_runBtn->setText(tr("▶  RUN BATCH"));
    m_runBtn->setProperty("variant", "accent");
    m_runBtn->setFixedHeight(34);
    m_runBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setFixedHeight(34);

    m_exportLogBtn = new QPushButton(tr("Export Log"));
    m_exportLogBtn->setStyleSheet("padding:4px 12px; font-size:10px;");
    m_exportLogBtn->setVisible(false);

    btnRow->addWidget(m_runBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(m_exportLogBtn);
    vlay->addLayout(btnRow);

    // Log view
    m_logView = new QTextEdit;
    m_logView->setReadOnly(true);
    m_logView->setFixedHeight(130);
    m_logView->setStyleSheet(
        "font-family:'JetBrains Mono',monospace; font-size:10px;"
        "background:#1a1b1e; color:#d4d4d4; border-top:1px solid #393b40;");

    vlay->addWidget(m_logView);

    connect(m_runBtn,       &QToolButton::clicked, this, &BatchMode::onRunClicked);
    connect(m_cancelBtn,    &QPushButton::clicked, this, &BatchMode::onCancelClicked);
    connect(m_exportLogBtn, &QPushButton::clicked, this, &BatchMode::onExportLog);
}

// ── AppContext injection ───────────────────────────────────────────────────────

void BatchMode::setAppContext(const AppContext* ctx) {
    m_ctx = ctx;

    // U08: mark OCR language items whose traineddata is missing. A supported
    // language is never disabled outright — the data is seeded from the
    // bundled copy or downloaded on first use (Degraded → tooltip disclosure,
    // mirroring OcrEngine::initialize's actual behavior); only an unsupported
    // language becomes a disabled-with-explanation item. The user's selection
    // is never silently switched.
    if (m_ctx && m_ctx->capabilities && m_ocrLanguage) {
        auto* model = qobject_cast<QStandardItemModel*>(m_ocrLanguage->model());
        for (int i = 0; i < m_ocrLanguage->count(); ++i) {
            QStandardItem* item = model ? model->item(i) : nullptr;
            if (!item) continue;
            const gp::Capability c = m_ctx->capabilities->query(
                gp::CapId::OcrLanguageData, m_ocrLanguage->itemData(i).toString());
            if (c.status == gp::Availability::UnavailableRuntime) {
                item->setEnabled(false);
                item->setToolTip(gp::CapabilityRegistry::combineWhyNot(c));
            } else if (c.status == gp::Availability::Degraded) {
                item->setToolTip(gp::CapabilityRegistry::combineWhyNot(c));
            }
        }
    }

    // R26 (batch-presets): the preset panel is built before the context (and
    // its capability registry) arrives — re-render the selected preset's
    // step/capability disclosure with the real registry answers.
    if (m_presetSelected && m_presetStepsLabel)
        m_presetStepsLabel->setText(
            presetStepsDisplayText(m_selectedPreset,
                                   m_ctx ? m_ctx->capabilities.get() : nullptr));
}

// ── Drag-drop (D1) ────────────────────────────────────────────────────────────

void BatchMode::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        // Accept only if at least one URL is a PDF
        for (const QUrl& url : e->mimeData()->urls()) {
            if (url.isLocalFile() && url.toLocalFile().endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
                e->acceptProposedAction();
                return;
            }
        }
    }
    e->ignore();
}

void BatchMode::dropEvent(QDropEvent* e) {
    QStringList paths;
    for (const QUrl& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            if (path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
                paths << path;
        }
    }
    if (!paths.isEmpty()) {
        addFilePaths(paths);
        e->acceptProposedAction();
    }
}

// ── File list helpers (D1) ────────────────────────────────────────────────────

void BatchMode::addFilePaths(const QStringList& paths) {
    QSet<QString> existing;
    for (int i = 0; i < m_fileModel->rowCount(); ++i)
        existing.insert(m_fileModel->item(i)->data(Qt::UserRole).toString());

    for (const QString& path : paths) {
        if (existing.contains(path)) continue;
        auto* item = new QStandardItem(QFileInfo(path).fileName());
        item->setData(path, Qt::UserRole);
        item->setToolTip(path);
        m_fileModel->appendRow(item);
        existing.insert(path);
    }
    syncFileList();
}

void BatchMode::syncFileList() {
    m_filesToProcess.clear();
    for (int i = 0; i < m_fileModel->rowCount(); ++i)
        m_filesToProcess << m_fileModel->item(i)->data(Qt::UserRole).toString();

    m_fileCountLabel->setText(tr("%1 file%2")
        .arg(m_filesToProcess.size())
        .arg(m_filesToProcess.size() == 1 ? QString() : tr("s")));
}

void BatchMode::onAddFiles() {
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Add PDF Files"), {},
        tr("PDF Files (*.pdf);;All Files (*)"));
    if (!paths.isEmpty())
        addFilePaths(paths);
}

void BatchMode::onAddFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (dir.isEmpty()) return;
    QStringList paths;
    QDir d(dir);
    const auto entries = d.entryInfoList(QStringList() << "*.pdf" << "*.PDF",
                                         QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries)
        paths << fi.absoluteFilePath();
    if (!paths.isEmpty())
        addFilePaths(paths);
    else
        QMessageBox::information(this, tr("No PDFs Found"),
            tr("No PDF files found in: %1").arg(dir));
}

void BatchMode::onClearFiles() {
    m_fileModel->clear();
    syncFileList();
}

void BatchMode::onRemoveSelected() {
    QList<int> rows;
    for (const QModelIndex& idx : m_fileView->selectionModel()->selectedIndexes())
        rows.prepend(idx.row()); // reverse order to not invalidate indices
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
        m_fileModel->removeRow(row);
    syncFileList();
}

void BatchMode::onOperationChanged(int index) {
    if (m_cfgStack)
        m_cfgStack->setCurrentIndex(index);
}

// ── Hot folder (Phase 3) ────────────────────────────────────────────────────────

// static — identity key for a file: name + last-modified time. A file is only
// auto-ingested once unless it is replaced/modified.
QString BatchMode::hotFileKey(const QFileInfo& fi) {
    return fi.fileName() + QLatin1Char('|')
         + QString::number(fi.lastModified().toMSecsSinceEpoch());
}

void BatchMode::buildHotFolderSection(QVBoxLayout* btnLay) {
    auto* hotLabel = new QLabel(tr("HOT FOLDER"));
    hotLabel->setProperty("mono", true);
    hotLabel->setStyleSheet("margin-top:6px;");
    btnLay->addWidget(hotLabel);

    m_hotFolderCheck = new QCheckBox(tr("Watch folder (hot folder)"));
    m_hotFolderCheck->setToolTip(tr("Auto-add new PDFs dropped into a folder"));
    btnLay->addWidget(m_hotFolderCheck);

    auto* hotRow = new QHBoxLayout;
    m_hotFolderEdit = new QLineEdit;
    m_hotFolderEdit->setReadOnly(true);
    m_hotFolderEdit->setPlaceholderText(tr("No folder watched"));
    auto* hotBrowse = new QPushButton(tr("…"));
    hotBrowse->setFixedWidth(28);
    hotRow->addWidget(m_hotFolderEdit);
    hotRow->addWidget(hotBrowse);
    btnLay->addLayout(hotRow);

    m_hotAutoRunCheck = new QCheckBox(tr("Auto-run on new files"));
    m_hotAutoRunCheck->setToolTip(tr("Start the batch automatically when new files arrive"));
    btnLay->addWidget(m_hotAutoRunCheck);

    connect(m_hotFolderCheck, &QCheckBox::toggled, this, &BatchMode::onToggleHotFolder);
    // Browse re-picks the folder by re-triggering the toggle flow.
    connect(hotBrowse, &QPushButton::clicked, this, [this]() {
        if (m_hotFolderCheck->isChecked())
            m_hotFolderCheck->setChecked(false);  // stop current watch
        m_hotFolderCheck->setChecked(true);       // prompt + start
    });
}

void BatchMode::onToggleHotFolder() {
    const bool on = m_hotFolderCheck && m_hotFolderCheck->isChecked();

    if (on) {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Hot Folder to Watch"));
        if (dir.isEmpty()) {
            // User cancelled — revert the checkbox without recursing into stop logic.
            QSignalBlocker block(m_hotFolderCheck);
            m_hotFolderCheck->setChecked(false);
            return;
        }
        m_hotFolderPath = dir;
        m_hotFolderEdit->setText(dir);

        // Seed the processed set with existing files so only NEW files trigger.
        m_hotProcessed.clear();
        const auto seed = QDir(dir).entryInfoList(QStringList() << "*.pdf" << "*.PDF",
                                                  QDir::Files);
        for (const QFileInfo& fi : seed)
            m_hotProcessed.insert(hotFileKey(fi));

        if (!m_hotFolderDebounce) {
            m_hotFolderDebounce = new QTimer(this);
            m_hotFolderDebounce->setSingleShot(true);
            m_hotFolderDebounce->setInterval(500);
            connect(m_hotFolderDebounce, &QTimer::timeout, this,
                    [this]() { onHotFolderChanged(m_hotFolderPath); });
        }
        if (!m_hotFolderWatcher) {
            m_hotFolderWatcher = new QFileSystemWatcher(this);
            connect(m_hotFolderWatcher, &QFileSystemWatcher::directoryChanged, this,
                    [this](const QString&) { if (m_hotFolderDebounce) m_hotFolderDebounce->start(); });
        }
        m_hotFolderWatcher->addPath(dir);
        appendLog(tr("Hot folder watching: %1").arg(dir), "#5b9bd5");
    } else {
        // Toggled off — tear down watcher + debounce and clear state.
        if (m_hotFolderWatcher) {
            delete m_hotFolderWatcher;
            m_hotFolderWatcher = nullptr;
        }
        if (m_hotFolderDebounce) {
            m_hotFolderDebounce->stop();
            delete m_hotFolderDebounce;
            m_hotFolderDebounce = nullptr;
        }
        m_hotFolderPath.clear();
        m_hotProcessed.clear();
        if (m_hotFolderEdit) m_hotFolderEdit->clear();
        appendLog(tr("Hot folder watching stopped."), "#71747a");
    }
}

void BatchMode::onHotFolderChanged(const QString& path) {
    if (path.isEmpty()) return;

    const auto entries = QDir(path).entryInfoList(QStringList() << "*.pdf" << "*.PDF",
                                                  QDir::Files, QDir::Name);
    QStringList newFiles;
    for (const QFileInfo& fi : entries) {
        const QString key = hotFileKey(fi);
        if (!m_hotProcessed.contains(key)) {
            m_hotProcessed.insert(key);
            newFiles << fi.absoluteFilePath();
        }
    }
    if (newFiles.isEmpty()) return;

    addFilePaths(newFiles);
    appendLog(tr("Hot folder: ingested %1 new file%2.")
                  .arg(newFiles.size())
                  .arg(newFiles.size() == 1 ? QString() : tr("s")),
              "#5b9bd5");

    if (m_hotAutoRunCheck && m_hotAutoRunCheck->isChecked() && !m_watcher.isRunning())
        onRunClicked();
}

// ── Output path resolution ────────────────────────────────────────────────────

QString BatchMode::resolveOutputPath(const QString& inputPath) const {
    int opIdx = m_opCombo ? m_opCombo->currentIndex() : 0;

    // Determine output directory
    QString outDir;
    QString outName = QFileInfo(inputPath).completeBaseName();

    auto pickOutDir = [](const QLineEdit* edit) -> QString {
        return edit ? edit->text().trimmed() : QString();
    };

    switch (opIdx) {
    case OpConvert: {
        outDir = pickOutDir(m_convertOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        // Extension from format
        int fmtIdx = m_fmtCombo ? m_fmtCombo->currentIndex() : 0;
        const QStringList exts = { ".docx", ".xlsx", ".html", ".png", ".csv" };
        QString ext = (fmtIdx >= 0 && fmtIdx < exts.size()) ? exts[fmtIdx] : ".out";
        return QDir(outDir).filePath(outName + ext);
    }
    case OpCompress:
        outDir = pickOutDir(m_compressOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        return QDir(outDir).filePath(outName + "_compressed.pdf");
    case OpWatermark:
        outDir = pickOutDir(m_wmOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        return QDir(outDir).filePath(outName + "_watermarked.pdf");
    case OpExportPdfA:
        outDir = pickOutDir(m_pdfaOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        return QDir(outDir).filePath(outName + "_pdfa.pdf");
    case OpOCR:
        outDir = pickOutDir(m_ocrOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        return QDir(outDir).filePath(outName + "_ocr.pdf");
    case OpRedact:
        outDir = pickOutDir(m_redactOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        return QDir(outDir).filePath(outName + "_redacted.pdf");
    case OpPresetPipeline: {
        // R26: the naming template is resolved exactly as the worker resolves
        // it (same pure function, same file index from the list order), so
        // the overwrite pre-check and the actual commit always agree.
        if (!m_presetSelected) return {};
        outDir = pickOutDir(m_presetOutDir);
        if (outDir.isEmpty()) outDir = QFileInfo(inputPath).absolutePath();
        const int n = m_filesToProcess.indexOf(inputPath) + 1;
        QString name;
        QString namingErr;
        if (!BatchPresetSchema::resolveNaming(m_selectedPreset.outputNaming, outName,
                                              m_selectedPreset.id, n,
                                              QDate::currentDate(), &name, &namingErr))
            return {};
        return QDir(outDir).filePath(name);
    }
    default:
        return {};
    }
}

bool BatchMode::confirmOverwrite(const QString& path) {
    if (!QFileInfo::exists(path)) return true;
    auto btn = QMessageBox::question(this,
        tr("Overwrite?"),
        tr("Output file already exists:\n%1\n\nOverwrite it?").arg(path),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return btn == QMessageBox::Yes;
}

// ── R26 (batch-presets P1): the per-file preset candidate chain ───────────────
// plan §3.1: per input file, steps run as a CANDIDATE CHAIN — each mutating
// step writes a unique SafeSave temp candidate, the candidate is validated
// (reopens as a PDF; page count equals the input's — every P1 preset op is
// page-count invariant), and the final candidate is committed ONCE through
// SafeSave::commitFileToDestination. A failed step aborts that file's chain,
// every intermediate is removed on every outcome, and the original is left
// byte-identical on failure. A preset changes WHAT runs — never HOW results
// are accounted: results flow through the same mapped pipeline + G12 ledger.

namespace {

// ── emergence E-2 (SWEEP-W3-EMERGENCE §1b) ──────────────────────────────────
// OcrEngine::initialize fails console-only when the language data is missing
// and the EFFECTIVE ocr/allowNetworkDownload value refuses the download. The
// batch worker must honor that failure — and the whyNot it reports must name
// the policy instead of a bare engine error. The local-availability
// discriminator mirrors the engine's own gate: the AppLocalData pack, the
// seeded bundled pack, then the effective download decision.
bool ocrLanguageDataAvailableLocally(const QString& lang)
{
    const QString filename = lang.trimmed().toLower() + QStringLiteral(".traineddata");
    if (filename == QLatin1String(".traineddata")) return false;
    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + QStringLiteral("/tessdata/");
    if (QFileInfo::exists(appData + filename)) return true;
    const QString bundled = QApplication::applicationDirPath()
        + QStringLiteral("/tessdata/") + filename;
    return QFileInfo::exists(bundled);
}

// The honest per-file failure reason for a failed OcrEngine::initialize in
// the batch worker: under a refused download it names the deciding half
// (machine policy when the key is managed, the user setting otherwise) and
// states plainly that NO output was written — never a silent success.
QString ocrInitFailureDetail(const QString& lang)
{
    auto& policy = PolicyController::instance();
    policy.ensureLoaded();
    const QString key = QStringLiteral("ocr/allowNetworkDownload");
    const bool downloadAllowed = policy.effectiveValue(
        key, QSettings().value(key, false)).toBool();
    if (!downloadAllowed && !ocrLanguageDataAvailableLocally(lang)) {
        const QString decidedBy = policy.isManaged(key)
            ? QStringLiteral("machine policy")
            : QStringLiteral("the OCR download setting");
        return QStringLiteral(
            "OCR language data for '%1' is not available and the required "
            "download was refused by %2 (ocr/allowNetworkDownload) — the file "
            "was NOT OCRed and no output was written. Install the '%1' "
            "language pack or allow the OCR download, then retry.")
            .arg(lang, decidedBy);
    }
    return QStringLiteral(
        "OCR engine initialization failed for language '%1' — the file was "
        "not OCRed and no output was written.").arg(lang);
}

// Schema level strings → the engine's conformance codes — the SAME codes the
// Export PDF/A combo feeds exportPdfA (1=1B, 2=2B, 4=2U, 3=3B, 5=3U;
// PoDoFoBackend::exportPdfA's switch).
int pdfaLevelCode(const QString& level) {
    if (level == QLatin1String("1b")) return 1;
    if (level == QLatin1String("2b")) return 2;
    if (level == QLatin1String("2u")) return 4;
    if (level == QLatin1String("3b")) return 3;
    if (level == QLatin1String("3u")) return 5;
    return 2;
}

// R26-P2 (plan §2.2): the schema's six named bates positions → the engine's
// HeaderFooterOptions::Position. Validation guarantees one of the six values;
// the fall-through default is the schema default (bottom-right).
HeaderFooterOptions::Position batesPositionFromSchema(const QString& position) {
    if (position == QLatin1String("top-left"))     return HeaderFooterOptions::Position::TopLeft;
    if (position == QLatin1String("top-center"))   return HeaderFooterOptions::Position::TopCenter;
    if (position == QLatin1String("top-right"))    return HeaderFooterOptions::Position::TopRight;
    if (position == QLatin1String("bottom-left"))  return HeaderFooterOptions::Position::BottomLeft;
    if (position == QLatin1String("bottom-center"))return HeaderFooterOptions::Position::BottomCenter;
    return HeaderFooterOptions::Position::BottomRight;
}

PdfAConformance pdfaConformance(const QString& level) {
    if (level == QLatin1String("1b")) return PdfAConformance::PDF_A_1B;
    if (level == QLatin1String("2u")) return PdfAConformance::PDF_A_2U;
    if (level == QLatin1String("3b")) return PdfAConformance::PDF_A_3B;
    if (level == QLatin1String("3u")) return PdfAConformance::PDF_A_3U;
    return PdfAConformance::PDF_A_2B;
}

// One MUTATING preset step against a freshly loaded editor, writing `dest`.
// The engine seams are exactly the single-op batch worker's set plus the
// strip-metadata sanitize pass and — R26-P2 (plan §4.1) — the bates path
// mutator. `currentLink` is the chain link the editor was loaded from (bates
// stages its own candidate from it); `effectiveBatesStart` is the run-state
// resolved start (N1); `lastBatesOut` receives the last number stamped.
bool runPresetMutatingStep(PdfEditorEngine& editor, const BatchPresetStep& step,
                           const QString& currentLink, const QString& dest,
                           int effectiveBatesStart, int* lastBatesOut,
                           QString* techDetail) {
    bool ok = false;
    if (step.op == QLatin1String("compress")) {
        OptimizeOptions opts;
        opts.jpegQuality = step.params.value(QStringLiteral("quality"), 75).toInt();
        opts.targetDpi   = BatchMode::resolveCompressTargetDpi(
            step.params.value(QStringLiteral("targetDpi"), 150).toInt());
        ok = editor.optimizeDocument(dest, opts);
    } else if (step.op == QLatin1String("watermark")) {
        TextWatermarkOptions opts;
        opts.text    = step.params.value(QStringLiteral("text"), QStringLiteral("CONFIDENTIAL")).toString();
        opts.opacity = step.params.value(QStringLiteral("opacity"), 30).toInt() / 100.0;
        ok = editor.addTextWatermark(opts);
        if (ok) ok = editor.saveDocument(dest);
    } else if (step.op == QLatin1String("pdfa-export")) {
        ok = editor.exportPdfA(dest, pdfaLevelCode(
            step.params.value(QStringLiteral("level"), QStringLiteral("2b")).toString()));
    } else if (step.op == QLatin1String("strip-metadata")) {
        // Resident mutation FIRST (metadata), then ONE terminal write — the
        // sanitize pass writes the already-cleared document.
        const bool clearInfoDict = step.params.value(QStringLiteral("clearInfoDict"), true).toBool();
        const bool sanitize      = step.params.value(QStringLiteral("sanitize"), true).toBool();
        ok = true;
        if (clearInfoDict)
            ok = editor.setMetadata(PdfMetadata{});
        if (ok)
            ok = sanitize ? editor.sanitizeDocument(dest) : editor.saveDocument(dest);
    } else if (step.op == QLatin1String("redact")) {
        const QStringList patterns = BatchMode::effectiveRedactPatterns(
            step.params.value(QStringLiteral("presets")).toStringList(),
            step.params.value(QStringLiteral("patterns")).toStringList());
        ok = editor.applyPatternRedactionsMulti(patterns, QList<int>(), dest);
    } else if (step.op == QLatin1String("bates")) {
        // R26-P2 (plan §4.1): bates is a PATH-based engine mutator (contract:
        // load→mutate→save over ONE path) and cannot write a distinct
        // destination from a resident document — the backend refuses a
        // different path while another file is loaded. The chain link is
        // therefore staged by copying the current link onto the reserved
        // candidate, and the copy is stamped in place through its OWN engine
        // instance. The candidate discipline is unchanged: on any failure the
        // candidate is discarded and the original is untouched.
        QFile::remove(dest);
        if (!QFile::copy(currentLink, dest)) {
            if (techDetail)
                *techDetail = QStringLiteral("bates step: could not stage the chain "
                                             "candidate from %1").arg(currentLink);
            return false;
        }
        BatesNumberingOptions opts;
        opts.prefix      = step.params.value(QStringLiteral("prefix")).toString();
        opts.suffix      = step.params.value(QStringLiteral("suffix")).toString();
        opts.startNumber = effectiveBatesStart;
        opts.digitCount  = step.params.value(QStringLiteral("digitCount"), 6).toInt();
        opts.position    = batesPositionFromSchema(
            step.params.value(QStringLiteral("position")).toString());
        PdfEditorEngine batesEditor;
        // The backend is created by the load (BackendRouter); loading the
        // staged candidate itself makes the mutator's residency guard pass —
        // the engine then stamps ITS OWN file in place.
        if (!batesEditor.loadDocumentForEditing(dest)) {
            if (techDetail)
                *techDetail = batesEditor.lastError().technicalDetails;
            return false;
        }
        ok = batesEditor.applyBatesNumbering(dest, opts, lastBatesOut);
        if (!ok && techDetail)
            *techDetail = batesEditor.lastError().technicalDetails;
    }
    if (!ok && techDetail && techDetail->isEmpty())
        *techDetail = editor.lastError().technicalDetails;
    return ok;
}

// pdfa-check: a NON-mutating step — validates the candidate in flight and
// never touches the destination (plan §3.1 step 4). A failed check fails the
// file honestly: a "web-optimize" that cannot pass its declared PDF/A level
// reports the failure instead of shipping an unverified file. When no
// capability registry was available to gate the step, the validator's own
// unavailable-report still fails the file — never a silent skip.
bool runPresetCheckStep(const QString& current, const BatchPresetStep& step,
                        QString* techDetail) {
    const QString level =
        step.params.value(QStringLiteral("level"), QStringLiteral("2b")).toString();
    const auto report = VeraPdfValidator::validate(current, pdfaConformance(level));
    if (!report.validatorAvailable) {
        *techDetail = QStringLiteral("PDF/A-%1 check could not run — %2")
                          .arg(level, report.errorMessage.isEmpty()
                                          ? QStringLiteral("the veraPDF validator is not available")
                                          : report.errorMessage);
        return false;
    }
    if (!report.isValid) {
        QStringList clauses;
        for (const auto& v : report.violations) {
            clauses << v.ruleId;
            if (clauses.size() >= 5) break;
        }
        *techDetail = QStringLiteral("PDF/A-%1 check failed — %2 rule violation(s)%3")
                          .arg(level).arg(report.violations.size())
                          .arg(clauses.isEmpty()
                                   ? QString()
                                   : QStringLiteral(": %1").arg(clauses.join(QStringLiteral(", "))));
        return false;
    }
    return true;
}

bool runPresetChain(const QString& inputPath, const QString& outputPath,
                    const BatchPreset& preset, QMutex* engineMutex,
                    PresetRunState* runState, QList<BatchStepResult>* stepRecords,
                    QString* techDetail) {
    // PDFium (QPdfDocument) is not thread-safe: the read-side probes of the
    // chain are serialized behind the SAME engine mutex the batch OCR path
    // uses for its PDFium probes (PoDoFo writer steps stay parallel).
    QMutexLocker pdfiumLock(engineMutex);
    QPdfDocument baseline;
    baseline.load(inputPath);
    if (baseline.status() != QPdfDocument::Status::Ready || baseline.pageCount() <= 0) {
        *techDetail = QStringLiteral("Failed to open PDF: %1").arg(inputPath);
        if (stepRecords && !preset.steps.isEmpty()) {
            // The chain never started — every step is a failed record with the
            // same honest reason (the file failed, not any single step).
            BatchStepResult record;
            record.stepIndex = 0;
            record.op = preset.steps.first().op;
            record.label = preset.steps.first().label;
            record.status = BatchStepResult::Status::Failed;
            record.detail = *techDetail;
            stepRecords->append(record);
        }
        return false;
    }
    const int expectedPages = baseline.pageCount();
    pdfiumLock.unlock();

    QString current = inputPath;
    QStringList intermediates;
    bool ok = true;
    int failedAt = -1;
    for (int i = 0; i < preset.steps.size() && ok; ++i) {
        const BatchPresetStep& step = preset.steps.at(i);
        BatchStepResult record;
        record.stepIndex = i;
        record.op = step.op;
        record.label = step.label;

        // R26-P2 (N1): bates only ever runs on the ordered lane. A bates step
        // reaching the parallel chain is an internal scheduling error — fail
        // the file honestly instead of stamping numbers out of order.
        if (step.op == QLatin1String("bates") && !runState) {
            *techDetail = QStringLiteral(
                "bates step on the parallel lane — a bates-bearing preset must "
                "run on the ordered lane (internal scheduling error)");
            ok = false;
        }

        // A check step validates the candidate in flight — no candidate of
        // its own.
        if (ok && step.op == QLatin1String("pdfa-check")) {
            ok = runPresetCheckStep(current, step, techDetail);
        } else if (ok) {
            // R26-P2 (N1): the effective start continues the run's sequence —
            // the last SUCCESSFUL stamp + 1 — and the first file (or the run
            // after a restart) uses the step's explicit startNumber (default
            // 1). A failed file's candidate is discarded and never burns a
            // number.
            int effectiveBatesStart = 1;
            int lastBatesOut = -1;
            if (step.op == QLatin1String("bates")) {
                effectiveBatesStart = runState->batesStarted
                    ? runState->lastBatesOut + 1
                    : step.params.value(QStringLiteral("startNumber"), 1).toInt();
            }

            QString candidate;
            QString candidateErr;
            if (!SafeSave::makeUniqueCandidate(&candidate, &candidateErr)) {
                *techDetail = candidateErr;
                ok = false;
            }
            if (ok) {
                intermediates.append(candidate);
                {
                    // Fresh per-file engine per step — the same TRUE-parallel
                    // pattern the single-op editor workers use (self-contained
                    // load/save).
                    PdfEditorEngine editor;
                    if (!editor.loadDocumentForEditing(current)) {
                        *techDetail = editor.lastError().technicalDetails;
                        ok = false;
                    } else {
                        ok = runPresetMutatingStep(editor, step, current, candidate,
                                                   effectiveBatesStart, &lastBatesOut,
                                                   techDetail);
                    }
                }
                if (ok) {
                    // Validate the candidate: it must open as a PDF and
                    // preserve the page count (bates is an overlay — the
                    // invariant holds for it too). A step that breaks the
                    // document never becomes the new chain link.
                    QMutexLocker lock(engineMutex);
                    QPdfDocument probe;
                    probe.load(candidate);
                    if (probe.status() != QPdfDocument::Status::Ready
                        || probe.pageCount() != expectedPages) {
                        *techDetail = QStringLiteral("step %1 (%2) produced an invalid candidate — "
                                                     "the file is left unchanged")
                                          .arg(i + 1).arg(step.op);
                        ok = false;
                    }
                }
                if (ok)
                    current = candidate;
                if (ok && step.op == QLatin1String("bates") && runState) {
                    runState->batesStarted = true;
                    runState->lastBatesOut = lastBatesOut;
                }
                if (step.op == QLatin1String("bates")) {
                    record.firstBates = effectiveBatesStart;
                    record.lastBates  = ok ? lastBatesOut : -1;
                }
            }
        }

        record.status = ok ? BatchStepResult::Status::Ok : BatchStepResult::Status::Failed;
        if (!ok) {
            record.detail = *techDetail;
            failedAt = i;
        }
        if (stepRecords)
            stepRecords->append(record);
    }

    if (ok) {
        QString commitErr;
        ok = SafeSave::commitFileToDestination(current, outputPath, &commitErr);
        if (!ok && techDetail->isEmpty())
            *techDetail = commitErr;
    } else if (stepRecords && failedAt >= 0) {
        // Steps after the failure were never attempted — honest skipped rows
        // (plan §3 status set), never a silently truncated record list.
        for (int j = failedAt + 1; j < preset.steps.size(); ++j) {
            BatchStepResult skipped;
            skipped.stepIndex = j;
            skipped.op = preset.steps.at(j).op;
            skipped.label = preset.steps.at(j).label;
            skipped.status = BatchStepResult::Status::Skipped;
            skipped.detail = QStringLiteral("not attempted — the chain aborted at step %1")
                                 .arg(failedAt + 1);
            stepRecords->append(skipped);
        }
    }

    // Intermediates are removed on EVERY outcome (the committed candidate
    // included — commitFileToDestination copied it to the destination).
    for (const QString& path : intermediates)
        QFile::remove(path);
    return ok;
}

} // namespace

// ── Execution engine (D3) ─────────────────────────────────────────────────────

void BatchMode::onRunClicked() {
    if (m_filesToProcess.isEmpty()) {
        QMessageBox::information(this, tr("No Files"), tr("Add PDF files to the list first."));
        return;
    }

    int opIdx = m_opCombo ? m_opCombo->currentIndex() : 0;

    // Guard: AppContext must be set for engine operations
    if (!m_ctx) {
        QMessageBox::warning(this, tr("Not Ready"),
            tr("The processing engines are not yet available.\n\n"
               "Please open a document first, then try again."));
        return;
    }

    // §9.12 P1: Merge is a single combined output over all files — it does not
    // fit the per-file mapped pipeline, but it must not run synchronously on
    // the GUI thread either (a large merge froze the whole app here). This
    // branch only stages the run (guards + output path + overwrite confirm);
    // the worker itself starts on the QtConcurrent pool at the end of this
    // function, behind the SAME QFutureWatcher and per-result accounting the
    // per-file ops use.
    QString mergeOutPath;
    if (opIdx == OpMerge) {
        if (m_filesToProcess.size() < 2) {
            QMessageBox::information(this, tr("Merge PDFs"),
                tr("Add at least two PDF files to merge."));
            return;
        }
        const QString first = m_filesToProcess.first();
        QString outDir = m_mergeOutDir ? m_mergeOutDir->text().trimmed() : QString();
        if (outDir.isEmpty()) outDir = QFileInfo(first).absolutePath();
        mergeOutPath = QDir(outDir).filePath(
            QFileInfo(first).completeBaseName() + QStringLiteral("_merged.pdf"));
        if (!confirmOverwrite(mergeOutPath)) return;
    }

    // Redact requires at least one effective pattern — a checked named
    // preset (§9.12 P1) or a free-form regex entry.
    if (opIdx == OpRedact &&
        effectiveRedactPatterns(checkedRedactPresetKeys(),
                                m_redactPatterns
                                    ? m_redactPatterns->text().split(QLatin1Char(','),
                                                                     Qt::SkipEmptyParts)
                                    : QStringList()).isEmpty()) {
        QMessageBox::information(this, tr("No Patterns"),
            tr("Check at least one quick preset or enter one or more "
               "comma-separated regex patterns to redact."));
        return;
    }

    // R26 (batch-presets): a preset run needs a SELECTED preset. A preset
    // changes WHAT runs, not HOW results are accounted — the run below flows
    // through the same per-file mapped pipeline, SafeSave commit and G12
    // exactly-once accounting as every other op.
    QString presetBlocker;
    if (opIdx == OpPresetPipeline) {
        if (!m_presetSelected) {
            QMessageBox::information(this, tr("No Preset Selected"),
                tr("Select a preset to run, or configure an operation and "
                   "choose \u201cSave as preset\u2026\u201d to create one."));
            return;
        }
        // Run gate: steps present, minAppVersion satisfied, and every step's
        // capability re-queried NOW (GUI thread, cached probes). An
        // unavailable step blocks the run with the registry's whyNot +
        // alternative — disclosed per file below, never silently skipped,
        // never faked.
        presetBlocker = presetRunBlocker();
    }

    // AR-8 D4: Pre-check output paths for overwrite conflicts.
    // For single-file runs, show a per-file dialog.
    // For multi-file runs, collect all conflicting paths and show one summary
    // dialog rather than flooding the user with N dialogs.
    // W1-01: a preset's onConflict "overwrite" NEVER bypasses this
    // confirmation — a preset is data, and the file it rides in is not the
    // user's answer to "may I destroy this output file?". "overwrite" maps to
    // the same interactive path as "ask": the user is asked exactly once (the
    // AR-8 summary dialog for multi-file runs), and declining cancels the run.
    // There is no silent overwrite.
    if (m_filesToProcess.size() == 1) {
        QString out = resolveOutputPath(m_filesToProcess.first());
        if (!out.isEmpty() && !confirmOverwrite(out)) return;
    } else if (m_filesToProcess.size() > 1) {
        QStringList willOverwrite;
        for (const QString& src : m_filesToProcess) {
            QString out = resolveOutputPath(src);
            if (!out.isEmpty() && QFileInfo::exists(out))
                willOverwrite << QFileInfo(out).fileName();
        }
        if (!willOverwrite.isEmpty()) {
            const auto btn = QMessageBox::warning(
                this,
                tr("Overwrite Existing Files?"),
                tr("%1 output file(s) already exist and will be overwritten:\n\n%2\n\n"
                   "This operation cannot be undone. Continue?")
                    .arg(willOverwrite.size())
                    .arg(willOverwrite.join(QStringLiteral("\n"))),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel);
            if (btn != QMessageBox::Yes) return;
        }
    }

    // Reset state
    m_overallProgress->setRange(0, m_filesToProcess.size());
    m_overallProgress->setValue(0);
    m_fileProgress->setValue(0);
    m_logView->clear();
    m_errorLog.clear();
    m_successCount = 0;
    m_failCount    = 0;
    m_skipCount    = 0;
    m_mergeOutputPath.clear();   // F2a-F1: named only by a merge that commits
    m_accountedIndices.clear();   // G12: per-run exactly-once accounting ledger
    m_lastRunResults.clear();     // R26-P2: fresh per-run result records
    m_exportLogBtn->setVisible(false);
    m_runBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_statusLabel->setText(tr("Running — %1 files").arg(m_filesToProcess.size()));
    m_etaLabel->clear();
    appendLog(tr("Starting batch: %1 files — operation: %2")
        .arg(m_filesToProcess.size())
        .arg(m_opCombo->currentText()));
    m_batchTimer.restart();

    // Capture config values for the worker lambda (all GUI data captured before worker starts)
    const int capturedOp = opIdx;
    const auto capturedCtx = m_ctx;

    // Convert config
    const TargetFormat capturedFmt = m_fmtCombo
        ? static_cast<TargetFormat>(m_fmtCombo->currentData().toInt())
        : TargetFormat::Word;

    // Compress config
    const int capturedQuality = m_qualitySlider ? m_qualitySlider->value() : 75;
    // §9.12 P1: the user-chosen target DPI (clamped through the named seam;
    // was hard-coded to 150).
    const int capturedTargetDpi =
        resolveCompressTargetDpi(m_dpiSpin ? m_dpiSpin->value() : kDefaultTargetDpi);

    // Watermark config
    const QString capturedWmText = m_wmTextEdit
        ? (m_wmTextEdit->text().trimmed().isEmpty() ? tr("CONFIDENTIAL") : m_wmTextEdit->text().trimmed())
        : tr("CONFIDENTIAL");
    const int capturedWmOpacity = m_wmOpacity ? m_wmOpacity->value() : 30;

    // PDF/A config
    const int capturedPdfALevel = m_pdfaLevel
        ? m_pdfaLevel->currentData().toInt()
        : 2;

    // Build output path resolver (capture by value so it's safe in worker thread)
    // We capture the config values directly in the lambda instead of calling resolveOutputPath
    // (which touches GUI objects — forbidden from worker threads).
    QStringList capturedFiles = m_filesToProcess;
    const QString capturedConvertOutDir = m_convertOutDir ? m_convertOutDir->text().trimmed() : QString();
    const QString capturedCompressOutDir = m_compressOutDir ? m_compressOutDir->text().trimmed() : QString();
    const QString capturedWmOutDir = m_wmOutDir ? m_wmOutDir->text().trimmed() : QString();
    const QString capturedPdfAOutDir = m_pdfaOutDir ? m_pdfaOutDir->text().trimmed() : QString();
    const int capturedFmtIdx = m_fmtCombo ? m_fmtCombo->currentIndex() : 0;

    // OCR config
    const QString capturedOcrOutDir = m_ocrOutDir ? m_ocrOutDir->text().trimmed() : QString();
    // §9.12 P0: resolve the selected UI language code to the engine code on the
    // GUI thread (QSettings/combos are GUI state; the worker only gets the value).
    const QString capturedOcrLang = m_ocrLanguage
        ? ocrEngineLanguageCode(m_ocrLanguage->currentData().toString())
        : QStringLiteral("eng");
    // N3 (pdf24 skip-already-text): checkbox state captured on the GUI thread
    // (QSettings-backed state is GUI-affine); the worker only gets values.
    const bool capturedOcrSkipFiles = m_ocrSkipFilesWithText
        ? m_ocrSkipFilesWithText->isChecked() : false;
    const bool capturedOcrSkipPages = m_ocrSkipPagesWithText
        ? m_ocrSkipPagesWithText->isChecked() : false;
    const bool capturedOcrForce = m_ocrForceOcr ? m_ocrForceOcr->isChecked() : false;
    // §9.4 P0 / U08: the SAME preprocessing options as the interactive path —
    // deskew/binarize/denoise/orientDetect are persisted prefs read on the GUI
    // thread (QSettings is not thread-safe); the worker only gets a copy.
    // Previously batch honored Auto-Rotate only, silently diverging from the
    // interactive panel's persisted preprocessing choices.
    OcrPreprocessOptions capturedOcrPreprocess;
    // F5-F2: same shipped defaults as the interactive path — the destructive
    // chain (deskew/binarize/denoise) is OFF out of the box so batch OCR on a
    // clean scan recognizes out of the box too (SWEEP-W3-UX F5-F2).
    capturedOcrPreprocess.deskew   = QSettings().value(
        QStringLiteral("ocr/preprocessDeskew"), false).toBool();
    capturedOcrPreprocess.binarize = QSettings().value(
        QStringLiteral("ocr/preprocessBinarize"), false).toBool();
    capturedOcrPreprocess.denoise  = QSettings().value(
        QStringLiteral("ocr/preprocessDenoise"), false).toBool();
    capturedOcrPreprocess.orientDetect = QSettings().value(
        QStringLiteral("ocr/orientDetect"), false).toBool();
    // U08: report intentionally unsupported batch options (engine selection)
    // instead of silently diverging from the interactive path; captured on the
    // GUI thread because it reads QSettings.
    const QString capturedOcrReviewNote =
        preFlightReviewNote(capturedOp, m_ctx ? m_ctx->capabilities.get() : nullptr);

    // Redact config
    const QString capturedRedactOutDir = m_redactOutDir ? m_redactOutDir->text().trimmed() : QString();
    // §9.12 P1: effective list = named-preset regex bodies + free-form
    // entries (deduped; resolved on the GUI thread — PatternRedactor and the
    // checkbox state are GUI-affine, the worker only gets the string list).
    const QStringList capturedRedactPatterns =
        effectiveRedactPatterns(checkedRedactPresetKeys(),
                                m_redactPatterns
                                    ? m_redactPatterns->text().split(QLatin1Char(','),
                                                                     Qt::SkipEmptyParts)
                                    : QStringList());

    const BatchPreset capturedPreset = m_selectedPreset;
    const QString capturedPresetBlocker = presetBlocker;
    QMap<QString, QString> capturedOutputs;
    if (opIdx == OpPresetPipeline) {
        for (const QString& f : capturedFiles)
            capturedOutputs.insert(f, resolveOutputPath(f));
    }

    // Worker lambda — runs on QtConcurrent thread pool.
    // All captured values are by-value copies of GUI state taken above on the GUI thread.
    // 'this' is not captured to avoid dangling if BatchMode is destroyed mid-batch.
    // Engine mutex is captured as a raw pointer (stable lifetime: member of BatchMode).
    // R26-P2: `runState` carries the ordered lane's bates continuity (nullptr
    // on the mapped pipeline — lane selection guarantees bates never maps
    // there, and the chain fails the file honestly if it ever does).
    QMutex* engineMutexPtr = &m_engineMutex;

    auto processFileReal = [=](const QString& inputPath, PresetRunState* runState) -> BatchFileResult {
        BatchFileResult result;
        result.inputPath = inputPath;

        // E-13: QtConcurrent::mapped stores any exception that escapes this lambda
        // in the QFuture; the QFutureWatcher connections never call result() to
        // retrieve it, so in Qt 6 the unobserved exception can call std::terminate
        // when the watcher is destroyed (and the file's result is never emitted, so
        // the progress count is wrong). The engine calls below go through PoDoFo,
        // which throws. Catch everything here and turn it into a failed result.
        try {

        // Resolve output path (pure string ops, no GUI)
        QString baseName = QFileInfo(inputPath).completeBaseName();
        auto resolveDir = [&](const QString& configured) -> QString {
            return configured.isEmpty() ? QFileInfo(inputPath).absolutePath() : configured;
        };

        switch (capturedOp) {
        case OpConvert: {
            QString outDir = resolveDir(capturedConvertOutDir);
            const QStringList exts = { ".docx", ".xlsx", ".html", ".png", ".csv" };
            QString ext = (capturedFmtIdx >= 0 && capturedFmtIdx < exts.size())
                ? exts[capturedFmtIdx] : ".out";
            result.outputPath = QDir(outDir).filePath(baseName + ext);
            break;
        }
        case OpPresetPipeline:
            // R26: resolved on the GUI thread from the SAME naming resolution
            // the overwrite pre-check used — the worker never recomputes it.
            result.outputPath = capturedOutputs.value(inputPath);
            break;
        case OpCompress:
            result.outputPath = QDir(resolveDir(capturedCompressOutDir)).filePath(baseName + "_compressed.pdf");
            break;
        case OpWatermark:
            result.outputPath = QDir(resolveDir(capturedWmOutDir)).filePath(baseName + "_watermarked.pdf");
            break;
        case OpExportPdfA:
            result.outputPath = QDir(resolveDir(capturedPdfAOutDir)).filePath(baseName + "_pdfa.pdf");
            break;
        case OpOCR:
            result.outputPath = QDir(resolveDir(capturedOcrOutDir)).filePath(baseName + "_ocr.pdf");
            break;
        case OpRedact:
            result.outputPath = QDir(resolveDir(capturedRedactOutDir)).filePath(baseName + "_redacted.pdf");
            break;
        default:
            result.errorMessage = QStringLiteral("Unsupported operation");
            return result;
        }

        bool ok = false;
        QString techDetail;

        if (capturedOp == OpPresetPipeline) {
            // R26: the transactional per-file candidate chain (SafeSave
            // candidate per step, validated, ONE atomic commit). Failures are
            // file-scoped and honest — the original is untouched, every
            // intermediate removed.
            if (result.outputPath.isEmpty()) {
                techDetail = QStringLiteral("no output path was resolved for this file");
                ok = false;
            } else {
                ok = runPresetChain(inputPath, result.outputPath, capturedPreset,
                                    engineMutexPtr, runState, &result.steps, &techDetail);
            }
        } else if (capturedOp == OpConvert) {
            // convertTo is specified as stateless (takes full pdfPath arg) — no mutex needed
            if (capturedCtx && capturedCtx->conversion) {
                ok = capturedCtx->conversion->convertTo(inputPath, result.outputPath, capturedFmt);
                if (!ok)
                    techDetail = QStringLiteral("IConversionEngine::convertTo returned false for: %1").arg(inputPath);
            } else {
                techDetail = QStringLiteral("IConversionEngine not available");
            }
        } else if (capturedOp == OpOCR) {
            // N3 (pdf24 skip-already-text): force-OCR overrides both skip
            // switches — the user's explicit "OCR everything" wins.
            const bool skipFiles = capturedOcrSkipFiles && !capturedOcrForce;
            const bool skipPages = capturedOcrSkipPages && !capturedOcrForce;
            // OCR uses the SHARED engine (OcrEngine + OcrPipeline are not
            // thread-safe), so it stays serialized behind the engine mutex.
            QMutexLocker locker(engineMutexPtr);
            if (!capturedCtx || !capturedCtx->ocr) {
                techDetail = QStringLiteral("OCR engine not available");
                ok = false;
            } else {
                QPdfDocument pdf;
                pdf.load(inputPath);
                if (pdf.status() != QPdfDocument::Status::Ready || pdf.pageCount() <= 0) {
                    techDetail = QStringLiteral("Failed to open PDF for rendering: %1").arg(inputPath);
                    ok = false;
                } else {
                    // N3: probe per-page text ONCE through the PDFium text
                    // extraction seam (the same one the Bates tests read) —
                    // inside the engine mutex, because PDFium is not
                    // thread-safe either. A FAILED probe never enables a skip
                    // (skipping is only honest on positive evidence of text).
                    QList<bool> pageHasText;
                    if (skipFiles || skipPages) {
                        PdfiumBackend probe;
                        if (probe.loadDocument(inputPath)) {
                            for (int p = 0; p < pdf.pageCount(); ++p) {
                                bool has = false;
                                const auto runs = probe.extractPageTextRuns(p);
                                for (const auto& run : runs) {
                                    if (!run.text.trimmed().isEmpty()) { has = true; break; }
                                }
                                pageHasText.append(has);
                            }
                        }
                    }

                    // N3: skip-files-with-text — any existing text layer means
                    // the file is already searchable. Report as SKIPPED (its
                    // own truthful bucket), never as success or failure.
                    if (skipFiles && pageHasText.contains(true)) {
                        locker.unlock();
                        result.skipped = true;
                        result.skipReason = QStringLiteral(
                            "already contains a text layer — not OCRed (skip-files-with-text)");
                        return result;
                    }

                    // emergence E-2 (SWEEP-W3-EMERGENCE §1b): honor
                    // initialize()'s result. The discarded return let the
                    // pipeline run on an uninitialized engine — processImage
                    // then re-initialized with the DEFAULT "eng" (wrong-
                    // language text layer) or produced zero-word output —
                    // and the file was accounted SUCCESSFUL either way while
                    // the policy whyNot stayed console-only. The file now
                    // fails honestly, naming the deciding half of the
                    // ocr/allowNetworkDownload value; nothing is exported.
                    if (!capturedCtx->ocr->initialize(capturedOcrLang)) {
                        locker.unlock();
                        result.success = false;
                        result.errorMessage = ocrInitFailureDetail(capturedOcrLang);
                        return result;
                    }
                    OcrPipeline pipeline(capturedCtx->ocr);
                    pipeline.setStrategy(OcrStrategy::PrimaryOnly);
                    // §9.4 P0 / U08: the SAME preprocessing options the
                    // interactive path honors (GUI-thread-captured prefs) —
                    // deskew/binarize/denoise/orientDetect, no divergence.
                    OcrPreprocessOptions preprocessOpts = capturedOcrPreprocess;
                    pipeline.setPreprocessing(preprocessOpts);

                    // N3: skip-pages-with-text — pages WITH text pass through
                    // UNCHANGED (extractPageAsBytes + the N09 page-copy
                    // writer keep the original page content, so the text page
                    // in the output is the original page, not a re-encoded
                    // image); pages WITHOUT text are OCRed into a per-page
                    // MRC fragment and assembled in order. When the probe
                    // found no text page at all this is a no-op vs. the
                    // plain path except for the per-page assembly.
                    if (skipPages && !pageHasText.isEmpty()) {
                        bool anyNeedsOcr = false;
                        for (bool has : pageHasText)
                            if (!has) { anyNeedsOcr = true; break; }
                        if (!anyNeedsOcr) {
                            locker.unlock();
                            result.skipped = true;
                            result.skipReason = QStringLiteral(
                                "every page already contains text — not OCRed (skip-pages-with-text)");
                            return result;
                        }
                    }

                    // Q1: the skip decision gates the RENDER/OCR work itself,
                    // not just the assembly. Pages that will be KEPT as
                    // original page objects (skip-pages active, page has
                    // text) are never rasterized nor pushed through the OCR
                    // engine — the old code rendered + OCRed every page up
                    // front and then silently discarded the text pages'
                    // results. Work is memoized per page so a lazy fallback
                    // (kept-page extraction failure, below) re-renders a page
                    // at most once.
                    const bool perPageSkipActive = skipPages && !pageHasText.isEmpty();
                    QMap<int, QImage> ocrImageByPage;
                    QMap<int, PageOcrResult> ocrResultByPage;
                    const double dpi = 150.0;
                    const auto renderAndOcrPage = [&](int p) {
                        if (ocrResultByPage.contains(p)) return;
                        const QSizeF pts = pdf.pagePointSize(p);
                        const QSize px(qMax(1, int(pts.width()  * dpi / 72.0)),
                                       qMax(1, int(pts.height() * dpi / 72.0)));
                        const QImage img = pdf.render(p, px);
                        PageOcrResult pr;
                        pr.pageIndex = p;
                        pr.words     = img.isNull() ? QList<MergedOcrWord>() : pipeline.run(img);
                        pr.success   = true;
                        ocrImageByPage.insert(p, img);
                        ocrResultByPage.insert(p, pr);
                    };
                    for (int p = 0; p < pdf.pageCount(); ++p) {
                        // Kept text pages skip the render+OCR pipeline
                        // entirely; everything else is OCRed (in page order).
                        if (perPageSkipActive && pageHasText.at(p)) continue;
                        renderAndOcrPage(p);
                    }
                    // Ordered snapshots for the legacy whole-document export
                    // and the confidence note (QMap iterates in key order;
                    // kept pages default-construct with no words, which the
                    // low-confidence note correctly ignores).
                    QList<QImage> images;
                    QList<PageOcrResult> pageResults;
                    for (int p = 0; p < pdf.pageCount(); ++p) {
                        images.append(ocrImageByPage.value(p));
                        pageResults.append(ocrResultByPage.value(p));
                    }

                    if (!capturedCtx->pdfEditor) {
                        techDetail = QStringLiteral("PDF editor engine not available");
                        ok = false;
                    } else if (skipPages && !pageHasText.isEmpty()) {
                        // N3: mixed assembly — original text pages + OCRed
                        // image-only pages, in the original page order.
                        // Q1: kept-page extraction needs its OWN loaded
                        // engine. The captured shared engine never runs
                        // loadDocumentForEditing in the batch worker, so its
                        // PoDoFo backend is null and extractPageAsBytes
                        // returned empty for EVERY page — the "keep the
                        // original page" path was silently dead and every
                        // text page fell back to an MRC re-encode. A fresh
                        // per-file engine (the same pattern the editor ops
                        // below use) with a real load makes extraction work;
                        // created lazily, only when a page actually needs
                        // keeping.
                        std::unique_ptr<PdfEditorEngine> keptPageExtractor;
                        auto extractKeptPage = [&](int p) -> QByteArray {
                            if (!keptPageExtractor) {
                                keptPageExtractor = std::make_unique<PdfEditorEngine>();
                                if (!keptPageExtractor->loadDocumentForEditing(inputPath))
                                    return {};   // caller falls back to OCR
                            }
                            return keptPageExtractor->extractPageAsBytes(inputPath, p);
                        };
                        QList<QByteArray> pageDocs;
                        int keptCount = 0;
                        bool assemblyOk = true;
                        for (int p = 0; p < pdf.pageCount() && assemblyOk; ++p) {
                            if (pageHasText.at(p)) {
                                const QByteArray orig = extractKeptPage(p);
                                if (!orig.isEmpty()) {
                                    pageDocs.append(orig);
                                    ++keptCount;
                                    continue;
                                }
                                // Extraction failed — fall back to OCR for
                                // this page rather than emit a broken page.
                                // Q1: this page was scheduled to be KEPT, so
                                // it was never rendered above — render+OCR it
                                // lazily now (memoized, at most once).
                                renderAndOcrPage(p);
                            }
                            const QString pageTmp =
                                result.outputPath + QStringLiteral(".page%1.mrc").arg(p);
                            if (!capturedCtx->pdfEditor->exportMrcPdfA(
                                    pageTmp, { ocrImageByPage.value(p) },
                                    { ocrResultByPage.value(p) })) {
                                techDetail = QStringLiteral(
                                    "per-page MRC export failed on page %1: %2")
                                    .arg(p + 1)
                                    .arg(capturedCtx->pdfEditor->lastError().technicalDetails);
                                assemblyOk = false;
                                break;
                            }
                            {
                                QFile f(pageTmp);
                                if (f.open(QIODevice::ReadOnly)) pageDocs.append(f.readAll());
                                f.close();
                            }
                            QFile::remove(pageTmp);
                            if (pageDocs.size() != p + 1) {
                                techDetail = QStringLiteral(
                                    "could not read the per-page MRC fragment for page %1").arg(p + 1);
                                assemblyOk = false;
                            }
                        }
                        if (assemblyOk)
                            ok = gp::writeDocumentFromPages(pageDocs, result.outputPath);
                        if (ok) {
                            result.reviewNote = QStringLiteral(
                                "%1 of %2 page(s) already contained text and were "
                                "kept unchanged (skip-pages-with-text)")
                                .arg(keptCount).arg(pdf.pageCount());
                        }
                    } else {
                        ok = capturedCtx->pdfEditor->exportMrcPdfA(result.outputPath, images, pageResults);
                        if (!ok) techDetail = capturedCtx->pdfEditor->lastError().technicalDetails;
                    }
                    // §9.12 P0: surface OcrPipeline's confidence data — flag
                    // low-confidence words for review instead of reporting a
                    // bare pass/fail with zero visibility. The N3 kept-pages
                    // note (skip-pages mode) is preserved and appended to.
                    if (ok) {
                        const QString confidenceNote = lowConfidenceNote(pageResults);
                        QString extra = confidenceNote;
                        // U08: report the intentionally unsupported batch
                        // engine option alongside the confidence note —
                        // never a silent divergence from the interactive
                        // path.
                        if (!capturedOcrReviewNote.isEmpty()) {
                            extra = extra.isEmpty()
                                ? capturedOcrReviewNote
                                : capturedOcrReviewNote + QLatin1Char(' ') + extra;
                        }
                        if (!extra.isEmpty()) {
                            result.reviewNote = result.reviewNote.isEmpty()
                                ? extra
                                : result.reviewNote + QLatin1Char(' ') + extra;
                        }
                    }
                }
            }
        } else {
            // §9.12 P0: editor ops (Compress/Watermark/ExportPdfA/Redact) each
            // get a FRESH per-file PdfEditorEngine so they run in TRUE parallel
            // across the QtConcurrent pool. Previously all 5 non-Convert ops
            // shared one stateful engine behind a single mutex, so 5 of 7
            // "parallel" operations were secretly serialized. The engine is a
            // self-contained load/save machine — no shared state — so no mutex
            // is needed here.
            PdfEditorEngine editor;
            if (capturedOp == OpCompress) {
                if (!editor.loadDocumentForEditing(inputPath)) {
                    techDetail = editor.lastError().technicalDetails;
                    ok = false;
                } else {
                    OptimizeOptions opts;
                    opts.jpegQuality = capturedQuality;
                    opts.targetDpi   = capturedTargetDpi; // §9.12 P1: user-configurable (was hard-coded 150)
                    ok = editor.optimizeDocument(result.outputPath, opts);
                    if (!ok) techDetail = editor.lastError().technicalDetails;
                }
            } else if (capturedOp == OpWatermark) {
                if (!editor.loadDocumentForEditing(inputPath)) {
                    techDetail = editor.lastError().technicalDetails;
                    ok = false;
                } else {
                    TextWatermarkOptions opts;
                    opts.text    = capturedWmText;
                    opts.opacity = capturedWmOpacity / 100.0;
                    ok = editor.addTextWatermark(opts);
                    if (ok)
                        ok = editor.saveDocument(result.outputPath);
                    if (!ok) techDetail = editor.lastError().technicalDetails;
                }
            } else if (capturedOp == OpExportPdfA) {
                if (!editor.loadDocumentForEditing(inputPath)) {
                    techDetail = editor.lastError().technicalDetails;
                    ok = false;
                } else {
                    ok = editor.exportPdfA(result.outputPath, capturedPdfALevel);
                    if (!ok) techDetail = editor.lastError().technicalDetails;
                }
            } else if (capturedOp == OpRedact) {
                if (!editor.loadDocumentForEditing(inputPath)) {
                    techDetail = editor.lastError().technicalDetails;
                    ok = false;
                } else {
                    // §9.12 P0: collapse N patterns into a single load / find /
                    // apply / sanitize-save cycle per file (previously N full
                    // document reloads+saves). Invalid patterns are rejected
                    // up front by the engine before any mutation.
                    ok = editor.applyPatternRedactionsMulti(capturedRedactPatterns,
                                                            QList<int>(),
                                                            result.outputPath);
                    if (!ok) techDetail = editor.lastError().technicalDetails;
                }
            } else {
                techDetail = QStringLiteral("Unsupported editor operation");
            }
        }

        if (ok) {
            result.success = true;
        } else {
            result.success = false;
            result.errorMessage = techDetail.isEmpty()
                ? QStringLiteral("Operation failed: %1").arg(QFileInfo(inputPath).fileName())
                : techDetail;
        }
        return result;

        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = QStringLiteral("Exception processing %1: %2")
                .arg(QFileInfo(inputPath).fileName(), QString::fromUtf8(e.what()));
            qWarning() << "BatchMode: exception processing" << inputPath << ":" << e.what();
            return result;
        } catch (...) {
            result.success = false;
            result.errorMessage = QStringLiteral("Unknown exception processing %1")
                .arg(QFileInfo(inputPath).fileName());
            qCritical() << "BatchMode: unknown exception processing" << inputPath;
            return result;
        }
    };

    // Wire per-result callback for inline progress updates.
    // We use explicit disconnect+reconnect rather than Qt::UniqueConnection because
    // UniqueConnection compares sender/signal/receiver/slot by pointer and would still
    // allow a second identical lambda connection (each lambda is a distinct functor
    // object with a unique address). The disconnect call is therefore load-bearing and
    // must not be removed or replaced with UniqueConnection.
    disconnect(&m_watcher, &QFutureWatcher<BatchFileResult>::resultReadyAt, this, nullptr);
    connect(&m_watcher, &QFutureWatcher<BatchFileResult>::resultReadyAt,
            this, [this](int idx) { accountResultAt(idx); },
            Qt::QueuedConnection); // Deduplication is enforced by the preceding disconnect call


    // U08 per-item pre-flight (GUI thread — probes are cached and GUI-affine):
    // every file is checked BEFORE the worker starts. Blocked items are staged
    // as failed BatchFileResults (log + error log + failCount) so the summary
    // reports success + failed + remaining truthfully — a skipped file is
    // never reported as completed. The future maps only the runnable files;
    // this reuses the existing QtConcurrent pipeline, no new scheduler.
    int preFlightBlocked = 0;
    QStringList runnableFiles;
    const gp::CapabilityRegistry* caps = m_ctx ? m_ctx->capabilities.get() : nullptr;
    for (const QString& f : capturedFiles) {
        QString blocker = preFlightBlocker(capturedOp, f, caps);
        // R26 (batch-presets): a preset-level blocker (a step whose capability
        // is unavailable, a newer required app version, an empty step list)
        // applies to EVERY file — each is staged as failed with the registry's
        // whyNot + alternative so the summary stays truthful. Never a silent
        // skip, never a fake success.
        if (blocker.isEmpty())
            blocker = capturedPresetBlocker;
        if (blocker.isEmpty()) {
            runnableFiles << f;
            continue;
        }
        ++preFlightBlocked;
        ++m_failCount;
        appendFileResult(f, false, blocker);
        ErrorInfo err = ErrorInfo::error(
            tr("Not processed: %1").arg(QFileInfo(f).fileName()),
            blocker, ErrorInfo::Skip);
        err.sourceFile = f;
        m_errorLog.append(std::move(err));
    }
    if (preFlightBlocked > 0) {
        appendLog(tr("Pre-flight: %1 of %2 file(s) cannot be processed with the "
                     "selected operation — see the reasons above.")
                      .arg(preFlightBlocked).arg(capturedFiles.size()), "#c8a000");
    }
    if (runnableFiles.isEmpty()) {
        // Everything was blocked pre-flight: no worker is started. Finish the
        // batch UI state truthfully (0 runnable files remain).
        m_overallProgress->setValue(100);
        m_fileProgress->setValue(100);
        m_runBtn->setEnabled(true);
        m_cancelBtn->setEnabled(false);
        m_etaLabel->clear();
        showSummary();
        emit batchFinished();
        return;
    }

    // §9.12 P1: Merge — N inputs → 1 combined output. The append loop runs on
    // the QtConcurrent thread pool behind the SAME m_watcher the per-file ops
    // use: per-input BatchFileResults flow through resultReadyAt (shared
    // accounting + progress wiring above), cancellation is polled at every
    // file boundary, and the destination is saved exactly once at the end — a
    // cancelled merge never publishes a partial output. Page order == input
    // order (each input's pages are appended in list order).
    if (opIdx == OpMerge) {
        m_mergeOutputPath = mergeOutPath;   // F2a-F1: the summary names it on success
        startMergeWorker(runnableFiles, mergeOutPath);
        return;
    }

    // R26-P2 (plan §4.2): bates-bearing presets run on the ORDERED lane — one
    // worker iterating the file list IN ORDER, so cross-file Bates continuity
    // (N1) is a loop invariant rather than scheduling luck. The parallelism
    // cost is disclosed, never silent. Everything else keeps the mapped
    // pipeline unchanged.
    if (opIdx == OpPresetPipeline && BatchMode::presetNeedsOrderedLane(capturedPreset)) {
        appendLog(tr("Preset \u201c%1\u201d numbers pages with Bates sequences \u2014 files "
                     "are processed in order (one at a time); large batches are slower.")
                      .arg(capturedPreset.name), "#c8a000");
        startPresetOrderedWorker(runnableFiles, processFileReal);
        return;
    }

    QFuture<BatchFileResult> future = QtConcurrent::mapped(
        runnableFiles,
        [processFileReal](const QString& inputPath) {
            return processFileReal(inputPath, static_cast<PresetRunState*>(nullptr));
        });
    m_watcher.setFuture(future);
}

// ── R26-P2 (plan §4.2): the ordered lane ──────────────────────────────────────
// One sequential worker over the file list, behind the SAME QFutureWatcher,
// result plumbing and G12 exactly-once accounting as the mapped pipeline —
// the file-loop is the only difference. Cancellation is polled at file
// boundaries only (never mid-chain); the boundary hook (test seam) runs in
// the same window, before the file's chain starts. Cross-file bates
// continuity lives in the worker-local PresetRunState: single thread by
// construction, advanced only by successful stamps.
void BatchMode::startPresetOrderedWorker(
    const QStringList& files,
    const std::function<BatchFileResult(const QString&, PresetRunState*)>& runOneFile) {
    // Captured by value like every worker input — the member is never read
    // cross-thread (m_mergeBoundaryHook's discipline).
    const std::function<void(int)> boundaryHook = m_presetBoundaryHook;

    auto orderedWorker = [files, runOneFile, boundaryHook](QPromise<BatchFileResult>& promise) {
        promise.setProgressRange(0, files.size());
        PresetRunState runState;
        for (int i = 0; i < files.size(); ++i) {
            if (promise.isCanceled()) return;    // file boundary only
            if (boundaryHook) boundaryHook(i);
            if (promise.isCanceled()) return;    // cancel landed in the boundary window
            promise.addResult(runOneFile(files.at(i), &runState));
            promise.setProgressValue(i + 1);
        }
    };
    m_watcher.setFuture(QtConcurrent::run(orderedWorker));
}

// ── Merge (single combined output) ──────────────────────────────────────────────
// §9.12 P1: the merge used to run gp::mergeDocuments synchronously on the GUI
// thread (runMerge), freezing the app for the duration of large merges. The
// file-append loop now runs on the QtConcurrent thread pool behind the SAME
// QFutureWatcher<BatchFileResult> the per-file ops use:
//   - per-input BatchFileResults flow through resultReadyAt → the shared
//     accounting/progress wiring in onRunClicked (one accounted item per
//     input; a corrupt input fails as an item instead of aborting the run);
//   - cancellation is polled at every file boundary — the worker returns
//     without saving, so a cancelled merge never publishes a partial output;
//   - the destination is built in memory and saved exactly once at the end;
//     each input's pages are appended in list order, so the merged page
//     order == input order (the pre-fix gp::mergeDocuments contract).
// PdfPageOps.h deliberately keeps PoDoFo headers out of its callers, but its
// only merge entry merges ALL inputs in one unobservable call — no boundary a
// worker could poll. The file-boundary loop therefore uses PoDoFo directly
// (podofo is already a link dependency of pdfws_ui), mirroring
// PdfPageOps::mergeDocuments' idiom: fresh destination, eager per-document
// page copy, ONE Save at the end.
void BatchMode::startMergeWorker(const QStringList& files, const QString& outPath) {
    // All captures are by-value copies of GUI state taken on the GUI thread
    // (same discipline as processFileReal). 'this' is not captured to avoid
    // dangling if BatchMode is destroyed mid-merge; the hook member is copied
    // here so it is never read cross-thread.
    const std::function<void(int)> boundaryHook = m_mergeBoundaryHook;

    auto mergeWorker = [files, outPath, boundaryHook](QPromise<BatchFileResult>& promise) {
        promise.setProgressRange(0, files.size());
        bool anyAppended = false;
        // SEP13 leads 9+10: per-input results are published ONLY once the
        // output's fate is known. The old code streamed per-input
        // success=true entries during the append loop (pointing at an output
        // that did not exist yet) and, on save failure or cancel, added an
        // N+1th "output artifact" result on top — a phantom beyond one
        // result per input file, with false successes against a
        // never-written output. QPromise results are append-only, so honest
        // accounting requires deferring publication.
        QList<BatchFileResult> perInput;
        try {
            PoDoFo::PdfMemDocument dst;
            for (int i = 0; i < files.size(); ++i) {
                // Cancel is honored at file boundaries only — never
                // mid-document. Nothing has been published yet, so a
                // cancelled merge reports zero results: no success may point
                // at the output that will never be written (lead 10).
                if (promise.isCanceled()) return;
                if (boundaryHook) boundaryHook(i);

                BatchFileResult r;
                r.inputPath  = files.at(i);
                r.outputPath = outPath;
                try {
                    PoDoFo::PdfMemDocument src;
                    src.Load(files.at(i).toUtf8().constData());
                    const int count = static_cast<int>(src.GetPages().GetCount());
                    if (count > 0) {
                        dst.GetPages().AppendDocumentPages(src, 0, count);
                        anyAppended = true;
                        r.success = true;
                    } else {
                        r.success = false;
                        r.errorMessage = QStringLiteral("Document has no pages");
                    }
                } catch (const std::exception& e) {
                    r.success = false;
                    r.errorMessage = QString::fromUtf8(e.what());
                    qWarning() << "BatchMode merge: failed to append"
                               << files.at(i) << ":" << e.what();
                } catch (...) {
                    r.success = false;
                    r.errorMessage = QStringLiteral("Unknown error appending this file");
                    qCritical() << "BatchMode merge: unknown error appending" << files.at(i);
                }
                perInput.append(r);
                promise.setProgressValue(i + 1); // file-boundary progress
            }
            if (!anyAppended) {
                // Nothing was appended: every per-input result already
                // carries its own failure reason. Publish them 1:1 with the
                // inputs — the old extra "output artifact" item was a
                // phantom (lead 9).
                for (const auto& r : perInput)
                    promise.addResult(r);
                return;
            }
            // Cancelled between the last append and the save: the accumulated
            // pages are discarded, never written as a partial merge — and no
            // result may claim success against an output that is never written.
            if (promise.isCanceled()) return;
            dst.Save(outPath.toUtf8().constData());
            // The output exists: NOW the per-input successes are real.
            for (const auto& r : perInput)
                promise.addResult(r);
        } catch (const std::exception& e) {
            // Save failed: no output exists, so NO input may report success.
            // Re-mark appended successes as failures — one result per input
            // file, all truthful (lead 9's contract (b)).
            for (auto r : perInput) {
                if (r.success) {
                    r.success = false;
                    r.outputPath.clear();
                    r.errorMessage = QStringLiteral("Merge failed — %1 (no output written)")
                                         .arg(QString::fromUtf8(e.what()));
                }
                promise.addResult(r);
            }
            qWarning() << "BatchMode merge: save failed for" << outPath << ":" << e.what();
        } catch (...) {
            for (auto r : perInput) {
                if (r.success) {
                    r.success = false;
                    r.outputPath.clear();
                    r.errorMessage = QStringLiteral("Merge failed — unknown error (no output written)");
                }
                promise.addResult(r);
            }
            qCritical() << "BatchMode merge: unknown error saving" << outPath;
        }
    };
    m_watcher.setFuture(QtConcurrent::run(mergeWorker));
}

// §9.12 P1 / G12: per-result accounting shared by the resultReadyAt handler
// and the completion drain in onBatchFinished. `idx` is the worker-report
// index (merge: strict file order; mapped ops: completion order — only the
// count matters). The m_accountedIndices ledger guarantees every completed
// result is reconciled EXACTLY ONCE, however its delivery races the summary.
void BatchMode::accountResultAt(int idx) {
    if (m_accountedIndices.contains(idx))
        return;                 // G12: a late queued callback cannot double count
    m_accountedIndices.insert(idx);
    BatchFileResult res = m_watcher.resultAt(idx);
    m_lastRunResults.append(res);   // R26-P2: per-file results incl. step records
    int completed = m_successCount + m_failCount + m_skipCount + 1;
    int total = m_filesToProcess.size();

    // N3: a deliberate skip (skip-already-text) is its own truthful bucket —
    // never success ("not processed" would be wrong too: nothing failed).
    if (res.skipped) {
        ++m_skipCount;
        appendLog(QStringLiteral("  \xE2\x8F\xAD %1 \xe2\x80\x94 skipped: %2")
                      .arg(QFileInfo(res.inputPath).fileName(), res.skipReason), "#7a9c6f");
        ErrorInfo info = ErrorInfo::error(
            tr("Skipped: %1").arg(QFileInfo(res.inputPath).fileName()),
            res.skipReason, ErrorInfo::Skip);
        info.sourceFile = res.inputPath;
        m_errorLog.append(std::move(info));
    } else if (res.success) {
        ++m_successCount;
        appendFileResult(res.inputPath, true, res.outputPath);
        // §9.12 P0: a successful file can still need review (low-confidence
        // OCR words). Log it as a warning so it lands in the summary and
        // the exportable error log — never a silent pass.
        if (!res.reviewNote.isEmpty()) {
            appendLog(QStringLiteral("  \xE2\x9A\xA0 %1").arg(res.reviewNote), "#d08b2c");
            ErrorInfo warn = ErrorInfo::warning(res.reviewNote);
            warn.sourceFile = res.inputPath;
            m_errorLog.append(std::move(warn));
        }
    } else {
        ++m_failCount;
        appendFileResult(res.inputPath, false, res.errorMessage);
        ErrorInfo err = ErrorInfo::error(
            tr("Failed: %1").arg(QFileInfo(res.inputPath).fileName()),
            res.errorMessage,
            ErrorInfo::Skip);
        err.sourceFile = res.inputPath;
        m_errorLog.append(std::move(err));
    }

    // Overall progress
    int pct = total > 0 ? (completed * 100 / total) : 0;
    m_overallProgress->setValue(pct);
    m_fileProgress->setValue(pct);

    // ETA calculation
    qint64 elapsed = m_batchTimer.elapsed();
    if (completed > 0 && completed < total) {
        qint64 msPerFile = elapsed / completed;
        qint64 remaining = msPerFile * (total - completed);
        int secRemain = static_cast<int>(remaining / 1000);
        m_etaLabel->setText(tr("ETA ~%1s").arg(secRemain));
    } else {
        m_etaLabel->clear();
    }
}

void BatchMode::onCancelClicked() {
    if (m_watcher.isRunning()) {
        m_watcher.cancel();
        m_statusLabel->setText(tr("Cancelling…"));
        m_cancelBtn->setEnabled(false);
        appendLog(tr("Cancel requested — waiting for in-progress file to finish…"), "#c8a000");
    }
}

// ── Progress / finish slots ────────────────────────────────────────────────────

void BatchMode::onBatchProgress(int value) {
    // QFutureWatcher::progressValueChanged gives raw future progress (0..fileCount)
    // We also update from resultReadyAt which is more granular — keep this as fallback
    int total = m_filesToProcess.size();
    int pct = total > 0 ? (value * 100 / total) : 0;
    m_overallProgress->setValue(pct);
    // SEP13 leads 9+10: make the worker's own progress observable (used by
    // the async-merge contract test; see mergeWorker for why per-item result
    // accounting cannot stream before the merge output's fate is known).
    emit batchProgress(value);
}

void BatchMode::onBatchFinished() {
    // G12 (QUALITY-GATE-2026-09-09): reconcile EVERY completed result exactly
    // once, for ALL batch modes, BEFORE the summary/finished contract. The
    // resultReadyAt deliveries of the mapped (watermark / PDF/A / redact / …)
    // workers are queued and can land after this finished callout — the
    // merge-only drain left them out, so the summary showed
    // "0 of 0 succeeded, 1 not processed" for a batch that had actually
    // succeeded. `finished` implies every result is already reported, so read
    // the unaccounted indices straight from the future; the ledger makes
    // callbacks that were merely queued late no-ops.
    const int reported = m_watcher.future().resultCount();
    for (int i = 0; i < reported; ++i) {
        if (!m_accountedIndices.contains(i))
            accountResultAt(i);
    }
    m_overallProgress->setValue(100);
    m_fileProgress->setValue(100);
    m_runBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_etaLabel->clear();

    // Disconnect the per-result slot to avoid stale connections on next run
    disconnect(&m_watcher, &QFutureWatcher<BatchFileResult>::resultReadyAt,
               this, nullptr);

    showSummary();
    emit batchFinished();
}

// ── Helpers ────────────────────────────────────────────────────────────────────

// §9.12 P0: single source of truth for the low-confidence review rule. Words
// at or below `threshold` (0-100) are flagged for human review; the note names
// the affected pages so a reviewer knows where to look. Pure function.
QString BatchMode::lowConfidenceNote(const QList<PageOcrResult>& pages,
                                     int confidenceThreshold) {
    int lowWords = 0;
    QSet<int> pagesAffected;
    for (const auto& pr : pages) {
        for (const auto& w : pr.words) {
            if (w.confidence >= 0 && w.confidence <= confidenceThreshold) {
                ++lowWords;
                pagesAffected.insert(pr.pageIndex);
            }
        }
    }
    if (lowWords == 0) return QString();
    QStringList pageList;
    QList<int> sorted(pagesAffected.constBegin(), pagesAffected.constEnd());
    std::sort(sorted.begin(), sorted.end());
    for (int p : sorted) pageList << QString::number(p + 1);  // 1-based for users
    return BatchMode::tr("%1 low-confidence word(s) on page(s) %2 — review recommended")
        .arg(lowWords).arg(pageList.join(QStringLiteral(", ")));
}

// ── §9.12 P1 seams: DPI presets + named redaction presets ────────────────────

// Clamp the user-chosen target DPI into the supported engine range. Pure
// function so the boundary is testable without driving a batch run.
int BatchMode::resolveCompressTargetDpi(int requestedDpi) {
    return qBound(kMinTargetDpi, requestedDpi, kMaxTargetDpi);
}

// The effective redaction pattern list: named-preset regex bodies first
// (resolved through PatternRedactor::namedPattern — the SAME built-in keys
// the interactive Redact mode consumes), then the free-form entries.
// Unresolvable keys produce an invalid regex and are dropped (never the
// sentinel broken pattern that namedPattern returns for unknown keys);
// empty and duplicate patterns collapse so one span is never excised twice.
QStringList BatchMode::effectiveRedactPatterns(const QStringList& presetKeys,
                                               const QStringList& freeFormPatterns) {
    QStringList result;
    auto add = [&result](const QString& pattern) {
        const QString t = pattern.trimmed();
        if (!t.isEmpty() && !result.contains(t)) result << t;
    };
    for (const QString& key : presetKeys) {
        const QRegularExpression rx = PatternRedactor::namedPattern(key);
        if (rx.isValid()) add(rx.pattern());
    }
    for (const QString& pattern : freeFormPatterns)
        add(pattern);
    return result;
}

// Keys of the currently checked named-PII preset checkboxes, in panel order.
QStringList BatchMode::checkedRedactPresetKeys() const {
    QStringList keys;
    for (const QCheckBox* chk : m_redactPresets)
        if (chk && chk->isChecked())
            keys << chk->property("presetKey").toString();
    return keys;
}

// ── R26 (batch-presets P1): named preset surface ──────────────────────────────
// Presets are data (plan §5.1): the picker lists saved presets with their ops,
// "Save as preset…" captures the currently configured classic operation, and
// the step/capability disclosure shows every step with the CapabilityRegistry's
// answer BEFORE anything runs. The store is file-per-preset JSON under
// <AppDataLocation>/presets (plan §2.1) — QSettings is never the source of
// truth; the test seam re-points the root for settings isolation.

QString BatchMode::s_presetStoreDirForTest;

BatchPresetStore BatchMode::presetStore() {
    // Re-built per call (never a static local): a test may re-point the store
    // directory between BatchMode construction and use.
    return BatchPresetStore(s_presetStoreDirForTest);
}

void BatchMode::setPresetStoreDirForTest(const QString& dir) {
    s_presetStoreDirForTest = dir;
}

void BatchMode::buildPresetPanel(QWidget* host) {
    auto* lay = new QVBoxLayout(host);

    // Broken-file disclosure: a preset file this build refuses to load is
    // never silently hidden from the user (store honesty surface).
    m_presetBrokenLabel = new QLabel;
    m_presetBrokenLabel->setWordWrap(true);
    m_presetBrokenLabel->setStyleSheet("color:#c8442b; font-size:10px;");
    m_presetBrokenLabel->hide();

    m_presetCombo = new QComboBox;
    m_presetCombo->setObjectName(QStringLiteral("batchPresetPicker"));

    m_presetStepsLabel = new QLabel(tr("No preset selected."));
    m_presetStepsLabel->setObjectName(QStringLiteral("batchPresetStepsLabel"));
    m_presetStepsLabel->setWordWrap(true);
    m_presetStepsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_presetStepsLabel->setStyleSheet("color:#71747a; font-size:10px;");

    auto* btnRow = new QHBoxLayout;
    auto* saveBtn = new QPushButton(tr("Save as preset…"));
    saveBtn->setObjectName(QStringLiteral("batchPresetSaveBtn"));
    saveBtn->setToolTip(tr("Save the currently configured operation as a reusable preset.\n"
                           "Convert, Merge and OCR runs cannot be saved as presets in this "
                           "version."));
    auto* renameBtn = new QPushButton(tr("Rename…"));
    renameBtn->setObjectName(QStringLiteral("batchPresetRenameBtn"));
    auto* deleteBtn = new QPushButton(tr("Delete"));
    deleteBtn->setObjectName(QStringLiteral("batchPresetDeleteBtn"));
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(renameBtn);
    btnRow->addWidget(deleteBtn);
    btnRow->addStretch(1);

    lay->addWidget(m_presetBrokenLabel);
    lay->addWidget(new QLabel(tr("Preset:")));
    lay->addWidget(m_presetCombo);
    lay->addWidget(m_presetStepsLabel, 1);
    lay->addLayout(btnRow);

    lay->addWidget(new QLabel(tr("Output Folder:")));
    auto* dirRow = new QHBoxLayout;
    m_presetOutDir = new QLineEdit;
    m_presetOutDir->setObjectName(QStringLiteral("batchPresetOutDir"));
    m_presetOutDir->setPlaceholderText(tr("Same folder as source"));
    auto* pickBtn = new QPushButton(tr("…"));
    pickBtn->setFixedWidth(28);
    dirRow->addWidget(m_presetOutDir);
    dirRow->addWidget(pickBtn);
    lay->addLayout(dirRow);
    connect(pickBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
        if (!dir.isEmpty()) m_presetOutDir->setText(dir);
    });
    lay->addStretch(1);

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatchMode::onPresetSelected);
    connect(saveBtn,   &QPushButton::clicked, this, &BatchMode::onSaveAsPresetClicked);
    connect(renameBtn, &QPushButton::clicked, this, &BatchMode::onRenamePresetClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &BatchMode::onDeletePresetClicked);

    refreshPresetPicker();
}

void BatchMode::refreshPresetPicker(const QString& selectId) {
    if (!m_presetCombo)
        return;
    QSignalBlocker block(m_presetCombo);
    m_presetCombo->clear();

    const auto store = presetStore();
    const auto presets = store.list();
    for (const BatchPreset& p : presets) {
        // The picker lists each preset WITH the ops it contains (the honest
        // "what will this run?" summary at a glance).
        QStringList ops;
        for (const BatchPresetStep& s : p.steps)
            ops << s.op;
        m_presetCombo->addItem(
            QStringLiteral("%1 (%2)").arg(p.name, ops.join(QStringLiteral(", "))), p.id);
    }
    if (presets.isEmpty()) {
        // Honest empty state: say HOW a preset is created instead of showing
        // a dead picker.
        m_presetCombo->addItem(
            tr("(no presets saved yet \u2014 configure an operation and choose "
               "\u201cSave as preset\u2026\u201d)"), QString());
    }

    const auto broken = store.brokenFiles();
    if (broken.isEmpty()) {
        m_presetBrokenLabel->clear();
        m_presetBrokenLabel->hide();
    } else {
        m_presetBrokenLabel->setText(
            tr("%1 unreadable preset file(s) in the preset store will not run \u2014 delete or "
               "fix them. First: %2")
                .arg(broken.size())
                .arg(QFileInfo(broken.first().path).fileName()
                     + QStringLiteral(" \u2014 ") + broken.first().error));
        m_presetBrokenLabel->show();
    }

    int sel = selectId.isEmpty() ? -1 : m_presetCombo->findData(selectId);
    if (sel < 0)
        sel = 0;   // placeholder (empty store) or first preset
    m_presetCombo->setCurrentIndex(sel);
    block.unblock();
    onPresetSelected(m_presetCombo->currentIndex());
}

void BatchMode::onPresetSelected(int index) {
    m_presetSelected = false;
    m_selectedPreset = BatchPreset{};
    if (!m_presetCombo || !m_presetStepsLabel)
        return;
    const QString id = m_presetCombo->itemData(index).toString();
    if (id.isEmpty()) {
        m_presetStepsLabel->setText(tr("No preset selected."));
        return;
    }
    BatchPreset p;
    QString err;
    if (!presetStore().get(id, &p, &err)) {
        // The store changed underneath the picker (external delete/corrupt):
        // disclose, never pretend the preset is runnable.
        m_presetStepsLabel->setText(tr("Preset could not be loaded: %1").arg(err));
        return;
    }
    m_selectedPreset = p;
    m_presetSelected = true;
    m_presetStepsLabel->setText(
        presetStepsDisplayText(p, m_ctx ? m_ctx->capabilities.get() : nullptr));
}

QString BatchMode::presetStepsDisplayText(const BatchPreset& preset,
                                          const gp::CapabilityRegistry* capabilities) {
    QStringList lines;
    lines << QObject::tr("Steps (%1):").arg(preset.steps.size());
    for (int i = 0; i < preset.steps.size(); ++i) {
        const BatchPresetStep& step = preset.steps.at(i);
        QString line = QStringLiteral("  %1. %2").arg(i + 1).arg(step.op);
        if (!step.label.isEmpty())
            line += QStringLiteral(" \u2014 %1").arg(step.label);
        lines << line;
    }
    // Design-time capability disclosure (plan §5.2): every non-Available step
    // shows the registry's whyNot + alternative. A disclosed-unavailable step
    // will refuse to run — the display says so before the user tries.
    for (int i = 0; i < preset.steps.size(); ++i) {
        const BatchPresetStep& step = preset.steps.at(i);
        const Capability c = batchPresetStepCapability(step, capabilities);
        if (c.status != Availability::Available)
            lines << QStringLiteral("  \u26A0 %1: %2")
                         .arg(step.op, CapabilityRegistry::combineWhyNot(c));
    }
    return lines.join(QLatin1Char('\n'));
}

QString BatchMode::presetRunBlocker() const {
    if (!m_presetSelected)
        return tr("Select a preset to run first.");
    if (m_selectedPreset.steps.isEmpty())
        return tr("Preset \u201c%1\u201d has no steps \u2014 nothing to run.")
                   .arg(m_selectedPreset.name);
    const gp::CapabilityRegistry* caps = m_ctx ? m_ctx->capabilities.get() : nullptr;
    for (const BatchPresetStep& step : m_selectedPreset.steps) {
        const Capability c = batchPresetStepCapability(step, caps);
        if (c.status == Availability::UnavailableRuntime
            || c.status == Availability::UnavailableBuild)
            return tr("Step \u201c%1\u201d cannot run: %2")
                       .arg(step.op, CapabilityRegistry::combineWhyNot(c));
    }
    if (!m_selectedPreset.minAppVersion.isEmpty()) {
        const QString appVersion = QCoreApplication::applicationVersion();
        if (!appVersion.isEmpty()
            && BatchPresetSchema::compareVersions(appVersion,
                                                  m_selectedPreset.minAppVersion) < 0)
            return tr("This preset requires GlyphPDF %1 or newer (this app reports %2).")
                       .arg(m_selectedPreset.minAppVersion, appVersion);
    }
    return {};
}

bool BatchMode::captureConfiguredOpAsPreset(const QString& name, QString* err) {
    const auto failWith = [err](const QString& message) {
        if (err) *err = message;
        return false;
    };
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return failWith(tr("Enter a name for the preset."));

    const int opIdx = m_opCombo ? m_opCombo->currentIndex() : 0;

    BatchPreset p;
    p.name = trimmed;
    p.created = p.modified = QDateTime::currentDateTimeUtc();
    if (!QCoreApplication::applicationVersion().isEmpty())
        p.authorApp = QStringLiteral("GlyphPDF %1")
                          .arg(QCoreApplication::applicationVersion());

    BatchPresetStep step;
    switch (opIdx) {
    case OpCompress: {
        step.op = QStringLiteral("compress");
        step.params.insert(QStringLiteral("quality"),
                           m_qualitySlider ? m_qualitySlider->value() : 75);
        step.params.insert(QStringLiteral("targetDpi"),
                           m_dpiSpin ? m_dpiSpin->value() : kDefaultTargetDpi);
        break;
    }
    case OpWatermark: {
        step.op = QStringLiteral("watermark");
        const QString text = m_wmTextEdit ? m_wmTextEdit->text().trimmed() : QString();
        step.params.insert(QStringLiteral("text"),
                           text.isEmpty() ? QStringLiteral("CONFIDENTIAL") : text);
        step.params.insert(QStringLiteral("opacity"),
                           m_wmOpacity ? m_wmOpacity->value() : 30);
        break;
    }
    case OpExportPdfA: {
        step.op = QStringLiteral("pdfa-export");
        // The combo's data codes are the engine's conformance codes
        // (1=1B, 2=2B, 4=2U, 3=3B, 5=3U) — mapped back to the schema strings.
        static const QHash<int, QString> levelNames = {
            { 1, QStringLiteral("1b") }, { 2, QStringLiteral("2b") },
            { 4, QStringLiteral("2u") }, { 3, QStringLiteral("3b") },
            { 5, QStringLiteral("3u") } };
        const int code = m_pdfaLevel ? m_pdfaLevel->currentData().toInt() : 2;
        step.params.insert(QStringLiteral("level"),
                           levelNames.value(code, QStringLiteral("2b")));
        break;
    }
    case OpRedact: {
        step.op = QStringLiteral("redact");
        const QStringList presets = checkedRedactPresetKeys();
        const QStringList freeForm = m_redactPatterns
            ? m_redactPatterns->text().split(QLatin1Char(','), Qt::SkipEmptyParts)
            : QStringList();
        if (effectiveRedactPatterns(presets, freeForm).isEmpty())
            return failWith(tr("This redaction configuration has no effective patterns \u2014 "
                               "check a quick preset or enter a regex before saving it as a "
                               "preset."));
        step.params.insert(QStringLiteral("presets"), presets);
        QStringList trimmedPatterns;
        for (const QString& pattern : freeForm) {
            const QString t = pattern.trimmed();
            if (!t.isEmpty()) trimmedPatterns << t;
        }
        step.params.insert(QStringLiteral("patterns"), trimmedPatterns);
        break;
    }
    default:
        return failWith(tr("Convert, Merge and OCR runs cannot be saved as presets in this "
                           "version \u2014 only Compress, Watermark, Export PDF/A and Redact."));
    }
    p.steps.append(step);

    auto store = presetStore();
    if (!store.save(&p, err))
        return false;
    refreshPresetPicker(p.id);
    // Switch to the Preset Pipeline showing the fresh preset - the configured
    // run has been captured; what follows is the preset view of it (and a
    // following Run would otherwise re-run the captured classic op).
    m_opCombo->setCurrentIndex(OpPresetPipeline);
    return true;
}

void BatchMode::onSaveAsPresetClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Save as Preset"));
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(tr("Preset name:"), &dlg));
    auto* edit = new QLineEdit(&dlg);
    edit->setObjectName(QStringLiteral("presetNameEdit"));
    lay->addWidget(edit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString err;
    if (!captureConfiguredOpAsPreset(edit->text(), &err)) {
        QMessageBox::warning(this, tr("Save as Preset"), err);
        return;
    }
    appendLog(tr("Preset saved: %1").arg(edit->text().trimmed()), "#5b9bd5");
}

void BatchMode::onRenamePresetClicked() {
    if (!m_presetSelected) {
        QMessageBox::information(this, tr("Rename Preset"),
            tr("Select a preset to rename first."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Rename Preset"));
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(tr("New name:"), &dlg));
    auto* edit = new QLineEdit(m_selectedPreset.name, &dlg);
    edit->setObjectName(QStringLiteral("presetRenameEdit"));
    lay->addWidget(edit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString err;
    if (!renamePresetForTest(m_selectedPreset.id, edit->text(), &err)) {
        QMessageBox::warning(this, tr("Rename Preset"), err);
        return;
    }
    appendLog(tr("Preset renamed: %1").arg(edit->text().trimmed()), "#5b9bd5");
}

void BatchMode::onDeletePresetClicked() {
    if (!m_presetSelected) {
        QMessageBox::information(this, tr("Delete Preset"),
            tr("Select a preset to delete first."));
        return;
    }
    // Files are user data — deletion is confirmed (default No).
    const auto btn = QMessageBox::question(this, tr("Delete Preset?"),
        tr("Delete preset \u201c%1\u201d?\n\nThe preset file will be removed from disk. This "
           "cannot be undone.").arg(m_selectedPreset.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btn != QMessageBox::Yes)
        return;
    QString err;
    if (!deletePresetForTest(m_selectedPreset.id, &err)) {
        QMessageBox::warning(this, tr("Delete Preset"), err);
        return;
    }
    appendLog(tr("Preset deleted: %1").arg(m_selectedPreset.name), "#71747a");
}

bool BatchMode::saveConfiguredOpAsPresetForTest(const QString& name, QString* err) {
    return captureConfiguredOpAsPreset(name, err);
}

bool BatchMode::selectPresetForTest(const QString& id) {
    if (!m_presetCombo)
        return false;
    const int idx = m_presetCombo->findData(id);
    if (idx < 0)
        return false;
    m_presetCombo->setCurrentIndex(idx);   // fires onPresetSelected
    return true;
}

QString BatchMode::presetStepsDisplayForTest() const {
    return m_presetStepsLabel ? m_presetStepsLabel->text() : QString();
}

QStringList BatchMode::presetIdsForTest() const {
    QStringList ids;
    if (!m_presetCombo)
        return ids;
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        const QString id = m_presetCombo->itemData(i).toString();
        if (!id.isEmpty())
            ids << id;
    }
    return ids;
}

bool BatchMode::renamePresetForTest(const QString& id, const QString& newName, QString* err) {
    auto store = presetStore();
    if (!store.rename(id, newName, err))
        return false;
    refreshPresetPicker(id);
    return true;
}

bool BatchMode::deletePresetForTest(const QString& id, QString* err) {
    auto store = presetStore();
    if (!store.remove(id, err))
        return false;
    if (m_presetSelected && m_selectedPreset.id == id) {
        m_presetSelected = false;
        m_selectedPreset = BatchPreset{};
    }
    refreshPresetPicker();
    return true;
}

// ── R26-P2 (batch-presets P2) ─────────────────────────────────────────────────

// Lane rule of record (plan §4.2): bates-bearing presets run on the ordered
// lane — cross-file continuity is a loop invariant there, never an accident
// of scheduling. Pure function.
bool BatchMode::presetNeedsOrderedLane(const BatchPreset& preset) {
    for (const BatchPresetStep& step : preset.steps)
        if (step.op == QLatin1String("bates"))
            return true;
    return false;
}

// ── U08 pre-flight seams ──────────────────────────────────────────────────────

// Pure function: a non-empty result blocks the file BEFORE any worker runs.
// Capability answers come from the registry (cached, GUI-thread probed); the
// input-existence check mirrors the engine's own first validation so a bad
// path fails at disclosure time instead of inside the pipeline.
QString BatchMode::preFlightBlocker(int opIndex, const QString& inputPath,
                                    const gp::CapabilityRegistry* capabilities) {
    if (!QFileInfo::exists(inputPath))
        return BatchMode::tr("Input file not found: %1").arg(inputPath);

    if (opIndex == OpOCR && capabilities) {
        // Batch OCR runs the Tesseract pipeline (PrimaryOnly) — apply the same
        // honest engine-availability gate the interactive path applies.
        const gp::Capability tesseract = capabilities->query(gp::CapId::OcrTesseract);
        if (tesseract.status != gp::Availability::Available)
            return gp::CapabilityRegistry::combineWhyNot(tesseract);
    }
    return QString();
}

// Pure function of the persisted prefs + capabilities (QSettings read happens
// on the GUI thread at capture time in onRunClicked). Reports the ONE batch
// option that intentionally does not apply: the engine selection. Batch OCR
// always runs Tesseract PrimaryOnly, while the interactive path may use
// RapidOCR/ensemble (either explicitly, or via "auto" when the PP-OCRv5
// models are installed).
QString BatchMode::preFlightReviewNote(int opIndex,
                                       const gp::CapabilityRegistry* capabilities) {
    if (opIndex != OpOCR)
        return QString();

    const QString engineKey = QSettings().value(
        QStringLiteral("ocr/engine"), QStringLiteral("auto")).toString();
    const bool autoSelect = engineKey.isEmpty() || engineKey == QStringLiteral("auto");
    const bool rapidPreferred =
        engineKey == QStringLiteral("rapidocr")
        || engineKey == QStringLiteral("ensemble")
        || (autoSelect && capabilities && capabilities->available(gp::CapId::OcrRapidModels));
    if (!rapidPreferred)
        return QString();

    return BatchMode::tr("Batch OCR runs the Tesseract engine only — the "
                         "RapidOCR/ensemble engine selection is not applied in batch.");
}

void BatchMode::appendLog(const QString& text, const QString& color) {
    if (color.isEmpty())
        m_logView->append(text);
    else
        m_logView->append(QStringLiteral("<span style='color:%1'>%2</span>")
            .arg(color, text.toHtmlEscaped()));
}

void BatchMode::appendFileResult(const QString& file, bool success, const QString& detail) {
    QString name = QFileInfo(file).fileName();
    if (success) {
        appendLog(QStringLiteral("  \xE2\x9C\x93 %1").arg(name), "#4ec96d");
    } else {
        appendLog(QStringLiteral("  \xE2\x9C\x95 %1 \xe2\x80\x94 %2").arg(name, detail), "#c8442b");
    }
}

void BatchMode::showSummary() {
    // N3: skipped files are part of the honest total (they WERE looked at and
    // deliberately left alone), reported in their own bucket.
    int total    = m_successCount + m_failCount + m_skipCount;
    int warnings = m_errorLog.warningCount();
    // U08: remaining = files neither succeeded nor failed (mid-run this is the
    // in-flight tail; after a cancel these were NOT processed — say so).
    const int remaining = remainingCount();

    QString summary = tr("BATCH COMPLETE — %1 of %2 succeeded").arg(m_successCount).arg(total);
    if (m_failCount > 0)
        summary += tr(", %1 failed").arg(m_failCount);
    if (m_skipCount > 0)
        summary += tr(", %1 skipped (already contained text)").arg(m_skipCount);
    if (warnings > 0)
        summary += tr(", %1 warnings").arg(warnings);
    if (remaining > 0)
        summary += tr(", %1 not processed").arg(remaining);
    if (m_watcher.isCanceled())
        summary += tr(" [CANCELLED]");

    appendLog(QString());
    appendLog(summary, m_failCount > 0 ? "#c8442b" : "#4ec96d");

    // F2a-F1 (SWEEP-W3-UX): "BATCH COMPLETE — N of M succeeded" alone never
    // says WHAT the batch produced. For a merge the ONE thing the user needs
    // to know is the combined output's location, and the per-input lines only
    // show input names. Name the output — but only when the merge actually
    // committed it: the worker publishes successes solely after the save, so
    // a cancelled or failed merge (successCount 0, no output on disk) is
    // never dressed up as one that wrote a file.
    if (!m_mergeOutputPath.isEmpty() && m_successCount > 0) {
        appendLog(tr("Merged output: %1").arg(QFileInfo(m_mergeOutputPath)
                                                  .absoluteFilePath()), "#4ec96d");
        summary += tr(" — merged into %1")
                       .arg(QFileInfo(m_mergeOutputPath).fileName());
    }

    m_statusLabel->setText(summary);
    m_exportLogBtn->setVisible(m_errorLog.count() > 0);
}

// ── Export log (D4) ────────────────────────────────────────────────────────────

void BatchMode::onExportLog() {
    if (m_errorLog.count() == 0) return;

    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Batch Log"), {},
        tr("JSON (*.json);;CSV (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    bool ok = path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)
        ? m_errorLog.exportCsv(path)
        : m_errorLog.exportJson(path);

    if (ok)
        appendLog(tr("Log exported to %1").arg(path), "#5b9bd5");
    else
        appendLog(tr("Failed to export log to %1").arg(path), "#c8442b");
}

} // namespace gp
