// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class ResizeImageCommand : public QUndoCommand {
public:
    ResizeImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                       int pageIndex, const QString& xobjectName,
                       double oldW, double oldH, double newW, double newH)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_oldW(oldW), m_oldH(oldH), m_newW(newW), m_newH(newH) {
        setText(QObject::tr("Resize image %1").arg(xobjectName));
    }
    void redo() override {
        m_engine->resizeImage(m_page, m_name, m_newW, m_newH);
        m_doc->markReload();
    }
    void undo() override {
        m_engine->resizeImage(m_page, m_name, m_oldW, m_oldH);
        m_doc->markReload();
    }
    int id() const override { return 0x111; }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    double m_oldW, m_oldH, m_newW, m_newH;
};
