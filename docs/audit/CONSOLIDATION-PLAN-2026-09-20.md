# End-Game Branch Consolidation Plan — 2026-09-20

**Lane:** consolidation-analyst (PLAN + EVIDENCE ONLY — no consolidation, deletions, or pushes were executed).
**Constraints honored:** all git operations read-only (fetch only; no push, no origin ref changes, no deletions, no gc/prune). One throwaway scratch worktree under `.context/consolidation-scratch/wt` was used for merge rehearsals and removed afterwards; its rehearsal commits (`3b19bdef..a82036df`) remain as unreachable-but-preserved objects (no gc ever runs).
**Goal (user order):** origin consolidates to **TWO branches** — `main` + the agents' integration line — with **ZERO work lost**: every branch merged or proven content-equivalent before any ref disappears.
**Scope:** the 54 origin heads (`main` + 53 others). Local-only branches are classified in §7 for loss-risk awareness; their cleanup is a separate lane.

---

## 1. Verdict at a glance

Origin is not 37 branches — it is **54 heads** (main + 53). Classification of the 53 against the mainline tip `feat/parity-glm` = `ec9f16f6` (2026-09-20 22:49, "Merge branch 'feat/rotate270-fix'"):

| Class | Count | Branches | Action |
|---|---|---|---|
| **A. Ancestor of mainline tip** (fully contained) | 38 | all 27 `feat/parity-glm-*` lane/gate/pack/r24/sep13/sweep-w1/w2 branches, `feat/glm-comp`, `feat/glm-ocr`, `feat/l7-rotate-annot`, `feat/accessibility-p1`, `feat/batch-presets-p1`, `feat/candidate-leak-fix`, `feat/followups-2026-09-15`, `feat/printable-summaries`, `feat/send-for-signing-p1`, `feat/sweep-legacy-fix`, `feat/sweep-w1-adversary`, `feat/sweep-w1-fixes`, `feat/sweep-w1-security`, `feat/sweep-w2-testing` (= `origin/feat/parity-glm` `b17106a3`), `feat/sweep-w2-verify-b` | **Fold: delete-after-proof** (post-consolidation `git rev-list <b> ^T` = 0 is the proof). Zero content risk today. |
| **B. Rides in via the `main` merge** (not ancestors of the line, but ancestors of / 1 commit off `main`) | 2 | `audit-remediation` (`fd74f5dc`, **ancestor of main**, 343 commits), `claude/modest-mccarthy-riuo2o` (`a8f50a44`, exactly **1** commit off main: its own docs tip `AR-PROMPT-1..12`) | Covered automatically by merging `main`; then fold with the same containment proof. |
| **C. Real merge required** (unique commits, all docs/tools or small code) | 8 | `feat/modularity-moves` (4 commits, **contains** `feat/sweep-w3-arch`), `feat/soak-48h` (1), `feat/soak-verdict` (1), `feat/sweep-quality-new` (3, code), `feat/sweep-w3-archaeo` (2), `feat/sweep-w3-devops` (1), `feat/sweep-w3-perf` (4, tools), `feat/sweep-w3-research` (5) — merging these 8 refs also contains `feat/sweep-w3-arch` (9 branches resolved) | **Merge into the integration line first** (order in §5; rehearsal: 7/8 conflict-free, one docs-file conflict). |
| **D. Archive, do NOT merge** (superseded parallel implementations) | 4 | `feature/editing-parity` (35 uniq), `feature/redaction-parity` (32), `feature/viewing-parity` (35), `feat/annotation-eraser` (origin tip `ebd022cc` = 1 commit off main; the June eraser) | **Bundle + tag archive** (§6) — zero loss without importing inferior duplicates of features the mainline already ships (evidence in §4.3). |

Plus `main` itself: **not an ancestor** — `git cherry feat/parity-glm main` shows 430 main-only commits of which **401 are patch-id-equivalent** (the other-session cherry-pick era, PROVEN) and only **14 carry content the line lacks** (§4.1). The endgame is **one real merge of `main`** (rehearsed; §5.2).

