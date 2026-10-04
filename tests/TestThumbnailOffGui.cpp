// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD 2026-09-30 §4 row 16 pins: thumbnail rendering must run OFF
// the GUI thread, reusing the RenderCache prefetch machinery (cancel-token
// epoch + in-flight future registry, EC06) — never a synchronous render inside
// first paint of a large document.
//
// Pre-fix behaviour (base a4cc1522): ThumbnailSidebar::createThumbWidget called
// RenderCache::getOrRender synchronously on the GUI thread, so first paint of
// an N-page document performed N full PDFium page renders inline with layout —
// the first-paint jank the scorecard row records.
//
// Contract under test:
//   1. First paint creates placeholder thumbnails (honest blank sheet — no
//      pixmap, no fabricated text) even when every render worker is PARKED,
//      i.e. the GUI thread provably rendered nothing itself; thumbnails then
//      arrive asynchronously as workers complete.
//   2. RenderCache::renderPageAsync schedules the render on a non-GUI thread
//      and invokes the callback with the rendered image; a cache hit is served
//      synchronously without re-rendering.
//   3. clear() (the document-changed path) cancels the in-flight render via
//      the existing cancel-token epoch AND joins the worker (EC06) before
//      returning; the stale render is never delivered nor left in the cache.
//   4. PdfViewerWidget::renderPageUncached is byte-identical to the cached
//      renderPage funnel and keeps the R12 finite-scale / pixel-budget guards
//      (it is the thread-safe seam the off-GUI renders go through).
//   5. Destroying the sidebar with renders in flight is safe: the destructor
//      drains via RenderCache::clear() while the renderer is still alive.
//   6. (R3-perf, audit finding 2) A duplicate renderPageAsync request for a
//      (page, scale) already in flight ATTACHES to the running render — one
//      worker, one renderPage call, both requesters delivered — instead of
//      queueing a redundant worker whose result is discarded at the insert.
//   7. (R3-perf, audit finding 7) A render superseded mid-flight (token epoch
//      advanced) is NOT inserted into the cache: the epoch is checked before
//      the insert, so a stale render never occupies cache space or evicts
//      fresh entries.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>
#include <QPdfWriter>
#include <QPainter>
#include <QMarginsF>
#include <QPageSize>
#include <QThread>
#include <QSemaphore>
#include <QSet>
#include <QFuture>
#include <QScopeGuard>
#include <QtConcurrent>
#include <atomic>
#include <limits>
#include <thread>
#include <functional>
#include "ui/ThumbnailSidebar.h"
#include "ui/PdfViewerWidget.h"
#include "engines/RenderCache.h"

namespace {

// Synthesize a text-bearing N-page PDF (QPdfWriter, 72 dpi → 612x792 pt pages).
QString createNPagePdf(const QTemporaryDir &tmpDir, const QString &name, int pages)
{
    const QString path = tmpDir.filePath(name);
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QSizeF(612.0, 792.0), QPageSize::Point));
    w.setPageMargins(QMarginsF(0, 0, 0, 0));
    w.setResolution(72);
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        p.drawText(QRect(50, 100, 500, 40), Qt::AlignLeft,
                   QStringLiteral("thumbnail offgui page %1").arg(i + 1));
    }
    p.end();
    return path;
}

QList<QLabel *> thumbImageLabels(const ThumbnailSidebar &sb)
{
    return sb.findChildren<QLabel *>(QString::fromLatin1("thumbPaperImage"));
}

int labelsWithPixmap(const ThumbnailSidebar &sb)
{
    int n = 0;
    const auto labels = thumbImageLabels(sb);
    for (QLabel *l : labels)
        if (!l->pixmap().isNull())
            ++n;
    return n;
}

