// SPDX-License-Identifier: Apache-2.0
// §9.16 P1 — unified open/drag-drop routing (audit: "Office/image import hidden
// behind separate menus instead of unified File>Open with drag-and-drop").
//
// Pins the pure routing seam installed on MainWindow (GpMainWindow.h):
//   - routeForFile(path)  : extension → OpenRoute classification matrix
//   - planDrop(paths)     : drop policy (first PDF wins → else ALL images
//                           combine into one PDF → else first Office file)
// File>Open, the Welcome Open card and drag-and-drop all funnel through
// openDocument()/the drop handlers, which delegate to these statics and route
// non-PDF targets to the SAME conversions the explicit Welcome cards run.
//
// The QWidget-level event delivery (dragEnterEvent/dropEvent themselves) is
// source-verified, not synthesized here: both are thin wrappers over
// planDrop()/routeForFile(), and no test in this suite constructs a full
// MainWindow (it boots every controller, ribbon, OCR panel and the update
// checker) — synthesizing QDropEvent against it would be an integration test
// of the whole shell, not of the routing policy pinned below.
#include <QtTest/QtTest>
#include <QStringList>

#include "GpMainWindow.h"

using gp::MainWindow;
using Route  = MainWindow::OpenRoute;
using Plan   = MainWindow::DropPlan;

class TestOpenRouting : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Isolate QSettings: never read the user's real prefs, never clobber
        // them (same idiom as TestWelcomeLayout / TestOcrPreprocessPrefs).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestOpenRouting"));
    }

    // THE pinned matrix from the mission: .pdf → direct, .docx → office,
    // .png → images, .txt → office (the ImportOffice filter includes *.txt),
    // .xyz → unsupported.
    void routingMatrix()
    {
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("doc.pdf")),     Route::PdfDirect);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("report.docx")), Route::OfficeConvert);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("photo.png")),   Route::ImagesConvert);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("notes.txt")),   Route::OfficeConvert);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("archive.xyz")), Route::Unsupported);
    }

    // Classification is by extension, case-insensitively (Windows filenames).
    void routingIsCaseInsensitive()
    {
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("REPORT.PDF")),  Route::PdfDirect);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("Report.DOCX")), Route::OfficeConvert);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("photo.PNG")),   Route::ImagesConvert);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("scan.TiFF")),   Route::ImagesConvert);
    }

    // The full accepted sets mirror the existing Welcome-card dialogs exactly:
    // HomeController's ImportOffice filter (which includes .txt) and
    // onImagesToPdf's image filter (which includes .bmp). Nothing the cards
    // accept may be refused by the unified flow, and vice versa.
    void routingFullExtensionSets()
    {
        const QStringList officeExts {
            "docx", "doc", "xlsx", "xls", "pptx", "ppt",
            "odt", "ods", "odp", "rtf", "csv", "txt" };
        for (const QString& e : officeExts)
            QCOMPARE(MainWindow::routeForFile(QStringLiteral("f.") + e),
                     Route::OfficeConvert);

        const QStringList imageExts { "png", "jpg", "jpeg", "tif", "tiff", "bmp" };
        for (const QString& e : imageExts)
            QCOMPARE(MainWindow::routeForFile(QStringLiteral("f.") + e),
                     Route::ImagesConvert);

        // Not routable: unknown extension, no extension, hidden dotfile-ish
        // base names. Unsupported falls through to the pre-existing PDF load
        // error path (unchanged behavior for odd files).
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("f.djot")),      Route::Unsupported);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("README")),      Route::Unsupported);
        QCOMPARE(MainWindow::routeForFile(QStringLiteral("noext.")),      Route::Unsupported);
        QCOMPARE(MainWindow::routeForFile(QString()),                     Route::Unsupported);
    }

    // Drop policy 1: any PDF in the drop wins and opens directly — the
    // pre-existing behavior for PDF-only drops is preserved verbatim.
    void dropPlanPdfWins()
    {
        const Plan p = MainWindow::planDrop(
            { QStringLiteral("/t/b.png"), QStringLiteral("/t/a.pdf"),
              QStringLiteral("/t/c.docx") });
        QCOMPARE(p.pdfToOpen, QStringLiteral("/t/a.pdf"));
        QVERIFY(p.imagesToConvert.isEmpty());
        QVERIFY(p.officeToConvert.isEmpty());

        const Plan first = MainWindow::planDrop(
            { QStringLiteral("/t/second.pdf"), QStringLiteral("/t/first.pdf") });
        QCOMPARE(first.pdfToOpen, QStringLiteral("/t/second.pdf"));
    }

    // Drop policy 2: with no PDF present, ALL images in the drop combine into
    // one PDF (mirrors multi-select in the Images-to-PDF card), beating any
    // Office file in the same drop.
    void dropPlanImagesCombine()
    {
        const Plan p = MainWindow::planDrop(
            { QStringLiteral("/t/a.png"), QStringLiteral("/t/b.JPG"),
              QStringLiteral("/t/c.docx") });
        QVERIFY(p.pdfToOpen.isEmpty());
        QCOMPARE(p.imagesToConvert,
                 (QStringList { QStringLiteral("/t/a.png"),
                                QStringLiteral("/t/b.JPG") }));
        QVERIFY(p.officeToConvert.isEmpty());

        const Plan single = MainWindow::planDrop(
            { QStringLiteral("/t/only.jpeg") });
        QCOMPARE(single.imagesToConvert,
                 QStringList { QStringLiteral("/t/only.jpeg") });
    }

    // Drop policy 3: first Office file converts; an all-unsupported drop
    // plans nothing (the event is then not accepted — pre-existing behavior
    // for non-openable drops).
    void dropPlanMixedAndUnsupported()
    {
        const Plan office = MainWindow::planDrop(
            { QStringLiteral("/t/x.docx"), QStringLiteral("/t/y.doc") });
        QVERIFY(office.pdfToOpen.isEmpty());
        QVERIFY(office.imagesToConvert.isEmpty());
        QCOMPARE(office.officeToConvert, QStringLiteral("/t/x.docx"));

        const Plan nothing = MainWindow::planDrop(
            { QStringLiteral("/t/a.xyz"), QStringLiteral("/t/b.exe") });
        QVERIFY(nothing.pdfToOpen.isEmpty());
        QVERIFY(nothing.imagesToConvert.isEmpty());
        QVERIFY(nothing.officeToConvert.isEmpty());
        QVERIFY(nothing.isEmpty());
    }
};

QTEST_MAIN(TestOpenRouting)
#include "TestOpenRouting.moc"