**End state: `main` + one integration branch, everything else deleted after the proofs in §8. Tags `v1.0.0/v1.0.1/v1.2.0/v1.2.1` are untouched and remain the release provenance anchors.**

---

## 2. Mainline and integration-line state

- **Mainline tip:** `feat/parity-glm` = `ec9f16f6` (local; 2026-09-20). `origin/feat/parity-glm` = `b17106a3` is **7 commits behind, fast-forwardable** (uniq 7/0) — the first execution step is simply `git push origin feat/parity-glm` (execution lane; not done here).
- **Integration line:** `feat/parity-glm-integration` (local = origin) = `2f755244` (2026-09-15) — **strict ancestor** of `feat/parity-glm`, 0 unique, 102 behind. The agents' line continued on `feat/parity-glm`; the `-integration` name is stale but aligned.
- **`main`:** local `2b715f47` (2026-09-13) = `origin/main` (`d03d6e94`, 2026-09-02) + 2 commits. `origin/main` **is an ancestor** of local main, so merging local main covers the remote.
- **Recommendation — what `main` becomes:** after the consolidation tip `T` exists (§5), **`main` := `T` and the surviving integration branch := `T`** (same commit, two names). Both moves are fast-forwards (`main` is an ancestor of `T` because `T` contains the main merge; `feat/parity-glm-integration` is an ancestor of `T` by construction), i.e. no history rewrite, no force-push, consistent with the repo's all-FF push record (deletion audit §1.3). Two candidate namings, zero content difference:
  - **Recommended:** canonical integration name = **`feat/parity-glm-integration`** (it says what it is; `feat/parity-glm` was a lane name that became the line by accretion). Push `T` to both, then fold `feat/parity-glm` (its tip `ec9f16f6` is an ancestor of `T`).
  - Acceptable alternative: keep `feat/parity-glm` as the line (125-push habit, all local lanes track it) and fold `-integration` instead. Pure naming; the user should pick once.
- Version note for the release owner: the consolidated tree inherits **CMakeLists `VERSION 1.3.2.3`** from the line while `main` carries the **v1.4.0** release engineering (bumps + `packaging/GlyphPDF.wxs` ProductCode, 2026-09-02, never cherry-picked). If releases resume from `T`, adopt main's version/packaging hunks in the main-merge resolution (§5.2, class R3).

## 3. Inventory of all 53 origin branches

Full machine-readable evidence: `.context/consolidation-scratch/inventory.txt` (branch | tip SHA | last-commit date | class | tip subject). Per-branch tips (short SHAs), grouped by class:

