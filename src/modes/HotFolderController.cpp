// SPDX-License-Identifier: Apache-2.0
// HotFolderController.cpp — see HotFolderController.h. The extraction of the
// inline BatchMode watch was behavior-preserving (commit 447fd5bd; pins from
// 8b3159b8 stayed green untouched). This file carries the two enterprise
// capabilities on top (PARITY-SCORECARD-2026-09-30 §3 row 84 / §4 row 9):
//
//   * RECURSIVE watching — the root and every subdirectory (present at start
//     or created later) sit under QFileSystemWatcher; the ingest scan walks
//     the whole tree; the file-identity key is the root-relative path +
//     mtime + size (flat files keep the historical filename|mtime key shape
//     plus the size term — wave-2b F-5; same-named files in different
//     subdirectories no longer collide, and length-changing rewrites with
//     preserved mtimes re-ingest).
//   * POLLING fallback — startPolling() replaces fs-events with a plain scan
//     timer for network shares where change notifications are unreliable.
//
// Wave-2b F-7: the full-tree watch refresh runs at the DEBOUNCE FIRE (once
// per quiet window, alongside the ingest pass that walks the same tree
// anyway) — never synchronously per fs-event on the GUI thread.
//
// R3-sec F-6 (adversary wave-2b): QFileSystemWatcher::addPaths fails
// SILENTLY once the OS watch budget is exhausted (inotify
// max_user_watches; the per-handle/thread cost on Windows) — subtrees
// beyond the cap became PERMANENTLY unwatched with no backstop in watcher
// mode: a drop there raised no event, ever, and isWatching() kept reporting
// healthy. The refresh now counts the refusals via addPaths' return value,
// engages the EXISTING polling fallback when anything could not be watched
// (the ingest scan walks the whole root per tick, so every unwatched
// subtree is covered), and discloses once per degraded subtree through the
// degraded-watch log channel (BatchMode's log) plus qWarning.
//
// r4-misc (cross-model audit round 4):
//   * security-auditor finding 3 — the processed-set is BOUNDED (TTL sweep
//     of unobserved entries + a hard entry cap); see HotFolderController.h
//     for the honest re-ingest tradeoff.
//   * adversary dynamic probe (junction loop) — the tree walk is loop-safe
//     (walkTree's per-pass visited set). Pre-fix, a directory junction
//     inside the watched root (root/loop → root) was DESCENDED: one real
//     file delivered 64 times, the walk stopping only at the OS reparse
//     resolution cap.
#include "modes/HotFolderController.h"

#include <QDateTime>
#include <QDir>
#include <QFileSystemWatcher>
#include <QTimer>

#include <algorithm>

