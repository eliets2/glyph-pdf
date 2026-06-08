// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDebug>
#include <QRectF>
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Undo/redo command for moving a form field to a new position.
/// redo(): calls IFormManager::updateFieldRect with newRect (persists to PDF).
/// undo(): calls IFormManager::updateFieldRect with oldRect (reverts).
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
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        const QString path = m_doc->path();
        const bool ok = m_engine->updateFieldRect(path, m_fieldName, m_pageIndex, m_newRect, path);
        if (!ok) {
            qWarning() << "MoveFormFieldCommand::redo: updateFieldRect failed for" << m_fieldName;
        }
        m_doc->markDirty();
    }

    void undo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        const QString path = m_doc->path();
        const bool ok = m_engine->updateFieldRect(path, m_fieldName, m_pageIndex, m_oldRect, path);
        if (!ok) {
            qWarning() << "MoveFormFieldCommand::undo: updateFieldRect (revert) failed for" << m_fieldName;
        }
        m_doc->markDirty();
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
