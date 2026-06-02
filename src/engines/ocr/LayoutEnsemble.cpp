// src/engines/ocr/LayoutEnsemble.cpp
// LayoutEnsemble implementation — IoU reconciliation + reading-order assignment.

#include "engines/ocr/LayoutEnsemble.h"

#include <QDebug>
#include <QtConcurrent>
#include <QFutureSynchronizer>

#include <algorithm>
#include <cmath>
#include <map>

// ── Private ─────────────────────────────────────────────────────────────────

class LayoutEnsemble::Private {
public:
    std::vector<ILayoutDetector*> detectors;
    gp::LaneScheduler *scheduler = nullptr;
};

// ── Construction ─────────────────────────────────────────────────────────────

LayoutEnsemble::LayoutEnsemble()
    : d(std::make_unique<Private>())
{}

LayoutEnsemble::LayoutEnsemble(gp::LaneScheduler *scheduler)
    : d(std::make_unique<Private>())
{
    d->scheduler = scheduler;
}

LayoutEnsemble::~LayoutEnsemble() = default;

void LayoutEnsemble::addDetector(ILayoutDetector *detector)
{
    if (detector)
        d->detectors.push_back(detector);
}

void LayoutEnsemble::clearDetectors()
{
    d->detectors.clear();
}

// ── IoU helper ───────────────────────────────────────────────────────────────

double LayoutEnsemble::computeIoU(const QRectF &a, const QRectF &b)
{
    if (a.isEmpty() || b.isEmpty()) return 0.0;
    QRectF inter = a.intersected(b);
    if (inter.isEmpty()) return 0.0;
    double interArea = inter.width() * inter.height();
    double unionArea = a.width() * a.height() + b.width() * b.height() - interArea;
    if (unionArea <= 0.0) return 0.0;
    return interArea / unionArea;
}

// ── Region merging ───────────────────────────────────────────────────────────

QList<LayoutRegion> LayoutEnsemble::mergeRegionLists(
    const QList<QList<LayoutRegion>> &perDetectorResults,
    double iouThreshold,
    double unpairedConfThresh)
{
    if (perDetectorResults.isEmpty())
        return {};

    // Single-detector fast path
    if (perDetectorResults.size() == 1) {
        QList<LayoutRegion> out;
        for (const auto &r : perDetectorResults[0]) {
            if (r.confidence >= unpairedConfThresh || perDetectorResults.size() == 1)
                out.append(r);
        }
        assignReadingOrder(out);
        return out;
    }

    // Multi-detector: greedy IoU grouping.
    // For each region in the first (primary) detector list, try to match it
    // against regions from every other detector list.  Matched regions form a
    // group; the merged region uses confidence-weighted type voting.
    //
    // Algorithm: O(N_detectors × N_regions²) — acceptable for typical page
    // region counts (< 50 per page).

    const int numDetectors = perDetectorResults.size();

    // Flatten all regions with their source detector index
    struct Tagged { LayoutRegion region; int detectorIdx; int origIdx; bool used; };
    QList<Tagged> all;
    for (int d = 0; d < numDetectors; ++d) {
        int idx = 0;
        for (const auto &r : perDetectorResults[d]) {
            all.append(Tagged{ r, d, idx++, false });
        }
    }

    QList<LayoutRegion> merged;

    // For each un-used region from detector 0, start a group
    for (auto &anchor : all) {
        if (anchor.detectorIdx != 0 || anchor.used) continue;
        anchor.used = true;

        QList<Tagged*> group;
        group.append(&anchor);

        // Try to match from every other detector
        for (auto &candidate : all) {
            if (candidate.detectorIdx == 0 || candidate.used) continue;
            if (computeIoU(anchor.region.bbox, candidate.region.bbox) > iouThreshold) {
                candidate.used = true;
                group.append(&candidate);
            }
        }

        // Confidence-weighted type vote over the group
        // Map: RegionType → accumulated confidence
        std::map<int, double> typeConf;
        double totalConf = 0.0;
        QRectF mergedBbox = anchor.region.bbox;

        for (const auto *t : group) {
            int typeKey = static_cast<int>(t->region.type);
            typeConf[typeKey] += t->region.confidence;
            totalConf         += t->region.confidence;
            mergedBbox = mergedBbox.united(t->region.bbox);
        }

        int bestType = static_cast<int>(RegionType::Other);
        double bestTypeConf = -1.0;
        for (const auto &[k, v] : typeConf) {
            if (v > bestTypeConf) { bestTypeConf = v; bestType = k; }
        }

        LayoutRegion out;
        out.bbox       = mergedBbox;
        out.type       = static_cast<RegionType>(bestType);
        out.confidence = totalConf / group.size();
        merged.append(out);
    }

    // Append un-matched regions from other detectors if confidence is high enough
    for (auto &t : all) {
        if (t.detectorIdx == 0 || t.used) continue;
        if (t.region.confidence >= unpairedConfThresh) {
            t.used = true;
            merged.append(t.region);
        }
    }

    assignReadingOrder(merged);
    return merged;
}

