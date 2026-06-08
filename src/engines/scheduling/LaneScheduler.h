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

    // Stop the GPU warm worker cleanly (call before destructor if needed).
    void shutdown();

private:
    // GPU lane internals
    struct GpuTask {
        std::function<void()> run; // wraps the QPromise<ScheduledValue<T>> logic
    };

    void gpuWorkerLoop();
    void enqueueGpu(GpuTask task);

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
            m_gpuSemaphore.acquire();  // blocking path only when capacity is full but not stopped
        }
        m_gpuInFlight.fetchAndAddOrdered(1);
        enqueueGpu(GpuTask{ std::move(runWork) });
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
