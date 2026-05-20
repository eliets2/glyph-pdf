#include "BatchMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include <QCoreApplication>

namespace gp {

BatchMode::BatchMode(QWidget* parent) : QWidget(parent) {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0,0,0,0); row->setSpacing(0);

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
    leftLay->addWidget(colHead("OPERATIONS"));
    auto* search = new QLineEdit; search->setPlaceholderText("Search…"); search->setProperty("mono",true);
    auto* searchHolder = new QFrame; auto* sh = new QHBoxLayout(searchHolder); sh->setContentsMargins(10,8,10,4); sh->addWidget(search);
    leftLay->addWidget(searchHolder);
    auto* ops = new QListWidget;
    const QVector<QPair<QString, QStringList>> cats = {
        {"ORGANIZE", {"Merge","Split","Extract","Reorder"}},
        {"TRANSFORM",{"OCR","Convert to Word","Watermark","Compress"}},
        {"PROTECT",  {"Encrypt","Redact","Sign"}},
        {"EXPORT",   {"Save As","Email","Cloud Upload"}},
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
    cLay->addWidget(colHead("PIPELINE · 5 STEPS"));
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
            auto* arrow = new QLabel("↓"); arrow->setAlignment(Qt::AlignCenter); arrow->setStyleSheet("color:#71747a;padding:6px;");
            pipeLay->addWidget(arrow);
        }
    }
    pipeLay->addStretch(1);
    scroll->setWidget(pipeHost);
    cLay->addWidget(scroll, 1);

    auto* run = new QToolButton; run->setText("▶  RUN NOW"); run->setProperty("variant","accent"); run->setFixedHeight(38);
    auto* runHolder = new QFrame; auto* rh = new QHBoxLayout(runHolder); rh->setContentsMargins(20,8,20,8); rh->addWidget(run);
    cLay->addWidget(runHolder);

    // Add progress bars and log view
    m_overallProgress = new QProgressBar; m_overallProgress->setRange(0, 100); m_overallProgress->setValue(0);
    m_fileProgress = new QProgressBar; m_fileProgress->setRange(0, 100); m_fileProgress->setValue(0);
    m_logView = new QTextEdit; m_logView->setReadOnly(true); m_logView->setFixedHeight(100);
    m_logView->setStyleSheet("font-family: monospace; background: #1e1e1e; color: #d4d4d4;");
    
    cLay->addWidget(new QLabel("Overall Progress:"));
    cLay->addWidget(m_overallProgress);
    cLay->addWidget(new QLabel("Current File Progress:"));
    cLay->addWidget(m_fileProgress);
    cLay->addWidget(m_logView);

    row->addWidget(center, 1);

    connect(run, &QToolButton::clicked, this, &BatchMode::onRunClicked);
    connect(&m_watcher, &QFutureWatcher<void>::progressValueChanged, this, &BatchMode::onBatchProgress);
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &BatchMode::onBatchFinished);

    // RIGHT — configure
    auto* right = new QFrame; right->setFixedWidth(260);
    auto* rLay = new QVBoxLayout(right); rLay->setContentsMargins(0,0,0,0); rLay->setSpacing(0);
    rLay->addWidget(colHead("CONFIGURE · OCR"));
    auto* cfg = new QLabel("Language · EN + FR\nEngine · Tesseract 5\nConfidence · 80%\nAuto-correct · ON\nPreserve images · ON\nDeskew · OFF\nPage range · all\nOutput · Searchable PDF");
    cfg->setProperty("mono", true); cfg->setMargin(14);
    rLay->addWidget(cfg); rLay->addStretch(1);
    row->addWidget(right);
}

void BatchMode::onRunClicked() {
    m_filesToProcess = {"doc1.pdf", "doc2.pdf", "doc3.pdf", "doc4.pdf", "doc5.pdf"};
    m_overallProgress->setRange(0, m_filesToProcess.size());
    m_overallProgress->setValue(0);
    m_logView->clear();
    m_logView->append("Starting batch processing...");

    // Process files in parallel
    QFuture<void> future = QtConcurrent::map(m_filesToProcess, [this](const QString& file) {
        // Simulate work that doesn't block UI
        for (int i = 0; i < 5; ++i) { // 5 steps per file
            QThread::msleep(200); // Simulate processing time
        }
        
        // Use QMetaObject::invokeMethod to safely update UI from worker thread
        QMetaObject::invokeMethod(this, [this, file]() {
            m_logView->append("Successfully processed: " + file);
        }, Qt::QueuedConnection);
    });

    m_watcher.setFuture(future);
}

void BatchMode::onBatchProgress(int value) {
    m_overallProgress->setValue(value);
    // Dummy file progress
    m_fileProgress->setValue((value * 100) % 100); 
}

void BatchMode::onBatchFinished() {
    m_overallProgress->setValue(m_overallProgress->maximum());
    m_fileProgress->setValue(100);
    m_logView->append("Batch processing completed.");
}

} // namespace gp
