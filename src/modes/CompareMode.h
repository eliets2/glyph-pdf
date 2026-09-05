// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>

#include "engines/DiffEngine.h"
#include <QFutureWatcher>

class CompareWidget;
class QTreeWidget;
class QLabel;
class QToolButton;

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
    QString buildHtmlReport() const;
    QString buildTextReport() const;

    // §9.10/R11: data roles tagging each CHANGES row with the filter gate it
    // obeys, plus (for structural rows) its index in the one shared change
    // sequence. Shared with tests so the seam stays honest.
    static constexpr int kHasTextRole         = static_cast<int>(Qt::UserRole) + 1;
    static constexpr int kHasMoveRole         = static_cast<int>(Qt::UserRole) + 2;
    static constexpr int kHasPixelRole        = static_cast<int>(Qt::UserRole) + 3;
    static constexpr int kIsPageMoveRole      = static_cast<int>(Qt::UserRole) + 4;
    static constexpr int kIsPageAddRemoveRole = static_cast<int>(Qt::UserRole) + 5;
    static constexpr int kAnchorIndexRole     = static_cast<int>(Qt::UserRole) + 6;

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
    QFutureWatcher<DiffResult> m_watcher;
    DiffResult m_lastResult;
    QString m_file1;
    QString m_file2;
};

} // namespace gp
