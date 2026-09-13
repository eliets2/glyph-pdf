// SPDX-License-Identifier: Apache-2.0
#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include <QSettings>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QPointer>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>     // ::rename
#include <fcntl.h>    // ::open, O_RDONLY, O_DIRECTORY
#include <unistd.h>   // ::fsync, ::close
#endif

static bool atomicRename(const QString &from, const QString &to)
{
#ifdef _WIN32
    std::wstring fromW = from.toStdWString();
    std::wstring toW = to.toStdWString();
    return MoveFileExW(fromW.c_str(), toW.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    // L08 (NATIVE-LINUX-READINESS-2026-09-10): the previous POSIX branch was
    // QFile::remove(to) followed by QFile::rename(from, to) — a non-atomic
    // delete-then-rename. If the process died (or the rename failed) between
    // the two calls, the ONLY recovery artifact was already gone. POSIX
    // rename(2) atomically REPLACES the destination: at every instant the
    // final path resolves to complete old-or-new bytes, never missing or
    // partial.
    //
    // Same-filesystem semantics: from/to live in the same directory by
    // construction (capturedFile + ".autosave.pdf.tmp" -> capturedFile +
    // ".autosave.pdf"), so the rename never crosses a mount point.
    //
    // Durability: fsync the containing directory after the rename so the
    // replacement itself survives sudden power loss (the analogue of
    // MOVEFILE_WRITE_THROUGH on the Windows branch). An fsync failure is
    // logged but does not undo the (already atomic) rename.
    const QByteArray fromName = QFile::encodeName(from);
    const QByteArray toName = QFile::encodeName(to);
    if (::rename(fromName.constData(), toName.constData()) != 0) {
        return false;
    }
    const QByteArray dirName = QFile::encodeName(QFileInfo(to).absolutePath());
    const int dirFd = ::open(dirName.constData(), O_RDONLY
#if defined(O_DIRECTORY)
                             | O_DIRECTORY
#endif
    );
    if (dirFd >= 0) {
        if (::fsync(dirFd) != 0) {
            qWarning() << "AutosaveManager: directory fsync failed after atomic rename of" << to;
        }
        ::close(dirFd);
    }
    return true;
#endif
}

AutosaveManager::AutosaveManager(std::shared_ptr<IPdfEditorEngine> pdfEditor, std::shared_ptr<DocumentSession> document, QObject* parent)
    : QObject(parent)
    , m_pdfEditor(std::move(pdfEditor))
    , m_document(std::move(document))
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &AutosaveManager::onTick);

    // Read initial interval from settings
    QSettings settings;
    int savedInterval = settings.value("autosave/intervalSeconds", 300).toInt();
    // Clamp [60, 1800]
    if (savedInterval < 60) savedInterval = 60;
    if (savedInterval > 1800) savedInterval = 1800;
    m_intervalSeconds = savedInterval;
}

AutosaveManager::~AutosaveManager()
{
    stop();
}

void AutosaveManager::start(int intervalSeconds)
{
    if (intervalSeconds < 60) intervalSeconds = 60;
    if (intervalSeconds > 1800) intervalSeconds = 1800;
    
    m_intervalSeconds = intervalSeconds;
    m_timer->start(m_intervalSeconds * 1000);
}

void AutosaveManager::stop()
{
    m_timer->stop();
}

bool AutosaveManager::isActive() const
{
    return m_timer->isActive();
}

int AutosaveManager::interval() const
{
    return m_intervalSeconds;
}

