# R7 evidence — hot-folder capability work (lane #9, wave 2b, finished 2026-10-02)

Mission: PARITY-SCORECARD-2026-09-30 §4 row 9 (§3 row 84) — recursive
hot-folder watch + polling fallback for filesystems where fs-events are
unreliable (network shares). The prior agent folded the
HotFolderController extraction (commit 447fd5bd) and captured the
capability fail-before, but died before the pass-after campaign and the
commits. This record covers the finisher's campaign on the inherited
capability WIP.

## Files

| File | What it is |
|---|---|
| `failbefore-extraction-compile-red.log` | Prior agent — extraction-phase NC (pins red at compile before the extraction existed). |
| `failbefore-capability-compile-red.log` | Prior agent — capability NC #1: capability pins (test file) compiled against HEAD whose BatchMode lacks the polling API (`hotFolderPollingEnabled`, `startHotFolderForTest`, `hotFolderForTest`): 13 compile errors. |
| `failbefore-capability-stubstate-red.log` | Prior agent — capability NC #2: with the polling checkbox + seams stub-wired but the controller scan/watch still flat, 7 capability pins fail behaviorally (16+2 harness pass). Prior agent's note (verbatim, bottom of the log): `pollingDedupsAndFlatDrops` was red on a wrong pin premise — "pre-start files are seeded by design — pin rewritten to drop after start". |
| `pass-after-capability-run1.log` | Serial run, 13/13 green (TestHotFolder 7.89s). |
| `pass-after-capability-run2.log` | Serial run during a concurrent machine-load window (headless label 390s vs 104s in run 1): TestSweepW3UxFlows exited 0xc0000602 (FAIL_FAST); the other 12 suites green. |
| `pass-after-capability-run2-flake-retry.log` | Runbook single re-run of the flaked suite: 13/14 flows pass, `flow6_certEncrypt_twoRecipients` assertion red; flow3 logs `cannot open destination for safe write: The system cannot find the path specified` — the suite's own QTemporaryDir tree vanished mid-flow. |
| `pass-after-capability-run3.log` | Serial run on the quiet machine: 13/13 green. Consecutive green #1. |
| `pass-after-capability-run4.log` | Serial: 13/13 green. Consecutive green #2. |
| `pass-after-capability-run5.log` | Serial: 13/13 green (TestHotFolder 7.13s). Consecutive green #3. |
| `final-touched-gate.log` | Final touched-gate after the last tree change (comment-only edit + rebuild, BUILD_RC=0): 13/13 green, TestHotFolder 7.19s. |

## Finisher's premise correction (documented, not silent)

`sameStemInDifferentSubdirsBothIngest` as inherited dropped both
`d1/same.pdf` + `d2/same.pdf` BEFORE `start()` — under the pinned seeding
semantics (pre-start files are marked processed) those drops can never
ingest, so the pin could not go green against the real capability. This is
the exact premise class the prior agent already documented and fixed for
`pollingDedupsAndFlatDrops` (see the stubstate note). Correction applied:

- drops moved AFTER `start()` (root mkpath'd first so the root watch binds);
- BOTH files' mtimes forged IDENTICAL — under the historical flat
  `filename|mtime` key the two drops share one identity and only one would
  ingest, so the green state proves exactly what the pin claims (the
  root-relative key tells same-stem drops apart).

The corrected pin remains a valid capability pin: against the stub (flat
scan, root-only watch) it is red (no nested watch, no nested scan), against
the capability it is green (runs 3–5, 7/7 capability pins green in each).

## Run-2 flake classification (recorded, not papered over)

- Run 2 ran during a ~4x machine-slowdown window (concurrent load from
  another session on this host). TestSweepW3UxFlows (a 13-flow UI sweep,
  link-neighbor only — it exercises welcome/redaction/tagging/cert-encrypt
  flows, no hot-folder code) crashed FAIL_FAST; its own temp-dir tree was
  found deleted mid-flow (SafeSave "cannot find the path specified").
- Re-run per runbook: still red in the suite (flow6 assertion).
- flow6 in ISOLATION on the quiet machine: green in 4.4s with a genuinely
  PubSec-encrypted output (open-with-owner modal proves the envelope).
- Run 1 had passed the identical binary of this suite green with the
  capability changes already built in.
- Classification: contention/environmental flake of the sweep's temp-dir
  lifecycle, not a capability regression. The ×3 contract is satisfied by
  runs 3–5: three consecutive green serial runs of the full 13-suite set
  on the quiet machine, followed by the final touched-gate.

## Suite set (touched code = BatchMode + HotFolderController + TestHotFolder)

`TestHotFolder`, `TestBatchMode`, `TestBatchPresets(P2)`, `TestBatchOpsCoverage`,
`TestBatchOcr{Language,SkipText,Confidence}`, `TestPgr35BatchCollision`,
`TestSep13LeadBatchMerge`, `TestGsdW2Probe`, `R14ProbeSep13Fixes`,
`TestSweepW3UxFlows` — 13 ctest entries (`R14ProbeBatchSkip` matched but is
disabled; not counted). All runs SERIAL, `QT_QPA_PLATFORM=offscreen`,
Release build in this worktree's `build-rel`.

Final tree: TestHotFolder = 21 test functions (23 Qt entries with
init/cleanup harness), all green in every recorded green run.