// Event-loop pump with a bounded wait: delivers queued completions without
// ever blocking longer than one step.
bool waitUntil(const std::function<bool()> &pred, int timeoutMs = 30000)
{
    QElapsedTimer t;
    t.start();
    while (!pred()) {
        if (t.elapsed() > timeoutMs) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return true;
}

// Records the thread renderPage ran on; returns a small non-null image.
class ProbeRenderer : public IPdfRenderer {
public:
    QAtomicInt renderCount{0};
    QAtomicInt ranOffGuiThread{0};
    QImage renderPage(int, int) override {
        renderCount.fetchAndAddRelaxed(1);
        if (QThread::currentThread() != QCoreApplication::instance()->thread())
            ranOffGuiThread.storeRelaxed(1);
        QImage img(32, 32, QImage::Format_ARGB32);
        img.fill(0xFF663399);
        return img;
    }
    QImage renderTile(int, const QRectF &, int) override { return QImage(); }
    QSizeF pageSize(int) const override { return QSizeF(612, 792); }
    QString extractText(int) override { return QString(); }
};

// Blocks inside renderPage until released — pins a worker deterministically
// INSIDE the render call (the state clear()/~RenderCache must join).
class BlockingProbeRenderer : public IPdfRenderer {
public:
    QSemaphore gate;
    QAtomicInt entered{0};
    QImage renderPage(int, int) override {
        entered.storeRelaxed(1);
        gate.acquire();
        QImage img(16, 16, QImage::Format_ARGB32);
        img.fill(Qt::white);
        return img;
    }
    QImage renderTile(int, const QRectF &, int) override { return QImage(); }
    QSizeF pageSize(int) const override { return QSizeF(612, 792); }
    QString extractText(int) override { return QString(); }
};

// Blocks inside renderPage — only for the given page — until released, and
// counts calls: the R3-perf coalescing / stale-insert pins observe renderer
// activity through it.
class PageGatedRenderer : public IPdfRenderer {
public:
    QSemaphore gate;
    QAtomicInt entered{0};
    QAtomicInt renderCalls{0};
    explicit PageGatedRenderer(int gatedPage = 0) : m_gatedPage(gatedPage) {}
    QImage renderPage(int page, int) override {
        renderCalls.fetchAndAddRelaxed(1);
        if (page == m_gatedPage) {
            entered.storeRelaxed(1);
            gate.acquire();
        }
        QImage img(16, 16, QImage::Format_ARGB32);
        img.fill(Qt::white);
        return img;
    }
    QImage renderTile(int, const QRectF &, int) override { return QImage(); }
    QSizeF pageSize(int) const override { return QSizeF(612, 792); }
    QString extractText(int) override { return QString(); }
private:
    const int m_gatedPage;
};

} // namespace

class TestThumbnailOffGui : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void sidebarFirstPaintIsPlaceholderEvenWithAllWorkersParked();
    void thumbnailsArriveAsynchronouslyAfterRelease();
    void placeholderThumbnailsExposeAccessiblePageNames();
    void renderPageAsyncRunsOffThreadAndDelivers();
    void renderPageAsyncCacheHitServedSynchronously();
    void clearCancelsInFlightRenderJoinsAndDeliversNothing();
    void duplicateAsyncRequestCoalescesIntoOneWorker();
    void staleRenderIsNotInsertedIntoCache();
    void churnDuplicateRequestsClearLatencyBenchmark();
    void renderPageUncachedMatchesRenderPageAndKeepsGuards();
    void sidebarDestructionWithRendersInFlightIsSafe();
};

