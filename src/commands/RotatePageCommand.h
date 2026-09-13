// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/CheckedHistory.h"

// WP-R03 (WHOLE-ARCHITECTURE-REVIEW A02): rotate used to be a raw
// QUndoCommand whose redo()/undo() ignored the engine result and always
// markReload()ed — a refused rotate was recorded in history as if it had
// happened, and a failed undo moved the index as if the rotation had been
// reverted. The command is now a CheckedUndoCommand at the ONE shared
// boundary:
//   - the initial redo() CHECKS the mutation; a failure sets obsolete(true)
//     so QUndoStack::push deletes the command — history never records a
//     rotation that did not happen (the G08 crop/form/image ownership rule);
//   - CheckedHistory::redo() traversals go through applyChecked(): the
//     rotation is re-applied while the history index is untouched, and only
//     a successful application moves it;
//   - CheckedHistory::undo() traversals go through restoreChecked(): the
//     inverse rotation is applied while the index is untouched, and a failed
//     restoration leaves the command current, the index and the clean state
//     unchanged, and the traversal RETRYABLE.
class RotatePageCommand : public CheckedUndoCommand {
public:
    RotatePageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                      int pageIndex, int degrees)
        : m_engine(engine), m_doc(doc), m_page(pageIndex), m_degrees(degrees) {
        setText(QObject::tr("Rotate page %1 by %2°").arg(pageIndex + 1).arg(degrees));
    }

    void redo() override {
        if (consumeArmedApply())
            return;   // checked traversal already applied; index-move only
        m_succeeded = false;
        m_error.clear();
        if (!applyRotation(m_degrees, &m_error)) {
            setObsolete(true);
            if (m_doc) emit m_doc->mutationFailed(m_error);
            return;
        }
        m_succeeded = true;
        setObsolete(false);
    }

    bool applyChecked() override {
        QString err;
        if (!applyRotation(m_degrees, &err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return false;   // history index unchanged — redo retryable
        }
        armCheckedApply();
        return true;
    }

    bool restoreChecked() override {
        QString err;
        if (!applyRotation(-m_degrees, &err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return false;   // command stays current — undo retryable
        }
        armCheckedRestore();
        return true;
    }

    void undo() override {
        if (consumeArmedRestore())
            return;   // the checked traversal already restored; index-move only
        QString err;
        if (!applyRotation(-m_degrees, &err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return;   // no markReload: the disk still carries the rotation
        }
    }

    int id() const override { return 0x100; }

    bool mergeWith(const QUndoCommand* other) override {
        if (other->id() != id()) return false;
        auto* o = static_cast<const RotatePageCommand*>(other);
        if (o->m_page != m_page) return false;
        m_degrees += o->m_degrees;
        setText(QObject::tr("Rotate page %1 by %2°").arg(m_page + 1).arg(m_degrees));
        return true;
    }

    // Introspection (same ownership rules as CropPageCommand: do not
    // dereference a pushed command after a possibly-failed push — inspect
    // via a direct redo() instead).
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    // The shared rotation body — no reporting, no obsoletion. Marks the
    // session reload only after a rotation that really happened.
    bool applyRotation(int degrees, QString* err) {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) {
            if (err) *err = QObject::tr("Rotate failed: no document is open.");
            return false;
        }
        if (!m_engine->rotatePage(m_doc->path(), m_page, degrees)) {
            if (err) *err = QObject::tr("Rotate of page %1 failed; the document was left unchanged.")
                                 .arg(m_page + 1);
            return false;
        }
        m_doc->markReload();
        return true;
    }

    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    int m_degrees;
    bool m_succeeded = false;
    QString m_error;
};
