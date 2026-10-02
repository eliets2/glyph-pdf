# LANE REPORT — compare progress + cancel (§4 row 7, wave 2b)

- **Date:** 2026-10-02
- **Lane:** #7 — CompareMode progress reporting + working cancel (dead-lane finisher)
- **Worktree:** D:/pdf/pdf-w2b-compare, branch `feat/compare-progress`
- **Base:** fe6ca6f2 · **RED pins:** b2ae1745 · **Implementation:** 90c11274 · **Evidence/docs:** (this commit)

## State found

Prior agent died mid-implementation. Committed at `b2ae1745`: three RED pins in
`tests/TestCompareIntegration.cpp` (progressDialogExistsDuringCompareRun runtime pin +
two hook-seam pins) with fail-before evidence `01`/`02` recorded. Uncommitted WIP on four
src files + the test file: a coherent, nearly complete design — DiffEngine::compare gains an
optional default-off `progress(stage, done, total)` probe; CompareMode::compareFiles runs the
diff on the QtConcurrent pool via the `QPromise<DiffResult>&` overload behind a per-run
QProgressDialog whose Cancel drives the watcher cancel, worker polls `isCanceled()` at the
same boundary probes the engine already honours (BatchMode idiom), and a
`setStageBoundaryHookForTest` seam (captured by value into the worker, never read cross-thread)
lets tests park the worker before any boundary's report. The WIP direction was sound and was
continued unchanged; it was **not** redesigned.

## What the finisher changed

Two latent bugs were found in the WIP **tests** (fixed; pin strength preserved or increased):

1. **Use-after-free in both new pins' tails.** `onDiffFinished()` closes AND `deleteLater`s the
   dialog, but the tests kept reading a raw `QProgressDialog*` afterwards
   (`dialog->isVisible()` after completion and after cancel). Under `QTest::qWait` the deferred
   delete reliably lands before those reads → heap UAF, non-deterministic. Fixed with
   `QPointer<QProgressDialog>` + `QTRY_VERIFY(dialog.isNull())` — the dialog must now be
   **destroyed** (not merely hidden) after completion and after cancel: a stronger pin.
2. **Factually wrong comment.** The WIP claimed `QProgressDialog::cancel()` "never emits the
   signal itself" — Qt's `cancel()` slot does `emit canceled()`. The pin still drives the real
   Cancel **button** (the honest user gesture); the comment now states the actual shipped
   `clicked → canceled()` wiring.

No src behaviour was altered from the WIP design. One deliberate tail restructure: the
completion pin no longer re-reads `dialog->value()` after the last park release (same UAF race);
the finish observation proves the "2/2" boundary was posted because QFutureInterface orders all
progress reports strictly before the finished delivery — documented in the test.

## R7 contract results

| Step | Result | Evidence |
|---|---|---|
| Fail-before (prior agent) | RED-1 runtime (dialog missing), RED-2 compile (hook seam) | `01-*.txt`, `02-*.txt` |
| Build (full incremental, `-k 0 -j 2`) | **BUILD_RC=0** (198/198 targets; only benign third-party lua warning). Heavy cross-lane contention (4 concurrent ninja builds) | `03-build1-full-incremental.txt` |
| Green (implementation complete) | TestCompareIntegration **11 passed / 0 failed** | `04-green1-*.txt` |
| NC ×1 (scoped revert of 90c11274's 5 files → b2ae1745 tree) | compile red at the hook seam (RED-2 shape, same error); with the by-design uncompilable hook tests `#if 0`-ed, the hook-free pin runs and **FAILS**: 8 passed / 1 failed, exit=1 — dialog absent on unfixed src | `05-nc1-scoped-revert-red.txt` |
| Restore HEAD + rebuild | RC=0, green again | (restore build; green re-proven by pass-after run 1) |
| Pass-after ×3 SERIAL (4 touched suites × 3) | **12/12 suite runs green, zero flakes** (83 tests per pass: 11 + 42 + 26 + 4) | `07-*/08-*/09-passafter-run{1,2,3}-*.txt` |
| Final touched gate | **83/83 green** (run 4) | `10-final-touched-gate.txt` |

Touched set = every compare/diff suite in the tree: TestCompareIntegration, TestDiffEngine,
TestCompareEntry, TestSep13LeadComparePerf.

## Behaviour delivered (§4 row 7)

- Compare runs on a worker thread; the GUI never blocks.
- Determinate per-stage progress ("Extracting text… N/M", "Comparing pages… N/M") streamed
  through `QPromise` → watcher → non-modal `QProgressDialog` (`cmpProgressDialog`); boundaries
  report BEFORE their unit's work (batch file-boundary semantics); values are 1-based because
  QFutureInterface suppresses 0/N reports (probed on Qt 6.11).
- Cancel via the dialog's real Cancel button → watcher cancel → engine's existing boundary
  polls abandon the diff; the partial result is discarded **by construction** (`addResult` on a
  canceled promise is a no-op), so no partial state reaches widget/tree/lastResult; nav/export
  stay off, swap re-enables, status says COMPARISON CANCELLED, re-run of the same pair works.
- Edge cases pinned: re-entry while a diff runs is refused; dialog destroyed (not hidden) after
  completion AND cancel; cancel state read before closing the dialog (close() would emit
  canceled() and misroute a real completion — the 1.7b trap).

## Known limits

- The cancel pin exercises the button click while the worker is parked at a boundary; cancel
  during a long single-page render surfaces at the NEXT boundary probe (existing engine probe
  granularity — unchanged by this lane).
- The dialog label text per stage is deliberately not pinned (user-facing polish); the stage
  transitions are pinned through the range stream ((0,0) → (0,4) → (0,2)).
- The NC pass is one revert→red→restore cycle (contract minimum), recorded in full.
- Other lanes were building concurrently on this host during the full build; tests were run
  strictly serially afterwards and all four passes were flake-free (no re-run needed).
