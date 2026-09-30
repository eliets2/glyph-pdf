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
    QString djotSource;    // Djot rich-text source (M6-P4). Internal authoring
                           // model; transcoded to /RC XHTML on save, original
                           // stashed in /PieceInfo. Empty => plain-text only.
    QRectF rect;
    // PARITY §9.3 P1 row 15 (scorecard 2026-09-30): per-LINE rects for
    // text-anchored Highlight/Underline/Strikeout/Squiggly — one quad per
    // text line, in the SAME display space as `rect`, TOP line first (PDF
    // reading order). Serialized as ISO 32000 §12.5.6.10 /QuadPoints so the
    // markup hugs the actual glyphs instead of the drag union rect (which
    // covers blank space and misses wrapped lines). EMPTY for legacy
    // drag-rect markup — the free-rect mode is unchanged, this is an
    // additional mode, not a replacement.
    QList<QRectF> quads;
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

    // ── T1 measurement (MeasureDistance/Perimeter/Area modes only) ──────────
    // Calibration carried by the measurement. unitsPerPt = real-world units per
    // PDF user-space unit (1/72 in) — the number written as the /C factor of
    // the /X number-format array in the ISO 32000-1 /Measure dictionary
    // (PoDoFoBackend). An UNCALIBRATED measurement carries the truthful 1 pt
    // scale (calibrated=false, unitsPerPt==1, unit=="pt"); it is never a
    // fabricated real-world claim. Defaults keep non-measure items unchanged.
    bool measureCalibrated = false;
    double measureUnitsPerPt = 1.0;
    QString measureUnit = QStringLiteral("pt");       // linear label, e.g. "mm"
    QString measureAreaUnit = QStringLiteral("pt\u00B2"); // area label, e.g. "mm²"
    QString measureRatio;                              // human scale text → /R
};
