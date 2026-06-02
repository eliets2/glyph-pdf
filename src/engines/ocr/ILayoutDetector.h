// SPDX-License-Identifier: Apache-2.0
// src/engines/ocr/ILayoutDetector.h
// ILayoutDetector — pure-virtual layout detection interface (M5-P2 D1).
//
// Each concrete implementation (PpDocLayoutDetector, SuryaDetector, …) runs
// on the lane chosen by the caller.  LayoutEnsemble submits them in parallel
// through LaneScheduler and reconciles the results.
#pragma once

#include <QImage>
#include <QList>
#include <QRectF>

#include "engines/scheduling/ILaneScheduler.h"  // Lane enum

/// Document-region semantic type.
/// Ordered by typical reading-priority (Title → content → navigational).
enum class RegionType {
    Title,
    Paragraph,
    Table,
    Figure,
    List,
    Header,
    Footer,
    Equation,
    Reference,
    Caption,
    Other
};

/// A single detected layout region with spatial + semantic metadata.
struct LayoutRegion {
    /// Bounding box in page-image pixel coordinates (top-left origin).
    QRectF  bbox;

    /// Semantic type inferred by the detector.
    RegionType type = RegionType::Other;

    /// Reading order index assigned by the detector (0 = first).
    /// LayoutEnsemble re-computes this field from centroids after merging.
    int readingOrderIndex = 0;

    /// Detection confidence in [0.0, 1.0].
    double confidence = 0.0;
};

/// Pure-virtual layout-detector interface.
/// Implementors MUST be thread-safe: detect() may be called concurrently from
/// different LaneScheduler worker threads.
class ILayoutDetector {
public:
    virtual ~ILayoutDetector() = default;

    /// Detect layout regions in `page`.
    ///
    /// `lane` hints which LaneScheduler lane to use for ONNX inference (or
    /// subprocess dispatch for external tools).  Implementations may ignore
    /// the hint if their backend is lane-agnostic.
    ///
    /// Returns an empty list on failure; never throws.
    virtual QList<LayoutRegion> detect(const QImage &page,
                                       gp::Lane lane = gp::Lane::GPU) = 0;

    /// Human-readable name for logging / CHANGELOG.
    virtual QString name() const = 0;
};
