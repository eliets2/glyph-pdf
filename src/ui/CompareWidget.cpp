// SPDX-License-Identifier: Apache-2.0
#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QTextBrowser>
#include <QEvent>
#include <QLabel>
#include <QPdfView>
#include <QTimer>

// ---------------------------------------------------------------------------
// Colour constants for the diff display
// ---------------------------------------------------------------------------
static constexpr const char* CLR_DEL  = "#cc3333";   // red   — deleted token
static constexpr const char* CLR_ADD  = "#2d9e2d";   // green — added token
static constexpr const char* CLR_MOV  = "#d97c00";   // orange — moved token
static constexpr const char* CLR_KEEP = "#888888";   // grey  — unchanged

CompareWidget::CompareWidget(QWidget *parent)
    : QWidget(parent)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Top: side-by-side PDF viewers ──────────────────────────────────
    QSplitter* pdfSplit = new QSplitter(Qt::Horizontal, this);
    m_viewerLeft  = new PdfViewerWidget(this);
    m_viewerRight = new PdfViewerWidget(this);
    pdfSplit->addWidget(m_viewerLeft);
    pdfSplit->addWidget(m_viewerRight);
    col->addWidget(pdfSplit, 3);  // 75% of space

    // U04: missing-side placeholders, parented on each viewer (float above
    // its content, mouse-transparent). A side without the selected change's
    // page must explain itself instead of showing a stale page.
    auto makePlaceholder = [this](PdfViewerWidget* viewer, const char* objectName) {
        auto* ph = new QLabel(viewer);
        ph->setObjectName(QString::fromLatin1(objectName));
        ph->setAttribute(Qt::WA_TransparentForMouseEvents);
        ph->setAlignment(Qt::AlignCenter);
        ph->setWordWrap(true);
        ph->setStyleSheet(
            "QLabel { background: rgba(30,31,34,220); color:#a8abb0;"
            " border:1px solid #393b40; border-radius:6px;"
            " padding:10px; font-size:12px; }");
        ph->hide();
        return ph;
    };
    m_leftPlaceholder  = makePlaceholder(m_viewerLeft,  "cmpLeftPlaceholder");
    m_rightPlaceholder = makePlaceholder(m_viewerRight, "cmpRightPlaceholder");
    m_viewerLeft->installEventFilter(this);
    m_viewerRight->installEventFilter(this);

    // ── Bottom: text diff panel ────────────────────────────────────────
    // Navigation bar
    auto* navBar = new QWidget(this);
    navBar->setFixedHeight(24);
    navBar->setStyleSheet("background:#1e1f22; border-top:1px solid #393b40;");
    auto* navRow = new QHBoxLayout(navBar);
    navRow->setContentsMargins(8, 0, 8, 0);
    navRow->setSpacing(6);
    auto* navTitle = new QLabel("TEXT DIFF", navBar);
    navTitle->setStyleSheet("color:#71747a; font-size:10px; font-family:monospace; font-weight:bold;");
    navRow->addWidget(navTitle);
    navRow->addStretch(1);
    m_navLabel = new QLabel("", navBar);
    m_navLabel->setObjectName(QStringLiteral("cmpNavLabel"));  // R11: testable navigation state
    m_navLabel->setStyleSheet("color:#71747a; font-size:10px; font-family:monospace;");
    navRow->addWidget(m_navLabel);
    col->addWidget(navBar);

    m_textDiff = new QTextBrowser(this);
    m_textDiff->setMinimumHeight(80);
    m_textDiff->setOpenLinks(false);
    m_textDiff->setStyleSheet(
        "QTextBrowser { background:#1a1b1e; color:#dfe1e5; "
        "font-family:monospace; font-size:11px; border:none; padding:4px; }");
    col->addWidget(m_textDiff, 1);  // 25% of space

    // U04: linked scrolling. Resolve each viewer's MAIN scrollbar by type +
    // objectName (PdfViewerWidget also contains a second QAbstractScrollArea
    // for two-page mode — a blind findChild could bind the wrong one) and
    // connect both directions through the guarded mapLinkedScroll mapping.
    // Linked by default; the connections are permanent, mapLinkedScroll
    // checks the flag.
    for (PdfViewerWidget* viewer : { m_viewerLeft, m_viewerRight }) {
        if (QScrollBar* bar = verticalScrollBarFor(viewer)) {
            connect(bar, &QScrollBar::valueChanged, this,
                    [this, viewer](int value) {
                if (!m_linkedScroll || m_suppressSync || m_syncingScroll)
                    return;
                if (QScrollBar* b = verticalScrollBarFor(viewer))
                    mapLinkedScroll(viewer, value, b->maximum());
            });
        }
    }
}

