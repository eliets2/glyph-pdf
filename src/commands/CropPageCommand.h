// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QUndoCommand>
#include <QString>
#include <QRectF>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class CropPageCommand : public QUndoCommand {
public:
    CropPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& cropRect, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

    // EC05 introspection (same ownership rules as EditFormFieldCommand: do not
    // dereference a pushed command after a possibly-failed push — inspect via
    // a direct redo() instead).
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession*  m_doc;
    int m_pageIndex;
    QRectF m_cropRect;
    // EC05 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): the effective original box is
    // captured once, before the first mutation, and undo restores it through
    // IPageEditor::cropPage — the same safe mutation boundary as the crop.
    // (An inherited box is restored as an explicit box equal to the MediaBox —
    // geometrically identical, and this writer treats the cases alike.)
    QRectF m_originalBox;
    bool m_haveOriginal = false;
    bool m_succeeded = false;
    QString m_error;
};
