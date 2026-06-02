// SPDX-License-Identifier: Apache-2.0
#pragma once

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

private:
    struct CharInfo {
        QChar  ch;
        QRectF bbox;   // Qt user-space coordinates (origin top-left)
    };

    /// Extract all characters with their per-character bounding boxes from a page.
    /// Uses FPDFText_LoadPage / FPDFText_GetCharBox internally.
    static QList<CharInfo> extractCharsWithPositions(const QString& pdfPath,
                                                      int            pageIndex);

    /// Merge per-character bounding boxes for the span [startIdx, endIdx) into one QRectF.
    static QRectF mergeCharBoxes(const QList<CharInfo>& chars, int startIdx, int endIdx);
};
