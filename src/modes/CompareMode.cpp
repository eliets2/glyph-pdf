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
#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include "ui/CompareWidget.h"

namespace gp {

// §9.10/R11: the CHANGES-row data roles live in CompareMode.h (shared with the
// tests that pin the filter seam).

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
    m_filesLabel = mono(tr("No files selected — use Compare Docs to open two PDFs"));
    m_filesLabel->setObjectName(QStringLiteral("cmpFilesLabel"));  // U04: testable Old/New surface
    hrow->addWidget(m_filesLabel);
    auto* pickBtn = new QToolButton;
    pickBtn->setText(tr("Compare Docs…"));
    pickBtn->setToolTip(tr("Choose two PDFs and compare them"));
    // §9.10 P0: this entry point was built but never connected — the whole
    // comparison feature was unreachable from the UI.
    connect(pickBtn, &QToolButton::clicked, this, [this] { promptAndCompare(); });
    hrow->addWidget(pickBtn);
    m_prevBtn = new QToolButton; m_prevBtn->setText(tr("← PREV")); m_prevBtn->setProperty("variant","ghost");
    m_prevBtn->setToolTip(tr("Navigate to previous change"));
    // O4: disabled until a diff completes and produces at least one change.
    m_prevBtn->setEnabled(false);
    hrow->addWidget(m_prevBtn);
    m_nextBtn = new QToolButton; m_nextBtn->setText(tr("NEXT →")); m_nextBtn->setProperty("variant","ghost");
    m_nextBtn->setToolTip(tr("Navigate to next change"));
    m_nextBtn->setEnabled(false);
    hrow->addWidget(m_nextBtn);
    m_statusLabel = mono(tr("CHANGE 0 OF 0"));
    m_statusLabel->setObjectName(QStringLiteral("cmpStatusLabel"));  // R11: testable total
    hrow->addWidget(m_statusLabel);
    hrow->addStretch(1);

    auto* toggleDiff = new QToolButton;
    toggleDiff->setText(tr("Toggle Overlay"));
    toggleDiff->setCheckable(true);
    connect(toggleDiff, &QToolButton::toggled, [this](bool checked) {
        m_compareWidget->setShowPixelDiff(checked);
    });
    hrow->addWidget(toggleDiff);

    // U04: linked scrolling — on by default (Acrobat-style linked compare);
    // the objectName is the test handle for the toggle.
    m_linkScrollBtn = new QToolButton;
    m_linkScrollBtn->setObjectName(QStringLiteral("cmpBtnLinkScroll"));
    m_linkScrollBtn->setText(tr("Link Scroll"));
    m_linkScrollBtn->setToolTip(
        tr("Scroll both documents together (proportional page mapping)"));
    m_linkScrollBtn->setCheckable(true);
    m_linkScrollBtn->setChecked(true);
    m_linkScrollBtn->setAutoRaise(true);
    connect(m_linkScrollBtn, &QToolButton::toggled, this, [this](bool linked) {
        m_compareWidget->setLinkedScrolling(linked);
    });
    hrow->addWidget(m_linkScrollBtn);

    // U04: swap original/revised sides and compare again.
    m_swapBtn = new QToolButton;
    m_swapBtn->setObjectName(QStringLiteral("cmpSwapButton"));
    m_swapBtn->setText(tr("\xE2\x87\x84 Swap Sides"));
    m_swapBtn->setToolTip(tr("Swap the original and revised documents and compare again"));
    m_swapBtn->setProperty("variant", "ghost");
    m_swapBtn->setEnabled(false);
    connect(m_swapBtn, &QToolButton::clicked, this, [this] {
        if (m_watcher.isRunning() || m_file1.isEmpty() || m_file2.isEmpty())
            return;
        // Copy first: compareFiles takes const refs, which would bind
        // directly to these members and alias the m_file1 = file1 store.
        const QString original = m_file1;
        const QString revised  = m_file2;
        compareFiles(revised, original);
    });
    hrow->addWidget(m_swapBtn);

    m_exportBtn = new QToolButton;
    m_exportBtn->setText(tr("Export Report"));
    m_exportBtn->setProperty("variant","ghost");
    m_exportBtn->setEnabled(false);
    m_exportBtn->setToolTip(tr("Export the comparison results to HTML or text"));
    connect(m_exportBtn, &QToolButton::clicked, this, &CompareMode::onExportReport);
    hrow->addWidget(m_exportBtn);

