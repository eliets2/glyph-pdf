# Deletion audit summary — GlyphPDF, 2026-09-13

Lane: repository-forensics (code-archaeologist + devops-engineer protocols, adapted).
Scope: GLM-FIX-FINDINGS handoff §4 — audit everything already deleted; prove or recover honestly.
Constraints honored: read-only git throughout (no gc/prune/reflog-expiry/reset/branch-deletion/stash-drop/worktree-remove); no writes to any repo or worktree except this `.context/deletion-audit/` directory.
Companion ledger: `DELETED-BRANCH-RECOVERY-LEDGER-2026-09-13.csv` (same directory). Verification scripts kept alongside (`verify_preservation.py`, `verify_r15_r18.py`).

## Verdict at a glance

| Disposition | Count | Items |
|---|---|---|
| proven-safe (committed history) | 6 | feat/sig-p1 branch; staging-chain content (temp/stage-push); remote stage-branch content; 2 historical main resets; superseded-commit class; transient-stash class |
| recovered (objects located, content already preserved) | 1 chain | temp/stage-push: 15 original commit objects found unreachable in the local object store (12 + 3 superseded attempt) |
| unknown — insufficient pre-deletion evidence | 5 | untracked/ignored files at pdf-stage removal; untracked/ignored files at pdf-sig removal; exact former remote SHA + time of origin/feat/parity-glm-stage; exact deletion times/operators for the 4 git deletions; non-regenerable byproducts inside deleted build dirs |
| confirmed loss | 0 | none found |

## 1. The deletions, reconstructed from evidence

### 1.1 `feat/sig-p1` (branch) + `C:\Users\User\Projects\pdf-sig` (worktree) — deleted 2026-09-09 (attested)

- Pre-deletion evidence is STRONG: merge commit `c2e127b6df38678c08836f93a67e1967323269da` ("merge: feat/sig-p1 (§9.7 P1 trio)", 2026-09-07 00:26:16 +0300, Elie Tanios) names the branch; its second parent is the branch tip `22a7b667...`; `work/sep7-log.txt` enumerates the trio; `C:\Users\User\Projects\pdf-sig-cmake.txt` (139,877 bytes, mtime 2026-09-07 00:25) is a physical trace the worktree existed.
- Committed history PROVEN SAFE: `git merge-base --is-ancestor 22a7b667 feat/parity-glm` → YES. All three commits (`be47cce` initials variant, `706a60c` session signature cache, `22a7b66` SignOutcome surfacing) are ancestors of `feat/parity-glm` (and sealed in the closure bundle).
- Deletion time: not provable (branch reflog removed with the branch). Bounded window: 2026-09-07 00:26 → 2026-09-13 10:51 (initial inventory already lacks it). Orchestrator attests 2026-09-09.
- Untracked/dirty files in the worktree at removal: no pre-deletion manifest exists (removal predates the 2026-09-13 preservation protocol; `uncommitted-state-backup.zip` was sealed 2026-09-13 11:01 and cannot cover it) → **unknown**.

### 1.2 `temp/stage-push` (branch) + `C:\Users\User\Projects\pdf-stage` (worktree) — deleted 2026-09-09 (attested)

No surviving artifact carries the literal ref name. However, a read-only `git fsck --unreachable --no-reflogs` pass recovered exactly the described object: a 12-commit cherry-pick chain rooted at `e99c733` (then tip of feat/parity-glm), all committer-stamped **2026-09-09 16:08 +0300** by Elie Tanios, tip `d367aca1b02f166f198e74f3c769974f5bbd7120`, plus a 3-commit superseded first attempt (`8e55018`→`7e3ccc0`). Two chain commits say "[workflow hunks parked pending push scope]" — a push-scope staging pattern.

