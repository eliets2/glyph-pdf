#include "engines/RenderCache.h"
#include <QtConcurrent>
#include <QFuture>

#ifdef HAS_PDFIUM
#include "engines/pdfium/PdfiumBackend.h"
#endif

RenderCache::RenderCache() {}

RenderCache::~RenderCache() {
    clear();
}

void RenderCache::setMaxCacheSize(qint64 bytes) {
    m_lock.lockForWrite();
    m_maxCacheSize = bytes;
    evictIfNeeded();
    m_lock.unlock();
}

qint64 RenderCache::maxCacheSize() const {
    m_lock.lockForRead();
    qint64 size = m_maxCacheSize;
    m_lock.unlock();
    return size;
}

void RenderCache::clear() {
    m_lock.lockForWrite();
    m_renderedPages.clear();
    m_lruList.clear();
    m_totalBytes = 0;
    m_pageSizes.clear();
    m_textLayer.clear();
    m_pageCount = 0;
    m_lock.unlock();
}

void RenderCache::resetStats() {
    m_hits.storeRelaxed(0);
    m_misses.storeRelaxed(0);
}

void RenderCache::setPageSize(int page, const QSizeF &size) {
    m_lock.lockForWrite();
    m_pageSizes.insert(page, size);
    m_lock.unlock();
}

QSizeF RenderCache::pageSize(int page, IPdfRenderer* renderer) {
    m_lock.lockForRead();
    if (m_pageSizes.contains(page)) {
        QSizeF size = m_pageSizes.value(page);
        m_lock.unlock();
        return size;
    }
    m_lock.unlock();

    if (!renderer) return QSizeF();

#ifdef HAS_PDFIUM
    auto* pdfium = dynamic_cast<PdfiumBackend*>(renderer);
    if (pdfium) {
        QSizeF size = pdfium->pageSize(page);
        if (size.isValid()) {
            setPageSize(page, size);
            return size;
        }
    }
#endif

    return QSizeF(595.276, 841.890); // Default A4 size
}

void RenderCache::setPageCount(int count) {
    m_lock.lockForWrite();
    m_pageCount = count;
    m_lock.unlock();
}

int RenderCache::pageCount() const {
    m_lock.lockForRead();
    int count = m_pageCount;
    m_lock.unlock();
    return count;
}

QImage RenderCache::getOrRender(int page, qreal scale, IPdfRenderer* renderer) {
    RenderCacheKey key{page, scale, false, QRectF()};

    // 1. Try to read
    m_lock.lockForRead();
    if (m_renderedPages.contains(key)) {
        m_hits.fetchAndAddRelaxed(1);
        QImage img = m_renderedPages.value(key).image;
        m_lock.unlock();

        // Update LRU
        m_lock.lockForWrite();
        m_lruList.removeOne(key);
        m_lruList.prepend(key);
        m_lock.unlock();

        return img;
    }
    m_lock.unlock();

    // 2. Miss: render outside lock
    m_misses.fetchAndAddRelaxed(1);
    if (!renderer) return QImage();

    int dpi = static_cast<int>(scale * 72.0);
    QImage rendered = renderer->renderPage(page, dpi);
    if (rendered.isNull()) return QImage();

    // 3. Insert and evict
    m_lock.lockForWrite();
    if (m_renderedPages.contains(key)) {
        QImage img = m_renderedPages.value(key).image;
        m_lock.unlock();
        return img;
    }

    qint64 bytes = imageSizeInBytes(rendered);
    m_renderedPages.insert(key, {rendered, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
    m_lock.unlock();

    return rendered;
}

QImage RenderCache::getOrRenderTile(int page, qreal scale, const QRectF &subRect, IPdfRenderer* renderer) {
    RenderCacheKey key{page, scale, true, subRect};

    // 1. Try to read
    m_lock.lockForRead();
    if (m_renderedPages.contains(key)) {
        m_hits.fetchAndAddRelaxed(1);
        QImage img = m_renderedPages.value(key).image;
        m_lock.unlock();

        // Update LRU
        m_lock.lockForWrite();
        m_lruList.removeOne(key);
        m_lruList.prepend(key);
        m_lock.unlock();

        return img;
    }
    m_lock.unlock();

    // 2. Miss: render outside lock
    m_misses.fetchAndAddRelaxed(1);
    if (!renderer) return QImage();

    int dpi = static_cast<int>(scale * 72.0);
    QImage tileImg = renderer->renderTile(page, subRect, dpi);
    if (tileImg.isNull()) return QImage();

    // 3. Insert and evict
    m_lock.lockForWrite();
    if (m_renderedPages.contains(key)) {
        QImage img = m_renderedPages.value(key).image;
        m_lock.unlock();
        return img;
    }

    qint64 bytes = imageSizeInBytes(tileImg);
    m_renderedPages.insert(key, {tileImg, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
    m_lock.unlock();

    return tileImg;
}

void RenderCache::insertPage(int page, qreal scale, const QImage &image) {
    m_lock.lockForWrite();
    RenderCacheKey key{page, scale, false, QRectF()};
    qint64 bytes = imageSizeInBytes(image);
    m_renderedPages.insert(key, {image, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
    m_lock.unlock();
}

void RenderCache::insertTile(int page, qreal scale, const QRectF &subRect, const QImage &image) {
    m_lock.lockForWrite();
    RenderCacheKey key{page, scale, true, subRect};
    qint64 bytes = imageSizeInBytes(image);
    m_renderedPages.insert(key, {image, bytes});
    m_lruList.removeOne(key);
    m_lruList.prepend(key);
    m_totalBytes += bytes;
    evictIfNeeded();
    m_lock.unlock();
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

    QtConcurrent::run([this, pagesToPrefetch, scale, renderer, currentToken]() {
        for (int p : pagesToPrefetch) {
            // Check cancellation token before rendering each page
            if (m_prefetchCancelToken.loadRelaxed() != currentToken) {
                return;
            }

            RenderCacheKey key{p, scale, false, QRectF()};
            m_lock.lockForRead();
            bool cached = m_renderedPages.contains(key);
            m_lock.unlock();

            if (!cached) {
                int dpi = static_cast<int>(scale * 72.0);
                QImage rendered = renderer->renderPage(p, dpi);
                if (!rendered.isNull()) {
                    m_lock.lockForWrite();
                    if (!m_renderedPages.contains(key)) {
                        qint64 bytes = imageSizeInBytes(rendered);
                        m_renderedPages.insert(key, {rendered, bytes});
                        m_lruList.removeOne(key);
                        m_lruList.prepend(key);
                        m_totalBytes += bytes;
                        evictIfNeeded();
                    }
                    m_lock.unlock();
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
        m_lock.lockForWrite();
        m_textLayer.insert(page, extracted);
        m_lock.unlock();
        return extracted;
    }
#endif

    return QString();
}

void RenderCache::insertText(int page, const QString &text) {
    m_lock.lockForWrite();
    m_textLayer.insert(page, text);
    m_lock.unlock();
}

void RenderCache::evictIfNeeded() {
    // Note: Assumes write lock is already held
    while (m_totalBytes > m_maxCacheSize && !m_lruList.isEmpty()) {
        RenderCacheKey oldestKey = m_lruList.takeLast();
        if (m_renderedPages.contains(oldestKey)) {
            m_totalBytes -= m_renderedPages.value(oldestKey).bytes;
            m_renderedPages.remove(oldestKey);
        }
    }
}

qint64 RenderCache::imageSizeInBytes(const QImage &image) const {
    if (image.isNull()) return 0;
    return image.sizeInBytes();
}
