# LANE REPORT — #9 wave 2b, hot-folder recursion + polling fallback (2026-10-02)

Lane: `feat/hotfolder-controller` @ worktree `D:/pdf/pdf-w2b-hotfolder`,
base tip `447fd5bd` (characterization pins `8b3159b8` + behavior-preserving
`HotFolderController` extraction — both already folded to main by the
integrator; nothing was rebased or merged onto main).
Mission: PARITY-SCORECARD-2026-09-30 §4 row 9 (§3 row 84) — "Recursive
hot-folder watch + polling fallback" — enterprise watched-folder
robustness for network shares and nested directories.

## State found

The prior agent died with the step-2 capability work **uncommitted**:
+314/−41 across `src/modes/HotFolderController.{cpp,h}`,
`src/modes/BatchMode.{cpp,h}`, `tests/TestHotFolder.cpp` (208 new test
lines), plus two capability fail-before logs in
`docs/audit/evidence-hotfolder/`. NC captured (compile-red + behavioral
stub-state red); pass-after ×3 and the commits were missing.

## Continuation decision (no redesign)

The inherited design is sound and was kept as-is:

- **Recursive watch** — `HotFolderController::watchSubdirectories()` puts
  the root and every (transitive) subdirectory under `QFileSystemWatcher`
  and re-runs on every directory-changed delivery, so directories created
  after start are picked up live; `recursivePdfEntries()` (QDirIterator,
  `*.pdf` + `*.PDF`, files only) walks the whole tree for seeding and
  ingest; the identity key became the **root-relative path + mtime** —
  flat files yield byte-identical keys to the historical
  `filename|mtime`, and same-stem files in different subdirectories no
  longer collide. Watch-add failures on dead network paths stay silent —
  that is what polling is for.
- **Polling fallback** — `startPolling(dir, intervalMs)` with
  `kPollIntervalMs = 2000`: a plain scan timer replaces fs-events for
  shares where change notifications are unreliable; identical seeding +
  dedup semantics. BatchMode grows a "Polling fallback (network drives)"
  checkbox and `beginHotFolderWatch()` (the shared post-picker half of
  the toggle's ON branch), with test seams
  `startHotFolderForTest` / `hotFolderForTest` / `hotFolderPollingEnabled`.
- **Pin flip** — `subdirectoryPdfNotIngested` (the characterization of the
  gap) flipped to `subdirectoryPdfIsIngestedRecursive`: deliberate
  behavior change, justified and fail-before-captured.

One genuine defect was found and fixed while finishing (same class the
prior agent had already documented for `pollingDedupsAndFlatDrops` in the
stub-state log): `sameStemInDifferentSubdirsBothIngest` dropped its two
files BEFORE `start()`, where recursive seeding marks them processed by
design — the pin could never go green. Corrected to post-start drops, and
strengthened: both mtimes are forged IDENTICAL so under the historical
flat key only one could ingest — green now proves precisely the
root-relative-key fix. No test was weakened; one impossible premise was
repaired and one pin made strictly stronger. Stale header comment in the
test suite updated to reflect the flip honestly.

## R7 record (capability contract)

- **NC (fail-before)** — prior agent's two logs:
  `failbefore-capability-compile-red.log` (pins vs HEAD: 13 compile
  errors, polling API absent) and `failbefore-capability-stubstate-red.log`
  (7 capability pins behaviorally red against a flat scan/watch stub).
- **Pass-after ×3** — serial, `QT_QPA_PLATFORM=offscreen`, 13-suite
  touched set (TestHotFolder + every BatchMode-linking neighbor):
  - run 1 green; run 2 hit a ~4x machine-load window and its one failure
    (`TestSweepW3UxFlows`, FAIL_FAST 0xc0000602; its own QTemporaryDir
    tree deleted mid-flow) was shown environmental — single re-run still
    red, then flow6 green in isolation (4.4s, genuinely PubSec-encrypted
    output) and the identical binary green in run 1. Recorded both per
    runbook.
  - **runs 3, 4, 5: three consecutive green serial runs, 13/13 each**
    (TestHotFolder 7.1–7.2s in each).
- **Final touched-gate** — after the last tree change (comment edit +
  rebuild BUILD_RC=0): 13/13 green (`final-touched-gate.log`).
- Full narrative: `docs/audit/evidence-hotfolder/README-capability-r7.md`.

## Gate results

- Build: runbook command, Release, `-j 2` — **BUILD_RC=0 on the first
  attempt** (160 targets, no LTO link failure, no retry needed).
- Final tree: TestHotFolder 21 test functions (23 Qt entries), all green;
  neighbor set 13/13 ctest entries green (R14ProbeBatchSkip matched but is
  disabled upstream; not counted).
- Nothing pushed, no merges/rebases onto main, worktree and branch left in
  place for the integrator.

## Known limits (documented in code)

- Polling ingests on the tick cadence (2s default) — intentionally not
  instant; the debounce path remains the fs-event half.
- Processed-set is still unbounded (scorecard §2 P2 "hot-folder processed-set
  TTL" remains open — separate row, untouched by this lane).
- Watch-add failures on unreachable network subpaths are silent by design;
  the polling fallback is the recovery path, not an error surface.
- Native recursive watch relies on per-directory `QFileSystemWatcher`
  coverage; on filesystems where even directory events never arrive, the
  polling checkbox is the supported mode.

## Owner items for the integrator

1. Scorecard §3 row 84 and §4 row 9 (and the §9.12 residual sentence
   "hot-folder watch is non-recursive, no polling fallback") are now
   closed by this lane — ready to mark DONE/GREEN at fold time. The
   scorecard itself was not edited (integrator's document, shared with
   in-flight lanes).
2. The run-2 contention flake on `TestSweepW3UxFlows` (temp-dir lifecycle
   under ~4x load; green in isolation and in 4 of 5 full runs) is worth a
   look at wave-end full-suite verification time — unrelated to this lane's
   code, but it can bite a parallel-fold verification run.
3. CHANGELOG: no user-facing entry added by this lane; if the fold wants
   one, "recursive hot-folder watch + polling fallback for network drives"
   is the one-liner.

## Commits (this lane, on feat/hotfolder-controller)

- `feat(hotfolder)`: recursive directory watch + polling fallback for
  network shares — controller recursion + subtree-unique keys + polling
  timer, BatchMode polling checkbox + beginHotFolderWatch + seams,
  capability pins (+7), justified pin flip, premise repair.
- `docs(audit)`: lane #9 wave-2b R7 evidence (NC logs, 3× consecutive
  green, flake record, final touched-gate) + this report.
