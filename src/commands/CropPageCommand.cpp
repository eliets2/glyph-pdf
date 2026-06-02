// SPDX-License-Identifier: Apache-2.0
#include "CropPageCommand.h"

CropPageCommand::CropPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& cropRect, QUndoCommand* parent)
    : QUndoCommand(parent), m_engine(engine), m_doc(doc), m_pageIndex(pageIndex), m_cropRect(cropRect)
{
    setText(QObject::tr("Crop Page %1").arg(pageIndex + 1));
}

void CropPageCommand::undo()
{
    // A robust implementation would restore the previous CropBox stored before redo().
    // Deferred until IPdfEditorEngine exposes getCropBox(). For now this is a no-op.
    if (m_doc) m_doc->markReload();
}

void CropPageCommand::redo()
{
    if (m_engine && m_doc) {
        m_engine->cropPage(m_doc->path(), m_pageIndex, m_cropRect);
        m_doc->markReload();
    }
}