    // "Close Compare" button removed (AR-8 D3): navigating away from compare mode
    // is handled by the mode strip, so a second button here was inert dead UI.
    col->addWidget(tb);

    m_compareWidget = new CompareWidget(this);

    // Wire PREV/NEXT to CompareWidget navigation (captured after m_compareWidget exists)
    connect(m_prevBtn, &QToolButton::clicked, m_compareWidget, &CompareWidget::prevChange);
    connect(m_nextBtn, &QToolButton::clicked, m_compareWidget, &CompareWidget::nextChange);

    // U04: one counter — the toolbar status tracks the selected change in the
    // shared sequence so tree, counter, next/previous and the views always
    // refer to the same change.
    connect(m_compareWidget, &CompareWidget::currentChangeChanged, this,
            [this](int index) {
        if (index < 0 || m_lastResult.isIdentical)
            return;
        m_statusLabel->setText(
            tr("CHANGE %1 OF %2").arg(index + 1).arg(m_compareWidget->changeCount()));
    });

    // changes panel
    auto* changes = new QFrame;
    // U04: the lower details area is resizable via the splitter below — the
    // old fixed 160px height becomes a floor (the text browser's own 80px
    // minimum still protects the inner pane).
    changes->setMinimumHeight(120);
    auto* cl = new QVBoxLayout(changes); cl->setContentsMargins(0,0,0,0); cl->setSpacing(0);
    auto* ch = new QFrame; ch->setProperty("role","modeToolbar"); ch->setFixedHeight(26);
    auto* chr = new QHBoxLayout(ch); chr->setContentsMargins(12,0,12,0);
    auto* chl = new QLabel(CompareMode::tr("CHANGES")); chl->setProperty("mono",true); chr->addWidget(chl); chr->addStretch(1);
    // §9.10: change-type filter toggles. The diff engine already tags every
    // change as text / move / pixel / page-move; these gate which rows the
    // tree SHOWS (view-level only — the diff result itself is never altered).
    // All default checked = pre-filter behaviour.
    auto addFilterToggle = [this, chr](QToolButton*& slot, const char* objectName,
                                       const QString& label, const QString& tip) {
        auto* b = new QToolButton;
        b->setObjectName(QString::fromLatin1(objectName));
        b->setText(label);
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setChecked(true);
        b->setAutoRaise(true);
        b->setFixedHeight(20);
        connect(b, &QToolButton::toggled, this, &CompareMode::applyChangeTypeFilters);
        chr->addWidget(b);
        slot = b;
    };
    addFilterToggle(m_filterText,     "cmpFilterText",     tr("Text"),       tr("Show text added/removed changes"));
    addFilterToggle(m_filterMove,     "cmpFilterMove",     tr("Moves"),      tr("Show moved-word changes"));
    addFilterToggle(m_filterPixel,    "cmpFilterPixel",    tr("Pixels"),     tr("Show pixel-difference changes"));
    addFilterToggle(m_filterPageMove, "cmpFilterPageMove", tr("Page moves"), tr("Show whole-page reorder changes"));
    // R11: pages added to / removed from the documents get their own gate so
    // structural rows can be filtered independently of whole-page reorders.
    addFilterToggle(m_filterPageAddRemove, "cmpFilterPageAddRemove", tr("Add/Rm pages"),
                    tr("Show pages added to or removed from the documents"));
    cl->addWidget(ch);

    m_tree = new QTreeWidget;
    m_tree->setObjectName("cmpChangesTree");  // §9.10: testable handle for the tree
    m_tree->setHeaderLabels({CompareMode::tr("#"), CompareMode::tr("Page"), CompareMode::tr("Description")});
    m_tree->setRootIsDecorated(false);
    cl->addWidget(m_tree, 1);

    // U04: the lower details area (document views vs changes list) is a
    // splitter — the user can give the change list more room instead of the
    // old fixed 160px band. Neither pane collapses away.
    auto* detailsSplit = new QSplitter(Qt::Vertical, this);
    detailsSplit->setObjectName(QStringLiteral("cmpDetailsSplitter"));
    detailsSplit->addWidget(m_compareWidget);
    detailsSplit->addWidget(changes);
    detailsSplit->setStretchFactor(0, 3);
    detailsSplit->setStretchFactor(1, 1);
    detailsSplit->setSizes({720, 200});
    detailsSplit->setChildrenCollapsible(false);
    col->addWidget(detailsSplit, 1);

