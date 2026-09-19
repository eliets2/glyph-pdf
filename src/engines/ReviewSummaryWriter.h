// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QList>
#include "core/AnnotationTypes.h"

// ── T2-3: review-summary DOCUMENT ───────────────────────────────────────────
// Generates the review-summary PDF the competitive synthesis requires
// (Acrobat summary export, PDF-XChange summarize-to-PDF, Bluebeam Markups
// Summary): a standalone, printable document built from the comment records
// — grouped by page, ordered by author then creation date inside each page —
// carrying each comment's status (ISO 32000 §12.5.6.4 review states), author,
// timestamps and full text, plus a header block with per-status totals.
//
// Pure-static, no UI dependency; the CommentsWidget panel exports the
// DISPLAYED scope (its active filter result) — the same contract its CSV
// export already follows — and tests exercise real artifacts.
//
// ── R27 tail: printable surface ─────────────────────────────────────────────
// writePrintable is the print-ready face of the SAME content model (no second
// derivation path): numbered entries behind a table of entries, per-sheet
// footers naming the sheet (real pagination furniture), a distinct-author
// markup count, and an honesty line for the redaction-proof section — listed
// with its verdict and the pack's OWN generation time when a proof pack is
// available, listed as NOT AVAILABLE when it is not (never silently omitted).
// BOTH write seams deliver through the SafeSave candidate transaction
// (unique candidate → validate → atomic commit), so an existing destination
// is only ever replaced by a validated artifact and a failed write leaves it
// byte-identical — never overwritten outside the guard.
class ReviewSummaryWriter {
public:
    // Optional sections for the printable surface.
    struct PrintOptions {
        // Redaction-proof pack (JSON) as written by the redaction
        // transaction beside a committed redacted output
        // (<base>_redaction-proof.json). Empty/absent => the header lists
        // the section as not available instead of omitting it.
        QString proofPackPath;
    };

    // ISO-32000-facing status label ("Open" / "Accepted" / "Rejected" /
    // "Cancelled" / "Completed" / "None") — the same review model the
    // annotation dictionary's /State carries.
    static QString statusLabel(ReviewState s);

    // Write the summary document. `comments` are the records to include (the
    // caller decides the scope); `docTitle` names the reviewed document on
    // the header block. Returns false (with a reason in *errorOut when
    // non-null) if the output cannot be written. Delivered through the
    // SafeSave candidate transaction (see class comment).
    static bool write(const QString& outPath,
                      const QString& docTitle,
                      const QList<AnnotationItem>& comments,
                      QString* errorOut = nullptr);

    // Printable surface: the same document with numbered entries + table of
    // entries, per-sheet footers and the redaction-proof availability line
    // driven by `options`. Same transaction/return contract as write().
    static bool writePrintable(const QString& outPath,
                               const QString& docTitle,
                               const QList<AnnotationItem>& comments,
                               const PrintOptions& options,
                               QString* errorOut = nullptr);

    // The header's redaction-proof availability line for `proofPackPath` —
    // verdict + excision count + the pack's own generation time when the
    // pack is readable; the honest "not available" wording when it is not.
    // Exposed so the honesty contract is testable without rendering a PDF.
    static QString proofSummaryLine(const QString& proofPackPath);

    // Testable layout seam: one rendered text block per entry, exactly as it
    // is drawn into the PDF — lets tests assert the grouping/label/timestamp
    // content without parsing the PDF back through a renderer.
    struct RenderedEntry {
        int pageIndex = 0;         // 0-based
        QString heading;           // "[Accepted] alice — 2026-09-09T10:00:00"
        QString body;              // wrapped comment text
    };
    static QList<RenderedEntry> renderEntries(const QList<AnnotationItem>& comments);
};
