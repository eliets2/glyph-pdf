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
