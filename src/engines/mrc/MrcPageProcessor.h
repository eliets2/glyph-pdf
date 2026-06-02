// src/engines/mrc/MrcPageProcessor.h
// MrcPageProcessor — Mixed Raster Content layer separation + encoding.
//
// The MRC pipeline separates a scanned-page image into three layers:
//   1. foregroundMask — 1bpp bitonal image (text/line-art); encoded as JBIG2
//   2. background     — continuous-tone image (photo/fill regions); JPEG2000
//   3. sandwichText   — invisible OCR word boxes for PDF text-search layer
//
// Design constraints (M7-P3):
//   - JBIG2 lossless generic-region mode ONLY. Pattern-matching / cross-page
//     symbol deduplication is NEVER used (Xerox 2013 digit-substitution
//     incident; German BSI banned pattern-matching JBIG2).
//   - JPEG2000 background via OpenJPEG (BSD-2-Clause, already in MSYS2).
//   - Layout regions (from ILayoutDetector / LayoutEnsemble) guide separation:
//       - RegionType::Title, Paragraph, List, Header, Footer, Equation,
//         Reference, Caption, Other → foreground mask
//       - RegionType::Figure → background
//       - RegionType::Table  → foreground mask (tables are bitonal)
//   - MergedOcrWord bounding boxes are passed through to sandwichText verbatim;
//     no hOCR roundtrip (as required by M7-P3 constraint).
//   - Thread-safe: all encode functions are const + stateless after construction.
//
// Dependencies:
//   - jbig2enc (Apache-2.0) vendored at third_party/jbig2enc/
//   - OpenJPEG 2.x (BSD-2-Clause) via MSYS2 ucrt64 + pkg-config libopenjp2
//   - Leptonica (BSD-2-Clause) via MSYS2 ucrt64 — required by jbig2enc
#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QString>
#include <memory>

#include "core/interfaces/IPdfEditorEngine.h"   // MrcMode enum
#include "engines/ocr/ILayoutDetector.h"         // LayoutRegion, RegionType
#include "engines/ocr/OcrPipeline.h"             // MergedOcrWord

/// A word from the OCR sandwich layer (for PDF text placement).
struct SandwichWord {
    QString text;
    QRectF  boundingBox;   ///< In page-image pixel coordinates (top-left origin)
    int     confidence = 0;
};

/// The three MRC layers produced by MrcPageProcessor::separatePage().
struct MrcLayers {
    /// 1bpp bitonal image containing text/line-art foreground.
    /// Encoded as JBIG2 lossless generic region (no pattern-matching).
    QByteArray foregroundJbig2;

    /// Continuous-tone background image (photos, fills, blank areas).
    /// Encoded as JPEG2000 at the configured quality ratio.
    QByteArray backgroundJp2;

    /// The page background image before JPEG2000 encoding (full color, used
    /// for size estimation and quality preview).
    QImage     backgroundImage;

    /// Width and height of the page image in pixels (for PDF assembly).
    int        pageWidthPx  = 0;
    int        pageHeightPx = 0;

    /// Word boxes for the invisible sandwich text layer (from OCR results).
    QList<SandwichWord> sandwichText;

    /// Whether encoding succeeded. If false, callers should fall back to
    /// standard PDF image embedding without MRC decomposition.
    bool        success = false;
    QString     errorMessage;
};

// MrcMode is defined in core/interfaces/IPdfEditorEngine.h — do not re-declare.

/// MRC page processor: layer separation + JBIG2/JPEG2000 encoding.
///
/// Thread-safety: separatePage() is re-entrant. Callers may invoke it from
/// multiple threads simultaneously (e.g. LaneScheduler CPU lane workers).
class MrcPageProcessor {
public:
    explicit MrcPageProcessor(MrcMode mode = MrcMode::Balanced);
    ~MrcPageProcessor();

    MrcMode mode() const;
    void    setMode(MrcMode m);

    /// Separate a scanned-page image into MRC layers.
    ///
    /// \param pageImage   Full-color rendered page at the scan DPI.
    /// \param regions     Layout regions from LayoutEnsemble (may be empty).
    /// \param words       ROVER-merged OCR word results (may be empty).
    ///
    /// \returns MrcLayers with foregroundJbig2, backgroundJp2, and sandwichText
    ///          populated. On failure, MrcLayers::success == false.
    MrcLayers separatePage(const QImage&              pageImage,
                           const QList<LayoutRegion>& regions,
                           const QList<MergedOcrWord>& words) const;

    // ── Size estimation ──────────────────────────────────────────────────────

    /// Estimate compressed size for a given image without encoding it.
    /// Used by CompressDialog for live size previews.
    ///
    /// \param pageImage   Page image to estimate.
    /// \param mode        MRC mode to use for estimation.
    /// \returns Estimated byte count after MRC encoding (rough heuristic).
    static qint64 estimateCompressedSize(const QImage& pageImage, MrcMode mode);

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    /// Build a 1bpp foreground mask from the page image using the layout regions
    /// to guide which areas are "text" vs "background".
    ///
    /// Regions typed as Title / Paragraph / List / Table / Header / Footer /
    /// Equation / Reference / Caption / Other → foreground (white on black mask).
    /// Regions typed as Figure → background (excluded from mask).
    /// Unregioned pixels: Otsu threshold to decide foreground vs background.
    static QImage buildForegroundMask(const QImage&              page,
                                      const QList<LayoutRegion>& regions);

    /// Build the background image: foreground mask pixels replaced with
    /// bilinear-interpolated background color (effectively "inpainting" text).
    static QImage buildBackground(const QImage& page,
                                  const QImage& foregroundMask);

    /// Encode a 1bpp QImage as a JBIG2 lossless generic-region bitstream.
    /// NEVER uses symbol extraction / pattern-matching / cross-page dict.
    /// Returns empty QByteArray on failure.
    static QByteArray encodeForegroundJbig2(const QImage& mask1bpp);

    /// Encode a color QImage as JPEG2000 at the given compression ratio.
    /// Uses OpenJPEG opj_compress. Returns empty QByteArray on failure.
    static QByteArray encodeBackgroundJp2(const QImage& bg,
                                          int compressionRatio);

    static int compressionRatioFor(MrcMode mode);

    class Private;
    std::unique_ptr<Private> d;
};
