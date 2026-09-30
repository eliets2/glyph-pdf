// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QList>
#include <QRectF>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

/// PatternRedactor — pure-static utility class for pattern-based redaction.
///
/// Uses PDFium per-character bounding boxes (via PdfiumBackend::searchText
/// infrastructure) to locate regex matches and map them to PDF user-space
/// rectangles that are consumed by the existing applyRedactions() pipeline.
///
/// Design constraints:
///  - No PoDoFo headers exposed (backend isolation)
///  - QRegularExpression only (no std::regex)
///  - Local variable named `tr` is NEVER used (shadows QObject::tr())
///  - Black-rectangle overlay is NEVER used; rects are fed to content-stream excision
class PatternRedactor {
public:
    /// PGR-52 (2026-09-30): the per-match compute budget, in PCRE2
    /// "match_limit" steps. Qt's QRegularExpression exposes NO match timeout
    /// (verified against the Qt 6.11 headers: PatternOption/MatchOption carry
    /// nothing of the sort), and PCRE2's own default MATCH_LIMIT was the only
    /// thing keeping a hostile shareable-preset pattern like `(a+)+$` from
    /// stalling the batch worker past the next cancel boundary — silently,
    /// because a limit-tripped match just yields "no match here" (a redaction
    /// FALSE NEGATIVE: text that must be excised survives). Matching now runs
    /// through the same PCRE2 engine Qt uses (the 16-bit build Qt6Core itself
    /// links), under an explicit per-match budget; a trip is an HONEST
    /// failure, never a silent skip. 10,000,000 is PCRE2's own default —
    /// every linear built-in pattern sits orders of magnitude below it, so
    /// benign corpora match exactly as before.
    static constexpr quint64 kDefaultMatchLimit = 10000000ULL;

    /// PGR-52: the bounded-search result — the per-page rectangles PLUS the
    /// honest-failure flag a caller with an error surface must consume.
    struct BoundedMatchResult {
        QHash<int, QList<QRectF>> byPage;
        bool budgetExceeded = false;
        int  exceededPage   = -1;   // the page whose match tripped the budget
    };

    /// Returns the QRegularExpression for one of the 12 built-in pattern names.
    /// Returns an invalid QRegularExpression if `name` is not recognised.
    static QRegularExpression namedPattern(const QString& name);

    /// Returns the 12 built-in pattern names in display order.
    static QStringList availablePatterns();

    /// Find all regex matches on a single PDF page.
    /// PGR-37 (D2 delta review 2026-09-23): coordinates are in RAW PDF USER
    /// space — FPDFText_GetCharBox boxes taken verbatim, stored y-up
    /// (QRectF::y() = the LOWER edge). This is the space the excision surgery
    /// (PoDoFoBackend::applyRedactionsUserSpace) operates in, so marks flow to
    /// the engine without a viewer transform. Viewers drawing these rects over
    /// the DISPLAYED page must convert at their boundary (top-origin display
    /// y = displayHeight − (userY + height) for /Rotate 0).
    /// Returns an empty list if PDFium is not available, the file cannot be opened,
    /// or the pattern is invalid.
    static QList<QRectF> findMatches(const QString& pdfPath,
                                     int             pageIndex,
                                     const QRegularExpression& pattern);

    /// Batch variant: find matches for a single pattern across many pages,
    /// opening (parsing) the PDF exactly ONCE. Equivalent to calling the
    /// single-page findMatches() for every entry in `pages`, but avoids the
    /// FPDF_LoadDocument()-per-page cost. Returns a map from page index to the
    /// rectangles found on that page (pages with no matches are omitted).
    /// Coordinates are in RAW PDF USER space (y-up) — see findMatches above.
    static QHash<int, QList<QRectF>> findMatches(const QString& pdfPath,
                                                 const QList<int>& pages,
                                                 const QRegularExpression& pattern);

    /// PGR-52: the bounded form of the batch search. Runs the pattern through
    /// the PCRE2 match budget (`matchLimit` steps per pcre2_match call) and
    /// reports a budget trip instead of silently yielding partial/no results.
    /// On budgetExceeded the caller must fail the operation honestly — the
    /// returned byPage payload is partial by definition. The matchLimit
    /// parameter is the SEAM: tests pin the honest failure with the default
    /// budget against a catastrophically backtracking pattern.
    static BoundedMatchResult findMatchesBounded(const QString& pdfPath,
                                                 const QList<int>& pages,
                                                 const QRegularExpression& pattern,
                                                 quint64 matchLimit = kDefaultMatchLimit);

private:
    struct CharInfo {
        QString ch;
        QRectF bbox;   // RAW PDF USER space (y-up: y() = the lower edge)
    };

    /// Extract per-character boxes for one page of an already-open FPDF_DOCUMENT.
    /// `docHandle` is an opaque FPDF_DOCUMENT (kept void* to avoid leaking PDFium
    /// headers into this interface). Only defined when HAS_PDFIUM is set.
    static QList<CharInfo> extractCharsFromOpenDoc(void* docHandle, int pageIndex);

    /// PGR-52: the bounded PCRE2 engine — the pattern compiled against the SAME
    /// 16-bit PCRE2 build Qt6Core links, with an explicit match_limit/depth
    /// limit match context. Defined in the .cpp (keeps pcre2 headers out of
    /// this interface). Invalid when the pattern is empty or the compile
    /// refused (Qt has already gated validity upstream — this is defensive).
    class BoundedMatcher;

    /// Run `matcher` over the reconstructed text of a single page's chars,
    /// returning the merged match rectangles. On a budget trip sets
    /// *budgetExceeded (never null when the caller consumes BoundedMatchResult)
    /// and returns the PARTIAL rectangles before the trip — callers convert
    /// that to an honest failure, never a silent skip.
    static QList<QRectF> matchChars(const QList<CharInfo>& chars,
                                    BoundedMatcher& matcher,
                                    bool* budgetExceeded);

    /// Merge per-character bounding boxes for the span [startIdx, endIdx) into one QRectF.
    static QRectF mergeCharBoxes(const QList<CharInfo>& chars, int startIdx, int endIdx);
};
