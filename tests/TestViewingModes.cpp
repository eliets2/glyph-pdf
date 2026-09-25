// SPDX-License-Identifier: Apache-2.0
// Reading filters on the page surfaces: Night Mode (content-level inversion,
// ported from the Wave 2B viewing-parity line) and Eye Care (sepia tint).
//
// Contract under test:
//   1. gp::NightModeEffect inverts the RGB of what a widget paints (red →
//      cyan, white → black) and keeps it opaque.
//   2. Night Mode installs that effect on BOTH page surfaces (single-page
//      QPdfView and the two-page spread) and removes it again.
//   3. Eye Care can be toggled on/off repeatedly. Qt deletes an installed
//      effect when it is replaced, so the old code (one cached colorize
//      effect) re-installed a deleted object on the second toggle-on — a
//      use-after-free that this cycle exercises.
//   4. The two filters are mutually exclusive: turning one on turns the other
//      off, and the surfaces always carry the effect of the active filter.
//   5. The installed effect really changes the viewer's rendered output.
#include <QtTest/QtTest>
#include <QGraphicsColorizeEffect>
#include <QLabel>
#include <QPdfView>
#include <QScrollArea>
#include "ui/NightModeEffect.h"
#include "ui/PdfViewerWidget.h"

class TestViewingModes : public QObject {
    Q_OBJECT
private slots:
    void nightModeEffectInvertsPixels();
    void nightModeCoversBothPageSurfaces();
    void eyeCareSurvivesRepeatedToggling();
    void readingFiltersAreMutuallyExclusive();
    void nightModeChangesTheRenderedViewer();

private:
    static QList<QWidget *> surfaces(PdfViewerWidget &viewer);
    static bool isSepia(QGraphicsEffect *effect);
    static bool isNight(QGraphicsEffect *effect);
};

QList<QWidget *> TestViewingModes::surfaces(PdfViewerWidget &viewer)
{
    auto *single = viewer.findChild<QPdfView *>(QStringLiteral("pdfView"));
    auto *spread = viewer.findChild<QScrollArea *>(QStringLiteral("twoPageScrollArea"));
    return { single, spread };
}

bool TestViewingModes::isSepia(QGraphicsEffect *effect)
{
    auto *c = qobject_cast<QGraphicsColorizeEffect *>(effect);
    return c && c->color() == QColor(245, 222, 179) && qFuzzyCompare(c->strength(), 0.5);
}

bool TestViewingModes::isNight(QGraphicsEffect *effect)
{
    return dynamic_cast<gp::NightModeEffect *>(effect) != nullptr;
}

void TestViewingModes::nightModeEffectInvertsPixels()
{
    QPixmap page(40, 20);
    page.fill(Qt::white);
    {
        QPainter p(&page);
        p.fillRect(QRect(0, 0, 20, 20), QColor(255, 0, 0));
    }
    QLabel label;
    label.setContentsMargins(0, 0, 0, 0);
    label.setPixmap(page);
    label.resize(page.size());
    label.setGraphicsEffect(new gp::NightModeEffect(&label));

    const QImage shot = label.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY(!shot.isNull());
    const QColor left = shot.pixelColor(5, 10);
    const QColor right = shot.pixelColor(35, 10);
    QCOMPARE(left, QColor(0, 255, 255));    // red → cyan
    QCOMPARE(right, QColor(0, 0, 0));       // white → black
    QCOMPARE(left.alpha(), 255);            // inversion keeps the page opaque
}

void TestViewingModes::nightModeCoversBothPageSurfaces()
{
    PdfViewerWidget viewer;
    const auto targets = surfaces(viewer);
    QCOMPARE(targets.size(), 2);
    for (QWidget *w : targets) QVERIFY(w);

    QVERIFY(!viewer.isNightMode());
    viewer.toggleNightMode();
    QVERIFY(viewer.isNightMode());
    for (QWidget *w : targets) QVERIFY2(isNight(w->graphicsEffect()), qPrintable(w->objectName()));

    viewer.toggleNightMode();
    QVERIFY(!viewer.isNightMode());
    for (QWidget *w : targets) QCOMPARE(w->graphicsEffect(), nullptr);
}

void TestViewingModes::eyeCareSurvivesRepeatedToggling()
{
    PdfViewerWidget viewer;
    const auto targets = surfaces(viewer);
    for (int cycle = 0; cycle < 4; ++cycle) {
        viewer.toggleEyeCareMode();
        QVERIFY(viewer.isEyeCareMode());
        for (QWidget *w : targets)
            QVERIFY2(isSepia(w->graphicsEffect()),
                     qPrintable(QStringLiteral("cycle %1, %2").arg(cycle).arg(w->objectName())));
        viewer.toggleEyeCareMode();
        QVERIFY(!viewer.isEyeCareMode());
        for (QWidget *w : targets) QCOMPARE(w->graphicsEffect(), nullptr);
    }
}

void TestViewingModes::readingFiltersAreMutuallyExclusive()
{
    PdfViewerWidget viewer;
    const auto targets = surfaces(viewer);

    viewer.toggleEyeCareMode();
    viewer.toggleNightMode();            // Night replaces Eye Care
    QVERIFY(viewer.isNightMode());
    QVERIFY(!viewer.isEyeCareMode());
    for (QWidget *w : targets) QVERIFY(isNight(w->graphicsEffect()));

    viewer.toggleEyeCareMode();          // Eye Care replaces Night
    QVERIFY(viewer.isEyeCareMode());
    QVERIFY(!viewer.isNightMode());
    for (QWidget *w : targets) QVERIFY(isSepia(w->graphicsEffect()));

    viewer.toggleEyeCareMode();          // and off leaves no filter at all
    QVERIFY(!viewer.isEyeCareMode());
    QVERIFY(!viewer.isNightMode());
    for (QWidget *w : targets) QCOMPARE(w->graphicsEffect(), nullptr);
}

void TestViewingModes::nightModeChangesTheRenderedViewer()
{
    PdfViewerWidget viewer;
    viewer.resize(480, 360);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    auto *single = viewer.findChild<QPdfView *>(QStringLiteral("pdfView"));
    QVERIFY(single);
    QVERIFY(single->width() > 10 && single->height() > 10);

    const QPoint probe(single->width() / 2, single->height() / 2);
    const QColor plain = single->grab().toImage().pixelColor(probe);
    viewer.toggleNightMode();
    const QColor night = single->grab().toImage().pixelColor(probe);
    QCOMPARE(night, QColor(255 - plain.red(), 255 - plain.green(), 255 - plain.blue()));
}

QTEST_MAIN(TestViewingModes)
#include "TestViewingModes.moc"
