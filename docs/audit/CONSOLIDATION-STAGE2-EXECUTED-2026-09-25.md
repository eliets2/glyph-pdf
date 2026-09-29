# CONSOLIDATION STAGE-2 EXECUTED — 2026-09-25 program, stage 2 (2026-09-29/30)

**Lane:** consolidation stage-2 executor (worktree `D:/pdf/pdf-secfix`, junction
`C:\Users\User\Projects\pdf-sec`). **Owner order:** "merge all of them into 1 for
Claude to review and push to origin" + "remove the other branches keep PR #2 and
main" (the CONSENT for the consolidation; deletions remain a later consent-gated
pass — none executed here). Governing docs:
`CONSOLIDATION-ACTION-PLAN-2026-09-25.md`, `CONSOLIDATION-CRITIQUE-2026-09-25.md`
(E-fixes applied), `CONSOLIDATION-EXECUTED-2026-09-25.md` (stage 1 of the endgame,
by consol-exec2), `CONSOLIDATION-BUNDLE-RECORD-2026-09-29.md`.
**Hard rules honored:** cherry-pick `-x` only; no merges; no stash; no deletions
(local or origin); no force-push; no gc/prune; main untouched; the live
`D:/pdf/pdf` worktree untouched; no test weakened (two pins STRENGTHENED the tree).

---

## 0. Ground truth at stage-2 start (measured, not trusted — the briefing predated reality)

The world had advanced past the stage-2 briefing:

- The 09-29 consol-exec2 run had already folded the 163-branch era into
  `review/consolidated-parity`; **PR #2 = MERGED** (checked via `gh`); the origin
  deletion pass had ALREADY run. `origin` now carries exactly two heads:
  `main` = `audit/sweep-all` = `7eb5c67b` — and `79a7a35f` (consol-exec2's final
  head) is an ancestor of `main`, so **`main` IS the consolidated survivor line**.
  `review/consolidated-parity` no longer exists as a ref (local or origin).
- The remaining ref space: 9 local branches — the survivor (`main`), the LIVE
  audit lane (`audit/sweep-all`), and 7 post-consolidation fix/doc lanes
  (`feat/secfix-v150`, `feat/prodfix-v150`, `feat/ole-export-fixes`,
  `feat/ci-failure-investigation`, `feat/ui-redesign-p0`, `feat/xmp-zoom-ci`,
  `feat/k-fixes`) plus 263 `archive/*` tags.

## 0-bis. Mid-flight event: the parallel integrator salvaged this lane (recorded 2026-09-30)

While this lane's gate ran, a PARALLEL integrator became active (evidence: main
moved `7eb5c67b` -> `73830ee4`; the pdf-secfix and prodfix worktree registrations
were removed from the shared repo and their dotfiles deleted; new
`fixall-*-review` worktrees appeared):

- It folded the LIVE audit lane (`audit/sweep-all` Wave-1, 9 commits -- docs-only,
  zero src/CMake changes verified by diff) into local `main`.
- It cherry-picked this lane's three unique commits (repair `90006a45`->`22b4dea7`,
  pin fold `176c6cb2`->`85abab41`, regex fix `447b3103`->`7e73cc1c`) onto main.
- It salvaged THIS document from the deregistered worktree and committed the
  PRE-gate draft as `73830ee4` (183 lines; missing the E2 100% result and the
  final head -- this file is the authoritative, completed version and supersedes
  the salvaged copy in a follow-up commit on this branch).

**Reconciliation:** this lane's branch was rebased onto `73830ee4` -- the three
salvaged commits auto-skipped (patch-identical), the six picks replayed cleanly.
The remaining delta of this branch over `main` is exactly the six picks (section 3):
the 1.7b images-route fix, the 1.7a staged-seed pin, and the ui-redesign
P0-1 batch -- the content the parallel integrator had NOT folded at salvage time.

## 1. Safety net FIRST (critique A1/E4; per-step committed)

- **9 new annotated freeze tags** (no `-f`, no overwrites; `archive/final/main`
  already existed pinning old main `2b715f47`, so the new pin is
  `archive/stage2/main` → `7eb5c67b`): `archive/final/{feat/secfix-v150→9279516e,
  feat/prodfix-v150→52f6c441, feat/ole-export-fixes→be3a7661,
  feat/ci-failure-investigation→6a63ff63, feat/ui-redesign-p0→bf2a9543,
  feat/xmp-zoom-ci→9575cb56, feat/k-fixes→bb02c1d4, audit/sweep-all→bdee1d79
  (LIVE-lane freeze pin)}`.
