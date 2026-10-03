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
#include "modes/HotFolderController.h"

#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QTimer>

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
QString HotFolderController::hotFileKey(const QFileInfo& fi) const {
    return QDir(m_dir).relativeFilePath(fi.absoluteFilePath()) + QLatin1Char('|')
         + QString::number(fi.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|')
         + QString::number(fi.size());
}

// The whole watched tree, files only, matching the historical two-pattern
// filter (*.pdf and *.PDF). Per-directory order follows the platform
// directory order (name-sorted on NTFS), as before.
QList<QFileInfo> HotFolderController::recursivePdfEntries() const {
    QList<QFileInfo> out;
    if (m_dir.isEmpty()) return out;
    QDirIterator it(m_dir,
                    QStringList() << QStringLiteral("*.pdf") << QStringLiteral("*.PDF"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        out << it.fileInfo();
    }
    return out;
}

void HotFolderController::seedProcessed() {
    m_processed.clear();
    const auto entries = recursivePdfEntries();
    for (const QFileInfo& fi : entries)
        m_processed.insert(hotFileKey(fi));
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
void HotFolderController::watchSubdirectories() {
    if (!m_watcher || m_dir.isEmpty()) return;
    ++m_watchWalks;  // F-7 test seam: this is the expensive full-tree walk
    const QStringList already = m_watcher->directories();
    QSet<QString> watched(already.cbegin(), already.cend());
    QStringList toAdd;
    QDirIterator it(m_dir, QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (!watched.contains(path)) {
            watched.insert(path);
            toAdd << path;
        }
    }
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

    for (const QFileInfo& fi : recursivePdfEntries()) {
        const QString key = hotFileKey(fi);
        if (!m_processed.contains(key)) {
            m_processed.insert(key);
            newFiles << fi.absoluteFilePath();
        }
    }
    if (!newFiles.isEmpty() && m_ingestHandler)
        m_ingestHandler(newFiles);
    return newFiles;
}

} // namespace gp
