// SPDX-License-Identifier: Apache-2.0
// §9.1 P0 regression test (audit 2026-07-01): two-page mode must not silently
// hide annotations and search highlights. Today setTwoPageMode(true) hides
// m_pdfView AND m_annotationLayer and shows two static page pixmaps, so every
// annotation, search highlight and OCR overlay the user placed vanishes with
// no warning — a trust/correctness gap.
//
// Contract under test (Option A — composite overlays onto the two-page pixmaps
// from the SAME models the single-page overlay path consumes):
//   1. An AnnotationItem placed via setAnnotations() (the same API the app's
//      controllers use) is visible on the page it belongs to, left AND right
//      side of the spread.
//   2. QPdfSearchModel results (the live search-highlight model QPdfView paints
//      in single-page mode) are visible on the spread pages.
//   3. Toggling two-page mode never drops or duplicates AnnotationItems, and
//      leaving it restores the live AnnotationLayer (single-page non-regression).
//   4. Odd page count: the last unpaired page still shows its annotation (left
//      slot), the missing right slot stays empty.
//   5. Empty annotation list / no document loaded: safe, no crash, pages still
//      render.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QLabel>
#include <QScrollArea>
#include <QPdfLink>
#include <podofo/podofo.h>
#include "ui/PdfViewerWidget.h"

class TestTwoPageOverlay : public QObject {
    Q_OBJECT
private slots:
    void twoPageShowsAnnotationsOnLeftAndRightPages();
    void twoPageShowsSearchHighlights();
    void modeTogglingPreservesAnnotations();
    void unpairedLastPageShowsItsAnnotation();
    void emptyAnnotationsAndMissingDocumentAreSafe();

private:
    static QString createNPagePdf(const QTemporaryDir &tmpDir, const QString &name,
                                  int pageCount, const QString &text);
    static QLabel *pageLabel(PdfViewerWidget &viewer, const char *objectName);
    static AnnotationItem makeMark(int pageIndex, const QRectF &viewRect);
    static bool magentaPresent(const QImage &img, const QRectF &viewRect, QString *why = nullptr);
    static bool coloredPixelIn(const QImage &img, const QRect &band);
};

// 3-page A4 PDF, one line of text at the same known PDF point on every page
// (bottom-left origin: x=50, y=700 on a 595x842 page).
QString TestTwoPageOverlay::createNPagePdf(const QTemporaryDir &tmpDir, const QString &name,
                                           int pageCount, const QString &text)
{
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int i = 0; i < pageCount; ++i) {
            auto &page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto &font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(text.toUtf8().constData(), 50, 700);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception &) {
        return {};
    }
    return path;
}

// The two-page labels live under the two-page QScrollArea. Prefer the explicit
// objectNames the fix gives them; fall back to creation order (left label is
// created first) so a reverted src tree still fails on the real assertion.
QLabel *TestTwoPageOverlay::pageLabel(PdfViewerWidget &viewer, const char *objectName)
{
    if (auto *named = viewer.findChild<QLabel *>(objectName))
        return named;
    // Fallback for a reverted src tree (no objectNames): the only QLabels the
    // viewer ever gives a pixmap to are the two-page labels, created left
    // before right.
    QList<QLabel *> labeled;
    const auto labels = viewer.findChildren<QLabel *>();
    for (QLabel *l : labels)
        if (!l->pixmap().isNull())
            labeled.append(l);
    if (labeled.count() < 2)
        return nullptr;
    return qstrcmp(objectName, "twoPageLeftLabel") == 0 ? labeled.first() : labeled.last();
}

AnnotationItem TestTwoPageOverlay::makeMark(int pageIndex, const QRectF &viewRect)
{
    AnnotationItem a;
    a.pageIndex = pageIndex;
    a.mode = ToolMode::Highlight;      // filled shape → pixel-checkable
    a.color = QColor(Qt::magenta);     // cannot collide with white page / black text
    a.thickness = 10;
    a.rect = viewRect;
    return a;
}

