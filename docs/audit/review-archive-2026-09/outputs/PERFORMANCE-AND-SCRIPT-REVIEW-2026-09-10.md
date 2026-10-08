# GlyphPDF: comparison, rendering and Form JavaScript review

Finalized 2026-09-11 against the explicitly recorded September 10 source snapshots. Later commits are outside this report.

Review date: 2026-09-10. This is an independent source review with three freshly compiled probes, not a full application benchmark or security certification. No production files were changed.

The comparison and rendering findings apply to published `origin/feat/parity-glm` at `9ba3cea499f67060604ee09903f63ea1a79c30fd`. The probes initially compiled the preserved `4e2c99a` snapshot; their four engine translation units are byte-identical in the published snapshot. Form JavaScript was reviewed separately at local integration commit `b38a4fd524e754fc91981f7bbeba254d85674c24`, which was not in the fetched published branch at the review cutoff. Its implementation ledger records Option A as user-authorized; this review did not install or approve a dependency.

All relative source references below resolve inside the preserved snapshots in `work/whole-project-2026-09-10`. `published-source` contains 9ba3cea; `local-formjs-source` contains the Form JavaScript files extracted from b38a4fd. Evidence JSON, build logs and runnable probe scripts are in that same evidence directory.

## P1 — PERF-01: structural page alignment does not drive content comparison

**Evidence: reproduced with real PDFs and the current engine.** `published-source/src/engines/DiffEngine.cpp:47–128` compares page `i` in both documents and appends the word/pixel results. Only afterward does the structural alignment run, beginning at line 131. That alignment records sets of matched pages, but does not rebuild content results from the matched old/new page pairs. `PageDiff` has one `pageIndex`, rather than separate old/new positions. `CompareMode.cpp` consumes these content rows in its change list and exports.

The fixture was old `[Alpha, Beta, Gamma]`, new `[Alpha, Inserted, Beta, Gamma]`. The structural result correctly reports one page addition at new index 1. The content result nevertheless reports `Beta → Inserted` and `Gamma → Beta`. Beta and Gamma were unchanged. This is a misleading review result for contracts, revisions and engineering drawings, even though the existing structural insertion test passes.

**Required change:** build one explicit alignment sequence first, with `{oldPage, newPage, kind}`. Run content comparison only for corresponding two-sided entries. Reuse that sequence for navigation, filters, counts and exports. Preserve uncertainty for ambiguous image-only or repeated pages rather than implying semantic certainty. Do not discard real content changes while suppressing the false positives.

**Acceptance:** prepend/middle/append insertion, removal, reordered pages, repeated boilerplate and insertion-plus-rewrite fixtures must agree across engine results and exported reports. For the reproduced fixture, Beta and Gamma have no content-change rows, and the inserted page appears exactly once. Retain the existing structural tests and add assertions on the content rows they currently miss.

## P2 — PERF-02: comparison retains full images for unchanged pages

**Evidence: reproduced allocation accounting plus source review.** `DiffEngine.cpp:90–124` creates an ARGB overlay and assigns it to every `PageDiff` even when no pixels differ. The result holds all overlays. It also hashes files through `readAll()` at lines 27–28, extracts text again during alignment, and allocates a full page-count LCS matrix at line 216. `DiffEngine.h` exposes no cancellation or work-budget input.

The control fixture contains three identical Letter pages with only a harmless trailing file comment distinguishing the two PDF byte streams. At 150 DPI it retains **25,245,000 overlay bytes**—8,415,000 per page—with zero changed pixels, no added/removed text and no structural changes. At the same dimensions, 1,000 overlays alone would require approximately **7.84 GiB**. That is arithmetic from the measured per-page allocation, not an executed 1,000-page memory benchmark. Render buffers, document objects and comparison bookkeeping add further memory.

**Required change:** stream the file hash; cache extracted page text within the operation; retain compact difference metadata; discard empty overlays; generate detailed overlays on demand into a bounded cache. Bound page-alignment work and memory before allocating its matrix. Add an operation cancellation/deadline contract and check it within expensive loops. A background thread is insufficient if its output remains unbounded or cancellation never reaches it.

**Acceptance:** compare hundreds of unchanged-but-byte-different pages without memory growing by one full image per page; navigate changed pages with bounded cache growth; cancel during hashing, extraction, matching and pixel scanning. Counts and exported results must remain stable. Test 4 GB machines or equivalently constrained test workers before retaining the README's low-memory recommendation.

