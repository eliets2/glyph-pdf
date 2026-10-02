// SPDX-License-Identifier: Apache-2.0
// HotFolderController.cpp — see HotFolderController.h. The extraction of the
// inline BatchMode watch was behavior-preserving (commit 447fd5bd; pins from
// 8b3159b8 stayed green untouched). This file carries the two enterprise
// capabilities on top (PARITY-SCORECARD-2026-09-30 §3 row 84 / §4 row 9):
//
//   * RECURSIVE watching — the root and every subdirectory (present at start
//     or created later) sit under QFileSystemWatcher; the ingest scan walks
//     the whole tree; the file-identity key is the root-relative path + mtime
//     (flat files keep the historical filename|mtime key; same-named files in
//     different subdirectories no longer collide).
//   * POLLING fallback — startPolling() replaces fs-events with a plain scan
//     timer for network shares where change notifications are unreliable.
#include "modes/HotFolderController.h"

#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QTimer>

namespace gp {

HotFolderController::HotFolderController(QObject* parent) : QObject(parent) {}

HotFolderController::~HotFolderController() = default;

// Identity key for a file: watched-root-relative path + last-modified time.
// A file is only auto-ingested once unless it is replaced/modified. Flat
// files yield exactly the pre-extraction filename|mtime key; nested files are
// subtree-unique (d1/same.pdf and d2/same.pdf are distinct drops).
QString HotFolderController::hotFileKey(const QFileInfo& fi) const {
    return QDir(m_dir).relativeFilePath(fi.absoluteFilePath()) + QLatin1Char('|')
         + QString::number(fi.lastModified().toMSecsSinceEpoch());
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

// Keep the root and every (transitive) subdirectory under native watch so a
// nested drop raises a change event without waiting for a poll tick.
void HotFolderController::watchSubdirectories() {
    if (!m_watcher || m_dir.isEmpty()) return;
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
    if (!toAdd.isEmpty())
        m_watcher->addPaths(toAdd);  // failures (dead network paths) are
                                     // silent — that is what polling is for
}

bool HotFolderController::start(const QString& dir) {
    m_dir = dir;
    seedProcessed();

    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged,
                this, &HotFolderController::onDirectoryChanged);
    }
    m_watcher->addPath(dir);
    watchSubdirectories();
    return true;
}

bool HotFolderController::startPolling(const QString& dir, int intervalMs) {
    m_dir = dir;
    seedProcessed();

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this,
                [this]() { ingestDeliver(); });
    }
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
        ingestDeliver();
    });
}

void HotFolderController::onDirectoryChanged() {
    if (m_dir.isEmpty()) return;  // stopped/never armed — inert (the pinned
                                  // empty-folder no-op; stop() must mean stopped)
    watchSubdirectories();  // a change may have created new subdirectories
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
