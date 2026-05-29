#pragma once
#include "core/ErrorInfo.h"
#include "core/AppContext.h"

#include <QWidget>
#include <QFutureWatcher>
#include <QProgressBar>
#include <QTextEdit>
#include <QStringList>
#include <QElapsedTimer>
#include <QMutex>
#include <QStandardItemModel>

class QLabel;
class QListView;
class QPushButton;
class QComboBox;
class QStackedWidget;
class QLineEdit;
class QSlider;
class QSpinBox;
class QToolButton;

namespace gp {

// Per-file result from batch worker
struct BatchFileResult {
    QString inputPath;
    QString outputPath;
    bool    success = false;
    QString errorMessage;
    QString techDetail;
};

class BatchMode : public QWidget {
    Q_OBJECT
public:
    explicit BatchMode(QWidget* parent = nullptr);

    void setAppContext(const AppContext* ctx);

    // Test seams — public so tests can drive BatchMode headlessly without subclassing.
    void addFilesForTest(const QStringList& paths) { addFilePaths(paths); }
    int  fileCount()     const { return m_filesToProcess.size(); }
    bool isBatchRunning() const { return m_watcher.isRunning(); }
    int  successCount()  const { return m_successCount; }
    int  failCount()     const { return m_failCount; }
    int  errorLogCount() const { return m_errorLog.count(); }

    // Programmatic run/cancel triggers (bypass UI state guards for tests)
    void onRunBatch()    { onRunClicked(); }
    void onCancelBatch() { onCancelClicked(); }

signals:
    // Emitted from onBatchFinished so tests can spy on completion.
    void batchFinished();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onAddFiles();
    void onAddFolder();
    void onClearFiles();
    void onRemoveSelected();
    void onRunClicked();
    void onCancelClicked();
    void onBatchProgress(int value);
    void onBatchFinished();
    void onExportLog();
    void onOperationChanged(int index);

private:
    void buildFilePanel(QWidget* host);
    void buildOperationPanel(QWidget* host);
    void buildProgressPanel(QWidget* host);
    void addFilePaths(const QStringList& paths);
    void syncFileList();
    QString resolveOutputPath(const QString& inputPath) const;
    bool confirmOverwrite(const QString& path);

    void appendLog(const QString& text, const QString& color = {});
    void appendFileResult(const QString& file, bool success, const QString& detail = {});
    void showSummary();

    // File list
    QListView*          m_fileView       = nullptr;
    QStandardItemModel* m_fileModel      = nullptr;
    QLabel*             m_fileCountLabel = nullptr;

    // Operation selector
    QComboBox*          m_opCombo        = nullptr;
    QStackedWidget*     m_cfgStack       = nullptr;

    // Convert panel
    QComboBox*          m_fmtCombo       = nullptr;
    QLineEdit*          m_convertOutDir  = nullptr;

    // Compress panel
    QSlider*            m_qualitySlider  = nullptr;
    QLabel*             m_qualityLabel   = nullptr;
    QLineEdit*          m_compressOutDir = nullptr;

    // Watermark panel
    QLineEdit*          m_wmTextEdit     = nullptr;
    QSpinBox*           m_wmOpacity      = nullptr;
    QLineEdit*          m_wmOutDir       = nullptr;

    // Export PDF/A panel
    QComboBox*          m_pdfaLevel      = nullptr;
    QLineEdit*          m_pdfaOutDir     = nullptr;

    // Merge panel (disabled)
    // OCR panel (disabled)
    // Redact panel (disabled)

    // Progress
    QProgressBar*       m_overallProgress = nullptr;
    QProgressBar*       m_fileProgress    = nullptr;
    QLabel*             m_statusLabel     = nullptr;
    QLabel*             m_etaLabel        = nullptr;
    QToolButton*        m_runBtn          = nullptr;
    QPushButton*        m_cancelBtn       = nullptr;
    QPushButton*        m_exportLogBtn    = nullptr;

    // Log
    QTextEdit*          m_logView         = nullptr;

    // State
    QStringList         m_filesToProcess;
    QFutureWatcher<BatchFileResult> m_watcher;
    ErrorLog            m_errorLog;
    int                 m_successCount    = 0;
    int                 m_failCount       = 0;
    QElapsedTimer       m_batchTimer;
    QMutex              m_engineMutex;       // serializes pdfEditor calls across threads

    const AppContext*   m_ctx             = nullptr;

    // Operation index constants (match m_opCombo order)
    enum OpIndex {
        OpConvert   = 0,
        OpCompress  = 1,
        OpWatermark = 2,
        OpExportPdfA = 3,
        OpMerge     = 4,   // disabled
        OpOCR       = 5,   // disabled
        OpRedact    = 6,   // disabled
    };
};

} // namespace gp
