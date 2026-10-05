// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test: the Compress Quality/DPI controls must do
// real work — DCTDecode (JPEG) images must be decoded, optionally downsampled
// and re-encoded honoring OptimizeOptions::jpegQuality, instead of being
// silently skipped (the old path only handled uncompressed /DeviceRGB raw
// streams and wrote raw RGB back with no /Filter).
//
// Discriminating cases:
//  1. per-image scoping on a multi-page doc: big JPEG downsampled+re-encoded,
//     small JPEG (below DPI threshold) byte-identical, /SMask image untouched,
//     legacy raw-RGB image converted to a real /DCTDecode stream;
//  2. jpegQuality is honored: re-encode at q15 is strictly smaller than q95
//     on noise-rich content;
//  3. adversarial/malformed image dicts (garbage JPEG bytes, missing /Width,
//     /DeviceCMYK, /ImageMask) are skipped safely and optimizeDocument still
//     succeeds.
//
// §9.13 P1 additions:
//  4. unused-object removal: a fixture carrying an orphaned (unreferenced)
//     /Page dictionary with a large stream — written with NoCollectGarbage so
//     it survives into the fixture file — must be (a) reflected in a REAL
//     estimateOptimization saving when options.removeUnusedObjects is set
//     (zeroed before the sweep landed) and (b) physically gone after
//     optimizeDocument, while referenced objects survive.
//  5. FlateDecode coverage: /DeviceGray and /DeviceRGB 8bpc images carried by
//     /FlateDecode (PoDoFo's own stream decode provides the pixels) are
//     re-encoded to real /DCTDecode JPEG honoring the same contract as the
//     JPEG path — while images with a /DecodeParms /Predictor are SKIPPED
//     byte-identical, because PoDoFo's filter expansion does not undo PNG/TIFF
//     predictors and treating predictor-filtered bytes as pixels corrupts the
//     image.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QBuffer>
#include <QColorSpace>
#include <QImage>
#include <QImageWriter>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

namespace {

// Deterministic pseudo-random noise image (LCG-seeded). Noise is deliberately
// JPEG-hostile so output size is strictly monotonic in quality.
QImage makeNoiseImage(int w, int h, quint32 seed)
{
    QImage img(w, h, QImage::Format_RGB888);
    quint32 s = seed;
    for (int y = 0; y < h; ++y) {
        uchar* line = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            s = s * 1664525u + 1013904223u;
            line[x * 3 + 0] = static_cast<uchar>((s >> 16) & 0xFF);
            s = s * 1664525u + 1013904223u;
            line[x * 3 + 1] = static_cast<uchar>((s >> 16) & 0xFF);
            s = s * 1664525u + 1013904223u;
            line[x * 3 + 2] = static_cast<uchar>((s >> 16) & 0xFF);
        }
    }
    return img;
}

QByteArray encodeJpeg(const QImage& img, int quality)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QImageWriter writer(&buf, "jpg");
    writer.setQuality(quality);
    if (!writer.write(img))
        return {};
    return buf.buffer();
}

// Embed pre-encoded JPEG bytes as a proper /DCTDecode image XObject, draw it
// on the page so the save-time GC keeps it, and return the underlying
// PdfObject (owned by the document — the CreateImage wrapper is caller-owned
// and dies at scope exit).
PoDoFo::PdfObject& embedJpeg(PoDoFo::PdfMemDocument& doc, PoDoFo::PdfPage& page,
                             const QByteArray& jpeg, unsigned w, unsigned h)
{
    auto img = doc.CreateImage();
    PoDoFo::PdfImageInfo info;
    info.Width = w;
    info.Height = h;
    info.BitsPerComponent = 8;
    info.Filters = PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::DCTDecode };
    info.ColorSpace = PoDoFo::PdfColorSpaceInitializer(PoDoFo::PdfColorSpaceType::DeviceRGB);
    img->SetDataRaw(PoDoFo::bufferview(jpeg.constData(), jpeg.size()), info);

    PoDoFo::PdfPainter painter;
    painter.SetCanvas(page);
    painter.DrawImage(*img, 40, 40, 200.0, 280.0);
    painter.FinishDrawing();
    return img->GetObject();
}

struct FoundImage {
    PoDoFo::PdfObject* obj = nullptr;
    int64_t w = 0;
    int64_t h = 0;
};

// Walk the saved document and return image XObjects, optionally matching
// exact dimensions (dimensions survive renumbering/recompression).
QVector<FoundImage> findImages(PoDoFo::PdfMemDocument& doc, int64_t w = -1, int64_t h = -1)
{
    QVector<FoundImage> out;
    for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
        PoDoFo::PdfObject* o = *it;
        if (!o->IsDictionary()) continue;
        auto& d = o->GetDictionary();
        auto* st = d.FindKey("Subtype");
        if (!st || !st->IsName() || std::string(st->GetName().GetString()) != "Image") continue;
        auto* wObj = d.FindKey("Width");
        auto* hObj = d.FindKey("Height");
        if (!wObj || !hObj || !wObj->IsNumber() || !hObj->IsNumber()) continue;
        int64_t iw = wObj->GetNumber();
        int64_t ih = hObj->GetNumber();
        if (w >= 0 && (iw != w || ih != h)) continue;
        out.append({ o, iw, ih });
    }
    return out;
}

QByteArray rawStream(PoDoFo::PdfObject& obj)
{
    PoDoFo::charbuff buf;
    obj.GetOrCreateStream().CopyTo(buf, /*raw=*/true);
    return QByteArray(buf.data(), static_cast<int>(buf.size()));
}

bool filterIs(PoDoFo::PdfObject& obj, const char* name)
{
    auto* f = obj.GetDictionary().FindKey("Filter");
    if (!f || !f->IsName()) return false;
    return std::string(f->GetName().GetString()) == name;
}

// Convert an already-embedded (drawn, referenced) image XObject into an
// indexed-color one: raw index stream + /ColorSpace [/Indexed <base> <hival>
// <lookup>]. Mutating after embedding mirrors the crafted-file discipline of
// malformedImagesAreSkippedSafely.
void makeIndexedImage(PoDoFo::PdfObject& img, const QByteArray& indices,
                      unsigned w, unsigned h, const QByteArray& lookup,
                      int hival, const char* baseCs, int bpc = 8)
{
    img.GetDictionary().AddKey("Width", static_cast<int64_t>(w));
    img.GetDictionary().AddKey("Height", static_cast<int64_t>(h));
    img.GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(bpc));
    PoDoFo::PdfArray cs;
    cs.Add(PoDoFo::PdfName("Indexed"));
    cs.Add(PoDoFo::PdfName(baseCs));
    cs.Add(static_cast<int64_t>(hival));
    // Palette bytes are binary: FromRaw + GetRawData is the raw-bytes contract.
    // (A UTF-8 text string both throws on non-ASCII and re-encodes on save.)
    cs.Add(PoDoFo::PdfString::FromRaw(
        PoDoFo::bufferview(lookup.constData(), static_cast<size_t>(lookup.size())),
        /*hex=*/true));
    img.GetDictionary().RemoveKey("ColorSpace");
    img.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
    img.GetOrCreateStream().SetData(
        PoDoFo::bufferview(indices.constData(), static_cast<size_t>(indices.size())),
        /*raw=*/true);
}

// Raw 8bpc index bytes for an image of w x h whose horizontal bands cycle
// through indices 0..bandCount-1 (band 0 at the top).
QByteArray bandedIndices(unsigned w, unsigned h, int bandCount)
{
    QByteArray out(static_cast<qint64>(w) * h, '\0');
    for (unsigned y = 0; y < h; ++y) {
        const int band = std::min(bandCount - 1,
                                  static_cast<int>((y * bandCount) / h));
        memset(out.data() + static_cast<qint64>(y) * w, static_cast<char>(band), w);
    }
    return out;
}

OptimizeOptions downsampleOptions()
{
    OptimizeOptions opts;
    opts.downsampleImages = true;
    opts.targetDpi = 72;
    opts.jpegQuality = 50;
    opts.deduplicateImages = false;
    opts.subsetFonts = false;
    opts.removeUnusedObjects = false;
    opts.stripMetadata = false;
    return opts;
}

