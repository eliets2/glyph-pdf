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
 *                            (one entry: page index 0, style name, startValue).
 *   - writeNumberTree()    → §9.9 P1 writer: serializes those entries into a
 *                            document catalog's /PageLabels dictionary (flat
 *                            /Nums tree; replaces any pre-existing tree).
 *
 * Scope (S, honest): one uniform labeling range per document — no per-range
 * UI, no /Kids branching, no in-place engine-resident mutation (the path
 * overload overwrites its input file; callers run it on a candidate path).
 * Bates numbering in this codebase stamps visible text only and never writes
 * /PageLabels, so the writer is the single owner of this catalog key.
 *
 * Label math contracts pinned by tests/TestPageLabels.cpp:
 *   - Decimal      : "startValue", "startValue+1", …
 *   - Roman (R/r)  : classic subtractive roman numerals, representable for
 *                    values 1..3999; beyond 3999 the label position is empty
 *                    (never silently wrong).
 *   - Letters (A/a): PDF repeated-letter cycles …Y, Z, AA, BB, … ZZ, AAA,
 *                    BBB … (ISO 32000-1 Table 159; each cycle repeats the
 *                    same letter — 27='AA', 28='BB', 52='ZZ', 53='AAA',
 *                    matching PDFium's decoder, NOT spreadsheet base-26).
 *   - Invalid input (pageCount <= 0 or startValue < 1) → empty output.
 */
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace PoDoFo { class PdfMemDocument; }

namespace gp {

// One /Nums key–value pair of the PDF /PageLabels number tree: the 0-based
// page index where a labeling range starts, the /S style name in force from
// that page on, and the /St numeric value of its first label.
struct PageLabelNumEntry {
    int     pageNum    = 0;
    QString style      = QStringLiteral("D");
    int     startValue = 1;

    // Value equality (used by tests to compare written vs. expected trees).
    friend bool operator==(const PageLabelNumEntry& a, const PageLabelNumEntry& b)
    {
        return a.pageNum == b.pageNum && a.style == b.style
            && a.startValue == b.startValue;
    }
};

namespace PageLabels {

enum class Style {
    Decimal,           // /S (D) — 1, 2, 3 …
    LowercaseRoman,    // /S (r) — i, ii, iii …
    UppercaseRoman,    // /S (R) — I, II, III …
    LowercaseLetters,  // /S (a) — a, b, … z, aa, bb … zz, aaa …
    UppercaseLetters   // /S (A) — A, B, … Z, AA, BB … ZZ, AAA …
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

// §9.9 P1 writer: create (or REPLACE) the document catalog's /PageLabels
// dictionary in `doc` with a proper /Nums number tree for `numberTreeEntries(
// startValue, style, pageCount)` — a single flat /Nums array covering all
// pages (valid per ISO 32000 7.9.3; /Kids is only needed for sparse trees).
// A pre-existing /PageLabels entry (and its stale /Nums) is removed first.
// /S and /St are always written explicitly. Returns false — touching nothing
// — for invalid input (pageCount <= 0 or startValue < 1) or a PoDoFo error.
bool writeNumberTree(PoDoFo::PdfMemDocument& doc, int startValue, Style style,
                     int pageCount);

// File convenience: write the tree for the document at `pdfPath` (the page
// count is taken from the document itself) and replace `pdfPath` with the
// labeled result. G13 (QUALITY-GATE-2026-09-09): the file is never loaded
// and saved over ITSELF — PoDoFo keeps the source device open for lazy
// stream loading, so a same-path save truncated content-bearing documents
// to 0 bytes. The mutation is serialized to a DISTINCT validated candidate
// and committed through the R01 safe-save primitives (SafeSave): on any
// failure the destination is byte-identical. Direct API callers cannot lose
// their file, and the PagesMode staged-candidate flow is itself safe.
bool writeNumberTree(const QString& pdfPath, int startValue, Style style);

} // namespace PageLabels
} // namespace gp