- **Bundle refreshed:** `D:/pdf/pdf-archive-final.bundle` (`git bundle create
  --all`), `git bundle verify` = **"The bundle records a complete history."**
  (sha1), 41,477,067 bytes, **498 refs**.
  **SHA-256: `dc4db675db744919f3cbb8b66a9f1d345bdda6951030797b57060dda673d99e7`**
  (the prior net `C:\Users\User\pdf-archive-final-2026-09-29.bundle`,
  `a049c96a…dd6`, remains untouched as a second copy).
- **Restore drill:** `git clone` of the bundle into scratch; `git cat-file -e`
  spot-checks `9279516e`, `bf2a9543`, `bdee1d79`, `7eb5c67b` all OK + blob
  spot-check `7eb5c67b:src/core/BatchPreset.cpp` OK; scratch removed.
- **Tag push:** the 1 previously-unpushed archive tag
  (`archive/final/fix/pr2-followups`) + the 9 new tags pushed; **archive-tag
  parity now local 263 = origin 263** (stray non-archive tags
  `m4-catchup-complete`, `vcpkg-build-final` remain local-only, noted).

## 2. Per-branch disposition — all 9 local branches (patch-id + content probes, not ancestry)

| Branch | tip | beyond-main / patch-unique | Disposition |
|---|---|---|---|
| `feat/k-fixes` | bb02c1d4 | 0 / 0 | **CONTAINED** (ancestry of main) |
| `feat/ole-export-fixes` | be3a7661 | 2 / 0 | **FOLDED** — both commits patch-equivalent in main (the §1.2 OLE magic + §1.6 SafeSave exportTo, pins `proofFailsOnOleCompoundAttachment` + `exportConfirmedOverwriteKeepsTargetWhenCommitFails` verified in main's tree) |
| `feat/prodfix-v150` | 52f6c441 | 6 / 0 | **FOLDED** — all 6 patch-equivalent in main (§1.3 `d36ca7f6`, §1.4 zoom, §1.7a staging, §1.7b pool, §1.7c ModDate, download-gate test) |
| `feat/ci-failure-investigation` | 6a63ff63 | 3 / 0 | **FOLDED** — all 3 patch-equivalent in main (`5390cd58`, `8e773ed7`, `84a30e10`) |
| `feat/ui-redesign-p0` | bf2a9543 | 3 / 3 | **PICKED** (all 3, §3) — unique: baseline docs + R17 evidence + P0-1 `gp::CommandRegistry` (additive, 3901 lines, zero deletions) |
| `feat/xmp-zoom-ci` | 9575cb56 | 7 / 7 | **2+1 PICKED, 4 SUPERSEDED/FOLDED** (§4) |
| `feat/secfix-v150` | 9279516e | 2 / 2 | **SUPERSEDED + 1 PIN RESCUED** (§5) |
| `audit/sweep-all` | bdee1d79 (moving) | 9 / — | **HOLD — LIVE lane** (Wave-1 audit in flight in `D:/pdf/pdf`, dirty `TestSignatureRealCrypto.cpp` there; commits minutes old at freeze). NOT folded, worktree untouched, tip pinned by tag |
| `main` | 7eb5c67b | — | the survivor; untouched (no push to main from this lane) |

## 3. The picks (cherry-pick `-x` onto `feat/consolidated-exec` from `origin/main` `7eb5c67b`)

| Pick | Source | Result | Notes |
|---|---|---|---|
| docs(ui-redesign) Phase-0 baseline | `3adc0d99` | `3807caba` | clean |
| docs(ui-redesign) R17 evidence | `3cc60632` | `cb206b10` | clean |
| feat(ui-redesign) P0-1 CommandRegistry | `bf2a9543` | `75380b87` | clean (CMakeLists applied; new files only) |
| fix(routes) §1.7b uncancellable dialog | `5bd6a34f` | `8acba0a9` | **COMPLEMENTARY, not superseded**: main's pool fix (`6bd5bcfa`, prodfix's) + this = the union; main's `onImagesToPdf` still carried `tr("Cancel")` + the `canceled→watcher cancel` wiring — the exact spurious-canceled-before-start skip observed in CI run 36538793928. Resolutions: TestWelcomeRoutes conflict took main's comment (superset); the lane's own duplicated 14-line comment block de-duplicated (documented cosmetic resolution); pool + cancel-state-read kept |
| fix(ocr) §1.7a staged-seed pin | `9eb15e50` | `b270858f` | CMake staging already in main (`e41dffda`) — CMake conflicts resolved to main's side (comment-only); the pick's value (TestOcrPreprocessPrefs `firstRunOcrSeedIsStagedBesideTheBuild` fast-fail pin + evidence dir) merged clean |
| test(ocr) §1.7a x3 evidence | `9575cb56` | `3094739f` | clean |

