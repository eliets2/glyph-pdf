// SPDX-License-Identifier: Apache-2.0
// Rotate View port (Wave 2b tail): session-only view rotation, ported from
// feature/viewing-parity cd82d701 and adapted to the CURRENT viewer
// architecture (the persisted Document▸Rotate route keeps its own path).
//
// Contract under test:
//   1. The RotateViewCW/CCW ToolIds exist, round-trip through the registry
//      strings, and are DISTINCT from the persisted RotateCW/RotateCCW.
//   2. Rotating the view is SESSION-ONLY: it never emits requestPageRotation
//      (no engine-side /Rotate write, no undo-stack entry) and cycles
//      90 → 180 → 270 → 0.
//   3. renderPage() genuinely rotates the rendered BITMAP: dimensions swap
//      for 90/270 and page content moves with the rotation (top-left marker
//      lands top-right after one clockwise step) — not just an overlay.
//   4. While rotated, the native QPdfView surface is replaced by the
//      disclosed fit-to-view fallback (objectName "rotatedPageView") showing
//      the swapped aspect; at 0 the native view returns. This is the honest,
//      disclosed limitation: free pixel-scrolling pauses while rotated.
//      The fallback SURFACE ITSELF carries that disclosure (r4-ux): an
//      accessibleName + accessibleDescription so a screen-reader user never
//      gets an unlabeled image exactly while scrolling is paused.
//   5. The session-only state resets on reload (the close/reload reset path
//      is loadDocument(), which every open crosses).
#include <QtTest/QtTest>
#include <QLabel>
#include <QPdfView>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include "core/ToolId.h"
#include "ui/PdfViewerWidget.h"

class TestRotateView : public QObject {
    Q_OBJECT
private slots:
    void rotateViewToolResolvesAndRoundTrips();
    void viewRotationIsSessionOnly();
    void renderPageReflectsViewRotation();
    void fallbackSwapsSurfacesAndReverts();
    void viewRotationResetsOnReload();

private:
    static QString writeMarkerPdf(const QTemporaryDir &dir, const QString &name);
    static bool isDark(const QImage &img, const QPointF &p);
    static bool isPaper(const QImage &img, const QPointF &p);
};

// One-page A4 PORTRAIT pdf with an asymmetric marker: a solid black square in
// the page's TOP-LEFT quadrant. After a clockwise view rotation that marker
// must appear in the TOP-RIGHT quadrant of the rotated bitmap.
QString TestRotateView::writeMarkerPdf(const QTemporaryDir &dir, const QString &name)
{
    const QString pdf = dir.filePath(name);
    {
        QPdfWriter w(pdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        w.setPageMargins(QMarginsF(0, 0, 0, 0));
        w.setResolution(72);
        QPainter p(&w);
        p.fillRect(40, 40, 160, 160, Qt::black);   // top-left quadrant marker
        p.fillRect(420, 640, 120, 120, QColor(0, 0, 200)); // bottom-right counterweight
        p.end();
    }
    return pdf;
}

bool TestRotateView::isDark(const QImage &img, const QPointF &pt)
{
    const QColor c = img.pixelColor(pt.toPoint());
    return c.red() < 80 && c.green() < 80 && c.blue() < 80;
}

bool TestRotateView::isPaper(const QImage &img, const QPointF &pt)
{
    const QColor c = img.pixelColor(pt.toPoint());
    return c.red() > 200 && c.green() > 200 && c.blue() > 200;
}

void TestRotateView::rotateViewToolResolvesAndRoundTrips()
{
    // Registry presence + round-trip (Menu bar items dispatch by these keys).
    const auto cw = toolIdFromString(QStringLiteral("rotateViewCW"));
    QVERIFY2(cw.has_value(), "rotateViewCW must resolve to a ToolId");
    QCOMPARE(*cw, ToolId::RotateViewCW);
    const auto ccw = toolIdFromString(QStringLiteral("rotateViewCCW"));
    QVERIFY2(ccw.has_value(), "rotateViewCCW must resolve to a ToolId");
    QCOMPARE(*ccw, ToolId::RotateViewCCW);
    QCOMPARE(toolIdToString(ToolId::RotateViewCW), QStringLiteral("rotateViewCW"));
    QCOMPARE(toolIdToString(ToolId::RotateViewCCW), QStringLiteral("rotateViewCCW"));
    // Distinct from the PERSISTED Document▸Rotate tools.
    QVERIFY(ToolId::RotateViewCW != ToolId::RotateCW);
    QVERIFY(ToolId::RotateViewCCW != ToolId::RotateCCW);
}

void TestRotateView::viewRotationIsSessionOnly()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeMarkerPdf(tmp, QStringLiteral("session.pdf"));

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    QCOMPARE(viewer.viewRotation(), 0);

    int engineDegrees = 0;
    connect(&viewer, &PdfViewerWidget::requestPageRotation,
            [&](int d) { engineDegrees += d; });

    // Session rotation cycles and never asks the engine to write /Rotate —
    // no requestPageRotation emission, hence no RotatePageCommand, hence
    // nothing on the undo stack and no document mutation.
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 90);
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 180);
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 270);
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 0);
    viewer.rotateViewCounterClockwise();
    QCOMPARE(viewer.viewRotation(), 270);
    viewer.rotateViewCounterClockwise();
    QCOMPARE(viewer.viewRotation(), 180);
    QCOMPARE(engineDegrees, 0);

    viewer.resetViewRotation();
    QCOMPARE(viewer.viewRotation(), 0);
}

