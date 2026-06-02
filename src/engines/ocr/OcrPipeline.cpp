#include "engines/ocr/OcrPipeline.h"
#include "engines/ocr/OcrDjotMapper.h"
#include "engines/ocr/LayoutEnsemble.h"
#include "engines/scheduling/PipelineStage.h"

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QtConcurrent>
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

    // Cross-page document OCR support (optional, set via setters)
    LayoutEnsemble   *layoutEnsemble = nullptr;   // non-owning
    gp::LaneScheduler *scheduler     = nullptr;   // non-owning
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

void OcrPipeline::setLayoutEnsemble(LayoutEnsemble *ensemble)
{
    d->layoutEnsemble = ensemble;
}

void OcrPipeline::setScheduler(gp::LaneScheduler *scheduler)
{
    d->scheduler = scheduler;
}

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

// ── Cross-page document OCR ──────────────────────────────────────────────────

QFuture<QList<PageOcrResult>> OcrPipeline::recognizeDocument(const QList<QImage> &pageImages)
{
    // Capture shared_ptr copies for async lifetime safety
    auto primaryEngine   = d->primary;
    auto secondaryEngine = d->secondary;
    OcrStrategy strategy = d->strategy;
    OcrPreprocessOptions preprocessOpts = d->preprocessOpts;
    LayoutEnsemble   *layoutEns = d->layoutEnsemble;
    gp::LaneScheduler *sched    = d->scheduler;

    const int pageCount = pageImages.size();

    // Sequential fallback when no scheduler is configured
    if (!sched) {
        return QtConcurrent::run([=]() -> QList<PageOcrResult> {
            QList<PageOcrResult> results;
            results.reserve(pageCount);
            for (int p = 0; p < pageCount; ++p) {
                PageOcrResult pr;
                pr.pageIndex = p;
                if (layoutEns)
                    pr.layoutRegions = layoutEns->detect(pageImages[p]);

                OcrPipeline pipe(primaryEngine, secondaryEngine);
                pipe.setStrategy(strategy);
                pipe.setPreprocessing(preprocessOpts);
                pr.words   = pipe.run(pageImages[p]);
                pr.success = true;
                results.append(pr);
            }
            return results;
        });
    }

    // Parallel cross-page pipeline via LaneScheduler CrossPagePipeline.
    //
    // Stage1 (CPU): fetch page image + run LayoutEnsemble (GPU warm worker
    //   is used inside PpDocLayoutDetector via its own session; the CPU stage
    //   here just dispatches to the ensemble's internal GPU work).
    // Stage2 (GPU): per-region OCR fanout (ROVER merge on top of layout regions).
    // Stage3 (CPU): fusion into PageOcrResult + OrderedResultQueue delivery.
    //
    // We run the whole CrossPagePipeline in a single QtConcurrent thread so the
    // calling thread is not blocked, and return a QFuture that completes when
    // all pages are done.

    return QtConcurrent::run([=, pageImages = pageImages]() -> QList<PageOcrResult> {
        QList<PageOcrResult> orderedResults(pageCount);

        // Capture stage1 layout output per-page so we don't re-detect after pipeline.
        // Protected by a mutex (stage1 runs on multiple CPU tasks concurrently).
        QMutex layoutMutex;
        QVector<QList<LayoutRegion>> capturedLayouts(pageCount);

        // Stage1: layout detection (GPU work dispatched inside PpDocLayoutDetector)
        auto stage1 = [=, &layoutMutex, &capturedLayouts](int p) -> QList<LayoutRegion> {
            QList<LayoutRegion> regions;
            if (layoutEns)
                regions = layoutEns->detect(pageImages[p]);
            {
                QMutexLocker lk(&layoutMutex);
                capturedLayouts[p] = regions;
            }
            return regions;
        };

        // Stage2: per-region OCR fanout
        auto stage2 = [=](int p, QList<LayoutRegion> regions) -> QList<MergedOcrWord> {
            const QImage &img = pageImages[p];
            OcrPipeline pipe(primaryEngine, secondaryEngine);
            pipe.setStrategy(strategy);
            pipe.setPreprocessing(preprocessOpts);

            if (regions.isEmpty()) {
                // No layout — OCR the whole page
                return pipe.run(img);
            }

            // Per-region OCR: crop each region, run OCR, map bbox back
            QList<MergedOcrWord> allWords;
            for (const auto &region : regions) {
                QRect cropRect = region.bbox.toRect()
                                     .intersected(QRect(0, 0, img.width(), img.height()));
                if (cropRect.isEmpty()) continue;

                QImage crop = img.copy(cropRect);
                QList<MergedOcrWord> regionWords = pipe.run(crop);

                // Translate bbox from crop-local to page coordinates
                for (auto &w : regionWords) {
                    w.boundingBox.translate(cropRect.left(), cropRect.top());
                    allWords.append(w);
                }
            }
            return allWords;
        };

        // Stage3: assemble PageOcrResult (runs CPU side; safe to read capturedLayouts)
        auto stage3 = [=, &capturedLayouts, &layoutMutex](int p,
                                                            QList<MergedOcrWord> words)
                       -> PageOcrResult {
            PageOcrResult pr;
            pr.pageIndex = p;
            pr.words     = std::move(words);
            {
                QMutexLocker lk(&layoutMutex);
                if (p < capturedLayouts.size())
                    pr.layoutRegions = capturedLayouts[p];
            }
            pr.success = true;
            return pr;
        };

        // Result handler: store in page-order array
        auto handler = [&orderedResults](int p, PageOcrResult result) {
            if (p >= 0 && p < orderedResults.size())
                orderedResults[p] = std::move(result);
        };

        // Run the 3-stage cross-page pipeline with backpressure = 4 pages
        gp::CrossPagePipeline<QList<LayoutRegion>, QList<MergedOcrWord>, PageOcrResult>
            pipeline(*sched, 4);
        pipeline.run(pageCount, stage1, stage2, stage3, handler);

        return orderedResults;
    });
}

