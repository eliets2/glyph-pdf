// SPDX-License-Identifier: Apache-2.0
#pragma once

// §9.8/U05 default-ON contract (review D07): both redaction entry paths — the
// dedicated Redact mode and the Security controller — seed their sanitize
// choice from this one policy. Explicit user opt-out is preserved in the
// dialog; only the INITIAL state is shared.
inline constexpr bool kDefaultSanitizeOn = true;
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
    // T1-2 Redaction Proof Mode: after the commit, verify the saved artifacts
    // (survival sweep over every surface) and write a proof pack
    // (<dest>_redaction-proof.json/.txt) the user can hand to counsel.
    // Default ON — the proof is the honest face of the excision engine.
    bool produceProof = true;
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
    void setProduceProofChecked(bool on);       // T1-2 proof pack
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
    QCheckBox*   m_proofCheck = nullptr;        // T1-2: generate the proof pack
    QLineEdit*   m_destinationEdit = nullptr;
    QLineEdit*   m_sanitizedDestinationEdit = nullptr;
    QLineEdit*   m_overlayEdit = nullptr;      // §9.8 P1: optional overlay text
    QLabel*      m_warningLabel = nullptr;      // reason the plan is invalid (or empty)
    QPushButton* m_okButton = nullptr;
};

// ── Shared plan→request conversion (BOTH entry paths) ───────────────────────
// N04 (review 2026-09-07): RedactMode and SecurityController both turn the
// Apply dialog's chosen plan into the operation's RedactRequest. Duplicating
// that field copy is exactly what dropped overlayText on the Security path
// (the dialog accepted a label the operation never received). This ONE
// conversion is the seam both callers must go through, so a plan field can
// never silently disappear on one entry path again.
RedactRequest redactRequestFromPlan(const RedactApplyPlan& plan,
                                    const QMap<int, QList<QRectF>>& marksByPage);

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
//                 Partial with Keep chosen, or a Partial whose Retry-sanitize
//                 SUCCEEDED): the marks' effect is in the saved artifacts.
//   RetainMarks — Partial with Discard chosen or a FAILED Retry, Failed, or
//                 Canceled: the marks are still recoverable in the viewer for
//                 a clean retry.
enum class MarkDecision { ClearMarks, RetainMarks };

// Labeled dialogs for each outcome. For PartialRedactedOnly offers
// Retry-sanitize / Keep redacted file / Discard output (delete confirmed;
// never touches the source). Returns the host's mark decision.
//
// D01: when `resultAfterRecovery` is non-null it ALWAYS receives the effective
// terminal result — identical to `result`, except that a SUCCESSFUL
// Retry-sanitize upgrades it to Completed with the committed sanitized
// destination. Hosts build their status banner from this effective result, so
// a recovered flow is never re-announced with the original failure wording.
RedactResultPresenter::MarkDecision present(QWidget* parent, const RedactResult& result,
                                            RedactResult* resultAfterRecovery = nullptr);

} // namespace RedactResultPresenter
} // namespace gp
