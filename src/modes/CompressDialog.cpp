// SPDX-License-Identifier: Apache-2.0
#include "CompressDialog.h"
#include "util/GpTheme.h"
#include "util/Badge.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/mrc/MrcPageProcessor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

// ── Preset card helper ────────────────────────────────────────────────────────

static QToolButton* presetCard(const QString& name, const QString& spec,
                               const QString& desc, bool active)
{
    auto* b = new QToolButton;
    b->setText(QString("%1\n\n%2\n%3").arg(name, spec, desc));
    b->setCheckable(true);
    b->setChecked(active);
    b->setMinimumHeight(100);
    // T-02: use GpTheme tokens instead of hardcoded dark hex so preset cards
    // render correctly in Light and High-Contrast themes.
    const QString accentHex  = gp::Theme::accent().name();
    const QString bg0Hex     = gp::Theme::bg0().name();
    const QString lineHex    = gp::Theme::line().name();
    const QString lineSHex   = gp::Theme::lineStrong().name();
    const QString fg0Hex     = gp::Theme::fg0().name();
    b->setStyleSheet(active
        ? QString("QToolButton{background:rgba(%1,%2,%3,0.13);border:1px solid %4;"
                  "color:%5;text-align:left;padding:14px;}")
              .arg(gp::Theme::accent().red())
              .arg(gp::Theme::accent().green())
              .arg(gp::Theme::accent().blue())
              .arg(accentHex, fg0Hex)
        : QString("QToolButton{background:%1;border:1px solid %2;"
                  "color:%3;text-align:left;padding:14px;}"
                  "QToolButton:hover{border-color:%4;}")
              .arg(bg0Hex, lineHex, fg0Hex, lineSHex));
    return b;
}

// ── Constructor ───────────────────────────────────────────────────────────────

