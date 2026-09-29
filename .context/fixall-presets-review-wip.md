# FIXALL presets review lane — WIP handoff

Lane: Presets review (feat/batch-presets-p2 as NEW, never-reviewed code)
Branch: `feat/fixall-presets-review` (worktree `D:/pdf/pdf-presets-review`,
junction `C:\Users\User\Projects\pdf-featplans`)
Base: **`7eb5c67b`** (= `origin/main` = merged `review/consolidated-parity`).
Status: IN PROGRESS

## Premise resolution (READ THIS FIRST — the task premise was stale)

The task said: checkout `origin/review/consolidated-parity` and review
`feat/batch-presets-p2` as never-reviewed code. Both refs are gone/changed:

- `origin/review/consolidated-parity` no longer exists — the PR was MERGED;
  its head `2ead0b17` is an ancestor of `main`/`origin/main` (`7eb5c67b`).
  Nearest equivalent base = `main`.
- `feat/batch-presets-p2` exists only as archive tag
  `archive/final/feat/batch-presets-p2` → `ec22eeed` (7 commits,
  `7d4d8d08..ec22eeed`). Its content was FOLDED into the PR as cherry-picks
  `ca7deeda..fde3ba46` (U1-U7) + ledger docs `32498fb8`
  (see `docs/audit/FIXALL-PROGRESS-2026-09-25.md` rows 13-19).
- A first-pass fold review WAS recorded (progress log row 20): import
  traversal blocked by "V8 id==stem rule"; schema v1 unchanged; atomic writes
  via `VersionedJson::atomicWrite`; abort-keeps-committed + honest reporting
  pinned; ONE LOW residual: `exportTo` remove-then-copy window.
- That LOW residual was already FIXED on main by `208b6c08`
  (SafeSave commit idiom, TestBatchPresetsP2 34P/0F x3, evidence under
  `docs/audit/evidence-ole-exp/`).

## This lane's mandate (unchanged value)

Independent adversarial security pass over the presets-p2 feature CONTENT as
it sits at the base head — checklist: import/export path traversal,
destructive defaults, schema/version handling, atomic writes, abort keeps
committed files, honest UI reporting. Method: audit-first with adversarial
fixtures; findings numbered **PGR-50+**; fix CRITICAL/HIGH with tests
(fail-before evidence per R7); MEDIUM/LOW fix-or-record. R8 (per-test
temp-roots block stays at END of CMakeLists.txt), R13 (pick discipline),
R16 (never two full test runs at once), no merges, no stash, commit per
cluster, never weaken tests.

## Working notes

### Findings ledger (audit complete 2026-09-29; numbering starts at PGR-50)

| ID | Sev | Where | Finding | Disposition |
|---|---|---|---|---|
| PGR-50 | MEDIUM | `BatchPresetCodec::validate()` (src/core/BatchPreset.cpp) | The save-path gate does not enforce the runnable-shape contract parse() enforces: outputNaming never resolved through the W1-01 containment choke point (a template like `../x.pdf` or `NUL.pdf` saves into the store/UI; only the run refuses), onConflict/onFileFailure enum values unchecked, minAppVersion shape unchecked (garbage semver silently disables the version guard — compareVersions parses non-numeric as 0). Editor cannot produce these fields today; this closes BatchPresetStore::save's public API and honors the documented validate() contract (BatchPreset.h:141-143). | FIX + pins |
| PGR-51 | MEDIUM | `BatchMode::exportRunReport` (src/modes/BatchMode.cpp) | U5 report export writes `QFile(WriteOnly\|Truncate)` — truncate-then-write: the prior report is destroyed at open, so a mid-write IO failure leaves NO report. Same loss class the U6 exportTo fix (208b6c08, PROGRAM-CONSOLIDATION §1.6) eliminated for preset exports. | FIX via SafeSave candidate+commit (FailBeforeCommit seam pin) |
| PGR-52 | MEDIUM | redact `patterns` (preset files → PoDoFoBackend) | Shareable preset redact patterns compile with only isValid(); no per-match timeout (Qt 6 QRegularExpression exposes none; PCRE2 default MATCH_LIMIT is the only bound). A hostile imported preset can stall the batch worker past the next cancel boundary (cancel polls at file boundaries only). No deterministic fail-before harness without a PCRE2 limit seam; interactive path shares exposure. | RECORD / defer — owner item |
| PGR-53 | LOW | `BatchPresetStore::{contains,get,remove,exportTo}` | Store paths built from caller-supplied id without re-checking the slug grammar every other boundary enforces (`[a-z0-9-]{1,64}`). Demonstrable: `remove("../evil")` deletes `root/../evil.glyphpreset.json` OUTSIDE the store (fixture proves). All production callers pass list()-derived ids — defense-in-depth. | FIX + pins |
| PGR-54 | LOW | runPresetChain bates record (src/modes/BatchMode.cpp) | A FAILED bates step's report row sets firstBates to the attempted start although the field contract (BatchMode.h:51-53) says "actually stamped (-1 = n/a)". No existing pin contradicts the fix, but no deterministic mid-chain bates-failure seam exists to prove it (MockPdfEditorEngine not injectable into runPresetChain) — R7 blocks an unproven code change. | RECORD — pin blocked, owner item |
| PGR-55 | LOW | PresetEditorDialog::applyParamFormToStep | Redact preset/pattern entries split on comma without trimming; natural input "email, phone-us" is refused at save though the runtime path (effectiveRedactPatterns) trims. Fail-safe UX nit. | RECORD |
| PGR-56 | LOW | PresetManagerDialog import/export confirms | Conflict-confirm flows keyed on `err.contains("already exists")` — string-match on a human diagnostic; rewording silently degrades to plain refusal (fail-safe, but the flow dies quietly). | RECORD |

