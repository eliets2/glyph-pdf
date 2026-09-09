// SPDX-License-Identifier: Apache-2.0
// U02 — the slim status bar.
//
// What these tests pin (plan U02 + research Q-U02 Module 5 / Finding U02-5):
//   1. The default bar shows only useful document state: page n of m, zoom,
//      saved/unsaved, plus the transient operation channel. The mode/screen/
//      tool/selection echo cells and the always-on debug values (PDF version,
//      page dimensions, doc info, OCR language) are GONE from the bar.
//   2. The "page 1 / 000" empty state is dead: with no document the total
//      reads "/ —" and the page spin box is disabled.
//   3. Operations go through the QStatusBar showMessage contract:
//      setOperation() is a message with no invented timeout, clearOperation()
//      clears it.
//   4. PDF version / dimensions / selection / OCR language / current task /
//      current tool live in a details affordance fed by a DocumentFacts
//      struct — the popup content is exactly detailsText().
//   5. PDF version parsing upgrades from "PDF --" only when a real %PDF-
//      header exists.
//   6. U02 theme pass: the new surfaces consume the GpTheme tokens through
//      the app's real mechanism (Theme::setMode + the mode's QSS resource —
//      exactly what MainWindow::applyTheme loads). Pinned by sampling flat,
//      text-free pixels of the themed widgets and comparing them to the
//      GpTheme token value for the mode.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolButton>

#include "shell/Ribbon.h"
#include "shell/ScreenNav.h"
#include "shell/StatusBar.h"
#include "util/GpTheme.h"

using namespace gp;

namespace {
// The bar carries exactly: page spin box, page total, zoom, unsaved, details
// button (the popup label lives inside the button's menu).
class TestBar : public gp::StatusBar {
public:
    using gp::StatusBar::StatusBar;
};

QLabel* findLabel(const QWidget* w, const QString& objectName) {
    return w->findChild<QLabel*>(objectName);
}

// The theme QSS resources are compiled into the app executable only
// (resources.qrc is a PdfWorkstation source), so a test binary resolves the
// sheet from the source tree — the same file Theme::sheetForMode() names and
// MainWindow::applyTheme() loads.
//
// G20 (QUALITY-GATE-2026-09-09): the source location is INJECTED by CMake as
// an absolute path (GLYPHPDF_SOURCE_RESOURCE_DIR — the DJOT_LIB_DIR
// injection pattern). The old applicationDirPath()+"/../" heuristic assumed
// the build directory is a direct child of the source root and failed every
// genuine out-of-source build (source and build dirs as siblings). The
// heuristic stays only as a last resort for non-CMake builds.
bool loadThemeSheet(Theme::Mode mode, QString* out) {
    const QString resourcePath = Theme::sheetForMode(mode);   // ":/resources/theme_X.qss"
    if (QFile::exists(resourcePath)) {
        QFile f(resourcePath);
        if (f.open(QIODevice::ReadOnly)) {
            *out = QString::fromUtf8(f.readAll());
            return true;
        }
    }
    const QString fileName = QFileInfo(resourcePath).fileName();  // "theme_X.qss"
#ifdef GLYPHPDF_SOURCE_RESOURCE_DIR
    {
        const QString fromSource = QDir(QStringLiteral(GLYPHPDF_SOURCE_RESOURCE_DIR))
                                       .filePath(fileName);
        QFile f(fromSource);
        if (f.open(QIODevice::ReadOnly)) {
            *out = QString::fromUtf8(f.readAll());
            return true;
        }
    }
#endif
    const QString fromBuildDir = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QStringLiteral("../") + QStringLiteral("resources/")
                                               + fileName);
    QFile f(fromBuildDir);
    if (f.open(QIODevice::ReadOnly)) {
        *out = QString::fromUtf8(f.readAll());
        return true;
    }
    return false;
}
} // namespace

class TestStatusBarSlim : public QObject {
    Q_OBJECT

private slots:
    void emptyStateHasNoPage000();
    void emptyStateDisablesPageJump();
    void permanentSetIsTheSlimSet();
    void noEchoCellsInBar();
    void operationChannelUsesShowMessage();
    void parsePdfVersionRealAndMissing();
    void factsReflectNavAndDocument();
    void detailsTextCarriesTheFacts();
    void detailsPopupShowsDetailsText();
    void u02SurfacesConsumeGpThemeTokens();
};

void TestStatusBarSlim::emptyStateHasNoPage000()
{
    TestBar bar;

    for (QLabel* l : bar.findChildren<QLabel*>())
        QVERIFY2(!l->text().contains(QStringLiteral("/ 000")),
                 qPrintable(QStringLiteral("label still shows / 000: %1").arg(l->text())));

    QLabel* total = findLabel(&bar, QStringLiteral("statusPageTotal"));
    QVERIFY(total != nullptr);
    QCOMPARE(total->text(), QStringLiteral("/ \u2014"));   // "/ —" (em dash)
}

