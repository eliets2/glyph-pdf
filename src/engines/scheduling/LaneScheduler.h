// SPDX-License-Identifier: Apache-2.0
// src/engines/scheduling/LaneScheduler.h
#pragma once
#include "ILaneScheduler.h"
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSemaphore>
#include <QThreadPool>
#include <QAtomicInt>
#include <QMap>
#include <QList>
#include <QPromise>
#include <QtConcurrent>
#include <functional>
#include <queue>
#include <memory>
#include <stdexcept>

namespace gp {

// OrderedResultQueue delivers futures in page-index order,
// emitting a sentinel ScheduledValue{ok=false, error.code=Timeout}
// for any gap in the expected [0..N-1] range.
template<typename T>
class OrderedResultQueue {
public:
    explicit OrderedResultQueue(int expectedCount);

    void submit(int pageIndex, QFuture<ScheduledValue<T>> future);

    // Returns results in pageIndex order (blocks until each is ready).
    // Missing indices emit a sentinel error result.
    QList<ScheduledValue<T>> collectOrdered();

private:
    int m_expectedCount;
    QMap<int, QFuture<ScheduledValue<T>>> m_futures;
};

class LaneScheduler : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LaneScheduler)

public:
    // GPU warm worker: persists across tasks; holds warm ONNX session or
    // any GPU context. Never spawned per-task (anti-spawn-per-page rule).
    // CPU elastic pool: own QThreadPool (not global) sized to idealThreadCount.
    explicit LaneScheduler(int gpuCapacity = 2,
                           int cpuCapacity = QThread::idealThreadCount(),
                           QObject* parent = nullptr);
    ~LaneScheduler() override;

    // Template submit — the primary public API.
    // Returns a SchedulerResult<T> (QFuture<ScheduledValue<T>>).
    // Callers cannot bypass the cancellation token.
    template<typename T>
    SchedulerResult<T> submit(SchedulerOptions opts, std::function<T()> work);

    // ILaneScheduler-compatible convenience for int tasks
    SchedulerResult<int> submitInt(SchedulerOptions opts,
                                   std::function<int()> work);

    void cancelAll();
    int inFlightCount(Lane lane) const;
    void setLaneCapacity(Lane lane, int capacity);

    // Current CPU pool size — used by CrossPagePipeline to clamp backpressure
    // below the pool size so admitted work is always guaranteed a thread.
    int cpuPoolMaxThreads() const { return m_cpuPool.maxThreadCount(); }

    // Stop the GPU warm worker cleanly (call before destructor if needed).
    void shutdown();

    // ── V-01 pin seam (AUDIT-VERIFICATION-2026-09-25) — inert in production ────
    // A submit racing shutdown() could orphan a GPU task forever: the submit
    // pushed into m_gpuQueue without re-checking m_gpuStopping under the same
    // mutex the worker drains under. The pin needs the exact interleave forced
    // deterministically, so submit() invokes these hooks (a) after it passes
    // the stopping pre-check and before it blocks in the GPU semaphore
    // acquire, and (b) after the acquire returns and before the enqueue
    // decision. Both run on the CALLER's thread and must not take m_gpuMutex
    // (shutdown() does). Never set outside TestLaneScheduler.
    struct GpuSubmitGateForTesting {
        std::function<void()> beforeAcquire; // after the stopping pre-check
        std::function<void()> afterAcquire;  // after the semaphore is held
    };
    void setGpuSubmitGateForTesting(GpuSubmitGateForTesting gate);

private:
    // GPU lane internals
    struct GpuTask {
        std::function<void()> run; // wraps the QPromise<ScheduledValue<T>> logic
    };

    void gpuWorkerLoop();
    // (enqueueGpu was folded into submit's check+push critical section —
    // V-01: the stopping re-check and the queue push must share ONE mutex
    // acquisition, which a separate locked helper would break apart.)

    QThread* m_gpuThread = nullptr;
    QMutex m_gpuMutex;
    QWaitCondition m_gpuCond;
    std::queue<GpuTask> m_gpuQueue;
    QSemaphore m_gpuSemaphore;
    QAtomicInt m_gpuInFlight{0};
    bool m_gpuStopping = false;

    // CPU lane internals
    QThreadPool m_cpuPool;
    QAtomicInt m_cpuInFlight{0};

    // Cancellation
    QAtomicInt m_cancelToken{0};

    // V-01 pin seam (see setGpuSubmitGateForTesting).
    GpuSubmitGateForTesting m_gpuSubmitGateForTesting;
};

// ---------------------------------------------------------------------------
// OrderedResultQueue implementation (header-only template)
// ---------------------------------------------------------------------------

template<typename T>
OrderedResultQueue<T>::OrderedResultQueue(int expectedCount)
    : m_expectedCount(expectedCount) {}

template<typename T>
void OrderedResultQueue<T>::submit(int pageIndex, QFuture<ScheduledValue<T>> future) {
    m_futures.insert(pageIndex, std::move(future));
}

template<typename T>
QList<ScheduledValue<T>> OrderedResultQueue<T>::collectOrdered() {
    QList<ScheduledValue<T>> results;
    for (int i = 0; i < m_expectedCount; ++i) {
        if (m_futures.contains(i)) {
            m_futures[i].waitForFinished();
            results.append(m_futures[i].result());
        } else {
            SchedulerError err;
            err.code = SchedulerErrorCode::Timeout;
            err.pageIndex = i;
            err.message = QString("Page %1 not submitted to queue").arg(i);
            results.append(ScheduledValue<T>::failure(err));
        }
    }
    return results;
}

