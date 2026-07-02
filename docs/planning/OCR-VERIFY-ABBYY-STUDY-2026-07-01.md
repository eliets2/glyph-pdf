# OCR Verify Screen — ABBYY FineReader Study & UI/UX Upgrade Plan

**Date:** 2026-07-01
**Scope:** UI/UX quality of GlyphPDF's OCR Verify screen (`src/modes/OCRMode.{h,cpp}`) only.
**Explicitly out of scope** (tracked and being fixed elsewhere per `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` §9.4):
- The interactive OCR→Accept path not persisting the searchable text layer (needs `exportMrcPdfA` wiring).
- The dead `orientDetect` flag / missing real orientation detection.
- The hardcoded-English OCR language selector (`EditController::runOcr()` ignores the picked language).
- `Re-OCR this region` running a whole-page re-OCR instead of a true region-scoped re-run.

This document is about making the review/proofing *experience itself* — layout, confidence signaling, correction interaction, navigation speed, batch operations — match or exceed ABBYY FineReader's, independent of those backend defects.

---

## 1. ABBYY FineReader's proven review-UI patterns

Sourced from ABBYY's official Help Center (FineReader 14/16 User's Guide, "Checking Recognized Text" and "Keyboard Shortcuts" pages).

1. **Three-pane synchronized layout in the OCR Editor**: **Image pane** (source scan, current region highlighted), **Text pane** (recognized, directly editable text), **Zoom pane** (magnified view of the exact word/character under the cursor). Clicking a word in the Text pane auto-scrolls and highlights the same location in the Image pane and refreshes the Zoom pane — this tight 3-way sync is the core proofing mechanism, not a nice-to-have.
2. **"Uncertain Characters" toggle button** on the Text window toolbar. When enabled, only low-confidence characters get a colored highlight inline in the running text (not every word — just the ones the engine itself flagged as doubtful). This keeps visual noise low: a clean page reads as clean, and the eye is drawn only to real risk.
3. **Next Error / Previous Error navigation** — dedicated toolbar buttons *and* keyboard shortcuts (**Alt+↓** / **Alt+↑** in FineReader 16) that jump the cursor (and the synced Image/Zoom panes) directly to the next/previous uncertain character or word. This is the single biggest speed lever: a proofreader never hunts with the mouse, they just tap a key repeatedly and fix what's flagged.
4. **Dedicated "Verify Text…" dialog** (**Ctrl+F7**, or Recognize → Verify Text…), separate from freeform inline editing. It steps through low-confidence words one at a time and offers:
   - **Skip** — dismiss this instance without changing text or engine confidence.
   - **Replace** — accept a suggested correction (from the recognition alternates or the dictionary).
   - **Add to Dictionary** — teach the language model this word so it's not re-flagged (this is the batch-productivity lever for repeated proper nouns/jargon across a long document).
   - A **symbol-insert control** for typing characters not on the keyboard (accented letters, special symbols) via Unicode code point or a picker.
5. **"Mark Text as Verified" (Ctrl+T)** — an explicit, keyboard-driven way to flag a block/page as human-reviewed, independent of whether every flagged word was individually visited. This is effectively a lightweight per-page "done" state, letting a reviewer triage a large document (verify the clean pages fast, spend time only on the messy ones).
6. **Dictionary/spell-check-assisted correction**: uncertain words are cross-checked against the selected recognition language's dictionary; when a word doesn't match any dictionary entry *and* has low OCR confidence, it is prioritized for review. Users can add domain terms to a custom dictionary so future documents in the same domain don't re-flag them — this is ABBYY's answer to domain-specific vocabulraces (legal, medical, technical documents).
7. **Whole-document comparison mode** (separate "Compare Documents" feature, not the same tool but same interaction family): **Ctrl+→ / Ctrl+← "Next/Previous Difference"**, **Del "Ignore Difference"** — the same next/prev-with-keyboard, dismiss-with-one-key interaction pattern reused across ABBYY's proofing surfaces. The consistency of this navigation idiom across features is itself a UX asset — users learn the gesture once.

