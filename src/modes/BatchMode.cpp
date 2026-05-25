#include "BatchMode.h"
#include "util/GpTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include <QCoreApplication>

namespace gp {

BatchMode::BatchMode(QWidget* parent) : QWidget(parent) {
    // ── v1.0.0 preview banner ─────────────────────────────────────────
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* preview = new QLabel(tr("Preview — not wired in v1.0.0\n\nUse the ribbon toolbar for production-ready operations.\nThis mode page is scheduled for v1.1."), this);
    preview->setObjectName("modePreviewBanner");
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet("QLabel#modePreviewBanner { background: rgba(255, 200, 100, 0.92); color: #5c3000; font-weight: bold; padding: 24px; border: 2px solid #c87000; border-radius: 8px; }");
    preview->setWordWrap(true);
    outer->addWidget(preview);

    auto* rowHost = new QWidget;
    auto* row = new QHBoxLayout(rowHost);
    row->setContentsMargins(0,0,0,0); row->setSpacing(0);
    outer->addWidget(rowHost, 1);

    auto colHead = [](const QString& title){
        auto* f = new QFrame; f->setProperty("role","modeToolbar"); f->setFixedHeight(28);
        auto* l = new QHBoxLayout(f); l->setContentsMargins(12,0,12,0);
        auto* t = new QLabel(title); t->setProperty("mono",true);
        l->addWidget(t); l->addStretch(1);
        return f;
    };

    // LEFT — operations palette
    auto* left = new QFrame; left->setFixedWidth(220);
    auto* leftLay = new QVBoxLayout(left); leftLay->setContentsMargins(0,0,0,0); leftLay->setSpacing(0);
    leftLay->addWidget(colHead(tr("OPERATIONS")));
    auto* search = new QLineEdit; search->setPlaceholderText(tr("Search…")); search->setProperty("mono",true);
    auto* searchHolder = new QFrame; auto* sh = new QHBoxLayout(searchHolder); sh->setContentsMargins(10,8,10,4); sh->addWidget(search);
    leftLay->addWidget(searchHolder);
    auto* ops = new QListWidget;
    const QVector<QPair<QString, QStringList>> cats = {
        {tr("ORGANIZE"), {tr("Merge"), tr("Split"), tr("Extract"), tr("Reorder")}},
        {tr("TRANSFORM"), {tr("OCR"), tr("Convert to Word"), tr("Watermark"), tr("Compress")}},
        {tr("PROTECT"),  {tr("Encrypt"), tr("Redact"), tr("Sign")}},
        {tr("EXPORT"),   {tr("Save As"), tr("Email"), tr("Cloud Upload")}},
    };
    for (const auto& c : cats) {
        auto* head = new QListWidgetItem("— " + c.first);
        head->setFlags(Qt::NoItemFlags);
        head->setForeground(Theme::fg3());
        ops->addItem(head);
        for (const auto& op : c.second) ops->addItem(op);
    }
    leftLay->addWidget(ops, 1);
    row->addWidget(left);

    // CENTER — pipeline
    auto* center = new QFrame; auto* cLay = new QVBoxLayout(center); cLay->setContentsMargins(0,0,0,0); cLay->setSpacing(0);
    cLay->addWidget(colHead(tr("PIPELINE · 5 STEPS")));
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* pipeHost = new QWidget; auto* pipeLay = new QVBoxLayout(pipeHost); pipeLay->setContentsMargins(20,18,20,18); pipeLay->setSpacing(0);
    const QVector<QPair<QString,QString>> steps = {
        {"1. INPUT",      "~/Watch/incoming/ · *.pdf · Watch Folder ON"},
        {"2. OCR",        "EN+FR · ≥80% confidence · Tesseract 5"},
        {"3. COMPRESS",   "eBook 150 DPI · ↓70%"},
        {"4. WATERMARK",  "CONFIDENTIAL · 30% · Center"},
        {"5. OUTPUT",     "~/Processed/processed_{filename}_{date}.pdf"},
    };
    for (int i = 0; i < steps.size(); ++i) {
        auto* card = new QFrame;
        card->setStyleSheet("border:1px solid #4a4d52; background:#1e1f22; padding:10px 12px;");
        auto* l = new QVBoxLayout(card);
        auto* t = new QLabel(steps[i].first); t->setStyleSheet("font-weight:700;color:#dfe1e5;");
        auto* d = new QLabel(steps[i].second); d->setProperty("mono",true);
        l->addWidget(t); l->addWidget(d);
        pipeLay->addWidget(card);
        if (i < steps.size() - 1) {
            auto* arrow = new QLabel(QString::fromUtf8("\xe2\x86\x93")); // ↓
            arrow->setAlignment(Qt::AlignCenter); arrow->setStyleSheet("color:#71747a;padding:6px;");
            pipeLay->addWidget(arrow);
        }
    }
    pipeLay->addStretch(1);
    scroll->setWidget(pipeHost);
    cLay->addWidget(scroll, 1);

    // ── Run bar ──────────────────────────────────────────────────────
    auto* run = new QToolButton; run->setText(tr("▶  RUN NOW")); run->setProperty("variant","accent"); run->setFixedHeight(38);
    auto* runHolder = new QFrame; auto* rh = new QHBoxLayout(runHolder); rh->setContentsMargins(20,8,20,8); rh->addWidget(run);
    cLay->addWidget(runHolder);

    // ── Progress & status ────────────────────────────────────────────
    auto* progressFrame = new QFrame;
    progressFrame->setStyleSheet("background:#1a1b1e; border-top:1px solid #393b40; padding:8px;");
    auto* pl = new QVBoxLayout(progressFrame);
    pl->setContentsMargins(12, 6, 12, 6);
    pl->setSpacing(4);

    m_statusLabel = new QLabel(tr("IDLE"));
    m_statusLabel->setProperty("mono", true);
    pl->addWidget(m_statusLabel);

    m_overallProgress = new QProgressBar; m_overallProgress->setRange(0, 100); m_overallProgress->setValue(0);
    m_fileProgress    = new QProgressBar; m_fileProgress->setRange(0, 100);    m_fileProgress->setValue(0);
    pl->addWidget(m_overallProgress);
    pl->addWidget(m_fileProgress);

    // Export log button (hidden until batch completes)
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    m_exportLogBtn = new QPushButton(tr("Export Log"));
    m_exportLogBtn->setStyleSheet("padding:4px 12px; font-size:10px;");
    m_exportLogBtn->setVisible(false);
    btnRow->addWidget(m_exportLogBtn);
    pl->addLayout(btnRow);

    cLay->addWidget(progressFrame);

    // ── Log view ─────────────────────────────────────────────────────
    m_logView = new QTextEdit; m_logView->setReadOnly(true); m_logView->setFixedHeight(130);
    m_logView->setStyleSheet(
        "font-family:'JetBrains Mono',monospace; font-size:10px;"
        "background:#1a1b1e; color:#d4d4d4; border-top:1px solid #393b40;");
    cLay->addWidget(m_logView);

    row->addWidget(center, 1);

    // ── Connections ──────────────────────────────────────────────────
    connect(run, &QToolButton::clicked, this, &BatchMode::onRunClicked);
    connect(&m_watcher, &QFutureWatcher<void>::progressValueChanged, this, &BatchMode::onBatchProgress);
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &BatchMode::onBatchFinished);
    connect(m_exportLogBtn, &QPushButton::clicked, this, &BatchMode::onExportLog);

