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
// R26-P2 (plan §3): one per-step record of a preset chain run, in chain order.
// status: Ok (promoted), Failed (aborted the chain; detail = techDetail),
// Blocked (refused pre-flight; detail = whyNot), Skipped (never attempted —
// the chain had already aborted at an earlier step; detail says why).
struct BatchStepResult {
    enum class Status { Ok, Failed, Blocked, Skipped };
    int     stepIndex = 0;
    QString op;
    QString label;
    Status  status = Status::Ok;
    // bates steps only: the first/last number actually stamped (-1 = n/a).
    int     firstBates = -1;
    int     lastBates  = -1;
    QString detail;
};

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
    // R26-P2 U4 (plan §4.6, N3): the failure is BATCH-SCOPED — the error class
    // is independent of file content (the same failure would hit any file, e.g.
    // the output directory vanished or the shared candidate store refused).
    // The run aborts at the current file boundary; this flag rides the result
    // so the report can name the batch-scoped cause ONCE (both lanes — the
    // ordered lane additionally drains the remainder as not-run).
    bool    batchScoped = false;
    // R26-P2 (plan §3): ordered per-step records of a preset chain run —
    // measured facts per step (bates ranges now; measured bytes with U5), so
    // the run is verifiable instead of summarized by a single techDetail.
    QList<BatchStepResult> steps;
};

// R26-P2 (plan §4.2): cross-file bates continuity state — owned by the
// ordered lane's single worker (no locking by construction) and advanced only
// by SUCCESSFUL stamps: a failed file's candidate is discarded and never
// burns a number (N1).
struct PresetRunState {
    bool batesStarted = false;
    int  lastBatesOut = 0;
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

    // ── R26-P2 (batch-presets P2) ─────────────────────────────────────────────
    // Per-file results of the LAST run in G12 accounting order, including the
    // per-step records (bates ranges, and measured bytes from U5). Test seam;
    // empty until the first run.
    QList<BatchFileResult> runResultsForTest() const { return m_lastRunResults; }

    // Lane rule of record (plan §4.2): bates-bearing presets need cross-file
    // continuity, which is only honest BY CONSTRUCTION — they run on the
    // ordered lane (one sequential worker in list order). Pure function.
    static bool presetNeedsOrderedLane(const BatchPreset& preset);

    // Ordered-lane boundary hook (test seam; the merge hook's analogue):
    // invoked on the worker thread at each file boundary BEFORE the file's
    // chain starts (`fileIndex` = 0-based runnable-list order). Production
    // never sets it; the run captures it by value, so the member itself is
    // never touched cross-thread. MUST be set before onRunBatch().
    void setPresetBoundaryHookForTest(std::function<void(int)> hook) {
        m_presetBoundaryHook = std::move(hook);
    }

    // R26-P2 U2 (plan §4.3): pre-check/commit race seam — invoked on the
    // worker immediately BEFORE commitFileToDestination with the final
    // destination path. Production never sets it; the run captures it by
    // value. MUST be set before onRunBatch().
    void setPresetRaceHookForTest(std::function<void(const QString& dest)> hook) {
        m_presetRaceHook = std::move(hook);
    }

    // R26-P2 U2: drives the watcher's ingest path synchronously (the debounce
    // timer's work — list the hot folder, ingest new files, auto-run when the
    // option is on). Tests never wait on QFileSystemWatcher timing.
    void runHotFolderIngestForTest() {
        if (!m_hotFolderPath.isEmpty())
            onHotFolderChanged(m_hotFolderPath);
    }

    // R26-P2 U2: arms the hot folder WITHOUT the native directory picker (the
    // checkbox path stays interactive). Same seeding as onToggleHotFolder's
    // ON branch; the auto-run option is switched on so the ingest runs.
    void armHotFolderForTest(const QString& dir);

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
    // R26-P2 U2: `deConflictErr` optionally receives the rename
    // de-confliction exhaustion reason (plan §4.3) when the returned path is
    // empty because every stem-N candidate was occupied — the caller reports
    // it instead of a generic resolution failure. Never overwrites.
    QString resolveOutputPath(const QString& inputPath,
                              QString* deConflictErr = nullptr) const;
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

    // R26-P2 (plan §4.2): the ordered lane — one sequential worker iterating
    // `files` IN LIST ORDER behind the same QFutureWatcher/G12 accounting;
    // `runOneFile` is the captured per-file worker (the mapped pipeline's
    // body) with the run-state pointer for bates continuity. Cancellation is
    // polled at file boundaries only — never mid-chain. U3 (plan §4.4):
    // `stopOnFileFailure` implements onFileFailure "stop" — after the first
    // failed (not skipped) file the queue halts at that file boundary and
    // every remaining file is reported as not-run with the policy reason
    // (the U08 skip bucket). Stop-policy presets run HERE because with the
    // mapped pipeline WHICH files would be skipped is pool-scheduling luck —
    // the not-run report must be deterministic, not scheduling luck.
    void startPresetOrderedWorker(
        const QStringList& files,
        const std::function<BatchFileResult(const QString&, PresetRunState*)>& runOneFile,
        bool stopOnFileFailure = false);

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

    // R26-P2: ordered-lane boundary hook (test seam; captured by value at run
    // start) and the last run's per-file results (G12 accounting order).
    std::function<void(int)> m_presetBoundaryHook;
    QList<BatchFileResult>   m_lastRunResults;
    // R26-P2 U2 (§4.3/§4.5): the pre-check/commit race seam and the
    // unattended-ingest state — the pending flag is consumed once per run;
    // while set, the staged preset's effective onConflict is "rename"
    // (ask degrades, logged) so the watcher path never opens a modal.
    std::function<void(const QString&)> m_presetRaceHook;
    bool     m_unattendedAutoRunPending = false;
    QString  m_presetConflictOverride;
    // R26-P2 U4 (plan §4.6, N3): the batch-scoped abort cause of the CURRENT
    // run — recorded from the first batch-scoped failure during accounting
    // (GUI thread), reported ONCE at completion, cleared per run. The
    // unattended flag mirrors m_unattendedAutoRunPending's consumed value so
    // the abort line can disclose that the hot folder stays armed.
    QString  m_batchAbortCause;
    bool     m_runWasUnattended = false;
};

} // namespace gp