## P2 — PERF-03: Myers diff has a measured quadratic memory path

**Evidence: current algorithm compiled with MSYS2 g++ 16.1.0, C++17, `-O2`, Qt6Core.** `MyersDiff.cpp:117–154` allocates a frontier sized from the total token count and retains a frontier snapshot after every edit-distance step. The storage is O((N+M)D), becoming quadratic for wholly different inputs. No memory/work/cancellation budget is present.

Each row below represents three separate child processes. Time is the median; memory is the maximum process peak working set. These are synthetic token-list measurements, not end-to-end PDF timings or a competitor benchmark.

| Tokens per document side | Identical median | Entirely different median | Different peak working set |
|---:|---:|---:|---:|
| 500 | 0.036 ms | 5.700 ms | 19.0 MiB |
| 1,000 | 0.068 ms | 22.280 ms | 42.9 MiB |
| 2,000 | 0.116 ms | 87.127 ms | 133.8 MiB |

All 18 insert/delete/keep count checks passed. The issue is resource growth, not a demonstrated incorrect edit script on these cases. Doubling the divergent input approximately quadrupled time and substantially increased memory. CPU/RAM inventory access was unavailable; the numbers must not be used as universal latency promises.

**Required change:** set an explicit budget in the operation contract, then choose a bounded strategy: partition around stable anchors and/or use linear-space reconstruction. Preserve exact matching where feasible and disclose a coarser fallback where the budget prevents exact output. Start by bounding the existing implementation rather than introducing another diff library.

**Acceptance:** the existing correctness corpus remains green; growing high-edit-distance inputs stay within the configured resource budget and return a truthful result or bounded refusal; cancellation works during the long path. Benchmark typical small edits separately so the optimization does not worsen the common case.

## P2 — PERF-04: the documented render hot path is not the active viewer path

**Evidence: production call-site inspection.** `docs/performance/hot-path-analysis.md:26–34` says `RenderCache::getOrRender` services PdfViewerWidget, ThumbnailSidebar and CompareWidget on every view event. In this snapshot its external production caller is ThumbnailSidebar (`src/ui/ThumbnailSidebar.cpp:313`). `prefetchViewport` has no production caller. RenderCache's list-based optimization has already changed since the old analysis.

The main viewer uses QPdfView and a separate page cache. Its custom two-page path calls `renderPage` at `src/ui/PdfViewerWidget.cpp:911/923`. `renderPage` starts at 1126, computes an unchecked pixel size at 1141, renders synchronously at 1148 and evicts only after the image/pixmap allocation at 1169. The separate cache limit is 256 MiB. Zoom entry points have no upper/finite-value clamp. These facts do **not** prove that ordinary QPdfView scrolling always uses this synchronous helper; the affected custom rendering path must be profiled separately.

**Required change:** correct the performance map first. Measure normal QPdfView navigation, two-page rendering, thumbnails and compare independently. Apply finite dimension/scale checks and a pixel-allocation ceiling before every relevant allocation, including peak image-plus-pixmap duplication. Prefer a shared checked-size/budget helper at real rendering boundaries. Do not force a backend rewrite merely to make a diagram show one cache.

**Acceptance:** large MediaBox, extreme zoom, NaN/infinite programmatic scale, HiDPI and rapid zoom reversal produce a bounded render or a useful refusal. UI input stays responsive and stale jobs cannot replace a newer document/zoom image. Measure cold first-page time and repeated-page cache hits on the real view.

## P2 — PERF-05: extracted text includes the API terminator

**Evidence: reproduced by the comparison probe.** `src/engines/pdfium/PdfiumBackend.cpp:265–267` passes the return count from `FPDFText_GetText` directly to `QString::fromUtf16`. The vendored API header documents that the count includes the trailing terminator. Consequently the probe's tokens contain `Beta\u0000`, `Gamma\u0000` and `Inserted\u0000`.

**Required change:** validate the returned count and remove only the API terminator from the constructed string. Do not strip legitimate interior document characters indiscriminately. Audit the other PDFium text conversion sites for the same return-count contract.

**Acceptance:** exact string comparison on one-word and multiline PDFs, empty pages, accented/Unicode text and no-text pages. Assert no appended API NUL in exports or comparison tokens. This small fix affects data quality beyond what is visible on screen.

## P1 — JS-01: the local Form JavaScript deadline can be bypassed

