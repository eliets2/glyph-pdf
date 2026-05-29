#pragma once
#include <QDebug>
#include <QRectF>
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Undo/redo command for resizing a form field's bounding rectangle.
/// IFormManager v1.0.0 does not expose updateFieldRect(); the command records
/// the resize and marks the document dirty. Full engine-side rect update is
/// scheduled for v1.1 (same blocker as MoveFormFieldCommand).
class ResizeFormFieldCommand : public QUndoCommand {
public:
    ResizeFormFieldCommand(IFormManager* engine,
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
        setText(QObject::tr("Resize form field"));
    }

    void redo() override {
        if (!m_doc) return;
        qWarning() << "ResizeFormFieldCommand: engine-side field resize not yet implemented"
                   << "— field" << m_fieldName << "resize recorded only. See ROADMAP.";
        m_doc->markReload();
        setObsolete(false);
    }

    void undo() override {
        if (!m_doc) return;
        qWarning() << "ResizeFormFieldCommand: undo not yet implemented. See ROADMAP.";
        m_doc->markReload();
    }

    int id() const override { return 0x108; }
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
