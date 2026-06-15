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
        // Capture snapshot BEFORE the edit so undo() can restore it.
        // Guard: only capture on the first redo() call. On subsequent redo() calls
        // (after an undo/redo cycle) m_originalPageBytes already holds the true
        // pre-edit state; overwriting it would cause undo() to restore to the wrong
        // page bytes. QByteArray is empty by default; extractPageAsBytes never
        // returns empty for a real page, so isEmpty() is a reliable once-only guard.
        if (m_originalPageBytes.isEmpty()) {
            m_originalPageBytes = m_engine->extractPageAsBytes(m_doc->path(), m_page);
        }

        m_engine->editTextInline(m_page, m_rect, m_newText, m_fontFamily, m_fontSize, m_color, m_bold, m_italic, m_alignment);
        m_doc->markReload();
    }

    void undo() override {
        if (m_originalPageBytes.isEmpty()) return;

        // Insert saved snapshot at the same position, then remove the edited page
        // that is now at m_page + 1
        m_engine->insertPageFromBytes(m_doc->path(), m_page, m_originalPageBytes);
        m_engine->deletePage(m_doc->path(), m_page + 1);

        m_doc->markReload();
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
    QByteArray m_originalPageBytes;
};
