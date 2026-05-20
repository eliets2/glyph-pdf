#include "ReorderPageCommand.h"

ReorderPageCommand::ReorderPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int fromIndex, int toIndex, QUndoCommand* parent)
    : QUndoCommand(parent), m_engine(engine), m_doc(doc), m_fromIndex(fromIndex), m_toIndex(toIndex)
{
    setText(QObject::tr("Reorder Page %1 to %2").arg(fromIndex + 1).arg(toIndex + 1));
}

void ReorderPageCommand::undo()
{
    if (m_engine && m_doc) {
        // To undo, reverse the move: account for the index shift from redo.
        int actualFromIndex = m_toIndex;
        int actualToIndex   = m_fromIndex;

        if (m_fromIndex < m_toIndex) {
            actualFromIndex = m_toIndex - 1;
        } else if (m_fromIndex > m_toIndex) {
            actualToIndex = m_fromIndex + 1;
        }

        m_engine->reorderPages(m_doc->path(), actualFromIndex, actualToIndex);
        m_doc->markReload();
    }
}

void ReorderPageCommand::redo()
{
    if (m_engine && m_doc) {
        m_engine->reorderPages(m_doc->path(), m_fromIndex, m_toIndex);
        m_doc->markReload();
    }
}
