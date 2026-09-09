// SPDX-License-Identifier: Apache-2.0
// T1 measurement toolset — pure calibration/geometry/formatting math.
// These tests pin the conversion identities, the 1:N and explicit ratio
// parsers, non-convex polygon area (shoelace), the area-squaring rule, the
// ft-in composite display, snap-to-vertex, and the uncalibrated "shows pt and
// says so" negative control.
#include <QtTest/QtTest>
#include <QList>
#include <QPointF>
#include "core/MeasureCore.h"

using namespace gp::measure;

class TestMeasureCore : public QObject {
    Q_OBJECT
private slots:
    // ── Unit identities ──────────────────────────────────────────────────────
    void unitIdentitiesInchFootMm() {
        // 72 pt = 1 in = 25.4 mm; 12 in = 1 ft (ISO user space = 1/72 in).
        QVERIFY(std::fabs(unitToMm(Unit::Pt) * 72.0 - unitToMm(Unit::In)) < 1e-9);
        QVERIFY(std::fabs(unitToMm(Unit::In) - 25.4) < 1e-9);
        QVERIFY(std::fabs(unitToMm(Unit::In) * 12.0 - unitToMm(Unit::Ft)) < 1e-9);
        QVERIFY(std::fabs(unitToMm(Unit::Cm) - 10.0 * unitToMm(Unit::Mm)) < 1e-12);
        QVERIFY(std::fabs(unitToMm(Unit::M) - 1000.0 * unitToMm(Unit::Mm)) < 1e-9);
    }

    void parseUnitTolerantAndHonest() {
        QCOMPARE(parseUnit(QStringLiteral("mm")).value_or(Unit::Pt), Unit::Mm);
        QCOMPARE(parseUnit(QStringLiteral("IN")).value_or(Unit::Pt), Unit::In);
        QCOMPARE(parseUnit(QStringLiteral("  Feet ")).value_or(Unit::Pt), Unit::Ft);
        QCOMPARE(parseUnit(QStringLiteral("points")).value_or(Unit::Mm), Unit::Pt);
        QVERIFY(!parseUnit(QStringLiteral("bogus")).has_value());
        QVERIFY(!parseUnit(QString()).has_value());
    }

    // ── Calibration: drag along a known length ───────────────────────────────
    void fromKnownLengthComputesUnitsPerPt() {
        // A dragged span of 100 pt declared to be 50 mm ⇒ 0.5 mm per pt.
        const Scale s = fromKnownLength(100.0, 50.0, Unit::Mm);
        QVERIFY(s.calibrated);
        QVERIFY(std::fabs(s.unitsPerPt - 0.5) < 1e-12);
        QCOMPARE(s.unit, Unit::Mm);
        // A 200 pt distance now measures as 100 mm.
        QVERIFY(std::fabs(convertLengthPt(200.0, s) - 100.0) < 1e-9);
        QVERIFY(s.ratio.contains(QStringLiteral("50")));
    }

    void fromKnownLengthRefusesGarbageByFallingBackToPt() {
        for (double bad : { 0.0, -100.0, std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity() }) {
            const Scale s = fromKnownLength(100.0, bad, Unit::Mm);
            QVERIFY(!s.calibrated);
            QCOMPARE(s.unitsPerPt, 1.0);
            QCOMPARE(s.unit, Unit::Pt);
        }
    }

    // ── Calibration: 1:N ratios ──────────────────────────────────────────────
    void ratio1To100MmGives35_28MmPerPt() {
        const Scale s = fromRatioParts(1.0, 100.0, Unit::Mm, Unit::Mm);
        QVERIFY(s.calibrated);
        // 1 pt = 25.4/72 paper-mm, each representing 100 real mm.
        QVERIFY(std::fabs(s.unitsPerPt - 100.0 * 25.4 / 72.0) < 1e-9);
        // A 10 pt line spans 3.528 paper-mm ⇒ 352.8 real mm on a 1:100 sheet.
        QVERIFY(std::fabs(convertLengthPt(10.0, s) - 352.7777778) < 1e-6);
    }

    void ratioIsUnitFreeSameIndexScalesAgree() {
        // 1:100 in mm and 1:100 in ft must agree once expressed in their own
        // unit: a span of N pt measures (N × unitsPerPt) of each unit, and
        // 35.2778 mm == 0.03889 ft for the same pt span.
        const Scale mmS = fromRatioParts(1.0, 100.0, Unit::Mm, Unit::Mm);
        const Scale ftS = fromRatioParts(1.0, 100.0, Unit::Ft, Unit::Ft);
        const double mm = convertLengthPt(10.0, mmS);          // 352.78 mm
        const double ft = convertLengthPt(10.0, ftS);          // 1.1545 ft
        QVERIFY(std::fabs(mm / 1000.0 - ft * unitToMm(Unit::Ft) / 1000.0) < 1e-9);
    }