    // RIGHT — configure
    auto* right = new QFrame; right->setFixedWidth(260);
    auto* rLay = new QVBoxLayout(right); rLay->setContentsMargins(0,0,0,0); rLay->setSpacing(0);
    rLay->addWidget(colHead(tr("CONFIGURE · OCR")));
    auto* cfg = new QLabel("Language · EN + FR\nEngine · Tesseract 5\nConfidence · 80%\nAuto-correct · ON\nPreserve images · ON\nDeskew · OFF\nPage range · all\nOutput · Searchable PDF");
    cfg->setProperty("mono", true); cfg->setMargin(14);
    rLay->addWidget(cfg); rLay->addStretch(1);
    row->addWidget(right);

    // ── Disable all interactive child widgets (preview-only mode) ────
    const auto allChildren = findChildren<QWidget*>();
    for (auto* w : allChildren) {
        if (w == preview) continue;
        if (qobject_cast<QPushButton*>(w) || qobject_cast<QLineEdit*>(w) ||
            qobject_cast<QComboBox*>(w) || qobject_cast<QCheckBox*>(w) ||
            qobject_cast<QRadioButton*>(w) || qobject_cast<QToolButton*>(w)) {
            w->setEnabled(false);
        }
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────

void BatchMode::appendLog(const QString& text, const QString& color) {
    if (color.isEmpty())
        m_logView->append(text);
    else
        m_logView->append(QStringLiteral("<span style='color:%1'>%2</span>").arg(color, text.toHtmlEscaped()));
}

void BatchMode::appendFileResult(const QString& file, bool success, const QString& detail) {
    if (success) {
        appendLog(QStringLiteral("  ✓ %1").arg(file), "#4ec96d");
    } else {
        appendLog(QStringLiteral("  ✕ %1 — %2").arg(file, detail), "#c8442b");
    }
}

void BatchMode::showSummary() {
    int total = m_successCount + m_failCount;
    int warnings = m_errorLog.warningCount();

    QString summary = tr("BATCH COMPLETE — %1 of %2 succeeded").arg(m_successCount).arg(total);
    if (m_failCount > 0)
        summary += tr(", %1 failed").arg(m_failCount);
    if (warnings > 0)
        summary += tr(", %1 warnings").arg(warnings);

    appendLog(QString());
    appendLog(summary, m_failCount > 0 ? "#c8442b" : "#4ec96d");

    m_statusLabel->setText(summary);
    m_exportLogBtn->setVisible(m_errorLog.count() > 0);
}

// ── Slots ────────────────────────────────────────────────────────────────

void BatchMode::onRunClicked() {
    m_filesToProcess.clear();
    m_overallProgress->setRange(0, m_filesToProcess.size());
    m_overallProgress->setValue(0);
    m_fileProgress->setValue(0);
    m_logView->clear();
    m_errorLog.clear();
    m_successCount = 0;
    m_failCount    = 0;
    m_exportLogBtn->setVisible(false);
    m_statusLabel->setText(tr("Batch operations preview — not wired in v1.0.0"));
    appendLog(tr("Starting batch processing — %1 files queued").arg(m_filesToProcess.size()));

    QFuture<void> future = QtConcurrent::map(m_filesToProcess, [this](const QString& file) {
        // Simulate per-file work — in production this calls the engine pipeline
        bool success = true;
        QString failReason;
        for (int i = 0; i < 5; ++i) {
            QThread::msleep(200);
        }

        QMetaObject::invokeMethod(this, [this, file, success, failReason]() {
            if (success) {
                ++m_successCount;
                appendFileResult(file, true);
            } else {
                ++m_failCount;
                appendFileResult(file, false, failReason);

                ErrorInfo err = ErrorInfo::error(
                    QObject::tr("Failed to process: %1").arg(file),
                    failReason,
                    ErrorInfo::Skip);
                err.sourceFile = file;
                m_errorLog.append(std::move(err));
            }
            m_overallProgress->setValue(m_successCount + m_failCount);
        }, Qt::QueuedConnection);
    });

    m_watcher.setFuture(future);
}

void BatchMode::onBatchProgress(int value) {
    m_overallProgress->setValue(value);
    int pct = m_filesToProcess.isEmpty() ? 0 : (value * 100 / m_filesToProcess.size());
    m_fileProgress->setValue(pct);
}

void BatchMode::onBatchFinished() {
    m_overallProgress->setValue(m_overallProgress->maximum());
    m_fileProgress->setValue(100);
    showSummary();
}

void BatchMode::onExportLog() {
    if (m_errorLog.count() == 0) return;

    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Batch Log"), {},
        tr("JSON (*.json);;CSV (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    bool ok = path.endsWith(".csv", Qt::CaseInsensitive)
                  ? m_errorLog.exportCsv(path)
                  : m_errorLog.exportJson(path);

    if (ok)
        appendLog(tr("Log exported to %1").arg(path), "#5b9bd5");
    else
        appendLog(tr("Failed to export log to %1").arg(path), "#c8442b");
}

} // namespace gp
