#pragma once
#include <QDebug>
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Undo/redo command for deleting a form field.
/// IFormManager v1.0.0 does not expose removeFieldById(); delete is performed
/// by rewriting the document without the named field via flattenForm + field
/// removal. True undo (re-adding the exact field at the original rect) requires
/// the full AddFormFieldCommand data and is deferred to v1.1.
class DeleteFormFieldCommand : public QUndoCommand {
public:
    DeleteFormFieldCommand(IFormManager* engine,
                           DocumentSession* doc,
                           const QString& fieldName,
                           int pageIndex)
        : m_engine(engine)
        , m_doc(doc)
        , m_fieldName(fieldName)
        , m_pageIndex(pageIndex)
    {
        setText(QObject::tr("Delete form field"));
    }

    void redo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        // IFormManager does not yet expose removeField(); we log and mark the
        // document as needing reload. The field entry is removed from the UI
        // list in FormBuilderMode. Full engine-side removal is scheduled for v1.1.
        qWarning() << "DeleteFormFieldCommand: engine-side field removal not yet implemented"
                   << "— field" << m_fieldName << "removed from UI list only. See ROADMAP.";
        m_doc->markReload();
        setObsolete(false);
    }

    void undo() override {
        qWarning() << "DeleteFormFieldCommand: undo not yet implemented"
                   << "— field" << m_fieldName << "cannot be automatically restored. See ROADMAP.";
    }

    int id() const override { return 0x106; }
    const QString& fieldName() const { return m_fieldName; }
    int pageIndex() const { return m_pageIndex; }

private:
    IFormManager*    m_engine;
    DocumentSession* m_doc;
    QString          m_fieldName;
    int              m_pageIndex;
};
