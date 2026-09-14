// SPDX-License-Identifier: Apache-2.0
#pragma once

// ── SEP13 L5/L8 root cause: ONE shared viewer→user page-space transform ─────
//
// Redaction marks live in VIEWER space: the displayed page's top-left origin,
// Y pointing down, dimensions = the page's DISPLAYED size (MediaBox W/H
// swapped for /Rotate 90/270). This is the space PDFium renders pages in.
// Measured convention (redactfix lane, 2026-09-14; render-probe of fixtures
// with text at user-space offset (dx, dy) from the MediaBox lower-left):
//
//     /Rotate 0:   display = (dx, H - dy)
//     /Rotate 90:  display = (dy, dx)
//     /Rotate 180: display = (W - dx, dy)
//     /Rotate 270: display = (H - dy, W - dx)
//
// with the MediaBox lower-left ORIGIN normalized away (the display size is
// (W, H), swapped for odd rotations — the same numbers FPDF_GetPageWidthF /
// FPDF_GetPageHeightF report).
//
// Content-stream surgery (glyph excision, cover fills, overlay burn-in) and
// source-side text attribution all operate in RAW USER space: origin at the
// MediaBox lower-left, Y up, /Rotate never applied — /Rotate is a view-time
// attribute only; it never enters the content stream.
//
// Every consumer that flipped viewer Y with the MediaBox HEIGHT alone (a)
// dropped the MediaBox lower-left origin and (b) ignored /Rotate entirely.
// On offset-origin or rotated pages marks landed in the wrong place while the
// redaction proof certified the region as clean — a user-data-loss false
// success (SEP13-LEADS L5/L8, confirmed 2026-09-14). This header is the ONE
// transform shared by the excision path (PoDoFoBackend::applyRedactions), the
// burn-in overlay (RedactOperation), and the proof path (RedactionProof
// string attribution). Do NOT re-derive this flip locally.

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <podofo/podofo.h>

namespace gp {
namespace PageSpace {

// Everything the viewer→user transform needs about one page.
struct PageGeometry {
    double x0 = 0.0;      // MediaBox lower-left X
    double y0 = 0.0;      // MediaBox lower-left Y
    double width = 0.0;   // MediaBox width  (rotation-independent)
    double height = 0.0;  // MediaBox height (rotation-independent)
    int rotation = 0;     // /Rotate normalized to {0, 90, 180, 270}
};

inline int normalizeRotation(int rotate)
{
    int r = rotate % 360;
    if (r < 0) r += 360;
    return r;
}

inline PageGeometry pageGeometryFromMediaBox(double x0, double y0,
                                             double width, double height,
                                             int rotation)
{
    PageGeometry g;
    g.x0 = x0;
    g.y0 = y0;
    g.width = width;
    g.height = height;
    g.rotation = normalizeRotation(rotation);
    return g;
}

// The one PoDoFo adapter: rotation-independent MediaBox + /Rotate degrees.
inline PageGeometry pageGeometry(PoDoFo::PdfPage& page)
{
    const PoDoFo::Rect media = page.GetMediaBox();
    return pageGeometryFromMediaBox(media.X, media.Y, media.Width, media.Height,
                                    static_cast<int>(page.GetRotation()));
}

// Displayed page size (what the viewer shows; the numbers FPDF_GetPageWidthF /
// FPDF_GetPageHeightF report).
inline QSizeF displaySize(const PageGeometry& g)
{
    if (g.rotation == 90 || g.rotation == 270)
        return QSizeF(g.height, g.width);
    return QSizeF(g.width, g.height);
}

// Viewer-space rect (top-left origin, Y down) → raw user-space rect.
//
// The returned QRectF is stored y-up: y() is the LOWER edge and height() is
// positive (PDF convention, matching PoDoFo::Rect's lower-left anchor).
// Because /Rotate is a multiple of 90 the transform is corner-exact: an
// axis-aligned rect stays axis-aligned, with width/height swapped for
// rotations 90/270.
inline QRectF viewerToUser(const QRectF& viewer, const PageGeometry& g)
{
    const double vx0 = viewer.x();
    const double vy0 = viewer.y();
    const double vx1 = viewer.x() + viewer.width();
    const double vy1 = viewer.y() + viewer.height();
    double ux0, uy0, ux1, uy1;
    switch (g.rotation) {
    case 90:
        ux0 = g.x0 + vy0;             uy0 = g.y0 + vx0;
        ux1 = g.x0 + vy1;             uy1 = g.y0 + vx1;
        break;
    case 180:
        ux0 = g.x0 + g.width - vx1;   uy0 = g.y0 + vy0;
        ux1 = g.x0 + g.width - vx0;   uy1 = g.y0 + vy1;
        break;
    case 270:
        ux0 = g.x0 + g.width - vy1;   uy0 = g.y0 + g.height - vx1;
        ux1 = g.x0 + g.width - vy0;   uy1 = g.y0 + g.height - vx0;
        break;
    case 0:
    default:
        ux0 = g.x0 + vx0;             uy0 = g.y0 + g.height - vy1;
        ux1 = g.x0 + vx1;             uy1 = g.y0 + g.height - vy0;
        break;
    }
    return QRectF(QPointF(qMin(ux0, ux1), qMin(uy0, uy1)),
                  QPointF(qMax(ux0, ux1), qMax(uy0, uy1)));
}

} // namespace PageSpace
} // namespace gp
