# CONSOLIDATION CRITIQUE — action-plan cycle 2026-09-25 (written 2026-09-29)

**Lane:** consolidation-critic (READ-ONLY on refs; one fetch; no push, no delete, no ref moves).
**Subject under critique:** `docs/audit/CONSOLIDATION-ACTION-PLAN-2026-09-25.md` (branch
`feat/consolidation-action-plan`). **That branch and its handoff had NOT landed when this critique
was finalized** (polled repeatedly through 2026-09-29 ~03:00; newest commit on any ref is the FIXALL
handoff `12da4e2f`, 2026-09-29 01:43; `pdf-clean/.context/consolidation-wip.md` still holds the
2026-09-20 analyst FINAL). This document therefore delivers, in order: (1) the verified ground truth
any plan must match — measured fresh, with commands; (2) the critique of the plan lineage the action
plan necessarily inherits (09-20 plan → stage-1 → stage-2 draft → line-recon → 09-24/09-25 ledgers);
(3) binding amendments; (4) an arrival checklist to run against the plan the moment it lands.

**Verification basis:** every number below was measured in this session at the shared repo behind
`D:/pdf/pdf` (read-only). Raw census: `.context/conscritic-scratch/census-local.txt`,
`census-remote.txt`, `census-local-vs-pg.txt` (this worktree). Handoff:
`.context/conscritic-wip.md`.

---

## 0. Verdict at a glance

The endgame is **not** the 09-20 plan's merge-based consolidation anymore, and it is **not** safe to
execute today. Three blockers:

- **F1 (BLOCKER — lost-work):** the only existing archive bundle is `C:\Users\User\pdf-archive-stage1.bundle`
  (2026-09-23 01:58, sha256 `e87d56da…401862`, verified intact today, 431 heads). Roughly **25 branch
  tips moved after it was cut**, the entire fixall era (7 `feat/fixall-*` branches + `fix/p0-blank-viewer` +
  `feat/final-pgr-closers`) was **never pushed to origin** (origin has zero `fixall-*` heads), and the
  `archive/branch/*` tag set pins **stale** tips (`archive/branch/feat/parity-glm` = `26c9a415`, while the
  local branch is `195e4309` — **585 commits ahead, on no other ref, in no bundle, on no tag**). Any
  deletion executed before a fresh `--all` bundle + tip-accurate tags is unrecoverable.
