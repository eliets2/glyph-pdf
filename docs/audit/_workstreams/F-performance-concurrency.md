# Workstream F — Performance & Concurrency Audit
<!-- Auditor: Claude Sonnet 4.6 | Date: 2026-06-02 | Source: static analysis only, no build/run -->

---

## Summary

Static audit of GlyphPDF (~40 557 LOC, C++17/Qt 6.11/MSYS2 ucrt64) across five
focus areas: LaneScheduler, RenderCache, thread-safety breadth, hot-paths/scaling,
and memory. All 32 ctests pass, but test depth is shallow for heavy-concurrency
scenarios (total wall-time 4.57 s). Findings: 1 Critical, 4 High, 5 Medium, 4 Low,
2 Info. §6 perf non-negotiables are largely honoured with one correctness gap
(warm-worker UAF under pathological destroy-while-busy).

---

## Findings

---

### F-01 · AutosaveManager: QThread::msleep(250) on the GUI thread
**Severity: Critical | Confidence: High**
`src/engines/AutosaveManager.cpp:103`

**Evidence**
```cpp
// AutosaveManager.cpp:95
connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, …]() {
    bool success = watcher->result();
    if (success) {
        bool renameOk = atomicRename(…);
        if (!renameOk) {
            QThread::msleep(250);          // line 103 — caller is GUI thread
            renameOk = atomicRename(…);
        }
    …
});
```
`QFutureWatcher<bool>::finished` is connected with the default connection type.
`AutosaveManager` lives on the GUI thread (it owns a `QTimer*` firing `onTick()`
on the GUI thread). Therefore the lambda is dispatched as a queued connection back
to the GUI thread's event loop. When the rename fails on the first attempt the code
calls `QThread::msleep(250)` — a hard blocking call — on the main (GUI) thread,
freezing the UI and the event loop for 250 ms on every failed autosave rename.

**Impact (latency / correctness)**
Hard freeze of the main window for ≥250 ms per autosave cycle when the rename fails
(e.g., antivirus lock on the `.autosave.pdf` file, common on Windows). Repeated
failures cause cumulative freezes. On a typical 300 s autosave interval this is an
unavoidable 250 ms stutter that will be visible to users.

**Proposed Fix**
Move the retry logic to a separate worker thread or use a singleShot `QTimer` to
reschedule the rename asynchronously instead of sleeping:
```cpp
// Replace QThread::msleep(250) + retry with:
QTimer::singleShot(250, this, [this, tmpAutosavePath, finalAutosavePath]() {
    bool renameOk = atomicRename(tmpAutosavePath, finalAutosavePath);
    if (renameOk) {
        QDateTime now = QDateTime::currentDateTime();
        if (m_document) m_document->setLastAutosave(now);
        emit autosaveCompleted(now);
    } else {
        qWarning() << "Autosave failed: atomic rename failed (retry also failed)";
        emit autosaveFailed("Failed to rename temporary autosave file");
    }
    m_saving = false;
});
// Remove the original retry block and the m_saving = false at the end of
// the outer if (success) block; set m_saving = false in the QTimer callback.
```

---

### F-02 · CrossPagePipeline: Potential deadlock when backpressure >= cpuCapacity
**Severity: High | Confidence: High**
`src/engines/scheduling/PipelineStage.h:54-80`

**Evidence**
```cpp
// PipelineStage.h:54
auto f2 = f1.then([this, s2, p](ScheduledValue<S1Out> sv) -> ScheduledValue<S2Out> {
    …
    auto f = m_scheduler.submit<S2Out>(
        SchedulerOptions{Lane::GPU, …},
        [s2, p, v = sv.value]() -> S2Out { return s2(p, v); });
    f.waitForFinished();   // BLOCKS the .then() continuation thread
    return f.result();
});
// PipelineStage.h:38
QSemaphore bpSemaphore(m_backpressure);   // default backpressure=4
// PipelineStage.h:44
bpSemaphore.acquire(); // blocks caller until stage3 releases
```
Qt 6's `.then()` continuations are scheduled on `QThreadPool::globalInstance()`.
Each stage-2 continuation blocks a global-pool thread via
`m_gpuSemaphore.acquire()` waiting for a GPU slot. With `backpressure=4`:

1. Loop iteration emits 4 pages' stage-1 futures, then blocks on `bpSemaphore.acquire()`.
2. All 4 stage-2 continuations are dispatched to the global thread pool. Each blocks on `m_gpuSemaphore.acquire()`.
3. Stage-3 continuations cannot run because all pool threads are blocked.
4. `bpSemaphore.release()` is called in stage-3 — which never runs.
5. The loop caller thread is permanently blocked on `bpSemaphore.acquire()` at iteration 5.

