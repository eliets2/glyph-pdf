// SPDX-License-Identifier: Apache-2.0
// R12 (PERF-04) guard tests: finite dimension/scale checks, the pixel-
// allocation ceiling, zoom-entry clamps, and stale-output suppression on the
// REAL viewer render path (PdfViewerWidget::renderPage — the funnel every
// rendering boundary shares: two-page spread, thumbnails, snapshots).
//
// The pre-guard behaviour (measured by tools/render_path_profile on the
// unrepaired tree): a 40000x40000 pt page at scale 2 rendered 80000x80000 px
// (6,400 Mpx) in ~12.7 s with a ~11.7 GiB peak working set; alternating deep
// zoom renders cost ~10 s per step. These tests pin the bounded behaviour.
#include <QtTest/QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QMarginsF>
#include <QPageSize>
#include <limits>
#include "ui/PdfViewerWidget.h"

class TestRenderGuards : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    static QString makePdf(const QString &dirPath, const QString &name, int pages,
                           QSizeF pageSizePt = {612.0, 792.0}, const QString &text = {}) {
        const QString path = QDir(dirPath).filePath(name);
        QPdfWriter w(path);
        w.setPageSize(QPageSize(pageSizePt, QPageSize::Point));
        w.setPageMargins(QMarginsF(0, 0, 0, 0));
        w.setResolution(72);
        QPainter p(&w);
        const QString label = text.isEmpty()
                                  ? QStringLiteral("render-guard page of %1")
                                  : text;
        for (int i = 0; i < pages; ++i) {
            if (i > 0) w.newPage();
            p.drawText(72, 200, label.arg(i + 1));
        }
        p.end();
        return path;
    }

    static qint64 pixels(const QImage &img) {
        return qint64(img.width()) * qint64(img.height());
    }