bool CompareWidget::loadDocuments(const QString& file1, const QString& file2)
{
    return m_viewerLeft->loadDocument(file1) && m_viewerRight->loadDocument(file2);
}

void CompareWidget::setDiffResult(const DiffResult& result)
{
    m_diffResult = result;
    m_currentAnchor = -1;   // a fresh result resets the selection
    rebuildDiff();
}

void CompareWidget::setShowPixelDiff(bool show)
{
    m_showPixelDiff = show;
    if (!show) {
        // One owner: the toggle clears whatever a change selection set.
        m_currentOverlay = QImage();
        m_viewerRight->setOverlayImage(QImage());
        return;
    }
    // The overlay follows the selected change; before any selection it falls
    // back to the FIRST change of the shared sequence (U04: this replaces the
    // old pages.first() shortcut).
    const int index = m_currentAnchor >= 0
                          ? m_currentAnchor
                          : (m_anchors.isEmpty() ? -1 : 0);
    showOverlayForChange(index);
}

// ---------------------------------------------------------------------------
// U04: the one filtered change sequence
// ---------------------------------------------------------------------------

void CompareWidget::setChangeFilter(const CompareChangeFilter& filter)
{
    if (filter == m_filter)
        return;   // no change — keeps toggle churn cheap
    m_filter = filter;
    rebuildDiff();   // rebuildDiff clamps m_currentAnchor into the new list
}

CompareWidget::ChangeAnchor CompareWidget::anchorAt(int index) const
{
    if (index < 0 || index >= m_anchors.size())
        return {};
    return m_anchors.at(index);
}

int CompareWidget::anchorIndexForStructuralChange(int pageChangeIndex) const
{
    for (int i = 0; i < m_anchors.size(); ++i)
        if (m_anchors.at(i).structuralIndex == pageChangeIndex)
            return i;
    return -1;
}

int CompareWidget::anchorIndexForPage(int pageDiffIndex) const
{
    for (int i = 0; i < m_anchors.size(); ++i)
        if (m_anchors.at(i).pageDiffIndex == pageDiffIndex)
            return i;
    return -1;
}

int CompareWidget::leftPageCount() const
{
    return m_viewerLeft ? m_viewerLeft->pageCount() : 0;
}

int CompareWidget::rightPageCount() const
{
    return m_viewerRight ? m_viewerRight->pageCount() : 0;
}

void CompareWidget::rebuildDiff()
{
    // buildHtml() rebuilds m_anchors from the same filtered walk it renders —
    // the anchors and the visible text can never disagree.
    m_textDiff->setHtml(buildHtml());

    // Clamp the selection into the (possibly shrunken) sequence — a stale
    // index would send next/prev's modulo out of bounds.
    if (m_currentAnchor >= m_anchors.size())
        m_currentAnchor = m_anchors.isEmpty() ? -1 : m_anchors.size() - 1;

    if (m_currentAnchor >= 0) {
        applyAnchor(m_currentAnchor);   // nav label, viewers, placeholders, signal
    } else {
        hidePlaceholders();
        if (m_diffResult.isIdentical)
            m_navLabel->setText(tr("files are identical"));
        else if (!m_anchors.isEmpty())
            m_navLabel->setText(
                QString("%1 change(s)  |  ← → to navigate").arg(m_anchors.size()));
        else
            m_navLabel->setText(tr("no changes match the filter"));
        emit currentChangeChanged(-1);
    }

    // One overlay owner: re-derive from the current (or first) change.
    if (m_showPixelDiff)
        showOverlayForChange(m_currentAnchor >= 0 ? m_currentAnchor
                                                  : (m_anchors.isEmpty() ? -1 : 0));
}

