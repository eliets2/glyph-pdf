// SPDX-License-Identifier: Apache-2.0
#include "CropPageCommand.h"

CropPageCommand::CropPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& cropRect, QUndoCommand* parent)
    : QUndoCommand(parent), m_engine(engine), m_doc(doc), m_pageIndex(pageIndex), m_cropRect(cropRect)
{
    setText(QObject::tr("Crop Page %1").arg(pageIndex + 1));
}

void CropPageCommand::redo()
{
    // EC05 (TEAM-ENGINE-CODE-REVIEW-2026-09-07): the old command mutated and
    // announced a reload unconditionally, and its undo was a no-op reload.
    // The restored command refuses to run when the geometry cannot be read or
    // the mutation fails — setObsolete(true) after the initial redo() makes
    // QUndoStack::push delete the command (Qt 5.15 through 6.x, not a 6.11
    // novelty), so a failed mutation never becomes an undoable step (the
    // same ownership rule EditFormFieldCommand documents).
    m_succeeded = false;
    if (!m_engine || !m_doc || m_doc->path().isEmpty()) {
        m_error = QObject::tr("Crop failed: no document is open.");
        setObsolete(true);
        if (m_doc) emit m_doc->mutationFailed(m_error);
        return;
    }
    if (!m_haveOriginal) {
        // Capture the EFFECTIVE original geometry BEFORE the first mutation,
        // exactly once, so undo can restore it through the engine boundary.
        bool ok = false;
        const QRectF box = m_engine->pageCropBox(m_doc->path(), m_pageIndex, &ok);
        if (!ok) {
            m_error = QObject::tr("Crop failed: page %1 geometry could not be read; nothing was changed.")
                          .arg(m_pageIndex + 1);
            setObsolete(true);
            emit m_doc->mutationFailed(m_error);
            return;
        }
        m_originalBox = box;
        m_haveOriginal = true;
    }
    if (!m_engine->cropPage(m_doc->path(), m_pageIndex, m_cropRect)) {
        m_error = QObject::tr("Crop of page %1 failed; the document was left unchanged.")
                      .arg(m_pageIndex + 1);
        setObsolete(true);
        emit m_doc->mutationFailed(m_error);
        return;
    }
    m_succeeded = true;
    setObsolete(false);
    m_doc->markReload();
}

void CropPageCommand::undo()
{
    // EC05: a real restoration through the same safe mutation boundary — the
    // captured original geometry is written back to the document, not a
    // viewer-only reload that leaves the on-disk CropBox cropped.
    if (!m_engine || !m_doc || !m_haveOriginal)
        return;
    if (!m_engine->cropPage(m_doc->path(), m_pageIndex, m_originalBox)) {
        emit m_doc->mutationFailed(
            QObject::tr("Undo of the crop failed on page %1; the cropped geometry is still in effect.")
                .arg(m_pageIndex + 1));
        return;   // no markReload: disk and viewer still show the cropped state
    }
    m_doc->markReload();
}
