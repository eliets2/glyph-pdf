// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QPointer>
#include <QString>
#include <QList>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "ui/PdfViewerWidget.h"

// T2-9: the undoable outline commit. `redo` (and the initial push) writes the
// NEW outline as one committed engine write; `undo` restores the snapshot the
// document had before the auto-bookmark run. Both paths reload the viewer so
// the outline panel always mirrors the committed file. Takes the INTERFACE
// (IOutlineEditor seam) like every other command — never a concrete engine —
// so BackendRouter is free to hand out any implementation.
class SetOutlineCommand : public QUndoCommand {
public:
    SetOutlineCommand(IPdfEditorEngine* engine,
                      DocumentSession* doc,
                      PdfViewerWidget* viewer,
                      const QString& path,
                      QList<OutlineEntry> oldEntries,
                      QList<OutlineEntry> newEntries)
        : m_engine(engine), m_doc(doc), m_viewer(viewer), m_path(path),
          m_old(oldEntries), m_new(newEntries) {
        setText(QObject::tr("Create auto-bookmarks"));
    }

    void redo() override {
        if (m_engine && m_engine->replaceOutline(m_path, m_new))
            reload();
    }

    void undo() override {
        if (m_engine && m_engine->replaceOutline(m_path, m_old))
            reload();
    }

    int id() const override { return 0x10A; }

private:
    void reload() {
        if (m_doc) m_doc->markReload();
        if (m_viewer) m_viewer->reload();
    }

    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    QPointer<PdfViewerWidget> m_viewer;
    QString m_path;
    QList<OutlineEntry> m_old;
    QList<OutlineEntry> m_new;
};