    // R11: selecting a structural CHANGES row jumps the one shared change
    // sequence (text panel anchor + viewer pages). Rows that map into the
    // sequence carry kAnchorIndexRole; token-level page rows keep their
    // per-token anchors reachable through next/previous.
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (!current) return;
        const QVariant anchor = current->data(0, kAnchorIndexRole);
        if (anchor.isValid())
            m_compareWidget->scrollToChange(anchor.toInt());
    });

    connect(&m_watcher, &QFutureWatcher<DiffResult>::finished, this, &CompareMode::onDiffFinished);
}

void CompareMode::compareFiles(const QString& file1, const QString& file2) {
    m_file1 = file1;
    m_file2 = file2;
    m_compareWidget->loadDocuments(file1, file2);
    // Update the toolbar label: actual Old/New filenames AND page counts
    // (U04 — the compared scope is visible before the diff finishes).
    if (m_filesLabel) {
        m_filesLabel->setText(QStringLiteral("%1 (%2 pp)   \xE2\x86\x94   %3 (%4 pp)")
                                  .arg(QFileInfo(file1).fileName())
                                  .arg(m_compareWidget->leftPageCount())
                                  .arg(QFileInfo(file2).fileName())
                                  .arg(m_compareWidget->rightPageCount()));
    }
    m_statusLabel->setText(tr("COMPARING..."));
    m_tree->clear();
    if (m_exportBtn) m_exportBtn->setEnabled(false);
    // O4: reset nav buttons while comparison is running.
    if (m_prevBtn) m_prevBtn->setEnabled(false);
    if (m_nextBtn) m_nextBtn->setEnabled(false);
    if (m_swapBtn) m_swapBtn->setEnabled(false);   // U04: no swap mid-flight

    QFuture<DiffResult> future = QtConcurrent::run([file1, file2]() {
        DiffEngine engine;
        return engine.compare(file1, file2, 150);
    });
    m_watcher.setFuture(future);
}

void CompareMode::onDiffFinished() {
    const DiffResult result = m_watcher.result();
    m_compareWidget->setDiffResult(result);
    if (m_exportBtn) m_exportBtn->setEnabled(true);

    // §9.10: showDiffResult stores m_lastResult, rebuilds the CHANGES tree and
    // re-applies the current filter toggles.
    showDiffResult(result);

    if (m_lastResult.isIdentical) {
        m_statusLabel->setText(tr("FILES ARE IDENTICAL"));
        // No changes — leave PREV/NEXT disabled.
        return;
    }

    // O4: enable PREV/NEXT only now that we know there are actual changes.
    if (m_prevBtn) m_prevBtn->setEnabled(true);
    if (m_nextBtn) m_nextBtn->setEnabled(true);
    if (m_swapBtn) m_swapBtn->setEnabled(true);   // U04: sides swappable again
}

