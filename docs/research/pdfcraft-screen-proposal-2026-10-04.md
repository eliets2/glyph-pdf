# Proposal: Professional task screens for PdfCraft

**Date:** 2026-10-04 · **Target:** github.com/storytold/pdfcraft (Rust/egui, M0–M14 roadmap)
**Author:** GlyphPDF integrator (comparative UX analysis; full context in
`printcraft-ux-enhancement-2026-10-08.md`, same folder)

---

## Executive summary

PdfCraft's mode bar has five tabs — **All tools | Read | Edit | Convert | E-Sign** — and every
professional workflow beyond those five opens as a *dialog over a reader* (Redact dialog,
OCR dialog, Compress dialog, Accessibility Checker dialog…). That is exactly right for
single-shot operations and exactly wrong for **review, correction, and monitoring workflows**,
which need persistent, purpose-laid-out screens with live state.

This proposal promotes **six professional workflows to persistent task screens**, in
PdfCraft's own design language (egui panels, milestone-labeling honesty, the shared command
registry), and shows the layout of each. The organizing idea is one structural import:

> **A workspaces strip** — a second, horizontal tab strip (below the mode bar) where each
> open *professional workspace* gets a persistent tab with live status. The reader stays
> king; workspaces are where work happens *on* the document.

GlyphPDF ships this as its ModeStrip (14 task surfaces). In PdfCraft it reuses the
machinery you already trust: the shared command registry, the automation tool table, the
document sessions/undo façade.

---

## Why screens, not dialogs — four arguments from your own repo

1. **Your Organize pages screen already proved it.** The card-grid with multi-select and a
   contextual toolbar outclasses any "Organize pages" dialog — you built it that way because
   page organization is *spatial and iterative*. OCR correction, accessibility repair, and
   comparison review are the same class of task.
2. **Long-running workflows need visible state.** OCR and optimization run minutes, not
   seconds. A dialog that closes on completion destroys the record; a workspace keeps the
   queue, the per-item outcomes, and the log while the run continues.
3. **Review is comparison-shaped.** Correction (OCR), repair (accessibility), and diff
   (compare) all need *two or more things visible at once* — source vs recognized, error vs
   context, doc A vs doc B. Dialogs cannot hold a 4-pane layout.
4. **Acrobat Pro — your stated parity target — agrees.** In Acrobat, Organize Pages, Scan &
   OCR, Prepare Form, and Accessibility tools are persistent tool workspaces with their own
   toolbars and panels, not dialogs. Screen-for-screen, these are the surfaces where Pro
   earns its price.

---

## The screens

### 1. OCR VERIFY — the correction workstation

**Why.** Your OCR is Latin-only searchable-image + batch files; there is no correction
surface at all. Every serious OCR product (Acrobat Find Suspect, ABBYY, GlyphPDF) treats
*review* as the core loop: recognition is never trusted, low-confidence words are flagged,
and the user corrects against the source image. The verify screen is also where your future
multi-script OCR lands — the screen outlives the engine.

**Layout (four-pane splitter):**