// ── PARITY row 13 second half (CMYK): fixture machinery ─────────────────────
// A minimal but genuine CMYK ICC profile: ICC v2 lut16 (mft2) A2B0 tag, PCS
// XYZ, 4 input channels, 4-point CLUT grid (vertices at 0/85/170/255 —
// multilinear interpolation is EXACT at every grid vertex, so band colors
// sampled at vertices have a mathematically defined expected value). The
// A2B0 mapping is defined as RGB = (C, M, Y) (K ignored): CMYK (85,170,255,0)
// must decode to rgb(85,170,255). The naive photographic conversion gives
// (170,85,0) — a per-channel distance of 255 — so these pins fail loudly if
// anyone swaps the color-managed decode for a naive one.
//
// Verified against the installed Qt 6.11.0 at lane time (probe3-pipeline):
// full decode→transform→downsample→JPEG-q50→decode chain kept bands within
// 2/255 of the defined mapping, and the same chain driven by Windows'
// CoatedFOGRA39.icc reproduced the plan's §3.4 reference values within 2/255.
QByteArray makeCmykIccProfile()
{
    auto put16 = [](QByteArray& b, quint16 v) {
        const quint16 be = qToBigEndian(v);
        b.append(reinterpret_cast<const char*>(&be), 2);
    };
    auto put32 = [](QByteArray& b, quint32 v) {
        const quint32 be = qToBigEndian(v);
        b.append(reinterpret_cast<const char*>(&be), 4);
    };
    auto s15Fixed16 = [](double v) { return quint32(qRound(v * 65536.0)); };

    const int grid = 4, inEntries = 2, outEntries = 2;
    const int clutEntries = grid * grid * grid * grid; // 256
    const int a2b0Size = 4 + 4 + 4 + 36 + 4
                       + 4 * inEntries * 2 + 3 * outEntries * 2
                       + clutEntries * 3 * 2;
    const int a2b0Offset = 128 + 16; // header + tag count + one tag entry
    const int profileSize = a2b0Offset + a2b0Size;

    QByteArray p;
    put32(p, profileSize);          // 0   profile size
    put32(p, 0);                    // 4   preferred CMM
    put32(p, 0x02100000);           // 8   version 2.1
    put32(p, 0x6D6E7472);           // 12  'mntr'
    put32(p, 0x434D594B);           // 16  'CMYK'
    put32(p, 0x58595A20);           // 20  'XYZ '
    put32(p, 0); put32(p, 0); put32(p, 0);   // 24  datetime
    put32(p, 0x61637370);           // 36  'acsp'
    put32(p, 0);                    // 40  platform
    put32(p, 0);                    // 44  flags
    put32(p, 0);                    // 48  manufacturer
    put32(p, 0);                    // 52  model
    put32(p, 0); put32(p, 0);       // 56  attributes
    put32(p, 0);                    // 64  rendering intent (perceptual)
    put32(p, s15Fixed16(0.9642));   // 68  D50 illuminant (ICC canonical)
    put32(p, s15Fixed16(1.0));
    put32(p, s15Fixed16(0.8249));
    put32(p, 0);                    // 80  creator
    for (int i = 0; i < 4; ++i) put32(p, 0); // 84  profile id
    for (int i = 0; i < 7; ++i) put32(p, 0); // 100 reserved
    put32(p, 1);                    // 128 tag count
    put32(p, 0x41324230);           // 'A2B0'
    put32(p, a2b0Offset);
    put32(p, a2b0Size);
    // lut16 (mft2) tag data
    put32(p, 0x6D667432);           // 'mft2'
    put32(p, 0);                    // reserved
    p.append(char(4));              // input channels (CMYK)
    p.append(char(3));              // output channels (XYZ)
    p.append(char(grid));           // CLUT grid points
    p.append(char(0));              // padding
    // matrix — unused for 4-input luts, identity keeps it sane
    put32(p, s15Fixed16(1.0)); put32(p, s15Fixed16(0.0)); put32(p, s15Fixed16(0.0));
    put32(p, s15Fixed16(0.0)); put32(p, s15Fixed16(1.0)); put32(p, s15Fixed16(0.0));
    put32(p, s15Fixed16(0.0)); put32(p, s15Fixed16(0.0)); put32(p, s15Fixed16(1.0));
    put16(p, inEntries);
    put16(p, outEntries);
    // input tables: identity ramps (2 entries each)
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < inEntries; ++i)
            put16(p, quint16(i * 65535 / (inEntries - 1)));
    // CLUT: XYZ_D50(sRGB(C,M,Y)); first channel (C) varies slowest.
    const double M[3][3] = {
        { 0.4360747, 0.3850649, 0.1430804 },
        { 0.2225045, 0.7168786, 0.0606169 },
        { 0.0139322, 0.0971045, 0.7141733 }
    };
    auto srgbDecode = [](double v) -> double {
        v /= 255.0;
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    auto putXyz = [&](int c, int m, int y) {
        const double lin[3] = { srgbDecode(c), srgbDecode(m), srgbDecode(y) };
        for (int r = 0; r < 3; ++r) {
            const double xyz = std::clamp(M[r][0] * lin[0] + M[r][1] * lin[1]
                                        + M[r][2] * lin[2], 0.0, 1.0);
            put16(p, quint16(qRound(xyz * 65535.0)));
        }
    };
    for (int iC = 0; iC < grid; ++iC)
        for (int iM = 0; iM < grid; ++iM)
            for (int iY = 0; iY < grid; ++iY)
                for (int iK = 0; iK < grid; ++iK)
                    putXyz(iC * 255 / (grid - 1), iM * 255 / (grid - 1),
                           iY * 255 / (grid - 1));
    // output tables: identity ramps
    for (int ch = 0; ch < 3; ++ch)
        for (int i = 0; i < outEntries; ++i)
            put16(p, quint16(i * 65535 / (outEntries - 1)));
    return p;
}

// The four band CMYK values shared by the CMYK fixtures — all at CLUT grid
// vertices so the profile-defined mapping is exact. Expected sRGB = (C,M,Y).
struct CmykQuad { uchar c, m, y, k; };
const CmykQuad kCmykBands[4] = {
    { 85, 170, 255, 0 },
    { 170, 255, 85, 0 },
    { 255, 0, 170, 0 },
    { 0, 0, 0, 255 }
};
QRgb cmykBandExpectedRgb(int band)
{
    return qRgb(kCmykBands[band].c, kCmykBands[band].m, kCmykBands[band].y);
}

// The naive photographic conversion (what pre-6.8 Qt / a plain .convert()
// would produce): 255·(1−C)·(1−K) per channel. This is the FAILURE signature:
// the color pins must never match these values (plan §1.2 quantified the
// drift at up to ~97/255 per channel on a real press profile).
QRgb naivePhotographicRgb(const CmykQuad& q)
{
    const double kk = (255 - q.k) / 255.0;
    return qRgb(qRound(255.0 * (255 - q.c) / 255.0 * kk),
                qRound(255.0 * (255 - q.m) / 255.0 * kk),
                qRound(255.0 * (255 - q.y) / 255.0 * kk));
}

QImage makeBandedCmyk8888(int w, int h, const QVector<CmykQuad>& quads)
{
    QImage img(w, h, QImage::Format_CMYK8888);
    for (int y = 0; y < h; ++y) {
        const int band = std::min(int(quads.size()) - 1, y * int(quads.size()) / h);
        const CmykQuad b = quads[band];
        auto* line = reinterpret_cast<quint32*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            // QCmyk32 little-endian layout: byte0=C, byte1=M, byte2=Y, byte3=K
            line[x] = quint32(b.c) | (quint32(b.m) << 8)
                    | (quint32(b.y) << 16) | (quint32(b.k) << 24);
#else
            line[x] = (quint32(b.c) << 24) | (quint32(b.m) << 16)
                    | (quint32(b.y) << 8) | quint32(b.k);
#endif
        }
    }
    return img;
}

// Convenience overload: the shared fixture bands (CLUT grid vertices).
QImage makeBandedCmyk8888(int w, int h)
{
    return makeBandedCmyk8888(w, h,
        QVector<CmykQuad>{ kCmykBands, kCmykBands + 4 });
}

// Encode a CMYK8888 image as a JPEG; when `profile` is valid the image
// carries it so the writer embeds it as an APP2 ICC_PROFILE marker (Qt writes
// CMYK JPEGs with the Adobe APP14 marker and the 0=100%-ink inversion, which
// Qt's reader undoes — probe-verified). An invalid QColorSpace produces a
// profile-less CMYK JPEG (the profile-less-pin fixture).
QByteArray encodeCmykJpeg(const QImage& cmyk, const QColorSpace& profile)
{
    QImage src = cmyk;
    if (profile.isValid())
        src.setColorSpace(profile);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QImageWriter writer(&buf, "jpg");
    writer.setQuality(100);
    if (!writer.write(src))
        return {};
    return buf.buffer();
}

// Add an /ICCBased stream object (/N 4 by default; pass another N to craft a
// non-CMYK profile claim) and return its indirect reference.
PoDoFo::PdfReference addIccProfileStream(PoDoFo::PdfMemDocument& doc,
                                        const QByteArray& profile, int n = 4)
{
    auto& obj = doc.GetObjects().CreateDictionaryObject();
    obj.GetDictionary().AddKey("N", static_cast<int64_t>(n));
    obj.GetOrCreateStream().SetData(
        PoDoFo::bufferview(profile.constData(), static_cast<size_t>(profile.size())),
        /*raw=*/true);
    return obj.GetIndirectReference();
}

} // namespace

class TestCompressJpegReencode : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;
    QString tmpPath(const QString& name) const { return m_tmpDir.filePath(name); }

    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

