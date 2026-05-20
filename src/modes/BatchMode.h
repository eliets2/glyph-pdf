#pragma once
#include <QWidget>

#include <QFutureWatcher>
#include <QProgressBar>
#include <QTextEdit>
#include <QStringList>

namespace gp {

class BatchMode : public QWidget {
    Q_OBJECT
public:
    explicit BatchMode(QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onBatchProgress(int value);
    void onBatchFinished();

private:
    QProgressBar* m_overallProgress;
    QProgressBar* m_fileProgress;
    QTextEdit* m_logView;
    QFutureWatcher<void> m_watcher;
    QStringList m_filesToProcess;
};

} // namespace gp
