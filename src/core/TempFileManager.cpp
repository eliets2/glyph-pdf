// SPDX-License-Identifier: Apache-2.0
#include "core/TempFileManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <cstdlib> // atexit

// ── Singleton ────────────────────────────────────────────────────────────

TempFileManager& TempFileManager::instance() {
    static TempFileManager mgr;
    return mgr;
}

TempFileManager::~TempFileManager() {
    cleanAll();
}

// ── Registration ─────────────────────────────────────────────────────────

void TempFileManager::track(const QString& path) {
    QMutexLocker lock(&m_mutex);
    if (!m_tracked.contains(path))
        m_tracked.append(path);
}

void TempFileManager::untrack(const QString& path) {
    QMutexLocker lock(&m_mutex);
    m_tracked.removeAll(path);
}

// ── Creation ─────────────────────────────────────────────────────────────

QString TempFileManager::createTempFile(const QString& suffix) {
    QDir dir(appTempDir());
    if (!dir.exists()) dir.mkpath(".");

    QTemporaryFile tmp(dir.filePath("glyph_XXXXXX" + suffix));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        qWarning() << "TempFileManager: failed to create temp file in" << dir.path();
        return {};
    }
    QString path = tmp.fileName();
    tmp.close();
    track(path);
    return path;
}

QString TempFileManager::createTempDir(const QString& prefix) {
    QDir parent(appTempDir());
    if (!parent.exists()) parent.mkpath(".");

    QTemporaryDir tmp(parent.filePath(prefix + "_XXXXXX"));
    if (!tmp.isValid()) {
        qWarning() << "TempFileManager: failed to create temp dir in" << parent.path();
        return {};
    }
    tmp.setAutoRemove(false);
    QString path = tmp.path();
    track(path);
    return path;
}

// ── Cleanup ──────────────────────────────────────────────────────────────

void TempFileManager::cleanAll() {
    QMutexLocker lock(&m_mutex);
    for (const QString& path : m_tracked) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        if (fi.isDir()) {
            QDir(path).removeRecursively();
        } else {
            QFile::remove(path);
        }
    }
    int count = m_tracked.size();
    m_tracked.clear();
    if (count > 0)
        qDebug() << "TempFileManager: cleaned" << count << "temp entries on exit";
}

void TempFileManager::cleanStaleTempFiles() {
    QDir dir(appTempDir());
    if (!dir.exists()) return;

    // Remove anything older than 24 hours from previous sessions
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    int removed = 0;
    for (const QFileInfo& fi : entries) {
        if (fi.lastModified() < cutoff) {
            if (fi.isDir()) {
                QDir(fi.absoluteFilePath()).removeRecursively();
            } else {
                QFile::remove(fi.absoluteFilePath());
            }
            ++removed;
        }
    }
    if (removed > 0)
        qDebug() << "TempFileManager: cleaned" << removed << "stale temp entries from previous sessions";
}

// ── Static helpers ───────────────────────────────────────────────────────

QString TempFileManager::appTempDir() {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + QDir::separator() + "GlyphPDF";
}

static void atexitCleanup() {
    TempFileManager::instance().cleanAll();
}

void TempFileManager::install() {
    // Clean stale files from previous runs
    instance().cleanStaleTempFiles();
    // Register atexit handler for this session
    std::atexit(atexitCleanup);
}
