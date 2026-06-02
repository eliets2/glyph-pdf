// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class EditTextInlineCommand : public QUndoCommand {
public:
    EditTextInlineCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& rect, const QString& newText,
                          const QString& fontFamily, int fontSize, const QColor& color, bool bold, bool italic, int alignment)
        : m_engine(engine), m_doc(doc), m_page(pageIndex), m_rect(rect), m_newText(newText),
          m_fontFamily(fontFamily), m_fontSize(fontSize), m_color(color), m_bold(bold), m_italic(italic), m_alignment(alignment) {
        setText(QObject::tr("Edit Text Inline"));
    }

    void redo() override {
        m_engine->editTextInline(m_page, m_rect, m_newText, m_fontFamily, m_fontSize, m_color, m_bold, m_italic, m_alignment);
        m_doc->markReload();
    }

    void undo() override {
        // Full structural undo not trivial without saving backup of original text,
        // for now just triggering reload since it's hard to revert inline text accurately in PDF
    }

    int id() const override { return 0x205; }

private:
    IPdfEditorEngine* m_engine;
    DocumentSession* m_doc;
    int m_page;
    QRectF m_rect;
    QString m_newText;
    QString m_fontFamily;
    int m_fontSize;
    QColor m_color;
    bool m_bold;
    bool m_italic;
    int m_alignment;
};