// §9.10: tree population split out of onDiffFinished so the change-type filter
// can be exercised (and re-applied) independently of the async watcher.
void CompareMode::showDiffResult(const DiffResult& result) {
    m_lastResult = result;

    // U04: the CompareWidget owns the one shared change sequence — give it
    // the result BEFORE the tree is built so every row's kAnchorIndexRole
    // maps into the live sequence (onDiffFinished drives the same order).
    m_compareWidget->setDiffResult(result);

    int totalChanges = 0;
    m_tree->clear();
    for (int pageIdx = 0; pageIdx < result.pages.size(); ++pageIdx) {
        const auto& page = result.pages.at(pageIdx);
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
            // §9.10: tag the row with the change types it actually contains.
            item->setData(0, kHasTextRole,
                          !page.textAdded.isEmpty() || !page.textRemoved.isEmpty());
            item->setData(0, kHasMoveRole, !page.moves.isEmpty());
            item->setData(0, kHasPixelRole, page.pixelDiffCount > 0);
            // U04: remember the row's raw position in DiffResult::pages so
            // the filtered anchor index can be recomputed on every toggle.
            item->setData(0, kPageDiffIndexRole, pageIdx);
            // Colour code items that have moves
            if (!page.moves.isEmpty())
                item->setForeground(2, QColor("#d97c00"));  // orange for moves
        }
    }

    // R11: structural page changes, in the one canonical order (pageChanges).
    // Moved pages appear exactly once as PageMoved entries — pageMoves is kept
    // populated by the engine for backward compatibility but is NOT emitted
    // separately, so nothing is double-counted. Added/removed rows carry
    // kIsPageAddRemoveRole (their own filter gate) and their sequence index so
    // tree selection jumps the shared next/previous sequence.
    int structuralAnchor = 0;
    for (const auto& ch : result.pageChanges) {
        ++totalChanges;
        QTreeWidgetItem* item = nullptr;
        switch (ch.type) {
        case DiffResult::PageChangeType::PageAdded:
            item = new QTreeWidgetItem(m_tree,
                {QString::number(totalChanges),
                 QString("p.%1 (new)").arg(ch.newPage + 1),
                 tr("Page %1 added in revised document").arg(ch.newPage + 1)});
            item->setForeground(2, QColor("#1f8a44"));  // green for additions
            item->setData(0, kIsPageAddRemoveRole, true);
            break;
        case DiffResult::PageChangeType::PageRemoved:
            item = new QTreeWidgetItem(m_tree,
                {QString::number(totalChanges),
                 QString("p.%1 (removed)").arg(ch.oldPage + 1),
                 tr("Page %1 removed from original document").arg(ch.oldPage + 1)});
            item->setForeground(2, QColor("#c8442b"));  // red for removals
            item->setData(0, kIsPageAddRemoveRole, true);
            break;
        case DiffResult::PageChangeType::PageMoved:
            item = new QTreeWidgetItem(m_tree,
                {QString::number(totalChanges),
                 QString("p.%1 \xE2\x86\x92 p.%2").arg(ch.oldPage + 1).arg(ch.newPage + 1),
                 tr("Page %1 moved to position %2").arg(ch.oldPage + 1).arg(ch.newPage + 1)});
            item->setForeground(2, QColor("#7a4cc8"));  // purple for page moves
            item->setData(0, kIsPageMoveRole, true);
            break;
        }
        // U04: raw position in the canonical pageChanges sequence.
        item->setData(0, kPageChangeIndexRole, structuralAnchor);
        // Build-time mapping (unfiltered index); applyChangeTypeFilters()
        // recomputes it against the live filtered sequence.
        item->setData(0, kAnchorIndexRole, structuralAnchor++);
    }

    // U04: the status total is the FILTERED change count — set by
    // applyChangeTypeFilters() below from the widget's shared sequence, so
    // the counter always matches what the tree shows.
    // Respect the toggles the user has already set (re-apply to the fresh rows).
    applyChangeTypeFilters();
}

// ── §9.10/U04: change-type filter — the ONE funnel ──────────────────────────────
// The toggles drive: (1) the widget's filtered change sequence (which
// next/prev, the viewers, the overlay and the counter read), (2) tree row
// visibility, (3) every row's kAnchorIndexRole mapping, and (4) the status
// total. One funnel, so the views cannot disagree.

CompareChangeFilter CompareMode::currentFilter() const {
    CompareChangeFilter f;
    f.showText          = !m_filterText     || m_filterText->isChecked();
    f.showMove          = !m_filterMove     || m_filterMove->isChecked();
    f.showPixel         = !m_filterPixel    || m_filterPixel->isChecked();
    f.showPageMove      = !m_filterPageMove || m_filterPageMove->isChecked();
    f.showPageAddRemove = !m_filterPageAddRemove || m_filterPageAddRemove->isChecked();
    return f;
}

