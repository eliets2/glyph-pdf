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
#include <QUuid>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <cstdlib> // atexit

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

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

// ── WP-R10 ownership records ─────────────────────────────────────────────

namespace {
// This process's session identity (owned directory + marker contents).
QString& sessionDirRef() {
    static QString path;
    return path;
}
QString& sessionTokenRef() {
    static QString token;
    return token;
}
// The root the current session directory was created under (the test seam
// can change the effective root — the session must follow it).
QString& sessionDirRootRef() {
    static QString root;
    return root;
}

// Test seam: isolated root for session creation AND cleanup. Empty = default.
QString& sessionRootOverrideRef() {
    static QString root;
    return root;
}

QString effectiveSessionRoot() {
    if (!sessionRootOverrideRef().isEmpty())
        return sessionRootOverrideRef();
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

constexpr qint64 kStaleLeaseMs = 24 * 60 * 60 * 1000;  // 24 h lease window
} // namespace

QString TempFileManager::ownershipMarkerName() {
    return QStringLiteral(".glyphpdf-session-owner");
}

bool TempFileManager::writeOwnershipMarker(const QString& dirPath, quint64 pid,
                                           const QString& token, qint64 createdMsUtc)
{
    if (dirPath.isEmpty() || token.isEmpty() || pid == 0 || createdMsUtc <= 0)
        return false;
    QFile marker(dirPath + QLatin1Char('/') + ownershipMarkerName());
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray body =
        QStringLiteral("glyphpdf-session-owner\npid=%1\ntoken=%2\ncreatedMs=%3\n")
            .arg(QString::number(pid), token, QString::number(createdMsUtc))
            .toUtf8();
    if (marker.write(body) != body.size()) {
        marker.close();
        return false;
    }
    marker.close();
    return true;
}

TempFileManager::SessionOwnershipRecord TempFileManager::readOwnershipMarker(const QString& dirPath)
{
    SessionOwnershipRecord rec;
    if (dirPath.isEmpty())
        return rec;
    QFile marker(dirPath + QLatin1Char('/') + ownershipMarkerName());
    if (!marker.open(QIODevice::ReadOnly))
        return rec;  // no/unreadable record: ownership NOT proven
    const QList<QByteArray> lines = marker.readAll().split('\n');
    marker.close();

    if (lines.isEmpty() || lines.first().trimmed() != QByteArrayLiteral("glyphpdf-session-owner"))
        return rec;  // a foreign or corrupted record: ownership NOT proven

    for (const QByteArray& raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.startsWith("pid="))
            rec.pid = line.mid(4).toULongLong();
        else if (line.startsWith("token="))
            rec.token = QString::fromUtf8(line.mid(6));
        else if (line.startsWith("createdMs="))
            rec.createdMs = line.mid(10).toLongLong();
    }
    rec.valid = rec.pid > 0 && !rec.token.isEmpty() && rec.createdMs > 0;
    return rec;
}

bool TempFileManager::isProcessAlive(quint64 pid)
{
    if (pid == 0)
        return false;
    if (pid == static_cast<quint64>(QCoreApplication::applicationPid()))
        return true;
#ifdef Q_OS_WIN
    // Query with minimal rights: SYNCHRONIZE lets us poll the exit state.
    HANDLE h = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                             FALSE, static_cast<DWORD>(pid));
    if (!h) {
        // ACCESS_DENIED means a process with that PID EXISTS but is not
        // inspectable (another user/session) — report ALIVE, never delete.
        return ::GetLastError() == ERROR_ACCESS_DENIED;
    }
    const bool alive = ::WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    ::CloseHandle(h);
    return alive;
#else
    // Signal 0 probes existence; EPERM means it exists and is not ours.
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

void TempFileManager::setSessionRootOverrideForTesting(const QString& rootPath)
{
    sessionRootOverrideRef() = rootPath;
}

// ── Creation ─────────────────────────────────────────────────────────────

QString TempFileManager::createTempFile(const QString& suffix) {
    const QString sessionDir = appTempDir();
    if (sessionDir.isEmpty()) {
        qWarning() << "TempFileManager: private session temp root unavailable — refusing to create temp files in a shared location";
        return {};
    }

    QTemporaryFile tmp(QDir(sessionDir).filePath(QStringLiteral("glyph_XXXXXX") + suffix));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        qWarning() << "TempFileManager: failed to create temp file in" << sessionDir;
        return {};
    }
    QString path = tmp.fileName();
    tmp.close();
    track(path);
    return path;
}

