// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QTransform>
#include <QString>

/// Configuration knobs for preprocessing before OCR.
/// F5-F2 (SWEEP-W3-UX): the shipped defaults are honest — only non-destructive
/// normalization is on. The destructive chain (deskew/denoise/binarize) is
/// opt-in: the audit observed the old on-by-default chain ZERO recognition on
/// a clean scan, and every production caller (EditController, BatchMode)
/// populates these fields from the persisted OCR-screen prefs anyway, whose
/// out-of-the-box state this struct now matches.
struct OcrPreprocessOptions {
    bool dpiNormalize  = true;   // Scale to 300 DPI if lower (non-destructive)
    int  targetDpi     = 300;
    bool deskew        = false;  // Opt-in: correct skew via Leptonica pixDeskew
    bool denoise       = false;  // Opt-in: median filter to remove speckle
    bool binarize      = false;  // Opt-in: Sauvola adaptive binarization
    bool orientDetect  = false;  // Auto-rotate 0/90/180/270
};

/// Result of preprocessing: the cleaned image + an inverse transform
/// that maps coordinates in the preprocessed image back to the original.
/// The inverse is the exact inverse of the composed forward pipeline
/// (dpiNormalize scale → optional orientDetect rotation → optional deskew
/// rotation, each about its documented origin), so word boxes measured on
/// the preprocessed image map back onto original scan coordinates.
struct PreprocessedImage {
    QImage image;
    QTransform inverseTransform;  // preprocessed-coords → original-coords
    int    effectiveDpi = 300;
};

/// Applies image preprocessing to improve OCR accuracy.
/// Uses Leptonica (under HAS_TESSERACT) for deskew, orientation & binarize,
/// falls back to Qt-based operations otherwise.
class OcrPreprocessor {
public:
    OcrPreprocessor() = default;

    /// Run the full preprocessing pipeline on a page image.
    PreprocessedImage process(const QImage &input, const OcrPreprocessOptions &opts = {}) const;

    /// Convenience: only deskew. The angle is measured by Leptonica's
    /// pixFindSkew on a temporary 1-bit estimator image (pixFindSkew requires
    /// 1 bpp) and the correction is applied to the 8-bit image. Blank pages,
    /// unreliable estimates (confidence ≤ 2.0) and already-level pages
    /// (|angle| < 0.1°) are returned unchanged with *angleOut = 0 — a
    /// documented no-op. Without Leptonica (no HAS_TESSERACT) the input is
    /// always returned unchanged.
    QImage deskew(const QImage &input, double *angleOut = nullptr) const;

    /// Convenience: only binarize (Sauvola).
    QImage binarize(const QImage &input) const;

    /// Convenience: only denoise.
    QImage denoise(const QImage &input) const;
};
