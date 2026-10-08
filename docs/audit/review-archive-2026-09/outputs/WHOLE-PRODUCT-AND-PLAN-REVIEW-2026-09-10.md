# GlyphPDF product and plan reconciliation — 2026-09-10

Finalized September 11; source cutoff September 10, published `9ba3cea` and separately identified local `b38a4fd`.

GlyphPDF has substantial local PDF functionality, but its plans, command surfaces and release claims are not yet a reliable account of what a user can complete. The immediate product priority is to finish and verify existing workflows: replacement must actually replace text, printing must honor the requested pages, enabled controls must perform their stated operation, and advanced signing must expose its required configuration. More ribbon entries or a longer feature list would not resolve those gaps.

## Scope and evidence boundary

This report began against immutable source at `4e2c99a`, then reconciled the published delta to `9ba3cea` using `work/whole-project-2026-09-10/published-source` and `published-delta.diff`. Unless stated otherwise, source references below point to that published snapshot. The inspected Replace, print, ribbon, welcome, comments and layer behavior persists at `9ba3cea`; newer OCR, annotation-sidecar, recovery, capability and signing-output repairs are acknowledged rather than reported again as unchanged defects.

Local integration `b38a4fd` adds form JavaScript Phase 1 after the published tip. It is a separate, unpublished state, recorded as **implemented-awaiting-review**. Its ledger records authorization for quickjs-ng Option A; authorization is not being requested or independently established by this report. The parent review reports a reproduced JavaScript deadline bypass and is documenting it separately, so this implementation must remain on hold for verification. A separate unintegrated quick-work branch also contains measurement CSV/XFA-banner work reported by the parent; those features are not counted as published here.

This is a product/source reconciliation, not an independent acceptance review of every package. No production code or installed application was changed. The older installed build was not inspected. Existing test counts and results quoted by the ledger are implementation evidence, not tests rerun by this report. The parent review's independent tests and release decision take precedence over broad feature summaries.

The [evidence ledger][ledger] reserves `verified` for independent acceptance review. This report does not upgrade any ledger row. `Implemented` below means a source path exists; `partial` identifies a specific remaining user-visible gap; `missing` means no implementing workflow was found in the inspected published source and relevant plans. None means a release-ready feature.

## Current product findings

### PP01 — Find & Replace does not implement content replacement

**Priority: P1. Source confirmed; saved-artifact reproduction still required.** Both single Replace and Replace All construct `QRectF(loc.x(), loc.y() - 15, 200, 20)` from a search location, rather than the match's actual bounds. They dispatch `editTextInline`. That backend draws an opaque white rectangle, then new text; it neither removes nor substitutes the original text operators. The original content can remain extractable while surrounding content inside the arbitrary 200-point rectangle is visually obscured. This is materially different from the controller comment claiming content-stream text substitution. [Replace handlers][replace] · [backend paint path][inline-backend]

The user-facing operation needs an explicit contract. Native replacement requires actual matched runs/glyph bounds, content removal or substitution, font handling, preservation of unrelated content and a layout-risk warning where necessary. Until that contract is implemented, do not present this path as trustworthy Find & Replace. Its visible unfinished control should explain the limitation and point to a supported alternative. It must never be presented as redaction.

Acceptance must examine saved PDF text and rendering: original term absent when replacement succeeds, replacement present exactly as intended, neighboring text unchanged, multiline/Unicode/subset-font and rotated-page cases handled or refused honestly. A screenshot showing new text over white is insufficient.

### PP02 — Replacement success, search options and read-only policy disagree

**Priority: P1. Source confirmed.** The separate regex/case/whole-word search path scans page text and navigates to the first matching page. It does not populate the `QPdfSearchModel` match set consumed by replacement. The controller first calls ordinary viewer search, and replacement then uses those literal-search rows. The replacement loops do not apply `searchText`, `useRegex`, `matchCase` or `wholeWords` to select their targets. A regular expression can therefore report matching pages while Replace has no corresponding matches, and case/word boundaries can differ between the reported search and the actual replacement source. The option path returns early for `ScopeAll`, so comments/bookmarks are not searched in that invocation. [Search implementation][search] · [Replace handlers][replace]

