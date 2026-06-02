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
 * the system temp dir, but we also clean stale files from previous runs.
 *
 * Singleton — thread-safe.
 */
class TempFileManager {
public:
    static TempFileManager& instance();

    // Register a file or directory to be cleaned on exit
    void track(const QString& path);
    void untrack(const QString& path);

    // Create a tracked temp file in the app's temp directory
    // Returns the path; the file is created but empty.
    QString createTempFile(const QString& suffix = ".tmp");

    // Create a tracked temp directory
    QString createTempDir(const QString& prefix = "glyphpdf");

    // Clean all tracked paths right now
    void cleanAll();

    // Clean stale files from previous sessions (called once at startup)
    void cleanStaleTempFiles();

    // Install the atexit() handler — call once in main()
    static void install();

    // App-specific temp directory (inside system temp)
    static QString appTempDir();

private:
    TempFileManager() = default;
    ~TempFileManager();
    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator=(const TempFileManager&) = delete;

    QMutex      m_mutex;
    QStringList m_tracked;
};
