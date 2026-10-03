# R3-PERF evidence — runbook, records, and honest caveats (2026-10-03)

Lane: `feat/r3-perf` @ `D:/pdf/pdf-r3-perf`, base `f6d9ca1f`.
Fixes: audit findings 2, 3, 5, 6, 7 (finding 1 verified already fixed by
round-2 F-7 debounce — skipped; finding 4 by-design — skipped).

## Files

| file | what it is |
|---|---|
| `red-printfix-run1.txt` | FIRST RED record (seam-only tree `9189d0cd`, pre-hardening pin harness): hot-folder re-entrancy, stale-insert, coalescing |
| `red-printfix-run2.txt` | FIRST RED record: compare promise-post throttle |
| `red-printfix-final-harness.txt` | AUTHORITATIVE RED record — all four pins, src reverted to seam-only `9189d0cd`, pins byte-identical to the committed (final) harness |
| `nc-printfix.txt` | Negative control: seven existing pins over the same machinery (RenderCache async pins, compare dialog stage/cancel pins, hot-folder polling + F-7 debounce pins) all PASS pre-fix |
| `benchmark-before.txt` | Two BEFORE runs of the churn benchmark (pre-fix behavior) |
| `green-x3-and-after-benchmark.txt` | AUTHORITATIVE pass-after record: 4 pins x3 SERIAL + two AFTER benchmark runs (fix commit `21c66127`) |

## R7 record

- **Fail-before RED** — 4/4 pins RED against the seam-only tree
  (`red-printfix-final-harness.txt`), each failing on exactly the
  load-bearing assertion:
  - `reentrantIngestDoesNotRescanTree`: `ingestScansForTest() == scansBefore + 1`
    FALSE (the nested call re-walked the tree → +2).
  - `duplicateAsyncRequestCoalescesIntoOneWorker`: `asyncWorkerRunsForTest()`
    2 vs expected 1 (a double request scheduled a second worker);
    `asyncCoalescedRequestsForTest()` stayed 0.
  - `staleRenderIsNotInsertedIntoCache`: a superseded render WAS found in
    the cache (legacy getOrRender inserted before the token check).
  - `promisePostsThrottledPerBoundaryStorm`: `promiseReportCountForTest() (20)
    < hookCalls (20)` FALSE — one promise post per boundary, unthrottled.
- **NC** — `nc-printfix.txt`: the machinery's existing pins pass pre-fix,
  so the REDs are attributable to the missing fixes, not harness breakage.
- **Pass-after x3 SERIAL** — `green-x3-and-after-benchmark.txt`:
  12/12 runs green, exit 0, no flakes (no re-runs needed).

### Pin-harness hardening note (honesty)

The coalescing pin's harness was hardened twice between the FIRST RED
record and the final one; the pinned property did not change (the same
`QCOMPARE(asyncWorkerRunsForTest(), 1)` assertion failed pre-fix in both
records and passes post-fix):

1. Delivery targets changed from stack-reference captures to
   shared-ptr-by-value captures — pre-fix, the pin REDs and returns
   early, after which the (now unblocked) legacy workers still deliver;
   reference captures would dangle.
2. The gate release moved before the delivery wait (it was previously
   only on the exit scope guard, which made the post-fix delivery wait
   out its 30 s budget instead of observing the delivery).

Known pre-fix-only artifact, documented rather than hidden: with the
legacy (uncoalesced) worker path, the coalescing pin segfaults in
process teardown AFTER the expected failure is printed and Totals are
written (exit 139; both RED records show it). Analysis: the early
return leaves TWO legacy workers that resume on gate release and run
their full render+deliver path concurrently with the test function's
teardown — an ordering the coalesced path cannot produce (post-fix
there is exactly one worker and it delivers before the pin's delivery
wait is allowed to complete; x3 GREEN show no crash). The crash is
deterministic pre-fix, absent post-fix, and never affects the recorded
assertion failure.

## Measured: document-switch (clear()) join latency under scroll churn

`churnDuplicateRequestsClearLatencyBenchmark` (in TestThumbnailOffGui;
measured, NOT asserted — no timing assertions by design): a renderer
that serializes renders behind a mutex (~15 ms each, modeling
PdfiumBackend's pdfMutex serialization) and parks the first render on a
gate; 8 duplicate `renderPageAsync` requests are fired for the same
page while it is in flight; `clear()` (the document-changed path) is
timed from a helper thread, then the gate is released.

| | BEFORE (seam-only tree) | AFTER (fix commit 21c66127) |
|---|---|---|
| run 1 | 2156 ms | 178 ms |
| run 2 | 12137 ms | 180 ms |
| workers scheduled | 8 | **1** |
| requests coalesced | 0 | **7** |
| workers inside renderPage when clear() landed | 5 / 8 | 1 |

Decomposition (both directions carry the same harness floor): the
measurement includes a deliberate 150 ms parked window (clear() is
joined-blocked on the parked first render before release) plus one
~15 ms render. The join WORK above that floor — the redundant queued
renders clear() must drain — collapses from ~2.0 s to ~12.0 s before
down to ~28 ms after, i.e. the churn join is bounded to ONE render
instead of N (finding 3's bound, delivered by finding 2's coalescing).

**Machine-load caveats (honest):** worker threads run at
`QThread::LowestPriority` by design, so absolute numbers are heavily
load-sensitive — the two BEFORE runs differ by 5.6x under identical
code (the 12137 ms run coincided with background build activity; the
2156 ms run did not). The AFTER runs were taken on the same machine
within minutes of each other with the same floor. Only the worker/coalesced
counter columns are deterministic; the latency columns are directional
evidence, not a benchmark contract.

Also observable in both benchmark records: `deliveries: 0` — after
clear() the epoch is stale and no stale render is delivered (unchanged
contract, pre and post).
