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
    // Pure seam: how many tree rows (per-page change rows + page-move rows) the
    // view would show with the given toggles. Display-layer only — never
    // mutates the diff result.
    static int rowsVisibleForFilters(const DiffResult& result, bool showText,
                                     bool showMove, bool showPixel, bool showPageMove);
    // Populate the CHANGES tree from a diff result and apply the current filter
    // toggles. Split out of onDiffFinished so tests can drive it without the
    // async watcher (no modal dialogs, no real files needed).
    void showDiffResult(const DiffResult& result);

private slots:
    void onDiffFinished();
    void onExportReport();
    void applyChangeTypeFilters();

private:
    QString buildHtmlReport() const;
    QString buildTextReport() const;

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
    QFutureWatcher<DiffResult> m_watcher;
    DiffResult m_lastResult;
    QString m_file1;
    QString m_file2;
};

} // namespace gp
