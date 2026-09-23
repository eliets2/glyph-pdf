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
};

QTEST_MAIN(TestCompressJpegReencode)
#include "TestCompressJpegReencode.moc"