void CompareMode::applyChangeTypeFilters() {
    if (!m_tree)
        return;
    const CompareChangeFilter filter = currentFilter();
    // U04: the widget owns the shared filtered sequence (rebuilds anchors,
    // clamps the selection, refreshes overlay/nav).
    m_compareWidget->setChangeFilter(filter);

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* item = m_tree->topLevelItem(i);
        bool show;
        bool structural = false;
        if (item->data(0, kIsPageMoveRole).toBool()) {
            show = filter.showPageMove;
            structural = true;
        } else if (item->data(0, kIsPageAddRemoveRole).toBool()) {
            show = filter.showPageAddRemove;
            structural = true;
        } else {
            // A page row carries several change types; it stays visible while
            // ANY of its (still-checked) tags matches.
            show = (item->data(0, kHasTextRole).toBool() && filter.showText)
                || (item->data(0, kHasMoveRole).toBool() && filter.showMove)
                || (item->data(0, kHasPixelRole).toBool() && filter.showPixel);
        }
        item->setHidden(!show);
        // U04: EVERY row maps into the filtered shared sequence (token page
        // rows navigate their page's anchor); rows the filter hides drop to
        // -1 so a stale selection can never jump a hidden change.
        const int anchorIndex = structural
            ? m_compareWidget->anchorIndexForStructuralChange(
                  item->data(0, kPageChangeIndexRole).toInt())
            : m_compareWidget->anchorIndexForPage(
                  item->data(0, kPageDiffIndexRole).toInt());
        item->setData(0, kAnchorIndexRole, anchorIndex);
    }

    // A filter change can hide the current row or invalidate its mapping —
    // drop the selection so the tree and the widget's clamped position agree.
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current && (current->isHidden()
                    || current->data(0, kAnchorIndexRole).toInt() < 0))
        m_tree->setCurrentItem(nullptr);

    // U04: the status total is the widget's filtered change count (pinned
    // equal to rowsVisibleForFilters by the tests). A zero-results filter
    // says so — never "identical" (engine-owned wording).
    if (m_lastResult.isIdentical)
        m_statusLabel->setText(tr("FILES ARE IDENTICAL"));
    else if (m_compareWidget->changeCount() == 0)
        m_statusLabel->setText(tr("NO CHANGES MATCH THE FILTER"));
    else
        m_statusLabel->setText(tr("%1 CHANGES").arg(m_compareWidget->changeCount()));
}

int CompareMode::rowsVisibleForFilters(const DiffResult& result, bool showText,
                                       bool showMove, bool showPixel, bool showPageMove,
                                       bool showPageAddRemove) {
    int visible = 0;
    for (const auto& page : result.pages) {
        const int changes = page.textAdded.size() + page.textRemoved.size()
                            + page.moves.size() + (page.pixelDiffCount > 0 ? 1 : 0);
        if (changes <= 0)
            continue;
        const bool hasText  = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty();
        const bool hasMove  = !page.moves.isEmpty();
        const bool hasPixel = page.pixelDiffCount > 0;
        if ((hasText && showText) || (hasMove && showMove) || (hasPixel && showPixel))
            ++visible;
    }
    // R11: structural rows come from the single canonical pageChanges sequence.
    // Moved pages appear exactly once there, so the legacy pageMoves list is
    // NOT counted separately (no double counting).
    for (const auto& ch : result.pageChanges) {
        if (ch.type == DiffResult::PageChangeType::PageMoved) {
            if (showPageMove)
                ++visible;
        } else if (showPageAddRemove) {
            ++visible;
        }
    }
    return visible;
}

// ── Report export ───────────────────────────────────────────────────────────────

void CompareMode::onExportReport() {
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Comparison Report"), QStringLiteral("compare-report.html"),
        tr("HTML Report (*.html);;Text Report (*.txt)"), &selectedFilter);
    if (path.isEmpty()) return;

    const bool asText = selectedFilter.contains(QStringLiteral("*.txt"))
                        || path.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive);
    if (!asText && !path.endsWith(QLatin1String(".html"), Qt::CaseInsensitive))
        path += QStringLiteral(".html");

    // U04: export the SAME selected scope and filter state the UI describes.
    const CompareChangeFilter filter = currentFilter();
    const QString content = asText ? buildTextReport(filter) : buildHtmlReport(filter);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Report"),
            tr("Could not write report to:\n%1").arg(path));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << content;
    f.close();

    m_statusLabel->setText(tr("Report exported: %1").arg(QFileInfo(path).fileName()));
}

QString CompareMode::buildHtmlReport() const {
    return buildHtmlReport(CompareChangeFilter{});
}

