// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/ErrorInfo.h"
#include "core/AppContext.h"
#include "core/OcrTypes.h" // ocrLanguages()/ocrEngineLanguageCode (§9.12 batch OCR language)
#include "engines/ocr/OcrPipeline.h" // PageOcrResult (§9.12 low-confidence seam)
                                     // Safe here: BatchMode.h already requires
                                     // Qt6::Concurrent (QFutureWatcher member).

#include <QWidget>
#include <QFutureWatcher>
#include <QProgressBar>
#include <QTextEdit>
#include <QStringList>
#include <QElapsedTimer>
#include <QMutex>
#include <QStandardItemModel>
#include <QFileSystemWatcher>
#include <QSet>

class QLabel;
class QListView;
class QPushButton;
class QComboBox;
class QStackedWidget;
class QLineEdit;
class QSlider;
class QSpinBox;
class QToolButton;
class QCheckBox;
class QTimer;
class QFileInfo;
class QVBoxLayout;

namespace gp {

// Per-file result from batch worker
struct BatchFileResult {
    QString inputPath;
    QString outputPath;
    bool    success = false;
    QString errorMessage;
    QString techDetail;
    // §9.12 P0: non-fatal review note (e.g. low-confidence OCR words). The file
    // succeeded, but the output needs human review; surfaced as a warning in
    // the batch log + error log instead of being silently dropped.
    QString reviewNote;
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
    // U08: success + failed + remaining summaries — files still being
    // processed (or dropped by cancel) without miscounting them as done.
    int  remainingCount() const { return qMax(0, fileCount() - successCount() - failCount()); }

    // U08: per-item pre-flight, run on the GUI thread BEFORE the worker starts
    // (probes are cached and GUI-affine). Returns a non-empty whyNot when
    // `inputPath` cannot be processed by operation `opIndex`; empty = runnable.
    // Blocked items are staged as failed BatchFileResults so the summary stays
    // truthful — never a silent skip, never a claimed completion.
    static QString preFlightBlocker(int opIndex, const QString& inputPath,
                                    const gp::CapabilityRegistry* capabilities);

    // U08: report intentionally unsupported batch options (plan U08) instead
    // of silently diverging from the interactive path. Pure function of the
    // persisted prefs + capabilities; empty when every interactive option
    // applies to the batch run. Surfaced via BatchFileResult::reviewNote.
    static QString preFlightReviewNote(int opIndex,
                                       const gp::CapabilityRegistry* capabilities);

    // Programmatic run/cancel triggers (bypass UI state guards for tests)
    void onRunBatch()    { onRunClicked(); }
    void onCancelBatch() { onCancelClicked(); }

    // Test seam: select the batch operation by index (matches m_opCombo order).
    void setOperationForTest(int index);

    // §9.12 P0 test seam: build the review note for a batch OCR result.
    // Returns an empty string when every word is at or above the confidence
    // threshold; otherwise "N low-confidence word(s) on page(s) … need review".
    // Pure function so the flagging rule is testable without running OCR.
    static QString lowConfidenceNote(const QList<PageOcrResult>& pages,
                                     int confidenceThreshold = 60);

    // ── §9.12 P1: Compress/Optimize target DPI ────────────────────────────────
    // The batch Compress op used to hard-code targetDpi = 150 with only a
    // static "150 DPI" note in the UI. The supported engine range and default
    // are named so the boundary is documented and testable.
    static constexpr int kMinTargetDpi    = 36;
    static constexpr int kMaxTargetDpi    = 600;
    static constexpr int kDefaultTargetDpi = 150;  // the previous hard-coded value
    // Pure seam: clamp a user-chosen target DPI into [kMinTargetDpi,
    // kMaxTargetDpi]. The worker applies this before OptimizeOptions so an
    // out-of-range spin value can never reach the engine.
    static int resolveCompressTargetDpi(int requestedDpi);

    // ── §9.12 P1: named PII redaction presets ────────────────────────────────
    // Pure seam: the effective redaction pattern list — the regex bodies of
    // the named presets (resolved through PatternRedactor::namedPattern, the
    // SAME built-in keys the interactive Redact mode offers: "email",
    // "phone-us", "ssn", …) followed by the free-form comma-separated
    // entries. Empty, unresolvable, and duplicate patterns are dropped, so a
    // span is never excised twice. Empty result = nothing to redact.
    static QStringList effectiveRedactPatterns(const QStringList& presetKeys,
                                               const QStringList& freeFormPatterns);

    // Test seam: the preset keys of the currently checked named-PII preset
    // checkboxes ("email", "phone-us", …), in panel order. GUI read — only
    // call from the GUI thread (tests, or onRunClicked's capture phase).
    QStringList checkedRedactPresetKeys() const;

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
    void onToggleHotFolder();
    void onHotFolderChanged(const QString& path);

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
    QComboBox*          m_dpiPresetCombo = nullptr;   // §9.12 P1: Low/Medium/High quick picks
    QSpinBox*           m_dpiSpin        = nullptr;   // §9.12 P1: the source of truth the worker captures
    QLineEdit*          m_compressOutDir = nullptr;

    // Watermark panel
    QLineEdit*          m_wmTextEdit     = nullptr;
    QSpinBox*           m_wmOpacity      = nullptr;
    QLineEdit*          m_wmOutDir       = nullptr;

    // Export PDF/A panel
    QComboBox*          m_pdfaLevel      = nullptr;
    QLineEdit*          m_pdfaOutDir     = nullptr;

    // Merge panel
    QLineEdit*          m_mergeOutDir    = nullptr;

    // OCR panel
    QLineEdit*          m_ocrOutDir      = nullptr;
    QComboBox*          m_ocrLanguage    = nullptr;   // §9.12 P0: batch OCR language

    // Redact panel
    QLineEdit*          m_redactPatterns = nullptr;   // comma-separated regex patterns
    QList<class QCheckBox*> m_redactPresets;             // §9.12 P1: named PII quick picks
    QLineEdit*          m_redactOutDir   = nullptr;

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
        OpMerge     = 4,
        OpOCR       = 5,
        OpRedact    = 6,
    };

    // Special-case handler for Merge (single combined output, not per-file mapped).
    void runMerge();

    // Hot folder (Phase 3) — watch a directory and auto-ingest new PDFs.
    void buildHotFolderSection(QVBoxLayout* btnLay);
    static QString hotFileKey(const QFileInfo& fi);   // filename + mtime identity

    QCheckBox*          m_hotFolderCheck   = nullptr;
    QLineEdit*          m_hotFolderEdit    = nullptr;
    QCheckBox*          m_hotAutoRunCheck  = nullptr;
    QFileSystemWatcher* m_hotFolderWatcher = nullptr;
    QString             m_hotFolderPath;
    QTimer*             m_hotFolderDebounce = nullptr;
    QSet<QString>       m_hotProcessed;     // already-seen files (filename+mtime)
};

} // namespace gp