Single Replace has no `EditPolicy::mutationBlocked` check; FindBar connects directly to its controller slot. Replace All has that guard. The single-replace command also has no equivalent read-only guard. This is a direct bypass of the shared command-dispatch policy, not merely a button appearance mismatch. Single Replace announces success without checking an operation result. Replace All ignores each `editTextInline` boolean and reports the initial search-row count after the final save succeeds; save success does not prove every replacement succeeded. [FindBar wiring][findbar-wiring] · [inline command][inline-command]

`EditTextInlineCommand` adds another incomplete history contract: `redo()` ignores the edit result; `undo()` performs an unchecked insert followed by an unchecked delete. It does not use the newer atomic page-restore/checked-history approach used by repaired commands. Fix this command family together with replacement, then test the real FindBar route on read-only files, invalid patterns, failed edits, failed restore and retry. Count only successful mutations; do not claim a complete Replace All after partial failure.

### PP03 — Printing needs a correctness pass before print presets

**Priority: P1 for page selection and job ownership; P2 for output controls. Source confirmed; no printer or native dialog run performed.** After accepting `QPrintDialog`, `printDocument()` takes the full document page count, starts at page zero and advances through every page. It never reads selected page ranges or requested page order to build the print sequence. The application therefore generates all pages irrespective of those dialog choices. Each page is rasterized at a fixed three pixels per PDF point, approximately 216 DPI, and fit to the printer painter viewport. [Print implementation][print]

Each page creates a new `QThread` capturing the viewer's raw `QPdfDocument*`. That worker reads the live document before checking the viewer guard; the viewer owns this document as a QObject child and reuses it for loads. The destructor does not join these print workers. The `QPointer` in the later queued callback does not protect the earlier raw document dereference. A document switch/destruction during rendering is consequently a lifetime/identity risk; this report does not claim a reproduced crash. The callback also ignores `newPage()` failure, does not validate a non-null rendered image or an active painter, and the in-flight cancellation branch does not follow the same progress-dialog cleanup as the normal completion branch.

First define one owned print job with an immutable input identity, an explicit selected-page sequence, cancellation and a checked terminal result. Verify selected ranges, reverse order, blank/render-failed pages, cancel, close/switch, and print-to-file output. Then add N-up/booklet, duplex guidance, saved settings and print-appearance disclosure. A disclosure alone does not fix ignored selections or job lifetime.

### PP04 — Layers offers an operation it does not perform

**Priority: P1 for honesty. Source confirmed.** The sidebar enumerates layer names and initializes every checkbox as checked. On checkbox change it only displays “Layer … is now visible/hidden”; there is no engine/viewer visibility call. The backend provides name enumeration, not the visibility mutation required by that control. The synthesis's “missing layers” description misses this existing but misleading partial UI. [Layer toggle handler][layers] · [layer population][layer-list] · [layer enumeration][layer-engine]

Keep layer discovery available, but disable visibility controls with a specific reason until rendering and state management exist. When implemented, initialize from the document's actual configuration and distinguish temporary viewing changes from persisted layer edits. Test a fixture with one initially hidden layer and confirm rendered pixels change before claiming success.

### PP05 — Advanced signing is an engine capability without a complete user workflow

**Priority: P1 for claims; P2 for completion. Source confirmed.** Production source contains no callers of `setTsaUrl` or `setSignatureLevel`; only interface declarations and implementations were found under `src`. The engine defaults to a timestamp-capable level but has an empty TSA URL, so the ordinary production signing route effectively lacks the timestamp needed for its advanced profile. The Timestamp command cannot be made useful by the existing dialogs. The new backlog correctly calls out this engine/product distinction. [Signing configuration API][signing-api] · [send-for-signing plan][sign-plan] · [new backlog][backlog]

