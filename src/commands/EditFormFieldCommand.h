// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDebug>
#include <QUndoCommand>
#include "core/interfaces/IFormManager.h"
#include "engines/DocumentSession.h"

/// Properties bundle passed to EditFormFieldCommand.
///
/// R02 (F09) documented persistence contract:
///   - `tooltip`    → /TU   (persisted; empty clears the key)
///   - `required`   → /Ff bit 2 (persisted)
///   - `defaultVal` → the field's CURRENT value, PDF /V (this is what the
///     properties panel's "Default" row has always edited, applied via
///     fillForm's SetText). An explicitly empty string CLEARS the value.
///   - `name`       → NOT persisted. Renaming an AcroForm field has no
///     complete persistence contract yet (full name lives in the field tree
///     and every widget's /T); the panel's Name row is display-only here.
///   - `placeholder`, `validRegex` → NOT persisted. Neither PDF AcroForm nor
///     this engine has a persistence contract for them; they stay client-side
///     panel state and are deliberately ignored by this command.
struct EditFormFieldProperties {
    QString name;           ///< New field name (NOT applied — rename is out of scope)
    QString tooltip;        ///< Tooltip / user-visible description → /TU
    bool    required = false;
    QString defaultVal;     ///< Field CURRENT value (/V); empty clears it
    QString placeholder;    ///< Placeholder text (NOT persisted — no contract)
    QString validRegex;     ///< Optional validation regex (NOT persisted — no contract)
};

/// Undo/redo command for editing form field properties.
///
/// R02 (F09) fix: the COMPLETE supported state is captured as a
/// FormFieldSnapshot BEFORE the first mutation (constructor reads the document
/// from disk). Redo applies the new snapshot, undo applies the old one — both
/// through IFormManager::applyFieldSnapshot, which persists value + tooltip +
/// required as ONE transactional mutation with ONE R01 safe-save commit, so a
/// failed edit cannot partially persist metadata. Snapshots are captured
/// exactly once: redo after undo re-applies the stored new snapshot and never
/// recaptures the edited state as the original. Missing fields resolve
/// explicitly (found == false → the command fails without writing); duplicate
/// full names address the first occurrence.
///
/// Failure handling respects QUndoStack ownership: Qt 6.11 push() DELETES a
/// command that is obsolete after its initial redo() (no undo entry at all);
/// during traversal, obsolete commands are skipped. Callers must not
/// dereference a pushed command after a possibly-failed push — inspect
/// succeeded()/lastError() via a direct redo() instead.
class EditFormFieldCommand : public QUndoCommand {
public:
    EditFormFieldCommand(IFormManager* engine,
                         DocumentSession* doc,
                         const QString& originalName,
                         const EditFormFieldProperties& newProps)
        : m_engine(engine)
        , m_doc(doc)
        , m_newProps(newProps)
    {
        setText(QObject::tr("Edit form field"));
        m_oldProps.name = originalName;

        // Capture BEFORE the first mutation. If engine/document are unusable
        // the snapshot stays found == false and every redo() fails explicitly.
        if (engine && doc && !doc->path().isEmpty())
            m_old = engine->captureFieldSnapshot(doc->path(), originalName);

        // The new state starts from the captured state and overrides ONLY the
        // properties the panel edits — unrelated metadata (/DV, other /Ff
        // bits, other fields) is preserved exactly through redo and undo.
        m_new = m_old;
        m_new.tooltip = newProps.tooltip;
        m_new.tooltipPresent = !newProps.tooltip.isEmpty(); // empty clears /TU
        m_new.required = newProps.required;
        m_new.value = newProps.defaultVal;  // documented meaning: current value /V
        m_new.valuePresent = true;          // the panel always writes /V, even explicitly empty
    }

    void redo() override {
        m_succeeded = false;
        m_error.clear();
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) {
            m_error = QObject::tr("no engine/document for edit form field");
            setObsolete(true);
            return;
        }
        if (!m_old.found) {
            m_error = QObject::tr("field %1 not found; nothing was changed").arg(m_oldProps.name);
            qWarning() << "EditFormFieldCommand::redo: field not found:" << m_oldProps.name << "— command marked obsolete";
            setObsolete(true);
            return;
        }
        const bool ok = m_engine->applyFieldSnapshot(m_doc->path(), m_new, m_doc->path());
        m_succeeded = ok;
        if (ok) {
            m_doc->markReload();
            setObsolete(false);
        } else {
            m_error = QObject::tr("editing form field %1 failed; document left unchanged").arg(m_oldProps.name);
            qWarning() << "EditFormFieldCommand::redo failed for" << m_oldProps.name << "— command marked obsolete, document not reloaded";
            setObsolete(true);
        }
    }

    void undo() override {
        if (!m_engine || !m_doc || m_doc->path().isEmpty()) return;
        if (!m_old.found) return; // nothing was ever applied
        const bool ok = m_engine->applyFieldSnapshot(m_doc->path(), m_old, m_doc->path());
        if (!ok) {
            qWarning() << "EditFormFieldCommand::undo failed for" << m_oldProps.name;
            return; // refresh state only after successful persistence
        }
        m_doc->markReload();
    }

    int id() const override { return 0x105; }

    const QString& originalName() const { return m_oldProps.name; }
    const EditFormFieldProperties& newProps() const { return m_newProps; }
    const EditFormFieldProperties& oldProps() const { return m_oldProps; }
    const FormFieldSnapshot& oldSnapshot() const { return m_old; }
    const FormFieldSnapshot& newSnapshot() const { return m_new; }

    /// R02: true when the most recent redo() persisted successfully.
    bool succeeded() const { return m_succeeded; }
    const QString& lastError() const { return m_error; }

private:
    IFormManager*            m_engine;
    DocumentSession*         m_doc;
    EditFormFieldProperties  m_newProps;
    EditFormFieldProperties  m_oldProps;  // carries the original name for introspection
    FormFieldSnapshot        m_old;       // captured ONCE at construction, before any mutation
    FormFieldSnapshot        m_new;       // derived from m_old: only panel-edited fields differ
    bool                     m_succeeded = false;
    QString                  m_error;
};
