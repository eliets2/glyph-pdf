// SPDX-License-Identifier: Apache-2.0
// T1 measurement toolset — interactive UI honesty contract.
//
// Two surfaces, one exe:
//   1. MeasureMode (the right-dock panel): the persistence disclosure and the
//      lane's deferred-scope disclosures are pinned verbatim; the calibration
//      scope text is honest about page-vs-document scope; an invalid custom
//      ratio (including the gate's G23 poison inputs) NEVER becomes a
//      calibration — the panel emits an honest error and keeps the previous
//      state; page-scoped calibration yields truthful pt on other pages.
//   2. AnnotationLayer (the click-based measure input): two-click distance,
//      click+Enter and click+double-click closing for perimeter/area, Esc
//      cancel, Backspace undo, draft discarded when leaving a measure tool,
//      degenerate drafts never commit, and the committed item rect is a real
//      bounding box (the overlay half of gate G21).
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalSpy>
#include <QToolButton>

#include "modes/MeasureMode.h"
#include "ui/AnnotationLayer.h"
#include "core/MeasureCore.h"

using namespace gp::measure;
using gp::MeasureMode;

namespace {
// Drive a real click-press on the layer (press+release at widget coords).
void layerClick(AnnotationLayer* layer, const QPointF& p)
{
    const QPoint wp = p.toPoint();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(wp), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(layer, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(wp), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(layer, &release);
}

void layerDoubleClick(AnnotationLayer* layer, const QPointF& p)
{
    const QPoint wp = p.toPoint();
    QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(wp), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(layer, &dbl);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(wp), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(layer, &release);
}
} // namespace

class TestMeasurePanelHonesty : public QObject {
    Q_OBJECT
private slots:
    // ── Panel: disclosure wording (clause 4) ────────────────────────────────
    void disclosurePinsPersistenceAndDeferredScopes() {
        const QString d = MeasureMode::disclosureText();
        // What persists.
        QVERIFY2(d.contains("ISO 32000-1"), "names the measure dictionary standard");
        QVERIFY2(d.contains("/Measure"), "names the dictionary key");
        QVERIFY2(d.contains("contents"), "value snapshot disclosed");
        // Calibration is session-only.
        QVERIFY2(d.contains("session-only"), "calibration not persisted");
        // Deferred scopes — named, not silent.
        QVERIFY2(d.contains("CSV"), "CSV export deferred");
        QVERIFY2(d.contains("comment popup"), "AP-stream value caption deferred");
        QVERIFY2(d.contains("per-viewport"), "per-viewport scales deferred");
        QVERIFY2(d.contains("decimal ft"), "ft-in written as decimal ft");
    }

    // ── Panel: scope text + page-scoped truthfulness ─────────────────────────
    void pageScopeYieldsTruthfulPtOnOtherPages() {
        MeasureMode panel;
        QVERIFY(!panel.pageScopeOnly());          // default: whole document
        panel.setPageScopeOnly(true);
        QVERIFY(panel.pageScopeOnly());

        // Uncalibrated: scope text says pt, and it says so.
        QVERIFY(panel.calibrationScopeText().contains("pt"));
        QVERIFY(panel.calibrationScopeText().contains("not calibrated"));

        // Calibrated 2 pt → 1 mm, page-scoped to page 0.
        panel.setPageScopeOnly(false);
        panel.setPendingScale(fromKnownLength(2.0, 1.0, Unit::Mm));
        panel.setPageScopeOnly(true);
        panel.setCalibratedPage(0);               // calibration made on page 0
        const Scale onCalibratedPage = panel.activeScaleForPage(0);
        QVERIFY(onCalibratedPage.calibrated);
        QCOMPARE(onCalibratedPage.unit, Unit::Mm);
        // Another page: truthful pt, NOT the real-world claim.
        const Scale otherPage = panel.activeScaleForPage(5);
        QVERIFY2(!otherPage.calibrated, "page-scoped calibration must not leak");
        QCOMPARE(otherPage.unit, Unit::Pt);
        QCOMPARE(otherPage.unitsPerPt, 1.0);
        // The scope disclosure names the page and the pt fallback.
        QVERIFY(panel.calibrationScopeText().contains("page 1"));
        QVERIFY(panel.calibrationScopeText().contains("not calibrated"));
    }

