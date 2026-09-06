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
//
// R05 (F05): binarization must keep black ink black and paper light — the
// 1-bit Pix→QImage conversion must follow Leptonica's ON(1)=black convention
// instead of inverting polarity.
//
// R06 (F10): deskew must measure the skew on a temporary 1-bit estimator
// image (pixFindSkew rejects the 8-bit Pix with "pixs not 1 bpp" and always
// reported angle 0), apply the correction to the output image, and compose
// scale + orientation + deskew into PreprocessedImage::inverseTransform.

#include <QtTest>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QTransform>
#include <QtMath>

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
QImage rotatedScan(const QImage &upright, int cwDegrees, int dpm = kDotsPerMeter)
{
    QImage out = upright.transformed(QTransform().rotate(cwDegrees));
    out.setDotsPerMeterX(dpm);
    out.setDotsPerMeterY(dpm);
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

// ── R06 fixtures and helpers: deskew + composed coordinate transforms ───────
namespace {

// Deskew-only pipeline: dpiNormalize stays on (300 dpi fixtures make it a
// no-op, like the production default) so the assertions isolate the deskew
// step and its inverse-transform contribution.
OcrPreprocessOptions deskewOnlyOptions()
{
    OcrPreprocessOptions opts;
    opts.dpiNormalize = true;
    opts.deskew = true;
    opts.denoise = false;
    opts.binarize = false;
    opts.orientDetect = false;
    return opts;
}

// A document page tilted by \p degrees (positive = clockwise on screen, the
// QTransform convention). QImage::transformed expands the canvas to the
// transformed bounding box and fills the exposed corners with transparent
// black; a real scan would be paper all the way to the edge, so the tilted
// content is composed over an opaque white sheet before the metadata is set.
QImage tiltedScan(int degrees, int dpm = kDotsPerMeter)
{
    const QImage tilted = makeDocumentPage().transformed(QTransform().rotate(degrees));
    QImage out(tilted.size(), QImage::Format_RGB888);
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, tilted);
    painter.end();
    out.setDotsPerMeterX(dpm);
    out.setDotsPerMeterY(dpm);
    return out;
}

// A dense roman-text page (real glyph shapes — pixOrientDetect's HMT sels
// need ascenders/descenders, which synthetic bars do not have) tilted by
// \p degrees, composed over white like a real scan.
QImage tiltedTextScan(int degrees, int dpm = kDotsPerMeter)
{
    const QImage tilted = makeTextPage().transformed(QTransform().rotate(degrees));
    QImage out(tilted.size(), QImage::Format_RGB888);
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, tilted);
    painter.end();
    out.setDotsPerMeterX(dpm);
    out.setDotsPerMeterY(dpm);
    return out;
}

// Count ink pixels (darker than mid-gray).
qint64 inkCount(const QImage &image)
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    qint64 ink = 0;
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            ink += line[x] < 128;
    }
    return ink;
}

// Independent geometric skew measure (no Leptonica): the slope of the ink
// centroid between the left and right thirds of the page. For the bar/markup
// fixture the thirds contain symmetric content, so the centroid difference
// tracks the tilt: ≈ ±3° for the ±3° fixtures, ≈ 0 for a level page.
double inkSlopeDegrees(const QImage &image)
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    const int w = gray.width(), h = gray.height();
    double lx = 0, ly = 0, ln = 0, rx = 0, ry = 0, rn = 0;
    for (int y = 0; y < h; ++y) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (line[x] >= 128) continue;
            if (x < w / 3)      { lx += x; ly += y; ln += 1; }
            else if (x > 2 * w / 3) { rx += x; ry += y; rn += 1; }
        }
    }
    if (ln < 50 || rn < 50) return 0.0;
    const double slope = (ry / rn - ly / ln) / (rx / rn - lx / ln);
    return qRadiansToDegrees(qAtan(slope));
}

// The deskew inverse-transform contribution for a reported angle: a rotation
// by −angle about the centre of the image the step was applied to (see
// OcrPreprocessor.cpp — pixRotate samples output pixel q from input position
// rotate(−θ)(q), so boxes map back by rotate(−θ)).
QTransform deskewInverseAboutCenter(double angle, const QSize &size)
{
    QTransform t;
    t.translate(size.width() / 2.0, size.height() / 2.0);
    t.rotate(-angle);
    t.translate(-size.width() / 2.0, -size.height() / 2.0);
    return t;
}

// The dpiNormalize forward is an exact ×scale about the origin (dims stay
// integral for integer scale factors), so its inverse is 1/scale.
QTransform dpiNormalizeInverse(int scale) { return QTransform::fromScale(1.0 / scale, 1.0 / scale); }

// The orientation fix forward as built by OcrPreprocessor::process for a 90°
// clockwise scan (pixOrientDetect reports the 270 fix), used by the tests to
// compose the expected inverse. Kept in lockstep with the production code.
QTransform orientFixForward90Scan(int scaledScanWidth)
{
    QTransform fwd;
    fwd.translate(0, scaledScanWidth);
    fwd.rotate(270);
    return fwd;
}

