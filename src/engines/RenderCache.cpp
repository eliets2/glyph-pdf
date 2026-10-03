// SPDX-License-Identifier: Apache-2.0
#include "engines/RenderCache.h"
#include <QtConcurrent>
#include <QFuture>
#include <QThread>
#include <QDebug>
#include <QScopeGuard>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef HAS_PDFIUM
// RenderCache only depends on the IPdfRenderer interface now.
#endif

namespace {
    thread_local bool t_writeLocked = false;

    class WriteLockGuard {
        QReadWriteLock& m_lock;
    public:
        explicit WriteLockGuard(QReadWriteLock& lock) : m_lock(lock) {
            m_lock.lockForWrite();
            t_writeLocked = true;
        }
        ~WriteLockGuard() {
            t_writeLocked = false;
            m_lock.unlock();
        }
    };
}

RenderCache::RenderCache() {}

RenderCache::~RenderCache() {
    drainPrefetches();
    clear();
}

// EC06: cancel and join EVERY in-flight prefetch worker. The futures are
// snapshotted under the lock and waited on OUTSIDE it — a draining worker
// needs m_lock to insert results, so holding it while waiting would deadlock.
// Without this, a superseded prefetch (its future replaced in
// prefetchViewport) stayed in flight across clear()/destruction while holding
// a raw IPdfRenderer* that the caller is entitled to retire after clear().
void RenderCache::drainPrefetches() {
    m_prefetchCancelToken.fetchAndAddRelaxed(1);  // stop new page renders early
    QList<QFuture<void>> pending;
    {
        WriteLockGuard guard(m_lock);
        pending = m_inFlightPrefetches;
        m_inFlightPrefetches.clear();
    }
    for (QFuture<void>& f : pending)
        if (f.isRunning()) f.waitForFinished();
}

void RenderCache::setMaxCacheSize(qint64 bytes) {
    WriteLockGuard guard(m_lock);
    m_maxCacheSize = bytes;
    evictIfNeeded();
}

qint64 RenderCache::maxCacheSize() const {
    m_lock.lockForRead();
    qint64 size = m_maxCacheSize;
    m_lock.unlock();
    return size;
}

void RenderCache::clear() {
    drainPrefetches();

    WriteLockGuard guard(m_lock);
    m_renderedPages.clear();
    m_lruOrder.clear();
    m_totalBytes = 0;
    m_pageSizes.clear();
    m_textLayer.clear();
    m_pageCount = 0;
}

// O(1) move-to-front. Caller holds the write lock. (AR-6 D2)
void RenderCache::touchLru(const RenderCacheKey &key, CacheValue &value) {
    // splice is O(1) and preserves iterator validity of the moved element.
    m_lruOrder.splice(m_lruOrder.begin(), m_lruOrder, value.lruIt);
    value.lruIt = m_lruOrder.begin();
    Q_UNUSED(key);
}

// O(1) insert-or-replace at the front. Caller holds the write lock. (AR-6 D2)
void RenderCache::insertLocked(const RenderCacheKey &key, const QImage &image) {
    qint64 bytes = imageSizeInBytes(image);
    auto it = m_renderedPages.find(key);
    if (it != m_renderedPages.end()) {
        // Replace existing entry: adjust byte accounting, refresh LRU position.
        m_totalBytes += bytes - it->bytes;
        it->image = image;
        it->bytes = bytes;
        touchLru(key, *it);
    } else {
        m_lruOrder.push_front(key);
        m_totalBytes += bytes;
        m_renderedPages.insert(key, CacheValue{image, bytes, m_lruOrder.begin()});
    }
    evictIfNeeded();
}

void RenderCache::resetStats() {
    m_hits.storeRelaxed(0);
    m_misses.storeRelaxed(0);
}

void RenderCache::setPageSize(int page, const QSizeF &size) {
    WriteLockGuard guard(m_lock);
    std::promise<QSizeF> p;
    p.set_value(size);
    m_pageSizes.insert(page, p.get_future());
}

