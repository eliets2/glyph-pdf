// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <QMap>
#include "engines/RedactOperation.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace gp {

// ── U05: pre-mutation summary (built by EITHER entry path) ──────────────────
struct RedactApplyPlan {
    QString sourcePath;                  // original — never written
    QString destinationPath;             // default: <base>_redacted.pdf
    QString sanitizedDestinationPath;    // default: <base>_redacted_sanitized.pdf
    int markCount = 0;                   // ToolMode::Redact annotations gathered
    QMap<int, int> marksPerPage;         // 0-based page -> mark count
    int sourcePageCount = 0;             // 0 when unknown (not loaded)
    bool sanitize = false;               // preset from the caller's checkbox
    // §9.8 P1: optional text drawn centered (white, 7pt) on every burn-in
    // box in the saved output — e.g. a redaction reason code. Empty = plain
    // black boxes (current behavior). Burn-in paint only; excision untouched.
    QString overlayText;
};

// Acrobat-shaped Apply step: mark/page counts, the actual sanitization choice,
// and destination pickers with normal overwrite handling — shown BEFORE any
// mutation. Replaces the plain "continue?" box in both entry paths.
class RedactApplyDialog : public QDialog {
    Q_OBJECT
public:
    explicit RedactApplyDialog(const RedactApplyPlan& plan, QWidget* parent = nullptr);

    // The user-adjusted result (destinations + sanitize choice).
    RedactApplyPlan plan() const;

    // Programmatic seams (tests offscreen; hosts prefilling from settings).
    void setDestinationPath(const QString& path);
    void setSanitizedDestinationPath(const QString& path);
    void setSanitizeChecked(bool on);
    void setOverlayText(const QString& text);   // §9.8 P1
    QString summaryText() const;

private slots:
    void browseDestination();
    void browseSanitizedDestination();
    void refreshState();

private:
    void buildUi();
    QString pickPdfFile(const QString& currentPath);

    RedactApplyPlan m_plan;

    QLabel*      m_docLabel = nullptr;          // "Document: X — N pages"
    QLabel*      m_summaryLabel = nullptr;      // "N marks on M pages, marked for removal"
    QLabel*      m_marksDetailLabel = nullptr;  // per-page breakdown + wording contract
    QCheckBox*   m_sanitizeCheck = nullptr;
    QLineEdit*   m_destinationEdit = nullptr;
    QLineEdit*   m_sanitizedDestinationEdit = nullptr;
    QLineEdit*   m_overlayEdit = nullptr;      // §9.8 P1: optional overlay text
    QLabel*      m_warningLabel = nullptr;      // reason the plan is invalid (or empty)
    QPushButton* m_okButton = nullptr;
};

// ── Shared U05 result presenter (BOTH entry paths) ──────────────────────────
//
// Terminal-state presentation for RedactOperation::finished. Every outcome gets
// explicit, artifact-specific text — the partial state is never replaced by a
// generic completion banner.
namespace RedactResultPresenter {

// Pure, unit-testable text builders.
QString bannerText(const RedactResult& result);  // one-line status for the status bar
QString detailText(const RedactResult& result);  // dialog body for the outcome

// What the host should do with the placed marks after presentation:
//   ClearMarks  — the redacted output was committed and kept (Completed, or
//                 Partial with Keep/Retry-sanitize chosen): the marks' effect
//                 is in the saved artifact.
//   RetainMarks — Partial with Discard chosen, Failed, or Canceled: the marks
//                 are still recoverable in the viewer for a clean retry.
enum class MarkDecision { ClearMarks, RetainMarks };

// Labeled dialogs for each outcome. For PartialRedactedOnly offers
// Retry-sanitize / Keep redacted file / Discard output (delete confirmed;
// never touches the source). Returns the host's mark decision.
RedactResultPresenter::MarkDecision present(QWidget* parent, const RedactResult& result);

} // namespace RedactResultPresenter
} // namespace gp