CompressDialog::CompressDialog(const AppContext* ctx, QWidget* parent)
    : QDialog(parent), _ctx(ctx)
{
    setProperty("role", "modal");
    setWindowTitle(tr("Compress"));
    setFixedSize(560, 580);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Header ──
    auto* head = new QFrame;
    head->setProperty("role", "modalHead");
    auto* hr = new QHBoxLayout(head);
    hr->setContentsMargins(12, 0, 12, 0);
    auto* t = new QLabel(tr("OPTIMIZE DOCUMENT"));
    t->setStyleSheet("font-weight:600;letter-spacing:1px;");
    _fileLabel = new QLabel;
    _fileLabel->setProperty("mono", true);
    hr->addWidget(t);
    hr->addWidget(_fileLabel);
    hr->addStretch(1);
    col->addWidget(head);

    // Set filename from current document
    if (_ctx && _ctx->pdfEditor) {
        QString file = _ctx->pdfEditor->currentFile();
        if (!file.isEmpty())
            _fileLabel->setText(QString::fromUtf8(" \xC2\xB7 ") + QFileInfo(file).fileName());
    }

    // ── Body ──
    auto* body = new QFrame;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(16, 14, 16, 14);
    bl->setSpacing(10);

    // Preset cards
    _presetGroup = new QButtonGroup(this);
    _presetGroup->setExclusive(true);
    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    auto* pScreen   = presetCard(tr("SCREEN"),   tr("72 DPI · Q50"),  tr("On-screen viewing"), false);
    auto* pEbook    = presetCard(tr("EBOOK"),    tr("150 DPI · Q75"), tr("E-readers & email"), true);
    auto* pPrinter  = presetCard(tr("PRINTER"),  tr("300 DPI · Q85"), tr("Desktop printing"),  false);
    auto* pCustom   = presetCard(tr("CUSTOM"),   tr("User settings"),  tr("Fine-tune below"),  false);
    _presetGroup->addButton(pScreen,  0);
    _presetGroup->addButton(pEbook,   1);
    _presetGroup->addButton(pPrinter, 2);
    _presetGroup->addButton(pCustom,  3);
    grid->addWidget(pScreen,  0, 0);
    grid->addWidget(pEbook,   0, 1);
    grid->addWidget(pPrinter, 1, 0);
    grid->addWidget(pCustom,  1, 1);
    bl->addLayout(grid);

    // Advanced option row
    // T-02: use GpTheme tokens; no hardcoded dark hex.
    auto* advFrame = new QFrame;
    advFrame->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:8px;")
            .arg(gp::Theme::bg0().name(), gp::Theme::line().name()));
    auto* af = new QGridLayout(advFrame);
    af->setSpacing(4);

    _chkDownsample = new QCheckBox(tr("Downsample images"));
    _chkDownsample->setChecked(true);
    af->addWidget(_chkDownsample, 0, 0);

    auto* dpiRow = new QHBoxLayout;
    dpiRow->addWidget(new QLabel(tr("Target DPI:")));
    _dpiSpin = new QSpinBox;
    _dpiSpin->setRange(36, 600);
    _dpiSpin->setValue(150);
    dpiRow->addWidget(_dpiSpin);
    dpiRow->addWidget(new QLabel(tr("  Quality:")));
    _qualitySpin = new QSpinBox;
    _qualitySpin->setRange(10, 100);
    _qualitySpin->setValue(75);
    _qualitySpin->setSuffix("%");
    dpiRow->addWidget(_qualitySpin);
    dpiRow->addStretch(1);
    af->addLayout(dpiRow, 0, 1);

    _chkDedup = new QCheckBox(tr("Deduplicate images"));
    _chkDedup->setChecked(true);
    af->addWidget(_chkDedup, 1, 0);

    _chkSubsetFonts = new QCheckBox(tr("Subset fonts"));
    _chkSubsetFonts->setChecked(true);
    af->addWidget(_chkSubsetFonts, 1, 1);

    _chkRemoveUnused = new QCheckBox(tr("Remove unused objects"));
    _chkRemoveUnused->setChecked(true);
    af->addWidget(_chkRemoveUnused, 2, 0);

    _chkStripMetadata = new QCheckBox(tr("Strip metadata"));
    _chkStripMetadata->setChecked(false);
    af->addWidget(_chkStripMetadata, 2, 1);

    bl->addWidget(advFrame);

    // ── MRC mode selector ──────────────────────────────────────────────────
    // Mixed Raster Content: JBIG2 foreground + JPEG2000 background for scanned PDFs.
    // Achieves 5-10× compression for scan-heavy documents.
    auto* mrcFrame = new QFrame;
    // T-02: theme-aware background/border.
    mrcFrame->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:8px;")
            .arg(gp::Theme::bg0().name(), gp::Theme::line().name()));
    auto* mf = new QHBoxLayout(mrcFrame);
    mf->setSpacing(8);

    auto* mrcLabel = new QLabel(tr("MRC scan compression:"));
    mrcLabel->setToolTip(tr(
        "Mixed Raster Content: compresses scanned PDFs using JBIG2 (text layer) "
        "and JPEG2000 (background layer). Achieves 5–10× compression. "
        "Requires pre-rendered page images from the OCR engine."));
    mf->addWidget(mrcLabel);

    _mrcModeCombo = new QComboBox;
    _mrcModeCombo->addItem(tr("Off (standard compression)"),      (int)MrcMode::Off);
    _mrcModeCombo->addItem(tr("Lossless (10:1 background)"),      (int)MrcMode::Lossless);
    _mrcModeCombo->addItem(tr("Balanced (30:1, recommended)"),    (int)MrcMode::Balanced);
    _mrcModeCombo->addItem(tr("Aggressive (50:1 background)"),    (int)MrcMode::Aggressive);
    _mrcModeCombo->setCurrentIndex(0);  // Off by default
    _mrcModeCombo->setToolTip(tr(
        "Balanced: best size/quality trade-off for archival.\n"
        "Aggressive: maximum compression; may introduce artefacts in photo regions."));
    mf->addWidget(_mrcModeCombo, 1);

    _mrcEstLabel = new QLabel(tr("MRC: off"));
    _mrcEstLabel->setProperty("mono", true);
    // T-02: use fg2 token.
    _mrcEstLabel->setStyleSheet(
        QString("font-size:10px; color:%1;").arg(gp::Theme::fg2().name()));
    mf->addWidget(_mrcEstLabel);

    bl->addWidget(mrcFrame);

    // Wire MRC combo to re-estimate
    connect(_mrcModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompressDialog::refreshEstimate);

    // Size estimation display
    auto* sizeRow = new QFrame;
    // T-02: theme-aware background/border.
    sizeRow->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:10px;")
            .arg(gp::Theme::bg0().name(), gp::Theme::line().name()));
    auto* sl = new QVBoxLayout(sizeRow);

    auto* origRow = new QHBoxLayout;
    auto* origLab = new QLabel(tr("ORIGINAL"));
    origLab->setProperty("mono", true);
    origLab->setFixedWidth(70);
    origRow->addWidget(origLab);
    _origBar = new QProgressBar;
    _origBar->setRange(0, 100);
    _origBar->setValue(100);
    _origBar->setTextVisible(false);
    _origBar->setFixedHeight(10);
    // T-02: theme-aware progress bar.
    _origBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:1px solid %2;}"
                "QProgressBar::chunk{background:%3;}")
            .arg(gp::Theme::bg3().name(), gp::Theme::lineStrong().name(),
                 gp::Theme::fg2().name()));
    origRow->addWidget(_origBar, 1);
    _origVal = new QLabel("— MB");
    _origVal->setProperty("mono", true);
    _origVal->setFixedWidth(80);
    _origVal->setAlignment(Qt::AlignRight);
    origRow->addWidget(_origVal);
    sl->addLayout(origRow);

    auto* estRow = new QHBoxLayout;
    auto* estLab = new QLabel(tr("ESTIMATED"));
    estLab->setProperty("mono", true);
    estLab->setFixedWidth(70);
    estRow->addWidget(estLab);
    _estBar = new QProgressBar;
    _estBar->setRange(0, 100);
    _estBar->setValue(0);
    _estBar->setTextVisible(false);
    _estBar->setFixedHeight(10);
    // T-02: theme-aware estimated bar (accent chunk).
    _estBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:1px solid %2;}"
                "QProgressBar::chunk{background:%3;}")
            .arg(gp::Theme::bg3().name(), gp::Theme::lineStrong().name(),
                 gp::Theme::accent().name()));
    estRow->addWidget(_estBar, 1);
    _estVal = new QLabel("— MB");
    _estVal->setProperty("mono", true);
    // T-02: use accent token.
    _estVal->setStyleSheet(QString("color:%1;").arg(gp::Theme::accent().name()));
    _estVal->setFixedWidth(80);
    _estVal->setAlignment(Qt::AlignRight);
    estRow->addWidget(_estVal);
    sl->addLayout(estRow);

    auto* badge = new QHBoxLayout;
    _reductBadge = new Badge("— REDUCTION", Badge::Warn);
    badge->addWidget(_reductBadge);
    badge->addStretch(1);
    _detailLabel = new QLabel;
    _detailLabel->setProperty("mono", true);
    // T-02: use fg2 token for dim detail text.
    _detailLabel->setStyleSheet(
        QString("font-size:10px; color:%1;").arg(gp::Theme::fg2().name()));
    badge->addWidget(_detailLabel);
    sl->addLayout(badge);
    bl->addWidget(sizeRow);

    bl->addStretch(1);
    col->addWidget(body, 1);

    // ── Footer ──
    auto* foot = new QFrame;
    foot->setProperty("role", "modalFoot");
    auto* fr = new QHBoxLayout(foot);
    fr->setContentsMargins(12, 8, 12, 8);
    fr->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setFlat(true);
    auto* go = new QPushButton(tr("COMPRESS DOCUMENT"));
    // T-02: accent button using GpTheme token; text color is always the bg0
    // (dark on accent) which gives sufficient contrast across all three themes.
    go->setStyleSheet(
        QString("background:%1;color:%2;border:1px solid %1;"
                "font-weight:700;padding:8px 20px;letter-spacing:0.6px;")
            .arg(gp::Theme::accent().name(), gp::Theme::bg0().name()));
    fr->addWidget(cancel);
    fr->addWidget(go);
    col->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(go, &QPushButton::clicked, this, &CompressDialog::onCompress);

    // Wire preset changes to update controls + estimate
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(_presetGroup, &QButtonGroup::idClicked, this, &CompressDialog::onPresetChanged);
#else
    connect(_presetGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &CompressDialog::onPresetChanged);