- **A. Ancestors (38):** feat/accessibility-p1 `c5496ba6` 09-19 · feat/batch-presets-p1 `f80033a4` 09-15 · feat/candidate-leak-fix `5c8fd087` 09-15 · feat/followups-2026-09-15 `247e0c76` 09-15 · feat/glm-comp `585cc45d` 08-27 · feat/glm-ocr `585cc45d` 08-27 · feat/l7-rotate-annot `71891494` 09-15 · feat/parity-glm `b17106a3` 09-20 · feat/parity-glm-clean `352c9b1e` 09-09 · feat/parity-glm-formjs `2290ab51` 09-10 · feat/parity-glm-gate `7f8cf950` 09-09 · feat/parity-glm-gateC `74be82cf` 09-10 · feat/parity-glm-infra `77e3b066` 09-10 · feat/parity-glm-integration `2f755244` 09-15 · feat/parity-glm-n17n18 `819d84a5` 09-14 · feat/parity-glm-packa `40a38cef` 09-13 · feat/parity-glm-packafix `5ae41536` 09-14 · feat/parity-glm-quick `2b81fc28` 09-14 · feat/parity-glm-r05 `5995b7fa` 09-13 · feat/parity-glm-r18f `7d5ae676` 09-14 · feat/parity-glm-r22 `7ceeba82` 09-15 · feat/parity-glm-resid2 `9129788f` 09-14 · feat/parity-glm-review `cd01e892` 09-15 · feat/parity-glm-sec `e36e714c` 09-09 · feat/parity-glm-t2 `21673bc0` 09-09 · feat/printable-summaries `b06b2f07` 09-15 · feat/r24-policy `335d1d3a` 09-15 · feat/r24-wiring `b8909dcb` 09-20 · feat/send-for-signing-p1 `f621416d` 09-19 · feat/sep13-fixes `e8b9a19e` 09-14 · feat/sep13-leads `b0fd8296` 09-14 · feat/sep13-residual `c7e9ecfe` 09-15 · feat/sweep-legacy-fix `a73419ed` 09-20 · feat/sweep-w1-adversary `8bcde898` 09-20 · feat/sweep-w1-fixes `e8e715f2` 09-20 · feat/sweep-w1-security `f6d46dd9` 09-20 · feat/sweep-w2-testing `b17106a3` 09-20 · feat/sweep-w2-verify-b `84a19f9e` 09-20
- **B. Rides main (2):** audit-remediation `fd74f5dc` 06-22 (ancestor of main; 11 patch-unique vs the line all live in main) · claude/modest-mccarthy-riuo2o `a8f50a44` 06-16 (224 uniq vs the line; **all but its own tip commit are in main**)
- **C. Real merges (8 refs → 9 branches):** feat/modularity-moves `3c411cc8` 09-20 (4 commits: SigningLabels seam `5c02b01d`, dead-include sweep `791115bb`, ledger row; **parent chain contains feat/sweep-w3-arch `46cae7ba`**) · feat/soak-48h `1d2e76b8` 09-15 (docs/audit/SOAK-48H-2026-09-15.md) · feat/soak-verdict `790a7197` 09-20 (SOAK-VERDICT doc + `tools/soak_verdict_summary.py`) · feat/sweep-quality-new `673129e1` 09-20 (3 commits: real code — `VersionedJson::atomicWrite` seam `0c1aeabd`, test pins `288c2815`) · feat/sweep-w3-archaeo `b8a5a564` 09-20 (2, one doc) · feat/sweep-w3-devops `fbfa6e47` 09-20 (1 doc) · feat/sweep-w3-perf `2ec24f41` 09-20 (4: perf harnesses `tools/perf/*` + baseline docs) · feat/sweep-w3-research `e97f7075` 09-20 (5, docs/research + matrix notes)
- **D. Archive-only (4):** feature/editing-parity `97172fbe` 07-12 · feature/redaction-parity `45bf4302` 09-13 · feature/viewing-parity `de1fa268` 09-07 · feat/annotation-eraser `ebd022cc` 06-16

## 4. Special-branch analysis

