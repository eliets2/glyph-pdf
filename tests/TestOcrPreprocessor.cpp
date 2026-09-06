// SPDX-License-Identifier: Apache-2.0
// §9.4 P0 regression test: OcrPreprocessor::process() must honour the
// orientDetect option by detecting page-level orientation (0/90/180/270) and
// rotating the page image upright, composing the fix rotation into
// PreprocessedImage::inverseTransform so OcrPipeline's word boxes still map
// back onto the original scan coordinates (OcrPipeline.cpp mapToOriginal).
// The flag existed but was read by nobody: a rotated scan stayed rotated.
//
// The synthetic fixture paints a dense roman-text page (Leptonica's
// ascender/descender HMT sels are tuned for 150-300 ppi text — flipdetect.c),
// so detection runs on real glyph shapes rather than synthetic strokes.

#include <QtTest>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QTransform>

#include "engines/ocr/OcrPreprocessor.h"

namespace {

// 300 DPI in dots-per-meter (QImage's unit). Fixture metadata only — it keeps
// the DPI-normalize step from rescaling so the assertions isolate orientation.
constexpr int kDotsPerMeter = 11811; // qRound(300 * 39.3701)

// Letter-ish page of ~10 pt bold text (~230 ppi at 32 px em): inside the
// 150-300 ppi band Leptonica's orientation detector is tuned for.
QImage makeTextPage(int w = 850, int h = 1100)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(Qt::white);
    img.setDotsPerMeterX(kDotsPerMeter);
    img.setDotsPerMeterY(kDotsPerMeter);
    QPainter painter(&img);
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(32);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    const QString line = QStringLiteral(
        "the quick brown fox jumps over the lazy dog and packs a box of mixed biscuits ");
    for (int y = 40; y < h - 40; y += 44)
        painter.drawText(20, y, line);
    painter.end();
    return img;
}

// Pixel-exact 90-degree rotation (Qt's orthogonal path) with the DPI metadata
// preserved — simulates a scan whose rotation is baked into the content.
QImage rotatedScan(const QImage &upright, int cwDegrees)
{
    QImage out = upright.transformed(QTransform().rotate(cwDegrees));
    out.setDotsPerMeterX(kDotsPerMeter);
    out.setDotsPerMeterY(kDotsPerMeter);
    return out;
}

// Only the feature under test: deskew/denoise/binarize are off because the
// small-angle skew pass and the filters would perturb the pixel-equality
// assertions; dpiNormalize stays on with a 300 dpi fixture (a no-op, like the
// production default).
OcrPreprocessOptions orientOnlyOptions()
{
    OcrPreprocessOptions opts;
    opts.dpiNormalize = true;
    opts.deskew = false;
    opts.denoise = false;
    opts.binarize = false;
    opts.orientDetect = true;
    return opts;
}

// Contract shared with OcrPipeline: a point in the preprocessed image must map
// back into original scan coordinates. The image centre is invariant under the
// fix rotation's bbox re-anchoring, so it must land on the scan's centre.
void verifyInverseMapsToScanCenter(const PreprocessedImage &pp, const QImage &scan)
{
    const QPointF preprocessedCenter(pp.image.width() / 2.0, pp.image.height() / 2.0);
    const QPointF scanCenter(scan.width() / 2.0, scan.height() / 2.0);
    const QPointF mapped = pp.inverseTransform.map(preprocessedCenter);
    QVERIFY2(qAbs(mapped.x() - scanCenter.x()) < 0.5 &&
             qAbs(mapped.y() - scanCenter.y()) < 0.5,
             "inverseTransform must map preprocessed coords back into scan coords");
}

} // namespace

// ── R05 fixtures: polarity through binarization ─────────────────────────────
namespace {

// Flat page of a single colour with 300 dpi metadata (R05 polarity inputs:
// all-white, all-black, colored paper). Takes QColor: a QRgb parameter would
// silently accept Qt::GlobalColor as its raw enum value (Qt::white == 3).
QImage makeSolidPage(QColor color, int dpm = kDotsPerMeter)
{
    QImage img(400, 300, QImage::Format_RGB888);
    img.fill(color);
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);
    return img;
}

// Deterministic document page: light paper, five 12 px ink bars standing in
// for text lines plus four 9x9 isolated marker squares at known positions.
// The bars/markers are deliberately thin: Leptonica's Sauvola binarizer
// reclasses the interior of thick dark regions as background (measured: the
// middle of a 100x100 black box comes out background), which would make
// interior-pixel and centroid assertions flaky.
// Marker squares: (140,140), (701,140), (140,951), (701,951); each 9x9.
// Ink bars: x in [120, 730], y = 194 + 200*i .. 205 + 200*i, i in [0,4].
QImage makeDocumentPage(QColor paper = Qt::white, QColor ink = Qt::black,
                        int dpm = kDotsPerMeter,
                        QImage::Format format = QImage::Format_RGB888)
{
    QImage img(850, 1100, format);
    img.fill(paper);
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);
    QPainter painter(&img);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    for (int i = 0; i < 5; ++i)
        painter.drawRect(QRect(120, 194 + 200 * i, 610, 12));
    for (const QPointF &m : {QPointF(140, 140), QPointF(701, 140),
                             QPointF(140, 951), QPointF(701, 951)})
        painter.drawRect(QRectF(m.x(), m.y(), 9, 9));
    painter.end();
    return img;
}

} // namespace