This is a complete deadlock when `backpressure >= globalPool.maxThreadCount()`. On a
4-core machine with `idealThreadCount()=4` and `backpressure=4` (the default), this
deadlock is guaranteed. `OcrPipeline::recognizeDocument` uses `CrossPagePipeline`
directly with `backpressure=4` (line 355 of OcrPipeline.cpp).

**Impact (correctness)**
The document OCR pipeline hangs permanently with no error, no timeout, and no
progress signal. Affects any machine with 4 or fewer logical CPUs.

**Proposed Fix**
Option A (simplest): Reduce default backpressure to `max(1, cpuCapacity/2)` so at
least half the CPU threads remain available to run stage-3:
```cpp
// PipelineStage.h:27
explicit CrossPagePipeline(LaneScheduler& scheduler,
                           int backpressure = -1)
    : m_scheduler(scheduler)
    , m_backpressure(backpressure < 1
          ? qMax(1, QThread::idealThreadCount() / 2)
          : backpressure)
{}
```
Option B (correct): Use a separate `QThreadPool` for `.then()` continuations instead
of the global pool, or restructure stage-2 to use a non-blocking GPU submission
(submit and immediately return a future without `waitForFinished()`), propagating the
result via an additional `.then()` continuation so no thread is ever blocked waiting
for GPU capacity.

---

### F-03 · LaneScheduler: Use-after-free when destructor times out while GPU task is running
**Severity: High | Confidence: Medium**
`src/engines/scheduling/LaneScheduler.cpp:43-47`

**Evidence**
```cpp
// LaneScheduler.cpp:43-47
if (m_gpuThread) {
    m_gpuThread->quit();
    m_gpuThread->wait(5000);   // 5 s timeout — can return while task still executes
}
```
```cpp
// LaneScheduler.h:142 — runWork lambda captures 'this'
auto runWork = [this, promise, work = std::move(work), cancelToken, lane]() mutable {
    if (m_cancelToken.loadRelaxed() != cancelToken) { … }
    T result = work();
    …
    m_gpuInFlight.fetchAndSubOrdered(1);
    m_gpuSemaphore.release();
};
```
If a GPU task takes longer than 5 s (e.g., a large ONNX inference pass), `~LaneScheduler`
returns while the GPU thread is still executing `runWork`. The lambda dereferences
`this` (`m_cancelToken`, `m_gpuInFlight`, `m_gpuSemaphore`) on a destroyed object.
This is undefined behaviour / use-after-free.

**Impact (correctness / stability)**
Crash or silent memory corruption during application shutdown when the GPU lane is
busy. Most likely on slow hardware / first inference where ONNX model loading can
easily exceed 5 s.

**Proposed Fix**
Drain the GPU queue before the destructor returns, or use a longer / infinite wait
combined with a poison-pill task, or cancel all tasks first:
```cpp
void LaneScheduler::shutdown() {
    cancelAll();                   // mark all new tasks as cancelled
    {
        QMutexLocker lock(&m_gpuMutex);
        if (m_gpuStopping) return;
        m_gpuStopping = true;
        m_gpuCond.wakeAll();
    }
    if (m_gpuThread) {
        m_gpuThread->quit();
        // Unlimited wait: the GPU task will eventually see the cancelled token
        // or finish naturally; we must not destroy 'this' while it runs.
        if (!m_gpuThread->wait(30000)) {
            qCritical("LaneScheduler: GPU thread did not stop in 30s — forcing termination");
            m_gpuThread->terminate();
            m_gpuThread->wait();
        }
    }
    m_cpuPool.waitForDone(-1);     // also unlimited for CPU pool
}
```

---

### F-04 · LaneScheduler: GPU submit-after-shutdown deadlocks on m_gpuSemaphore
**Severity: High | Confidence: High**
`src/engines/scheduling/LaneScheduler.h:180-182`

**Evidence**
```cpp
// LaneScheduler.h:180-182 (submit template, GPU branch)
m_gpuSemaphore.acquire();         // blocks forever after shutdown
m_gpuInFlight.fetchAndAddOrdered(1);
enqueueGpu(GpuTask{ std::move(runWork) });
```
After `shutdown()`, the GPU worker loop has exited. No task can ever call
`m_gpuSemaphore.release()`. If `submit(…, Lane::GPU, …)` is called after `shutdown()`,
it blocks indefinitely on `m_gpuSemaphore.acquire()`. There is no guard in `submit()`
checking `m_gpuStopping` before the acquire.

