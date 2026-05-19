#pragma once
#include <QUndoCommand>
#include <QString>
#include <QByteArray>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class DeleteImageCommand : public QUndoCommand {
public:
    DeleteImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                       int pageIndex, const QString& xobjectName,
                       const QByteArray& pageBackup)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_backup(pageBackup) {
        setText(QObject::tr("Delete image %1").arg(xobjectName));
    }
    void redo() override {
        m_engine->deleteImage(m_page, m_name);
        m_doc->markReload();
    }
    void undo() override {
        // Restore the full page from backup to recover deleted image
        m_engine->insertPageFromBytes(m_doc->path(), m_page, m_backup);
        m_engine->deletePage(m_doc->path(), m_page + 1);
        m_doc->markReload();
    }
    int id() const override { return 0x114; }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    QByteArray m_backup;
};