QSizeF RenderCache::pageSize(int page, IPdfRenderer* renderer) {
    m_lock.lockForRead();
    if (m_pageSizes.contains(page)) {
        auto fut = m_pageSizes.value(page);
        m_lock.unlock();
        return fut.get();
    }
    m_lock.unlock();

    if (!renderer) return QSizeF();

    std::shared_ptr<std::promise<QSizeF>> promisePtr;
    std::shared_future<QSizeF> fut;

    {
        WriteLockGuard guard(m_lock);
        if (m_pageSizes.contains(page)) {
            fut = m_pageSizes.value(page);
        } else {
            promisePtr = std::make_shared<std::promise<QSizeF>>();
            fut = promisePtr->get_future();
            m_pageSizes.insert(page, fut);
        }
    }

    if (promisePtr) {
        // AR-6 D4: if renderer->pageSize() throws, the promise must STILL be
        // fulfilled — otherwise fut.get() (here and on every concurrent waiter)
        // blocks/throws on a broken promise forever, permanently poisoning this
        // page's metadata cache. On failure we satisfy the promise with a safe
        // default AND evict the cached future under the write lock so a later
        // call can retry a real measurement.
        QSizeF size;
        bool failed = false;
        try {
            size = renderer->pageSize(page);
        } catch (...) {
            failed = true;
        }
        if (!size.isValid()) {
            size = QSizeF(595.276, 841.890); // Default A4 size
        }
        // Always set the value first so no waiter ever sees a broken promise.
        promisePtr->set_value(size);
        if (failed) {
            WriteLockGuard guard(m_lock);
            // Only erase if it's still our (now-fulfilled-with-default) future,
            // so a concurrent successful insert is not clobbered.
            if (m_pageSizes.contains(page))
                m_pageSizes.remove(page);
        }
    }

    return fut.get();
}

void RenderCache::setPageCount(int count) {
    WriteLockGuard guard(m_lock);
    m_pageCount = count;
}

int RenderCache::pageCount() const {
    m_lock.lockForRead();
    int count = m_pageCount;
    m_lock.unlock();
    return count;
}

QImage RenderCache::getOrRender(int page, qreal scale, IPdfRenderer* renderer) {
    // Memory guard: check pressure periodically
    checkMemoryPressure();

    // Auto-tile guard: if the page would exceed 50 MP, fall back to tiled
    if (renderer && shouldAutoTile(page, scale, renderer)) {
        qWarning() << "RenderCache: page" << page << "exceeds"
                   << MemoryGuard::LargePageMegapixels << "MP at scale" << scale
                   << "— rendering center tile only";
        QSizeF pSize = pageSize(page, renderer);
        double dpi = scale * 72.0;
        double pxW = pSize.width() * dpi / 72.0;
        double pxH = pSize.height() * dpi / 72.0;
        // Render center tile
        double tileW = qMin(static_cast<double>(MemoryGuard::TileSize), pxW);
        double tileH = qMin(static_cast<double>(MemoryGuard::TileSize), pxH);
        QRectF centerRect((pxW - tileW) / 2.0, (pxH - tileH) / 2.0, tileW, tileH);
        return getOrRenderTile(page, scale, centerRect, renderer);
    }

    RenderCacheKey key{page, scale, false, QRectF()};

    // 1. Try to read and update LRU together (TOCTOU fix)
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            m_hits.fetchAndAddRelaxed(1);
            touchLru(key, *it);
            return it->image;
        }
    }

    // 2. Miss: render outside lock
    m_misses.fetchAndAddRelaxed(1);
    if (!renderer) return QImage();

    int dpi = static_cast<int>(scale * 72.0);
    QImage rendered = renderer->renderPage(page, dpi);
    if (rendered.isNull()) return QImage();

    // 3. Insert and evict
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            touchLru(key, *it);
            return it->image;
        }
        insertLocked(key, rendered);
    }

    return rendered;
}

