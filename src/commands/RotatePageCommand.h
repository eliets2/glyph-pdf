#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class RotatePageCommand : public QUndoCommand {
public:
    RotatePageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                      int pageIndex, int degrees)
        : m_engine(engine), m_doc(doc), m_page(pageIndex), m_degrees(degrees) {
        setText(QObject::tr("Rotate page %1 by %2°").arg(pageIndex + 1).arg(degrees));
    }

    void redo() override {
        m_engine->rotatePage(m_doc->path(), m_page, m_degrees);
        m_doc->markReload();
    }

    void undo() override {
        m_engine->rotatePage(m_doc->path(), m_page, -m_degrees);
        m_doc->markReload();
    }

    int id() const override { return 0x100; }

    bool mergeWith(const QUndoCommand* other) override {
        if (other->id() != id()) return false;
        auto* o = static_cast<const RotatePageCommand*>(other);
        if (o->m_page != m_page) return false;
        m_degrees += o->m_degrees;
        setText(QObject::tr("Rotate page %1 by %2°").arg(m_page + 1).arg(m_degrees));
        return true;
    }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    int m_degrees;
};