// Pin 1a — the fail-before pin. With EVERY QtConcurrent worker parked inside a
// test semaphore, first paint must still produce only placeholders: a
// synchronous GUI-thread render cannot be parked, so the pre-fix tree fills
// pixmaps here and fails this pin; the off-GUI tree cannot (its renders queue
// behind the parked pool).
void TestThumbnailOffGui::sidebarFirstPaintIsPlaceholderEvenWithAllWorkersParked()
{
    QVERIFY(m_dir.isValid());
    const QString pdf = createNPagePdf(m_dir, "parked.pdf", 6);

    // Park every global-pool worker: thumbnail renders (QtConcurrent::run on
    // the RenderCache prefetch path) queue behind these tasks and cannot start,
    // let alone deliver, until released.
    QThreadPool *pool = QThreadPool::globalInstance();
    const int poolThreads = pool->maxThreadCount();
    QSemaphore park, parked;
    QList<QFuture<void>> parkedFutures;
    for (int i = 0; i < poolThreads; ++i)
        parkedFutures.append(QtConcurrent::run([&park, &parked] { parked.release(); park.acquire(); }));
    parked.acquire(poolThreads); // all pool workers are parked inside our tasks

    QString why;
    bool placeholderHeld = true;
    bool checked = false;

    {
        PdfViewerWidget viewer;
        QVERIFY(viewer.loadDocument(pdf));

        ThumbnailSidebar sb;
        sb.setViewer(&viewer); // queues updateVisibleThumbnails (queued call)
        sb.resize(280, 900);
        sb.show();

        // Unpark on EVERY exit path (including a failing assert, which returns
        // early from the test function): the guard is declared AFTER `sb`, so
        // its destructor runs BEFORE the sidebar destructor drains via
        // RenderCache::clear() — with the pool still parked that join could
        // never complete and the parked workers would hang process exit.
        const auto unpark = qScopeGuard([&] { park.release(poolThreads); });

        // FIFO guarantee: this checker is posted AFTER the sidebar's queued
        // updateVisibleThumbnails (which creates the widgets), so it always
        // runs after widget creation — and before any completion, because no
        // completion can even be posted while the pool is parked.
        QEventLoop loop;
        QMetaObject::invokeMethod(&sb, [&] {
            const auto labels = thumbImageLabels(sb);
            if (labels.isEmpty()) {
                placeholderHeld = false;
                why = QStringLiteral("no thumbnail widgets created on first paint");
            }
            for (QLabel *l : labels) {
                if (!l->pixmap().isNull()) {
                    placeholderHeld = false;
                    why = QStringLiteral("thumbnail pixmap rendered synchronously "
                                         "on the GUI thread during first paint");
                }
                if (!l->text().isEmpty()) {
                    placeholderHeld = false;
                    why = QStringLiteral("placeholder fabricated text content");
                }
            }
            checked = true;
            loop.quit();
        }, Qt::QueuedConnection);
        QTimer::singleShot(20000, &loop, &QEventLoop::quit);
        loop.exec();

        QVERIFY2(checked, "first-paint checker never ran (event loop stalled)");
        QVERIFY2(placeholderHeld, qUtf8Printable(why));
    } // unpark guard releases first, then the sidebar dtor joins real renders

    QVERIFY(waitUntil([&] { return pool->activeThreadCount() == 0; }, 20000));
}

// Pin 1b — after the workers are unparked the thumbnails arrive asynchronously
// through the queued GUI-thread hop.
void TestThumbnailOffGui::thumbnailsArriveAsynchronouslyAfterRelease()
{
    QVERIFY(m_dir.isValid());
    const QString pdf = createNPagePdf(m_dir, "arrive.pdf", 4);

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    ThumbnailSidebar sb;
    sb.setViewer(&viewer);
    sb.resize(280, 900);
    sb.show();

    // Let the queued updateVisibleThumbnails create the (placeholder) widgets.
    QVERIFY(waitUntil([&] { return !thumbImageLabels(sb).isEmpty(); }));
    const int created = thumbImageLabels(sb).size();
    QVERIFY(created > 0);

    // Async arrival: every created thumbnail slot eventually gets its real
    // render via the queued GUI-thread hop. (The placeholder-AT-creation
    // property is pinned deterministically by the parked-pool pin above.)
    QVERIFY2(waitUntil([&] { return labelsWithPixmap(sb) == created; }),
             "thumbnails did not arrive asynchronously within the timeout");
}

