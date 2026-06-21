// SPDX-License-Identifier: Apache-2.0
// src/engines/scheduling/PipelineStage.h
#pragma once
#include "LaneScheduler.h"
#include <functional>
#include <QSemaphore>
#include <QList>
#include <QtConcurrent>
#include <QFuture>
#include <QDebug>

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

    explicit CrossPagePipeline(LaneScheduler& scheduler, int backpressure = -1)
        : m_scheduler(scheduler)
        , m_backpressure(backpressure < 1 ? qMax(1, QThread::idealThreadCount() / 2) : backpressure)
    {}

    void run(int pageCount,
             Stage1Fn s1, Stage2Fn s2, Stage3Fn s3,
             ResultHandler handler)
    {
        // AR-6 D3: backpressure MUST be < the CPU pool size. A CPU-lane task
        // must never synchronously fan out to and block on the GPU lane; here
        // stages are chained as futures-driving-futures (.then().unwrap()), so
        // no pool thread ever blocks waiting on another lane. But the invariant
        // still matters: if more pages are admitted than the pool can run, and
        // any stage continuation were to block, the pool would starve. We clamp
        // backpressure to (poolSize - 1) so admitted work always has a thread.
        const int poolSize = qMax(1, m_scheduler.cpuPoolMaxThreads());
        int effectiveBp = m_backpressure;
        if (effectiveBp >= poolSize) {
            qWarning() << "CrossPagePipeline: backpressure" << effectiveBp
                       << ">= CPU pool size" << poolSize
                       << "— clamping to" << qMax(1, poolSize - 1)
                       << "to guarantee forward progress";
            effectiveBp = qMax(1, poolSize - 1);
        }

        // Backpressure semaphore: limits how many pages stage1 can run
        // ahead of stage3, preventing memory accumulation on large documents.
        QSemaphore bpSemaphore(effectiveBp);

        QList<QFuture<ScheduledValue<S3Out>>> resultFutures;
        resultFutures.reserve(pageCount);

        for (int p = 0; p < pageCount; ++p) {
            bpSemaphore.acquire(); // block the PRODUCER (this loop), never a pool thread

            // Stage 1 — CPU (data fetch / layout)
            auto f1 = m_scheduler.submit<S1Out>(
                SchedulerOptions{Lane::CPU, TaskPriority::Normal, p, "stage1"},
                [s1, p]() -> S1Out { return s1(p); });

            // Stage 2 — GPU (inference). Continuation-chained, NOT blocking.
            // The stage1 continuation RETURNS the GPU future; .unwrap() flattens
            // QFuture<QFuture<...>> into a single QFuture that completes when the
            // GPU task completes. No waitForFinished() on any worker thread — the
            // previous design blocked a CPU-pool thread here, which deadlocked
            // the pool at setLaneCapacity(CPU,2) + backpressure 4 (the GPU result
            // a blocked thread awaited could need a thread that was itself
            // blocked). Driving futures with futures removes the circular wait.
            auto f2 = f1.then(
                [this, s2, p](ScheduledValue<S1Out> sv) -> QFuture<ScheduledValue<S2Out>> {
                    if (!sv.ok) {
                        return QtFuture::makeReadyValueFuture(
                            ScheduledValue<S2Out>::failure(sv.error));
                    }
                    return m_scheduler.submit<S2Out>(
                        SchedulerOptions{Lane::GPU, TaskPriority::Normal, p, "stage2"},
                        [s2, p, v = sv.value]() -> S2Out { return s2(p, v); });
                }).unwrap();

            // Stage 3 — CPU (fusion / output). Chained off the GPU future's
            // completion; runs on whatever thread reports stage2's result. No
            // blocking wait — the bpSemaphore is released here so the producer
            // loop can admit the next page.
            auto f3 = f2.then([s3, handler, p, &bpSemaphore](ScheduledValue<S2Out> sv)
                               -> ScheduledValue<S3Out> {
                ScheduledValue<S3Out> result;
                if (!sv.ok) {
                    result = ScheduledValue<S3Out>::failure(sv.error);
                } else {
                    try {
                        result = ScheduledValue<S3Out>::success(s3(p, sv.value));
                    } catch (const std::exception& e) {
                        SchedulerError err;
                        err.code = SchedulerErrorCode::WorkerCrashed;
                        err.pageIndex = p;
                        err.message = QString::fromStdString(e.what());
                        result = ScheduledValue<S3Out>::failure(err);
                    }
                    if (result.ok) handler(p, result.value);
                }
                bpSemaphore.release();
                return result;
            });

            resultFutures.append(std::move(f3));
        }

        // Wait for all pages to complete before returning. This blocks the
        // CALLER (which already runs on its own QtConcurrent thread, not a
        // pipeline pool thread), so it cannot starve the pool.
        for (auto& f : resultFutures) {
            f.waitForFinished();
        }
    }

private:
    LaneScheduler& m_scheduler;
    int m_backpressure;
};

} // namespace gp
