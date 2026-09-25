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
#include <QApplication>
#include <QGraphicsColorizeEffect>
#include <QLabel>
#include <QPageSize>
#include <QPainter>
#include <QPdfView>
#include <QPdfWriter>
#include <QScrollArea>
#include <QTemporaryDir>
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
    void loadedPageIsVisibleUnderTheShippedStylesheet();
    void loadingADocumentAnnouncesItsFirstPage();

private:
    static QString writeOnePagePdf(const QTemporaryDir &dir, const QString &name);
    static QList<QWidget *> surfaces(PdfViewerWidget &viewer);
    static int paperPixels(const QImage &image);
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

int TestViewingModes::paperPixels(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); y += 2)
        for (int x = 0; x < image.width(); x += 2) {
            const QRgb c = image.pixel(x, y);
            if (qRed(c) > 200 && qGreen(c) > 200 && qBlue(c) > 200) ++count;
        }
    return count;
}

// P0 (2026-09-25 live UI check): under the shipped themes' base rule
// `QWidget { background-color: … }` a loaded document showed an EMPTY canvas.
// The full-size signature-badge overlay has no Q_OBJECT, so style sheets treat
// it as a plain QWidget and painted the theme background over every page.
// Grabbing the inner QPdfView alone bypasses the overlays stacked above it —
// why nightModeChangesTheRenderedViewer stayed green — so this pin grabs the
// COMPOSITED viewer.
void TestViewingModes::loadedPageIsVisibleUnderTheShippedStylesheet()
{
    struct SheetGuard {
        QString previous = qApp->styleSheet();
        ~SheetGuard() { qApp->setStyleSheet(previous); }
    } guard;
    qApp->setStyleSheet(QStringLiteral("QWidget { background-color: #1e1f22; }"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pdf = dir.filePath(QStringLiteral("page.pdf"));
    {
        QPdfWriter writer(pdf);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(QPointF(600, 600), QStringLiteral("GlyphPDF"));
    }

    PdfViewerWidget viewer;
    viewer.resize(640, 480);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    QVERIFY(viewer.loadDocument(pdf));

    // Pages render asynchronously: poll the whole viewer until paper shows.
    QTRY_VERIFY_WITH_TIMEOUT(paperPixels(viewer.grab().toImage()) > 5000, 5000);
}

QString TestViewingModes::writeOnePagePdf(const QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    QPainter painter(&writer);
    painter.drawText(QPointF(600, 600), name);
    return path;
}

// The thumbnail rail, comments list, files list, status bar and Measure panel
// all refresh on pageChanged. QPdfPageNavigator only signals a CHANGE, and
// page 0 → page 0 across two documents is none — so after an open the rail
// stayed at "PAGES · 0" and the comments/files panes kept the previous
// document. A successful load must announce page 1 of the new document.
void TestViewingModes::loadingADocumentAnnouncesItsFirstPage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PdfViewerWidget viewer;

    QSignalSpy spy(&viewer, &PdfViewerWidget::pageChanged);
    QVERIFY(viewer.loadDocument(writeOnePagePdf(dir, QStringLiteral("a.pdf"))));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    QCOMPARE(spy.takeFirst(), (QList<QVariant>{ 1, 1 }));

    // A second document on the same page index is still a page change.
    QVERIFY(viewer.loadDocument(writeOnePagePdf(dir, QStringLiteral("b.pdf"))));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    QCOMPARE(viewer.filePath(), dir.filePath(QStringLiteral("b.pdf")));
}

QTEST_MAIN(TestViewingModes)
#include "TestViewingModes.moc"
