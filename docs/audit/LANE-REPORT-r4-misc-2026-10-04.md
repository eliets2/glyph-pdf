# LANE REPORT — r4-misc (round 4: miscellaneous LOW findings + dynamic probes)

Lane: `feat/r4-misc` @ `D:/pdf/pdf-r4-misc`, base `39c32506` (native-Linux
merge), finished 2026-10-04. Source reports:
`verification/crossmodel/security-auditor.md` (findings 3, 4, 5) and
`performance-optimizer.md` (finding 8). Evidence:
`docs/audit/evidence-r4-misc/` (R7 protocol: fail-before RED, NC once,
pass-after ×3 SERIAL).

Disposition summary:

| # | Item | Disposition |
|---|---|---|
| 1 | Hot-folder processed-set unbounded growth (security LOW) | **FIXED** — TTL sweep + entry cap, honest re-ingest tradeoff documented |
| 2 | quickjs pin reads header macros, not the linked library (security LOW) | **FIXED** — runtime `JS_GetVersion` gate at the point of use + configure-time DLL hash ledger record; residual gap disclosed |
| 3 | deploy.ps1 VC++ runtime System32 fallback (security LOW) | **FIXED** — Redist payload only, hard honest failure, never System32 |
| 4 | DiffEngine on the generic QtConcurrent pool (perf LOW) | **DECLINED** — LaneScheduler cannot support it cleanly (no per-task cancel, no progress channel; see §4) |
| 5a | Decode-bomb dynamic probe | **PROBE + FIX** — typed refusal for header-declared rasters past the limit |
| 5b | Junction-loop dynamic probe | **PROBE + FIX** — found a REAL defect (see §5b): the walk descended junction loops (one file delivered 64×) |

Commits: `4f266cf6` (stage seams + probes, inert), `5fde32d2` (deploy.ps1),
`c2255249` (the four behavior fixes). Test deltas: TestHotFolder 31→35,
TestFormJsCalc 49→51, TestStampImageImport 10→11. No existing test
weakened (the one harness correction is documented in the evidence
README).

---

## 1. Bounded hot-folder processed-set (security-auditor finding 3)

`HotFolderController::m_processed` grew by one entry per ingested file and
never shrank — a slow leak on the months-long enterprise watches this
feature targets. The set is now a `QHash<QString, qint64>` of key →
last-observed msecs with two retention passes run per ingest pass:

- **TTL sweep** — entries not re-observed by a scan for 7 days
  (`kProcessedTtlMs`) are evicted. Observation, not wall-clock presence,
  is what keeps an entry alive: every scan pass refreshes the entries of
  files it sees, so a static archive in the tree NEVER re-ingests (pinned
  by `presentFilesAreNotReingestedAcrossTtlWindows`), while entries whose
  file vanished or changed stop being refreshed and are reclaimed.
- **Entry cap** — the set never holds more than 100k entries
  (`kProcessedMaxEntries`); past the cap the OLDEST-OBSERVED entries are
  evicted (deterministic by lastSeen + key). Memory is bounded at a few MB
  regardless of churn.

The re-ingest tradeoff is documented at the constants and pinned honestly:
once an entry is evicted (file absent > 7 days, or churn past the cap), a
byte-identical re-drop of the same file (same relpath|mtime|size key)
RE-INGESTS — at-least-once delivery after eviction, never unbounded
memory. Pins: `processedSetEvictsEntriesPastTtlAndReingests` (RED pre-fix:
the evicted identity never re-ingested),
`processedSetEntryCapBoundsMemoryWhileDeliveringAll` (RED pre-fix: 5
entries past a 3-entry cap; post-fix: all 5 still DELIVERED, set trimmed,
the 2 evicted identities re-ingest on the next pass).

A test-seam pair (`processedCountForTest`, `setProcessedLimitsForTest`)
allows the pins to compress the 7-day TTL and the 100k cap; production
never sets them.

## 2. Linked-library quickjs verification (security-auditor finding 4)

The configure-time pin (`GLYPHPDF_QUICKJS_PIN`) parses `quickjs.h` macros
— it proves the HEADERS advertise 0.15.1, not that the libqjs this binary
loads is that engine. quickjs-ng DOES expose a version symbol
(`JS_GetVersion`, quickjs.h:1415), so per the tasking the enforceable fix
is the runtime gate:

