// SPDX-License-Identifier: Apache-2.0
#include "engines/ocr/OcrPreprocessor.h"

#include <QDebug>
#include <QtMath>
#include <QPainter>
#include <cstring>

#ifdef HAS_TESSERACT
#include <leptonica/allheaders.h>
#endif

// ── Helper: QImage ↔ Leptonica Pix ──────────────────────────────────────────

#ifdef HAS_TESSERACT
namespace {

/// Convert QImage (grayscale8) → Leptonica 8-bit Pix.  Caller owns result.
Pix* qimageToPix(const QImage &img)
{
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    Pix *pix = pixCreate(gray.width(), gray.height(), 8);
    if (!pix) return nullptr;

    for (int y = 0; y < gray.height(); ++y) {
        const uchar *src = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            pixSetPixel(pix, x, y, src[x]);
        }
    }
    return pix;
}

/// Convert Leptonica Pix (1 or 8 bpp) → QImage.
QImage pixToQImage(Pix *pix)
{
    if (!pix) return {};

    int d = pixGetDepth(pix);
    int w = pixGetWidth(pix);
    int h = pixGetHeight(pix);

    if (d == 8) {
        QImage out(w, h, QImage::Format_Grayscale8);
        for (int y = 0; y < h; ++y) {
            uchar *dst = out.scanLine(y);
            for (int x = 0; x < w; ++x) {
                l_uint32 val = 0;
                pixGetPixel(pix, x, y, &val);
                dst[x] = static_cast<uchar>(val);
            }
        }
        return out;
    }

    if (d == 1) {
        // Binary: 0 = black, 1 = white in Leptonica convention
        QImage out(w, h, QImage::Format_Grayscale8);
        out.fill(0);
        for (int y = 0; y < h; ++y) {
            uchar *dst = out.scanLine(y);
            for (int x = 0; x < w; ++x) {
                l_uint32 val = 0;
                pixGetPixel(pix, x, y, &val);
                dst[x] = val ? 255 : 0;
            }
        }
        return out;
    }

    // For 32-bit (RGB/RGBA) Pix, extract as ARGB32
    QImage out(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        QRgb *dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            l_uint32 val = 0;
            pixGetPixel(pix, x, y, &val);
            // Leptonica stores 0xRRGGBB00 for 32bpp
            int r = (val >> 24) & 0xff;
            int g = (val >> 16) & 0xff;
            int b = (val >>  8) & 0xff;
            dst[x] = qRgb(r, g, b);
        }
    }
    return out;
}

} // namespace
#endif // HAS_TESSERACT

// ── Public API ──────────────────────────────────────────────────────────────

PreprocessedImage OcrPreprocessor::process(const QImage &input, const OcrPreprocessOptions &opts) const
{
    PreprocessedImage result;
    result.inverseTransform = QTransform(); // identity
    result.effectiveDpi = opts.targetDpi;

    if (input.isNull()) {
        result.image = input;
        return result;
    }

    QImage working = input;

    // 1. DPI normalize — scale up small images
    if (opts.dpiNormalize) {
        int srcDpi = input.dotsPerMeterX() > 0
                         ? qRound(input.dotsPerMeterX() / 39.3701)
                         : 72; // assume 72 if unknown
        if (srcDpi < opts.targetDpi && srcDpi > 0) {
            double scale = static_cast<double>(opts.targetDpi) / srcDpi;
            working = working.scaled(
                qRound(working.width() * scale),
                qRound(working.height() * scale),
                Qt::IgnoreAspectRatio,
                Qt::SmoothTransformation);
            // inverse: to go from preprocessed coords → original, divide by scale
            result.inverseTransform = QTransform::fromScale(1.0 / scale, 1.0 / scale) * result.inverseTransform;
        }
        result.effectiveDpi = opts.targetDpi;
    }

    // 2. Deskew
    if (opts.deskew) {
        double angle = 0.0;
        QImage deskewed = deskew(working, &angle);
        if (!deskewed.isNull() && qAbs(angle) > 0.01) {
            // Build inverse rotation around center
            QPointF center(working.width() / 2.0, working.height() / 2.0);
            QTransform fwd;
            fwd.translate(center.x(), center.y());
            fwd.rotate(-angle);
            fwd.translate(-center.x(), -center.y());
            result.inverseTransform = fwd.inverted() * result.inverseTransform;
            working = deskewed;
        }
    }

    // 3. Denoise
    if (opts.denoise) {
        working = denoise(working);
    }

    // 4. Binarize
    if (opts.binarize) {
        working = binarize(working);
    }

    result.image = working;
    return result;
}

QImage OcrPreprocessor::deskew(const QImage &input, double *angleOut) const
{
    if (angleOut) *angleOut = 0.0;
    if (input.isNull()) return input;

#ifdef HAS_TESSERACT
    Pix *pix = qimageToPix(input);
    if (!pix) return input;

    l_float32 angle = 0.f;
    l_float32 conf  = 0.f;
    // pixFindSkew returns 0 on success
    if (pixFindSkew(pix, &angle, &conf) == 0 && conf > 2.0f) {
        if (angleOut) *angleOut = static_cast<double>(angle);
        Pix *rotated = pixRotate(pix, static_cast<l_float32>(angle * (M_PI / 180.0)),
                                 L_ROTATE_AREA_MAP, L_BRING_IN_WHITE, 0, 0);
        pixDestroy(&pix);
        if (rotated) {
            QImage out = pixToQImage(rotated);
            pixDestroy(&rotated);
            return out;
        }
        return input;
    }
    pixDestroy(&pix);
    return input;
#else
    // Qt-only fallback: no deskew without Leptonica
    Q_UNUSED(input)
    return input;
#endif
}

QImage OcrPreprocessor::binarize(const QImage &input) const
{
    if (input.isNull()) return input;

#ifdef HAS_TESSERACT
    Pix *pix = qimageToPix(input);
    if (!pix) return input;

    // Sauvola binarization: window 8, reduction factor 0, k=0.34
    Pix *binPix = nullptr;
    if (pixSauvolaBinarize(pix, 8, 0.34f, 1, nullptr, nullptr, nullptr, &binPix) == 0 && binPix) {
        QImage out = pixToQImage(binPix);
        pixDestroy(&binPix);
        pixDestroy(&pix);
        return out;
    }
    pixDestroy(&pix);
    return input;
#else
    // Qt-only fallback: simple threshold
    QImage gray = input.convertToFormat(QImage::Format_Grayscale8);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *src = gray.constScanLine(y);
        uchar *dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            dst[x] = src[x] > 128 ? 255 : 0;
        }
    }
    return out;
#endif
}

QImage OcrPreprocessor::denoise(const QImage &input) const
{
    if (input.isNull()) return input;

    // 3×3 median filter (Qt-only, works everywhere)
    QImage gray = input.convertToFormat(QImage::Format_Grayscale8);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    int w = gray.width(), h = gray.height();

    for (int y = 0; y < h; ++y) {
        uchar *dst = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            uchar window[9];
            int k = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int sy = qBound(0, y + dy, h - 1);
                    int sx = qBound(0, x + dx, w - 1);
                    window[k++] = gray.constScanLine(sy)[sx];
                }
            }
            // Partial sort to find median (5th element)
            std::nth_element(window, window + 4, window + 9);
            dst[x] = window[4];
        }
    }
    return out;
}
