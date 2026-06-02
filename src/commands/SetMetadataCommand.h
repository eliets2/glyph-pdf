// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class SetMetadataCommand : public QUndoCommand {
public:
    SetMetadataCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                       const PdfMetadata& newMeta)
        : m_engine(engine), m_doc(doc), m_newMeta(newMeta) {
        setText(QObject::tr("Edit metadata"));
        m_engine->loadDocumentForEditing(m_doc->path());
        m_engine->getMetadata(m_oldMeta);
    }

    void redo() override {
        m_engine->loadDocumentForEditing(m_doc->path());
        m_engine->setMetadata(m_newMeta);
        m_engine->saveDocument(m_doc->path());
        m_doc->markReload();
    }

    void undo() override {
        m_engine->loadDocumentForEditing(m_doc->path());
        m_engine->setMetadata(m_oldMeta);
        m_engine->saveDocument(m_doc->path());
        m_doc->markReload();
    }

    int id() const override { return 0x108; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    PdfMetadata m_oldMeta;
    PdfMetadata m_newMeta;
};
