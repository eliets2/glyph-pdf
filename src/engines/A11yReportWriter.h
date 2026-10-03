// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include "engines/AccessibilityChecker.h"

// ── Scorecard row 15 (wave 2b): EXPORTABLE accessibility results ────────────
// Turns the checker's A11yReport into a compliance artifact — a
// machine-readable CSV and a print-ready PDF summary — modeled on
// engines/ReviewSummaryWriter (same pure-static shape, same SafeSave
// candidate transaction, same standard-14 discipline via
// engines/Standard14Text.h).
//
// HONESTY CONTRACT (mirrors the checker's, pinned in tests):
//   * The artifact carries the checker's verdict, its capability
//     degradations and the standing disclosure — never a conformance
//     claim. Zero findings are worded "No gaps found by these checks…
//     not a PDF/UA verdict", exactly like the panel.
//   * A FAILED scan (loadOk=false) exports AS failed: the verdict names
//     the load error and no findings exist to invent. Never greenwashed
//     into a clean report.
//   * Bounded per-object samples carry their totals: "X of Y reported"
//     plus an explicit Truncated flag — truncation is disclosed, never
//     silent.
//
// CSV discipline (single-contract): every cell leaves through
// csvCell() = ConversionManager::csvCell — csvFormulaSafeCell (PGR-16/M3
// formula-injection hardening) + RFC-4180 quoting, ONE composition shared
// with the conversion and comments exporters so the three cannot drift at
// the composition layer — UTF-8, CRLF, the same emission discipline as
// CommentsWidget::csvEscapeField.
namespace gp {

class A11yReportWriter {
public:
    // Write the machine-readable CSV artifact. Returns false (with a reason
    // in *errorOut when non-null) if the output cannot be written.
    static bool writeCsv(const QString& outPath,
                         const A11yReport& report,
                         QString* errorOut = nullptr);

    // The full CSV payload as a string — the exact bytes writeCsv emits:
    // testable seam, like CommentsWidget::displayedCsv.
    static QString csvPayload(const A11yReport& report);

    // ONE cell-escaping contract: ConversionManager::csvCell
    // (csvFormulaSafeCell + RFC-4180 quoting — quote when the cell carries
    // '"', ',' or a newline; inner quotes doubled). Public so the escaping
    // contract is directly testable — the same reasoning as
    // CommentsWidget::csvEscapeField (both delegate to the ONE shared
    // composition).
    static QString csvCell(const QString& raw);

    // Write the print-ready PDF summary. Delivered through the SafeSave
    // candidate transaction (unique candidate → validate → atomic commit),
    // so an existing destination is only ever replaced by a validated
    // artifact. Same return contract as writeCsv.
    static bool writePdf(const QString& outPath,
                         const QString& docTitle,
                         const A11yReport& report,
                         QString* errorOut = nullptr);

    // The honest verdict sentence for `report` — the exact verdict line
    // both artifacts carry. Exposed so the wording contract is testable
    // without rendering anything.
    static QString verdictLine(const A11yReport& report);

    // The standing disclosure line both artifacts carry: detection only,
    // bounded samples, never a conformance verdict.
    static QString disclosureLine();

    // "HIGH" / "MEDIUM" / "LOW" — the same labels the panel displays.
    static QString severityLabel(A11ySeverity s);
};

} // namespace gp