- **Runtime gate (enforced)** — `FormJsSandbox`'s constructor now calls
  `JS_GetVersion()` and compares against `GLYPHPDF_QUICKJS_VERSION`. On
  divergence the runtime is torn down before any script runs; every entry
  point then refuses with an honest reason ("quickjs-ng runtime version
  mismatch: the linked libqjs reports X, the build enforces Y…") surfaced
  through `unavailableReason()` into all five FormJsRunner failure
  channels (cascade, validate, keystroke, keystroke-commit, format) —
  the same fail-closed shape as the 7-Zip runtime hash check, at the
  point of use. Pinned by `sandboxRefusesLinkedRuntimeVersionMismatch`
  (RED pre-fix: the mismatched runtime constructed and executed) and the
  healthy-engine guard `sandboxConstructsWhenRuntimeVersionMatchesPin`
  (the real engine's real report matches the real pin — green everywhere).
- **Configure-time DLL hash (record, not pin)** — CMake `file(SHA256)`s
  the resolved `libqjs-0.dll` and prints it against the ledger row
  (verified matching this machine: `cc92ba7e…83ce`, package
  0.15.1-1). A hard byte pin was deliberately NOT added: the MSYS2
  package updates independently of this tree, and a repackage with no
  version change would brick every developer configure for a byte
  difference the version gate already bounds.
- **Residual gap (disclosed, in-source + here)** — the enforced gate
  verifies the version STRING, not the DLL's bytes: a same-version
  rebuilt or tampered libqjs passes. Byte identity of a system package is
  not enforceable without owning the artifact (the 7-Zip bundle is
  committed; libqjs is not) — the hash record above keeps the drift
  visible to a human at configure time.

## 3. deploy.ps1 VC++ runtime staging (security-auditor finding 5)

The System32 fallback is REMOVED entirely (the task's preferred direction,
stronger than a release-only gate): the deploy tree stages the VC++
runtime from the official Redist payload ONLY. Without it the script
throws an honest, remediation-bearing error instead of producing a
shippable-looking bundle carrying build-host runtime revisions — the
exact "procedure entry point not found" class AR-11 D4 documents. A
PARTIAL Redist staging (payload found but a DLL missing) still fails the
script's existing final critical-files validation. No automated harness
exists for packaging scripts; evidence is the PowerShell parser check
(PARSE OK, recorded in the transcript and here) plus the release pipeline
that exercises the script (`build-msi.ps1` requires a VS installation, so
the release leg always has the payload).

## 4. DiffEngine on the LaneScheduler CPU lane (performance-optimizer finding 8) — DECLINED

The tasking authorizes a decline if the migration is not clean. It is not
clean, on four counts from the actual API (`src/engines/scheduling/LaneScheduler.h`):

1. **No per-task cancellation.** `LaneScheduler` exposes only
   `cancelAll()`, which bumps ONE token shared by every lane of the
   app-wide scheduler instance (`AppContext::scheduler`, also carrying
   OCR/LayoutEnsemble work). CompareMode's Cancel button cancels exactly
   the compare run's promise today (`QFutureWatcher::cancel` →
   `QPromise::isCanceled()`); on the scheduler it would either cancel
   unrelated in-flight OCR/GPU work or require adding a per-task token to
   the V-01/V-02-audited submit path. The finding's premise that the lane
   scheduler's cancel-awareness would refine the cancel granularity is
   backwards for this API.
2. **No progress channel.** The compare worker posts per-stage progress
   through the `QPromise<DiffResult>&` that `QtConcurrent::run`'s promise
   overload provides; `LaneScheduler::submit` takes a bare
   `std::function<T()>` with no promise access. The progress dialog's
   determinate range/value/text (and the R3-perf throttle pin) ride that
   channel.
3. **Delivery type change** (`QFuture<DiffResult>` →
   `QFuture<ScheduledValue<DiffResult>>`) touches the watcher wiring, the
   cancel-state-before-close discipline (the 1.7b trap), and the existing
   compare pins.
4. **Severity/shape** — LOW; the compare is a single long-running task
   (one diff at a time by the §4 row 7 contract), not a fan-out; the
   starvation scenario is bounded, and the r3-perf lane already throttled
   the GUI-side cost.

Declined with rationale; no code changed. Revisit only if the scheduler
grows a per-task cancel token and a promise-exposing submit overload.

## 5a. Decode-bomb dynamic probe (stamp import)

The probe (the adversary's "needs dynamic probe"): a real PNG whose IHDR
declares 20000×20000 (~1.6 GB ARGB32 decode, ~500 bytes on disk), fed
through `StampLibrary::addImageStampTo` with
`QImageReader::setAllocationLimit(1)` (the probe tasking; note the
limit is on `QImageReader`, not `QImage` — the `QImage`-named API does
not exist in Qt 6.11).

**RED finding:** pre-fix, Qt's allocation limit fired inside `read()`,
the null image hit the generic refusal branch, and the user was told
"Could not read dimension-bomb.png as an image (unsupported or corrupt
file)" — a lie (the file IS a valid image) and no typed size refusal.

**Fix:** `addImageStampTo` now checks the reader's HEADER-declared size
BEFORE decoding and refuses past `min(reader allocation limit, 64 Mpx
stamp ceiling)` with a typed reason naming the declared dimensions and
the decode limit ("…declares a 20000x20000 pixel image — too large to
import as a stamp (the decode limit is 1 MB). Resize the image
first."). The ceiling math uses the reader's limit so the probe's
small-limit scenario and production (Qt default limit) both land on the
honest refusal; no pixels are ever decoded; no catalog entry or copied
image is left behind (pinned). With production limits the ceiling is the
256 MB stamp bound — a 4000×3000 photo (48 MB) still imports unchanged.

## 5b. Junction-loop dynamic probe (hot folder) — REAL DEFECT FOUND

The probe: a directory junction loop inside the hot-folder root
(`root/loop` → `root`; created with `mklink /J`, no privileges needed),
with a real PDF in the tree.

**RED finding (the adversary was right):** the ingest scan DESCENDED the
junction. QDirIterator does not treat junctions as symlinks on Windows,
so the walk enumerated the target tree once per loop hop — measured: ONE
real file delivered **64 times** (`loop/real/x.pdf`,
`loop/loop/real/x.pdf`, …), the walk stopping only where the OS refuses
the 64th reparse hop. An accidental bound is not a bound: correctness was
broken (mass duplicate ingest of one drop) and the "bounded" scan was
bounded by a path-resolution quirk, not by design. (On POSIX the same
probe shape is a symlink loop, which QDirIterator does not descend — the
defect is Windows-specific; the Linux leg of the probe pins the symlink
shape and is exercised on Linux runs of this suite — NOT run on this
Windows lane, an honest caveat.)

**Fix:** the whole-tree walk is a new loop-safe engine
(`HotFolderController::walkTree`) used by the seed, the ingest scan and
the watch refresh: an iterative walk with a per-pass VISITED set keyed on
canonical paths (catches POSIX symlink loops and dedups multi-link
paths), PLUS the measured Qt 6.11 behavior that `canonicalFilePath` does
NOT resolve junctions (it reports the junction's own path;
`isSymLink()` is false for them) — so an NTFS junction is a LEAF and is
never descended (catches junction loops the visited set provably
cannot). Documented narrowing: a junction to an EXTERNAL location is no
longer walked (pre-fix it was, loop and all); drops behind it ingest via
the target's own path when that lies inside the root. Side benefit: the
root and a junction onto it no longer get two watch handles (the
duplicate watch double-delivered every event). Pinned by
`directoryJunctionLoopScanTerminatesBounded` (RED pre-fix at 64 ≠ 1;
post-fix: exactly the real drop, exactly once, no `/loop/` path, second
pass silent, the watch walk terminates; the junction is removed by the
test so the suite's temp cleanup stays clean).

## Build & gate record

- Configure: runbook line, CONFIGURE_RC=0 (`build-configure.log`).
- Stage builds: `build-stageA.log` (seams+pins) BUILD_RC=0;
  fix-build retry after ONE compile error (`const_iterator` assignment;
  runbook incremental retry, recorded in the evidence README) → BUILD_RC=0
  (`build-fix*.log`); NC build BUILD_RC=0; restore build BUILD_RC=0
  (`build-restore.log`).
- RED (final harness): `red-final-{hotfolder,formjs,stamp}.log` —
  32/3, 50/1, 10/1, every RED on exactly its load-bearing assertion, all
  guards green.
- NC: `nc-{hotfolder,hotfolder-rerun,formjs,stamp}.log` — each fix's own
  pins RED with guards green; one timing flake
  (`batchModeDisclosesWatchDegradationInLog`, 700 ms debounce-wait pin)
  failed once and passed on the re-run (both recorded).
- Pass-after ×3 SERIAL: `pass1/2/3-*.log` — TestHotFolder 35/35,
  TestFormJsCalc 51/51, TestStampImageImport 11/11, every run.
- Full-tree build + full serial ctest gate: first full-build attempt killed
  externally at [760/873] (the prior lanes' documented contention class),
  resumed incrementally → BUILD_RC=0, 0 FAILED (`build-final-resume.log`).
  Closing full serial gate: run 1 hit ONE flake
  (`TestBatchMode::testBatchWithOneBadFile` — passes 18/18 standalone;
  recorded, not hidden); run 2 **203 passed, 0 failed, GATE_RC=0**
  (`full-gate.log`), the 2 non-runs being the disabled-by-design R14
  probes — the same closing-gate shape as the prior lanes.

## Known limits (honest)

- The junction probe's POSIX leg (symlink loop) was not executed on this
  Windows lane; the code path is `#ifdef`-separated and the Linux CI runs
  the same suite. The Windows junction leg is the one the adversary named.
- The quickjs byte-identity gap (same-version rebuilt/tampered libqjs)
  remains, disclosed in §2 — enforceable scope was the version gate.
- deploy.ps1 has no automated pin; the change is parser-checked and
  release-pipeline-exercised (§3).
- The bounded-set TTL (7 days) and cap (100k) are policy constants chosen
  for multi-MB worst-case memory; both are compressed in tests via the
  documented seams only.
