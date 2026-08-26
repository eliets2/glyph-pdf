// SPDX-License-Identifier: Apache-2.0
#pragma once

// §9.9 P0 (audit 2026-07-01): RETIRED — no longer compiled into the build.
// Superseded by ReorderPermutationCommand (atomic single-write reorder);
// PagesController::onPageReordered now builds a permutation from the swap and
// pushes the atomic command. Delete this file after human review.

#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class ReorderPageCommand : public QUndoCommand {
public:
    ReorderPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int fromIndex, int toIndex, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    IPdfEditorEngine* m_engine;
    DocumentSession*  m_doc;
    int m_fromIndex;
    int m_toIndex;
};
