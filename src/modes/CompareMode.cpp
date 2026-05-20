#include "CompareMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include "ui/CompareWidget.h"

namespace gp {

CompareMode::CompareMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // toolbar
    auto* tb = new QFrame;
    tb->setProperty("role","modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* tr = new QHBoxLayout(tb);
    tr->setContentsMargins(10,0,10,0); tr->setSpacing(6);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    tr->addWidget(mono("COMPARE"));
    tr->addWidget(mono("Q4-Report-v1.pdf   ↔   Q4-Report-v2.pdf"));
    auto* prev = new QToolButton; prev->setText("← PREV"); prev->setProperty("variant","ghost"); tr->addWidget(prev);
    auto* next = new QToolButton; next->setText("NEXT →"); next->setProperty("variant","ghost"); tr->addWidget(next);
    m_statusLabel = mono("CHANGE 0 OF 0");
    tr->addWidget(m_statusLabel);
    tr->addStretch(1);
    
    auto* toggleDiff = new QToolButton; 
    toggleDiff->setText("Toggle Overlay"); 
    toggleDiff->setCheckable(true);
    connect(toggleDiff, &QToolButton::toggled, [this](bool checked) {
        m_compareWidget->setShowPixelDiff(checked);
    });
    tr->addWidget(toggleDiff);
    
    auto* close = new QToolButton; close->setText("Close Compare"); close->setProperty("variant","ghost"); tr->addWidget(close);
    col->addWidget(tb);

    m_compareWidget = new CompareWidget(this);
    col->addWidget(m_compareWidget, 1);

    // changes panel
    auto* changes = new QFrame;
    changes->setFixedHeight(160);
    auto* cl = new QVBoxLayout(changes); cl->setContentsMargins(0,0,0,0); cl->setSpacing(0);
    auto* ch = new QFrame; ch->setProperty("role","modeToolbar"); ch->setFixedHeight(26);
    auto* chr = new QHBoxLayout(ch); chr->setContentsMargins(12,0,12,0);
    auto* chl = new QLabel("CHANGES"); chl->setProperty("mono",true); chr->addWidget(chl); chr->addStretch(1);
    cl->addWidget(ch);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"#","Page","Description"});
    m_tree->setRootIsDecorated(false);
    cl->addWidget(m_tree, 1);
    col->addWidget(changes);

    connect(&m_watcher, &QFutureWatcher<DiffResult>::finished, this, &CompareMode::onDiffFinished);
}

void CompareMode::compareFiles(const QString& file1, const QString& file2) {
    m_compareWidget->loadDocuments(file1, file2);
    m_statusLabel->setText("COMPARING...");
    m_tree->clear();

    QFuture<DiffResult> future = QtConcurrent::run([file1, file2]() {
        DiffEngine engine;
        return engine.compare(file1, file2, 150);
    });
    m_watcher.setFuture(future);
}

void CompareMode::onDiffFinished() {
    m_lastResult = m_watcher.result();
    m_compareWidget->setDiffResult(m_lastResult);

    if (m_lastResult.isIdentical) {
        m_statusLabel->setText("FILES ARE IDENTICAL");
        return;
    }

    int totalChanges = 0;
    m_tree->clear();
    for (const auto& page : m_lastResult.pages) {
        int changes = page.textAdded.size() + page.textRemoved.size() + (page.pixelDiffCount > 0 ? 1 : 0);
        if (changes > 0) {
            totalChanges += changes;
            QString desc = QString("Added: %1 words, Removed: %2 words, Pixels changed: %3")
                               .arg(page.textAdded.size())
                               .arg(page.textRemoved.size())
                               .arg(page.pixelDiffCount);
            new QTreeWidgetItem(m_tree, {QString::number(totalChanges), QString("p.%1").arg(page.pageIndex + 1), desc});
        }
    }

    m_statusLabel->setText(QString("%1 CHANGES").arg(totalChanges));
}

} // namespace gp
