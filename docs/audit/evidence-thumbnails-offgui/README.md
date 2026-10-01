# Evidence — lane #16 wave 2b: thumbnail render off the GUI thread (2026-10-01)

Lane: `feat/thumbnails-offgui` @ `D:/pdf/pdf-w2b-thumbnails`, base `a4cc1522`.
Mission: PARITY-SCORECARD-2026-09-30 §4 row 16 — `ThumbnailSidebar.cpp:35-46`
renderer → `RenderCache` prefetch path.

## Files

| File | Gate |
|---|---|
| `red-before-pins-run1.log` | **Fail-before** — `TestThumbnailOffGui` built against base `a4cc1522` sources (only `ThumbnailSidebar`/`PdfViewerWidget`/`RenderCache` reverted; test + CMake present). The jank pin fails with `FAIL ... 'placeholderHeld' returned FALSE. (thumbnail pixmap rendered synchronously on the GUI thread during first paint)` — caught with every QtConcurrent worker PARKED in a semaphore, so the pixmap can only have come from a synchronous GUI-thread render. Positive controls (`thumbnailsArriveAsynchronouslyAfterRelease`, `sidebarDestructionWithRendersInFlightIsSafe`) pass at base: 1 FAILED / 4 passed, rc=1, 520 ms. |
| `negative-control-scoped-revert.log` | **NC once** — final tree with ONLY the `ThumbnailSidebar.cpp` createThumbWidget off-GUI wiring scoped back to the synchronous `getOrRender` call (RenderCache `renderPageAsync` API and `PdfViewerWidget::renderPageUncached` seam retained). Full 8-pin suite: the first-paint pin is RED again, all machinery/seam pins stay GREEN (they pin the machinery, not the wiring) — proving the pins detect the gap, not merely the API's existence. Totals: 8 passed / 1 failed, rc=1, 5.6 s. |
| `green-after-fix-run{1,2,3}.log` | **Pass-after ×3, SERIAL** (no `-j`, `QT_QPA_PLATFORM=offscreen`), consecutive runs of the full 9-check suite on the final tree: 9 passed / 0 failed each — 9.36 s, 12.75 s, 10.19 s (rc=0 ×3). |
| `green-after-fix-run4-post-negative-control.log` | 4th green run after the negative-control restore: 9 passed / 0 failed, 18.1 s (rc=0). |

Negative-control note (same adaptation the fdf lane recorded): the
`renderPageAsync` machinery pins (thread/delivery/cache-hit/cancel-join) stay
green under the NC revert because the scoped revert deliberately retains the
new API — those pins' before-state evidence is the base run, where the API did
not exist at all (the fail-before phase-1 suite is the exact subset of this
suite that compiles at base; the API pins are its additive complement).

## Build provenance

- Configure: runbook flags exactly (ucrt64-first PATH, LTO/TESSERACT/LIBSECRET ON, fixtures OFF, fuzz OFF, Ninja).
- Phase-A target build + full builds: `cmake --build build-rel --config Release -- -k 0 -j 2` (parallelism capped per runbook — 11 co-tenant lanes).
- Runtime DLLs: `stage_runtime_dlls` ALL-target stages Qt/toolchain + `third_party/pdfium/bin/pdfium.dll` + `third_party/podofo/install/bin/libpodofo.dll` into `build-rel` (all present in the worktree; nothing copied from the main tree).