#endif

    // Wire advanced control changes to re-estimate
    connect(_chkDownsample,   &QCheckBox::toggled, this, &CompressDialog::refreshEstimate);
    connect(_chkDedup,        &QCheckBox::toggled, this, &CompressDialog::refreshEstimate);
    connect(_chkSubsetFonts,  &QCheckBox::toggled, this, &CompressDialog::refreshEstimate);
    connect(_chkRemoveUnused, &QCheckBox::toggled, this, &CompressDialog::refreshEstimate);
    connect(_chkStripMetadata,&QCheckBox::toggled, this, &CompressDialog::refreshEstimate);
    connect(_dpiSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CompressDialog::refreshEstimate);
    connect(_qualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CompressDialog::refreshEstimate);

    // Initialize with Ebook preset
    onPresetChanged(1);
}

// ── Preset handling ───────────────────────────────────────────────────────────

void CompressDialog::onPresetChanged(int id) {
    // Apply preset values to advanced controls (block signals to avoid recursive refresh)
    _dpiSpin->blockSignals(true);
    _qualitySpin->blockSignals(true);
    _chkDownsample->blockSignals(true);
    _chkDedup->blockSignals(true);
    _chkSubsetFonts->blockSignals(true);
    _chkRemoveUnused->blockSignals(true);
    _chkStripMetadata->blockSignals(true);

    switch (id) {
    case 0: // Screen
        _dpiSpin->setValue(72);
        _qualitySpin->setValue(50);
        _chkDownsample->setChecked(true);
        _chkDedup->setChecked(true);
        _chkSubsetFonts->setChecked(true);
        _chkRemoveUnused->setChecked(true);
        _chkStripMetadata->setChecked(true);
        break;
    case 1: // Ebook
        _dpiSpin->setValue(150);
        _qualitySpin->setValue(75);
        _chkDownsample->setChecked(true);
        _chkDedup->setChecked(true);
        _chkSubsetFonts->setChecked(true);
        _chkRemoveUnused->setChecked(true);
        _chkStripMetadata->setChecked(false);
        break;
    case 2: // Printer
        _dpiSpin->setValue(300);
        _qualitySpin->setValue(85);
        _chkDownsample->setChecked(true);
        _chkDedup->setChecked(true);
        _chkSubsetFonts->setChecked(true);
        _chkRemoveUnused->setChecked(true);
        _chkStripMetadata->setChecked(false);
        break;
    case 3: // Custom — leave controls as-is
        break;
    }

    _dpiSpin->blockSignals(false);
    _qualitySpin->blockSignals(false);
    _chkDownsample->blockSignals(false);
    _chkDedup->blockSignals(false);
    _chkSubsetFonts->blockSignals(false);
    _chkRemoveUnused->blockSignals(false);
    _chkStripMetadata->blockSignals(false);

    // Enable/disable advanced controls for non-custom presets
    bool custom = (id == 3);
    _dpiSpin->setEnabled(custom || id < 3); // Always accessible
    _qualitySpin->setEnabled(custom || id < 3);

    refreshEstimate();
}

