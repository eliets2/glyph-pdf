// src/engines/scheduling/LaneScheduler.cpp
#include "LaneScheduler.h"
#include <QtConcurrent>

namespace gp {

LaneScheduler::LaneScheduler(int gpuCapacity, int cpuCapacity, QObject* parent)
    : QObject(parent)
    , m_gpuSemaphore(gpuCapacity)
{
    // CPU pool — own instance, never the global QThreadPool
    m_cpuPool.setMaxThreadCount(cpuCapacity);
    m_cpuPool.setExpiryTimeout(-1); // keep threads alive

    // GPU warm worker — single persistent thread (anti-spawn-per-page rule)
    m_gpuThread = new QThread(this);
    m_gpuThread->setObjectName("GlyphPDF-GPU-Lane");
    m_gpuThread->start();

    // Post the worker loop to the GPU thread via a transient QObject relay.
    // The relay object lives on the GPU thread and deletes itself when the
    // loop exits.
    auto* relay = new QObject();
    relay->moveToThread(m_gpuThread);
    QMetaObject::invokeMethod(relay, [this, relay]() {
        gpuWorkerLoop();
        relay->deleteLater();
    }, Qt::QueuedConnection);
}

LaneScheduler::~LaneScheduler() {
    shutdown();
}

void LaneScheduler::shutdown() {
    {
        QMutexLocker lock(&m_gpuMutex);
        if (m_gpuStopping) return;
        m_gpuStopping = true;
        m_gpuCond.wakeAll();
    }
    if (m_gpuThread) {
        m_gpuThread->quit();
        m_gpuThread->wait(5000);
    }
    m_cpuPool.waitForDone(5000);
}

void LaneScheduler::gpuWorkerLoop() {
    while (true) {
        GpuTask task;
        {
            QMutexLocker lock(&m_gpuMutex);
            while (m_gpuQueue.empty() && !m_gpuStopping) {
                m_gpuCond.wait(&m_gpuMutex);
            }
            if (m_gpuStopping && m_gpuQueue.empty()) break;
            task = std::move(m_gpuQueue.front());
            m_gpuQueue.pop();
        }
        task.run();
    }
}

void LaneScheduler::enqueueGpu(GpuTask task) {
    QMutexLocker lock(&m_gpuMutex);
    m_gpuQueue.push(std::move(task));
    m_gpuCond.wakeOne();
}

SchedulerResult<int> LaneScheduler::submitInt(SchedulerOptions opts,
                                               std::function<int()> work) {
    return submit<int>(std::move(opts), std::move(work));
}

void LaneScheduler::cancelAll() {
    m_cancelToken.fetchAndAddOrdered(1);
}

int LaneScheduler::inFlightCount(Lane lane) const {
    if (lane == Lane::GPU) return m_gpuInFlight.loadRelaxed();
    return m_cpuInFlight.loadRelaxed();
}

void LaneScheduler::setLaneCapacity(Lane lane, int capacity) {
    if (lane == Lane::CPU) {
        m_cpuPool.setMaxThreadCount(capacity);
    }
    // GPU capacity is fixed at construction via QSemaphore;
    // runtime resize would require draining the queue first and is not supported.
}

} // namespace gp