    // ── Panel: G23 poison inputs never become a calibration ──────────────────
    void invalidCustomRatioIsRejectedWithAnHonestError() {
        MeasureMode panel;
        panel.setPendingScale(fromKnownLength(2.0, 1.0, Unit::Mm));
        QSignalSpy status(&panel, &MeasureMode::statusMessageRequested);

        auto* combo = panel.findChild<QComboBox*>();
        auto* edits = panel.findChild<QLineEdit*>();
        QVERIFY2(combo && edits, "unit combo and custom-ratio edit exist");

        for (const QString& poison : { QString("1 typo = 1 ft"),
                                       QString("1 in = 1 typo"),
                                       QString("1/4 999 in = 1 ft"),
                                       QString("garbage") }) {
            edits->setText(poison);
            edits->editingFinished();
            // The pending calibration is UNCHANGED (still 0.5 mm/pt) …
            const Scale s = panel.pendingScale();
            QVERIFY2(s.calibrated, "previous calibration kept");
            QVERIFY(std::fabs(s.unitsPerPt - 0.5) < 1e-12);
            // … and an honest error was emitted.
            QVERIFY2(!status.isEmpty(), "error surfaced for " + poison.toUtf8());
            QVERIFY(status.last().first().toString().contains("Could not read"));
            status.clear();
        }
    }

    // ── Panel: calibration-stroke seam ───────────────────────────────────────
    void calibrationStrokeSeamComputesKnownLengthScale() {
        const Scale s = MeasureMode::scaleFromCalibrationStroke(
            ptScale(), 100.0, 50.0, Unit::Mm);
        QVERIFY(s.calibrated);
        QVERIFY(std::fabs(s.unitsPerPt - 0.5) < 1e-12);
        QCOMPARE(s.unit, Unit::Mm);
        // Garbage real length falls back to the uncalibrated truth.
        const Scale bad = MeasureMode::scaleFromCalibrationStroke(
            ptScale(), 100.0, -5.0, Unit::Mm);
        QVERIFY(!bad.calibrated);
        QCOMPARE(bad.unit, Unit::Pt);
    }

    // ── Layer: two-click distance commits a real item (and a real rect) ─────
    void distanceTwoClicksCommit() {
        AnnotationLayer layer;
        layer.setMode(ToolMode::MeasureDistance);
        layer.setActiveMeasureScale(fromKnownLength(2.0, 1.0, Unit::Mm));
        QSignalSpy finished(&layer, &AnnotationLayer::measurementFinished);
        QSignalSpy changed(&layer, &AnnotationLayer::annotationsChanged);

        layerClick(&layer, QPointF(10, 10));
        QCOMPARE(finished.count(), 0);            // one click = draft only
        QVERIFY(layer.isMeasuring());
        layerClick(&layer, QPointF(82, 10));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!layer.isMeasuring());

        const QList<AnnotationItem> items = layer.annotations();
        QCOMPARE(items.size(), 1);
        const AnnotationItem& item = items.first();
        QCOMPARE(item.mode, ToolMode::MeasureDistance);
        QCOMPARE(item.points.size(), 2);
        // Calibration stamped on the item.
        QVERIFY(item.measureCalibrated);
        QVERIFY(std::fabs(item.measureUnitsPerPt - 0.5) < 1e-12);
        // The committed rect is the G21-correct bounding box (plus stroke pad),
        // never the 0×0 last-point rect.
        QVERIFY2(item.rect.width() >= 72.0, "G21 overlay: rect spans the stroke");
        QVERIFY2(item.rect.height() > 0.0, "G21 overlay: nonzero height");
        // The /Contents snapshot states the calibrated value (72 pt = 36 mm).
        QCOMPARE(item.text, QStringLiteral("36.00 mm"));
    }

    // ── Layer: click + Enter closes a perimeter with CLOSED semantics ───────
    void perimeterEnterClosesWithClosedSemantics() {
        AnnotationLayer layer;
        layer.setMode(ToolMode::MeasurePerimeter);
        layer.setActiveMeasureScale(fromKnownLength(2.0, 1.0, Unit::Mm));
        QSignalSpy finished(&layer, &AnnotationLayer::measurementFinished);

        layerClick(&layer, QPointF(10, 10));
        layerClick(&layer, QPointF(82, 10));
        layerClick(&layer, QPointF(82, 82));
        layerClick(&layer, QPointF(10, 82));
        QCOMPARE(finished.count(), 0);
        layer.setFocus();
        layer.activateWindow();
        QTest::keyClick(&layer, Qt::Key_Return);
        QCOMPARE(finished.count(), 1);

        const QList<AnnotationItem> items = layer.annotations();
        QCOMPARE(items.size(), 1);
        const AnnotationItem& item = items.first();
        QCOMPARE(item.points.size(), 4);
        // THE G22 interactive contract: the committed value counts the closing
        // segment — 4 sides of 72 pt = 288 pt = 144 mm, exactly what the
        // serialized /PolyLine path (with the closing vertex, G22 writer) walks
        // out to and what the label snapshot states.
        QVERIFY(std::fabs(closedPerimeter(item.points) - 288.0) < 1e-9);
        QCOMPARE(item.text, QStringLiteral("144.00 mm"));
    }

