// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QString>

class QUndoStack;
class DocumentSession;
class IFormManager;
struct FieldSuggestion;

// V06 (PARITY-BRANCH-REVIEW-2026-09-05): auto-detected form fields used to be
// placed by commands that were redone and dropped — never pushed on the
// application's undo stack — while the success message promised
// "undo ... as needed" and a partial failure claimed the document was
// unchanged. AutoDetectPlacement is the ONE seam that places suggested fields
// through the application's undo stack as a single compound command, reports
// honest placed/failed counts, and builds a status message that only promises
// what really happened. FormsController feeds it the viewer's document and
// the shared AppContext stack.
namespace gp {

class AutoDetectPlacement {
public:
    // How many suggested fields were placed vs could not be saved.
    struct Outcome { int placed = 0; int failed = 0; };

    // Places the mappable suggestions through ONE compound command on the
    // application's undo stack (a single Undo removes every field the run
    // placed; failed placements never join the compound).
    static Outcome apply(IFormManager* forms, DocumentSession* doc,
                         QUndoStack* stack,
                         const QList<FieldSuggestion>& suggestions,
                         int pageIndex);

    // The status message may promise only what really happened: partial
    // success names the counts and the working one-step Undo; a total failure
    // is the only case that may claim the document is unchanged.
    static QString statusMessage(int placed, int failed);
};

} // namespace gp
