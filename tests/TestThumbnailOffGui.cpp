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

} // namespace

class TestThumbnailOffGui : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void sidebarFirstPaintIsPlaceholderEvenWithAllWorkersParked();
    void thumbnailsArriveAsynchronouslyAfterRelease();
    void renderPageAsyncRunsOffThreadAndDelivers();
    void renderPageAsyncCacheHitServedSynchronously();
    void clearCancelsInFlightRenderJoinsAndDeliversNothing();
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