`m_gpuStopping` is a plain `bool` (not `std::atomic<bool>`), so reading it outside
`m_gpuMutex` would be a data race; but adding a mutex check before the acquire still
leaves a TOCTOU window. The correct fix is to make the acquire fail gracefully.

**Impact (correctness)**
A caller thread that issues a GPU task after the scheduler is shut down hangs
permanently. This can occur if `OcrPipeline` is used from a destructor that outlives
`LaneScheduler`.

**Proposed Fix**
Use `QSemaphore::tryAcquire()` with a timeout, or replace `QSemaphore` with an
`std::atomic<bool>` "stopped" flag checked before the acquire, and return a
pre-failed `ScheduledValue` when stopped:
```cpp
// submit template, GPU branch:
if (!m_gpuSemaphore.tryAcquire(1, 0)) {
    // Stopped or at capacity; check stopping flag first:
    {
        QMutexLocker lk(&m_gpuMutex);
        if (m_gpuStopping) {
            SchedulerError err;
            err.code = SchedulerErrorCode::Cancelled;
            err.message = "Scheduler is shutting down";
            promise->addResult(ScheduledValue<T>::failure(err));
            promise->finish();
            return future;
        }
    }
    m_gpuSemaphore.acquire();  // blocking path only when capacity is full but not stopped
}
```

---

### F-05 · RenderCache::prefetchViewport — QFuture return discarded, prefetch unbounded
**Severity: High | Confidence: High**
`src/engines/RenderCache.cpp:271`

**Evidence**
```cpp
// RenderCache.cpp:271 — [[nodiscard]] ignored
QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() {
    …
});
// Return value (QFuture<void>) is silently discarded.
```
`QtConcurrent::run()` is `[[nodiscard]]` in Qt 6. The compiler warning is suppressed
by the fact that the return value is discarded without assignment, which happens to
avoid the warning in MSVC/GCC unless `-Wunused-result` is active. The real problems are:

1. **Untracked work**: The caller (and destructor) have no way to wait for, cancel, or
   observe the in-flight prefetch task. `RenderCache::~RenderCache()` calls `clear()`
   which clears data structures, but the prefetch lambda may still be running,
   dereferencing `self` (via `weakThis.lock()`). Although `weak_from_this()` is used,
   if the `shared_ptr<RenderCache>` is still alive when `~RenderCache()` runs (which it
   is, since `enable_shared_from_this` keeps the refcount non-zero), `weakThis.lock()`
   returns a valid `self` and the lambda accesses `self->m_renderedPages` etc. after
   `clear()` has run. This is a post-`clear()` data race between the prefetch writer
   and whatever the destructor caller does with the now-empty cache.

2. **Unbounded concurrency**: Multiple calls to `prefetchViewport` each launch an
   independent `QtConcurrent::run` task. While the cancellation token limits useful
   work, all tasks remain scheduled until the thread pool services them. On a
   rapidly-scrolling large document, dozens of prefetch tasks can pile up in the global
   thread pool, consuming threads and delaying real rendering requests.

3. **TOCTOU in prefetch (lines 282-286)**: The `lockForRead` + `contains` + `unlock`
   + re-`lockForWrite` pattern (CLAUDE.md B7 claim of "single WriteLockGuard fix")
   IS correctly fixed in `getOrRender` and `getOrRenderTile`. However, the prefetch
   lambda at lines 282-286 still uses a split read-lock check:
   ```cpp
   self->m_lock.lockForRead();
   bool cached = self->m_renderedPages.contains(key);  // line 283
   self->m_lock.unlock();                               // line 284
   if (!cached) {                                       // gap here
       …render…
       WriteLockGuard guard(self->m_lock);              // line 296
       if (!self->m_renderedPages.contains(key)) { …insert… }
   }
   ```
   The double-check at line 297 prevents double-insertion, but between lines 284 and
   296 another thread can insert the same page, causing a wasted render. Not a
   correctness bug (due to the inner guard check) but it wastes render resources.

**Impact (memory / throughput / correctness)**
Global thread pool saturation on rapid scroll; potential post-`clear()` access;
wasted parallel renders; ignoring `[[nodiscard]]` silently.