    // ── Ratio parser ─────────────────────────────────────────────────────────
    void parseScaleRatioForms() {
        // "1:100" (form A)
        const auto a = parseScaleRatio(QStringLiteral("1:100"), Unit::Mm, Unit::Mm);
        QVERIFY(a.has_value());
        QVERIFY(std::fabs(a->unitsPerPt - 100.0 * 25.4 / 72.0) < 1e-9);
        QCOMPARE(a->ratio, QStringLiteral("1:100"));

        // "1/4 in = 1 ft" (form B): 0.25 in = 18 pt on paper = 1 ft real.
        const auto b = parseScaleRatio(QStringLiteral("1/4 in = 1 ft"));
        QVERIFY(b.has_value());
        QCOMPARE(b->unit, Unit::Ft);
        QVERIFY(std::fabs(convertLengthPt(18.0, *b) - 1.0) < 1e-9); // 18 pt → 1.0 ft
        QVERIFY(std::fabs(b->unitsPerPt - 1.0 / 18.0) < 1e-9);      // 1/18 ft per pt

        // explicit-units metric form: "10 mm = 1 m" → 1:100 cross-check
        const auto c = parseScaleRatio(QStringLiteral("10 mm = 1 m"), Unit::Mm, Unit::Mm);
        QVERIFY(c.has_value());
        QCOMPARE(c->unit, Unit::M);
        // 10 mm = 28.3465 pt of paper ⇒ that span reads 1 m.
        QVERIFY(std::fabs(convertLengthPt(10.0 * 72.0 / 25.4, *c) - 1.0) < 1e-9);
    }

    void parseScaleRatioRefusesGarbage() {
        QVERIFY(!parseScaleRatio(QStringLiteral("abc")).has_value());
        QVERIFY(!parseScaleRatio(QStringLiteral("1:")).has_value());
        QVERIFY(!parseScaleRatio(QStringLiteral(":100")).has_value());
        QVERIFY(!parseScaleRatio(QStringLiteral("1 : 2 : 3")).has_value());
        QVERIFY(!parseScaleRatio(QString()).has_value());
        QVERIFY(!parseScaleRatio(QStringLiteral("1/0 in = 1 ft")).has_value()); // zero fraction
    }

    // ── Geometry ─────────────────────────────────────────────────────────────
    void distanceAndPolyline() {
        QCOMPARE(distance({0, 0}, {3, 4}), 5.0);
        // Open polyline: 3-4-5 path measures 7 — it must NOT close the loop.
        const QList<QPointF> path = { {0, 0}, {4, 0}, {4, 3} };
        QVERIFY(std::fabs(polylineLength(path) - 7.0) < 1e-12);
        QCOMPARE(polylineLength({ {1, 1} }), 0.0);
        QCOMPARE(polylineLength({}), 0.0);
    }

    void closedPerimeterClosesTheLoop() {
        // The PERIMETER tool measures the closed boundary: a 4-vertex square
        // is 4× its side (an open-path mistake would report 3×).
        const QList<QPointF> square = { {0, 0}, {72, 0}, {72, 72}, {0, 72} };
        QVERIFY(std::fabs(closedPerimeter(square) - 288.0) < 1e-12);
        QVERIFY(std::fabs(polylineLength(square) - 216.0) < 1e-12);  // open ≠ closed
        // 2-point "shape" closes back on itself: 2× the span.
        QVERIFY(std::fabs(closedPerimeter({ {0, 0}, {10, 0} }) - 20.0) < 1e-12);
        QCOMPARE(closedPerimeter({ {1, 1} }), 0.0);
        QCOMPARE(closedPerimeter({}), 0.0);
    }

    void polygonAreaNonConvexLShape() {
        // L-shape (non-convex): 100×100 square with a 60×60 bite — shoelace
        // handles the reflex vertex exactly; convexifying would report 10000.
        const QList<QPointF> lShape = {
            {0, 0}, {100, 0}, {100, 40}, {40, 40}, {40, 100}, {0, 100}
        };
        QVERIFY(std::fabs(polygonArea(lShape) - 6400.0) < 1e-9);

        // Same polygon wound the other way — area must stay positive.
        QList<QPointF> reversed;
        for (auto it = lShape.rbegin(); it != lShape.rend(); ++it) reversed.append(*it);
        QVERIFY(std::fabs(polygonArea(reversed) - 6400.0) < 1e-9);

        // 3-4-5 right triangle = 6.
        QVERIFY(std::fabs(polygonArea({ {0, 0}, {4, 0}, {0, 3} }) - 6.0) < 1e-12);

        QCOMPARE(polygonArea({ {0, 0}, {10, 10} }), 0.0);  // not a polygon
        QCOMPARE(polygonArea({}), 0.0);
    }

