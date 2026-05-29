#pragma once
#include <QUndoCommand>
#include <QString>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Properties bundle passed to EditFormFieldCommand.
struct EditFormFieldProperties {
    QString name;           ///< New field name (may be the same as old name)
    QString tooltip;        ///< Tooltip / user-visible description
    bool    required = false;
    QString defaultVal;     ///< Default value (text fields)
    QString placeholder;    ///< Placeholder text shown when field is empty
    QString validRegex;     ///< Optional validation regex (empty = no validation)
};

/// Undo/redo command for editing form field metadata.
/// Uses IFormManager::fillForm to update the field value stored in the PDF.
/// Full round-trip (name rename, tooltip, JS actions) requires PoDoFo direct
/// access which is intentionally hidden behind IFormManager; those properties
/// are stored client-side for display in FormFieldPropertiesPanel and applied
/// to the document via fillForm for the value/default fields.
class EditFormFieldCommand : public QUndoCommand {
public:
    EditFormFieldCommand(IFormManager* engine,
                         DocumentSession* doc,
                         const QString& originalName,
                         const EditFormFieldProperties& newProps)
        : m_engine(engine)
        , m_doc(doc)
        , m_originalName(originalName)
        , m_newProps(newProps)
    {
        setText(QObject::tr("Edit form field"));

        // Store old props (name only — other props are not readable back from
        // IFormManager in v1.0.0; they are stored as undo state)
        m_oldProps.name = originalName;
    }

    void redo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        // Apply new default value via fillForm if the default value changed
        if (!m_newProps.defaultVal.isEmpty()) {
            QVariantMap data;
            data[m_originalName] = m_newProps.defaultVal;
            m_engine->fillForm(m_doc->path(), data, m_doc->path());
        }
        m_doc->markReload();
        setObsolete(false);
    }

    void undo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        // Restore old default value
        if (!m_oldProps.defaultVal.isEmpty()) {
            QVariantMap data;
            data[m_originalName] = m_oldProps.defaultVal;
            m_engine->fillForm(m_doc->path(), data, m_doc->path());
        }
        m_doc->markReload();
    }

    int id() const override { return 0x105; }

    const QString& originalName() const { return m_originalName; }
    const EditFormFieldProperties& newProps() const { return m_newProps; }
    const EditFormFieldProperties& oldProps() const { return m_oldProps; }

private:
    IFormManager*            m_engine;
    DocumentSession*         m_doc;
    QString                  m_originalName;
    EditFormFieldProperties  m_newProps;
    EditFormFieldProperties  m_oldProps;  // captured at command creation
};
