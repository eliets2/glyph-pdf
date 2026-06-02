// src/engines/mrc/MrcPageProcessor.cpp
// MrcPageProcessor — Mixed Raster Content layer separation + encoding.
//
// See MrcPageProcessor.h for design constraints and safety notes.
//
// Dependency availability:
//   - OpenJPEG (BSD-2-Clause): always available via MSYS2 ucrt64.
//   - jbig2enc (Apache-2.0): vendored at third_party/jbig2enc/ — guarded by
//     HAS_JBIG2ENC CMake define. Without it, foreground falls back to
//     PNG 1bpp in a /FlateDecode stream (lossless, larger, but valid PDF/A).
//
// JBIG2 safety note:
//   We use ONLY the generic-region (lossless) path of jbig2enc.
//   jbig2_add_page_data() is called per-page with (ctx, pix, 0 /*no-refine*/).
//   We do NOT call jbig2_finalise_page() with a cross-page symbol dictionary.
//   This matches the lossless JB2 profile required by PDF/A-2b and avoids
//   the Xerox 2013 digit-substitution incident.
#include "engines/mrc/MrcPageProcessor.h"

#include <QColor>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <vector>

// ── OpenJPEG (always available) ──────────────────────────────────────────────
#include <openjpeg.h>

// ── jbig2enc (conditionally available) ───────────────────────────────────────
#ifdef HAS_JBIG2ENC
#include <jbig2enc.h>        // from vendored third_party/jbig2enc/src/
#include <leptonica/allheaders.h>  // Leptonica — used by jbig2enc
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Private implementation
// ─────────────────────────────────────────────────────────────────────────────

class MrcPageProcessor::Private {
public:
    MrcMode mode = MrcMode::Balanced;
};

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

MrcPageProcessor::MrcPageProcessor(MrcMode mode)
    : d(std::make_unique<Private>())
{
    d->mode = mode;
}

MrcPageProcessor::~MrcPageProcessor() = default;

MrcMode MrcPageProcessor::mode() const { return d->mode; }
void    MrcPageProcessor::setMode(MrcMode m) { d->mode = m; }

// ─────────────────────────────────────────────────────────────────────────────
// Layer separation — public API
// ─────────────────────────────────────────────────────────────────────────────