void TestRotateView::renderPageReflectsViewRotation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeMarkerPdf(tmp, QStringLiteral("marker.pdf"));

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    const QImage upright = viewer.renderPage(0, 1.0);
    QVERIFY(!upright.isNull());
    QVERIFY2(upright.height() > upright.width(), "A4 portrait baseline");

    // Baseline: the marker is in the TOP-LEFT quadrant, paper elsewhere.
    QVERIFY2(isDark(upright, QPointF(upright.width() * 0.20, upright.height() * 0.20)),
             "top-left marker must be dark in the upright render");

    viewer.rotateViewClockwise();   // session-only 90° CW
    const QImage turned = viewer.renderPage(0, 1.0);
    QVERIFY(!turned.isNull());
    // The real bitmap turned: 90° swaps the output dimensions.
    QCOMPARE(turned.width(), upright.height());
    QCOMPARE(turned.height(), upright.width());
    // Content moves with the rotation: top-left marker → top-right quadrant.
    QVERIFY2(isDark(turned, QPointF(turned.width() * 0.80, turned.height() * 0.20)),
             "top-left marker must land top-right after a clockwise view rotation");
    QVERIFY2(isPaper(turned, QPointF(turned.width() * 0.20, turned.height() * 0.20)),
             "former marker corner must be paper after the rotation");

    // A full cycle returns the exact upright orientation.
    viewer.rotateViewClockwise();
    viewer.rotateViewClockwise();
    viewer.rotateViewClockwise();
    const QImage back = viewer.renderPage(0, 1.0);
    QCOMPARE(back.width(), upright.width());
    QCOMPARE(back.height(), upright.height());
    QVERIFY(isDark(back, QPointF(back.width() * 0.20, back.height() * 0.20)));
}

void TestRotateView::fallbackSwapsSurfacesAndReverts()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeMarkerPdf(tmp, QStringLiteral("surfaces.pdf"));

    PdfViewerWidget viewer;
    viewer.resize(800, 600);
    QVERIFY(viewer.loadDocument(pdf));

    auto *native = viewer.findChild<QPdfView *>(QStringLiteral("pdfView"));
    QVERIFY(native);
    QVERIFY2(!native->isHidden(), "native view must be up at rotation 0");
    QVERIFY(!viewer.findChild<QLabel *>(QStringLiteral("rotatedPageView")));

    // Rotate: the disclosed fallback replaces the native surface (QPdfView
    // exposes no rotation API) and shows the swapped (landscape) aspect.
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 90);
    auto *fallback = viewer.findChild<QLabel *>(QStringLiteral("rotatedPageView"));
    QVERIFY2(fallback, "the rotated-view fallback surface must exist while rotated");
    QVERIFY2(native->isHidden(), "native QPdfView must stand down while rotated");
    QVERIFY2(!fallback->isHidden(), "the fallback must be the visible page surface");
    const QPixmap pm = fallback->pixmap();
    QVERIFY2(!pm.isNull(), "the fallback must carry the rotated page bitmap");
    QVERIFY2(pm.width() > pm.height(),
             "the fallback bitmap must show the swapped (landscape) aspect");

    // r4-ux: the fallback must disclose itself. A pixmap-only QLabel is an
    // unlabeled image to a screen-reader user exactly while free scrolling
    // is paused — the accessible name names the surface, the accessible
    // description discloses the paused scrolling AND the surviving page
    // navigation (the same contract the Rotate View status message tells
    // sighted users).
    const QString accName = fallback->accessibleName();
    QVERIFY2(accName.contains(QStringLiteral("Rotated"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("fallback accessibleName must name the "
                                      "rotated surface, got \"%1\"").arg(accName)));
    const QString accDesc = fallback->accessibleDescription();
    QVERIFY2(accDesc.contains(QStringLiteral("scrolling"), Qt::CaseInsensitive)
             && accDesc.contains(QStringLiteral("paus"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("fallback accessibleDescription must "
                                      "disclose the paused scrolling, got \"%1\"")
                            .arg(accDesc)));
    QVERIFY2(accDesc.contains(QStringLiteral("page navigation"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("fallback accessibleDescription must point "
                                      "at the surviving page navigation, got \"%1\"")
                            .arg(accDesc)));

    // Back to 0: the native view returns for full scrolling fidelity.
    viewer.rotateViewCounterClockwise();
    QCOMPARE(viewer.viewRotation(), 0);
    QVERIFY2(!native->isHidden(), "native view must return at rotation 0");
    QVERIFY2(fallback->isHidden(), "the fallback must stand down at rotation 0");
}

void TestRotateView::viewRotationResetsOnReload()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeMarkerPdf(tmp, QStringLiteral("reload.pdf"));

    PdfViewerWidget viewer;
    viewer.resize(800, 600);
    QVERIFY(viewer.loadDocument(pdf));
    viewer.rotateViewClockwise();
    QCOMPARE(viewer.viewRotation(), 90);

    // Session-only contract: a (re)load — the path every document close/open
    // crosses — resets the view rotation and restores the native surface, so
    // the next document can never inherit the previous session's rotation.
    viewer.reload();
    QCOMPARE(viewer.viewRotation(), 0);
    auto *native = viewer.findChild<QPdfView *>(QStringLiteral("pdfView"));
    QVERIFY(native);
    QVERIFY2(!native->isHidden(), "native view must be back after reload");
    const QImage after = viewer.renderPage(0, 1.0);
    QVERIFY(!after.isNull());
    QVERIFY2(after.height() > after.width(),
             "after a reload the render must be the upright portrait again");
}

QTEST_MAIN(TestRotateView)
#include "TestRotateView.moc"
