# LANE REPORT — #16 wave 2b, thumbnail render off the GUI thread (2026-10-01)

Lane: `feat/thumbnails-offgui` @ worktree `D:/pdf/pdf-w2b-thumbnails`, base
`a4cc1522`. Mission: PARITY-SCORECARD-2026-09-30 §4 row 16 — "Thumbnail render
off the GUI thread (locking done)"; dispatch target
`ThumbnailSidebar.cpp:35-46` renderer → `RenderCache` prefetch path.

Status: **COMPLETE** — all work committed, R7 evidence contract satisfied.

## CHECK FIRST (confirmed NOT done at base)

At `a4cc1522` the thumbnail rail rendered **synchronously on the GUI thread**:

- `ThumbnailSidebar::createThumbWidget` called `m_renderCache->getOrRender(...)`
  inline during widget creation; on a cache miss `getOrRender` calls
  `renderer->renderPage(...)` on the calling (GUI) thread
  (`RenderCache.cpp:224-229`), and the sidebar's `ThumbnailRenderer` adapter
  forwarded to `PdfViewerWidget::renderPage` — a full PDFium page render per
  visible page during first paint of a large document. That is exactly the
  first-paint jank the scorecard row records.
- The row's premise checked out too: `RenderCache::prefetchViewport` (cancel
  token + in-flight future registry + low-priority worker + insert-under-lock)
  had **zero callers** in the tree — machinery built (EC06/AR-6 hardening) and
  waiting for exactly this consumer.

## What was built

| File | Change |
|---|---|
| `src/engines/RenderCache.h/.cpp` | New `renderPageAsync(page, scale, renderer, onRendered)` — the row's "prefetch path" opened up as a general API. Cache hit → callback invoked inline, returns true. Miss → the render is scheduled on the **existing** prefetch machinery verbatim: the current `m_prefetchCancelToken` epoch, registration in `m_inFlightPrefetches` (so `clear()`/`~RenderCache` cancel + join it before any renderer is retired — EC06), `LowestPriority` worker (AR-6 D1), cache insert under the lock via `getOrRender`. If the token epoch advanced mid-render (document changed) the result is **never delivered** to the consumer. Unlike `prefetchViewport` it does not bump the token per request — a thumbnail grid fires N concurrent requests and none may cancel the others. No new generation counter anywhere: staleness is handled entirely by the cache's existing epoch + `clear()`-wipes-after-join ordering. |
| `src/ui/PdfViewerWidget.h/.cpp` | New public `renderPageUncached(page, scaleFactor)` — the R12 checked-size / pixel-budget guard + `QPdfDocument::render` + white-paper compositing, with **no** `m_pageCache` access. `renderPage` was refactored to funnel through it (pixel-identical, pinned). Thread-safety evidence: Qt 6's `QPdfDocument::render` serializes all PDFium access on a process-wide recursive mutex (`Q_GLOBAL_STATIC(QRecursiveMutex, pdfMutex)` in `qpdfdocument.cpp`), so the only shared state this method touches is safe off-GUI. Side benefit: 75-DPI thumbnail renders no longer thrash the viewer's one-slot-per-page cache against the live view. |
| `src/ui/ThumbnailSidebar.cpp` | (1) `ThumbnailRenderer::renderPage` now goes through `renderPageUncached` (thread-safe; no widget-cache state). (2) `createThumbWidget` **never renders on the GUI thread**: cache hit delivered inline; miss scheduled via `renderPageAsync`, the label stays an honest blank sheet (`thumbPending` property, empty text — never fabricated content), and the pixmap arrives via a queued GUI-thread hop contexted on the sidebar (dropped if the sidebar died) with a `QPointer<QLabel>` guard (dropped if virtualization/rebuild retired the label) plus a page+scale slot-property check (belt-and-braces against a delivery racing `deleteLater`). (3) `setViewer` retires the old pipeline in EC06 order — `clear()` (cancel+join) **before** the old renderer is destroyed (was: renderer reset first, a latent UAF for any in-flight worker). (4) Out-of-line destructor drains via `m_renderCache->clear()` so no worker can outlive the renderer/sidebar (plain member destruction order would destroy `m_renderer` before `m_renderCache`). |
| `tests/TestThumbnailOffGui.cpp` + CMake | 8-pin suite (QTest, offscreen, SERIAL, TIMEOUT 300) registered beside `TestThumbnailZoom`. Pins listed below. |
| `docs/audit/evidence-thumbnails-offgui/` | R7 evidence (README + 6 run logs). |

