// SPDX-License-Identifier: Apache-2.0
// HotFolderController.h — the watched-folder engine extracted from BatchMode
// (PARITY-SCORECARD-2026-09-30 §4 row 9, per PROGRAM-CONSOLIDATION §4).
//
// Owns the hot-folder lifecycle that used to live inline in
// BatchMode::onToggleHotFolder / BatchMode::onHotFolderChanged: the
// QFileSystemWatcher, the 500 ms single-shot debounce, the processed set
// (file-identity keys), and the PDF scan. BatchMode keeps the UI (picker
// dialog, checkbox, read-only path line, log) and reacts to the ingest
// handler — same messages, same order, same auto-run semantics.
//
// Characterization safety net: tests/TestHotFolder.cpp pinned the
// pre-extraction behavior; those pins stay green untouched through this
// extraction.
//
// R3-sec F-6 adds the watcher fan-out backstop: addPaths refusals (OS watch
// budget exhausted) are counted, ENGAGE the polling fallback for the
// unwatched subtrees, and are disclosed through the degraded-watch handler —
// silent permanent ingest gaps are not a strategy.
#ifndef GLYPHPDF_HOTFOLDERCONTROLLER_H
#define GLYPHPDF_HOTFOLDERCONTROLLER_H

#include <functional>

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QFileSystemWatcher;
class QTimer;

namespace gp {

class HotFolderController : public QObject {
    Q_OBJECT
public:
    // Debounce window for fs-event storms — BatchMode's historical 500 ms.
    static constexpr int kDebounceMs = 500;
    // Polling-fallback tick for network shares: fresh enough for a
    // watched-folder ingest, light enough for a share. Public because the
    // owner of the toggle (BatchMode) passes it to startPolling — the
    // constant and the parameter are one contract, not two magic numbers.
    static constexpr int kPollIntervalMs = 2000;

    explicit HotFolderController(QObject* parent = nullptr);
    ~HotFolderController() override;

    // Files discovered by an ingest pass. Fired ONLY when a pass found
    // something new (the historical contract: an empty pass stays silent).
    void setIngestHandler(std::function<void(const QStringList& files)> handler) {
        m_ingestHandler = std::move(handler);
    }

    // r3-sec F-6 disclosure channel: fired when subdirectories could not be
    // placed under native watch (addPaths failures — the OS watch budget is
    // finite and QFileSystemWatcher fails SILENTLY beyond it) and the polling
    // fallback engaged for the unwatched subtrees. BatchMode wires this to
    // its log; the controller also qWarnings.
    void setWatchDegradedHandler(std::function<void(const QStringList& unwatched)> handler) {
        m_watchDegradedHandler = std::move(handler);
    }

    // Watch-mode start: seeds the processed set with the folder's existing
    // PDFs (only NEW drops ingest) and watches the folder through
    // QFileSystemWatcher + the debounce.
    bool start(const QString& dir);

    // Polling fallback for filesystems where fs-events are unreliable
    // (network shares): no QFileSystemWatcher — a plain scan timer walks the
    // watched folder and delivers new files each tick.
    bool startPolling(const QString& dir, int intervalMs);

    // Seam parity with BatchMode::armHotFolderForTest: seed WITHOUT watching
    // (the historical arm creates no watcher); ingest runs via ingestDeliver.
    void arm(const QString& dir);

    // Tear down watch/poll + debounce and clear all state (the historical
    // toggle-off branch, verbatim).
    void stop();

    bool isWatching() const;
    bool hasFolder() const { return !m_dir.isEmpty(); }
    QString watchedPath() const { return m_dir; }
    bool isPolling() const { return m_pollTimer != nullptr; }

    // One scan-diff-deliver pass NOW (the debounce-timeout / poll-tick body).
    // Returns the delivered files (empty when nothing new).
    QStringList ingestDeliver();

