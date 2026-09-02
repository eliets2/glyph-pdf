// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/PdfEnums.h"
#include <QImage>
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
    Completed,
    LastValue = Completed  // sentinel — update if new values are added
};

// §9.1 P0: a clickable link annotation on a page (URI or internal GoTo).
struct PdfLinkInfo {
    QRectF rect;        // click region in PDF user space (bottom-left origin)
    bool   isUri = false;
    QString uri;        // valid when isUri
    int    targetPage = -1;  // 0-based page for internal GoTo links
};

struct AnnotationItem {
    int pageIndex = 0;
    ToolMode mode = ToolMode::HandTool;
    QList<QPointF> points;
    QColor color = Qt::yellow;
    int thickness = 2;
    QString text;          // Plain-text fallback (PDF /Contents). Always kept.
    // §9.3 P0 (audit 2026-07-01): DEAD FIELD — never set or read anywhere.
    // CHANGELOG v1.3.0 wrongly claimed comment file attachments shipped; the
    // claim was corrected in docs/audit/CORRECTIONS-2026-07-01.md. Implement
    // file attachments or remove this field before claiming the capability.
    QString attachmentPath;// absolute path to attached file; empty = no attachment
    QString djotSource;    // Djot rich-text source (M6-P4). Internal authoring
                           // model; transcoded to /RC XHTML on save, original
                           // stashed in /PieceInfo. Empty => plain-text only.
    QRectF rect;
    // §9.7 P0 (audit 2026-07-01): raster ink for the signature picker's Type
    // and Upload modes (typed text rendered by SignatureContent::renderTyped,
    // or the decoded image from SignatureContent::loadUploaded). Null for all
    // other modes. Persisted inside the PDF as an image appearance stream.
    QImage image;
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
