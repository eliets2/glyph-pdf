# GLM execution prompt — GlyphPDF quality, performance and complete native UI

Paste this file into the GLM session that owns the GlyphPDF implementation. This is an execution request, not a request to produce another plan and stop.

## Mission and current authorization

You are the implementation coordinator for GlyphPDF, a native C++17 / Qt Widgets Windows PDF workstation. Run the specialist agents listed below in bounded waves. Repair the independently reported defects, expose existing capabilities, complete the necessary command wiring and missing workflows, improve measured performance, and deliver a professional ribbon and useful welcome screen. Use PDF-XChange Editor's organized command groups and Wondershare PDFelement's task-oriented home as references. Preserve GlyphPDF's own visual identity and local document processing.

The user explicitly requests richer tabs, distinct useful buttons, visibility of features currently hidden, and more welcome-screen quick tools. This supersedes the old AR-8 hide-planned-tools UI decision and the earlier repair-only restriction for this UI/command-completion work. It does not authorize a web rewrite, a new cloud service, telemetry, document uploads, paid accounts, or copying another vendor's assets. Do not substitute a wall of placeholders for completed functionality.

Implement and test. Continue through independent review and correction of findings. Do not claim to have optimized everything or achieved competitor parity from a screenshot, a feature count, a passing helper test, or one successful build.

## Read these inputs before editing

The review output directory is:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs`

Read in this order:

1. `UI-BUTTON-REVIEW-2026-09-09.md` — source findings, rendered UI evidence, button inventory, target ribbon/welcome behavior and validation criteria.
2. `QUALITY-GATE-2026-09-09.md` — G01–G23 findings, scoped acceptances, repros and repair order. These are historical findings to revalidate against your current revision, not permission to overwrite newer repairs.
3. `GLM-REPAIR-PROMPT-2026-09-09.md`, `QUALITY-GATE-VERIFICATION-2026-09-09.json` and `quality-gate-evidence-2026-09-09.zip` — repair contracts and reproducible evidence. This prompt extends the UI scope while preserving their data-integrity requirements.
4. `TEAM-QUALITY-REVIEW-2026-09-08.md`, `RESEARCH-RECONCILIATION-2026-09-08.md`, then relevant engine/architecture/infrastructure/new-commit reports referenced by them. Do not mark old scoped acceptances as acceptance of an entire subsystem.
5. In the actual working repo: applicable `AGENTS.md`, `CLAUDE.md`, `.context/research/`, `.context/resume*`, `.planning/STATE.md` if present, `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md`, `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md`, the current roadmap and any newer execution plans. Index research by topic/date and distinguish design-only documents from implemented code and independently reproduced results. Repository facts override stale branch hashes/test counts in memory files.

Preserved review sources and probes are under:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\gate-2026-09-09`

The new UI probe, screenshots, role-profile index and command inventory are under sibling `work\ui-prompt-2026-09-09`. Copy a probe into your own scratch directory before modifying it. Preserve the original evidence.

## Establish the real baseline and protect concurrent work

1. Start at `C:\Users\User\Projects\pdf-parity`, expected branch `feat/parity-glm`. Inspect status, worktree list, branch, remotes, local tip and upstream. Run `git fetch origin`. Pin the fetched `origin/feat/parity-glm` and your selected implementation base by full SHA.
2. Review `git diff main..origin/feat/parity-glm` and `git diff main..feat/parity-glm`, plus upstream-to-local and uncommitted differences. Account for earlier cherry-picks: compare actual content, not just commit counts. Do not confuse `C:\Users\User\Projects\pdf` on `main` with the parity worktree.
3. Earlier independent quality evidence covered published `e99c73365e14189fc6343da5836d84df24183152` and local Release `a5840dcfc6b2716e5b11e626a46ae3ec0ba0eaeb`. During prompt preparation the working tip had advanced to `48087ad` and was 18 commits ahead of its locally recorded upstream. That was a read-only snapshot, not a new fetch or a quality verdict on those commits. Recheck everything now; never reset to these historical hashes.
4. Dirty files included measurement/UI/backend/tests and model provenance. Record current owners and preserve their changes. Coordinate or isolate your work; do not stash, clean, reset, force-push, merge into `main`, or replace another agent's edits. Create agent worktrees from the agreed integration base when needed. Keep local-only changes and published changes separately identified.
5. Fresh worktrees need the three untracked vendor binary trees documented by commit `05a3336`. Read that commit and the current build scripts for exact locations and integrity checks. Use the existing MSYS2 UCRT64 toolchain. Do not mix Qt/MinGW distributions or silently fetch an incompatible substitute. Keep vendor binaries out of commits.
6. Save a baseline manifest with source SHA, dirty paths, dependency/model versions, configure flags, build directory and test inventory. The previous independent local Release result was **116/118**, not a green baseline. New results need their own logs.