namespace gp {

HotFolderController::HotFolderController(QObject* parent) : QObject(parent) {}

HotFolderController::~HotFolderController() = default;

// Identity key for a file: watched-root-relative path + last-modified time
// + size. A file is only auto-ingested once unless it is replaced/modified.
// Flat files yield exactly the pre-extraction filename|mtime key shape (plus
// the size term); nested files are subtree-unique (d1/same.pdf and
// d2/same.pdf are distinct drops).
//
// Wave-2b F-5: path+mtime is NOT content identity on the filesystems this
// feature targets — mtime-preserving producers (cp -p, archive
// re-extraction, backup restore, robocopy /COPYTIMES) and NAS shares with
// 1–2 s timestamp granularity make a replaced file's mtime equal the old
// one, and the rewrite was silently never re-ingested. The size term makes
// every length-changing rewrite a new identity. Residual, documented and by
// design (the poll tick cadence is the feature's contract): a rewrite that
// preserves BOTH size and mtime is still one ingest per session.
// Pure stat fields only — the key never opens or decodes file content.
// R3-perf: \p root is the caller's hoisted QDir(m_dir); building the key for
// the whole tree constructs the root once per pass, not once per file.
QString HotFolderController::hotFileKey(const QFileInfo& fi, const QDir& root) const {
    return root.relativeFilePath(fi.absoluteFilePath()) + QLatin1Char('|')
         + QString::number(fi.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|')
         + QString::number(fi.size());
}

// The whole watched tree, loop-safe, streamed entry-by-entry through the
// visitor callbacks. r4-misc (adversary junction-loop probe): the naive
// QDirIterator walk DESCENDED a directory junction (root/loop → root),
// enumerating the whole tree once per loop hop — measured pre-fix: one real
// file delivered 64 times, the walk terminating only where the OS refuses
// to resolve the 64th reparse hop. An accidental bound is not a bound. The
// iterative engine keeps a per-pass VISITED set keyed on canonical paths:
// every real directory is descended exactly once, whatever links point at
// it (a junction to any already-visited directory is skipped; two junctions
// to the same target both skip once the target is visited). On POSIX the
// same guard covers symlink loops. Pure stat/realpath work — never opens
// file content.
void HotFolderController::walkTree(
        const std::function<void(const QFileInfo&)>& visit,
        const std::function<void(const QFileInfo&)>& dirVisit) const {
    if (m_dir.isEmpty()) return;
    QSet<QString> visited;   // canonical paths of descended directories
    QList<QString> stack{m_dir};
    while (!stack.isEmpty()) {
        const QString dir = stack.takeLast();
        const QString canon = QFileInfo(dir).canonicalFilePath();
        const QString visitedKey = canon.isEmpty() ? QDir::cleanPath(dir) : canon;
        if (visited.contains(visitedKey)) continue;
        visited.insert(visitedKey);
        const QDir d(dir);
        if (visit) {
            // The historical two-pattern filter (*.pdf AND *.PDF).
            const QList<QFileInfo> files =
                d.entryInfoList(QStringList() << QStringLiteral("*.pdf")
                                              << QStringLiteral("*.PDF"),
                                QDir::Files);
            for (const QFileInfo& fi : files) visit(fi);
        }
        const QList<QFileInfo> subs =
            d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& sub : subs) {
            // r4-misc junction-loop probe: an NTFS directory junction is
            // NEVER descended. Qt's canonicalFilePath does NOT resolve
            // junctions (measured on Qt 6.11: it reports the junction's own
            // path, and isSymLink() is false for them), so the canonical
            // visited-set above cannot catch a junction loop — and descending
            // one double-enumerates the target tree under different
            // root-relative identity keys (the adversary probe measured ONE
            // real file delivered 64 times, the walk stopping only at the OS
            // reparse resolution cap). A junction is a leaf: drops behind it
            // ingest via the target's own path when that lies inside the
            // root; a junction to an external location is not walked (a
            // documented narrowing — the pre-fix walk descended it, loop and
            // all). POSIX/Windows true symlinks keep the canonical
            // visited-set guard: canonicalFilePath resolves those.
            if (sub.isJunction()) continue;
            if (dirVisit) dirVisit(sub);
            stack.append(sub.absoluteFilePath());
        }
    }
}

void HotFolderController::seedProcessed() {
    m_processed.clear();
    const QDir root(m_dir);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    walkTree([this, &root, now](const QFileInfo& fi) {
        m_processed.insert(hotFileKey(fi, root), now);
    }, {});
}

// The single choke point through which every directory enters the native
// watch. Production forwards to QFileSystemWatcher::addPaths, whose return
// value is exactly the paths the OS refused (the silent failures F-6
// exploits); the test hook substitutes a deterministic cap simulation.
QStringList HotFolderController::addWatchPaths(const QStringList& paths) {
    if (paths.isEmpty()) return {};
    if (m_addPathsHookForTest) return m_addPathsHookForTest(paths);
    return m_watcher->addPaths(paths);
}

void HotFolderController::ensurePollTimer() {
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this,
                [this]() { ingestDeliver(); });
        m_pollTimer->setInterval(kPollIntervalMs);
    }
}

// F-6 backstop: native watch could not cover these paths (OS watch budget —
// the refusals used to be swallowed). Engage the EXISTING polling fallback:
// the tick's ingest scan walks the whole root, so every unwatched subtree is
// covered at kPollIntervalMs cadence. Once engaged for degradation, polling
// stays engaged for the session — a later successful re-add narrows the gap,
// but the tick is the honest guarantee (and delivery is deduped by the
// shared processed set, so watch+poll never double-delivers). Disclose once
// per subtree: the refresh re-learns the same losses every pass and must not
// spam the log channel.
void HotFolderController::engagePollingBackstop(const QStringList& failed) {
    QStringList fresh;
    for (const QString& path : failed) {
        m_unwatched.insert(path);
        ++m_watchFailures;
        if (!m_degradedDisclosed.contains(path)) {
            m_degradedDisclosed.insert(path);
            fresh << path;
        }
    }
    if (!fresh.isEmpty()) {
        qWarning() << "hot folder:" << fresh.size()
                   << "subdirector(ies) could not be placed under native "
                      "watch (OS watch budget?) — polling fallback engaged "
                      "for the unwatched subtrees:"
                   << fresh.join(QStringLiteral(", "));
        if (m_watchDegradedHandler) m_watchDegradedHandler(fresh);
    }
    ensurePollTimer();
    if (!m_pollTimer->isActive()) m_pollTimer->start();
}

