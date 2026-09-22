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

