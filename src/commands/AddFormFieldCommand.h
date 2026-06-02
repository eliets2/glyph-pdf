// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDebug>
#include <QUndoCommand>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

class AddFormFieldCommand : public QUndoCommand {
public:
    enum class FieldType { Text, Checkbox, Radio, Dropdown, ListBox, Date, Numeric, Button };

    AddFormFieldCommand(IFormManager* engine, DocumentSession* doc, FieldType type,
                        int pageIndex, const QRectF& rect, const QString& name, const QStringList& options = {})
        : m_engine(engine), m_doc(doc), m_type(type), m_page(pageIndex), m_rect(rect), m_name(name), m_options(options) {
        setText(QObject::tr("Add form field"));
    }

    void redo() override {
        switch (m_type) {
            case FieldType::Text: m_engine->addTextField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Checkbox: m_engine->addCheckBox(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Radio: m_engine->addRadioButton(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Dropdown: m_engine->addDropdown(m_doc->path(), m_page, m_rect, m_name, m_options, m_doc->path()); break;
            case FieldType::ListBox: m_engine->addListBox(m_doc->path(), m_page, m_rect, m_name, m_options, true, m_doc->path()); break;
            case FieldType::Date: m_engine->addDateField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Numeric: m_engine->addNumericField(m_doc->path(), m_page, m_rect, m_name, m_doc->path()); break;
            case FieldType::Button: m_engine->createButton(m_doc->path(), m_page, m_rect, m_name, m_options.isEmpty() ? "" : m_options.first(), m_doc->path()); break;
        }
        m_doc->markReload();
        // Keep the undo entry visible in the stack even though undo() is a no-op
        // so the user knows something happened. v1.1 will implement true field
        // removal once IFormManager exposes removeFieldById.
        setObsolete(false);
    }

    void undo() override {
        qWarning() << "AddFormFieldCommand: undo not yet implemented — field will remain. See ROADMAP.";
        // To implement: call IFormManager::removeFieldById(m_fieldId, m_pageIndex)
    }

    int id() const override { return 0x104; }

private:
    IFormManager* m_engine;
    DocumentSession* m_doc;
    FieldType m_type;
    int m_page;
    QRectF m_rect;
    QString m_name;
    QStringList m_options;
};