    // ── Formatting + the area-squaring rule ─────────────────────────────────
    void formatLengthAndAreaWithSquaring() {
        const Scale s = fromKnownLength(50.0, 100.0, Unit::Mm); // 2 mm per pt
        QCOMPARE(formatLength(200.0, s), QStringLiteral("400.00 mm"));

        // THE classic takeoff bug: doubling the linear calibration quadruples
        // areas. A 10×10 pt rectangle is 20×20 mm = 400 mm², never 200.
        QCOMPARE(formatArea(100.0, s), QStringLiteral("400.00 mm\u00B2"));
        QVERIFY(std::fabs(convertAreaPt2(100.0, s) - 400.0) < 1e-9);
    }

    void formatLengthFtInComposite() {
        // 18 pt = 1 ft calibration.
        const Scale s = fromKnownLength(18.0, 1.0, Unit::Ft);
        QCOMPARE(formatLength(18.0, s), QStringLiteral("1 ft"));
        QCOMPARE(formatLength(22.5, s), QStringLiteral("1 ft 3 in"));   // 1.25 ft
        QCOMPARE(formatLength(15.0, s), QStringLiteral("0 ft 10 in"));  // 10 in
        // Rounding carry: 1.9999… ft must not display "1 ft 12 in".
        QVERIFY(!formatLength(17.999999, s, 2).contains(QStringLiteral("12 in")));
        // Area uses sq ft with the SQUARED factor: 0.5 ft per pt ⇒ 1 pt² is
        // 0.25 sq ft (linear 0.5, squared 0.25 — never 0.5).
        const Scale half = fromKnownLength(2.0, 1.0, Unit::Ft);
        QCOMPARE(formatArea(1.0, half), QStringLiteral("0.25 sq ft"));
    }

    // ── Negative control: uncalibrated shows pt AND says so ──────────────────
    void uncalibratedScaleShowsPtAndSaysSo() {
        const Scale s = ptScale();
        QVERIFY(!s.calibrated);
        QCOMPARE(s.unitsPerPt, 1.0);
        QCOMPARE(formatLength(12.30, s), QStringLiteral("12.30 pt"));
        const QString truthful = formatLengthTruthful(12.30, s);
        QVERIFY(truthful.contains(QStringLiteral("pt")));
        QVERIFY(truthful.contains(QStringLiteral("not calibrated")));
        // Calibrated readouts must NOT carry the disclaimer.
        QVERIFY(!formatLengthTruthful(12.30, fromKnownLength(1.0, 1.0, Unit::Mm))
                     .contains(QStringLiteral("not calibrated")));
    }

    // ── Snap-to-vertex ───────────────────────────────────────────────────────
    void snapVertexNearestWithinTolerance() {
        const QList<QPointF> verts = { {0, 0}, {50, 0}, {100, 40} };
        bool snapped = false;
        const QPointF p = snapVertex(verts, {3, 2}, 5.0, &snapped);
        QVERIFY(snapped);
        QCOMPARE(p, QPointF(0, 0));
        // Nearest wins, not first.
        bool snapped2 = false;
        const QPointF p2 = snapVertex(verts, {97, 39}, 5.0, &snapped2);
        QVERIFY(snapped2);
        QCOMPARE(p2, QPointF(100, 40));
        // Outside tolerance → unchanged, honest flag.
        bool snapped3 = true;
        const QPointF p3 = snapVertex(verts, {50, 30}, 5.0, &snapped3);
        QVERIFY(!snapped3);
        QCOMPARE(p3, QPointF(50, 30));
        // Empty candidates → unchanged.
        QCOMPARE(snapVertex({}, {1, 1}, 5.0), QPointF(1, 1));
    }

    // ── Mode plumbing ────────────────────────────────────────────────────────
    void measureModePredicate() {
        QVERIFY(isMeasureToolMode(ToolMode::MeasureDistance));
        QVERIFY(isMeasureToolMode(ToolMode::MeasurePerimeter));
        QVERIFY(isMeasureToolMode(ToolMode::MeasureArea));
        QVERIFY(!isMeasureToolMode(ToolMode::DrawLine));
        QVERIFY(!isMeasureToolMode(ToolMode::HandTool));
    }

    void scaleEquality() {
        const Scale a = fromKnownLength(100.0, 50.0, Unit::Mm);
        const Scale b = fromKnownLength(100.0, 50.0, Unit::Mm);
        const Scale c = fromKnownLength(100.0, 60.0, Unit::Mm);
        QVERIFY(a == b);
        QVERIFY(!(a == c));
        QVERIFY(!(ptScale() == a));
    }
};

QTEST_GUILESS_MAIN(TestMeasureCore)
#include "TestMeasureCore.moc"