// Keep the root and every (transitive) subdirectory under native watch so a
// nested drop raises a change event without waiting for a poll tick.
// r4-misc: the walk is loop-safe (walkTree's visited set) — a junction to an
// already-covered directory is no longer re-walked, and (a side benefit)
// the root and a junction pointing at it no longer BOTH get watch handles
// (the duplicate watch double-delivered every event on the shared tree).
void HotFolderController::watchSubdirectories() {
    if (!m_watcher || m_dir.isEmpty()) return;
    ++m_watchWalks;  // F-7 test seam: this is the expensive full-tree walk
    const QStringList already = m_watcher->directories();
    QSet<QString> watched(already.cbegin(), already.cend());
    QStringList toAdd;
    walkTree({}, [this, &watched, &toAdd](const QFileInfo& sub) {
        const QString path = sub.absoluteFilePath();
        if (!watched.contains(path)) {
            watched.insert(path);
            toAdd << path;
        }
    });
    if (!toAdd.isEmpty()) {
        // R3-sec F-6: the refusals are no longer silent — anything the OS
        // would not watch degrades to the polling backstop and is disclosed.
        const QStringList failed = addWatchPaths(toAdd);
        if (!failed.isEmpty())
            engagePollingBackstop(failed);
    }
}

bool HotFolderController::start(const QString& dir) {
    m_dir = dir;
    seedProcessed();

    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged,
                this, &HotFolderController::onDirectoryChanged);
    }
    // The root is the top subtree: a refused root (nonexistent path, exhausted
    // budget, dead share) degrades the WHOLE watch to polling (F-6).
    const QStringList rootFailed = addWatchPaths(QStringList{ dir });
    if (!rootFailed.isEmpty())
        engagePollingBackstop(rootFailed);
    watchSubdirectories();
    return true;
}

bool HotFolderController::startPolling(const QString& dir, int intervalMs) {
    m_dir = dir;
    seedProcessed();

    ensurePollTimer();
    m_pollTimer->setInterval(intervalMs > 0 ? intervalMs : kPollIntervalMs);
    m_pollTimer->start();
    return true;
}

void HotFolderController::arm(const QString& dir) {
    m_dir = dir;
    seedProcessed();
}

void HotFolderController::stop() {
    if (m_watcher) {
        delete m_watcher;
        m_watcher = nullptr;
    }
    if (m_debounce) {
        m_debounce->stop();
        delete m_debounce;
        m_debounce = nullptr;
    }
    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
        m_pollTimer = nullptr;
    }
    m_dir.clear();
    m_processed.clear();
    // F-6 degradation accounting is per-watch state: a fresh start() begins
    // healthy again.
    m_unwatched.clear();
    m_degradedDisclosed.clear();
    m_watchFailures = 0;
}

bool HotFolderController::isWatching() const {
    if (m_watcher && !m_watcher->directories().isEmpty()) return true;
    return m_pollTimer && m_pollTimer->isActive();
}

QStringList HotFolderController::watchedDirectoriesForTest() const {
    return m_watcher ? m_watcher->directories() : QStringList();
}

void HotFolderController::ensureDebounce() {
    if (m_debounce) return;
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, [this]() {
        ++m_debouncePasses;
        // Wave-2b F-7: the watch refresh runs HERE, at the debounce fire —
        // once per quiet window, immediately before the ingest pass (which
        // already re-walks the same tree). QFileSystemWatcher's signal
        // carries no per-event cause, so a "walk only on directory
        // creation" refinement is not expressible over this API; what the
        // finding demands — never a full-tree walk per fs-event — holds:
        // N events inside a window cost ONE debounced walk, not N
        // synchronous ones, and nothing walks on the GUI thread before the
        // debounce arms.
        watchSubdirectories();
        ingestDeliver();
    });
}

