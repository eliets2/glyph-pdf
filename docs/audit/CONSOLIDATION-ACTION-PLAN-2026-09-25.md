# Consolidation Action Plan — 2026-09-25 (analysis refreshed 2026-09-29)

**Lane:** consolidation-planning (PLAN + EVIDENCE ONLY — nothing executed beyond this docs
commit on `feat/consolidation-action-plan`). Read-only against every other ref: no merges, no
deletions, no pushes, no ref moves. One scratch worktree (`.context/consplan-wt`) was used to
commit this plan and removed afterwards (stage-1 precedent).
**Owner order:** consolidate the repository into ONE branch, verified, then push into `main`;
keep zero work lost; a second (critique) agent must pass over this plan before execution.

---

## 0. Reconciliation with the prior plans (read these first)

| Prior document | What it established | Status vs this plan |
|---|---|---|
| `CONSOLIDATION-PLAN-2026-09-20.md` @ `744e9e20` | 54-origin-head classification (A/B/C/D), R1–R4 merge policies, archive-bundle strategy, §8 proof table | Superseded in mechanics by what actually happened: the September line never merged `main` directly; **two parallel integration lines formed and were reconciled** (below). Its class table and proof commands remain the backbone of §3/§4 here. |
| `CONSOLIDATION-STAGE1-2026-09-23.md` | Bundle `C:\Users\User\pdf-archive-stage1.bundle` SHA-256 `e87d56da558c912f61cd8a66c48decb3967f4e4436ad15615df57e1ebb401862` (431 refs, verify OK); 4 local `archive/*` tags; 38/38 A-class fold proofs at `26c9a415`; main divergence 401 patch-eq / 14 real; stage-2 draft | All still valid. Its binding refresh obligation (§1.5) is encoded as Phase 0 here. Its `claude/modest-mccarthy-riuo2o` reclassification is now **moot for content** (the docs tip's file is byte-present in the PR tree — §2.4) but the branch stays until the blob check passes. |
| `CONSOLIDATION-STAGE2-SCRIPT-2026-09-23.md` + `consolidation-stage2-draft.sh` | CONSENT-gated, fail-closed, DRYRUN-default deletion runbook | **Its shape is reused verbatim** (phases, `delete_proven()`, gates) but its merge phases never ran: the merge work was overtaken by the line-reconciliation + the PR program. The script here (`consolidation-action-draft.sh`) replaces it; the old draft stays as history. |
| `LINE-RECONCILIATION-PLAN/EXECUTION-2026-09-23.md` | `feat/consolidated` @ `eb0efa21` = `feat/parity-glm`@`ef371ad0` + consolidate/all + consolidated-parity@`f4750af5` + main, 29/29 conflicts resolved, zero-loss proofs ×4, build+ctest verified | Executed and verified. `feat/consolidated` was then **fast-forwarded into `feat/parity-glm`** (fc is an ancestor of pg @ `195e4309`), which added the ux-integration-fixes2 + accessibility-p2 merges. Every proof from that run still holds at `195e4309`. |
| `CONSOLIDATION-HANDOFF-2026-09-24.md` + `CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` (on `review/consolidated-parity`) | PR #2 (branch `review/consolidated-parity` @ `12da4e2f`): Phase B ledger, Phase C PGR picks (22 commits, all `-x`), C.6 quickjs 0.15.1 pin coordination, then the FIXALL session: Phase 0 acceptance (`fbb40295`), 22 §4 folds + 3 addendum picks, CX-01..17, PGR-46, PGR-23 (superseded WIP), N1, INV-1 — gates E1 build 945/945, E2 serial **185/185 = 100%**, E3 purge+linear, E4 secret scan 0/166,613 lines, E5 ledger 0 unexplained (105 `-x` trailers), E6 pushed + CI recorded | **This is the most advanced, most-verified state in the repository** and the governing handoff for the next actor (its §9: "Merge PR #2 into main keeping its commits; refresh the all-refs backup bundle; delete the other branches — as the owner decided"). This plan operationalizes exactly that, with the proofs to make it zero-loss. |
| `UNREVIEWED-CODE-MAP-2026-09-23.md` | 222 implemented-awaiting-review ledger rows, **all reachable** from the united line; 254 carrying commits, all ancestors | Still true at pg @ `195e4309` (map checked at `26c9a415`; pg only advanced by contained lanes + the reconciliation). The PR tree contains the map document byte-identical (`48ecc79b`) and contains every carrying surface; the PR additionally *reviewed and fixed* large parts of it (the CX/PGR waves). The critique pass must verify the 63-line ledger delta (§5, item 5). |

**What changed since the stage-2 draft was written (drift this plan absorbs):**

1. The merge phases of stage 2 **already happened, twice over**: once as the line-reconciliation
   (merge topology, on `feat/consolidated` → ff into `feat/parity-glm`), once as the PR program
   (squash + cherry-pick topology, on `review/consolidated-parity`). The two were proven
   content-identical at `f4750af5`/`eb0efa21` by the reconciliation; the PR then advanced ~140
   reviewed commits further (PGR + FIXALL), which the merge line does NOT have.
2. The "five in-flight remediation lanes" the stage-1 bundle refresh was waiting for **have
   landed** (batch-presets-p2, pr-review-fixes, redaction-gaps, dispatch-gates, soak-followups —
   all contained in the united line or folded into the PR; §3).
3. The two "in-flight fix lanes" from the 09-25 brief are **no longer in flight**:
   `feat/batch-presets-p2` (U1–U7 + ledger rows) and `feat/pr-review-fixes` (M1–M3 + §3 items)
   are folded into the PR — content-verified below (§3.4), including the two commits the lanes
   themselves later superseded.
4. The archive-tag estate has grown to 75 lightweight `archive/branch/*` tags + 7 `archive/*`
   tags + stashes — but they are **local-only** (origin carries only the 13 release tags), and
   several current tips are **unpinned** (§3.5). This plan fixes both.

---

## 1. The target-branch decision

### 1.1 The candidates, measured today (2026-09-29)

| Ref | Tip | Contains (commit-level) | Tree vs the others | Distinct strengths |
|---|---|---|---|---|
| `review/consolidated-parity` (PR #2) | `12da4e2f` | `origin/main`, `main`@`2b715f47`, `audit-remediation`@`fd74f5dc` — **0 merge commits in `origin/main..HEAD`**, linear, purge intact | **Strict content superset of `feat/consolidated`@`eb0efa21`** (name-set diff empty; every differing file is a documented PR-side fix wave — see §1.3) and of `feat/parity-glm`@`195e4309` (pg's 11 beyond-reconciliation commits all patch-equivalent, `git cherry` = 0 unique) | Claude's independent verification + PR #2 review history + CI runs; E1 fresh build 945/945; E2 serial 185/185 = **100 %** (+ sanitizer gate green); E3 purge + linearity; E4 secret scan 0 matches/166,613 lines; E5 ledger **0 unexplained** (105 `-x` trailers); the CX-01..17, PGR-46, PGR-23, N1, INV-1, PGR-42..45 fixes exist **only here**; **the only line fully pushed to origin** (origin = local = `12da4e2f`) |
| `feat/consolidated` | `eb0efa21` | `feat/parity-glm`@`ef371ad0` + `consolidate/all`@`95dccb23` + `review/consolidated-parity`@`f4750af5` + `main`@`2b715f47` (merge topology; `rev-list --count <tip>..fc` = 0 for all four) | Subset of the PR's tree (the PR added ~140 reviewed commits after `f4750af5`) | The four-line commit-level union; 29/29 documented conflict resolutions; merge-based zero-loss proofs |
| `feat/parity-glm` | `195e4309` | everything `feat/consolidated` has (fc is its ancestor) **+** ux-integration-fixes2 + accessibility-p2 (11 commits, all patch-equivalent in the PR) | Subset of the PR's tree (same +11-commit delta, all patch-eq) | The September program's real commit history (1000+ commits, lane merges, G-gates); `origin/feat/parity-glm` @ `26c9a415` is 176 behind (unpushed tail) |
| `main` | `2b715f47` | ancestor of BOTH candidates | oldest | the push target, not a candidate |

### 1.2 Recommendation — **`review/consolidated-parity` @ `12da4e2f` is the consolidated branch; it becomes `main` by fast-forward**

Reasoning:

1. **Content:** the PR's tree is a verified superset of the reconciliation tree. Nothing that
   exists on `feat/consolidated` or `feat/parity-glm` is missing from the PR (proofs §4.2–4.3);
   the delta runs the other way — ~140 commits of reviewed fixes (two CRITICAL-class: PGR-21
   signature bypass `5cec76cb`, PGR-44 space law `6600a429`) that exist **only** on the PR.
   Choosing fc/pg as the survivor would mean re-playing ~140 cherry-picks across the exact
   conflict surfaces (EncryptedFileSecretStore, PoDoFoBackend, RedactionProof, CMakeLists) the
   PR already resolved with green gates — a week of re-work to reach a strictly worse state.
2. **Verification:** the PR carries the strongest gate record in the repo (fresh build, serial
   100 %, purge, secret scan, ledger accounting, independent review, CI with captured-artifact
   explanations of every red). The owner's "to be verified" is already satisfied there.
3. **The governing handoff says so:** the FIXALL handoff §9 (written for the next actor) directs
   merging PR #2 into main. Overriding it would strand the PR branch unmerged or force a
   rewrite of a pushed branch (forbidden, R3).
4. **Push topology is clean:** `origin/main` (`d03d6e94`) and `main` (`2b715f47`) are both
   ancestors of `12da4e2f` — `git push origin review/consolidated-parity:main` is a pure
   fast-forward, no merge commit, no force, and GitHub marks PR #2 merged automatically.
5. **The cost, stated honestly:** the PR's history is **linear-by-construction** (squash
   `1991d9c1` + cherry-picks), so the pg line's ~1000-commit granular history and the
   reconciliation's M1/M2 merge commits are **not in the survivor's ancestry**. Commit-level
   `rev-list` emptiness proofs can therefore never hold for pg/fc/ca against the PR. Zero-loss
   is instead proven at three other levels (§4) and the history itself is preserved by pinning:
   **one new tag on `feat/parity-glm`@`195e4309` keeps every one of those commits reachable
   forever** (fc, ca, cp@`f4750af5`, main, all A-class tips, all pg lane branches are its
   ancestors), on top of the refreshed `--all` bundle. This is the same trade the 09-20 plan
   already priced when it blessed the squash topology as "content-absorbed".

Rejected alternatives:
- **`feat/consolidated` as survivor:** loses the entire PGR/FIXALL program (§1.2 item 1); would
  require re-folding ~140 commits and re-running every gate; PR #2 would dangle unmerged.
- **A new branch:** a new name for one of the two above; adds a third line to fold; no benefit.
- **Welding the histories (merge `feat/consolidated` into the PR)** as a *fallback only*:
  measured at **2 conflicted paths** (ledger → row-union; `src/core/RedactionProof.cpp` → take
  the PR's PGR-23-recursion side) and the merged tree keeps the purge intact — but it breaks
  the PR's E3 linearity gate (0 merges) and re-opens verified surfaces. Not recommended; kept
  in the script behind `HISTORY_WELD=true` in case the owner weighs commit-granularity above
  linearity.

**Decision encoded:** `SURVIVOR=review/consolidated-parity`, `FINAL_HEAD=12da4e2f` (re-resolve at
execution; any new PR-head commit requires re-running the §4 proofs), end state = `main` at the
survivor's head, `main` the only branch on origin (survivor ref deleted after PR #2 records as
merged, per the owner's "one branch" reading; keep it instead if the owner wants 2 heads).

---

## 2. The fold order (dependency-ordered; cherry-pick `-x`, never merge, per R2)

The fold surface is **two candidate folds**, not 248 — everything else is
verify-pin-and-archive. Order below is execution order.

### 2.1 FOLD-1 — `ar/prompt-1` @ `5d999987` (5 commits, June AR-1 safety fixes) — CONDITIONAL

Beyond-line commits: `db90eb41` (D1 watermark null-deref), `7ba4fa43` (D2 render-prefetch UAF),
`c6d67de9` (D3 blanket soffice taskkill → data loss), `2ffe0712` (D4 autosave-rename QTimer
UAF), `5d999987` (D5 AIChatPanel void* UAF). These are June-era real bug fixes on old bases
(`uPG=5`, all patch-unique vs the PR).

**Gate G-FOLD-1 (per-fix supersession review, before any pick):**
- D2 — **proven superseded**: the PR's `RenderCache.h` ships the stronger EC06 fix
  (`drainPrefetches()` — cancels and waits on **all** in-flight prefetch futures from BOTH
  `clear()` and the destructor, vs D2's single-token `cancelAndWaitForPrefetch`). Skip.
- D3 — **likely superseded**: the PR's `ConversionManager.cpp:646` kills by PID
  (`taskkill /F /T /PID <pid>`), not the blanket image-name kill D3 removed. Confirm the
  surrounding code has no blanket variant, then skip.
- D1 / D4 / D5 — **must each be re-derived** against the PR tree (watermark path in
  `PoDoFoBackend.cpp` was rewritten many times since June; `AutosaveManager` and `AIChatPanel`
  both still exist). For each: if the hazard survives in the current code, cherry-pick `-x` the
  fix, resolve onto the current shape (forecast: small per-file conflicts; the June patches
  touch 2–3 files each — §3.2), add/refresh a test pin, run the touched suite ×3.
  If the hazard is gone, record SUPERSEDED with the file:line evidence in the ledger.

Expected outcome (planning estimate): 0–3 picks; the branch then deletes under the standard
proof. **No pick may be skipped without a written supersession row** — these are UAF/data-loss
fixes.

### 2.2 FOLD-2 — `feat/ocr-verify-finereader` @ `f5b59e66` (20 commits, FineReader-style OCR verify UI) — RECOMMEND ARCHIVE, owner decides

The largest genuinely-unmerged feature in the repository (B1–B15: verify dialog, word
synchronization, reading-order reorder, dictionaries, zoom pane, hotkeys). Whole-branch merge
forecast vs the PR: **468 conflicted paths** (June-era base, pre-purge) — this is a feature
port, not a consolidation step. Recommendation: **archive-tag it and let the owner open a
dedicated port lane after the endgame** (same treatment as the July branches, R4-3). If the
owner insists on folding now: port commit-by-commit (B1 first, dependency order) with `-x`,
touched-suite gates, expecting heavy hand-resolution in `src/ui/*` — out of consolidation
scope, do not let it block the endgame.

### 2.3 Everything else — no fold, three dispositions

**(a) ALREADY-CONTAINED (delete after proof):**
- **Commit-level contained in the PR** (delete any time after the freeze): `feat/fixall-ci`,
  `feat/fixall-tagging`, `feature/m4-forms`, `msys2-migration-backup-pre`,
  `subagent-Forms-Specialist-self-26881e20`, `subagent-View-Specialist-…-ecb2f058`
  (+ `main` and the survivor themselves, handled by the phase sequence).
- **United-line ancestors** (uPG = 0 → pinned by the new `archive/line/parity-glm-195e4309`
  tag; delete after the tag + bundle exist): 116 local + 74 remote branches — every
  `feat/parity-glm-*` lane/gate/pack branch, every sweep/sep13/r24/soak/residual/docs branch,
  `feat/consolidated`, `consolidate/all`, `feat/ux-integration-fixes2`, `feat/accessibility-p2`,
  `feat/redaction-gaps`, `feat/dispatch-gates`, `feat/sweep-w3-ui`, `feature/accessibility-parity`,
  the r2-*/r3-* era, the gemini `m4-*`/subagent remnants, `feat/consolidation-plan/-stage1`,
  `feat/unreviewed-map`, `feat/line-reconciliation` (its 1 beyond-commit `3811cc6a` is
  patch-equivalent to the reconciliation's `f1163f0a`, both docs), `fix/p0-blank-viewer`
  (3 PR-side commits, patch-equivalent), the `archive/branch/*`-tagged origin twins, etc.
  Full machine list: `.context/consplan/` sweep output (regenerated by the script, phase 0).
- **Fold-lane branches whose content the PR holds** (delete after their §4.5 proof):
  `feat/pgr-critical-fixes`, `feat/pgr-c2`, `feat/pgr-c3`, `feat/pgr-c4`, `feat/formjs-review`,
  `feat/pgr40-quickjs-bump`, `feat/pgr-d2`, `feat/fixall-images`, `feat/fixall-inv1`,
  `feat/fixall-tagging2`, `feat/fixall-redaction`, `feat/fixall-forms(2)`, `feat/fixall-ci2`,
  `feat/final-pgr-closers`, `feat/pr-review-fixes`, `feat/batch-presets-p2`,
  `feat/residual-exec` + their origin twins. Each carries ≤4 patch-unique commits and every one
  of them is individually dispositioned in §3.4 / §4.5 (pick, resolve-fold, supersession, or
  docs-absorption — with the exact evidence).

**(b) ARCHIVE (bundle + tag; never merge; no deletion pressure):**
- July-era Wave-1A/1B/2B siblings — merge forecasts vs the PR: editing 475, security 472,
  viewing/redaction similar; all also re-import `CLAUDE.md`/`SECURITY.md` (pre-purge bases),
  which alone forbids merging: `feature/editing-parity` `97172fbe`,
  `feature/redaction-parity` `45bf4302` (local) / `1263df9a` (origin),
  `feature/viewing-parity` `de1fa268` (tags exist: `archive/editing-parity`,
  `archive/redaction-parity`, `archive/viewing-parity` — pinned to the supersets),
  `feature/security-parity` `8bb20c52` (**needs new tag**),
  `feature-elevation-wave1a` = `feature/ocr-parity` `055592df` (ancestor of editing-parity, so
  already tag-pinned by ancestry; a dedicated tag is cheap — add it).
- `feat/annotation-eraser`: origin tip `ebd022cc` tagged (`archive/annotation-eraser`); **local
  twin `916a4b7d` (1 commit off main, differs from origin) needs its own tag**.
- `feat/ocr-verify-finereader` (§2.2) — tag + owner lane.
- Local-only June branches (superseded content, tag-cheap, delete-after-tag at owner's leisure):
  `feat/regex-find-replace` = `backup/regex-verified-a39356e` `a39356e7` (PRD §9.15 regex
  find&replace — the line's own `FindReplaceDialog` ships regex; superseded),
  `feat/regex-ox-auto` `7d54b387` (same PRD item, other base), `feat/erase-ox-auto` `faa6cf10`
  (PRD §9.3 eraser — shipped via `deleteObjectAt`), `ar/prompt-1` after FOLD-1 closes.
- Preservation/debris refs: `reconexec/snapshot-redaction-gaps-dirty` `2adc3df6` (probe-dirty
  WIP snapshot; underlying `feat/redaction-gaps` IS contained),
  `recovery/temp-stage-push-20260909` `d367aca1` (the 09-13 recovery ref — bundle covers it;
  tag it anyway before any worktree/ref cleanup),
  `subagent-AST-Architect-self-54b52cfc` `d7a8ca06` + `subagent-Vendoring-Specialist-self-ca2c3f27`
  `5a018ae5` (staged-artifact dumps; selected content hash-verified preserved by the Sept-13
  preservation commits, per plan §4.2).

**(c) HOLD (do not fold, do not delete, explicit owner items):**
- `feat/soak-48h-resume` `0fad38c0` — the R25b re-soak candidate (SOAK-VERDICT requires a
  re-soak; plan §4.4 off-limits). Unpins nothing (1 docs commit); tag it so it survives any
  cleanup, keep the branch until the re-soak concludes.
- `test/view-parity-baseline` `0948743b` — owner ruled out of scope (2026-09-28). Tag + leave.
- The 06:01 continuity automation (owner-side scheduler): **pause it before the freeze** and
  confirm no lane is mid-write (the FIXALL handoff's deviation 1 shows lanes have raced the
  integrator before).

### 2.4 The four united lines themselves

- `main` — ancestor of the survivor; becomes the survivor's head via the FF push (Phase C).
- `feat/parity-glm` @ `195e4309` — **pin first** (`archive/line/parity-glm` tag, Phase 0), then
  delete after PR #2 is merged and the proofs pass. The tag pins fc, ca, cp@`f4750af5`,
  `main`@`2b715f47`, every A-class tip, every pg lane — the entire September program's history.
- `feat/consolidated` @ `eb0efa21`, `consolidate/all` @ `95dccb23` — ancestors of pg; same tag
  covers them; delete in the same pass.
- `claude/modest-mccarthy-riuo2o` @ `a8f50a44` — stage-1 reclassified it REAL-MERGE, but its
  unique file `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` is **byte-present in the PR
  tree** (blob-verified 2026-09-29) and `archive/branch/claude/modest-mccarthy-riuo2o` already
  pins the tip: disposition CONTENT-CONTAINED, delete after the blob check re-runs green.

---

## 3. Classification of the 158 local + 90 remote refs (measured 2026-09-29)

Method per ref: `git rev-list --count <survivor>..<ref>` (commit containment in PR),
`git rev-list --count feat/parity-glm..<ref>` (containment in the united line),
`git cherry` scoped to `merge-base(pg, ref)..ref` (patch equivalence), tree/blob spot-checks
for resolve-folded content. Full tables: `.context/consplan/local-sweep.txt`,
`.context/consplan/remote-sweep.txt` (regenerate at execution, phase 0).

| Class | Local | Remote | Disposition |
|---|---|---|---|
| Commit-level contained in the PR (`uPR`=0) | 8 (incl. `main`, survivor, `feat/fixall-ci`, `feat/fixall-tagging`) | 4 (incl. `origin/main`, `origin/audit-remediation`) | delete after freeze + proof |
| United-line ancestors (`uPG`=0, not in PR history) | 108 | 70 | covered by the pg line tag; delete after tag+bundle+PR-merged |
| Fold-lane branches, content verified in the PR | 17 | 11 | delete after per-branch §4.5 proof |
| Beyond-line uniques — FOLD candidates | 2 (`ar/prompt-1`, `feat/ocr-verify-finereader`) | 0 | §2.1/§2.2 |
| Beyond-line uniques — ARCHIVE | 6 (security-parity, annotation-eraser twin, regex ×3, erase-ox-auto) | 4 (July trio + eraser origin) | tag → optional delete |
| Beyond-line — PRESERVATION/debris | 3 (recovery ref, reconexec snapshot, + the 2 subagent dumps counted above) | 1 | tag; delete only after bundle hash check |
| HOLD / OWNER | 2 (`feat/soak-48h-resume`, `test/view-parity-baseline`) | 0 | untouched |
| `claude/modest-mccarthy-riuo2o` | — | 1 | CONTENT-CONTAINED (blob check) |
| Total | 158 | 90 | — |

### 3.4 The fold-lane ledger (every patch-unique commit accounted)

| Branch (tip) | patch-unique vs PR | Disposition (evidence) |
|---|---|---|
| `feat/pgr-critical-fixes` `a690d7f4` | 0 | exact picks `5cec76cb`←8f07066c, `6ed13c52`←29365772, `0b06214a`←a690d7f4 (handoff-0924 §3, E5) |
| `feat/pgr-c2` `552c615f` / `feat/pgr-c3` `5ff0d618` | 0 | exact picks (4 + 3) per handoff-0924 §3 |
| `feat/pgr-c4` `16145af2` | 2 | `9f0ddb63` picked as `6722356f` **with conflict resolution** (deviation D2: T6 zeroization ∪ PGR-20/25 readSecret migration — union, TestSecretStore green); `a2a8ff4f` ledger rows absorbed into the ledger's Phase C section + PGR-STATUS (deviation D5) |
| `feat/formjs-review` `433b77e7` | 0 | 6 exact picks `a851464f`…`8bf27032` (D1 order deviation documented) |
| `feat/pgr40-quickjs-bump` `020c0734` | 0 | pick `77db5be5` (C.6) |
| `feat/pgr-d2` `958bd7c0` | 0 | picks `6600a429` `f90b4c71` `1d59f241` `ddef6a8f` (PGR-42..45; CRITICAL PGR-44) — handoff-FIXALL §4; residual TestBatchOcrSkipText flake disclosed |
| `feat/final-pgr-closers` `68bc917e` | 1 | the WIP **SUPERSEDED** by `6841247d` on the PR (superset: decoded streams + PDFium depth-3; 2 pins ported `06ce3ae4`; OLE-signature residual recorded as owner item §7.9) |
| `feat/fixall-images` `d63ed76e` | 0 | picks CX-02/08..12 + N1 `2ead0b17` (squash note: raced pick proven byte-identical to source diff) |
| `feat/fixall-inv1` `898d362f` | 0 | pick `64baba6a` (INV-1) |
| `feat/fixall-tagging2` `0144d056` / `feat/fixall-tagging` `9d150be2` | 0 / 0 (`uPR`=0 commit-level) | CX-01/04/07 picks + direct lane pushes |
| `feat/fixall-redaction` `5461b72d` | 1 | its N1 variant **SUPERSEDED** by `2ead0b17` (final occurrence-index addressing; TestImageAppearance 41P/4F → 47P/0F) — critique re-diff recommended (§5 item 3) |
| `feat/fixall-forms` `4d410c10` | 11 | **all REVERT commits** (the lane's own duplicate-pick repair) — the PR took the clean Phase-0 path instead; reverts are meaningless on the PR; disposition SUPERSEDED (handoff-FIXALL §3 "fixall-forms lane repair SUPERSEDED (Phase 0)") |
| `feat/fixall-forms2` `5cadce52` | 1 | CX-06 folded with resolution (`f6ef7d68`; same re-wrap-only-entry-read logic, TestSecretStore 23P/0F ×3) |
| `feat/fixall-ci2` `590c6c27` | 1 | docs-only progress note; FIXALL-PROGRESS CP entries on the PR cover the content (spot-check §5 item 4) |
| `feat/pr-review-fixes` `a25c37b7` | 4 | `e89f1234`/`a830d999` (merge-artifact repairs) **SUPERSEDED by the Phase-0 repair** (`fbb40295` acceptance); `3c1e60a6` (read-only gate) and `31ad94ed` (M2 save-first prompt + flow7c) **folded with resolution — content blob-verified in the PR tree** (EditPolicy §3.1 wording, flow7c pin); remaining 10 commits patch-equivalent |
| `feat/batch-presets-p2` `ec22eeed` | 3 | U1 `779fbb1e`/U2 `0f704539` folded with resolution — **distinctive content byte-present in the PR tree** (BatchPreset.h ordered-lane comment verbatim; onConflict rename semantics); `ec22eeed` ledger rows present (R26-P2 ×8) |
| `feat/residual-exec` `5d74c348` | 1 | `9b2b2727` (T2-2 page-space law) **PORTED with documented modification** (replace-side geometry derived from display basis to keep PGR-37 green; fail-before/pass-after transcripts) |
| `origin/claude/modest-mccarthy-riuo2o` `a8f50a44` | 1 | docs file byte-present in PR tree (`docs/planning/AUDIT-2026-06-16-REMEDIATION.md`) — CONTENT-CONTAINED |

### 3.5 Tag estate gaps (fix in Phase 0 — additive)

Existing: 75 × `archive/branch/*` (lightweight), `archive/{editing,redaction,viewing}-parity`,
`archive/annotation-eraser`, stashes, `archive/pr-head-before-picks` `7d4d8d08`,
release tags v1.0.0…v1.4.0. **All archive tags are local-only** (origin carries only release
tags) — they MUST be pushed before any origin branch deletion.
**Unpinned tips needing new tags** (all additive, message + provenance per stage-1 §2 style):
`archive/line/parity-glm` → `195e4309` (pins fc/ca/cp/main/A-class/pg-lanes — THE critical
one), `archive/branch/feature/security-parity` → `8bb20c52`, `archive/branch/feature-elevation-wave1a`
→ `055592df`, `archive/branch/feat/annotation-eraser-local` → `916a4b7d`,
`archive/branch/feat/ocr-verify-finereader` → `f5b59e66`, `archive/branch/ar/prompt-1` →
`5d999987`, `archive/branch/feat/soak-48h-resume` → `0fad38c0`,
`archive/branch/test/view-parity-baseline` → `0948743b`,
`archive/branch/recovery/temp-stage-push-20260909` → `d367aca1`,
`archive/branch/reconexec/snapshot-redaction-gaps-dirty` → `2adc3df6`,
`archive/branch/feat/erase-ox-auto` → `faa6cf10`,
`archive/branch/feat/regex-find-replace` → `a39356e7`, `archive/branch/feat/regex-ox-auto` →
`7d54b387`, `archive/pr-head-final` → `12da4e2f` (merge-time PR head).

---

## 4. Zero-loss proofs (exact command sequences)

`PR = review/consolidated-parity`, `T` = execution-time PR head, `PGT = 195e4309` (re-resolve).

### 4.1 Universal freeze proofs (run before anything else)
```bash
git fetch origin --prune=never                 # refresh, never prune
git bundle create "$ARCHIVE_FINAL" --all       # pdf-archive-final-<date>.bundle on C:
git bundle verify "$ARCHIVE_FINAL"             # must print: complete history
sha256sum "$ARCHIVE_FINAL"                     # record in the report; gates every deletion
```

### 4.2 The survivor ⊇ reconciliation tree (content superset)
```bash
# (a) no file exists on fc/pg that the PR lacks (name set)
diff <(git ls-tree -r feat/consolidated --name-only | sort) \
     <(git ls-tree -r "$PR" --name-only | sort) | grep -c '^<'        # must be 0
# (b) every differing file is a documented PR-side fix: audit the numstat deletions
git diff --numstat feat/consolidated "$PR" | awk '$2>0'                # expected: the CX/PGR/
      # N1/INV-1 surfaces + the restructured ledger; each row must map to a handoff-FIXALL §4
      # table row or a ledger entry — the critique agent walks this list (§5 item 1)
# (c) the reconciliation + audit docs rode along byte-identical
for f in docs/audit/LINE-RECONCILIATION-EXECUTION-2026-09-23.md \
         docs/audit/LINE-RECONCILIATION-PLAN-2026-09-23.md \
         docs/audit/UNREVIEWED-CODE-MAP-2026-09-23.md \
         docs/audit/CONSOLIDATION-PLAN-2026-09-20.md \
         docs/audit/CONSOLIDATION-STAGE1-2026-09-23.md \
         docs/audit/CONSOLIDATED-REPORT-2026-09-20.md; do
  [ "$(git rev-parse feat/parity-glm:$f)" = "$(git rev-parse $PR:$f)" ] || echo "BLOB DRIFT: $f"
done                                                                            # must print nothing
```

### 4.3 The pg line's 11 post-reconciliation commits
```bash
git cherry "$PR" feat/parity-glm "$(git merge-base f4750af5 feat/parity-glm)" \
  | grep -c '^+'    # scoped to commits fc does not already pin → must be 0
# (already measured: 69600dcc 0e1f6535 4f345147 9b408951 14d69ce9 983dd81d d409a739 acf7e888
#  4e70217e (+2 merges) — all patch-equivalent in the PR)
```

### 4.4 United-line and A-class containment (via the new line tag)
```bash
git rev-list --count feat/consolidated..feat/parity-glm        # 0 (fc ancestor of pg)
git rev-list --count consolidate/all..feat/consolidated        # 0
git rev-list --count f4750af5..feat/consolidated | head -1     # 0 beyond the reconciliation
for b in $(cat .context/consplan/united-line-ancestors.txt); do # the 182-ref uPG=0 set
  git rev-list --count "$b" --not archive/line/parity-glm | grep -q '^0$' || echo "NOT PINNED: $b"
done                                                            # must print nothing
```

### 4.5 Per-branch deletion proofs (fail-closed `delete_proven()`, stage-2 shape)
```bash
delete_proven() {  # $1 = branch, $2 = pinned tip SHA
  git rev-parse --verify "$1" = "$2"                               || return 1   # tip unmoved
  case "$1" in main|*consolidated-parity) return 1;; esac                       # never these
  # containment, any one of:
  [ "$(git rev-list --count "$2" --not "$PR")" = 0 ]               || :         # commit-level, or
  git cherry "$PR" "$2" "$(git merge-base feat/parity-glm "$2")" \
      | grep -q '^+'                                               || :         # patch-level, or
  <recorded content pin>                                           || return 1  # tree/blob check
  git bundle verify "$ARCHIVE_FINAL" \
    | grep -q 'complete history'                                   || return 1  # bundle sound
  [ "$(sha256sum "$ARCHIVE_FINAL" | cut -d' ' -f1)" = "$ARCHIVE_FINAL_SHA" ] || return 1
  git push origin --delete "${1#origin/}"                          # and git branch -d locally
}
```
Per fold-lane branch, the `<recorded content pin>` comes from §3.4's table (e.g. for
`feat/batch-presets-p2`: `git show $PR:src/core/BatchPreset.h | grep -F -q 'run-ordered
cross-file continuity'` plus the R26-P2 ledger-row count; for `feat/pr-review-fixes`:
EditPolicy `PR-review §3.1` blob grep + flow7c pin grep; for `feat/final-pgr-closers`:
`git merge-base --is-ancestor 6841247d $PR`).

### 4.6 Post-fold, post-merge end-state proofs
```bash
git push origin review/consolidated-parity:main        # FAST-FORWARD ONLY (--force-with-lease absent)
git merge-base --is-ancestor d03d6e94 main && echo origin-main-ancestor-OK
git rev-list --merges --count d03d6e94..main           # 0 (linearity preserved on main)
git log --full-history d03d6e94..main -- CLAUDE.md SECURITY.md   # empty (purge intact)
git ls-remote --heads origin                           # exactly: main
git tag --list 'archive/*' | wc -l                     # all pushed; spot: git ls-remote --tags origin
git rev-list --count 195e4309 --not archive/line/parity-glm      # 0 — the pg line survives its ref
```

---

## 5. Critique checklist (for the reviewing agent — check, question, reject)

**Check (re-derive, do not trust this document):**
1. **The superset claim** (§4.2): run the name-set diff and — the load-bearing part — walk
   every row of `git diff --numstat feat/consolidated $PR` with deletions > 0 and map it to a
   handoff-FIXALL §4 table row or ledger entry. Expected hot rows: `PoDoFoBackend.cpp`
   (511+/184−), `CURRENT-EVIDENCE-LEDGER` (251+/63−), `ContentSpans.cpp` (409+/61−),
   `BatchMode.cpp` (836+/52−), `EncryptedFileSecretStore.cpp` (270+/46−),
   `SignatureManager.cpp` (233+/38−), `RedactionProof.cpp` (345+/16−). Any row without a
   disposition = REJECT the plan until explained.
2. **The ledger 63-line delta** between pg's and the PR's `CURRENT-EVIDENCE-LEDGER` blob
   (`2fc16ace` vs `9c006e93`): confirm every pg-era row survives in the PR blob or is
   dispositioned in `CONSOLIDATION-LEDGER-2026-09-24/25` ("0 unexplained" is the PR's claim —
   re-verify by diffing the row sets, not by trusting E5).
3. **The two supersession verdicts that rest on one actor's word**: `feat/fixall-redaction`'s
   N1 variant `5461b72d` vs the PR's squashed `2ead0b17` (handoff admits a mid-fold race and a
   squash; re-diff `git diff 5461b72d $PR -- src/ tests/` for the image surface and confirm the
   occurrence-index semantics are the union, not the last writer), and `68bc917e` vs `6841247d`
   (confirm the OLE-signature residual really is recorded for the owner, not dropped).
4. **FOLD-1's supersession review** (§2.1): independently re-derive D1/D3/D4/D5 against the PR
   tree (D2 is proven here). Reject any "probably fine".
5. **The `-x` accounting**: `git log --format=%B d03d6e94..$PR | grep -o 'cherry picked from
   commit [0-9a-f]\{40\}' | sort | uniq -d` — must be exactly the 12 Phase-0 sources + the
   documented sets; sample 10 picks at random: `git diff <source> <pick> -- <paths>` must show
   only documented resolution deltas.
6. **Worktree map vs deletion list** (`git worktree list`): 22 worktrees are live (2026-09-29),
   holding main, pg, consolidate/all, the survivor, 5 fixall lanes, the July trio, pr-review-
   fixes, batch-presets-p2, view-parity-baseline, 5 gemini subagents, detached `pdf-base7d`.
   Branches checked out anywhere cannot be deleted until their worktree is removed — the
   execution script must enumerate and sequence this, and `pdf-base7d`'s baseline artifacts
   (flow7 disposition) should be exported or explicitly abandoned by the owner.
7. **CI state at merge time**: the PR's Test step has never been fully green on the runner
   (Fontconfig/runner-limitation + the `TestAccessibilityPanel` load race, artifacts captured).
   Confirm the owner accepts merge-with-explained-reds, or land the fontconfig step
   (`328a224b` on `feat/pr-review-fixes`, already content-folded) — wait, it IS on the PR
   (verify: `git show $PR:.github/workflows/ci.yml | grep -q fontconfig`) — and re-check.
8. **Re-run stage-1's proofs** at execution time (38 A-class emptiness vs `195e4309`, main
   401/14 stability) — the stage-1 numbers were taken at `26c9a415`; pg has moved.
9. **The PR head is still `12da4e2f`** (or re-run §4 for the new head). The 06:01 automation
   and any lane agent must be confirmed idle BEFORE the freeze (deviation D1 in the FIXALL
   handoff is the precedent for why).

**Question:**
10. The "one branch" end-state reading: main only (survivor ref deleted after PR #2 records
    merged) vs main + survivor. This plan picks main-only per the owner's words; the survivor
    ref deletion is the last, most reversible step — confirm.
11. `feat/ocr-verify-finereader`: archive vs port — the largest unmerged feature; the owner
    should explicitly accept "archived, port later" (§2.2).
12. `feat/soak-48h-resume`: the re-soak never ran. Does the endgame proceed without a re-soak
    at T (the 09-20 plan §9.1 permits consolidation-first), or does the owner want the re-soak
    on the consolidated head before the deletion pass?
13. `test/view-parity-baseline` and `recovery/temp-stage-push-20260909`: tag-and-keep vs
    tag-and-delete — owner's call; this plan defaults to tags + delete only the branch refs,
    never before the final bundle hash check.

**Reject on sight:**
14. Any **merge** (instead of cherry-pick `-x`) of a lane branch into the survivor (R2); any
    `-X ours/theirs` on the ledger (row-union only, stage-1 §5.1 precedent); any force-push,
    history rewrite of a pushed ref, `gc`, `prune`, or reflog expiry anywhere (standing orders);
    any deletion before the final bundle verifies with a matching SHA-256; any deletion of
    `main`/the survivor by the script's own logic; any deletion of a branch whose tip is not
    pinned by a tag AND covered by the bundle AND proof-passed.
15. Editing the quickjs pin (0.15.1) or weakening any CI guard during the endgame (the
    coordination record and its operational gotchas are in handoff-0924 §3 D3).

---

## 6. Execution script — DRAFT (NOT EXECUTED)

Companion: `docs/audit/consolidation-action-draft.sh` (same directory). Default **DRYRUN**;
every mutation requires `CONSENT=I-UNDERSTAND-THIS-DELETES-BRANCHES`; fail-closed
`delete_proven()`; refuses to touch `main`/survivor/tags; never force/gc/prune; FF-only pushes.
Phases:

| Phase | What | Gate |
|---|---|---|
| 0 | Preconditions: pause the 06:01 automation; freeze announced; `git worktree list` exported; sweep re-run (classification tables regenerated, diffed against §3 — any drift aborts); **final bundle + verify + SHA**; **new archive tags created (§3.5) + all archive tags pushed**; stage-1 proofs re-run (§5 item 8) | any drift ⇒ abort; bundle verify must pass |
| 1 | FOLD-1: `ar/prompt-1` supersession review (G-FOLD-1) → cherry-pick `-x` the survivors onto the PR in a scratch worktree of `D:/pdf/pdf-review`; build + touched suites ×3; push FF (R3) | every fix has a row: PICKED (with pin) or SUPERSEDED (with file:line evidence) |
| 2 | (owner decision point) FOLD-2 ocr-verify: default ARCHIVE — skip | owner's written choice recorded in the ledger |
| 3 | Merge to main: `git push origin review/consolidated-parity:main` (FF); verify §4.6; PR #2 records merged | FF check fails ⇒ stop, no force ever |
| 4 | Suite gate at main's new head (E1 fresh build + serial ctest per handoff §8; sanitizer job green; CI re-checked) | serial 100 % modulo the dispositioned flake class |
| 5 | Deletion pass, fail-closed `delete_proven()` per branch, order: worktree-released fold-lane branches → commit-contained set → united-line set (after PR #2 shows merged) → archive classes (branch refs only, tags stay) → HOLD set **skipped** | any failed proof stops the whole phase; log to `CONSOLIDATION-ACTION-PROOFS-<date>.txt` |
| 6 | End state: `git ls-remote --heads origin` = `main` only; tags intact; report written; bundle re-verified one last time | reviewer one-liners from §4.6 |

**Consent text the operator must set:** `CONSENT=I-UNDERSTAND-THIS-DELETES-BRANCHES`
(deletions) and `CONSENT_PUSH=I-UNDERSTAND-THIS-MOVES-MAIN` (phase 3). Without them the script
prints the planned actions and exits 0.

## 7. Rollback plan

| Failure moment | Recovery |
|---|---|
| Before phase 3 (nothing pushed) | Nothing to roll back: phases 0–2 are additive or scratch. Drop the scratch worktree, `git branch -D` any scratch ref, delete the picks from the PR only if they were pushed (then `git push origin +<old>:$PR` is **forbidden** — instead `git revert` the picks; history never rewrites, R3) |
| Phase 3 pushed main, something is wrong | main is FF-only: `git push origin <previous-main-sha>:main` is a fast-forward **backwards** only if no one built on it; the honest path is `git revert` on main. Previous main = `2b715f47` (local) / `d03d6e94` (origin) — both remain tag- and bundle-pinned |
| A branch was deleted that shouldn't have been | (a) local: `git branch <name> <pinned-tip>` — every deleted branch's tip was pinned by an `archive/branch/*` tag or the line tag BEFORE deletion (phase 0 gate); (b) origin: `git fetch origin` still has the bundle — `git push origin <tip>:refs/heads/<name>`; (c) worst case, full restore: `git clone C:\Users\User\pdf-archive-final-<date>.bundle restored` or `git fetch "$ARCHIVE_FINAL" 'refs/*:refs/restore/*'` — the bundle contains every ref incl. all deleted branches and tags |
| The bundle itself is lost/corrupt | The stage-1 bundle (`e87d56da…`, 2026-09-23 ref space) is the second copy; SHA-256 recorded in stage-1 §1.2 and here; `git bundle verify` both. Reflog: never expired (standing order) — `git reflog` covers every local ref move for as long as the repo exists |
| The whole directory dies | The bundle on C: + this plan's branch + the PR on origin + the release tags on origin reconstruct everything; fresh-worktree staging notes (vendored podofo, pdfium/onnxruntime DLLs, quickjs pin cache) are in handoff-0924 §8 |

## 8. SHA index (measured 2026-09-29)

| Object | SHA |
|---|---|
| Survivor / PR #2 head (= origin) | `review/consolidated-parity` = `12da4e2f4785d9548f0661b1c72cd06346d42f41` |
| Phase-0 acceptance head | `fbb40295` |
| Phase-B head (prior run) | `7d4d8d08`; pre-Phase-B `8a0a8b3d` |
| Squash (consolidation) commit | `1991d9c1` |
| feat/parity-glm (united line) | `195e430937efef57e0926bf94a33ee17d6eaf635` |
| feat/consolidated (reconciliation tip) | `eb0efa216c6837795575f9b3d6d2ea678ed7de38` (M2 `e11aa083`, M1 `b7a7d6b2`, base `ef371ad0`) |
| review/consolidated-parity at reconciliation time | `f4750af5` |
| consolidate/all | `95dccb23` |
| main / origin/main | `2b715f47` / `d03d6e94` |
| Stage-1 bundle | `C:\Users\User\pdf-archive-stage1.bundle`, SHA-256 `e87d56da558c912f61cd8a66c48decb3967f4e4436ad15615df57e1ebb401862` |
| Final bundle (to create) | `C:\Users\User\pdf-archive-final-<date>.bundle`, SHA recorded at phase 0 |
| Key fix commits on the PR | PGR-21 `5cec76cb`, PGR-44/42..45 `6600a429`/`f90b4c71`/`1d59f241`/`ddef6a8f`, PGR-46 `99dd7b67`, PGR-23 `6841247d` (+pins `06ce3ae4`), N1 `2ead0b17`, INV-1 `64baba6a`, CX-03 `220b5f2b`, CX-06 `f6ef7d68`, C.6 pin `77db5be5` (full table: handoff-FIXALL §4) |
| FOLD candidates | `ar/prompt-1` @ `5d999987` (D1 `db90eb41`, D2 `7ba4fa43`, D3 `c6d67de9`, D4 `2ffe0712`, D5 `5d999987`), `feat/ocr-verify-finereader` @ `f5b59e66` |
| This plan | branch `feat/consolidation-action-plan` (from pg @ `195e4309`), file `docs/audit/CONSOLIDATION-ACTION-PLAN-2026-09-25.md` + `consolidation-action-draft.sh` |

## 9. Residuals (carried, not blocking)

1. PGR-23 WIP's OLE-signature signature (`\xD0\xCF\x11\xE0`) not ported — owner item (handoff-FIXALL §7.9).
2. `TestAccessibilityPanel` load race (the one crash-class item), `TestSignatureRealCrypto` DSS
   flake, `TestWelcomeRoutes` flake, flow7/capture robustness — owner items (handoff-FIXALL §7.5–8).
3. PGR-40 (quickjs interrupt polling — no MSYS2 release carries it; pin auto-arms), PGR-41
   (fresh-runtime-per-event design), PGR-33 (open-as-is) — deferred by owner decision.
4. `feat/soak-48h-resume`: the R25b re-soak has not run — schedule at the consolidated head.
5. CI runner Test-step reds (Fontconfig default config) — the fix exists folded on the PR
   (`328a224b` content); verify presence at execution and re-check CI after merge.
6. The gemini subagent worktrees + `pdf-base7d` baseline worktree — retire after the deletion
   pass (owner), exporting the baseline logs first.
7. The origin tail of `feat/parity-glm` (`26c9a415`, 176 behind local) never advanced — moot
   after the line tag exists; do NOT push it (the survivor supersedes it).
8. This plan's numbers were measured 2026-09-29 against the requested 2026-09-25 filename;
   every number is re-derived by the phase-0 sweep before anything executes.
