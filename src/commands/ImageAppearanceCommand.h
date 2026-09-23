// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/CheckedHistory.h"

// Stacking order (bring to front / send to back) and constant opacity for one
// image placement. Undo restores the page from the backup taken before the
// edit — the same single-step, checked restoration DeleteImageCommand uses
// (G08), because neither edit has an exact inverse once other content moved.
class ImageAppearanceCommand : public CheckedUndoCommand {
public:
    enum class Kind { BringToFront, SendToBack, Opacity };

    ImageAppearanceCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                           int pageIndex, const QString& xobjectName, Kind kind,
                           double opacity, const QByteArray& pageBackup)
        : m_engine(engine), m_doc(doc), m_page(pageIndex), m_name(xobjectName),
          m_kind(kind), m_opacity(opacity), m_backup(pageBackup) {
        switch (kind) {
        case Kind::BringToFront:
            setText(QObject::tr("Bring image %1 to front").arg(xobjectName));
            break;
        case Kind::SendToBack:
            setText(QObject::tr("Send image %1 to back").arg(xobjectName));
            break;
        case Kind::Opacity:
            setText(QObject::tr("Set image %1 opacity to %2%")
                        .arg(xobjectName).arg(qRound(opacity * 100)));
            break;
        }
    }

    bool applyChecked() override {
        QString err;
        if (!performApply(&err)) {
            if (!m_doc) return false;
            emit m_doc->mutationFailed(err);
            return false;
        }
        armCheckedApply();
        return true;
    }

    void redo() override {
        if (consumeArmedApply())
            return;   // checked traversal already applied; index-move only
        QString err;
        if (!performApply(&err)) {
            // A failed mutation must not become an undoable step (QUndoStack
            // deletes a command that is obsolete after its redo()).
            setObsolete(true);
            if (m_doc && !err.isEmpty())
                emit m_doc->mutationFailed(err);
            return;
        }
        setObsolete(false);
    }

    bool restoreChecked() override {
        if (!performRestore()) {
            if (!m_doc) return false;
            emit m_doc->mutationFailed(restoreFailedMessage());
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
        if (!performRestore())
            emit m_doc->mutationFailed(restoreFailedMessage());
    }

    int id() const override { return 0x115; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    Kind m_kind;
    double m_opacity;
    QByteArray m_backup;

    QString restoreFailedMessage() const {
        return QObject::tr("Undo of \"%1\" failed: the page could not be restored; nothing was changed.")
            .arg(text());
    }

    bool performRestore() {
        if (!m_engine || !m_doc) return false;
        if (!m_engine->restorePageFromBytes(m_doc->path(), m_page, m_backup))
            return false;
        m_doc->markReload();
        return true;
    }

    bool performApply(QString* err) {
        if (!m_engine || !m_doc) {
            if (err) *err = QObject::tr("\"%1\" failed: no document.").arg(text());
            return false;
        }
        // Never start the edit without a restorable backup (EC03): undo
        // replaces the page from it.
        if (m_backup.isEmpty()) {
            if (err) *err = QObject::tr("\"%1\" was refused: the page backup is missing; nothing was changed.")
                                .arg(text());
            return false;
        }
        const bool ok = m_kind == Kind::Opacity
            ? m_engine->setImageOpacity(m_page, m_name, m_opacity)
            : m_engine->setImageZOrder(m_page, m_name, m_kind == Kind::BringToFront);
        if (!ok) {
            if (err) {
                const QString why = m_engine->lastError().userMessage;
                *err = why.isEmpty()
                    ? QObject::tr("\"%1\" failed; the document was left unchanged.").arg(text())
                    : why;
            }
            return false;
        }
        m_doc->markReload();
        return true;
    }
};
