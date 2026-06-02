// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class InsertPageCommand : public QUndoCommand {
public:
    InsertPageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex)
        : m_engine(engine), m_doc(doc), m_page(pageIndex) {
        setText(QObject::tr("Insert page %1").arg(pageIndex + 1));
    }

    void redo() override {
        m_engine->insertBlankPage(m_doc->path(), m_page);
        m_doc->markReload();
    }

    void undo() override {
        m_engine->deletePage(m_doc->path(), m_page);
        m_doc->markReload();
    }

    int id() const override { return 0x102; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
};
