// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QPointer>
#include <QString>
#include "engines/DocumentSession.h"

class PdfViewerWidget;

// Uses QPointer to safely guard against a deleted viewer.
// Delegates the mark-all-matches operation to the viewer if alive,
// and signals DocumentSession for reload.
class RedactCommand : public QUndoCommand {
public:
    RedactCommand(PdfViewerWidget* viewer, DocumentSession* doc,
                  const QString& text, bool matchCase, bool wholeWords)
        : m_viewer(viewer), m_doc(doc), m_text(text),
          m_matchCase(matchCase), m_wholeWords(wholeWords) {
        setText(QObject::tr("Redact '%1'").arg(text));
    }

    void redo() override;
    void undo() override { /* redaction marks are non-reversible at this level */ }
    int id() const override { return 0x10A; }

private:
    QPointer<PdfViewerWidget> m_viewer;
    DocumentSession* m_doc;
    QString m_text;
    bool m_matchCase;
    bool m_wholeWords;
};


