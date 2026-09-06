// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QList>
#include <QFutureWatcher>
#include <QImage>
#include <memory>

struct AppContext;
class QListWidget;
class QSpinBox;
class QLineEdit;
class QLabel;
class QListWidgetItem;
class QRadioButton;
class QComboBox;
class IPdfRenderer;
class QMenu;
class QUndoCommand;

namespace gp {

class PagesMode : public QWidget {
    Q_OBJECT
public:
    explicit PagesMode(QWidget* parent = nullptr);
    // Out-of-line destructor: IPdfRenderer (held by unique_ptr) is only a
    // complete type in PagesMode.cpp (which includes BackendRouter.h).
    ~PagesMode();

    // Called by ModeController after construction (same pattern as BatchMode).
    void setAppContext(const AppContext* ctx);

    // Refresh page list and split-form state when a document is opened/closed.
    void refreshPageList();

    // --- Test seams (headless, no display required) ---
    // Parse "1-3,5,7-9" into 0-based indices [0,1,2,4,6,7,8].
    static QList<int> parsePageRange(const QString& expr, int pageCount);

    // §9.9 P1: parse "1-3,4-6,7" into ONE 0-based index group per
    // comma-separated segment — one output file per group, single-page
    // segments included. Segments with no valid pages are skipped; segment
    // order is preserved; overlapping segments are allowed; a single-segment
    // expression reproduces the old single-output split ("1-5" → one group).
    static QList<QList<int>> parsePageRangeSegments(const QString& expr, int pageCount);

    // §9.8+§9.9 P0: the local-first differentiator, shared by the Pages and
    // Redaction panels and the About dialog — one source of truth for the
    // claim (factual: all of these features run in-process on this machine).
    static QString localFirstClaim();

    // Execute split without UI: returns paths of produced files.
    QStringList executeSplit(const QString& sourcePath,
                             const QList<QList<int>>& groups,
                             const QString& outputDir,
                             const QString& stemPattern);

    // Write a minimal valid one-page PDF stub (used by executeSplit + tests).
    static bool writeMinimalPdf(const QString& path);

    // §9.9 P0 test seam: convert a drag result into an engine permutation.
    // Returns the permutation over positions in `snapshot` that yields
    // `newOrder`; empty when the two are equal or sizes differ.
    static QList<int> gridMovePermutation(const QList<int>& snapshot,
                                          const QList<int>& newOrder);

private slots:
    void onPreviewSplit();
    void onSplit();
    void onApplyReorder();
    void onResetReorder();
    void onThumbnailSizeChanged();
    // AR-7 D2: called on the GUI thread when the off-thread page-count query finishes.
    void onPageCountReady();

    // U06: keyboard/context moves of the selected page(s) by delta (-1 up,
    // +1 down) through the SAME atomic command path as drag.
    void moveSelectedPagesBy(int delta);
    // U06: fill the thumbnail context menu (Move Up/Down, Select All,
    // Clear Selection — no destructive entries) from the grid's own commands.
    void fillGridContextMenu(QMenu* menu);
    // U06: keep the selection label and the selection snapshot in step.
    void onGridSelectionChanged();
    // U06: undo/redo anywhere can change the page order this grid displays —
    // reload coalesced and restore selection + current page across it.
    void onUndoStackIndexChanged(int index);

private:
    // Build sub-widgets
    void buildPageListPanel(QWidget* host);
    void buildSplitPanel(QWidget* host);
    void buildReorderPanel(QWidget* host);

    // Compute split groups from current form state.
    QList<QList<int>> computeSplitGroups() const;
    // Build output filename for part n (1-based) from pattern and stem.
    QString makeOutputName(const QString& pattern, const QString& stem, int part) const;
    // Page list (D1)
    QListWidget*  m_pageList    = nullptr;
    QLabel*       m_pageCountLabel = nullptr;

    // Split form (D2)
    QRadioButton* m_splitAtRadio    = nullptr;
    QRadioButton* m_splitEveryRadio = nullptr;
    QRadioButton* m_splitRangeRadio = nullptr;
    QSpinBox*     m_splitAtSpin     = nullptr;   // split at page N
    QSpinBox*     m_splitEverySpin  = nullptr;   // split every N pages
    QLineEdit*    m_splitRangeEdit  = nullptr;   // "1-5,7,9-12" expression
    QLineEdit*    m_namingEdit      = nullptr;
    QLineEdit*    m_outDirEdit      = nullptr;
    QListWidget*  m_previewList     = nullptr;   // filename preview

    // Reorder panel (D3)
    QListWidget*  m_reorderList     = nullptr;
    QList<int>    m_originalOrder;  // 0-based original page indices

    // §9.9 P0: grid drag-and-drop reorder state.
    QList<int> m_dragSnapshot;      // visual order (original indices) at drag start
    void finishGridReorder();
    void rebuildFromOrder(const QList<int>& order);

    // ── U06: selection visibility, readable labels, keyboard moves, ───────
    // insertion indicator, and selection/current-page restore after undo.
    QLabel*    m_selectionLabel   = nullptr;  // "N pages selected · pages X-Y"
    QList<int> m_selectedPages;             // selected items' UserRole data values
    int        m_currentPageData    = -1;     // current item's UserRole data value
    QList<int> m_pendingSelection;            // rows to reselect after next repopulation
    int        m_pendingCurrentPage = -1;     // row to make current after next repopulation
    bool       m_gridRebuildGuard   = false;  // suppress drag-signal machinery during programmatic rebuilds
    bool       m_ownPush            = false;  // suppress undo-index reaction during our own command push
    bool       m_undoRefreshScheduled = false;
    QUndoCommand* m_lastGridCmd     = nullptr; // our last pushed grid command (identity only, never dereferenced)
    QList<int> m_lastGridSnapshot;            // pre-push visual order
    QList<int> m_lastGridNewOrder;            // post-push visual order

    // Shared tail for drag (finishGridReorder) and keyboard/context moves:
    // gridMovePermutation → ReorderPermutationCommand → reload. The selected
    // rows are reselected (pendingSelection/pendingCurrent) after the reload.
    void commitGridOrder(const QList<int>& snapshot, const QList<int>& newOrder,
                         const QList<int>& pendingSelection, int pendingCurrent);
    // One thumbnail-grid item: placeholder icon, "Page N" label, identity in
    // Qt::UserRole, and a theme-token foreground so the label stays readable.
    static QListWidgetItem* makePageItem(int pageData);
    void updateSelectionLabel();
    void restorePendingSelection();   // position-based (after a fresh load generation)
    void restoreSelectionByData();    // identity-based (after a visual revert)
    void scheduleUndoRefresh();

    const AppContext* m_ctx = nullptr;

    // AR-7 D2: worker for the page-count binary-search (avoids blocking the GUI thread).
    QFutureWatcher<int>* m_pageCountWatcher{nullptr};

    // AR-8 D5: real thumbnail rendering.
    // A single PdfiumBackend (owned here) is reused across all per-page render
    // futures so PDFium is initialised once per document load.
    std::unique_ptr<IPdfRenderer> m_thumbRenderer;

    // One watcher per page; each fires renderOneDone(int pageIndex, QImage) on
    // the GUI thread when its background render completes.
    QList<QFutureWatcher<QImage>*> m_thumbWatchers;

    // Cancel in-flight thumbnail renders and clear m_thumbWatchers.
    void cancelThumbnailRenders();

private slots:
    void onThumbnailReady(int pageIndex, const QImage& img);
};

} // namespace gp
