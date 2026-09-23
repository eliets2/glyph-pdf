# CONSOLIDATION LEDGER — 2026-09-24 (integrator, review/consolidated-parity)

Integrator lane for PR #2 (`review/consolidated-parity`). Extends the PR body's
"No-work-lost accounting" with the branches that moved or were newly dispositioned
after that body was written. Base at lane start: `8a0a8b3d` (= origin tip). Method:
`git cherry` patch-equivalence + feature-presence checks in the PR tree + tree-level
`git diff` against every candidate tip. Target: **0 unexplained commits** across the
outstanding set — achieved (every `+`-marked commit below is named and dispositioned).

## B1 — commits brought in this lane (cherry-pick `-x`, linear, oldest first)

| Picked SHA | Source commit | Branch | Content | Gate after pick |
|---|---|---|---|---|
| `885c9f0b` | `983dd81d` | feat/accessibility-p2 | T2-4 P2 engine: AccessibilityTagger.{h,cpp} + TestAccessibilityTagger + CMake registration | build 522/522; TestAccessibilityTagger **14 passed / 0 failed / 1 skip** (veraPDF subprocess QSKIPs offline by design in this tree — real-CLI evidence lives in the pdf-featplans worktree) |
| `bf85bfbd` | `d409a739` | feat/accessibility-p2 | T2-4 P2 panel: Tag Document action (AccessibilityPanel, GpMainWindow shell wiring) + 4 panel tests | build green; TestAccessibilityTagger + TestAccessibilityPanel both green |
| `09ef9bfa` | `acf7e888` | feat/accessibility-p2 | T2-4 P2 veraPDF subprocess invocation hardening (test-side) | (doc/test-only; covered by the next gate) |
| `0e3dcdd` | `4e70217e` | feat/accessibility-p2 | T2-4 P2 ledger rows | CONFLICT on CURRENT-EVIDENCE-LEDGER resolved as ROW UNION (PR's sections kept; T2-4 section appended); suite gate: all a11y suites green |
| `64c48ba1` | `f1163f0a` | feat/consolidated | LINE-RECONCILIATION-PLAN-2026-09-23.md (plan, 169 lines) | docs-only |
| `fe61a662` | `a59f92c3` | feat/consolidated | LINE-RECONCILIATION-EXECUTION-2026-09-23.md (M1/M2/M3 record, 124 lines) | docs-only |
| `905c9bc4` | `eb0efa21` | feat/consolidated | LINE-RECONCILIATION-EXECUTION verification section (build 690/690, ctest 176/179, flake disposition) | docs-only |

## B1c — remote-tip drift check (`git ls-remote --heads origin` vs local + archive tags)

80 remote heads checked; 7 drift rows, all dispositioned with no pick required:

| Remote tip | Remote | Local | Verdict |
|---|---|---|---|
| `main` | `d03d6e94` | `2b715f47` | remote tip **IS contained in PR HEAD** (ancestry-verified); local `main` = remote + 2 unpushed doc commits, also contained |
| `audit-remediation` | `fd74f5dc` | `84445698` | remote tip **IS contained in PR HEAD** (ancestry-verified) |
| `feat/parity-glm` | `26c9a415` | `195e4309` | remote stale (ancestor of local). Local delta = lane merges + the accessibility-p2/consolidated-doc commits this lane picked + the PR's own ports; every non-merge commit has a HEAD twin (see row 6 below) |
| `feature/redaction-parity` | `1263df9a` | `45bf4302` | remote stale (ancestor of local); local tip is the tagged superset (archive) |
| `feat/annotation-eraser` | `ebd022cc` | `916a4b7d` | remote = the old-line eraser tip, pinned by archive tag; local twin diverged (see row 8) |
| `claude/modest-mccarthy-riuo2o` | `a8f50a44` | (none) | remote tip = archive tag content exactly; v2 doc is in HEAD (see row 9) |
| `review/consolidated-parity` | `8a0a8b3d` | this lane | expected: this lane's own fast-forward push |

## B2 — the 14-branch disposition table

Verdicts: FOLDED (picked `-x` onto the PR, SHA given) · PRESENT (content already in
the PR, proof given) · SUPERSEDED (a newer implementation of the same content is in
the PR, named) · ARCHIVE-ONLY (provenance-only; recoverable from tags/bundle).

| # | Branch (tip) | Verdict | Evidence |
|---|---|---|---|
| 1 | feat/accessibility-p2 `4e70217e` | **FOLDED** | 4/4 commits picked: `983dd81d`→`885c9f0b`, `d409a739`→`bf85bfbd`, `acf7e888`→`09ef9bfa`, `4e70217e`→`0e3dcdd` (ledger conflict = row union). Build 522/522; a11y suites green after each code pick. Branch is now fully contained (its only content was these 4 commits on `feat/feature-plans`). |
| 2 | feat/consolidated `eb0efa21` | **FOLDED** (docs) + rest SUPERSEDED | Its 3 net-new doc commits picked (`f1163f0a`→`64c48ba1`, `a59f92c3`→`fe61a662`, `eb0efa21`→`905c9bc4`). Its 2 merge parents: `e11aa083` merges review/consolidated-parity itself (reverse-merge, contained by definition); `b7a7d6b2` merges consolidate/all (stale earlier consolidation line — see residual R2). Tree diff consolidated-vs-HEAD after picks: 15 files, all HEAD-side ahead (it lacks the P2 tagger, TestEraserEngine, flow3b) or consolidated-side stale revisions (P1-era panel honesty text that HEAD's P2 panel replaced; older SECURITY-QUALITY-REVIEW status/PGR-34 row that HEAD's `8a0a8b3d` supersedes). Nothing net-new remains. |
| 3 | feat/line-reconciliation `3811cc6a` | **PRESENT** | The branch is 1 commit (the plan doc) on `ef371ad0`; its `docs/audit/LINE-RECONCILIATION-PLAN-2026-09-23.md` is **byte-identical** (`git diff` empty) to the file landed by pick `64c48ba1`. |
| 4 | feat/ux-integration-fixes2 `14d69ce9` | **PRESENT** | Lane commits after `ef371ad0`: 4/5 patch-equivalent (`git cherry` `-`) to the PR's existing picks (`8cfee283`, `6f7cadb2`, `3d342639`, …). The 1 `+` is `14d69ce9` (lane ledger rows) — superseded by HEAD's own "UX-INTEGRATION FIX LANE 2" ledger section (`f07711e1`, ledger line 1382), which is the union version of the same rows. |
| 5 | feat/redaction-gaps `b454d071` | **PRESENT** | Lane commits after `b8d17885`: 7/8 patch-equivalent to the PR's G1–G6 + R14-probe picks. The 1 `+` is `b454d071` (lane ledger rows) — superseded by HEAD's "redaction gap-fix lane" section (ledger line 1270), the union version. |
| 6 | feat/parity-glm `195e4309` | **PRESENT** (content) / SUPERSEDED (docs) | Local delta over origin `26c9a415` = merges of lanes already in the PR + the accessibility-p2 and consolidated-doc commits this lane picked + the PR's own port/squash-era commits. Every non-merge commit has a HEAD twin. Ledger: all sections HEAD also carries (AM1/AM2 at line 1194, ux-defects UX-F4d-D1/F5-F1/F5-F2/F6-F1/S4S-SLOT at line 1207, dispatch-gates at 1303, emergence at 1326, W2-GSD at 1364 — verified by grep, the tree-diff `+`s were positional duplicates). Remaining tree delta (4 files) = HEAD-ahead or older ledger/review-doc revisions. Nothing net-new remains. |
| 7 | recovery/temp-stage-push-20260909 `d367aca1` | **PRESENT / SUPERSEDED / ARCHIVE-ONLY** (12 commits, each dispositioned) | The deletion audit's recovery ref; exactly 12 non-merge commits beyond `feat/parity-glm`. (a) 7 infra fixes INF02–INF06 + Q02 (`88c15da6`, `a9ad3bba`, `2848dbd7`, `35f160d3`, `d830c5ff`, `245a267b`, `23703294`) — PRESENT via the "repair-order step 5" lane (HEAD ledger line 269); presence greps: `GLYPH_TESTING` production gate + per-test-target definitions and the `DJOT_LIB_DIR` bootstrap note in HEAD's CMakeLists. Their `[workflow hunks parked pending push scope]` annotations are the workflow-file variants the prior audit called out — **ARCHIVE-ONLY by design** (the .github workflow hunks were never pushed, purge-safety); the code hunks are in. (b) `f7d57f6d` (step-5 ledger rows) — SUPERSEDED by HEAD's step-5 section. (c) `6b26d0fb` (form-JS plan) + `d367aca1` (send-for-signing plan) — SUPERSEDED: HEAD's `docs/research/form-js-implementation-plan.md` and `send-for-signing-implementation-plan.md` are later revisions (88/25-line deltas), and both features are implemented (src/engines/formjs/, feat/send-for-signing-p1 lane). (d) `fd0338bd` + `422d8d49` (measure T1 A/B) — SUPERSEDED: HEAD's measure toolset is a strict superset (MeasureCore.h superset with the G23 parseQuantityToken gate; MeasureMode.{h,cpp} +707 lines vs the T1B tree; TestMeasurePanelHonesty registered). |
| 8 | feat/annotation-eraser (origin `ebd022cc`, local twin `916a4b7d`) | **SUPERSEDED** + ARCHIVE tags | PR body's disposition re-verified: eraser present across src/core/ToolId, EditController, AnnotationLayer, PdfEditorEngine, RibbonModel; `tests/TestEraserEngine.cpp` in HEAD and registered (CMakeLists:5163). The 14 non-equivalent commits on the local twin are old-line docs/memory/vault/release commits whose content landed in the squash or was superseded; the twin is pinned in the stage-1 bundle + archive tags. |
| 9 | claude/modest-mccarthy-riuo2o `a8f50a44` | **SUPERSEDED** (v2) + ARCHIVE tag | Remote tip = archive tag content exactly. Its v1 prompts doc superseded by v2 `41ab3331` — `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` is **content-identical in HEAD** (diff empty). |
| 10 | audit-remediation `fd74f5dc` | **PRESENT** | Remote tip is an ancestor of HEAD (ancestry-verified). Local `84445698` diverged (stage-1 finding) — contained in the stage-1 bundle; nothing net-new (docs-only lane per plan §5.3). |
| 11 | `main` `d03d6e94` | **PRESENT** | Remote tip is an ancestor of HEAD (ancestry-verified); local main = remote + 2 unpushed doc commits (§9.8/§9.9 copy + Sept-13 findings appendix), which the PR carries as its own first commits (`703fa34e`, `2b715f47` per the PR body). |
| 12 | feature/redaction-parity (origin `1263df9a`, local `45bf4302`) | **ARCHIVE-ONLY** | Old Wave-1A/1B/2B parallel line. Unique work ported per the PR body (redaction parity items on this line's model); remote tip = stale ancestor of the tagged local superset (`archive/redaction-parity` 4617f9ff → `45bf4302`). No merge (would import a stale parallel implementation into re-owned files — plan §4.3). |
| 13 | feature/editing-parity `97172fbe` | **ARCHIVE-ONLY** | Tagged `archive/editing-parity` (357c676f). Ports landed in the PR: TestOfficeExport (`e6497dbe` twin), letter/line spacing + opacity for inline text edits (`3d8bca5d` twin). Remainder superseded (PR body file-level proof). |
| 14 | feature/viewing-parity `de1fa268` | **ARCHIVE-ONLY** | Tagged `archive/viewing-parity` (89603df2). Ports landed: Night Mode + Eye Care use-after-free fix (`6020ea8b` twin). Session-only Rotate View deliberately not carried (PR body "Not carried forward" — product decision, stays in archive refs). |

**Unexplained commits: 0.** Every `+`-marked commit encountered during the B2 sweep is
named above with its disposition.

## Residuals (recorded, not silently dropped)

- **R1 — B1c picks:** none required. No remote tip carried work the PR lacked after the
  B1a/B1b picks; the two lanes that pushed post-consolidation (redaction-gaps,
  ux-integration-fixes2) were already fully picked before this lane.
- **R2 — local-only branches out of origin scope** (consolidation plan §7):
  `consolidate/all` `95dccb23` (stale earlier consolidation line, 63-file tree delta all
  HEAD-ahead or superseded) and `feat/erase-ox-auto` `faa6cf10` (1-uniq eraser variant,
  superseded by the same landed eraser implementation). Both remain local-only, covered
  by the stage-1 bundle; dispositioning them is the local-lane follow-up, not the origin
  endgame.
- **R3 — bundle refresh:** the stage-1 executor's loud finding stands — re-create the
  `--all` bundle after the final merge batch (these picks included) and verify SHA-256
  before any ref deletion.
- **R4 — veraPDF CLI evidence:** TestAccessibilityTagger's veraPDF subprocess test QSKIPs
  in this tree (no CLI); the real-CLI run (15/15) is evidenced in the pdf-featplans
  worktree (`tools-verapdf/`, untracked tool bundle) and recorded in the T2-4 ledger rows.

## Verification performed by this lane

- `cmake --build build-review --parallel 6`: green (522/522 targets) after the code picks.
- `ctest -R TestAccessibility(Tagger|Panel)`: green (Tagger 14 passed / 0 failed / 1
  environment skip; Panel green incl. the 4 new P2 tests).
- Phase A frozen-state sweep: all 7 lane worktrees clean of tracked source changes
  (untracked logs/evidence/tool bundles only) — recorded in `.context/integrator-wip.md`.
- Ancestry containment (`git merge-base --is-ancestor`): main, audit-remediation remote
  tips inside HEAD.
- Patch-equivalence (`git cherry`) scoped to each lane's base; tree-level `git diff` for
  feat/consolidated, feat/parity-glm, consolidate/all.