Non-PGR notes: (a) task premise stale — base `origin/review/consolidated-parity`
merged to main and deleted; reviewed the feature content AS MERGED (cherry-picks
ca7deeda..fde3ba46 + 208b6c08) at base 7eb5c67b; (b) CMake R8 per-test
temp-roots block verified last at base (CMakeLists.txt:6160-6173); (c) the
known U6 LOW residual is already fixed on main (208b6c08) — not re-found.

## Final state

STATUS: FINAL (2026-09-29, presets review lane) — REBASED onto main `061fea2d`

- **Branch `feat/fixall-presets-review`** (worktree `D:/pdf/pdf-presets-review`,
  junction `C:\Users\User\Projects\pdf-featplans`), linear on `main`
  (`061fea2d` — main ADVANCED +20 commits mid-lane; the branch was rebased,
  which resolved the only clash: both sides appended pins at the
  TestBatchPresetsP2 tail; the integrator's new U6 midway-failure pin is kept
  intact alongside this lane's pins). 3 fix commits + 1 docs commit, 0 merges:
  - `992daad6` PGR-50 — validate() re-checks the runnable-shape fields on the
    save path (naming containment via the W1-01 choke point, policy enums,
    minAppVersion shape). Pins ×4 (3 adversarial + 1 positive control).
  - `3f526601` PGR-51 — U5 run-report export through the SafeSave commit idiom
    (the truncate-then-write loss class). Pin with the FailBeforeCommit seam.
  - `b0d5261d` PGR-53 — the store re-checks the id grammar at its own boundary
    (the fixture PROVED remove("../evil") deleted a file outside the store).
    Pin.
  - `2992113e` docs — handoff + gates evidence.
  (Pre-rebase SHAs `0c095dc1`/`8a151c8f`/`4a96f10f`/`dab7254a` are superseded;
  evidence files are unchanged — the fail-before runs were captured against
  the identical source content.)
- **Pick verification**: `git cherry-pick 992daad6 3f526601 b0d5261d` onto
  `main` `061fea2d` applies CLEAN end-to-end (verified in a throwaway worktree
  and discarded; resulting picks `07e42e55`/`474262f4`/`4cce1b8d`). The chain
  is order-dependent only in that PGR-50 introduces the `isValidStoreId`
  helper PGR-53 consumes — pick in ledger order.
- **Gates (on the rebased head)**: TestBatchPresetsP2 42 slots green (this
  lane's 8 pins + main's new exportToMidwayFailure pin + the 33 pre-existing);
  TestBatchPresets 15P/0F; the 14 active touched-surface suites (everything
  including modes/BatchMode.h or core/BatchPreset.h; R14ProbeBatchSkip is a
  DISABLED probe, not run by design) ×3 consecutive green: 13/13 per run,
  0 failures
  (`docs/audit/evidence-presets-review/4-touched-suites-rebase-run{1,2,3}.txt`;
  the pre-rebase ×3 runs `4-touched-suites-run{1,2,3}.txt` remain on file).
  Fail-before/pass-after per cluster under
  `docs/audit/evidence-presets-review/1|2|3-*.txt` — every fix has a red-only
  fail-before run.
- **Residuals (recorded, not fixed)**:
  - PGR-52 (MEDIUM, owner item): shareable preset redact `patterns` run with
    no per-match timeout (Qt 6 exposes none; PCRE2 default MATCH_LIMIT is the
    only bound); a hostile imported preset can stall a batch worker past the
    next cancel boundary. Needs a PCRE2 limit seam for a deterministic pin —
    not fixable honestly in this lane (R7).
  - PGR-54 (LOW): a FAILED bates step's report row sets `firstBates` to the
    attempted start ("actually stamped" contract says -1). No deterministic
    mid-chain bates-failure seam exists (MockPdfEditorEngine is not injectable
    into runPresetChain) — pin blocked, fix deferred per R7.
  - PGR-55 (LOW): PresetEditorDialog splits redact entries on comma without
    trimming — natural "email, phone-us" input refused at save although the
    runtime path trims. UX-only, fail-safe.
  - PGR-56 (LOW): manager import/export confirm flows keyed on
    `err.contains("already exists")` — brittle string-match on a diagnostic;
    rewording silently degrades to plain refusal (fail-safe).
- **Process notes**: (a) the task premise was stale — `origin/review/
  consolidated-parity` was merged to main and deleted; the lane reviewed the
  feature content AS MERGED (picks `ca7deeda..fde3ba46` + the U6 residual fix
  `208b6c08`) at base `7eb5c67b`; (b) the previously-recorded U6 LOW residual
  (exportTo remove-then-copy) is confirmed already fixed on main; (c) build
  setup for future lanes in a fresh worktree: pass
  `-Dpodofo_DIR=<worktree>/third_party/podofo/install/lib/cmake/podofo` AND
  copy `bin/libpodofo.dll` + `third_party/pdfium/bin/pdfium.dll` from an
  existing lane worktree (both are gitignored binaries) —
  `stage_runtime_dlls` then stages pdfium, per-test POST_BUILD deploys podofo.
- **Handoff discipline**: lane ends here; the branch is NOT merged anywhere
  (no merges rule); the integrator cherry-picks.