Certify is already a separate dispatched operation, and `SecurityController` passes `certLevel = 1`; the missing piece is permitted-change selection/explanation and the surrounding workflow, not an absent DocMDP implementation. Do not rebuild the crypto engine or repeat the synthesis's blanket “DocMDP missing” claim. [Certify dispatch][certify] · [fixed certification level][cert-level]

The published signing-output transaction repair improves failed-output preservation; it does not add TSA/profile configuration, recipients, signing order, package import, or completion auditing. Deliver local preparation/completion first, then consented TSA/validation choices and accurate attained-profile labels. Distinguish signature appearance, cryptographic validity, trust, timestamp availability and certification restrictions in the UI. Never call a local activity report proof of recipient identity or a server-enforced signing order.

### PP06 — Discoverability and action state remain inconsistent

**Priority: P2, following correctness repairs. Source rechecked at `9ba3cea`.** The ribbon still declares 144 entries and hides 52 via `plannedTools()`; the count was independently recomputed from the current source. Edit therefore exposes only a small subset of its declared workflow. Several hidden IDs correspond to functionality that exists elsewhere, including measurement, review filters/statuses and OCR options. A hidden row is not proof of a missing engine. [Ribbon hiding][ribbon] · [ribbon model][ribbon-model]

The [prior UI review][ui-review] measured actual control/action enablement divergence and catalogued icon/resource gaps on an earlier immutable build. The relevant ribbon construction and welcome source remain in the new snapshot, but its native screenshots and runtime observations must not be relabeled as fresh `9ba3cea` acceptance. The newer G11 capability fix preserves a foreign disabled state across transitions; that alone does not establish that every ribbon/menu/shortcut path shares canonical action state.

Follow the user's explicit requirement: unfinished tools remain visible, disabled and accompanied by a clear reason; working equivalents get canonical routes. Reuse `ToolRegistry`, `CapabilityRegistry` and existing task routing. Do not make inert buttons active to fill space. The existing feature-command matrix is the starting inventory; every entry needs a disposition and a real outcome, not a new competing command map.

### PP07 — Welcome Convert/Protect still discard task intent

**Priority: P2. Source confirmed.** The welcome view has six cards in a content column capped at 600 pixels. Convert and Protect still route to `_home->activate(ToolId::Open)` with no continuation into the chosen task. These labels promise more than the handlers deliver. [Welcome cards][welcome] · [welcome routing][welcome-route]

Expand the dashboard around real tasks—Open/Create, Edit, OCR, Convert, Compress, Merge, Split/Extract, Organize, Annotate, Fill & Sign, Protect/Redact—with Batch, Compare and All Tools reachable. Preserve the chosen task through file selection, cancellation and successful load. Use compact readable groups and responsive reflow; avoid extra permanent navigation rows. Verify actual window sizes and DPI in a freshly built native application, including 1366×768 and higher scaling.

## Tier-1 and Tier-2 reconciliation

These rows reconcile the September 8 synthesis, the September 10 backlog, current published source and ledger. Demand counts in the research corpus are research inputs, not independent market verification by this report. They should guide prioritization without turning uncertain competitor comparisons into release claims.