```
┌ Tool bar: [Language ▾] [Engine ▾] [Preprocessing ▾] [Run ▶] [Accept ✓] ───────────────┐
├────────────┬──────────────────────────────┬────────────────────────┬─────────────────┤
│ PAGES      │ SOURCE SCAN                  │ RECOGNIZED · PREVIEW   │ WORD ZOOM 200%  │
│ 1 thumb    │ (page image, low-confidence  │ (recognized text,      │ (magnified word │
│ 2 thumb ◀  │  words boxed amber/red,      │  corrections editable, │  crop +         │
│ 3 thumb    │  click = jump to word)       │  "saved, not this")    │  alternates     │
│ …          │                              │                        │ confidence 34%  │
├────────────┴──────────────────────────────┴────────────────────────┴─────────────────┤
│ Status: Reviewing page 2/7 — 14 low-confidence words · ReviewState: REVIEWREADY        │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

* Page list: per-page recognition status (done / flagged / failed).
* Source scan: the rendered page with confidence-colored word boxes; click a box → the text
  pane scrolls to that word, the zoom pane magnifies it.
* Recognized pane: editable text; honest label — corrections save into the invisible text
  layer, the preview is not the artifact.
* **ReviewState state machine** (Idle → Running → ReviewReady → Saving → Error): every
  terminal outcome re-arms Run. No stuck buttons, ever.
* Alternates row in the zoom pane: engine suggestions one click away.

**Build:** `crates/ocr` grows a per-word confidence + alternates output (already has the
data); `crates/ui-egui/src/ocr_verify.rs` renders the splitter. Engine calls go through the
shared registry (`ocr.verify`, `ocr.accept`) so CLI/MCP get them for free.

---

### 2. MEASURE — calibration and dimensional readouts

**Why.** Absent entirely — and it is a standard Acrobat Pro tool (engineering contractors,
print buyers, and CAD reviewers are your Legal/IT personas). Cheap to build (everything is
already rendered; you only need two clicks and a ratio), high perceived value.

**Layout (right dock beside the reader):**

```
┌ MEASURE ────────────────────────────────────────────────────────┐
│ CALIBRATION   [ preset: A4 297mm ]  or  [distance + unit]       │
│   "Uncalibrated: measurements are in pt"  (always visible)      │
│ TOOL   (•) Distance   ( ) Perimeter   ( ) Area                  │
│ SNAP    [x] Snap to points  [ ] Snap to edges                   │
│ ─────────────────────────────────────────────────────────────── │
│ READOUT   Distance: 142.3 mm   Δx 96.1  Δy 107.2  angle 41.8°   │
│           (live, follows the cursor until second click)         │
│ [x] Keep measurements as /Measure annotations on save           │
└─────────────────────────────────────────────────────────────────┘
```

* The reader stays center; the dock is where calibration lives.
* Honesty contract (pinned in the dock): measurements without calibration are pt; the
  calibration itself is session-scoped; measurements persist as /Measure annotations.
* Markup rendering in the reader: thin accent-colored lines + readout labels.

**Build:** pure `ui-egui` + a small measure markup type in the render layer. No engine
changes. ~1 week.

---

### 3. COMPARE — upgrade to side-by-side visual review

**Why.** Your compare is text-only with a PDF report. The comparison moment users pay for
is *seeing* the two documents with linked scrolling and walking a list of changes — the
report is the artifact, not the workflow. (GlyphPDF's Compare screen is exactly this and is
among its most-used surfaces.)

**Layout (full workspace):**

```
┌ [A contract-v1.pdf] [B contract-v2.pdf] [Swap] [Link scroll ✓] [Export report] ──────┐
├───────────────────────┬───────────────────────┬──────────────────────────────────────┤
│ DOC A (page 4)        │ DOC B (page 4)        │ CHANGES (9)         [text][moved]    │
│                       │                       │ ▸ Page 3: paragraph modified (p4↔p4) │
│  (rendered page A)    │  (rendered page B)    │ ▸ Page 5: page removed               │
│  changed regions      │  changed regions      │ ▸ Page 7: inserted (p7↔p8)           │
│  outlined             │  outlined             │ …                                    │
├───────────────────────┴───────────────────────┴──────────────────────────────────────┤
│ ◀ PREV   3 of 9   NEXT ▸                       [Overlay toggle]                      │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

* Linked scrolling on by default (toggleable); PREV/NEXT drive both panes.
* Change types filterable (text-modified / page-moved / page-removed / inserted).
* Your existing report export stays; the screen is the review layer above it.
* One click on a change → both panes scroll to it, outlines pulse once.

**Build:** `crates/compare` already produces page+rect changes; add rendered page views
(`render` crate) + a `compare_ui.rs` workspace. ~2 weeks.

---

### 4. BATCH OPERATIONS — the queue screen

**Why.** Your automation table has ~105 tools and batch OCR for files — but no surface where
a user *composes a queue, watches it run, and reads per-item outcomes*. The CLI `run
--script` is the engine; the screen is its face. Hot-folder monitoring lands here too.

**Layout (two-pane + pinned log):**

