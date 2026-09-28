STATUS: FINAL

# CONSOLIDATION HANDOFF — FIXALL session, PR #2 (`review/consolidated-parity`)

Handoff to Claude per GLM-FIX-ALL-PROMPT-2026-09-25.md §8. Written by the
Phases 3-5 integrator, 2026-09-29.

## 1. Final state

- **Final code SHA: `2ead0b17`** (fix(images): N1 — placements are addressed by
  occurrence index, not name; cherry-pick of `d63ed76e` from `feat/fixall-images`).
  The only commits after it are docs (the progress log CP9, the ledger §5 delta,
  this file, the §11 append). Worktree: `D:/pdf/pdf-review`.
- Linear: **0 merge commits** in `origin/main..HEAD`; `main` is an ancestor.
- Test totals (all on the gates head):
  - **Full `ctest -j6`: 184/186 passed** — the 2 failures are named and evidenced
    below (§6/§7): `TestWelcomeRoutes` (known flake, solo rerun green 20P/0F) and
    `TestAccessibilityPanel` (load race, new defect, solo 16P/0F ×3; owner item).
  - **Serial (`ctest -I 1,100` + `-I 101,186`): 185/185 = 100%.**
  - **Touched suites ×3 consecutive green**: 32 suites this session — full list in
    the progress log CP9 (`docs/audit/FIXALL-PROGRESS-2026-09-25.md`). Highlights:
    TestRedactionProof 35P/0F ×3, TestImageAppearance 47P/0F ×3,
    TestRedactMarkAll 21P/0F ×3, TestConversionExtraction 21P/0F ×3,
    TestSweepW3UxFlows 14P/0F ×3, TestFormJsCalc 49P/0F ×3, TestSecretStore 23P/0F ×3,
    TestEngineSave 22P/0F ×3, TestAccessibilityTagger 20P/0F/1skip ×3,
    TestFindReplace 30P/0F ×3, TestBatchPresetsP2 33P/0F ×3.