- Content PROVEN SAFE, three ways:
  1. 10/12 chain commits have **patch-id-identical** reachable twins on feat/parity-glm (`3b3ec1d`, `bc14f3c`, `e006a52`, `f96efe7`, `48a9552`, `955d61d`, `269a73c`, `3166dc7`, `a5840dc`, `f443f59`), as do all 3 superseded-attempt commits (`9024124`, `699a89a`, `b2d9906`).
  2. The two parked variants differ from the reachable full commits **only in `.github/workflows/*.yml`** (verified by path-scoped diff) — the parked hunks exist on reachable history (INF04 was workflow-only, hence absent from the chain entirely).
  3. Nothing in the chain is reachable from any ref — matching a deleted branch, and the deletion window matches the attested date.
- The original SHAs survive **only as unreachable objects in the local object store**. They are in NO bundle (all bundles were sealed 2026-09-13, after the deletion, and carry ref-reachable history only). Until a recovery ref exists, they are at theoretical GC risk — nobody may run gc/prune.
- RECOMMENDED (could not execute — this lane is read-only): `git branch recovery/temp-stage-push-20260909 d367aca1b02f166f198e74f3c769974f5bbd7120` (plus optionally `recovery/temp-stage-push-20260909-attempt1 7e3ccc0`).
- Worktree untracked files at removal: **unknown** (same evidence situation as pdf-sig).

### 1.3 `origin/feat/parity-glm-stage` (remote branch deletion, authorized)

- Current remote state verified live: `git ls-remote origin` → 19 heads, no `*stage*`.
- The local repo never had a remote-tracking reflog for it (pushed, never fetched) → former remote SHA and deletion timestamp are **unknown**; the GitHub-side audit/push log would resolve both.
- Push hygiene verified: all 125 `refs/remotes/origin/feat/parity-glm` tracking-reflog updates are fast-forward (0 non-FF) — pushes were additive; no force-rewrites of shared remote history.

### 1.4 Build-directory removals — correction to the baseline

The baseline called these "gitignored artifacts". They were **untracked, not ignored**: `.gitignore:2` covers `build/` only; `gate-2026-09-09/status.txt` shows `?? build-rel/`, `?? build-wt/` as untracked. They were never git state; they are regenerable CMake outputs and vendor downloads re-fetchable via the Q02 bootstrap (`e006a52`). Disposition: proven-safe (regenerable), with a residual minor unknown for any non-regenerable byproducts that may have lived inside them.

## 2. Additional findings the orchestrator's reconstruction missed

