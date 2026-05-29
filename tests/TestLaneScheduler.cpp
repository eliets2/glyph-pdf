#include <QtTest>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QMutex>
#include <QSet>
#include "engines/scheduling/LaneScheduler.h"
#include "engines/scheduling/PipelineStage.h"

using namespace gp;

class TestLaneScheduler : public QObject {
    Q_OBJECT

private slots:

    void testGpuLaneSerializesExecution() {
        LaneScheduler sched(/*gpuCapacity=*/2, /*cpuCapacity=*/4);
        QAtomicInt inFlight{0};
        QAtomicInt maxSeen{0};

        const int taskCount = 10;
        QList<SchedulerResult<int>> futures;
        for (int i = 0; i < taskCount; ++i) {
            futures.append(sched.submit<int>(
                {Lane::GPU, TaskPriority::Normal, i, "gpu-test"},
                [&inFlight, &maxSeen]() -> int {
                    int cur = inFlight.fetchAndAddOrdered(1) + 1;
                    // Update max atomically
                    int old = maxSeen.loadRelaxed();
                    while (cur > old && !maxSeen.testAndSetOrdered(old, cur))
                        old = maxSeen.loadRelaxed();
                    QThread::msleep(10);
                    inFlight.fetchAndSubOrdered(1);
                    return cur;
                }));
        }
        for (auto& f : futures) f.waitForFinished();
        QVERIFY2(maxSeen.loadRelaxed() <= 2,
            qPrintable(QString("Max GPU in-flight was %1, expected <= 2")
                       .arg(maxSeen.loadRelaxed())));
    }

    void testCpuLaneParallelism() {
        int ideal = QThread::idealThreadCount();
        if (ideal < 2) QSKIP("Need >= 2 logical CPUs");

        LaneScheduler sched(2, ideal);
        QAtomicInt inFlight{0};
        QAtomicInt maxSeen{0};

        const int taskCount = ideal * 4;
        QList<SchedulerResult<int>> futures;
        for (int i = 0; i < taskCount; ++i) {
            futures.append(sched.submit<int>(
                {Lane::CPU, TaskPriority::Normal, i, "cpu-test"},
                [&inFlight, &maxSeen]() -> int {
                    int cur = inFlight.fetchAndAddOrdered(1) + 1;
                    int old = maxSeen.loadRelaxed();
                    while (cur > old && !maxSeen.testAndSetOrdered(old, cur))
                        old = maxSeen.loadRelaxed();
                    QThread::msleep(30);
                    inFlight.fetchAndSubOrdered(1);
                    return cur;
                }));
        }
        for (auto& f : futures) f.waitForFinished();
        QVERIFY2(maxSeen.loadRelaxed() >= ideal - 1,
            qPrintable(QString("Max CPU in-flight was %1, expected >= %2")
                       .arg(maxSeen.loadRelaxed()).arg(ideal - 1)));
    }

    void testWarmWorkerReuse() {
        LaneScheduler sched(2, 4);
        const int taskCount = 20;
        QMutex mutex;
        QSet<Qt::HANDLE> threadIds;

        QList<SchedulerResult<int>> futures;
        for (int i = 0; i < taskCount; ++i) {
            futures.append(sched.submit<int>(
                {Lane::GPU, TaskPriority::Normal, i, "warm-test"},
                [&mutex, &threadIds]() -> int {
                    QMutexLocker lock(&mutex);
                    threadIds.insert(QThread::currentThreadId());
                    return 1;
                }));
        }
        for (auto& f : futures) f.waitForFinished();
        QCOMPARE(threadIds.size(), 1);
    }

    void testCancellationStopsInFlightWork() {
        LaneScheduler sched(2, 4);
        QAtomicInt completed{0};
        const int taskCount = 40;

        QList<SchedulerResult<int>> futures;
        for (int i = 0; i < taskCount; ++i) {
            futures.append(sched.submit<int>(
                {Lane::CPU, TaskPriority::Normal, i, "cancel-test"},
                [&completed]() -> int {
                    QThread::msleep(50);
                    completed.fetchAndAddOrdered(1);
                    return 1;
                }));
        }
        // Cancel after a brief delay — some tasks will have started
        QThread::msleep(20);
        sched.cancelAll();

        for (auto& f : futures) f.waitForFinished();

        // Cancelled futures must have ok=false with Cancelled code
        int cancelledCount = 0;
        for (auto& f : futures) {
            auto sv = f.result();
            if (!sv.ok) {
                QCOMPARE(sv.error.code, SchedulerErrorCode::Cancelled);
                ++cancelledCount;
            }
        }
        QVERIFY2(cancelledCount > 0, "At least some tasks must report cancelled");
        // Total submitted = completed + cancelled (no gaps)
        QCOMPARE(completed.loadRelaxed() + cancelledCount, taskCount);
    }

    void testCrossPagePipelining() {
        LaneScheduler sched(2, QThread::idealThreadCount());
        const int pageCount = 10;

        QElapsedTimer timer;
        timer.start();

        CrossPagePipeline<int, int, int> pipeline(sched, /*backpressure=*/4);

        QAtomicInt resultsReceived{0};
        pipeline.run(
            pageCount,
            [](int /*p*/) -> int { QThread::msleep(30); return 1; },        // stage1: 30ms
            [](int /*p*/, int v) -> int { QThread::msleep(50); return v; }, // stage2: 50ms (GPU)
            [](int /*p*/, int v) -> int { QThread::msleep(20); return v; }, // stage3: 20ms
            [&resultsReceived](int, int) { resultsReceived.fetchAndAddOrdered(1); }
        );

        qint64 elapsed = timer.elapsed();
        // Without overlap: 10 * (30+50+20) = 1000ms
        // With overlap: bottleneck is GPU lane (50ms * 10 / gpuCapacity=2) ~= 250ms + overhead
        // Conservative bound: < 10 * 100ms = 1000ms (must beat serial sum)
        QVERIFY2(elapsed < 10 * 100,
            qPrintable(QString("Cross-page pipeline took %1ms, expected < 1000ms (overlap required)")
                       .arg(elapsed)));
        QCOMPARE(resultsReceived.loadRelaxed(), pageCount);
    }

    void testOrderedResultQueueWithMissingPages() {
        LaneScheduler sched(2, 4);

        OrderedResultQueue<int> queue(6); // expecting pages 0..5

        // Submit pages 0,1,2,4,5 — skip page 3
        for (int p : {0, 1, 2, 4, 5}) {
            queue.submit(p, sched.submit<int>(
                {Lane::CPU, TaskPriority::Normal, p, "ordered-test"},
                [p]() -> int { return p * 10; }));
        }

        auto results = queue.collectOrdered();
        QCOMPARE(results.size(), 6);

        for (int i = 0; i < 6; ++i) {
            if (i == 3) {
                // Sentinel for missing page
                QVERIFY2(!results[i].ok,
                    qPrintable(QString("Page 3 sentinel: expected ok=false")));
                QCOMPARE(results[i].error.code, SchedulerErrorCode::Timeout);
            } else {
                QVERIFY2(results[i].ok,
                    qPrintable(QString("Page %1: expected ok=true").arg(i)));
                QCOMPARE(results[i].value, i * 10);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestLaneScheduler)
#include "TestLaneScheduler.moc"
