// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>
#include <QMutex>

/**
 * Centralized temp file tracking for GlyphPDF.
 *
 * All engine code that creates temporary files should register them here.
 * On normal exit the atexit() handler cleans up; on crash the OS handles
 * the system temp dir.
 *
 * WP-R10 (WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A06): startup cleanup used to
 * delete every GlyphPDF-* / glyph_* entry older than 24 hours from the system
 * temp directory — name+age treated as ownership. A SECOND instance could
 * therefore delete an OLDER but STILL-ACTIVE session's working files
 * (autosaves, candidates). Cleanup is now ownership-proven:
 *
 *  - every instance creates its OWN private session directory and writes an
 *    ownership record beside the session data (PID + session token + lease
 *    timestamp — see writeOwnershipMarker);
 *  - startup cleanup only considers session directories carrying a VALID
 *    ownership record, and removes one only when the owner process is dead
 *    (liveness check) AND the lease is stale (older than the 24h window);
 *  - entries without a valid record (unrelated or foreign content that
 *    merely matches the name) are NEVER touched;
 *  - when the private session root cannot be created the manager fails
 *    CLOSED: appTempDir() returns an empty path and the creation APIs report
 *    failure instead of falling back to a shared, unowned directory.
 *
 * Singleton — thread-safe.
 */
class TempFileManager {
public:
    struct SessionOwnershipRecord {
        bool valid = false;
        quint64 pid = 0;
        QString token;
        qint64 createdMs = 0;   // UTC msecs since epoch
    };

    static TempFileManager& instance();

    // Register a file or directory to be cleaned on exit
    void track(const QString& path);
    void untrack(const QString& path);

    // Create a tracked temp file in the app's temp directory
    // Returns the path; the file is created but empty. Returns an EMPTY path
    // when the private session root is unavailable (fail closed).
    QString createTempFile(const QString& suffix = ".tmp");

    // Create a tracked temp directory. Empty path on failure (fail closed).
    QString createTempDir(const QString& prefix = "glyphpdf");

    // Clean all tracked paths right now
    void cleanAll();

    // Clean orphaned OWNED session directories from previous runs (called
    // once at startup). Only directories with a valid ownership record whose
    // owner process is dead AND whose lease is stale are removed.
    void cleanStaleTempFiles();

    // Install the atexit() handler — call once in main()
    static void install();

    // App-specific session temp directory (inside the system temp).
    // Empty when the private root cannot be created (fail closed).
    static QString appTempDir();

    // ── WP-R10 ownership records ────────────────────────────────────────────
    // The marker is written INSIDE a session directory and names its owner.
    static QString ownershipMarkerName();
    static bool writeOwnershipMarker(const QString& dirPath, quint64 pid,
                                     const QString& token, qint64 createdMsUtc);
    static SessionOwnershipRecord readOwnershipMarker(const QString& dirPath);

    // Liveness of a recorded owner PID. Conservative: when existence cannot
    // be determined (access denied), the process is reported ALIVE so cleanup
    // never deletes data of a possibly-running owner.
    static bool isProcessAlive(quint64 pid);

    // Test seam: operate on an isolated root instead of the system temporary
    // directory (session creation AND cleanup). Empty string restores the
    // default. Never set in production.
    static void setSessionRootOverrideForTesting(const QString& rootPath);

private:
    TempFileManager() = default;
    ~TempFileManager();
    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator=(const TempFileManager&) = delete;

    QMutex      m_mutex;
    QStringList m_tracked;
};