## Agent routing — use all 18 roles where they fit

The supplied locations are directories. Load each role's actual Markdown entrypoint before its assigned task, then only its relevant referenced documents. Do not load every profile into every worker. Keep GLM as the configured model; Opus/Sonnet names in profile frontmatter are not instructions to change provider or model.

| Agent | Exact entrypoint | Bounded responsibility |
|---|---|---|
| orchestrator | `C:\Users\User\.claude\agents\orchestrator\orchestrator.md` | Coordinate waves, assign file ownership, integrate accepted changes and maintain the evidence ledger. |
| code-archaeologist | `C:\Users\User\.claude\agents\code-archaeologist\code-archaeologist.md` | Trace ribbon/home/menu/shortcut routes through controllers, engines, document state and output. Identify existing implementations before adding code. |
| solution-architect | `C:\Users\User\.claude\agents\solution-architect\solution-architect.md` | Review native module boundaries, save/undo/session contracts, ownership and dependency direction; propose the smallest necessary design changes. |
| backend-specialist | `C:\Users\User\.claude\agents\backend-specialist\backend-specialist.md` | Inspect service and persistence contracts behind commands, failures, cancellation and state transitions. Adapt relevant concepts to C++/Qt; hand native implementation to the executor. |
| ux-specialist | `C:\Users\User\.claude\agents\ux-specialist\ux-specialist.md` | Define task flows, command grouping, welcome content, context/disabled states, keyboard navigation and error recovery. |
| ui-specialist | `C:\Users\User\.claude\agents\ui-specialist\ui-specialist.md` | Specify and review Qt ribbon/home typography, icons, spacing, contextual controls, themes, density and screenshots. Use native Qt/QSS; pair with the executor for implementation. |
| gsd-roadmapper | `C:\Users\User\.claude\agents\gsd-roadmapper\gsd-roadmapper.md` | Reconcile the existing roadmap into finite phases with observable acceptance criteria and dependencies. |
| gsd-planner | `C:\Users\User\.claude\agents\gsd-planner\gsd-planner.md` | Turn approved phase outcomes into small executable tasks with exact owners, files, preconditions, checks and handoffs. |
| gsd-executor | `C:\Users\User\.claude\agents\gsd-executor\gsd-executor.md` | Implement scoped native repairs and UI/command changes, with tests and evidence; never independently certify its own work. |
| gsd-debugger | `C:\Users\User\.claude\agents\gsd-debugger\gsd-debugger.md` | Reproduce failures, test competing hypotheses and identify root causes. Use it for remaining baseline failures as well as new regressions. |
| performance-optimizer | `C:\Users\User\.claude\agents\performance-optimizer\performance-optimizer.md` | Measure native startup, rendering, navigation, OCR/batch throughput, memory and cancellation; fix evidenced bottlenecks and compare identical workloads. |
| security-auditor | `C:\Users\User\.claude\agents\security-auditor\security-auditor.md` | Static review of document, filesystem, process, credential, update and privacy boundaries. Preserve the role's read-only/static remit. |
| native-adversary | `C:\Users\User\.claude\agents\native-adversary\native-adversary.md` | Analyze owned native parsing/mutation/state boundaries; reproduce reachable failures and triage findings using safe synthetic fixtures. |
| fuzz-harness-engineer | `C:\Users\User\.claude\agents\fuzz-harness-engineer\fuzz-harness-engineer.md` | Write/run bounded native fuzz and differential harnesses in `fuzz/` or scratch, with deterministic seeds, timeouts and fail-loud results. Do not patch production code in this role. |
| testing-specialist | `C:\Users\User\.claude\agents\testing-specialist\testing-specialist.md` | Own QtTest/CTest workflow tests, UI action/state checks, real-artifact round trips and final regression validation; hand feature fixes to the executor. |
| guarantee-verification-engine | `C:\Users\User\.claude\agents\guarantee-verification-engine\guarantee-verification-engine.md` | Independently translate product claims into invariants and check evidence for save, redaction, signature/trust, PDF/A, offline operation and completion reporting. |
| emergence-engine | `C:\Users\User\.claude\agents\emergence-engine\emergence-engine.md` | After component reports exist, synthesize cross-component hypotheses and falsification plans. Hypotheses are not verified findings; route them to native/testing/fuzz for proof. |
| devops-engineer | `C:\Users\User\.claude\agents\devops-engineer\devops-engineer.md` | Verify clean builds, optional configurations, resources, dependency bootstrap, CI, reproducibility and package/release evidence. |