    // ── Test seams ─────────────────────────────────────────────────────────
    // Drives exactly what a QFileSystemWatcher::directoryChanged delivery
    // does (debounce restart; the watch refresh runs at the fire — F-7),
    // without OS-event timing.
    void triggerDirectoryChangedForTest() { onDirectoryChanged(); }
    // Debounce passes delivered so far (trigger→one-pass wiring pins).
    int debouncePassesForTest() const { return m_debouncePasses; }
    // The directories currently under QFileSystemWatcher watch.
    QStringList watchedDirectoriesForTest() const;
    // Number of full-tree watch-walks performed (start + refresh passes).
    // Wave-2b F-7 pin seam: the refresh must run once per debounce fire,
    // never once per fs-event.
    int watchWalkCountForTest() const { return m_watchWalks; }
<<<<<<< HEAD
    // r3-sec F-6 seams. The hook, when set, REPLACES
    // QFileSystemWatcher::addPaths and returns exactly the paths that "hit
    // the OS watch budget" — a deterministic cap simulation (real addPaths
    // fails only past an OS limit no test can size). The remaining getters
    // expose the degradation accounting the backstop keeps.
    void setWatchAddPathsHookForTest(
        std::function<QStringList(const QStringList& paths)> hook) {
        m_addPathsHookForTest = std::move(hook);
    }
    // Subtrees native watch could not cover (cumulative set).
    QStringList unwatchedSubtreesForTest() const { return m_unwatched.values(); }
    // Cumulative count of refused addPaths entries (every refresh re-learns
    // the same losses; the disclosure fires once per subtree, not per pass).
    int watchFailureCountForTest() const { return m_watchFailures; }
=======
    // R3-perf seam: full-tree ingest SCANS actually performed by
    // ingestDeliver (poll ticks + debounce fires + explicit calls). A
    // re-entrant call — the ingest handler walking back into ingestDeliver —
    // must not run a second concurrent full-tree scan once the re-entrancy
    // guard lands; it returns empty instead of re-walking.
    int ingestScansForTest() const { return m_ingestScans; }
>>>>>>> 9189d0cd (feat(seams): three behavior-neutral observable counters for the r3-perf pins — RenderCache::renderPageAsync worker-runs/coalesced/completions, HotFolderController full-tree ingest scans, CompareMode posted QPromise progress reports)

private slots:
    void onDirectoryChanged();

private:
    void ensureDebounce();
    void ensurePollTimer();
    void seedProcessed();
    void watchSubdirectories();  // keep root + every subdirectory under watch
    // R3-sec F-6: the single choke point into the native watch (returns the
    // refused paths — what addPaths used to drop silently) and the backstop
    // that engages the polling fallback + disclosure on any refusal.
    QStringList addWatchPaths(const QStringList& paths);
    void engagePollingBackstop(const QStringList& failed);
    QList<QFileInfo> recursivePdfEntries() const;  // the whole tree, *.pdf/*.PDF

    // Subtree-unique identity: path relative to the watched root + mtime +
    // size (flat files yield the historical filename|mtime key shape plus
    // the size term; wave-2b F-5 — mtime-preserving or same-tick rewrites on
    // NAS-class shares used to share the old key and never re-ingest).
    //
    // CROSS-PATH DEDUP INVARIANT: the fs-event path (debounce fire) and the
    // polling path (tick) funnel through the SAME ingestDeliver() pass and
    // the SAME m_processed key set, so a file delivered by either path is
    // never re-delivered by the other — even when both paths are live at
    // once (a stray fs-event on a polled network share). Pinned by
    // TestHotFolder::crossPathIngestSharesOneProcessedSet.
    QString hotFileKey(const QFileInfo& fi) const;

    QString m_dir;
    QSet<QString> m_processed;          // already-seen file keys
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_debounce = nullptr;       // single-shot kDebounceMs
    QTimer* m_pollTimer = nullptr;      // polling fallback (network shares)
    std::function<void(const QStringList&)> m_ingestHandler;
    // r3-sec F-6: degradation accounting for the watcher fan-out backstop.
    std::function<QStringList(const QStringList&)> m_addPathsHookForTest;
    std::function<void(const QStringList&)> m_watchDegradedHandler;
    QSet<QString> m_unwatched;          // subtrees native watch could not cover
    QSet<QString> m_degradedDisclosed;  // disclosure fires once per subtree
    int m_watchFailures = 0;
    int m_debouncePasses = 0;
    int m_watchWalks = 0;               // full-tree walk counter (F-7 seam)
    int m_ingestScans = 0;              // full-tree ingest scans (R3-perf seam)
};

} // namespace gp
#endif // GLYPHPDF_HOTFOLDERCONTROLLER_H
