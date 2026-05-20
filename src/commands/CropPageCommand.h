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

private:
    IPdfEditorEngine* m_engine;
    DocumentSession*  m_doc;
    int m_pageIndex;
    QRectF m_cropRect;
    // Note: A fully robust undo would capture and restore the previous CropBox.
    // That requires a getCropBox() API addition to IPdfEditorEngine — deferred.
};
