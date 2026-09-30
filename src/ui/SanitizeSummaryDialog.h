// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <QHash>
#include "core/SanitizeTypes.h"

class QCheckBox;
class QLabel;
class QPushButton;

// Pre-commit summary for Sanitize Document (PARITY-SCORECARD-2026-09-30 §4 #4,
// July rows 72-73). Lists, per category, what the classify walk found, with
// checkboxes: checked = removed from the saved copy, unchecked = kept exactly
// as it is. All-checked is the default — today's all-or-nothing behavior. A
// zero-checked commit is refused honestly: removing nothing is a no-op, not a
// sanitize. The dialog states plainly what will be removed and what will
// remain; the counts come from the same traversal the removal runs.
class SanitizeSummaryDialog : public QDialog {
    Q_OBJECT
public:
    explicit SanitizeSummaryDialog(const SanitizePlan &plan, QWidget *parent = nullptr);

    // The categories the user kept checked (what the commit will remove).
    SanitizeCategories selectedCategories() const;

    // Honest-commit gate: false + `refusalReason` when the commit must not run
    // (nothing found, or zero categories checked — there would be nothing to
    // remove). The OK path goes through this; a refused commit leaves the
    // document and the dialog untouched.
    bool canCommit(QString *refusalReason = nullptr) const;

private:
    void refreshCommitControls();

    QHash<int, QCheckBox *> m_boxes;   // int(SanitizeCategory) -> checkbox
    QHash<int, int> m_counts;          // int(SanitizeCategory) -> found count
    QLabel *m_remainingLabel = nullptr;
    QPushButton *m_commitButton = nullptr;
};
