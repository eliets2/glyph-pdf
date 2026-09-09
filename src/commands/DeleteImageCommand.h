// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/CheckedHistory.h"

class DeleteImageCommand : public CheckedUndoCommand {
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
    // G08 (QUALITY-GATE-2026-09-09): the restoration is ONE committed step
    // (IPageEditor::restorePageFromBytes — insert the backup page and remove
    // the displaced edited page inside a single engine transaction) and a
    // CHECKED traversal: restoreChecked() restores while the history index is
    // untouched; a FAILED restoration reports mutationFailed and leaves this
    // command current, the index and the clean state unchanged, and the
    // traversal RETRYABLE. The pre-G08 EC03 contract is preserved
    // structurally: a refused restoration can never touch the page at
    // m_page+1 (the original following page) — no mutation is committed at
    // all unless the whole restoration succeeded.
    bool restoreChecked() override {
        if (!performRestore()) {
            if (!m_doc) return false;
            qWarning() << "DeleteImageCommand::undo failed for" << m_name;
            emit m_doc->mutationFailed(
                QObject::tr("Undo of image %1 deletion failed: the page could not be restored; nothing was changed.")
                    .arg(m_name));
            return false;
        }
        armCheckedRestore();
        return true;
    }

    void undo() override {
        if (consumeArmedRestore())
            return;   // the checked traversal already restored; index-move only
        if (!m_engine || !m_doc)
            return;
        if (performRestore())
            return;
        emit m_doc->mutationFailed(
            QObject::tr("Undo of image %1 deletion failed: the page could not be restored; nothing was changed.")
                .arg(m_name));
    }
    int id() const override { return 0x114; }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    QByteArray m_backup;

    // G08: the shared restoration body — one committed step, no reporting.
    bool performRestore() {
        if (!m_engine || !m_doc) return false;
        if (!m_engine->restorePageFromBytes(m_doc->path(), m_page, m_backup))
            return false;
        m_doc->markReload();
        return true;
    }
};
