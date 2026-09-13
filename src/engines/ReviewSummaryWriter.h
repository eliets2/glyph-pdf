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
class ReviewSummaryWriter {
public:
    // ISO-32000-facing status label ("Open" / "Accepted" / "Rejected" /
    // "Cancelled" / "Completed" / "None") — the same review model the
    // annotation dictionary's /State carries.
    static QString statusLabel(ReviewState s);

    // Write the summary document. `comments` are the records to include (the
    // caller decides the scope); `docTitle` names the reviewed document on
    // the header block. Returns false (with a reason in *errorOut when
    // non-null) if the output cannot be written.
    static bool write(const QString& outPath,
                      const QString& docTitle,
                      const QList<AnnotationItem>& comments,
                      QString* errorOut = nullptr);

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
