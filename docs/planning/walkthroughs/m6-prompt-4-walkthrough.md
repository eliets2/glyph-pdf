# M6-PROMPT-4 Walkthrough — Djot Annotation Rich-Text Model (WS2 role 3)

**Date:** 2026-06-01
**Result:** Shipped — djotSource model + Djot editor + save/load dual-write + tests. WS2 role 3 complete.
**Commits:** `386453e` (D1) · `b6ef672` (D3 + new transcoder) · `d7764df` (D4) · `219bb09` (D2) · `862bc05` (D5) · `f108830` (D6) · D7-MEMORY (this doc + CHANGELOG)
**Test count:** 26 → 27 (`TestAnnotationDjot` added, 12 cases)
**Build:** clean (MSYS2 ucrt64, Ninja). **ctest: 100% — 27/27, 0 failed.**

---

## What this prompt does

Makes **Djot the internal authoring model** for annotation + comment rich text. On save the Djot is transcoded three ways so the file is both spec-compliant and perfectly round-trippable:

| PDF entry | Content | Purpose |
|---|---|---|
| `/Contents` | plain-text projection of the Djot | spec-required fallback; Acrobat/Foxit render this |
| `/RC` | restricted XHTML per ISO 32000-1 §12.5.6.4 | rich-text interop for Acrobat/Foxit |
| `/PieceInfo /GlyphPDF /Private /DjotSource` | original Djot, escaped | lossless GlyphPDF→PDF→GlyphPDF roundtrip |

This honours the non-negotiable **"Raw Djot in PDF /RC NEVER"** — /RC always gets transcoded XHTML; the raw Djot lives only in the /PieceInfo sidecar.

---

## COMPLETED

### D1 — `AnnotationItem::djotSource` — `386453e`
- `src/core/AnnotationTypes.h`: added `QString djotSource` (internal rich-text model); `text` kept as the plain-text /Contents fallback.
- `src/core/AnnotationSerializer.{cpp}`: `toJson`/`fromJson` preserve `djotSource` → lossless `.ann` JSON roundtrip.
- Flows through undo/redo automatically (`EditAnnotationCommand` stores full `AnnotationItem` copies).

