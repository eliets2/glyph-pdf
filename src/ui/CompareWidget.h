// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QWidget>
#include <QImage>
#include "engines/DiffEngine.h"

class PdfViewerWidget;
class QTextBrowser;
class QLabel;
class QScrollBar;

// U04: which change types the shared change sequence shows. Mirrors the five
// CHANGES-tree filter toggles in CompareMode 1:1; CompareMode's pure count
// seam rowsVisibleForFilters() consumes the same five gates, and the U04 test
// suite pins changeCount() == rowsVisibleForFilters() for every combination,
// so the widget's navigable sequence and the tree can never drift.
struct CompareChangeFilter {
    bool showText = true;
    bool showMove = true;
    bool showPixel = true;
    bool showPageMove = true;
    bool showPageAddRemove = true;
    // C++17: no defaulted comparison — spelled out.
    bool operator==(const CompareChangeFilter& o) const
    {
        return showText == o.showText && showMove == o.showMove
               && showPixel == o.showPixel && showPageMove == o.showPageMove
               && showPageAddRemove == o.showPageAddRemove;
    }
    bool operator!=(const CompareChangeFilter& o) const { return !(*this == o); }
};

class CompareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CompareWidget(QWidget *parent = nullptr);

    bool loadDocuments(const QString &file1, const QString &file2);
    void setDiffResult(const DiffResult &result);
    void setShowPixelDiff(bool show);

    // ── U04: the one filtered change sequence ─────────────────────────────
    // One canonical, FILTER-AWARE sequence of navigable changes: structural
    // page changes first (DiffResult::pageChanges order), then one entry per
    // visible page row — exactly the rows CompareMode's CHANGES tree shows
    // under the same filter. next/previous, tree selection, the pixel
    // overlay, the status counter and the exported reports all read this one
    // sequence.
    void setChangeFilter(const CompareChangeFilter& filter);
    const CompareChangeFilter& changeFilter() const { return m_filter; }
    /// Filtered change count — pinned equal to
    /// CompareMode::rowsVisibleForFilters() for every toggle combination.
    int changeCount() const { return m_anchors.size(); }

    /// One navigable change in the shared sequence.
    struct ChangeAnchor {
        QString id;                 // HTML anchor id in the text diff panel
        int oldPage = -1;           // 0-based page in doc1 (-1 = missing side)
        int newPage = -1;           // 0-based page in doc2 (-1 = missing side)
        int structuralIndex = -1;   // >=0: index into DiffResult::pageChanges
        int pageDiffIndex = -1;     // >=0: index into DiffResult::pages
    };
    /// Bounds-checked accessor for the filtered sequence (default ChangeAnchor
    /// on an out-of-range index).
    ChangeAnchor anchorAt(int index) const;
    int currentAnchorIndex() const { return m_currentAnchor; }
    /// Filtered-sequence index of structural change #pageChangeIndex, or -1
    /// when the filter hides it. CompareMode stores this in kAnchorIndexRole
    /// so every tree row navigates the same sequence.
    int anchorIndexForStructuralChange(int pageChangeIndex) const;
    /// Filtered-sequence index of the page row for pages[pageDiffIndex], or
    /// -1 when the filter hides the row.
    int anchorIndexForPage(int pageDiffIndex) const;

    // ── U04: pixel overlay follows the selected change ────────────────────
    /// Push the diff image of change #anchorIndex onto the right viewer.
    /// Honours the show-pixel-diff toggle (that toggle is the one owner of
    /// the feature; a selection while it is off sets nothing).
    void showOverlayForChange(int anchorIndex);
    /// What the overlay path last pushed to the right viewer. Test seam —
    /// PdfViewerWidget exposes no overlay getter.
    const QImage& currentOverlayImage() const { return m_currentOverlay; }

    // ── U04: linked scrolling ─────────────────────────────────────────────
    /// Link the two viewers' vertical scrolling: the follower's position is
    /// mapped by scroll RATIO (proportional position / page index), never by
    /// raw scrollbar value — documents with different page counts and sizes
    /// stay aligned. On by default (Acrobat-style linked compare).
    void setLinkedScrolling(bool linked);
    bool isLinkedScrolling() const { return m_linkedScroll; }
    /// Apply the linked-scroll mapping as if `leader`'s vertical scrollbar had
    /// moved to value/maximum. This is the exact function the scrollbar
    /// connections call; tests drive it directly so the mapping and the
    /// re-entrancy guard are pinned without depending on widget layout.
    void mapLinkedScroll(PdfViewerWidget* leader, int value, int maximum);

    // Page counts of the two loaded documents (0 when nothing is loaded).
    int leftPageCount() const;
    int rightPageCount() const;

