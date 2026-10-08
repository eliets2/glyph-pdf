# GLM prompt — resume GlyphPDF implementation from the independent reviews

Paste this prompt into the GLM session that owns GlyphPDF implementation.

You are the implementation coordinator. Read the completed reviews below, reconcile them with the latest code and existing agent checkpoints, and resume the remaining work. Implement, test, integrate and independently review bounded packages. Do not stop after summarizing the reports or generating another roadmap. Preserve working features and concurrent changes.

Keep the native C++/Qt architecture. Complete reliable PDF workflows, improve the UI and measured performance, and advance native Linux support according to the staged plan. A rewritten framework, larger button count or green helper test does not establish a finished product.

## 1. Read the review package

Review directory:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs`

Read these files in order:

1. `WHOLE-PROJECT-READINESS-REVIEW-2026-09-10.md` — overall assessment, architectural contracts, corporate requirements, UX priorities and release gates.
2. `WHOLE-ARCHITECTURE-REVIEW-2026-09-10.md` — A01–A07: resident-state rollback, command/history gaps, package export, repaired-load identity, external conflicts, temporary ownership and the latent scheduler issue.
3. `PERFORMANCE-AND-SCRIPT-REVIEW-2026-09-10.md` — PERF-01–PERF-05 and JS-01, including actual comparison/resource/deadline reproductions and required fixes.
4. `WHOLE-PRODUCT-AND-PLAN-REVIEW-2026-09-10.md` — PP01–PP07, current feature matrix, replacement/printing/layers/signing problems, professional UI and plan reconciliation.
5. `NATIVE-LINUX-READINESS-2026-09-10.md` — L01–L13 and native dependency, file-safety, credentials, packaging and desktop acceptance gates.
6. `WHOLE-PROJECT-IMPLEMENTATION-BACKLOG-2026-09-10.csv` — 27 work packages, owners, dependencies and observable acceptance criteria.
7. `WHOLE-PROJECT-REVIEW-VERIFICATION-2026-09-10.json` — exactly what was checked and what was not.
8. **Additional September 13 review, stored in the main checkout:** `C:\Users\User\Projects\pdf\docs\audit\PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`. Read this file explicitly even when implementation takes place in another worktree. It was not an input to the September 10/11 audit above; include it now as a separate source with its own evidence and confidence labels.

Reconcile every item in that additional report, not just its first four findings. Prioritize its reported CID `/W` bounds/memory corruption issue, no-RapidOCR compile failure, duplicate XLSX cell references and empty-result signing-validation acceptance. Route its timestamp downgrade, HTML font-name injection, alternate Excel writer, capability transitions, certificate lifetime and redaction-proof geometry/extraction leads to the appropriate independent reviewer. The PPTX entry is explicitly a likely non-issue: investigate, and refute/close with evidence if appropriate rather than implementing a speculative patch.

Use a distinct `SEP13:<heading or item>` reference for these additions; extend the current execution table without renumbering the 27-package CSV or implying it already covered them. Recheck each item against the selected current candidate and correlate with existing fixes and overlapping findings. The report's checkmarks include adversarial or inline confirmation; they do not automatically satisfy the separate per-package implementation/release verification protocol. Keep reported confidence, your reproduction result and release acceptance separate. Do not retroactively alter the earlier audit's scope or evidence bundle.

The compact evidence bundle is `whole-project-review-evidence-2026-09-10.zip`. Original evidence, immutable source snapshots and probes are under:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\whole-project-2026-09-10`

Read `baseline.json`, `published-baseline.json`, `compare-results.json`, `myers-benchmark.json` and `formjs-deadline-results.json`. The corresponding probe scripts are `compare-probe.py`, `perf-probe.py` and `formjs-probe.py`.

Those scripts reference historical source roots/commits. Copy them into your own evidence directory and explicitly adapt them to the candidate under test. Record source hashes, compiler/dependency versions and commands. Running a historical probe against the historical snapshot again is not verification of today's implementation. Do not modify or erase the original audit evidence.

Use the earlier `UI-BUTTON-REVIEW-2026-09-09.md`, `GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md` and `GLM-DEAD-FILE-AND-TEST-CLEANUP-PROMPT-2026-09-09.md` for UI detail, agent routing and cleanup safeguards. Older findings/test counts are historical context; revalidate before acting on them.