// ---------------------------------------------------------------------------
// LaneScheduler::submit template implementation
// ---------------------------------------------------------------------------

template<typename T>
SchedulerResult<T> LaneScheduler::submit(SchedulerOptions opts,
                                          std::function<T()> work) {
    auto promise = std::make_shared<QPromise<ScheduledValue<T>>>();
    SchedulerResult<T> future = promise->future();
    promise->start();

    const int cancelToken = m_cancelToken.loadRelaxed();
    const Lane lane = opts.lane;

    auto runWork = [this, promise, work = std::move(work),
                    cancelToken, lane]() mutable {
        if (m_cancelToken.loadRelaxed() != cancelToken) {
            SchedulerError err;
            err.code = SchedulerErrorCode::Cancelled;
            err.message = "Task cancelled before execution";
            promise->addResult(ScheduledValue<T>::failure(err));
            promise->finish();
            if (lane == Lane::GPU) {
                m_gpuInFlight.fetchAndSubOrdered(1);
                m_gpuSemaphore.release();
            } else {
                m_cpuInFlight.fetchAndSubOrdered(1);
            }
            return;
        }
        try {
            T result = work();
            promise->addResult(ScheduledValue<T>::success(std::move(result)));
        } catch (const std::exception& e) {
            SchedulerError err;
            err.code = SchedulerErrorCode::WorkerCrashed;
            err.message = QString::fromStdString(e.what());
            promise->addResult(ScheduledValue<T>::failure(err));
        } catch (...) {
            // V-02 fix (AUDIT-VERIFICATION-2026-09-25): a throwable outside
            // std::exception used to escape this boundary into the
            // QThreadPool / GPU loop uncaught → std::terminate() killed the
            // process (and, on the GPU lane, leaked the semaphore slot, since
            // the release below never ran). The worker boundary converts ANY
            // escaping throwable into an honest reported failure — the
            // process survives and the accounting below is released.
            SchedulerError err;
            err.code = SchedulerErrorCode::WorkerCrashed;
            err.message = QStringLiteral("worker crashed with a non-standard exception");
            promise->addResult(ScheduledValue<T>::failure(err));
        }
        promise->finish();
        if (lane == Lane::GPU) {
            m_gpuInFlight.fetchAndSubOrdered(1);
            m_gpuSemaphore.release();
        } else {
            m_cpuInFlight.fetchAndSubOrdered(1);
        }
    };

    if (lane == Lane::GPU) {
        // Acquire blocks if gpuCapacity tasks already in-flight.
        // Anti-spawn-per-page: the GPU thread is persistent; only the task
        // payload is enqueued, never a new thread.
        if (!m_gpuSemaphore.tryAcquire(1, 0)) {
            // Stopped or at capacity; check stopping flag first:
            {
                QMutexLocker lk(&m_gpuMutex);
                if (m_gpuStopping) {
                    SchedulerError err;
                    err.code = SchedulerErrorCode::Cancelled;
                    err.message = "Scheduler is shutting down";
                    promise->addResult(ScheduledValue<T>::failure(err));
                    promise->finish();
                    return future;
                }
            }
            // V-01 pin seam: the caller has committed to the blocking
            // acquire past the stopping pre-check — the exact window the
            // audit names (stopping may now be set concurrently).
            if (m_gpuSubmitGateForTesting.beforeAcquire)
                m_gpuSubmitGateForTesting.beforeAcquire();
            m_gpuSemaphore.acquire();  // blocking path only when capacity is full but not stopped
        }
        // V-01 pin seam: the semaphore is held; the enqueue decision follows.
        if (m_gpuSubmitGateForTesting.afterAcquire)
            m_gpuSubmitGateForTesting.afterAcquire();
        // V-01 fix (AUDIT-VERIFICATION-2026-09-25): re-check m_gpuStopping
        // under the SAME mutex the GPU worker drains under. The pre-check
        // above cannot cover the windows where stopping flips after it ran —
        // a tryAcquire succeeding during the drain (a completing task
        // released its slot), or this submit waking from the blocking
        // acquire after the drain already closed. Serialized by the mutex,
        // exactly one of two outcomes holds: we see stopping and refuse
        // honestly below (future finishes with Cancelled, semaphore slot
        // released, in-flight never incremented), or the worker's drain
        // check happens after our push and sees the task. A task can never
        // again land in a queue no thread will read.
        {
            QMutexLocker lk(&m_gpuMutex);
            if (m_gpuStopping) {
                m_gpuSemaphore.release();
                SchedulerError err;
                err.code = SchedulerErrorCode::Cancelled;
                err.message = "Scheduler is shutting down";
                promise->addResult(ScheduledValue<T>::failure(err));
                promise->finish();
                return future;
            }
            // Push under the same lock acquisition as the check (inlined
            // enqueueGpu — its own lock would reopen the gap), and bump the
            // in-flight counter only for tasks that actually entered the
            // queue, so every counter/semaphore transition stays balanced.
            m_gpuInFlight.fetchAndAddOrdered(1);
            m_gpuQueue.push(GpuTask{ std::move(runWork) });
            m_gpuCond.wakeOne();
        }
    } else {
        m_cpuInFlight.fetchAndAddOrdered(1);
        // Use QThreadPool::start on own pool (never global pool).
        // We manage the promise ourselves so the QtConcurrent::run QFuture
        // return value is not needed — use start() to avoid [[nodiscard]] warning.
        m_cpuPool.start([runWork = std::move(runWork)]() mutable {
            runWork();
        });
    }

    return future;
}

} // namespace gp
