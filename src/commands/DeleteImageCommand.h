// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class DeleteImageCommand : public QUndoCommand {
public:
    DeleteImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                       int pageIndex, const QString& xobjectName,
                       const QByteArray& pageBackup)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_backup(pageBackup) {
        setText(QObject::tr("Delete image %1").arg(xobjectName));
    }
    void redo() override {
        if (!m_engine || !m_doc) { setObsolete(true); return; }
        // EC03 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): never start the
        // destructive edit without a restorable backup — undo replaces the
        // page from this backup, and an empty one could never be restored.
        if (m_backup.isEmpty()) {
            setObsolete(true);
            emit m_doc->mutationFailed(
                QObject::tr("Deleting image %1 was refused: the page backup is missing; nothing was changed.")
                    .arg(m_name));
            return;
        }
        if (!m_engine->deleteImage(m_page, m_name)) {
            // A failed mutation must not become an undoable step (QUndoStack
            // push() — Qt 5.15 through 6.x, not a 6.11 novelty — deletes a
            // command that is obsolete after its redo()).
            setObsolete(true);
            emit m_doc->mutationFailed(
                QObject::tr("Deleting image %1 failed; the document was left unchanged.")
                    .arg(m_name));
            return;
        }
        m_doc->markReload();
    }
    void undo() override {
        if (!m_engine || !m_doc) return;
        // EC03: restore = insert the backup page, then remove the displaced
        // edited page. A FAILED insertion means the page at m_page+1 is the
        // ORIGINAL FOLLOWING page — deleting it would destroy unrelated
        // content. Stop and report instead of advancing the destruction.
        if (!m_engine->insertPageFromBytes(m_doc->path(), m_page, m_backup)) {
            emit m_doc->mutationFailed(
                QObject::tr("Undo of image %1 deletion failed: the page could not be restored; no page was removed.")
                    .arg(m_name));
            return;
        }
        if (!m_engine->deletePage(m_doc->path(), m_page + 1)) {
            // Insertion succeeded but the displaced page could not be removed:
            // the document now carries the restored page — reload truthfully.
            emit m_doc->mutationFailed(
                QObject::tr("Undo of image %1 deletion is incomplete: the page was restored but the edited copy could not be removed.")
                    .arg(m_name));
            m_doc->markReload();
            return;
        }
        m_doc->markReload();
    }
    int id() const override { return 0x114; }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    QByteArray m_backup;
};
