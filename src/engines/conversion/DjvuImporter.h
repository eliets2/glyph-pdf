// SPDX-License-Identifier: Apache-2.0
// src/engines/conversion/DjvuImporter.h
// DjvuImporter — optional DjVu → MRC PDF/A importer.
//
// Design constraints (M7-P3 D6):
//   - Gated by HAS_DJVU CMake option (default OFF).
//   - IMPORT ONLY — no DjVu output (excluded per ROADMAP §"DjVu as output format").
//   - DjVuLibre license: GPL-2.0+. When HAS_DJVU=ON the user explicitly accepts
//     the GPL-2.0+ dependency. See docs/MRC-ENCODER-LICENSE-AUDIT.md §3.
//   - The importer extracts: bitonal foreground layers → JBIG2, background layers
//     → JPEG2000, and hidden text (if present in the DjVu) → sandwich text.
//     Output is assembled as a PDF/A-2b MRC document via MrcPageProcessor.
//
// When HAS_DJVU is OFF (the default), this file compiles to a stub that always
// returns an empty result with a "DjVu support not built" message.
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QImage>

/// Result of a DjVu import operation.
struct DjvuImportResult {
    bool    success     = false;
    QString errorMessage;

    /// Rendered page images from the DjVu document.
    /// Empty if import failed or HAS_DJVU is not defined.
    QList<QImage> pageImages;

    /// Page count from the DjVu document (0 if failed).
    int pageCount = 0;

    /// Embedded text (from DjVu hidden text layer), one entry per page.
    /// Each entry is the raw text for that page; empty strings for pages
    /// with no embedded text.
    QList<QString> pageTexts;
};

/// DjVu importer — converts DjVu files to lists of page images for MRC processing.
///
/// Usage:
///   DjvuImporter importer;
///   DjvuImportResult result = importer.importFile("/path/to/doc.djvu");
///   if (result.success) {
///       // Pass result.pageImages to MrcPageProcessor::separatePage per page
///       // Pass result.pageTexts to build sandwich word boxes
///   }
///
/// Thread-safety: importFile() is re-entrant (no shared mutable state).
class DjvuImporter {
public:
    DjvuImporter()  = default;
    ~DjvuImporter() = default;

    /// Import a DjVu file and render all pages to QImage.
    ///
    /// \param filePath  Absolute path to the .djvu or .djv file.
    /// \param dpi       Rendering DPI (default 150 — matches standard scan resolution
    ///                  used by the MRC pipeline).
    ///
    /// \returns DjvuImportResult with pageImages + pageTexts populated on success.
    DjvuImportResult importFile(const QString& filePath, int dpi = 150) const;

    /// Returns true if DjVu support was compiled in (HAS_DJVU defined).
    static bool isAvailable();
};
