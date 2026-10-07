# PrintCraft/PdfCraft — where GlyphPDF's UI patterns would enhance it (2026-10-08)

Target: github.com/storytold/pdfcraft (renamed from PrintCraft 2026-10-07) — Rust/egui
"clean-room Acrobat clone", MIT OR Apache-2.0, created 2026-09-30, ~2,282 stars, ~2 human
committers + AI agents, 5 OS targets + web. Method: GlyphPDF UI/UX inventory (modes, shell,
themes, a11y, transactional/history discipline) mapped against PdfCraft's surfaces, flows,
issue tracker, and self-declared gaps.

## Their landscape (evidence in the research notes)

Strong: Acrobat mental model (mode tabs, tool names, dialogs), file safety (incremental
saves, atomic swap verified with qpdf, autosave, crash recovery, named undo surviving
saves), automation as first-class citizen (~123 JSON-Schema'd tools behind CLI `run`, MCP
server, loopback control channel, Rust API; `--root` sandbox + token), honesty affordances
(in-dev tools labeled with shipping milestone), 983-file corpus with 0 crashes, privacy
(no accounts/telemetry/cloud), day-one breadth (macOS universal, Win x64+x86, Linux
x64+ARM64, FreeBSD, WASM).

Gaps (their own issues + ROADMAP): unsigned/notarized builds; reading-flow navigation bugs
(#185/#186/#188); MCP `doc_info` returns annotation/link rects in raw PDF user space while
`comment_list`/`link_list` use displayed top-left coordinates — issue #129, dangerous for
exactly the agent workflows they court; editing existing text thin esp. CJK (#184/#155/#156);
OCR Latin-only searchable-image, batch files only, no verify screen; compare is text-only
report, no side-by-side; accessibility checker yes but no keyboard-only/screen-reader
operation, no Tags/Reading-Order panels; CLI second-class off macOS; bus factor ~2; Discord-
gated support.

## Where our UI enhances them (ranked)

1. **Coordinate-space discipline as an API contract (fixes their #129 class).** Our rule:
   every rect-returning service names its space (page-space vs display-space), converters
   are named and pinned, and view-layer artifacts never serialize (Rotate-View fallback,
   signature badges are view-layer only — ISO 32000-2 forbids embedding validation state).
   Their #129 is two services returning two spaces with no contract. The enhancement is not
   a patch — it is adopting the contract: `doc_info` documents its space, comment/link
   services match it, and a cross-space regression pin (two fixtures, one assertion each)
   makes the mismatch un-shippable.

2. **Offscreen-testable accessibility + painted-reality pins (their weakest axis).** egui's
   immediate-mode tree makes a11y an afterthought; their control channel exposes whatever
   egui reports. Our discipline transfers as patterns: accessibleName(+description) on every
   interactive surface including containers, verified headlessly; disabled-state parity
   probed on the *painted role* per widget family across every theme (pixel pins, not
   string checks); shipped-sheet page-visibility pins; HC AAA tokens (2px borders, no
   transparency, 7:1) as a third mandatory theme. Their `cargo xtask screenshots` harness is
   the natural home — our pin suite slots next to it.

3. **The honest-disclosure genre, systematized.** They label in-dev tools with a milestone
   (good, ad hoc). Our genre generalizes: planned/unbuilt = visible-disabled with reason +
   supported alternative on tooltip/statusTip/accessibleDescription (never tooltip-only);
   bounded-analysis truncation surfaced as warning rows, never silent under-reporting;
   "no verdict" language (a clean checker report is never a conformance claim); capability-
   composed tooltips from runtime queries (a build without a dependency must say so).
   Their ROADMAP's self-critical honesty table proves the culture is receptive.

4. **Pick-and-stage side-by-side composition (directly upgrades Organize + Combine).** Our
   ComposeMode: two live panes, checkbox page picks from either side, image inventory picker,
   a pending-transfer list that is the review of record carrying per-row size-mismatch
   disclosures computed *before* any byte is written, one checked-undo step per apply,
   transactional commit with byte-exact refusal. Their organize is single-document card-grid;
   their combine is a file-list dialog. The pick-and-stage cross-document model composes the
   two into the workflow their users keep requesting.

5. **Checked history — undo that cannot lie.** Their named undo survives saves (strong).
   Our addition: a traversal boundary above the undo stack that performs the restoration
   *before* moving the index, so a failed undo leaves the document consistent and retryable,
   and redo mirrors it. For an app whose brand is "never corrupt the user's file," undo that
   cannot desynchronize from document state is the missing guarantee.

6. **E-6 destination-identity guard.** Their atomic swap is verified; the residual risk is a
   second instance or sync client changing the destination mid-operation. Our commit refuses
   when the on-disk file changed between candidate staging and commit (byte-identical
   refusal is pinned). Small, and it closes the one hole in an otherwise excellent safety
   story.

7. **State-machine honesty for long operations (ReviewState pattern).** Their OCR/optimize
   flows are command-shaped; our `ReviewState` enum (Idle/Running/ReviewReady/Saving/
   RecoverableError) guarantees every terminal outcome restores the entry affordance — no
   stuck buttons. Plus our non-modal progress discipline: keep the dialog non-modal (modal
   `setValue()` pumps a nested event loop that can deliver `finished` inside a `setValue`
   frame — we observed the 0xc0000005), wire Cancel to worker-side probes at stage
   boundaries, owner closes the dialog after reading final state.

8. **OCR Verify 4-pane review screen.** Their OCR: Latin-only, searchable-image, batch.
   Beyond the engine gap, the *correction UX* is absent: our four-pane splitter (source
   scan with confidence overlay | recognized text | magnified word preview | page list),
   honest pane labels ("word corrections are saved, not this text"), and a ReviewState
   lifecycle is the professional correction workflow they will need the moment their OCR
   grows past Latin.

9. **Read-only enablement as ONE predicate.** Our EditPolicy: the dispatch boundary and
   every QAction's enabled state query the same predicate — a control can never claim an
   enabled state the dispatcher would refuse — while viewing/copy/export deliberately stay
   available. Their command registry (123 tools, one table) is the right chassis; adding
   the single read-only predicate closes the per-dialog check drift.

10. **Theme tokens as the single source for painted code AND stylesheets.** Our GpTheme
    constants feed both the QSS sheets and C++ painting (muted disclosures use `fg2()`,
    accent appears only on active/selected), with one coherent disabled-state family.
    egui's style system can adopt the same single-source pattern; their dark/light toggle
    becomes a third HC theme away from WCAG AAA parity.

## What we would take FROM them (honest reverse direction)

MCP/automation as a compiled-in, excludable feature with a JSON-Schema'd tool table;
`cargo xtask screenshots`-style reproducible README evidence; the corpus-sweep robustness
harness as CI; milestone-labeled in-dev tools in the first-user-facing surface; FreeBSD/
web packaging breadth. Their file-safety story matches ours; their CLI breadth exceeds ours.

## Sourcing

GlyphPDF inventory: modes/shell/themes/a11y files (this repo) + LANE-REPORT-r4-ux-2026-10-04,
LANE-REPORT-litems-2026-10-05. PrintCraft: github.com/storytold/pdfcraft (README, ROADMAP,
AGENTS.md, crates/ui-egui/src/chrome.rs, crates/automation, apps/pdfcraft-cli), issue #129,
PR #174, getartcraft.com, AUR printcraft, HN/Gigazine coverage.

## Screen-by-screen: the TaskNav strip vs PdfCraft's surfaces

GlyphPDF's mode strip carries 14 persistent task screens (STANDARD, OCR VERIFY, REDACTION,
SIGNATURES, MEASURE, COMPARE, PAGES, BATCH, AI CHAT, FORM BUILDER, COMPRESS, PDF/A,
ACCESSIBILITY, WATERMARK). PdfCraft's mode bar has five tabs (All tools | Read | Edit |
Convert | E-Sign) and flattens most professional tasks into dialogs over a single reader.
Screen-by-screen, what our layouts would add:

| Our screen | Layout (and why) | PdfCraft today | What our layout adds |
|---|---|---|---|
| STANDARD | Full viewer; 4 layouts; painted night-mode; rotate fallback with named disclosure; view-layer signature badges | Reader with tiled zoom, reading-order selection, form-field infobar (strong) | The disclosure discipline: engine limitations named on the surface (rotated fallback accessibleDescription), validity badges never serialized |
| OCR VERIFY | 4-pane splitter: page list \| source scan + confidence overlay \| RECOGNIZED·PREVIEW \| zoom word crop; ReviewState lifecycle (no stuck Run) | OCR dialog/flow only — Latin searchable-image, batch files; **no correction screen** | The entire professional correction workstation: simultaneous source/text/zoom comparison, honest pane labels, review lifecycle |
| REDACTION | Marks placed on the live canvas; pattern pills + danger-variant Apply; config panel below; exit leaves marks recoverable | Redact dialog: mark + Search & Redact + verification (strong) | Canvas-centered placement (theirs is dialog-first), in-process "local-only" disclosure on the surface, marks-recoverable exit semantics |
| SIGNATURES | Right dock over the live document — place-while-viewing; DIGITAL ID card; view-layer badges | Sign dialog + macOS Keychain; missing timestamps/LTV | Panel-not-mode hosting: the page stays center so placement is visual; badge state in the mode strip |
| MEASURE | Right dock: calibration presets/custom, snap, live readouts; uncalibrated = pt, disclosed | **No measure tool** | The whole screen; plus the honesty contract (calibration session-only, measurements persist as /Measure) |
| COMPARE | Side-by-side CompareWidget, linked scroll on-by-default, PREV/NEXT change nav, 5 change-type filter toggles, overlay, export | Text diff + PDF report + mark-as-comments; **no side-by-side view** | The visual comparison surface: synchronized panes + filterable change tree; their text report complements it |
| PAGES | 3-pane: page grid (stretch) \| split form 280 \| reorder 220; atomic drag permutation shared with keyboard; split filename preview | Organize card grid + multi-select + contextual toolbar (strong; split preview) | Keyboard-parity reorder + the undo-coupled grid (any undo anywhere reloads selection); both sides preview-first |
| BATCH | Queue UI: input panel 280 \| operation panel with progress + log pinned bottom; hot folder | Batch OCR files only | The full batch screen (convert/OCR/compress/watermark/redact/merge) with persistent per-item log |
| AI CHAT | 340px toggle dock, never takes the canvas | JS console; MCP; **no assistant surface** | Side-channel assistant hosting pattern |
| FORM BUILDER | 10 field pills → fields list \| live canvas \| properties; tab-order Apply mirrors read-only authority | Prepare-a-form authoring + fill + JS engine (strong) | Calculated-field UI, tab-order editor over the live canvas, properties-dock hosting |
| COMPRESS | Preset-card modal (560×580), honesty estimator | Reduce File Size + PDF Optimizer + space audit (their audit is stronger) | Preset-card UX + the new real engines (color-managed CMYK, TrueType+CFF subsetting — now genuine reductions, honestly estimated) |
| PDF/A | Right dock VeraPDF-backed validator + reading-order walk + truncation disclosures | pdfa_convert/verify CLI tools; **no panel** | The validation panel with identity-checked refresh (verdicts attach to the displayed file) |
| ACCESSIBILITY | Right dock checker + Tag preflight + honesty contract in the header; HC theme; offscreen a11y pins | Checker w/ 32 rules + alt-text; **no Tags/Reading-Order panels, no keyboard-only** | The panels they name as missing, plus the a11y-as-build-gate system |
| WATERMARK | WatermarkDialog | Watermark/background in Edit content | Parity |

**The meta-enhancement: the strip itself.** One static TaskNav table drives three nav
surfaces (bottom strip, Tools menu, tool↔screen sync) with one declared kind per task
(Standard / Workspace / Panel / Dialog / Toggle). PdfCraft's five-tab bar flattens
professional tasks into dialogs; a per-task kind means an OCR entry *is* the OCR screen,
signatures stay a dock over the live page, and compress stays a dialog — each task gets
the layout its workflow needs. Adding a 14-screen strip (and the one-table rule that
keeps it honest) is the single structural upgrade we would hand them.