// Pin 1c — r5-litems (DeepSeek UX audit finding 10): every thumbnail widget
// exposes an accessibleName carrying ITS page number, so a screen reader
// identifies the page instead of walking an unnamed interactive widget, and
// the interaction contract (click to navigate, drag to reorder) ships as the
// accessibleDescription of the surface that receives it. Pre-fix neither was
// set on the thumbnail surface (RED: accessibleName was empty for all slots).
void TestThumbnailOffGui::placeholderThumbnailsExposeAccessiblePageNames()
{
    QVERIFY(m_dir.isValid());
    const QString pdf = createNPagePdf(m_dir, "thumba11y.pdf", 3);

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    ThumbnailSidebar sb;
    sb.setViewer(&viewer);
    sb.resize(280, 900);
    sb.show();

    QVERIFY(waitUntil([&] { return !thumbImageLabels(sb).isEmpty(); }));

    // 260 px per slot: a 3-page document is fully visible at this size, so
    // virtualization cannot quietly shrink the asserted set.
    const auto items = sb.findChildren<QWidget *>(QString::fromLatin1("thumbItem"));
    QVERIFY2(items.size() == 3,
             "every page of a fully-visible document must have a thumbnail widget");
    QSet<int> pages;
    for (QWidget *w : items) {
        const int page = w->property("pageIndex").toInt();
        QVERIFY2(!pages.contains(page),
                 qPrintable(QStringLiteral("duplicate thumbnail widget for page %1").arg(page + 1)));
        pages.insert(page);
        // The name must carry THIS slot's page number — a constant name reused
        // across slots fails here exactly as an empty one does.
        QVERIFY2(w->accessibleName() == QStringLiteral("Page %1").arg(page + 1),
                 qPrintable(QStringLiteral("thumbnail page slot %1 exposes accessibleName "
                                          "\"%2\", expected \"Page %3\"")
                                .arg(page + 1)
                                .arg(w->accessibleName())
                                .arg(page + 1)));
        QVERIFY2(w->accessibleDescription().contains(QLatin1String("Click to navigate")),
                 "the thumbnail must disclose the interaction contract to screen readers");
    }
    QCOMPARE(pages.size(), 3);
}

// Pin 2 — RenderCache::renderPageAsync schedules off-thread and delivers.
void TestThumbnailOffGui::renderPageAsyncRunsOffThreadAndDelivers()
{
    ProbeRenderer probe;
    auto cache = std::make_shared<RenderCache>();

    QImage delivered;
    Qt::HANDLE renderThread = nullptr;
    const bool scheduled = cache->renderPageAsync(1, 1.0, &probe,
        [&](const QImage &img) {
            renderThread = QThread::currentThreadId();
            delivered = img;
        });

    // Miss → scheduled, not rendered inline (if this call rendered inline the
    // flag below would already be set before any event processing).
    QVERIFY(!scheduled);
    QCOMPARE(probe.renderCount.loadRelaxed(), 0);

    QVERIFY(waitUntil([&] { return !delivered.isNull(); }));
    QCOMPARE(probe.renderCount.loadRelaxed(), 1);
    QCOMPARE(probe.ranOffGuiThread.loadRelaxed(), 1);
    QVERIFY(renderThread != QThread::currentThreadId());
    QCOMPARE(cache->cacheMisses(), 1);

    // Callback contract: delivered on the worker thread (consumers marshal).
    QCOMPARE(delivered.size(), QSize(32, 32));
}

// Pin 2b — cache hit is served synchronously without a second render.
void TestThumbnailOffGui::renderPageAsyncCacheHitServedSynchronously()
{
    ProbeRenderer probe;
    auto cache = std::make_shared<RenderCache>();

    QImage first;
    QVERIFY(!cache->renderPageAsync(2, 2.0, &probe,
                                    [&](const QImage &img) { first = img; }));
    QVERIFY(waitUntil([&] { return !first.isNull(); }));

    QImage second;
    const bool servedFromCache = cache->renderPageAsync(2, 2.0, &probe,
                                                        [&](const QImage &img) { second = img; });
    QVERIFY(servedFromCache);
    // Callback fired INLINE (before this line) on the caller's thread.
    QVERIFY(!second.isNull());
    QCOMPARE(second, first);
    QCOMPARE(probe.renderCount.loadRelaxed(), 1); // no re-render
    QVERIFY(cache->cacheHits() >= 1);
}