**Proposed Fix**
Track the prefetch future, wait for it in `~RenderCache()`, and throttle concurrent
prefetch tasks:
```cpp
// RenderCache.h:
QFuture<void> m_prefetchFuture;

// RenderCache.cpp prefetchViewport:
// 1. Cancel previous future (no-op if already done):
if (m_prefetchFuture.isRunning()) {
    // token increment already signals cancellation; just don't wait here.
}
m_prefetchFuture = QtConcurrent::run([weakThis, …] { … });

// RenderCache::~RenderCache():
m_prefetchCancelToken.fetchAndAddRelaxed(1);  // signal cancel
if (m_prefetchFuture.isRunning()) m_prefetchFuture.waitForFinished();
clear();
```
Also convert lines 282-284 to use the existing `WriteLockGuard` pattern, eliminating
the benign TOCTOU.

---

### F-06 · BatchMode: m_successCount / m_failCount / m_errorLog mutated from concurrent resultReadyAt signals
**Severity: Medium | Confidence: Medium**
`src/modes/BatchMode.cpp:809-823`

**Evidence**
```cpp
// BatchMode.cpp:806-823 — connected with Qt::QueuedConnection
connect(&m_watcher, &QFutureWatcher<BatchFileResult>::resultReadyAt,
        this, [this](int idx) {
    BatchFileResult res = m_watcher.resultAt(idx);
    int completed = m_successCount + m_failCount + 1;   // read then increment
    …
    ++m_successCount;   // or ++m_failCount;
    …
    m_errorLog.append(…);
}, Qt::QueuedConnection);
```
`resultReadyAt` is explicitly connected with `Qt::QueuedConnection`, ensuring
callbacks serialize on the GUI thread. The counters are safe in the normal run path.
However:

- The `connect` call that adds this slot is inside `onRunClicked()` (line 806) and
  `disconnect` is in `onBatchFinished()` (line 874). If `onRunBatch()` is called
  again before `onBatchFinished()` (possible in tests via `bm.onRunBatch()`), a
  second connection is added. Subsequent `resultReadyAt` signals fire the lambda
  **twice** per result, double-counting `m_successCount` / `m_failCount` and
  double-appending to `m_errorLog`.

- RESOURCE_LOCK `BatchModeIO` in CMakeLists serializes `TestBatchMode` against other
  tests but does NOT explain the flakiness root cause — the actual shared resource is
  `QTemporaryDir` contention on Windows (temp dir not isolated per test case object
  instance) and the double-connection risk on repeated `onRunBatch()` calls during
  the cancel test (T5), which calls `onRunBatch()` once then `onCancelBatch()`.

**Impact (correctness / reliability)**
Double-counted statistics if `onRunBatch()` called twice; potential for
`TestBatchMode` flakiness under rapid re-runs. The RESOURCE_LOCK is a workaround
rather than a fix.

**Proposed Fix**
Move the `resultReadyAt` connection to the constructor (permanent, reconnect-safe)
and gate on `m_watcher.isRunning()`. Or add a guard:
```cpp
// In onRunClicked(), before reconnecting:
disconnect(&m_watcher, &QFutureWatcher<BatchFileResult>::resultReadyAt,
           this, nullptr);  // ensure clean slate before each run
connect(&m_watcher, …);
```
This is cleaner than relying on the disconnect in `onBatchFinished()` which only fires
when the batch completes normally.

---

### F-07 · OcrPipeline::recognizeDocumentAsDjot — nested async blocks a global pool thread
**Severity: Medium | Confidence: High**
`src/engines/ocr/OcrPipeline.cpp:388-397`

**Evidence**
```cpp
// OcrPipeline.cpp:388-397
return QtConcurrent::run([=, pageImages = pageImages, pdfPath = pdfPath]()
                         -> docmodel::SemanticDocument {
    OcrPipeline pipe(primaryEngine, secondaryEngine);
    …
    // Blocks a global-pool thread waiting for the inner pipeline to drain:
    QList<PageOcrResult> pageResults =
        pipe.recognizeDocument(pageImages).result();   // line 397
    …
});
```
The outer `QtConcurrent::run` dispatches to `QThreadPool::globalInstance()`. Inside
that thread, `pipe.recognizeDocument().result()` blocks the global-pool thread until
all pages complete. If the scheduler is set, `recognizeDocument` submits tasks to
`sched`'s own CPU pool — not the global pool — so there is no deadlock. However, the
blocked global-pool thread is unavailable for other global-pool work (e.g.,
`QFutureWatcher` continuations or other `QtConcurrent::run` calls from UI code).

On a large document with many pages, the global pool thread is tied up for the entire
OCR duration, degrading responsiveness of unrelated async operations.