void TestStatusBarSlim::emptyStateDisablesPageJump()
{
    TestBar bar;
    auto* spin = bar.findChild<QSpinBox*>();
    QVERIFY(spin != nullptr);
    QVERIFY2(!spin->isEnabled(), "page spin box must be disabled with no document");

    // A real page report enables it again (the "document-specific controls
    // with no document open" rule).
    bar.setPage(3, 12);
    QVERIFY(spin->isEnabled());
    QCOMPARE(spin->value(), 3);
    QCOMPARE(findLabel(&bar, QStringLiteral("statusPageTotal"))->text(),
             QStringLiteral("/ 12"));

    // Losing the document disables it again.
    bar.updateFromDocument(nullptr, QString());
    QVERIFY(!spin->isEnabled());
    QCOMPARE(findLabel(&bar, QStringLiteral("statusPageTotal"))->text(),
             QStringLiteral("/ \u2014"));
}

void TestStatusBarSlim::permanentSetIsTheSlimSet()
{
    TestBar bar;
    // Exactly: one page spin box, one details button, and the three kept
    // cells (page total, zoom, unsaved). The popup label is a child of the
    // details menu, so it is filtered out by name here.
    QCOMPARE(bar.findChildren<QSpinBox*>().size(), 1);
    QCOMPARE(bar.findChildren<QToolButton*>().size(), 1);
    QCOMPARE(bar.findChild<QToolButton*>()->objectName(), QStringLiteral("statusDetails"));

    QStringList labelNames;
    for (QLabel* l : bar.findChildren<QLabel*>())
        labelNames << l->objectName();
    labelNames.removeAll(QStringLiteral("statusDetailsLabel"));
    labelNames.sort();
    QCOMPARE(labelNames.join(QLatin1Char(',')),
             QStringLiteral("statusPageTotal,statusUnsaved,statusZoom"));
}

void TestStatusBarSlim::noEchoCellsInBar()
{
    TestBar bar;
    // No cell may echo navigation state the ScreenNav/ModeStrip already show,
    // and no permanent debug value may be displayed by default.
    const QStringList bannedPrefixes = {
        QStringLiteral("MODE "), QStringLiteral("SCREEN "), QStringLiteral("TOOL "),
        QStringLiteral("SEL "),  QStringLiteral("PDF "),   QStringLiteral("OCR \u00B7"),
    };
    for (QLabel* l : bar.findChildren<QLabel*>()) {
        for (const QString& prefix : bannedPrefixes)
            QVERIFY2(!l->text().startsWith(prefix),
                     qPrintable(QStringLiteral("bar echoes state: '%1'").arg(l->text())));
    }
}

void TestStatusBarSlim::operationChannelUsesShowMessage()
{
    TestBar bar;
    QVERIFY(bar.currentMessage().isEmpty());

    bar.setOperation(QStringLiteral("Saving page 2\u2026"));
    QCOMPARE(bar.currentMessage(), QStringLiteral("Saving page 2\u2026"));

    bar.clearOperation();
    QVERIFY(bar.currentMessage().isEmpty());
}

void TestStatusBarSlim::parsePdfVersionRealAndMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("fake.pdf"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
    f.close();

    QCOMPARE(StatusBar::parsePdfVersion(path), QStringLiteral("PDF 1.7"));
    QCOMPARE(StatusBar::parsePdfVersion(dir.filePath(QStringLiteral("missing.pdf"))),
             QStringLiteral("PDF --"));

    // A file that does not start with %PDF- never upgrades the placeholder.
    const QString notPdf = dir.filePath(QStringLiteral("not.pdf"));
    QFile n(notPdf);
    QVERIFY(n.open(QIODevice::WriteOnly));
    n.write("PK\x03\x04NOTAPDF");
    n.close();
    QCOMPARE(StatusBar::parsePdfVersion(notPdf), QStringLiteral("PDF --"));
}

void TestStatusBarSlim::factsReflectNavAndDocument()
{
    TestBar bar;
    StatusBar::DocumentFacts facts = bar.currentFacts();
    QCOMPARE(facts.currentTask, QStringLiteral("Standard"));
    QCOMPARE(facts.pdfVersion, QStringLiteral("PDF --"));
    QCOMPARE(facts.pageSize, QStringLiteral("--\u00D7--"));   // "--×--"
    QCOMPARE(facts.selection, QStringLiteral("\u2014"));  // em dash
    QVERIFY(!facts.ocrLanguage.isEmpty());

    bar.setScreen(QStringLiteral("ocr"));
    bar.setTool(QStringLiteral("search"));
    bar.setSelection(QStringLiteral("3 words"));

    facts = bar.currentFacts();
    QCOMPARE(facts.currentTask, QStringLiteral("OCR Verify"));
    QCOMPARE(facts.tool, QStringLiteral("search"));
    QCOMPARE(facts.selection, QStringLiteral("3 words"));

    bar.setSelection(QString());
    QCOMPARE(bar.currentFacts().selection, QStringLiteral("\u2014"));
}

void TestStatusBarSlim::detailsTextCarriesTheFacts()
{
    TestBar bar;
    bar.setScreen(QStringLiteral("ocr"));
    bar.setTool(QStringLiteral("ocr"));

    const QString text = bar.detailsText();
    QVERIFY2(text.contains(QStringLiteral("OCR Verify")),
             "details must carry the current task title");
    QVERIFY2(text.contains(QStringLiteral("PDF --")),
             "details must carry the PDF version placeholder before a document loads");
    QVERIFY2(text.contains(QStringLiteral("--\u00D7--")),
             "details must carry the page-size placeholder before a document loads");
}