private slots:

    // A4@150dpi JPEG (estDpi≈150) must be downsampled toward targetDpi=72 and
    // re-encoded as DCTDecode; a small JPEG must stay byte-identical; an
    // /SMask image must be untouched; a legacy raw-RGB image must become a
    // real DCTDecode stream instead of raw RGB.
    void perImageScopingOnMultiPageDoc() {
        const int BIG_W = 1240, BIG_H = 1754;   // estDpi = 1240/8.27 ≈ 150
        const int SMALL_W = 200, SMALL_H = 280; // estDpi ≈ 24 → below threshold
        QByteArray bigJpeg = encodeJpeg(makeNoiseImage(BIG_W, BIG_H, 42), 85);
        QByteArray smallJpeg = encodeJpeg(makeNoiseImage(SMALL_W, SMALL_H, 7), 85);
        QVERIFY2(!bigJpeg.isEmpty() && !smallJpeg.isEmpty(), "test JPEG encode failed");

        QString pdf = tmpPath("scoping.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto mkPage = [&doc]() {
                return &doc.GetPages().CreatePage(
                    PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            };
            embedJpeg(doc, *mkPage(), bigJpeg, BIG_W, BIG_H);
            embedJpeg(doc, *mkPage(), smallJpeg, SMALL_W, SMALL_H);

            // /SMask-carrying image: must be skipped (mask would desync).
            auto page3 = mkPage();
            auto smaskImg = doc.CreateImage();
            QByteArray px(8 * 8 * 3, '\x30');
            smaskImg->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                              8, 8, PoDoFo::PdfPixelFormat::RGB24);
            auto mask = doc.CreateImage();
            QByteArray mpx(8 * 8, '\x80');
            mask->SetData(PoDoFo::bufferview(mpx.constData(), mpx.size()),
                          8, 8, PoDoFo::PdfPixelFormat::Grayscale);
            smaskImg->GetDictionary().AddKey("SMask", mask->GetObject().GetIndirectReference());
            {
                PoDoFo::PdfPainter painter;
                painter.SetCanvas(*page3);
                painter.DrawImage(*smaskImg, 40, 40, 20.0, 20.0);
                painter.FinishDrawing();
            }

            // Legacy raw-RGB uncompressed image on page 4.
            auto page4 = mkPage();
            auto rgbImg = doc.CreateImage();
            QByteArray rgbPx(BIG_W * BIG_H * 3, '\x60');
            rgbImg->SetData(PoDoFo::bufferview(rgbPx.constData(), rgbPx.size()),
                            BIG_W, BIG_H, PoDoFo::PdfPixelFormat::RGB24);
            {
                PoDoFo::PdfPainter painter;
                painter.SetCanvas(*page4);
                painter.DrawImage(*rgbImg, 40, 40, 200.0, 280.0);
                painter.FinishDrawing();
            }

            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 72;
        opts.jpegQuality = 15;
        opts.deduplicateImages = false;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = false;
        opts.stripMetadata = false;
        QString out = tmpPath("scoping_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        QCOMPARE(doc.GetPages().GetCount(), 4u);

        // 1) Big JPEG: still DCTDecode, downsampled, smaller than the input.
        // After re-encode the big image no longer has BIG_WxBIG_H; find it as
        // "an image with ~595x841 dims" (72/150.06 ratio).
        auto downImgs = findImages(doc); // all images with dims
        FoundImage down;
        for (const auto& im : downImgs) {
            if (im.w >= 580 && im.w <= 610 && im.h >= 830 && im.h <= 850) { down = im; break; }
        }
        QVERIFY2(down.obj != nullptr,
                 qPrintable(QString("expected downsampled image ~595x841; image dims found: %1")
                     .arg([&]{ QString s; for (auto& im : downImgs) s += QString("%1x%2 ").arg(im.w).arg(im.h); return s; }())));
        QVERIFY2(filterIs(*down.obj, "DCTDecode"),
                 "re-encoded image must be /DCTDecode");
        QByteArray downRaw = rawStream(*down.obj);
        QVERIFY2(downRaw.size() < bigJpeg.size(),
                 qPrintable(QString("re-encoded stream (%1) must be smaller than input JPEG (%2)")
                     .arg(downRaw.size()).arg(bigJpeg.size())));
        { // decode roundtrip sanity
            QImage check;
            QVERIFY(check.loadFromData(downRaw, "JPEG"));
            QCOMPARE(check.width(), static_cast<int>(down.w));
        }
        QVERIFY2(!down.obj->GetDictionary().FindKey("DecodeParms"),
                 "stale /DecodeParms must be removed when re-encoding to DCTDecode");

        // 2) Small JPEG below threshold: byte-identical stream.
        auto smallImgs = findImages(doc, SMALL_W, SMALL_H);
        QCOMPARE(smallImgs.size(), 1);
        QVERIFY2(rawStream(*smallImgs[0].obj) == smallJpeg,
                 "image below DPI threshold must not be re-encoded (bytes must be identical)");

        // 3) SMask image: untouched (its 8x8 grayscale mask object also
        // survives — it matches the same dims and carries no /SMask itself).
        auto smaskImages = findImages(doc, 8, 8);
        QVERIFY2(smaskImages.size() >= 2,
                 qPrintable(QString("SMask base + mask must survive, found %1 8x8 images")
                     .arg(smaskImages.size())));
        bool sawMasked = false;
        for (const auto& im : smaskImages) {
            if (im.obj->GetDictionary().FindKey("SMask")) { sawMasked = true; break; }
        }
        QVERIFY2(sawMasked, "the masked 8x8 image must survive with its /SMask intact");

        // 4) Legacy raw-RGB image: now a real DCTDecode stream, downsampled.
        bool sawRgbConverted = false;
        for (const auto& im : downImgs) {
            if (im.obj == down.obj) continue;
            if (filterIs(*im.obj, "DCTDecode") && im.w >= 580 && im.w <= 610) {
                sawRgbConverted = true;
                QVERIFY(rawStream(*im.obj).size() < static_cast<qint64>(BIG_W) * BIG_H * 3);
            }
        }
        QVERIFY2(sawRgbConverted, "raw-RGB image must be re-encoded to DCTDecode");
    }

    // jpegQuality must be honored: same fixture optimized at q15 vs q95 must
    // produce strictly different stream sizes (noise content ⇒ strict order).
    void qualityIsHonored() {
        const int W = 1240, H = 1754;
        QImage noise = makeNoiseImage(W, H, 1234);
        QByteArray jpeg = encodeJpeg(noise, 85);
        QVERIFY2(!jpeg.isEmpty(), "fixture JPEG encode failed");

        OptimizeOptions base;
        base.downsampleImages = true;
        base.targetDpi = 72;
        base.deduplicateImages = false;
        base.subsetFonts = false;
        base.removeUnusedObjects = false;
        base.stripMetadata = false;

        auto runOnce = [&](int quality, const QString& tag, QByteArray& outStream) -> QString {
            QString pdf = tmpPath("q_%1.pdf").arg(tag);
            {
                PoDoFo::PdfMemDocument doc;
                auto& page = doc.GetPages().CreatePage(
                    PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
                embedJpeg(doc, page, jpeg, W, H);
                doc.Save(pdf.toUtf8().constData());
            }
            PdfEditorEngine engine;
            if (!engine.loadDocumentForEditing(pdf))
                return QStringLiteral("loadDocumentForEditing failed (%1)").arg(tag);
            OptimizeOptions opts = base;
            opts.jpegQuality = quality;
            QString out = tmpPath("q_%1_out.pdf").arg(tag);
            if (!engine.optimizeDocument(out, opts))
                return QStringLiteral("optimizeDocument failed (%1)").arg(tag);
            PoDoFo::PdfMemDocument doc;
            doc.Load(out.toUtf8().constData());
            auto imgs = findImages(doc);
            if (imgs.size() != 1)
                return QStringLiteral("expected 1 image, found %1 (%2)").arg(imgs.size()).arg(tag);
            if (!filterIs(*imgs[0].obj, "DCTDecode"))
                return QStringLiteral("image must stay DCTDecode (%1)").arg(tag);
            outStream = rawStream(*imgs[0].obj);
            return {};
        };

        QByteArray low, high;
        QString errLow = runOnce(15, "15", low);
        QVERIFY2(errLow.isEmpty(), qPrintable(errLow));
        QString errHigh = runOnce(95, "95", high);
        QVERIFY2(errHigh.isEmpty(), qPrintable(errHigh));
        QVERIFY2(low.size() < high.size(),
                 qPrintable(QString("q15 stream (%1 B) must be strictly smaller than q95 (%2 B)")
                     .arg(low.size()).arg(high.size())));
    }

    // Malformed / unsupported image dicts must be skipped safely — the pass
    // must succeed and leave those images exactly as they were. Each image is
    // created valid, drawn (so it is referenced), and only then mutated into
    // its adversarial shape, mirroring a crafted PDF.
    void malformedImagesAreSkippedSafely() {
        QString pdf = tmpPath("malformed.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            QByteArray garbage(4096, '\xA5');
            auto& bad = embedJpeg(doc, page, garbage, 1240, 1754);

            QByteArray noiseJpeg = encodeJpeg(makeNoiseImage(1240, 1754, 9), 85);
            auto& cmyk = embedJpeg(doc, page, noiseJpeg, 1240, 1754);
            auto mimg = doc.CreateImage();
            QByteArray mpx(64 * 64, '\x40');
            mimg->SetData(PoDoFo::bufferview(mpx.constData(), mpx.size()),
                          64, 64, PoDoFo::PdfPixelFormat::Grayscale);

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*mimg, 200, 640, 30.0, 30.0);
            painter.FinishDrawing();

            // Now mutate into the adversarial shapes:
            // a) garbage DCT payload (decode must fail → skip, no crash);
            //    embedJpeg already wrote 4 KiB of '\xA5' as the DCT stream.
            // b) /DeviceCMYK label over an RGB JPEG payload — the decoded
            //    pixels are NOT CMYK and no profile exists, so the image is
            //    not color-manageable and stays untouched (re-labeling the
            //    dictionary must not unlock a naive re-encode).
            cmyk.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            // c) ImageMask — bi-level masks are not for lossy re-encode.
            mimg->GetDictionary().AddKey("ImageMask", PoDoFo::PdfVariant(true));
            mimg->GetDictionary().RemoveKey("ColorSpace");
            // d) crafted dict with no /Width at all.
            bad.GetDictionary().RemoveKey("Width");

            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 72;
        opts.jpegQuality = 50;
        opts.deduplicateImages = false;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = false;
        opts.stripMetadata = false;
        QString out = tmpPath("malformed_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts),
                 "optimizeDocument must succeed despite malformed images");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        // Garbage-DCT image: bytes unchanged (skip, no crash). Note its /Width
        // was removed in (d), so identify it by stream size via a raw walk.
        int garbageSurvivors = 0;
        for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* o = *it;
            if (!o->IsDictionary() || !o->HasStream()) continue;
            auto& d = o->GetDictionary();
            auto* st = d.FindKey("Subtype");
            if (!st || !st->IsName() || std::string(st->GetName().GetString()) != "Image") continue;
            if (!d.FindKey("Width")) {           // the crafted no-/Width image
                QCOMPARE(rawStream(*o).size(), 4096);
                ++garbageSurvivors;
            }
        }
        QCOMPARE(garbageSurvivors, 1);
        // The CMYK-labeled and ImageMask images must survive with dims intact.
        // (The CMYK case carries an RGB JPEG payload with no profile: the
        // color-managed lift does not apply, so the image stays untouched.
        // Genuine profile-ful CMYK downsampling is pinned positively by
        // cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically.)
        QVERIFY2(findImages(doc, 1240, 1754).size() == 1,
                 "unmanageable CMYK-labeled image must survive untouched");
        QVERIFY2(findImages(doc, 64, 64).size() == 1,
                 "ImageMask image must survive untouched");
    }

    // A wrapped-JPEG filter chain [/FlateDecode /DCTDecode] must be SKIPPED by
    // the downsample pass — reading such streams with the expanding CopyTo()
    // throws UnsupportedFilter and used to abort the whole optimization. The
    // image must be BIG enough (estDpi > targetDpi*1.2) to reach the decode.
    void mediaFilterChainImageIsSkipped() {
        const int W = 1240, H = 1754;
        QByteArray jpeg = encodeJpeg(makeNoiseImage(W, H, 77), 85);
        QVERIFY2(!jpeg.isEmpty(), "fixture JPEG encode failed");

        QString pdf = tmpPath("chain.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            auto& img = embedJpeg(doc, page, jpeg, W, H);
            // Craft the chain AFTER the image is valid: wrapped-JPEG encoding.
            PoDoFo::PdfArray chain;
            chain.Add(PoDoFo::PdfName("FlateDecode"));
            chain.Add(PoDoFo::PdfName("DCTDecode"));
            img.GetDictionary().RemoveKey("Filter");
            img.GetDictionary().AddKey("Filter", PoDoFo::PdfObject(chain));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 72;
        opts.jpegQuality = 50;
        opts.deduplicateImages = false;
        QString out = tmpPath("chain_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts),
                 "a media-filter chain image must be skipped, not fail the pass");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto imgs = findImages(doc);
        QCOMPARE(imgs.size(), 1);
        QVERIFY2(rawStream(*imgs[0].obj) == jpeg,
                 "chain-filter image must be left byte-identical");
    }

    // A dangling /XObject reference must not make the dedup rewiring throw —
    // unresolvable entries are skipped, the pass still succeeds.
    void danglingXobjectRefDoesNotFailDedup() {
        QString pdf = tmpPath("dangling.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            auto& page2 = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            Q_UNUSED(page2);
            // Craft a dangling reference in the page resources.
            auto* res = page.GetDictionary().FindKey("Resources");
            QVERIFY(res != nullptr);
            res->GetDictionary().AddKey(
                "XObject", PoDoFo::PdfObject(PoDoFo::PdfDictionary()));
            auto* xobjs = res->GetDictionary().FindKey("XObject");
            xobjs->GetDictionary().AddKey(
                "Dangling", PoDoFo::PdfObject(PoDoFo::PdfReference(9999, 0)));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = false;
        opts.deduplicateImages = true; // exercises the /XObject rewiring loop
        QString out = tmpPath("dangling_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts),
                 "a dangling /XObject reference must be skipped by the dedup rewiring");
    }

    // targetDpi <= 0 would invert the downsample ratio and clamp every image
    // to 1x1 — the pass must refuse to destroy images instead.
    void degenerateTargetDpiLeavesImagesUntouched() {
        const int W = 1240, H = 1754;
        QByteArray jpeg = encodeJpeg(makeNoiseImage(W, H, 55), 85);
        QVERIFY2(!jpeg.isEmpty(), "fixture JPEG encode failed");

        QString pdf = tmpPath("dpi0.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            embedJpeg(doc, page, jpeg, W, H);
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 0; // crafted degenerate setting
        opts.jpegQuality = 50;
        QString out = tmpPath("dpi0_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto imgs = findImages(doc, W, H);
        QCOMPARE(imgs.size(), 1);
        QVERIFY2(rawStream(*imgs[0].obj) == jpeg,
                 "targetDpi=0 must not re-encode or destroy the image (no 1x1 clamp)");
    }

    // ── §9.13 P1: unused-object removal ─────────────────────────────────────
    // A fixture carrying an orphaned /Page dictionary (in the xref, referenced
    // by nothing) plus a ~256 KiB stream, written with NoCollectGarbage so the
    // orphan genuinely survives into the fixture bytes. The sweep must (a) make
    // estimateOptimization report REAL savings for removeUnusedObjects — the
    // R12-era estimate zeroed this pass — and (b) physically remove the orphan
    // on optimizeDocument while every referenced object survives.
    void unusedObjectSweepRemovesOrphans() {
        constexpr int SMALL_W = 200, SMALL_H = 280;
        QByteArray smallJpeg = encodeJpeg(makeNoiseImage(SMALL_W, SMALL_H, 13), 85);
        QVERIFY2(!smallJpeg.isEmpty(), "test JPEG encode failed");

        QString pdf = tmpPath("orphan.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            embedJpeg(doc, page, smallJpeg, SMALL_W, SMALL_H); // referenced, must survive

            // The orphan: a well-formed /Page dictionary with a marker key and
            // a large pseudo-random (JPEG-hostile, ~incompressible) stream,
            // never referenced by the page tree, annotations, or anywhere else.
            PoDoFo::PdfObject& orphan = doc.GetObjects().CreateDictionaryObject("Page");
            orphan.GetDictionary().AddKey("OrphanSweepMarker", PoDoFo::PdfVariant(true));
            std::string junk(256 * 1024, '\0');
            quint32 s = 99;
            for (size_t i = 0; i < junk.size(); ++i) {
                s = s * 1664525u + 1013904223u;
                junk[i] = static_cast<char>((s >> 16) & 0xFF);
            }
            orphan.GetOrCreateStream().SetData(PoDoFo::charbuff(std::string_view(junk)));

            // NoCollectGarbage: the orphan must reach the file for this test —
            // a default Save() would sweep it before the fixture exists.
            doc.Save(pdf.toUtf8().constData(), PoDoFo::PdfSaveOptions::NoCollectGarbage);
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        // Fixture sanity: the orphan really is in the loaded document.
        qint64 orphanCount = 0;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
                PoDoFo::PdfObject* o = *it;
                if (o->IsDictionary() && o->GetDictionary().FindKey("OrphanSweepMarker"))
                    ++orphanCount;
            }
        }
        QCOMPARE(orphanCount, qint64(1));

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        // (a) The estimate must report real sweep savings when the option is on…
        OptimizeEstimate estOn;
        {
            OptimizeOptions opts;
            opts.downsampleImages = false;
            opts.targetDpi = 150;
            opts.jpegQuality = 75;
            opts.deduplicateImages = false;
            opts.subsetFonts = false;
            opts.removeUnusedObjects = true;
            opts.stripMetadata = false;
            estOn = engine.estimateOptimization(opts);
        }
        QVERIFY2(estOn.originalBytes > 200 * 1024,
                 qPrintable(QString("fixture too small for the orphan premise: %1 bytes")
                     .arg(estOn.originalBytes)));
        QVERIFY2(estOn.estimatedBytes <= estOn.originalBytes - 100 * 1024,
                 qPrintable(QString("estimate must reflect the ~256 KiB orphaned stream for "
                                    "removeUnusedObjects=true: original=%1 estimated=%2")
                     .arg(estOn.originalBytes).arg(estOn.estimatedBytes)));

        // …and must NOT claim sweep savings when the option is off.
        OptimizeEstimate estOff;
        {
            OptimizeOptions opts;
            opts.downsampleImages = false;
            opts.targetDpi = 150;
            opts.jpegQuality = 75;
            opts.deduplicateImages = false;
            opts.subsetFonts = false;
            opts.removeUnusedObjects = false;
            opts.stripMetadata = false;
            estOff = engine.estimateOptimization(opts);
        }
        QVERIFY2(estOff.estimatedBytes == estOff.originalBytes,
                 qPrintable(QString("estimate must not claim sweep savings for "
                                    "removeUnusedObjects=false: original=%1 estimated=%2")
                     .arg(estOff.originalBytes).arg(estOff.estimatedBytes)));

        // (b) The write path must physically remove the orphan.
        OptimizeOptions opts;
        opts.downsampleImages = false;
        opts.targetDpi = 150;
        opts.jpegQuality = 75;
        opts.deduplicateImages = false;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = true;
        opts.stripMetadata = false;
        QString out = tmpPath("orphan_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        QCOMPARE(doc.GetPages().GetCount(), 1u); // real page tree untouched
        orphanCount = 0;
        for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* o = *it;
            if (o->IsDictionary() && o->GetDictionary().FindKey("OrphanSweepMarker"))
                ++orphanCount;
        }
        QCOMPARE(orphanCount, qint64(0));
        auto imgs = findImages(doc, SMALL_W, SMALL_H);
        QCOMPARE(imgs.size(), 1); // the referenced image survives the sweep
        QVERIFY2(rawStream(*imgs[0].obj) == smallJpeg,
                 "referenced image must survive the sweep byte-identical");
        QVERIFY2(QFileInfo(out).size() < QFileInfo(pdf).size() - 100 * 1024,
                 qPrintable(QString("output (%1) must be materially smaller than input (%2)")
                     .arg(QFileInfo(out).size()).arg(QFileInfo(pdf).size())));
    }

    // ── §9.13 P1: FlateDecode RGB/Gray downsampling coverage ────────────────
    // /DeviceGray and /DeviceRGB 8bpc images carried by /FlateDecode must be
    // decoded (PoDoFo's own stream expansion) and re-encoded as real
    // /DCTDecode JPEG, exactly like the pre-existing JPEG path.
    void flateGrayAndRgbImagesAreReencodedToJpeg() {
        const int W = 1240, H = 1754; // estDpi ≈ 150 > 72 * 1.2 threshold
        QString pdf = tmpPath("flate.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);

            // Smooth gradients — decode-friendly content.
            QImage gray(W, H, QImage::Format_Grayscale8);
            for (int y = 0; y < H; ++y) {
                uchar* line = gray.scanLine(y);
                for (int x = 0; x < W; ++x)
                    line[x] = static_cast<uchar>((x + y) & 0xFF);
            }
            auto gimg = doc.CreateImage();
            gimg->SetData(PoDoFo::bufferview(reinterpret_cast<const char*>(gray.constBits()),
                                            static_cast<size_t>(W) * H),
                          W, H, PoDoFo::PdfPixelFormat::Grayscale);

            QImage rgb(W, H, QImage::Format_RGB888);
            for (int y = 0; y < H; ++y) {
                uchar* line = rgb.scanLine(y);
                for (int x = 0; x < W; ++x) {
                    line[x * 3 + 0] = static_cast<uchar>(x & 0xFF);
                    line[x * 3 + 1] = static_cast<uchar>(y & 0xFF);
                    line[x * 3 + 2] = static_cast<uchar>((x ^ y) & 0xFF);
                }
            }
            auto rimg = doc.CreateImage();
            rimg->SetData(PoDoFo::bufferview(reinterpret_cast<const char*>(rgb.constBits()),
                                            static_cast<size_t>(W) * H * 3),
                          W, H, PoDoFo::PdfPixelFormat::RGB24);

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*gimg, 40, 420, 200.0, 280.0);
            painter.DrawImage(*rimg, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        // Fixture sanity: both images must genuinely be FlateDecode streams.
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            QCOMPARE(findImages(doc, W, H).size(), 2);
            for (const auto& im : findImages(doc, W, H))
                QVERIFY2(filterIs(*im.obj, "FlateDecode"),
                         "fixture images must be /FlateDecode streams");
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 72;
        opts.jpegQuality = 50;
        opts.deduplicateImages = false;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = false;
        opts.stripMetadata = false;
        QString out = tmpPath("flate_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        QCOMPARE(doc.GetPages().GetCount(), 1u);

        auto outImages = findImages(doc); // dims changed by the downsample
        QCOMPARE(outImages.size(), 2);
        int dctCount = 0;
        for (const auto& im : outImages) {
            QVERIFY2(filterIs(*im.obj, "DCTDecode"),
                     qPrintable(QString("FlateDecode image %1x%2 must be re-encoded to "
                                        "/DCTDecode").arg(im.w).arg(im.h)));
            QVERIFY2(!im.obj->GetDictionary().FindKey("DecodeParms"),
                     "stale /DecodeParms must be removed when re-encoding to DCTDecode");
            QByteArray raw = rawStream(*im.obj);
            QImage check;
            QVERIFY2(check.loadFromData(raw, "JPEG"),
                     "re-encoded stream must be a decodable JPEG");
            QCOMPARE(check.width(), static_cast<int>(im.w));
            QVERIFY2(im.w >= 580 && im.w <= 610,
                     qPrintable(QString("image must be downsampled toward 72dpi, got %1x%2")
                         .arg(im.w).arg(im.h)));
            // Colorspace must stay device-operational: the gray fixture ends
            // DeviceGray, the RGB fixture DeviceRGB.
            auto* cs = im.obj->GetDictionary().FindKey("ColorSpace");
            QVERIFY2(cs && cs->IsName(), "re-encoded image must carry a /ColorSpace name");
            const std::string_view csName = cs->GetName().GetString();
            QVERIFY2(csName == "DeviceGray" || csName == "DeviceRGB",
                     qPrintable(QString("unexpected colorspace /%1")
                         .arg(QString::fromLatin1(csName.data(), int(csName.size())))));
            ++dctCount;
        }
        QCOMPARE(dctCount, 2);
        bool sawGray = false, sawRgb = false;
        for (const auto& im : outImages) {
            auto* cs = im.obj->GetDictionary().FindKey("ColorSpace");
            const std::string_view csName = cs->GetName().GetString();
            if (csName == "DeviceGray") sawGray = true;
            if (csName == "DeviceRGB") sawRgb = true;
        }
        QVERIFY2(sawGray, "the DeviceGray fixture image must be re-encoded (was skipped entirely before)");
        QVERIFY2(sawRgb, "the DeviceRGB fixture image must be re-encoded");
    }

    // A /FlateDecode image whose /DecodeParms carry a /Predictor must be
    // SKIPPED byte-identical: PoDoFo's filter expansion does not undo PNG/TIFF
    // predictors, so the expanded bytes are not pixel data and re-encoding
    // them as JPEG would silently corrupt the image.
    void predictorImageIsSkippedSafely() {
        const int W = 1240, H = 1754;
        QString pdf = tmpPath("predictor.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto img = doc.CreateImage();
            QByteArray px(W * H * 3, '\x50');
            img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         W, H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();

            // Craft the crafted-file shape AFTER the image is valid and drawn:
            // claim PNG predictor 15 over the (unpredictor-ed) data. The pass
            // must not interpret these bytes as pixels.
            img->GetDictionary().AddKey("DecodeParms", PoDoFo::PdfDictionary());
            auto parms = img->GetDictionary().FindKey("DecodeParms");
            parms->GetDictionary().AddKey("Predictor", static_cast<int64_t>(15));
            parms->GetDictionary().AddKey("Colors", static_cast<int64_t>(3));
            parms->GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
            parms->GetDictionary().AddKey("Columns", static_cast<int64_t>(W));
            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        QByteArray before;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            auto imgs = findImages(doc, W, H);
            QCOMPARE(imgs.size(), 1);
            before = rawStream(*imgs[0].obj);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.targetDpi = 72;
        opts.jpegQuality = 50;
        opts.deduplicateImages = false;
        opts.subsetFonts = false;
        opts.removeUnusedObjects = false;
        opts.stripMetadata = false;
        QString out = tmpPath("predictor_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto imgs = findImages(doc, W, H);
        QCOMPARE(imgs.size(), 1);
        QVERIFY2(filterIs(*imgs[0].obj, "FlateDecode"),
                 "predictor image must be left /FlateDecode (not re-encoded)");
        QVERIFY2(rawStream(*imgs[0].obj) == before,
                 "predictor image stream must stay byte-identical (expansion is not pixel data)");
    }

    // ── PARITY §4 row 13 (scorecard 2026-09-30): indexed downsampling ───────
    // An /Indexed /DeviceRGB 8bpc image (raw index stream) must be decoded
    // through its palette, downsampled and re-encoded as a real /DCTDecode
    // /DeviceRGB JPEG with the palette colors preserved, while a small indexed
    // image below the DPI threshold stays byte-identical. Re-indexing after
    // the smooth downsample is a net loss (interpolated pixels leave the
    // palette; snapping back would re-introduce banding), so staying expanded
    // as RGB JPEG is the documented contract.
    void indexedRgbImageIsDownsampledToRgbJpeg() {
        const int BIG_W = 1240, BIG_H = 1754;   // estDpi ≈ 150 > 72 * 1.2
        const int SMALL_W = 200, SMALL_H = 280; // estDpi ≈ 24 → below threshold
        const QRgb palette[4] = { qRgb(200, 30, 40), qRgb(30, 200, 50),
                                  qRgb(40, 60, 220), qRgb(230, 200, 40) };
        QByteArray lookup;
        for (const auto c : palette) {
            lookup.append(static_cast<char>(qRed(c)));
            lookup.append(static_cast<char>(qGreen(c)));
            lookup.append(static_cast<char>(qBlue(c)));
        }

        QString pdf = tmpPath("indexed.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto big = doc.CreateImage();
            QByteArray px(BIG_W * BIG_H * 3, '\x10');
            big->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         BIG_W, BIG_H, PoDoFo::PdfPixelFormat::RGB24);
            auto small = doc.CreateImage();
            QByteArray spx(SMALL_W * SMALL_H * 3, '\x20');
            small->SetData(PoDoFo::bufferview(spx.constData(), spx.size()),
                           SMALL_W, SMALL_H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*big, 40, 420, 200.0, 280.0);
            painter.DrawImage(*small, 40, 40, 100.0, 140.0);
            painter.FinishDrawing();

            makeIndexedImage(big->GetObject(), bandedIndices(BIG_W, BIG_H, 4),
                             BIG_W, BIG_H, lookup, 3, "DeviceRGB");
            makeIndexedImage(small->GetObject(),
                             bandedIndices(SMALL_W, SMALL_H, 4),
                             SMALL_W, SMALL_H, lookup, 3, "DeviceRGB");
            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        QByteArray smallBefore;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            auto imgs = findImages(doc, SMALL_W, SMALL_H);
            QCOMPARE(imgs.size(), 1);
            smallBefore = rawStream(*imgs[0].obj);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("indexed_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());

        // Small indexed image: untouched, byte-identical.
        auto smallImgs = findImages(doc, SMALL_W, SMALL_H);
        QCOMPARE(smallImgs.size(), 1);
        QVERIFY2(rawStream(*smallImgs[0].obj) == smallBefore,
                 "small indexed image below the DPI threshold must stay byte-identical");

        // Big indexed image: decoded via palette, downsampled, re-encoded.
        auto outImages = findImages(doc); // big dims changed by the downsample
        QCOMPARE(outImages.size(), 2);
        const FoundImage* big = nullptr;
        for (const auto& im : outImages)
            if (im.w != SMALL_W) big = &im;
        QVERIFY2(big != nullptr, "big indexed image must survive the pass");
        QVERIFY2(filterIs(*big->obj, "DCTDecode"),
                 "indexed image must be re-encoded to /DCTDecode");
        QVERIFY2(big->w >= 580 && big->w <= 610,
                 qPrintable(QString("indexed image must be downsampled toward 72dpi, "
                                    "got %1x%2").arg(big->w).arg(big->h)));
        auto* bpc = big->obj->GetDictionary().FindKey("BitsPerComponent");
        QVERIFY2(bpc && bpc->IsNumberOrReal() && bpc->GetReal() == 8,
                 "re-encoded image must stay 8bpc");
        auto* cs = big->obj->GetDictionary().FindKey("ColorSpace");
        QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceRGB",
                 "indexed/DeviceRGB image must re-encode as /DeviceRGB JPEG "
                 "(expanded, not re-indexed)");

        // Palette colors must survive decode+downsample+JPEG round-trip.
        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*big->obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        for (int band = 0; band < 4; ++band) {
            const int y = check.height() * (2 * band + 1) / 8; // band interior
            const QRgb got = check.pixel(check.width() / 2, y);
            const QRgb want = palette[band];
            QVERIFY2(qAbs(qRed(got) - qRed(want)) <= 15
                     && qAbs(qGreen(got) - qGreen(want)) <= 15
                     && qAbs(qBlue(got) - qBlue(want)) <= 15,
                 qPrintable(QString("band %1 color drifted: got rgb(%2,%3,%4), want rgb(%5,%6,%7)")
                     .arg(band).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                     .arg(qRed(want)).arg(qGreen(want)).arg(qBlue(want))));
        }
    }

    // An /Indexed /DeviceGray image must come out as a real grayscale JPEG
    // (/DeviceGray) — grayscale stays grayscale, matching the raw-gray path.
    void indexedGrayImageBecomesGrayscaleJpeg() {
        const int W = 1240, H = 1754;
        const uchar palette[2] = { 30, 200 };
        QByteArray lookup(reinterpret_cast<const char*>(palette), 2);

        QString pdf = tmpPath("indexed_gray.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto img = doc.CreateImage();
            QByteArray px(W * H * 3, '\x30');
            img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         W, H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();

            makeIndexedImage(img->GetObject(), bandedIndices(W, H, 2),
                             W, H, lookup, 1, "DeviceGray");
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("indexed_gray_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto outImages = findImages(doc);
        QCOMPARE(outImages.size(), 1);
        QVERIFY2(filterIs(*outImages[0].obj, "DCTDecode"),
                 "indexed gray image must be re-encoded to /DCTDecode");
        auto* cs = outImages[0].obj->GetDictionary().FindKey("ColorSpace");
        QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceGray",
                 "indexed/DeviceGray image must re-encode as /DeviceGray JPEG");
        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*outImages[0].obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        QCOMPARE(int(check.format()), int(QImage::Format_Grayscale8));
        const QRgb top = check.pixel(check.width() / 2, check.height() / 8);
        const QRgb bottom = check.pixel(check.width() / 2, check.height() * 7 / 8);
        QVERIFY2(qAbs(qGray(top) - 30) <= 12,
                 qPrintable(QString("top band must stay dark gray 30, got %1").arg(qGray(top))));
        QVERIFY2(qAbs(qGray(bottom) - 200) <= 12,
                 qPrintable(QString("bottom band must stay light gray 200, got %1").arg(qGray(bottom))));
    }

    // Malformed / out-of-bounded-scope indexed images must be skipped safely
    // and byte-identical:
    //  a) lookup table shorter than (hival+1) * base-components — indices
    //     would read past the palette;
    //  b) /Indexed /DeviceCMYK base — a plain /DeviceCMYK base carries NO
    //     profile and PDF 2.0 Annex B defines no default CMYK→RGB, so it
    //     stays skipped (the profile-ful counterpart — an /ICCBased /N 4
    //     base — is lifted color-managed; pinned by
    //     indexedCmykBaseWithProfileIsDownsampled);
    //  c) 4bpc indices — bounded scope is 8bpc, consistent with the raw path.
    void malformedIndexedImagesAreSkippedSafely() {
        // Widths distinct for findImages and multiples of 4: PdfImage::SetData
        // reads RGB24 rows on 4-byte-aligned strides (a width not divisible by
        // 4 throws UnexpectedEOF) — a fixture constraint, not the behavior
        // under test.
        const int W1 = 1240, W2 = 1244, W3 = 1248, H = 1754;
        QByteArray lookup4(4, '\x50');    // far too short for hival=7 RGB
        QByteArray lookupCmyk(4 * 4, '\x60');
        QByteArray idx1 = bandedIndices(W1, H, 4);
        QByteArray idx2 = bandedIndices(W2, H, 4);
        QByteArray idx4bpc((static_cast<qint64>(W3) * H + 1) / 2, '\x33');

        QString pdf = tmpPath("indexed_malformed.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto mkImage = [&](unsigned w) {
                auto img = doc.CreateImage();
                QByteArray px(static_cast<qint64>(w) * H * 3, '\x40');
                img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                             w, H, PoDoFo::PdfPixelFormat::RGB24);
                return img;
            };
            auto i1 = mkImage(W1), i2 = mkImage(W2), i3 = mkImage(W3);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*i1, 20, 500, 60.0, 80.0);
            painter.DrawImage(*i2, 100, 500, 60.0, 80.0);
            painter.DrawImage(*i3, 180, 500, 60.0, 80.0);
            painter.FinishDrawing();

            makeIndexedImage(i1->GetObject(), idx1, W1, H, lookup4, 7, "DeviceRGB"); // (a)
            makeIndexedImage(i2->GetObject(), idx2, W2, H, lookupCmyk, 3, "DeviceCMYK"); // (b)
            makeIndexedImage(i3->GetObject(), idx4bpc, W3, H,
                             QByteArray(16 * 3, '\x70'), 15, "DeviceRGB", /*bpc=*/4); // (c)
            doc.Save(pdf.toUtf8().constData());
        }

        QByteArray before1, before2, before3;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            before1 = rawStream(*findImages(doc, W1, H)[0].obj);
            before2 = rawStream(*findImages(doc, W2, H)[0].obj);
            before3 = rawStream(*findImages(doc, W3, H)[0].obj);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("indexed_malformed_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts),
                 "optimizeDocument must succeed despite malformed indexed images");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto im1 = findImages(doc, W1, H);
        auto im2 = findImages(doc, W2, H);
        auto im3 = findImages(doc, W3, H);
        QCOMPARE(im1.size(), 1);
        QCOMPARE(im2.size(), 1);
        QCOMPARE(im3.size(), 1);
        QVERIFY2(rawStream(*im1[0].obj) == before1,
                 "short-lookup indexed image must stay byte-identical");
        QVERIFY2(rawStream(*im2[0].obj) == before2,
                 "CMYK-base indexed image (no profile) must stay byte-identical "
                 "(profile-less CMYK is never guessed at — only an /ICCBased "
                 "base is lifted, color-managed)");
        QVERIFY2(rawStream(*im3[0].obj) == before3,
                 "4bpc indexed image must stay byte-identical (bounded scope: 8bpc)");
    }

    // ── PARITY row 13 second half (CMYK), feat/cmyk-decode 2026-10-04 ──────
    // CONTRACTED TRIPWIRE FLIP (per docs/research/cmyk-lcms2-plan-2026-10-04
    // §4.3): the former pin rawCmykImageStaysSkippedUntilColorManagedDecode
    // ("CMYK stays skipped; if this pin ever goes RED someone lifted the skip
    // without color management") is superseded. Qt 6.8+ on the pinned Qt
    // 6.11.0 provides the color-managed CMYK decode the old pin demanded
    // (Format_CMYK8888 + QColorSpace ICC parsing + CLUT transform,
    // probe-verified against lcms2 within ≤5/255), so profile-FUL CMYK is now
    // downsampled colorimetrically — pinned positively by the three slots
    // below. The surviving half of the old guard — and the new tripwire — is
    // THIS pin: profile-LESS CMYK stays skipped, because PDF 2.0 Annex B
    // defines no default CMYK→RGB conversion and guessing a CMYK space is
    // exactly the uncontrolled recolor the guard was written to prevent.
    // If this pin ever goes RED, someone lifted the profile-less skip or is
    // naive-converting CMYK.
    void profileLessCmykStaysSkipped() {
        const int W = 1240, H = 1754;
        const int SMALL_W = 200, SMALL_H = 280; // estDpi ≈ 24 → below threshold

        // (a) raw uncompressed /DeviceCMYK 8bpc stream — no profile anywhere.
        QString pdf = tmpPath("raw_cmyk.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto img = doc.CreateImage();
            QByteArray px(W * H * 3, '\x50');
            img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         W, H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();

            // Genuine CMYK-shaped payload: 4 bytes per pixel, uncompressed.
            QByteArray cmyk(static_cast<qint64>(W) * H * 4, '\x22');
            img->GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            img->GetObject().GetOrCreateStream().SetData(
                PoDoFo::bufferview(cmyk.constData(), static_cast<size_t>(cmyk.size())),
                /*raw=*/true);
            doc.Save(pdf.toUtf8().constData());
        }

        QByteArray before;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            before = rawStream(*findImages(doc, W, H)[0].obj);
        }

        // (b) profile-less CMYK JPEG — 4-component, Adobe-encoded, but the
        // JPEG carries no APP2 ICC profile and the dictionary names only
        // /DeviceCMYK: no source colorspace ⇒ no colorimetric transform.
        QByteArray cmykJpegNoProfile =
            encodeCmykJpeg(makeBandedCmyk8888(W, H), QColorSpace());
        QByteArray smallCmykJpegNoProfile =
            encodeCmykJpeg(makeBandedCmyk8888(SMALL_W, SMALL_H), QColorSpace());
        QVERIFY2(!cmykJpegNoProfile.isEmpty() && !smallCmykJpegNoProfile.isEmpty(),
                 "profile-less CMYK JPEG fixtures must encode");

        QString pdf2 = tmpPath("profileless_cmyk_jpeg.pdf");
        QByteArray smallBefore;
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto& big = embedJpeg(doc, page, cmykJpegNoProfile, W, H);
            big.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            auto& small = embedJpeg(doc, page, smallCmykJpegNoProfile, SMALL_W, SMALL_H);
            small.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            doc.Save(pdf2.toUtf8().constData());
        }
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf2.toUtf8().constData());
            smallBefore = rawStream(*findImages(doc, SMALL_W, SMALL_H)[0].obj);
        }

        // Run the pass on both fixtures.
        for (const QString& src : { pdf, pdf2 }) {
            PdfEditorEngine engine;
            QVERIFY2(engine.loadDocumentForEditing(src),
                     qPrintable(QStringLiteral("loadDocumentForEditing failed: %1").arg(src)));
            OptimizeOptions opts = downsampleOptions();
            QString out = tmpPath(src == pdf ? "profileless_cmyk_out.pdf"
                                             : "profileless_cmykjpeg_out.pdf");
            QVERIFY2(engine.optimizeDocument(out, opts),
                     "optimizeDocument must succeed");
            PoDoFo::PdfMemDocument doc;
            doc.Load(out.toUtf8().constData());
            if (src == pdf) {
                auto imgs = findImages(doc, W, H);
                QCOMPARE(imgs.size(), 1);
                QVERIFY2(rawStream(*imgs[0].obj) == before,
                         "raw /DeviceCMYK image without a profile must stay "
                         "byte-identical (no default CMYK→RGB in PDF 2.0 "
                         "Annex B — lifting the skip would silently recolor)");
                auto* cs = imgs[0].obj->GetDictionary().FindKey("ColorSpace");
                QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceCMYK",
                         "skipped CMYK image must keep its colorspace");
            } else {
                // Big profile-less CMYK JPEG: re-encoded nowhere — dims intact.
                // (It is above the DPI threshold but has no profile: skip.)
                auto bigs = findImages(doc, W, H);
                QCOMPARE(bigs.size(), 1);
                QVERIFY2(rawStream(*bigs[0].obj) == cmykJpegNoProfile,
                         "profile-less CMYK JPEG must stay byte-identical");
                auto* csBig = bigs[0].obj->GetDictionary().FindKey("ColorSpace");
                QVERIFY2(csBig && csBig->IsName()
                         && csBig->GetName().GetString() == "DeviceCMYK",
                         "skipped CMYK JPEG must keep its colorspace");
                // Small one below the threshold: byte-identical either way.
                auto smalls = findImages(doc, SMALL_W, SMALL_H);
                QCOMPARE(smalls.size(), 1);
                QVERIFY2(rawStream(*smalls[0].obj) == smallBefore,
                         "below-threshold image must stay byte-identical");
            }
        }
    }

    // Positive pin (DCT door): a CMYK JPEG with an EMBEDDED ICC profile must
    // be decoded through Qt's color-managed CMYK path, downsampled, and
    // re-encoded as a real /DeviceRGB JPEG whose band colors match the
    // profile-defined mapping — NOT the naive photographic conversion. A
    // small profile-ful CMYK JPEG below the DPI threshold stays byte-identical
    // (per-image scoping).
    void cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically() {
        const int BIG_W = 1240, BIG_H = 1754;   // estDpi ≈ 150 > 72 * 1.2
        const int SMALL_W = 200, SMALL_H = 280; // estDpi ≈ 24 → below threshold
        const QColorSpace profile = QColorSpace::fromIccProfile(makeCmykIccProfile());
        QVERIFY2(profile.isValid()
                 && profile.colorModel() == QColorSpace::ColorModel::Cmyk,
                 "fixture ICC profile must parse as CMYK");
        QByteArray bigJpeg = encodeCmykJpeg(makeBandedCmyk8888(BIG_W, BIG_H), profile);
        QByteArray smallJpeg = encodeCmykJpeg(makeBandedCmyk8888(SMALL_W, SMALL_H), profile);
        QVERIFY2(!bigJpeg.isEmpty() && !smallJpeg.isEmpty(), "CMYK JPEG fixtures must encode");
        // Fixture sanity: Qt decodes these as CMYK8888 with the profile
        // attached (the Adobe 0=100%-ink inversion already undone).
        {
            QImage probe;
            QVERIFY(probe.loadFromData(bigJpeg, "JPG"));
            QCOMPARE(int(probe.format()), int(QImage::Format_CMYK8888));
            QVERIFY2(probe.colorSpace().isValid()
                     && probe.colorSpace().colorModel() == QColorSpace::ColorModel::Cmyk,
                     "fixture JPEG must carry a parseable CMYK ICC profile");
        }

        QString pdf = tmpPath("cmyk_dct.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto& big = embedJpeg(doc, page, bigJpeg, BIG_W, BIG_H);
            big.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            auto& small = embedJpeg(doc, page, smallJpeg, SMALL_W, SMALL_H);
            small.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            doc.Save(pdf.toUtf8().constData());
        }
        QVERIFY2(QFileInfo::exists(pdf), "source PDF must be written");

        QByteArray smallBefore;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            smallBefore = rawStream(*findImages(doc, SMALL_W, SMALL_H)[0].obj);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("cmyk_dct_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());

        // Small profile-ful CMYK JPEG below the threshold: byte-identical.
        auto smalls = findImages(doc, SMALL_W, SMALL_H);
        QCOMPARE(smalls.size(), 1);
        QVERIFY2(rawStream(*smalls[0].obj) == smallBefore,
                 "below-threshold CMYK JPEG must stay byte-identical");

        // Big profile-ful CMYK JPEG: color-managed lift → /DeviceRGB JPEG.
        auto outImages = findImages(doc);
        QCOMPARE(outImages.size(), 2);
        const FoundImage* big = nullptr;
        for (const auto& im : outImages)
            if (im.w != SMALL_W) big = &im;
        QVERIFY2(big != nullptr, "big CMYK image must survive the pass");
        QVERIFY2(filterIs(*big->obj, "DCTDecode"),
                 "CMYK image must be re-encoded to /DCTDecode");
        QVERIFY2(big->w >= 580 && big->w <= 610,
                 qPrintable(QString("CMYK image must be downsampled toward 72dpi, "
                                    "got %1x%2").arg(big->w).arg(big->h)));
        auto* cs = big->obj->GetDictionary().FindKey("ColorSpace");
        QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceRGB",
                 "color-managed CMYK must re-encode as /DeviceRGB JPEG");
        auto* bpc = big->obj->GetDictionary().FindKey("BitsPerComponent");
        QVERIFY2(bpc && bpc->IsNumberOrReal() && bpc->GetReal() == 8,
                 "re-encoded image must stay 8bpc");

        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*big->obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        for (int band = 0; band < 4; ++band) {
            const int y = check.height() * (2 * band + 1) / 8;
            const QRgb got = check.pixel(check.width() / 2, y);
            const QRgb want = cmykBandExpectedRgb(band);
            const QRgb naive = naivePhotographicRgb(kCmykBands[band]);
            QVERIFY2(qAbs(qRed(got) - qRed(want)) <= 15
                     && qAbs(qGreen(got) - qGreen(want)) <= 15
                     && qAbs(qBlue(got) - qBlue(want)) <= 15,
                 qPrintable(QString("band %1 drifted from the profile-defined "
                                    "color: got rgb(%2,%3,%4), want rgb(%5,%6,%7)")
                     .arg(band).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                     .arg(qRed(want)).arg(qGreen(want)).arg(qBlue(want))));
            if (band < 3) { // band 3 is full-K black: naive happens to agree
                QVERIFY2(qAbs(qRed(got) - qRed(naive)) > 15
                         && qAbs(qGreen(got) - qGreen(naive)) > 15
                         && qAbs(qBlue(got) - qBlue(naive)) > 15,
                         qPrintable(QString("band %1 matches the NAIVE conversion "
                                            "(rgb(%2,%3,%4)) — a naive CMYK→RGB "
                                            "swap is the failure signature")
                             .arg(band).arg(qRed(naive)).arg(qGreen(naive))
                             .arg(qBlue(naive))));
            }
        }
    }

    // Positive pin (raw/Flate door): a raw 8bpc /ICCBased /N 4 CMYK image must
    // be decoded through its profile, downsampled and re-encoded colorimetrically.
    void rawCmykIccBasedImageIsDownsampledColorimetrically() {
        const int W = 1240, H = 1754;

        QString pdf = tmpPath("cmyk_icc_raw.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto img = doc.CreateImage();
            QByteArray px(W * H * 3, '\x40');
            img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         W, H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();

            // Craft the CMYK shape: raw uncompressed 4-bytes-per-pixel stream
            // (filter explicitly cleared) + /ICCBased /N 4 colorspace.
            QByteArray cmyk(static_cast<qint64>(W) * H * 4, '\0');
            for (int y = 0; y < H; ++y) {
                const int band = std::min(3, y * 4 / H);
                const CmykQuad b = kCmykBands[band];
                for (int x = 0; x < W; ++x) {
                    const qint64 o = (static_cast<qint64>(y) * W + x) * 4;
                    cmyk[o + 0] = char(b.c);
                    cmyk[o + 1] = char(b.m);
                    cmyk[o + 2] = char(b.y);
                    cmyk[o + 3] = char(b.k);
                }
            }
            img->GetDictionary().RemoveKey("Filter");
            img->GetObject().GetOrCreateStream().SetData(
                PoDoFo::bufferview(cmyk.constData(), static_cast<size_t>(cmyk.size())),
                /*raw=*/true);
            const auto iccRef = addIccProfileStream(doc, makeCmykIccProfile());
            PoDoFo::PdfArray cs;
            cs.Add(PoDoFo::PdfName("ICCBased"));
            cs.Add(iccRef);
            img->GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("cmyk_icc_raw_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto outImages = findImages(doc);
        QCOMPARE(outImages.size(), 1);
        QVERIFY2(filterIs(*outImages[0].obj, "DCTDecode"),
                 "raw /ICCBased CMYK image must be re-encoded to /DCTDecode");
        QVERIFY2(outImages[0].w >= 580 && outImages[0].w <= 610,
                 qPrintable(QString("image must be downsampled toward 72dpi, got %1x%2")
                     .arg(outImages[0].w).arg(outImages[0].h)));
        auto* cs = outImages[0].obj->GetDictionary().FindKey("ColorSpace");
        QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceRGB",
                 "color-managed CMYK must re-encode as /DeviceRGB JPEG");
        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*outImages[0].obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        for (int band = 0; band < 4; ++band) {
            const int y = check.height() * (2 * band + 1) / 8;
            const QRgb got = check.pixel(check.width() / 2, y);
            const QRgb want = cmykBandExpectedRgb(band);
            const QRgb naive = naivePhotographicRgb(kCmykBands[band]);
            QVERIFY2(qAbs(qRed(got) - qRed(want)) <= 15
                     && qAbs(qGreen(got) - qGreen(want)) <= 15
                     && qAbs(qBlue(got) - qBlue(want)) <= 15,
                 qPrintable(QString("band %1 drifted from the profile-defined "
                                    "color: got rgb(%2,%3,%4), want rgb(%5,%6,%7)")
                     .arg(band).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                     .arg(qRed(want)).arg(qGreen(want)).arg(qBlue(want))));
            if (band < 3) {
                QVERIFY2(qAbs(qRed(got) - qRed(naive)) > 15
                         && qAbs(qGreen(got) - qGreen(naive)) > 15
                         && qAbs(qBlue(got) - qBlue(naive)) > 15,
                         "band matches the NAIVE conversion — failure signature");
            }
        }
    }

    // Positive pin (indexed door): an /Indexed image whose base is an
    // /ICCBased /N 4 CMYK profile must be lifted — the palette is transformed
    // colorimetrically ONCE, then the shared expansion/downsample/re-encode
    // runs. Output: /DeviceRGB JPEG with the transformed palette colors.
    void indexedCmykBaseWithProfileIsDownsampled() {
        const int W = 1240, H = 1754;
        QByteArray lookup;
        for (const auto& b : kCmykBands) {
            lookup.append(char(b.c));
            lookup.append(char(b.m));
            lookup.append(char(b.y));
            lookup.append(char(b.k));
        }

        QString pdf = tmpPath("cmyk_indexed.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto img = doc.CreateImage();
            QByteArray px(W * H * 3, '\x30');
            img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                         W, H, PoDoFo::PdfPixelFormat::RGB24);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*img, 40, 40, 200.0, 280.0);
            painter.FinishDrawing();

            // Craft: raw index stream + /ColorSpace [/Indexed [/ICCBased ref]
            // 3 <lookup>] (the base is an ARRAY — the profile-ful CMYK base).
            img->GetDictionary().RemoveKey("Filter");
            img->GetObject().GetOrCreateStream().SetData(
                PoDoFo::bufferview(bandedIndices(W, H, 4).constData(),
                                   static_cast<size_t>(W * H)),
                /*raw=*/true);
            const auto iccRef = addIccProfileStream(doc, makeCmykIccProfile());
            PoDoFo::PdfArray base;
            base.Add(PoDoFo::PdfName("ICCBased"));
            base.Add(iccRef);
            PoDoFo::PdfArray cs;
            cs.Add(PoDoFo::PdfName("Indexed"));
            cs.Add(PoDoFo::PdfObject(base));
            cs.Add(static_cast<int64_t>(3));
            cs.Add(PoDoFo::PdfString::FromRaw(
                PoDoFo::bufferview(lookup.constData(), static_cast<size_t>(lookup.size())),
                /*hex=*/true));
            img->GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
            img->GetDictionary().AddKey("BitsPerComponent", static_cast<int64_t>(8));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("cmyk_indexed_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto outImages = findImages(doc);
        QCOMPARE(outImages.size(), 1);
        QVERIFY2(filterIs(*outImages[0].obj, "DCTDecode"),
                 "indexed CMYK-base image must be re-encoded to /DCTDecode");
        QVERIFY2(outImages[0].w >= 580 && outImages[0].w <= 610,
                 qPrintable(QString("image must be downsampled toward 72dpi, got %1x%2")
                     .arg(outImages[0].w).arg(outImages[0].h)));
        auto* cs = outImages[0].obj->GetDictionary().FindKey("ColorSpace");
        QVERIFY2(cs && cs->IsName() && cs->GetName().GetString() == "DeviceRGB",
                 "indexed CMYK-base image must re-encode expanded as /DeviceRGB");
        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*outImages[0].obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        for (int band = 0; band < 4; ++band) {
            const int y = check.height() * (2 * band + 1) / 8;
            const QRgb got = check.pixel(check.width() / 2, y);
            const QRgb want = cmykBandExpectedRgb(band);
            QVERIFY2(qAbs(qRed(got) - qRed(want)) <= 15
                     && qAbs(qGreen(got) - qGreen(want)) <= 15
                     && qAbs(qBlue(got) - qBlue(want)) <= 15,
                 qPrintable(QString("palette band %1 drifted: got rgb(%2,%3,%4), "
                                    "want rgb(%5,%6,%7)")
                     .arg(band).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                     .arg(qRed(want)).arg(qGreen(want)).arg(qBlue(want))));
        }
    }

    // Profile-handling refusal pin: a CMYK image whose ICC profile cannot be
    // used must stay SKIPPED byte-identical — never naive-converted, never
    // partially processed:
    //  a) raw /ICCBased /N 4 whose stream the ICC parser refuses (mangled
    //     lut16 input-channel count — parseA2B rejects it);
    //  b) raw /ICCBased with /N 3 (not a CMYK profile — cannot manage the
    //     4-channel samples);
    //  c) profile-less CMYK JPEG with a PDF-side /ICCBased profile that the
    //     parser refuses.
    void cmykWithUnusableIccProfileStaysSkipped() {
        // Widths distinct for findImages and multiples of 4 (SetData stride
        // constraint). All above the DPI threshold so they reach the decode.
        const int W1 = 1240, W2 = 1244, W3 = 1248, H = 1754;
        QByteArray mangledProfile = makeCmykIccProfile();
        const int a2b0Offset = 128 + 16;
        // Lut16TagData: 'mft2'(4) + reserved(4) + inputChannels at +8:
        mangledProfile[a2b0Offset + 8] = char(7); // != 4 → parseA2B refuses
        QVERIFY2(!QColorSpace::fromIccProfile(mangledProfile).isValid(),
                 "mangled fixture profile must be refused by the ICC parser");

        QString pdf = tmpPath("cmyk_bad_icc.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto mkImage = [&](unsigned w) {
                auto img = doc.CreateImage();
                QByteArray px(static_cast<qint64>(w) * H * 3, '\x50');
                img->SetData(PoDoFo::bufferview(px.constData(), px.size()),
                             w, H, PoDoFo::PdfPixelFormat::RGB24);
                return img;
            };
            // Only i1 and i2 are raw placeholders — NO third placeholder: the
            // case-(c) JPEG below owns W3, and an unconverted RGB placeholder
            // sharing its dimensions would be a legitimate re-encode that
            // findImages(W3,H) could not tell apart from the CMYK JPEG.
            auto i1 = mkImage(W1), i2 = mkImage(W2);
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*i1, 20, 500, 60.0, 80.0);
            painter.DrawImage(*i2, 100, 500, 60.0, 80.0);
            painter.FinishDrawing();

            // (a) and (b): raw 4-byte-per-pixel streams with ICC colorspaces.
            const auto badRef = addIccProfileStream(doc, mangledProfile);
            const auto rgbRef = addIccProfileStream(doc, makeCmykIccProfile(), /*n=*/3);
            for (PoDoFo::PdfImage* img : { i1.get(), i2.get() }) {
                QByteArray cmyk(static_cast<qint64>(img->GetObject().GetDictionary()
                                                    .FindKey("Width")->GetNumber()) * H * 4, '\x33');
                img->GetDictionary().RemoveKey("Filter");
                img->GetObject().GetOrCreateStream().SetData(
                    PoDoFo::bufferview(cmyk.constData(), static_cast<size_t>(cmyk.size())),
                    /*raw=*/true);
            }
            {
                PoDoFo::PdfArray cs;
                cs.Add(PoDoFo::PdfName("ICCBased"));
                cs.Add(badRef);
                i1->GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
            }
            {
                PoDoFo::PdfArray cs;
                cs.Add(PoDoFo::PdfName("ICCBased"));
                cs.Add(rgbRef);
                i2->GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
            }
            // (c) profile-less CMYK JPEG + parser-refused PDF-side profile.
            auto& cmykJpeg = embedJpeg(doc, page,
                                       encodeCmykJpeg(makeBandedCmyk8888(W3, H), QColorSpace()),
                                       W3, H);
            {
                const auto badRef2 = addIccProfileStream(doc, mangledProfile);
                PoDoFo::PdfArray cs;
                cs.Add(PoDoFo::PdfName("ICCBased"));
                cs.Add(badRef2);
                cmykJpeg.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfObject(std::move(cs)));
            }
            doc.Save(pdf.toUtf8().constData());
        }

        QByteArray before1, before2, before3;
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(pdf.toUtf8().constData());
            before1 = rawStream(*findImages(doc, W1, H)[0].obj);
            before2 = rawStream(*findImages(doc, W2, H)[0].obj);
            before3 = rawStream(*findImages(doc, W3, H)[0].obj);
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("cmyk_bad_icc_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto im1 = findImages(doc, W1, H);
        auto im2 = findImages(doc, W2, H);
        auto im3 = findImages(doc, W3, H);
        QCOMPARE(im1.size(), 1);
        QCOMPARE(im2.size(), 1);
        QCOMPARE(im3.size(), 1);
        QVERIFY2(rawStream(*im1[0].obj) == before1,
                 "CMYK with a parser-refused ICC profile must stay byte-identical");
        QVERIFY2(rawStream(*im2[0].obj) == before2,
                 "CMYK with a non-CMYK (/N 3) ICC profile must stay byte-identical");
        QVERIFY2(rawStream(*im3[0].obj) == before3,
                 "CMYK JPEG with a refused PDF-side profile must stay byte-identical");
    }

    // Render-color reference pin (supplementary, environment-conditional):
    // drives the SAME lift through Windows' real CoatedFOGRA39 press profile
    // and asserts the plan's §3.4 colorimetric reference values through the
    // full decode→transform→downsample→re-encode chain (probe-measured drift
    // ≤2/255). QSKIPs loudly on machines without the profile — the
    // synthetic-profile pins above carry the guarantee everywhere.
    void cmykDownsampleMatchesFogra39Reference() {
        static const char* kFogra39Path =
            "C:/Windows/System32/spool/drivers/color/CoatedFOGRA39.icc";
        QFile f39{ QString::fromLatin1(kFogra39Path) };
        if (!f39.open(QIODevice::ReadOnly))
            QSKIP("CoatedFOGRA39.icc not present on this machine — "
                  "synthetic-profile pins carry the colorimetric guarantee");
        const QColorSpace profile = QColorSpace::fromIccProfile(f39.readAll());
        if (!profile.isValid() || profile.colorModel() != QColorSpace::ColorModel::Cmyk)
            QSKIP("the installed CoatedFOGRA39.icc did not parse as CMYK");

        // Plan §3.4 reference table (Qt QColorSpace == lcms2 within ≤5/255).
        struct F39Band { uchar c, m, y, k; QRgb want; };
        const F39Band bands[4] = {
            { 0, 255, 255, 0, qRgb(227, 6, 20) },     // pure magenta
            { 255, 0, 0, 0, qRgb(0, 159, 227) },      // pure cyan
            { 0, 0, 0, 255, qRgb(29, 29, 27) },       // full black ink
            { 0, 0, 0, 0, qRgb(255, 255, 255) },      // paper white
        };
        QVector<CmykQuad> quads;
        for (const auto& b : bands)
            quads.append({ b.c, b.m, b.y, b.k });

        const int W = 1240, H = 1754;
        QString pdf = tmpPath("cmyk_fogra39.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPageSize::A4);
            auto& img = embedJpeg(
                doc, page, encodeCmykJpeg(makeBandedCmyk8888(W, H, quads), profile),
                W, H);
            img.GetDictionary().AddKey("ColorSpace", PoDoFo::PdfName("DeviceCMYK"));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        OptimizeOptions opts = downsampleOptions();
        QString out = tmpPath("cmyk_fogra39_out.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts), "optimizeDocument must succeed");

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto outImages = findImages(doc);
        QCOMPARE(outImages.size(), 1);
        QVERIFY2(filterIs(*outImages[0].obj, "DCTDecode"),
                 "FOGRA39 CMYK image must be re-encoded to /DCTDecode");
        QImage check;
        QVERIFY2(check.loadFromData(rawStream(*outImages[0].obj), "JPEG"),
                 "re-encoded stream must be a decodable JPEG");
        for (int band = 0; band < 4; ++band) {
            const int y = check.height() * (2 * band + 1) / 8;
            const QRgb got = check.pixel(check.width() / 2, y);
            const QRgb want = bands[band].want;
            QVERIFY2(qAbs(qRed(got) - qRed(want)) <= 8
                     && qAbs(qGreen(got) - qGreen(want)) <= 8
                     && qAbs(qBlue(got) - qBlue(want)) <= 8,
                 qPrintable(QString("FOGRA39 band %1 off the §3.4 reference: got "
                                    "rgb(%2,%3,%4), want rgb(%5,%6,%7)")
                     .arg(band).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                     .arg(qRed(want)).arg(qGreen(want)).arg(qBlue(want))));
        }
    }
};

QTEST_MAIN(TestCompressJpegReencode)
#include "TestCompressJpegReencode.moc"