| Research item | Current published state (`9ba3cea`) | Remaining product work / correction |
|---|---|---|
| T1-1 Measurement | Implemented-awaiting-review: distance/perimeter/area, calibration UI, persisted measurement annotations; G21–G23 fixes recorded. | Calibration session-only; CSV export, per-viewport scales and appearance-stream value captions deferred. Separate local CSV work is not counted as published. Reconcile synthesis's stale MISSING label. |
| T1-2 Redaction Proof | Implemented-awaiting-review: saved-artifact manifest, hashes, survival sweeps, JSON/TXT proof pack, UI toggle and outcome wording. | Independently verify scope and failure disclosure. Image-rendered text lies outside string-sweep coverage; per-surface isolation fixtures remain incomplete. Proof must stay scoped evidence, not a universal audit/certification claim. |
| T1-3 Form JavaScript | Published source writes calculate/format actions but does not execute them. Local `b38a4fd` adds Phase 1 Calculate/Format, awaiting review. | Parent review found a local deadline bypass; hold until corrected and retested. Validate/Keystroke and document-level events remain later phases; format display is not page-render integration. Check packaging of the new runtime. |
| T1-4 Local send-for-signing | Design-only workflow; local signing and verification foundations exist. | Recipients/fields/order, request bundle, signed-copy import and completion report missing. TSA/profile setup and consent decisions must be implemented explicitly; do not claim the plan is a shipped envelope workflow. |
| T2-1 Named multistep batch presets | Existing seven single-operation batch paths and hot-folder infrastructure; multistep preset plan remains design-only. | Versioned preset data, per-step results, transactional per-file runner and import/export UI. G12 accounting prerequisite is now implemented; avoid restarting that completed repair. |
| T2-2 Find/Replace and search depth | FindBar/options and replacement handlers exist. | PP01/PP02 are correctness gaps, not simple missing ribbon wiring. Complete one match model, accurate mutation semantics, scope navigation and history before exposing wider routes. |
| T2-3 Comment review | Threads, filters, status changes, summary table and CSV export exist. Statuses are Open/Accepted/Rejected/Completed/Cancelled, with PDF state serialization. | Printable summary document/export remains incomplete; page-level navigation is weaker than geometry focus. Reconcile PRD “resolved” wording to the actual state model and verify persistence/undo. Do not reimplement statuses. |
| T2-4 Accessibility authoring | Reading-order heuristic/check UI exists. | Tag-tree authoring/repair, full checker/report, auto-tag workflow and PDF/UA acceptance remain incomplete. The reading-order heuristic is not PDF/UA validation. |
| T2-5 Legal batch | Cross-document Bates with continuity, collision preflight and re-anchor repair is implemented-awaiting-review. | Batch split-to-single-pages and batch password/permissions removal remain workflow gaps. Validate permissions, output naming and per-file failure behavior. Synthesis's single-document-only Bates claim is stale. |
| T2-6 Dynamic stamps/library | Basic stamp annotations exist. | Dynamic date/user/sequential stamps and managed/shareable library incomplete. New backlog identifies three unconnected menu stamp actions; fix the dead actions as a correctness item rather than wait for a large library project. |
| T2-7 DocMDP certify/approve | Certification engine and separate Certify dispatch exist; level 1 hardcoded in the controller. | Level choice and clear permitted-change semantics; advanced signing configuration. Reclassify as partial UI/workflow, not missing engine. |
| T2-8 OCG layers | Layer-name enumeration and checkbox list exist. | PP04: checkboxes only report a change. Functional view configuration, persistence, properties/order/create remain incomplete. |
| T2-9 Auto-bookmarks | Existing bookmark navigation/editing foundation. | No implemented style/heading/TOC generation workflow found. Keep planned until a real preview/accept operation exists. |
| T2-10 Print presets/N-up/booklet | Basic raster printing exists. | PP03 first; no implemented preset/N-up/booklet workflow found. Do not schedule cosmetic print disclosure ahead of selection correctness. |
| T2-11 Read-aloud/page transforms | Dark/eye-care UI exists. | No TTS execution or full page-content color-transform workflow found in the inspected source. Qt Speech is a proposed dependency, not an already delivered feature. |
| T2-12 Content-driven split/naming | Page/range/segment split implemented. | Bookmark/text-driven segmentation and reusable naming grammar remain incomplete. Batch-plan grammar is design, not an executing splitter. |
| T2-13 PDF/A accessible levels | 1B/2B/2U/3B/3U implementation and optional veraPDF validation recorded in ledger. | 1a/2a/3a depend on correct tagged output. Do not relabel the existing export selector as accessible-level support. Conformance evidence is fixture/profile-specific. |

