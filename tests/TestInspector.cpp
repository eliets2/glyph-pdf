#include <QtTest>
#include <QApplication>
#include <QStackedWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QListWidget>

#include "ui/InspectorWidget.h"
#include "core/AnnotationTypes.h"

class TestInspector : public QObject {
    Q_OBJECT

private slots:
    void testEmptyState();
    void testSetAnnotation();
    void testClearAnnotation();
    void testRefreshPropertiesFields();
    void testGeometryBinding();
    void testAppearanceBinding();
    void testContentsBinding();
    void testReplyListPopulates();
};

void TestInspector::testEmptyState()
{
    InspectorWidget inspector;
    // By default, content stack should show empty state (index 0)
    auto* stack = inspector.findChild<QStackedWidget*>();
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->currentIndex(), 0);
}

void TestInspector::testSetAnnotation()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "test-id-1234";
    ann.mode = ToolMode::Highlight;
    ann.color = Qt::red;
    ann.pageIndex = 2;
    ann.author = "Test Author";
    ann.rect = QRectF(10.0, 20.0, 100.0, 50.0);
    ann.text = "Hello world";

    inspector.setAnnotation(&ann);

    auto* stack = inspector.findChild<QStackedWidget*>();
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->currentIndex(), 1);  // properties view
}

void TestInspector::testClearAnnotation()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "test-clear";
    ann.mode = ToolMode::Highlight;

    inspector.setAnnotation(&ann);
    inspector.clearAnnotation();

    auto* stack = inspector.findChild<QStackedWidget*>();
    QCOMPARE(stack->currentIndex(), 0);  // back to empty
}

void TestInspector::testRefreshPropertiesFields()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "abcdef12-3456-7890";
    ann.mode = ToolMode::AddComment;
    ann.color = Qt::blue;
    ann.pageIndex = 4;
    ann.author = "Jane Doe";
    ann.creationDate = "2026-05-29T10:00:00";
    ann.modificationDate = "2026-05-29T12:00:00";
    ann.thickness = 5;
    ann.opacity = 0.75;
    ann.blendMode = "Multiply";
    ann.rect = QRectF(15.0, 25.0, 200.0, 100.0);
    ann.text = "Test content";

    inspector.setAnnotation(&ann);

    // Verify type name displays COMMENT
    auto* typeName = inspector.findChild<QLabel*>("typeName");
    QVERIFY(typeName != nullptr);
    QCOMPARE(typeName->text(), QString("COMMENT"));

    // Verify subheader has page number
    auto* subHeader = inspector.findChild<QLabel*>("subHeader");
    QVERIFY(subHeader != nullptr);
    QVERIFY(subHeader->text().contains("Page 5"));
}

void TestInspector::testGeometryBinding()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "geom-test";
    ann.mode = ToolMode::DrawRectangle;
    ann.rect = QRectF(10.5, 20.3, 150.0, 75.5);

    inspector.setAnnotation(&ann);

    // Find the QDoubleSpinBoxes — they should be populated with the rect values
    auto spinBoxes = inspector.findChildren<QDoubleSpinBox*>();
    QVERIFY(spinBoxes.size() >= 4);

    // The first 4 spinboxes should be X, Y, W, H
    QCOMPARE(spinBoxes[0]->value(), 10.5);
    QCOMPARE(spinBoxes[1]->value(), 20.3);
    QCOMPARE(spinBoxes[2]->value(), 150.0);
    QCOMPARE(spinBoxes[3]->value(), 75.5);
}

void TestInspector::testAppearanceBinding()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "appear-test";
    ann.mode = ToolMode::Highlight;
    ann.opacity = 0.6;
    ann.blendMode = "Screen";
    ann.thickness = 4;

    inspector.setAnnotation(&ann);

    // Find QSlider (opacity)
    auto* slider = inspector.findChild<QSlider*>();
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 60);  // 0.6 * 100

    // Find QComboBox (blend)
    auto* combo = inspector.findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->currentText(), QString("Screen"));

    // Find QSpinBox (border)
    auto* spin = inspector.findChild<QSpinBox*>();
    QVERIFY(spin != nullptr);
    QCOMPARE(spin->value(), 4);
}

void TestInspector::testContentsBinding()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "contents-test";
    ann.mode = ToolMode::AddTextBox;
    ann.text = "Sample annotation text";

    inspector.setAnnotation(&ann);

    // Find QTextEdit
    auto editors = inspector.findChildren<QTextEdit*>();
    QVERIFY(!editors.isEmpty());
    // The first QTextEdit should be the contents editor
    QCOMPARE(editors[0]->toPlainText(), QString("Sample annotation text"));

    // Char count label
    auto* charLabel = inspector.findChild<QLabel*>("charCount");
    QVERIFY(charLabel != nullptr);
    QVERIFY(charLabel->text().contains("22 chars"));
}

void TestInspector::testReplyListPopulates()
{
    InspectorWidget inspector;
    AnnotationItem ann;
    ann.id = "parent-id";
    ann.mode = ToolMode::AddComment;
    ann.text = "Main comment";
    // No viewer wired → replies list should show "No replies yet" placeholder
    ann.replies.clear();

    inspector.setAnnotation(&ann);

    auto* list = inspector.findChild<QListWidget*>();
    QVERIFY(list != nullptr);
    QVERIFY(list->count() >= 1);  // at least the placeholder
}

QTEST_MAIN(TestInspector)
#include "TestInspector.moc"
