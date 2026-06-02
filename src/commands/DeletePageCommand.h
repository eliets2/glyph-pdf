// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class DeletePageCommand : public QUndoCommand {
public:
    DeletePageCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex)
        : m_engine(engine), m_doc(doc), m_page(pageIndex) {
        setText(QObject::tr("Delete page %1").arg(pageIndex + 1));
        // Save the page before deletion so we can undo
        m_savedData = m_engine->extractPageAsBytes(m_doc->path(), m_page);
    }

    void redo() override {
        m_engine->deletePage(m_doc->path(), m_page);
        m_doc->markReload();
    }

    void undo() override {
        if (!m_savedData.isEmpty()) {
            m_engine->insertPageFromBytes(m_doc->path(), m_page, m_savedData);
            m_doc->markReload();
        }
    }

    int id() const override { return 0x101; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QByteArray m_savedData;
};
