# LINE-RECONCILIATION-EXECUTION-2026-09-23

Status: COMPLETE (merges, proofs, build, suites, dispositions). Author: line-reconciliation executor, 2026-09-23.
Plan: `docs/audit/LINE-RECONCILIATION-PLAN-2026-09-23.md` (on feat/line-reconciliation 3811cc6a, cherry-picked onto feat/consolidated as f1163f0a). This document records the execution of that plan. Handoff: `.context/reconexec-wip.md`.

## 0. TL;DR

`feat/consolidated` = feat/parity-glm (ef371ad0) + plan doc + **M1 merge consolidate/all (b7a7d6b2)** + **M2 merge review/consolidated-parity (e11aa083)**; **M3 merge main = "Already up to date"** (merge-base(HEAD, main) = main's own tip — main was already folded via consolidated-parity, exactly as forecast). All 29 forecast conflicts resolved (1 in M1, 28 in M2, both counts matching the plan's dry run exactly). **Zero loss proven for all four folded lines**: `feat/consolidated..<line>` = 0 commits for ef371ad0, 95dccb23, f4750af5, 2b715f47; all four are ancestors; `git cherry` shows 0 patch-unique non-merge commits on each line. Owner decisions R4-1/R4-2 executed per the plan's documented recommendations (recorded in §4, reversible); R4-3 recorded, nothing merged, nothing deleted. July-era branches remain OPEN pending owner sign-off (Stage 2: deletion pass + archive-bundle refresh — NOT executed, consent-gated).

## 1. Branch construction

| Step | Result |
|---|---|
| Base | `git checkout -B feat/consolidated feat/parity-glm` at **ef371ad0** (verified September line) |
| Plan doc | cherry-pick 3811cc6a → **f1163f0a** (docs/audit/LINE-RECONCILIATION-PLAN-2026-09-23.md rides on the branch) |
| Pre-flight | worktree pdf-keyA was on feat/redaction-gaps with probe-dirty state (M PoDoFoBackend.cpp = 22-line G3-block probe deletion + untracked ad_diag/rma_diag/tests/W2Probe{Certs,Forms,RedactProof}.cpp). Preserved on **reconexec/snapshot-redaction-gaps-dirty @ 2adc3df6**; feat/redaction-gaps itself untouched. |
| Build pre-flight | build-ra (Debug, Ninja, D:/pdf/msys64 UCRT64 toolchain, configured against this worktree) with pdfium.dll + vendored libpodofo.dll present; D: 65–66G free (> 40G guard) |

## 2. M1 — merge consolidate/all (95dccb23) → commit b7a7d6b2

Conflicts: **1 of 1 forecast**.

| File | Resolution |
|---|---|
| src/shell/EditPolicy.h (content) | Doc-comment only. Took pg's route-list text (union superset: emergence E-1 route named). Same choice 1991d9c1 documented in consolidated-parity. Note: pg's tip carries a trailing mid-sentence comment fragment (dangling "PagesMode reorder/labels, … emergence E-1) call mutationBlocked()…" duplicate); it is preserved byte-faithfully in M1 and only cleaned in M2 (see below). |

Landed: ca's 5 patch-unique commits (soak-start 1d2e76b8, shared-seams 0c1aeabd, policy-seam pins 288c2815, quality-new fixes-log 673129e1, soak-verdict FAIL 790a7197).

## 3. M2 — merge review/consolidated-parity (f4750af5) → commit e11aa083

Conflicts: **28 of 28 forecast** (8 add/add). Per-file resolutions (policy classes per plan §3.2):

### R1 — take-parity-glm (cp's copy is the squash's snapshot echo; cp-side deltas audited before discard)

