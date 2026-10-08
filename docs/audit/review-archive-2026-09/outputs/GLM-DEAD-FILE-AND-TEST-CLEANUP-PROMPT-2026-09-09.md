# GLM prompt — launch a repository cleanup agent

Launch one dedicated cleanup subagent using this role entrypoint:

`C:\Users\User\.claude\agents\code-archaeologist\code-archaeologist.md`

Keep the configured GLM model. Adapt the profile's examples to GlyphPDF's C++17/Qt/CMake stack. Use the real agent mechanism and give this worker a bounded task; do not merely describe an agent or stop after a cleanup proposal. You remain the coordinator and review its deletions independently.

## Task

Remove demonstrably dead first-party files, obsolete code and genuinely redundant or valueless test files from GlyphPDF. Update references and build/test registration, preserve useful coverage, and leave a smaller working repository with reproducible evidence. There is no deletion quota. A small justified cleanup is better than removing useful files to produce impressive numbers.

Work from `C:\Users\User\Projects\pdf-parity`, expected branch `feat/parity-glm`. Leave `C:\Users\User\Projects\pdf` and the installed app untouched. This cleanup complements the active UI/quality implementation; it does not replace that work.

## Establish the baseline and ownership

1. Read applicable `AGENTS.md`, `CLAUDE.md`, the current `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, and relevant `.context/research/` and execution checkpoints. Confirm actual Git state instead of trusting old hashes in memory files.
2. Read these review files under `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs\`:
   - `QUALITY-GATE-2026-09-09.md`
   - `UI-BUTTON-REVIEW-2026-09-09.md`
   - `GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md`
3. Inspect status, worktree list and current branches; fetch origin and record exact upstream/local SHAs. Check local-only commits and dirty files before selecting an implementation base. Assign the cleanup worker an isolated worktree/branch from the agreed integration base. Do not reset, stash or delete another session's work.
4. Record tracked files with `git ls-files`. Inventory untracked and ignored files separately; being ignored, old or absent from Git is not evidence that a file is disposable. Fresh worktrees require the three untracked vendor binary trees documented in commit `05a3336`; preserve them.
5. Record configured CTest targets using `ctest --show-only=json-v1`, relevant build options and the baseline test results. Check optional targets in CMake and CI too: one configured build does not list every supported test.

The worker owns only its reviewed cleanup candidates and necessary reference/registration edits. Shared CMake, resource or registry files have one integration owner; coordinate changes with the UI/feature workers rather than edit them concurrently.

## Prove each candidate is removable

Create `docs/audit/CLEANUP-LEDGER-2026-09-09.md` or a dated successor with:

`path | tracked/owner | purpose | proposed action | evidence | consumers checked | retained replacement/coverage | validation | commit | review status`

Use `rg` for references, but never treat zero text matches as sufficient proof. Inspect:

- CMake targets, source lists, globbed files, generated headers, compile definitions and optional configurations.
- Qt resources, translations, stylesheets, plugin metadata, object names, signals/slots, `invokeMethod`, string-based tool IDs and runtime file lookup.
- Scripts, CI workflows, install/deploy/package manifests, documentation commands and external/manual entrypoints.
- Tests and fixtures, corpus discovery, fixture generators, fuzz harnesses, benchmarks and audit reproduction instructions.
- Git history and current plans: why the file was introduced, what replaced it, and whether another worktree is completing its wiring.

Deletion candidates include obsolete unreferenced implementations with a working replacement, accidental tracked generated outputs with a documented regeneration path, duplicate fixtures with no meaningful distinctions, and superseded one-off scripts whose behavior and consumers have been accounted for. Remove their stale references in the same coherent change. Do not replace deleted files with empty stubs.

**Preserve these unless separate, concrete evidence establishes a safe replacement:** vendored dependencies, binary SDKs, model weights/provenance, licenses/notices, signing fixtures and intentionally test-only keys, installer assets, translations, fuzz seeds, active research/planning material and independent audit evidence. Do not clean the review workspace or external agent definitions. Do not prune vendor sources or their upstream test suites as part of this first-party cleanup.

The user explicitly wants hidden features exposed and completed. A command, panel, source file or resource is not dead merely because `plannedTools()` hides its button, it has no UI route yet, or its optional dependency is unavailable. Put these in **retain-and-wire** or **needs investigation**, not the deletion list. Do not remove entries from the feature matrix to make completeness look better.

## Decide which tests are actually useless

Read each candidate's assertions, fixtures, setup and failure conditions. Compare behavior coverage, not filenames or line similarity.

| Situation | Required action |
|---|---|
| Exact duplicate with the same contract, inputs, negative cases and configuration coverage | Keep the stronger canonical test and migrate any unique assertions/fixtures before deleting the redundant file. |
| Placeholder, unconditional success or test that never invokes relevant production behavior | Replace with a meaningful regression if the contract is still needed; otherwise remove with a documented reason. |
| Test of a retired implementation | Remove only after proving that implementation and its supported behavior were retired, or that replacement behavior is covered. |
| Unregistered test | Determine why. Register or migrate it if valuable; lack of CTest registration alone does not make it dead. |
| Slow, flaky, failing, skipped or environment-dependent test | Investigate, fix or accurately classify it. None of these properties alone justify deletion. |
| Similar tests exercising different failure modes or layers | Preserve their distinct coverage. Unit, integration and UI tests may intentionally overlap. |
| Historical regression or adversarial fixture | Retain unless equivalent protection is demonstrated in a surviving test. Age is not a reason to delete it. |

For every removed test, record its protected behavior, retained test/case names, relevant inputs and negative cases, and evidence that the replacement detects the failure. For substantial consolidation, reproduce the original defect or use a small controlled negative case in scratch to show the surviving check fails appropriately; do not introduce a broad mutation-testing project for trivial duplicates.

Preserve protection for save/rollback, undo/redo, autosave/recovery, session switching, batch output collisions, redaction, signatures/trust, OCR readiness/staleness, geometry, packaging and optional builds. Reconcile G01–G23 and newer ledger rows. In particular, do not delete `TestBatchOpsCoverage` or `TestStatusBarSlim` merely because they failed the previous quality gate. Their underlying failures require repair and revalidation.

Do not weaken assertions, change expected output to match a bug, add broad skips, reduce configuration coverage or remove negative controls just to get a green suite. A lower test count is acceptable only when every reduction has a justified coverage disposition.

## Execute the cleanup safely

The user has authorized cleanup: proceed with proven tracked deletions and necessary edits without asking for routine confirmation. Before deleting, record each exact candidate and ensure its current contents still match the inspected version. Defer uncertain or concurrently edited candidates while continuing proven work.

Use explicit path lists and small coherent commits, with test consolidation separate from unrelated production-file cleanup. Prefer tracked-file removal through Git so restoration is straightforward. Do not use `git clean`, broad wildcard deletion, recursive extension sweeps, history rewriting or force operations.

For any necessary Windows recursive deletion, resolve the absolute target and verify it is inside this agent's isolated worktree, is not the worktree root, and does not cross a junction/symlink. Use one shell end to end and native PowerShell `-LiteralPath`; do not enumerate in PowerShell and pass paths to `cmd /c`. Existing untracked/ignored files of uncertain ownership are report-only. The worker may remove its own reproducible scratch outputs after recording needed evidence.

## Verification and independent review

1. Run affected tests before and after each coherent cleanup. Use a fresh out-of-source configure/build to detect dependence on stale objects or removed generated files.
2. Update CMake/CTest, resource and packaging references together. Check for dangling references and run applicable resource, translation, script and package checks. Retain supported optional configurations; absent dependencies remain explicit verification gaps.
3. Run the relevant full suite at the final integrated commit. Compare before/after target and case inventories. Explain every removed or renamed test and every skip; an unchanged pass percentage is not proof of preserved coverage.
4. Review the final diff against concurrent changes before integration. Rerun affected integration checks after combining with feature/UI work.
5. The coordinator independently reviews each deletion and its evidence, applying the native-testing principles in `C:\Users\User\.claude\agents\testing-specialist\testing-specialist.md`. It must inspect the surviving assertions and actual results, not simply repeat the cleanup agent's summary. Mark implementation `implemented-awaiting-review`; reserve `verified` for the repository's independent-review protocol.
6. Leave publishing, installation, main-branch merges and existing user documents untouched. If uncertainty remains about a candidate, retain it with the reason; do not block the entire cleanup over that file.

## Final report

Return the exact branch and commit; deleted/consolidated files with reasons; before/after tracked file count and test inventory; meaningful coverage replacements; fresh build/test results; retained uncertain candidates; residual failures; and the cleanup ledger/evidence paths. Report disk-space savings only if measured, and distinguish source cleanup from generated build artifacts. Do not claim the repository is free of dead code or that all tests are valuable without complete evidence.

Start by launching the cleanup agent, pinning its baseline and assigning the first bounded candidate group. Complete the justified deletions and review, not only the inventory.
