// SPDX-License-Identifier: Apache-2.0
#pragma once

// T1 measurement toolset — the pure calibration/geometry/formatting core.
//
// Header-only on purpose: pdfws_core is an INTERFACE (header-only) target by
// design, and every consumer of this math (AnnotationLayer overlay, MeasureMode
// panel, PoDoFoBackend writer/reader, tests) already compiles against src/.
// No GUI, no engines, no I/O — everything here is deterministic and directly
// unit-testable (tests/TestMeasureCore.cpp).
//
// Unit model (verified against ISO 32000-1:2008 clause 12.9):
//  * PDF default user space unit = 1/72 inch. Every calibration reduces to
//    `unitsPerPt` = how many real-world units ONE user-space unit represents.
//  * That number is exactly the /C factor of the FIRST number-format dictionary
//    in the /X array of an ISO 32000-1 rectilinear measure dictionary
//    (spec example: "/R (1in = 0.1 mi)" ⇒ /C 0.00139 = 0.1/72).
//  * Areas scale with unitsPerPt² (the /A array's first /C converts from
//    (largest X unit)² to the display area unit; we keep both in the same
//    unit family, so the factor is 1 and area = pt² × unitsPerPt²).

#include <QList>
#include <QPointF>
#include <QRegularExpression>
#include <QString>
#include <cmath>
#include <optional>

#include "core/PdfEnums.h"

namespace gp {
namespace measure {

enum class Unit { Pt, Mm, Cm, M, In, Ft };

// Calibration state carried by every measurement. An UNCALIBRATED measurement
// is a truthful scale of 1 pt per pt — never a fabricated real-world claim.
struct Scale {
    bool   calibrated = false;
    double unitsPerPt = 1.0;   // real-world units per one user-space unit (1/72 in)
    Unit   unit       = Unit::Pt;
    QString ratio;             // human-readable scale text; becomes the /R string

