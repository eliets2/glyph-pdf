#include "engines/DocumentSession.h"
#include <QFileInfo>

DocumentSession::DocumentSession(QObject *parent)
    : QObject(parent)
{
}

QString DocumentSession::path() const {
    return m_path;
}

void DocumentSession::setPath(const QString &path) {
    if (m_path != path) {
        m_path = path;
        m_dirty = false;
        m_lastAutosave = QDateTime();
        emit dirtyChanged(m_dirty);
        emit lastAutosaveChanged(m_lastAutosave);
    }
}

void DocumentSession::markReload() {
    m_dirty = true;
    emit dirtyChanged(m_dirty);
    emit reloadRequested();
}

bool DocumentSession::isDirty() const {
    return m_dirty;
}

void DocumentSession::setClean() {
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged(m_dirty);
    }
}

void DocumentSession::markDirty() {
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged(m_dirty);
    }
}

QDateTime DocumentSession::lastAutosave() const {
    return m_lastAutosave;
}

void DocumentSession::setLastAutosave(const QDateTime &time) {
    m_lastAutosave = time;
    emit lastAutosaveChanged(m_lastAutosave);
}

QStringList DocumentSession::findOrphanedAutosaves(const QStringList &recentFiles) {
    QStringList orphans;
    for (const auto &file : recentFiles) {
        if (file.isEmpty()) continue;
        QString autosavePath = file + ".autosave.pdf";
        QFileInfo originalInfo(file);
        QFileInfo autosaveInfo(autosavePath);
        if (autosaveInfo.exists()) {
            if (!originalInfo.exists() || autosaveInfo.lastModified() > originalInfo.lastModified()) {
                orphans.append(file);
            }
        }
    }
    return orphans;
}
