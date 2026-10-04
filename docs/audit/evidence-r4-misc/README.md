# R7 evidence — r4-misc lane (finished 2026-10-04)

Mission: round-4 miscellaneous LOW-severity cross-model findings + two
dynamic probes. Worktree `D:/pdf/pdf-r4-misc`, branch `feat/r4-misc`, base
`39c32506` (the native-Linux merge), build dir `build-rel` (in-worktree
only). Fixes: security-auditor findings 3 (bounded hot-folder
processed-set), 4 (linked-qjs runtime gate), 5 (deploy.ps1 VC++ System32
fallback); performance-optimizer finding 8 (DECLINED — see the lane
report); adversary dynamic probes: decode bomb (fixed, typed refusal) and
junction loop (REAL defect found — the pre-fix walk descended junctions;
fixed with the loop-safe walk).

All test runs SERIAL (one ctest test at a time / one exe at a time),
offscreen, from `build-rel`. Per behavior change: fail-before RED,
NC once (all four behavior fixes neutralized behind `NC STAGE` markers →
each fix's own pins RED, guards green), pass-after ×3 SERIAL.

## Files

| File | What it is |
|---|---|
| `build-configure.log` | Runbook configure (Release, LTO, Tesseract, libsecret, no fixtures, no fuzz). CONFIGURE_RC=0. quickjs-ng pin satisfied 0.15.1; runtime DLL hash record added later (see `build-reconf-fix.log`). |
| `build-stageA.log` | Stage-A build (seams + pins only, behavior NOT changed): the three affected suites, BUILD_RC=0, 0 errors. |
| `red-hotfolder.log` | FIRST RED record (harness as first staged): TestHotFolder 32/3 — TTL pin, cap pin, junction probe (64 deliveries) RED; guard green. |
| `red-formjs.log` | FIRST RED record: TestFormJsCalc 50/1 — mismatch pin RED (`!sandbox.isValid()` FALSE — a mismatched runtime still constructs/executes), guard green. |
| `red-stamp.log` | FIRST RED record: TestStampImageImport 10/1 — bomb pin RED on the honesty assertion (Qt's allocation limit surfaced as "unsupported or corrupt file"). |
| `red-final-hotfolder.log` | **AUTHORITATIVE RED** (final harness, src reverted to inert-seam commit `4f266cf6`, pins byte-identical to the committed final harness): TestHotFolder 32 passed, 3 failed — `processedSetEvictsEntriesPastTtlAndReingests` (`redelivered.size()==1` FALSE — the evicted identity never re-ingests), `processedSetEntryCapBoundsMemoryWhileDeliveringAll` (5 entries past the 3-entry cap), `directoryJunctionLoopScanTerminatesBounded` (64 ≠ 1 — the walk DESCENDS the junction loop). Guard `presentFilesAreNotReingestedAcrossTtlWindows` GREEN. |
| `red-final-formjs.log` | **AUTHORITATIVE RED**: TestFormJsCalc 50/1 — `sandboxRefusesLinkedRuntimeVersionMismatch` at `!sandbox.isValid()` FALSE. Guard `sandboxConstructsWhenRuntimeVersionMatchesPin` GREEN (the real JS_GetVersion matches the real pin). |
| `red-final-stamp.log` | **AUTHORITATIVE RED**: TestStampImageImport 10/1 — `dimensionBombRefusedWithHonestSizeDisclosure` at `error.contains("20000")` FALSE (the dishonest corrupt/unsupported wording, quoted in the failure). |
| `build-reconf-fix.log` | Reconfigure on the fix commit: quickjs runtime-DLL sha256 STATUS record matches the ledger row (`cc92ba7e…83ce`). RECONF_RC=0. |
| `build-fix.log` / `build-fix2.log` / `build-fix3.log` | Fix-stage builds: first attempt hit ONE compile error (`const_iterator` assignment in the refresh branch; incremental retry per runbook — recorded in the transcript, log overwritten by the retry), second attempt completed [8/8], third = no-op rebuild for the definitive BUILD_RC=0. |
| `nc-hotfolder.log` | **NC run 1** (all four fixes neutralized behind `NC STAGE` markers, BUILD_RC=0): TestHotFolder 32/3 — BOTH retention pins RED (TTL eviction gone → evicted identity never re-ingests; cap gone → 5 entries past the cap), junction pin GREEN (its fix — the junction-leaf rule — is separate code, deliberately NOT neutralized here; its neutralization evidence IS the authoritative RED-replay, where the whole walk fix is absent), guard green. `batchModeDisclosesWatchDegradationInLog` FAILED — a TIMING FLAKE (see `nc-hotfolder-rerun.log`; both runs recorded per the runbook). |
| `nc-hotfolder-rerun.log` | **NC run 2** (re-run once on the flake): TestHotFolder 33/2 — exactly the two retention pins RED; the F-6 wiring pin GREEN (flake confirmed: the 700 ms debounce-wait pin misfired once on a machine running concurrent LTO builds). |
| `nc-formjs.log` | **NC**: TestFormJsCalc 50/1 — the mismatch pin RED (gate neutralized → mismatched runtime constructs again), guard green, all 50 cascade/sandbox pins green. |
| `nc-stamp.log` | **NC**: TestStampImageImport 10/1 — the bomb pin RED (ceiling neutralized → the dishonest wording returns), all other pins green. |
| `build-restore.log` | Fixes restored (`NC STAGE` grep = 0 across src/ — verified by `r4misc-nc.py check`), BUILD_RC=0. |
| `pass1-*.log` / `pass2-*.log` / `pass3-*.log` | **Pass-after ×3 SERIAL** (post-restore, byte-identical to commit `c2255249`): TestHotFolder 35/35, TestFormJsCalc 51/51, TestStampImageImport 11/11 — every run, no flakes. |
| `build-final-killed-segment.log` / `build-final-resume.log` | Closing full-tree build: first attempt externally KILLED at [760/873] (the prior lanes' documented contention class), 0 FAILED lines up to the kill; incremental resume completed BUILD_RC=0, 0 FAILED. |
| `full-gate-run1-flake.log` / `full-gate.log` | Closing FULL SERIAL ctest gate. Run 1: 202/203 passed — ONE flake (`TestBatchMode::testBatchWithOneBadFile`, `successCount()` 0 vs 2; the suite passes 18/18 standalone immediately after, and the batch-convert path touches none of this lane's changed code — recorded per the runbook). Run 2 (authoritative): **203 passed, 0 failed**, GATE_RC=0; the 2 non-runs are R14ProbeRedactSpace / R14ProbeBatchSkip, disabled by design (the same disabled-by-design state as the prior lanes' closing gates). |

## Harness-hardening note (honesty, r3-perf precedent)

Two harness corrections happened between the FIRST RED records and the
authoritative ones; neither changed a load-bearing RED assertion:

1. `TestStampImageImport::patchPngDimensions` wrote the IHDR CRC at byte 30
   instead of 29 — the first bomb pin run failed at the PREMISE check
   (libpng rejected the patched header outright). Fixed; the pin then REDs
   at the honesty assertion it exists for.
2. `presentFilesAreNotReingestedAcrossTtlWindows` (guard) originally
   asserted presence across TTL windows WITHOUT an observing pass; the
   fixed semantics refresh entries at scan time, so a file unobserved past
   the TTL legitimately expires. The corrected pin runs each pass INSIDE
   the TTL window (the production shape: poll tick ≪ TTL). The guard is
   green pre-fix (RED-replay) and post-fix in the same way; it never was a
   RED pin.
3. `FormJsSandbox::unavailableReason()` (seam) was corrected to return
   empty when the sandbox is VALID (the first version returned the
   fallback wording unconditionally, which broke the guard pin's
   `unavailableReason().isEmpty()` assertion at stage A). Seam-only
   correction; the mismatch pin's RED was unaffected.

## Test-count deltas (no test weakened)

- TestHotFolder: 31 → 35 test functions (+2 bounded-set pins, +1 TTL
  refresh guard, +1 junction-loop probe). No existing pin touched except
  the documented guard-harness correction above (it is a NEW pin, not an
  existing one).
- TestFormJsCalc: 49 → 51 (+mismatch pin, +healthy-engine guard).
- TestStampImageImport: 10 → 11 (+decode-bomb probe). One helper
  (`patchPngDimensions`) and two statics added; no existing assertion
  changed.
- deploy.ps1 carries no automated harness (packaging script): evidence is
  the PowerShell parser check (PARSE OK) recorded in the lane report plus
  the release pipeline that exercises it.
