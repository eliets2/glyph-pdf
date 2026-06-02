// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QTransform>
#include <QString>

/// Configuration knobs for preprocessing before OCR.
struct OcrPreprocessOptions {
    bool dpiNormalize  = true;   // Scale to 300 DPI if lower
    int  targetDpi     = 300;
    bool deskew        = true;   // Correct skew via Leptonica pixDeskew
    bool denoise       = true;   // Median filter to remove speckle
    bool binarize      = true;   // Sauvola adaptive binarization
    bool orientDetect  = false;  // Auto-rotate 0/90/180/270
};

/// Result of preprocessing: the cleaned image + an inverse transform
/// that maps coordinates in the preprocessed image back to the original.
struct PreprocessedImage {
    QImage image;
    QTransform inverseTransform;  // preprocessed-coords → original-coords
    int    effectiveDpi = 300;
};

/// Applies image preprocessing to improve OCR accuracy.
/// Uses Leptonica (under HAS_TESSERACT) for deskew & binarize,
/// falls back to Qt-based operations otherwise.
class OcrPreprocessor {
public:
    OcrPreprocessor() = default;

    /// Run the full preprocessing pipeline on a page image.
    PreprocessedImage process(const QImage &input, const OcrPreprocessOptions &opts = {}) const;

    /// Convenience: only deskew.
    QImage deskew(const QImage &input, double *angleOut = nullptr) const;

    /// Convenience: only binarize (Sauvola).
    QImage binarize(const QImage &input) const;

    /// Convenience: only denoise.
    QImage denoise(const QImage &input) const;
};