// Pin 3 — clear() (the document-changed path) cancels via the existing token
// epoch AND joins the in-flight worker before returning (EC06); the stale
// render is never delivered and the cache is left empty.
void TestThumbnailOffGui::clearCancelsInFlightRenderJoinsAndDeliversNothing()
{
    BlockingProbeRenderer blocker;
    auto cache = std::make_shared<RenderCache>();

    bool delivered = false;
    QVERIFY(!cache->renderPageAsync(0, 1.0, &blocker,
                                    [&](const QImage &) { delivered = true; }));

    // The caller was not blocked: the worker entered renderPage while this
    // thread kept running.
    QVERIFY(waitUntil([&] { return blocker.entered.loadRelaxed(); }, 10000));

    // clear() from a helper thread must BLOCK until the in-flight worker is
    // joined — the EC06 guarantee that a renderer is never retired mid-render.
    QAtomicInt clearDone{0};
    std::thread clearer([&] { cache->clear(); clearDone.storeRelaxed(1); });
    QTest::qWait(300);
    QCOMPARE(clearDone.loadRelaxed(), 0); // still joining the blocked worker

    blocker.gate.release(); // the worker's token epoch is stale now: no delivery
    clearer.join();
    QCOMPARE(clearDone.loadRelaxed(), 1);
    QVERIFY(!delivered);

    // Nothing stale survived the wipe (getOrRender with a null renderer is a
    // pure lookup: hit → image, miss → null).
    QVERIFY(cache->getOrRender(0, 1.0, nullptr).isNull());
}

// R3-perf pin (audit finding 2) — duplicate-request coalescing. A second
// renderPageAsync for a (page, scale) already in flight must ATTACH to the
// running render (one worker, one renderPage call, both requesters delivered)
// instead of queueing a redundant worker whose result is discarded at the
// insert-time TOCTOU re-check. RED pre-fix: two workers scheduled, zero
// coalesced, two renderPage calls.
void TestThumbnailOffGui::duplicateAsyncRequestCoalescesIntoOneWorker()
{
    PageGatedRenderer blocker;
    auto cache = std::make_shared<RenderCache>();
    // Shared-ptr capture BY VALUE: pre-fix this pin REDs at the first
    // QCOMPARE and returns early — the (then unblocked) workers may still run
    // their deliveries afterwards, so the callback targets must outlive the
    // test function's stack frame.
    auto first = std::make_shared<QImage>();
    auto second = std::make_shared<QImage>();

    QVERIFY(!cache->renderPageAsync(0, 1.0, &blocker,
                                    [first](const QImage &img) { *first = img; }));
    QVERIFY(waitUntil([&] { return blocker.entered.loadRelaxed(); }, 10000));

    // The duplicate arrives while the first render is in flight. Its job is
    // registered synchronously by the first call, so the attach is
    // deterministic — no waiting needed.
    QVERIFY(!cache->renderPageAsync(0, 1.0, &blocker,
                                    [second](const QImage &img) { *second = img; }));

    // The coalescing property (RED pre-fix: runs==2, coalesced==0).
    QCOMPARE(cache->asyncWorkerRunsForTest(), 1);
    QCOMPARE(cache->asyncCoalescedRequestsForTest(), 1);

    // Released on EVERY exit path: pre-fix the duplicate runs its own worker
    // parked inside renderPage — the pool must never be left blocked (the
    // guard dies before the cache, whose dtor joins the workers).
    const auto unpark = qScopeGuard([&] { blocker.gate.release(4); });
    blocker.gate.release(4);   // the renders may now complete and be delivered
    QVERIFY(waitUntil([&] { return !first->isNull() && !second->isNull(); }));
    QCOMPARE(blocker.renderCalls.loadRelaxed(), 1);   // RED pre-fix: 2
    QCOMPARE(cache->asyncWorkerCompletionsForTest(), 1);
    QCOMPARE(*second, *first);   // both requesters received THE one render

    // A request after completion is a plain synchronous cache hit: no new
    // worker, no re-render.
    QVERIFY(cache->renderPageAsync(0, 1.0, &blocker, [](const QImage &){}));
    QCOMPARE(blocker.renderCalls.loadRelaxed(), 1);
    QCOMPARE(cache->asyncWorkerRunsForTest(), 1);
}