- **F2 (BLOCKER — broken proofs):** the surviving line is `review/consolidated-parity` (PR #2), a
  **cherry-pick-built linear line on `main`**. Against it, **150 of 158 local branches and 86 of 89
  remote branches have >0 commits not in the PR by ancestry** (feat/parity-glm 1023, feat/consolidated
  1012, consolidate/all 977). The 09-20 plan's fold proof (`git rev-list <b> ^T` = 0) **cannot pass for
  any pick-built line** and would fail-closed forever — or get quietly waived. The proof protocol must
  be per-class (see A3), or the plan is wrong on every page it reuses the old proof.
- **F3 (BLOCKER — the "0 unexplained" claim is already false):** `5461b72d` on `feat/fixall-redaction`
  ("fix(images): N1 — image edits address ONE placement by (name, occurrence)", committed 2026-09-29
  01:24 — **19 minutes before** the "STATUS: FINAL" handoff) is a **parallel N1 implementation,
  patch-unique vs the PR, un-dispositioned by `CONSOLIDATION-LEDGER-2026-09-25.md` §5 and by the
  handoff** (which records only `d63ed76e` as the N1 source, folded as `2ead0b17`). It is not pushed.
  Either it is folded/superseded with a written content diff vs `2ead0b17`, or the accounting claim
  must be downgraded.

High-severity: three divergent evidence ledgers (F4), the asymmetric purge boundary with 202
purge-carrying refs (F5), a dirty `main` worktree (F6), and staged private signing keys in five
antigravity worktrees (F7). All are survivable if the amendments below are adopted before execution.

---

## 1. Ground truth measured 2026-09-29 (the plan must match this, not 09-20 or 09-23)

### 1.1 Ref space

158 local branches, 89 remote heads (+ `origin/HEAD`), 102 tags (88 lightweight + 14 annotated),
7 stashes, 22 worktrees. Origin = `https://github.com/eliets2/glyph-pdf.git`.

### 1.2 The five lines (tips, divergence vs the PR line `review/consolidated-parity` @ `12da4e2f`, local = origin there)

| Line | Tip | commits not in PR (ancestry) | patch-unique vs PR (non-merge) | tree delta vs PR | CLAUDE.md/SECURITY.md at tip |
|---|---|---|---|---|---|
| review/consolidated-parity (PR #2, survivor) | `12da4e2f` (pushed, in sync) | — | — | — | 0 (purged) |
| main | `2b715f47` (= origin/main + 2; **fully contained in PR**, incl. the 14 "real" commits via cp ancestry) | **0** | 0 | 0 files | 0 |
| feat/parity-glm (September real history) | `195e4309` local; origin = `26c9a415` (**local +585, unpushed, unbundled, untagged**) | **1023** | 907 | 222 files, +23,809/−888 (the −888 is pg-side content the PR lacks; mostly stale ledger/review-doc revisions — unproven per file) | 0 |
| consolidate/all | `95dccb23` (reachable from origin only via `feat/consolidated`'s history) | 977 | 501 | 250 files | **2 — the ONLY line still carrying the purge files** |
| feat/consolidated (the merge-based reconciliation) | `eb0efa21` | 1012 | — | 222 files, +27,866/−901 | 0 (purge adopted in M2/R4-1) |

The PR's ledger is not the pg ledger: `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` carries **438 table rows
(276 `implemented-awaiting-review`)** on the PR vs **392 (265 awaiting)** on pg and **358 (244)** on
consolidate/all. No union proof exists in either direction.

### 1.3 Every-branch containment census (the plan's fold list must cover ALL of these)

Method: `git rev-list --count review/consolidated-parity..<ref>` for every ref (full dumps in scratch).
Result: **150/158 local and 86/89 remote are non-contained by ancestry** — expected, because the PR
carries content by cherry-pick, not history. Classes actually in play:

- **Contained (`0`)**: main, origin/main, and 8/8 local + 3/89 remote. Nothing to do.
- **Pick-built September lanes (≈100 local + ≈70 remote)**: divergence 874–1023 by ancestry;
  content transferred via cp's squash + integrator picks; dispositions live in the 09-24 ledger §B2
  (14 branches) + 09-25 ledger §1/§5 + handoff §3. Their zero-loss proof is the LEDGER, not ancestry.
  **Hole:** ledgers were never proven to be row-supersets of each other (F4), and pg-side ledger rows
  that exist only on pg (e.g. lane sections landed 09-23/24 on `feat/parity-glm` after the PR's
  ledger froze) have no explicit PR-twin row in several cases.
- **Small-divergence branches (1–15 commits not in PR)** — each needs an explicit row in the plan:
  `feature/m4-djot-foundation` (15), `feat/pr-review-fixes` (14 — 8 accounted + 2 superseded + 2
  conflict-resolved picks), `feat/fixall-forms` (11 — all SUPERSEDED), `feat/residual-exec` (8 —
  7 landed/superseded; `9b2b2727` PORTED per handoff §3), `feat/fixall-ci2` (8; `590c6c27` =
  lane-side docs checkpoint, deliberately unpicked), `feat/batch-presets-p2` (8), `feat/pgr-c4` (7),
  `feat/fixall-images` (7), `feat/formjs-review` (6), `subagent-AST-…-54b52cfc` (5), 
  `feat/final-pgr-closers` (5), `test/view-parity-baseline` (4 — **owner out-of-scope 09-28**),
  `subagent-Vendoring-…-ca2c3f27` (4), `feat/pgr-d2` (4 — PGR-42…45 **never picked**; owner item),
  `feat/pgr-c2` (4), `fix/p0-blank-viewer` (3 — FOLDED addendum), `feat/pgr40-quickjs-bump` (3),
  `feat/pgr-critical-fixes`/`feat/pgr-c3` (3 each), `feat/fixall-tagging2`/`fixall-redaction`/
  `fixall-forms2` (3 each; redaction's `5461b72d` = **F3**), `subagent-…-7708fcd5`/`-a1982f71` (2),
  `feature/m4-security`/`m4-edge` (2), `feat/fixall-inv1` (1 — picked, patch-equivalent).
- **July-era Wave 1A/1B/2B (archive-only, per 09-20 §4.3 + line-recon §1)**: editing-parity 35,
  viewing-parity 35, redaction-parity 32 (local superset `45bf4302` tagged), security-parity 31
  (local-only), ocr-parity = elevation-wave1a 30 (local-only, shared tip `055592df`),
  accessibility-parity (0, absorbed, tip `84445698` local-only).
- **Recovery/odd**: `recovery/temp-stage-push-20260909` (12, dispositioned 09-24 §B2.7),
  `ar/prompt-1` (5), `feat/ocr-verify-finereader` (20, local-only, 09-20 §7), `feat/erase-ox-auto` (1),
  `feat/regex-find-replace` = `backup/regex-verified-a39356e` (1), `feat/regex-ox-auto` (1),
  `feat/soak-48h-resume` (1, R25 re-soak pending), `feat/line-reconciliation` (1),
  `feat/annotation-eraser` local twin (1), `reconexec/snapshot-redaction-gaps-dirty` (1).

### 1.4 Safety-net inventory (rollback inputs)

- Bundle: **only** `pdf-archive-stage1.bundle` (2026-09-23 01:58). Hash re-verified today. It predates:
  `feat/consolidation-stage1` itself (02:18), gsd tail, line-reconciliation, ux-integration-fixes2 tip,
  `feat/consolidated`, accessibility-p2 tip, pg +585, all pgr-* tips (09-24), residual-exec (09-24),
  batch-presets-p2 tip (09-24), pr-review-fixes tip (09-25), p0-blank-viewer (09-25),
  view-parity-baseline (09-28), final-pgr-closers (09-28), and the whole fixall era (09-25…29).
- Tags: `archive/branch/*` (≈80) pin 09-23-era tips; `archive/{editing,redaction,viewing}-parity` +
  `archive/annotation-eraser` pin July tips correctly; `archive/stash-0..6` cover all 7 stashes;
  `archive/pr-head-before-picks`, `archive/squash-v1-e6872ed2`, `archive/docs-v1-8017c525` exist.
  **NO tag at tip for**: `feat/parity-glm 195e4309`, `feat/consolidated eb0efa21`,
  `feat/unreviewed-map 0bcbd6bf`, `feat/consolidation-stage1 8507573e`, `feat/sweep-w2-gsd 125ae929`,
  `feat/accessibility-p2 4e70217e`, every `feat/fixall-*`, `feat/pr-review-fixes a25c37b7`,
  `feat/batch-presets-p2 ec22eeed`, `feat/final-pgr-closers 68bc917e`, `fix/p0-blank-viewer 86f2cf34`,
  `feat/pgr-d2 958bd7c0`, `feat/ocr-verify-finereader f5b59e66`, `feat/residual-exec 5d74c348`,
  `feat/line-reconciliation 3811cc6a`, `test/view-parity-baseline 0948743b`, and the July siblings
  `feature/security-parity 8bb20c52`, `feature-elevation-wave1a` = `feature/ocr-parity 055592df`,
  `feature/accessibility-parity 84445698`, `feat/soak-48h-resume 0fad38c0`, `feat/sweep-w3-ui 164dabd4`,
  `feat/erase-ox-auto faa6cf10`, the regex trio.
- Stage-1's binding refresh obligation ("re-create the `--all` bundle after the final merge batch and
  before any deletion") is **6 days unmet**.

### 1.5 Unreviewed code (the "222 awaiting rows")

Source: `docs/audit/UNREVIEWED-CODE-MAP-2026-09-23.md` @ `feat/unreviewed-map` (`0bcbd6bf`; the doc
**is present on both the PR and pg**). It proves all **222** awaiting rows (254 carrying commits,
348 files) reachable from **`26c9a415`** — not from the survivor. Since then the PR's own ledger grew
to **276 awaiting rows** (fixall era added rows), and the map's §6 in-flight overlay (gsd/ux-defects/
redaction-gaps/ui-narrow-viewport) has since landed via picks. The map's method (parser + carrier
resolution, scripts in `.context/unrevmap-scratch/`) is the right instrument — it just has to be
**re-run at the final survivor tip**.

### 1.6 Worktree hazards

- `D:/pdf/pdf` (the `main` worktree) is **dirty**: tracked mod `docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`
  + 17 untracked (incl. an **uncommitted** `docs/audit/SECURITY-QUALITY-REVIEW-parity-glm.md` and
  `deploy.bak-2026-09-09/`). The doc is also **divergent between pg and the PR** (an `M` in the
  pg↔PR diff), so this dirty edit is consolidation-relevant, not noise.
- Five antigravity subagent worktrees each hold **~703 staged files** (157k+ lines: vcpkg artifacts,
  MSIs) **including `tests/fixtures/signing/ca.key`, `signer.key`, `test_signer.p12` staged for
  commit**. One careless `git commit` there poisons the history the consolidation is trying to
  canonize; a script that sweeps `git add`/`stash`/`clean` across worktrees is a hazard.
- 12 of 13 `C:/Users/User/Projects` worktrees are clean/untracked-logs-only; `pdf-redaction` has
  untracked `task.md`. `pdf-r15` (this worktree) sits on `feat/fixall-inv1` @ `898d362f` (picked).

---

## 2. Critique by checklist axis

### Axis 1 — Does the plan handle EVERY branch?
**Gap:** the 09-20 plan scoped 54 origin heads; the world is now 158 local + 89 remote, and the
post-09-24 eras (Phase B/C/D, pgr-*, fixall-*) were dispositioned across THREE ledgers + two handoffs
rather than one fold table. Any plan that reprints the 09-20 fold list misses: `feat/residual-exec`,
`feat/unreviewed-map`, `feat/ocr-verify-finereader`, the `pgr-*` family, `fix/p0-blank-viewer`,
`feat/final-pgr-closers`, `test/view-parity-baseline`, `feat/line-reconciliation`,
`reconexec/snapshot-redaction-gaps-dirty`, the seven `feat/fixall-*`, and the July local-only
siblings. **Required:** a fold table enumerating all 247 refs with class + proof + disposition
source; my census (§1.3 / scratch dumps) is the cross-check. Any ref absent from the plan's table
is a plan bug.

### Axis 2 — The THREE-LINE reality
**Gap:** prior plans treat pg↔cp↔ca↔main; the operative reality is **PR (cherry-pick line) vs
everything else**, with pg and consolidate/all and feat/consolidated holding 1000+ commits of real
history each, overlapping but NOT identical (ledgers diverge: 438 vs 392 vs 358 rows; tree deltas
222/250/222 files). A plan that says "merge T into main" (09-20 shape) is solving last cycle's
problem: **main is already contained in the PR** (0 commits, 0 patch-uniques, 0 file delta). The
remaining question is the reverse one: does the PR contain everything pg/ca/fc have? — answerable
only by tree-delta arbitration + ledger union, NOT by `git cherry` (907 and 501 patch-uniques
respectively, mostly false positives from conflict-resolved picks) and NOT by ancestry. The plan
must name the survivor (`review/consolidated-parity`), state that pg/ca/fc are **history-only
archives of content already picked**, and still disposition the −888 pg-side lines file-by-file
(closed-set rule: every pg-side hunk must trace to a named pick or a documented supersession —
the line-recon §3.2 method, inverted).

### Axis 3 — Pre-purge vs post-purge boundary
**Gap:** **202 of the 247 non-contained refs still carry `CLAUDE.md`/`SECURITY.md` at tip** (every
pre-purge branch incl. `consolidate/all`, `ar/prompt-1`, `audit-remediation`, all `feat/parity-glm-*`
era). Among the five LINES only consolidate/all still carries them. Cherry-picks are safe; **any
merge-based fold re-imports the purge files**. The plan must hard-ban merges of pre-purge refs into
the survivor, keep picks-only, and re-run the E3 gate at T
(`git log --full-history origin/main..T -- CLAUDE.md SECURITY.md` must be empty). Note
`consolidate/all`'s own disposition must be picks/archive — never a merge (its 5 patch-unique commits
landed long ago via cp/fc/PR).

### Axis 4 — Zero-loss proof for the 222 awaiting rows
**Gap:** the map proves reachability at `26c9a415` (a pg-era tip), not at the survivor; the awaiting
row count has since moved to 276 on the PR. **Required:** re-run the unreviewed-map at final T
(scripts are on `feat/unreviewed-map`), publish `UNREVIEWED-CODE-MAP-<date>-at-T.md`, and forbid the
plan from claiming any review coverage. The plan must also state the honest residual: awaiting ≠
reviewed; consolidation preserves unreviewed code, it does not bless it (the map's §5 "what review
still owes" table carries over).

### Axis 5 — Does rollback actually work?
**Gap:** the stage-1 bundle verifies and its hash still matches — but **no restore has ever been
drilled**, the bundle is 6 days stale, and ~25 tips (including the entire fixall era and pg's +585)
are in NO bundle. **Required (amendments A1/A11):** fresh final bundle + tip-accurate archive tags
BEFORE any ref move; then a restore drill into a scratch dir
(`git clone <bundle> … && git rev-parse` every pinned tip + `git cat-file -e` spot blobs) whose
transcript is committed; re-hash after every refresh; the deletion script re-verifies bundle + hash
per deletion (stage-2 draft already has this — keep it).

### Axis 6 — Conflict resolutions at FILE level?
**Mixed:** line-recon execution and the integrator runs recorded per-file resolutions exemplary
(M2's 28-file table, R1–R4 classes, PoDoFoBackend's 19 hunks). But those records apply to merges
that already happened. For the REMAINING resolution surface (pg-side −888 lines vs PR; the
`SECURITY-QUALITY-REVIEW-parity-glm.md` dirty edit; ledger row union) the plan must specify
file-level dispositions up front, not "resolve conflicts" — including the three hardest known
files: `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` (row union, never `-X ours`), `ci.yml`/
`glyphpdf-fuzz.yml` (union: INF04 filters + fuzz provisioning — both lines touched them), and
`docs/audit/SECURITY-QUALITY-REVIEW-parity-glm.md` (pg vs PR revisions + the uncommitted main-worktree
edit — three-way).

### Axis 7 — Is the execution script truly fail-closed?
**Mostly, with holes.** The stage-2 draft has the right skeleton (DRYRUN default, CONSENT token,
`delete_proven()` pin→proof→bundle-verify→delete, no force/gc/prune, refuses main/survivor). Holes
to close: (a) it pins **origin** tips — the exposure is **local** tips (fixall lanes exist only
locally); pin and prove LOCAL tips; (b) no check that the target branch is **checked out in a
worktree** (`git worktree list`) — deleting a checked-out branch fails messily and a "successful"
fold that only handled the origin ref would strand the local one; (c) no dirty-worktree abort for
`D:/pdf/pdf` (F6); (d) no antigravity-worktree quarantine note (F7); (e) the proof step must be the
A3 per-class proof, not bare `rev-list`; (f) deletion must never run on a branch whose disposition
row cites "ledger says PRESENT" without the tree-level proof attached in the same run log.

### Axis 8 — In-flight lanes
**Gap:** the fixall round is declared closed (handoff FINAL) but its own branches betray activity
minutes before the final handoff (F3's `5461b72d` at 01:24). `test/view-parity-baseline` was declared
owner-out-of-scope on 09-28 and its tip (0948743b, 09-28 18:27) has 4 commits with no tag and no
bundle. **Required:** the plan must (a) freeze lanes at NAMED tips (list: the 7 fixall branches'
current tips + view-parity-baseline + soak-48h-resume + sweep-w3-ui), (b) re-run the census at
execution time and route any moved branch back into disposition, (c) disposition `5461b72d` before
claiming 0 unexplained, (d) leave the owner-scoped branch tagged but un-deleted.

### Axis 9 — July-era branches (30–35 unique commits each)
**Disposition stands (archive-only, do NOT merge)** — 35–39 conflicts each for zero feature gain,
unique features ported (Night Mode + Eye-Care UAF `6020ea8b`, image restack/opacity/rotate
`e552df26`, letter/line spacing `3d8bca5d`, TestOfficeExport `e6497dbe`, checked-redo `ab82e81d`).
Two corrections to carry forward: (a) the 09-20 plan's grep-only residual for Night Mode/hyperlinks
is **closed by the 09-24 ledger** (ported twins + TestViewingModes 10P/0F in the p0 addendum gates) —
the plan should record the closure, not re-inherit the doubt; (b) the archive net is incomplete for
the **local-only siblings** (`security-parity`, `elevation-wave1a`/`ocr-parity`,
`accessibility-parity`, plus `soak-48h-resume`, `sweep-w3-ui`, `erase-ox-auto`, regex trio) — tag
them in A1.

### Axis 10 — `main` itself
**Resolved by containment, but the plan must say so and handle the leftovers.** main = `2b715f47` =
origin/main + 2, **0 commits not in the PR**; its 14 "real" commits ride cp ancestry (verified today:
`git cherry PR main` = 0 `+`). The "main merge" is therefore: PR #2 merges into main (or main FFs to
T) with **zero conflict forecast** — the 09-20 plan's rehearsed 130-path main merge is obsolete and
must not be re-executed. Leftovers the plan must own: (a) the dirty tracked mod in `D:/pdf/pdf`
(paragraph above), (b) the release-engineering state is already main's v1.4.0 via the M2 resolutions
(VERSION/ProductCode landed on the PR) — the 09-20 R3 release-owner sign-off is moot and should be
recorded as satisfied by `e11aa083`, (c) after T, `main := T` must be FF (verifiable:
`git merge-base --is-ancestor main T`), never a rewrite.

---

## 3. Binding amendments for CONSOLIDATION-ACTION-PLAN-2026-09-25

- **A1 (Phase 0, blocking):** Stop-the-world + fresh safety net. Create `pdf-archive-final-<date>.bundle --all`
  on C:, `git bundle verify` + sha256 into the plan doc; create `archive/final/<branch>` annotated tags
  at the CURRENT tip of every branch listed in §1.4's NO-TAG list (26+ branches); re-verify stash
  coverage (`archive/stash-0..6` exist — confirm all 7). Then the restore drill (Axis 5) with its
  transcript committed. **No ref move before A1 completes.** Cheapest second copy for the biggest
  exposure: push `feat/parity-glm` (`195e4309`) to origin (owner action, FF from `26c9a415`) — 585
  commits of unique real history currently live on one local ref.
- **A2:** Name the survivor explicitly: `review/consolidated-parity` @ `12da4e2f` (+ final docs), endgame
  `main := T` (FF). Record the 09-20 §2 naming question as superseded/decided; do not re-open it.
- **A3:** Replace the ancestry-only fold proof with per-class proofs (Axis 2): ancestry for main and
  true ancestors; ledger-row + patch-id + **tree-delta closed-set attribution** for pg/consolidate-all/
  feat-consolidated and every pick-built lane branch. Every disposition row must cite its ledger row
  or carry its own proof output in the run log.
- **A4:** Reconcile the three ledgers before freezing the accounting: prove the PR's
  `CURRENT-EVIDENCE-LEDGER` is a row-union superset of pg's and consolidate/all's (the row-union
  precedent: 09-24 B1 `0e3dcdd`), or land the missing rows on the survivor first.
- **A5:** Disposition `feat/fixall-redaction` `5461b72d` (F3) with a written content diff vs the landed
  `2ead0b17` (fold / supersede / owner-decide), then re-run the E5 `-x` accounting. Until then the
  correct claim is "0 unexplained as of ledger §5 + 1 open item", not "0 unexplained".
- **A6:** Purge boundary (Axis 3): merges of the 202 purge-carrying refs are forbidden; picks only;
  E3 re-run at T as a deletion precondition.
- **A7:** Worktree hygiene phase before any checkout/merge anywhere: disposition `D:/pdf/pdf`'s tracked
  mod (commit as docs or park on a tag with an owner note — it touches a file the pg↔PR delta already
  flags); forbid all add/commit/stash/clean in the five antigravity worktrees; surface the staged
  signing keys (`ca.key`/`signer.key`/`test_signer.p12`) to the owner as a quarantine item.
- **A8:** In-flight freeze (Axis 8): named tip pins, execution-time census re-run, `5461b72d` +
  `view-parity-baseline` explicit rows.
- **A9:** July era (Axis 9): archive-only upheld; add the missing sibling tags; record the Night Mode
  residual as closed by the 09-24 ledger.
- **A10:** Unreviewed code (Axis 4): re-run the map at T; publish the at-T map; no review-coverage claims.
- **A11:** Script hardening (Axis 7): local-tip pins, worktree-checked-out refusal, dirty-`main`-worktree
  abort, per-class proofs, per-deletion bundle re-verify + hash check, DRYRUN default, CONSENT token,
  never force/gc/prune. Reuse `consolidation-stage2-draft.sh` as the base and diff it against this list.
- **A12:** Deletion ordering: pick-built history lines (pg, ca, fc) delete LAST and only behind explicit
  owner sign-off, because after deletion the (fresh) bundle is their ONLY backup — they were never
  pushed. Origin-head deletions (the 53+ stale origin heads) may proceed first once per-class proofs
  pass; every deletion re-verifies the bundle.
- **A13:** Gates at T before `main := T`: fresh Release build + full serial ctest (the four named flakes
  re-dispositioned against T), E0–E5 re-run; PR #2 CI re-checked (Test step has not been fully green on
  any head this session — Panel race item 5 of handoff §7 is the owner blocker).
- **A14:** The plan's fold table must enumerate all 247 refs (Axis 1) — cross-check against the census
  dumps; a ref with no row is a blocker.
- **A15:** Correct inherited-claim arithmetic: "430 vs 415" (stage-1 §4), "0 unexplained" is era-scoped
  (state the scope + the A5 exception), bundle size estimate ~110 MB vs actual 38.2 MB (no action), and
  do not re-adopt the 09-20 plan's `feat/parity-glm-integration`-naming step (superseded by the PR line).

---

## 4. Arrival checklist — run this against the plan when it lands

1. Fold table covers all 247 refs? (diff against `census-local.txt` + `census-remote.txt`.)
2. Survivor named as the PR line; `main := T` FF; no re-litigation of the obsolete main merge?
3. Proof protocol per class (A3), not bare `rev-list <b> ^T`?
4. Phase 0 = final bundle + tip-accurate tags + restore drill BEFORE any ref move (A1)?
5. `5461b72d` dispositioned (A5); ledger union addressed (A4)?
6. Purge rule: picks-only for the 202 carriers; E3 at T (A6)?
7. Script: fail-closed per A11 (local tips, worktree refusal, dirty abort, consent, no force/gc)?
8. In-flight freeze with named tips + execution-time census re-run (A8)?
9. July era archive-only with sibling tags; Night-Mode residual recorded closed (A9)?
10. Unreviewed map re-run at T planned (A10)?
11. Deletion order protects unpushed history lines until owner sign-off (A12)?
12. Gates at T + CI reality stated (A13), inherited-claim corrections applied (A15)?

If any of 1–12 fails, the plan is not executable without the corresponding amendment.

---

## 5. Residuals of this critique (honest)

- The pg-side −888 lines were not arbitrated per file (222-file delta enumerated; direction and
  samples verified; the closed-set attribution is the executor's task under A3).
- The three-ledger row sets were counted, not diffed row-by-row (counts prove divergence, A4 requires
  the union proof).
- `5461b72d` vs `2ead0b17` content diff not analyzed here (both are N1 implementations on different
  bases; 82 files / +7407 vs the fixall-inv1 base — substantive).
- `origin/feat/accessibility-p2` tip parity vs local `4e70217e` not checked; CI status not re-queried
  (records say Test step red on the four named items).
- The action plan itself was never observed; if it lands and already satisfies A1–A15, this document
  stands as its verification record; if it contradicts §1's numbers, §1 wins (measured 2026-09-29).

---

## 6. Addendum — observed plan-branch facts (before the plan text exists)

- During this critique's final poll, branch **`feat/consolidation-action-plan` was created at tip
  `195e4309` — the `feat/parity-glm` tip, NOT the PR line** (no worktree holds it; no doc committed
  yet at write time). Two consequences: (a) the plan is being authored on the **pg base**, so it must
  explicitly state which line it consolidates ONTO — if it proposes pg-based merges, F2/A2/A3 apply
  with full force (the PR line already exists, is pushed, CI'd, and gates-verified; rebuilding the
  endgame on pg would discard that and re-import the purge files via pg-merges unless picks-only);
  (b) arrival checks 2, 3, 5, 12 are the first to run.
- This critique is committed on `feat/consolidation-critique` (worktree `D:/pdf/pdf-conscritic`),
  based on `review/consolidated-parity` @ `12da4e2f` — the survivor per A2 — so the file lands in the
  PR's `docs/audit/` tree; cherry-pick to the plan branch is trivial (docs-only, no conflicts).

---

## 7. Critique of the LANDED plan (`745f2649` + script `3a0c485b`, branch `feat/consolidation-action-plan`)

The plan text landed during this critique and was reviewed in full. **Verdict: sound architecture,
executable after the fixes in §7.2; none of the found errors changes the survivor decision, the fold
order, or the phase sequence.** The plan correctly supersedes the 09-20 lineage: survivor =
`review/consolidated-parity` @ `12da4e2f`, FF to main, two fold candidates, verify-pin-and-archive
for everything else, per-class proofs, phase-0 bundle + tag push.

### 7.1 Independently VERIFIED correct (measured, not trusted)

- **Survivor + FF topology** (§1.2): main = `2b715f47` has **0** commits not in the PR (ancestry and
  patch-id both) — the FF push precondition holds.
- **§4.2(a) name-set superset**: `feat/consolidated` **and** `feat/parity-glm` both have **0 files**
  the PR lacks (the plan's command only tests fc; I ran pg too — it passes).
- **§4.2(c)**: all 6 named docs blob-identical pg↔PR. **§4.2(b)** numstat walk is aimed at the true
  loss direction.
- **§3.4 fold-lane table**: every patch-unique count matches my independent census exactly
  (pgr-c4=2, fixall-redaction=1, fixall-forms=11, fixall-forms2=1, fixall-ci2=1, pr-review-fixes=4,
  batch-presets-p2=3, residual-exec=1, all others=0).
- **Phase 0** (final `--all` bundle + new archive tags + **pushing all archive tags**) resolves this
  critique's F1: the tag push gives origin a second copy of pg's unpushed tail before any deletion.
- HOLD set, 06:01-automation pause, DRYRUN/CONSENT script skeleton: all present and right.

### 7.2 Errors to fix BEFORE execution (E1–E11)

- **E1 — wrong unpushed-tail number (§1.1, §9.7):** `origin/feat/parity-glm..feat/parity-glm` =
  **585 commits (523 non-merge)**, not "176 behind". Bigger exposure than stated; the phase-0 tag
  push covers it, but the risk statement must carry the real number.
- **E2 — §4.3's proof command is false as written:** with the documented limit
  (`merge-base(f4750af5, pg)` = `f4750af5`) it returns **907 `+`**, not 0. With the intended limit
  (`feat/consolidated`) it returns **3 `+`** — `14d69ce9`, `983dd81d`, `4e70217e` — not 0. All three
  are ledger-dispositioned (09-24 ledger B1 picks-with-resolution / B2 row-union supersession), so
  the CONCLUSION stands, but the plan must state: "3 patch-uniques, each carrying a named ledger
  disposition" — not "git cherry = 0 unique".
- **E3 — §2.4's claude/modest disposition is false as proven:** the branch's
  `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` blob is `accc2cc1`; the PR's is `989c0f72`;
  delta **579+/661−** — roughly 661 lines of the branch's copy are NOT in the PR (the doc was
  rewritten on the PR after the 09-24 ledger's "content-identical" check). Downgrade to
  **ARCHIVE-CONTAINED** (`archive/branch/claude/…` tag exists; bundle covers it), or re-derive
  content containment line-by-line. The plan's "blob-verified 2026-09-29" must be corrected.
- **E4 — the plan violates its own reject-rule #14:** that rule requires tag AND bundle AND proof
  for every deleted branch, but §3.5's tag list omits all fold-lane tips (`fixall-*`, `pgr-*`,
  `pr-review-fixes`, `batch-presets-p2`, `residual-exec`, `fix/p0-blank-viewer`, …). The
  supersession-class deletions (fixall-forms' 11 never-landed reverts, fixall-redaction
  `5461b72d`, final-pgr-closers `68bc917e`) delete never-landed content on judgment alone. Add
  `archive/branch/*` tags for the fold-lane tips in phase 0 (cheap; satisfies the rule).
- **E5 — `5461b72d` supersession is asserted, not proven (the plan's own §5 item 3; I ran it):**
  vs the PR, the branch carries **62 lines the PR lacks** on the image surface alone (21 files,
  +345/−469 vs `2ead0b17`, including its own TestImageAppearance variant). SUPERSEDED is plausible
  (the PR's N1 is the gates-verified one) but requires a line-level disposition of those 62 lines
  BEFORE `feat/fixall-redaction` is deletable. Until then "0 unexplained" is provisional (this
  critique's F3, now measured).
- **E6 — script (`3a0c485b`) fail-closed holes:** (i) line 33's containment check is a **no-op**
  (`… = 0 ] && : || true` swallows both outcomes) — with the default `pin=true`, a deletion entry
  has NO containment proof at all, only tip-pin + bundle; (ii) line 39 likewise; (iii) no
  patch-level (cherry) check implemented; (iv) no checked-out-in-worktree pre-check and
  `push --delete` runs BEFORE `branch -d` — a checked-out branch yields a half-deleted state
  (origin gone, local kept); sequence worktree-release → local `-d` → remote delete; (v) line 60
  uses `git tag -f` (force-overwrites tags on re-run — against the plan's own no-force order);
  (vi) line 52's drift gate diffs against `.context/consplan/sweep-plan.txt`, which the script
  never creates and which lives in unstaged scratch — commit the baseline (e.g. hash the §3 table
  into the plan doc); (vii) phase 2 (FOLD-2 owner gate) is absent from the script; (viii)
  `ARCHIVE_FINAL` default embeds a literal `<date>`.
- **E7 — classification arithmetic does not sum:** §3's table totals 146 local (claims 158) and 91
  remote (claims 90); §2.3 says 116/74 united-line ancestors vs §3's 108/70 (measured: **116**
  local is right — 158 minus the 42 uPG>0); remote fold-lane twins measure **9**, not 11; and
  `fix/p0-blank-viewer` (uPG=107 — it rides the PR line) is placed in the united-line-ancestor
  class (its disposition TEXT is correct). The phase-0 sweep re-derives, but a plan that is meant
  to be the authority must sum.
- **E8 — two real-world hazards absent from the plan:** (a) the local `main` worktree
  `D:/pdf/pdf` is **dirty** (tracked mod of `docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`
  + an uncommitted `docs/audit/SECURITY-QUALITY-REVIEW-parity-glm.md`) — the plan never updates
  local `main` after the FF push nor dispositions that dirty file (a later `pull --ff-only` there
  will refuse or entangle); (b) the five antigravity worktrees hold ~703 STAGED files each,
  **including `tests/fixtures/signing/ca.key`, `signer.key`, `test_signer.p12`** — §9.6 retires
  them post-pass with no quarantine warning; any `add`/`commit`/`stash` sweep there poisons the
  freshly consolidated history. Both need explicit rows (do-not-touch + owner security item).
- **E9 — rollback table row 2 is technically wrong:** pushing an older SHA to a moved `main` is a
  **non-fast-forward (rejected)**, not "a fast-forward backwards". The only sanctioned undo is
  `git revert` (the row's own honest path) or force (forbidden). Fix the row before an operator
  tries the impossible path.
- **E10 — §4.6 checks `git merge-base --is-ancestor d03d6e94 main` against LOCAL `main`**, which
  the plan never moves — trivially true, proves nothing; check `origin/main`.
- **E11 — minor:** §1.1 "the only line fully pushed" — `feat/consolidated` (`eb0efa21`) is also
  pushed (`origin/feat/consolidated` = local, verified); §5 item 7 contains an unresolved
  mid-sentence self-correction ("— wait, it IS on the PR (verify…)") — clean up.

### 7.3 Amendments to the amendments (reconciling §3 of this critique with the landed plan)

The landed plan already satisfies A2 (survivor named), A3 (per-class proofs — fix E2's command),
A6 (picks-only; purge named as a merge-forbidden reason in §2.3(b)), A8 (freeze + sweep re-run),
A9 (July archive-only + missing tags listed), A12 (united-line deletions gated on PR-merged),
A14 (full enumeration intent; fix E7's sums). Still to adopt: **A1's restore drill** (§7 has
recipes, never rehearsed — add a phase-0 `git clone <bundle>` into scratch + tip/blob spot-checks
with a committed transcript), **A4's ledger-union proof** (the plan's §5 item 2 assigns it to the
critique — this critique confirms the delta is real: blobs `2fc16ace` vs `9c006e93`, 392/265 vs
438/276 rows; the union proof itself remains owed), **A10's at-T unreviewed-map re-run** (the
name-set superset proven in §7.1 covers FILE presence for all 348 mapped files; a carrier-level
re-run stays recommended-cheap, not blocking), and **E4's fold-lane tags**.
