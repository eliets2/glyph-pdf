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

private slots:
    void onDiffFinished();
    void onExportReport();

private:
    QString buildHtmlReport() const;
    QString buildTextReport() const;

    CompareWidget* m_compareWidget;
    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QLabel* m_filesLabel = nullptr;   // AR-8 D1: shows actual compared filenames
    QToolButton* m_exportBtn = nullptr;
    QFutureWatcher<DiffResult> m_watcher;
    DiffResult m_lastResult;
    QString m_file1;
    QString m_file2;
};

} // namespace gp
