// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDebug>
#include <QUndoCommand>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

class AddFormFieldCommand : public QUndoCommand {
public:
    enum class FieldType { Text, Checkbox, Radio, Dropdown, ListBox, Date, Numeric, Button, Calculated };

    AddFormFieldCommand(IFormManager* engine, DocumentSession* doc, FieldType type,
                        int pageIndex, const QRectF& rect, const QString& name, const QStringList& options = {})
        : m_engine(engine), m_doc(doc), m_type(type), m_page(pageIndex), m_rect(rect), m_name(name), m_options(options) {
        setText(QObject::tr("Add form field"));
    }

    // R01 (F01): the engine operation's result is now checked. The form save
    // boundary is transactional — a failed operation leaves the document on
    // disk untouched — so on failure the session is NOT marked for reload and
    // the command marks itself obsolete: QUndoStack keeps ownership (the
    // entry stays visible, struck through in undo views) but undo()/redo()
    // traversal skips it, so a failed command never reads as a success entry.
    void redo() override {
        m_succeeded = false;
        m_error.clear();
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) {
            m_error = QObject::tr("no engine/document for add form field");
            setObsolete(true);
            return;
        }
        bool ok = false;
        switch (m_type) {
            case FieldType::Text: ok = m_engine->addTextField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Checkbox: ok = m_engine->addCheckBox(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Radio: ok = m_engine->addRadioButton(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Dropdown: ok = m_engine->addDropdown(m_doc->path(), m_page, m_rect, m_name, m_options, m_doc->path()); break;
            case FieldType::ListBox: ok = m_engine->addListBox(m_doc->path(), m_page, m_rect, m_name, m_options, true, m_doc->path()); break;
            case FieldType::Date: ok = m_engine->addDateField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Numeric: ok = m_engine->addNumericField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Button: ok = m_engine->createButton(m_doc->path(), m_page, m_rect, m_name, m_options.isEmpty() ? "" : m_options.first(), m_doc->path()); break;
            // Calculated reuses m_options.first() to carry the JS calculation expression (like Button's action).
            case FieldType::Calculated: ok = m_engine->addCalculatedField(m_doc->path(), m_page, m_rect, m_name, m_options.isEmpty() ? "" : m_options.first(), m_doc->path()); break;
        }
        m_succeeded = ok;
        if (ok) {
            m_doc->markReload();
        } else {
            m_error = QObject::tr("adding form field %1 failed; document left unchanged").arg(m_name);
            qWarning() << "AddFormFieldCommand::redo failed for" << m_name << "— command marked obsolete, document not reloaded";
            setObsolete(true);
        }
    }

    void undo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        const bool ok = m_engine->removeFieldByName(m_doc->path(), m_name, m_doc->path());
        if (!ok) {
            qWarning() << "AddFormFieldCommand::undo: removeFieldByName failed for" << m_name;
            return; // refresh state only after successful persistence
        }
        m_doc->markReload();
    }

    int id() const override { return 0x104; }

    /// R01: true when the most recent redo() persisted successfully.
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    IFormManager* m_engine;
    DocumentSession* m_doc;
    FieldType m_type;
    int m_page;
    QRectF m_rect;
    QString m_name;
    QStringList m_options;
    bool m_succeeded = false;
    QString m_error;
};