void CompareWidget::showOverlayForChange(int anchorIndex)
{
    m_currentOverlay = QImage();
    if (m_showPixelDiff && anchorIndex >= 0 && anchorIndex < m_anchors.size()) {
        const ChangeAnchor& anchor = m_anchors.at(anchorIndex);
        int pageIdx = anchor.pageDiffIndex;
        if (pageIdx < 0 && anchor.structuralIndex >= 0) {
            // Structural change: the overlay lives on the page the change
            // positions — prefer the revised side, fall back to the original.
            const int target = anchor.newPage >= 0 ? anchor.newPage : anchor.oldPage;
            if (target >= 0) {
                for (int j = 0; j < m_diffResult.pages.size(); ++j) {
                    if (m_diffResult.pages.at(j).pageIndex == target) {
                        pageIdx = j;
                        break;
                    }
                }
            }
        }
        if (pageIdx >= 0 && pageIdx < m_diffResult.pages.size())
            m_currentOverlay = m_diffResult.pages.at(pageIdx).diffImage;
    }
    m_viewerRight->setOverlayImage(m_currentOverlay);
}

// ---------------------------------------------------------------------------
// Text diff display — anchors and HTML from ONE filtered walk
// ---------------------------------------------------------------------------

QString CompareWidget::buildHtml()
{
    m_anchors.clear();

    if (m_diffResult.isIdentical)
        return QStringLiteral("<span style='color:#4ec96d'>Files are identical.</span>");

    QString html;
    html.reserve(4096);
    html += "<style>body{font-family:monospace;font-size:11px;color:#dfe1e5;background:#1a1b1e;}</style>";

    // R11/U04: structural page changes lead the shared change sequence (the
    // canonical pageChanges order), gated by the page-level filter toggles.
    for (int i = 0; i < m_diffResult.pageChanges.size(); ++i) {
        const DiffResult::PageChange& ch = m_diffResult.pageChanges.at(i);
        const bool visible = (ch.type == DiffResult::PageChangeType::PageMoved)
                                 ? m_filter.showPageMove
                                 : m_filter.showPageAddRemove;
        if (!visible)
            continue;
        const QString aid = QString("chg%1").arg(m_anchors.size());
        m_anchors.append({aid, ch.oldPage, ch.newPage, i, -1});
        QString line;
        switch (ch.type) {
        case DiffResult::PageChangeType::PageAdded:
            line = QString("<p><a name='%1'/><span style='color:%2'>+</span> "
                           "<span style='color:%2'>Page %3 added in revised document</span>%4</p>")
                       .arg(aid, CLR_ADD)
                       .arg(ch.newPage + 1)
                       .arg(ch.excerpt.isEmpty()
                                ? QString()
                                : QStringLiteral(" <span style='color:%1'>%2</span>")
                                      .arg(CLR_KEEP, ch.excerpt.toHtmlEscaped()));
            break;
        case DiffResult::PageChangeType::PageRemoved:
            line = QString("<p><a name='%1'/><span style='color:%2'>&minus;</span> "
                           "<span style='color:%2;text-decoration:line-through'>Page %3 removed from original document</span>%4</p>")
                       .arg(aid, CLR_DEL)
                       .arg(ch.oldPage + 1)
                       .arg(ch.excerpt.isEmpty()
                                ? QString()
                                : QStringLiteral(" <span style='color:%1'>%2</span>")
                                      .arg(CLR_KEEP, ch.excerpt.toHtmlEscaped()));
            break;
        case DiffResult::PageChangeType::PageMoved:
            line = QString("<p><a name='%1'/><span style='color:%2'>&#x21c4;</span> "
                           "<span style='color:%2'>Page %3 moved to position %4</span>%5</p>")
                       .arg(aid, CLR_MOV)
                       .arg(ch.oldPage + 1)
                       .arg(ch.newPage + 1)
                       .arg(ch.excerpt.isEmpty()
                                ? QString()
                                : QStringLiteral(" <span style='color:%1'>%2</span>")
                                      .arg(CLR_KEEP, ch.excerpt.toHtmlEscaped()));
            break;
        }
        html += line;
    }

    // U04: one navigable entry per page row, gated exactly like
    // CompareMode::rowsVisibleForFilters() — so the text panel, the CHANGES
    // tree and the counter can never disagree about what a "change" is.
    // The page's single anchor sits on its header; the page's token lines
    // render below it as the content of that one change.
    for (int j = 0; j < m_diffResult.pages.size(); ++j) {
        const PageDiff& page = m_diffResult.pages.at(j);
        const bool hasText  = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty();
        const bool hasMove  = !page.moves.isEmpty();
        const bool hasPixel = page.pixelDiffCount > 0;
        if (!hasText && !hasMove && !hasPixel)
            continue;
        const bool visible = (hasText && m_filter.showText)
                          || (hasMove && m_filter.showMove)
                          || (hasPixel && m_filter.showPixel);
        if (!visible)
            continue;

        const QString aid = QString("chg%1").arg(m_anchors.size());
        m_anchors.append({aid, page.pageIndex, page.pageIndex, -1, j});

        html += QString("<p><a name='%1'/><b style='color:#a8abb0'>Page %2</b></p>")
                    .arg(aid)
                    .arg(page.pageIndex + 1);

        // Moves (orange)
        for (const MoveOperation& mv : page.moves) {
            html += QString("<p><span style='color:%1'>&#x2194;</span> "
                            "<span style='color:%1;text-decoration:underline'>%2</span> "
                            "<span style='color:%3'>[moved from pos&nbsp;%4 → %5]</span></p>")
                        .arg(CLR_MOV,
                             mv.token.toHtmlEscaped(),
                             CLR_KEEP)
                        .arg(mv.fromIndex)
                        .arg(mv.toIndex);
        }

        // Additions (green)
        for (const QString& tok : page.textAdded) {
            html += QString("<p><span style='color:%1'>+</span> "
                            "<span style='color:%1'>%2</span></p>")
                        .arg(CLR_ADD, tok.toHtmlEscaped());
        }

        // Deletions (red)
        for (const QString& tok : page.textRemoved) {
            html += QString("<p><span style='color:%1'>−</span> "
                            "<span style='color:%1;text-decoration:line-through'>%2</span></p>")
                        .arg(CLR_DEL, tok.toHtmlEscaped());
        }

        if (hasPixel) {
            html += QString("<p><span style='color:#888'>~ pixel diff: %1 px changed</span></p>")
                        .arg(page.pixelDiffCount);
        }
    }

    // U04: a filter that hides everything must say so — NEVER "identical"
    // (that wording stays engine-owned via isIdentical above).
    if (m_anchors.isEmpty())
        html += QStringLiteral(
            "<span style='color:#e5c07b'>No changes match the filter.</span>");

    return html;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

// R11/U04: one shared change sequence. Every anchor scrolls the text panel,
// moves each viewer to the page the change lives on, explains a missing side,
// and reports the position through currentChangeChanged so the tree, the
// counter and the views refer to the same change.
void CompareWidget::applyAnchor(int index)
{
    const ChangeAnchor anchor = m_anchors.at(index);
    m_textDiff->scrollToAnchor(anchor.id);
    m_navLabel->setText(
        QString("change %1 of %2  |  ← → to navigate")
            .arg(index + 1)
            .arg(m_anchors.size()));

    // Anchor-driven jumps are exact (R11 oldPage/newPage); suppress free-
    // scroll syncing so it cannot fight the change being navigated to.
    m_suppressSync = true;
    if (anchor.oldPage >= 0 && anchor.oldPage < m_viewerLeft->pageCount())
        m_viewerLeft->goToPage(anchor.oldPage);
    if (anchor.newPage >= 0 && anchor.newPage < m_viewerRight->pageCount())
        m_viewerRight->goToPage(anchor.newPage);
    m_suppressSync = false;
    // Deferred scrollbar events (if any) still land inside the suppression.
    QTimer::singleShot(0, this, [this] { m_suppressSync = false; });

    updatePlaceholders(anchor);
    emit currentChangeChanged(index);
}

void CompareWidget::nextChange()
{
    if (m_anchors.isEmpty()) return;
    m_currentAnchor = (m_currentAnchor + 1) % m_anchors.size();
    applyAnchor(m_currentAnchor);
}

void CompareWidget::prevChange()
{
    if (m_anchors.isEmpty()) return;
    m_currentAnchor = (m_currentAnchor - 1 + m_anchors.size()) % m_anchors.size();
    applyAnchor(m_currentAnchor);
}

void CompareWidget::scrollToChange(int index)
{
    if (index < 0 || index >= m_anchors.size()) return;
    m_currentAnchor = index;
    applyAnchor(index);
}

// ---------------------------------------------------------------------------
// Missing-side placeholders
// ---------------------------------------------------------------------------

void CompareWidget::updatePlaceholders(const ChangeAnchor& anchor)
{
    // PageChange's -1 means "no such page" (DiffEngine.h contract), so an
    // added page explains the original side and a removed page the revised
    // side; both-side anchors clear both.
    if (anchor.oldPage < 0) {
        m_leftPlaceholder->setText(
            tr("Page %1 does not exist in the original document — added in the revised document")
                .arg(anchor.newPage + 1));
        repositionPlaceholders();
        m_leftPlaceholder->show();
        m_leftPlaceholder->raise();
    } else {
        m_leftPlaceholder->hide();
    }
    if (anchor.newPage < 0) {
        m_rightPlaceholder->setText(
            tr("Page %1 does not exist in the revised document — removed from the original document")
                .arg(anchor.oldPage + 1));
        repositionPlaceholders();
        m_rightPlaceholder->show();
        m_rightPlaceholder->raise();
    } else {
        m_rightPlaceholder->hide();
    }
}

void CompareWidget::hidePlaceholders()
{
    m_leftPlaceholder->hide();
    m_rightPlaceholder->hide();
}

void CompareWidget::repositionPlaceholders()
{
    auto fit = [](QLabel* label) {
        if (!label || !label->parentWidget())
            return;
        const QRect r = label->parentWidget()->rect();
        const int h = 72;
        label->setGeometry(16, qMax(8, (r.height() - h) / 2),
                           qMax(80, r.width() - 32), h);
    };
    fit(m_leftPlaceholder);
    fit(m_rightPlaceholder);
}

bool CompareWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_viewerLeft || watched == m_viewerRight) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            repositionPlaceholders();
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// Linked scrolling (U04)
// ---------------------------------------------------------------------------

