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

/// Copy resolution metadata from \p meta into \p out when present, so
/// freshly built QImages (pixToQImage, the denoise buffer) keep the input's
/// DPI instead of resetting it to the Qt default (F05 acceptance: preserve
/// DPI metadata through preprocessing).
void carryResolution(const QImage &meta, QImage &out)
{
    if (out.isNull() || meta.isNull()) return;
    if (meta.dotsPerMeterX() > 0) out.setDotsPerMeterX(meta.dotsPerMeterX());
    if (meta.dotsPerMeterY() > 0) out.setDotsPerMeterY(meta.dotsPerMeterY());
}

/// Convert Leptonica Pix (1 or 8 bpp) → QImage. Resolution metadata (DPI) is
/// carried over from \p meta when it has any, so a fresh QImage built from a
/// Pix does not silently report the Qt default of 72 dpi.
QImage pixToQImage(Pix *pix, const QImage &meta = {})
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
        carryResolution(meta, out);
        return out;
    }

    if (d == 1) {
        // Binary: Leptonica 1 bpp convention (measured against the linked
        // lept build): ON (1) = black ink, OFF (0) = white paper —
        // pixConvertTo8() of an all-ON 1 bpp pix is all zeros. Map 1 → 0 and
        // 0 → 255 so dark ink stays dark and paper stays light (F05: the old
        // `val ? 255 : 0` inverted the polarity — a white page came out
        // black). The 8-bit and RGB paths below keep their own mapping; this
        // is not a global inversion of the input.
        QImage out(w, h, QImage::Format_Grayscale8);
        for (int y = 0; y < h; ++y) {
            uchar *dst = out.scanLine(y);
            for (int x = 0; x < w; ++x) {
                l_uint32 val = 0;
                pixGetPixel(pix, x, y, &val);
                dst[x] = val ? 0 : 255;
            }
        }
        carryResolution(meta, out);
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
    carryResolution(meta, out);
    return out;
}

} // namespace
#endif // HAS_TESSERACT

// ── Orientation detection ───────────────────────────────────────────────────

