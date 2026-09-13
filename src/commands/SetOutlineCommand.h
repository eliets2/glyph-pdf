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
//
// packa-F3 single-writer ownership: when the PRODUCER (EditController::
// runAutoBookmarks) has already committed exactly this outline before pushing
// the command, pass alreadyApplied=true — the command's FIRST redo then only
// reloads the viewer instead of writing the same outline a second time (one
// user action = one expensive path-based save). Undo/redo still write.
class SetOutlineCommand : public QUndoCommand {
public:
    SetOutlineCommand(IPdfEditorEngine* engine,
                      DocumentSession* doc,
                      PdfViewerWidget* viewer,
                      const QString& path,
                      QList<OutlineEntry> oldEntries,
                      QList<OutlineEntry> newEntries,
                      bool alreadyApplied = false)
        : m_engine(engine), m_doc(doc), m_viewer(viewer), m_path(path),
          m_old(std::move(oldEntries)), m_new(std::move(newEntries)),
          m_skipInitialWrite(alreadyApplied) {
        setText(QObject::tr("Create auto-bookmarks"));
    }

    void redo() override {
        if (m_skipInitialWrite) {
            // packa-F3: the producer committed this exact outline already;
            // the initial redo mirrors it in the viewer without a second write.
            m_skipInitialWrite = false;
            reload();
            return;
        }
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
    bool m_skipInitialWrite = false;
};