## 4. feat/xmp-zoom-ci — the other four commits, dispositioned with content probes

- `35c95678` (§1.3 XMP) — **SUPERSEDED** by main's `d36ca7f6` (prodfix's
  patch-identical fix; `SyncXMPMetadata(false)` no-reset verified at
  PoDoFoBackend.cpp:792; main's pin `expiryMarkerSurvivesADocumentPropertyEdit`).
  xmp's variant carries a duplicated comment block and a parallel pin.
- `20f27df0` (§1.4 zoom) — **SUPERSEDED**: main holds prodfix's patch-eq version
  incl. the TestViewParity pins (`zoomInAfterFitLeavesFitMode`,
  `ZoomMode::Custom` ×4 in PdfViewerWidget.cpp).
- `e3b4af21` (§1.7c later-second) — **FOLDED**: main's `7eb5c67b` carries the
  exact pin `testDssAppendDoesNotReemitInfoAcrossSecondBoundary` + the same
  NoMetadataUpdate guard (folded-with-resolution pick).
- `77bf1da6` (download-gate) — **FOLDED**: main's `bb02c1d4` is the same
  substance (word-level comment drift only).

## 5. feat/secfix-v150 — two alternative implementations superseded, one orphaned pin rescued

- `657032f6` (§1.2 OLE) and `9279516e` (§1.6 exportTo) are the secfix lane's
  independent implementations of the SAME two PROGRAM items the ole lane's
  patch-equivalent picks already cover in main (same magic byte list; equivalent
  atomic-replace idiom — main's exportTo reads source bytes before staging the
  SafeSave candidate and only `commitFileToDestination` touches the destination,
  so secfix's midway-failure invariant holds structurally). **SUPERSEDED** with
  this written disposition (critique E5 standard).
- **Discovery:** the secfix lane's §1.6 pin
  (`exportToMidwayFailureLeavesTheTargetIntact`) was NEVER committed — `9279516e`
  shipped the src fix without the pin; it sat uncommitted in the lane worktree
  (its handoff records pass-after runs that included it). Rescued: preserved at
  `.context/secfix-16-pin-uncommitted.patch`, folded onto the consolidated tree
  as `176c6cb2` (assertions verbatim; idiom comment adapted; the pin PASSES
  against main's implementation — it is the proof of the supersession).
- The lane's own §1.1 CSV fix (`b1854eae`) is in main via `feat/k-fixes`
  (ancestry), exactly as the secfix handoff's R12 note anticipated.

## 6. Consolidation-integrity repair — main's tip was BROKEN (found by this stage's gate)

`origin/main` @ `7eb5c67b` **does not compile**: the 1.7c pick `7eb5c67b` merged
the two signing pins into a mangled union — `testOwnModDateRefreshNotDowngraded`
lost its body (its opening was glued into the second-boundary pin: duplicate
`QString output`, an unterminated `QVERIFY(` call), and the boundary pin's tail
assertions were spliced onto the wrong fixture walk. Since the breakage is
syntactic, NOTHING on main after `cd5a949d`+`7eb5c67b` can ever have compiled —
main's tip was pushed unverified.

- `90006a45` reconstructs BOTH pins verbatim from their pristine sources
  (`cd5a949d` for the save-noise pin, `e3b4af21` for the boundary pin); no
  assertion weakened or added.
- `447b3103` repairs the reconstruction itself: one escaping level was lost in
  the regex literal (`\s` for `\\s` — caught immediately by the pin's own red
  "fixture must carry an indirect trailer /Info"); body now diff-verified
  byte-identical to the pristine pin.
- Post-repair: **TestSignatureRealCrypto 30P/0F/1 skip** (the 1 skip is the
  fixture-conditional slot, identical to the lanes' own recorded evidence).

## 7. Gates at the consolidated head `447b3103`

- **Re-gate at the final head `1b3fe0b8`** (after the rebase + this doc): build
  reconfigured + full rebuild green (1065/1065 targets, exit 0; the parallel
  integrator's cleanup had swept build-sec/CMakeCache.txt and this worktree's
  dotfiles -- restored from HEAD, cache regenerated with the lane flags); the 9
  touched suites green; fresh full serial `ctest -j 1` offscreen:
  **100% tests passed, 0 failed out of 188** (`/d/stage2-ctest-serial3.txt`;
  R14ProbeBatchSkip disabled by design). Earlier runs: 188/188 at the
  pre-rebase gated tree (serial2), 186/188 serial1 with both reds green solo x3.
- **E1 build:** full Release rebuild of the switched base (PCH purged after the
  base switch brought PCH-fed header changes; Ninja recompiled the tree, 697
  targets, exit 0; vendored PoDoFo 1.1.0 DLL verified
  `b25f21f9…087` = the lanes' vintage; tessdata staged).
- **Touched suites:** TestExpiryInterface, TestWelcomeRoutes, TestBatchPresets,
  TestBatchPresetsP2, TestOcrPreprocessPrefs, TestRedactionProof, TestViewParity,
  TestCommandRegistry — **8/8 PASS**; the flake-prone trio
  (WelcomeRoutes/BatchPresetsP2/CommandRegistry) **×3 consecutive greens**.
- **E2 full serial gate:** `ctest -j 1` offscreen at `447b3103` — **run 2:
  100% tests passed, 0 failed out of 188** (`R14ProbeBatchSkip` disabled by
  design; transcripts `/d/stage2-ctest-serial2.txt`). Run 1 had 186/188 —
  `TestSidecarReopenState` + `TestFileHandleCoordination` red under serial load;
  both suites are byte-identical to main's (no pick touches their surfaces) and
  passed **solo ×3 immediately after** (Sidecar 1.3–21 s, FileHandle 17–21 s;
  the serial reds were an "Access denied" commit race and a modal-timing flag,
  the documented load-flake class of the FIXALL gate precedent).
- **E3-style:** linear (0 merge commits `origin/main..HEAD`); purge intact
  (no CLAUDE.md/SECURITY.md touches); no secrets added; `-x` trailers verified
  on all 6 picks.

## 8. Push

`feat/consolidated-exec` @ the head below (the doc commit rides the branch, consol-exec2 precedent) → `origin review/consolidated-parity` (ref CREATION —
the name is free since the merged PR #2's head branch was deleted; no force, no
main push, `audit/sweep-all` untouched). This doc rides the pushed branch.

## 9. Residuals carried (not blocking, not lost)

1. **Merge to main** remains the owner's post-review step: `main` (7eb5c67b) is
   the direct ancestor of the consolidated head — the FF push to main after
   Claude's review is trivial and safe.
2. **`audit/sweep-all` is LIVE** (Wave-1 audit: security sweep, verification,
   archaeology, adversarial — AD-11/AD-01 findings at `bdee1d79`). Its worktree
   is dirty (`tests/TestSignatureRealCrypto.cpp`). Fold its FINAL state in a
   later pass; the freeze tag + this bundle pin today's state.
3. **The office route** (`onOfficeToPdf`) still carries the same
   Cancel-button/canceled-wiring shape xmp's §1.7b fix removed from the images
   route (no CI observation, no pin) — owner item, do not fix blind in a
   consolidation stage.
4. **Tracked test-only signing fixtures** (`tests/fixtures/signing/*.key/.p12`,
   R2-8 "fresh test-only signing fixtures", commit `666ac611`) are tracked by
   long-standing decision — recorded here so the secret-scan class is not
   re-flagged as a new regression.
5. **Deletion pass** (local lane branches + stale archive classes) remains the
   owner-ordered, consent-gated, fail-closed `delete_proven()` pass AFTER review
   + merge — every tip is now tag-pinned AND bundle-covered.
6. The 06:01 continuity automation: already removed (verified earlier runs);
   confirm again before any future deletion pass.
7. Two local-only non-archive tags (`m4-catchup-complete`,
   `vcpkg-build-final`) — junk-class, unpushed, owner's call.

**Executor raw evidence:** `.context/stage2-exec-wip.md`,
`.context/secfix-16-pin-uncommitted.patch`, `/d/stage2-ctest-serial.txt`.
