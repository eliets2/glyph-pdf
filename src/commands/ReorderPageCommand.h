#pragma once

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
