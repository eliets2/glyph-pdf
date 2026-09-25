// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>
#include <QImage>
#include <QStringList>
#include <functional>
#include "engines/MyersDiff.h"

struct PageDiff {
    int         pageIndex      = 0;   ///< legacy single position (doc1 side; kept for
                                      ///< back-compat rows that predate the alignment)
    int         oldPage        = -1;  ///< R06: 0-based index in doc1 (aligned pair; >= 0
                                      ///< for every engine-produced row)
    int         newPage        = -1;  ///< R06: 0-based index in doc2 (aligned pair; >= 0
                                      ///< for every engine-produced row)
    QImage      diffImage;           ///< visual pixel-diff overlay (R11: retained
                                      ///< ONLY for pairs with pixelDiffCount > 0 and
                                      ///< within the overlay pixel ceiling — an
                                      ///< unchanged or skipped pair carries a null image)
    QStringList textRemoved;         ///< tokens deleted (non-move deletes)
    QStringList textAdded;           ///< tokens inserted (non-move inserts)
    QList<MoveOperation> moves;      ///< tokens that moved position
    int         pixelDiffCount = 0;
    bool        textDiffTruncated = false;  ///< R11 (PERF-03): the word diff for
                                      ///< this pair exceeded its resource budget and
                                      ///< was completed with the coarse (non-minimal)
                                      ///< fallback — callers must disclose this
                                      ///< rather than presenting it as an exact diff.

    /// R06: resolved sides. Rows produced by the engine always carry the
    /// explicit old/new alignment; synthetic or back-compat rows that only
    /// set the legacy single index compare i-to-i through the fallback.
    int oldSide() const { return oldPage >= 0 ? oldPage : (newPage >= 0 ? -1 : pageIndex); }
    int newSide() const { return newPage >= 0 ? newPage : (oldPage >= 0 ? -1 : pageIndex); }
};

struct DiffResult {
    bool isIdentical = false;
    int  pageCount1  = 0;
    int  pageCount2  = 0;
    /// R11: set when the operation was abandoned through its cancellation
    /// probe. The result is a partial artifact (whatever was computed before
    /// cancellation) — consumers must treat it as incomplete, never as a
    /// complete "no changes" report.
    bool cancelled   = false;
    /// R06 (PERF-01): one row per two-sided page pair of THE alignment
    /// mapping, in doc1 order — each row compares its old page with ITS
    /// aligned new page (oldPage/newPage), never an index-wise neighbor, so
    /// an inserted page never turns unchanged matched pages into false
    /// content changes. Content comparison, navigation and the exported
    /// reports all read this one mapping (plus pageChanges for the one-sided
    /// remainder).
    QList<PageDiff> pages;

    /// A page from doc1 that appears at a different index in doc2 (reorder).
    struct PageMove { int fromPage; int toPage; QString excerpt; };
    QList<PageMove> pageMoves;

    /// R11: explicit structural page changes. A page can exist on only one
    /// side (added/removed) or on both sides at different positions (moved).
    /// A side with no page carries -1 — an explicit "missing" marker, never a
    /// valid page-zero sentinel (check hasOldSide()/hasNewSide() first).
    enum class PageChangeType { PageAdded, PageRemoved, PageMoved };
    struct PageChange {
        PageChangeType type = PageChangeType::PageAdded;
        int oldPage = -1;   ///< 0-based index in doc1; -1 = no such page (added)
        int newPage = -1;   ///< 0-based index in doc2; -1 = no such page (removed)
        QString excerpt;    ///< start of the page's text ("" for blank pages)
        bool hasOldSide() const { return oldPage >= 0; }
        bool hasNewSide() const { return newPage >= 0; }
    };
    /// Single canonical STRUCTURAL sequence, in deterministic page order.
    /// Built by a three-stage alignment: pages are matched across documents by
    /// exact deterministic fingerprints (SHA-256 over normalized extracted
    /// text — a page inserted in the middle surfaces as ONE PageAdded at its
    /// true position, never as a remove+add chain), then leftovers fall back
    /// to fuzzy word-set matching (moved pages appear here exactly once), and
    /// finally the remaining leftover pairs align in order as MODIFIED pages
    /// (V04): a page rewritten in place is a content change carried by
    /// result.pages, deliberately absent here — low text similarity is not
    /// proof that the page structure changed, so only the one-sided leftover
    /// pages are PageRemoved / PageAdded.
    /// Pages with no extractable text (image-only) have no fingerprint and
    /// keep the fuzzy/index-wise fallback semantics. The CHANGES tree, the
    /// change-type filters, the next/previous sequence, the status totals and
    /// the exported reports all read this one list (moved pages appear here
    /// exactly once — pageMoves above stays populated only for backward
    /// compatibility).
    QList<PageChange> pageChanges;
};

class DiffEngine {
public:
    DiffEngine();
    ~DiffEngine();

    /// R11 (PERF-02/03) resource contracts, all driven by the optional
    /// \p cancelled probe (return true to abandon the operation):
    ///   - file hashes are streamed in bounded chunks (never readAll),
    ///   - page alignment never allocates an unbounded LCS matrix,
    ///   - pixel overlays are retained ONLY for pairs whose comparison
    ///     found changed pixels, within a hard pixel ceiling,
    ///   - the word diff runs inside MyersDiff's trace budget with an
    ///     honest per-pair truncation flag (PageDiff::textDiffTruncated).
    /// Checks run inside every expensive loop (hashing, extraction,
    /// alignment, per-pair diff, pixel scanning); an abandoned comparison
    /// returns a partial result with DiffResult::cancelled set.
    DiffResult compare(const QString &file1, const QString &file2, int dpi = 150,
                       const std::function<bool()> &cancelled = {});
};
