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
}