Primary reconciliation references: [current ledger][ledger], [synthesis][synthesis], [new backlog][backlog], [batch preset plan][batch-plan], [form JavaScript plan][js-plan], [signing plan][sign-plan], [comments statuses and context menu][comments], [comment-state mutation][comment-state].

## Product-plan corrections

1. **Keep implementation status separate from release status.** The new backlog explicitly defines `SHIPPED-<ref>` as “ledger-verified, incl. implemented-awaiting-review.” That collapses two different states and contradicts the ledger's independent-review rule. Replace those labels with the exact ledger state plus the integrated/published revision. A source review, passing implementation tests or a design document cannot supply independent release acceptance.

2. **Retire stale “done” and “missing” snapshots.** PRD §27 remains dated v1.3.1/June 16 and labels editing, forms, OCR and other large domains done. Its v1.4/v1.5 list also names gaps that now have partial implementation. The synthesis similarly calls measurement, search, DocMDP and cross-document Bates absent despite newer code, and overstates advanced signing as a shipped advantage. Keep the PRD as requirements; link status to dated ledger/command-matrix entries instead of duplicating broad checkmarks. [PRD][prd]

3. **Clarify the active architecture and release scope.** The PRD's future React/FastAPI stack and desktop/mobile/web/enterprise aspirations coexist with the active C++/Qt desktop product. The roadmap retains a May 21 header, early-session completion tables and a scope lock requiring every PRD/roadmap/marketing claim before public release. Those are not a current execution schedule. State which requirements are release commitments, which are deferred and which are proposals; obtain product decisions through the existing process rather than silently treating research recommendations as authorization. [Roadmap][roadmap]

4. **Preserve design-only boundaries.** Batch presets and local signing remain plans. The form-JS plan was a dependency decision request; its later local implementation now has a separately recorded authorization/status, which does not make all phases complete. The new backlog calls the three plans “signed design docs” without itself providing approval evidence. Cite actual decisions and delivered phases, not that phrase.

5. **Use research to choose work, not promise unique superiority.** Statements such as “zero competitors,” “strongest in class,” “zero egress” and “no engine risk” exceed what this review can establish. The local signing workflow still contemplates TSA/OCSP requests, and measured redaction proof has explicit limitations. Use per-workflow locality and exact supported outcomes. Competitor claims require refreshed primary evidence before external use.

6. **Do not convert rough size labels into deadlines.** The new backlog's “tonight-feasible” one-to-three-hour estimates assume completed identity, persistence and UI seams. A small wire-up can still expose a destructive engine path. Acceptance should be a saved result, truthful cancellation/failure and appropriate independent evidence; code volume is not completion evidence.

## Professional UX and corporate workflows

The product can serve office and professional users without becoming a cloud document-management platform. Its clearest near-term opportunities are reliable local workflows:

| User workflow | What prevents completion today | Minimum coherent next delivery |
|---|---|---|
| Review a contract → compare → redact → obtain signatures | Local review/redaction foundations exist, but comparison needs the parent review's correctness fixes; printed review summaries and recipient completion workflow are incomplete. | Save/reopen review evidence, accurate compare output, explicit redaction proof limitations, then local signing-request preparation/completion. |
| Correct a scanned form and trust its totals | OCR review has new identity fixes; published forms do not execute authored calculations; local Phase 1 requires security repair/review. | Validate OCR result against document identity, secure calculation/format execution, field-attributed failures and visible compatibility limits. |
| Repeat a mailroom/legal batch | One-operation batch tools exist; repeatable multistep presets and content-driven splitting/naming do not. | A named local preset with per-file/per-step outcome, preflight collisions, deterministic names, cancellation and preserved previous outputs. |
| Print a selected contract section | Current loop generates every page and uses a live document across workers. | Correct selected-page job and checked output first; saved print settings second. |
| Give a colleague a review pack | CSV and state persistence exist; a printable annotated summary and richer import/export workflow are incomplete. | A summary with document identity, page anchors, current statuses and clearly stated included filters. |
| Administer an offline installation | PRD mentions SSO, licensing, DLP and dashboards, while desktop strategy defers that platform. A complete corporate management product is not established by an MSI alone. | Verified installer/update behavior, dependency/license inventory, deployment documentation and explicit policy settings. GPO/ADMX/offline licensing require scoped implementation decisions. |