Several profiles are written around React/Next/Tailwind, Node/Supabase, Jest or Playwright. Apply their relevant engineering principles to this native app; do not introduce those stacks, browser benchmarks or URL-state rules just to follow examples. If a profile cannot execute native work, use it for bounded review/specification and route the native patch to `gsd-executor`.

Use the platform's real subagent mechanism if available. Keep at most three workers plus the coordinator active unless actual limits support a different safe count. Reuse idle workers with the appropriate role loaded; run sequentially if slots are unavailable and report that accurately. Do not impersonate independent agents in a single unreviewed answer or claim delegation that did not occur.

Each assignment must include: goal; exact base SHA; finding/command IDs; relevant input files; owned paths; off-limits files; required artifacts; validation commands; and exit criteria. Give one owner to shared integration files such as `GpMainWindow`, `RibbonModel`, `ToolId`, `ToolRegistry`, `TaskNav`, theme resources and CMake. No concurrent edits to the same file. Review worktree diffs before integration. A compiler failure is not permission to delete someone else's work.

## Execution waves

**Wave 0 — establish facts.** Orchestrator pins the baseline. Archaeologist creates the command inventory; architect reviews save/session/action boundaries; UX reviews the actual UI and task flows. Then roadmapper reconciles current plans and planner creates bounded implementation tasks. Start focused security/static review once relevant maps exist. Do not spend an entire run planning without addressing an executable repair.

**Wave 1 — restore correctness.** Reproduce and repair still-open G01–G06 first, including sibling callers at the same common boundary. Debugger and backend specialist provide reproduction/contract analysis; executor owns production fixes. Testing specialist adds meaningful failure/control cases. Work on independent UI specification and inventory can continue alongside these repairs.

**Wave 2 — finish command semantics.** Fix the remaining applicable quality findings and implement missing native command routes. Reconcile newer commits, redaction proof mode, measurement and research plans before duplicating work. For each visible verb, establish its specific result, requirements and persistence behavior. Review in small coherent batches.

**Wave 3 — implement the professional UI.** UI/UX specialists supply and review specifications; executor implements Qt changes under one integration owner. Expand the welcome screen, make hidden commands discoverable, wire available capabilities, provide precise availability states and improve contextual controls. Render and interact with the freshly built version after each meaningful layout change.

**Wave 4 — optimize measured bottlenecks.** Performance agent establishes a reproducible baseline and bounded target; executor fixes confirmed hot paths. Testing checks correctness and responsiveness. Keep lazy ribbon construction, local processing and bounded caches unless measurements justify changing them.

**Wave 5 — adversarial and independent gate.** Native adversary and fuzz engineer test owned boundaries; security auditor completes static review. Guarantee verifier checks claims. Emergence engine uses their completed reports to propose cross-component cases, then native/testing workers validate those cases. DevOps validates fresh builds/package provenance. Independent reviewers return defects to the executor and rerun the affected checks after fixes.

## Build the feature-to-command matrix first

