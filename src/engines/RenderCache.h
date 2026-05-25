#pragma once

#include <QImage>
#include <QHash>
#include <QList>
#include <QReadWriteLock>
#include <QSizeF>
#include <QRectF>
#include <QString>
#include <QAtomicInt>
#include "core/interfaces/IPdfRenderer.h"

// Memory-guard thresholds (Session 16 D5)
namespace MemoryGuard {
    constexpr qint64 LargeFileSizeBytes   = 500LL * 1024 * 1024; // 500 MB
    constexpr qint64 LowMemoryThreshold   = 500LL * 1024 * 1024; // 500 MB free
    constexpr qint64 AggressiveCacheLimit = 64LL * 1024 * 1024;   // 64 MB when low
    constexpr double LargePageMegapixels  = 50.0;                // 50 MP
    constexpr int    TileSize             = 2048;                // tile px
}

struct RenderCacheKey {
    int page;
    qreal scale;
    bool isTile = false;
    QRectF subRect; // only valid if isTile is true

    bool operator==(const RenderCacheKey &other) const {
        if (page != other.page || !qFuzzyCompare(scale, other.scale) || isTile != other.isTile) {
            return false;
        }
        if (isTile) {
            return qFuzzyCompare(subRect.x(), other.subRect.x()) &&
                   qFuzzyCompare(subRect.y(), other.subRect.y()) &&
                   qFuzzyCompare(subRect.width(), other.subRect.width()) &&
                   qFuzzyCompare(subRect.height(), other.subRect.height());
        }
        return true;
    }
};

inline size_t qHash(const RenderCacheKey &key, size_t seed = 0) {
    size_t hash = qHash(key.page, seed) ^ qHash(static_cast<double>(key.scale), seed) ^ qHash(key.isTile, seed);
    if (key.isTile) {
        hash ^= qHash(static_cast<double>(key.subRect.x()), seed) ^
                qHash(static_cast<double>(key.subRect.y()), seed) ^
                qHash(static_cast<double>(key.subRect.width()), seed) ^
                qHash(static_cast<double>(key.subRect.height()), seed);
    }
    return hash;
}

class RenderCache {
public:
    RenderCache();
    ~RenderCache();

    // Cache limits & config
    void setMaxCacheSize(qint64 bytes);
    qint64 maxCacheSize() const;
    void clear();

    // Stats
    qint64 cacheHits() const { return m_hits.loadRelaxed(); }
    qint64 cacheMisses() const { return m_misses.loadRelaxed(); }
    void resetStats();

    // Tier 1: Metadata (always resident)
    void setPageSize(int page, const QSizeF &size);
    QSizeF pageSize(int page, IPdfRenderer* renderer = nullptr);
    void setPageCount(int count);
    int pageCount() const;

    // Tier 2: Rendered Pages & Tiles (LRU cache keyed by page and scale)
    QImage getOrRender(int page, qreal scale, IPdfRenderer* renderer);
    QImage getOrRenderTile(int page, qreal scale, const QRectF &subRect, IPdfRenderer* renderer);
    void insertPage(int page, qreal scale, const QImage &image);
    void insertTile(int page, qreal scale, const QRectF &subRect, const QImage &image);

    // Viewport Prefetch
    void prefetchViewport(int centerPage, qreal scale, IPdfRenderer* renderer);

    // Memory guards (Session 16 D5)
    static qint64 availableSystemMemory();
    bool isSystemMemoryLow() const;
    bool shouldAutoTile(int page, qreal scale, IPdfRenderer* renderer);
    void checkMemoryPressure();

    // Tier 3: Text Layer (always resident after first parse)
    QString getOrExtractText(int page, IPdfRenderer* renderer);
    void insertText(int page, const QString &text);

private:
    void evictIfNeeded();
    qint64 imageSizeInBytes(const QImage &image) const;

    mutable QReadWriteLock m_lock;

    // Configuration
    qint64 m_maxCacheSize = 256 * 1024 * 1024; // 256 MB default

    // Performance Stats
    mutable QAtomicInt m_hits{0};
    mutable QAtomicInt m_misses{0};

    // Viewport prefetch cancellation token
    QAtomicInt m_prefetchCancelToken{0};

    // Tier 1: Metadata
    int m_pageCount = 0;
    QHash<int, QSizeF> m_pageSizes;

    // Tier 2: Rendered Pages
    struct CacheValue {
        QImage image;
        qint64 bytes = 0;
    };
    QHash<RenderCacheKey, CacheValue> m_renderedPages;
    QList<RenderCacheKey> m_lruList; // front is most recently used, back is oldest
    qint64 m_totalBytes = 0;

    // Tier 3: Text Layer
    QHash<int, QString> m_textLayer;
};