    bool operator==(const Scale& o) const
    {
        return calibrated == o.calibrated
            && std::fabs(unitsPerPt - o.unitsPerPt) < 1e-12
            && unit == o.unit && ratio == o.ratio;
    }
};

// ── Unit plumbing ────────────────────────────────────────────────────────────

inline double unitToMm(Unit u)
{
    switch (u) {
    case Unit::Pt: return 25.4 / 72.0;
    case Unit::Mm: return 1.0;
    case Unit::Cm: return 10.0;
    case Unit::M:  return 1000.0;
    case Unit::In: return 25.4;
    case Unit::Ft: return 304.8;
    }
    return 1.0;
}

inline QString unitLabel(Unit u)
{
    switch (u) {
    case Unit::Pt: return QStringLiteral("pt");
    case Unit::Mm: return QStringLiteral("mm");
    case Unit::Cm: return QStringLiteral("cm");
    case Unit::M:  return QStringLiteral("m");
    case Unit::In: return QStringLiteral("in");
    case Unit::Ft: return QStringLiteral("ft");
    }
    return QStringLiteral("pt");
}

inline QString areaLabel(Unit u)
{
    switch (u) {
    case Unit::Pt: return QStringLiteral("pt\u00B2");   // pt²
    case Unit::Mm: return QStringLiteral("mm\u00B2");
    case Unit::Cm: return QStringLiteral("cm\u00B2");
    case Unit::M:  return QStringLiteral("m\u00B2");
    case Unit::In: return QStringLiteral("sq in");
    case Unit::Ft: return QStringLiteral("sq ft");
    }
    return QStringLiteral("pt\u00B2");
}

// Tolerant unit-name parse ("mm", "MM", "inch", "inches", "feet", …).
inline std::optional<Unit> parseUnit(QString text)
{
    const QString s = text.trimmed().toLower();
    if (s == QLatin1String("pt") || s == QLatin1String("point") || s == QLatin1String("points"))
        return Unit::Pt;
    if (s == QLatin1String("mm"))
        return Unit::Mm;
    if (s == QLatin1String("cm"))
        return Unit::Cm;
    if (s == QLatin1String("m") || s == QLatin1String("meter") || s == QLatin1String("meters")
        || s == QLatin1String("metre") || s == QLatin1String("metres"))
        return Unit::M;
    if (s == QLatin1String("in") || s == QLatin1String("inch") || s == QLatin1String("inches"))
        return Unit::In;
    if (s == QLatin1String("ft") || s == QLatin1String("foot") || s == QLatin1String("feet"))
        return Unit::Ft;
    return std::nullopt;
}

inline bool isMeasureToolMode(ToolMode mode)
{
    return mode == ToolMode::MeasureDistance
        || mode == ToolMode::MeasurePerimeter
        || mode == ToolMode::MeasureArea;
}

// ── Calibration entry points (contract clause 1) ─────────────────────────────

// The uncalibrated truth: 1 pt measures as 1 pt and says so.
inline Scale ptScale()
{
    Scale s;
    s.calibrated = false;
    s.unitsPerPt = 1.0;
    s.unit = Unit::Pt;
    s.ratio = QStringLiteral("1 pt = 1 pt");
    return s;
}

// "Drag along a known length": the user drew `pdfLengthPt` user-space units and
// declares that span to really be `realLength` `unit`s. Refuses non-finite or
// non-positive input by falling back to the uncalibrated truth.
inline Scale fromKnownLength(double pdfLengthPt, double realLength, Unit unit,
                             const QString& ratioNote = QString())
{
    Scale s = ptScale();
    if (!std::isfinite(pdfLengthPt) || !std::isfinite(realLength)
        || pdfLengthPt <= 0.0 || realLength <= 0.0)
        return s;
    s.calibrated = true;
    s.unitsPerPt = realLength / pdfLengthPt;
    s.unit = unit;
    s.ratio = ratioNote.isEmpty()
        ? QStringLiteral("%1 %2 = %3 pt")
              .arg(QString::number(realLength, 'f', 4), unitLabel(unit),
                   QString::number(pdfLengthPt, 'f', 4))
        : ratioNote;
    return s;
}

// Direct preset: this many real-world `unit`s per user-space unit.
inline Scale fromUnitsPerPt(double unitsPerPt, Unit unit, const QString& ratioNote = QString())
{
    Scale s = ptScale();
    if (!std::isfinite(unitsPerPt) || unitsPerPt <= 0.0)
        return s;
    s.calibrated = true;
    s.unitsPerPt = unitsPerPt;
    s.unit = unit;
    s.ratio = ratioNote.isEmpty()
        ? QStringLiteral("1 pt = %1 %2").arg(QString::number(unitsPerPt, 'g', 6), unitLabel(unit))
        : ratioNote;
    return s;
}

// "1:100"-style scale of a drawing. `drawingUnit` is the unit the sheet is
// drawn in; `realUnit` the unit the ratio's denominator counts in.
//
// Dimensional note (why realUnit cancels): a ratio N:D is unit-free —
// 1 drawing-unit spans D real-units. The sheet carries unitToMm(drawingUnit)
// millimetres per drawing-unit and unitToMm(Pt) millimetres per user-space
// unit, so real-units-per-pt = unitToMm(Pt) × (N/D) / unitToMm(drawingUnit).
// Cross-unit scales ("1/4 in = 1 ft") are NOT a ratio — they go through
// parseScaleRatio form B / fromKnownLength, which convert both sides in mm.
inline Scale fromRatioParts(double numerator, double denominator,
                            Unit drawingUnit, Unit realUnit,
                            const QString& ratioNote = QString())
{
    Q_UNUSED(realUnit);  // cancels — see dimensional note above
    Scale s = ptScale();
    if (!std::isfinite(numerator) || !std::isfinite(denominator)
        || numerator <= 0.0 || denominator <= 0.0)
        return s;
    // "1:100" ⇒ 1 paper-unit spans 100 real-units (denominator/numerator).
    const double unitsPerPt =
        unitToMm(Unit::Pt) * (denominator / numerator) / unitToMm(drawingUnit);
    return fromUnitsPerPt(unitsPerPt, realUnit, ratioNote);
}

// Parses the two scale syntaxes the UI offers:
//   "1:100", "1:2000"                     — same-unit ratio (drawing/real unit taken
//                                            from the panel's unit combo)
//   "1/4 in = 1 ft", "10 mm = 1 m", "0.5 in = 1 ft" — explicit both-side form
// Returns nullopt for anything it cannot fully parse (the UI shows an honest
// error instead of guessing).
inline std::optional<Scale> parseScaleRatio(QString text,
                                            Unit drawingUnit = Unit::Mm,
                                            Unit realUnit = Unit::Mm)
{
    text = text.trimmed();
    if (text.isEmpty()) return std::nullopt;

    // Form A: "N:D" (optionally "N/D") — ratio in the chosen units.
    static const QRegularExpression ratioRx(
        QStringLiteral("^\\s*(\\d+(?:\\.\\d+)?)\\s*[:/]\\s*(\\d+(?:\\.\\d+)?)\\s*$"));
    {
        const auto m = ratioRx.match(text);
        if (m.hasMatch()) {
            const double n = m.captured(1).toDouble();
            const double d = m.captured(2).toDouble();
            Scale s = fromRatioParts(n, d, drawingUnit, realUnit,
                                     QStringLiteral("1:%1").arg(QString::number(n / d, 'g', 6)));
            if (!s.calibrated) return std::nullopt;
            s.ratio = text;   // keep the user's original ratio text for /R (e.g. "1:100")
            return s;
        }
    }

    // Form B: "<n> [<u1>] = <m> [<u2>]" with optional "n/d" fraction on the left.
    static const QRegularExpression eqRx(QStringLiteral(
        "^\\s*(?:(\\d+)\\s*/\\s*(\\d+)\\s+)?(\\d+(?:\\.\\d+)?)?\\s*([A-Za-z]*)\\s*"
        "=\\s*(\\d+(?:\\.\\d+)?)\\s*([A-Za-z]*)\\s*$"));
    {
        const auto m = eqRx.match(text);
        if (m.hasMatch()) {
            const QString fracNum = m.captured(1);
            const QString fracDen = m.captured(2);
            const QString leftValS = m.captured(3);
            const QString leftUnitS = m.captured(4);
            const double rightVal = m.captured(5).toDouble();
            const QString rightUnitS = m.captured(6);

            double leftVal = leftValS.isEmpty() ? 1.0 : leftValS.toDouble();
            if (!fracNum.isEmpty() && !fracDen.isEmpty()) {
                const double fn = fracNum.toDouble();
                const double fd = fracDen.toDouble();
                if (fn <= 0.0 || fd <= 0.0) return std::nullopt;
                leftVal = fn / fd;   // e.g. "1/4 in": value becomes 0.25
            }
            const Unit leftUnit = leftUnitS.trimmed().isEmpty()
                ? drawingUnit : parseUnit(leftUnitS).value_or(Unit::Pt);
            const Unit rightUnit = rightUnitS.trimmed().isEmpty()
                ? realUnit : parseUnit(rightUnitS).value_or(Unit::Pt);

            if (rightVal <= 0.0 || leftVal <= 0.0) return std::nullopt;
            // left side expressed in user-space points vs right side's real length.
            const double leftPt  = leftVal * unitToMm(leftUnit) / unitToMm(Unit::Pt);
            return fromKnownLength(leftPt, rightVal, rightUnit, text);
        }
    }
    return std::nullopt;
}

// ── Geometry (user-space units; clause 2) ────────────────────────────────────

inline double distance(const QPointF& a, const QPointF& b)
{
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    return std::sqrt(dx * dx + dy * dy);
}

// Open polyline (perimeter of a PolyLine measurement): sum of segment lengths.
inline double polylineLength(const QList<QPointF>& pts)
{
    if (pts.size() < 2) return 0.0;
    double total = 0.0;
    for (int i = 0; i + 1 < pts.size(); ++i)
        total += distance(pts[i], pts[i + 1]);
    return total;
}

// Closed boundary length — the PERIMETER of the shape the vertices outline
// (open-path length + the closing last→first segment). A 4-vertex square is
// 4× its side, never 3×; this is the number the perimeter tool and the /IT
// /PolyLineDimension annotation report.
inline double closedPerimeter(const QList<QPointF>& pts)
{
    if (pts.size() < 2) return 0.0;
    return polylineLength(pts) + distance(pts.last(), pts.first());
}

// Shoelace formula — exact for ANY simple polygon including non-convex ones
// (the polygon measurement tool must not silently triangulate or convexify).
// Returns the unsigned area in user-space units².
inline double polygonArea(const QList<QPointF>& pts)
{
    if (pts.size() < 3) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < pts.size(); ++i) {
        const QPointF& a = pts[i];
        const QPointF& b = pts[(i + 1) % pts.size()];
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return std::fabs(sum) / 2.0;
}

// ── Conversion + formatting (live readout; PDF /Contents snapshot) ──────────

// Rebuilds a Scale from persisted fields (AnnotationItem measure fields, the
// /Measure dictionary's /X[0], or sidecar JSON — all share these semantics).
// Honesty rule enforced here too: pt at 1.0 is never reported as calibrated.
inline Scale scaleFrom(double unitsPerPt, const QString& unitLabel,
                       bool calibrated, const QString& ratioText = QString())
{
    Scale s = ptScale();
    if (!std::isfinite(unitsPerPt) || unitsPerPt <= 0.0) return s;
    s.unitsPerPt = unitsPerPt;
    s.unit = parseUnit(unitLabel).value_or(Unit::Pt);
    s.calibrated = calibrated && !(s.unit == Unit::Pt && unitsPerPt == 1.0);
    s.ratio = ratioText;
    return s;
}

inline double convertLengthPt(double userUnits, const Scale& s)
{
    return userUnits * s.unitsPerPt;
}

// Real-world area. Squaring is the honest part: a 2× mm/pt calibration makes
// areas 4× — getting this wrong is the classic takeoff bug.
inline double convertAreaPt2(double userUnits2, const Scale& s)
{
    return userUnits2 * s.unitsPerPt * s.unitsPerPt;
}

namespace detail {
inline QString num(double v, int decimals)
{
    // Locale-invariant ('.' decimal point) — the PDF /Contents snapshot and the
    // on-screen readout must never diverge by locale.
    return QString::number(v, 'f', decimals);
}
// Whole numbers stay whole in composite displays ("1 ft 3 in", not "1 ft 3.00 in").
inline QString numTrimmed(double v, int decimals)
{
    QString s = QString::number(v, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0'))) s.chop(1);
        if (s.endsWith(QLatin1Char('.'))) s.chop(1);
    }
    return s;
}
} // namespace detail

inline QString formatLength(double userUnits, const Scale& s, int decimals = 2)
{
    const double value = convertLengthPt(userUnits, s);
    if (s.unit == Unit::Ft) {
        // Composite feet-and-inches display ("1 ft 3 in"); the PDF /Measure
        // still carries the decimal-ft value so other viewers agree numerically.
        const double totalInches = value * 12.0;
        qint64 ft = static_cast<qint64>(std::floor(totalInches / 12.0 + 1e-9));
        double rem = totalInches - ft * 12.0;
        const double rounded = std::round(rem * std::pow(10.0, decimals))
            / std::pow(10.0, decimals);
        if (rounded >= 12.0) { ++ft; rem = 0.0; }
        else rem = rounded;
        if (rem > 0.0)
            return QStringLiteral("%1 ft %2 in").arg(QString::number(ft),
                                                     detail::numTrimmed(rem, decimals));
        return QStringLiteral("%1 ft").arg(QString::number(ft));
    }
    return detail::num(value, decimals) + QLatin1Char(' ') + unitLabel(s.unit);
}

inline QString formatArea(double userUnits2, const Scale& s, int decimals = 2)
{
    return detail::num(convertAreaPt2(userUnits2, s), decimals)
        + QLatin1Char(' ') + areaLabel(s.unit);
}

// Honest readout with calibration state — used by the panel and the overlay so
// the "uncalibrated shows pt and says so" clause is one shared string builder.
inline QString formatLengthTruthful(double userUnits, const Scale& s, int decimals = 2)
{
    const QString base = formatLength(userUnits, s, decimals);
    return s.calibrated ? base : base + QStringLiteral(" (not calibrated)");
}

// ── Snap-to-vertex (optional assist; clause 2) ───────────────────────────────

// Returns the nearest candidate vertex within `maxDist` user-space units of p
// (snapped=false and p unchanged when nothing qualifies).
inline QPointF snapVertex(const QList<QPointF>& candidates, const QPointF& p,
                          double maxDist, bool* snapped = nullptr)
{
    if (snapped) *snapped = false;
    if (candidates.isEmpty() || !(maxDist > 0.0)) return p;
    double best = maxDist;
    QPointF result = p;
    bool found = false;
    for (const QPointF& c : candidates) {
        const double d = distance(p, c);
        if (d <= best) { best = d; result = c; found = true; }
    }
    if (found && snapped) *snapped = true;
    return result;
}

} // namespace measure
} // namespace gp