QImage RenderCache::getOrRenderTile(int page, qreal scale, const QRectF &subRect, IPdfRenderer* renderer) {
    RenderCacheKey key{page, scale, true, subRect};

    // 1. Try to read and update LRU together (TOCTOU fix)
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            m_hits.fetchAndAddRelaxed(1);
            touchLru(key, *it);
            return it->image;
        }
    }

    // 2. Miss: render outside lock
    m_misses.fetchAndAddRelaxed(1);
    if (!renderer) return QImage();

    int dpi = static_cast<int>(scale * 72.0);
    QImage tileImg = renderer->renderTile(page, subRect, dpi);
    if (tileImg.isNull()) return QImage();

    // 3. Insert and evict
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            touchLru(key, *it);
            return it->image;
        }
        insertLocked(key, tileImg);
    }

    return tileImg;
}

void RenderCache::insertPage(int page, qreal scale, const QImage &image) {
    WriteLockGuard guard(m_lock);
    RenderCacheKey key{page, scale, false, QRectF()};
    insertLocked(key, image);
}

void RenderCache::insertTile(int page, qreal scale, const QRectF &subRect, const QImage &image) {
    WriteLockGuard guard(m_lock);
    RenderCacheKey key{page, scale, true, subRect};
    insertLocked(key, image);
}

void RenderCache::prefetchViewport(int centerPage, qreal scale, IPdfRenderer* renderer) {
    if (!renderer) return;

    // Cancel previous prefetch sessions
    int currentToken = m_prefetchCancelToken.fetchAndAddRelaxed(1) + 1;

    m_lock.lockForRead();
    int totalPages = m_pageCount;
    m_lock.unlock();

    if (totalPages <= 0) return;

    QList<int> pagesToPrefetch;
    for (int offset = 1; offset <= 3; ++offset) {
        int next = centerPage + offset;
        int prev = centerPage - offset;
        if (next < totalPages) pagesToPrefetch.append(next);
        if (prev >= 0) pagesToPrefetch.append(prev);
    }
    std::weak_ptr<RenderCache> weakThis = weak_from_this();

    // EC06: retain the new future alongside every still-running predecessor —
    // superseding a prefetch must not orphan its worker. Finished futures are
    // pruned so the list stays bounded.
    auto future = QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() {
        auto self = weakThis.lock();
        if (!self) return;

        // AR-6 D1: rendering is SERIAL — the backend (PdfiumBackend) serialises
        // every renderPage()/renderTile() behind one mutex (PDFium page objects
        // are not safe to render concurrently against a single FPDF_DOCUMENT).
        // We deliberately keep render serial rather than maintaining a pool of
        // per-thread document instances; the chosen mitigation is to run this
        // BACKGROUND prefetch at the lowest thread priority so it can never
        // starve the foreground UI render. When the UI thread requests a page,
        // the OS scheduler preempts this thread for the (normal-priority) caller,
        // so foreground latency is bounded by at most one in-flight page render.
        if (QThread* t = QThread::currentThread())
            t->setPriority(QThread::LowestPriority);

        for (int p : pagesToPrefetch) {
            // Check cancellation token before rendering each page
            if (self->m_prefetchCancelToken.loadRelaxed() != currentToken) {
                return;
            }

            RenderCacheKey key{p, scale, false, QRectF()};
            
            bool cached = false;
            {
                WriteLockGuard guard(self->m_lock);
                cached = self->m_renderedPages.contains(key);
            }

            if (!cached) {
                int dpi = static_cast<int>(scale * 72.0);
                QImage rendered = renderer->renderPage(p, dpi);
                
                // cancellation token check between lookup and write
                if (self->m_prefetchCancelToken.loadRelaxed() != currentToken) {
                    return;
                }

                if (!rendered.isNull()) {
                    WriteLockGuard guard(self->m_lock);
                    if (!self->m_renderedPages.contains(key)) {
                        self->insertLocked(key, rendered);
                    }
                }
            }
        }
    });

    {
        WriteLockGuard guard(m_lock);
        m_inFlightPrefetches.erase(
            std::remove_if(m_inFlightPrefetches.begin(), m_inFlightPrefetches.end(),
                           [](const QFuture<void>& f) { return f.isFinished(); }),
            m_inFlightPrefetches.end());
        m_inFlightPrefetches.append(future);
    }
}

