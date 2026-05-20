#pragma once
#include <QWidget>

#include "engines/DiffEngine.h"
#include <QFutureWatcher>

class CompareWidget;
class QTreeWidget;
class QLabel;

namespace gp {

class CompareMode : public QWidget {
    Q_OBJECT
public:
    explicit CompareMode(QWidget* parent = nullptr);
    void compareFiles(const QString& file1, const QString& file2);

private slots:
    void onDiffFinished();

private:
    CompareWidget* m_compareWidget;
    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QFutureWatcher<DiffResult> m_watcher;
    DiffResult m_lastResult;
};

} // namespace gp
