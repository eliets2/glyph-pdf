# GlyphPDF UI and button review — 9 September 2026

## Verdict and scope

The sparse UI has concrete causes: the ribbon hides 52 of its 144 defined entries; the Edit tab renders only five of 21; the welcome screen provides six cards in a column capped at 600 pixels. Two of those cards, Convert and Protect, only invoke Open rather than carry the chosen task forward. The next implementation should improve discoverability **and** command semantics, not just add controls.

This review uses immutable source at local commit `a5840dcfc6b2716e5b11e626a46ae3ec0ba0eaeb`, the independently built Release libraries and resources from that snapshot, and a small Qt UI probe. During this review the parity worktree advanced to `48087ad` and contained dirty measurement/backend/model files. A read-only comparison found no committed changes from the reviewed snapshot to that tip in the inspected ribbon, welcome, task navigation, command registry, main-window source and resources. This does not certify the new engine commits or dirty changes; the implementation session must fetch and rebaseline.

The probe instantiates the actual `gp::MainWindow` and bootstrap context, opens a generated non-private PDF, visits every ribbon tab and inspects actual widgets and registry actions. It does **not** execute every tool. Screenshots use Qt's offscreen platform and are not native computer-use or installed-app evidence. Offscreen font fallback required an explicit harness adaptation to load local Segoe UI/Consolas and substitute the unavailable Manrope/JetBrains Mono families. Typography and Windows scaling require fresh native verification. Do not treat a probe rendering anomaly as a product defect without a control in the normal app.

The previously installed app predates these packages and was not inspected. No production source or installed application was changed for this review.

## Reproducible evidence

Evidence directory:

`C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\ui-prompt-2026-09-09`

- `ui-probe.cpp` and `run.py`: probe and exact compile/run harness using the reviewed Release libraries.
- `command-inventory.json`: 144 ribbon records, per-tab counts, controller presence, real button state, canonical QAction state and welcome button names.
- `ribbon-command-baseline.csv`: the same ribbon entries in a format suitable for extending into GLM's full feature-command matrix.
- `images/`: welcome and all eight tabs, plus a narrow-window request. File names describe requested dimensions; use actual PNG/window dimensions before claiming a specific viewport passed.
- `agent-profiles.json`: entrypoint paths, SHA-256 hashes, role introductions and selected routing/mandate sections for all 18 user-specified agents. This is not a claim to have reviewed every profile reference library.
- `build.log`, `run.log`: compilation and execution evidence.
- `evidence-manifest.json`: source identity, screenshot dimensions, scope and evidence hashes. The portable bundle is [ui-review-evidence-2026-09-09.zip](./ui-review-evidence-2026-09-09.zip).

The quality findings G01–G23 remain in [QUALITY-GATE-2026-09-09.md](./QUALITY-GATE-2026-09-09.md); revalidate them against newer commits. Layout quality does not override a failed data-integrity gate.

The final probe reported one loaded PDF page. The 1440 screenshots are 1440×900; the requested 1100×768 window settled at 1124×768. The viewer canvas and thumbnail list did not paint the loaded page in this harness, so these images support review of the surrounding controls only, not document-rendering acceptance. Some captures also show menu/ribbon overlap or missing group captions. Reproduce those observations in the normal freshly built executable before promoting them to product defects; the probe does not execute the full application startup path.

Representative captures:

![Welcome screen rendered by the Qt probe](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/ui-prompt-2026-09-09/images/welcome-1440.png)

![Edit ribbon rendered by the Qt probe](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/ui-prompt-2026-09-09/images/edit-1440.png)

## Findings and implementation instructions

### UI01 — the ribbon deliberately hides a large part of the product model

`src/shell/Ribbon.cpp:216–236` skips entries contained in `RibbonModel::plannedTools()` and deletes groups whose entries are all hidden. The model/header comments still describe disabled planned buttons, which disagrees with the renderer.

