# GlyphPDF — Next Claude Code Session Brief

**Generated:** 2026-05-21 | **Project:** `C:\Users\User\Projects\pdf`
**Reference:** `SESSION_PROMPTS_V2.md` (full deliverables + success criteria per session)
**Amended roadmap:** `ROADMAP.md` (architectural decisions, dependency graph, license matrix)

---

## Project State at a Glance

### Committed (Sessions 1–6)

| Session | What was committed |
|---------|--------------------|
| 1 | `AppContext` shared_ptr DI + `Bootstrapper` wiring |
| 2 | `ToolId` enum + `QAction` dispatch via `ToolRegistry` |
| 3 | `IPdfBackend` / `IPdfRenderer` / `IPdfSearcher` abstractions; `PoDoFoBackend` |
| 4 | `PdfiumBackend`; 3-tier `RenderCache` (LRU, 256 MB, tiled, viewport prefetch) |
| 5 | `QpdfBackend`; save pipeline (PoDoFo → qpdf linearize); repair-on-open |
| 6 | `ISignatureManager` with `PAdESLevel` enum; PAdES B-LT/B-LTA; DSS/VRI; cert encryption |

All six sessions are clean commits; the project builds and all tests pass as of Session 6.

Key interfaces already committed to `src/core/interfaces/`:
- `IPdfEditorEngine.h` — full page-ops surface including crop/resize/reorder/header-footer/
  Bates/annotation embedding (these stubs were scaffolded in the remediation phase)
- `ISignatureManager.h` — `PAdESLevel` enum, `setTsaUrl`, `setSignatureLevel`,
  `signDocument`, `validateSignatures`

### Possibly Implemented but Unverified (Sessions 7–11 may be partially scaffolded)

The interface stubs in `IPdfEditorEngine.h` suggest Sessions 7–11 features were partially
scaffolded during the remediation phase, but it is NOT confirmed that:
- Session 7 redaction hardening (glyph-advance normalization, OCR text-layer scrub,
  structure-tree scrub, audit log) is fully implemented and tested
- Session 8 forms (radio, ComboBox, ListBox, date/numeric, FDF export/import) is complete
- Session 9 OCR pipeline (Tesseract via MSYS2, RapidOcrEngine, OcrPreprocessor, ROVER) is
  integrated
- Session 10 conversion pipeline (HTML/image/CSV/diff) is implemented
- Session 11 page operations (crop/resize/reorder as commands, header-footer injection,
  Bates numbering, thumbnail DnD) have implementations behind the scaffolded interface

**Do not assume any Session 7–11 deliverable is done until you have read the source files
and confirmed the implementation exists and tests pass.**

### What is NOT yet started (confirmed)

- Session 3.5 / Phase 1.5: `docmodel` + `pdfws_djot` + liblua (Djot interchange layer)
- Sessions 12–20: search/nav, watermark, MRC compression, accessibility, l10n, error
  handling, installer, auto-update, print polish, integration tests

---

## Amended Roadmap Summary

Three new workstreams have been incorporated into `ROADMAP.md`:

**Workstream 1 — OCR Ensemble Pipeline (extends Session 9)**
Layout-first pipeline (PP-DocLayoutV2 / Surya detects regions before recognition), dual
layout-detector ensemble (IoU reconciliation), word-level ROVER merge (Tesseract 5.5.x CPU +
PP-OCRv5 via ONNXRuntime GPU, confidence-weighted), heterogeneous lane scheduler (persistent
warm ONNXRuntime session — never spawn-per-page), cross-page pipeline
(`layout(P+1) ║ ocr(P) ║ fusion(P-1)`), per-word confidence in UI, per-region redo.

**Workstream 2 — Djot Interchange Layer (new Phase 1.5 + extends Sessions 8/9/10/13)**
`docmodel` (SemanticDocument tree) + `pdfws_djot` (Lua reference parser, MIT vendored).
Dual-model boundary: Structural (PDF object graph) ↔ Semantic (SemanticDocument).
Djot↔Semantic is lossless; Semantic↔PDF is explicitly lossy. Provenance guard blocks
born-PDF+signed Djot-edit-then-save-back. OCR output maps to Djot; annotation rich-text
modeled as Djot internally. Phase 1.5 = new Session 3.5 before existing Session 4 numbering.

**Workstream 3 — MRC Compression inside PDF/A (extends Session 13)**
MRC (Mixed Raster Content): JBIG2 bitonal mask + JPEG2000 background + invisible OCR text
sandwich. Text layer generated from WS1 word boxes (not hOCR round-trip). Expected 5–10×
size reduction for scanned content. DjVu EXCLUDED as output; import-only if needed.
License action item: audit JBIG2 encoder before merging (must not be GPL/AGPL).

