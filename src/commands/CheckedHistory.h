// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QUndoStack>
#include <QUndoCommand>

// G08 (QUALITY-GATE-2026-09-09) — the checked history-traversal boundary.
//
// Qt gives undo() NO failure channel: QUndoStack::undo() calls the current
// command's undo() and moves the index UNCONDITIONALLY (qtbase
// qundostack.cpp; verified for 5.15 through 6.x — see
// .context/research/step3-persistence-topics.md §4). A restoration that fails
// (engine refusal, safe-save commit fault, read-only destination) therefore
// used to advance history into a false state: stack index back at the clean
// point, isClean() true, canUndo() false — while the document on disk still
// carried the mutation, and the failed command was no longer reachable for a
// retry. Emitting DocumentSession::mutationFailed reports the failure; it
// cannot repair history, and manual index surgery is forbidden (setIndex()
// re-runs undo/redo of intervening commands).
//
// The boundary therefore lives ABOVE QUndoStack, in the command/session
// layer, and is the ONE traversal entry every undo caller must use:
//
//   1. restoreChecked() performs the REAL restoration while the index is
//      untouched. On failure it reports mutationFailed and returns false —
//      the command stays CURRENT (top of stack), the clean state is
//      untouched, and the traversal is RETRYABLE.
//   2. Only after a successful restoration does the boundary call
//      QUndoStack::undo() to move the index. The command's undo() sees the
//      armed flag and no-ops, so the restoration is applied exactly once.
//
// Commands that do not opt in (plain QUndoCommand) keep the plain Qt
// semantics via the dynamic_cast fallback.

// Base class for commands whose undo() is a real RESTORATION that can fail.
class CheckedUndoCommand : public QUndoCommand {
public:
    using QUndoCommand::QUndoCommand;

    // Perform the restoration WITHOUT moving the history index. Returns
    // false when the document could not be restored: nothing was traversed,
    // the command remains current, and the caller may retry.
    virtual bool restoreChecked() = 0;

protected:
    // Mark the restoration as applied so the follow-up QUndoStack::undo()
    // (which re-invokes undo() to move the index) does not restore twice.
    void armCheckedRestore() { m_armed = true; }

    // Consume a pending arm inside undo(). Returns true when this undo()
    // call is the index-move follow-up of a checked traversal — the real
    // restoration already happened in restoreChecked().
    bool consumeArmedRestore()
    {
        if (!m_armed)
            return false;
        m_armed = false;
        return true;
    }

private:
    bool m_armed = false;
};

namespace CheckedHistory {

// Attempt to undo the current command with a checked restoration.
// Returns false when nothing was traversed: no document command was current,
// or the restoration failed (command stays current, index unchanged,
// retryable).
inline bool undo(QUndoStack *stack)
{
    if (!stack || stack->index() == 0)
        return false;
    QUndoCommand *cmd = const_cast<QUndoCommand *>(
        stack->command(stack->index() - 1));   // command() hands back const
    if (auto *checked = dynamic_cast<CheckedUndoCommand *>(cmd)) {
        if (!checked->restoreChecked())
            return false;   // G08: history position unchanged — retryable
        // Restoration succeeded and is armed; undo() will only consume the
        // arm. This moves the history position after the fact — the only
        // index movement, and only for a restoration that really happened.
        stack->undo();
        return true;
    }
    stack->undo();   // legacy commands: plain Qt semantics
    return true;
}

} // namespace CheckedHistory

