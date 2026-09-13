// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QRectF>
#include <QRegularExpression>
#include <QString>

// ── T2-2: Find & Replace matcher ────────────────────────────────────────────
//
// TextMatchFinder locates exact / regex / case-sensitive / whole-word matches
// in a PDF's REAL text layer and reports, for every match, the geometry and
// font metrics the replace pipeline needs:
//
//   rect      — Qt top-left user-space union of the matched glyphs' boxes
//               (the same coordinate contract PatternRedactor and
//               PoDoFoBackend::applyRedactions already share), and
//   fontSize  — the largest per-character font size in the match (points), so
//               the replacement can be drawn at the matched text's size, and
//   text      — the matched substring (decoded Unicode through PDFium, never
//               raw glyph codes).
//
// Design follows PatternRedactor (pure-static, no PoDoFo/PDFium header leaks,
// ReDoS-bounded matching). The REPLACE side lives in the editor engine
// (ITextReplacer::replaceTextRegions); this class only FINDS.
struct TextMatch {
    int pageIndex = 0;
    QRectF rect;          // Qt top-left user space (origin top-left, Y down)
    QString text;         // matched substring (decoded)
    double fontSize = 0;  // points; 0 when the page carries no size info
};

class TextMatchFinder {
public:
    // Build the search pattern from the user options. Mirrors the FindBar
    // semantics (EditController::pageTextPattern) but self-contained so the
    // engine-side replace pipeline cannot drift from the dialog:
    //   useRegex    — raw pattern (wrapped in \b guards when wholeWords)
    //   otherwise   — QRegularExpression::escape(search)
    //   matchCase   — CaseInsensitiveOption when false
    //   wholeWords  — \b(?:...)\b guards
    // Returns an invalid QRegularExpression for a syntactically bad pattern
    // (check isValid(); errorString() carries the reason).
    static QRegularExpression buildPattern(const QString& search, bool matchCase,
                                           bool wholeWords, bool useRegex);

    // Find every match of `pattern` on the given 0-based pages, parsing the
    // PDF exactly once. Pages with no matches are omitted from the result.
    // Coordinates are Qt top-left user space. Matching is ReDoS-bounded
    // (input length cap + wall-clock budget per page, mirroring
    // PatternRedactor::matchChars) — a pathological pattern yields partial
    // results with a qWarning, never a hang.
    static QList<TextMatch> findMatches(const QString& pdfPath,
                                        const QList<int>& pages,
                                        const QRegularExpression& pattern);

    // T2-2 honesty seam: per-match reflow/geometry warnings for a planned
    // replacement. A warning is emitted when the replacement's length differs
    // from the matched text's length (a drawn-width estimate is impossible
    // before the engine chooses the standard-14 substitute font; the engine
    // reports MEASURED drawn widths after apply — see replaceTextRegions).
    struct ReflowWarning {
        int pageIndex = 0;
        QString matched;      // the text that will be excised
        QString replacement;  // the text that will be drawn
    };
    static QList<TextMatchFinder::ReflowWarning> reflowWarnings(
        const QList<TextMatch>& matches, const QString& replacement);
};

// ── T2-2: one find&replace request / outcome ───────────────────────────────
// Shared by EditController (the canonical pipeline) and FindReplaceDialog —
// the dialog lives in the ui layer and must not depend on the shell layer,
// so the payload types live here beside TextMatch.
struct ReplaceOptions {
    QString searchText;
    QString replaceText;
    bool matchCase = false;
    bool wholeWords = false;
    bool useRegex = false;
    QList<int> pages;   // 0-based inclusive scope; empty = ALL pages
};

struct ReplaceOutcome {
    bool ok = false;            // pipeline ran (matches found ≠ required)
    int requested = 0;          // matches found in scope
    int applied = 0;            // replacements drawn
    int widthChanged = 0;       // engine-measured drawn width differs
    QString firstChangedPage;   // "page N" of the first width change ('' if none)
    QString message;            // human-readable summary / failure reason
    QList<TextMatch> matches;   // the matches found in scope (for dialogs)
};
