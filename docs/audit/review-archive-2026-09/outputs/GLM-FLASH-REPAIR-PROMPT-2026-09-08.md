Implement the unresolved GlyphPDF audit fixes. Read the reports, reproduce the failures, fix their shared production causes, and supply evidence for independent review. This is an implementation task: continue beyond planning until the issues are addressed or a specific dependency genuinely blocks further progress.

## Repository and current state

Work in `C:\Users\User\Projects\pdf-parity` on `feat/parity-glm`. The main checkout at `C:\Users\User\Projects\pdf` belongs to another session; leave it unchanged.

Start with `git status`, `git worktree list`, and `git fetch origin`. Inspect the current local and remote commits. The newest reviewed revision was `b58b91054ca7a573a09c56d950588ac4f966df42`; the earlier review baseline was `0caa45e7d0751caaa36a54a085c41211a422f019`. Compare subsequent changes before assuming a finding remains unfixed. Preserve other sessions' dirty files and commits. Do not reset, clean, overwrite, or automatically discard divergent work. If isolation is needed, use a separate writable worktree and record its branch/revision.

Read applicable repository instructions. Fresh checkouts need the three vendor binary trees documented in commit `05a3336`; establish the intended dependencies instead of silently substituting incompatible system libraries.

## Read these files first

All review files below are in:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs\`

Read in this order:

1. `TEAM-QUALITY-REVIEW-2026-09-08.md` — current repair queue, priorities, scoped acceptances and implementation order.
2. `TEAM-ENGINE-CODE-REVIEW-2026-09-07.md` — EC01–EC06, reproductions and acceptance contracts.
3. `TEAM-ARCHITECTURE-REVIEW-2026-09-07.md` — ARC01–ARC07 and document/session integration failures.
4. `TEAM-NEW-COMMITS-REVIEW-2026-09-07.md` — accepted repairs, NCR-01/NCR-02 and remaining N/D issues.
5. `TEAM-INFRASTRUCTURE-REVIEW-2026-09-07.md` — INF01–INF06.
6. `RESEARCH-RECONCILIATION-2026-09-08.md` — corrections to research assumptions; prevents duplicate features and premature acceptance claims.
7. `GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md` — detailed older implementation specifications; its current checkpoint points to the newer reports.

Follow the historical report links for unresolved V/D/N findings. Read the repository's `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` row by row. Use `team-quality-evidence-2026-09-08.zip` and `TEAM-REVIEW-VERIFICATION-2026-09-08.json` for the saved probes, logs, manifests and fixture evidence. Original review work is under the sibling `work` directory; copy probes into your own scratch/build area rather than overwriting preserved evidence.

Research originals are at `C:\Users\User\Projects\pdf\.context\research\`, including `synthesis.md`. Read them as design context. They are not proof that GlyphPDF features work. Current source and scoped independent acceptance take precedence over broad research/PRD claims.

## Repair order

1. **Prevent data loss:** EC01 common engine save and INF01 cleanup CLI. Serialize to a distinct candidate, validate, then commit safely. Preserve existing bytes on failure and preserve signed incremental-update semantics.
2. **Unify document identity:** ARC05/ARC01/ARC02 and EC02. Keep viewer, editor, session, history, annotations, autosave and async completion tied to the correct document/revision. Cover fresh Open, A→B→A and same-path reloads.
3. **Make persistence and history truthful:** ARC03/ARC04, EC03/EC05 and V01/V02/V06. Failed/canceled Save must keep work open. Failed mutation/restoration must not silently advance history or delete another page.
4. **Finish remaining contracts:** ARC06/ARC07, EC04, NCR-01/NCR-02, D01/D06/N08 and all other unresolved legacy findings, including V03/V04/V05 and D03/D04. Complete the unaccepted N06 retry boundaries and remaining N09/D02 scope. For EC06, repair the worker-drain contract or explicitly keep the unused API disabled; do not invent production usage.
5. **Make release checks reliable:** INF02–INF06, dependency bootstrap and test source-root handling. Use a dedicated, validated Release configuration with correct artifact versions and meaningful failing gates.

Preserve already accepted fixes. DocMDP certification and recipient certificate encryption already have code paths; assess their actual gaps rather than implementing duplicate features. New research features such as measurement, general form scripting, MCP, cloud services or a new workflow framework are outside this repair pass. Leave installation and live UI review for the subsequent phase.

## Implementation and evidence rules

- Maintain one checklist mapping every finding to its current status, affected code, reproduction, fix, tests and residuals. Reconcile already-fixed items using evidence at the actual revision.
- Trace callers and fix the common boundary. Reuse existing `SafeSave`, `DocumentSession`, serializers and capability interfaces. Keep changes small and coherent; avoid broad architecture rewrites.
- Use synthetic PDFs and credentials. Test real saved/reopened artifacts for persistence claims: bytes/hashes, page identity/count, text, geometry, annotation state and signature behavior as applicable. Use deterministic barriers for identity/lifetime races and controlled failure injection for write/commit failures.
- A mock call count alone cannot prove PDF correctness. A file-existence probe cannot prove OCR initialization. A PDF version/metadata assertion cannot prove PDF/A conformance. A passing helper test cannot prove its controller is wired.
- Confirm regression slots are registered and execute. Run targeted tests per repair, then the relevant integration tests and full suite on the final coherent revision. Run the optional-dependency configurations implicated by findings. Report skips, fixture setup, failures, reruns and unavailable external validators separately.
- The previous newest suite was 106/108 initially, then 2/2 after Djot fixture staging—not one clean 108/108 run. Do not copy historical totals as your evidence.
- Update the repository ledger with exact commits and evidence paths. Use `implemented-awaiting-review` for your repairs. Reserve `verified` for a separate independent reviewer. Preserve earlier scoped acceptances, merge duplicate U07/D02-redaction rows, distinguish AI D02 from redaction D02, and remove stale placeholders/counts.
- Make small reviewable commits containing only your changes. Keep progress updates concise. When context or limits interrupt work, save the checkpoint, remaining checklist and exact next command so resumption does not restart the audit.

## Completion handoff

Provide the final branch/commit, finding-by-finding disposition, implementation summary, exact build/test commands and results, saved evidence locations, and any remaining blockers. A blocker must name the missing dependency or failing contract and what was attempted; continue unrelated work where possible.

Do not declare the branch accepted merely because it builds or tests pass. Finish with a concrete repair set ready for independent review, while leaving the installed app and main checkout unchanged.
