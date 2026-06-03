// SPDX-License-Identifier: Apache-2.0
#pragma once
//
// PdfPageOps — thin free-function wrappers for stand-alone PoDoFo page
// manipulation that does not go through a loaded DocumentSession.
//
// All functions operate directly on file paths (load → mutate → save).
// They are implemented in PdfPageOps.cpp (which includes <podofo/podofo.h>)
// so that callers in pdfws_ui do NOT need to include PoDoFo headers.
//
#include <QString>
#include <QStringList>
#include <QList>
#include <QRectF>

namespace gp {

/// Extract pages [from, to] (0-based, inclusive) from inputPath into outputPath.
bool extractPages(const QString& inputPath, int from, int to, const QString& outputPath);

/// Delete pages [from, to] (0-based, inclusive) from inputPath, writing to outputPath.
bool deletePages(const QString& inputPath, int from, int to, const QString& outputPath);

/// Insert a blank A4 page at position atIndex (0-based) in inputPath,
/// writing to outputPath.
bool insertBlankPage(const QString& inputPath, int atIndex, const QString& outputPath);

/// Rotate pages [from, to] (0-based, inclusive) by angle degrees in inputPath,
/// writing to outputPath.
bool rotatePages(const QString& inputPath, int from, int to, int angle,
                 const QString& outputPath);

/// Apply redactions (black fill boxes + text-operator scrub) to the pages
/// identified by the rects map (key = 0-based page index), writing to outputPath.
/// The coordinates are in PDF user-space (origin bottom-left).
bool applyRedactionsToFile(const QString& inputPath,
                           const QMap<int, QList<QRectF>>& rectsByPage,
                           const QString& outputPath);

/// Merge all files in inputs into outputPath.
bool mergeDocuments(const QStringList& inputs, const QString& outputPath);

} // namespace gp