class TestOcrPreprocessor : public QObject
{
    Q_OBJECT

private slots:
    // Zero behaviour change when off: the flag must default to false and the
    // option must remain a no-op, both for upright pages and rotated scans.
    void orientDetectDefaultsToOff()
    {
        const OcrPreprocessOptions defaults;
        QVERIFY2(!defaults.orientDetect,
                 "orientDetect must default to false — existing callers must "
                 "not gain a preprocessing step they never asked for");
    }

    void flagOffLeavesUprightPageUntouched()
    {
        const QImage page = makeTextPage();
        OcrPreprocessOptions opts = orientOnlyOptions();
        opts.orientDetect = false;

        const PreprocessedImage pp = OcrPreprocessor().process(page, opts);
        QCOMPARE(pp.image, page);
        QVERIFY(pp.inverseTransform.isIdentity());
    }

    void flagOffLeavesRotatedScanUntouched()
    {
        const QImage page = makeTextPage();
        const QImage scan = rotatedScan(page, 90);
        OcrPreprocessOptions opts = orientOnlyOptions();
        opts.orientDetect = false;

        // The pre-fix status quo, kept as a pinned assertion: with the flag
        // off the pipeline must not touch the image at all.
        const PreprocessedImage pp = OcrPreprocessor().process(scan, opts);
        QCOMPARE(pp.image, scan);
        QVERIFY(pp.inverseTransform.isIdentity());
    }

    // An upright page must not be rotated (no false positive).
    void uprightPageIsNotRotated()
    {
        const QImage page = makeTextPage();

        const PreprocessedImage pp = OcrPreprocessor().process(page, orientOnlyOptions());
        QCOMPARE(pp.image, page);
        QVERIFY(pp.inverseTransform.isIdentity());
        verifyInverseMapsToScanCenter(pp, page);
    }

    // 90-degree scans: the correction swaps the dimensions back to the upright
    // page's, restores it pixel-exactly (the orthogonal fix rotation is exact),
    // and maps preprocessed coords back into the rotated scan's frame.
    void rotated90IsDetectedAndCorrected()
    {
        const QImage page = makeTextPage();
        const QImage scan = rotatedScan(page, 90);

        const PreprocessedImage pp = OcrPreprocessor().process(scan, orientOnlyOptions());
        QCOMPARE(pp.image.width(), page.width());
        QCOMPARE(pp.image.height(), page.height());
        QCOMPARE(pp.image, page);
        verifyInverseMapsToScanCenter(pp, scan);
    }

    // 180-degree scans keep their dimensions, so the pixel comparison is the
    // discriminating assertion (unfixed content is upside-down).
    void rotated180IsDetectedAndCorrected()
    {
        const QImage page = makeTextPage();
        const QImage scan = rotatedScan(page, 180);

        const PreprocessedImage pp = OcrPreprocessor().process(scan, orientOnlyOptions());
        QCOMPARE(pp.image, page);
        verifyInverseMapsToScanCenter(pp, scan);
    }

    void rotated270IsDetectedAndCorrected()
    {
        const QImage page = makeTextPage();
        const QImage scan = rotatedScan(page, 270);

        const PreprocessedImage pp = OcrPreprocessor().process(scan, orientOnlyOptions());
        QCOMPARE(pp.image.width(), page.width());
        QCOMPARE(pp.image.height(), page.height());
        QCOMPARE(pp.image, page);
        verifyInverseMapsToScanCenter(pp, scan);
    }

    // Edge cases: null and degenerate images must pass through untouched.
    void nullImageIsHandled()
    {
        const PreprocessedImage pp = OcrPreprocessor().process(QImage(), orientOnlyOptions());
        QVERIFY(pp.image.isNull());
    }

    void tinyOneByOneImageIsHandled()
    {
        QImage tiny(1, 1, QImage::Format_RGB888);
        tiny.fill(Qt::white);
        tiny.setDotsPerMeterX(kDotsPerMeter);
        tiny.setDotsPerMeterY(kDotsPerMeter);

        const PreprocessedImage pp = OcrPreprocessor().process(tiny, orientOnlyOptions());
        QCOMPARE(pp.image.size(), tiny.size());
        QVERIFY(pp.inverseTransform.isIdentity());
    }

