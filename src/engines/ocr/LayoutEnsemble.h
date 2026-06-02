// SPDX-License-Identifier: Apache-2.0
// src/engines/ocr/LayoutEnsemble.h
// LayoutEnsemble — IoU-based multi-detector reconciliation (M5-P2 D4).
//
// Runs 1+ ILayoutDetector* in parallel via LaneScheduler, then merges their
// results:
//   - Regions whose bbox IoU > kMergeThreshold (0.5) are grouped.
//   - Within each group: type is decided by confidence-weighted majority vote.
//   - Un-grouped (unique) regions from any detector are kept if confidence > 0.3.
//   - Reading order is re-computed from bbox centroids (top-to-bottom, left-to-right).
//
// Single-detector mode: works correctly when only one detector is added.
#pragma once

#include "engines/ocr/ILayoutDetector.h"
#include "engines/scheduling/LaneScheduler.h"

#include <QList>
#include <memory>
#include <vector>

class LayoutEnsemble {
public:
    /// Construct without a scheduler (single-threaded sequential fallback).
    LayoutEnsemble();

    /// Construct with a LaneScheduler for parallel detector dispatch.
    explicit LayoutEnsemble(gp::LaneScheduler *scheduler);

    ~LayoutEnsemble();

    /// Add a detector.  Does NOT take ownership.
    /// If Surya is not available (HAS_SURYA undefined), simply don't add it;
    /// single-detector mode works correctly.
    void addDetector(ILayoutDetector *detector);

    /// Remove all detectors.
    void clearDetectors();

    /// Run all detectors on `page` and return the merged region list.
    /// Each detector is submitted to the LaneScheduler GPU lane (if a scheduler
    /// was provided) in parallel; results are collected and reconciled.
    QList<LayoutRegion> detect(const QImage &page);

    // ── Helpers (public for testability) ────────────────────────────────────

    /// Compute IoU of two bounding boxes.  Returns 0.0 if either is empty.
    static double computeIoU(const QRectF &a, const QRectF &b);

    /// Merge a list of per-detector region lists using IoU grouping.
    /// iouThreshold: groups regions with IoU > this value (default 0.5).
    /// unpairedConfThreshold: keeps unpaired regions above this confidence (default 0.3).
    static QList<LayoutRegion> mergeRegionLists(
        const QList<QList<LayoutRegion>> &perDetectorResults,
        double iouThreshold        = 0.5,
        double unpairedConfThresh  = 0.3);

    /// Sort regions into reading order (top-to-bottom, left-to-right centroid)
    /// and assign readingOrderIndex 0..N-1.
    static void assignReadingOrder(QList<LayoutRegion> &regions);

private:
    class Private;
    std::unique_ptr<Private> d;
};
