# LANE REPORT — round 3, performance lane (2026-10-03)

Lane: `feat/r3-perf` @ worktree `D:/pdf/pdf-r3-perf`, base `f6d9ca1f`.
Input: `D:/pdf/verification/crossmodel/performance-optimizer.md`
(PASS-WITH-FINDINGS: H:2 · M:4 · L:2 over the wave-2b hot paths).
Mission: findings 2, 3, 5, 6, 7. Finding 1 (per-event watch walk) was
already fixed by the round-2 lane (F-7, commit `b429f483` — debounce;
verified intact, including the `controllerBulkFileEventsDoNotWalkPerEvent`
pin and the watched-set so `addPaths` only ever sees NEW directories);
finding 4 (indexed expansion) is by-design per the CMYK-downsample lane —
both skipped without further change. Finding 8 (LaneScheduler routing)
was not in this lane's mandate and is left open.

## Disposition

| finding | severity | disposition |
|---|---|---|
| 2 — renderPageAsync duplicate requests not coalesced | H | **FIXED** (commit `21c66127`) |
| 3 — clear()/drainPrefetches() GUI-thread join, N-deep | M | **BOUNDED** via finding 2 (see design decision) |
| 5 — compare progress probe per page-pair boundary | M | **FIXED** (promise posts throttled; cancel granularity untouched) |
| 6 — polling scan cost per 2 s tick | M | **FIXED** (re-entrancy guard + stat-only streaming scan) |
| 7 — insert before token check | L | **FIXED** (epoch checked under the insert's write lock) |

## The fixes

### RenderCache::renderPageAsync — coalescing (finding 2) + the join bound (finding 3) + insert-after-token-check (finding 7)

One mechanism, three findings. A per-`(page, scale)` in-flight job
registry (`m_asyncJobs`, guarded by the cache lock) now absorbs duplicate
requests: the second requester's callback is appended to the live job and
delivered by THE one worker when the render completes — no second worker
is queued, so scroll churn can no longer saturate the QtConcurrent pool
with renders whose results were discarded at the insert-time TOCTOU
re-check. Job retirement happens under the cache lock BEFORE delivery, so
a request arriving after completion cache-hits (or, for a superseded
render, starts fresh) and can never attach to a dead job; the waiters
snapshot is taken under that same lock, so every attach made during the
render is delivered exactly once. Attach preserves the delivery contract:
coalesced callbacks run on the worker thread, same as the initiator's.

The worker's render half moved to `renderForAsyncWorker()`:
lookup → render outside the lock → **epoch re-checked under the write
lock that performs the insert** → insert. The legacy path called
`getOrRender()`, whose unconditional insert ran BEFORE the post-render
token check — a superseded render paid a full cache insert and could
evict fresh (LRU) entries for content `clear()` would only mop up later.
Checking under the insert's lock closes the check-vs-insert window
against the wipe (a `clear()` wipe is strictly ordered after
`drainPrefetches()` joins this very worker, so a stale render now inserts
nothing and delivers nothing). Auto-tile routing is preserved verbatim:
tile-worthy (>50 MP) pages still delegate to `getOrRender`'s center-tile
path — a corner far outside thumbnail scales, documented as the fix's
residual scope note.

**Finding 3 design decision — bound, not move.** The instruction offered
"bound OR move the wait off the GUI thread". The move (async/deferred
join, "release the cache immediately, join on the worker side") was
evaluated and rejected: `clear()`'s join IS the EC06 guarantee that a
renderer is never retired mid-render — `ThumbnailSidebar::setViewer`
resets the renderer immediately after `clear()` returns, the sidebar
destructor relies on the same ordering, and the existing pin
`clearCancelsInFlightRenderJoinsAndDeliversNothing` (which may not be
weakened) pins that `clear()` blocks until the in-flight worker is
joined. Deferring the join would trade a bounded wait for a UAF class.
The bound route is taken instead: with coalescing, `drainPrefetches()`
joins at most ONE worker per distinct `(page, scale)` key — the document
switch waits one render, never one render per queued duplicate. The
header contract documents the bound.

### CompareMode worker — promise-post throttle (finding 5)

Each `QPromise::setProgressValueAndText` is a queued cross-thread
delivery plus a dialog repaint; one per page-pair boundary made a 1000-
page comparison a 1000-repaint storm on the GUI thread. The worker's
report lambda now posts at most once per 8 boundaries
(`kProgressEveryNBoundaries`), with a forced post at every stage change
(the dialog's range switch (0,N1+N2)→(0,pairs) must land immediately —
pinned by `progressDialogTracksStagesDuringDiff`) and at every stage
completion (the final value; `QFutureInterface` orders progress reports
strictly before the finished delivery, so the completion signal is
bit-identical). The stage-boundary hook — and therefore the engine's
cancel-probe granularity — stays PER-BOUNDARY, exactly the finding's own
direction ("the cancel poll can stay per-boundary; only the report needs
throttling"): the 6v6-page pin asserts `hookCalls == 20` unchanged while
promise posts drop 20 → 5. All three pre-existing dialog pins pass
untouched (the throttle's flush points are precisely what they observe).

A time-based relaxation (post if ≥N ms elapsed) was considered and
rejected: it is unobservable under the parked-boundary test seams and
would make the post-count pin flaky on a loaded machine; the boundary
cap plus stage-change/completion flushes bound the storm deterministically
and keep the dialog live (a pair that takes seconds is pathological, and
even then cancel works and the completion signal is exact).

### HotFolderController — poll-tick cost (finding 6)

Two changes, both behavior-preserving under the existing suite:

1. **Re-entrancy guard.** The ingest handler runs synchronously inside
   `ingestDeliver()`; a handler that walks back in (batch auto-run
   re-entering the controller) used to trigger a second concurrent
   full-tree scan on exactly the slow shares the polling fallback
   exists for. The nested call is now absorbed (empty delivery, no
   re-scan, RAII guard reset).
2. **Stat-only streaming scan.** The tick used to materialize the whole
   tree into a `QList<QFileInfo>` (plus a fresh `QDir` per file for the
   identity key) every 2 s. It now streams the `QDirIterator` walk
   directly into the processed-set comparison and builds keys with one
   hoisted root. The tick is a pure stat walk — nothing decodes or
   re-reads file content (keys are relpath|mtime|size, all stat fields;
   the F-5 size term is a stat, not a hash — that was verified, not
   changed). Per-tick allocations proportional to the tree are gone.

Finding 6's "cheaper change-detection primitive" (directory-mtime
short-circuit to skip descending) was considered and left out: it changes
discovery semantics for writers that update directory mtimes
unreliably on NAS shares (the F-5 finding's own subject), and the tick's
dominant cost was the per-file key churn and the container rebuild, both
now gone. The 2 s full-stat walk stays the feature's documented cadence
contract.

## R7 record (per-runbook)

Evidence: `docs/audit/evidence-r3-perf/` (runbook + records, see
`RUNBOOK-R3-PERF.md` there).

- **Fail-before RED, once** — `red-printfix-final-harness.txt`: src
  reverted to seam-only `9189d0cd`, pins byte-identical to the committed
  harness; 4/4 RED on the load-bearing assertions (counter shows 2 runs
  for a double request; stale render found in cache; 20 posts == 20
  boundaries; nested scan adds a second walk). Two earlier RED records
  (`red-printfix-run1/2.txt`) predate a pin-harness hardening — the
  pinned property is identical; the hardening (shared-ptr delivery
  targets, gate released before the delivery wait) is documented in the
  runbook, including the deterministic pre-fix-only teardown segfault
  (after the failure prints) and its analysis.
- **NC, once** — `nc-printfix.txt`: seven existing pins over the same
  machinery (RenderCache async delivery/hit/join pins, compare dialog
  stage + cancel pins, hot-folder polling dedup + F-7 debounce pins)
  all PASS pre-fix.
- **Pass-after ×3 SERIAL** — `green-x3-and-after-benchmark.txt`:
  12/12 green, exit 0, no flakes, no re-runs needed.

## Measured (where measurable)

`churnDuplicateRequestsClearLatencyBenchmark` (committed; measured, NOT
asserted — timing assertions would flake on a loaded box): with 8
duplicate render requests in flight for one page, the document-switch
`clear()` join drops from **2156 ms / 12137 ms** (two BEFORE runs; the
spread itself is machine-load evidence — workers run at LowestPriority
by design) to **178 ms / 180 ms**, with workers scheduled 8 → **1** and
coalesced 0 → **7**; workers inside `renderPage` when `clear()` lands:
5–8 → **1**. Both directions carry the same harness floor (a deliberate
150 ms parked window + one ~15 ms render), so the join work above the
floor collapses from ~2–12 s to ~28 ms. Full decomposition and caveats
in the runbook. The deterministic columns are the counters; the latency
columns are directional.

Not separately benchmarked (honest): the compare throttle (the win is a
bounded delivery count — 20 → 5 on the pin fixture, K-bounded on any
size — not a wall-clock effect worth a flaky timing pin) and the
poll-tick streaming (allocation-behavior change covered by the
per-tick-correctness pins; wall-clock on a real NAS share is not
reproducible in this environment).

## Incidents (recorded honestly)

1. During the RED/GREEN cycle the working tree's uncommitted fix sources
   were wiped by a `git checkout 9189d0cd -- src/...` followed by
   `git checkout HEAD -- src/...` (HEAD was the seam-only commit at that
   moment — the fixes had not been committed yet). The fixes were
   re-applied from the session record and the RED was re-recorded
   against the FINAL pin harness, which turned the incident into the
   cleanest possible R7 record. The fix commit (`21c66127`) now lands
   before any further checkout.
2. The coalescing pin initially used stack-reference delivery targets
   and a scope-exit-only gate release; both hardened (see runbook). No
   product behavior was involved in either hardening.

## No test weakened

All pre-existing pins of the three touched suites pass unmodified
(`TestThumbnailOffGui`, `TestCompareIntegration`, `TestHotFolder` —
full-suite serial gate recorded in the commit message trail); the new
pins extend them. `clearCancelsInFlightRenderJoinsAndDeliversNothing`
(EC06 join semantics) passes as-is against the coalescing fix.

## Residuals / handoff

- Finding 8 (compare worker on the global QtConcurrent pool instead of
  the LaneScheduler CPU lane) — open, low, not in this lane's mandate.
- Finding 7's auto-tile corner: tile-worthy pages still take the legacy
  `getOrRender` path (insert before delivery check) — unreachable at
  thumbnail scales, documented in `renderForAsyncWorker`.
- The processed-set of the hot folder remains unbounded per session
  (pre-existing shape, noted by the original audit; not in scope).
- `churnDuplicateRequestsClearLatencyBenchmark` is a printed measurement,
  not a contract; its numbers are machine-relative by design.
