#pragma once
#include <QUndoCommand>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

class AddFormFieldCommand : public QUndoCommand {
public:
    enum class FieldType { Text, Checkbox, Radio, Dropdown };

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
        }
        m_doc->markReload();
    }

    void undo() override {
        // Implementation omitted for brevity, but it would remove the added field
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
