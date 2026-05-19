#pragma once
#include <QString>
#include "core/interfaces/ISignatureManager.h"
#include "engines/DocumentSession.h"

// Simple callable helper -- signing is irreversible so it does not
// belong on the QUndoStack.
struct SignDocumentHelper {
    static bool execute(ISignatureManager* engine, DocumentSession* doc,
                        const QString& outPath, const QString& certPath,
                        const QString& pwd, const QString& reason,
                        const QString& location)
    {
        if (!engine || !doc || doc->path().isEmpty())
            return false;
        bool ok = engine->signDocument(doc->path(), outPath, certPath,
                                       pwd, reason, location);
        if (ok) doc->markReload();
        return ok;
    }
};
