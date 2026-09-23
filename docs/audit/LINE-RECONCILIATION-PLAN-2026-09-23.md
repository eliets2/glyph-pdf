# LINE-RECONCILIATION-PLAN-2026-09-23

Status: PLAN (analysis complete; no merges executed). Author: line-reconciliation analyst, 2026-09-23.
Method: patch-id (`git cherry`) both directions, tree diffs, `git merge-tree --write-tree` pairwise forecasts, and a real chained scratch-merge dry run in a throwaway detached-HEAD worktree (removed after; no refs touched). All scratch data: `.context/line-recon/` (gitignored), handoff `.context/line-recon-wip.md`.

## 0. TL;DR

- **Base = feat/parity-glm (ef371ad0)** — the verified September line. Merge **consolidate/all (95dccb23)** first (1 conflict), then **review/consolidated-parity (f4750af5)** (28 conflicts, validated by dry run; it carries main). **main (2b715f47)** then folds with **zero** conflicts (it is fully contained in consolidated-parity — merge-base = main's own tip). Total reconciliation surface: **29 conflicted files**, of which ~10 need real semantic work.
- The feared hot-spots mostly dissolve: **find&replace is already one implementation** (trees byte-identical across the two lines), consolidate/all is 99.5% inside parity-glm, and main's content is 401/428 patch-equivalent + 14 real commits that ride in via consolidated-parity.
- The genuinely duplicated work is the **July-era Wave 1A/1B/2B branches** — superseded duplicates of September items (per consolidated-parity's own consolidation ledger, commit 1991d9c1); their unique features were already re-landed as follow-up commits (Night Mode, image restack/opacity/rotate, letter/line spacing+opacity, TestOfficeExport). **Do not merge them** (35–39 conflicts each for no content); archive + ledger instead.
- Zero-loss is provable: real merges ⇒ post-reconciliation `git rev-list --count <line>..consolidated == 0` for all four folded lines; July branches stay open (nothing deleted) with an archive bundle refresh as the safety net.

## 1. Verified inventory (SHAs, counts, what each line holds)

Divergence point: **1669f70d** (2026-06-02, the jbig2enc-vendor / pre-purge base).

| Line | Tip | Commits since 1669f70d | Patch-unique vs the others |
|---|---|---|---|
| feat/parity-glm | **ef371ad0** | 1031 | 506 non-merge commits not patch-equivalent anywhere in consolidated-parity (the whole Sept program's post-snapshot evolution); 30 vs consolidate/all (10 patch-unique: G1–G6 redaction fixes, R14 probe-scoping, SF-1/SF-2 soak follow-ups) |
| review/consolidated-parity | **f4750af5** | 487 (439 not in pg) | only **23 patch-unique** commits — 401 are patch-equivalent (main's content), 15 are its consolidation merges |
| consolidate/all | **95dccb23** | 1025 | 24 not in pg (19 are its branch merges); **5 patch-unique** (soak-start doc 1d2e76b8, shared-seams refactor 0c1aeabd, policy-seam pins 288c2815, quality-new fixes-log 673129e1, soak-verdict FAIL 790a7197) |
| main | **2b715f47** | 478 | **14 real commits — prior analysis CONFIRMED** (14 uniq vs pg and vs ca; **0 vs cp**: main is an ancestor of consolidated-parity) |

**Structure of consolidated-parity** (its own history + commit 1991d9c1's message):
`main` + **one squash** 1991d9c1 ("consolidate: land 20 parity/hardening branch lines onto main (squashed, purge kept)", 687 files, +134,701 — consolidated all 71 origin `feat/*` branches via 23 leaves, with documented conflict resolutions) + **8 follow-ups**: the four July ports (Night Mode 6020ea8b incl. Eye Care use-after-free fix; image restack/opacity/rotate "make image matrix edits work" e552df26; letter/line spacing + opacity 3d8bca5d; TestOfficeExport port e6497dbe), checked-redo double-apply fix ab82e81d, test-infra per-test temp root 98b22b46 (CMakeLists-level), and two docs audits (ec3b0e63 `SECURITY-QUALITY-REVIEW-parity-glm.md` + `PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` edits; f4750af5 defects record).

**What each line holds that the others lack:**
- **parity-glm**: the authoritative, verified September program as real history — G-gates, form-JS P1+P2, redaction proof + G1–G6 + R14 (Sept 23), W1/W2/W3 sweeps, ux-integration/modularity/dispatch/resoak/consolidation-stage1 lanes, soak follow-ups SF-1/SF-2, `CONSOLIDATED-REPORT-2026-09-20.md` and friends. Lacks: main's 14 commits, the 4 July ports, the redo fix, the CMake temp-root infra, cp's 2 audit docs, the quality-new seams/pins.
- **consolidated-parity**: main's 14 (v1.0.0 launch-gate/MSI/winget 5ca7e36e, v1.4.0 version+packaging 1abf3008/2e51b2c4, M4-PROMPT-5 security tools 06cb0bd0, OSS governance a4466674, memory-doc commits, WP-0/audit-post fixes, Sept-13 audit 2b715f47), the CLAUDE.md/SECURITY.md purge, the four July ports, the redo fix, the temp-root test infra, its own security & quality review docs.
- **consolidate/all**: the 5 commits above (quality-new shared-seam refactor `VersionedJson::atomicWrite`, PoFoDictRead seams, policy-path seam pins + hermetic TestSupportBundle, soak verdict FAIL doc) — shared with cp (which got the same content via the squash), absent from pg.
- **July-era branches** (patch-unique vs BOTH lines): feature/editing-parity **35**, feature/viewing-parity **35**, feature/redaction-parity **32**, feature/security-parity **31**, feature-elevation-wave1a = feature/ocr-parity **30**, feature/accessibility-parity **0** (fully absorbed). Mission's "30–35 uniq" confirmed. Per cp's consolidation ledger these are the older Wave 1A/1B/2B implementations of the *same* audit items; their unique work (Night Mode, image z-order+opacity, letter/line spacing, view-only rotation, custom rotate angle, TestOfficeExport, TestEditingWave1B) was ported; the rest is superseded.

**Duplicate-vs-disjoint verdicts (the "implemented twice?" question):**
| Surface | Verdict |
|---|---|
| find&replace (WP-R07 vs "regex port") | **Same implementation, one copy.** `FindReplaceDialog.cpp/.h` + `TestFindReplace.cpp` are byte-identical in both tips. Not a conflict surface. |
| Night Mode | **Disjoint.** pg has no `NightModeEffect.h`; pg already has Eye Care (ToolId/RibbonModel/ViewController/PdfViewerWidget). cp's port is a new feature + a UAF fix in pg's existing Eye Care. Port cleanly. |
| letter/line spacing + opacity | **Disjoint addition** to pg's inline-text editing (new `ContentSpans.*`, style attrs on `EditTextInlineCommand`). Port. |
| image restack/opacity/rotate | **Overlapping duplicate risk.** Both lines changed `PoDoFoBackend`/`IPdfEditorEngine`/`EditController` for image edits; cp's commit claims to "make image matrix edits work". Owner decision R4-2 below. |
| redaction proof | **Same feature, two evolutions.** pg's is newer (G1–G6, R14) and verified; cp's copy ≈ the squash snapshot + a small delta (6+/32− vs the pg snapshot). pg survives; audit cp's delta for unique bits. |
| test temp-root infra | **Two reforms.** cp's is one CMakeLists-level change (98b22b46); pg reformed tests individually. Union at CMakeLists; rerun everything under it. |
| checked-redo fix | cp-only bugfix (command headers). Port. |
| quality-new seams/pins | Present in ca AND cp, absent in pg — arrives via merges, already co-resolved (the ca merge pre-resolves 5 conflicts of the cp merge). |

## 2. Conflict forecast (validated)

Pairwise `git merge-tree --write-tree` + a real chained scratch-merge (detached HEAD, throwaway worktree, removed):

| Step | Merge | Conflicted files | Notes |
|---|---|---|---|
| M1 | consolidate/all → pg | **1** (`src/shell/EditPolicy.h`, content) | doc-comment only; resolution = pg's route-list (superset). Matches the resolution 1991d9c1 already documented. |
| M2 | consolidated-parity → (pg+ca) | **28** (8 add/add) | dry-run-verified; 5 files pre-resolved by M1 (BatchPreset.cpp, AccessibilityChecker.cpp, AccessibilityFixes.cpp, TestPolicyController.cpp, TestSupportBundle.cpp — the quality-new seams both sides agree on). Pairwise cp→pg is 33; chaining saves 5. |
| M3 | main → (pg+ca+cp) | **0** | main ⊂ cp (merge-base = 2b715f47). Avoids the 144-file/44-add-add pairwise mess entirely — order is everything. |
| — | July branches → pg | 35–39 each | **not merged** (superseded duplicates); forecast only. |

**Total: 29 unique conflicted paths.** The 28 of M2:

```
CMakeLists.txt                        packaging/GlyphPDF.wxs
src/commands/CropPageCommand.cpp      src/commands/DeleteImageCommand.h
src/commands/EditFormFieldCommand.h   src/commands/EditTextInlineCommand.h
src/commands/ReplaceImageCommand.h    src/core/RedactionProof.cpp (add/add)
src/core/ToolId.cpp                   src/core/ToolId.h
src/core/interfaces/IPdfEditorEngine.h
src/engines/PdfEditorEngine.cpp       src/engines/PdfEditorEngine.h
src/engines/RedactOperation.cpp (add/add)
src/engines/podofo/PoDoFoBackend.cpp  src/engines/podofo/PoDoFoBackend.h
src/shell/EditPolicy.h (add/add)      src/shell/MenuBar.cpp
src/shell/controllers/EditController.cpp  src/shell/controllers/EditController.h
src/shell/controllers/ViewController.cpp
tests/R14ProbeRedactSpace.cpp (add/add)   tests/TestCheckedMutationCoverage.cpp (add/add)
tests/TestExcisionCorruption.cpp (add/add)  tests/TestRedactTransaction.cpp (add/add)
tests/TestRedactionProof.cpp (add/add)  tests/TestSanitization.cpp (add/add)
tests/mocks/MockPdfEditorEngine.h
```

Tree-delta sizes for the big ones: `PoDoFoBackend.cpp` 669 lines pg↔cp (cp vs pg-snapshot: 132+/277−); `CMakeLists.txt` 154; `IPdfEditorEngine.h` 24; `EditController.cpp` 62. Clean (auto-merged) parts of the cp merge: all cp-only new files (NightModeEffect.h, ContentSpans.*, ImageAppearanceCommand.h, TestImageAppearance, TestTextEditStyle, TestViewingModes, TestOfficeExport) and the docs.

## 3. The plan

### 3.1 Base and order (recommended)

**Base = feat/parity-glm (ef371ad0)**, on branch `feat/line-reconciliation` (this document is its first commit):
1. **M1 — merge 95dccb23 (consolidate/all)**. 1 conflict, EditPolicy.h doc-comment (take pg's superset). Lands quality-new seams/pins + soak-verdict docs with zero risk.
2. **M2 — merge f4750af5 (review/consolidated-parity)**. 28 conflicts per §2. Brings main + the four July ports + redo fix + temp-root infra + review docs + the purge.
3. **M3 — merge main (2b715f47)**. Proven no-op conflict-wise (contained); run it for the record so main's ancestry is explicit.
4. **M4 — July-era branches: do NOT merge.** Archive + ledger (§3.5). They stay open until owner sign-off (nothing deleted).
5. **M5 — verification protocol** (§3.4), zero-loss proofs (§3.5), then fold: **main fast-forwards (or --no-ff merges) to the consolidated tip** — zero conflicts by containment.

Why pg as base: it is the program owner's verified line (every Sept wave pinned in docs/audit/CONSOLIDATED-REPORT-2026-09-20.md), it has full real history (cp's July content exists only as a squash + follow-ups, ca is pg + 5), and both other lines merge INTO it with the smallest total conflict surface (29 files) — the reverse order (cp as base) would re-litigate 506 commits' worth of content and rebuild pg's merge history as squashes. Why ca before cp: it is 99.5% pg, its single conflict pre-resolves 5 of cp's 28, and it keeps the cp merge the only "real" resolution pass.

### 3.2 Policy classes for the 28+1 conflicts

- **R1 — take-parity-glm** (cp's copy is the squash's snapshot echo of pg's own older state; pg's tip is newer and verified; zero loss — proven by "cp has no post-snapshot novelty in this file"): `RedactOperation.cpp`, `R14ProbeRedactSpace.cpp`, `TestExcisionCorruption.cpp`, `TestRedactTransaction.cpp`, `TestSanitization.cpp`, and the bulk of `RedactionProof.cpp`/`TestRedactionProof.cpp` (still audit cp's 6+/32− delta first).
- **R2 — union** (both sides carry post-snapshot deltas; ports + fixes land on pg's newer code): `PoDoFoBackend.cpp/.h`, `IPdfEditorEngine.h`, `PdfEditorEngine.cpp/.h`, `EditController.cpp/.h`, `EditToolBar.*`, `PdfViewerWidget.*`, `ToolId.cpp/.h` (enum entries: pg's + Night Mode's), `MenuBar.cpp`, `ViewController.cpp`, `EditPolicy.h` (route-list union, pg superset + cp delta), `EditTextInlineCommand.h` + the four command headers carrying the redo fix (`CropPageCommand.cpp`, `DeleteImageCommand.h`, `EditFormFieldCommand.h`, `ReplaceImageCommand.h`), `TestCheckedMutationCoverage.cpp`, `CMakeLists.txt` (test registrations BOTH ways + cp's per-test temp-root runner + pg's new tests), `GlyphPDF.wxs` (keep v1.4.0 ProductCode + INF03 single-source VERSION), `MockPdfEditorEngine.h`.
- **R3 — take-consolidated-parity/main-carried**: `CLAUDE.md` + `SECURITY.md` purge (owner sign-off, R4-1), `release-notes-v1.4.0.md`, packaging version state.
- **R4 — owner decision** (§3.6): image-edit-path survivor; July-branch disposition.

Rule of thumb applied per hunk: pg side wins unless the hunk is attributable to one of cp's 8 follow-up commits or main's 14 (`git log --follow` on the cp side hunk; the 8+14 commit lists are closed sets — every cp delta must trace to one of them or it is snapshot echo ⇒ R1).

### 3.3 Per-surface keep-vs-port (with evidence)

| Surface | Decision | Evidence |
|---|---|---|
| find&replace | nothing to do | trees identical (`git diff ef371ad0 f4750af5 -- src/ui/FindReplaceDialog.* tests/TestFindReplace.cpp` = empty) |
| redaction proof + G1–G6 + R14 | keep pg (R1) | pg tip has Sept-23 G1–G6 commits d07b0645/d129324f/b4fe2a5f/c2776ec4/6c785eba/33a70818 + 198e3bf2; cp copy diverges from pg-snapshot by only 6+/32− |
| Night Mode + Eye Care UAF fix | port cp (R2) | pg lacks NightModeEffect.h; disjoint feature; ports = 6020ea8b |
| image restack/opacity/rotate | port cp, pending R4-2 | e552df26; overlaps pg's image-edit evolution in PoDoFoBackend — tests arbitrate |
| letter/line spacing + opacity | port cp (R2) | 3d8bca5d; disjoint addition, new ContentSpans files |
| TestOfficeExport | port cp (R2/CMake) | e6497dbe; tests+CMake only |
| checked-redo fix | port cp (R2) | ab82e81d; pure bugfix on command headers |
| temp-root test infra | port cp (R2/CMake) | 98b22b46; single CMakeLists change; rerun full suite under it |
| quality-new seams + policy pins | arrive via M1 (ca) | 0c1aeabd, 288c2815 — pinned by TestBatchPresets 14/14, TestAccessibilityChecker 10/10, TestAccessibilityFixes 9/9 |
| main's 14 (v1.4.0, M4-PROMPT-5, governance, memory docs, Sept-13 audit) | ride in via M2 (R3) | main ⊂ cp; patch-unique lists archived |
| cp's review docs | keep (docs) | ec3b0e63, f4750af5 — both land clean |
| CLAUDE.md / SECURITY.md | purge (R3, owner R4-1) | cp's consolidation deliberately purge-kept; pg's copies are stale |

### 3.4 Verification protocol (both suites or it doesn't count)

Every reconciled surface must pass **the survivor's suite AND the ported feature's suite**; a surface where one side's suite is consciously retired needs a written waiver in the merge commit message. Full gate: configure + build all + `ctest` green under cp's per-test temp-root infra.

| Surface | Survivor suite | Ported suite |
|---|---|---|
| redaction | TestRedactionProof, TestRedactTransaction, TestSanitization, TestExcisionCorruption, R14ProbeRedactSpace (+ REDACTION-RESEARCH-2026-09-21 §2 pins, G1–G6 blocks) | cp-side redaction tests that survive R1 audit (TestRedactionProof deltas) |
| Night Mode / viewing | existing viewing/controller suite | TestViewingModes (new) + TestControllers + Eye Care UAF regression |
| image edits | pg's image suite + TestCheckedMutationCoverage | TestImageAppearance (new, 484 lines) |
| text style | pg's inline-text suite + TestCheckedMutationCoverage | TestTextEditStyle (new) |
| office export | convert suite | TestOfficeExport (new) |
| history/redo | TestCheckedMutationCoverage (union of both sides' cases) | — (same file) |
| test infra | full ctest under the CMake temp-root change | — |
| quality-new seams | TestBatchPresets 14/14, TestAccessibilityChecker 10/10, TestAccessibilityFixes 9/9, TestSupportBundle hermetic, TestPolicyController/Wiring, TestSendForSigning RUN_SERIAL | — |
| find&replace | TestFindReplace (single shared implementation) | — |
| main-carried (packaging) | MSI/portable build smoke with v1.4.0 ProductCode + INF03 VERSION single-source | governance files + release notes present |

### 3.5 Zero-loss proofs (run after M3, before any cleanup)

1. **Ancestry emptiness per folded line** (real merges make this exact): `git rev-list --count ef371ad0..HEAD == 0`, `git rev-list --count 95dccb23..HEAD == 0`, `git rev-list --count f4750af5..HEAD == 0`, `git rev-list --count 2b715f47..HEAD == 0` (all against the consolidated tip).
2. **Patch-id audit**: `git cherry <consolidated> <line>` shows 0 `+` non-merge commits for each of the four lines.
3. **Tree spot-checks** on the 59-file pg↔cp surface: every cp-only new file present (`NightModeEffect.h`, `ContentSpans.*`, `ImageAppearanceCommand.h`, `TestImageAppearance.cpp`, `TestTextEditStyle.cpp`, `TestViewingModes.cpp`, `TestOfficeExport.cpp`), every pg-only file intact (`TestRedactTransaction.cpp` full 447 lines, `SOAK-FOLLOWUP-2026-09-23.md`).
4. **main**: containment pre-proven (merge-base = main tip); post-merge `git merge-base --is-ancestor main HEAD`.
5. **July branches (not folded)**: (a) unique-content ledger — the per-branch patch-unique commit lists are archived in `.context/line-recon/july-*.set` and summarized here; (b) their ported unique work is pinned by the follow-up commits' suites (§3.4); (c) **archive bundle refresh** covering ALL refs (branches + the four local `archive/*` tags + this line) before any future cleanup: `git bundle create <archive>.bundle --all`. Branches stay until the owner signs off; only then may deletion be considered (explicitly out of scope today).

### 3.6 Effort + risk table

Forecast: **29 conflicted paths; ~15 mechanical (R1/R3 + header/enum unions), ~8 semantic unions, 1 doc-comment**. Estimate: 3–5 working days resolution (PoDoFoBackend alone ~1–2), +1–2 days full verification, +0.5 day proofs/docs. Risk: medium — worst file is well-understood (1991d9c1 already had to semantically merge it once).

**Top-10 hardest resolutions:**
1. `src/engines/podofo/PoDoFoBackend.cpp` — 669-line delta; image+text-spans ports onto pg's newer backend; the squash needed a semantic merge here too.
2. `src/core/interfaces/IPdfEditorEngine.h` — API union: image-appearance/style methods vs pg's engine additions; MockPdfEditorEngine.h must follow.
3. `src/engines/PdfEditorEngine.cpp/.h` — same union, implementation side.
4. `src/shell/controllers/EditController.cpp/.h` — controller wiring for the ports vs pg's controller evolution.
5. `CMakeLists.txt` — test registrations BOTH ways + cp's temp-root runner + pg's new tests; mis-registration silently drops suites (verification gate catches it).
6. `src/core/RedactionProof.cpp` (add/add) — keep pg's G1–G6 state; audit cp's 6+/32− delta for unique bits before discarding.
7. `src/ui/PdfViewerWidget.cpp/.h` — Night Mode hooks + Eye Care UAF fix onto pg's newer viewer code.
8. `src/core/ToolId.cpp/.h` + `EditPolicy.h` — enum-entry + route-list unions (EditPolicy's resolution already documented by 1991d9c1).
9. `tests/TestRedactTransaction.cpp` / `TestSanitization.cpp` (R1) — take pg, but re-check under cp's temp-root infra (CMake semantic interaction).
10. `packaging/GlyphPDF.wxs` + CMake VERSION — reconcile v1.4.0 ProductCode/INF03 single-source with pg-side state.

**Needs the program owner's decision:**
- **R4-1 Purge adoption**: CLAUDE.md/SECURITY.md — adopt consolidated-parity's purge (recommended; 71-branch consolidation was deliberately purge-kept) or keep pg's stale copies.
- **R4-2 Image-edit survivor**: cp's "make image matrix edits work" (e552df26) vs pg's own image-edit evolution — if both work, which implementation survives (suites arbitrate; loser retired with waiver).
- **R4-3 July-branch disposition**: confirm do-not-merge/superseded (evidence: 35–39 conflicts each; Wave 1A/1B/2B duplicates; unique work already ported) + archive-bundle-only, with deletion deferred until sign-off.

## 4. Residuals / notes

- `origin/feat/parity-glm` (26c9a415) is 106 commits behind local pg; everything after it exists in cp only as squash content (patch-ids don't match a squash — that is why cherry shows 506 "uniques" spanning all of September; tree-level the real pg↔cp surface is just 59 files). After reconciliation, origin needs a push of the consolidated line (owner action; never done by this analysis).
- pg↔ca tree delta is 27 files; the M1 merge auto-resolves all but EditPolicy.h.
- The scratch dry run created dangling commits on a detached HEAD (no refs); worktree removed; nothing deleted, no gc run.
- Raw evidence: `.context/line-recon/` (commit lists, cherry outputs, merge-tree outputs, chained conflict list, July unique sets); handoff: `.context/line-recon-wip.md`.
