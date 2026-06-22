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
    /// Returns the QRegularExpression for one of the 12 built-in pattern names.
    /// Returns an invalid QRegularExpression if `name` is not recognised.
    static QRegularExpression namedPattern(const QString& name);

    /// Returns the 12 built-in pattern names in display order.
    static QStringList availablePatterns();

    /// Find all regex matches on a single PDF page.
    /// Coordinates are in Qt PDF user-space (origin top-left, same as PdfViewerWidget).
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
    /// Coordinates are in Qt PDF user-space (origin top-left).
    static QHash<int, QList<QRectF>> findMatches(const QString& pdfPath,
                                                 const QList<int>& pages,
                                                 const QRegularExpression& pattern);

private:
    struct CharInfo {
        QString ch;
        QRectF bbox;   // Qt user-space coordinates (origin top-left)
    };

    /// Extract all characters with their per-character bounding boxes from a page.
    /// Uses FPDFText_LoadPage / FPDFText_GetCharBox internally. Opens the document.
    static QList<CharInfo> extractCharsWithPositions(const QString& pdfPath,
                                                      int            pageIndex);

    /// Extract per-character boxes for one page of an already-open FPDF_DOCUMENT.
    /// `docHandle` is an opaque FPDF_DOCUMENT (kept void* to avoid leaking PDFium
    /// headers into this interface). Only defined when HAS_PDFIUM is set.
    static QList<CharInfo> extractCharsFromOpenDoc(void* docHandle, int pageIndex);

    /// Run `pattern` over the reconstructed text of a single page's chars,
    /// returning the merged match rectangles. Carries the M-3 ReDoS bounds.
    static QList<QRectF> matchChars(const QList<CharInfo>& chars,
                                    const QRegularExpression& pattern);

    /// Merge per-character bounding boxes for the span [startIdx, endIdx) into one QRectF.
    static QRectF mergeCharBoxes(const QList<CharInfo>& chars, int startIdx, int endIdx);
};