## 2. Establish what exists now; do not restart completed work

Start with `C:\Users\User\Projects\pdf-parity`. Inspect applicable repository instructions, Git status, remotes, local branches, worktrees and running agent state. Run `git fetch origin`, then pin the relevant published branch and the chosen integration candidate by full SHA. Read both `main..origin/feat/parity-glm` and the meaningful newer deltas; account for earlier cherry-picks by examining content rather than counting commits.

The reports were finalized September 11 against September 10 snapshots:

- Published: `9ba3cea499f67060604ee09903f63ea1a79c30fd`.
- Local integration including Form JavaScript: `b38a4fd524e754fc91981f7bbeba254d85674c24`.
- The active worktree was then on a different quick-work branch containing measurement CSV/XFA disclosure work.

These are evidence references, not instructions to reset or check out those commits. The paths, branches and ownership may have changed. Discover the real current state. `C:\Users\User\Projects\pdf` on `main` is not automatically the implementation checkout; do not disturb another session's checkout.

Read the current repository's `AGENTS.md`/`CLAUDE.md`, PRD, ROADMAP, `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, feature-command matrix, `docs/research/`, `.context/research/` where present, the three implementation plans and any newer backlog/resume files. Research migrated between locations; index it before deciding files are absent. Inspect existing agent outputs, branches and uncommitted changes before assigning replacements.

Create or update one current status table mapping each review finding and each CSV package to:

`current source/ref | owner | reproduced / source-only / no longer applicable | implementation status | independent review status | release artifact status | evidence | next action`.

Use `WP-R01` through `WP-R27` when referring to the CSV's R01–R27 packages, so they cannot be confused with older parity-ledger R-package IDs. Keep Axx, PPxx, PERF-xx, JS-01, Lxx and historical Gxx references intact.

If a finding appears fixed, inspect the fix and run its relevant acceptance test before closing it. If it was already independently verified, record that evidence and move on. Do not duplicate measurement, comments, signing, OCR or search implementations because an older synthesis called them missing.

Protect dirty files and other agents' work. Never use destructive reset/clean, force-push, blanket stash or main-branch reconciliation as a shortcut. Coordinate existing owners or create isolated worktrees from the agreed candidate. Read commit `05a3336` and current bootstrap/deployment documentation for untracked vendor binary trees; do not assume a fresh worktree contains them. Retain the coherent MSYS2 UCRT64 Windows toolchain. Keep binaries out of source commits.

## 3. Resume real agents with bounded ownership

Resume relevant existing agents when their state is available and matches this task. If an agent cannot be resumed, create a replacement with its checkpoint, current base SHA, owned files, completed evidence and next unfinished step. Do not claim an unavailable agent resumed or reconstruct its unreported work from guesses.

The agent profile root is:

`C:\Users\User\.claude\agents`

For each role, read its actual entrypoint before assigning work: `<root>\<role>\<role>.md`. Load referenced files only as needed. Use these roles across sequential waves; do not launch all 18 simultaneously:

| Role | Responsibility |
|---|---|
| orchestrator | Integration candidate, ownership, dependencies, checkpoints and evidence/status reconciliation |
| code-archaeologist | Current-code tracing, recent-commit reconciliation, existing-feature reuse and proven dead-file inventory |
| solution-architect | Minimal shared mutation/session/job/platform contracts and cross-component review |
| backend-specialist | Engine and persistence analysis; pair with the native executor for implementation |
| gsd-debugger | Reproduce remaining failures and isolate their root causes |
| gsd-executor | Scoped C++/Qt production repairs and implementation |
| gsd-planner | Turn the next ready packages into exact executable assignments |
| gsd-roadmapper | Reconcile the existing roadmap and release scope, without inventing another competing backlog |
| security-auditor | Static security review of document, script, file, credential, update and privacy boundaries |
| native-adversary | Safe adversarial reproduction against owned native code and synthetic fixtures |
| fuzz-harness-engineer | Bounded fuzz/differential harnesses with deterministic inputs and timeout/failure reporting |
| performance-optimizer | Current-path measurements, budgets, cancellation and before/after evidence |
| testing-specialist | Meaningful QtTest/CTest, saved-output, UI workflow and configuration tests |
| guarantee-verification-engine | Independent check of claims, invariants, evidence and package acceptance |
| devops-engineer | Dependency closure, native builds, CI, resources, reproducible packaging and Linux delivery |
| ui-specialist | Qt ribbon/home/controls, icons, themes, scaling and accessibility implementation/review |
| ux-specialist | Complete task flows, discoverability, keyboard use, output feedback and error recovery |
| emergence-engine | Cross-component hypotheses from completed findings; route hypotheses to tests before calling them facts |

