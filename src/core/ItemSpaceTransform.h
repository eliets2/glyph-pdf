// SPDX-License-Identifier: Apache-2.0
#pragma once

// ── sweep-legacy 2026-09-19: the user→viewer inverse of the page-space law ──
//
// core/PageSpaceTransform.h (SEP13 L5/L8) defines THE viewer→user transform
// shared by every consumer of viewer marks; it is owned by the redaction-proof
// lane and must not be re-derived. This header adds ONLY the inverse direction
// — raw user space → viewer (display) space — for the legacy read boundaries
// that must surface foreign annotations / form widgets at the spot a
// spec-compliant writer put them (PoDoFoBackend::extractAnnotations):
//
//     viewerToUser(userToViewer(r, g), g) == r   for every /Rotate in {0,90,
//     180,270} and every MediaBox offset (corner-exact: /Rotate is a multiple
//     of 90, so an axis-aligned rect stays axis-aligned and dims swap for the
//     odd rotations exactly as in the forward law).
//
// The forward direction is DELEGATED to gp::PageSpace::viewerToUser — there is
// deliberately no second implementation of it here. A round-trip identity
// against the forward law is pinned by tests/TestLegacyOriginSpace.cpp so any
// drift between the two directions fails loudly.

#include <QPointF>
#include <QRectF>

#include "core/PageSpaceTransform.h"

namespace gp {
namespace ItemSpace {

// Raw user-space rect → viewer-space rect (top-left origin, Y down, displayed
// dims: MediaBox W/H swapped for /Rotate 90/270). Exact inverse of
// gp::PageSpace::viewerToUser.
inline QRectF userToViewer(const QRectF& user, const PageSpace::PageGeometry& g)
{
    const double ux0 = user.x();
    const double uy0 = user.y();
    const double ux1 = user.x() + user.width();
    const double uy1 = user.y() + user.height();
    double vx0, vy0, vx1, vy1;
    switch (g.rotation) {
    case 90:
        vx0 = uy0 - g.y0;             vy0 = ux0 - g.x0;
        vx1 = uy1 - g.y0;             vy1 = ux1 - g.x0;
        break;
    case 180:
        vx0 = g.x0 + g.width - ux1;   vy0 = uy0 - g.y0;
        vx1 = g.x0 + g.width - ux0;   vy1 = uy1 - g.y0;
        break;
    case 270:
        vx0 = g.y0 + g.height - uy1;  vy0 = g.x0 + g.width - ux1;
        vx1 = g.y0 + g.height - uy0;  vy1 = g.x0 + g.width - ux0;
        break;
    case 0:
    default:
        vx0 = ux0 - g.x0;             vy0 = g.y0 + g.height - uy1;
        vx1 = ux1 - g.x0;             vy1 = g.y0 + g.height - uy0;
        break;
    }
    return QRectF(QPointF(qMin(vx0, vx1), qMin(vy0, vy1)),
                  QPointF(qMax(vx0, vx1), qMax(vy0, vy1)));
}

// Single-point convenience (corner-exact for 90-degree rotations, so a
// degenerate rect maps point-to-point).
inline QPointF userPointToViewer(const QPointF& p, const PageSpace::PageGeometry& g)
{
    return userToViewer(QRectF(p, p), g).topLeft();
}

// Single-point forward convenience (same degenerate-rect argument).
inline QPointF viewerPointToUser(const QPointF& p, const PageSpace::PageGeometry& g)
{
    return PageSpace::viewerToUser(QRectF(p, p), g).topLeft();
}

} // namespace ItemSpace
} // namespace gp