### 4.1 `main` — the cherry-pick era, proven
`git cherry feat/parity-glm main` → **401 `-` / 14 `+`** of 430 main-only commits. The 14 (SHAs): `06cb0bd0` (M4-PROMPT-5 security tools, 05-30 — superseded: the line ships a far deeper certify/timestamp/PAdES stack per the evidence ledger), 6 memory-vault docs commits (`4488f7f6`, `2026e1ab`, `1b055370`, `bd522c75`, `8a6d25a4`, `022dadc1`), `a4466674` (OSS governance files), `cafe4e65` + `793beb71` (WP-0/WP-7 memory truth-ups), `5ca7e36e` (**release v1.0.0** launch-gate/MSI/winget), `1abf3008` + `2e51b2c4` (**release v1.4.0** version+packaging — the only release engineering the line lacks), `2b715f47` (tip: Sept-13 parity-review findings preservation — this is the deletion audit's own 62-path preservation commit).
Content verdict: docs/governance/release-eng = **take**; code = superseded by reviewed line implementations. The 401 equivalents are exactly the "other-session cherry-pick era" the mission asked about — nothing there is lost by merging with line-side conflict resolution, and all of it stays reachable via main's parent history forever regardless.

### 4.2 Foreign gemini-worktree branches (local-only; honest classification)
`subagent-AST-Architect-self-54b52cfc` (`d7a8ca06`, 1 uniq), `subagent-Vendoring-Specialist-self-ca2c3f27` (`5a018ae5`, 1 uniq), and the rest (`-a1982f71`, `subagent-Forms-Specialist-self-26881e20`/`feature/m4-forms` `68c4734a`, `subagent-Vendoring-…-7708fcd5`, `subagent-View-Specialist-…-ecb2f058`, `feature/m4-edge`, `feature/m4-security`, `feature/m4-djot-foundation`) are **all ancestors of `feat/parity-glm`** (verified per-branch). The two 1-uniq branches are staged-artifact snapshots whose selected content was preserved on main's history by the Sept-13 preservation commits (`d7a8ca0` ast, `5a018ae` djot — 62/62 paths hash-verified, deletion audit §3). Classification: **artifact dumps, contained or path-preserved; not part of the origin endgame; safe to fold locally whenever the gemini worktrees are retired.** No origin action.

### 4.3 The July-era parity trio + the June eraser — real unique work, honestly dispositioned
All three parity branches share one 30-commit "Wave 1A" run (`c61edbed`…`055592df`, 2026-07-01) plus their own tails:

- **editing-parity** tail: Cut/Copy/Delete (`d955fc6c`), eraser via deleteObjectAt (`a52bbf9a`, `787c3d82`), letter/line-spacing + image z-order (`d40f129e`), `TestEditingWave1B` + 2 bug fixes (`97172fbe`).
- **redaction-parity** tail: two WIP snapshots securing in-flight engine-interface changes (`1263df9a` 09-07, `45bf4302` 09-13).
- **viewing-parity** tail: real bitmap rotate (`cd82d701`), hyperlink GoTo/URI navigation (`c537dd62`), two-page continuous layout (`f12e2c06`), pixel-inversion Night Mode (`01f92bc8`), WIP snapshot (`de1fa268`).
- **feat/annotation-eraser** (origin `ebd022cc`): one June-16 commit implementing the eraser (5 files, +80) on the pre-v1.3.1 base; local twin `916a4b7d` sits one commit off `main`.

`git cherry` says **none of these patches exist on the line** (35/32/35/1 `+`). But patch-inequivalence is not feature-absence: the line **re-implemented the same Wave-1A/1B items during the parity program** — verified in the `feat/parity-glm` tree: eraser/`deleteObjectAt` present in EditController/AnnotationLayer/engines; `setExpiryDate` UI caller (SecurityController.cpp:474); `CompareMode::compareFiles` real entry point (CompareMode.cpp:791); `ReorderPermutationCommand` (atomic page reorder) present; Validate-All signatures (Capability/SignatureManager). Merging would import 35+32+35 one-year-stale parallel implementations into files the line has since re-owned (EditController, PoDoFoBackend, AnnotationLayer, PdfViewerWidget, CMakeLists on all three) — a maximal-conflict, zero-feature-gain operation. **Disposition: ARCHIVE with evidence (§6), not merge.** Honest residual: Night Mode and hyperlink navigation were verified only by grep hits in `PdfViewerWidget.{cpp,h}` (the R24/viewing lanes touched the same surfaces); if the release owner wants per-feature parity confirmation before deletion, run the §8 grep checklist first — the branches remain recoverable from the bundle either way.

### 4.4 In-flight lanes explicitly out of consolidation scope
- `feat/soak-48h-resume` (`0fad38c0`, checked out in **pdf-keyC — SOAK, off-limits**; 1 uniq commit). Do not fold or delete until the R25 re-soak (SOAK-VERDICT: re-soak required) concludes.
- `feat/sweep-w3-ui` (`164dabd4`, checked out in pdf-sec; 1 uniq) — an open lane, local-only; fold when the lane lands.

## 5. The consolidation sequence (execution lane's runbook; this lane executed none of it)

### 5.0 Preconditions
1. `git push origin feat/parity-glm` (FF `b17106a3` → `ec9f16f6`) — origin gets the mainline tip.
2. Create the archive of record (§6) — **before** any ref moves.
3. Freeze: no lane pushes during consolidation.

### 5.1 Phase 1 — fold the eight real merges into the line (rehearsed sequentially in the scratch worktree from `ec9f16f6`; rehearsal tip after this phase: `833c3064`)

| Order | Branch | Rehearsal merge commit | Conflicts |
|---|---|---|---|
| 1 | feat/soak-48h | `3b19bdef` | none |
| 2 | feat/soak-verdict | `4f2b0de1` | none |
| 3 | feat/sweep-w3-archaeo | `271b49bd` | none |
| 4 | feat/sweep-w3-devops | `7a59b66b` | none |
| 5 | feat/sweep-w3-perf | `cb9428d0` | none |
| 6 | feat/sweep-w3-research | `c750d92b` | none |
| 7 | feat/sweep-quality-new | `9c677c0e` | none |
| 8 | feat/modularity-moves | `833c3064` | **exactly one file: `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`** (both quality-new and modularity-moves append rows). Resolve by **row-union** (precedent: the sep13-residual merge). Do NOT use `-X ours` — the rehearsal only used it to prove mechanical resolvability and drops modularity's ledger rows. |

This phase also contains `feat/sweep-w3-arch` (parent of modularity-moves). CMakeLists.txt is touched by quality-new **and** modularity-moves but did not conflict in rehearsal.

### 5.2 Phase 2 — the `main` merge (the only hard merge; rehearsed at `a82036df`)
`git merge main` from the phase-1 tip. **Forecast (measured): ~130 conflicted paths** — both lines evolved the same code since merge-base `1669f70d` (2026-06-02) even where individual patches are equivalent, plus workflows/packaging/docs. Resolution policy (decide once, apply per class):

- **R1 — code (`src/**`, `tests/**`): integration side wins.** Main's unique code content is superseded (§4.1); its 401 patch-equivalents already exist on the line in reviewed form. Modify/delete case measured: `tests/mocks/MockFormManager.h` — **keep the deletion** (line's forms refactor removed the mock; main modified dead code).
- **R2 — docs/governance/memory vault: take main's additive content.** `.github/` templates, `CONTRIBUTING/CODE_OF_CONDUCT/SECURITY`, `docs/release/release-notes-v1.4.0.md`, `docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`, `docs/planning/**`, `memory/**`, `.context/m2p*/m3p*` vault analysis land via the merge; where the line later deleted a path (52 of the 121 paths touched by the 14 commits — full list in §8 checklist), **keep the line's deletion**: every one of them remains byte-reachable via `git show 2b715f47:<path>` for as long as history exists, which is what zero-loss requires. Tip hygiene is a separate, optional sweep (recommend leaving as-is).
- **R3 — release engineering (`CMakeLists.txt` version, `packaging/*.wxs/.ps1`, `.github/workflows/release.yml`): adopt main's state** if releases resume from `T` (line is at 1.3.2.3, main at 1.4.0 + new ProductCode). This is the one class needing the release owner's sign-off.
- **R4 — workflows (`ci.yml`, `glyphpdf-fuzz.yml`, `license-guard.yml`): union** — line's INF04 push filters (`feat/**`) + main's release workflow. The INF04 evidence (`CURRENT-EVIDENCE-LEDGER` INF04 row) pins the exact filter semantics.
- **Gate:** after resolution, run the line's standard gate (host full serial ctest; the R22 Linux gate if available). No consolidation commit should bypass the suite the lane already uses for merges.