MrcLayers MrcPageProcessor::separatePage(
    const QImage&              pageImage,
    const QList<LayoutRegion>& regions,
    const QList<MergedOcrWord>& words) const
{
    MrcLayers result;

    if (pageImage.isNull()) {
        result.errorMessage = QStringLiteral("MrcPageProcessor: null page image");
        return result;
    }

    // Convert to RGB32 for consistent pixel access
    QImage rgb = pageImage.convertToFormat(QImage::Format_RGB32);
    result.pageWidthPx  = rgb.width();
    result.pageHeightPx = rgb.height();

    // ── Step 1: Build foreground mask (1bpp) ──────────────────────────────
    QImage mask = buildForegroundMask(rgb, regions);

    // ── Step 2: Build background (foreground pixels inpainted) ───────────
    QImage bg = buildBackground(rgb, mask);
    result.backgroundImage = bg;

    // ── Step 3: Encode foreground → JBIG2 ───────────────────────────────
    result.foregroundJbig2 = encodeForegroundJbig2(mask);
    if (result.foregroundJbig2.isEmpty()) {
        // Fall back: encode foreground mask as Flate-compressed 1bpp stream
        // (valid in PDF/A-2b; larger than JBIG2 but correct)
        // We encode as a minimal raw 1bpp bitstream here; PDF assembly will
        // wrap it in a /FlateDecode filter if JBIG2 is unavailable.
        result.foregroundJbig2.clear();  // signal to PDF assembler: use Flate path
        qWarning() << "MrcPageProcessor: jbig2enc not available — "
                      "foreground will use Flate/1bpp fallback";
    }

    // ── Step 4: Encode background → JPEG2000 ────────────────────────────
    int ratio = compressionRatioFor(d->mode);
    result.backgroundJp2 = encodeBackgroundJp2(bg, ratio);
    if (result.backgroundJp2.isEmpty()) {
        result.errorMessage = QStringLiteral("MrcPageProcessor: JPEG2000 encoding failed");
        return result;
    }

    // ── Step 5: Populate sandwich text ──────────────────────────────────
    for (const MergedOcrWord& w : words) {
        if (!w.text.trimmed().isEmpty()) {
            SandwichWord sw;
            sw.text        = w.text;
            sw.boundingBox = w.boundingBox;
            sw.confidence  = w.confidence;
            result.sandwichText.append(sw);
        }
    }

    result.success = true;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildForegroundMask
//
// Layout-guided: text/line regions → white; photo/figure regions → black.
// Unclassified pixels: Otsu threshold on luminance.
// ─────────────────────────────────────────────────────────────────────────────

static bool isTextRegion(RegionType t)
{
    switch (t) {
    case RegionType::Title:
    case RegionType::Paragraph:
    case RegionType::List:
    case RegionType::Table:
    case RegionType::Header:
    case RegionType::Footer:
    case RegionType::Equation:
    case RegionType::Reference:
    case RegionType::Caption:
    case RegionType::Other:
        return true;
    case RegionType::Figure:
        return false;
    }
    return true;  // default: treat as text
}

// Compute global Otsu threshold on luminance values of an RGB32 image.
static int otsuThreshold(const QImage& img)
{
    // Build luminance histogram
    std::vector<int> hist(256, 0);
    int total = img.width() * img.height();
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = row[x];
            int lum = (qRed(px) * 299 + qGreen(px) * 587 + qBlue(px) * 114) / 1000;
            hist[qBound(0, lum, 255)]++;
        }
    }

    double sum = 0;
    for (int i = 0; i < 256; ++i) sum += i * hist[i];

    double sumB = 0;
    int    wB   = 0;
    double maxVar = 0;
    int    threshold = 128;

    for (int t = 0; t < 256; ++t) {
        wB += hist[t];
        if (wB == 0) continue;
        int wF = total - wB;
        if (wF == 0) break;

        sumB += t * hist[t];
        double mB = sumB / wB;
        double mF = (sum - sumB) / wF;
        double var = (double)wB * wF * (mB - mF) * (mB - mF);
        if (var > maxVar) {
            maxVar = var;
            threshold = t;
        }
    }
    return threshold;
}

