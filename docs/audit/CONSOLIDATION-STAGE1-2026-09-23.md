# Consolidation Stage-1 Execution Report (ADDITIVE-ONLY) — 2026-09-23

**Lane:** consolidation-executor, stage 1 (zero-loss archive bundle, archive tags, fold proofs, main-divergence re-verification, stage-2 draft). No deletion, no push, no ref move on origin, no force/gc/prune — nothing was removed; every operation in this stage is additive.
**Executed on:** shared repo behind `C:\Users\User\Projects\pdf-clean` (junction to `D:\pdf\pdf-clean`). The shared worktree stays on `feat/parity-glm`; all work used explicit ref arguments and a private scratch worktree (`.context/consexec-stage1/wt`, removed after use) on branch `feat/consolidation-stage1` (created from `feat/parity-glm` @ `26c9a4154cd7e2e9acca7fa714faaaa835bd97de`).
**Plan of record:** `docs/audit/CONSOLIDATION-PLAN-2026-09-20.md` @ `feat/consolidation-plan` = `744e9e20257ae3463f2838cfd5cc3d6158a2b5a2`.
**Line tip at execution:** `feat/parity-glm` = `origin/feat/parity-glm` = `26c9a415` ("Merge branch 'feat/modularity-moves' into feat/parity-glm") — the line has fast-forwarded from the plan's rehearsal tip `ec9f16f6`; drift measured in §4/§6.
**IMPORTANT — bundle refresh obligation:** five remediation lanes are still running and will land via a final merge batch AFTER this stage. The stage-1 bundle therefore freezes the ref space as of 2026-09-23 and MUST be refreshed (re-create + re-verify + re-hash) at consolidation completion, after the final merge batch and before any deletion pass. Recorded in §1.5 and in the stage-2 draft preconditions.

---

## 1. Task 1 — Zero-loss archive bundle

### 1.1 Command + result

```
git bundle create C:\Users\User\pdf-archive-stage1.bundle --all
git bundle verify C:\Users\User\pdf-archive-stage1.bundle
```

- `git bundle verify` exit 0: **"The bundle records a complete history."** (sha1 hash algorithm)
- **431 refs** recorded in the bundle: all local heads (126), all `refs/remotes/origin/*` (70), all `refs/tags/*` (incl. release tags v1.0.0…v1.4.0, untouched), plus the prior `refs/archive/cleanup-20260913-*` preservation refs and worktree HEADs — i.e. the entire ref space including every branch tip.
- **Archive-class coverage confirmed present in the bundle's ref list:**
  - `refs/heads/feature/editing-parity` = `97172fbe893eab534a8299ab9eb36bf505dfe0d5`
  - `refs/heads/feature/redaction-parity` = `45bf4302b9d43b8f7d866eb2ca9adf36c8004ba0`
  - `refs/heads/feature/viewing-parity` = `de1fa268bdb5a2d4f4d5148336909dab29ab20f9`
  - `refs/heads/feat/annotation-eraser` = `916a4b7d5c34d695a8531a5a4c2094b06e6f6243` (local twin) and `refs/remotes/origin/feat/annotation-eraser` = `ebd022ccb7d68136e467ee5d2f0b59b7abf60077` (origin tip)
  - plus origin + local tips of all 38 A-class, 2 B-class and 8 C-class branches.

### 1.2 Bundle identity

