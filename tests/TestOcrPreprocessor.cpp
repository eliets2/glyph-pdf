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
};

QTEST_MAIN(TestOcrPreprocessor)
#include "TestOcrPreprocessor.moc"
