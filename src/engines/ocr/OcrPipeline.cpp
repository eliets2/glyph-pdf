#include "engines/ocr/OcrPipeline.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

// ── Local helpers ───────────────────────────────────────────────────────────

namespace {

QList<MergedOcrWord> toPrimaryOnly(const QList<OcrResult> &results)
{
    QList<MergedOcrWord> out;
    out.reserve(results.size());
    for (const auto &r : results) {
        MergedOcrWord mw;
        mw.text         = r.text;
        mw.boundingBox  = r.boundingBox;
        mw.confidence   = r.confidence;
        mw.sourceEngine = "Tesseract";
        out.append(mw);
    }
    return out;
}

} // namespace

// ── Private ─────────────────────────────────────────────────────────────────

class OcrPipeline::Private {
public:
    std::shared_ptr<IOcrEngine> primary;
    std::shared_ptr<IOcrEngine> secondary;
    OcrStrategy strategy = OcrStrategy::PrimaryOnly;
    OcrPreprocessOptions preprocessOpts;
    
    // OcrPreprocessor is stateless; OcrPipeline::run may be called concurrently. 
    // If preprocessor ever becomes stateful, add explicit synchronization.
    OcrPreprocessor preprocessor;
};

// ── Construction ────────────────────────────────────────────────────────────

OcrPipeline::OcrPipeline(std::shared_ptr<IOcrEngine> primary,
                         std::shared_ptr<IOcrEngine> secondary)
    : d(std::make_unique<Private>())
{
    d->primary   = std::move(primary);
    d->secondary = std::move(secondary);
}

OcrPipeline::~OcrPipeline() = default;

void OcrPipeline::setStrategy(OcrStrategy strategy)
{
    d->strategy = strategy;
}

OcrStrategy OcrPipeline::strategy() const
{
    return d->strategy;
}

void OcrPipeline::setPreprocessing(const OcrPreprocessOptions &opts)
{
    d->preprocessOpts = opts;
}

OcrPreprocessOptions OcrPipeline::preprocessingOptions() const
{
    return d->preprocessOpts;
}

// ── IoU ─────────────────────────────────────────────────────────────────────

double OcrPipeline::computeIoU(const QRectF &a, const QRectF &b)
{
    QRectF inter = a.intersected(b);
    if (inter.isEmpty()) return 0.0;

    double interArea = inter.width() * inter.height();
    double unionArea = a.width() * a.height() + b.width() * b.height() - interArea;
    if (unionArea <= 0.0) return 0.0;

    return interArea / unionArea;
}

// ── ROVER Merge ─────────────────────────────────────────────────────────────

QList<MergedOcrWord> OcrPipeline::roverMerge(
    const QList<OcrResult> &primary,   const QString &primaryName,
    const QList<OcrResult> &secondary, const QString &secondaryName,
    double iouThreshold)
{
    QList<MergedOcrWord> merged;
    QVector<bool> secondaryUsed(secondary.size(), false);

    for (const auto &pw : primary) {
        MergedOcrWord mw;
        mw.text         = pw.text;
        mw.boundingBox  = pw.boundingBox;
        mw.confidence   = pw.confidence;
        mw.sourceEngine = primaryName;

        // Find best-matching secondary word by IoU
        int bestIdx = -1;
        double bestIoU = 0.0;
        for (int j = 0; j < secondary.size(); ++j) {
            if (secondaryUsed[j]) continue;
            double iou = computeIoU(pw.boundingBox, secondary[j].boundingBox);
            if (iou > bestIoU) {
                bestIoU = iou;
                bestIdx = j;
            }
        }

        if (bestIdx >= 0 && bestIoU >= iouThreshold) {
            secondaryUsed[bestIdx] = true;
            const auto &sw = secondary[bestIdx];

            // Pick the word with higher confidence
            if (sw.confidence > pw.confidence) {
                mw.text         = sw.text;
                mw.boundingBox  = sw.boundingBox;
                mw.confidence   = sw.confidence;
                mw.sourceEngine = secondaryName;
            }
            // Mark as ROVER-resolved regardless of winner
            mw.sourceEngine = "ROVER";
        }

        merged.append(mw);
    }

    // Append unmatched secondary words (extra detections)
    for (int j = 0; j < secondary.size(); ++j) {
        if (secondaryUsed[j]) continue;
        MergedOcrWord mw;
        mw.text         = secondary[j].text;
        mw.boundingBox  = secondary[j].boundingBox;
        mw.confidence   = secondary[j].confidence;
        mw.sourceEngine = secondaryName;
        merged.append(mw);
    }

    return merged;
}

// ── Pipeline Run ────────────────────────────────────────────────────────────

QList<MergedOcrWord> OcrPipeline::run(const QImage &pageImage)
{
    if (pageImage.isNull() || !d->primary) return {};

    // 1. Preprocess
    PreprocessedImage pp = d->preprocessor.process(pageImage, d->preprocessOpts);
    const QImage &img = pp.image.isNull() ? pageImage : pp.image;

    // 2. Run primary engine
    QList<OcrResult> primaryResults = d->primary->processImage(img);

    // Map results back to original coordinates via inverse transform
    auto mapToOriginal = [&](QList<OcrResult> &results) {
        for (auto &r : results) {
            r.boundingBox = pp.inverseTransform.mapRect(r.boundingBox);
        }
    };
    mapToOriginal(primaryResults);

    // 3. Strategy routing
    switch (d->strategy) {

    case OcrStrategy::PrimaryOnly:
        return toPrimaryOnly(primaryResults);

    case OcrStrategy::ConfidenceWeighted: {
        if (!d->secondary) {
            return toPrimaryOnly(primaryResults);
        }

        // Compute mean confidence
        double totalConf = 0;
        for (const auto &r : primaryResults) totalConf += r.confidence;
        double meanConf = primaryResults.isEmpty() ? 100.0 : totalConf / primaryResults.size();

        if (meanConf >= 70.0) {
            return toPrimaryOnly(primaryResults);
        }

        // Low confidence — run secondary, then ROVER merge
        QList<OcrResult> secondaryResults = d->secondary->processImage(img);
        mapToOriginal(secondaryResults);
        return roverMerge(primaryResults, "Tesseract", secondaryResults, "RapidOCR");
    }

    case OcrStrategy::RoverVote: {
        if (!d->secondary) {
            return toPrimaryOnly(primaryResults);
        }
        QList<OcrResult> secondaryResults = d->secondary->processImage(img);
        mapToOriginal(secondaryResults);
        return roverMerge(primaryResults, "Tesseract", secondaryResults, "RapidOCR");
    }

    } // switch

    return {};
}
