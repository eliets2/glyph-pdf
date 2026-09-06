// SPDX-License-Identifier: Apache-2.0
/**
 * PageLabels — §9.9 P1 groundwork: pure generation of PDF /PageLabels
 * number-tree entries and the matching human-readable label strings for a
 * single labeling range (startValue, style, pageCount).
 *
 * What this seam covers
 * ---------------------
 *   - styleName()          → the PDF /S name for a style ("D","r","R","a","A",
 *                            ISO 32000 Table 159).
 *   - labelsFor()          → the per-page label strings, e.g. startValue=4,
 *                            LowercaseRoman, 3 pages → "iv","v","vi".
 *   - numberTreeEntries()  → the minimal /Nums array shape for the range
 *                            (one entry: page index 0, style name, startValue),
 *                            ready for a future writer to serialize into the
 *                            document catalog's /PageLabels dictionary.
 *
 * Scope (S, honest): data only — this seam performs NO document I/O. Bates
 * numbering in this codebase stamps visible text only and never writes
 * /PageLabels (verified against PoDoFoBackend::applyBatesNumbering), so there
 * was no writer to extend; a future Page-Labels mode needs its own package
 * (UI + writer). See the groundwork commit message for the full scoping note.
 *
 * Label math contracts pinned by tests/TestPageLabels.cpp:
 *   - Decimal      : "startValue", "startValue+1", …
 *   - Roman (R/r)  : classic subtractive roman numerals, representable for
 *                    values 1..3999; beyond 3999 the label position is empty
 *                    (never silently wrong).
 *   - Letters (A/a): bijective base-26 …Y, Z, AA, AB… (1-based, no zero).
 *   - Invalid input (pageCount <= 0 or startValue < 1) → empty output.
 */
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace gp {

// One /Nums key–value pair of the PDF /PageLabels number tree: the 0-based
// page index where a labeling range starts, the /S style name in force from
// that page on, and the /St numeric value of its first label.
struct PageLabelNumEntry {
    int     pageNum    = 0;
    QString style      = QStringLiteral("D");
    int     startValue = 1;
};

namespace PageLabels {

enum class Style {
    Decimal,           // /S (D) — 1, 2, 3 …
    LowercaseRoman,    // /S (r) — i, ii, iii …
    UppercaseRoman,    // /S (R) — I, II, III …
    LowercaseLetters,  // /S (a) — a, b, … z, aa …
    UppercaseLetters   // /S (A) — A, B, … Z, AA …
};

// The PDF /S name for a style (ISO 32000 Table 159).
QString styleName(Style style);

// Per-page label strings for `pageCount` consecutive pages whose first label
// is `startValue`. Returns pageCount strings, or an empty list for invalid
// input (pageCount <= 0 or startValue < 1). Roman positions whose value
// exceeds 3999 are empty strings (unrepresentable, honestly blank).
QStringList labelsFor(int startValue, Style style, int pageCount);

// The minimal /PageLabels number-tree entries covering a whole document
// labeled uniformly: one {0, styleName(style), startValue} entry for a
// non-empty valid range; empty for invalid input.
QList<PageLabelNumEntry> numberTreeEntries(int startValue, Style style, int pageCount);

} // namespace PageLabels
} // namespace gp