| Property | Value |
|---|---|
| Path | `C:\Users\User\pdf-archive-stage1.bundle` (outside the repo, on C: per the plan's D:-disk note) |
| Size | 40,097,287 bytes (38.2 MiB) |
| **SHA-256** | `e87d56da558c912f61cd8a66c48decb3967f4e4436ad15615df57e1ebb401862` |
| Hash cross-check | `sha256sum` (Git Bash) and `certutil -hashfile … SHA256` agree |
| Created | 2026-09-23 |
| Verify | exit 0, complete history, no prerequisites |

### 1.3 Restore recipe (any future repo or bare clone)

```
git clone C:\Users\User\pdf-archive-stage1.bundle restored-repo
# or, into an existing repo that has the base history:
git fetch C:\Users\User\pdf-archive-stage1.bundle 'refs/*:refs/restore/*'
```

### 1.4 Size note vs the plan's estimate

The plan estimated ~110 MB from `size-pack` (111,938 KB at rehearsal; 111.21 MiB today). The actual `--all` bundle is 38.2 MiB because `size-pack` also counts unreachable objects (rehearsal commits `3b19bdef..a82036df`, recovery refs' orphaned objects, loose garbage) that a bundle correctly omits — a bundle packs only what is reachable from refs. `git bundle verify` confirms completeness; the plan's estimate was an upper bound, not a target. No action.

### 1.5 Refresh obligation (binding for later stages)

Five remediation lanes land AFTER this bundle was cut. Before the deletion pass (stage 2/3), re-create the bundle from the then-current refs (recommended name `pdf-archive-final-<date>.bundle`), `git bundle verify` it, record its SHA-256 in the final consolidation report, and require `git bundle verify` of that final bundle as a precondition for every deletion. The stage-1 bundle remains a valid zero-loss snapshot of the 2026-09-23 ref space forever.

---

## 2. Task 2 — Archive tags (LOCAL ONLY — not pushed)

Created per the plan §6, names per the stage-1 execution order. All four are annotated tags in the local repo; **no `git push --tags` was run** (push is a later stage's decision).

| Tag (refs/tags/) | Tag object | Points at (commit) | Branch / note |
|---|---|---|---|
| `archive/editing-parity` | `357c676fb326953ed6343696f194eb43aec84a73` | `97172fbe893eab534a8299ab9eb36bf505dfe0d5` | `feature/editing-parity` tip (local = origin, checked out in `pdf-worktrees/editing`) |
| `archive/redaction-parity` | `4617f9ff3205a48a3402ea0c5da8f87e70ffec94` | `45bf4302b9d43b8f7d866eb2ca9adf36c8004ba0` | `feature/redaction-parity` LOCAL tip (pdf-redaction worktree) — **superset of origin**: `origin/feature/redaction-parity` = `1263df9a` (2026-09-07), `45bf4302` (2026-09-13, "preserve local region and overlay changes") is its child |
| `archive/viewing-parity` | `89603df262c916f9dec5b096d22c89e0cbdaf43c` | `de1fa268bdb5a2d4f4d5148336909dab29ab20f9` | `feature/viewing-parity` tip (local = origin, checked out in `pdf-worktrees/viewing`) |
| `archive/annotation-eraser` | `eda11849a82fa53150f808eb9d1bd211f3316c3c` | `ebd022ccb7d68136e467ee5d2f0b59b7abf60077` | `origin/feat/annotation-eraser` tip per plan §3 class D (local twin `feat/annotation-eraser` = `916a4b7d`, one commit off main, differs — both tips are in the bundle) |

Each tag message records: what the branch holds (per plan §3/§4.3 evidence, with the Wave-1A base `c61edbed..055592df` and per-branch tail commits), where the features live in the mainline (line re-implemented Wave-1A/1B during the parity program; eraser via `deleteObjectAt` in EditController/AnnotationLayer/engines; Night Mode + hyperlink nav grep-verified in `PdfViewerWidget.{cpp,h}` with the plan §9.4 grep-only residual), why merge was rejected, the bundle reference (`C:\Users\User\pdf-archive-stage1.bundle` + SHA-256), the restore recipe (`git clone <bundle>` → `git merge <tag>`), and the refresh-at-completion note.

**Topology findings recorded while tagging (drift vs plan §3):**
1. `feature/redaction-parity`: origin is at `1263df9a`, one commit BEHIND the local branch tip `45bf4302` (never pushed; branch checked out in the `pdf-redaction` worktree). The plan's §3 listed `45bf4302` — that is the local tip; the tag pins the superset, so both snapshots are preserved (both are in the bundle either way).
2. `feat/annotation-eraser`: local (`916a4b7d`) and origin (`ebd022cc`) tips differ; the tag pins the ORIGIN tip per the plan. The local twin is a separate one-commit-off-main branch handled by the local consolidation lane (plan §7).
3. The three `feature/*` archive branches are currently checked out in live worktrees (`pdf-worktrees/editing`, `pdf-redaction`, `pdf-worktrees/viewing`). Tags were added without touching those worktrees; no branch ref was moved.

---

## 3. Task 3 — Fold proofs (zero-loss containment at the CURRENT tip)

Proof command per branch: `git rev-list <origin-ref> ^feat/parity-glm` (empty = branch tip fully contained in the line tip `26c9a415`). Branch refs resolved to `origin/<name>` (the plan's classification scope is the 54 origin heads). **A-class verdict: 38/38 PASS, zero failures — no reclassification needed.** B-class results require one loud reclassification (§3.3).

### 3.1 A-class — 38 branches classified "ancestor of mainline tip" (plan §3)

| # | Branch (origin ref) | Tip SHA | uniq vs `feat/parity-glm` @ `26c9a415` | Verdict | Plan-tip drift |
|---|---|---|---|---|---|
| 1 | origin/feat/accessibility-p1 | `c5496ba6` | 0 | PASS | same as plan |
| 2 | origin/feat/batch-presets-p1 | `f80033a4` | 0 | PASS | same as plan |
| 3 | origin/feat/candidate-leak-fix | `5c8fd087` | 0 | PASS | same as plan |
| 4 | origin/feat/followups-2026-09-15 | `247e0c76` | 0 | PASS | same as plan |
| 5 | origin/feat/glm-comp | `585cc45d` | 0 | PASS | same as plan |
| 6 | origin/feat/glm-ocr | `585cc45d` | 0 | PASS | same as plan |
| 7 | origin/feat/l7-rotate-annot | `71891494` | 0 | PASS | same as plan |
| 8 | origin/feat/parity-glm | `26c9a415` | 0 | PASS | moved FF `b17106a3` → `26c9a415` (the line itself; expected) |
| 9 | origin/feat/parity-glm-clean | `352c9b1e` | 0 | PASS | same as plan |
| 10 | origin/feat/parity-glm-formjs | `2290ab51` | 0 | PASS | same as plan |
| 11 | origin/feat/parity-glm-gate | `7f8cf950` | 0 | PASS | same as plan |
| 12 | origin/feat/parity-glm-gateC | `74be82cf` | 0 | PASS | same as plan |
| 13 | origin/feat/parity-glm-infra | `77e3b066` | 0 | PASS | same as plan |
| 14 | origin/feat/parity-glm-integration | `2f755244` | 0 | PASS | same as plan |
| 15 | origin/feat/parity-glm-n17n18 | `819d84a5` | 0 | PASS | same as plan |
| 16 | origin/feat/parity-glm-packa | `40a38cef` | 0 | PASS | same as plan |
| 17 | origin/feat/parity-glm-packafix | `5ae41536` | 0 | PASS | same as plan |
| 18 | origin/feat/parity-glm-quick | `2b81fc28` | 0 | PASS | same as plan |
| 19 | origin/feat/parity-glm-r05 | `5995b7fa` | 0 | PASS | same as plan |
| 20 | origin/feat/parity-glm-r18f | `7d5ae676` | 0 | PASS | same as plan |
| 21 | origin/feat/parity-glm-r22 | `7ceeba82` | 0 | PASS | same as plan |
| 22 | origin/feat/parity-glm-resid2 | `9129788f` | 0 | PASS | same as plan |
| 23 | origin/feat/parity-glm-review | `cd01e892` | 0 | PASS | same as plan |
| 24 | origin/feat/parity-glm-sec | `e36e714c` | 0 | PASS | same as plan |
| 25 | origin/feat/parity-glm-t2 | `21673bc0` | 0 | PASS | same as plan |
| 26 | origin/feat/printable-summaries | `b06b2f07` | 0 | PASS | same as plan |
| 27 | origin/feat/r24-policy | `335d1d3a` | 0 | PASS | same as plan |
| 28 | origin/feat/r24-wiring | `b8909dcb` | 0 | PASS | same as plan |
| 29 | origin/feat/send-for-signing-p1 | `f621416d` | 0 | PASS | same as plan |
| 30 | origin/feat/sep13-fixes | `e8b9a19e` | 0 | PASS | same as plan |
| 31 | origin/feat/sep13-leads | `b0fd8296` | 0 | PASS | same as plan |
| 32 | origin/feat/sep13-residual | `c7e9ecfe` | 0 | PASS | same as plan |
| 33 | origin/feat/sweep-legacy-fix | `a73419ed` | 0 | PASS | same as plan |
| 34 | origin/feat/sweep-w1-adversary | `8bcde898` | 0 | PASS | same as plan |
| 35 | origin/feat/sweep-w1-fixes | `e8e715f2` | 0 | PASS | same as plan |
| 36 | origin/feat/sweep-w1-security | `f6d46dd9` | 0 | PASS | same as plan |
| 37 | origin/feat/sweep-w2-testing | `b17106a3` | 0 | PASS | same as plan |
| 38 | origin/feat/sweep-w2-verify-b | `84a19f9e` | 0 | PASS | same as plan |

### 3.2 B-class — 2 branches classified "rides in via the `main` merge" (plan §1 class B)

These are NOT ancestors of the line, so `rev-list <b> ^feat/parity-glm` is non-empty **by the plan's own design**; the zero-loss proof that matters for them is containment via `main` (which stage 2 merges into the line):

| Branch (origin ref) | Tip SHA | uniq vs line | uniq vs `main` @ `2b715f47` | Verdict |
|---|---|---|---|---|
| origin/audit-remediation | `fd74f5dc` | 343 | **0** — fully contained in main (`fd74f5dc` is an ancestor of main) | RIDE-MAIN CONFIRMED — will be contained at T by the main merge |
| origin/claude/modest-mccarthy-riuo2o | `a8f50a44` | 224 | **1** — `a8f50a44` itself is NOT in main | **REAL-MERGE RECLASSIFICATION — see §3.3 (LOUD)** |

### 3.3 LOUD FINDING — plan's class-B claim does not hold for `claude/modest-mccarthy-riuo2o`

**`git rev-list origin/claude/modest-mccarthy-riuo2o ^main` = 1, not 0.** The plan (§1 class B, §8) asserts this branch is "covered automatically by merging main" and its post-consolidation proof `git rev-list a8f50a44 ^T → 0` — that proof would FAIL if stage 2 merges only `main`. The stray commit is the branch's own tip:

- `a8f50a44` — "docs(audit): add 2026-06-16 remediation prompts (AR-PROMPT-1..12)" — docs-only: adds `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` (879 lines) + 1 line in `docs/planning/README.md`.

**Disposition (per the stage order's rule "any branch that FAILS the proof is reclassified REAL-MERGE — flag loudly, do not fold"):** `origin/claude/modest-mccarthy-riuo2o` is reclassified **REAL-MERGE (docs-only, 1 commit)** and added to the stage-2 merge list (order: after the C-class merges, before/with `main`). It must NOT be folded until `git rev-list a8f50a44 ^T` = 0. The stage-2 draft (§5) implements this.

---

## 4. Task 4 — `main` divergence re-verification at the current tip

Re-run at `feat/parity-glm` = `26c9a415` (the line has moved 7+ commits past the plan's rehearsal base `ec9f16f6`; `main` = `2b715f47` and `origin/main` = `d03d6e94` are UNMOVED since the plan, `origin/main..main` = 2 as rehearsed):

```
git cherry feat/parity-glm main
```

| Metric | Plan rehearsal (@ `a82036df`, line `ec9f16f6`) | This stage (@ line `26c9a415`) | Drift |
|---|---|---|---|
| main-only commits listed | 401 `-` / 14 `+` "of 430" | **415 total = 401 `-` / 14 `+`** | **The plan's "of 430" was internally inconsistent: 401+14 = 415, and 415 is what the command actually returns today.** Core claim unaffected |
| patch-id-equivalent (`-`) | 401 | **401** | none — exact match |
| real unique (`+`) | 14 | **14** | none — exact match |
| The 14 `+` SHAs | listed in plan §4.1 | `06cb0bd0` `4488f7f6` `2026e1ab` `1b055370` `bd522c75` `8a6d25a4` `022dadc1` `a4466674` `cafe4e65` `793beb71` `5ca7e36e` `1abf3008` `2e51b2c4` `2b715f47` | **identical list, SHA for SHA** |

**Verdict: the plan's 401-equivalent / 14-real claim REPRODUCES EXACTLY at the current tip** — the line's advance since the rehearsal (`ec9f16f6` → `26c9a415`) absorbed none of main's 14 unique patches and no new patch-equivalences appeared. The content verdict in plan §4.1 (take the docs/governance/release-eng uniques via the merge; code superseded; all 401 equivalents remain reachable via main's parent history forever) stands as written.

**Rehearsal drift that stage 2 must absorb (measured this stage):**
1. `feat/modularity-moves` (`3c411cc8`) and its parent `feat/sweep-w3-arch` (`46cae7ba`) are **already merged into the line** (uniq 0 each; line tip `26c9a415` is that merge). Phase-1 of the plan's sequence shrinks from 8 refs to **7** (`soak-48h` 1, `soak-verdict` 1, `sweep-w3-archaeo` 2, `sweep-w3-devops` 1, `sweep-w3-perf` 4, `sweep-w3-research` 5, `sweep-quality-new` 3 unique commits still outstanding), plus the reclassified `claude/…` docs tip.
2. The main-merge conflict forecast (~130 paths) was measured at `ec9f16f6`; from `26c9a415` the line side carries additional ledger/docs rows, so the conflict list must be re-measured at merge time (the §5.2 policy classes R1–R4 are unchanged).
3. Five remediation lanes will move the line further before stage 2 runs — the stage-2 draft re-runs the fold-proof sweep and the cherry count at execution time as gating preconditions, and requires a fresh final bundle (§1.5).