private slots:

    void normalRenderIsUnchanged() {
        // Control: ordinary Letter page at the snapshot scale still renders
        // exactly as before the guards (non-null, full resolution).
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("normal.pdf"), 2);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        const QImage img = viewer.renderPage(0, 2.0);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 1224);   // 612 pt * 2
        QCOMPARE(img.height(), 1584);  // 792 pt * 2
    }

    void largeMediaBoxIsBounded() {
        // 40000x40000 pt page at scale 2 would be 80000x80000 px (6,400 Mpx).
        // The pixel ceiling must bound the render (proportional shrink), not
        // allocate a multi-GiB buffer.
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("huge.pdf"), 1,
                                    {40000.0, 40000.0});
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        QElapsedTimer t;
        t.start();
        const QImage img = viewer.renderPage(0, 2.0);
        const qint64 ms = t.nsecsElapsed() / 1000000;

        QVERIFY2(!img.isNull(), "a bounded render must still show the page");
        QVERIFY2(pixels(img) <= 65 * 1000 * 1000,
                 qPrintable(QStringLiteral("render is %1x%2 (%3 Mpx) — over the 64 Mpx ceiling")
                                .arg(img.width()).arg(img.height())
                                .arg(double(pixels(img)) / 1e6, 0, 'f', 1)));
        QVERIFY2(img.width() <= 32767 && img.height() <= 32767,
                 "per-side dimension must stay inside QImage's hard limit");
        QVERIFY2(ms < 10000,
                 qPrintable(QStringLiteral("bounded render took %1 ms (pre-guard: ~12700 ms)")
                                .arg(ms)));
    }

    void extremeZoomIsBounded() {
        // A programmatic scale of 1e9 must neither overflow int nor allocate:
        // it gets a bounded render.
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("zoom.pdf"), 1);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        QElapsedTimer t;
        t.start();
        const QImage img = viewer.renderPage(0, 1e9);
        const qint64 ms = t.nsecsElapsed() / 1000000;
        QVERIFY(!img.isNull());
        QVERIFY2(pixels(img) <= 65 * 1000 * 1000,
                 qPrintable(QStringLiteral("1e9-scale render is %1x%2")
                                .arg(img.width()).arg(img.height())));
        QVERIFY2(ms < 10000, qPrintable(QStringLiteral("took %1 ms").arg(ms)));
    }

    void nanAndInfScalesAreRefused() {
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("nan.pdf"), 1);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        // Useful refusal: null image, not an undefined QSize(int(NaN)) path.
        QImage img = viewer.renderPage(0, std::numeric_limits<double>::quiet_NaN());
        QVERIFY(img.isNull());
        img = viewer.renderPage(0, std::numeric_limits<double>::infinity());
        QVERIFY(img.isNull());
        img = viewer.renderPage(0, -3.0);
        QVERIFY(img.isNull());
    }

    void tinyScalesAreRefused() {
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("tiny.pdf"), 1);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));
        // Below the operating floor a render is meaningless; refusal, not a
        // 0x0 / 1x1 artifact.
        const QImage img = viewer.renderPage(0, 0.0001);
        QVERIFY(img.isNull());
    }

    void zoomEntryPointsAreClampedAndFinite() {
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("clamp.pdf"), 1);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        // zoomIn cannot grow without bound (pre-fix: 1.25^n, unbounded).
        for (int i = 0; i < 100; ++i)
            viewer.zoomIn();
        const qreal up = viewer.zoomLevel();
        QVERIFY2(qIsFinite(up) && up <= 16.0 + 1e-9,
                 qPrintable(QStringLiteral("zoom after 100x zoomIn = %1").arg(up)));

        // zoomOut keeps the existing floor.
        for (int i = 0; i < 100; ++i)
            viewer.zoomOut();
        const qreal down = viewer.zoomLevel();
        QVERIFY2(qIsFinite(down) && down >= 0.1 - 1e-9 && down <= 16.0,
                 qPrintable(QStringLiteral("zoom after 100x zoomOut = %1").arg(down)));

        // Programmatic extremes: huge levels clamp, non-finite levels are
        // refused (current zoom kept).
        viewer.setZoomLevel(1e9);
        QCOMPARE(viewer.zoomLevel(), 16.0);
        viewer.setZoomLevel(std::numeric_limits<double>::quiet_NaN());
        QVERIFY(qIsFinite(viewer.zoomLevel()));
        QCOMPARE(viewer.zoomLevel(), 16.0);
        viewer.setZoomLevel(1.5);
        QCOMPARE(viewer.zoomLevel(), 1.5);
    }

    void rapidZoomReversalStaysBounded() {
        // Alternating extreme scales on the two-page render path: pre-guard
        // each deep step cost ~10 s and drove peak working set to ~11.7 GiB;
        // bounded, every step renders inside the pixel ceiling quickly.
        const QString pdf = makePdf(m_dir.path(), QStringLiteral("rapid.pdf"), 2);
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 20; ++i) {
            viewer.clearPageCache();
            const qreal scale = (i % 2 == 0) ? 0.2 : 32.0;
            QElapsedTimer step;
            step.start();
            const QImage img = viewer.renderPage(0, scale);
            QVERIFY2(step.nsecsElapsed() / 1000000 < 2000,
                     qPrintable(QStringLiteral("step %1 (scale %2) took %3 ms")
                                    .arg(i).arg(scale)
                                    .arg(step.nsecsElapsed() / 1000000)));
            QVERIFY(!img.isNull());
        }
        QVERIFY2(t.nsecsElapsed() / 1000000 < 40000,
                 "20 bounded reversal steps must finish well inside the ctest timeout");
    }

    void documentSwitchDoesNotServeStalePages() {
        // Stale-output suppression contract: switching documents clears the
        // page cache, so page 0 of document B can never be the cached render
        // of document A (R12 pins the behaviour setDocument relies on).
        const QString docA = makePdf(m_dir.path(), QStringLiteral("stale_a.pdf"), 1,
                                     {612.0, 792.0}, QStringLiteral("Document A page %1"));
        const QString docB = makePdf(m_dir.path(), QStringLiteral("stale_b.pdf"), 1,
                                     {612.0, 792.0}, QStringLiteral("Document B page %1"));
        QVERIFY(!docA.isEmpty() && !docB.isEmpty());

        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(docA));
        const QImage fromA = viewer.renderPage(0, 2.0);
        QVERIFY(!fromA.isNull());

        QVERIFY(viewer.loadDocument(docB));
        const QImage fromB = viewer.renderPage(0, 2.0);
        QVERIFY(!fromB.isNull());
        QVERIFY2(fromA != fromB,
                 "page 0 after the switch must be re-rendered from document B, "
                 "not served from document A's cache");
    }
};

QTEST_MAIN(TestRenderGuards)
#include "TestRenderGuards.moc"
