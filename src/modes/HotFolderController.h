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
#ifndef GLYPHPDF_HOTFOLDERCONTROLLER_H
#define GLYPHPDF_HOTFOLDERCONTROLLER_H

#include <functional>

#include <QFileInfo>
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

    explicit HotFolderController(QObject* parent = nullptr);
    ~HotFolderController() override;

    // Files discovered by an ingest pass. Fired ONLY when a pass found
    // something new (the historical contract: an empty pass stays silent).
    void setIngestHandler(std::function<void(const QStringList& files)> handler) {
        m_ingestHandler = std::move(handler);
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
    // does (watch refresh + debounce restart), without OS-event timing.
    void triggerDirectoryChangedForTest() { onDirectoryChanged(); }
    // Debounce passes delivered so far (trigger→one-pass wiring pins).
    int debouncePassesForTest() const { return m_debouncePasses; }
    // The directories currently under QFileSystemWatcher watch.
    QStringList watchedDirectoriesForTest() const;

private slots:
    void onDirectoryChanged();

private:
    void ensureDebounce();
    void seedProcessed();
    QStringList scanPdfFiles() const;

    static QString hotFileKey(const QFileInfo& fi);

    QString m_dir;
    QSet<QString> m_processed;          // already-seen file keys
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_debounce = nullptr;       // single-shot kDebounceMs
    QTimer* m_pollTimer = nullptr;      // polling fallback (network shares)
    std::function<void(const QStringList&)> m_ingestHandler;
    int m_debouncePasses = 0;
};

} // namespace gp
#endif // GLYPHPDF_HOTFOLDERCONTROLLER_H