#ifdef HAS_TESSERACT
namespace {

/// Confidence threshold for acting on Leptonica's statistics — shared by
/// pixOrientDetect (orientation) and pixFindSkew (deskew): below it the
/// measured signal is indistinguishable from noise, so the page is left
/// as-is (documented no-op).
constexpr l_float32 kSignalConfThreshold = 2.0f;

/// Leptonica's orientation HMT sels expect roman text rasterized at
/// 150-300 ppi; smaller inputs carry no signal and only produce error spew.
constexpr int kMinOrientDimension = 32;

/// Detect the clockwise rotation (0/90/180/270) needed to bring a rotated
/// page upright, from Leptonica's roman-text ascender/descender statistics.
/// Returns 0 whenever the signal is inconclusive.
int uprightRotation(const QImage &input)
{
    if (input.isNull() || input.width() < kMinOrientDimension
                       || input.height() < kMinOrientDimension)
        return 0;

    Pix *pix8 = qimageToPix(input);
    if (!pix8) return 0;
    Pix *pix1 = pixConvertTo1(pix8, 128); // dark text on light paper
    pixDestroy(&pix8);
    if (!pix1) return 0;

    l_float32 upconf = 0.f, leftconf = 0.f;
    const l_ok ok = pixOrientDetect(pix1, &upconf, &leftconf, 0, 0);
    pixDestroy(&pix1);
    if (ok != 0) return 0;

    // Decision table from Leptonica flipdetect.c (pixOrientDetect note 5):
    // the axis with the larger |confidence| carries the orientation and its
    // sign gives the cw rotation to upright. Comparing magnitudes matters:
    // for a sideways page upconf can also dip below the threshold, but
    // |leftconf| then dominates.
    if (qAbs(leftconf) > qAbs(upconf)) {
        if (leftconf >  kSignalConfThreshold) return 90;
        if (leftconf < -kSignalConfThreshold) return 270;
    } else if (upconf < -kSignalConfThreshold) {
        return 180;
    }
    return 0; // upright (or no usable signal) — no rotation
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

    // 2. Orientation — page-level 0/90/180/270 detection. This must run
    // BEFORE deskew: pixFindSkew only measures small angles, so a sideways
    // page would defeat it, while an uprighted page deskews normally.
    if (opts.orientDetect) {
        // upgrade path: Tesseract's DetectOrientationScript (baseapi.h) is the
        // script-aware replacement once osd.traineddata ships with installs;
        // for now Leptonica's roman-text ascender statistics do the job.
#ifdef HAS_TESSERACT
        const int rotation = uprightRotation(working);
        if (rotation != 0) {
            // Forward transform: rotate, then re-anchor the mapped bbox at the
            // origin (90: (x,y)→(h−y,x); 180: →(w−x,h−y); 270: →(y,w−x)) so
            // QImage::transformed's output coords equal fwd.map(source coords)
            // and the composed inverse stays exact. Orthogonal transforms take
            // Qt's lossless path, so the pixels stay crisp.
            QTransform fwd;
            const int w = working.width(), h = working.height();
            if (rotation == 90)       fwd.translate(h, 0);
            else if (rotation == 180) fwd.translate(w, h);
            else                      fwd.translate(0, w);
            fwd.rotate(rotation);
            QImage uprighted = working.transformed(fwd);
            if (!uprighted.isNull()) {
                result.inverseTransform = fwd.inverted() * result.inverseTransform;
                working = uprighted;
            }
        }
#endif
    }

    // 3. Deskew. deskew() measures the angle on a temporary 1-bit estimator
    // image (pixFindSkew needs 1 bpp — F10) and rotates the actual image by
    // that angle; it returns the input untouched (with *angleOut = 0) for
    // blank pages, unreliable estimates and already-level pages, in which
    // case this block stays a documented no-op.
    if (opts.deskew) {
        double angle = 0.0;
        QImage deskewed = deskew(working, &angle);
        if (!deskewed.isNull() && qAbs(angle) > 0.01) {
            // The forward step rotated the content by `angle` degrees about
            // the image centre (pixRotate(+θ) is clockwise-on-screen, the
            // same direction as QTransform::rotate(+θ) — measured; the old
            // code composed the inverse from rotate(−angle), which mapped
            // word boxes twice as skewed as the scan instead of upright).
            // pixRotate samples output pixel q from input position
            // rotate(−θ)(q), so a box in deskewed coords maps back onto
            // pre-deskew coords by the coordinate rotation rotate(−angle)
            // about the same centre — exactly fwd.inverted() below.
            QPointF center(working.width() / 2.0, working.height() / 2.0);
            QTransform fwd;
            fwd.translate(center.x(), center.y());
            fwd.rotate(angle);
            fwd.translate(-center.x(), -center.y());
            result.inverseTransform = fwd.inverted() * result.inverseTransform;
            working = deskewed;
        }
    }

    // 4. Denoise
    if (opts.denoise) {
        working = denoise(working);
    }

    // 5. Binarize
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
    // F10/R06: pixFindSkew requires 1 bpp — it used to be handed the 8-bit
    // Pix directly, errored out with "pixs not 1 bpp" and always reported
    // angle 0, so tilted scans were returned unchanged. The measurement now
    // runs on a temporary 1-bit estimator image; the correction itself is
    // applied to the 8-bit image so the area-map resampling stays on
    // grayscale data (the 1-bit estimator is thrown away).
    static constexpr int kMinSkewDimension = 64; // pixFindSkew reduces by 4 internally
    if (input.width() < kMinSkewDimension || input.height() < kMinSkewDimension)
        return input;

    Pix *pix = qimageToPix(input);
    if (!pix) return input;

    Pix *estimator = pixConvertTo1(pix, 128); // dark ink → ON (see pixToQImage)
    if (!estimator) {
        pixDestroy(&pix);
        return input;
    }

    l_float32 angle = 0.f;
    l_float32 conf  = 0.f;
    const l_ok ok = pixFindSkew(estimator, &angle, &conf);
    pixDestroy(&estimator);

    // Documented no-op for blank pages (pixFindSkew reports an error or zero
    // confidence on a page without ON pixels), unreliable estimates (leptonica
    // confidence at or below the shared signal threshold) and already-level
    // pages (estimates below the 0.1° gate are resampling noise — measured
    // baseline of a level page is ±0.08°): the input is returned untouched
    // and *angleOut stays 0.
    if (ok != 0 || conf <= kSignalConfThreshold || qAbs(angle) < 0.1f) {
        pixDestroy(&pix);
        return input;
    }

    if (angleOut) *angleOut = static_cast<double>(angle);

    // Sign convention (measured with a probe against the linked lept build):
    // pixRotate(+θ) rotates the content clockwise on screen — the same
    // direction as QTransform::rotate(+θ) — and pixFindSkew reports the
    // angle whose pixRotate() deskews the scan (negative for content skewed
    // clockwise, e.g. ≈ −3° for a +3° clockwise tilt). Rotating by the
    // reported angle reduces a 3° tilt to the ±0.08° level-page baseline.
    Pix *rotated = pixRotate(pix, static_cast<l_float32>(angle * (M_PI / 180.0)),
                             L_ROTATE_AREA_MAP, L_BRING_IN_WHITE, 0, 0);
    pixDestroy(&pix);
    if (rotated) {
        QImage out = pixToQImage(rotated, input);
        pixDestroy(&rotated);
        return out;
    }
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
        QImage out = pixToQImage(binPix, input);
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
    carryResolution(input, out);
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
    carryResolution(input, out);
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