**Evidence: reproduced using the actual local FormJsSandbox and AFormShim sources, freshly compiled with the installed quickjs-ng library.** The runtime is in the application process. The script body's `JS_Eval` has an interrupt deadline, but `FormJsSandbox.cpp` sets `deadlineMs = -1` before invoking the script-visible `globalThis.__gpEndEvent()` and reading its result. Document code can replace that function, or install a getter on `event.value` that executes during result serialization.

The probe set a **50 ms** event deadline and used an external **3 s** timeout to kill only its own child process:

| Case | Observed result |
|---|---|
| Normal `event.value = 42` | Returns successfully, value 42 |
| Infinite loop in script body | Reports timeout at 51 ms |
| Script replaces end-event helper with a loop | Still running at 3 s; harness terminates child |
| Script installs looping `event.value` getter | Still running at 3 s; harness terminates child |

Evidence: `formjs-deadline-results.json`, `formjs-probe.py`, `formjs-probe.cpp`, `formjs-build.log`. No customer PDF or installed application was used. This demonstrates a resource-limit escape/hang, **not** file access, network access or native code execution.

**Required change before enabling this implementation in a release:** treat every call into the JS engine, including setup, field refresh, exception properties, coercion and result collection, as execution of potentially hostile code. Keep a single absolute deadline active across the entire operation and honor the cascade budget. Avoid mutable script-global helper names as trusted entry points. Protecting helper names alone does not fix getters, proxies or `toJSON` hooks. Constrain input/output sizes on the C++ side as well; the JS heap cap does not cover all host allocations.

A restricted worker process with a parent watchdog is the recommended next security boundary for native parser/script hangs and crashes. Reuse existing command/serialization contracts and add it narrowly; do not call an in-process interpreter an OS sandbox. Disabling host I/O modules is a useful defense, but does not contain a native engine vulnerability.

**Acceptance:** both bypasses above must terminate within the documented whole-operation budget without the harness killing the process. Add modified setup-helper, exception-getter, coercion, proxy and serialization cases. On failure preserve the user's entered value according to the chosen transaction policy, identify affected calculation fields, and disclose stale derived results. Run the integration tests through the real form UI/save boundary and confirm the program remains usable after repeated timeouts.

## Form JavaScript: remaining release conditions

The local code is substantial progress: memory/stack limits, interrupt handling, blocked egress verbs, an AF compatibility shim, field-attributed failures and calculation within the form transaction all exist. The implementation ledger's test results are implementation evidence; this review does not upgrade its row to verified.

Before closure, also require:

- Record the exact quickjs-ng package/runtime hash and the exact Mozilla reference revision/ported subset. Comments and an unversioned `find_package(qjs)` are not an enforced dependency pin. Preserve attribution and package the actual runtime dependency closure.
- Test real third-party forms containing indirect action streams, parent/child fields, duplicate/Unicode names, locale-sensitive numbers/dates, missing or malformed calculation order, dependent failures and signed/read-only documents. Keep a declared compatibility subset instead of promising all Acrobat JavaScript.
- Correct the existing conflation of keyboard tab order and calculation order: FormManager::setTabOrder writes `/CO`. Adding script execution makes this old modeling error more consequential.
- Keep Calculate, Format, Validate, Keystroke and document-open support separate in CapabilityRegistry. The local ledger explicitly leaves Validate/Keystroke and document-open consent for later phases. Format preview is not yet formatted page appearance or exported appearance parity.
- Make the failure policy explicit. Saving the user's input while keeping a previous total is acceptable only with persistent, field-specific stale-calculation disclosure. Later fields must not silently be advertised as valid after using stale dependencies.
- Verify the no-engine build, runtime-DLL deployment, effective policy/consent behavior and UI responsiveness. The new CMake test staging still assumes Windows DLL names; include it in the Linux conversion.

## Reproduce and retain evidence

From the audit workspace, `python work/whole-project-2026-09-10/perf-probe.py`, `compare-probe.py` and `formjs-probe.py` each compile the actual scoped sources into audit-owned executables. The first two use the preserved initial snapshot; the third extracts pinned b38a4fd Form JavaScript sources through read-only Git. They require the already installed MSYS2 Qt/compiler and the documented vendor PDFium files; they do not download dependencies. The Form JavaScript probe intentionally demonstrates hangs inside disposable, time-limited child processes.

Do not replace the full product gate with these probes. Fresh full Release tests, clean-machine packaging, current-build native UI review, printer verification, multi-hour soak and native Linux execution remain outstanding for the reviewed release candidate.
