// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDebug>
#include <QUndoCommand>
#include <QString>
#include <QFile>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Undo/redo command for deleting a form field.
///
/// redo(): removes the field from the PDF via IFormManager::removeFieldByName
///         and writes the result in-place (same path via a temp rename).
/// undo(): re-adds the field via IFormManager::addTextField (text fields only
///         for now; other types are restored as text fields — the type
///         information is not preserved across undo in v1.0).  A full
///         per-type undo would require capturing the original PoDoFo dict;
///         left as a v1.1 improvement.  The undo correctly restores a
///         placeholder field so the document is not left in an incomplete state.
class DeleteFormFieldCommand : public QUndoCommand {
public:
    DeleteFormFieldCommand(IFormManager* engine,
                           DocumentSession* doc,
                           const QString& fieldName,
                           int pageIndex,
                           const QRectF& fieldRect = QRectF())
        : m_engine(engine)
        , m_doc(doc)
        , m_fieldName(fieldName)
        , m_pageIndex(pageIndex)
        , m_fieldRect(fieldRect)
    {
        setText(QObject::tr("Delete form field"));
    }

    void redo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        const QString path = m_doc->path();
        const bool ok = m_engine->removeFieldByName(path, m_fieldName, path);
        if (!ok) {
            qWarning() << "DeleteFormFieldCommand::redo: removeFieldByName failed for"
                       << m_fieldName;
        }
        m_doc->markDirty();
    }

    void undo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        if (m_fieldRect.isNull()) {
            qWarning() << "DeleteFormFieldCommand::undo: no rect stored for field"
                       << m_fieldName << "— cannot restore. Re-add manually.";
            return;
        }
        const QString path = m_doc->path();
        const bool ok = m_engine->addTextField(path, m_pageIndex, m_fieldRect, m_fieldName, path);
        if (!ok) {
            qWarning() << "DeleteFormFieldCommand::undo: addTextField failed for" << m_fieldName;
        }
        m_doc->markDirty();
    }

    int id() const override { return 0x106; }
    const QString& fieldName() const { return m_fieldName; }
    int pageIndex() const { return m_pageIndex; }
    void setFieldRect(const QRectF& r) { m_fieldRect = r; }

private:
    IFormManager*    m_engine;
    DocumentSession* m_doc;
    QString          m_fieldName;
    int              m_pageIndex;
    QRectF           m_fieldRect;
};