### 5.3 Phase 3 — refs
1. Tag/archive the D-class tips (§6).
2. `T` = the phase-2 tip. Push `T:main` (FF from `d03d6e94`) and `T:feat/parity-glm-integration` (FF from `2f755244`).
3. Fold, with proofs (§8), in this order: the 38 A-class, then B-class (`audit-remediation`, `claude/…` — now contained via main), then C-class (8 merged), then D-class (archived), then `feat/parity-glm` (after the naming choice in §2) and `feat/parity-glm-integration`'s stale twin if the alternative naming was chosen. **End state: `main` + one integration branch = 2 heads.**
4. Tags untouched. Never gc/prune (deletion-audit standing order).

## 6. Zero-loss archive (the alternative to deletion, evaluated)

**Recommendation: one full `--all` bundle as the archive of record** — `git bundle create pdf-clean-consolidation-archive-2026-09-20.bundle --all` (~110 MB; `size-pack` = 111,938 KB) — plus **archive tags** for the four D-class tips (`tag archive/2026-09-20/feature-editing-parity 97172fbe -m …`, etc.) so provenance stays ref-visible on origin without branch spam. The bundle contains every ref and tag, verifies with `git bundle verify`, and can be cloned from standalone — it is the insurance that makes every later deletion reversible. Range bundles were also validated for per-branch archives (`git bundle create x.bundle feature/editing-parity --not feat/parity-glm` → 118 KB / 288 objects, `bundle verify` OK) but record **prerequisites** (the excluded base) and only open in repos that have it — acceptable inside this repo, not as the standalone archive of record. Object-count sizing for the D/local archive classes: editing 288, redaction 234, viewing 273, security-parity 257, ocr-verify-finereader 170, recovery/temp-stage-push 81, origin/annotation-eraser 2 620, origin/claude 2 274 objects. Note: D: has 24 GB free (98% used) — create the bundle on C: or verify space first.