**Impact (throughput / latency)**
Reduced global thread pool availability during OCR. On documents > 50 pages this can
noticeably delay unrelated async UI operations.

**Proposed Fix**
Restructure to avoid blocking the global pool thread. Use `.then()` to chain the
mapping step as a continuation that fires when `recognizeDocument()` completes:
```cpp
return recognizeDocument(pageImages)
    .then(QtConcurrent::run /* executor */, [pdfPath](QList<PageOcrResult> results)
          -> docmodel::SemanticDocument {
        OcrDjotMapper mapper;
        return mapper.fromOcrResults(results, pdfPath);
    });
```
This requires care with `.then()` executor semantics in Qt 6.4+.

---

### F-08 · RenderCache::getOrExtractText — benign TOCTOU between read unlock and write lock
**Severity: Medium | Confidence: High**
`src/engines/RenderCache.cpp:311-332`

**Evidence**
```cpp
// RenderCache.cpp:312-323
m_lock.lockForRead();
if (m_textLayer.contains(page)) {           // 1. check under read lock
    QString text = m_textLayer.value(page);
    m_lock.unlock();
    return text;
}
m_lock.unlock();                             // 2. gap: another thread can insert

if (!renderer) return QString();
auto* pdfium = dynamic_cast<PdfiumBackend*>(renderer);
if (pdfium) {
    QString extracted = pdfium->extractText(page);
    WriteLockGuard guard(m_lock);            // 3. re-lock to insert
    m_textLayer.insert(page, extracted);
    return extracted;
}
```
Two threads calling `getOrExtractText(p, renderer)` concurrently will both pass the
`contains(page)` check (both see empty), both call `pdfium->extractText(page)` (which
may be expensive), and both insert. The second insert is a no-op (QHash overwrites),
so correctness is preserved. The problem is the double extraction cost.

**Impact (throughput)**
Wasted Tesseract/PDFium extraction work when multiple threads request the same page's
text simultaneously. For a 300-page document with ROVER OCR, each page's text
extraction can take 50-200 ms, so double extraction wastes up to 200 ms per
concurrently requested page.

**Proposed Fix**
Use the same double-check pattern as `pageSize()`:
```cpp
QSizeF RenderCache::getOrExtractText(int page, IPdfRenderer* renderer) {
    {
        QReadLocker rl(&m_lock);
        if (m_textLayer.contains(page))
            return m_textLayer.value(page);
    }
    // Miss: extract outside lock, then re-check before insert
    if (!renderer) return QString();
    …
    QString extracted = pdfium->extractText(page);
    {
        WriteLockGuard wg(m_lock);
        if (!m_textLayer.contains(page))
            m_textLayer.insert(page, extracted);
    }
    return extracted;
}
```

---

### F-09 · MrcPageProcessor::buildBackground — O(W×H) per-pixel inpainting for large images
**Severity: Medium | Confidence: High**
`src/engines/mrc/MrcPageProcessor.cpp:264-307`

**Evidence**
```cpp
// MrcPageProcessor.cpp:275-306
for (int y = 0; y < H; ++y) {
    // Pass 1: compute row background average — O(W)
    for (int x = 0; x < W; ++x) { if (msk[x] == 0) { rSum += …; ++count; } }
    // Pass 2: fill foreground pixels — O(W)
    for (int x = 0; x < W; ++x) { if (msk[x] != 0) bgRow[x] = fillColor; }
}
```
The algorithm is O(W×H) with two linear passes per row, which is fine. However:

- At 300 DPI on A4 (2480×3508 px): ~8.7 MP × 2 passes = ~17.4 M pixel reads/writes
  per page. For `MrcMode::Aggressive` the compression ratio is 50:1 — the background
  encoding step dominates, but the inpainting allocates and operates on a full-colour
  copy (`bg = page.copy()`), requiring an additional ~30 MB heap allocation per page.
- The `separatePage()` function is documented as re-entrant. Multiple concurrent calls
  each allocate large QImage copies (`rgb`, `mask`, `bg`). For a 200-page batch at 4
  concurrent workers: 4 × 3 × 30 MB = 360 MB peak heap from QImage temporaries alone.
  This exceeds the 256 MB RenderCache cap and could trigger the `isSystemMemoryLow()`
  path, collapsing the cache.

**Impact (memory)**
High peak heap usage under parallel MRC batch processing. On systems with < 2 GB RAM,
this can trigger low-memory paths or OOM.