public slots:
    /// Navigate to the next change (add / delete / move / structural page
    /// change).  Wraps around.
    void nextChange();
    /// Navigate to the previous change (add / delete / move / structural page
    /// change).  Wraps around.
    void prevChange();

public:
    /// R11: jump directly to change #index (0-based) in the one shared change
    /// sequence — used by CHANGES-tree selection. Same state as next/previous.
    void scrollToChange(int index);

signals:
    /// U04: the selected change in the shared sequence moved to `index` (-1 =
    /// none, e.g. a fresh result or a filter that emptied the sequence).
    /// Drives CompareMode's CHANGE X OF Y counter so the tree, the counter,
    /// next/previous and both document views always refer to the same change.
    void currentChangeChanged(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Rebuild the text panel and m_anchors from m_diffResult under the
    /// current filter, clamp the selection, refresh the overlay. The single
    /// funnel for setDiffResult/setChangeFilter so anchors and the visible
    /// text can never disagree.
    void rebuildDiff();
    /// Gated walk that rebuilds m_anchors AND renders the text panel HTML
    /// from the same pass — one source of truth.
    QString buildHtml();
    void applyAnchor(int index);
    void updatePlaceholders(const ChangeAnchor& anchor);
    void hidePlaceholders();
    void repositionPlaceholders();
    /// The viewer's MAIN vertical scrollbar, resolved once by type +
    /// objectName and cached. Never findChild<QAbstractScrollArea*>():
    /// PdfViewerWidget also contains a second scroll area (two-page mode)
    /// that findChild could bind by mistake.
    QScrollBar* verticalScrollBarFor(PdfViewerWidget* viewer) const;

    PdfViewerWidget* m_viewerLeft  = nullptr;
    PdfViewerWidget* m_viewerRight = nullptr;
    QTextBrowser*    m_textDiff    = nullptr;
    QLabel*          m_navLabel    = nullptr;

    DiffResult       m_diffResult;
    CompareChangeFilter m_filter;
    bool             m_showPixelDiff = false;
    QImage           m_currentOverlay;

    // Navigation: the filtered shared change sequence (structural page
    // changes first, then one entry per visible page row).
    QList<ChangeAnchor> m_anchors;
    int              m_currentAnchor = -1;

    // Linked scrolling (U04). m_syncingScroll cuts the valueChanged loop the
    // moment one side drives the other; m_suppressSync keeps anchor-driven
    // page jumps (applyAnchor) exact — free-scroll syncing must never fight
    // a change the user explicitly navigated to.
    bool             m_linkedScroll  = true;
    bool             m_syncingScroll = false;
    bool             m_suppressSync  = false;

    // Missing-side placeholders (added/removed pages), parented on each
    // viewer so a side without the selected change's page explains itself
    // instead of showing a stale page.
    QLabel* m_leftPlaceholder  = nullptr;
    QLabel* m_rightPlaceholder = nullptr;

    // Resolved once by verticalScrollBarFor() (see its comment).
    mutable QScrollBar* m_leftBar  = nullptr;
    mutable QScrollBar* m_rightBar = nullptr;
};
