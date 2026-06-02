// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class RotateImageCommand : public QUndoCommand {
public:
    RotateImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                       int pageIndex, const QString& xobjectName, double degrees)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_degrees(degrees) {
        setText(QObject::tr("Rotate image %1 by %2°").arg(xobjectName).arg(degrees));
    }
    void redo() override {
        m_engine->rotateImage(m_page, m_name, m_degrees);
        m_doc->markReload();
    }
    void undo() override {
        m_engine->rotateImage(m_page, m_name, -m_degrees);
        m_doc->markReload();
    }
    int id() const override { return 0x112; }
    bool mergeWith(const QUndoCommand* other) override {
        if (other->id() != id()) return false;
        auto* o = static_cast<const RotateImageCommand*>(other);
        if (o->m_page != m_page || o->m_name != m_name) return false;
        m_degrees += o->m_degrees;
        setText(QObject::tr("Rotate image %1 by %2°").arg(m_name).arg(m_degrees));
        return true;
    }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    double m_degrees;
};