QString CompareMode::buildHtmlReport(const CompareChangeFilter& filter) const {
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };
    // U04: gates mirror the widget's filtered sequence — a structural change
    // shows behind its page-level toggle; a page shows while ANY of its
    // still-checked tags matches (same rule as rowsVisibleForFilters).
    auto pageVisible = [&filter](const PageDiff& page) {
        const bool hasText = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty();
        const bool hasMove = !page.moves.isEmpty();
        const bool hasPixel = page.pixelDiffCount > 0;
        return (hasText && filter.showText) || (hasMove && filter.showMove)
               || (hasPixel && filter.showPixel);
    };

    int totalAdded = 0, totalRemoved = 0, totalMoved = 0, changedPages = 0;
    for (const auto& page : m_lastResult.pages) {
        if (!pageVisible(page))
            continue;
        totalAdded   += page.textAdded.size();
        totalRemoved += page.textRemoved.size();
        totalMoved   += page.moves.size();
        ++changedPages;
    }
    const int totalChanges = totalAdded + totalRemoved + totalMoved;

    // R11: structural page changes, summarised per type (filtered).
    int pagesAdded = 0, pagesRemoved = 0, pagesMoved = 0;
    for (const auto& ch : m_lastResult.pageChanges) {
        if (ch.type == DiffResult::PageChangeType::PageMoved) {
            if (filter.showPageMove) ++pagesMoved;
        } else if (filter.showPageAddRemove) {
            if (ch.type == DiffResult::PageChangeType::PageAdded)   ++pagesAdded;
            if (ch.type == DiffResult::PageChangeType::PageRemoved) ++pagesRemoved;
        }
    }

    QString html;
    QTextStream o(&html);
    o << "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n";
    o << "<title>GlyphPDF Comparison Report</title>\n";
    o << "<style>\n"
         "body{font-family:'Segoe UI',Arial,sans-serif;margin:24px;color:#1a1b1e;}\n"
         "h1{font-size:20px;} h2{font-size:15px;margin-top:24px;}\n"
         ".summary{background:#f4f5f7;border:1px solid #d8dade;border-radius:6px;padding:12px 16px;}\n"
         ".summary td{padding:2px 16px 2px 0;}\n"
         "table.diff{border-collapse:collapse;width:100%;margin-top:8px;}\n"
         "table.diff th,table.diff td{border:1px solid #d8dade;padding:4px 8px;"
         "vertical-align:top;font-size:13px;width:50%;}\n"
         "table.diff th{background:#eceef1;text-align:left;}\n"
         ".removed{color:#c8442b;background:#fbecea;}\n"
         ".added{color:#1f8a44;background:#e9f6ee;}\n"
         ".unchanged{color:#71747a;}\n"
         ".moved{color:#d97c00;}\n"
         "</style></head>\n<body>\n";

    o << "<h1>GlyphPDF Comparison Report</h1>\n";
    o << "<table class=\"summary\">\n";
    o << "<tr><td><b>Original</b></td><td>" << esc(m_file1) << "</td></tr>\n";
    o << "<tr><td><b>Revised</b></td><td>"  << esc(m_file2) << "</td></tr>\n";
    o << "<tr><td><b>Date</b></td><td>" << esc(QDate::currentDate().toString(Qt::ISODate)) << "</td></tr>\n";
    o << "<tr><td><b>Pages</b></td><td>" << m_lastResult.pageCount1 << " \xE2\x86\x92 "
      << m_lastResult.pageCount2 << "</td></tr>\n";
    o << "<tr><td><b>Total changes</b></td><td>" << totalChanges
      << " (" << totalAdded << " added, " << totalRemoved << " removed, "
      << totalMoved << " moved across " << changedPages << " pages)</td></tr>\n";
    o << "<tr><td><b>Page changes</b></td><td>" << pagesAdded << " added, "
      << pagesRemoved << " removed, " << pagesMoved << " moved</td></tr>\n";
    o << "</table>\n";

    if (m_lastResult.isIdentical) {
        o << "<p class=\"unchanged\">The documents are identical.</p>\n";
        o << "</body></html>\n";
        return html;
    }

    // R11/U04: structural page changes — every entry names the page and the
    // side it lives on (or moved between). Single canonical sequence
    // (pageChanges), gated by the filter; the legacy pageMoves list is not
    // emitted separately.
    bool hasStructural = false;
    for (const auto& ch : m_lastResult.pageChanges) {
        if (ch.type == DiffResult::PageChangeType::PageMoved
                ? !filter.showPageMove : !filter.showPageAddRemove)
            continue;
        hasStructural = true;
    }
    if (hasStructural) {
        o << "<h2>Structural page changes</h2>\n<ul>\n";
        for (const auto& ch : m_lastResult.pageChanges) {
            if (ch.type == DiffResult::PageChangeType::PageMoved
                    ? !filter.showPageMove : !filter.showPageAddRemove)
                continue;
            switch (ch.type) {
            case DiffResult::PageChangeType::PageAdded:
                o << "<li class=\"added\">Page " << (ch.newPage + 1)
                  << " added in revised document";
                break;
            case DiffResult::PageChangeType::PageRemoved:
                o << "<li class=\"removed\">Page " << (ch.oldPage + 1)
                  << " removed from original document";
                break;
            case DiffResult::PageChangeType::PageMoved:
                o << "<li class=\"moved\">Page " << (ch.oldPage + 1)
                  << " moved to position " << (ch.newPage + 1);
                break;
            }
            if (!ch.excerpt.isEmpty())
                o << " <span class=\"unchanged\">" << esc(ch.excerpt) << "</span>";
            o << "</li>\n";
        }
        o << "</ul>\n";
    }

    for (const auto& page : m_lastResult.pages) {
        if (!pageVisible(page)) continue;

        o << "<h2>Page " << (page.pageIndex + 1) << "</h2>\n";
        o << "<table class=\"diff\"><tr><th>Removed</th><th>Added</th></tr>\n";
        o << "<tr><td class=\"removed\">"
          << (page.textRemoved.isEmpty() ? QStringLiteral("<span class=\"unchanged\">\xE2\x80\x94</span>")
                                         : esc(page.textRemoved.join(QLatin1Char(' '))))
          << "</td><td class=\"added\">"
          << (page.textAdded.isEmpty() ? QStringLiteral("<span class=\"unchanged\">\xE2\x80\x94</span>")
                                       : esc(page.textAdded.join(QLatin1Char(' '))))
          << "</td></tr>\n</table>\n";

        if (!page.moves.isEmpty()) {
            o << "<p class=\"moved\">Moved: ";
            QStringList mv;
            for (const auto& m : page.moves)
                mv << esc(m.token) + QStringLiteral(" (%1\xE2\x86\x92%2)").arg(m.fromIndex).arg(m.toIndex);
            o << mv.join(QStringLiteral(", ")) << "</p>\n";
        }
        if (page.pixelDiffCount > 0)
            o << "<p class=\"unchanged\">~" << page.pixelDiffCount << " pixels differ visually.</p>\n";
    }

    // U04: a filter that empties the report says so — never "identical"
    // (that wording stays engine-owned via isIdentical above).
    if (!hasStructural && changedPages == 0)
        o << "<p class=\"unchanged\">No changes match the filter.</p>\n";

    o << "</body></html>\n";
    return html;
}