// The two-page pixmaps are rendered at zoom*2 and the zoom factor is pinned to
// 1.0, so a view-space rect lands at rect*2 in the pixmap, anchored at the
// page's top-left. Highlight fills with anno.color at alpha 100 over the white
// page → ≈(255,155,255) at the rect center.
bool TestTwoPageOverlay::magentaPresent(const QImage &img, const QRectF &viewRect, QString *why)
{
    if (img.isNull()) {
        if (why) *why = QStringLiteral("image is null");
        return false;
    }
    const QRectF imgRect(viewRect.x() * 2.0, viewRect.y() * 2.0,
                         viewRect.width() * 2.0, viewRect.height() * 2.0);
    const QPointF center = imgRect.center();
    for (const QPointF &probe : {center, center + QPointF(2, 0), center + QPointF(0, 2),
                                 center - QPointF(2, 0), center - QPointF(0, 2)}) {
        const QColor c = img.pixelColor(probe.toPoint());
        if (c.red() > 240 && c.blue() > 240 && c.green() < 200)
            return true;
        if (why)
            *why = QStringLiteral("pixel at (%1,%2) = rgb(%3,%4,%5)")
                       .arg(probe.x()).arg(probe.y())
                       .arg(c.red()).arg(c.green()).arg(c.blue());
    }
    return false;
}

// True if any pixel in the band is saturated (a colored overlay mark), i.e.
// neither the white page background, nor black/gray text, nor antialiasing.
bool TestTwoPageOverlay::coloredPixelIn(const QImage &img, const QRect &band)
{
    const QRect clipped = band.intersected(img.rect());
    for (int y = clipped.top(); y <= clipped.bottom(); y += 2)
        for (int x = clipped.left(); x <= clipped.right(); x += 2) {
            const QColor c = img.pixelColor(x, y);
            const int sat = qMax(c.red(), qMax(c.green(), c.blue()))
                          - qMin(c.red(), qMin(c.green(), c.blue()));
            if (sat > 40)
                return true;
        }
    return false;
}