Create `docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv` or a clearly dated successor. Inventory **every** ribbon entry, welcome card, task-navigation item, menu/context-menu action and shortcut, including hidden entries and actions with no surface. Columns:

`surface, tab, group, label, stable_command_id, aliases, visible, implementation_status, availability_reason, required_document_or_selection, controller, engine_operation, input_dialog_or_preset, output_or_state_change, undo_contract, dependency, source_location, test_or_repro, implementation_commit, review_status, evidence_path`

Classify separately:

- Implemented and available: visible, enabled, executable, with meaningful result.
- Implemented but missing context: visible, disabled until requirements hold; explain “Open a PDF”, “Select pages”, “Select a field”, or the actual prerequisite.
- Implemented but dependency unavailable: visible, disabled with specific local setup/readiness guidance. File existence alone cannot prove an OCR model is usable.
- Planned/unimplemented: visible in its intended ribbon group or an explicitly labelled overflow/catalogue, disabled and clearly marked “Planned”. Do not dispatch to a stub, show a false success, or silently hide it. Core commands required for this delivery must be implemented, not reclassified as planned to pass the gate.
- Obsolete alias or duplicate: document the actual semantic relationship; keep a useful alias or consolidate deliberately. Do not create multiple active buttons with different names that all open the same generic destination without selecting the requested operation.

Visibility, context enablement, dependency availability and independent verification are different dimensions. Do not encode them in one boolean or one manually maintained hidden-ID set. Prefer extending the existing command/capability metadata and canonical QAction/ToolRegistry path over a new framework. Check ribbon buttons, menus, shortcuts, command search and task navigation all obey the same state. A command's lower-level safety check still runs even when the UI disables it.

Specifically audit `src/shell/Ribbon.cpp`, `RibbonModel.cpp/.h`, `ToolRegistry.cpp/.h`, `src/core/ToolId.cpp/.h`, `src/shell/TaskNav.cpp`, `src/GpMainWindow.cpp`, `src/ui/WelcomeWidget.cpp`, controllers and capability probes. Update tests and stale comments that enforce the superseded hide-planned policy. Do not merely remove `if (planned.contains(t.id)) continue` and enable everything.

## Required ribbon behavior

Use the current eight tabs as the starting structure. Expand by genuine task groups; keep common operations directly visible and put lower-frequency variants in clearly labelled split buttons/overflow. Contextual property controls should appear for selected text, images, annotations and form fields. The accompanying UI review defines the detailed target groups.

- **Home:** Open/Create, Save/Save As/Print, Undo/Redo, Select/Hand/Snapshot, Find and useful shortcuts to Edit/OCR/Organize/Sign. Document search and command search must have distinct names and behavior.
- **View:** fit/zoom, page layouts, rotation for viewing, Thumbnails/Bookmarks/Comments/Layers, reading options, Compare and supported window modes. Pane commands must actually toggle the indicated pane and preserve state.
- **Edit:** edit existing text versus add text; add/replace/crop image and object deletion; links/attachments; contextual alignment/order controls; Run OCR, Review OCR, language/settings with correct preselection; measurement entry points and calibration. Do not equate visual covering with content deletion/redaction.
- **Organize:** Insert/Delete/Duplicate/Replace/Extract/Split/Merge/Reorder/Reverse/Rotate/Crop as supported, page labels versus printed page numbers, headers/footers, Bates, watermark/background. Different verbs must have distinct input/output semantics and safe batch destination handling.
- **Comment:** markup, notes, text boxes/callouts, drawing, stamps, comment list/filter/status/replies/import/export/summary as implemented or completed. Selection exposes style controls such as color, opacity and line width. Persisted annotations must participate in correct dirty/save behavior.
- **Convert:** distinct export targets and relevant settings; Office/images to PDF; table detection/review/extraction/CSV; compress/optimize/PDF-A; batch operations. “Compress” and “Reduce Size” cannot be duplicates pretending to be separate capabilities. Format/dependency limitations must be visible and truthful.
- **Forms:** field types, field properties, tab order, required/validation/calculation where supported, import/export/reset/flatten. Separate form design, fill and data operations. Validate the real saved PDF rather than only in-memory widgets.
- **Protect:** password/encryption/permissions/removal with the chosen operation preselected; mark/apply/pattern redaction, sanitize and proof review if present; sign/certify/timestamp/validate/trust. Preserve precise status distinctions and do not market a local signature as independently trusted or conformant without evidence.

