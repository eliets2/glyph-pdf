// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include "core/interfaces/IPdfEditorEngine.h"     // OutlineEntry
#include "engines/pdfium/PdfiumBackend.h"         // TextRun (groupRuns input)

// ── T2-9: auto-bookmarks from text styles ───────────────────────────────────
// HeadingOutlineDetector turns page text runs into bookmark CANDIDATES. It is
// a HEURISTIC (the research row's honest basis): font size relative to the
// document's body size, bold styling, and TOC-page patterns (dot leaders +
// trailing number). The preview dialog discloses exactly this basis, lets the
// user edit titles / uncheck rows, and nothing is written until confirmed.
class HeadingOutlineDetector {
public:
    // One visual line of a page (runs merged by baseline proximity).
    struct TextLine {
        QString text;
        QRectF rect;        // Qt top-left user space
        double maxSize = 0; // largest run font size in the line (points)
        bool bold = false;  // any run's font name reports Bold
    };

    struct Candidate {
        QString title;
        int pageIndex = 0;      // fallback destination (where the heading is)
        int level = 1;          // 1..3 from the size ratio
        double fontSize = 0;
        bool fromToc = false;   // detected on a TOC page via dot-leader pattern
        int tocTargetPage = -1; // 0-based target parsed from the TOC line
    };

    static constexpr int kMaxCandidates = 500;

    // Merge PdfiumBackend::extractPageTextRuns output into visual lines
    // (baseline-Y proximity, in-order concatenation).
    static QList<TextLine> groupRuns(const QList<PdfiumBackend::TextRun>& runs);

    // Run the heuristic over every page's lines. Deterministic.
    static QList<Candidate> detect(const QList<QList<TextLine>>& pages);

    // Nest accepted candidates (page order) into an outline tree by level
    // (depth capped at 3). TOC candidates point at their parsed target page.
    static QList<OutlineEntry> buildOutlineTree(const QList<Candidate>& accepted);
};
