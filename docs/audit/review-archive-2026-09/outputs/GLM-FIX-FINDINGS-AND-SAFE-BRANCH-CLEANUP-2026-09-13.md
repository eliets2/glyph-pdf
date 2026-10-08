# GLM handoff: fix the findings, reconcile active work, and prove cleanup preserved it

**September 14 update:** Start with `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs\PROJECT-PROGRESS-AND-QUALITY-REVIEW-2026-09-14.md` and its two detailed source reviews. It records newer integration/pending branch SHAs and new findings R14-P01–P06, including a form-command lifetime defect, false signing-level disclosure and a compiler-reproduced no-PDFium regression. Reconcile those first; the September 13 counts and tips below are historical baselines, not current state.

You are continuing GlyphPDF's implementation and repository cleanup. Read the evidence below, reconcile it with current work, fix confirmed current defects, and independently verify the results. Before deleting any further branch or worktree, preserve all original work and demonstrate that the exact state being removed is recoverable. Also audit every branch/worktree you already deleted: recover anything missing and report uncertainty honestly.

Do the work, not just propose a plan. Continue existing agents where possible. Do not mistake a preservation commit for a passing quality gate, or an old finding for proof that the current code is still defective.

## 1. Read these inputs first

Use these absolute roots; discover the actual current checkout/branch with Git rather than assuming directory names identify branches:

- Repository: `C:\Users\User\Projects\pdf`
- Review workspace: `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf`
- Evidence folder: `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\branch-preservation-2026-09-13`

Read these files in the review workspace:

1. `outputs\BRANCH-PRESERVATION-AND-CLEANUP-2026-09-13.md`.
2. In the evidence folder: `packa-review.md`, `quick-review.md`, `legacy-review.md`, `r15-review.md`, and `r18-review.md`, including their candidate/hash JSON files.
3. `graph-inventory.json`, `branch-cleanup.csv`, `branch-cleanup-unblocked-candidates.csv`, `branch-cleanup-summary.json`, and `inventory-closure.json`. Compare earlier inventories and initial/interim graph records where necessary.
4. `glm-handoff-state-check.json`: a later read-only comparison, not a new release gate or proof of pre-deletion cleanliness.
5. `backup-verification.json`, `closure-verification.json`, `closure-archive-refs.json`, `uncommitted-state-manifest.json`, `local-notes-and-probes-manifest.json`, and `closure-local-notes-manifest.json`.
6. `outputs\GLM-RESUME-WHOLE-PROJECT-PROMPT-2026-09-13.md`, `outputs\WHOLE-PROJECT-READINESS-REVIEW-2026-09-10.md`, and `outputs\WHOLE-PROJECT-IMPLEMENTATION-BACKLOG-2026-09-10.csv`. Preserve the existing architecture, product, performance, security, UI and native-Linux workstream requirements.

Also read `C:\Users\User\Projects\pdf\docs\audit\PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`. It is explicitly included in this handoff. Its statuses refer to historical source snapshots; map every finding to current code and any subsequent fix.