    // ── Layer: double-click closes (Qt routes the 2nd press to dbl-click) ───
    void doubleClickClosesPerimeter() {
        AnnotationLayer layer;
        layer.setMode(ToolMode::MeasureArea);
        layer.setActiveMeasureScale(ptScale());
        QSignalSpy finished(&layer, &AnnotationLayer::measurementFinished);

        layerClick(&layer, QPointF(0, 0));
        layerClick(&layer, QPointF(40, 0));
        layerClick(&layer, QPointF(40, 40));
        layerDoubleClick(&layer, QPointF(0, 0));   // close on the first vertex
        QCOMPARE(finished.count(), 1);
        const QList<AnnotationItem> items = layer.annotations();
        QCOMPARE(items.size(), 1);
        // Double-click on an existing vertex must not duplicate it.
        QCOMPARE(items.first().points.size(), 3);
    }

    // ── Layer: Esc cancels; leaving the measure toolset discards the draft ──
    void escapeAndModeSwitchDiscardDrafts() {
        AnnotationLayer layer;
        layer.setMode(ToolMode::MeasurePerimeter);
        layerClick(&layer, QPointF(1, 1));
        layerClick(&layer, QPointF(50, 1));
        QVERIFY(layer.isMeasuring());
        layer.setFocus();
        layer.activateWindow();
        QTest::keyClick(&layer, Qt::Key_Escape);
        QVERIFY2(!layer.isMeasuring(), "Esc cancels the draft");
        QCOMPARE(layer.annotations().size(), 0);

        // A stale draft must never survive a switch into an unrelated (non-
        // measure) tool: the "later unrelated drag commits a polygon" bug class.
        layer.setMode(ToolMode::MeasureArea);
        layerClick(&layer, QPointF(1, 1));
        QVERIFY(layer.isMeasuring());
        layer.setMode(ToolMode::HandTool);
        QVERIFY2(!layer.isMeasuring(), "non-measure mode discards the draft");
        layer.setMode(ToolMode::MeasureDistance);
        layerClick(&layer, QPointF(99, 99));
        layerClick(&layer, QPointF(120, 120));     // completes a FRESH distance
        const QList<AnnotationItem> items = layer.annotations();
        QCOMPARE(items.size(), 1);
        QVERIFY(!layer.isMeasuring());
        // Only the two fresh vertices — nothing leaked from the discarded draft.
        QCOMPARE(items.first().points,
                 QList<QPointF>({ QPointF(99, 99), QPointF(120, 120) }));

        // Within the measure toolset a draft carries over by design (the click
        // model is shared) — pinned so a future change is a conscious one.
        layer.setMode(ToolMode::MeasureArea);
        layerClick(&layer, QPointF(0, 0));
        layer.setMode(ToolMode::MeasurePerimeter);
        QVERIFY2(layer.isMeasuring(), "measure→measure keeps the draft");
    }

    // ── Layer: degenerate drafts are discarded, Backspace undoes ────────────
    void degenerateDraftsNeverCommitAndBackspaceUndoes() {
        AnnotationLayer layer;
        layer.setMode(ToolMode::MeasureArea);
        QSignalSpy finished(&layer, &AnnotationLayer::measurementFinished);

        layerClick(&layer, QPointF(0, 0));
        layerClick(&layer, QPointF(10, 0));
        layer.setFocus();
        layer.activateWindow();
        QTest::keyClick(&layer, Qt::Key_Return);   // area needs >= 3
        QCOMPARE(finished.count(), 0);
        QVERIFY2(!layer.isMeasuring(), "degenerate draft is discarded, not committed");
        QCOMPARE(layer.annotations().size(), 0);

        // Backspace undoes a vertex; emptying the draft cancels it.
        layerClick(&layer, QPointF(0, 0));
        layerClick(&layer, QPointF(40, 0));
        layerClick(&layer, QPointF(40, 40));
        QVERIFY(layer.isMeasuring());
        QTest::keyClick(&layer, Qt::Key_Backspace);
        QVERIFY(layer.isMeasuring());
        QTest::keyClick(&layer, Qt::Key_Backspace);
        QTest::keyClick(&layer, Qt::Key_Backspace);
        QVERIFY(!layer.isMeasuring());
        QCOMPARE(finished.count(), 0);
        QCOMPARE(layer.annotations().size(), 0);
    }
};

QTEST_MAIN(TestMeasurePanelHonesty)
#include "TestMeasurePanelHonesty.moc"