Add only necessary missing commands for these workflows; reuse existing engines, dialogs and resources first. Broader capabilities requiring a new subsystem remain explicit roadmap items with honest visible availability. Deliver the agreed core set end to end before declaring this phase complete. Do not claim “all features complete” while planned entries remain.

Every label must identify a purpose. Use consistent verbs, capitalization, tooltips including prerequisites and shortcuts, recognizable icons and meaningful checked/active states. Repeated generic document icons are not sufficient for visually distinguishing unrelated tools. Avoid cramped abbreviations such as “Remove Sec.” when a clearer label or tooltip fits. Preserve ribbon collapse and keyboard access. At narrower widths use deliberate group overflow; no clipped labels or unreachable controls.

Resolve the concrete UI review findings: 52 hidden entries; Edit buttons visually enabled despite read-only action state; Convert/Protect welcome routes that only open a file; six-card/600-pixel home; missing camel-case icon resources and excessive generic icons. Reproduce the probe's menu/caption/canvas anomalies in the normal executable before treating them as product bugs. Simplify competing navigation where useful: the current menu/ribbon/mode strip/bottom task strip should not grow another persistent row just because more commands are exposed. Preserve the existing task registry and routes while improving their presentation.

## Required welcome screen

Replace the six-card, narrow centered launch area with a responsive task dashboard. Keep a prominent **Open PDF** action, **Create PDF** menu and file drop target, then a grouped quick-tools area and usable recent files. Start from 12 useful quick routes: Edit PDF, Convert PDF, OCR & Review, Compress PDF, Merge PDFs, Split/Extract Pages, Organize Pages, Annotate PDF, Fill & Sign, Protect/Redact, Office to PDF and Images to PDF. Provide Batch Tools, Compare and All Tools as additional clear destinations. Reuse existing workflows and let UX tune grouping to avoid crowding.

Primary actions and at least eight quick tools should be visible in a 1366×768 client-layout review at 100% scaling. Keep all routes reachable when Windows scaling reduces logical space; do not shrink labels to force a numeric target. Use a wider adaptive content area on large windows and fewer columns on small windows. Keep recent documents useful with filename/path context, missing-file handling, pinning only if there is a simple existing pattern, and a clear action to remove an entry without deleting the PDF.

Clicking a task from home must collect the correct input(s), then open the selected task with the document loaded. Merge collects multiple files; OCR opens OCR review/run; Compress opens compression options; Edit enters editing; Compare requests the comparison pair. Cancelling input returns cleanly to home. Do not route every card to Open PDF or a generic Convert/Protect screen and count that as completion.

Use restrained accent colors, consistent native typography and icon sizing, visible focus rings, useful hover/disabled states, readable light/dark themes, and adequate spacing. Avoid oversized branding, decorative empty space and fake metrics. Preserve local/private operation; no AI/cloud tile unless the actual available local feature and its limitations are accurately represented.

## Correctness, performance and architecture gates

Revalidate all G01–G23 and linked legacy rows. Priorities include: encrypted saves must never truncate; batch operations must preserve unrelated inputs/outputs; stale A→B→A asynchronous work must not write; recovery must save to the intended document; failed mutations must roll back resident state as well as disk; failed Undo must not advance history; crop/page-label/measurement geometry must round-trip; batch completion counts must reflect drained results; capabilities must preserve context restrictions; build/fuzz/release gates must fail on real failures. The quality report supplies exact reproductions and acceptance criteria.

Preserve one authoritative document identity and generation, explicit mutation transactions and operation outcomes, clear worker lifetime/cancellation ownership and layered dependencies. Reduce duplication at shared boundaries only when it removes an evidenced problem. Do not replace the application architecture for aesthetics or produce a generic abstraction layer with no concrete consumer.