QString TempFileManager::createTempDir(const QString& prefix) {
    const QString sessionDir = appTempDir();
    if (sessionDir.isEmpty()) {
        qWarning() << "TempFileManager: private session temp root unavailable — refusing to create temp dirs in a shared location";
        return {};
    }

    QTemporaryDir tmp(QDir(sessionDir).filePath(prefix + QStringLiteral("_XXXXXX")));
    if (!tmp.isValid()) {
        qWarning() << "TempFileManager: failed to create temp dir in" << sessionDir;
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
    const QDir root(effectiveSessionRoot());
    if (!root.exists()) return;

    // WP-R10: ONLY owned, proven-orphaned session directories are candidates.
    // A directory without a valid ownership record (unrelated content that
    // merely matches the name, a symlink, foreign debris) is NEVER touched —
    // name+age is not ownership. An owned directory is removed only when its
    // owner process is DEAD (liveness) AND its lease is STALE (24 h).
    const auto entries = root.entryInfoList({ QStringLiteral("GlyphPDF-*") },
                                            QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    const QDateTime cutoff = QDateTime::currentDateTime().addMSecs(-kStaleLeaseMs);

    int removed = 0;
    for (const QFileInfo& fi : entries) {
        const QString dirPath = fi.absoluteFilePath();
        const SessionOwnershipRecord rec = readOwnershipMarker(dirPath);
        if (!rec.valid)
            continue;  // ownership not proven — leave it alone

        // Never remove THIS instance's own session directory via the scan.
        if (rec.token == sessionTokenRef())
            continue;

        if (isProcessAlive(rec.pid))
            continue;  // the owner is running — its data stays, however old

        // Dead owner: require a stale lease as well. The lease timestamp is
        // the marker's own record; the directory mtime backs it up in case a
        // marker was written with an unusable clock.
        const bool leaseStale = rec.createdMs < cutoff.toMSecsSinceEpoch()
                                || fi.lastModified() < cutoff;
        if (!leaseStale)
            continue;

        if (QDir(dirPath).removeRecursively())
            ++removed;
    }
    if (removed > 0)
        qDebug() << "TempFileManager: cleaned" << removed
                 << "orphaned owned session directories from previous runs";
}

// ── Static helpers ───────────────────────────────────────────────────────

QString TempFileManager::appTempDir() {
    static QMutex initMutex;
    QMutexLocker lock(&initMutex);
    QString& path = sessionDirRef();
    if (!path.isEmpty()) {
        // Re-create the session if it vanished or the effective root changed
        // (the test seam switches roots between scenarios).
        if (QDir(path).exists() && sessionDirRootRef() == effectiveSessionRoot())
            return path;
        path.clear();
        sessionTokenRef().clear();
        sessionDirRootRef().clear();
    }

    // WP-R10: this instance owns a PRIVATE session directory with an
    // ownership record beside the session data. There is deliberately NO
    // fallback to a shared, unowned directory: if the private root cannot be
    // created we fail closed (empty path, creation APIs report failure).
    const QString root = effectiveSessionRoot();
    if (root.isEmpty()) {
        qWarning() << "TempFileManager: no usable temporary root — failing closed";
        return {};
    }

    const QString token = QUuid::createUuid().toString(QUuid::Id128);
    QTemporaryDir session(root + QStringLiteral("/GlyphPDF-XXXXXX"));
    if (!session.isValid()) {
        qWarning() << "TempFileManager: cannot create private session directory in" << root
                   << "— failing closed";
        return {};
    }
    session.setAutoRemove(false);
    path = session.path();
    sessionTokenRef() = token;
    sessionDirRootRef() = root;

    const qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    if (!writeOwnershipMarker(path, static_cast<quint64>(QCoreApplication::applicationPid()),
                              token, now)) {
        qWarning() << "TempFileManager: cannot write the session ownership record — failing closed";
        path.clear();
        sessionTokenRef().clear();
        return {};
    }

    // The session directory itself is tracked, so a normal exit removes it
    // (the ownership marker is what lets ANOTHER run clean it after a crash).
    instance().track(path);
    return path;
}

static void atexitCleanup() {
    TempFileManager::instance().cleanAll();
}

void TempFileManager::install() {
    // Clean orphaned owned sessions from previous runs (WP-R10: never data of
    // a live owner; never unowned entries).
    instance().cleanStaleTempFiles();
    // Ensure this instance's session exists before any consumer needs it and
    // register the atexit handler for this session.
    instance().appTempDir();
    std::atexit(atexitCleanup);
}