    // ── R05: black text must stay black and paper light through binarization ──
    //
    // Leptonica 1 bpp convention (measured against the linked lept build):
    // ON (1) = black ink, OFF (0) = white paper — pixConvertTo8() of an
    // all-ON 1 bpp pix is all zeros. The 1-bit Pix→QImage conversion must
    // therefore map 1 → 0 and 0 → 255; the pre-fix `val ? 255 : 0` inverted
    // polarity (F05: a white page came out black).

    // Isolated binarizer on a flat white page: paper must stay light.
    void binarizeAllWhiteStaysLight()
    {
        const QImage white = makeSolidPage(Qt::white);
        const QImage out = OcrPreprocessor().binarize(white);
        QVERIFY(!out.isNull());
        QCOMPARE(out.size(), white.size());
        QCOMPARE(out.format(), QImage::Format_Grayscale8);
        QCOMPARE(out.pixel(200, 150), QRgb(0xffffffff));
        QCOMPARE(out.pixel(10, 10), QRgb(0xffffffff));
    }

    // Flat black page: Leptonica's Sauvola classes flat regions as background
    // (measured), so the conversion must come out light — not the inverted
    // all-black the pre-fix mapping produced.
    void binarizeAllBlackComesOutLightPerLeptonicaSemantics()
    {
        const QImage black = makeSolidPage(Qt::black);
        const QImage out = OcrPreprocessor().binarize(black);
        QVERIFY(!out.isNull());
        QCOMPARE(out.size(), black.size());
        QCOMPARE(out.pixel(200, 150), QRgb(0xffffffff));
        QCOMPARE(out.pixel(10, 10), QRgb(0xffffffff));
    }

    // Isolated binarizer on a document page: paper stays light, ink stays dark
    // at known interior pixels.
    void binarizedDocumentKeepsDarkInkOnLightPaper()
    {
        const QImage page = makeDocumentPage();
        const QImage out = OcrPreprocessor().binarize(page);
        QVERIFY(!out.isNull());
        QCOMPARE(out.format(), QImage::Format_Grayscale8);
        // paper interior: light
        QCOMPARE(out.pixel(60, 60), QRgb(0xffffffff));
        QCOMPARE(out.pixel(800, 60), QRgb(0xffffffff));
        QCOMPARE(out.pixel(60, 1060), QRgb(0xffffffff));
        // bar interiors: dark
        for (int i = 0; i < 5; ++i)
            QCOMPARE(out.pixel(400, 200 + 200 * i), QRgb(0xff000000));
    }

    // The full pipeline with production defaults (dpiNormalize + deskew +
    // denoise + binarize) must preserve the same polarity.
    void processDefaultsKeepDarkInkOnLightPaper()
    {
        const QImage page = makeDocumentPage();
        const PreprocessedImage pp = OcrPreprocessor().process(page, {});

        QCOMPARE(pp.image.format(), QImage::Format_Grayscale8);
        QCOMPARE(pp.image.pixel(60, 60), QRgb(0xffffffff));
        QCOMPARE(pp.image.pixel(800, 60), QRgb(0xffffffff));
        for (int i = 0; i < 5; ++i)
            QCOMPARE(pp.image.pixel(400, 200 + 200 * i), QRgb(0xff000000));
    }

    // Grayscale and colored inputs keep their polarity too (dark strokes on
    // light paper in, dark strokes on light paper out); dimensions and format
    // are preserved through the binarizer.
    void binarizeGrayscaleAndColoredInputsPreservePolarity()
    {
        const QImage grayPage = makeDocumentPage(QColor(0xffcdcdcd), QColor(0xff0a0a0a),
                                                 kDotsPerMeter, QImage::Format_Grayscale8);
        const QImage coloredPage = makeDocumentPage(QColor(0xffe8e850), QColor(0xff1e3cb4));

        for (const QImage &page : {grayPage, coloredPage}) {
            const QImage out = OcrPreprocessor().binarize(page);
            QVERIFY(!out.isNull());
            QCOMPARE(out.size(), page.size());
            QCOMPARE(out.format(), QImage::Format_Grayscale8);
            QCOMPARE(out.pixel(60, 60), QRgb(0xffffffff));
            for (int i = 0; i < 5; ++i)
                QCOMPARE(out.pixel(400, 200 + 200 * i), QRgb(0xff000000));
        }
    }

    // DPI metadata: the binarizer builds a fresh QImage, it must carry the
    // input's resolution over (F05 acceptance: preserve DPI metadata).
    void binarizePreservesDpiMetadata()
    {
        const QImage page = makeDocumentPage();
        const QImage out = OcrPreprocessor().binarize(page);
        QCOMPARE(out.dotsPerMeterX(), page.dotsPerMeterX());
        QCOMPARE(out.dotsPerMeterY(), page.dotsPerMeterY());
    }
};

QTEST_MAIN(TestOcrPreprocessor)
#include "TestOcrPreprocessor.moc"
