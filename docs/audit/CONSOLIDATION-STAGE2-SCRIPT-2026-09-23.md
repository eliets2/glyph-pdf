# Consolidation Stage-2 Script (DRAFT — NOT EXECUTED) — 2026-09-23

> **STATUS: DRAFT-NOT-EXECUTED.** This document and its companion script
> `docs/audit/consolidation-stage2-draft.sh` were written by the stage-1
> (additive-only) executor lane as the runbook for the POST-merge-batch
> stages. **Nothing in them has been executed.** They must not be run until
> the five in-flight remediation lanes have landed via the final merge batch,
> the plan §2 naming choice is made, and a human release owner has reviewed
> every gate below.

**Inputs this draft is built on (all re-verified at the current tip by stage 1, see `CONSOLIDATION-STAGE1-2026-09-23.md`):**
- Plan of record: `docs/audit/CONSOLIDATION-PLAN-2026-09-20.md` @ `feat/consolidation-plan` (`744e9e20`).
- Stage-1 archive bundle: `C:\Users\User\pdf-archive-stage1.bundle`, SHA-256 `e87d56da558c912f61cd8a66c48decb3967f4e4436ad15615df57e1ebb401862`, `git bundle verify` OK (complete history, 431 refs).
- Stage-1 archive tags (local, not yet pushed): `archive/editing-parity` → `97172fbe`, `archive/redaction-parity` → `45bf4302`, `archive/viewing-parity` → `de1fa268`, `archive/annotation-eraser` → `ebd022cc`.
- Fold proofs at `26c9a415`: 38/38 A-class empty; `audit-remediation` contained in `main` (0 uniq vs main); **`claude/modest-mccarthy-riuo2o` RECLASSIFIED REAL-MERGE** (1 docs commit `a8f50a44` not in main).
- Main divergence: 401 patch-id-equivalent / 14 real (SHAs match plan §4.1 exactly); `main` = `2b715f47` = `origin/main` + 2, unmoved.

## Updates vs the plan's §5 sequence (drift absorbed by this draft)

1. **Phase 1 shrinks from 8 to 8-with-different-members**: `feat/modularity-moves` and `feat/sweep-w3-arch` are already contained in the line (`26c9a415` is their merge), so the outstanding real merges are the 7 remaining C-class refs (`soak-48h`, `soak-verdict`, `sweep-w3-archaeo`, `sweep-w3-devops`, `sweep-w3-perf`, `sweep-w3-research`, `sweep-quality-new`) **plus the reclassified `claude/modest-mccarthy-riuo2o` docs tip** (plan §1 class B claim corrected by stage-1 §3.3).
2. **A final bundle refresh is a hard precondition** (the five remediation lanes land after the stage-1 bundle): create `pdf-archive-final-<date>.bundle --all`, `git bundle verify`, record SHA-256; every deletion re-verifies the bundle and its hash before pushing the delete.
3. **Everything is proof-gated fail-closed**: each branch's tip SHA is pinned, `git rev-list <pinned-tip> ^T` must be 0, else the run aborts and the branch is treated as REAL-MERGE material instead of foldable.
4. **Conflict forecast must be re-measured** at the phase-2 merge (the ~130-path forecast was measured at `ec9f16f6`; the line has advanced). Policy classes R1–R4 are unchanged (plan §5.2): code → line wins (keep `tests/mocks/MockFormManager.h` deleted); docs/governance/memory → main additive (the line's 52 deletions stay deleted; paths remain readable at `2b715f47` forever); release engineering → adopt main's v1.4.0 state **with release-owner sign-off**; workflows → union (INF04 `feat/**` filters + release workflow).
5. **Gate before any push**: the line's standard full serial ctest (and the R22 Linux gate if available) must be green at T. No consolidation commit bypasses the suite the lanes use for merges.

## Phase sequence (as encoded in `consolidation-stage2-draft.sh`)

| Phase | What | Non-negotiable gates |
|---|---|---|
| 0 | Preconditions: freeze announced; final bundle created + verified + hashed; stage-1 proof sweep re-run at execution time; `main`-no-rewrite check | any A-class branch non-contained ⇒ reclassify, abort; bundle verify must exit 0 |
| 1 | Real merges: 7 C-class + claude docs tip into the line, rehearsed order; ledger conflicts resolve by ROW-UNION (never `-X ours`) | any unresolved conflict stops the run for manual resolution |
| 2 | The `main` merge, per R1–R4 | release-owner sign-off for R3; gate after resolution |
| 3 | Suite gate; FF-only pushes: `T:main`, `T:<survivor>`; push the four archive tags | no force of any kind; FF checked |
| 4 | Deletion pass in plan §5.3 order (A → B → real-merge inputs → archived D → folded line name): pin tip SHA → `rev-list <tip> ^T` = 0 → bundle verify + hash check → `git push origin --delete` | fail-closed on any nonzero proof; refuses to touch `main` or the survivor |
| 5 | End-state verification: `git ls-remote --heads origin` = exactly `main` + survivor; proofs logged to `CONSOLIDATION-STAGE2-PROOFS-<date>.txt` | reviewer one-liner from plan §8 |

Explicitly out of scope (plan §7 / §9): the local-only 16-branch set, the gemini subagent worktrees, `feat/soak-48h-resume` (until the R25 re-soak concludes) and `feat/sweep-w3-ui` (until that lane lands), and any `gc`/`prune` (standing order: never).

## Open decisions this draft does NOT make for you

1. **Surviving integration branch name** (plan §2): `feat/parity-glm-integration` (recommended) vs `feat/parity-glm`. The script refuses to run until `SURVIVOR` is set.
2. R3 release-engineering adoption (main's v1.4.0 version/packaging vs line's 1.3.2.3) — release owner's sign-off required at phase 2.
3. Whether the R25 re-soak at T runs before or after the deletion pass (plan §9.1 permits consolidation first; re-soak at T afterwards).