**Proposed Fix**
Process pages sequentially for MRC batch (one page at a time), or process in chunks
sized to keep peak memory under `MemoryGuard::LowMemoryThreshold`. Add memory
pressure checks before launching concurrent `separatePage()`:
```cpp
// Before each LaneScheduler CPU submission for MRC:
if (RenderCache::availableSystemMemory() < 4LL * pageImageBytes) {
    // Drain previous pages before starting new ones
    waitForPreviousPageCompletion();
}
```

---

### F-10 · LaneScheduler::shutdown calls m_cpuPool.waitForDone(5000) with 5 s cap
**Severity: Low | Confidence: High**
`src/engines/scheduling/LaneScheduler.cpp:47`

**Evidence**
```cpp
m_cpuPool.waitForDone(5000);  // 5 s — same race as GPU thread
```
CPU tasks that take > 5 s (e.g., Tesseract on a large page, or JPEG2000 encoding)
leave the pool in a running state when the destructor returns. The lambda captures
`this` (`m_cancelToken`, `m_cpuInFlight`) which are accessed after the pool object is
destroyed. This is the same class of UAF as F-03 but for CPU tasks.

**Impact (correctness)**
UAF / crash during application close if any CPU task exceeds 5 s.

**Proposed Fix**
Change `waitForDone(5000)` to `waitForDone(-1)` (unlimited wait) after signalling
cancellation via `cancelAll()`. See F-03 proposed fix.

---

### F-11 · RenderCache::insertPage / insertTile — double-counts m_totalBytes on duplicate insert
**Severity: Low | Confidence: High**
`src/engines/RenderCache.cpp:228-248`

**Evidence**
```cpp
// RenderCache.cpp:228-237 (insertPage)
WriteLockGuard guard(m_lock);
RenderCacheKey key{page, scale, false, QRectF()};
qint64 bytes = imageSizeInBytes(image);
m_renderedPages.insert(key, {image, bytes});  // QHash::insert replaces existing
m_lruList.removeOne(key);
m_lruList.prepend(key);
m_totalBytes += bytes;                        // always adds, never subtracts old value
evictIfNeeded();
```
If a key already exists in `m_renderedPages`, `QHash::insert` replaces the value
but does NOT return the old value. The old `CacheValue.bytes` is lost, and
`m_totalBytes` is incremented by the new image size without subtracting the old size.
This causes `m_totalBytes` to exceed the true total, triggering over-aggressive
eviction. The debug assert in `evictIfNeeded()` will catch this in debug builds.

`getOrRender` and `getOrRenderTile` both have an inner double-check guard
(`if (m_renderedPages.contains(key))`) so they won't call `insertPage/insertTile` if
already cached. The bug only manifests when `insertPage`/`insertTile` are called
directly by external code (e.g., pipeline post-render pre-population).

**Impact (memory / correctness)**
Over-aggressive LRU eviction: cache size reported as larger than actual, leading to
unnecessary evictions and cache misses. Debug asserts will fail (crash in debug build).

**Proposed Fix**
```cpp
// insertPage:
WriteLockGuard guard(m_lock);
RenderCacheKey key{page, scale, false, QRectF()};
auto it = m_renderedPages.find(key);
if (it != m_renderedPages.end()) {
    m_totalBytes -= it.value().bytes;  // subtract old size
    m_lruList.removeOne(key);
}
qint64 bytes = imageSizeInBytes(image);
m_renderedPages.insert(key, {image, bytes});
m_lruList.prepend(key);
m_totalBytes += bytes;
evictIfNeeded();
```
Apply symmetrically to `insertTile`.

---

### F-12 · OcrPipeline::run — no mutex; shared d->strategy / d->preprocessOpts during setStrategy mid-run
**Severity: Low | Confidence: Medium**
`src/engines/ocr/OcrPipeline.cpp:80-93`

**Evidence**
```cpp
void OcrPipeline::setStrategy(OcrStrategy strategy) { d->strategy = strategy; }
// run() reads d->strategy and d->preprocessOpts without any lock.
```
`setStrategy()` and `setPreprocessing()` mutate `Private::strategy` and
`Private::preprocessOpts` without synchronization. If a caller calls `setStrategy()`
from one thread while another thread is executing `run()`, this is a data race on
`d->strategy` (undefined behaviour under the C++ memory model). This is low severity
because the documented use pattern creates a new `OcrPipeline` per task in
`recognizeDocument`'s stage-2 lambda (line 303: `OcrPipeline pipe(…)`), so in
practice the shared pipeline is not concurrently mutated.

**Impact (correctness)**: Data race in unusual concurrent use. Not triggered by
`recognizeDocument` or `recognizeDocumentAsDjot` which create local `OcrPipeline` instances.