// R3-perf pin (audit finding 7) — insert AFTER the token check. A render
// superseded mid-flight (the epoch advanced while it ran) must not be
// inserted into the cache: the legacy worker called getOrRender, whose
// unconditional insert ran BEFORE the post-render token check, so the stale
// entry occupied cache space (and evicted fresh entries) until clear() mopped
// it up. RED pre-fix: the stale page-0 entry is present.
void TestThumbnailOffGui::staleRenderIsNotInsertedIntoCache()
{
    PageGatedRenderer blocker;
    auto cache = std::make_shared<RenderCache>();
    cache->setPageCount(10);

    bool delivered = false;
    QVERIFY(!cache->renderPageAsync(0, 1.0, &blocker,
                                    [&](const QImage &) { delivered = true; }));
    QVERIFY(waitUntil([&] { return blocker.entered.loadRelaxed(); }, 10000));

    // Advance the token epoch WITHOUT clear()'s join: a viewport prefetch on
    // the same cache supersedes the async render's epoch while it runs (the
    // prefetch pages — 2..8 around center 5 — are not gated, so the fresh
    // worker still inserts them: the cache is demonstrably not simply off).
    cache->prefetchViewport(5, 1.0, &blocker);
    const auto unpark = qScopeGuard([&] { blocker.gate.release(4); });
    blocker.gate.release(4);   // the stale render may now finish (and be judged)

    // Both the stale worker and the prefetch worker must have finished before
    // any cache-state assertion (the completion seam makes this wait exact).
    QVERIFY(waitUntil([&] { return cache->asyncWorkerCompletionsForTest() >= 1; },
                      30000));
    QVERIFY(waitUntil([&] { return !cache->getOrRender(6, 1.0, nullptr).isNull(); },
                      30000));

    QVERIFY2(cache->getOrRender(0, 1.0, nullptr).isNull(),
             "a stale (superseded) render was inserted into the cache — the "
             "epoch check must precede the insert");
    QVERIFY(!delivered);   // a stale render is never delivered (unchanged)
}

// R3-perf — MEASURED EXPERIMENT, not a pass/fail pin: the document-switch
// (clear()) join latency under scroll churn, i.e. with K duplicate render
// requests already in flight for the same page. Coalescing bounds the join
// set to one worker per distinct key (finding 3's GUI-thread bound); this
// function PRINTS the measured latency for docs/audit/evidence-r3-perf/
// (before vs after, with machine-load caveats). Assertions are ordering
// invariants only — never timing.
void TestThumbnailOffGui::churnDuplicateRequestsClearLatencyBenchmark()
{
    // Serialized slow renderer: renderPage holds a mutex for ~15 ms (a small
    // honest model of PdfiumBackend's process-wide pdfMutex serialization),
    // and the first call parks on a gate while holding that mutex — the exact
    // shape pre-fix churn produces: N workers queued behind the in-flight
    // render, all past their entry checks when clear() lands.
    class SlowSerializedRenderer : public IPdfRenderer {
    public:
        QMutex mutex;
        QSemaphore gate;
        QAtomicInt entered{0};
        QAtomicInt insideRender{0};
        QImage renderPage(int, int) override {
            insideRender.fetchAndAddRelaxed(1);
            QMutexLocker l(&mutex);
            if (entered.loadRelaxed() == 0) {
                entered.storeRelaxed(1);
                gate.acquire();          // park render #1, holding the mutex
            }
            QThread::msleep(15);         // one small honest render cost
            const QImage img(16, 16, QImage::Format_ARGB32);
            insideRender.fetchAndAddRelaxed(-1);
            return img;
        }
        QImage renderTile(int, const QRectF &, int) override { return QImage(); }
        QSizeF pageSize(int) const override { return QSizeF(612, 792); }
        QString extractText(int) override { return QString(); }
    };

    SlowSerializedRenderer slow;
    auto cache = std::make_shared<RenderCache>();

    constexpr int kRequests = 8;
    QAtomicInt deliveries{0};
    QVERIFY(!cache->renderPageAsync(0, 1.0, &slow,
                                    [&](const QImage &) { deliveries.fetchAndAddRelaxed(1); }));
    QVERIFY(waitUntil([&] { return slow.entered.loadRelaxed(); }, 10000));
    for (int i = 1; i < kRequests; ++i)
        QVERIFY(!cache->renderPageAsync(0, 1.0, &slow, [](const QImage &){}));

    const auto unpark = qScopeGuard([&] { slow.gate.release(2 * kRequests); });
    // Give the queue time to form (pre-fix: workers 2..K march into
    // renderPage and block behind the parked mutex holder).
    QTest::qWait(300);
    const int queued = slow.insideRender.loadRelaxed();

    // clear() on a helper thread — the document-changed path. Timing covers
    // the whole call (token bump + join + wipe).
    QElapsedTimer timer;
    QSemaphore done;
    qint64 elapsedMs = -1;
    std::thread clearer([&] {
        timer.start();
        cache->clear();
        elapsedMs = timer.elapsed();
        done.release();
    });
    QTest::qWait(150);          // clear() is now blocked in the join
    slow.gate.release(2 * kRequests);
    QVERIFY(done.tryAcquire(1, 30000));
    clearer.join();

    qInfo() << "[r3-perf benchmark] requests:" << kRequests
            << "queued in renderPage at clear():" << queued
            << "clear() join+wipe latency:" << elapsedMs << "ms"
            << "workerRuns:" << cache->asyncWorkerRunsForTest()
            << "coalesced:" << cache->asyncCoalescedRequestsForTest()
            << "deliveries:" << deliveries.loadRelaxed();

    // Ordering invariants only (no timing assertions): the joined workers are
    // done, the stale cache is empty, and — coalescing or not — at most the
    // first requester's delivery can land (the rest are superseded).
    QVERIFY(cache->asyncWorkerCompletionsForTest() >= 1);
    QVERIFY(cache->getOrRender(0, 1.0, nullptr).isNull());
}

