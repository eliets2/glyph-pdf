// SPDX-License-Identifier: Apache-2.0
#include "ReorderPermutationCommand.h"

ReorderPermutationCommand::ReorderPermutationCommand(IPdfEditorEngine* engine,
                                                     DocumentSession*  doc,
                                                     const QList<int>& permutation,
                                                     QUndoCommand*     parent)
    : QUndoCommand(parent)
    , m_engine(engine)
    , m_doc(doc)
    , m_desired(permutation)
    , m_inverse(computeInverse(permutation))
{
    setText(QObject::tr("Reorder pages (%1 pages)").arg(permutation.size()));
}

void ReorderPermutationCommand::redo()
{
    if (m_engine && m_doc) {
        m_engine->reorderAllPages(m_doc->path(), m_desired);
        m_doc->markReload();
    }
}

void ReorderPermutationCommand::undo()
{
    if (m_engine && m_doc) {
        m_engine->reorderAllPages(m_doc->path(), m_inverse);
        m_doc->markReload();
    }
}

QList<int> ReorderPermutationCommand::computeInverse(const QList<int>& perm)
{
    QList<int> inv(perm.size(), 0);
    for (int i = 0; i < perm.size(); ++i)
        inv[perm[i]] = i;
    return inv;
}
