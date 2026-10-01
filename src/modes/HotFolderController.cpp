// SPDX-License-Identifier: Apache-2.0
// HotFolderController.cpp — see HotFolderController.h. This extraction is
// behavior-preserving against the inline BatchMode implementation (commit
// 8b3159b8 pins it): same seeding, same filename+mtime keys, same 500 ms
// single-shot debounce, same delete-on-stop teardown, same silent empty
// passes. Recursive watching and the polling fallback arrive with the
// capability commit and are noted where they hook in.
#include "modes/HotFolderController.h"

#include <QDir>
#include <QFileSystemWatcher>
#include <QTimer>

namespace gp {

HotFolderController::HotFolderController(QObject* parent) : QObject(parent) {}

HotFolderController::~HotFolderController() = default;

// static — identity key for a file: name + last-modified time. A file is only
// auto-ingested once unless it is replaced/modified. (Historical BatchMode
// key, verbatim.)
QString HotFolderController::hotFileKey(const QFileInfo& fi) {
    return fi.fileName() + QLatin1Char('|')
         + QString::number(fi.lastModified().toMSecsSinceEpoch());
}

QStringList HotFolderController::scanPdfFiles() const {
    QStringList paths;
    if (m_dir.isEmpty()) return paths;
    const auto entries = QDir(m_dir).entryInfoList(QStringList() << QStringLiteral("*.pdf")
                                                                 << QStringLiteral("*.PDF"),
                                                   QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries)
        paths << fi.absoluteFilePath();
    return paths;
}

void HotFolderController::seedProcessed() {
    m_processed.clear();
    if (m_dir.isEmpty()) return;
    const auto entries = QDir(m_dir).entryInfoList(QStringList() << QStringLiteral("*.pdf")
                                                                 << QStringLiteral("*.PDF"),
                                                   QDir::Files);
    for (const QFileInfo& fi : entries)
        m_processed.insert(hotFileKey(fi));
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
    m_pollTimer->setInterval(intervalMs > 0 ? intervalMs : 1000);
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
    // Recursive capability hook: a change may have created subdirectories;
    // they need watches too (see the recursion commit).
    ensureDebounce();
    m_debounce->start();
}

QStringList HotFolderController::ingestDeliver() {
    QStringList newFiles;
    if (m_dir.isEmpty()) return newFiles;

    const auto entries = QDir(m_dir).entryInfoList(QStringList() << QStringLiteral("*.pdf")
                                                                 << QStringLiteral("*.PDF"),
                                                   QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries) {
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