// Pin 4 — renderPageUncached is the same funnel minus the cache: identical
// pixels, and the R12 guards (non-finite refusal, pixel budget) stay active.
void TestThumbnailOffGui::renderPageUncachedMatchesRenderPageAndKeepsGuards()
{
    QVERIFY(m_dir.isValid());
    const QString pdf = createNPagePdf(m_dir, "guards.pdf", 2);

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    const QImage cached = viewer.renderPage(0, 2.0);
    const QImage uncached = viewer.renderPageUncached(0, 2.0);
    QVERIFY(!cached.isNull());
    QVERIFY(!uncached.isNull());
    // The cached funnel stores a QPixmap; compare in a common format so only
    // real pixel differences fail (format normalization is not behavior).
    QCOMPARE(uncached.convertToFormat(QImage::Format_ARGB32),
             cached.convertToFormat(QImage::Format_ARGB32));

    // Non-finite/degenerate scales refused.
    QVERIFY(viewer.renderPageUncached(0, std::numeric_limits<double>::quiet_NaN()).isNull());
    QVERIFY(viewer.renderPageUncached(0, 0.0).isNull());
    QVERIFY(viewer.renderPageUncached(-1, 2.0).isNull());
    QVERIFY(viewer.renderPageUncached(99, 2.0).isNull());

    // Pixel budget: an absurd scale is bounded, never a multi-GB allocation.
    const QImage bounded = viewer.renderPageUncached(0, 1e9);
    QVERIFY(!bounded.isNull());
    const qint64 px = qint64(bounded.width()) * bounded.height();
    QVERIFY2(px <= 64LL * 1000 * 1000 + 4LL * 1000 * 1000,
             "renderPageUncached exceeded the 64 Mpx render budget");
}

// Pin 5 — destroying the sidebar with renders in flight is safe: the dtor
// drains via RenderCache::clear() while the renderer (and the sidebar) are
// still fully alive.
void TestThumbnailOffGui::sidebarDestructionWithRendersInFlightIsSafe()
{
    QVERIFY(m_dir.isValid());
    const QString pdf = createNPagePdf(m_dir, "teardown.pdf", 8);

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));

    for (int cycle = 0; cycle < 3; ++cycle) {
        auto *sb = new ThumbnailSidebar;
        sb->setViewer(&viewer);
        sb->resize(280, 900);
        sb->show();
        // Let widget creation schedule background renders, then destroy while
        // they are plausibly still in flight — no worker may outlive this.
        QVERIFY(waitUntil([&] { return !thumbImageLabels(*sb).isEmpty(); }, 10000));
        delete sb;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }

    // Any straggler completions must be dropped, not crash: pump the loop.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    QVERIFY(true);
}

QTEST_MAIN(TestThumbnailOffGui)
#include "TestThumbnailOffGui.moc"