void AutosaveManager::onTick()
{
    if (m_saving) return;

    if (!m_document || !m_pdfEditor) return;
    if (!m_document->isDirty()) return;

    // EC02: capture the identity this autosave run belongs to — the editor's
    // resident document AND the session's document generation. Everything
    // below (worker paths, rename, timestamp) is validated against this
    // capture, so a document switch that lands while the queued save is
    // pending can never write B's bytes into A's recovery file or timestamp
    // B's session for A's work.
    const QString capturedFile = m_pdfEditor->currentFile();
    if (capturedFile.isEmpty()) return;
    // G04 (QUALITY-GATE-2026-09-09): the path string alone accepts a REPLACED
    // document (A→B→A re-open). The engine-owned resident-load identity is
    // captured with it and validated at the save AND again at the commit, so
    // a stale incarnation can never touch the captured recovery file.
    const qint64 capturedLoadId = m_pdfEditor->documentLoadId();
    const qint64 capturedGeneration = m_document->documentGeneration();

    m_saving = true;
    emit autosaveStarted();

    QString tmpAutosavePath = capturedFile + ".autosave.pdf.tmp";
    QString finalAutosavePath = capturedFile + ".autosave.pdf";

    std::weak_ptr<IPdfEditorEngine> weakEditor = m_pdfEditor;

    // EC02 worker outcome: identity-guarded save classification.
    enum SaveOutcome { SaveFailed = 0, SaveOk = 1, SaveStale = 2 };

    auto watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher, capturedFile, capturedLoadId, capturedGeneration, tmpAutosavePath, finalAutosavePath]() {
        const int result = watcher->result();
        watcher->deleteLater();

        // EC02: the completion may land after a session switch. Identity
        // matching means BOTH the path and the generation captured at tick
        // time — a same-path reopen (A→A) is a new session for which the old
        // run's completion must not claim an autosave.
        const bool sessionMatches = m_document
            && m_document->path() == capturedFile
            && m_document->documentGeneration() == capturedGeneration;
        // G04: carry the identity through the COMMIT as well — the worker
        // wrote the captured incarnation's bytes into the temp, but if the
        // engine has since moved to a different load, promoting the temp over
        // the captured recovery file would overwrite the NEW session's
        // recovery state with stale bytes.
        const bool engineMatches = m_pdfEditor
            && m_pdfEditor->documentLoadId() == capturedLoadId;

        if (result == SaveOk && engineMatches) {
            bool renameOk = atomicRename(tmpAutosavePath, finalAutosavePath);
            if (renameOk) {
                QDateTime now = QDateTime::currentDateTime();
                if (sessionMatches && m_document) {
                    m_document->setLastAutosave(now);
                }
                emit autosaveCompleted(now);
            } else {
                // Retry once after 250ms asynchronously
                QPointer<AutosaveManager> weakThis(this);
                QTimer::singleShot(250, [weakThis, capturedFile, capturedLoadId, capturedGeneration, tmpAutosavePath, finalAutosavePath]() {
                    if (!weakThis) return;
                    const bool retryEngineMatches = weakThis->m_pdfEditor
                        && weakThis->m_pdfEditor->documentLoadId() == capturedLoadId;
                    bool retryOk = retryEngineMatches
                        && atomicRename(tmpAutosavePath, finalAutosavePath);
                    if (retryOk) {
                        QDateTime now = QDateTime::currentDateTime();
                        if (weakThis->m_document
                            && weakThis->m_document->path() == capturedFile
                            && weakThis->m_document->documentGeneration() == capturedGeneration) {
                            weakThis->m_document->setLastAutosave(now);
                        }
                        emit weakThis->autosaveCompleted(now);
                    } else if (!retryEngineMatches) {
                        // G04: the identity moved on — drop the stale temp
                        // instead of promoting it over the new recovery state.
                        QFile::remove(tmpAutosavePath);
                        emit weakThis->autosaveStale(capturedFile);
                    } else {
                        qWarning() << "Autosave failed: atomic rename failed from" << tmpAutosavePath << "to" << finalAutosavePath;
                        emit weakThis->autosaveFailed("Failed to rename temporary autosave file");
                    }
                    weakThis->m_saving = false;
                });
                return; // Return early, m_saving = false will be handled in the timer
            }
        } else if (result == SaveOk && !engineMatches) {
            // G04: the save succeeded but the engine moved to a different
            // resident load before the commit — the temp holds the OLD
            // incarnation's bytes and must never overwrite the captured
            // recovery file. Terminate as stale.
            QFile::remove(tmpAutosavePath);
            emit autosaveStale(capturedFile);
        } else if (result == SaveStale) {
            // EC02: the resident document changed between capture and save —
            // the engine refused WITHOUT writing. Clear any leftover temp and
            // terminate with a clear stale outcome; neither session is
            // timestamped and the captured document's recovery file is
            // untouched.
            QFile::remove(tmpAutosavePath);
            emit autosaveStale(capturedFile);
        } else {
            qWarning() << "Autosave failed during document save";
            emit autosaveFailed("Failed to save temporary document");
        }
        m_saving = false;
    });

    QFuture<int> future = QtConcurrent::run([weakEditor, capturedFile, capturedLoadId, tmpAutosavePath]() -> int {
        auto editor = weakEditor.lock();
        if (!editor) return SaveFailed;
        try {
            // EC02/G04: identity-guarded save — the engine checks under its
            // serialization lock that the document it holds is still the one
            // captured for this run (path AND resident-load identity), so a
            // switch — or a same-path RELOAD — cannot be serialized into the
            // captured path. A `false` with a changed resident identity is a
            // stale run, not a save failure.
            if (!editor->saveDocumentIfCurrent(capturedFile, capturedLoadId, tmpAutosavePath)) {
                const bool identityCurrent = editor->currentFile() == capturedFile
                    && editor->documentLoadId() == capturedLoadId;
                return identityCurrent ? SaveFailed : SaveStale;
            }
            return SaveOk;
        } catch (const std::exception &e) {
            qWarning("Autosave failed: %s", e.what());
            return SaveFailed;
        } catch (...) {
            qWarning("Autosave failed with unknown exception");
            return SaveFailed;
        }
    });
    watcher->setFuture(future);
}
