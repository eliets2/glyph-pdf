#pragma once
#include <QString>
#include <QDebug>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

// Simple callable helper -- encryption is irreversible so it does not
// belong on the QUndoStack.
struct EncryptDocumentHelper {
    static bool execute(IPdfEditorEngine* engine, DocumentSession* doc,
                        const QString& userPwd, const QString& ownerPwd,
                        bool canPrint, bool canCopy, bool canModify)
    {
        if (!engine || !doc || doc->path().isEmpty())
            return false;
        engine->loadDocumentForEditing(doc->path());
        if (!engine->encryptDocument(userPwd, ownerPwd, canPrint, canCopy, canModify)) {
            qCritical() << "SECURITY: Document encryption routine failed. Aborting save operation.";
            return false;
        }
        bool ok = engine->saveDocument(doc->path());
        if (ok) doc->markReload();
        return ok;
    }
};