1. **Two historical main resets** (reflog-documented, NOT from the audited session): 2026-09-02 17:55 discarded `0e30e4a` (v1.4.0 bump; superseded — `1abf300` on main already lacks CLAUDE.md/SECURITY.md); 2026-09-10 14:08 discarded `c0ae827` (E-1 cherry-pick; identical content reachable as `2c5a0d6`). Both objects survive unreachable.
2. **Superseded-commit class**: ~30 non-stash unreachable commits are amend/reset/cherry-pick iterations (sig-p1 pre-merge drafts `8dae489`/`6b3d4ea`, G20 `66a7481`→`29b2b33`, N1-CSV, form-JS goldens, R18(b), N06, SEP13:4 …). Sampled twins verified reachable; all classified superseded, objects recoverable.
3. **`feat/parity-glm-packafix` looked like a deleted branch** (reflog file with no live ref in the 14:49 state-check) — it is a NEW branch created 2026-09-13 18:45 +0300 from origin/feat/parity-glm, i.e. after the state check. Not a deletion; an active lane.
4. **`C:\Users\User\Projects\pdf-clean-notess`** exists as a non-worktree directory (leftover copy, not registered, not a deletion concern — flagged for the cleanup lane's awareness).
5. The stash reflog plus fsck show **transient stash push/pop cycles** (revert-verification, documented inside commit messages) and post-closure active-lane stashes (`r15-src-wip`, sep13 WIPs, t1-*-fix-tmp). All six surviving stashes are sealed under `refs/archive/cleanup-20260913-closure/stash-entries/0-5` with matching immutable SHAs; the transient ones' untracked-file parent commits survive unreachable.

## 3. Preservation-evidence verification (pre-cleanup state, 2026-09-13)

Method: recorded SHA-256s are raw disk bytes (per `commit_selected.py`: hashed at prepare, re-verified at commit and after commit). Blobs are post-clean-filter (LF). Comparison: exact, then CRLF-normalized, then raw `selected-files/` backup vs recorded hash.

- 62/62 paths across the six preservation commits (`2b715f4` findings, `7e32093` packa, `2a82738` quick, `d7a8ca0` ast, `5a018ae` djot, `45bf430` redaction): **20 byte-exact; 42 differ only by line endings with the raw backups byte-exact vs recorded; 0 real mismatches.**
- R15 owner commit `b9f417c`: 4/4 paths present (CMakeLists.txt and TestUiAccessibility.cpp byte-exact; PdfViewerWidget.cpp/.h EOL-variant — size deltas +1710/+347 bytes, consistent with CRLF→LF, but the 13:39 raw disk bytes were not independently archived → EOL-level certainty only).
- R18 owner commit `d71a4c2`: 12/12 paths present.
- Closure inventory: 17 worktrees, zero staged/changed tracked files anywhere except the five legacy Gemini subagent indexes (703 staged/44621 index entries — untouched, exactly as handoff §6 warns); 6 stashes.

## 4. Bundle verification (read-only)

| Bundle | `git bundle verify` | SHA-256 |
|---|---|---|
| glyph-pdf-closure.bundle | OK — "complete history" (99 sealed refs) | `1c63ed3e6024d5bfc1153073cfd72401d9f1a9f81e7b131fc37ca3940a1cc596` — **matches expected** |
| glyph-pdf-all-refs.bundle | OK — complete history (219 refs) | (recorded `83cc4255…`; not re-hashed, verify only) |
| djot-7708fcd5.bundle | OK — complete history (7 refs) | (recorded `a2a9236e…`) |
| djot-ca2c3f27.bundle | OK — complete history (5 refs) | (recorded `2693d479…`) |

Important limitation: bundles contain **ref-reachable history only**. The temp/stage-push chain is NOT in any bundle; its only copy is the local object store's unreachable objects.

## 5. Periods with no deletion observed (never certified from absence)

- 2026-09-13 10:51 → 13:57 (preservation window): 66 local / 19 remote / 17 worktrees / 6 stashes constant; `missing_initial_refs: []`; only additions and forward moves.
- 2026-09-13 14:49 UTC state check → this audit's live re-check (17 worktrees, 6 stashes): no baseline ref or registered worktree missing. Exact live diff vs the state-check ref set: local added `feat/parity-glm-packafix` (new lane, created 18:45 +0300 from origin/feat/parity-glm); remote added `refs/remotes/origin/HEAD` (symbolic remote-head, not a branch); **removed: none**. Delete-and-recreate and server-side events remain excluded per the state-check's own limitation note.
- 2026-09-10 → 2026-09-13 10:50: **no surviving inventory covers this window** — no certification either way. The fsck pass found no unexplained unreachable chains, which is consistent with no deletion but is not proof.

## 6. What remains blocked

Per handoff §4.6, further related deletion stays blocked while unknowns exist. Concrete blockers:

1. Untracked/ignored files in deleted `pdf-stage` and `pdf-sig` worktrees — would be resolved by the orchestrator session's command transcript for the removals, Windows Recycle Bin contents, or editor/local backups.
2. `temp/stage-push` original objects are unreachable-only: create the recovery ref (command in §1.2) before ANY future gc/prune conversation; until then gc/prune remain forbidden (already honored).
3. Remote `feat/parity-glm-stage` former SHA + deletion timestamp — resolvable only from GitHub audit/push records.
4. R15's two EOL-variant files' exact raw-disk bytes — immaterial to content, noted for completeness.

No confirmed loss was found. This report does not certify that nothing was lost — it certifies exactly what the evidence proves, and lists what it cannot.