For performance, record hardware, build mode, dependency versions, fixture hashes, sample count, cold/warm conditions and raw samples. Measure startup to first usable window, document-open to first page, first/repeated ribbon-tab activation, scroll/zoom responsiveness, text search, representative OCR/batch throughput, cancellation latency, peak memory and memory after repeated open/close. Use small text PDFs, a large text PDF and scanned/image-heavy PDFs with stated page counts. Never benchmark private user documents.

Choose numerical targets after baseline measurements and record them before modifying the hot path. Compare the same workload and build configuration; report median and tail measurements where sample count supports them. A faster operation with missing pages, lower output quality or incorrect completion is a failure. Fix measured bottlenecks first; do not add cache layers, threads or dependencies speculatively. Keep memory bounded and UI input responsive during long work. No invented speedup percentages.

## Independent verification and native UI review

For each enabled command, test from the actual visible surface, then inspect the result: expected mode/dialog/preset, active document identity, selection context, output contents, save/reopen, failure/cancel state and undo/redo where promised. Cover no-document, ordinary document, read-only, missing dependency, invalid input, repeated activation and document switch during work. Verify the same restrictions apply through menus/shortcuts/search. Registry presence alone proves wiring, not correctness.

Use synthetic PDFs and an isolated settings/profile directory. Run targeted QtTest/CTest cases after each repair and the full relevant suite at the final integrated commit. Record actual skips and capability coverage. Perform required Release/LTO and optional-feature configurations; a single translation unit compile does not prove an optional build. Reproduce known failures rather than weakening assertions, adding sleeps to hide races, or suppressing errors.

Review fresh-build screenshots and real interaction at 1366×768 and 1920×1080, plus Windows scaling 125%, 150% and 200% as available. Cover welcome, every ribbon tab, contextual controls, narrow-window overflow, light/dark, keyboard traversal, visible focus, screen-reader names and longer/RTL labels where supported. Automated offscreen screenshots are layout evidence only; do not call them native computer-use verification. Use the available native computer-use tools for the newly built app when supported. If unavailable, report that exact gap and continue source/Qt automation checks without claiming live interaction.

The already installed app predates these packages. Do not use it as evidence for the current code. Run a newly built executable against fixtures for development QA. After the quality gate passes, prepare the new installer and exact build identity for the user's separate reinstall/UI-review phase. Do not silently replace the installed app, publish a release or merge into `main`.

An implementer marks a row `implemented-awaiting-review`. Only a separate reviewer who examined the exact final code and independently reproduced the relevant acceptance checks may mark it `verified` under the existing plan protocol. Review after integration, not just isolated agent branches. Design proposals, simulated controls, unavailable external validators and unrun dependencies retain accurate unresolved statuses. Hypotheses from emergence analysis need reproduction before promotion to findings.

## Deliverables and finish criteria

Maintain concise, durable repo artifacts, dated to the actual execution:

1. A baseline manifest and implementation plan with agent/file ownership.
2. Feature-command matrix covering all surfaces, including hidden/planned/unmapped entries.
3. Finding-by-finding repair ledger with original ID, repro, fix commit, independent evidence and residual.
4. UI acceptance report with before/after screenshots, exact build IDs and command-by-command results. Include unsuccessful cases.
5. Performance baseline/comparison with raw measurements and remaining bottlenecks.
6. Build/test/fuzz/package logs and validator availability, plus an independent final verdict.
7. A resume checkpoint listing completed commits, active worktrees, dirty ownership, next executable tasks and blockers if interrupted.

Finish when the selected core workflows are implemented and independently checked; no known unresolved data-loss blocker remains; every existing/added UI command has truthful availability and a recorded disposition; the richer welcome/ribbon passes interaction/layout checks; required tests/build configurations pass with justified explicit skips; and performance changes have comparable evidence. If a genuine dependency or tool limitation prevents a check, record its scope and continue other work. Do not turn missing verification into success.

Final response: exact branch/commit and local-vs-published state, implemented user-visible changes, closed/reopened findings, remaining planned capabilities, measured performance results, gate verdict and clickable evidence paths. Keep implementation and independent verification clearly separated. Start now with baseline inspection and the first bounded agent wave.
