// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QHash>
#include <QHashFunctions>
#include <QList>
#include <QReadWriteLock>
#include <QSizeF>
#include <QRectF>
#include <QString>
#include <QAtomicInt>
#include <cmath>
#include <cstdint>
#include <list>
#include <memory>
#include <future>
#include <QFuture>
#include "core/interfaces/IPdfRenderer.h"

// Memory-guard thresholds (Session 16 D5)
namespace MemoryGuard {
    constexpr qint64 LargeFileSizeBytes   = 500LL * 1024 * 1024; // 500 MB
    constexpr qint64 LowMemoryThreshold   = 500LL * 1024 * 1024; // 500 MB free
    constexpr qint64 AggressiveCacheLimit = 64LL * 1024 * 1024;   // 64 MB when low
    constexpr double LargePageMegapixels  = 50.0;                // 50 MP
    constexpr int    TileSize             = 2048;                // tile px
}

// AR-6 D2: scale and sub-rect coordinates are quantized to an integer grid so
// that hashing and equality operate on the SAME discrete values. The previous
// design hashed the exact double bits but compared with qFuzzyCompare, which
// violated the hash invariant (a == b must imply qHash(a) == qHash(b)): two keys
// that compared "equal" under fuzzy comparison could land in different buckets,
// producing duplicate tiles and missed lookups. Quantizing once, up front, makes
// hash and equality consistent by construction.
namespace RenderCacheGrid {
    // Scale grid: 1/1000 of a scale unit (0.001). Two scales that differ by less
    // than this round to the same cell — the same tolerance the UI uses for zoom.
    constexpr double ScaleStep = 1000.0;
    // Sub-rect grid: 1 PDF point. Tile rects are point-aligned in practice.
    constexpr double RectStep = 1.0;

    inline qint64 quantize(double v, double step) {
        return static_cast<qint64>(std::llround(v * step));
    }
}

struct RenderCacheKey {
    int page;
    qreal scale;
    bool isTile = false;
    QRectF subRect; // only valid if isTile is true

    // Quantized, integral representation used for BOTH hashing and equality.
    qint64 scaleQ() const {
        return RenderCacheGrid::quantize(scale, RenderCacheGrid::ScaleStep);
    }
    qint64 rxQ() const { return RenderCacheGrid::quantize(subRect.x(),      RenderCacheGrid::RectStep); }
    qint64 ryQ() const { return RenderCacheGrid::quantize(subRect.y(),      RenderCacheGrid::RectStep); }
    qint64 rwQ() const { return RenderCacheGrid::quantize(subRect.width(),  RenderCacheGrid::RectStep); }
    qint64 rhQ() const { return RenderCacheGrid::quantize(subRect.height(), RenderCacheGrid::RectStep); }

    bool operator==(const RenderCacheKey &other) const {
        if (page != other.page || scaleQ() != other.scaleQ() || isTile != other.isTile) {
            return false;
        }
        if (isTile) {
            return rxQ() == other.rxQ() && ryQ() == other.ryQ() &&
                   rwQ() == other.rwQ() && rhQ() == other.rhQ();
        }
        return true;
    }
};

inline size_t qHash(const RenderCacheKey &key, size_t seed = 0) {
    // qHashMulti mixes its arguments (no XOR-collision between equal-but-swapped
    // fields). Only the quantized integers participate — matching operator==.
    if (key.isTile) {
        return qHashMulti(seed, key.page, key.scaleQ(), true,
                          key.rxQ(), key.ryQ(), key.rwQ(), key.rhQ());
    }
    return qHashMulti(seed, key.page, key.scaleQ(), false);
}

class RenderCache : public std::enable_shared_from_this<RenderCache> {
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
    QFuture<void> m_prefetchFuture;

    // AR-6 D5: throttle the memory-pressure syscall. checkMemoryPressure() runs
    // on every getOrRender()/getOrRenderTile() — calling GlobalMemoryStatusEx
    // per tile during a scroll is a needless syscall storm. Only poll the OS
    // once per MemoryPressurePollInterval calls.
    static constexpr int MemoryPressurePollInterval = 32;
    QAtomicInt m_memoryPressureTick{0};

    // Tier 1: Metadata
    int m_pageCount = 0;
    QHash<int, std::shared_future<QSizeF>> m_pageSizes;

    // Tier 2: Rendered Pages — intrusive O(1) LRU (AR-6 D2).
    // m_lruOrder holds keys most-recently-used at the FRONT, oldest at the BACK.
    // Each cache entry stores an iterator into m_lruOrder so touch (move-to-front)
    // and evict (pop-back) are O(1) — the previous QList::removeOne+prepend was
    // O(n) per hit, i.e. O(n²) while scrolling.
    std::list<RenderCacheKey> m_lruOrder;
    struct CacheValue {
        QImage image;
        qint64 bytes = 0;
        std::list<RenderCacheKey>::iterator lruIt;
    };
    QHash<RenderCacheKey, CacheValue> m_renderedPages;
    qint64 m_totalBytes = 0;

    // O(1) LRU helpers — caller must hold the write lock.
    void touchLru(const RenderCacheKey &key, CacheValue &value);
    void insertLocked(const RenderCacheKey &key, const QImage &image);

    // Tier 3: Text Layer
    QHash<int, QString> m_textLayer;
};