QString CompareMode::buildTextReport() const {
    return buildTextReport(CompareChangeFilter{});
}

QString CompareMode::buildTextReport(const CompareChangeFilter& filter) const {
    // U04: gates mirror the widget's filtered sequence (see buildHtmlReport).
    auto pageVisible = [&filter](const PageDiff& page) {
        const bool hasText = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty();
        const bool hasMove = !page.moves.isEmpty();
        const bool hasPixel = page.pixelDiffCount > 0;
        return (hasText && filter.showText) || (hasMove && filter.showMove)
               || (hasPixel && filter.showPixel);
    };

    QString out;
    QTextStream o(&out);

    int totalAdded = 0, totalRemoved = 0, totalMoved = 0;
    for (const auto& page : m_lastResult.pages) {
        if (!pageVisible(page))
            continue;
        totalAdded   += page.textAdded.size();
        totalRemoved += page.textRemoved.size();
        totalMoved   += page.moves.size();
    }

    // R11: structural page-change counts for the summary line (filtered).
    int pagesAdded = 0, pagesRemoved = 0, pagesMoved = 0;
    for (const auto& ch : m_lastResult.pageChanges) {
        if (ch.type == DiffResult::PageChangeType::PageMoved) {
            if (filter.showPageMove) ++pagesMoved;
        } else if (filter.showPageAddRemove) {
            if (ch.type == DiffResult::PageChangeType::PageAdded)   ++pagesAdded;
            if (ch.type == DiffResult::PageChangeType::PageRemoved) ++pagesRemoved;
        }
    }

    o << "GlyphPDF Comparison Report\n";
    o << "==========================\n";
    o << "Original: " << m_file1 << "\n";
    o << "Revised:  " << m_file2 << "\n";
    o << "Date:     " << QDate::currentDate().toString(Qt::ISODate) << "\n";
    o << "Pages:    " << m_lastResult.pageCount1 << " -> " << m_lastResult.pageCount2 << "\n";
    o << "Changes:  " << (totalAdded + totalRemoved + totalMoved)
      << " (" << totalAdded << " added, " << totalRemoved << " removed, "
      << totalMoved << " moved)\n";
    o << "Page changes: " << pagesAdded << " added, " << pagesRemoved
      << " removed, " << pagesMoved << " moved\n\n";

    if (m_lastResult.isIdentical) {
        o << "The documents are identical.\n";
        return out;
    }

    // R11/U04: structural page changes — page and side named explicitly;
    // single canonical sequence (pageChanges), gated by the filter; the
    // legacy pageMoves list is not emitted separately.
    bool hasStructural = false;
    for (const auto& ch : m_lastResult.pageChanges) {
        if (ch.type == DiffResult::PageChangeType::PageMoved
                ? !filter.showPageMove : !filter.showPageAddRemove)
            continue;
        hasStructural = true;
    }
    if (hasStructural) {
        o << "Structural page changes:\n";
        for (const auto& ch : m_lastResult.pageChanges) {
            if (ch.type == DiffResult::PageChangeType::PageMoved
                    ? !filter.showPageMove : !filter.showPageAddRemove)
                continue;
            switch (ch.type) {
            case DiffResult::PageChangeType::PageAdded:
                o << "  Page " << (ch.newPage + 1) << " added in revised document";
                break;
            case DiffResult::PageChangeType::PageRemoved:
                o << "  Page " << (ch.oldPage + 1) << " removed from original document";
                break;
            case DiffResult::PageChangeType::PageMoved:
                o << "  Page " << (ch.oldPage + 1) << " moved to position "
                  << (ch.newPage + 1);
                break;
            }
            if (!ch.excerpt.isEmpty())
                o << " (" << ch.excerpt << ")";
            o << "\n";
        }
        o << "\n";
    }

    for (const auto& page : m_lastResult.pages) {
        if (!pageVisible(page)) continue;

        o << "--- Page " << (page.pageIndex + 1) << " ---\n";
        for (const QString& w : page.textRemoved)
            o << "- " << w << "\n";
        for (const QString& w : page.textAdded)
            o << "+ " << w << "\n";
        for (const auto& m : page.moves)
            o << "~ " << m.token << " (" << m.fromIndex << " -> " << m.toIndex << ")\n";
        if (page.pixelDiffCount > 0)
            o << "  (~" << page.pixelDiffCount << " pixels differ visually)\n";
        o << "\n";
    }

    // U04: a filter that empties the report says so — never "identical".
    if (!hasStructural && (totalAdded + totalRemoved + totalMoved) == 0
        && pagesAdded + pagesRemoved + pagesMoved == 0)
        o << "No changes match the filter.\n";
    return out;
}

