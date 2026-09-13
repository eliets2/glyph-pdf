// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QRectF>
#include <QRegularExpression>
#include <QString>
#include <atomic>

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

    // packa-F4: cooperative budget for ONE findMatches job (all pages).
    //
    // HONEST bound contract — do not overstate it:
    //   * a SINGLE QRegularExpression (PCRE2) match cannot be interrupted
    //     from this synchronous API. It is bounded only by the page-text
    //     input cap (256 KB below) and PCRE2's internal match/depth limits,
    //     which make a pathological pattern FAIL the match rather than spin
    //     forever — but a single attempt can still cost seconds.
    //   * `cancelled` and `deadlineMs` stop the scan BETWEEN matches and
    //     BETWEEN pages; with them, findMatches returns PARTIAL results.
    //     Callers must never present the deadline as a wall-clock guarantee
    //     for the whole call.
    // `cancelled` is atomic so a future dispatch layer may flip it from any
    // thread — no hidden workers are created here.
    struct MatchBudget {
        qint64 deadlineMs = 1500;   // per JOB (was per page before packa-F4)
        std::atomic_bool cancelled{false};
        bool shouldStop(qint64 elapsedNs) const {
            return cancelled.load(std::memory_order_relaxed)
                || elapsedNs > deadlineMs * 1000000;
        }
    };

    // Find every match of `pattern` on the given 0-based pages, parsing the
    // PDF exactly once. Pages with no matches are omitted from the result.
    // Coordinates are Qt top-left user space. ReDoS bounding (see
    // MatchBudget for the exact, honest limits): the input cap bounds each
    // single PCRE2 attempt; the job-scoped deadline and cancellation flag
    // stop the scan between matches/pages — a pathological pattern yields
    // PARTIAL results with a qWarning, and callers must not advertise a
    // hard wall-clock bound (packa-F4 honesty rule).
    static QList<TextMatch> findMatches(const QString& pdfPath,
                                        const QList<int>& pages,
                                        const QRegularExpression& pattern,
                                        MatchBudget* budget = nullptr);

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
    // packa-F1: an empty `pages` list legitimately means "all pages", so an
    // UNUSABLE scope (malformed range, range outside the document) must be
    // its own state — never encoded as the same empty list. Producers set
    // scopeValid=false for a refused scope; EditController::replaceAllInDocument
    // refuses scopeValid=false with zero mutation instead of widening to the
    // whole document.
    bool scopeValid = true;
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