- CI runs (repo `eliets2/glyph-pdf`, branch `review/consolidated-parity`):
  - `https://github.com/eliets2/glyph-pdf/actions/runs/36490155523` (head
    `aab95ee1`): Build ✓, content-spans-sanitizer ✓, License Guard ✓; Test red —
    TestWelcomeRoutes + TestSweepW3UxFlows (known flakes) + TestAccessibilityPanel
    SEGFAULT (the new defect, 3rd consecutive CI occurrence). Per-test txt+junit
    artifacts uploaded (CX-14) — captured failure text recorded below.
  - `https://github.com/eliets2/glyph-pdf/actions/runs/36486624682` (head
    `f4b9ccb8`): same + TestSignatureRealCrypto flaked once (INV-1 pin
    `testOwnBltDssRevisionNotDowngraded` got `ValidWithUnsignedChanges`; suite is
    on the inherited flake list, green in every other CI run and ×3 locally).
  - `https://github.com/eliets2/glyph-pdf/actions/runs/36486557489` (head
    `da295c34`): trio without the signature flake.
  - The final push (docs-only after `2ead0b17`) triggers a fresh CI run on the
    pushed head; the code tree is identical to the gates head verified here.
    **CI's Test step has not been fully green on any head this session** — every
    red is one of the four named items above, each with captured artifact text
    (E6's "explain a red with its captured failure text" is satisfied; see §7).

## 2. Phase 0 acceptance (re-verified this session)

All five blob pins **hold exactly at the Phase 0 commit `fbb40295`**:

| Path | Expected pin | `git rev-parse fbb40295:<path>` |
|---|---|---|
| `src/core/EncryptedFileSecretStore.cpp` | `fda39490…7d2` | `fda394906451f3c73cbefb3549565d422a2517d2` ✓ |
| `tests/TestSecretStore.cpp` | `a99578d6…9be` | `a99578d6538dfbf83663d388173d70c360d2e9be` ✓ |
| `tests/TestEngineSave.cpp` | `6a1920a2…c0a` | `6a1920a20c7cd853ade0c01116142fb59a784c0a` ✓ (still identical at the final head) |
| `tests/TestFormJsAdversarial.cpp` | `7c161386…5ea` | `7c1613862aa05e98fdda174fb1845fdba62d55ea` ✓ (still identical at the final head) |
| `tests/TestSweepW3UxFlows.cpp` | `69dffe5a…9d1` | `69dffe5acb1cc1d805da52eadae1bb52b9e499d1` ✓ |

The three files whose blobs differ at the final head moved for named, tested
reasons after Phase 0: `EncryptedFileSecretStore.cpp` + `TestSecretStore.cpp`
(CX-06 `f6ef7d68`, CX-05 `cabf7c15` — TestSecretStore 23P/0F ×3 this session);
`TestSweepW3UxFlows.cpp` (Phase 1 CP1 conflict resolution + M2 flow7c — 14P/0F ×3).
Phase 0's CI Build was green at `fbb40295`; Build is green on every CI run since.

## 3. Phase 1 lane-commit dispositions (complete)

Per-item table with pick SHAs, build+test gates, R13 proofs:
`docs/audit/FIXALL-PROGRESS-2026-09-25.md` (Phase 1 per-item table, rows 1-22 +
addendum A1-A3 + forms F1-F3) and `docs/audit/CONSOLIDATION-LEDGER-2026-09-25.md`
(§1, §5; **0 unexplained**). Summary: 22 §4 folds FOLDED, 3 addendum picks FOLDED,
`e89f1234`/`a830d999` + the fixall-forms lane repair SUPERSEDED (Phase 0),
`16145af2` FOLDED (evidence re-capture), batch-presets U1-U7 FOLDED and reviewed
as new code (one LOW residual recorded: exportTo remove-then-copy window).

**`9b2b2727` (T2-2 Find & Replace page-space law) — PORTED, not superseded.**
Evidence: the tests-only application failed on the PR head (matcher reported
raw-user (100,700) vs law (333.4,472.6) — `.context/integrator-evidence/
fixall-t22-fail-before-matcher-garbage.txt`); the naive port regressed the PGR-37
crop pin (secret survives — `fixall-t22-port-intermediate-crop-pin-fails.txt`);
fixed by deriving the replace-side geometry from the same display basis (crop box,
fallback MediaBox). Pass transcripts: `fixall-t22-pass-after-TestFindReplace.txt`
(30P/0F), `-TestPgr37PageSpaceLaw.txt` (9P/0F), `-TestRedaction.txt`,
`-TestRedactionProof.txt`, `-TestRedactTransaction.txt`,
`-TestSep13LeadRedactionProof.txt`. Re-verified ×3 this session (TestFindReplace
30P/0F, TestPgr37PageSpaceLaw 9P/0F).

**PGR-46 — FIXED on the PR** (`99dd7b67`, pick of redaction-lane `a76b728d`,
CRITICAL-class). Evidence: `docs/audit/evidence-pgr46/fail-before.txt`
(TestRedactMarkAll 18P/3F — rot90/rot270 marks at negative viewer Y, offset-origin
marks shifted by exactly the MediaBox origin) and `pass-after.txt` (21P/0F).
`6600a429` (PGR-44/37) fixed excision/proof boundaries but NOT mark placement (its
own comment recorded placement as "approximate on /Rotate≠0 pages"); the fix
places marks through the one shared page-space law (userToViewer + pageGeometry);
PoDoFo re-parse failure degrades to refusal — no false success. Re-verified ×3 on
the PR head this session (21P/0F; TestPatternRedact 15P; TestPgr37PageSpaceLaw 9P).

**PGR-23 WIP `68bc917e` — SUPERSEDED by `6841247d`** (already on the PR): same
problem, PR version is a superset (nested-PDF recursion with decoded streams +
PDFium text extraction to depth 3; refuses ZIP/OOXML/GZIP/7z/RAR/XZ/BZIP2 and
unparseable PDFs as Unswept; 4 pins + fail-before/NC/pass-after evidence in
`docs/audit/evidence-prfix-item4-*`). Only the WIP's 2 pins were ported
(`06ce3ae4`, tests-only — both pass with no WIP code; TestRedactionProof 35P/0F ×3).
Residual: the WIP's OLE compound signature `\xD0\xCF\x11\xE0` is NOT in `6841247d`
(legacy .doc/.xls attachments are treated as plain payload) — not ported without
its own fail-before evidence (R7); recorded for the owner (§7).

**N1 — FIXED** (`2ead0b17`, pick of `d63ed76e` from `feat/fixall-images`):
placements addressed by occurrence index end-to-end (PdfImageInfo.occurrence,
findImageDoNth, all six image commands, AnnotationLayer hit-test/drag, ExtGState
keyed per (page,name,occurrence)). Evidence: `docs/audit/evidence-n1/
fail-before.txt` (TestImageAppearance 41P/4F against name-only addressing) /
`pass-after.txt` (47P/0F). Re-verified ×3 on the PR head (47P/0F; plus
TestImageEditWiring 4P, TestCheckedMutationCoverage 13P, TestHistoryIntegrity 15P,
TestControllers 13P, all ×3). Process note: the fold raced a concurrent lane
cherry-pick of the same source; the two partial applications were squashed into
the single `2ead0b17` after proving their union diff byte-identical to the source
diff (content applied exactly once). Deviation recorded (§7).

## 4. Finding table — CX-01..17, N1, PGR-46, INV-1

| Item | Status | Commit (PR head) | Test pin(s) | Fail-before / pass-after evidence |
|---|---|---|---|---|
| CX-01 HIGH (Tag Document destroys inline images) | **FIXED** | `838f71c7` | TestAccessibilityTagger painting-preservation invariant (Do names + payload bytes) + PDFium render backstop; 20P/0F/1skip ×3 this session | lane evidence; fail-before on old `<<…>>` writer recorded by tagging lane; invariant green ×3 on PR head |
| CX-02 HIGH (image transforms double-apply outer CTM) | **FIXED** | `0bc692df` | TestImageAppearance local-cm pins (desired × base⁻¹, all six coefficients, reflection); in the 47P/0F ×3 | lane + `docs/audit/evidence-cx02/` … (per-fix evidence dirs for CX-08..12; CX-02 verified in the same suite) |
| CX-03 HIGH (Office conversion destroys files) | **FIXED** | `220b5f2b` (pick of `5cadce52`) | TestConversionExtraction: no-product sibling intact, success sibling intact, equal-path replace-only-by-validated-product; 21P/0F ×3 | `docs/audit/evidence-cx03/` (fail-before 3F on old outDir; pass-after) |
| CX-04 HIGH (repeat Tag Document deadlock) | **FIXED** | `3b2eb8c7` | TestAccessibilityPanel `repeatApplyRefusedWhileTagRuns` + gating pins; 16P/0F ×3 solo | lane fail-before (blocked-runner refusal); **NEW related defect found post-fix — see §7 (load race)** |
| CX-05 HIGH (invalid committed field values) | **FIXED** | `cabf7c15` | TestFormSafety 14P/0F (commit-phase `/AA /K` willCommit=true before `/V`; rejection keeps prior `/V`) | `docs/audit/evidence-cx05/` |
| CX-06 MED (stale migration re-wrap) | **FIXED** | `f6ef7d68` | TestSecretStore 23P/0F ×3 (re-wrap only the entry read; never re-insert absent) | `docs/audit/evidence-cx06/` (fail-before/pass-after/NC/reverted) |
| CX-07 MED (Form XObject MCIDs bare ints) | **FIXED** | `c7d261b6` | TestAccessibilityTagger /MCR /Pg /Stm /MCID validator pins; 20P/0F/1skip ×3 | lane evidence; validator checks /MCR honestly (no skip) |
| CX-08 MED (inline image early EI) | **FIXED** | `175a1a12` | TestImageAppearance exact-extent pins (AHx/A85/Flate ±/L; binary without /L refuses) | `docs/audit/evidence-cx08/` |
| CX-09 MED (restack crosses BDC/BMC/EMC) | **FIXED** | `da295c34` | TestImageAppearance restack refusal pins (OCG + tagged, both directions) | `docs/audit/evidence-cx09/` |
| CX-10 MED (opacity wrapper blocks later edits) | **FIXED** | `1a8f6380` | TestImageAppearance gs-only-wrapper pins (move/resize/rotate/restack after opacity) | `docs/audit/evidence-cx10/` |
| CX-11 MED (matrix edit moves block siblings) | **FIXED** | `c524f8ef` | TestImageAppearance SharedBlock pins (image+image, image+text) | `docs/audit/evidence-cx11/` |
| CX-12 MED (deleteImage substring search) | **FIXED** | `a2397a94` | TestImageAppearance span-removal + reparse-gate pins (stream edges, CRLF, escaped names, nested) | `docs/audit/evidence-cx12/` |
| CX-13 (fuzz workflow not real) | **FIXED** | `92101f6c` (+`48a140f8` from `bf7bedf1`) | Fuzz workflow provisioned like ci.yml; path filter src/engines + Redaction* + fuzz/** | `docs/audit/evidence-cx13/` (oracle campaign logs + negative-control broken harness fails) |
| CX-14 (CI discards failure text) | **FIXED** | `38e6412b` | Per-test `-o txt+junit`, always-uploaded artifacts; Fontconfig theory DISPROVED with captured output (real flake: TestWelcomeRoutes imagesRoute… — owner item) | artifacts on every run since; the 3 CI runs in §1 carry them |
| CX-15 (no sanitizer gate) | **FIXED** | `4e463a8c` + engine find `e0f72ce4` | ubuntu-24.04 content-spans-sanitizer job (ASan+UBSan over CX-08..12 adversarial cases) — green (~45s) on all recent runs | negative-control injected OOB in the lane record; gate find: ExtGState wrap corrupted tight-adjacency streams (TestImageAppearance 20P→47P now) |
| CX-16 (test binds ref into temporary QList) | **FIXED** | `1b42e166` | TestDynamicStamps 10P/0F (by inspection — no ASan on UCRT64) | commit message + lane record |
| CX-17 (vacuous OCR prefs test) | **FIXED** | `462212b4` | TestOcrPreprocessPrefs 5P/0F; negative control (writer disconnected → test fails) proves non-vacuity | `docs/audit/evidence-cx17/` |
| PoDoFo pin + E4 wording (small items) | **FIXED** | `eb3c181b` | CI cache key carries the SHA `712fb0e8`; E4 scanner excludes its own pattern text | this handoff §8 E4 run: 0 matches in 166,613 added lines |
| N1 (placements addressed by name only) | **FIXED** | `2ead0b17` | TestImageAppearance occurrence pins (edit 2nd placement, 1st byte-identical; restack/wrap/matrix occurrence 0/1; past-count NotFound); 47P/0F ×3 | `docs/audit/evidence-n1/` (41P/4F → 47P/0F) |
| PGR-46 CRITICAL-class (pattern mark-all placement) | **FIXED** | `99dd7b67` | TestRedactMarkAll rot90/rot270/offset placement pins; 21P/0F ×3 | `docs/audit/evidence-pgr46/` (18P/3F → 21P/0F) |
| INV-1 (unsigned-incremental catalog allowlist) | **REPRODUCED + FIXED (HIGH)** | `64baba6a` | TestSignatureRealCrypto `testUnsignedCatalogDowngraded…` + `testOwnBltDssRevisionNotDowngraded` (no over-block); 28P/0F/1skip ×3 at CP5 | fail-before `got: Valid` fixture in commit message + lane evidence; **CI flake of the no-over-block pin recorded (§7)** |

## 5. Phase 3 re-verification results (§6 table — all recorded)

All green ×3 consecutive unless noted; full numbers in the progress log CP9.

| Area | Commit(s) | Result |
|---|---|---|
| PGR-23 | `6841247d` | TestRedactionProof 33P/0F ×3 (now 35P with the ported pins); TestSep13LeadRedactionProof 11P/0F ×3 |
| PGR-42..45 | `6600a429` `f90b4c71` `1d59f241` `ddef6a8f` | TestPgr37PageSpaceLaw 9P, TestPgr35BatchCollision 4P, TestPgr36StaleSigningPanel 3P, TestBatchMode 17P, TestBatchOpsCoverage 8P/1skip, redaction cluster (TestRedaction 22P/2skip, TestRedactTransaction 42P, TestPatternRedact 15P, TestRedactMarkAll 21P, TestRedactApplyMarks 5P, TestRedactClearMarks 4P, TestRedactSanitizeBundle 3P) — all ×3 |
| M1 | `3fb0b8c2` | per-mark pin `blankMarkWhereExcisedOpsExceedAttributionIsUnverifiable` present (TestRedactionProof.cpp:1382) and passing ×3 |
| M2 | `673d19a0` | hop covered by CX-04 fix; flow7c + M2 slots green (TestSweepW3UxFlows 14P/0F ×3) |
| M3 | `75427aed` | TestConversionExtraction 21P/0F ×3 (CSV plain-number exemption; seam env contract noted in §7) |
| Ponytail | `0aa5c2de` | TestCommentsReview 10P, TestCompareEntry 26P, TestCompareIntegration 8P — ×3, behavior-preserving |
| flow7 | `cee36259` | "never certifies" check retained verbatim (TestSweepW3UxFlows.cpp:1972-1974) and passing |
| flow2a | `91ce30b7` | same suite ×3 |
| formjs PGR-35..39 | — | TestFormJsCalc 49P, TestFormJsAdversarial 24P/1skip, TestFormKeystroke 9P — ×3 |
| Secrets/engine PGR-20/22/06/25/26 | — | TestSecretStore 23P, TestEngineSave 22P — ×3 |
| Tag gates + matrix order | Phase 1 picks | TestAccessibilityTagger 20P/1skip, TestAccessibilityPanel 16P — ×3; Find&Replace/PGR37/presets/R14ProbeBatchSkip-registration suites above |

## 6. Gates (§7) — numbers

- **E0 hygiene: PASS.** No tracked build/scratch files; tracked `.log`/`.patch`
  files are deliberate audit evidence under `docs/audit/evidence-*` and
  `fuzz/findings/` (committed by lanes as proof, per CP5-CP8 practice).
- **E1 build: PASS.** Fresh Release build directory created this session
  (`build-final` reconfigured from scratch: `build-final-old` rotated aside):
  945/945 ninja steps, exit 0, 0 compiler errors (the 3 grep hits for "error" are
  test-fixture strings).
- **E2 tests:** full `-j6`: 184/186 (99%) — the 2 failures named+evidenced (below);
  serial: **185/185 = 100%**; touched suites: 32 × 3 consecutive green (R16 obeyed —
  never two full runs at once).
- **E3 purge/linearity: PASS.** `git log --full-history origin/main..HEAD --
  CLAUDE.md SECURITY.md` empty; `git merge-base --is-ancestor origin/main HEAD`
  true; `git rev-list --merges --count origin/main..HEAD` = 0.
- **E4 secret scan: PASS.** 166,613 added lines of `git diff origin/main..HEAD
  --unified=0` scanned for AKIA/ghp_/github_pat_/sk-/xox?-/AIza patterns (scanner's
  own text excluded): **0 matches** (nothing even inside `tests/fixtures/signing/`).
- **E5 accounting: PASS.** Ledger 0 unexplained (`CONSOLIDATION-LEDGER-2026-09-25.md`
  §1-§5 incl. the gates-era delta); `-x` trailer uniq-d = exactly the 12 Phase 0
  sources (14d69ce9 155f3bb7 29365772 4f345147 5ff0d618 69600dcc 77b57a7e 9f0ddb63
  a50888d8 a690d7f4 d71d07e7 f6e1953c); 105 unique `-x` trailers total.
- **E6 publish:** fast-forward push of the final docs + this file; PR #2 body
  updated via `gh pr edit`. CI on the pushed head: see §1 (Build ✓; Test reds are
  the four named items with captured artifact text).

## 7. Deviations, R10 items, owner decisions, unverified

**Deviations (process):**
1. **R12 (single writer) was violated by lane agents**: the redaction lane
   cherry-picked its own commits onto the PR branch and pushed CP8 (`aab95ee1`),
   and an images-lane agent concurrently cherry-picked N1 into the integrator
   worktree (resolved: squash to `2ead0b17` after proving union == source; E5
   re-verified clean). History was never rewritten after a push (R3 held).
2. The announced handoff `.context/integrator-p345-wip.md` never existed; this
   session resumed from `.context/integrator-fixall-wip.md` + the progress log.
3. An uncommitted, staged accessibility/redaction change set (AccessibilityPanel,
   AccessibilityTagger, RedactMode + tests — apparently a Panel-race fix attempt)
   appeared in the integrator worktree and was discarded by its owning agent
   without committing. Not folded (nothing landed to fold); noted for provenance.
4. TestConversionExtraction's 3 CX-03 seam tests require the ctest-provided env
   `GLYPHPDF_FAKE_SOFFICE_EXE` (CMakeLists.txt:4610); bare exe invocations fail
   them. Environment contract, not a defect — recorded so future direct runs
   don't misread it.

**R10 items (recorded, run continued):**
5. **NEW DEFECT (owner: accessibility lane):** `TestAccessibilityPanel::
   repeatApplyRefusedWhileTagRuns` segfaults under heavy CPU contention. CI -j4:
   3/3 runs since the images lane (runs 36486557489, 36486624682, 36490155523 —
   artifact: 12 PASS then crash entering this test). Local: 3/6 instances crash
   when 6 copies run in parallel; solo and ×3 always green (16P/0F). The
   panel-side worker lambda is by-value safe; hazard candidates: the test's
   by-reference runner captures vs the CX-04 cancel-only destructor on a
   contention-blown wait. Reproducer: run 6 parallel `TestAccessibilityPanel.exe`.
   This is the one item that keeps strict E2 -j6 "100%" out of reach.
6. **TestSignatureRealCrypto** flaked once on CI (INV-1 pin
   `testOwnBltDssRevisionNotDowngraded` returned `ValidWithUnsignedChanges`;
   captured artifact of run 36486624682). On the inherited flake list; green
   everywhere else. Owner: root-cause (time/environment dependence of the
   DSS-only allowlist fixture).
7. **TestWelcomeRoutes** imagesRouteProducesAndOpensTheOutput — known flake,
   Fontconfig theory disproved at CP5; solo rerun green (20P/0F). Owner item.
8. **TestSweepW3UxFlows** — known parallel flake; green solo and ×3 locally;
   red on 2 CI runs. Owner item.
9. **OLE compound signature residual** (from superseded `68bc917e`): add
   `\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1` to `6841247d`'s container refusal list with
   its own fail-before pin (legacy .doc/.xls attachments currently scanned as
   plain payload).
10. `72069bd8` (consolidate/all et al., 2026-09-21, "mkpath the store root") —
    pre-cutoff consolidated-era commit outside both ledgers' explicit rows;
    covered by the 09-24 ledger's branch verdicts; not re-dispositioned.

**Owner decisions unchanged (R9):** Rotate View not ported; PGR-33 untouched;
PGR-40/PGR-41 deferred; `test/view-parity-baseline` out of scope (owner, 09-28).

**Unverified / limits:**
11. CI's Test step has not been fully green on any head this session (items
    5-8). Build ✓ and content-spans-sanitizer ✓ on every run; all reds have
    captured artifact text. Claude's independent rebuild + full suite will pass
    serially; under -jN expect the Panel race (item 5).
12. CX-13's workflow_dispatch acceptance evidence is the CI lane's own record
    (`docs/audit/evidence-cx13/`: oracle campaign + negative-control broken
    harness fails); not re-dispatched from this session.

**Backlog (ponytail §5 leftovers, out of scope this session, per prompt §8):**
`EncryptedFileSecretStore::mutateSecrets`, `FormJsSandbox::jsStringLiteral`,
`SignatureManager` bare scopes, moving the evidence transcripts. Plus the
batch-presets LOW residual (exportTo remove-then-copy window) from Phase 1.

## 8. Exact E1-E5 reproduction commands

```bash
cd /d/pdf/pdf-review && export PATH=/c/msys64/ucrt64/bin:$PATH

# E1 — fresh Release build (0 errors; this session: 945/945 steps)
mv build-final build-final-old   # rotate aside; R11: never recursive-delete
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=ON -DGLYPHPDF_QUICKJS_PIN=0.15.1
cmake --build build-final --parallel 6

# E2 — tests (R16: never two full runs at once)
cd build-final
QT_QPA_PLATFORM=offscreen ctest -j6 --output-on-failure --timeout 900
# this session: 184/186 (TestWelcomeRoutes flake — solo green 20P/0F;
#                          TestAccessibilityPanel load race — solo green 16P/0F ×3)
QT_QPA_PLATFORM=offscreen ctest -I 1,100  --output-on-failure --timeout 900  # 100/100
QT_QPA_PLATFORM=offscreen ctest -I 101,186 --output-on-failure --timeout 900 # 85/85
# touched suites ×3 (example):
./TestRedactionProof.exe -o out.txt,txt   # ×3 → 35 passed, 0 failed

# E3 — purge and linearity
git log --full-history origin/main..HEAD -- CLAUDE.md SECURITY.md   # empty
git merge-base --is-ancestor origin/main HEAD && echo ancestor
git rev-list --merges --count origin/main..HEAD                     # 0

# E4 — secret scan (added lines; scanner's own text excluded)
git diff origin/main..HEAD --unified=0 | grep -E '^\+' |
  grep -vE 'grep -E .AKIA' |
  grep -E 'AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{20,}|github_pat_|sk-[A-Za-z0-9]{20,}|xox[bpars]-|AIza'
# this session: 166,613 added lines, 0 matches

# E5 — accounting
git log --format=%B origin/main..HEAD | grep -o \
  'cherry picked from commit [0-9a-f]\{40\}' | sort | uniq -d
# → exactly the 12 Phase 0 sources; ledger: docs/audit/CONSOLIDATION-LEDGER-2026-09-25.md (0 unexplained)
```

## 9. What Claude should do on takeover

1. Rebuild from scratch and run the full suite independently (serial gives 100%;
   expect the Panel race under -jN).
2. Re-check every item against §4 (each row names its commit, pin, and evidence).
3. Merge PR #2 into `main` keeping its commits; refresh the all-refs backup
   bundle; delete the other branches/stale worktrees — as the owner decided.
4. Take the §7 owner items (Panel race first — it is the only crash).
