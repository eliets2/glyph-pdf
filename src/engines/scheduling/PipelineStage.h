// src/engines/scheduling/PipelineStage.h
#pragma once
#include "LaneScheduler.h"
#include <functional>
#include <QSemaphore>
#include <QList>

namespace gp {

// CrossPagePipeline: overlaps three pipeline stages across consecutive pages.
// Stage execution: stage1(P+1) || stage2(P) || stage3(P-1)
// Backpressure via QSemaphore when output stage lags.
//
// Usage:
//   CrossPagePipeline<int,int,int> pipeline(scheduler, 4 /*backpressure*/);
//   pipeline.run(pageCount, stage1fn, stage2fn, stage3fn,
//                [](int pageIdx, int result){ /* handle result */ });
template<typename S1Out, typename S2Out, typename S3Out>
class CrossPagePipeline {
public:
    using Stage1Fn = std::function<S1Out(int pageIndex)>;
    using Stage2Fn = std::function<S2Out(int pageIndex, S1Out)>;
    using Stage3Fn = std::function<S3Out(int pageIndex, S2Out)>;
    using ResultHandler = std::function<void(int pageIndex, S3Out)>;

    explicit CrossPagePipeline(LaneScheduler& scheduler, int backpressure = 4)
        : m_scheduler(scheduler)
        , m_backpressure(backpressure)
    {}

    void run(int pageCount,
             Stage1Fn s1, Stage2Fn s2, Stage3Fn s3,
             ResultHandler handler)
    {
        // Backpressure semaphore: limits how many pages stage1 can run
        // ahead of stage3, preventing memory accumulation on large documents.
        QSemaphore bpSemaphore(m_backpressure);

        QList<QFuture<ScheduledValue<S3Out>>> resultFutures;
        resultFutures.reserve(pageCount);

        for (int p = 0; p < pageCount; ++p) {
            bpSemaphore.acquire(); // block if too far ahead

            // Stage 1 — CPU (data fetch / layout)
            auto f1 = m_scheduler.submit<S1Out>(
                SchedulerOptions{Lane::CPU, TaskPriority::Normal, p, "stage1"},
                [s1, p]() -> S1Out { return s1(p); });

            // Stage 2 — GPU (inference) — chained from stage1 result
            // waitForFinished() used rather than .then() for clarity and
            // guaranteed ordering on all Qt 6 versions.
            auto f2 = f1.then([this, s2, p](ScheduledValue<S1Out> sv) -> ScheduledValue<S2Out> {
                if (!sv.ok) return ScheduledValue<S2Out>::failure(sv.error);
                auto f = m_scheduler.submit<S2Out>(
                    SchedulerOptions{Lane::GPU, TaskPriority::Normal, p, "stage2"},
                    [s2, p, v = sv.value]() -> S2Out { return s2(p, v); });
                f.waitForFinished();
                return f.result();
            });

            // Stage 3 — CPU (fusion / output)
            // Captures bpSemaphore by reference; safe because bpSemaphore
            // outlives all futures (defined above this loop).
            auto f3 = f2.then([this, s3, handler, p, &bpSemaphore](ScheduledValue<S2Out> sv)
                               -> ScheduledValue<S3Out> {
                ScheduledValue<S3Out> result;
                if (!sv.ok) {
                    result = ScheduledValue<S3Out>::failure(sv.error);
                } else {
                    auto f = m_scheduler.submit<S3Out>(
                        SchedulerOptions{Lane::CPU, TaskPriority::Normal, p, "stage3"},
                        [s3, p, v = sv.value]() -> S3Out { return s3(p, v); });
                    f.waitForFinished();
                    result = f.result();
                    if (result.ok) handler(p, result.value);
                }
                bpSemaphore.release();
                return result;
            });

            resultFutures.append(std::move(f3));
        }

        // Wait for all pages to complete before returning
        for (auto& f : resultFutures) {
            f.waitForFinished();
        }
    }

private:
    LaneScheduler& m_scheduler;
    int m_backpressure;
};

} // namespace gp