void TestStatusBarSlim::detailsPopupShowsDetailsText()
{
    TestBar bar;
    bar.setScreen(QStringLiteral("redact"));
    bar.show();   // visibility assertions need a shown widget

    bar.showDetailsPopup();
    QLabel* popupLabel = bar.findChild<QLabel*>(QStringLiteral("statusDetailsLabel"));
    QVERIFY(popupLabel != nullptr);
    QCOMPARE(popupLabel->text(), bar.detailsText());
    QVERIFY2(popupLabel->text().contains(QStringLiteral("Redaction")),
             "popup must show the current task");

    // The affordance itself is a real, visible control of the bar.
    auto* btn = bar.findChild<QToolButton*>(QStringLiteral("statusDetails"));
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isVisible());

    if (auto* menu = bar.findChild<QMenu*>())
        menu->close();
}

void TestStatusBarSlim::u02SurfacesConsumeGpThemeTokens()
{
    // Declared LAST so the app-wide stylesheet swap cannot leak into the
    // slots above. For each of the three modes: switch GpTheme and load the
    // mode's QSS resource through the production mechanism (the exact two
    // steps of MainWindow::applyTheme), then verify the U02 surfaces actually
    // CONSUME the tokens — a flat, text-free pixel of the themed widget must
    // equal the GpTheme token value for that mode. Sampling points are chosen
    // inside padding zones (never over glyphs) so no font/antialiasing
    // variance can flake the comparison.
    const QVector<Theme::Mode> modes = {
        Theme::Dark, Theme::Light, Theme::HighContrast,
    };

    for (const Theme::Mode mode : modes) {
        Theme::setMode(mode);
        QString sheetText;
        QVERIFY2(loadThemeSheet(mode, &sheetText),
                 qPrintable(QStringLiteral("cannot load theme sheet for mode %1").arg(int(mode))));
        qApp->setStyleSheet(sheetText);

        // ── Ribbon collapsed: the tab row (#ribbonTabRow) carries the
        //    collapsed state, so the transparent active-task label shows the
        //    tab-row token (bg2) — NOT the base QWidget background (bg1).
        //    Without the theme-pass rule this is bg1 in all three modes and
        //    the comparison fails.
        {
            Ribbon ribbon;
            ribbon.resize(900, 200);
            ribbon.show();
            QVERIFY(QTest::qWaitForWindowExposed(&ribbon));
            QApplication::processEvents();
            ribbon.setCollapsed(true);
            QApplication::processEvents();

            auto* tabRow = ribbon.findChild<QWidget*>(QStringLiteral("ribbonTabRow"));
            auto* label  = ribbon.findChild<QLabel*>(QStringLiteral("ribbonActiveTaskLabel"));
            QVERIFY(tabRow != nullptr);
            QVERIFY(label != nullptr);
            QVERIFY(label->isVisible());
            QVERIFY(label->width() > 4);

            const QPoint p = tabRow->geometry().topLeft() + label->geometry().topLeft()
                             + QPoint(3, label->height() / 2);   // left padding zone — no glyphs
            const QColor px = ribbon.grab().toImage().pixelColor(p);
            QCOMPARE(px, Theme::bg2());

            ribbon.setCollapsed(false);
        }

        // ── ScreenNav: the TaskStateSync active state (checked item) must
        //    consume the theme token (bg2), not the unstyled base.
        {
            ScreenNav nav;
            nav.resize(900, 26);
            nav.show();
            QVERIFY(QTest::qWaitForWindowExposed(&nav));
            QApplication::processEvents();

            nav.setActive(QStringLiteral("ocr"));
            auto* btn = nav.findChild<QToolButton*>(QStringLiteral("screenNav_ocr"));
            QVERIFY(btn != nullptr);
            QVERIFY(btn->isChecked());

            const QPoint p = btn->geometry().topLeft() + QPoint(3, 3);   // padding zone
            const QColor px = nav.grab().toImage().pixelColor(p);
            QCOMPARE(px, Theme::bg2());
        }

        // ── Details popup: the DocumentFacts menu surface must consume bg1.
        {
            TestBar bar;
            bar.show();
            QVERIFY(QTest::qWaitForWindowExposed(&bar));

            bar.showDetailsPopup();
            auto* menu = bar.findChild<QMenu*>();
            QVERIFY(menu != nullptr);
            QApplication::processEvents();

            const QImage img = menu->grab().toImage();
            QVERIFY(img.width() > 12 && img.height() > 12);
            const QPoint p(img.width() / 2, img.height() - 5);   // bottom padding band
            QCOMPARE(img.pixelColor(p), Theme::bg1());

            menu->close();
        }
    }

    qApp->setStyleSheet(QString());
    Theme::setMode(Theme::Dark);
}

QTEST_MAIN(TestStatusBarSlim)
#include "TestStatusBarSlim.moc"