Keep GLM as the configured model. Profile examples for React/Next/Node/Tailwind/Jest do not authorize changing this application's technology. Apply relevant principles to C++/Qt; use a role as a reviewer if its implementation specialization does not fit.

Use at most three workers plus the coordinator initially, or fewer if the environment requires it. Each task must name its goal, exact candidate SHA, finding/package IDs, required readings, owned paths, off-limits paths, validation command/evidence and exit criterion. Give shared files and interfaces one owner. In particular, do not let unrelated agents concurrently change FormManager, document identity, history, GpMainWindow, command registries or CMake.

An implementer cannot independently approve its own package. Use an actual separate review assignment. If independent execution is unavailable, retain `implemented-awaiting-review` and continue other useful work.

## 4. Execute in dependency order

**Wave 0 — current evidence and candidate (WP-R01).** Reconcile refs, checkpoints and status. Select the next unfixed package with a concrete test. Do not spend the entire run generating plans.

**Wave 1 — integrity/security and reliable basic actions.** Run disjoint lanes:

- Persistence/history owner: WP-R02 → WP-R03, then related WP-R09/WP-R10. Preserve the complete pre-operation resident state after failure; cover legacy rotate/inline-text commands; mint load identity on repair too; address external file conflicts and live temporary-session ownership.
- Script owner: WP-R05, then WP-R18. Reproduce both post-evaluation deadline bypasses, keep budgets active across every JS entry/result/exception/coercion path, and preserve truthful field outcomes. Use disposable child processes with parent timeouts. Freezing a helper name alone does not fix hostile getters or serialization. Distinguish restricted in-process execution from OS isolation.
- Comparison owner: WP-R06, then WP-R11/WP-R13. Build content differences from actual aligned old/new page pairs; bound retained overlays and matching work; handle PDFium's terminator count correctly.

Schedule WP-R04 encrypted-package candidate replacement and WP-R08 print-job ownership when their shared controller files have an available owner. WP-R07 replacement depends on the persistence/history contract: fix actual content semantics, exact match targets, search options, checked counts and read-only enforcement. A white rectangle over old text is not content replacement or redaction.

**Wave 2 — verify recent repairs and finish professional workflows.** Complete WP-R14 and the remaining applicable historical repairs without undoing newer fixes. Deliver WP-R15–WP-R19 in dependency order: one command identity, honest visible availability, welcome task continuation, accessibility/localization and truthful signing/form capability configuration.

The user wants richer tabs, distinct functional tools and a welcome screen with more fast actions. Keep unfinished tools discoverable with disabled reasons and useful alternatives, while completing real routes. Fix layer/stamp no-ops. Use the report's twelve welcome actions and retain intent after Open. Use PDF-XChange's organization and PDFelement's task-oriented flow as references, preserving GlyphPDF branding. A screenshot or many buttons does not demonstrate working actions.

The installed app previously reviewed was old. Build the selected candidate and verify its identity before reviewing its UI. Use generated/non-sensitive fixtures and exercise every changed route, save/reopen result, keyboard path, disabled state and cancellation path. Do not relabel old screenshots as new acceptance. Prepare the new build before any reinstall; preserve user settings/documents and follow the current session's deployment authorization.

**Wave 3 — measured performance, managed delivery and native Linux.** Complete WP-R12 and WP-R20–WP-R25. Measure the actual viewer, two-page, thumbnail and compare paths; the old RenderCache performance map was inaccurate. Treat startup/latency numbers in the report as proposed targets, not measured promises. Keep typical-edit performance and correctness controls while bounding worst cases.

