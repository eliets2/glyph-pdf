#pragma once

#include "core/PdfEnums.h"
#include <QList>
#include <QPointF>
#include <QColor>
#include <QRectF>
#include <QString>

struct AnnotationItem {
    int pageIndex = 0;
    ToolMode mode = ToolMode::HandTool;
    QList<QPointF> points;
    QColor color = Qt::yellow;
    int thickness = 2;
    QString text;
    QRectF rect;
};
