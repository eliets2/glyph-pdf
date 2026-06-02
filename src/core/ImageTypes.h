// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QRectF>
#include <QImage>
#include <QTransform>

struct PdfImageInfo {
    int pageIndex = 0;
    QString xobjectName;       // e.g., "/Im0" — the resource name on that page
    QRectF placement;          // position/size in PDF user-space coords (bottom-left origin)
    double rotation = 0.0;     // degrees, extracted from the CTM
    int widthPx = 0;           // native pixel width of the XObject
    int heightPx = 0;          // native pixel height of the XObject
    QImage thumbnail;          // rendered preview for the overlay
};
