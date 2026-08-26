// SPDX-License-Identifier: Apache-2.0
// Audit 9.1 P0 regression test: viewer rotation must affect the REAL rendered
// bitmap — renderPage() snapshots honor the rotation (swapped dimensions for
// 90/270), and rotateClockwise requests engine-side page rotation instead of
// silently rotating only the annotation overlay.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QPdfWriter>
#include <QPainter>
#include "ui/PdfViewerWidget.h"

class TestViewerRotation : public QObject {
    Q_OBJECT
private slots:
    void renderPageHonorsRotation();
    void rotateEmitsEngineRequest();
};
void TestViewerRotation::renderPageHonorsRotation() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("portrait.pdf");
    {
        QPdfWriter w(pdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter p(&w);
        p.drawText(100, 100, QStringLiteral("rotation probe"));
        p.end();
    }

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    const QImage upright = viewer.renderPage(0, 1.0);
    QVERIFY(!upright.isNull());
    {
        QFile diag(QStringLiteral("vr_diag.txt"));
        if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag);
            ts << "upright=" << upright.width() << "x" << upright.height() << "\n";
        }
    }
    QVERIFY2(upright.height() > upright.width(), "A4 portrait baseline");

    viewer.rotateClockwise(); // sets m_rotation=90 and requests engine rotation
    const QImage rotated = viewer.renderPage(0, 1.0);
    QVERIFY(!rotated.isNull());
    {
        QFile diag2(QStringLiteral("vr_diag2.txt"));
        if (diag2.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag2);
            ts << "rotated=" << rotated.width() << "x" << rotated.height() << "\n";
        }
    }
    QVERIFY2(rotated.width() > rotated.height(),
             "renderPage must honor rotation: dimensions swap for 90°");
}

void TestViewerRotation::rotateEmitsEngineRequest() {
    PdfViewerWidget viewer;
    int degrees = 0;
    connect(&viewer, &PdfViewerWidget::requestPageRotation,
            [&](int d) { degrees = d; });
    viewer.rotateClockwise();
    QCOMPARE(degrees, 90);
    viewer.rotateCounterClockwise();
    QCOMPARE(degrees, -90);
}
QTEST_MAIN(TestViewerRotation)
#include "TestViewerRotation.moc"
