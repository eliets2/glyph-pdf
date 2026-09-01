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
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>
#include <QImageWriter>
#include <QFile>
#include <QFileInfo>

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
            // b) CMYK colorspace — outside the supported re-encode set.
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
        // CMYK and ImageMask images must survive with dims intact.
        QVERIFY2(findImages(doc, 1240, 1754).size() == 1,
                 "CMYK image must survive untouched");
        QVERIFY2(findImages(doc, 64, 64).size() == 1,
                 "ImageMask image must survive untouched");
    }
};

QTEST_MAIN(TestCompressJpegReencode)
#include "TestCompressJpegReencode.moc"