## 7. Local-only branches (out of origin scope; loss-risk awareness)

55 local branches are not on origin: **39 are ancestors** of `feat/parity-glm` (all r2-*/r3-* era, all m4-*/subagent-* except two, gateE, linux, r04r08r10/r06r13/r11r12/r15r17/r18r19/resid/sep13, sweep-w1-fuzz, sweep-w2-verify, sweep-w2c, sweep-w3-emergence, sweep-w3-ux, parity-nemo-b, parity-ox-auto, rotate270-fix, sanitize-assert-mutable, accessibility-parity, audit/parity-glm, cleanup/post-v1.3.2, msys2-migration-backup-pre, fuzz/redaction-rig). **16 carry unique commits** (uniq): ar/prompt-1 (5), feat/ocr-verify-finereader (20), feature/security-parity (31 — July fourth sibling, same Wave-1A base), feature-elevation-wave1a = feature/ocr-parity (30 — the Wave-1A run itself), feat/erase-ox-auto (1), feat/regex-find-replace = backup/regex-verified-a39356e (1), feat/regex-ox-auto (1), feat/sweep-w3-ui (1, in-flight), feat/soak-48h-resume (1, SOAK, off-limits), subagent-AST-…-54b52cfc (1), subagent-Vendoring-…-ca2c3f27 (1), recovery/temp-stage-push-20260909 (12 — the deletion audit's recovery ref; keep until the consolidation bundle supersedes it). None are reachable from origin today except via the bundle; a **local** consolidation lane should apply the same merge/archive/fold protocol after the origin endgame, and only after the gemini worktrees and soak worktree are released.

## 8. Risk table + post-consolidation verification

Loss-risk if the branch were deleted **today** (before consolidation), merge-conflict forecast, and the reviewer's proof command after consolidation (`T` = final tip):

| Branch | Loss risk today (content reachable?) | Conflict forecast | Post-consolidation proof |
|---|---|---|---|
| 38 A-class ancestors | **None** — every commit reachable from `ec9f16f6` | none (by definition) | `git rev-list <b> ^T \| wc -l` → `0`; spot: `git diff <b> T -- docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` sanity only (ledger evolves by union) |
| audit-remediation | **None** — ancestor of `main` (`2b715f47`) | rides the main merge (its 11 vs-line patch-uniques live in main) | `git rev-list fd74f5dc ^T` → `0`; `git merge-base --is-ancestor fd74f5dc T` |
| claude/modest-mccarthy-riuo2o | **One commit** (`a8f50a44` docs) not on any line — lost if deleted pre-merge | none (docs-only, via main) | `git rev-list a8f50a44 ^T` → `0`; `git show T:docs/audit/REMEDIATION-SESSION-PROMPTS.md`-class spot check if restored |
| 8 C-class branches | **Yes** — 1–5 unique commits each (docs/tools/code) not on origin's line | rehearsed: 7 clean + 1 file (ledger union) | `git rev-list <b> ^T` → `0` for each; `git log --oneline T -9` shows the 8 merges |
| main | Only its 14 `+` commits are unique; all 121 touched paths stay readable at `2b715f47` forever via T's second parent | ~130 paths, per §5.2 policy | `git rev-list 2b715f47 ^T` → `0`; `git show T:docs/release/release-notes-v1.4.0.md` lands; `git show 2b715f47:memory/Home.md` always works |
| feature/editing-parity | **Yes** — 35 commits | would conflict hard across src/ (superseded impl) — do not merge | Archive proof: `git bundle verify <bundle>`; `git ls-remote archive-tag`; restore recipe `git clone bundle → git merge 97172fbe` |
| feature/redaction-parity | **Yes** — 32 commits (incl. 2 WIP snapshots; selected paths also preserved on main by `45bf430`-preservation commit) | same | same |
| feature/viewing-parity | **Yes** — 35 commits | same | same |
| feat/annotation-eraser | Only its 1 straggler commit (`ebd022cc`); feature itself ships on the line | trivial but pointless to merge (June impl vs shipped eraser) | archive tag + bundle; `git show ebd022cc --stat` provenance |
| feat/sweep-w3-arch | None after phase 1 (contained by modularity-moves) | n/a | `git rev-list 46cae7ba ^T` → `0` |
| soak-48h-resume / sweep-w3-ui (local, in-flight) | Yes — unique, unreached; **do not touch this round** | n/a | excluded from proofs; fold when lanes land |

**Reviewer one-liner after the endgame (origin):** `git ls-remote --heads origin` shows exactly `main` + the integration branch; `git rev-list --count <each-old-tip> ^T` = 0 for every A/B/C branch; `git bundle verify <archive.bundle>` exits 0; `git log --oneline T -12` shows the 8 small merges + the main merge; suite gate green at `T`.

## 9. Residuals / unknowns (honest)

1. **R25 re-soak pending** (SOAK-VERDICT verdict FAIL after Windows-Update restart at 4h00m21s; 0 candidate-attributable failures; `TestSanitization` PoDoFo AssertMutable SegFault ×2 is the top follow-up). Consolidation may proceed — the soak candidate `2f755244` is an ancestor of `T` — but the re-soak should run at `T` afterwards.
2. `origin/feat/parity-glm` is 7 commits behind the local tip (§5.0 step 1 fixes it; not pushed by this lane).
3. The main-merge resolution classes R2/R3/R4 involve judgment (52 deleted-paths dispositions, version adoption, workflow union) — the rehearsal proved mechanical resolvability (`-X ours` + one `git rm`), not the policy; the ~30 min human pass over the 130-file conflict list with the §5.2 classes is the remaining cost, followed by the full suite gate.
4. Viewing-parity Night Mode / hyperlink navigation verified by grep only (§4.3 residual).
5. `git fsck`-visible garbage: `D:/pdf/pdf/.git/worktrees/pdf-clean/refs` flagged by git as garbage during reads — harmless, never pruned, noted for the cleanup lane.
6. A `graphify` post-commit hook fires on commits in worktrees (background rebuild to `~/.cache/graphify-rebuild.log`) — cosmetic; rehearsal commits triggered it.
7. Local-only unmerged set (16 branches, §7) is deliberately out of scope; two of its branches are pinned by live worktrees (pdf-keyC soak, pdf-sec UI lane).
8. The July-trio archive decision (§4.3) rests on 6 spot-checked feature equivalences + patch-id analysis; the bundle makes it reversible if any spot-check is later disputed.

## 10. Rehearsal evidence ledger

Scratch worktree `.context/consolidation-scratch/wt` (detached at `ec9f16f6`, removed after use). Sequential merges: `3b19bdef` soak-48h, `4f2b0de1` soak-verdict, `271b49bd` archaeo, `7a59b66b` devops, `cb9428d0` perf, `c750d92b` research, `9c677c0e` quality-new, `833c3064` modularity-moves (ours-strategy on the single ledger file — **rehearsal only**), `a82036df` main (`-X ours` + `git rm tests/mocks/MockFormManager.h` — **rehearsal only**; production resolution per §5.2 classes). At `a82036df`: `rev-list --count <b> ^HEAD` = 0 for all ten merge inputs (soak-48h, soak-verdict, archaeo, devops, perf, research, quality-new, modularity-moves, main, audit-remediation). These commits are now unreachable-but-preserved (no gc); the execution lane reproduces them with real merge messages on the live line.
