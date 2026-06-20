// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include "core/interfaces/ISignatureManager.h"
#include "engines/DocumentSession.h"

// Simple callable helper -- signing is irreversible so it does not
// belong on the QUndoStack.
struct SignDocumentHelper {
    static SignOutcome execute(ISignatureManager* engine, DocumentSession* doc,
                        const QString& outPath, const QString& certPath,
                        const QString& pwd, const QString& reason,
                        const QString& location)
    {
        if (!engine || !doc || doc->path().isEmpty())
            return SignOutcome::Failed;
        SignOutcome ok = engine->signDocument(doc->path(), outPath, certPath,
                                       pwd, reason, location);
        if (ok != SignOutcome::Failed) doc->markReload();
        return ok;
    }
};
