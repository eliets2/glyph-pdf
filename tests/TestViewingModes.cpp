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
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsColorizeEffect>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPainter>
#include <QPdfView>
#include <QPdfWriter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTemporaryDir>
#include "ui/NightModeEffect.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"

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
    void renderedPagesArePaperWhite();
    void disabledControlsReadAsDisabledUnderEveryShippedSheet();

private:
    static QString writeOnePagePdf(const QTemporaryDir &dir, const QString &name);
    static QList<QWidget *> surfaces(PdfViewerWidget &viewer);
    static int paperPixels(const QImage &image);
    static bool isSepia(QGraphicsEffect *effect);
    static bool isNight(QGraphicsEffect *effect);
    static bool loadThemeSheet(gp::Theme::Mode mode, QString *out);
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

// renderPage feeds the thumbnail rail, the two-page spread, interactive OCR
// and clipboard snapshots. QPdfDocument::render leaves a page's unpainted
// areas TRANSPARENT (QPdfView paints white paper beneath its own pages), so
// every one of those consumers showed the dark theme through the page.
void TestViewingModes::renderedPagesArePaperWhite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(writeOnePagePdf(dir, QStringLiteral("paper.pdf"))));

    const QImage page = viewer.renderPage(0, 1.0);
    QVERIFY(!page.isNull());
    QCOMPARE(page.pixelColor(2, 2), QColor(Qt::white));   // blank margin = paper
    QCOMPARE(page.pixelColor(page.width() - 3, page.height() - 3), QColor(Qt::white));
}

// The theme QSS resources are compiled into the app executable only
// (resources.qrc is a PdfWorkstation source — G20), so resolve the sheet from
// the injected source dir when the resource path is absent.
bool TestViewingModes::loadThemeSheet(gp::Theme::Mode mode, QString *out)
{
    const QString resourcePath = gp::Theme::sheetForMode(mode);
    if (QFile::exists(resourcePath)) {
        QFile f(resourcePath);
        if (f.open(QIODevice::ReadOnly)) {
            *out = QString::fromUtf8(f.readAll());
            return true;
        }
    }
    const QString fileName = QFileInfo(resourcePath).fileName();
#ifdef GLYPHPDF_SOURCE_RESOURCE_DIR
    {
        QFile f(QDir(QStringLiteral(GLYPHPDF_SOURCE_RESOURCE_DIR)).filePath(fileName));
        if (f.open(QIODevice::ReadOnly)) {
            *out = QString::fromUtf8(f.readAll());
            return true;
        }
    }
#else
    Q_UNUSED(fileName);
#endif
    return false;
}

// feat/ui-polish regression: before the :disabled sections existed, NO shipped
// sheet styled disabled controls, so a disabled input/button/checkbox
// inherited the enabled foreground (probe: #dfe1e5 dark / #1a1b1e light /
// #ffffff high-contrast) and contextually unavailable actions invited clicks
// that do nothing. Every shipped sheet must now dim all three recurring
// control families, per-theme, while leaving enabled colours untouched.
void TestViewingModes::disabledControlsReadAsDisabledUnderEveryShippedSheet()
{
    struct SheetGuard {
        QString previous = qApp->styleSheet();
        ~SheetGuard() { qApp->setStyleSheet(previous); }
    } guard;

    struct Expectation {
        gp::Theme::Mode mode;
        const char *disabledText;
    };
    const Expectation expectations[] = {
        { gp::Theme::Dark,         "#52555a" },
        { gp::Theme::Light,        "#9c9a90" },
        { gp::Theme::HighContrast, "#999999" },
    };

    for (const Expectation &e : expectations) {
        QString sheet;
        QVERIFY2(loadThemeSheet(e.mode, &sheet),
                 qPrintable(QStringLiteral("cannot load sheet for mode %1").arg(int(e.mode))));
        // High-contrast parity with the other sheets: the thumbnail paper
        // preview (semantic paper colours) and the mono input font must be
        // styled here too — they were missing and the preview vanished
        // against the black sidebar.
        if (e.mode == gp::Theme::HighContrast) {
            QVERIFY2(sheet.contains(QStringLiteral("QWidget#thumbPaper")),
                     "high-contrast sheet must style the thumbnail paper preview");
            QVERIFY2(sheet.contains(QStringLiteral("QLineEdit[mono=\"true\"]")),
                     "high-contrast sheet must style mono inputs");
        }
        QVERIFY2(sheet.contains(QStringLiteral(":disabled")),
                 "every shipped sheet must carry disabled-state rules");

        qApp->setStyleSheet(sheet);

        QLineEdit edit;
        QPushButton button;
        QCheckBox check;
        QSpinBox spin;
        QWidget *controls[] = { &edit, &button, &check, &spin };
        for (QWidget *w : controls) {
            w->ensurePolished();
            const QColor enabledText =
                w->palette().color(QPalette::WindowText);
            w->setEnabled(false);
            w->ensurePolished();
            const QColor disabledText =
                w->palette().color(QPalette::Disabled, QPalette::WindowText);
            QVERIFY2(disabledText != enabledText,
                     qPrintable(QStringLiteral("%1: disabled %2 still paints in the "
                                              "enabled foreground")
                                    .arg(gp::Theme::sheetForMode(e.mode), w->metaObject()->className())));
            QVERIFY2(disabledText.name() == QLatin1String(e.disabledText),
                     qPrintable(QStringLiteral("%1: disabled %2 text is %3, expected %4")
                                    .arg(gp::Theme::sheetForMode(e.mode),
                                         w->metaObject()->className(),
                                         disabledText.name(),
                                         QLatin1String(e.disabledText))));
        }
    }
}

QTEST_MAIN(TestViewingModes)
#include "TestViewingModes.moc"