From the current integration checkout, read the repository instructions, current parity plan, `docs\audit\CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, `docs\audit\FEATURE-COMMAND-MATRIX-2026-09-09.csv`, and relevant `.context\research\` and active-agent handoffs. Inspect current requirements before changing dependencies or toolchain configuration. Do not mix incompatible Windows dependency builds; account for the untracked vendor binary trees needed by fresh worktrees.

## 2. Established facts and moving work

The preservation reviewer created these six commits, covering 62 file paths:

| Work | Branch at preservation | Commit |
|---|---|---|
| Existing September 13 findings | `main` | `2b715f47fd4141619eb0751c051929c8608d715c` |
| Pack A feature WIP | `feat/parity-glm-packa` | `7e32093c67c10896a0654adf820b6092d3956083` |
| Batch OCR skip-text WIP | `feat/parity-glm-quick` | `2a82738ebc3fbb1e683c22c2f7c44e49f06656ee` |
| Historical AST generator | `subagent-AST-Architect-self-54b52cfc` | `d7a8ca068eeca1145138558177d88b7503b84c3a` |
| Unfinished original Djot scaffolding | `subagent-Vendoring-Specialist-self-ca2c3f27` | `5a018ae5fb3b5d00d854c20058cc9cb12beaa6f2` |
| Redaction WIP | `feature/redaction-parity` | `45bf4302b9d43b8f7d866eb2ca9adf36c8004ba0` |

Implementation owners also committed the reviewed R15 files as `b9f417c9346e4dfcd9b0a3c6693eec77ee22fd08` and R18 files as `d71a4c2dde5e3fe25487602d4a431d507f3352c3`. No duplicate reviewer commits were necessary. The R18 typo-named panel copy matched the canonical file; its owner removed it. Do not recreate that duplicate.

At the closing observation there were 66 local branches, 19 remote-tracking branches, 17 worktrees and six stashes. No original application-source changes remained uncommitted **at that time**, but many artifacts remained local. The reviewer neither deleted branches/worktrees nor pushed commits.

At the later handoff check, **2026-09-13 14:49:21 UTC**, no baseline local/cached-remote ref or registered worktree directory was missing. Integration had advanced to `586d6e4c74f5f75900b3d387d47f9419715fb29d`; the R15/R17 and R18/R19 lane tips had also advanced. This does not exclude earlier deletions, remote-only deletion, or delete-and-recreate events. Establish what actually happened from your command records and fresh evidence; do not invent deleted branches to satisfy the request.

The old 36-local/11-remote cleanup candidate list is historical evidence. It is not a current deletion instruction. `main` and `feat/parity-glm` have substantially divergent history; keep both while reconciling it. An old feature branch can contain unique work even if similarly named work was cherry-picked elsewhere.

## 3. Coordinate active work before mutating shared state

Start with a fresh owner map: agent/task, worktree, checked-out branch, HEAD, assigned files, pending edits, running build/test jobs and intended integration target. Inspect your existing agents and resume their tasks instead of launching duplicate implementations.

Use the installed profiles under `C:\Users\User\.claude\agents\`, resolving actual filenames before use. Suggested responsibilities:

- `orchestrator`: ownership, integration order, evidence reconciliation and final cleanup decisions.
- `code-archaeologist` with `devops-engineer`: deletion reconstruction, source recovery, branch graph and backup verification.
- `backend-specialist`, `solution-architect`, and `performance-optimizer`: current backend, forms, replacement and batch fixes.
- `ui-specialist` / `ux-specialist`: current UI, accessibility and interaction defects.
- `testing-specialist`, `guarantee-verification-engine`, and relevant `security-auditor` / `native-adversary` / `fuzz-harness-engineer`: independent regression and adversarial verification.

These are responsibilities, not a requirement to spawn every profile simultaneously. Use a bounded number of workers within current capacity. Assign disjoint files or isolated worktrees; one integrator owns shared files such as CMake, AppContext, controllers and PoDoFoBackend. Do not let agents independently clean branches, bulk-stage files, reset shared checkouts or merge overlapping work.

An active owner must explicitly finish/checkpoint and release its branch/worktree before cleanup. A quiet interval is not proof that the owner stopped. Keep active branches out of the deletion set while continuing independent work elsewhere. Recheck branch, HEAD, index and file hashes immediately before each commit/integration; if they moved, reconcile the new work rather than overwrite it.

## 4. Audit anything already deleted before further cleanup

Uncommitted work belongs to a worktree/index, not to a branch ref. Prove two separate things: **committed history survived**, and **local file/index state survived**.

1. Preserve current refs, stash entries, worktree metadata, status and relevant reflogs before fetching or pruning. Then fetch without pruning and inspect advertised remote heads. Do not expire reflogs, run aggressive GC, prune worktree metadata, reset, clean, or delete anything while recovery is unresolved.
2. Reconstruct your deletion operations from available execution logs and agent handoffs. Record exact repo, local/remote ref name, former SHA, worktree path, command, time and operator. Distinguish local branch deletion, remote branch deletion, remote-tracking pruning, switching branches, and actual directory removal. Include deletions before the latest audit if your session performed them.
3. Compare the recorded operations and live state with the initial/interim/closure inventories, archive refs and bundles. For a missing historical tip, prove reachability in retained integration history or recover it under a clearly named recovery ref without changing an active checkout. Look for renamed/recreated refs and relocated directories; matching names alone are insufficient. Inspect detached HEADs and preserve every stash by immutable SHA, including its untracked-file parent when present.
4. For each associated worktree, account for staged changes, unstaged changes, untracked files, ignored research/handoffs, fixtures, binaries and nested repositories. Map every meaningful pre-deletion item to a surviving commit/blob, verified archive member, surviving file copy, or reproducible artifact with its provenance. Compare hashes; handle Git newline normalization explicitly rather than dismissing mismatches.
5. Use existing archive refs, all stash entries, the verified bundles, binary patches, raw selected-file snapshots and note archives first. Restore into a new isolated recovery directory. If required, inspect surviving HEAD reflogs and unreachable objects with a read-only Git fsck pass; recovery from those is opportunistic, not guaranteed.
6. Never claim a clean branch tip, a passing `git bundle verify`, or a merged commit proves that never-added untracked/ignored files survived. Git may have no copy of those files. Work created after the snapshot requires later owner/command evidence or backups; the old inventory cannot cover that interval. Where evidence is missing, check surviving directories and available editor/local backups, record **unknown—insufficient pre-deletion evidence**, and keep further related deletion blocked. Distinguish unknown loss from confirmed loss and from successful recovery.

Produce `DELETED-BRANCH-RECOVERY-LEDGER-2026-09-13.csv` with: operation/time, ref, former SHA, worktree, pre-deletion evidence, committed-history disposition, local-file disposition, recovery source/hash, recovery commit/ref, verification and remaining uncertainty. If no deletion is observed in a given period, say so; do not certify all historical deletions from the absence of missing refs today.

## 5. Fix and verify current defects

Create a finding-to-current-code matrix before editing: source report and ID, reviewed SHA, current code location, current status, existing owner/fix commit, action and independent evidence. Use statuses such as confirmed-open, already-fixed-and-reverified, superseded, disproved-with-evidence, or unresolved. Avoid reapplying completed work.

### Pack A

- Invalid Find/Replace ranges must be invalid, not encoded as whole-document scope. Disable mutation until scope is valid; a regression must prove zero edits for malformed ranges.
- Map QString UTF-16 match offsets correctly to glyph/codepoint geometry. Test supplementary characters before and inside a match, neighboring glyph preservation, save and reopen.
- Give one layer ownership of the initial outline write. Cover first application, failure, undo and redo without double saves or false history entries.
- Resolve synchronous regex/search stalls at their actual execution boundary. A timer checked between matches cannot interrupt one long match. Reuse existing safe matching/job infrastructure; bound work and provide cancellation without leaving hidden workers running. Exercise adversarial input and a large document.

### Batch OCR skip-text

- Skip retained text pages before rendering/OCR, maintaining correct page-indexed results. A mixed two-page test should assert only the image page invokes OCR; confidence reporting must exclude discarded OCR results.
- Fix the idempotency test's retained original input/overwrite modal using a fresh run containing only the output or a deliberate list reset. The test must finish unattended and assert counts and files.
- Either implement persistence for the three advertised options or correct the product claim. Isolate test preferences and prove reopening behavior.
- State and test the real page-preservation contract. Extracted-text equality does not establish byte identity, annotations, form data, resources or geometry. Use actual image-containing fixtures and relevant page-property round trips.

### R15 and R18

- R15: recheck the encoding issue and replace misleading coverage claims with real keyboard/UI interaction checks for the required commands. Inspect current commits before duplicating tests. Do not claim a stale installed binary verifies the newest source.
- R18: use explicit transaction success, not `undoStack.count() == before + 1`. Cover redo-tail truncation and undo limits. Update stale-state tracking through successful initial apply, undo and redo. Make the accepted session/restart/Save As persistence contract explicit and verify it. Do not blindly add persistent storage if the agreed requirement is session-only.

### Legacy work and the September 13 audit

- Reconcile old Djot/redaction prototypes with current implementations before porting anything. Preserve their provenance; do not resurrect obsolete scaffolding just to make every old branch build.
- Where a legacy fix remains necessary, complete the API/build wiring, correct malformed headers and cancellation ownership, and preserve unsaved edits on transaction failure. Keep Lua execution constrained. Correct whitespace only as part of a relevant retained change.
- Re-triage every confirmed item and lead in `PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`, including CID bounds/resource limits, build-configuration failures, spreadsheet export collisions, signing postconditions, secret-entry binding, response-size limits and redaction-proof correctness. Use current fixes and evidence rather than treating historical labels as current verification.

Fix shared root causes with existing helpers and dependencies. Preserve existing behavior outside the intended fix. Run focused regressions, then the required integration checks on the exact combined commit, including relevant build variants. For save, forms, redaction, export and signing, inspect resulting artifacts through a reopen/independent reader where appropriate. Record toolchain, command, exit code, logs, fixture and SHA. Only an independent reviewer may mark the package verified under the plan's protocol; unresolved work stays explicitly open.

## 6. Commit and preserve before deleting anything

Every original source, test, configuration, documentation or research change must be accounted for and preserved before removing its branch/worktree. Commit coherent reviewed changes on the appropriate owner branch; use clearly labelled WIP/recovery commits for unfinished work. Separately archive intentionally ignored research where appropriate. Keep credentials and generated artifacts out of source commits. A stash alone is not the finished commit-and-integration outcome requested here.

Use exact reviewed file lists and preserve unrelated staged entries. The five legacy Gemini indexes each had **703 staged artifact paths**; never use blanket `git add .`, `git add -A`, or a normal whole-index commit there. They include vendor installation output, installers and signing fixtures. Do not turn nested Djot repositories into accidental gitlinks. Preserve their separate histories and any local deltas.

Keep the existing backups immutable. The evidence folder contains:

- `glyph-pdf-closure.bundle`, with complete Git history and sealed final refs; expected SHA-256 `1c63ed3e6024d5bfc1153073cfd72401d9f1a9f81e7b131fc37ca3940a1cc596`.
- `uncommitted-state-backup.zip`, `local-notes-and-probes.zip`, and `closure-local-notes.zip`.
- `djot-7708fcd5.bundle` and `djot-ca2c3f27.bundle`.
- Source/index manifests and backup refs under `refs/archive/cleanup-20260913-initial/` and `refs/archive/cleanup-20260913-closure/`.

Read the manifests and verify archives before relying on them. Do not blindly rerun old mutation scripts or apply historical patches over active files. These backups do not contain every ignored runtime/model/vendor binary tree. Inventory and retain or separately archive those before removing a directory. Test restoration into an isolated location for the state you intend to remove.

A local branch is eligible for deletion only when all of the following hold:

1. Its owner has released it; no active process depends on its checkout.
2. Its exact current tip is recorded and backed up; original local work is committed or explicitly preserved in a verified non-source archive, with no unknown omissions.
3. Unique committed changes are integrated and verified, or deliberately retained on a durable, named recovery/WIP branch. A WIP backup is not permission to declare the feature complete or abandon it.
4. Associated worktree state is fully accounted for. Worktree removal has its own file/nested-repository/runtime check.
5. The deletion record names the exact source SHA, retained destination/ref, file-manifest evidence, independent check and recovery route. Recheck all of these immediately before deletion; abort that deletion if the state changes.

Use Git's normal local safe-delete path where applicable. A refusal is a reason to inspect ancestry/upstream configuration, not to switch automatically to force deletion. Do not delete `main`, the active integration branch, active lanes or archive/recovery refs. Do not bulk-delete by branch age, prefix or the old candidate CSV. Remote branch deletion and publishing remain separate actions: use existing explicit authorization where present; otherwise report the concrete proposed remote deletion without executing it. On Windows, verify resolved paths stay within the exact intended worktree before any directory operation; avoid cross-shell or string-built recursive deletion.

## 7. Deliver concrete evidence

Save the current finding/fix matrix, active-owner map, deletion recovery ledger and cleanup decision ledger in the repository's appropriate audit location. Commit the reports and remediation work without absorbing another agent's pending files. Include exact final commit IDs, independent checks, remaining WIP and preserved local artifacts.

The final response must distinguish: defects fixed and verified; work already fixed by others; original work preserved but unfinished; deleted refs/files recovered; deletions proven safe; cases with insufficient evidence; and cleanup still blocked by active work. List precisely what you deleted during this run, if anything. Finish with fresh refs/worktree/status evidence and verified backups of the resulting state. Never substitute “all clean” for a file-by-file preservation account or claim “nothing was lost” beyond what the evidence proves.