Design decision worth recording: **no duplicate-request coalescing** was added
(scroll churn can queue a re-request for a page whose widget was recreated).
Workers dedupe at insert time (`getOrRender`'s TOCTOU re-check), deliveries are
guarded per-label, and the workload is bounded by the document size and the
32 MB thumbnail cache — adding sidebar-side request state would have recreated
a second generation mechanism, which the mission explicitly excluded.

## Test pins (`TestThumbnailOffGui`)

1. `sidebarFirstPaintIsPlaceholderEvenWithAllWorkersParked` — the jank pin.
   Parks **every** QtConcurrent worker in a semaphore before first paint; an
   unpark `qScopeGuard` declared after the sidebar releases on every exit path
   before the drain-dtor runs (a QTest early-return with the pool parked would
   otherwise hang process exit — found and fixed during bring-up). First paint
   must show only honest placeholders; a synchronous GUI-thread render cannot
   be parked, so the pre-fix tree fills pixmaps here.
2. `thumbnailsArriveAsynchronouslyAfterRelease` — thumbnails arrive via the
   queued hop once workers run.
3. `renderPageAsyncRunsOffThreadAndDelivers` — render provably off the GUI
   thread (thread recorded inside `renderPage`), delivered with the image,
   counted as a miss.
4. `renderPageAsyncCacheHitServedSynchronously` — second request served inline,
   no second render.
5. `clearCancelsInFlightRenderJoinsAndDeliversNothing` — worker pinned inside a
   blocking render; `clear()` from a helper thread demonstrably JOINS it
   (`clearDone` still 0 after 300 ms while blocked), the stale render is never
   delivered, and the cache is left empty (EC06 + token epoch, end to end).
6. `renderPageUncachedMatchesRenderPageAndKeepsGuards` — byte-identical to the
   cached funnel (format-normalized), NaN/zero/out-of-range refused, 1e9 scale
   bounded under the 64 Mpx budget.
7. `sidebarDestructionWithRendersInFlightIsSafe` — 3 create/schedule/destroy
   cycles; no worker may outlive the sidebar.
8. `initTestCase`/`cleanupTestCase` — suite scaffolding.

## R7 evidence (docs/audit/evidence-thumbnails-offgui/)

| Gate | Result | Evidence |
|---|---|---|
| 1. Fail-before (base `a4cc1522` src, tests present) | **1 FAILED / 4 passed** (rc=1, 520 ms). The jank pin fails with `FAIL ... 'placeholderHeld' returned FALSE. (thumbnail pixmap rendered synchronously on the GUI thread during first paint)` — captured with all workers parked, so the pixmap can only be a synchronous GUI-thread render. Arrival + teardown pins pass at base as positive controls. The API pins are not in this run: they reference `renderPageAsync`/`renderPageUncached`, which do not exist at base — the fail-before suite is the exact subset of the final suite that compiles at base. | `red-before-pins-run1.log` |
| 2. Negative control (recorded ONCE) | Scoped revert of ONLY the `createThumbWidget` off-GUI wiring back to synchronous `getOrRender` (the `RenderCache::renderPageAsync` API and `renderPageUncached` seam retained, per the fdf-lane NC precedent): **1 FAILED / 8 passed** (rc=1, 5.6 s). The first-paint pin goes RED again while all machinery/seam pins stay GREEN — the pins detect the wiring gap, not merely the API's existence. The teardown pin stays green under this revert because synchronous wiring has no in-flight renders to outlive; its before-state coverage is the fail-before run. | `negative-control-scoped-revert.log` |
| 3. Pass-after ×3 consecutive SERIAL runs | **9 passed / 0 failed** each: 9.36 s, 12.75 s, 10.19 s (rc=0 ×3). Plus a 4th green run after the negative-control restore (9/0, 18.1 s). No flakes observed; no re-runs needed. | `green-after-fix-run{1,2,3}.log`, `green-after-fix-run4-post-negative-control.log` |
| Ripple sanity (serial) | `TestThumbnailZoom` 5/0 (12 ms) · `TestThreadSafety` 12/0 (20.8 s) · `TestPerformance` 8/0 (0.24 s) · `TestRenderGuards` 10/0 (12.5 s; a rerun confirmed rc=0 after a loop-script grep artifact on the first run's output — both runs' totals identical). | `build-rel/lane-cache/ripple-*.log` (session transcript) |

No existing test was weakened; `TestThumbnailZoom`, `TestThreadSafety`,
`TestPerformance`, `TestRenderGuards` all pass unmodified.

## Build results (Release, ucrt64-first PATH, this worktree's build-rel, `-j 2` cap)

- Configure: runbook flags exactly (LTO/TESSERACT/LIBSECRET ON, fixtures OFF, fuzz OFF). Clean.
- Phase-A (base src) targeted build of the new suite: BUILD_RC=0.
- Gate-targets build (fix applied; `TestThumbnailOffGui TestThumbnailZoom TestThreadSafety TestPerformance TestRenderGuards`): BUILD_RC=0.
- NC scoped-revert rebuild + post-NC restore rebuild: BUILD_RC=0 ×2.
- Full project build: **BUILD_RC=0** — `cmake --build build-rel --config Release -- -k 0 -j 2` completed all 895 targets on the final tree (two earlier background build attempts were SIGKILLed externally under co-tenant load at ~[682/895] and mid-link; ninja resumed incrementally and the final run closed the remaining 74 targets with exit code 0. No source consequence; the lua `-Wstringop-overflow` warnings in the log are pre-existing third-party noise).
- Runtime DLLs: staged into `build-rel` by the existing `stage_runtime_dlls` ALL target (`third_party/pdfium/bin/pdfium.dll`, `third_party/podofo/install/bin/libpodofo.dll`, Qt/toolchain); nothing copied from other worktrees. New test target needed the standard per-exe qoffscreen deploy + the ALL-target staging to be built before its exe can run (exit 127 the first time the targeted build skipped `stage_runtime_dlls`).

## Known limits / behavior notes (shipped scope)

- `renderPageAsync`'s worker calls `getOrRender`, which inserts the render into the cache BEFORE the post-render token check; a cancelled render can therefore leave a page-content-keyed entry that the accompanying `clear()` wipes (drain joins before the wipe). Delivery to the consumer is strictly gated. Documented at the call site.
- `clear()`/`drainPrefetches()` joins in-flight workers on the GUI thread on document change — bounded by at most one in-flight 75-DPI page render (pre-existing EC06 tradeoff, now exercised by a real consumer for the first time).
- Thumbnails render at `ThumbnailDpi × zoom` exactly as before; cache keys unchanged.
- Offscreen test runs exercise the real `QPdfDocument::render` path end to end; pixel-content fidelity of thumbnails is unchanged from base (same funnel, pinned by pin 6).

## Owner items for the integrator

1. Scorecard §4 row 16 can be closed at fold time; the row's dispatch note ("prefetch machinery exists to reuse") was accurate — the machinery had zero callers until this lane.
2. `RenderCache::renderPageAsync` is now a general-purpose seam; the compare-progress lane (§4 row 7) and any other GUI-blocking render site can adopt it the same way.
3. The `ThumbnailRenderer`/viewer retirement-order fix in `setViewer` (clear-before-reset) closes a latent EC06 UAF that predates this lane; worth a line in the next release notes only if behavior is attributed, otherwise silent.

## Commits

- `fix(viewing)`: thumbnail render off the GUI thread via RenderCache prefetch path (row 16) — see git log for the final hash(es).
- `docs(audit)`: lane report + `evidence-thumbnails-offgui/` (fail-before, NC once, pass-after ×3, README).
