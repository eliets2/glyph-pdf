# FOLD-2 TRIAGE — `feat/ocr-verify-finereader` (B1–B15) port to current main

Lane: `feat/ocr-verify-port` (worktree `D:/pdf/pdf-w2-bfr`).
Base: **02d1a898** (origin/main, verified 2026-09-30).
Archive: tag `archive/final/feat/ocr-verify-finereader` = f5b59e66d0c1975cd46cebbb76cb3a4e419326a5.
Port discipline: PROGRAM-CONSOLIDATION §3.6 — patch-only, no merges, every ported unit
is a new commit with provenance + R7 evidence (discriminating pin RED on main, NC, ×3 serial pass).

## Where the B-numbering comes from

The archived branch's own commits carry the B-numbers in their subjects
(`feat(ocr-verify): B1 …` … `B15 …`), planned in
`docs/planning/OCR-VERIFY-ABBYY-STUDY-2026-07-01.md` (§3 items 1–11 → B-series).
**B8 and B13 have no commit anywhere on the archived branch** (verified by searching
every commit subject and body on the branch for "B8"/"B13" and the archived test file
`tests/TestOcrVerify.cpp` for per-B test coverage: B1–B7, B9–B12, B14–B15 all have
tests; B8/B13 appear nowhere). They were planned slots that were never implemented —
there is nothing to port for them.

## Current main's OCR verify surface (what the archive branched FROM vs. now)

The archived branch forked from a ~741-line `OCRMode` with: no stable word identity,
no review session, a fake zoom label, no correction interaction, no lifecycle states.
Current main (U03/R07/R08 line) has: `OcrReviewedWord` stable-ID records,
`OcrReviewSession` (V05 stale-revision + G10 generation guard), `selectWord()` single
selection funnel, `nextUncertainWord()` wrap-around navigation (F8/Shift+F8 + buttons),
`OcrScanCanvas` (real source image + word boxes), `OcrWordMagnifier` (computed ×N.N),
`OcrConfidence` (THE one 90/70 classifier driving legend + overlay + counts + nav),
word inspector (correction + delete), and the full R07 lifecycle state machine.
Consequently most B-items are already covered by a *better* mechanism.

## Triage table

