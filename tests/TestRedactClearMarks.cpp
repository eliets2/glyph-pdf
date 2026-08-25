// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 regression test: RedactMode's Clear Marks button must remove
// placed redaction marks (ToolMode::Redact annotations) and keep everything else.
#include <QtTest>
#include "modes/RedactMode.h"
#include "ui/PdfViewerWidget.h"
#include "core/AnnotationTypes.h"
class TestRedactClearMarks : public QObject {
    Q_OBJECT
private slots:
    void clearRemovesOnlyRedactMarks();
    void clearWithNoMarksIsSafe();
private:
    static AnnotationItem makeAnno(ToolMode mode, int page);
};

AnnotationItem TestRedactClearMarks::makeAnno(ToolMode mode, int page) {
    AnnotationItem a;
    a.mode = mode;
    a.pageIndex = page;
    a.rect = QRectF(10, 10, 50, 20);
    a.text = QStringLiteral("x");
    return a;
}

void TestRedactClearMarks::clearRemovesOnlyRedactMarks() {
    PdfViewerWidget viewer;
    gp::RedactMode mode;
    mode.setViewer(&viewer);

    QList<AnnotationItem> annos;
    annos << makeAnno(ToolMode::Redact, 0)
          << makeAnno(ToolMode::Highlight, 0)
          << makeAnno(ToolMode::Redact, 1)
          << makeAnno(ToolMode::AddComment, 1)
          << makeAnno(ToolMode::Redact, 2);
    viewer.setAnnotations(annos);
    QCOMPARE(viewer.annotations().size(), 5);
    QVERIFY(QMetaObject::invokeMethod(&mode, "onClearMarks"));

    const QList<AnnotationItem> after = viewer.annotations();
    QCOMPARE(after.size(), 2);
    for (const auto& a : after)
        QVERIFY(a.mode != ToolMode::Redact);
}

void TestRedactClearMarks::clearWithNoMarksIsSafe() {
    PdfViewerWidget viewer;
    gp::RedactMode mode;
    mode.setViewer(&viewer);

    QList<AnnotationItem> annos;
    annos << makeAnno(ToolMode::Highlight, 0);
    viewer.setAnnotations(annos);

    QVERIFY(QMetaObject::invokeMethod(&mode, "onClearMarks"));
    QCOMPARE(viewer.annotations().size(), 1);
    QCOMPARE(viewer.annotations().first().mode, ToolMode::Highlight);
}

QTEST_MAIN(TestRedactClearMarks)
#include "TestRedactClearMarks.moc"