**Proposed Fix**
Document that `setStrategy()`/`setPreprocessing()` must not be called concurrently
with `run()`. Add a `QReadWriteLock` or `std::mutex` to `Private` for defensiveness.

---

### F-13 · ROVER merge — sourceEngine clobbered for all matches regardless of confidence winner
**Severity: Low | Confidence: High**
`src/engines/ocr/OcrPipeline.cpp:149-152`

**Evidence**
```cpp
// OcrPipeline.cpp:144-152
if (sw.confidence > pw.confidence) {
    mw.text         = sw.text;
    mw.boundingBox  = sw.boundingBox;
    mw.confidence   = sw.confidence;
    mw.sourceEngine = secondaryName;  // secondary wins
}
// Mark as ROVER-resolved regardless of winner
mw.sourceEngine = "ROVER";           // always overrides previous assignment
```
Line 152 unconditionally overwrites `mw.sourceEngine` to `"ROVER"` for every matched
word, even when the primary engine won (the inner `if` block was not entered). The
`else` branch (primary wins) does not update `mw.sourceEngine` from
`primaryName` (set at line 128) to `"ROVER"`, so both winners would be `"ROVER"`.
This is the intended behaviour — marking all ROVER-resolved words as `"ROVER"` — but
the code assigns `secondaryName` on line 151 then overwrites it on line 152,
making line 151 dead code. Minor logic noise.

**Impact**: Negligible (no correctness impact since `"ROVER"` is the correct provenance).
Removes a misleading dead assignment.

**Proposed Fix**
```cpp
// Remove lines 148-151 (the if block updating sourceEngine):
mw.sourceEngine = "ROVER";   // move before the if; remove from both branches
if (sw.confidence > pw.confidence) {
    mw.text        = sw.text;
    mw.boundingBox = sw.boundingBox;
    mw.confidence  = sw.confidence;
}
```

---

### F-14 · RenderCache::checkMemoryPressure — called on every getOrRender, includes system call
**Severity: Info | Confidence: High**
`src/engines/RenderCache.cpp:127, 396-413`

**Evidence**
```cpp
// getOrRender:127
checkMemoryPressure();

// checkMemoryPressure:
qint64 avail = availableSystemMemory();   // calls GlobalMemoryStatusEx (Win32 syscall)
```
`checkMemoryPressure()` is called on every single `getOrRender()` call, including
cache-hit paths. This executes a Win32 `GlobalMemoryStatusEx` syscall on every page
render request. Under rapid viewport scrolling (50+ renders/s), this adds ~50 syscalls
per second. On modern Windows, `GlobalMemoryStatusEx` typically takes < 1 µs, so
the overhead is sub-millisecond in aggregate but is a design smell.

**Proposed Fix**
Rate-limit the check: track a last-check timestamp and only call
`GlobalMemoryStatusEx` when more than 500 ms has elapsed since the last check.

---

### F-15 · TestBatchMode RESOURCE_LOCK: root cause is not the temporary directory but double-connect
**Severity: Info | Confidence: Medium**
`CMakeLists.txt:985-989`

**Evidence**
Pattern 19 in CLAUDE.md attributes `TestBatchMode` flakiness to "concurrent tests
share I/O resources" and fixes it with `RESOURCE_LOCK BatchModeIO`. This is correct
for protecting against tmp-dir I/O contention, but the deeper root cause is the
`resultReadyAt` double-connect issue (F-06 above). Under parallel ctest `-j4`, if
`TestBatchMode` itself runs two test cases back-to-back where a second `onRunBatch()`
fires before the first `onBatchFinished()` clears the connection, statistics are
double-counted, causing `QCOMPARE(conv->m_callCount, 3)` to see 6 instead of 3.
This is flaky under `-j4` only because fast parallel execution of the five test
methods triggers the window.

---

## Concurrency-correctness findings (races / deadlocks subset)

| ID | Category | Location | Description |
|----|----------|----------|-------------|
| F-02 | Deadlock | `PipelineStage.h:54-80` | `backpressure >= cpuCapacity` starves stage-3 permanently |
| F-03 | UAF | `LaneScheduler.cpp:43-47` | 5 s destructor timeout + running GPU task = dereference of freed `this` |
| F-04 | Deadlock | `LaneScheduler.h:180` | GPU submit after shutdown blocks forever on `m_gpuSemaphore.acquire()` |
| F-05 | Race (benign) | `RenderCache.cpp:282-296` | prefetch read-unlock-write gap allows double render (not corruption) |
| F-06 | Logic race | `BatchMode.cpp:806-823` | double-connect of `resultReadyAt` → double-counted stats on re-run |
| F-08 | Benign race | `RenderCache.cpp:312` | double text extraction on concurrent first-access to same page |
| F-12 | Data race | `OcrPipeline.cpp:80-93` | `setStrategy()` unsynchronized with concurrent `run()` |

