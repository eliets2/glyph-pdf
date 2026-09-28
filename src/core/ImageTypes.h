// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QRectF>
#include <QImage>
#include <QTransform>

struct PdfImageInfo {
    int pageIndex = 0;
    QString xobjectName;       // e.g., "/Im0" — the resource name on that page
    // N1: the 0-based occurrence of this placement in stream order among the
    // placements of the SAME name on this page. One XObject may be drawn any
    // number of times; every placement then shares xobjectName, and only
    // (xobjectName, occurrence) addresses ONE of them — addressing by name
    // alone always hit the first /Do and edited the wrong placement.
    int occurrence = 0;
    QRectF placement;          // position/size in PDF user-space coords (bottom-left origin)
    double rotation = 0.0;     // degrees, extracted from the CTM
    int widthPx = 0;           // native pixel width of the XObject
    int heightPx = 0;          // native pixel height of the XObject
    QImage thumbnail;          // rendered preview for the overlay

    // CX-02: the placement geometry kept as all six affine coefficients —
    // never a (w, h, rotation) breakdown, which drops skew and reflection.
    // matrix is the effective matrix at the image's Do (image space → page
    // space). baseMatrix is the CTM in force just before the image's own last
    // cm, so an edit can write desired × base⁻¹ as the new local cm instead
    // of letting every enclosing transform apply twice. hasLocalMatrix is
    // false when the placement sets no cm of its own (nothing to rewrite);
    // baseInvertible is false when the enclosing CTM is singular (edits are
    // refused — a compensating local cm does not exist).
    double matrix[6] = {1, 0, 0, 1, 0, 0};
    double baseMatrix[6] = {1, 0, 0, 1, 0, 0};
    bool hasLocalMatrix = false;
    bool baseInvertible = false;
};
