// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class ReplaceImageCommand : public QUndoCommand {
public:
    ReplaceImageCommand(IPdfEditorEngine* engine, DocumentSession* doc,
                        int pageIndex, const QString& xobjectName,
                        const QString& newImagePath, const QByteArray& pageBackup)
        : m_engine(engine), m_doc(doc), m_page(pageIndex),
          m_name(xobjectName), m_newPath(newImagePath), m_backup(pageBackup) {
        setText(QObject::tr("Replace image %1").arg(xobjectName));
    }
    void redo() override {
        m_engine->replaceImage(m_page, m_name, m_newPath);
        m_doc->markReload();
    }
    void undo() override {
        // Restore the full page from backup to recover original image
        m_engine->insertPageFromBytes(m_doc->path(), m_page, m_backup);
        m_engine->deletePage(m_doc->path(), m_page + 1);
        m_doc->markReload();
    }
    int id() const override { return 0x113; }
private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QString m_name;
    QString m_newPath;
    QByteArray m_backup;
};