// R3-perf (audit finding 7): the worker's render half. The legacy path called
// getOrRender(), whose insert ran unconditionally BEFORE the post-render
// token check — a superseded render paid a full insert into the cache and
// could evict fresh entries (the cache is LRU) for content clear() would
// only mop up later. This explicit sequence is getOrRender's miss path with
// ONE reordering: the epoch is re-checked inside the write lock that performs
// the insert, so a stale render inserts nothing. (clear()'s wipe happens
// after drainPrefetches() joins this very worker, so check-then-insert under
// the cache lock cannot interleave with a wipe.)
//
// Auto-tile routing (Session 16 D5) is preserved verbatim: a tile-worthy page
// (>50 MP at this scale) still goes through getOrRender's center-tile path —
// a corner case far outside the thumbnail scales this async path serves.
QImage RenderCache::renderForAsyncWorker(int page, qreal scale,
                                         IPdfRenderer* renderer,
                                         int currentToken,
                                         const RenderCacheKey &key) {
    if (shouldAutoTile(page, scale, renderer))
        return getOrRender(page, scale, renderer);

    checkMemoryPressure();

    // 1. Another path may have rendered this key while we were queued.
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            m_hits.fetchAndAddRelaxed(1);
            touchLru(key, *it);
            return it->image;
        }
    }

    // 2. Miss: render OUTSIDE the lock (same shape as getOrRender).
    m_misses.fetchAndAddRelaxed(1);
    const int dpi = static_cast<int>(scale * 72.0);
    QImage rendered = renderer->renderPage(page, dpi);
    if (rendered.isNull()) return QImage();

    // 3. Insert — epoch re-checked under the insert's write lock (the fix):
    //    a render superseded mid-flight inserts NOTHING and is dropped by the
    //    caller's delivery gate.
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            m_hits.fetchAndAddRelaxed(1);
            touchLru(key, *it);
            return it->image;
        }
        if (m_prefetchCancelToken.loadRelaxed() == currentToken)
            insertLocked(key, rendered);
    }
    return rendered;
}

