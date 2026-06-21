// SPDX-License-Identifier: Apache-2.0
#pragma once

// AR-8 D5: Atomic page-reorder undo command.
// Wraps a single reorderAllPages() call so the entire permutation is one undo
// step rather than N separate disk writes (the previous approach used N calls to
// reorderPages(), each flushing to disk individually — partial-failure risk and
// N separate undo steps).
//
// redo(): apply desiredPermutation   (the user's drag-drop order)
// undo(): apply inversePermutation   (restores the original order)

#include <QUndoCommand>
#include <QList>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class ReorderPermutationCommand : public QUndoCommand {
public:
    // permutation[i] = original 0-based page index that should appear at position i.
    ReorderPermutationCommand(IPdfEditorEngine* engine,
                              DocumentSession* doc,
                              const QList<int>& permutation,
                              QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    // Compute the inverse of a permutation so undo() restores the original order.
    static QList<int> computeInverse(const QList<int>& perm);

    IPdfEditorEngine* m_engine;
    DocumentSession*  m_doc;
    QList<int>        m_desired;  // permutation to apply on redo
    QList<int>        m_inverse;  // permutation to apply on undo
};
