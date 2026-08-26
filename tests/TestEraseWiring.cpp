// SPDX-License-Identifier: Apache-2.0
// Audit 9.2 P0 regression test: the eraser is real now — Erase mode clicks
// emit eraseRequested(page, pos) from the annotation layer (wired to
// deleteObjectAt by EditController), instead of a 'not implemented' dialog.
#include <QtTest/QtTest>
#include "ui/AnnotationLayer.h"
#include "core/PdfEnums.h"

class TestEraseWiring : public QObject {
    Q_OBJECT
private slots:
    void eraseClickEmitsPageAndPosition();
};
void TestEraseWiring::eraseClickEmitsPageAndPosition() {
    AnnotationLayer layer;
    layer.setMode(ToolMode::Erase);
    layer.resize(400, 300);
    layer.setPageAtCallback([](QPoint) { return 3; });

    int gotPage = -99;
    QPointF gotPos;
    connect(&layer, &AnnotationLayer::eraseRequested,
            [&](int page, QPointF pos) { gotPage = page; gotPos = pos; });

    // Synthesize a left-click at (120, 80).
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(120, 80),
                      QPointF(120, 80), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&layer, &press);

    QCOMPARE(gotPage, 3);
    QCOMPARE(gotPos, QPointF(120, 80));
}
QTEST_MAIN(TestEraseWiring)
#include "TestEraseWiring.moc"