bool CompareMode::pathsAreComparable(const QString& a, const QString& b, QString* why) {
    if (!QFileInfo::exists(a)) {
        if (why) *why = tr("File not found: %1").arg(a);
        return false;
    }
    if (!QFileInfo::exists(b)) {
        if (why) *why = tr("File not found: %1").arg(b);
        return false;
    }
    if (QFileInfo(a).canonicalFilePath() == QFileInfo(b).canonicalFilePath()) {
        if (why) *why = tr("Please choose two different files to compare.");
        return false;
    }
    return true;
}

bool CompareMode::startComparison(const QString& a, const QString& b) {
    QString why;
    if (!pathsAreComparable(a, b, &why)) {
        QMessageBox::warning(this, tr("Compare Documents"), why);
        return false;
    }
    compareFiles(a, b);
    return true;
}

void CompareMode::promptAndCompare(const QString& suggested) {
    const QString f1 = QFileDialog::getOpenFileName(this, tr("Select Original Document"), suggested,
                                                    tr("PDF files (*.pdf);;All files (*)"));
    if (f1.isEmpty()) return;
    const QString f2 = QFileDialog::getOpenFileName(this, tr("Select Revised Document"), QString(),
                                                    tr("PDF files (*.pdf);;All files (*)"));
    if (f2.isEmpty()) return;
    startComparison(f1, f2);
}

} // namespace gp