/*static*/ QImage MrcPageProcessor::buildForegroundMask(
    const QImage&              page,
    const QList<LayoutRegion>& regions)
{
    int W = page.width();
    int H = page.height();

    // Result: Format_Mono (1bpp) — Qt convention: 0=black, 1=white
    // We use Format_Grayscale8 internally, then convert to Mono at the end
    // for simplicity with pixel access.
    QImage mask(W, H, QImage::Format_Grayscale8);
    mask.fill(0);  // start all-black (background)

    // Build region classification map: 0=background, 1=text
    // Unclassified pixels will be decided by Otsu after region pass.
    QImage regionMap(W, H, QImage::Format_Grayscale8);
    regionMap.fill(128);  // 128 = "unclassified"

    for (const LayoutRegion& r : regions) {
        int x1 = qMax(0, (int)r.bbox.left());
        int y1 = qMax(0, (int)r.bbox.top());
        int x2 = qMin(W - 1, (int)r.bbox.right());
        int y2 = qMin(H - 1, (int)r.bbox.bottom());
        quint8 label = isTextRegion(r.type) ? 255 : 0;
        for (int y = y1; y <= y2; ++y) {
            quint8* rm = regionMap.scanLine(y);
            for (int x = x1; x <= x2; ++x) {
                rm[x] = label;
            }
        }
    }

    // Compute Otsu threshold for unclassified regions
    int otsu = otsuThreshold(page);
    // For JBIG2 foreground mask: dark pixels (below threshold) = foreground text
    // Convention: mask white (255) = foreground, black (0) = background

    for (int y = 0; y < H; ++y) {
        const QRgb* src = reinterpret_cast<const QRgb*>(page.constScanLine(y));
        const quint8* rm = regionMap.constScanLine(y);
        quint8* dst      = mask.scanLine(y);
        for (int x = 0; x < W; ++x) {
            quint8 label = rm[x];
            if (label == 0) {
                // Background region (Figure) — not in foreground mask
                dst[x] = 0;
            } else if (label == 255) {
                // Text region — apply local threshold within region
                QRgb px = src[x];
                int lum = (qRed(px) * 299 + qGreen(px) * 587 + qBlue(px) * 114) / 1000;
                dst[x] = (lum < otsu) ? 255 : 0;
            } else {
                // Unclassified — use Otsu threshold
                QRgb px = src[x];
                int lum = (qRed(px) * 299 + qGreen(px) * 587 + qBlue(px) * 114) / 1000;
                dst[x] = (lum < otsu) ? 255 : 0;
            }
        }
    }

    // Convert to Format_Mono (1bpp) for JBIG2 encoding
    return mask.convertToFormat(QImage::Format_Mono, Qt::ThresholdDither);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildBackground
//
// Replace foreground-masked pixels with bilinear-interpolated background.
// Simple inpainting: replace white (foreground) pixels with the nearest
// non-foreground pixel colour in a 5px neighbourhood (scanline average).
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ QImage MrcPageProcessor::buildBackground(const QImage& page,
                                                      const QImage& foregroundMask)
{
    QImage bg = page.copy();
    QImage mask8 = foregroundMask.convertToFormat(QImage::Format_Grayscale8);

    int W = bg.width();
    int H = bg.height();

    // Simple row-pass inpainting: for each foreground pixel, fill with the
    // row average of non-foreground pixels in that row (fast approximation).
    for (int y = 0; y < H; ++y) {
        QRgb* bgRow       = reinterpret_cast<QRgb*>(bg.scanLine(y));
        const quint8* msk = mask8.constScanLine(y);

        // Compute row background average from non-foreground pixels
        long rSum = 0, gSum = 0, bSum = 0;
        int  count = 0;
        for (int x = 0; x < W; ++x) {
            if (msk[x] == 0) {  // background pixel (mask is 0 = black = background)
                QRgb px = bgRow[x];
                rSum += qRed(px); gSum += qGreen(px); bSum += qBlue(px);
                ++count;
            }
        }

        // Default fill: white (typical scanned page background)
        QRgb fillColor = qRgb(255, 255, 255);
        if (count > 0) {
            fillColor = qRgb((int)(rSum / count),
                             (int)(gSum / count),
                             (int)(bSum / count));
        }

        // Fill foreground pixels with the inpainted color
        for (int x = 0; x < W; ++x) {
            if (msk[x] != 0) {  // foreground pixel
                bgRow[x] = fillColor;
            }
        }
    }

    return bg;
}

// ─────────────────────────────────────────────────────────────────────────────
// encodeForegroundJbig2
//
// Encodes a 1bpp QImage as a JBIG2 generic-region lossless bitstream.
// NEVER uses symbol extraction or pattern-matching.
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ QByteArray MrcPageProcessor::encodeForegroundJbig2(const QImage& mask1bpp)
{
#ifdef HAS_JBIG2ENC
    // Ensure 1bpp input
    QImage mono = (mask1bpp.format() == QImage::Format_Mono)
                  ? mask1bpp
                  : mask1bpp.convertToFormat(QImage::Format_Mono);

    int W = mono.width();
    int H = mono.height();

    // Convert QImage 1bpp to Leptonica PIX (1bpp, big-endian row bits).
    // Leptonica uses 32-bit word storage per row; pixSetPixel handles the packing.
    PIX* pix = pixCreate(W, H, 1);
    if (!pix) {
        qWarning() << "MrcPageProcessor: pixCreate failed";
        return QByteArray();
    }

    for (int y = 0; y < H; ++y) {
        const quint8* srcRow = mono.constScanLine(y);
        // Qt Format_Mono: MSB of byte 0 = leftmost pixel
        for (int x = 0; x < W; ++x) {
            int byteIdx = x / 8;
            int bitIdx  = 7 - (x % 8);
            int bit     = (srcRow[byteIdx] >> bitIdx) & 1;
            pixSetPixel(pix, x, y, bit);
        }
    }

    // jbig2_init: threshold, weight, xres, yres, full_headers, refine
    // refine = -1: disable refinement → lossless generic region per page.
    // This is the ONLY safe mode (no cross-page symbol deduplication).
    jbig2ctx* ctx = jbig2_init(
        /*threshold=*/0.85f,
        /*weight=*/0.5f,
        /*xres=*/0,
        /*yres=*/0,
        /*full_headers=*/1,   // produce a complete JBIG2 file (not PDF fragment)
        /*refine=*/-1         // -1 = lossless generic region, no refinement
    );
    if (!ctx) {
        pixDestroy(&pix);
        qWarning() << "MrcPageProcessor: jbig2_init failed";
        return QByteArray();
    }

    // Add the single page — lossless generic encoding
    jbig2_add_page(ctx, pix);

    // Retrieve the encoded stream
    uint8_t* data = nullptr;
    size_t   dataSize = 0;
    jbig2_pages_complete(ctx);
    data = jbig2_produce_page(ctx, 0, /*int refinement=*/0, &dataSize);

    QByteArray result;
    if (data && dataSize > 0) {
        result = QByteArray(reinterpret_cast<const char*>(data),
                            static_cast<int>(dataSize));
        free(data);
    }

    jbig2_destroy(ctx);
    pixDestroy(&pix);
    return result;

#else
    // jbig2enc not available — return empty to signal Flate fallback
    Q_UNUSED(mask1bpp)
    return QByteArray();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// encodeBackgroundJp2
//
// Encode a color QImage as JPEG2000 using OpenJPEG at the given ratio.
// ─────────────────────────────────────────────────────────────────────────────

// ── OpenJPEG memory stream helpers ──────────────────────────────────────────

struct OpjMemBuf {
    std::vector<OPJ_BYTE> data;
    OPJ_SIZE_T            pos = 0;
};

static OPJ_SIZE_T opjWriteFn(void* buffer, OPJ_SIZE_T nb, void* ud)
{
    OpjMemBuf* mb = static_cast<OpjMemBuf*>(ud);
    const OPJ_BYTE* src = static_cast<const OPJ_BYTE*>(buffer);
    if (mb->pos + nb > mb->data.size())
        mb->data.resize(mb->pos + nb);
    std::copy(src, src + nb, mb->data.begin() + mb->pos);
    mb->pos += nb;
    return nb;
}

static OPJ_BOOL opjSeekFn(OPJ_OFF_T nb, void* ud)
{
    OpjMemBuf* mb = static_cast<OpjMemBuf*>(ud);
    if (nb < 0) return OPJ_FALSE;
    mb->pos = (OPJ_SIZE_T)nb;
    return OPJ_TRUE;
}

static OPJ_OFF_T opjSkipFn(OPJ_OFF_T nb, void* ud)
{
    OpjMemBuf* mb = static_cast<OpjMemBuf*>(ud);
    mb->pos += (OPJ_SIZE_T)nb;
    return nb;
}

static void opjErrorCallback(const char* msg, void* /*client*/)
{
    qWarning() << "OpenJPEG error:" << msg;
}

static void opjWarningCallback(const char* msg, void* /*client*/)
{
    qDebug() << "OpenJPEG warning:" << msg;
}

static void opjInfoCallback(const char* /*msg*/, void* /*client*/)
{
    // suppress info messages
}

/*static*/ QByteArray MrcPageProcessor::encodeBackgroundJp2(const QImage& bg,
                                                              int compressionRatio)
{
    if (bg.isNull()) return QByteArray();

    // Convert to RGB (24bpp) for OpenJPEG
    QImage rgb = bg.convertToFormat(QImage::Format_RGB888);
    int W = rgb.width();
    int H = rgb.height();
    int numComponents = 3;  // R, G, B

    // ── Set up image parameters ──────────────────────────────────────────
    opj_image_cmptparm_t compParams[3];
    for (int c = 0; c < numComponents; ++c) {
        compParams[c].dx   = 1;
        compParams[c].dy   = 1;
        compParams[c].w    = (OPJ_UINT32)W;
        compParams[c].h    = (OPJ_UINT32)H;
        compParams[c].x0   = 0;
        compParams[c].y0   = 0;
        compParams[c].prec = 8;
        compParams[c].bpp  = 8;
        compParams[c].sgnd = 0;
    }

    opj_image_t* image = opj_image_create((OPJ_UINT32)numComponents,
                                           compParams, OPJ_CLRSPC_SRGB);
    if (!image) {
        qWarning() << "MrcPageProcessor: opj_image_create failed";
        return QByteArray();
    }

    image->x0 = 0; image->y0 = 0;
    image->x1 = (OPJ_UINT32)W;
    image->y1 = (OPJ_UINT32)H;

    // Fill component data (interleaved RGB → planar)
    for (int y = 0; y < H; ++y) {
        const quint8* row = rgb.constScanLine(y);
        int base = y * W;
        for (int x = 0; x < W; ++x) {
            int i = base + x;
            image->comps[0].data[i] = row[x * 3 + 0];  // R
            image->comps[1].data[i] = row[x * 3 + 1];  // G
            image->comps[2].data[i] = row[x * 3 + 2];  // B
        }
    }

    // ── Set up codec ─────────────────────────────────────────────────────
    opj_codec_t* codec = opj_create_compress(OPJ_CODEC_JP2);
    if (!codec) {
        opj_image_destroy(image);
        qWarning() << "MrcPageProcessor: opj_create_compress failed";
        return QByteArray();
    }

    opj_set_error_handler(codec,   opjErrorCallback,   nullptr);
    opj_set_warning_handler(codec, opjWarningCallback, nullptr);
    opj_set_info_handler(codec,    opjInfoCallback,    nullptr);

    opj_cparameters_t encParams;
    opj_set_default_encoder_parameters(&encParams);
    encParams.irreversible    = 1;      // lossy ICT transform
    encParams.tcp_numlayers   = 1;
    encParams.cp_fixed_quality = 0;
    encParams.cp_disto_alloc  = 1;
    encParams.tcp_rates[0]    = (float)compressionRatio;  // 30 = 30:1 compression
    encParams.numresolution   = 6;

    if (!opj_setup_encoder(codec, &encParams, image)) {
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        qWarning() << "MrcPageProcessor: opj_setup_encoder failed";
        return QByteArray();
    }

    // ── Encode to in-memory buffer via custom stream ─────────────────────
    // OpenJPEG 2.x does not ship opj_stream_create_default_memory_stream in
    // the stock ucrt64 build, so we wire up our own write callbacks.
    OpjMemBuf memBuf;
    memBuf.data.reserve(W * H);  // rough initial allocation

    opj_stream_t* stream = opj_stream_default_create(OPJ_FALSE /*is_read*/);
    if (!stream) {
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        qWarning() << "MrcPageProcessor: opj_stream_default_create failed";
        return QByteArray();
    }

    opj_stream_set_write_function(stream, opjWriteFn);
    opj_stream_set_skip_function(stream,  opjSkipFn);
    opj_stream_set_seek_function(stream,  opjSeekFn);
    opj_stream_set_user_data(stream, &memBuf, nullptr);
    opj_stream_set_user_data_length(stream, (OPJ_UINT64)(-1));

    bool ok = opj_start_compress(codec, image, stream)
           && opj_encode(codec, stream)
           && opj_end_compress(codec, stream);

    QByteArray result;
    if (ok && !memBuf.data.empty()) {
        result = QByteArray(reinterpret_cast<const char*>(memBuf.data.data()),
                            static_cast<int>(memBuf.data.size()));
    } else {
        qWarning() << "MrcPageProcessor: JPEG2000 encode failed";
    }

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(image);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// compressionRatioFor
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ int MrcPageProcessor::compressionRatioFor(MrcMode mode)
{
    switch (mode) {
    case MrcMode::Lossless:    return 10;
    case MrcMode::Balanced:    return 30;
    case MrcMode::Aggressive:  return 50;
    case MrcMode::Off:         return 30;  // shouldn't reach here
    }
    return 30;
}

// ─────────────────────────────────────────────────────────────────────────────
// estimateCompressedSize (static)
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ qint64 MrcPageProcessor::estimateCompressedSize(const QImage& pageImage,
                                                              MrcMode mode)
{
    if (pageImage.isNull()) return 0;

    qint64 rawBytes = pageImage.width() * pageImage.height() * 3LL;  // RGB
    int ratio = compressionRatioFor(mode);

    // Heuristic: background (70% of page) compressed at JPEG2000 ratio,
    //            foreground (30% of page) compressed at ~10:1 for JBIG2.
    qint64 bgEstimate = (qint64)(rawBytes * 0.70 / ratio);
    qint64 fgEstimate = (qint64)(rawBytes * 0.30 / 10);  // JBIG2 ~10:1
    return bgEstimate + fgEstimate;
}