| File | Audit of cp's delta vs pg | Verdict |
|---|---|---|
| src/core/RedactionProof.cpp (add/add) | cp-only lines: old pre-G6 `if (!contents) return m;`, raw-rect block (superseded by G5 effectiveAnnotRectUser), SHORT disclaimer (pg's adds XFA/OCG/widget-value disclosure). Zero cp-only novelty. | take pg |
| src/engines/RedactOperation.cpp (add/add) | 0 cp-only lines | take pg |
| tests/R14ProbeRedactSpace.cpp (add/add) | cp-only: 4 lines of older/weaker honest-failure logging; pg's richer content/annot-entry assertions | take pg |
| tests/TestExcisionCorruption.cpp (add/add) | cp-only: 2 lines old helper formatting; pg adds createPatternPdf positive control | take pg |
| tests/TestRedactTransaction.cpp (add/add) | 0 cp-only lines | take pg |
| tests/TestSanitization.cpp | cp-only: 5 lines incl. "Vector 4: /OCProperties removed (G-04)" — pg DELIBERATELY superseded this: /OCProperties removal revealed hidden layers; pg's fixture builds a real OCG layer and sanitize keeps hidden layers hidden (all-OFF policy). | take pg (documented supersession) |
| tests/TestRedactionProof.cpp (add/add) | cp-only: 1 line (old save call) | take pg |

### R2 — union (both sides' post-snapshot deltas survive)

- **src/shell/EditPolicy.h (add/add)** — cp's integrated route-list wording adopted: it names the field-properties panel + emergence E-1 routes (pg's content, properly integrated into the sentence), drops pg's dangling fragment (merge debris whose text is present in the integrated sentence), and adds cp's clause "and EditController's image action menu: rotate, restack, opacity, replace, delete" (doc for the ported image routes, which call EditPolicy::mutationBlocked — see EditController).
- **src/core/ToolId.h / ToolId.cpp** — NightMode enum entry + `"nightMode"` string + aliases unioned onto pg's list.
- **src/core/interfaces/IPdfEditorEngine.h** — cp's editTextInline 12-arg signature (+opacity/letterSpacing/lineSpacing doc), setImageZOrder, setImageOpacity; pg's hasXfaDocument (G1) KEPT (cp's copy predates it).
- **tests/mocks/MockPdfEditorEngine.h** — follows the interface: cp's image stubs; pg's hasXfaDocument/m_hasXfaDocument kept; cp's 12-arg editTextInline stub.
- **src/engines/PdfEditorEngine.h/.cpp** — cp's editTextInline pass-through + setImageZOrder/setImageOpacity impls; pg's G2 abort-reason surfacing (lastRedactionAbortReason) and hasXfaDocument impl kept. (Method note: an initial `checkout --ours` would have silently dropped cp's auto-merged additions; redone via `checkout -m` + per-hunk edits — verified both sides' content present, brace balance 0.)
- **src/engines/podofo/PoDoFoBackend.h** — 1 hunk: pg's G1/G2 declarations kept; cp's editTextInline params + image methods auto-merged.
- **src/engines/podofo/PoDoFoBackend.cpp** — 19 hunks (pre-resolution snapshot: `.context/reconexec-PoDoFoBackend.cpp.m2-conflicted`):
  - h1: include UNION — pg's core/RedactionProof.h + cp's ContentSpans.h both included.
  - h2, h3, h5–h12: take pg — G1 (XFA preflight + sanitize scrub), G2 (redactionAbortReason member, lastRedactionAbortReason, named aborts + tiling-pattern-text guard in exciseContentRegions), G3 (widget /V//DV /Parent-chain clear), G4 (OCProperties keep-hidden all-OFF policy), G5 (effectiveAnnotRectUser + effective-coverage removal test), F-05 audit-log opt-in tail.
  - h13: take cp — rewriteImageMatrix reimplemented via gp::content::replaceImageMatrix (R4-2, below; helpers pageContentBytes/setPageContentBytes/describeRefusal auto-merged, present).
  - h14: take cp — listImages PdfVariantStack operand-order FIX (stack[5]=a; pg's forward read reported non-symmetric placements wrong). pg's own stack[5] uses live in the redaction canvas walk (different function) — pg knew the convention there but the image read path was unfixed.
  - h15–h18: take cp — `catch (const std::exception&)` in moveImage/resizeImage/rotateImage/replaceImage: PdfError is `final : public std::exception` (verified in vendored podofo PdfError.h), so the catch is strictly broader and also covers commitMutation refusal throws (cp's transactional image path).
  - h19: take cp — deleteImage tail catch + the whole setImageZOrder/setImageOpacity implementation section (gp::content byte-exact edits).
- **src/shell/controllers/EditController.h/.cpp** — 3 hunks take cp (verified superset): image action menu expansion (Rotate 180°, Rotate by Angle…, Bring to Front, Send to Back, Opacity…), ARC07 EditPolicy::mutationBlocked gate at menu top, ImageAppearanceCommand dispatch with page backup (kMaxPageBackupBytes contract); onTextStyleChanged slot + _textOpacity/_letterSpacing/_lineSpacing + textStyleChanged connect auto-merged.
- **src/commands/{CropPageCommand.cpp, DeleteImageCommand.h, EditFormFieldCommand.h, ReplaceImageCommand.h}** — take cp: consumeArmedApply() early-return (checked-redo applies exactly once, ab82e81d); body otherwise identical to pg's.
- **src/commands/EditTextInlineCommand.h** — 2 hunks take cp: 12-arg engine call + style-attr members; constructor changes auto-merged.
- **src/shell/MenuBar.cpp, src/shell/controllers/ViewController.cpp** — take cp (pg side empty in all hunks): nightMode registry/menu entries + ViewController NightMode case (toggleNightMode/isNightMode — PdfViewerWidget.h auto-merged with the API).
- **tests/TestCheckedMutationCoverage.cpp (add/add)** — take cp: verified PURELY ADDITIVE over pg's copy (+82/−1; the −1 is the FaultEngine editTextInline stub adapted to the 12-arg interface). pg's cases all present.
- **CMakeLists.txt** — 5 hunks, all take cp, verified union-complete: (1) project VERSION 1.4.0; (2) ContentSpans.h/.cpp sources; (3) TestViewingModes registration; (4) TestImageAppearance/TestOfficeExport/TestTextEditStyle registrations — hunk verified by direct containment check: pg's side byte-contained in cp's side, ZERO deletions (TestPersistenceOutcomes, render_path_profile, TestRenderGuards, TestSep13LeadCapability blocks identical both sides); (5) per-test temp-root runner (98b22b46). Post-merge registration census: **HEAD 188 add_executable = pg's 184 (zero pg-only names dropped, proven by comm set-difference) + cp's 4 new**.
- **packaging/GlyphPDF.wxs** — take cp (R3): v1.4.0 ProductCode 9B76D748-BA88-4AE3-83F8-22D396F2433E with pinned-per-release history comment.

### R3 — take-consolidated-parity/main-carried

- VERSION 1.4.0, GlyphPDF.wxs v1.4.0 ProductCode, release-notes-v1.4.0.md — all landed.
- **CLAUDE.md / SECURITY.md purge ADOPTED (R4-1)** — see §4.

### R4 — owner decisions

- **R4-1 (purge adoption): EXECUTED per the plan's documented recommendation.** main and consolidated-parity both lack CLAUDE.md/SECURITY.md (purged); pg's copies were stale. Removed in the M2 commit. Zero loss: both files remain recoverable from any pre-merge ref (e.g. feat/redaction-gaps b454d071, feat/parity-glm ef371ad0). **Owner may veto by reverting the two deletions from e11aa083.**
- **R4-2 (image-edit survivor): EXECUTED per plan §3.3 ("port cp, pending R4-2; tests arbitrate").** cp's gp::content image-edit implementation survives (rewriteImageMatrix + setImageZOrder/setImageOpacity + transactional command path); pg's PdfContentStreamReader rewriteImageMatrix variant is retired with this written waiver. Arbitration: TestImageAppearance (ported, rendered-pixel judgments) AND pg's image suite + TestCheckedMutationCoverage must both pass (results §5); a ported-feature-vs-pg-implementation failure here would be documented, never silently dropped.
- **R4-3 (July-branch disposition): RECORDED, not executed.** feature/{editing,viewing,redaction,security}-parity + feature-elevation-wave1a/ocr-parity NOT merged (superseded Wave 1A/1B/2B duplicates per cp's consolidation ledger 1991d9c1; 35–39 conflicts each; unique work already ported as the four July ports). Branches remain open; deletion + archive-bundle refresh are Stage 2, consent-gated.

## 4. M3 — merge main (2b715f47)

`git merge main` → **"Already up to date"**. Proof: `git merge-base HEAD main` = 2b715f47 = main's own tip (main is an ancestor of consolidated-parity, folded via M2). No merge commit is created for an up-to-date target; containment is the record. `git merge-base --is-ancestor main HEAD` = YES.

## 5. Verification (build + suites)

- **Full build**: ninja -j2 in build-ra (Debug, UCRT64) after PCH purge → **SUCCESS, 690/690 targets** (app + all 188 test executables + probe targets incl. R14ProbeRedactSpace.exe, all four ported suites, exit 0). CMake re-registered cleanly under the merged CMakeLists (temp-root infra + v1.4.0).
- **Full serial offscreen ctest** (`ctest -j 1`, per-test temp-root env): **176/179 passed**, real time ~726 s. Per-surface results for every suite the plan §3.4 table requires — **ALL GREEN**:

| Surface | Suite(s) | Result |
|---|---|---|
| redaction (survivor) | TestRedactionProof, TestRedactTransaction, TestSanitization, TestExcisionCorruption, TestSep13LeadRedactionProof | all Passed |
| redaction sweeps (Sept) | TestSweepW1{PresetAdversary,SigningAdversary,SummaryPolicyAdversary,SecProbe}, R14ProbeSep13Fixes | all Passed |
| signing | TestSendForSigning | Passed |
| quality-new seams (M1) | TestBatchPresets, TestAccessibilityChecker, TestAccessibilityFixes, TestSupportBundle, TestPolicyController | all Passed |
| history/redo (union) | TestCheckedMutationCoverage (incl. cp's redo-once cases over the union interface) | Passed |
| find&replace (single impl) | TestFindReplace | Passed |
| Night Mode / viewing (port) | TestViewingModes + TestControllers (NightMode tool routing) | both Passed |
| image edits (R4-2 arbitration) | TestImageAppearance (ported) AND pg image suite via TestCheckedMutationCoverage | both Passed |
| text style (port) | TestTextEditStyle | Passed |
| office export (port) | TestOfficeExport | Passed |

- **The 3 failures and their disposition (all pre-existing machine-side, NOT merge regressions — proven by baseline)**:
  1. **TestReadOnlyGate** — failed once inside the full serial run (`readExpiryDate(dest).isValid()` FALSE); **passes in isolation** (0.77 s). Load-dependent flake.
  2. **TestWelcomeRoutes::imagesRouteProducesAndOpensTheOutput** — `QFileInfo::exists(out)` raced a temp-file write under 110 s of disk contention; **passes in isolation** (4.5 s, 19/20 slots green even in the failing run).
  3. **TestSweepW3UxFlows** — fails nondeterministically in the modal-driving flows (F2a completion-feedback capture, F2b preset-name dialog, F3 redact-apply destination). **Baseline experiment**: built pure feat/parity-glm (ef371ad0) from a detached checkout in a separate build dir (`build-ra-pg`, this worktree) and ran the identical suite twice directly: run 1 = 10/11 with flow3 failing identically (`QFileInfo::exists(redactedOut)` FALSE); run 2 = 8/11 with **flow2a + flow2b + flow3 failing — the exact same three flows, at the same or higher rate, on the untouched pre-merge line**. The suite itself documents this fragility class (F2b-D1 first-run preset-store breaker; flow1 records "offscreen harness limitation"; budgets of 20–60 s exhausted only when the modal genuinely never opens). Conclusion: machine/profile drift since the lane's 2026-09-20/22 green runs; **the reconciliation introduced no regression here** — no merged file participates in those flows (verified: `git diff ef371ad0 HEAD` app-code surface is Night Mode/viewer, image/text-style commands, tool registry/ribbon/menu entries only).
  - Note: R14ProbeRedactSpace is a probe executable, not ctest-registered (same as on pg); it builds cleanly on the consolidated line and its subject matter is pinned in ctest by the TestRedactionProof family + TestSep13LeadRedactionProof + R14ProbeSep13Fixes (all Passed).
- Baseline artifacts kept: `build-ra-pg/` (pg-state build + baseline-run.log, baseline-run2.log).

## 6. Zero-loss proofs (run at e11aa083, pre-verification)

1. **Ancestry emptiness per folded line** — the loss direction is `git rev-list --count feat/consolidated..<line>` (commits on the line NOT in consolidated); real merges make this exact:
   | Line | Tip | lost | ancestor of HEAD |
   |---|---|---|---|
   | feat/parity-glm | ef371ad0 | **0** | YES |
   | consolidate/all | 95dccb23 | **0** | YES |
   | review/consolidated-parity | f4750af5 | **0** | YES |
   | main | 2b715f47 | **0** | YES |
   (Direction note: the plan wrote the ranges as `<line>..HEAD`, which counts the consolidation's own new commits — the merge commits + the plan cherry-pick on top of each line's tip — and can never be 0; the zero-loss content is the `HEAD..<line>` direction above, recorded here with both to avoid ambiguity.)
2. **Patch-id audit**: `git cherry HEAD <tip>` shows **0 `+` non-merge commits** for each of the four lines.
3. **Tree spot-checks**: every cp-only new file present in HEAD (src/ui/NightModeEffect.h, src/engines/podofo/ContentSpans.h/.cpp, src/commands/ImageAppearanceCommand.h, tests/TestImageAppearance.cpp, tests/TestTextEditStyle.cpp, tests/TestViewingModes.cpp, tests/TestOfficeExport.cpp, docs/release/release-notes-v1.4.0.md); every pg-only file intact (tests/TestRedactTransaction.cpp byte-identical to pg tip at 2243 lines, docs/audit/SOAK-FOLLOWUP-2026-09-23.md); find&replace single implementation (src/ui/FindReplaceDialog.*, tests/TestFindReplace.cpp identical to pg); G1–G6+R14 commits (d07b0645, d129324f, b4fe2a5f, c2776ec4, 6c785eba, 33a70818, 198e3bf2) all ancestors.
4. **main**: containment pre-proven (merge-base = main tip) and `--is-ancestor` YES (see §4).
5. **July branches (not folded)**: unique-content ledger lives in the plan §1 + cp's 1991d9c1 ledger; ported work pinned by the follow-up suites (§5). **Stage-2 readiness**: branches all intact (no ref touched), archive-bundle refresh `git bundle create <archive>.bundle --all` is a single command when the owner signs off — deliberately NOT executed now.

## 7. SHA index

| Object | SHA |
|---|---|
| feat/consolidated tip (post-M2) | e11aa083 |
| M2 merge commit | e11aa083 |
| M1 merge commit | b7a7d6b2 |
| plan-doc cherry-pick | f1163f0a |
| base (feat/parity-glm) | ef371ad0 |
| consolidate/all | 95dccb23 |
| review/consolidated-parity | f4750af5 |
| main | 2b715f47 |
| preservation snapshot (r14 probe-dirty state) | reconexec/snapshot-redaction-gaps-dirty @ 2adc3df6 |
| feat/line-reconciliation (plan) | 3811cc6a |

## 8. Residuals

- `origin/feat/parity-glm` remains 106+ commits behind; pushing feat/consolidated is an OWNER action (never done by this run).
- R4-1/R4-2 are executed per documented recommendation and are one-revert reversible; R4-3 deletion pass + archive bundle = Stage 2 (consent gate).
- `.context/reconexec-PoDoFoBackend.cpp.m2-conflicted` keeps the pre-resolution M2 state of the worst file for review.
- EditPolicy.h M1 note: pg's dangling comment fragment was preserved in M1 and integrated/cleaned in M2 (cp's wording) — final state names every gated route including the ported image menu.