// ── Reading order ─────────────────────────────────────────────────────────────

void LayoutEnsemble::assignReadingOrder(QList<LayoutRegion> &regions)
{
    // Sort by bbox centroid: primary = top-to-bottom (y), secondary = left-to-right (x).
    // Regions within 20 px of the same y-band are sorted left-to-right (column awareness).
    std::sort(regions.begin(), regions.end(),
              [](const LayoutRegion &a, const LayoutRegion &b) {
                  double ay = a.bbox.top() + a.bbox.height() * 0.5;
                  double by = b.bbox.top() + b.bbox.height() * 0.5;
                  if (std::abs(ay - by) > 20.0)
                      return ay < by;
                  double ax = a.bbox.left() + a.bbox.width() * 0.5;
                  double bx = b.bbox.left() + b.bbox.width() * 0.5;
                  return ax < bx;
              });

    for (int i = 0; i < regions.size(); ++i)
        regions[i].readingOrderIndex = i;
}

// ── detect ───────────────────────────────────────────────────────────────────

QList<LayoutRegion> LayoutEnsemble::detect(const QImage &page)
{
    if (page.isNull() || d->detectors.empty())
        return {};

    QList<QList<LayoutRegion>> perDetectorResults;
    perDetectorResults.reserve((int)d->detectors.size());

    if (d->scheduler && d->detectors.size() > 1) {
        // Parallel dispatch via LaneScheduler GPU lane.
        // Each detector holds its own ONNX session / mutex — safe to run concurrently.
        using ResultType = QList<LayoutRegion>;
        QList<gp::SchedulerResult<ResultType>> futures;
        futures.reserve((int)d->detectors.size());

        for (ILayoutDetector *det : d->detectors) {
            gp::SchedulerOptions opts;
            opts.lane     = gp::Lane::GPU;
            opts.taskName = det->name();
            auto fut = d->scheduler->submit<ResultType>(
                opts,
                [det, &page]() -> ResultType {
                    return det->detect(page, gp::Lane::GPU);
                });
            futures.append(std::move(fut));
        }

        for (auto &fut : futures) {
            fut.waitForFinished();
            const auto &sv = fut.result();
            if (sv.ok)
                perDetectorResults.append(sv.value);
            else {
                qWarning() << "LayoutEnsemble: detector future failed:"
                           << sv.error.message;
                perDetectorResults.append(QList<LayoutRegion>());  // empty placeholder
            }
        }

    } else {
        // Sequential fallback (no scheduler, or single detector).
        for (ILayoutDetector *det : d->detectors) {
            perDetectorResults.append(det->detect(page, gp::Lane::GPU));
        }
    }

    return mergeRegionLists(perDetectorResults);
}
