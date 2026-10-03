// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>

#include <functional>

#include "engines/DiffEngine.h"
#include <QAtomicInt>
#include <QFutureWatcher>
#include <QPointer>
#include <QSharedPointer>

class CompareWidget;
class QTreeWidget;
class QLabel;
class QProgressDialog;
class QToolButton;
struct CompareChangeFilter;   // U04: defined in ui/CompareWidget.h

namespace gp {

class CompareMode : public QWidget {
    Q_OBJECT
public:
    explicit CompareMode(QWidget* parent = nullptr);
    void compareFiles(const QString& file1, const QString& file2);

    bool isBusy() const { return m_watcher.isRunning(); }
    const DiffResult& lastResult() const { return m_lastResult; }
    bool startComparison(const QString& a, const QString& b);
    void promptAndCompare(const QString& suggested = QString());
    static bool pathsAreComparable(const QString& a, const QString& b, QString* why = nullptr);

    // ── §9.10: change-type filter for the CHANGES tree ──────────────────────────
    // Pure seam: how many tree rows (per-page change rows + structural page
    // change rows) the view would show with the given toggles. Display-layer
    // only — never mutates the diff result.
    // R11: showPageAddRemove gates pages added to / removed from the documents;
    // whole-page reorders stay behind showPageMove.
    static int rowsVisibleForFilters(const DiffResult& result, bool showText,
                                     bool showMove, bool showPixel, bool showPageMove,
                                     bool showPageAddRemove = true);
    // Populate the CHANGES tree from a diff result and apply the current filter
    // toggles. Split out of onDiffFinished so tests can drive it without the
    // async watcher (no modal dialogs, no real files needed).
    void showDiffResult(const DiffResult& result);

    // R11: report builders exposed read-only so tests can assert structural
    // changes name the correct page and side without driving the save dialog.
    // U04: filter-honoring overloads — the report covers exactly the scope
    // and filter state the UI describes (the no-arg forms keep producing the
    // full all-on report).
    QString buildHtmlReport() const;
    QString buildTextReport() const;
    QString buildHtmlReport(const CompareChangeFilter& filter) const;
    QString buildTextReport(const CompareChangeFilter& filter) const;

    // §4 row 7 (wave 2b): test seam, copied from BatchMode's
    // setPresetBoundaryHookForTest — invoked from the WORKER at every stage
    // boundary BEFORE that boundary's progress report is posted, with the
    // DiffEngine::ProgressStage and the (zero-based) unit the boundary sits
    // in front of. The hook is captured BY VALUE into the worker lambda when
    // the diff starts (the member is never read cross-thread), so tests can
    // park the diff mid-run deterministically — before any boundary's report
    // — and observe each stage's dialog state (cancel/stage pins).
    void setStageBoundaryHookForTest(std::function<void(int stage, int done)> hook) {
        m_stageBoundaryHook = std::move(hook);
    }

    // R3-perf seam: QPromise progress reports actually POSTED by the compare
    // worker (the setProgressValueAndText crossings to the GUI thread). The
    // stage-boundary hook above stays per-boundary — only the promise posts
    // are throttled — so this count is the observable half of the throttle:
    // it must fall below the hook's boundary count while the hook count (the
    // cancel-probe granularity) stays untouched. The counter lives behind a
    // shared_ptr that the worker captures BY VALUE, so a mid-run teardown of
    // the mode cannot dangle the worker's increment (same discipline as the
    // by-value hook capture).
    int promiseReportCountForTest() const {
        return m_promiseReports ? m_promiseReports->loadRelaxed() : 0;
    }

    // §9.10/R11: data roles tagging each CHANGES row with the filter gate it
    // obeys, plus (for structural rows) its index in the one shared change
    // sequence. Shared with tests so the seam stays honest.
    static constexpr int kHasTextRole         = static_cast<int>(Qt::UserRole) + 1;
    static constexpr int kHasMoveRole         = static_cast<int>(Qt::UserRole) + 2;
    static constexpr int kHasPixelRole        = static_cast<int>(Qt::UserRole) + 3;
    static constexpr int kIsPageMoveRole      = static_cast<int>(Qt::UserRole) + 4;
    static constexpr int kIsPageAddRemoveRole = static_cast<int>(Qt::UserRole) + 5;
    static constexpr int kAnchorIndexRole     = static_cast<int>(Qt::UserRole) + 6;
    // U04: every row's raw position in the canonical data (structural rows:
    // index into DiffResult::pageChanges; page rows: index into
    // DiffResult::pages) so the filtered anchor index can be recomputed
    // whenever the filter changes.
    static constexpr int kPageChangeIndexRole = static_cast<int>(Qt::UserRole) + 7;
    static constexpr int kPageDiffIndexRole   = static_cast<int>(Qt::UserRole) + 8;

private:
    // U04: build the CompareChangeFilter from the current toggle states and
    // funnel it into the CompareWidget (the one shared filtered sequence).
    CompareChangeFilter currentFilter() const;

private slots:
    void onDiffFinished();
    void onExportReport();
    void applyChangeTypeFilters();

private:
    CompareWidget* m_compareWidget;
    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QLabel* m_filesLabel = nullptr;   // AR-8 D1: shows actual compared filenames
    QToolButton* m_exportBtn = nullptr;
    QToolButton* m_prevBtn   = nullptr;  // O4: disabled until diff produces changes
    QToolButton* m_nextBtn   = nullptr;  // O4: disabled until diff produces changes
    QToolButton* m_filterText     = nullptr;  // §9.10: change-type toggles
    QToolButton* m_filterMove     = nullptr;
    QToolButton* m_filterPixel    = nullptr;
    QToolButton* m_filterPageMove = nullptr;
    QToolButton* m_filterPageAddRemove = nullptr;  // R11: pages added/removed
    QToolButton* m_linkScrollBtn = nullptr;  // U04: linked scrolling toggle
    QToolButton* m_swapBtn       = nullptr;  // U04: swap original/revised sides
    QFutureWatcher<DiffResult> m_watcher;
    QPointer<QProgressDialog> m_progress;   // §4 row 7: per-run progress dialog
    std::function<void(int, int)> m_stageBoundaryHook;   // §4 row 7: worker-side test seam
    QSharedPointer<QAtomicInt> m_promiseReports =
        QSharedPointer<QAtomicInt>::create(0);   // R3-perf seam: promise posts
    DiffResult m_lastResult;
    QString m_file1;
    QString m_file2;
};

} // namespace gp
