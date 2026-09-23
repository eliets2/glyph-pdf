// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QUndoCommand>
#include <QString>
#include <QRectF>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/CheckedHistory.h"

class CropPageCommand : public CheckedUndoCommand {
public:
    CropPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& cropRect, QUndoCommand* parent = nullptr);

    // G08 (QUALITY-GATE-2026-09-09): checked traversal — the restoration is
    // attempted while the history position is still untouched; a failed
    // restore leaves this command current (retryable, index/clean unchanged).
    bool restoreChecked() override;

    // WP-R03 (WHOLE-ARCHITECTURE-REVIEW A02): checked APPLY traversal — the
    // mutation is re-applied while the history position is still untouched;
    // a failed application leaves this command where it is (retryable,
    // index/clean unchanged).
    bool applyChecked() override;

    void undo() override;
    void redo() override;

    // EC05 introspection (same ownership rules as EditFormFieldCommand: do not
    // dereference a pushed command after a possibly-failed push — inspect via
    // a direct redo() instead).
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    // G07: perform the restoration (explicit box rewrite, or removal of the
    // explicit key to re-expose inherited/absent semantics). No reporting.
    bool restoreOriginal();

    // WP-R03: the shared mutation body (snapshot + crop). No reporting.
    bool performMutation(QString* err);
    IPdfEditorEngine* m_engine;
    DocumentSession*  m_doc;
    int m_pageIndex;
    QRectF m_cropRect;
    // EC05 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): the effective original box is
    // captured once, before the first mutation, and undo restores it through
    // the safe mutation boundary.
    // G07 (QUALITY-GATE-2026-09-09): the snapshot also records HOW the box was
    // determined, and undo restores that semantic — an explicit box is written
    // back as the same explicit box; an INHERITED or ABSENT box restores the
    // original semantics by removing the page's explicit /CropBox again (the
    // inherited box — not the MediaBox — shows through, or true absence).
    QRectF m_originalBox;
    int    m_originalOrigin = IPdfEditorEngine::kCropBoxAbsent;
    bool m_haveOriginal = false;
    bool m_succeeded = false;
    QString m_error;
};