void TestTwoPageOverlay::twoPageShowsAnnotationsOnLeftAndRightPages()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdf(tmp, "spread.pdf", 3, QStringLiteral("needle target"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    QCOMPARE(viewer.zoomLevel(), 1.0);

    // One mark on the left page of the spread (page 0), one on the right (page 1),
    // placed through the same API the app's controllers use.
    const QRectF leftRect(60, 200, 120, 60);
    const QRectF rightRect(200, 380, 120, 60);
    viewer.setAnnotations({makeMark(0, leftRect), makeMark(1, rightRect)});

    viewer.setTwoPageMode(true);

    QLabel *left = pageLabel(viewer, "twoPageLeftLabel");
    QLabel *right = pageLabel(viewer, "twoPageRightLabel");
    QVERIFY(left);
    QVERIFY(right);

    QString whyLeft, whyRight;
    QVERIFY2(magentaPresent(left->pixmap().toImage(), leftRect, &whyLeft),
             qPrintable(QStringLiteral("left page (page 0) must show its annotation: %1")
                            .arg(whyLeft)));
    QVERIFY2(magentaPresent(right->pixmap().toImage(), rightRect, &whyRight),
             qPrintable(QStringLiteral("right page (page 1) must show its annotation: %1")
                            .arg(whyRight)));

    // Cross-check: each page shows only ITS own mark (per-page placement).
    QVERIFY(!magentaPresent(left->pixmap().toImage(), rightRect));
    QVERIFY(!magentaPresent(right->pixmap().toImage(), leftRect));

    // Single-page non-regression: annotations survive and the live layer is back.
    viewer.setTwoPageMode(false);
    QCOMPARE(viewer.annotations().size(), 2);
    viewer.show();
    QVERIFY(viewer.annotationLayer()->isVisible());
}

void TestTwoPageOverlay::twoPageShowsSearchHighlights()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdf(tmp, "search.pdf", 3, QStringLiteral("needle target"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    // Same live search model QPdfView paints in single-page mode.
    viewer.searchDocument(QStringLiteral("needle"), true, false, false);
    QTRY_VERIFY(viewer.searchModel()->resultsOnPage(0).size() >= 1);

    viewer.setTwoPageMode(true);

    QLabel *left = pageLabel(viewer, "twoPageLeftLabel");
    QVERIFY(left);
    const QImage leftImg = left->pixmap().toImage();
    QVERIFY(!leftImg.isNull());

    // "needle target" was drawn at PDF point (50,700) on a 595x842 page. At
    // zoom 1 with 2x render scale the text sits near image y=(842-712)*2=260.
    // A search highlight must appear in that band.
    const QRect textBand(60, 240, 300, 70);
    QVERIFY2(coloredPixelIn(leftImg, textBand),
             "two-page mode must composite search highlights over the page text");

    // Control: an empty region of the page must stay uncolored — guards against
    // the whole pixmap being tinted instead of the actual matches.
    const QRect emptyBand(60, 1300, 300, 70);
    QVERIFY2(!coloredPixelIn(leftImg, emptyBand),
             "no overlay mark may appear where there is neither text nor annotation");
}

void TestTwoPageOverlay::modeTogglingPreservesAnnotations()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdf(tmp, "toggle.pdf", 3, QStringLiteral("needle target"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    const QRectF rect(60, 200, 120, 60);
    viewer.setAnnotations({makeMark(0, rect)});

    viewer.setTwoPageMode(true);
    QCOMPARE(viewer.annotations().size(), 1);
    QString why;
    QVERIFY2(magentaPresent(pageLabel(viewer, "twoPageLeftLabel")->pixmap().toImage(),
                            rect, &why),
             qPrintable(QStringLiteral("mark visible in two-page mode: %1").arg(why)));

    viewer.setTwoPageMode(false);
    QCOMPARE(viewer.annotations().size(), 1);

    viewer.setTwoPageMode(true);
    QCOMPARE(viewer.annotations().size(), 1);
    QVERIFY2(magentaPresent(pageLabel(viewer, "twoPageLeftLabel")->pixmap().toImage(),
                            rect, &why),
             qPrintable(QStringLiteral("mark still visible after re-entering two-page mode: %1")
                            .arg(why)));

    viewer.setTwoPageMode(false);
    QCOMPARE(viewer.annotations().size(), 1);
    viewer.show();
    QVERIFY(viewer.annotationLayer()->isVisible());
}

void TestTwoPageOverlay::unpairedLastPageShowsItsAnnotation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdf(tmp, "odd.pdf", 3, QStringLiteral("needle target"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    viewer.goToPage(2);                    // last page of 3 → spread has no partner
    const QRectF rect(60, 200, 120, 60);
    viewer.setAnnotations({makeMark(2, rect)});
    viewer.setTwoPageMode(true);

    QLabel *left = pageLabel(viewer, "twoPageLeftLabel");
    QLabel *right = pageLabel(viewer, "twoPageRightLabel");
    QVERIFY(left);
    QVERIFY(right);

    QString why;
    QVERIFY2(magentaPresent(left->pixmap().toImage(), rect, &why),
             qPrintable(QStringLiteral("unpaired last page must show its annotation: %1").arg(why)));
    QVERIFY(right->pixmap().isNull());     // no page 3 — right slot stays empty
}

void TestTwoPageOverlay::emptyAnnotationsAndMissingDocumentAreSafe()
{
    // No document loaded at all: entering two-page mode must not crash.
    {
        PdfViewerWidget viewer;
        viewer.setTwoPageMode(true);
        viewer.setTwoPageMode(false);
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdf(tmp, "empty.pdf", 3, QStringLiteral("needle target"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    viewer.setTwoPageMode(true);

    // Invalid/empty annotation list while two-page mode is active: pages must
    // still render, no crash.
    viewer.setAnnotations({});
    QLabel *left = pageLabel(viewer, "twoPageLeftLabel");
    QVERIFY(left);
    QVERIFY(!left->pixmap().isNull());
    QCOMPARE(viewer.annotations().size(), 0);
}

QTEST_MAIN(TestTwoPageOverlay)
#include "TestTwoPageOverlay.moc"