void HotFolderController::onDirectoryChanged() {
    if (m_dir.isEmpty()) return;  // stopped/never armed — inert (the pinned
                                  // empty-folder no-op; stop() must mean stopped)
    ensureDebounce();
    m_debounce->start();
}

QStringList HotFolderController::ingestDeliver() {
    QStringList newFiles;
    if (m_dir.isEmpty()) return newFiles;

    // R3-perf (audit finding 6): re-entrancy guard. The ingest handler runs
    // SYNCHRONOUSLY inside this pass; a handler that walks back into
    // ingestDeliver (batch auto-run re-entering the controller) used to
    // trigger a second concurrent full-tree scan — on exactly the slow
    // shares this fallback exists for. The nested call is absorbed (empty
    // delivery, no re-scan); the outer pass owns the tick. Guard stays armed
    // across the handler call (RAII reset at scope exit).
    if (m_scanActive) return newFiles;
    struct ScanActiveGuard {
        bool& flag;
        explicit ScanActiveGuard(bool& f) : flag(f) { flag = true; }
        ~ScanActiveGuard() { flag = false; }
    } scanGuard(m_scanActive);

    ++m_ingestScans;  // R3-perf seam: one full-tree scan per pass
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // r4-misc (security-auditor finding 3): the bounded-set retention pass.
    // Entries whose file stopped being observed are swept before the scan;
    // the cap trim runs after it (both no-ops on a healthy small set).
    evictStaleProcessed(now);
    const QDir root(m_dir);
    walkTree([this, &root, &newFiles, now](const QFileInfo& fi) {
        const QString key = hotFileKey(fi, root);
        auto known = m_processed.find(key);
        if (known != m_processed.end()) {
            // Still present, still the same identity: refresh the
            // observation so a live file never ages out (a static archive
            // in the tree must not re-ingest every TTL window).
            known.value() = now;
            return;
        }
        m_processed.insert(key, now);
        newFiles << fi.absoluteFilePath();
    }, {});
    enforceProcessedCap();
    if (!newFiles.isEmpty() && m_ingestHandler)
        m_ingestHandler(newFiles);
    return newFiles;
}

// ── r4-misc (security-auditor finding 3): bounded-set retention ──────────────

qint64 HotFolderController::processedTtlMs() const {
    return m_ttlForTest > 0 ? m_ttlForTest : kProcessedTtlMs;
}

int HotFolderController::processedCapEntries() const {
    return m_capForTest > 0 ? m_capForTest : kProcessedMaxEntries;
}

// Sweep entries whose file stopped being OBSERVED past the TTL. A present
// file's entry is refreshed by every scan pass, so the sweep only ever
// removes history for files that vanished or changed — the dead weight the
// unbounded set used to accumulate forever.
void HotFolderController::evictStaleProcessed(qint64 nowMs) {
    const qint64 ttl = processedTtlMs();
    for (auto it = m_processed.begin(); it != m_processed.end();) {
        if (nowMs - it.value() > ttl)
            it = m_processed.erase(it);
        else
            ++it;
    }
}

// Hard memory bound: never hold more than the cap. Runs after every pass;
// on a healthy watch it is a size check. Past the cap (drop churn within
// one TTL window beyond 100k entries) the OLDEST-OBSERVED entries are
// evicted — an honest at-least-once tradeoff (their identical re-drops
// re-ingest) against unbounded memory. The key tiebreak keeps the eviction
// deterministic for the pins.
void HotFolderController::enforceProcessedCap() {
    const int cap = processedCapEntries();
    if (cap <= 0 || m_processed.size() <= cap) return;
    QList<QPair<qint64, const QString*>> byAge;
    byAge.reserve(m_processed.size());
    for (auto it = m_processed.constBegin(); it != m_processed.constEnd(); ++it)
        byAge.append({it.value(), &it.key()});
    std::sort(byAge.begin(), byAge.end(),
              [](const QPair<qint64, const QString*>& a,
                 const QPair<qint64, const QString*>& b) {
                  if (a.first != b.first) return a.first < b.first;
                  return *a.second < *b.second;
              });
    const int toRemove = m_processed.size() - cap;
    for (int i = 0; i < toRemove; ++i)
        m_processed.remove(*byAge[i].second);
}

} // namespace gp