// ── Live estimation ───────────────────────────────────────────────────────────

static QString formatBytes(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
}

void CompressDialog::refreshEstimate() {
    if (!_ctx || !_ctx->pdfEditor) {
        _origVal->setText("— MB");
        _estVal->setText("— MB");
        _reductBadge->setText(tr("NO DOCUMENT"));
        _reductBadge->setKind(Badge::Err);
        return;
    }

    OptimizeOptions opts;
    opts.downsampleImages   = _chkDownsample->isChecked();
    opts.targetDpi          = _dpiSpin->value();
    opts.jpegQuality        = _qualitySpin->value();
    opts.deduplicateImages  = _chkDedup->isChecked();
    opts.subsetFonts        = _chkSubsetFonts->isChecked();
    opts.removeUnusedObjects= _chkRemoveUnused->isChecked();
    opts.stripMetadata      = _chkStripMetadata->isChecked();

    OptimizeEstimate est = _ctx->pdfEditor->estimateOptimization(opts);

    _origVal->setText(formatBytes(est.originalBytes));
    _estVal->setText(formatBytes(est.estimatedBytes));

    // Bar proportions
    if (est.originalBytes > 0) {
        int pct = static_cast<int>(100.0 * est.estimatedBytes / est.originalBytes);
        _estBar->setValue(qBound(1, pct, 100));
    } else {
        _estBar->setValue(0);
    }

    // Reduction badge
    QString redText = QString::fromUtf8("\xe2\x86\x93 ") // ↓
        + QString::number(est.reductionPercent, 'f', 0) + "% REDUCTION";
    _reductBadge->setText(redText);
    _reductBadge->setKind(est.reductionPercent > 20 ? Badge::Ok : Badge::Info);

    // Detail line
    _detailLabel->setText(
        QString("%1 images · %2 fonts · %3 duplicates")
        .arg(est.imageCount).arg(est.fontCount).arg(est.duplicateImages));

    // MRC estimate label update
    if (_mrcModeCombo) {
        MrcMode mrcMode = (MrcMode)_mrcModeCombo->currentData().toInt();
        if (mrcMode == MrcMode::Off) {
            _mrcEstLabel->setText(tr("MRC: off"));
        } else {
            // Rough MRC estimate: use page count × average page image size heuristic.
            // We don't have actual page images here; use the standard estimate as baseline
            // and apply MRC ratio reduction on top.
            qint64 mrcEst = (qint64)(est.estimatedBytes * 0.25);  // MRC ≈ 75% further reduction
            _mrcEstLabel->setText(tr("MRC est: ≈%1").arg(formatBytes(mrcEst)));
        }
    }
}

