// SPDX-License-Identifier: Apache-2.0
// Audit 9.15 P0 regression test: the thumbnail sidebar +/- zoom buttons were
// built but never connected (dead controls). They must change the real
// thumbnail size (zoom factor), clamp to bounds, and disable at the limits.
#include <QtTest>
#include <QPushButton>
#include "ui/ThumbnailSidebar.h"
class TestThumbnailZoom : public QObject {
    Q_OBJECT
private slots:
    void buttonsExistAndAreWired();
    void zoomStepsChangeFactor();
    void zoomClampsToBoundsAndDisablesButtons();
};
void TestThumbnailZoom::buttonsExistAndAreWired() {    ThumbnailSidebar sb;
    auto* zin = sb.findChild<QPushButton*>(QString::fromLatin1("thumbZoomInBtn"));
    auto* zout = sb.findChild<QPushButton*>(QString::fromLatin1("thumbZoomOutBtn"));
    QVERIFY(zin);
    QVERIFY(zout);
    const double before = sb.thumbZoom();
    zout->click();
    QVERIFY(sb.thumbZoom() < before);
    zin->click();
    QCOMPARE(sb.thumbZoom(), before);
}
void TestThumbnailZoom::zoomStepsChangeFactor() {
    ThumbnailSidebar sb;
    const double base = sb.thumbZoom();
    QVERIFY(QMetaObject::invokeMethod(&sb, "zoomIn"));
    QCOMPARE(sb.thumbZoom(), base + 0.25);
    QVERIFY(QMetaObject::invokeMethod(&sb, "zoomOut"));
    QCOMPARE(sb.thumbZoom(), base);
}
void TestThumbnailZoom::zoomClampsToBoundsAndDisablesButtons() {
    ThumbnailSidebar sb;
    auto* zin = sb.findChild<QPushButton*>(QString::fromLatin1("thumbZoomInBtn"));
    auto* zout = sb.findChild<QPushButton*>(QString::fromLatin1("thumbZoomOutBtn"));
    QVERIFY(QMetaObject::invokeMethod(&sb, "setZoom", Q_ARG(double, 99.0)));
    QCOMPARE(sb.thumbZoom(), 3.0);
    QVERIFY(!zin->isEnabled());
    QVERIFY(zout->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&sb, "setZoom", Q_ARG(double, -1.0)));
    QCOMPARE(sb.thumbZoom(), 0.5);
    QVERIFY(!zout->isEnabled());
    QVERIFY(zin->isEnabled());
}
QTEST_MAIN(TestThumbnailZoom)
#include "TestThumbnailZoom.moc"
