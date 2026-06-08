// SPDX-License-Identifier: Apache-2.0
#include "CompareMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QColor>
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
    auto* hrow = new QHBoxLayout(tb);
    hrow->setContentsMargins(10,0,10,0); hrow->setSpacing(6);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    hrow->addWidget(mono(tr("COMPARE")));
    hrow->addWidget(mono("Q4-Report-v1.pdf   ↔   Q4-Report-v2.pdf"));
    auto* prev = new QToolButton; prev->setText(tr("← PREV")); prev->setProperty("variant","ghost");
    prev->setToolTip(tr("Navigate to previous change"));
    hrow->addWidget(prev);
    auto* next = new QToolButton; next->setText(tr("NEXT →")); next->setProperty("variant","ghost");
    next->setToolTip(tr("Navigate to next change"));
    hrow->addWidget(next);
    m_statusLabel = mono(tr("CHANGE 0 OF 0"));
    hrow->addWidget(m_statusLabel);
    hrow->addStretch(1);

    auto* toggleDiff = new QToolButton;
    toggleDiff->setText(tr("Toggle Overlay"));
    toggleDiff->setCheckable(true);
    connect(toggleDiff, &QToolButton::toggled, [this](bool checked) {
        m_compareWidget->setShowPixelDiff(checked);
    });
    hrow->addWidget(toggleDiff);

    auto* close = new QToolButton; close->setText(tr("Close Compare")); close->setProperty("variant","ghost");
    close->setEnabled(false);
    close->setToolTip(tr("Not available in this version"));
    hrow->addWidget(close);
    col->addWidget(tb);

    m_compareWidget = new CompareWidget(this);
    col->addWidget(m_compareWidget, 1);

    // Wire PREV/NEXT to CompareWidget navigation (captured after m_compareWidget exists)
    connect(prev, &QToolButton::clicked, m_compareWidget, &CompareWidget::prevChange);
    connect(next, &QToolButton::clicked, m_compareWidget, &CompareWidget::nextChange);

    // changes panel
    auto* changes = new QFrame;
    changes->setFixedHeight(160);
    auto* cl = new QVBoxLayout(changes); cl->setContentsMargins(0,0,0,0); cl->setSpacing(0);
    auto* ch = new QFrame; ch->setProperty("role","modeToolbar"); ch->setFixedHeight(26);
    auto* chr = new QHBoxLayout(ch); chr->setContentsMargins(12,0,12,0);
    auto* chl = new QLabel(CompareMode::tr("CHANGES")); chl->setProperty("mono",true); chr->addWidget(chl); chr->addStretch(1);
    cl->addWidget(ch);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({CompareMode::tr("#"), CompareMode::tr("Page"), CompareMode::tr("Description")});
    m_tree->setRootIsDecorated(false);
    cl->addWidget(m_tree, 1);
    col->addWidget(changes);

    connect(&m_watcher, &QFutureWatcher<DiffResult>::finished, this, &CompareMode::onDiffFinished);
}

void CompareMode::compareFiles(const QString& file1, const QString& file2) {
    m_compareWidget->loadDocuments(file1, file2);
    m_statusLabel->setText(tr("COMPARING..."));
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
        m_statusLabel->setText(tr("FILES ARE IDENTICAL"));
        return;
    }

    int totalChanges = 0;
    m_tree->clear();
    for (const auto& page : m_lastResult.pages) {
        const int textChanges = page.textAdded.size() + page.textRemoved.size()
                                + page.moves.size();
        const int changes = textChanges + (page.pixelDiffCount > 0 ? 1 : 0);
        if (changes > 0) {
            totalChanges += changes;
            QString desc = tr("+%1 words  -%2 words  ↔%3 moved  ~%4 px")
                               .arg(page.textAdded.size())
                               .arg(page.textRemoved.size())
                               .arg(page.moves.size())
                               .arg(page.pixelDiffCount);
            auto* item = new QTreeWidgetItem(m_tree,
                {QString::number(totalChanges),
                 QString("p.%1").arg(page.pageIndex + 1),
                 desc});
            // Colour code items that have moves
            if (!page.moves.isEmpty())
                item->setForeground(2, QColor("#d97c00"));  // orange for moves
        }
    }

    m_statusLabel->setText(tr("%1 CHANGES").arg(totalChanges));
}

} // namespace gp
