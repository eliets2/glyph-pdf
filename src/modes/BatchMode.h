// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/ErrorInfo.h"
#include "core/AppContext.h"
#include "core/BatchPreset.h" // R26 (batch-presets P1): named preset model + JSON store
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
#include <QMap>
#include <QStandardItemModel>
#include <QFileSystemWatcher>
#include <QSet>
#include <functional>

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
    // N3 (pdf24 skip-already-text pattern): the file was DELIBERATELY not
    // OCRed — it already carries a text layer and the skip option is on (or
    // force-OCR is off). Truthful accounting: a skipped file is never counted
    // as success OR failure; the summary reports it in its own "N skipped"
    // bucket with the reason.
    bool    skipped = false;
    QString skipReason;
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
    // N3: files deliberately skipped by the skip-already-text options —
    // reported in their own bucket, never as completed OCR work.
    int  skipCount()     const { return m_skipCount; }
    int  errorLogCount() const { return m_errorLog.count(); }
    // emergence E-2: pins read back the detail the worker recorded for a
    // failed file (e.g. the policy whyNot for a refused OCR download) —
    // the ErrorInfo technicalDetails of the index-th log entry.
    QString errorDetailForTest(int index) const
        { return m_errorLog.entries.value(index).technicalDetails; }
    // U08: success + failed + remaining summaries — files still being
    // processed (or dropped by cancel) without miscounting them as done.
    int  remainingCount() const { return qMax(0, fileCount() - successCount() - failCount() - skipCount()); }

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

    // §9.12 P1 test seam: invoked on the merge worker thread at each file
    // boundary BEFORE the file is appended. Tests use it to make the boundary
    // windows deterministic (cancel/progress timing); production never sets
    // it. MUST be set before onRunBatch() — the run captures it by value, so
    // the member itself is never touched cross-thread.
    void setMergeBoundaryHookForTest(std::function<void(int)> hook) {
        m_mergeBoundaryHook = std::move(hook);
    }

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

    // PGR-35 (D2 delta review 2026-09-23): pure seam — the cross-file
    // output-collision rule. input[i]'s output is outputs[i]; the first input
    // (list order) claiming an output path keeps it, every later input
    // resolving to the same path gets a staged pre-flight blocker. Path
    // identity is case-insensitive on Windows, case-sensitive elsewhere.
    // onRunClicked stages these blockers like the U08 pre-flight refusals —
    // a colliding file fails honestly instead of silently overwriting an
    // earlier output the ledger already counted as a success.
    static QMap<QString, QString> outputCollisionBlockers(const QStringList& inputs,
                                                          const QStringList& outputs);

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

    // ── R26 (batch-presets P1): named preset surface ─────────────────────────
    // Presets are DATA (docs/research/batch-presets-implementation-plan.md
    // §5.1): a preset changes WHAT runs, never HOW results are accounted —
    // a preset run flows through the same per-file worker, SafeSave
    // transactional commit and G12 exactly-once accounting as every other op.
    //
    // The store root is redirected for tests (settings isolation); production
    // uses <AppDataLocation>/presets (BatchPresetStore::defaultRootDir).
    static void setPresetStoreDirForTest(const QString& dir);

    // Capture the CURRENTLY configured classic operation (Compress/Watermark/
    // Export-PDF/A/Redact) as a one-step named preset. Convert/Merge/OCR
    // configurations are refused with an honest explanation (those engines
    // are not preset steps in this build). GUI thread only.
    bool saveConfiguredOpAsPresetForTest(const QString& name, QString* err = nullptr);

    // Load + display the preset with `id` (the same path the picker's
    // currentIndexChanged takes). False when the id is unknown.
    bool selectPresetForTest(const QString& id);

    // The step/capability disclosure text shown for the selected preset:
    // one line per step (op + key params) plus, per registry-gated step,
    // the CapabilityRegistry's whyNot + alternative. Empty when no preset
    // is selected.
    QString presetStepsDisplayForTest() const;

    QStringList presetIdsForTest() const;

    // Rename edits the DISPLAY NAME only; the id (and file) stay stable.
    bool renamePresetForTest(const QString& id, const QString& newName, QString* err = nullptr);
    // Delete WITHOUT the interactive confirm (the button path asks; this seam
    // is the post-confirm action so tests never drive a native modal).
    bool deletePresetForTest(const QString& id, QString* err = nullptr);

    // Re-read the preset store into the picker (tests root preset files into
    // the store externally; production refreshes on save/rename/delete).
    void refreshPresetsForTest() { refreshPresetPicker(); }
    // The GUI-thread run gate for the currently selected preset (empty =
    // runnable) — the same answer onRunClicked stages per file.
    QString presetRunBlockerForTest() const { return presetRunBlocker(); }

signals:
    // Emitted from onBatchFinished so tests can spy on completion.
    void batchFinished();
    // SEP13 leads 9+10: per-item FUTURE progress (raw worker value, e.g.
    // merge file boundaries) re-emitted from onBatchProgress. With merge
    // result publication deferred until the output's fate is known, this is
    // the honest mid-run observable for "the worker advances on its own"
    // (per-item success accounting no longer streams before the save).
    void batchProgress(int value);

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
    // R26 (batch-presets P1)
    void onPresetSelected(int index);
    void onSaveAsPresetClicked();
    void onRenamePresetClicked();
    void onDeletePresetClicked();