| Tab | Defined entries | Hidden | Rendered | Whole groups hidden |
|---|---:|---:|---:|---:|
| Home | 13 | 4 | 9 | 0 |
| View | 19 | 6 | 13 | 1 |
| Edit | 21 | 16 | 5 | 2 |
| Organize | 15 | 3 | 12 | 0 |
| Comment | 21 | 7 | 14 | 1 |
| Convert | 22 | 10 | 12 | 1 |
| Forms | 19 | 5 | 14 | 1 |
| Protect | 14 | 1 | 13 | 0 |
| **Total** | **144** | **52** | **92** | **6** |

Counts are entries in the ribbon model, not independently implemented features. The inspected hidden string IDs did not have registered controllers through their current canonical lookup; some equivalent functionality may already exist in task panels or engines. A hidden-ID list cannot determine that by itself.

**Change:** inventory all hidden IDs end to end. For existing functionality, wire the intended command to its implementation and remove stale planned classification. For genuinely unfinished functionality, retain a visible disabled item with a clear Planned state and reason. Use labelled overflow for lower-frequency groups on small windows; do not silently hide the existence of a capability. Implement the agreed core missing workflows before calling the application complete. Update old AR-8 tests and comments to the user's new visibility requirement.

**Acceptance:** every one of the 144 current entries has a documented disposition; every new entry is added to the same matrix; no active control dispatches an unknown ID or a placeholder. In particular, review Arrange/Measure in Edit, Panes in View, Review in Comment, Batch in Convert and Validate in Forms.

### UI02 — ribbon enablement disagrees with the canonical action state

`Ribbon::makeTool` creates stand-alone `QToolButton`s with text/icon metadata, rather than using the canonical action. `ToolRegistry::actionFor` and `refreshEnabledActions` separately derive enablement from controllers. In the probe, after the document session became read-only:

| Edit button ID | Button enabled | Registry action enabled |
|---|---|---|
| `editText` | true | false |
| `addText` | true | false |
| `image` | true | false |
| `delete` | true | false |

The registry retains a dispatch-time safety guard. This finding demonstrates misleading UI state, not proof that those mutations bypassed read-only protection. OCR remained enabled in this probe and needs its own entry/review/export context checks.

**Change:** synchronize visible controls with canonical command/capability/context state. Prefer extending the existing QAction/ToolRegistry path; preserve the main-window task-entry routing so changing to `setDefaultAction` does not accidentally bypass OCR/Compare/Compress/Watermark workflows. Do not create another independent enablement map or remove lower-layer checks. Context and runtime availability must compose; a model becoming available must not re-enable a read-only mutation.

**Acceptance:** ribbon/menu/shortcut/search paths agree after open, close, selection change, read-only changes and dependency refresh. Tests check actual buttons and behavior, not only QAction values. Refused operations preserve active mode/history/document state.

### UI03 — welcome Convert and Protect do not retain task intent

`src/GpMainWindow.cpp:205–218` connects Open, Convert and Protect welcome signals to `_home->activate(ToolId::Open)`. Convert/Protect provide no follow-on operation in these handlers. Merge has a distinct Combine route, and Office/images use their own import routes.

**Change:** use a small task-entry contract: collect input, load successfully, then activate the selected task/tab/preset. For Convert, carry the requested conversion target or open the conversion chooser; for Protect, show the requested security operation or the Protect controls. Reuse current `TaskNav` and main-window routing rather than introduce parallel navigation machinery.

**Acceptance:** from a fresh welcome state, choose Convert/Protect, select a fixture, and reach the chosen workflow. Cancel and failed-open cases stay consistent. A test asserting “a file dialog opened” is insufficient.

### UI04 — welcome content does not expose the app's main tasks

`src/ui/WelcomeWidget.cpp:30–35` caps the content column at 600 pixels and the grid at three columns. Lines 232–267 create Open PDF, Merge files, Convert, Protect, Import Office and Images to PDF. Existing accessible names and keyboard focus on cards are useful foundations; preserve them.

**Change:** use a responsive task dashboard with prominent Open PDF/Create PDF/drop input, then about 12 quick routes grouped by purpose, with Batch, Compare and All Tools accessible. Use Edit, Convert, OCR & Review, Compress, Merge, Split/Extract, Organize, Annotate, Fill & Sign, Protect/Redact, Office to PDF and Images to PDF as the starting set. The implementation prompt defines exact entry behavior. Expand the content area on wider windows while keeping comfortable card/text sizes and accessible reflow on narrower windows.

