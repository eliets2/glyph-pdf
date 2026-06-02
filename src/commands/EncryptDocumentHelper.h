// SPDX-License-Identifier: Apache-2.0
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
                        const DocumentPermissions& perms)
    {
        if (!engine || !doc || doc->path().isEmpty())
            return false;
        engine->loadDocumentForEditing(doc->path());
        if (!engine->encryptDocument(userPwd, ownerPwd, perms)) {
            qCritical() << "SECURITY: Document encryption routine failed. Aborting save operation.";
            // Reset engine state by reloading the original file. This prevents a
            // subsequent save from persisting a partially-encrypted in-memory
            // document that the user believes is encrypted.
            engine->loadDocumentForEditing(doc->path());
            return false;
        }
        bool ok = engine->saveDocument(doc->path());
        if (ok) doc->markReload();
        return ok;
    }
};