**Net takeaway**: ABBYY's speed advantage is not a fancier confidence visualization — GlyphPDF's is already visually richer (see §2). ABBYY's advantage is entirely in **interaction efficiency**: (a) it only highlights what actually needs attention, (b) it lets you step through *only those flagged spots* with one key each direction, (c) it gives one-keystroke bulk dispositions (Skip / Replace / Add to Dictionary / Mark Verified) so a reviewer never needs the mouse for the repetitive 90% of a pass, and (d) dictionary learning reduces repeat-flagging across a document/session.

---

## 2. Side-by-side: GlyphPDF's OCR Verify screen today vs. ABBYY

GlyphPDF's screen lives entirely in **`src/modes/OCRMode.h`** / **`src/modes/OCRMode.cpp`** (class `gp::OCRMode`, ~741 lines), registered in the shell's screen strip as `"ocr" → "OCR Verify"` (`src/shell/ScreenNav.cpp:27`), driven by `EditController` (`src/shell/controllers/EditController.{h,cpp}`) which feeds it real `MergedOcrWord` results from `OcrPipeline`/`OcrEngine` (`src/engines/ocr/OcrPipeline.h`, struct `MergedOcrWord { text, boundingBox, confidence, sourceEngine }`).

| Aspect | GlyphPDF today (file/class cited) | ABBYY FineReader | Verdict |
|---|---|---|---|
| **Layout** | 4-pane `QSplitter` in `OCRMode::buildPanes()`: page list (`m_pageList`, fixed 180px) · image/confidence-overlay pane (`m_imagePane`/`m_scanContentLabel`, a read-only rich-text `QLabel` in a `QScrollArea`) · editable text pane (`m_textEdit`, a flat `QPlainTextEdit`) · zoom/legend pane (`m_zoomPane`/`m_zoomBig`/`m_zoomMeta` + static confidence legend). | 3-pane: Image · Text (editable) · Zoom, tightly synchronized on click/selection. | GlyphPDF's extra page-list pane is a genuine plus (ABBYY's page nav lives elsewhere in its shell). But GlyphPDF's 3 core panes are **not wired to each other** — clicking a word in the scan overlay does not move the text-pane cursor, scroll position, or populate the zoom pane. ABBYY's whole value is the sync; GlyphPDF has the boxes but not the wiring. |
| **Confidence visualization** | Per-word background+outline color coding directly in running text (`updateConfidenceOverlay()`): green ≥90, yellow 70–89, red <70, plus a numeric legend and aggregate `AVG CONFIDENCE` / `LOW-CONFIDENCE WORDS` labels in the info strip (`buildInfoStrip()`). | Single "Uncertain Characters" highlight color (binary flagged/not-flagged), no numeric confidence shown to the user. | **GlyphPDF is already ahead here** (confirmed by `docs/audit/COMPARISON-TABLES-2026-07-01.md` line 77, rated 🟡 Parity-or-better). Keep this; do not regress it while adding the interaction layer below. |
| **Correction interaction** | None. `m_textEdit` is a bare `QPlainTextEdit` — user can retype freely but there is no per-word alternates dropdown, no click-to-jump from a flagged word to its location for editing, no "Skip / Replace / Add to Dictionary" flow. `m_zoomBig`/`m_zoomMeta` exist but are never populated (`m_zoomBig` is hardcoded to an em-dash placeholder and there's no click handler wiring a word selection to it). | Verification dialog with Skip / Replace (from suggestion list) / Add to Dictionary, plus inline correction directly against the synced Image+Zoom view. | **Biggest gap.** GlyphPDF has zero structured correction workflow — it's "stare at colored text, retype by hand," which is strictly slower than ABBYY's guided flow. |
| **Navigation/keyboard speed** | None beyond normal text-edit cursor movement. No "next/previous low-confidence word" concept exists anywhere in `OCRMode.cpp`. Right-click context menu (`onImagePaneContextMenu()`) offers "Re-OCR this region" / "Accept this region" / "Reject this region" but these are mouse-only, whole-page-scoped (see out-of-scope note on Re-OCR), and not part of a review-stepping workflow. | Alt+↓/Alt+↑ Next/Previous Error, auto-syncs all 3 panes; Ctrl+F7 opens dedicated Verify dialog; Ctrl+T marks verified. | **Second biggest gap.** This is exactly the category ABBYY is famous for (proofing speed) and GlyphPDF currently has nothing in this category. |
| **Batch/bulk operations** | Page-level only: global `Accept` (`onAcceptResults()`) / `Reject` (`onRejectResults()`) buttons in the toolbar, plus the region-scoped context-menu Accept/Reject stubs (which today just re-enable the page-level buttons — see `onImagePaneContextMenu()` lines ~438–448 — they don't actually do per-region persistence). No "accept all words ≥ threshold X" or dictionary-style suppression of repeat flags. | Skip / Replace per-instance; Add to Dictionary suppresses future re-flagging of the same word document/session-wide; Mark Text as Verified for page-level triage independent of per-word review. | GlyphPDF's page Accept/Reject is coarser than ABBYY's per-word dispositions layered with page-level triage. No dictionary-style learning exists at all. |
| **Language/dictionary-assisted correction** | `m_langCombo` exists (12 languages) but per the audit is **UI theater** — `EditController::runOcr()` hardcodes the OCR engine invocation regardless of the picked language (tracked separately, not in scope here). No dictionary/spell-check suggestion mechanism exists in the review UI at all. | Dictionary-backed suggestion list per uncertain word; user-extensible custom dictionaries. | Not in scope to fix the language wiring here, but the *review-screen* dictionary-suggestion feature (independent of which language ends up running) is a legitimate UI/UX gap worth closing. |

---

## 3. Prioritized, concrete UI/UX changes for `OCRMode`

Ordered by proofing-speed impact per ABBYY's playbook. Each item names the actual file/class/widget to touch.

### P0 — Pane synchronization (the ABBYY core mechanic)
1. **Make `m_scanContentLabel` words clickable and drive `m_textEdit` + `m_zoomBig`/`m_zoomMeta` from the click.** In `OCRMode::updateConfidenceOverlay()` (`src/modes/OCRMode.cpp` ~line 488), wrap each per-word `<span>` with a stable `href`-style anchor (e.g. `<a href="word:INDEX">`) instead of a plain `<span>`, and connect `QLabel::linkActivated` (needs `m_scanContentLabel->setOpenExternalLinks(false)` + `setTextInteractionFlags(Qt::TextBrowserInteraction)`) to a new slot `onScanWordActivated(const QString &anchor)` that: (a) moves `m_textEdit`'s cursor to the corresponding character offset and highlights it (`QTextCursor::setPosition` + `QPlainTextEdit::setTextCursor`), (b) sets `m_zoomBig`'s text to the word and restyles it per its confidence tier, and (c) populates `m_zoomMeta` with `"<confidence>% · <sourceEngine> · bbox(x,y)"` pulled from the matching `MergedOcrWord` in `m_currentWords`.
2. **Reverse-wire text-pane cursor movement back to the scan pane.** Connect `QPlainTextEdit::cursorPositionChanged` on `m_textEdit` to scroll `m_imagePane`'s `QScrollArea` to the corresponding word's `boundingBox` region and flash/outline it briefly (a simple `QTimer`-driven temporary border-color change on the matching span is enough — full pixel-perfect scan-image highlighting is a larger project and can be a fast-follow).

### P0 — Keyboard-driven "next/previous flagged word" navigation
3. **Add `Alt+Down` / `Alt+Up` next/previous low-confidence-word navigation.** In `OCRMode` (header: add `void onNextFlagged(); void onPreviousFlagged();` private slots; add `int m_flaggedCursor = -1;` state), build a sorted index of `m_currentWords` entries with `confidence < 70` (reuse the existing threshold from `updateInfoStrip()`) each time `setOcrResults()`/`setSemanticDocument()` runs. Bind the shortcuts via `QShortcut` in the constructor (`OCRMode::OCRMode()`), each call driving the same word-activation path as item 1 (so keyboard nav and mouse click converge on one code path). This single change is the direct GlyphPDF equivalent of ABBYY's Next/Previous Error buttons and is the highest-leverage item in this whole list for proofing speed.
4. **Add visible Next/Previous toolbar buttons next to the existing Accept/Reject controls** in `OCRMode::buildToolbar()` (`src/modes/OCRMode.cpp` ~line 170), styled like `m_btnAccept`/`m_btnReject` (`QToolButton`, `variant=ghost`), with `accessibleName`/`accessibleDescription` set (matching the existing accessibility pattern already used for Run/Accept/Reject), so the feature is discoverable without knowing the shortcut, and shows current position (e.g. `"3 / 17 flagged"`) in a small label next to them.

### P1 — Structured per-word correction workflow
5. **Add a lightweight "Verify Word" popup/panel** triggered by clicking a flagged word (extending item 1) or by pressing **Enter** while a flagged word is focused via keyboard nav (item 3). Implement as a small `QMenu`- or `QFrame`-based popover anchored to the word's position in `m_scanContentLabel`, offering three actions mirroring ABBYY's Skip/Replace/Add-to-Dictionary: **Skip** (advance `m_flaggedCursor` without editing), **Edit inline** (focus `m_textEdit` at that offset, already achieved by item 1), **Mark as correct** (removes this instance from the flagged-navigation list without changing text — the GlyphPDF equivalent of ABBYY's per-word "this one's fine, stop bugging me"). This is new UI, best added as a small new widget class, e.g. `src/ui/controllers` or inline in `OCRMode.cpp` as a private helper — do not create a whole new dialog class unless the popover approach proves too cramped.
6. **Wire `m_zoomBig` to actually populate on selection** (it is currently a static em-dash placeholder per `OCRMode.cpp` line 329 comment `"— no selection"`) as part of item 1 — this alone closes a "looks unfinished" visual gap independent of the deeper interaction work.

### P1 — Page-level triage state (ABBYY's "Mark Text as Verified")
7. **Add a per-page "Reviewed" toggle to `m_pageList`.** In `buildPanes()`, give `m_pageList` items a checkable state (or a small trailing badge/icon drawn via a custom `QListWidgetItem` delegate) that the user can set with a **Ctrl+T**-bound shortcut (matching ABBYY's own binding, minimizing relearning cost for switchers) to mark the current page as human-verified independent of whether every flagged word was visited. Persist this state alongside `m_currentWords`/page results so re-opening the OCR Verify screen for the same document remembers which pages were already triaged — this is the fast-batch-review lever for long documents (skim clean pages, spend time only where flags cluster).

### P2 — Dictionary-style suppression / low-confidence batch actions
8. **Add a per-session "ignore this word going forward" list.** When the user chooses "Mark as correct" (item 5) or explicitly adds a word via a new small "Add to session dictionary" action, store the normalized word text in an in-memory `QSet<QString>` (or a small `SessionOcrDictionary` helper) on `OCRMode`/`EditController`, and have `updateConfidenceOverlay()` skip flagging any future occurrence of that exact text as low-confidence (still show its true engine confidence color, but exclude it from the Next/Previous-flagged index built in item 3). This is the direct, scoped equivalent of ABBYY's "Add to Dictionary" without requiring a persistent cross-document dictionary file (which would be a much larger, separate feature).
9. **Add an "Accept all ≥ N% confidence" quick action.** A small `QComboBox` or slider next to the existing Accept/Reject buttons in `buildToolbar()` (default threshold e.g. 90%) that, on click, marks all words at or above the chosen confidence as reviewed/skipped in the flagged-navigation index (item 3) in one action — letting a user quickly triage a mostly-clean page/document and focus manual attention only on the words below the chosen bar. This is additive to, not a replacement for, the existing whole-page Accept/Reject.

### P2 — Polish / discoverability
10. **Surface the confidence legend's thresholds and the new keyboard shortcuts in the existing `ShortcutHelpDialog`** (`src/ui/ShortcutHelpDialog.{h,cpp}`) so the new Alt+↓/Alt+↑/Ctrl+T bindings are discoverable the same way other GlyphPDF shortcuts already are — do not let this ship as a shortcut only power users discover by accident.
11. **Add a status-strip note when preprocessing settings are non-default or a preprocessing backend is degraded** (this overlaps with, but is distinct from, the already-tracked "surface which binarization/deskew path is active" P0 audit item — that item is about correctness/honesty; this one is purely about UI placement) so the new toolbar row (items 4/9) doesn't get visually cluttered as more controls are added — group Run/Accept/Reject/Next/Previous/Threshold controls into clearly separated `QFrame`+`VLine` sections following the existing separator pattern already used in `buildToolbar()` (`sep1`, `sep2`).

---

## 4. Implementation prompt (self-contained — hand to a fresh Claude Code session)

```
You are implementing UI/UX improvements to GlyphPDF's OCR Verify screen. This is a
C++17 / Qt 6 native Windows desktop PDF editor at C:\Users\User\Projects\pdf.

FIRST, read these two files in full before writing any code:
1. C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md
   (background on GlyphPDF's competitive position — orients you on why this work
   matters and what NOT to touch: the OCR language-picker wiring, the searchable
   text layer persistence bug, and orientation detection are ALL OUT OF SCOPE for
   this task and are being fixed in separate sessions. Do not touch
   EditController::runOcr()'s language/engine invocation logic or exportMrcPdfA
   wiring as part of this task.)
2. C:\Users\User\Projects\pdf\docs\planning\OCR-VERIFY-ABBYY-STUDY-2026-07-01.md
   (this document — contains the full ABBYY research, the current-vs-target
   comparison, and the prioritized, file-specific implementation list in §3).

YOUR TASK: Implement the P0 and P1 items from §3 of OCR-VERIFY-ABBYY-STUDY-2026-07-01.md
against src/modes/OCRMode.h and src/modes/OCRMode.cpp (class gp::OCRMode). In priority
order:
  P0-1: Clickable per-word spans in the scan/confidence pane wired to move the text-pane
        cursor and populate the zoom pane (currently a static em-dash placeholder).
  P0-2: Reverse-wire text-pane cursor movement to scroll/highlight the matching word in
        the scan pane.
  P0-3: Alt+Down / Alt+Up keyboard navigation between low-confidence (<70%) words,
        converging on the same word-activation code path as P0-1.
  P0-4: Visible Next/Previous toolbar buttons with a position indicator ("3 / 17 flagged"),
        matching the existing QToolButton/accessibleName/accessibleDescription pattern
        already used for Run/Accept/Reject in buildToolbar().
  P1-5: A lightweight per-word "Verify Word" popover (Skip / Edit inline / Mark as correct)
        triggered by click or Enter-while-focused.
  P1-6: (folds into P0-1) — confirm m_zoomBig/m_zoomMeta actually populate on selection.
  P1-7: Per-page "Reviewed" toggle on m_pageList items, bound to Ctrl+T, persisted so it
        survives re-opening the screen for the same document.

Also implement P2 items 8-11 if time/context budget allows, but P0+P1 are the required
minimum for this task to be considered complete.

Constraints:
- Do NOT regress the existing green/yellow/red per-word confidence overlay in
  updateConfidenceOverlay() — it is already ahead of ABBYY's single-highlight-color
  convention per the competitive audit; keep it and layer the new interaction on top.
- Do NOT change MergedOcrWord's fields, OcrPipeline, or any engine/backend code.
  This is a UI-layer-only task confined to src/modes/OCRMode.{h,cpp} and, if you add
  a small new widget/popover class, a new file under src/ui/ following the existing
  naming convention (e.g. OcrWordVerifyPopover.h/.cpp) plus its CMakeLists.txt entry.
- Follow existing code conventions in OCRMode.cpp exactly: QToolButton with
  variant="ghost"/"accent" properties, monoLab()/infoLab() helpers for labels,
  accessibleName/accessibleDescription set on every new interactive control,
  SPDX-License-Identifier: Apache-2.0 header on any new file.
- New keyboard shortcuts must be added to src/ui/ShortcutHelpDialog.{h,cpp} so they
  are discoverable (per §3 item 10).

Work on a NEW branch (e.g. ocr-verify-abbyy-parity-2026-07-01) off the current HEAD.
Do NOT push or merge — leave the branch local for review.

When implementation is complete:
1. Build the project (check CMakeLists.txt / README.md / CLAUDE.md at the repo root
   for the exact configured build steps — this project uses CMake + a MSYS2/ucrt64
   toolchain on Windows; do not assume a generic `cmake --build` invocation works
   without checking the documented flags first).
2. Run the full ctest suite and confirm it is 100% green — this is a hard gate.
   If any test fails, fix the regression before considering the task done; do not
   skip, disable, or weaken a test to make it pass.
3. Report back: which of the P0/P1/P2 items were completed, the build result, the
   full ctest pass/fail summary, and the branch name so it can be reviewed and
   merged manually.
```
