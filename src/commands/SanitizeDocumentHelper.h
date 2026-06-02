// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QFileInfo>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

// Simple callable helper -- sanitization is irreversible so it does not
// belong on the QUndoStack.
struct SanitizeDocumentHelper {
    static bool execute(IPdfEditorEngine* engine, DocumentSession* doc,
                        const QString& outPath)
    {
        if (!engine || !doc || doc->path().isEmpty() || outPath.isEmpty())
            return false;

        const QFileInfo sourceInfo(doc->path());
        const QFileInfo outputInfo(outPath);
        const QString sourcePath = sourceInfo.canonicalFilePath().isEmpty()
            ? sourceInfo.absoluteFilePath()
            : sourceInfo.canonicalFilePath();
        const QString outputPath = outputInfo.canonicalFilePath().isEmpty()
            ? outputInfo.absoluteFilePath()
            : outputInfo.canonicalFilePath();
        if (QString::compare(sourcePath, outputPath, Qt::CaseInsensitive) == 0)
            return false;

        if (!engine->loadDocumentForEditing(doc->path()))
            return false;

        return engine->sanitizeDocument(outPath);
    }
};
