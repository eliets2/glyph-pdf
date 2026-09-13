// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "commands/CheckedHistory.h"

// WP-R03 (WHOLE-ARCHITECTURE-REVIEW A02): inline-text editing used to be a
// raw QUndoCommand that ignored the edit result, and whose undo restored the
// original page as TWO separately committed steps (insertPageFromBytes then
// deletePage) with neither result checked. A failed edit was recorded in
// history; a failed insertion let the undo DELETE the real adjacent page;
// a failed deletion left an extra page. The command is now a
// CheckedUndoCommand at the ONE shared boundary:
//   - the initial redo() refuses to run without a captured restorable
//     snapshot and CHECKS the edit; a failure sets obsolete(true) so push()
//     deletes the command — history never records an edit that did not
//     happen (the EC03 "no destructive edit without a restorable backup"
//     ownership rule);
//   - the undo/redo traversals are CHECKED (applyChecked/restoreChecked):
//     they run while the history index is untouched, and a failed step
//     leaves the index, the clean state and the retryability truthful;
//   - the restoration is the G08 ATOMIC seam (restorePageFromBytes — insert
//     the snapshot and remove the displaced page inside one engine
//     transaction), so a failed insertion can never permit the deletion of
//     the original adjacent page: the two-step path no longer exists here.
class EditTextInlineCommand : public CheckedUndoCommand {
public:
    EditTextInlineCommand(IPdfEditorEngine* engine, DocumentSession* doc, int pageIndex, const QRectF& rect, const QString& newText,
                          const QString& fontFamily, int fontSize, const QColor& color, bool bold, bool italic, int alignment)
        : m_engine(engine), m_doc(doc), m_page(pageIndex), m_rect(rect), m_newText(newText),
          m_fontFamily(fontFamily), m_fontSize(fontSize), m_color(color), m_bold(bold), m_italic(italic), m_alignment(alignment) {
        setText(QObject::tr("Edit Text Inline"));
    }

    void redo() override {
        if (consumeArmedApply())
            return;   // checked traversal already applied; index-move only
        m_succeeded = false;
        m_error.clear();
        if (!applyEdit(&m_error)) {
            setObsolete(true);
            if (m_doc && !m_error.isEmpty())
                emit m_doc->mutationFailed(m_error);
            return;
        }
        m_succeeded = true;
        setObsolete(false);
    }

    bool applyChecked() override {
        QString err;
        if (!applyEdit(&err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return false;   // history index unchanged — redo retryable
        }
        armCheckedApply();
        return true;
    }

    bool restoreChecked() override {
        QString err;
        if (!applyRestore(&err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return false;   // command stays current — undo retryable
        }
        armCheckedRestore();
        return true;
    }

    void undo() override {
        if (consumeArmedRestore())
            return;   // the checked traversal already restored; index-move only
        QString err;
        if (!applyRestore(&err)) {
            if (m_doc) emit m_doc->mutationFailed(err);
            return;   // no markReload: the disk still carries the edited page
        }
    }

    int id() const override { return 0x205; }

    // Introspection (same ownership rules as CropPageCommand: do not
    // dereference a pushed command after a possibly-failed push).
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    // The shared mutation body — snapshot capture (exactly once, the
    // original once-only guard) + the checked edit. No reporting, no
    // obsoletion. Marks the session reload only after an edit that really
    // happened.
    bool applyEdit(QString* err) {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) {
            if (err) *err = QObject::tr("Inline text edit failed: no document is open.");
            return false;
        }
        // Capture the snapshot BEFORE the edit so undo() can restore it.
        // Guard: only capture on the first application. On subsequent
        // applications (after an undo/redo cycle) m_originalPageBytes already
        // holds the true pre-edit state; overwriting it would restore the
        // wrong page bytes. QByteArray is empty by default; a REAL page never
        // extracts to empty, so emptiness also detects a FAILED capture —
        // and an edit without a restorable snapshot is refused outright
        // (EC03's "no destructive edit without a restorable backup").
        if (m_originalPageBytes.isEmpty()) {
            m_originalPageBytes = m_engine->extractPageAsBytes(m_doc->path(), m_page);
            if (m_originalPageBytes.isEmpty()) {
                if (err) *err = QObject::tr("Inline text edit refused: page %1 could not be captured for undo; nothing was changed.")
                                     .arg(m_page + 1);
                return false;
            }
        }
        if (!m_engine->editTextInline(m_page, m_rect, m_newText, m_fontFamily, m_fontSize, m_color, m_bold, m_italic, m_alignment)) {
            if (err) *err = QObject::tr("The inline text edit on page %1 failed; the document was left unchanged.")
                                 .arg(m_page + 1);
            return false;
        }
        m_doc->markReload();
        return true;
    }

    // G08: the shared restoration body — ONE committed atomic transaction
    // (restorePageFromBytes), never the two-step insert+delete path. No
    // reporting; callers own the reporting.
    bool applyRestore(QString* err) {
        if (m_originalPageBytes.isEmpty()) {
            if (err) *err = QObject::tr("Inline text undo failed: no page snapshot was captured.");
            return false;
        }
        if (!m_engine->restorePageFromBytes(m_doc->path(), m_page, m_originalPageBytes)) {
            if (err) *err = QObject::tr("Undo of the inline text edit failed on page %1; the edited page is still in effect.")
                                 .arg(m_page + 1);
            return false;
        }
        m_doc->markReload();
        return true;
    }

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
    bool m_succeeded = false;
    QString m_error;
};