bool RenderCache::renderPageAsync(int page, qreal scale, IPdfRenderer* renderer,
                                  std::function<void(const QImage&)> onRendered) {
    RenderCacheKey key{page, scale, false, QRectF()};

    // 1. Hit: serve synchronously — no worker, no GUI-thread render anywhere.
    {
        WriteLockGuard guard(m_lock);
        auto it = m_renderedPages.find(key);
        if (it != m_renderedPages.end()) {
            m_hits.fetchAndAddRelaxed(1);
            touchLru(key, *it);
            if (onRendered) onRendered(it->image);
            return true;
        }
    }

    if (!renderer) return false;

    // 2. Miss. R3-perf (audit finding 2): a request for a key that is ALREADY
    //    being rendered attaches to the in-flight job — the running worker
    //    delivers to every attached callback when it completes. No second
    //    worker is queued (scroll churn used to saturate the pool with
    //    renders whose results were thrown away at the TOCTOU re-check), and
    //    drainPrefetches() consequently joins at most one worker per distinct
    //    key on document change (finding 3's bound).
    QSharedPointer<AsyncRenderJob> job;
    int currentToken = 0;
    {
        WriteLockGuard guard(m_lock);
        auto jit = m_asyncJobs.find(key);
        if (jit != m_asyncJobs.end()) {
            (*jit)->waiters.append(std::move(onRendered));
            m_asyncCoalescedRequests.fetchAndAddRelaxed(1);
            return false;   // the in-flight worker owns the delivery
        }

        // Register a fresh job and capture the epoch under the same lock.
        job = QSharedPointer<AsyncRenderJob>::create();
        if (onRendered) job->waiters.append(std::move(onRendered));
        m_asyncJobs.insert(key, job);
        currentToken = m_prefetchCancelToken.loadRelaxed();

        // EC06: retain the future alongside every still-running predecessor;
        // finished futures are pruned so the list stays bounded.
        m_inFlightPrefetches.erase(
            std::remove_if(m_inFlightPrefetches.begin(), m_inFlightPrefetches.end(),
                           [](const QFuture<void>& f) { return f.isFinished(); }),
            m_inFlightPrefetches.end());
    }

    std::weak_ptr<RenderCache> weakThis = weak_from_this();

    // R3-perf seam: one counter tick per worker actually scheduled (the
    // coalesced attach above returned already).
    m_asyncWorkerRuns.fetchAndAddRelaxed(1);

    auto future = QtConcurrent::run([weakThis, page, scale, renderer,
                                     currentToken, job, key]() {
        auto self = weakThis.lock();
        if (!self) return;

        // AR-6 D1 (same discipline as prefetchViewport): the backend serializes
        // renders behind one mutex, so background renders run at the lowest
        // thread priority and can never starve a foreground render.
        if (QThread* t = QThread::currentThread())
            t->setPriority(QThread::LowestPriority);

        QImage rendered;
        // Superseded before we even started (clear()/prefetch bump): render
        // nothing, deliver nothing — but the job is still retired below.
        if (self->m_prefetchCancelToken.loadRelaxed() == currentToken)
            rendered = self->renderForAsyncWorker(page, scale, renderer,
                                                  currentToken, key);

        // Retire the job BEFORE delivering, under the cache lock — on EVERY
        // exit path: from this point a new request either cache-hits (fresh
        // render inserted) or starts fresh (superseded render inserted
        // nothing) — it can never attach to a worker that is about to exit.
        // The waiters snapshot is taken under the same lock, so every attach
        // that happened during the render is captured exactly once.
        QList<std::function<void(const QImage&)>> waiters;
        {
            WriteLockGuard guard(self->m_lock);
            self->m_asyncJobs.remove(key);
            waiters = job->waiters;
            self->m_asyncWorkerCompletions.fetchAndAddRelaxed(1);
        }

        // Cancellation between render and delivery: a stale render (document
        // changed mid-render) is neither inserted (renderForAsyncWorker) nor
        // DELIVERED to the consumer. The renderPageAsync hit path and the
        // token epoch make every document change route through clear(),
        // which joins this worker via drainPrefetches() BEFORE wiping, so
        // nothing stale is ever misattributed to new content.
        if (rendered.isNull()) return;
        if (self->m_prefetchCancelToken.loadRelaxed() != currentToken) return;

        // Deliver to THE requester and every coalesced duplicate. All waiters
        // run sequentially on THIS worker thread (consumers marshal).
        for (const auto& waiter : waiters)
            if (waiter) waiter(rendered);
    });

    {
        WriteLockGuard guard(m_lock);
        m_inFlightPrefetches.append(future);
    }

    // Stats: hits/misses are counted by the actual cache accesses (the sync
    // hit above, renderForAsyncWorker inside the worker) — a scheduled-but-
    // cancelled request counts as neither.
    return false;
}

QString RenderCache::getOrExtractText(int page, IPdfRenderer* renderer) {
    m_lock.lockForRead();
    if (m_textLayer.contains(page)) {
        QString text = m_textLayer.value(page);
        m_lock.unlock();
        return text;
    }
    m_lock.unlock();

    if (!renderer) return QString();

    QString extracted = renderer->extractText(page);
    WriteLockGuard guard(m_lock);
    m_textLayer.insert(page, extracted);
    return extracted;

    return QString();
}