Keep recent files visible and usable; distinguish “Remove from recent list” from deleting a file. Missing entries need clear recovery. Avoid oversized branding, promotional tiles and decorative empty panels. Do not add nonfunctional cloud/AI cards to increase density.

**Acceptance:** prominent input and at least eight quick routes are visible at the target 1366×768 layout/100% scaling, with the remaining routes easily reachable. At high scaling, preserve readability and access rather than squeeze the same column count. Every card opens its own workflow, including cancel/error behavior.

### UI05 — repeated and missing icons weaken command recognition

The model uses the generic `form` icon for 62 of 144 entries, including 32 of the 92 rendered entries. Five requested icon names (`zoomIn`, `zoomOut`, `editText`, `insertPage`, `deletePage`) have no corresponding SVG file or alias in the inspected resource collection. `src/util/Icons.cpp:17–36` looks up names directly and falls back to a circle when lookup fails. The rendered Edit Text fallback is consistent with that source path.

**Change:** map commands to appropriate existing registered icons first. Add a small coherent set of SVGs only for real gaps, following the current resource system and license policy. Audit all model/icon references against the compiled resource collection. Keep tooltips and labels informative even when icons are recognizable. Invalidate/rebuild icon state appropriately when themes change; verify rather than assume cached tints update.

**Acceptance:** no missing asset fallback for shipped commands; unrelated actions are distinguishable; icons remain legible in light/dark/high-contrast and at supported scale factors. Test resource existence separately from a non-null QIcon, because the fallback itself is a non-null icon.

### UI06 — a rich ribbon needs distinct operations and contextual properties

The model already names many useful verbs, but a row in `RibbonModel` is only a declaration. The current task table has its own routing semantics: for example, the OCR entry deliberately opens the OCR review workspace, whose Run button starts recognition. Renaming the ribbon entry without changing behavior will not create separate Run, Review, Language and Settings operations.

**Change:** define the result of each verb before adding its control. Where several buttons share a panel, they must preselect distinct relevant operations/options. Reuse the same underlying service where semantics are the same, and disclose aliases rather than count them as different features. Add contextual controls for selected content: text formatting, image operations, annotation style and field properties. Keep commands with dangerous or irreversible output semantics clear about the affected document and destination.

**Acceptance:** every enabled command has a real expected result and test, including no-document, selection, read-only, dependency, cancellation and save/reopen cases. Preserve undo guarantees; do not add Undo labels to operations the implementation cannot reverse.

### UI07 — dense controls require deliberate responsive behavior

The existing ribbon lazily constructs tabs, supports Ctrl+F1 collapse and uses a horizontal scroll area. Preserve those useful mechanisms while expanding it. More entries can make the layout harder to use if every command is made a large tile.

**Change:** prioritize a few large primary actions per group, use consistent compact rows for secondary commands, and labelled split buttons/overflow for variants. Make the active mode and selected properties clear. Keep document search distinct from command search. If adding command search, source it from the same registry and availability metadata. Audit tab/shortcut navigation, focus visibility, tooltips, longer labels, RTL where supported, and status feedback.

**Acceptance:** every command stays reachable at supported window sizes and scale factors; group labels and buttons do not clip; no blank placeholders or unexplained icons; keyboard users can enter/exit each region and return to the document. Native review must record actual window size/DPI, not infer it from a requested resize.

### UI08 — simplify competing navigation while increasing useful tools

The rendered workspace contains the menu, eight ribbon tabs, a separate mode strip, side-pane tabs and a bottom screen-navigation strip with 12 tasks. More capabilities should not mean adding another persistent navigation row. The current home also distributes its small amount of content across a large vertical area, pushing recent files close to the bottom even at 900 pixels high.

**Change:** make the ribbon the main task organizer, retain quick access to frequent workflows, and make task switching predictable through the existing task registry. Assess whether the bottom screen strip can become a compact task chooser or optional shortcut strip while preserving every route. Keep properties contextual. Bring welcome actions and recent files into a tighter, readable hierarchy with less vertical separation. Verify the observed menu/ribbon overlap and caption clipping in the normal executable before choosing a fix.