---

## Environment Constants (all sessions)

```
Project root:  C:\Users\User\Projects\pdf
Qt:            C:\Qt\6.10.2\mingw_64
MinGW:         C:\Qt\Tools\mingw1310_64
vcpkg:         C:\vcpkg   (triplet: x64-mingw-dynamic)
Plugins:       set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins
Headless:      set QT_QPA_PLATFORM=offscreen
Build:         cmake --build build -- -j8
Test:          cd build && ctest --output-on-failure -j4
Context gate:  write C:\Users\User\Projects\pdf\STATE.md at 50% context usage
```

---

## Clarifying Questions — Answer Before Starting Work

The next session agent should ask the user (or you should answer here before pasting):

**Q1 — Verify or commit Sessions 7–11?**
The interface stubs in `IPdfEditorEngine.h` (crop/resize/reorder/addHeaderFooter/
applyBatesNumbering, etc.) suggest Sessions 7–11 may be partially or fully scaffolded.
Should the session agent:
- (a) Read the implementation files first and verify which deliverables are actually done,
  then commit any complete ones before starting new work?
- (b) Treat everything after Session 6 as unstarted and begin fresh from Session 7?
- (c) Start by running `ctest` and reviewing test output to determine what passes?

**Q2 — Which session to prioritize next?**
Given the interface stubs, two sessions are candidates for the immediate next work unit:
- **Session 7** (Redaction Hardening) — security-critical, marked BLOCKING; glyph-advance
  normalization, OCR layer scrub, structure-tree scrub, audit log, 15+ sanitization vectors
- **Session 11** (Page Operations) — interface fully scaffolded; crop/resize/Bates/DnD may
  already have partial implementations
Should we do 7 first (security blocking, linear dependency), or verify 11 first since it
may be closest to complete?

**Q3 — Insert Djot / WS2 as Phase 1.5 now or defer?**
Phase 1.5 (Session 3.5) is architecturally upstream of Sessions 8, 9, 10, and 13 (all of
which have WS2 integration points). However, it is not on the critical path for Sessions 7
and 11. Should Phase 1.5 be:
- (a) Inserted now (before Session 8 work begins) to establish the Djot foundation early?
- (b) Deferred until after Session 11 (so existing Session 4–11 work lands cleanly first)?
- (c) Deferred until Session 9 (since OCR→Djot output is the primary first consumer)?

**Q4 — Test targets for Sessions 7–11 features?**
The test suite was expanded during the remediation phase. For each of Sessions 7–11,
are there specific test file names or test targets that should be run to verify those
deliverables? Or should the session agent infer from `tests/` directory structure?
Specifically: does a `TestRedaction` target exist, and does `TestPageOps` exist?

**Q5 — Target timeline (weekly cadence vs. burst mode)?**
The `SESSION_PROMPTS_V2.md` header estimates 28–36 weeks total (20 sessions).
Are you running:
- (a) Weekly cadence — one session per week, giving time for review between sessions?
- (b) Burst mode — multiple sessions per day / weekend sprint until a milestone?
- (c) Milestone-driven — complete Phase 3 (security) before pausing, then resume?
This affects whether we should write a comprehensive `STATE.md` at each session end
or keep tighter context-gate thresholds.

---

## How to Start the Next Session

1. Paste this entire file as the opening message to Claude Code.
2. Before any implementation, instruct the agent to run:
   ```
   set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4
   ```
   and report all passing/failing tests.
3. Then read the relevant session prompt from `SESSION_PROMPTS_V2.md` for the chosen
   session number.
4. Follow the Standard Execution Protocol in `SESSION_PROMPTS_V2.md` (ANALYZE → PLAN →
   IMPLEMENT+VERIFY → CONTEXT GATE → FINAL VERIFICATION).

---

## Architectural Non-Negotiables (remind every session)

- **MuPDF (AGPL-3.0): NEVER link in-process**
- **Poppler (GPL-2.0+): NEVER link in-process**
- **qpdf in the signing path: NEVER — flattens xref, invalidates ByteRange**
- **Black rectangle redaction: NEVER — always excise from content stream**
- **Spawn-per-page ONNX process: NEVER — use LaneScheduler persistent warm worker**
- **Character-level OCR majority vote: NEVER — word-level ROVER only**
- **DjVu output format: EXCLUDED**
- **Djot grammar reimplementation: do NOT — vendor Lua reference parser**
- **All incremental saves must use PoDoFo WriteUpdate (not full rewrite) when signatures exist**

---

*Brief generated from: ROADMAP.md + SESSION_PROMPTS_V2.md + IPdfEditorEngine.h + ISignatureManager.h + LICENSE-3RD-PARTY.md*