// ── recognizeDocumentAsDjot ───────────────────────────────────────────────────

QFuture<docmodel::SemanticDocument> OcrPipeline::recognizeDocumentAsDjot(
    const QString& pdfPath,
    const QList<QImage>& pageImages)
{
    // Capture what we need for the async chain.
    // recognizeDocument() returns a QFuture; we chain it via QtConcurrent::run
    // so that the mapping step executes on the CPU lane (thread pool).
    //
    // Pattern: launch an outer QtConcurrent::run that:
    //   1. Calls recognizeDocument() synchronously (waits on future result).
    //   2. Maps the PageOcrResult list via OcrDjotMapper.
    //   3. Returns SemanticDocument.
    //
    // This keeps OcrDjotMapper on a CPU worker thread (not the calling/UI thread),
    // satisfying the "CPU-bound mapping; use CPU lane" constraint.

    // Snapshot pipeline members for async capture
    auto primaryEngine   = d->primary;
    auto secondaryEngine = d->secondary;
    OcrStrategy strat    = d->strategy;
    OcrPreprocessOptions ppOpts = d->preprocessOpts;
    LayoutEnsemble*   layoutEns = d->layoutEnsemble;
    gp::LaneScheduler* sched    = d->scheduler;

    return QtConcurrent::run([=, pageImages = pageImages,
                               pdfPath = pdfPath]() -> docmodel::SemanticDocument {
        // Step 1: run the recognizeDocument pipeline (synchronous inside this worker)
        OcrPipeline pipe(primaryEngine, secondaryEngine);
        pipe.setStrategy(strat);
        pipe.setPreprocessing(ppOpts);
        if (layoutEns) pipe.setLayoutEnsemble(layoutEns);
        if (sched)     pipe.setScheduler(sched);

        QList<PageOcrResult> pageResults = pipe.recognizeDocument(pageImages).result();

        // Step 2: map via OcrDjotMapper (stateless, CPU-bound)
        OcrDjotMapper mapper;
        return mapper.fromOcrResults(pageResults, pdfPath);
    });
}

