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
#include <functional>
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

    // Async render request (PARITY-SCORECARD 2026-09-30 §4 row 16 — the
    // thumbnail rail's off-GUI path). Cache hit: onRendered is invoked INLINE
    // (on the caller's thread) and true is returned. Miss: the render is
    // scheduled on the SAME machinery prefetchViewport uses — the current
    // m_prefetchCancelToken epoch, registration in m_inFlightPrefetches (so
    // clear()/~RenderCache cancel + join it before any renderer is retired,
    // EC06), lowest worker priority, insert into the cache under the lock —
    // and false is returned. On completion onRendered is invoked ON THE WORKER
    // THREAD (consumers must marshal to their own thread; the thumbnail rail
    // hops via queued invokeMethod). If the token epoch advanced while the
    // render ran (clear()/prefetch supersession — the document-changed case)
    // the result is discarded and onRendered is NOT invoked: a stale render is
    // never delivered to the consumer. Unlike prefetchViewport this does NOT
    // bump the cancel token per request — a thumbnail grid fires N concurrent
    // requests and none may cancel the others.
    //
    // Cancellation invariant (TOKEN-EPOCH-DELIVERY): delivery and insertion
    // are deliberately asymmetric — a superseded render may already have
    // INSERTED its bitmap without DELIVERING it (getOrRender inserts; the
    // epoch re-check then skips onRendered). That is safe by construction,
    // not by caller discipline: the cache is keyed by page+scale of the
    // CURRENT document only, and every document change routes through
    // clear(), which JOINS this worker (drainPrefetches) BEFORE wiping — so
    // a cancelled render can leave a FRESH entry that a later getOrRender
    // serves, but a STALE entry can never survive the wipe and nothing is
    // ever misattributed to new content. Pinned by TestThumbnailOffGui
    // clearCancelsInFlightRenderJoinsAndDeliversNothing (pin 3).
    bool renderPageAsync(int page, qreal scale, IPdfRenderer* renderer,
                         std::function<void(const QImage&)> onRendered);

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
    // EC06: ALL in-flight prefetch futures, not just the newest. Superseding a
    // prefetch must not orphan the previous worker while it can still be
    // inside renderer->renderPage (each worker captures a raw IPdfRenderer*):
    // drainPrefetches() — used by BOTH clear() and ~RenderCache — cancels and
    // waits for every retained future, so a renderer can only be retired after
    // all of its prefetch work has actually finished. Guarded by m_lock
    // (workers never touch this list — they communicate only through the
    // cancel token and the cache locks).
    QList<QFuture<void>> m_inFlightPrefetches;
    void drainPrefetches();

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