| B | What it did (archive) | Archive commit(s) | Current main | Verdict |
|---|---|---|---|---|
| B1 | Uncertain-word highlighting inside the *editable text pane* (ExtraSelections + toggle) | b7a5daee | Main deliberately demoted the text pane to "RECOGNIZED · PREVIEW" (R08/F04: edits there were never read back — corrections go through authoritative word records). Uncertain visibility lives in the scan-pane confidence bands + selection. | **OBSOLETE** — porting would re-introduce a dead-end edit surface that main's R08 doctrine removed on purpose. |
| B2 | Next/previous low-confidence word navigation | 0579826b | `nextUncertainWord()` wrap-around iterator, Next/Prev toolbar buttons, F8/Shift+F8 shortcuts, lifecycle-gated enable. | **COVERED-ALREADY** (superset: main's version is lifecycle-gated and stable-ID based). |
| B3 | "Verify Text" dialog shell (magnified crop + reason + Confirm/Skip) | 69d131d5 | No floating dialog; the guided flow exists on the main seam: F8 walk + word inspector (correction field = Confirm, next-uncertain = Skip) + magnifier crop. | **COVERED-ALREADY** (different mechanism, same capability; a separate dialog shell adds nothing the funnel lacks). |
| B4 | Dialog bulk actions: **Skip All / Replace All** (token-scoped, session-suppression list); **Re-recognize** button | b4dfe4ea, 893e5ba9 | Skip All / Replace All: **absent**. Re-recognize: covered by `reOcrRegionRequested` + context-menu "Re-OCR entire page". | **PORT** (Skip All / Replace All as session-scoped bulk dispositions on reviewed records); Re-recognize covered. |
| B5 | image↔text↔zoom word synchronization | d6233644 | `selectWord()` is THE one funnel (word links, canvas clicks, keyboard nav all land there); scan highlight + magnifier + inspector can never disagree. | **COVERED-ALREADY**. |
| B6 | Zoom pane magnified crop of the selected word + page-raster plumbing + Ctrl++/Ctrl+-/Ctrl+0 hotkeys | 55ea5d56, 8f21aac2, ecf7f4bd | Crop: `OcrWordMagnifier` over the real session image with a *computed* ×N.N header. Plumbing: `setReviewSession` carries the page image. Hotkeys: absent — but a user zoom multiplier needs a designed seam against U03's "computed magnification, never a static claim" honesty contract. | **COVERED-ALREADY** (crop + plumbing); hotkeys **DEFERRED** (see deferrals). |
| B7 | Status strip: page x OF y, language cell, **VERIFIED %** cell, zoom % cell | d38a6e08, d8a23cb5 | PAGE x OF y: from the review session. Language: toolbar combo, persisted, StatusBar reads the same key. Zoom %: computed ZOOM · ×N.N header (better than the archived static %). VERIFIED %: **absent** — main has no per-word verified concept. | **PORT** (verified %, folded with B12); page/language/zoom covered. |
| B8 | — | none (no commit, no doc reference on the archived branch) | — | **NO ARCHIVED COMMIT** — planned slot never implemented; nothing to port. |
| B9 | Ranked spelling suggestions (Damerau-Levenshtein ≤ 2, top-5) over dictionary + document vocabulary | b6753485 | **Absent.** | **PORT** (pure `suggestCorrections` seam + wired into the word inspector). |
| B10 | Per-language user dictionary (file-backed) + Add to Dictionary; dictionary words stop being flagged | 45f5d13b | **Absent.** | **PORT** (statics + uncertain-walk suppression + inspector action). |
| B11 | Reading-order badges + move-word-earlier/later reflow | 87162140, bac4a0d6 | Main's R08 contract: stable IDs, immutable source boxes, reviewed records zip 1:1 **in order** against original boxes at export. Reordering words breaks that order-based zip; needs an export-order redesign (carrying a permutation) first. | **DEFERRED** (conflicts with the stable-ID export contract; not portable without redesign — see deferrals). |
| B12 | Mark-page-as-verified state (Ctrl+T), resets on fresh results | ffc1777d | **Absent.** | **PORT** (with B7's verified %). |
| B13 | — | none (no commit, no doc reference on the archived branch) | — | **NO ARCHIVED COMMIT** — planned slot never implemented; nothing to port. |
| B14 | FineReader layout preset hotkeys (Ctrl+F5 zoom-pane collapse etc.) + Ctrl+Tab / Ctrl+Shift+Tab pane focus cycling | ddff7e2b, e5e245da | **Absent.** Focus cycling is pane-agnostic and ports cleanly (adapted to main's pane set). Layout presets need splitter-state plumbing main does not have (no stored splitter handle in OCRMode). | **PORT** (focus cycling); layout presets **DEFERRED** (see deferrals). |
| B15 | User-editable verification thresholds + spell-check / verify-low-conf option toggles | 7fd69cb8, f5b59e66 | Main: `OcrConfidence` is deliberately "THE one classifier" (fixed 90/70) — legend, overlay, low-count and navigation all read `bandFor()` so they can never disagree. User-editable thresholds would create a second source of truth. The spell-check toggle's useful core = dictionary flagging (B10 port). | **DEFERRED** (architectural conflict with the single-classifier doctrine — see deferrals); useful core covered by the B10 port. |

## Port units (this lane)

| Unit | B-item(s) | Shape on main | R7 pin |
|---|---|---|---|
| P1 | B10 | `OCRMode::userDictionaryPath/loadUserDictionary/addUserDictionaryWord` (static, file-backed) + per-language dictionary loaded with deliveries + dictionary words leave the uncertain walk + "Add to Dictionary" action in the word inspector | `TestOcrVerifyPort` dictionary tests |
| P2 | B9 | `OCRMode::suggestCorrections` (pure, Damerau-Levenshtein ≤ 2, ranked, top-5) + suggestions combo in the word inspector (activating applies the correction via `applyWordCorrection`) | `TestOcrVerifyPort` suggestion tests |
| P3 | B4 | `OCRMode::skipAllOccurrences(token)` (session suppression of the token in the uncertain walk) + `replaceAllOccurrences(token, replacement)` (bulk `applyWordCorrection`, returns count) | `TestOcrVerifyPort` bulk tests |
| P4 | B7+B12 | `OCRMode::markWordVerified(stableId)` / `verifiedPercent()` / `setPageVerified()` / `isPageVerified()` (reset on fresh deliveries) + VERIFIED % info-strip cell + Ctrl+T toggle | `TestOcrVerifyPort` verification tests |
| P5 | B14 | Ctrl+Tab / Ctrl+Shift+Tab focus cycling over main's pane set (page list, scan pane, text preview, word inspector) | `TestOcrVerifyPort` focus tests |

## Deferrals (owner-readable)

1. **B6 magnification hotkeys** — main's magnifier header shows the *computed* fit
   magnification ("ZOOM · ×N.N"); the U03 audit explicitly removed static multiplier
   claims. Hotkeys need a designed user-zoom-multiplier seam (userZoom × fit, header
   showing the product) so the honesty contract is preserved. Small, but it is UI
   polish on an already-covered capability — deferred rather than rushed.
2. **B11 reading-order reorder** — conflicts with R08's export contract (reviewed
   records zip 1:1 in delivery order against immutable source boxes). Reordering
   without an export-order redesign would either silently desync the text layer from
   the boxes or require inventing coordinates — both forbidden by the current
   doctrine. Needs: a documented permutation in the reviewed-record list + export
   change + fresh review; deferred as a design task, not a patch.
3. **B15 user-editable thresholds** — `OcrConfidence` is intentionally the single
   classifier (the pre-U03 bug was legend/highlight threshold drift). Parameterizing
   thresholds per-screen would reintroduce a second truth. If wanted later, the
   design is: threshold *fields* on `OcrConfidence` + one QSettings read at delivery
   + legend re-render — a deliberate architecture change, not a port.
4. **B14 layout preset hotkeys** — OCRMode on main does not own splitter geometry
   state; presets are low-value without it. Deferred.
5. **B3 dialog shell as such** — the guided walk exists via F8 + word inspector on
   the main seams; a second, floating verification surface would split the
   "one selection funnel" guarantee U03 bought. Covered; no port.

## Nothing-to-port

- **B8, B13** — no commit, no test, no doc reference anywhere on the archived branch.
  Planned slots never implemented.

## Port evidence (R7 — measured 2026-09-30)

Branch `feat/ocr-verify-port`, base 02d1a898 (origin/main; main had NOT moved at
finish — rebase-check performed, merge-base == origin/main == base → FF-ready).

| Unit | Commit | Archive source | Pin RED on main | ×3 serial pass |
|---|---|---|---|---|
| P1 B10 dictionary | 4088682b | 45f5d13b | compile error: `'userDictionaryPath' is not a member of 'gp::OCRMode'` (+9 more: API absent) | 3/3 `100% tests passed` |
| P2 B9 suggestions | c7292374 | b6753485 | compile error: `'suggestCorrections' is not a member of 'gp::OCRMode'` (+4: API absent) | 3/3 `100% tests passed` |
| P3 B4 Skip/Replace All | bb513050 | b4dfe4ea | compile error: `'class gp::OCRMode' has no member named 'skipAllOccurrences'` (+3: API absent) | 3/3 `100% tests passed` |
| P4 B7+B12 verified state | ca220d4a | d38a6e08 + ffc1777d | compile error: `'class gp::OCRMode' has no member named 'markWordVerified'` (+5: API absent) | 3/3 `100% tests passed` |
| P5 B14 focus cycling | 98455870 | e5e245da | compile error: `'class gp::OCRMode' has no member named 'cyclePaneFocus'` (+3: API absent) | 3/3 `100% tests passed` |

Pin tests: `tests/TestOcrVerifyPort.cpp` (12 test functions), registered as the
`TestOcrVerifyPort` ctest target. Each pin was built and run against unported
main code in the same worktree (base 02d1a898 = main) BEFORE its unit commit:
the fail-before is a compile failure of the pin itself — the strongest possible
evidence the capability is absent on main. Each unit then landed as one new
commit (test + implementation together), and `ctest -R TestOcrVerifyPort` passed
3 consecutive serial runs afterwards. No test on main was weakened or removed;
`TestOcrVerifyNavigation` (U03 pins) and `TestOcrReviewLifecycle` (R07/R08 pins)
were re-run green after every unit.

Full gate (serial, build-port Release, all targets): the suite has 190 tests;
`R14ProbeBatchSkip` (#184) is disabled by design → 189 active, the same gate size
the endgame recorded at the base. Five full serial runs at the branch tip:

| Run | Result | Failing set |
|---|---|---|
| 1 | 187/189 | TestWelcomeRoutes, TestSweepW3UxFlows — both **documented known flakes** (BRANCH-LANDSCAPE-2026-09-25: "known flake (solo green)" / "known parallel flake"); both green on immediate solo rerun |
| 2 | 187/189 | same two, same disposition |
| 3 | 186/189 | the two above + a third documented flake — varying membership run-to-run is the flake signature |
| 4 | **189/189 — 100%** | clean |
| 5 | 188/189 | TestLaneScheduler only — the "documented transient" of CLEANUP-LEDGER-2026-09-09; green on solo rerun |

`TestOcrVerifyPort` (the port pins) passed in **every** full run and in every
per-unit ×3 block. The flaky set never contains an OCR-verify test; all three
flakes pre-date this lane, are documented on main, and rerun green solo
(--rerun-failed: 3/3 Passed).

## FF-ready

`git merge-base HEAD origin/main` == origin/main == 02d1a898 == this branch's
base: the branch is a strict 6-commit fast-forward of main, no divergence.
Rebase-check at finish: main had not moved (verified via `git fetch origin`).



## ADDENDUM — rebase onto fresh main (2026-09-30, post-evidence)

During the lane's final check origin/main advanced 02d1a898 → 181247b2
(4 commits: forms /FT /Sig field, signing OCSP consent gate, MCID reading
order ×2 — no OCR-verify overlap). Per lane instructions the branch was
REBASED onto 181247b2 (zero conflicts; single-writer discipline kept —
rebase, not merge).

Post-rebase unit SHAs (the pre-rebase SHAs in the table above are the same
content under old parentage):

| Unit | Post-rebase SHA |
|---|---|
| P1 B10 dictionary | b0451e84 |
| P2 B9 suggestions | 9098a7c2 |
| P3 B4 Skip/Replace All | e77a2fd7 |
| P4 B7+B12 verified state | 2762cf5e |
| P5 B14 focus cycling | decf43c5 |
| triage | 7d422a0b |
| evidence | d4bd448d (+ this addendum) |

Re-verification at the new base:
- rebuild all 526 targets: clean;
- TestOcrVerifyPort + TestOcrVerifyNavigation + TestOcrReviewLifecycle
  ×3 serial: 3/3 × `100% tests passed`;
- full serial gate: **100% tests passed, 0 failed out of 191** (main's new
  commits added one test) — a fully clean gate, no flake involvement.

## FF-ready (final)

`git merge-base HEAD origin/main` == origin/main == **181247b2**; the branch
tip is a strict fast-forward of origin/main.