### D2 — InspectorWidget Djot editor — `219bb09`
- `src/ui/InspectorWidget.{h,cpp}`: the Contents section is now a **Djot source editor** + **formatting toolbar** (bold `**` / italic `_` / inline code `` ` `` / link `[..](..)` / list `- ` / heading `# `) + a **live read-only HTML preview** pane.
- Preview re-renders on every keystroke via `pdfws_djot::djotToHtmlFragment` (Qt rich text reads the XHTML body fragment directly).
- On commit (focus-out): writes `djotSource` and auto-derives `text = djotToPlainText(djotSource)` as one undoable edit. Existing annotations with no djotSource show their plain text as editable Djot, so nothing regresses.

### D3 — Save-time dual-write — `b6ef672`
- **NEW** `src/pdfws_djot/DjotToRichTextXhtml.{h,cpp}`: bounded inline+block transcoder for the annotation subset. `djotToXhtml` (full `<?xml…?><body xmlns=…>` per §12.5.6.4), `djotToPlainText` (markup-stripped projection), `djotToHtmlFragment` (preview). Registered in `src/pdfws_djot/CMakeLists.txt` (+ `Qt6::Core`); engines link it (root `CMakeLists.txt`).
- `PoDoFoBackend::embedAnnotations`: writes `/Contents` (plain), `/RC` (XHTML — only when real djotSource exists), and the `/PieceInfo /GlyphPDF /Private /DjotSource` sidecar via `writeDjotPieceInfo` (escaped with `pdfEscapeLiteralString`, the M1 injection-defence helper).

### D4 — Load-time restore — `d7764df`
- **NEW** `PoDoFoBackend::extractAnnotations(path)` (declared in `.h` in this commit; body shares `PoDoFoBackend.cpp` so it landed with D3): reads annotation dicts back into `AnnotationItem`, mapping `/Subtype`→`ToolMode`, `/Rect`→top-left `QRectF`, `/Contents`/`/NM`/`/T`/`/CreationDate`.
- Restores `djotSource` from the `/PieceInfo` sidecar via **NEW** `pdfUnescapeLiteralString` (`src/engines/podofo/PdfStringEscape.{h,cpp}`) — the exact inverse of `pdfEscapeLiteralString` (reverses `\( \) \\ \n \r \t \b \f` and octal `\ooo`), lossless for arbitrary UTF-8. Falls back to deriving a trivial djotSource from `/Contents` when no sidecar is present.

### D5 — Tests — `862bc05`
- **NEW** `tests/TestAnnotationDjot.cpp` (registered in `CMakeLists.txt`, qoffscreen deploy). **12 cases, all pass:**
  - `testRichTextRoundtripThroughPdf` — `*bold* and _italic_ and ` `` `code` `` → save → reopen → djotSource matches; /Contents == `bold and italic and code`.
  - `testPlainTextOnlyLoadsAsTrivialDjot` — plain annotation (no sidecar) loads with djotSource == plain text.
  - `testUnicodeRichTextRoundtrip` — UTF-8 markup survives the sidecar.
  - transcoder structure / XML-escaping / plain-text projection / heading+list.
  - `pdfEscapeLiteralString` ↔ `pdfUnescapeLiteralString` lossless (ASCII + UTF-8).
  - `AnnotationSerializer` preserves djotSource.
- Verified deterministic: `ctest -R TestAnnotationDjot --repeat until-fail:3` → 3/3 pass.

### D6 — Comment integration — `f108830`
- PROMPT-10 threading **already exists** (CommentsWidget `buildTree` + `replyToComment` + IRT `/IRT` linking + `parentId`/`replies`). D6 wires its rich text into the Djot model: `CommentsWidget::addComment`/`replyToComment` and the InspectorWidget reply composer populate `djotSource` (plain composer text = trivial Djot source) and derive `text` from it.
- The shared `embedAnnotations` dual-write therefore covers comments + replies identically to top-level annotations — no separate comment serialization path.

### D7-MEMORY — this walkthrough + CHANGELOG
- `CHANGELOG.md`: new **Djot Annotation Rich Text (M6-PROMPT-4)** section under `[Unreleased]` with the required summary sentence.
- Vault update: deferred to the orchestrator (this session does not edit `C:\Users\User\.claude\memory`). Suggested text is in the session report.

---

## DEFERRED / SEAMS (honest list)

- **Rich Djot composer for comments/replies** → `TODO(M6-P5)` in `CommentsWidget.cpp`. The CommentsWidget composer stays plain text; its text is treated as trivial Djot source and still dual-writes correctly. The full toolbar + live preview (as in InspectorWidget) is M6-PROMPT-5 scope per the prompt's D6 note.
- **App import-from-PDF path:** GlyphPDF's normal annotation persistence is the `.ann` JSON sidecar (`PdfViewerWidget::save/loadAnnotations`); the app does not yet replace its in-memory list from a PDF's annotation dicts on open. `extractAnnotations` is the real, tested PDF→AnnotationItem load path that makes the *PDF* roundtrip genuine and is ready for the UI to adopt when an "import annotations from PDF" entry point is added.
- **`/RC` heading rendering:** §12.5.6.4 has no `<h1..h6>`; headings render as bold `<p>` (closest spec-legal form).
- **Manual Adobe Reader check** (prompt `<verification>`): not performed in this headless session — no Adobe Reader in the MSYS2/offscreen environment. The /RC output is asserted well-formed + spec-subset by `testDjotToXhtmlStructure`; a manual Acrobat render should be done before tagging.

## TESTS ADDED vs SKIPPED
- **Added:** `TestAnnotationDjot` (12 cases). No tests were intentionally skipped.

## TODOs INSERTED
- `TODO(M6-P5)` ×2 in `src/ui/CommentsWidget.cpp` (rich Djot composer for comments/replies). No `TODO(audit-*)`/`FIXME`/`XXX`/`HACK` introduced.

## PRE-EXISTING ISSUES OBSERVED (not mine, not fixed)
- `TestBatchMode` is flaky under parallel ctest (Pattern 19) — failed once in the pre-work baseline, passed in isolation and on both post-work full runs. Untouched by this prompt.
- `reviewStateColor` unused-function warning in `CommentsWidget.cpp` — pre-existing static helper, not introduced here.

---

## Verification evidence
- Full build: clean (`ninja: no work to do` after incremental; all targets link, incl. `PdfWorkstation.exe`).
- `QT_QPA_PLATFORM=offscreen ctest`: **100% tests passed, 0 failed out of 27** (run twice, stable).
- `TestAnnotationDjot`: 12 passed / 0 failed; deterministic across 3 repeats.
