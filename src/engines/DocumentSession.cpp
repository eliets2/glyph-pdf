// SPDX-License-Identifier: Apache-2.0
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

void DocumentSession::beginDocument(const QString &path)
{
    // ARC01: unconditional — a same-path reopen (A→A) is a NEW document
    // identity loaded from disk; nothing about the previous session (dirty
    // state, autosave stamp, generation-scoped history) carries over.
    ++m_documentGeneration;
    m_path = path;
    m_dirty = false;
    m_lastAutosave = QDateTime();
    m_recoverySource.clear();   // G05: a fresh open is never a recovery
    emit dirtyChanged(m_dirty);
    emit lastAutosaveChanged(m_lastAutosave);
}

qint64 DocumentSession::documentGeneration() const
{
    return m_documentGeneration;
}

void DocumentSession::markReload() {
    // V05: every markReload() is a successful mutation/reload boundary (the
    // mutate-commands call it on redo AND undo) — the document's content
    // identity changes even when path and page count do not. Advances
    // unconditionally: two successive reloads are two distinct contents.
    ++m_mutationRevision;
    m_dirty = true;
    emit dirtyChanged(m_dirty);
    emit reloadRequested();
}

qint64 DocumentSession::mutationRevision() const {
    return m_mutationRevision;
}

// G05 (QUALITY-GATE-2026-09-09): see DocumentSession.h.
QString DocumentSession::recoverySource() const {
    return m_recoverySource;
}

void DocumentSession::setRecoverySource(const QString &autosaveInputPath) {
    m_recoverySource = autosaveInputPath;
}

void DocumentSession::clearRecoverySource() {
    m_recoverySource.clear();
}

bool DocumentSession::isDirty() const {
    return m_dirty;
}

// ARC07: the one editability switch. Change-only signalling so consumers
// (viewer tool gate, action enablement) can re-sync cheaply.
void DocumentSession::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly)
        return;
    m_readOnly = readOnly;
    emit readOnlyChanged(m_readOnly);
}

void DocumentSession::setClean() {
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged(m_dirty);
    }
}

void DocumentSession::markDirty() {
    // V05: advances UNCONDITIONALLY — outside the dirty guard — because a
    // second edit while already dirty is still a new content mutation.
    ++m_mutationRevision;
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
