// SPDX-License-Identifier: Apache-2.0
#include "engines/RenderCache.h"
#include <QtConcurrent>
#include <QFuture>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef HAS_PDFIUM
#include "engines/pdfium/PdfiumBackend.h"
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
    m_prefetchCancelToken.fetchAndAddRelaxed(1);
    if (m_prefetchFuture.isRunning()) m_prefetchFuture.waitForFinished();
    clear();
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
    WriteLockGuard guard(m_lock);
    m_renderedPages.clear();
    m_lruList.clear();
    m_totalBytes = 0;
    m_pageSizes.clear();
    m_textLayer.clear();
    m_pageCount = 0;
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
        QSizeF size;
#ifdef HAS_PDFIUM
        auto* pdfium = dynamic_cast<PdfiumBackend*>(renderer);
        if (pdfium) {
            size = pdfium->pageSize(page);
        }
#endif
        if (!size.isValid()) {
            size = QSizeF(595.276, 841.890); // Default A4 size
        }
        promisePtr->set_value(size);
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
        if (m_renderedPages.contains(key)) {
            m_hits.fetchAndAddRelaxed(1);
            QImage img = m_renderedPages.value(key).image;
            m_lruList.removeOne(key);
            m_lruList.prepend(key);
            return img;
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
        if (m_renderedPages.contains(key)) {
            return m_renderedPages.value(key).image;
        }

        qint64 bytes = imageSizeInBytes(rendered);
        m_renderedPages.insert(key, {rendered, bytes});
        m_lruList.removeOne(key);
        m_lruList.prepend(key);
        m_totalBytes += bytes;
        evictIfNeeded();
    }

    return rendered;
}

QImage RenderCache::getOrRenderTile(int page, qreal scale, const QRectF &subRect, IPdfRenderer* renderer) {
    RenderCacheKey key{page, scale, true, subRect};

    // 1. Try to read and update LRU together (TOCTOU fix)
    {
        WriteLockGuard guard(m_lock);
        if (m_renderedPages.contains(key)) {
            m_hits.fetchAndAddRelaxed(1);
            QImage img = m_renderedPages.value(key).image;
            m_lruList.removeOne(key);
            m_lruList.prepend(key);
            return img;
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
        if (m_renderedPages.contains(key)) {
            return m_renderedPages.value(key).image;
        }

        qint64 bytes = imageSizeInBytes(tileImg);
        m_renderedPages.insert(key, {tileImg, bytes});
        m_lruList.removeOne(key);
        m_lruList.prepend(key);
        m_totalBytes += bytes;
        evictIfNeeded();
    }

    return tileImg;
}

void RenderCache::insertPage(int page, qreal scale, const QImage &image) {
    WriteLockGuard guard(m_lock);
    RenderCacheKey key{page, scale, false, QRectF()};
    qint64 bytes = imageSizeInBytes(image);
    m_renderedPages.insert(key, {image, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
}

void RenderCache::insertTile(int page, qreal scale, const QRectF &subRect, const QImage &image) {
    WriteLockGuard guard(m_lock);
    RenderCacheKey key{page, scale, true, subRect};
    qint64 bytes = imageSizeInBytes(image);
    m_renderedPages.insert(key, {image, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
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

    if (m_prefetchFuture.isRunning()) {
        // cancellation signaled via token above
    }

    m_prefetchFuture = QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() {
        auto self = weakThis.lock();
        if (!self) return;

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
                        qint64 bytes = self->imageSizeInBytes(rendered);
                        self->m_renderedPages.insert(key, {rendered, bytes});
                        self->m_lruList.removeOne(key);
                        self->m_lruList.prepend(key);
                        self->m_totalBytes += bytes;
                        self->evictIfNeeded();
                    }
                }
            }
        }
    });
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

#ifdef HAS_PDFIUM
    auto* pdfium = dynamic_cast<PdfiumBackend*>(renderer);
    if (pdfium) {
        QString extracted = pdfium->extractText(page);
        WriteLockGuard guard(m_lock);
        m_textLayer.insert(page, extracted);
        return extracted;
    }
#endif

    return QString();
}

void RenderCache::insertText(int page, const QString &text) {
    WriteLockGuard guard(m_lock);
    m_textLayer.insert(page, text);
}

void RenderCache::evictIfNeeded() {
    // Note: Assumes write lock is already held
    Q_ASSERT(t_writeLocked);
    while (m_totalBytes > m_maxCacheSize && !m_lruList.isEmpty()) {
        RenderCacheKey oldestKey = m_lruList.takeLast();
        if (m_renderedPages.contains(oldestKey)) {
            m_totalBytes -= m_renderedPages.value(oldestKey).bytes;
            m_renderedPages.remove(oldestKey);
        }
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
    if (!isSystemMemoryLow()) return;

    qWarning() << "RenderCache: system memory low (<"
               << (MemoryGuard::LowMemoryThreshold / (1024*1024)) << "MB free)"
               << "— aggressive eviction";

    WriteLockGuard guard(m_lock);

    // Aggressive eviction: shrink to AggressiveCacheLimit
    while (m_totalBytes > MemoryGuard::AggressiveCacheLimit && !m_lruList.isEmpty()) {
        RenderCacheKey oldestKey = m_lruList.takeLast();
        if (m_renderedPages.contains(oldestKey)) {
            m_totalBytes -= m_renderedPages.value(oldestKey).bytes;
            m_renderedPages.remove(oldestKey);
        }
    }
}
