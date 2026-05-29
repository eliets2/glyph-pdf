#pragma once
#include <QDebug>
#include <QRectF>
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Undo/redo command for moving a form field to a new position.
/// IFormManager v1.0.0 does not expose a moveField() method; the command
/// records the move intent and marks the document as dirty. Full engine-side
/// rect update requires PoDoFo widget annotation dictionary surgery and is
/// scheduled for v1.1 (blocked on IFormManager::updateFieldRect API addition).
class MoveFormFieldCommand : public QUndoCommand {
public:
    MoveFormFieldCommand(IFormManager* engine,
                         DocumentSession* doc,
                         const QString& fieldName,
                         int pageIndex,
                         const QRectF& oldRect,
                         const QRectF& newRect)
        : m_engine(engine)
        , m_doc(doc)
        , m_fieldName(fieldName)
        , m_pageIndex(pageIndex)
        , m_oldRect(oldRect)
        , m_newRect(newRect)
    {
        setText(QObject::tr("Move form field"));
    }

    void redo() override {
        if (!m_doc) return;
        qWarning() << "MoveFormFieldCommand: engine-side field move not yet implemented"
                   << "— field" << m_fieldName << "move recorded only. See ROADMAP.";
        m_doc->markReload();
        setObsolete(false);
    }

    void undo() override {
        if (!m_doc) return;
        qWarning() << "MoveFormFieldCommand: undo not yet implemented. See ROADMAP.";
        m_doc->markReload();
    }

    int id() const override { return 0x107; }
    const QString& fieldName() const { return m_fieldName; }
    int pageIndex() const { return m_pageIndex; }
    QRectF oldRect() const { return m_oldRect; }
    QRectF newRect() const { return m_newRect; }

private:
    IFormManager*    m_engine;
    DocumentSession* m_doc;
    QString          m_fieldName;
    int              m_pageIndex;
    QRectF           m_oldRect;
    QRectF           m_newRect;
};
