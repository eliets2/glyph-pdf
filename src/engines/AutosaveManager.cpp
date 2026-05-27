#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include <QSettings>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#ifdef _WIN32
#include <windows.h>
#endif

static bool atomicRename(const QString &from, const QString &to)
{
#ifdef _WIN32
    std::wstring fromW = from.toStdWString();
    std::wstring toW = to.toStdWString();
    return MoveFileExW(fromW.c_str(), toW.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (QFile::exists(to)) {
        QFile::remove(to);
    }
    return QFile::rename(from, to);
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

    QString currentFile = m_pdfEditor->currentFile();
    if (currentFile.isEmpty()) return;

    m_saving = true;
    emit autosaveStarted();

    QString tmpAutosavePath = currentFile + ".autosave.pdf.tmp";
    QString finalAutosavePath = currentFile + ".autosave.pdf";

    std::weak_ptr<IPdfEditorEngine> weakEditor = m_pdfEditor;
    
    auto watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, tmpAutosavePath, finalAutosavePath]() {
        bool success = watcher->result();
        watcher->deleteLater();

        if (success) {
            bool renameOk = atomicRename(tmpAutosavePath, finalAutosavePath);
            if (!renameOk) {
                // Retry once after 250ms
                QThread::msleep(250);
                renameOk = atomicRename(tmpAutosavePath, finalAutosavePath);
            }

            if (renameOk) {
                QDateTime now = QDateTime::currentDateTime();
                if (m_document) {
                    m_document->setLastAutosave(now);
                }
                emit autosaveCompleted(now);
            } else {
                qWarning() << "Autosave failed: atomic rename failed from" << tmpAutosavePath << "to" << finalAutosavePath;
                emit autosaveFailed("Failed to rename temporary autosave file");
            }
        } else {
            qWarning() << "Autosave failed during document save";
            emit autosaveFailed("Failed to save temporary document");
        }
        m_saving = false;
    });

    QFuture<bool> future = QtConcurrent::run([weakEditor, tmpAutosavePath]() -> bool {
        auto editor = weakEditor.lock();
        if (!editor) return false;
        try {
            return editor->saveDocument(tmpAutosavePath);
        } catch (const std::exception &e) {
            qWarning("Autosave failed: %s", e.what());
            return false;
        } catch (...) {
            qWarning("Autosave failed with unknown exception");
            return false;
        }
    });
    watcher->setFuture(future);
}
