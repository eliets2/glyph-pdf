#include "commands/RedactCommand.h"
#include "ui/PdfViewerWidget.h"

void RedactCommand::redo() {
    if (!m_viewer || !m_doc) return;

    const int before = m_viewer->annotations().size();
    m_viewer->redactAllMatches(m_text, m_matchCase, m_wholeWords);
    const int after = m_viewer->annotations().size();
    if (after > before) {
        m_doc->markReload();
    }
    // Redaction is correctly non-reversible; mark the command obsolete so it
    // does not sit in QUndoStack with a no-op undo (audit 2026-05-23).
    setObsolete(true);
}