void RenderCache::insertText(int page, const QString &text) {
    WriteLockGuard guard(m_lock);
    m_textLayer.insert(page, text);
}

void RenderCache::evictIfNeeded() {
    // Note: Assumes write lock is already held
    Q_ASSERT(t_writeLocked);
    while (m_totalBytes > m_maxCacheSize && !m_lruOrder.empty()) {
        // O(1) eviction: oldest is at the back of the intrusive LRU list.
        const RenderCacheKey oldestKey = m_lruOrder.back();
        auto it = m_renderedPages.find(oldestKey);
        if (it != m_renderedPages.end()) {
            m_totalBytes -= it->bytes;
            m_renderedPages.erase(it);
        }
        m_lruOrder.pop_back();
    }

#ifndef QT_NO_DEBUG
    qint64 actualBytes = 0;
    for (auto it = m_renderedPages.constBegin(); it != m_renderedPages.constEnd(); ++it) {
        actualBytes += it.value().bytes;
    }
    Q_ASSERT_X(m_totalBytes == actualBytes, "RenderCache", "m_totalBytes invariant violated");
#endif
}

qint64 RenderCache::imageSizeInBytes(const QImage &image) const {
    if (image.isNull()) return 0;
    return image.sizeInBytes();
}

// ── Memory guards (Session 16 D5) ────────────────────────────────────────

qint64 RenderCache::availableSystemMemory() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex))
        return static_cast<qint64>(statex.ullAvailPhys);
#endif
    return -1; // Unknown
}

bool RenderCache::isSystemMemoryLow() const {
    qint64 avail = availableSystemMemory();
    if (avail < 0) return false; // Can't determine — assume OK
    return avail < MemoryGuard::LowMemoryThreshold;
}

bool RenderCache::shouldAutoTile(int page, qreal scale, IPdfRenderer* renderer) {
    QSizeF pSize = pageSize(page, renderer);
    if (!pSize.isValid()) return false;

    // Compute rendered pixel dimensions at the given scale
    double dpi = scale * 72.0;
    double pxW = pSize.width() * dpi / 72.0;
    double pxH = pSize.height() * dpi / 72.0;
    double megapixels = (pxW * pxH) / 1'000'000.0;

    return megapixels > MemoryGuard::LargePageMegapixels;
}

void RenderCache::checkMemoryPressure() {
    // AR-6 D5: throttle the OS poll. This runs on every getOrRender(); querying
    // GlobalMemoryStatusEx per tile during a scroll is a syscall storm. Only
    // actually poll once per MemoryPressurePollInterval calls. Eviction is not
    // time-critical — a 32-call delay before reacting to low memory is fine, and
    // the next interval re-checks. (fetchAndAdd wraps harmlessly.)
    const int tick = m_memoryPressureTick.fetchAndAddRelaxed(1);
    if ((tick % MemoryPressurePollInterval) != 0) return;

    if (!isSystemMemoryLow()) return;

    qWarning() << "RenderCache: system memory low (<"
               << (MemoryGuard::LowMemoryThreshold / (1024*1024)) << "MB free)"
               << "— aggressive eviction";

    WriteLockGuard guard(m_lock);

    // Aggressive eviction: shrink to AggressiveCacheLimit
    while (m_totalBytes > MemoryGuard::AggressiveCacheLimit && !m_lruOrder.empty()) {
        const RenderCacheKey oldestKey = m_lruOrder.back();
        auto it = m_renderedPages.find(oldestKey);
        if (it != m_renderedPages.end()) {
            m_totalBytes -= it->bytes;
            m_renderedPages.erase(it);
        }
        m_lruOrder.pop_back();
    }
}