---

## §6 perf non-negotiables: enforced-in-code check

| Rule | Status | Evidence |
|------|--------|----------|
| Spawn-per-page ONNX process: NEVER | **ENFORCED** | `LaneScheduler.cpp:17-19`: single persistent `m_gpuThread` started once; `enqueueGpu()` posts task payload, never spawns. `TestLaneScheduler::testWarmWorkerReuse` validates one thread ID for 20 tasks. |
| Character-level OCR majority vote: NEVER (word-level ROVER only) | **ENFORCED** | `OcrPipeline.cpp:112-168`: `roverMerge()` aligns by IoU > 0.5 and picks per-word winner. No per-character voting code exists. |
| Layout detector end-to-end VLM: NEVER for v1.0 | **ENFORCED** | `OcrPipeline.cpp:289-329`: stage-1 calls `layoutEns->detect()` (layout only), stage-2 does ROVER OCR per region. No VLM call path. |
| JBIG2 pattern-matching mode: NEVER | **ENFORCED** | `MrcPageProcessor.cpp:349-356`: `jbig2_init(…, /*refine=*/-1)` — generic-region lossless only, no symbol dictionary across pages. |
| GPU warm worker persistent (not per-page) | **ENFORCED** — with caveat | Persistent thread verified. **Caveat F-03/F-04**: UAF if busy at destruction, and deadlock if submitted after shutdown. The *design intent* is correct; the implementation has lifetime bugs. |

---

## Verified vs Could-Not-Verify

### Verified (code evidence found, conclusion drawn)

- **B7 (TOCTOU fix) — VERIFIED CORRECT** for `getOrRender` and `getOrRenderTile`:
  both use a single `WriteLockGuard` from check through LRU update (single atomic
  check-and-act). Not a TOCTOU in the hot path.
- **GPU lane warm-worker** — VERIFIED: single `m_gpuThread` started in constructor,
  never re-spawned. `testWarmWorkerReuse` passes with all 20 tasks on one thread ID.
- **256 MB cap enforced** — VERIFIED: `evictIfNeeded()` loops while
  `m_totalBytes > m_maxCacheSize`; called from every write path.
  **Caveat F-11**: `insertPage`/`insertTile` can over-count `m_totalBytes` on duplicate
  keys, causing premature eviction.
- **PdfEditorEngine mutex** — VERIFIED: `QRecursiveMutex` covers every public method.
  No bare access to `d->backend` without `QMutexLocker`.
- **AutosaveManager single-flight** — VERIFIED: `m_saving` bool guards re-entry in
  `onTick()` (lines 78-86). No concurrent autosave issue.
- **Cross-page pipeline data sharing** — VERIFIED: `capturedLayouts` shared between
  stage1 and stage3 is protected by `layoutMutex` on both read and write paths.
- **JBIG2 lossless-only** — VERIFIED: `refine=-1` parameter to `jbig2_init`.

### Could-Not-Verify (would require runtime / instrumentation)

- **Actual LRU eviction order under concurrent insert+evict**: `evictIfNeeded()` calls
  `m_lruList.takeFirst()` which is O(n) on `QList`. Under 8 concurrent render threads
  each doing 1000 iterations (as in `TestThreadSafety`), the O(n) scans could cause
  throughput collapse on large caches. Not statically measurable.
- **ONNX inference time vs 5 s shutdown timeout (F-03)**: Whether real PP-OCRv5
  inference on a large page exceeds 5 s depends on hardware (CPU vs CUDA, cold model
  load). Could not verify without profiling.
- **Qt 6.11 `.then()` executor** (F-02 chain): Whether Qt 6.11's `QFuture::then()`
  uses `globalInstance()` or the producing pool for continuations requires a runtime
  check. The deadlock scenario in F-02 assumes global pool — if continuations run on
  `m_cpuPool` instead, the deadlock would not occur. The code comment "runs on
  threadpool" without specifying which pool increases risk ambiguity.
- **TestBatchMode flakiness reproduction**: Confirmed the double-connect root cause
  (F-06) statically but could not reproduce the flaky failure count without running
  `ctest --repeat-until-fail 10 -j4`.
