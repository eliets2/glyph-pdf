// SPDX-License-Identifier: Apache-2.0
#include "commands/AutoDetectPlacement.h"

#include <QObject>
#include <QUndoStack>

#include "commands/AddFormFieldCommand.h"
#include "engines/DocumentSession.h"
#include "core/interfaces/IFormManager.h"

namespace gp {

namespace {

// The compound operation for one auto-detect run. Its children are applied as
// they are added (each placement is attempted immediately, so a
// partially-failing run still keeps the successful fields and reports honest
// counts), so the INITIAL redo — performed by QUndoStack::push() — must not
// re-apply them; after a real undo the in-order child redo is what re-places
// the fields. undo() removes the placed fields last-first in one step.
class PlacedFieldsCompound final : public QUndoCommand {
public:
    explicit PlacedFieldsCompound(const QString& text) : QUndoCommand(text) {}
    ~PlacedFieldsCompound() override { qDeleteAll(m_children); }

    void adopt(AddFormFieldCommand* child) { m_children.append(child); }

    void redo() override {
        if (!m_childrenApplied) {
            m_childrenApplied = true;
            return;
        }
        for (AddFormFieldCommand* child : m_children)
            child->redo();
    }
    void undo() override {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
            (*it)->undo();
    }
private:
    QList<AddFormFieldCommand*> m_children;
    bool m_childrenApplied = false;
};

} // namespace

AutoDetectPlacement::Outcome
AutoDetectPlacement::apply(IFormManager* forms, DocumentSession* doc,
                           QUndoStack* stack,
                           const QList<FieldSuggestion>& suggestions,
                           int pageIndex) {
    Outcome outcome;
    if (!forms || !doc || !stack) return outcome;

    qint64 placed = 0;
    const QMetaObject::Connection placedCounter =
        QObject::connect(doc, &DocumentSession::reloadRequested,
                         [&placed]() { ++placed; });

    // The compound is only pushed when at least one placement succeeded, so
    // a fully-failed run never leaves an (empty) entry on the undo stack.
    std::unique_ptr<PlacedFieldsCompound> compound;
    int mapped = 0;
    for (const auto& s : suggestions) {
        AddFormFieldCommand::FieldType type = AddFormFieldCommand::FieldType::Text;
        if (s.type == QLatin1String("Text")) {
            type = AddFormFieldCommand::FieldType::Text;
        } else if (s.type == QLatin1String("Date")) {
            type = AddFormFieldCommand::FieldType::Date;
        } else if (s.type == QLatin1String("Checkbox")) {
            type = AddFormFieldCommand::FieldType::Checkbox;
        } else {
            continue;   // unsupported suggestion: never attempted, never counted
        }
        ++mapped;
        // Direct redo() applies the placement and lets us read the result
        // safely: a failed AddFormFieldCommand marks itself obsolete and
        // QUndoStack::push() would DELETE it, losing the outcome — so commands
        // are pushed only as already-applied children of OUR compound, and
        // the compound itself is the only command handed to the stack.
        AddFormFieldCommand cmd(forms, doc, type, pageIndex, s.rect, s.suggestedName);
        cmd.redo();
        if (cmd.succeeded()) {
            if (!compound)
                compound = std::make_unique<PlacedFieldsCompound>(
                    QObject::tr("Auto-detect form fields"));
            compound->adopt(new AddFormFieldCommand(forms, doc, type, pageIndex,
                                                    s.rect, s.suggestedName));
        }
    }
    QObject::disconnect(placedCounter);

    if (compound)
        stack->push(compound.release());   // initial redo is a no-op by design

    outcome.placed = static_cast<int>(placed);
    outcome.failed = mapped - outcome.placed;
    return outcome;
}

QString AutoDetectPlacement::statusMessage(int placed, int failed) {
    if (failed > 0 && placed > 0) {
        return QObject::tr("Auto-detect (experimental): placed %1 suggested field(s); "
                           "%2 could not be saved — Undo removes the placed fields as one step.")
            .arg(placed).arg(failed);
    }
    if (failed > 0) {
        return QObject::tr("Auto-detect (experimental): no suggested field could be "
                           "saved (%1 attempted) — document unchanged.").arg(failed);
    }
    if (placed == 0) {
        return QObject::tr("Auto-detect (experimental): no supported form fields "
                           "detected on this page.");
    }
    return QObject::tr("Auto-detect (experimental): placed %1 suggested field(s) for "
                       "review — Undo removes them as one step.").arg(placed);
}

} // namespace gp
