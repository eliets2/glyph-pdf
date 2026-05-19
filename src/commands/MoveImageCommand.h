#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class MoveImageCommand : public QUndoCommand {
public:
    MoveImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                     int pageIndex, const QString& xobjectName, double dx, double dy)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_dx(dx), m_dy(dy) {
        setText(QObject::tr("Move image %1").arg(xobjectName));
    }
    void redo() override {
        m_engine->moveImage(m_page, m_name, m_dx, m_dy);
        m_doc->markReload();
    }
    void undo() override {
        m_engine->moveImage(m_page, m_name, -m_dx, -m_dy);
        m_doc->markReload();
    }
    int id() const override { return 0x110; }
    bool mergeWith(const QUndoCommand* other) override {
        if (other->id() != id()) return false;
        auto* o = static_cast<const MoveImageCommand*>(other);
        if (o->m_page != m_page || o->m_name != m_name) return false;
        m_dx += o->m_dx;
        m_dy += o->m_dy;
        return true;
    }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    double m_dx, m_dy;
};
