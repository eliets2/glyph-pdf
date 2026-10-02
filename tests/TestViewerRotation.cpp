// SPDX-License-Identifier: Apache-2.0
// Audit 9.1 P0 regression test (revised): viewer rotation delegates to the
// engine (/Rotate + DocumentSession::reloadRequested -> viewer::reload, wired
// once in GpMainWindow). One source of truth: reload() resets the viewer's
// own rotation state so snapshots are rotated exactly once by PDFium — never
// doubled by a stale overlay rotation.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include "core/ToolId.h"
#include "ui/PdfViewerWidget.h"

class TestViewerRotation : public QObject {
    Q_OBJECT
private slots:
    void rotateEmitsEngineRequest();
    void reloadResetsRotationState();
    void rotateViewToolsResolveInRegistry();
};
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
void TestViewerRotation::reloadResetsRotationState() {
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
    QVERIFY2(upright.height() > upright.width(), "A4 portrait baseline");

    // Simulate the engine having baked the rotation into the file and the
    // view reloading: rotation state must reset so the next snapshot is NOT
    // rotated a second time.
    viewer.rotateClockwise();          // emits requestPageRotation(90)
    viewer.reload();                   // GpMainWindow wires reloadRequested -> this
    const QImage afterReload = viewer.renderPage(0, 1.0);
    QVERIFY(!afterReload.isNull());
    QCOMPARE(afterReload.width(), upright.width());
    QCOMPARE(afterReload.height(), upright.height());
}
void TestViewerRotation::rotateViewToolsResolveInRegistry() {
    // Rotate View port (Wave 2b tail): the SESSION-ONLY View-menu rotation
    // must exist in the ToolId registry, round-trip its dispatch strings, and
    // stay distinct from the persisted engine-side rotate tools — expressed
    // through the dispatch strings alone so this pin also RUNS (red) on a
    // tree where the RotateView ToolIds are absent.
    const auto cw = toolIdFromString(QStringLiteral("rotateViewCW"));
    QVERIFY2(cw.has_value(), "rotateViewCW must resolve to a ToolId (session-only view rotation is wired)");
    const auto ccw = toolIdFromString(QStringLiteral("rotateViewCCW"));
    QVERIFY2(ccw.has_value(), "rotateViewCCW must resolve to a ToolId (session-only view rotation is wired)");
    if (cw.has_value()) {
        QCOMPARE(toolIdToString(*cw), QStringLiteral("rotateViewCW"));
        QCOMPARE(toolIdFromString(toolIdToString(*cw)), cw);
    }
    if (ccw.has_value()) {
        QCOMPARE(toolIdToString(*ccw), QStringLiteral("rotateViewCCW"));
        QCOMPARE(toolIdFromString(toolIdToString(*ccw)), ccw);
    }
    // Distinct from the PERSISTED Document▸Rotate tools (dispatched by
    // "rotate"/"rotate-cw"/"rotate-ccw").
    const auto persistedCw = toolIdFromString(QStringLiteral("rotate-cw"));
    const auto persistedCcw = toolIdFromString(QStringLiteral("rotate-ccw"));
    QVERIFY2(cw.has_value() && persistedCw.has_value() && *cw != *persistedCw,
             "session-only view rotation must be a distinct tool from the persisted rotate-cw");
    QVERIFY2(ccw.has_value() && persistedCcw.has_value() && *ccw != *persistedCcw,
             "session-only view rotation must be a distinct tool from the persisted rotate-ccw");
}
QTEST_MAIN(TestViewerRotation)
#include "TestViewerRotation.moc"
