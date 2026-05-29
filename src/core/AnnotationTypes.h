#pragma once

#include "core/PdfEnums.h"
#include <QList>
#include <QPointF>
#include <QColor>
#include <QRectF>
#include <QString>

enum class ReviewState {
    None,
    Open,
    Accepted,
    Rejected,
    Cancelled,
    Completed
};

struct AnnotationItem {
    int pageIndex = 0;
    ToolMode mode = ToolMode::HandTool;
    QList<QPointF> points;
    QColor color = Qt::yellow;
    int thickness = 2;
    QString text;
    QRectF rect;
    
    // Comment threading & metadata
    QString id;
    QString parentId;
    QList<QString> replies;
    QString author;
    QString creationDate;
    QString modificationDate;
    ReviewState reviewState = ReviewState::None;
    bool locked = false;
    QString blendMode = "Normal";
    double opacity = 1.0;
};
