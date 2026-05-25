#pragma once
#include "core/ErrorInfo.h"

#include <QWidget>
#include <QFutureWatcher>
#include <QProgressBar>
#include <QTextEdit>
#include <QStringList>

class QLabel;
class QListWidget;
class QPushButton;
class QListWidgetItem;

namespace gp {

class BatchMode : public QWidget {
    Q_OBJECT
public:
    explicit BatchMode(QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onBatchProgress(int value);
    void onBatchFinished();
    void onExportLog();

private:
    void appendLog(const QString& text, const QString& color = {});
    void appendFileResult(const QString& file, bool success, const QString& detail = {});
    void showSummary();

    QProgressBar*   m_overallProgress = nullptr;
    QProgressBar*   m_fileProgress    = nullptr;
    QTextEdit*      m_logView         = nullptr;
    QLabel*         m_statusLabel     = nullptr;
    QPushButton*    m_exportLogBtn    = nullptr;
    QFutureWatcher<void> m_watcher;
    QStringList     m_filesToProcess;
    ErrorLog        m_errorLog;
    int             m_successCount = 0;
    int             m_failCount    = 0;
};

} // namespace gp