// ── Compress action ───────────────────────────────────────────────────────────

void CompressDialog::onCompress() {
    if (!_ctx || !_ctx->pdfEditor) {
        QMessageBox::warning(this, tr("Error"), tr("PDF editing engine is not available."));
        return;
    }

    QString currentFile = _ctx->pdfEditor->currentFile();
    if (currentFile.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No document is currently loaded."));
        return;
    }

    // Ask user where to save the optimized file
    QString outPath = QFileDialog::getSaveFileName(
        this, tr("Save Optimized PDF"),
        QFileInfo(currentFile).absolutePath() + "/" +
        QFileInfo(currentFile).completeBaseName() + "_optimized.pdf",
        "PDF Files (*.pdf)");

    if (outPath.isEmpty()) return;

    // Check MRC mode
    MrcMode mrcMode = MrcMode::Off;
    if (_mrcModeCombo)
        mrcMode = (MrcMode)_mrcModeCombo->currentData().toInt();

    bool success = false;

    if (mrcMode != MrcMode::Off) {
        // MRC path: requires page images from OCR pipeline.
        // In this dialog, we don't have pre-rendered images — show informational dialog.
        QMessageBox::information(this, tr("MRC Export"),
            tr("MRC (Mixed Raster Content) export requires the document to have been "
               "processed through the OCR pipeline first.\n\n"
               "To use MRC compression:\n"
               "1. Run OCR on the document (OCR mode)\n"
               "2. Use Export → MRC PDF/A from the File menu.\n\n"
               "Falling back to standard compression for this export."));
        mrcMode = MrcMode::Off;
    }

    OptimizeOptions opts;
    opts.downsampleImages   = _chkDownsample->isChecked();
    opts.targetDpi          = _dpiSpin->value();
    opts.jpegQuality        = _qualitySpin->value();
    opts.deduplicateImages  = _chkDedup->isChecked();
    opts.subsetFonts        = _chkSubsetFonts->isChecked();
    opts.removeUnusedObjects= _chkRemoveUnused->isChecked();
    opts.stripMetadata      = _chkStripMetadata->isChecked();

    success = _ctx->pdfEditor->optimizeDocument(outPath, opts);

    if (success) {
        QFileInfo fi(outPath);
        QMessageBox::information(this, tr("Optimization Complete"),
            tr("Document optimized successfully.\n\nSaved to: %1\nNew size: %2")
            .arg(fi.fileName(), formatBytes(fi.size())));
        accept();
    } else {
        QMessageBox::warning(this, tr("Optimization Failed"),
            tr("Could not optimize the document. See application log for details."));
    }
}

} // namespace gp
