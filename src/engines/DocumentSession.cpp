#include "engines/DocumentSession.h"

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
        emit dirtyChanged(m_dirty);
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
