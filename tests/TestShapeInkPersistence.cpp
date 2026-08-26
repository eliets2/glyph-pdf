// SPDX-License-Identifier: Apache-2.0
// Audit 9.3 P0 regression test: shapes and freehand ink must persist as their
// REAL PDF annotation subtypes (/Square, /Circle, /Line, /Ink) and survive a
// save/reload round-trip — they used to degrade to invisible Text notes.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "engines/podofo/PoDoFoBackend.h"

class TestShapeInkPersistence : public QObject {
    Q_OBJECT
private slots:
    void shapesRoundTripAsRealSubtypes();
    void inkRoundTripsWithPoints();
};
void TestShapeInkPersistence::shapesRoundTripAsRealSubtypes() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = tmp.filePath("seed.pdf");
    {
        QFile f(seed);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(
            "%PDF-1.4\n"
            "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
            "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
            "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
            "xref\n0 4\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "trailer<</Size 4/Root 1 0 R>>\n"
            "startxref\n183\n%%EOF\n");
    }
    PoDoFoBackend backend;

    AnnotationItem rect;
    rect.mode = ToolMode::DrawRectangle;
    rect.pageIndex = 0;
    rect.rect = QRectF(50, 50, 120, 80);
    rect.color = Qt::red;

    AnnotationItem ell;
    ell.mode = ToolMode::DrawEllipse;
    ell.pageIndex = 0;
    ell.rect = QRectF(200, 60, 90, 90);
    ell.color = Qt::blue;

    AnnotationItem line;
    line.mode = ToolMode::DrawLine;
    line.pageIndex = 0;
    line.rect = QRectF(10, 10, 100, 40);
    line.points << QPointF(10, 10) << QPointF(110, 50);
    line.color = Qt::black;

    const QString out = tmp.filePath("shapes.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, {rect, ell, line}));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 3);
    bool sawRect = false, sawEllipse = false, sawLine = false;
    for (const auto& a : back) {
        if (a.mode == ToolMode::DrawRectangle) sawRect = true;
        if (a.mode == ToolMode::DrawEllipse)   sawEllipse = true;
        if (a.mode == ToolMode::DrawLine)      sawLine = true;
        QVERIFY(a.mode != ToolMode::AddComment); // must not degrade to notes
    }
    QVERIFY(sawRect);
    QVERIFY(sawEllipse);
    QVERIFY(sawLine);
}
void TestShapeInkPersistence::inkRoundTripsWithPoints() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString seed = tmp.filePath("seed2.pdf");
    {
        QFile f(seed);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(
            "%PDF-1.4\n"
            "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
            "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
            "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
            "xref\n0 4\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "trailer<</Size 4/Root 1 0 R>>\n"
            "startxref\n183\n%%EOF\n");
    }
    PoDoFoBackend backend;

    AnnotationItem ink;
    ink.mode = ToolMode::DrawFreehand;
    ink.pageIndex = 0;
    ink.rect = QRectF(20, 20, 200, 100);
    ink.points << QPointF(20, 30) << QPointF(60, 80) << QPointF(120, 40) << QPointF(220, 90);
    ink.color = Qt::green;

    const QString out = tmp.filePath("ink.pdf");
    QVERIFY(backend.embedAnnotations(seed, out, {ink}));

    const QList<AnnotationItem> back = backend.extractAnnotations(out);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().mode, ToolMode::DrawFreehand);
    QCOMPARE(back.first().points.size(), 4);
    // Y-flip must be symmetric: first point returns in top-left space.
    QVERIFY(qAbs(back.first().points.first().x() - 20.0) < 0.01);
}
QTEST_MAIN(TestShapeInkPersistence)
#include "TestShapeInkPersistence.moc"
