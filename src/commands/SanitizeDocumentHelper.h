// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QFileInfo>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

// Simple callable helper -- sanitization is irreversible so it does not
// belong on the QUndoStack.
struct SanitizeDocumentHelper {
    // Legacy all-or-nothing commit: exactly "all categories" (the default).
    // Routes through the one-argument engine entry so legacy callers keep the
    // exact legacy path (and its observed counters).
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

    // Selective commit (PARITY-SCORECARD-2026-09-30 §4 #4): removes ONLY the
    // categories in `selected`; `removedOut` (optional) is the proof-carrying
    // per-category result so the caller can compare outcome vs the summary the
    // user approved. An empty selection refuses honestly — removing nothing is
    // a no-op, not a sanitize.
    static bool execute(IPdfEditorEngine* engine, DocumentSession* doc,
                        const QString& outPath, SanitizeCategories selected,
                        SanitizePlan* removedOut = nullptr)
    {
        if (!engine || !doc || doc->path().isEmpty() || outPath.isEmpty())
            return false;
        if (!selected)
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

        return engine->sanitizeDocument(outPath, selected, removedOut);
    }

    // Classify-only (§4 #4): loads the session document and reports what a
    // sanitize WOULD remove, per category, without mutating anything. The plan
    // feeds the pre-commit summary dialog.
    static SanitizePlan classify(IPdfEditorEngine* engine, DocumentSession* doc)
    {
        if (!engine || !doc || doc->path().isEmpty())
            return {};
        if (!engine->loadDocumentForEditing(doc->path()))
            return {};
        return engine->sanitizeClassify();
    }
};
