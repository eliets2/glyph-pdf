// SPDX-License-Identifier: Apache-2.0
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
    QString text;          // Plain-text fallback (PDF /Contents). Always kept.
    QString djotSource;    // Djot rich-text source (M6-P4). Internal authoring
                           // model; transcoded to /RC XHTML on save, original
                           // stashed in /PieceInfo. Empty => plain-text only.
    QRectF rect;
    
    // Appearance
    double opacity = 1.0;
    QString blendMode = "Normal";
    bool locked = false;

    // Comment threading & metadata
    QString id;
    QString parentId;
    QList<QString> replies;
    QString author;
    QString creationDate;
    QString modificationDate;
    ReviewState reviewState = ReviewState::None;
};