void verifyPointsMapWithinTolerance(const PreprocessedImage &pp, const QTransform &expected,
                                    const QList<QPointF> &points, double tolerancePx,
                                    const char *context)
{
    for (const QPointF &p : points) {
        const QPointF got = pp.inverseTransform.map(p);
        const QPointF want = expected.map(p);
        QVERIFY2(qAbs(got.x() - want.x()) < tolerancePx &&
                 qAbs(got.y() - want.y()) < tolerancePx,
                 qPrintable(QStringLiteral("%1: point (%2,%3) mapped to (%4,%5), expected (%6,%7) "
                                          "(tolerance %8 px)")
                                .arg(QLatin1String(context), QString::number(p.x()),
                                     QString::number(p.y()), QString::number(got.x()),
                                     QString::number(got.y()), QString::number(want.x()),
                                     QString::number(want.y()), QString::number(tolerancePx))));
    }
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

    // ── R06: deskew on a 1-bit estimator + composed coordinate transforms ──
    //
    // F10: pixFindSkew requires 1 bpp. It used to be handed the 8-bit Pix,
    // printed "pixs not 1 bpp" and always reported angle 0, so a three-degree
    // tilted text image came out unchanged. Sign conventions below were
    // measured against the linked lept build with a standalone probe:
    // pixRotate(+θ) rotates content clockwise on screen (same direction as
    // QTransform::rotate(+θ)), and pixFindSkew reports the angle whose
    // pixRotate() deskews the scan — negative for clockwise-skewed content.
    // Tolerances: the estimate must land within 1° of the known tilt, the
    // residual (measured independently of Leptonica, via the ink-centroid
    // slope) within 0.5°, and mapped points within the stated pixel
    // tolerances.

    void deskewCorrectsThreeDegreeTiltBothSigns()
    {
        OcrPreprocessor pre;
        for (const int tilt : {3, -3}) {
            const QImage scan = tiltedScan(tilt);

        double reported = 0.0;
        const QImage deskewed = pre.deskew(scan, &reported);
        // The estimate matches the known tilt within 1° and has the
        // measured sign (clockwise +3° skew → negative angle).
        QVERIFY2(qAbs(reported + tilt) < 1.0,
                     qPrintable(QStringLiteral("tilt %1°: reported angle %2° outside ±1° of %3°")
                                    .arg(tilt).arg(reported).arg(-tilt)));

            // Same dimensions (rotation about the centre, no bounds change)
            // and the content is measurably more upright afterwards.
            QCOMPARE(deskewed.size(), scan.size());
            const double residual = inkSlopeDegrees(deskewed);
            QVERIFY2(qAbs(residual) < 0.5,
                     qPrintable(QStringLiteral("tilt %1°: residual slope %2° not under 0.5°")
                                    .arg(tilt).arg(residual)));
            // Content is not clipped: most of the ink survives the rotation.
            QVERIFY2(inkCount(deskewed) * 10 >= inkCount(scan) * 7,
                     "deskew must not clip the page content");

            // The full pipeline keeps the same guarantees and maps the
            // preprocessed centre back onto the scan centre.
            const PreprocessedImage pp = pre.process(scan, deskewOnlyOptions());
            QCOMPARE(pp.image.size(), scan.size());
            QVERIFY2(qAbs(inkSlopeDegrees(pp.image)) < 0.5, "pipeline output must be level");
            verifyInverseMapsToScanCenter(pp, scan);
        }
    }

    void deskewBlankPageIsDocumentedNoOp()
    {
        QImage blank(400, 300, QImage::Format_RGB888);
        blank.fill(Qt::white);
        blank.setDotsPerMeterX(kDotsPerMeter);
        blank.setDotsPerMeterY(kDotsPerMeter);

        double reported = 42.0;
        const QImage out = OcrPreprocessor().deskew(blank, &reported);
        QCOMPARE(reported, 0.0);
        QCOMPARE(out, blank); // pixel-identical: no resampling, no polarity games

        const PreprocessedImage pp = OcrPreprocessor().process(blank, deskewOnlyOptions());
        QCOMPARE(pp.image, blank);
        QVERIFY(pp.inverseTransform.isIdentity());
    }

    void deskewTinyImageIsDocumentedNoOp()
    {
        QImage tiny(40, 40, QImage::Format_RGB888); // below the 64 px estimator floor
        tiny.fill(Qt::gray);
        tiny.setDotsPerMeterX(kDotsPerMeter);
        tiny.setDotsPerMeterY(kDotsPerMeter);

        double reported = 42.0;
        const QImage out = OcrPreprocessor().deskew(tiny, &reported);
        QCOMPARE(reported, 0.0);
        QCOMPARE(out, tiny);

        const PreprocessedImage pp = OcrPreprocessor().process(tiny, deskewOnlyOptions());
        QCOMPARE(pp.image, tiny);
        QVERIFY(pp.inverseTransform.isIdentity());
    }

    // Several known points and rectangle corners must map from preprocessed
    // coords back into scan coords as the exact inverse of the deskew
    // rotation: rotate(−angle) about the scan centre.
    void deskewInverseTransformMapsPointsBack()
    {
        const QImage scan = tiltedScan(3);
        OcrPreprocessor pre;
        double reported = 0.0;
        pre.deskew(scan, &reported);
        QVERIFY(qAbs(reported) > 1.0);

        const PreprocessedImage pp = pre.process(scan, deskewOnlyOptions());
        const QTransform expected = deskewInverseAboutCenter(reported, scan.size());
        const QList<QPointF> points = {
            QPointF(scan.width() / 2.0, scan.height() / 2.0),
            QPointF(scan.width() / 2.0, 0.0),
            QPointF(0.0, scan.height() / 2.0),
            QPointF(scan.width() - 1.0, scan.height() - 1.0),
            QPointF(0.0, 0.0),
            QPointF(200, 300),
            QPointF(700, 900),
        };
        verifyPointsMapWithinTolerance(pp, expected, points, 0.5, "deskew inverse");
    }

    // DPI normalization composes on the left of the deskew step (scale about
    // the origin first, then a centre rotation), and the inverse must undo
    // them in reverse order.
    void deskewComposesWithDpiNormalization()
    {
        const int dpm = 5906; // 150 dpi
        const QImage scan = tiltedScan(3, dpm);
        const PreprocessedImage pp = OcrPreprocessor().process(scan, deskewOnlyOptions());

        QCOMPARE(pp.image.width(), scan.width() * 2);
        QCOMPARE(pp.image.height(), scan.height() * 2);
        QCOMPARE(pp.effectiveDpi, 300);

        // Replicate the pipeline's exact measurement input (the scaled image).
        const QImage scaled = scan.scaled(scan.width() * 2, scan.height() * 2,
                                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        double reported = 0.0;
        OcrPreprocessor().deskew(scaled, &reported);
        QVERIFY(qAbs(reported) > 1.0);

        const QTransform expected = deskewInverseAboutCenter(reported, scaled.size())
                                  * dpiNormalizeInverse(2);
        const QList<QPointF> points = {
            QPointF(scaled.width() / 2.0, scaled.height() / 2.0),
            QPointF(0.0, 0.0),
            QPointF(scaled.width() - 1.0, scaled.height() - 1.0),
            QPointF(scaled.width() / 2.0, 0.0),
            QPointF(500, 1200),
        };
        verifyPointsMapWithinTolerance(pp, expected, points, 0.5, "scale+deskew inverse");
        verifyInverseMapsToScanCenter(pp, scan);
    }

    // The complete forward chain — dpiNormalize scale → 270° orientation fix
    // (90° clockwise scan) → deskew — and its inverse, at stated tolerances.
    void deskewWithOrientationComposesFullPipeline()
    {
        const int dpm = 5906; // 150 dpi → ×2 normalize
        // Roman-text fixture: the orientation detector needs real glyph
        // shapes; the deskew estimator works on either fixture.
        const QImage scan = rotatedScan(tiltedTextScan(3, dpm), 90, dpm);
        OcrPreprocessOptions opts = deskewOnlyOptions();
        opts.orientDetect = true;

        const PreprocessedImage pp = OcrPreprocessor().process(scan, opts);

        QCOMPARE(pp.image.width(), scan.height() * 2);
        QCOMPARE(pp.image.height(), scan.width() * 2);
        QCOMPARE(pp.effectiveDpi, 300);
        QVERIFY2(qAbs(inkSlopeDegrees(pp.image)) < 0.5, "uprighted page must be level");

        // Replicate the pipeline's exact deskew measurement input (the
        // uprighted, scaled image) and compose the expected inverse:
        // undo deskew, then the orientation fix, then the scale.
        const QImage scaled = scan.scaled(scan.width() * 2, scan.height() * 2,
                                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QTransform fwdOrient = orientFixForward90Scan(scaled.width());
        const QImage upright = scaled.transformed(fwdOrient);
        double reported = 0.0;
        OcrPreprocessor().deskew(upright, &reported);
        QVERIFY(qAbs(reported) > 1.0);

        const QTransform expected = deskewInverseAboutCenter(reported, upright.size())
                                  * fwdOrient.inverted()
                                  * dpiNormalizeInverse(2);
        const QList<QPointF> points = {
            QPointF(upright.width() / 2.0, upright.height() / 2.0),
            QPointF(0.0, 0.0),
            QPointF(upright.width() - 1.0, upright.height() - 1.0),
            QPointF(upright.width() / 2.0, 0.0),
            QPointF(0.0, upright.height() / 2.0),
            QPointF(900, 1600),
        };
        verifyPointsMapWithinTolerance(pp, expected, points, 1.0, "scale+orient+deskew inverse");
        verifyInverseMapsToScanCenter(pp, scan);
    }
};

QTEST_MAIN(TestOcrPreprocessor)
#include "TestOcrPreprocessor.moc"