**Acceptance:** users can identify the current document, tool and task without interpreting three competing active-tab states. No extra chrome row is added merely to house new buttons. Every existing route remains reachable and produces the same canonical operation. Compare actual document viewport area before and after at the target window sizes.

## Target command groups

This is a product proposal for GlyphPDF, not a claim that the commands below all work today. The GLM session must classify and implement/complete them against the current engine and roadmap.

| Tab | Organize around these tasks | Important semantic distinction |
|---|---|---|
| Home | File, selection, find, history, frequent tasks | Open is not Convert; document search is not command search. |
| View | Zoom/layout, navigation panes, reading, compare/window | View rotation versus persisted page rotation; toggling a pane versus opening another task. |
| Edit | Existing text, added text, images/objects, links/attachments, OCR, measurement | Editing source content versus annotation; Run OCR versus review/export; calibration versus measured geometry. |
| Organize | Page operations, split/merge/extract, numbering, decoration | Printed page numbers versus page labels; replace versus insert; safe batch outputs. |
| Comment | Text markup, notes, drawing, stamps, review, selected-item style | Markup does not remove confidential text; changes must dirty/save the document correctly. |
| Convert | Export targets, create/import, tables, optimization, batch | Different export writers/options; compression results versus a marketing promise; PDF/A validation versus conversion. |
| Forms | Design, field properties, order, validation, data | Fill versus design; reset versus flatten; saved field behavior versus widget appearance. |
| Protect | Access control, redaction/sanitization, signatures, validation | Mark versus apply; signed versus trusted; structural checks versus full conformance. |

## Competitor research translated into concrete changes

PDF-XChange's current V11 workspace documentation describes grouped ribbon commands, configurable layouts, quick access, command search, navigation panes and contextual document controls. The useful lesson is predictable task grouping with multiple routes to the same command. GlyphPDF should keep a stable command identity across those routes and expose selected-object options close to the task. A larger button count alone does not reproduce that experience. [PDF-XChange V11 workspace basics](https://help.pdf-xchange.com/pdfxt11/workspace-basics.html)

Wondershare's current Windows guide is labelled PDFelement 13 and documents dedicated workflows across editing, creation, organization, OCR, forms and batch processing. Its homepage guide provides a direct Open PDF entry. These support a task-oriented home and task-specific launch routes in GlyphPDF. The proposed 12-card arrangement above is our design recommendation, not a copied or verified current PDFelement layout. [PDFelement Windows guide](https://pdf.wondershare.com/guide/pdfelement-windows.html), [Open PDF from Home](https://pdf.wondershare.com/guide/open-pdf-from-home.html)

An older V12 PDF guide's search index also exposes Quick Tools/batch workflows, but the full PDF could not be opened by the research tool and its linked home screenshot was not retrieved. Do not describe that material as a visually inspected current UI. Prefer the accessible current HTML guide and refresh vendor references during implementation.

Keep GlyphPDF's own branding, licensed assets and local-processing constraints. Do not copy subscriptions, cloud uploads, AI services or vendor-specific features simply to imitate their navigation.

## Delivery and review order

1. Fetch/rebaseline, reconcile new commits and owner changes, and build the complete feature-command matrix.
2. Repair still-open data-loss/session/undo blockers from the quality gate while UI/UX specifications proceed independently.
3. Finish native command semantics and common action-state wiring, then expand the welcome/ribbon and verify contextual properties.
4. Measure actual native bottlenecks and optimize only against comparable correctness-preserving workloads.
5. Run independent functional, adversarial, build/package and UI checks on the final integrated commit. Reserve `verified` for the plan's independent per-package protocol.
6. Prepare the newest installer after the gate; use the user's later reinstall/native UI review to close desktop-specific gaps. Do not review the old installed version as if it represented these changes.

The execution prompt is [GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md](./GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md). It assigns all 18 profiles, defines their native scope and sets the acceptance gates.