Follow the Linux report's native sequence: portable CMake and platform-specific dependency artifacts; atomic file/recovery behavior; native credential storage; installed resources; native app/CLI build; desktop/MIME/XDG integration; printing, Wayland/X11, clipboard/IME/accessibility; then one reproducible distribution channel. Retain Windows validation. A Linux fuzz job or offscreen Qt test is not a native application acceptance run. Start with the already selected target in the current plan, or the report's narrow initial target if none exists. Do not add unsupported distro/architecture claims.

If no usable Linux environment is available, finish portable changes and build/test recipes that can be checked locally, record the exact missing gate and continue Windows work. Never report an unrun native build as passed or silently install a whole OS environment as a workaround.

**Wave 4 — selected expansions and pilot preparation (WP-R26/WP-R27).** After the reliability gates, extend existing seams for approved batch presets, printable review summaries, local signing requests and other chosen workflow packs. Treat new cloud services, broad enterprise integrations, paid services and new dependencies as separate decisions unless existing authorization covers them. Prepare pilot fixtures/scripts and acceptance criteria; actual customer recruitment or external contact requires the user's authorization. Do not invent pilot results.

## 5. Evidence and acceptance rules

- Preserve separate implementation, independent-review and release states. `verified` is reserved for the plan's independent per-package protocol. Neither a merge nor `SHIPPED` wording in the research backlog upgrades a row.
- Use the current candidate for tests. Record full SHA, dirty-state scope, enabled capabilities, dependency/model versions, command, exit status, output and meaningful skips. Old full-suite totals apply only to their old commits.
- Where reproducible, demonstrate the pre-fix failure and post-fix pass with the same fixture. For source-only findings, first build the smallest reachable reproduction; do not describe an inferred crash or data loss as already observed.
- Assert disk bytes, resident state, history, dirty/recovery identity and retryability on failed mutations. Inspect successful saved PDFs with an independent read path; inspect rendering when geometry/appearance matters.
- For compare, require insertion/removal/reorder/duplicate-page cases to agree across content rows, navigation and exports. For JS, include helper overrides, getters/proxies/coercion/exception access, dependent failures and aggregate budgets.
- For security-sensitive operations, state the supported scope: redaction string sweeps are not universal proof of image removal; signing appearance, integrity, trust and attained profile differ; calculation/formatting is not all Acrobat JavaScript.
- Keep optional-feature builds, runtime dependency absence, read-only/signed/encrypted documents, full disk/failed commit, stale callbacks, cancel and teardown in the gate. Do not remove failing tests, weaken assertions or hide failures as skips to get a green result.
- Keep the existing no-new-dependency rule except for explicitly recorded exceptions. The local Form JavaScript ledger records quickjs-ng Option A authorization; verify that record and current scope. It does not grant blanket permission for other runtimes/services. Pin approved libraries and reference ports, retain attribution and verify the actual packaged closure.
- A cleanup agent may remove proven dead first-party files or consolidate genuinely redundant tests after tracing callers/build entries/coverage. Preserve evidence, fixtures, optional paths, assets with runtime lookup and tests that expose failures. Keep cleanup separate from functional repairs.
- Final release acceptance requires the exact integrated Release build, appropriate full/optional suites, clean-machine runtime/installer checks and the relevant native UI/OS matrix. Do not broadly retest unchanged work without a reason, but do run required integration gates after merging overlapping behavior.

## 6. Checkpoint and resume without losing work

Update one existing resume document if suitable; otherwise create `docs/audit/GLM-RESUME-STATE-2026-09-13.md`. Record:

`candidate SHA and branch/worktree | active agent IDs and owners | dirty files | completed package commits | independent evidence | running processes/tests | outstanding failures | next exact task/command | external blockers`.

Checkpoint after each bounded package and before a context/usage interruption when possible. Mark interrupted tests incomplete. When limits reset, inspect this state and actual Git/agent/process state, then resume the unfinished step. Do not restart the whole audit, spawn duplicate workers, count a terminated test as a pass or leave agents editing after claiming completion.

Continue through review findings and corrections until the authorized work is complete or an external prerequisite genuinely blocks a specific gate. Ask only for a missing decision that materially blocks dependent work, explaining the exact reason; continue independent authorized work meanwhile.

At completion, return the integrated commit/ref, a concise list of completed versus awaiting-review/blocked packages, exact build/test/packaging evidence, current UI evidence, Linux support actually demonstrated, remaining risks and the resume-file path. Do not claim corporate readiness or competitor parity beyond that evidence.