private:
    void buildFilePanel(QWidget* host);
    void buildOperationPanel(QWidget* host);
    void buildProgressPanel(QWidget* host);
    void addFilePaths(const QStringList& paths);
    void syncFileList();
    QString resolveOutputPath(const QString& inputPath) const;
    bool confirmOverwrite(const QString& path);

    // R26 (batch-presets P1): the preset config panel (picker + step/capability
    // disclosure + Save as/Rename/Delete) and its store plumbing.
    void buildPresetPanel(QWidget* host);
    void refreshPresetPicker(const QString& selectId = {});
    // The store over the test-re-pointable root (rebuilt per call on purpose:
    // tests may re-point the root between construction and use).
    static BatchPresetStore presetStore();
    bool captureConfiguredOpAsPreset(const QString& name, QString* err);
    // GUI-thread run gate for the selected preset: non-empty whyNot when the
    // preset is unselected, has no steps, requires a newer app, or carries a
    // step whose capability is currently unavailable (registry-queried).
    QString presetRunBlocker() const;
    // The step/capability disclosure text (shared by the panel and the test seam).
    static QString presetStepsDisplayText(const BatchPreset& preset,
                                          const gp::CapabilityRegistry* capabilities);

    void appendLog(const QString& text, const QString& color = {});
    void appendFileResult(const QString& file, bool success, const QString& detail = {});
    // §9.12 P1: per-result accounting (log + counters + error log) shared by
    // the resultReadyAt handler and the merge drain in onBatchFinished.
    void accountResultAt(int idx);
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
    // N3: pdf24 skip-already-text options. Force overrides both skips.
    class QCheckBox*    m_ocrSkipFilesWithText = nullptr;
    class QCheckBox*    m_ocrSkipPagesWithText = nullptr;
    class QCheckBox*    m_ocrForceOcr          = nullptr;

    // Redact panel
    QLineEdit*          m_redactPatterns = nullptr;   // comma-separated regex patterns
    QList<class QCheckBox*> m_redactPresets;             // §9.12 P1: named PII quick picks
    QLineEdit*          m_redactOutDir   = nullptr;

    // R26 (batch-presets P1): Preset Pipeline panel
    QComboBox*          m_presetCombo      = nullptr;
    QLabel*             m_presetStepsLabel = nullptr;
    QLabel*             m_presetBrokenLabel = nullptr;
    QLineEdit*          m_presetOutDir     = nullptr;
    BatchPreset         m_selectedPreset;              // valid only when m_presetSelected
    bool                m_presetSelected    = false;
    static QString      s_presetStoreDirForTest;       // settings-isolation seam

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
    // PGR-38 (D2 delta review 2026-09-23): the file count captured at run
    // staging. The worker maps a COPY of the list, so progress/ETA/accounting
    // must be computed against the run's own total — the file list stays
    // mutable mid-run (add/remove/hot-folder). Reset after the completion
    // contract so post-run reads keep the historical live-list semantics.
    int                 m_runFileTotal    = 0;
    QFutureWatcher<BatchFileResult> m_watcher;
    ErrorLog            m_errorLog;
    int                 m_successCount    = 0;
    int                 m_failCount       = 0;
    int                 m_skipCount       = 0;   // N3: truthful skip bucket
    // G12 (QUALITY-GATE-2026-09-09): exactly-once result accounting. A result
    // index is reconciled a single time no matter how its delivery races the
    // completion summary — `finished` can outrun the queued resultReadyAt
    // deliveries of the mapped workers, so onBatchFinished drains every
    // reported-but-unaccounted index from the future and the queued callbacks
    // that land afterwards are ignored by their index here.
    QSet<int>           m_accountedIndices;
    QElapsedTimer       m_batchTimer;
    QMutex              m_engineMutex;       // serializes pdfEditor calls across threads
    // F2a-F1 (SWEEP-W3-UX): the merge completion feedback must NAME the output
    // file. Set on the GUI thread when a merge run is dispatched; showSummary
    // surfaces it only when the merge actually committed (successCount > 0 —
    // a cancelled or failed merge writes no output and must not name one).
    QString             m_mergeOutputPath;

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
        // R26 (batch-presets P1): appended AFTER Redact — the append-only rule
        // keeps every existing OpIndex (and every test that drives
        // setOperationForTest by index) stable. The plan (§4.1) places the
        // preset entry FIRST in the combo; that would renumber every existing
        // op, so the additive position wins (recorded deviation).
        OpPresetPipeline = 7,
    };

    // Special-case handler for Merge (single combined output, not per-file mapped).
    void runMerge();

    // §9.12 P1: async merge worker — appends each input on the QtConcurrent
    // pool behind m_watcher (see startMergeWorker definition for the contract).
    void startMergeWorker(const QStringList& files, const QString& outPath);

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

    // §9.12 P1: merge file-boundary hook (test seam; see the setter above).
    std::function<void(int)> m_mergeBoundaryHook;
};

} // namespace gp