QScrollBar* CompareWidget::verticalScrollBarFor(PdfViewerWidget* viewer) const
{
    if (!viewer)
        return nullptr;
    QScrollBar*& cache = (viewer == m_viewerLeft) ? m_leftBar : m_rightBar;
    if (!cache) {
        // The main view is a QPdfView named "pdfView"; the two-page mode area
        // is a plain QScrollArea. Resolve by TYPE + objectName so the link
        // can never bind the wrong scroll area.
        if (QPdfView* view = viewer->findChild<QPdfView*>(QStringLiteral("pdfView")))
            cache = view->verticalScrollBar();
    }
    return cache;
}

void CompareWidget::setLinkedScrolling(bool linked)
{
    m_linkedScroll = linked;
}

void CompareWidget::mapLinkedScroll(PdfViewerWidget* leader, int value, int maximum)
{
    if (!m_linkedScroll || m_syncingScroll || m_suppressSync || maximum <= 0)
        return;
    PdfViewerWidget* follower = nullptr;
    if (leader == m_viewerLeft)
        follower = m_viewerRight;
    else if (leader == m_viewerRight)
        follower = m_viewerLeft;
    if (!follower || follower->pageCount() <= 0)
        return;

    const qreal ratio = qreal(value) / qreal(maximum);

    m_syncingScroll = true;   // re-entrancy guard: one hop per leader change
    QScrollBar* followerBar = verticalScrollBarFor(follower);
    if (followerBar && followerBar->maximum() > 0) {
        // Proportional position mapping: equal ratios land on the same
        // relative position (which crosses page boundaries naturally) —
        // never a raw value↔value binding across different documents.
        const int target = qRound(ratio * qreal(followerBar->maximum()));
        if (target != followerBar->value())
            followerBar->setValue(target);
    } else {
        // No follower layout yet (never shown / offscreen tests): fall back
        // to page-index mapping — scroll ratio → follower page index.
        const int pages = follower->pageCount();
        const int page = qBound(0, int(ratio * qreal(pages)), pages - 1);
        if (page != follower->currentPage())
            follower->goToPage(page);
    }
    m_syncingScroll = false;
}
