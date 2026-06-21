// SPDX-License-Identifier: Apache-2.0
#include "BatchMode.h"
#include "util/GpTheme.h"

#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/IConversionEngine.h"
#include "core/interfaces/IOcrEngine.h"
#include "engines/ocr/OcrPipeline.h"
#include "engines/podofo/PdfPageOps.h"

using TargetFormat = IConversionEngine::TargetFormat;

#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
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
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

namespace gp {

// ── Constructor ───────────────────────────────────────────────────────────────

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
        auto* dpiNote = new QLabel(tr("150 DPI — balanced quality/size"));
        dpiNote->setStyleSheet("color:#71747a; font-size:10px;");
        lay->addWidget(dpiNote);

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

    vlay->addWidget(m_cfgStack, 1);

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

// ── Execution engine (D3) ─────────────────────────────────────────────────────

void BatchMode::onRunClicked() {
    if (m_filesToProcess.isEmpty()) {
        QMessageBox::information(this, tr("No Files"), tr("Add PDF files to the list first."));
        return;
    }

    int opIdx = m_opCombo ? m_opCombo->currentIndex() : 0;

    // Guard: AppContext must be set for engine operations
    if (!m_ctx) {
        QMessageBox::warning(this, tr("Engine Unavailable"),
            tr("No AppContext available. Ensure the application is fully initialized."));
        return;
    }

    // Merge is a single combined output over all files — it does not fit the
    // per-file mapped pipeline, so it has a dedicated synchronous handler.
    if (opIdx == OpMerge) {
        runMerge();
        return;
    }

    // Redact requires at least one pattern.
    if (opIdx == OpRedact &&
        (!m_redactPatterns || m_redactPatterns->text().trimmed().isEmpty())) {
        QMessageBox::information(this, tr("No Patterns"),
            tr("Enter one or more comma-separated regex patterns to redact."));
        return;
    }

    // AR-8 D4: Pre-check output paths for overwrite conflicts.
    // For single-file runs, show a per-file dialog.
    // For multi-file runs, collect all conflicting paths and show one summary
    // dialog rather than flooding the user with N dialogs.
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

    // Redact config
    const QString capturedRedactOutDir = m_redactOutDir ? m_redactOutDir->text().trimmed() : QString();
    const QStringList capturedRedactPatterns = m_redactPatterns
        ? m_redactPatterns->text().split(QLatin1Char(','), Qt::SkipEmptyParts)
        : QStringList();

    // Worker lambda — runs on QtConcurrent thread pool.
    // All captured values are by-value copies of GUI state taken above on the GUI thread.
    // 'this' is not captured to avoid dangling if BatchMode is destroyed mid-batch.
    // Engine mutex is captured as a raw pointer (stable lifetime: member of BatchMode).
    QMutex* engineMutexPtr = &m_engineMutex;

    auto processFileReal = [=](const QString& inputPath) -> BatchFileResult {
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

        if (capturedOp == OpConvert) {
            // convertTo is specified as stateless (takes full pdfPath arg) — no mutex needed
            if (capturedCtx && capturedCtx->conversion) {
                ok = capturedCtx->conversion->convertTo(inputPath, result.outputPath, capturedFmt);
                if (!ok)
                    techDetail = QStringLiteral("IConversionEngine::convertTo returned false for: %1").arg(inputPath);
            } else {
                techDetail = QStringLiteral("IConversionEngine not available");
            }
        } else {
            // Editor operations: serialize via mutex (engine is a stateful load/save machine)
            QMutexLocker locker(engineMutexPtr);
            if (!capturedCtx || !capturedCtx->pdfEditor) {
                result.errorMessage = QStringLiteral("PDF editor engine not available");
                return result;
            }
            auto& editor = *capturedCtx->pdfEditor;

            if (capturedOp == OpCompress) {
                if (!editor.loadDocumentForEditing(inputPath)) {
                    techDetail = editor.lastError().technicalDetails;
                    ok = false;
                } else {
                    OptimizeOptions opts;
                    opts.jpegQuality = capturedQuality;
                    opts.targetDpi   = 150;
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
                    ok = true;
                    for (const QString& patStr : capturedRedactPatterns) {
                        QRegularExpression re(patStr.trimmed());
                        if (!re.isValid()) {
                            techDetail = QStringLiteral("Invalid regex pattern: %1").arg(patStr.trimmed());
                            ok = false;
                            break;
                        }
                        // startPage = endPage = -1 → redact all pages.
                        if (!editor.applyPatternRedactions(re, QList<int>(), result.outputPath)) {
                            techDetail = editor.lastError().technicalDetails;
                            ok = false;
                            break;
                        }
                    }
                    // applyPatternRedactions already saves via sanitizeDocument
                }
            } else if (capturedOp == OpOCR) {
                // Render each page, OCR it, then assemble a searchable MRC PDF/A
                // (image background + invisible text layer from the OCR words).
                if (!capturedCtx->ocr) {
                    techDetail = QStringLiteral("OCR engine not available");
                    ok = false;
                } else {
                    QPdfDocument pdf;
                    pdf.load(inputPath);
                    if (pdf.status() != QPdfDocument::Status::Ready || pdf.pageCount() <= 0) {
                        techDetail = QStringLiteral("Failed to open PDF for rendering: %1").arg(inputPath);
                        ok = false;
                    } else {
                        capturedCtx->ocr->initialize(QStringLiteral("eng"));
                        OcrPipeline pipeline(capturedCtx->ocr);
                        pipeline.setStrategy(OcrStrategy::PrimaryOnly);

                        QList<QImage> images;
                        QList<PageOcrResult> pageResults;
                        const double dpi = 150.0;
                        for (int p = 0; p < pdf.pageCount(); ++p) {
                            const QSizeF pts = pdf.pagePointSize(p);
                            const QSize px(qMax(1, int(pts.width()  * dpi / 72.0)),
                                           qMax(1, int(pts.height() * dpi / 72.0)));
                            const QImage img = pdf.render(p, px);
                            images.append(img);

                            PageOcrResult pr;
                            pr.pageIndex = p;
                            pr.words     = img.isNull() ? QList<MergedOcrWord>() : pipeline.run(img);
                            pr.success   = true;
                            pageResults.append(pr);
                        }
                        ok = editor.exportMrcPdfA(result.outputPath, images, pageResults);
                        if (!ok) techDetail = editor.lastError().technicalDetails;
                    }
                }
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
            this, [this](int idx) {
        BatchFileResult res = m_watcher.resultAt(idx);
        int completed = m_successCount + m_failCount + 1;
        int total = m_filesToProcess.size();

        if (res.success) {
            ++m_successCount;
            appendFileResult(res.inputPath, true, res.outputPath);
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
    }, Qt::QueuedConnection); // Deduplication is enforced by the preceding disconnect call


    QFuture<BatchFileResult> future = QtConcurrent::mapped(capturedFiles, processFileReal);
    m_watcher.setFuture(future);
}

// ── Merge (single combined output) ──────────────────────────────────────────────
// Merge does not fit the per-file QtConcurrent::mapped pipeline (N inputs → 1
// output), so it runs synchronously here via gp::mergeDocuments.
void BatchMode::runMerge() {
    if (m_filesToProcess.size() < 2) {
        QMessageBox::information(this, tr("Merge PDFs"),
            tr("Add at least two PDF files to merge."));
        return;
    }

    const QString first = m_filesToProcess.first();
    QString outDir = m_mergeOutDir ? m_mergeOutDir->text().trimmed() : QString();
    if (outDir.isEmpty()) outDir = QFileInfo(first).absolutePath();
    const QString outPath = QDir(outDir).filePath(
        QFileInfo(first).completeBaseName() + QStringLiteral("_merged.pdf"));

    if (!confirmOverwrite(outPath)) return;

    m_logView->clear();
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_fileProgress->setValue(0);
    m_statusLabel->setText(tr("Merging %1 files…").arg(m_filesToProcess.size()));
    appendLog(tr("Merging %1 files → %2")
        .arg(m_filesToProcess.size()).arg(QFileInfo(outPath).fileName()));

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = false;
    QString err;
    try {
        ok = gp::mergeDocuments(m_filesToProcess, outPath);
    } catch (const std::exception& e) {
        err = QString::fromUtf8(e.what());
    } catch (...) {
        err = tr("unknown error");
    }
    QApplication::restoreOverrideCursor();

    m_overallProgress->setValue(100);
    m_fileProgress->setValue(100);
    if (ok) {
        appendLog(QStringLiteral("  \xE2\x9C\x93 %1").arg(outPath), "#4ec96d");
        m_statusLabel->setText(tr("MERGE COMPLETE — %1").arg(QFileInfo(outPath).fileName()));
    } else {
        appendLog(QStringLiteral("  \xE2\x9C\x95 %1")
            .arg(err.isEmpty() ? tr("Merge failed") : tr("Merge failed — %1").arg(err)), "#c8442b");
        m_statusLabel->setText(tr("MERGE FAILED"));
    }
    emit batchFinished();
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
}

void BatchMode::onBatchFinished() {
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
    int total    = m_successCount + m_failCount;
    int warnings = m_errorLog.warningCount();

    QString summary = tr("BATCH COMPLETE — %1 of %2 succeeded").arg(m_successCount).arg(total);
    if (m_failCount > 0)
        summary += tr(", %1 failed").arg(m_failCount);
    if (warnings > 0)
        summary += tr(", %1 warnings").arg(warnings);
    if (m_watcher.isCanceled())
        summary += tr(" [CANCELLED]");

    appendLog(QString());
    appendLog(summary, m_failCount > 0 ? "#c8442b" : "#4ec96d");

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