```
┌ Tool bar: [Add files] [Add folder] [Hot folder ▾] ───────────────────────────────────┐
├──────────────────────────┬───────────────────────────────────────────────────────────┤
│ INPUT FILES (14)         │ OPERATION PIPELINE  [OCR ▾] → [Compress ▾] → [Export ▾]   │
│ ☐ report-q1.pdf          │ [Options…] per step; steps run in order, one at a time    │
│ ☐ scan_batch/*.pdf (32)  │ ───────────────────────────────────────────────────────── │
│ ☐ …                      │ ▶ RUN   ■ CANCEL                                          │
│ [Hot folder: watch ▸]    │                                                           │
├──────────────────────────┴───────────────────────────────────────────────────────────┤
│ RUN LOG (pinned, scrolls)                                                            │
│ ✓ report-q1.pdf — OCR ok (2.1s) — Compress ok (0.8s, −62%) — Export ok               │
│ ✕ scan_03.pdf — OCR failed: page 4 has no text layer (skipped to export)             │
│ ⋯ 12 running — OCR page 6/32                                                         │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

* Pipeline = ordered steps from the same tool table the CLI uses (compose in UI, save as
  `steps.json`, run identically headless) — one pipeline definition, two front doors.
* Per-item outcomes persist in the log while the run continues; failed items are
  retryable individually.
* Cancel at stage boundaries; hot-folder mode turns the queue into a watcher.

**Build:** `automation` crate drives; `batch_ui.rs` renders. The tool table already has
JSON Schema — the pipeline editor is a structured list, not free-form. ~2–3 weeks.

---

### 5. ACCESSIBILITY — Tags tree + Reading Order + repair

**Why.** You have a 32-rule checker with alt-text tools — the audit half. The *repair*
half is missing: a Tags tree, a Reading Order panel, and a Setup Assistant. Your own gap
list says accessibility operation (keyboard-only, screen-reader) is not there; these panels
are where that work becomes visible, and the checker's findings should deep-link into them.

**Layout (two docks + assistant):**

```
Tags dock (left rail):                    Reading Order dock (right):
┌ TAGS ──────────────────┐                ┌ READING ORDER ─────────────────┐
│ ▾ Document             │                │ 1  H1  "Quarterly report" [▲▼] │
│   ▾ P                  │                │ 2  P   "Revenue grew…"    [▲▼] │
│     ▾ H1  "Report"     │                │ 3  H2  "Outlook"          [▲▼] │
│     ▾ L   (3 items)    │                │ ⚠ Unmarked artifact: logo (p1) │
│ …  [role badges]       │                │ [Auto-detect order] [Verify ▸] │
└────────────────────────┘                └────────────────────────────────┘
```

* Tags tree: structure with role badges, rename/re-parent/re-order, content highlighting
  in the reader on selection.
* Reading Order: numbered overlay on the page + list; drag to re-order; auto-detect as a
  starting point; unmarked-artifact warnings.
* Setup Assistant: the checker's findings become a checklist; each finding deep-links to
  the fix (alt text → image list; reading order → the panel; contrast → the value).
* Honesty contract (yours already): a clean report is "no gaps found by these checks,"
  never a conformance claim.

**Build:** `a11y` crate grows a tag-tree model (the cos layer has the objects);
`tags_panel.rs` + `reading_order.rs`. The checker→panel deep-linking uses the existing
finding IDs. ~3–4 weeks (the largest item; ship the Reading Order panel first).

---

### 6. PDF/A — validation + conversion panel

**Why.** `pdfa_convert`/`pdfa_verify` exist as automation tools but have no surface. PDF/A
is the compliance question every archives/legal user asks first.

**Layout (right dock):**

```
┌ PDF/A ─────────────────────────────────────────────────────┐
│ CURRENT   Not validated           [Validate ▸]             │
│ ────────────────────────────────────────────────────────── │
│ STANDARD  (•) PDF/A-1b  ( ) PDF/A-2b  ( ) PDF/A-3b         │
│           ( ) PDF/A-2u  ( ) PDF/A-3u                       │
│ CONVERT    [Convert & save as…]  (transactional save)      │
│ ────────────────────────────────────────────────────────── │
│ VERDICT    ✗ 3 violations  (details ▸)                     │
│  • 6.1.2 Font not embedded: Helvetica (p2, p5)             │
│  • 6.2.3 Image lacks Alt alternatives (p3)                 │
│  • 6.6.2 XMP metadata missing                              │
│ "A valid report is not a conformance claim." (honesty pin) │
└────────────────────────────────────────────────────────────┘
```

* Validate → verdict + violation list; Convert → transactional save; identity-checked so
  the verdict attaches to the displayed file.
* VeraPDF-class checks can be licensed/embedded later; the panel precedes it.

**Build:** wrap the existing tools in a dock; ~1 week.

---

### 7. (Phase 2) COMPOSE — cross-document pick-and-stage

Your Organize grid + Combine dialog cover single-document reorder and file-level merge. The
gap between them is *cross-document visual composition*: open A and B side by side, check
pages/images from either, stage them into a pending-transfer list with per-row disclosures,
apply as one undoable step. (GlyphPDF's ComposeMode is the reference implementation; the
layout and interaction are documented in the enhancement analysis.) Phase 2 because it
touches the session façade — the six screens above reuse existing crates.

---

## The navigation: where six screens live in your chrome

**Do not extend the five mode tabs** — Read/Edit/Convert/E-Sign are reader modes and should
stay a small, honest set. Instead, import the second strip:

```
┌ Tabs: [report.pdf •] [+]                    [⌘K] [Discord] [◐] ───────────────┐
┌ Mode bar: All tools | Read | Edit | Convert | E-Sign            [Find ⌘K] ────┘
┌ Workspaces: [OCR Verify] [Compare •2] [Batch ▶running] [Accessibility] [ × ]  │
├───────────────────────────────────────────────────────────────────────────────┤
│ (workspace surface or reader)                                                 │
```

* The **Workspaces strip** appears only when a workspace is open; each tab carries live
  status (a dot for unsaved review state, ▶ for a running batch) — your unsaved-dot idiom,
  reused.
* All tools keeps its catalogue; workspace entries get the same milestone-labeling honesty
  you already ship.
* Closing a workspace with unsaved review state → your existing unsaved-changes contract.
* Keyboard: Ctrl+Tab cycles workspaces; ⌘K already reaches every command.

This is GlyphPDF's ModeStrip pattern (one static table → strip + menu + sync; a `kind` per
entry; lazy construction) mapped onto egui. In your architecture it is one `WorkspaceBar`
widget + a `Workspaces` enum in the engine façade — no new crate.

---

## Effort and sequencing

| # | Screen | New code | Reuses | Est. |
|---|---|---|---|---|
| 1 | OCR Verify | ocr confidence/alternates output; ocr_verify.rs | render, text extraction, registry | 2–3 wk |
| 2 | Measure | measure markup + dock | render layer only | 1 wk |
| 3 | PDF/A panel | dock wrapping existing tools | pdfa tools | 1 wk |
| 4 | Compare side-by-side | compare_ui.rs workspace | crates/compare, render | 2 wk |
| 5 | Batch screen | batch_ui.rs + pipeline editor | automation tool table | 2–3 wk |
| 6 | A11y Tags+ReadingOrder | tag-tree model + panels | cos, a11y checker | 3–4 wk |
| — | Workspaces strip | workspace_bar.rs | engine façade sessions | 1 wk |

Sequencing: Workspaces strip → Measure (quick win) → PDF/A panel → OCR Verify → Compare
→ Batch → A11y panels. Each ships independently; none blocks another.

---

## Honesty rules carried over (your culture, formalized)

Every screen inherits the disclosure contracts this analysis documented in GlyphPDF, which
match the milestone-labeling you already do: in-development = visible-disabled with reason
+ alternative on every assistive channel; truncation/bounded-analysis disclosed as rows,
never silently; "a valid report is not a conformance claim"; private-profile and session-
scoped state named where felt; no fabricated content in loading states. The screens are
professional *because* the limits are on the surface.
