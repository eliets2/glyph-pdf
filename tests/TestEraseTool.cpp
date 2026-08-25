// SPDX-License-Identifier: Apache-2.0
// PRD §9.3 — Annotation Eraser tool.
//
// Drives AnnotationLayer in ToolMode::Erase with QTest-synthesized clicks
// (events are sent directly, so no running event loop is needed) and asserts
// the shared topmost hit-test + deleteAnnotation() behaviour:
//   - the topmost annotation under the cursor is the one erased,
//   - a click over empty space erases nothing,
//   - thin DrawLine segments are hittable along the painted segment,
//   - locked annotations are skipped.
#include <QtTest>
#include "ui/AnnotationLayer.h"

class TestEraseTool : public QObject
{
    Q_OBJECT

private slots:
    void erasesTopmostAnnotationUnderCursor();
    void clickOnEmptySpaceErasesNothing();
    void thinLineIsHittableAlongItsSegment();
    void lockedAnnotationIsNotErased();

private:
    static AnnotationItem makeAnno(int page, const QRectF &rect, ToolMode mode,
                                   const QString &tag, bool locked = false);
};

AnnotationItem TestEraseTool::makeAnno(int page, const QRectF &rect, ToolMode mode,
                                       const QString &tag, bool locked)
{
    AnnotationItem a;
    a.pageIndex = page;
    a.mode = mode;
    a.rect = rect;
    a.text = tag;
    a.locked = locked;
    return a;
}

void TestEraseTool::erasesTopmostAnnotationUnderCursor()
{
    AnnotationLayer layer;
    layer.resize(400, 300);

    // Two overlapping annotations on different pages; the later one paints on
    // top, so the click must erase IT and leave the bottom one untouched.
    AnnotationItem bottom = makeAnno(0, QRectF(10, 10, 200, 40),
                                     ToolMode::Highlight, QStringLiteral("bottom"));
    AnnotationItem top = makeAnno(1, QRectF(20, 20, 100, 30),
                                  ToolMode::AddTextBox, QStringLiteral("top"));
    layer.setAnnotations({bottom, top});

    int changed = 0;
    connect(&layer, &AnnotationLayer::annotationsChanged, [&changed]() { ++changed; });

    layer.setMode(ToolMode::Erase);
    QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(50, 30));

    QCOMPARE(changed, 1);
    const QList<AnnotationItem> after = layer.annotations();
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().text, QStringLiteral("bottom"));
    QCOMPARE(after.first().pageIndex, 0);
}

void TestEraseTool::clickOnEmptySpaceErasesNothing()
{
    AnnotationLayer layer;
    layer.resize(400, 300);
    layer.setAnnotations({makeAnno(0, QRectF(10, 10, 50, 50),
                                   ToolMode::Highlight, QStringLiteral("keep"))});

    int changed = 0;
    connect(&layer, &AnnotationLayer::annotationsChanged, [&changed]() { ++changed; });

    layer.setMode(ToolMode::Erase);
    QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(350, 250));

    QCOMPARE(changed, 0);
    QCOMPARE(layer.annotations().size(), 1);
}

void TestEraseTool::thinLineIsHittableAlongItsSegment()
{
    AnnotationLayer layer;
    layer.resize(400, 300);

    // A horizontal line has a zero-height rect, so rect.contains() can never
    // hit it; the shared hit-test falls back to distance-to-segment (< 10 px).
    const AnnotationItem line = makeAnno(0, QRectF(20, 100, 160, 0),
                                         ToolMode::DrawLine, QStringLiteral("line"));

    // Far off the segment (20 px away): nothing erased, no signal.
    layer.setAnnotations({line});
    int changed = 0;
    connect(&layer, &AnnotationLayer::annotationsChanged, [&changed]() { ++changed; });

    layer.setMode(ToolMode::Erase);
    QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 120));
    QCOMPARE(changed, 0);
    QCOMPARE(layer.annotations().size(), 1);

    // On the segment (3 px away): erased. Reset the counter after the
    // setAnnotations() re-setup, which itself emits annotationsChanged().
    layer.setAnnotations({line});
    changed = 0;
    QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 103));
    QCOMPARE(changed, 1);
    QCOMPARE(layer.annotations().size(), 0);
}

void TestEraseTool::lockedAnnotationIsNotErased()
{
    AnnotationLayer layer;
    layer.resize(400, 300);
    layer.setAnnotations({makeAnno(0, QRectF(10, 10, 80, 60), ToolMode::AddTextBox,
                                   QStringLiteral("pinned"), true)});

    int changed = 0;
    connect(&layer, &AnnotationLayer::annotationsChanged, [&changed]() { ++changed; });

    layer.setMode(ToolMode::Erase);
    QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));

    QCOMPARE(changed, 0);
    QCOMPARE(layer.annotations().size(), 1);
}

QTEST_MAIN(TestEraseTool)
#include "TestEraseTool.moc"