For desktop usability, prioritize a single command identity, visible unavailable-state reasons, contextual properties, task-preserving welcome routes, readable ribbon groups/overflow, reliable keyboard focus and clear current-document/current-tool feedback. The side panels, ribbon, mode strip and task navigation should cooperate; adding another persistent row increases navigation burden. Native QA must cover dark/light/high contrast, scale factors, long translated labels, keyboard-only operation, cancel/error paths and real document viewport space.

## Recommended delivery order and exit criteria

1. **Close integrity/security failures on the selected integrated revision.** Include parent-review findings, PP01/PP02 and print job ownership. Preserve unfinished controls as visibly unavailable with reasons while repairs are incomplete.
2. **Finish canonical command semantics.** Reconcile all 144 current ribbon entries with the existing matrix, fix layers/stamp no-ops and welcome task continuation, and make enabled states agree across ribbon/menu/shortcut/task routes.
3. **Complete the smallest professional workflow packs.** Printable comment summary, reviewed measurement follow-ups, corrected print settings, named batch presets and local signing preparation/completion. Keep form-JS phases and advanced signing/network choices explicit.
4. **Run independent acceptance on the final integrated build.** Check meaningful saved artifacts with another read path where appropriate; use fault/cancel/read-only/identity cases; inspect a freshly built native UI and packaged installation. Passing one package does not certify adjacent features.
5. **Publish a reconciled claim set.** PRD, roadmap, backlog, command matrix, ledger and marketing should name the same supported behavior and limitations. `verified` is assigned per the independent protocol, and the published commit/installer identity must match the evidence.

The latest published repairs are substantial progress. They do not eliminate the remaining semantic gaps or justify treating the full PRD, research list, or unpublished branch as a completed professional product.

[ledger]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md
[replace]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/EditController.cpp:371
[search]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/EditController.cpp:237
[inline-backend]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/podofo/PoDoFoBackend.cpp:995
[inline-command]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/commands/EditTextInlineCommand.h:18
[findbar-wiring]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/GpMainWindow.cpp:394
[print]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/ui/PdfViewerWidget.cpp:1419
[layers]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/Sidebar.cpp:195
[layer-list]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/Sidebar.cpp:260
[layer-engine]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/podofo/PoDoFoBackend.cpp:773
[signing-api]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/SignatureManager.h:73
[certify]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/SecurityController.cpp:250
[cert-level]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/SecurityController.cpp:861
[ribbon]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/Ribbon.cpp:216
[ribbon-model]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/RibbonModel.cpp:6
[welcome]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/ui/WelcomeWidget.cpp:232
[welcome-route]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/GpMainWindow.cpp:216
[comments]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/ui/CommentsWidget.cpp:386
[comment-state]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/ui/CommentsWidget.cpp:874
[synthesis]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/research/synthesis.md
[backlog]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/research/RESEARCH-BACKLOG-2026-09-10.md
[batch-plan]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/research/batch-presets-implementation-plan.md
[js-plan]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/research/form-js-implementation-plan.md
[sign-plan]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/docs/research/send-for-signing-implementation-plan.md
[prd]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/PRD.md
[roadmap]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/ROADMAP.md
[ui-review]: C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UI-BUTTON-REVIEW-2026-09-09.md

