# GlyphPDF — Months 2-8 Execution Prompts (Branch C SCOPE LOCK)

**Purpose:** Full 7-H expansion of every Claude Code launch prompt for the remaining v1.0.0 work after Month 1 + MSYS2 migration. Each prompt is self-contained; paste any one as the first message in a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`.

**Pre-conditions for using these prompts:**
- Month 1 work committed (commit `a6ea6aa` or later; 12+ tests passing).
- MSYS2 migration complete (`UnitTests` + `TestControllers` now passing; 14/14 ctest green).
- Working tree clean before starting any prompt.

**Branch C SCOPE LOCK:** real public v1.0.0 does NOT ship until M8-PROMPT-3 completes. Current `dist\GlyphPDF-1.0.0-x64.msi` is private/internal. CHANGELOG entry `[1.0.0-internal]` marker exists. Do not publish anything labeled v1.0.0 until M8 launch.

---

## Index

| Month | Sprint focus | Prompts | Effort |
|---|---|---|---|
| **M2** | Security tier completion + LaneScheduler infrastructure (WS1 dep) | 5 prompts | 5-6 weeks |
| **M3** | Mode-page completions (4 preview banners + Inspector Properties) | 5 prompts | 4 weeks |
| **M4** | 23 ribbon tools → real engine wiring + **WS2 Djot foundation (Phase 1.5)** | 7 prompts | 5-6 weeks |
| **M5** | OCR ensemble (WS1 layout+OCR) + Office import + **WS2 OCR→Djot mapping** | 4 prompts | 5 weeks |
| **M6** | DiffEngine LCS + i18n + AI + **WS2 Djot rich-text annotations** + comments + edge cleanup | 7 prompts | 5 weeks |
| **M7** | Hardening (audit + bug bash) + **WS3 MRC compression pipeline** | 3 prompts | 6 weeks |
| **M8** | Launch (marketing, governance, release) | 3 prompts | 4 weeks |
| **Total** | | **34 prompts** | **34-36 weeks (~8-9 months)** |

**Three integrated workstreams (WS1 + WS2 + WS3 per ROADMAP Phase 1.5/5/6):**
- **WS1 — Parallel Layout + OCR Ensemble Pipeline.** Extends Phase 5 (OCR). Layout-first SOTA (PP-DocLayoutV2 + Surya layout ensemble with IoU reconciliation) → per-region Tesseract + RapidOCR PP-OCRv5 fan-out → word-level confidence-weighted ROVER. **LaneScheduler (M2 infrastructure) — GPU lane warm worker (never spawn-per-page) + CPU lane (QtConcurrent core-count) + cross-page pipelining (layout(P+1) ‖ ocr(P) ‖ fusion(P-1))**. Per-region redo + fused per-word confidence in OCRMode review UI.
- **WS2 — Djot Full Document Interchange.** New Phase 1.5 (M4) + cross-cuts Phases 5-6. Dual-model architecture: Structural model (PDF object graph, source of truth) ↔ Semantic model (`docmodel::SemanticDocument`, editing/interchange). Djot ↔ Semantic = LOSSLESS; Semantic ↔ PDF = EXPLICITLY LOSSY. ProvenanceGuard refuses edit-via-Djot-save-back for signed/born-PDF docs. `pdfws_djot` lib vendors the Lua 5.4 reference parser (MIT — no grammar reimplementation). Three roles: (a) OCR output mapping (M5); (b) authoring input (M5); (c) annotation/comment rich text — Djot internal model transcoded to /RC XHTML + /Contents plain text on save, original stashed in /PieceInfo (M6).
- **WS3 — MRC Layered Compression in PDF/A ("Modern DjVu").** Extends Phase 6 (M7). Layout/mask separation → JBIG2 bitonal foreground (text/line-art) + JPEG2000 background (continuous tone) + invisible OCR sandwich layer aligned to WS1 word boxes → PDF/A-2b. Target 5-10× size reduction for scanned content. **No DjVu output** (legacy; dead browser support); optional DjVu importer if legacy corpus ingestion needed. License check: JBIG2 encoder + OpenJPEG (JPEG2000) must be permissive.

Many prompts within a month can run in parallel sessions (e.g., M3-PROMPT-1 through M3-PROMPT-5 touch different files). The schedule assumes 2-3 parallel sessions where feasible. **Dependency edges:**
- LaneScheduler (M2-PROMPT-5) → consumed by M5 (OCR ensemble), M7 (MRC pipeline), and any future GPU workload.
- Djot foundation (M4-PROMPT-7) → prerequisite for M5 OCR→Djot mapping, M6 Djot annotation rich-text, M7 MRC sandwich text alignment.
- WS1 OCR word boxes (M5-PROMPT-1/2) + WS2 Djot (M4-PROMPT-7) → BOTH required before M7-PROMPT-3 MRC pipeline.

---

## Standard Execution Protocol (inherited by every prompt)

```
PHASE 0 — READ VAULT MEMORY (NEW; mandatory for token savings)
  Before reading any source file, read these vault notes in this order:
    1. C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md
       → 17 cross-project agent failure modes; read Patterns 1-4 minimum.
    2. C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md
       → GlyphPDF-specific lessons + meta-observations.
    3. C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md
       → architectural constraints to enforce.
    4. C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md
       → current commit + test status (don't make stale assumptions).
    5. C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md
       → dependency-aware execution order (confirm your prompt's prerequisites).
  The PROJECT CLAUDE.md auto-loads when session opens in C:\Users\User\Projects\pdf — no re-read needed.
  Skipping Phase 0 = re-deriving context from source = ~5,000-10,000 tokens wasted.

PHASE 1 — ANALYZE
  Read every file in <files_to_read>. Map the current implementation.
  Identify integration points and potential conflicts.

PHASE 2 — PLAN
  For each deliverable, write a brief approach comment.
  Identify the order that minimizes rework.

PHASE 3 — IMPLEMENT + VERIFY
  For each deliverable:
    a. Write the code
    b. Build: cmake --build build --parallel 8
    c. If build fails → read error, fix, rebuild. Do NOT proceed with errors.
    d. Test: $env:QT_QPA_PLATFORM='offscreen'; cd build; ctest --output-on-failure -j4
    e. If tests fail → isolate, diagnose, fix, re-run full suite.
    f. INDEPENDENT verification per Pattern 1 (agent-execution-anti-patterns):
       - Check build/*_results.txt mtime is NEWER than start of this deliverable's work
       - tail -30 the relevant result file; verify Totals line shows expected pass count
       - DO NOT claim "tests pass" without these two checks
    g. Atomic commit: git add [specific files] && git commit -m "feat(scope): D# description"
       (Per Pattern 4: atomic per-deliverable commits, NOT a single mega-commit)

PHASE 4 — CONTEXT GATE
  If context usage past 50%, write STATE.md and stop.
  Do NOT rush remaining work. Quality over completion.

PHASE 5 — FINAL VERIFICATION
  Run the prompt's <success_criteria> checks.
  Every criterion must pass before declaring complete.
  Walkthrough.md must explicitly list (Pattern 10):
    - what's complete (with test names that pass)
    - what's deferred + reason
    - what tests added vs skipped
    - any TODO comments inserted (must be zero in production-bound source)

PHASE 6 — UPDATE MEMORY (NEW; mandatory)
  If you discovered a NEW failure mode not in Patterns 1-17:
    - Add Pattern 18+ to vault knowledge/agent-execution-anti-patterns.md
    - Cross-link from projects/glyphpdf/08-lessons-learned.md
  If you closed a CHANGELOG Known Issue: remove the bullet.
  If you discovered a new architectural constraint: add to 06-non-negotiables.md.
  Update projects/glyphpdf/07-sessions-log.md with this session's entry.
  Append a 1-line summary to projects/glyphpdf/01-current-state.md commit table.
```

**Why Phase 0 + Phase 6 are mandatory:** the vault is the project's institutional memory. Sessions that don't read it re-derive context (wastes ~5-10K tokens). Sessions that don't update it leave the next session with stale info (compounds across 34 prompts).

**Environment (post-MSYS2 ucrt64 migration):**
- PATH: `C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;$PATH`
- QT_PLUGIN_PATH: `C:\msys64\ucrt64\share\qt6\plugins`
- QT_QPA_PLATFORM: `offscreen` (for headless tests)
- Build: `cmake --build build --parallel 8` (Ninja generator)
- Test: `$env:QT_QPA_PLATFORM='offscreen'; cd build; ctest --output-on-failure -j4`

**Reference docs in repo (NOT on Desktop anymore — moved to docs/planning/):** `PRD.md`, `ROADMAP.md`, `CHANGELOG.md`, `SESSION_BRIEF_NEXT.md`, `CLAUDE.md` (auto-loads), `docs/planning/AUDIT-v1.0.0.md`, `docs/planning/MONTHS-2-8-PROMPTS.md` (this file).

**Reference notes in Obsidian vault** (`C:\Users\User\.claude\memory\`):
- Project branch: `projects/glyphpdf/{00-overview, 01-current-state, 02-architecture, 03-build-environment, 04-scope-lock, 05-prompts-index, 06-non-negotiables, 07-sessions-log, 08-lessons-learned, 09-license-policy}.md`
- Cross-project knowledge: `knowledge/agent-execution-anti-patterns.md` (17 patterns; tool/stack-agnostic; applies to ALL projects in workspace)

---

---

## 🔒 PROTOCOL ENFORCEMENT (every prompt below)

> **EVERY individual prompt below inherits the STANDARD EXECUTION PROTOCOL above. PHASE 0 (read vault memory) and PHASE 6 (update vault) are NON-NEGOTIABLE.** If you copied a single prompt and pasted it into a fresh session WITHOUT including the protocol section, you are missing the read-vault-first + update-vault-after instructions. Either:
> 1. Open this entire file in the new session and `Read` the Standard Execution Protocol section first, OR
> 2. The first action in any prompt session should be to `Read C:\Users\User\Projects\pdf\CLAUDE.md` (auto-loads) which references the vault notes, OR
> 3. The first action in any prompt session should be to `Read C:\Users\User\Projects\pdf\docs\planning\MONTHS-2-8-PROMPTS.md` and scroll to the Standard Execution Protocol section.
>
> **Phase 0 vault reads (in order):**
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` (17 cross-project failure patterns)
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` (GlyphPDF-specific lessons)
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` (architectural constraints)
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` (commit + test status — don't assume stale)
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` (dependency-aware order — confirm prereqs)
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` (installed tools + MSYS2 packages — don't re-query)
>
> **Phase 6 vault updates (after final verification):**
> - Add a session entry to `projects/glyphpdf/07-sessions-log.md`
> - Append 1-line summary to `projects/glyphpdf/01-current-state.md` commit table
> - If new failure pattern discovered: add to `knowledge/agent-execution-anti-patterns.md` AND `projects/glyphpdf/08-lessons-learned.md`
> - If new architectural constraint: add to `projects/glyphpdf/06-non-negotiables.md`
> - If CHANGELOG Known Issue closed: remove the bullet

---

# MONTH 2 — Security tier completion

Closes the security-grade gaps identified in the audit Branch B tier. After M2, the security claims GlyphPDF intends to make publicly (PAdES B-LTA verified, Edact-Ray defended, PDF/A conformance enforced) are all backed by real implementation + tests.

---

## M2-PROMPT-1 — Edact-Ray glyph-advance normalization in redaction

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M2 security tier | Priority: HIGH (closes ROADMAP S7 D1 + audit R2 risk)
Depends on: Month 1 + MSYS2 migration complete
Agents: /security (primary), /backend (PoDoFo content stream)
Estimated context: ~30% | Effort: 1-2 person-weeks
</session_metadata>

<role>
You are a senior PDF security engineer with deep familiarity with the Bland 2023 PETS paper "Story Beyond the Eye: Glyph Positions Break PDF Text Redaction" (arXiv 2206.02285). You understand PDF content-stream text positioning operators (Tj, TJ, Tf, Tm, Td, TD), glyph advance widths from font metrics, and why simply excising text without normalizing the surrounding glyph-advance run leaks recoverable information.
</role>

<project_context>
GlyphPDF redaction already performs real content-stream surgery (`redactCanvasRecursively` at `src/engines/podofo/PoDoFoBackend.cpp:1134-1291`) — not visual-overlay. Tokenizes BT/ET text blocks, removes operators within redaction rectangles, writes the modified stream back. But: after excision, the remaining glyph-advance positions are unchanged, leaking character widths of removed text. A redacted "John Smith" leaves a gap whose pixel width is the sum of width(J) + width(o) + width(h) + width(n) + width(space) + width(S) + width(m) + width(i) + width(t) + width(h) — different from any other 10-character name in the same font.
</project_context>

<current_state>
- `redactCanvasRecursively` excises text but does NOT replace the gap with width-equivalent spacer.
- Glyph-advance normalization heuristic exists at `PoDoFoBackend.cpp:1054-1070` (audit Section 4 MEDIUM #1) using `GetSpaceCharLength` × space-glyph substitution. Limited correctness: assumes ASCII space exists in font (fails on CJK Identity-H fonts), unit mismatch on FontScale ≠ 1.0.
- No test verifies the defense works against a crafted-width attack.
</current_state>

<objective>
Replace the existing partial glyph-normalization with a robust implementation that: (a) sums original glyph advances within the redaction span, (b) emits a single TJ operator with numeric position adjustment matching the sum-of-advances, (c) handles fonts without ASCII space (CID Type0 with Identity-H), (d) honors text-matrix scale + font-scale correctly. Add a property-based test that crafted-width sequences cannot be character-recovered from the output.
</objective>

<files_to_read>
src/engines/podofo/PoDoFoBackend.cpp (lines 1000-1291 — redactCanvasRecursively; lines 1054-1070 — current normalization)
src/engines/podofo/PoDoFoBackend.cpp (font handling helpers — search "resolvedFont", "GetSpaceCharLength")
tests/TestRedaction.cpp (existing redaction tests — extend)
tests/TestSanitization.cpp (escape/injection patterns to emulate)
ROADMAP.md (Phase 3 § Session 7 D1 + Risk Register R2)
docs/PDF-spec-references.md (if exists; otherwise reference ISO 32000-2 §9.4.3 text positioning)
</files_to_read>

<deliverables>

### D1: GlyphAdvanceCalculator helper
**Files (NEW):** `src/engines/podofo/GlyphAdvanceCalculator.h`, `src/engines/podofo/GlyphAdvanceCalculator.cpp`
**Acceptance:**
- `class GlyphAdvanceCalculator` with method `double sumAdvances(const PoDoFo::PdfFont& font, const std::string& utf8Text, const TextState& state)` returning total advance in text-space units.
- `TextState` struct captures: fontSize, fontScale (Tz parameter), characterSpacing (Tc), wordSpacing (Tw), textMatrix (Tm).
- Handles three font encoding cases: Simple (1-byte), CID Type0 with Identity-H, CID Type0 with /ToUnicode CMap. Uses `PdfFont::GetStringLength` when available; falls back to per-glyph `GetGlyphWidth` for CID.
- Throws `std::runtime_error` on unsupported font (caller catches + aborts redaction safely per existing pattern at line 1184).

### D2: Replace current normalization with full implementation
**Files:** `src/engines/podofo/PoDoFoBackend.cpp:1054-1070`
**Acceptance:**
- Compute `totalAdvance = GlyphAdvanceCalculator::sumAdvances(font, removedText, textState)`.
- Emit gap as `[<totalAdvance * 1000 / fontSize / fontScale>] TJ` (numeric-only TJ, no glyph) — this is the spec-correct way to advance text cursor without rendering a glyph.
- For multi-line redactions: process per text-segment between Tj/TJ operators; preserve line breaks (Td/TD).
- Edge: if `totalAdvance` is 0 (empty span or font lookup failed), emit nothing — don't accidentally inject malformed PDF.

### D3: Edact-Ray defense regression tests
**Files:** `tests/TestRedaction.cpp`
**Acceptance:**
- New test `testGlyphAdvancesAreNormalized`: load PDF with text "John Smith Smith John" (two equally-character-counted names with different glyph widths). Redact "John Smith" (positions 0-10). Re-extract glyph positions from redacted output. Assert: gap-width is approximately equal-width irrespective of WHICH name was redacted (within ε = 0.5 text-units).
- New test `testCJKFontHandling`: PDF with CJK Identity-H font containing 4 characters; redact 2 of them; assert no exception thrown + glyph advances normalized.
- New test `testRedactionFailsAfterFontResolutionFailure`: PDF with corrupted font reference; assert redaction returns false (safe failure) rather than producing leaky output.

### D4: Documentation
**Files:** `CHANGELOG.md` (add to [1.0.0-internal] Security section), `ROADMAP.md` (mark Session 7 D1 as done)
**Acceptance:**
- CHANGELOG entry: "Edact-Ray (PETS 2023, Bland et al.) glyph-advance side-channel defense in `applyRedactions`. Sums original glyph widths of redacted span and emits numeric-only TJ position adjustment so output cannot be character-reconstructed from gap pixel widths. Test fixture confirms equal-character-count redactions of different text produce indistinguishable output."
- ROADMAP §"Session 7 — Redaction Hardening" D1 line: mark ✅ DONE.

</deliverables>

<verification>
```powershell
cd build
$env:QT_QPA_PLATFORM='offscreen'
.\TestRedaction.exe 2>&1 | Select-String "PASS|FAIL|Totals"
# Expected: all PASS including 3 new tests
ctest --output-on-failure -j4
# Expected: 14/14 pass
```
</verification>

<constraints>
- DO NOT use visual-only overlay (black rectangles) as a fallback — keep the safe-abort pattern.
- DO NOT inject the literal space character — use numeric-only TJ for the gap.
- DO NOT optimize away the GlyphAdvanceCalculator helper into the redaction function — it's reused by future M3 inline edit features.
- DO NOT claim "Edact-Ray defended" publicly until D3 tests pass AND third-party audit (M7) confirms.
</constraints>

<success_criteria>
- [ ] Build clean
- [ ] All 14 ctest targets pass
- [ ] 3 new TestRedaction cases pass
- [ ] grep for "GlyphAdvanceCalculator" in src returns the new files + the call site
- [ ] CHANGELOG + ROADMAP updated
- [ ] Atomic commit: `feat(security): Edact-Ray glyph-advance normalization in redaction (S7 D1)`
</success_criteria>

---

## M2-PROMPT-2 — OCR text-layer scrub in redaction rectangles

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M2 security tier | Priority: HIGH (ROADMAP S7 D2)
Depends on: M2-PROMPT-1
Agents: /security, /backend
Estimated context: ~25% | Effort: 1 person-week
</session_metadata>

<role>
You are a senior PDF security engineer specializing in OCR'd document workflows and invisible text-layer security. You understand that when a scanned PDF is OCR'd, the recognized text is written to an invisible text layer (using text rendering mode 3 — `3 Tr`) overlaid on the image. Redacting the visible image without also scrubbing the invisible OCR text leaves the redacted content fully recoverable via copy-paste or text extraction.
</role>

<project_context>
GlyphPDF's OCR pipeline (`OcrEngine` + `RapidOcrEngine` + `OcrPipeline`) produces text that can be embedded as an invisible layer via existing PoDoFo content-stream injection paths. Redaction (after M2-PROMPT-1) handles visible glyphs correctly with Edact-Ray defense. But: invisible text from prior OCR within the redaction rectangle is currently NOT scrubbed — it persists and is extractable via `pdftotext` or any text-extraction API.
</project_context>

<current_state>
- `redactCanvasRecursively` iterates content-stream text operators but doesn't differentiate between visible (Tr 0/2) and invisible (Tr 3) text — it treats them uniformly. This is OK for the visible case but means invisible OCR text in redaction regions might be partially scrubbed by accident OR fully preserved depending on whether the OCR layer was content-stream injected or separate XObject.
- No test verifies that OCR-injected text within a redaction box is removed from extractable text.
</current_state>

<objective>
Add an explicit pass that walks all content streams + Form XObjects, identifies text operators rendered invisibly (BT...ET sequences with `3 Tr` set), and scrubs any glyphs whose bounding box intersects a redaction rectangle. Add a regression test using an OCR'd test PDF.
</objective>

<files_to_read>
src/engines/podofo/PoDoFoBackend.cpp (redactCanvasRecursively + helper structure)
src/engines/podofo/PoDoFoBackend.cpp (search "Tr " for any existing rendering-mode handling)
src/engines/OcrEngine.cpp (how OCR text is currently produced — is it ever embedded as invisible layer?)
src/engines/ocr/OcrPipeline.cpp (the fused-word output format)
tests/TestRedaction.cpp (extend)
tests/fixtures/ — check if any OCR'd test PDFs exist; if not, plan to generate one
ROADMAP.md (S7 D2)
</files_to_read>

<deliverables>

### D1: Detect + scrub invisible text in redaction span
**Files:** `src/engines/podofo/PoDoFoBackend.cpp`
**Acceptance:**
- Extend `redactCanvasRecursively` text-operator handling to track current `Tr` state via a stack (BT initializes Tr=0; explicit `N Tr` operator updates it; ET resets).
- For ANY text operator (Tj/TJ/'/") executed while Tr==3 (invisible), apply the same redaction-rect intersection check as visible text. If intersecting: remove the operator from the output stream.
- Skip glyph-advance normalization for invisible scrubs (no visible gap to preserve).
- Form XObject content streams that contain Tr==3 text are walked recursively (existing recursive pattern).

### D2: OCR-text scrub regression test
**Files:** `tests/TestRedaction.cpp`. **New fixture:** `tests/fixtures/ocr_scanned_test.pdf` (script-generated)
**Acceptance:**
- Test `testInvisibleOcrTextIsScrubbed`:
  - Load `ocr_scanned_test.pdf` (a PDF with an image + invisible text layer containing "SECRET PASSWORD: hunter2").
  - Apply redaction over the visible image region containing "PASSWORD: hunter2".
  - Re-extract text from the redacted output via PoDoFo or PDFium text API.
  - Assert "hunter2" is NOT in the extracted text.
  - Assert "SECRET" (outside redaction) IS preserved.
- Test fixture generation script: add to `tests/fixtures/generate_ocr_fixture.cmake` (analog to the signing fixture generator from M1). Uses PoDoFo to produce a PDF with image + invisible Tj operators in known positions.

### D3: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`
**Acceptance:**
- CHANGELOG: "Invisible OCR text layer scrub in redaction (`Tr 3` text within redaction rect now removed; matches Edact-Ray defense for visible glyphs)."
- ROADMAP: Session 7 D2 ✅ DONE.

</deliverables>

<verification>
```powershell
$env:QT_QPA_PLATFORM='offscreen'
cd build
ctest --output-on-failure -j4
.\TestRedaction.exe 2>&1 | Select-String "testInvisibleOcrTextIsScrubbed"
# Expected: PASS
```
</verification>

<constraints>
- DO NOT scrub text outside redaction rects — only the spatial intersection counts.
- DO NOT remove the invisible text layer entirely — preserve OCR text outside the redaction box.
- DO NOT break visible-text redaction behavior (regression: M2-PROMPT-1 tests must still pass).
</constraints>

<success_criteria>
- [ ] Build clean, 14/14 ctest pass + new invisible-OCR test
- [ ] OCR fixture generation script committed
- [ ] CHANGELOG + ROADMAP updated
- [ ] Commit: `feat(security): scrub invisible OCR text in redaction rectangles (S7 D2)`
</success_criteria>

---

## M2-PROMPT-3 — veraPDF subprocess for PDF/A conformance validation

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M2 security tier | Priority: MEDIUM (ROADMAP S19 partial completion + R8 risk closure)
Depends on: none specific
Agents: /backend, /devops
Estimated context: ~20% | Effort: 3-5 days
</session_metadata>

<role>
You are a senior C++ engineer specializing in subprocess orchestration on Windows and PDF/A conformance validation. You understand why veraPDF (AGPL-3.0) must be invoked as an out-of-process subprocess rather than linked — to preserve our Apache-2.0 license posture.
</role>

<project_context>
GlyphPDF supports PDF/A export via `IPdfEditorEngine::exportPdfA(level)`. The current PdfAValidationPanel shows hardcoded mock results (5 fake rule violations). M2-PROMPT-3 wires real validation by invoking the veraPDF CLI as a subprocess and parsing its JSON output.
</project_context>

<current_state>
- `src/modes/PdfAValidationPanel.cpp:56-83` shows hardcoded validation results.
- `src/engines/PdfEditorEngine.cpp` has `exportPdfA` but no verify step.
- veraPDF is not installed on the build machine; user must download `veraPDF-rest-1.x.zip` or similar.
- License risk: veraPDF is AGPL-3.0 — must subprocess, never link.
</current_state>

<objective>
Add `VeraPdfValidator` class that invokes veraPDF CLI as a subprocess (`QProcess`), parses JSON output, returns structured `PdfAValidationReport`. Wire to `PdfAValidationPanel` so it displays real results. Add a CMake option `VERAPDF_CLI_PATH` for users to point to their veraPDF install.
</objective>

<files_to_read>
src/modes/PdfAValidationPanel.h + .cpp
src/engines/PdfEditorEngine.cpp (exportPdfA)
src/core/interfaces/IPdfEditorEngine.h (PdfA-related types)
ROADMAP.md (S19 + R8 risk)
LICENSE-3RD-PARTY.md (must mark veraPDF subprocess-only)
</files_to_read>

<deliverables>

### D1: VeraPdfValidator class
**Files (NEW):** `src/engines/VeraPdfValidator.h`, `src/engines/VeraPdfValidator.cpp`
**Acceptance:**
- `class VeraPdfValidator` with `PdfAValidationReport validate(QString pdfPath, PdfAConformance level)`.
- `PdfAConformance` enum: `PDF_A_1B, PDF_A_2B, PDF_A_3B, PDF_A_2U, PDF_A_3U`.
- `PdfAValidationReport` struct: bool isValid, QString conformanceLevel, QList<RuleViolation> violations.
- `RuleViolation` struct: QString ruleId, QString clause, QString description, int pageNumber, severity.
- Implementation invokes `veraPDF --format json --flavour <level> <pdfPath>` via QProcess; reads stdout; parses JSON; returns report.
- Falls back to "validator not available" status if `VERAPDF_CLI_PATH` not set or binary not found.

### D2: CMake option for veraPDF path
**Files:** `CMakeLists.txt`
**Acceptance:**
```cmake
set(VERAPDF_CLI_PATH "" CACHE FILEPATH "Path to veraPDF CLI executable (AGPL-3.0; subprocess only)")
if(VERAPDF_CLI_PATH)
    target_compile_definitions(pdfws_engines PRIVATE VERAPDF_CLI_PATH="${VERAPDF_CLI_PATH}")
    message(STATUS "veraPDF CLI: ${VERAPDF_CLI_PATH}")
else()
    message(STATUS "veraPDF CLI not configured. PdfAValidationPanel will show 'validator unavailable'.")
endif()
```

### D3: Wire PdfAValidationPanel to real validator
**Files:** `src/modes/PdfAValidationPanel.h + .cpp`
**Acceptance:**
- Remove hardcoded violations list at lines 71-75.
- On panel show: call `VeraPdfValidator::validate(currentDocPath, currentConformance)`.
- Populate violations list from real report. Show conformance status (PASS / FAIL / VALIDATOR-UNAVAILABLE).
- Wire "Fix Automatically", "Convert to PDF/A-2B", "Export Report" buttons to real handlers (or disable with tooltip if not implemented yet — track in TODO list, not silent).

### D4: Tests
**Files (NEW):** `tests/TestVeraPdf.cpp`
**Acceptance:**
- Skip if VERAPDF_CLI_PATH not configured (QSKIP with message).
- Test valid PDF/A-2B → isValid=true, 0 violations.
- Test deliberately-malformed PDF (missing /Metadata) → isValid=false, contains rule 6.7.x violation.

### D5: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`, `LICENSE-3RD-PARTY.md`, `README.md`
**Acceptance:**
- CHANGELOG: "Real PDF/A conformance validation via veraPDF CLI subprocess (AGPL-3.0; never linked in-process). PdfAValidationPanel now shows live results."
- ROADMAP S19: mark veraPDF subprocess as ✅ DONE.
- LICENSE-3RD-PARTY: confirm veraPDF row says "Subprocess only — never link in-process".
- README "Build Instructions": add optional step "Install veraPDF CLI and configure -DVERAPDF_CLI_PATH=... for PDF/A validation."

</deliverables>

<verification>
```powershell
# With VERAPDF_CLI_PATH set:
cmake -B build -DVERAPDF_CLI_PATH=C:/Tools/verapdf/verapdf.exe
cmake --build build
$env:QT_QPA_PLATFORM='offscreen'
cd build && ctest -R TestVeraPdf --output-on-failure

# Without it set (CI without veraPDF):
ctest -R TestVeraPdf --output-on-failure
# Expected: SKIP with clear message, not FAIL
```
</verification>

<constraints>
- DO NOT add veraPDF as a library dependency. Subprocess only.
- DO NOT bundle the veraPDF JAR or Java runtime — let users install it themselves.
- DO NOT make veraPDF a hard build requirement.
- DO NOT remove the existing hardcoded panel without ensuring the new real path works (or shows "validator unavailable" cleanly).
</constraints>

<success_criteria>
- [ ] Build with + without VERAPDF_CLI_PATH both clean
- [ ] PdfAValidationPanel shows real results when veraPDF available, clean message when not
- [ ] LICENSE-3RD-PARTY confirms subprocess-only stance
- [ ] Commit: `feat(pdfa): real veraPDF conformance validation via subprocess (S19, R8)`
</success_criteria>

---

## M2-PROMPT-5 — LaneScheduler infrastructure (WS1 Phase 1/2 dependency)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M2 infrastructure | Priority: HIGH (unlocks WS1 M5 + WS3 M7; consumed by future GPU workloads)
Depends on: M1 (RenderCache concurrency hardening already done)
Agents: /backend (Qt concurrency), /performance (lane sizing)
Estimated context: ~30% | Effort: 1-1.5 person-weeks
</session_metadata>

<role>
You are a senior Qt 6 + multithreaded systems engineer specializing in heterogeneous compute orchestration: GPU lanes with bounded serialized execution + warm persistent workers, CPU lanes with elastic QtConcurrent pools, and cross-stage pipelining via QFuture chaining. You understand the anti-pattern of spawn-per-page model loading (kills GPU warm-up benefits) and word-level fusion patterns that consume scheduler output.
</role>

<project_context>
Per ROADMAP "WS1 — Parallel Layout + OCR Ensemble Pipeline" and §architecture diagram, the `LaneScheduler` is a Phase 1/2 infrastructure component (not OCR-only). It is reused by: M5 OCR ensemble (primary user — layout detection + per-region OCR fanout), M7 MRC compression (per-page JBIG2/JPEG2000 + text alignment), and any v2.0 GPU workload (local-AI, future VLM-based extraction). Building it as infrastructure-first prevents per-consumer reinvention.
</project_context>

<current_state>
- `RenderCache` concurrency hardening landed in M1 (TOCTOU race fix at lines 117-130, 164-178; thread-local sentinel for `evictIfNeeded`; weak_ptr capture in prefetch lambda).
- No general-purpose `LaneScheduler` exists. Each engine (OcrPipeline, ConversionManager batch loop) spawns its own QtConcurrent::run with ad-hoc concurrency.
- ONNX Runtime sessions in `RapidOcrEngine` are already warm-persistent (compliant with anti-spawn-per-page rule), but the orchestration logic to feed them is missing.
</current_state>

<objective>
Define and implement `LaneScheduler` infrastructure: GPU lane (bounded by QSemaphore, default 2 in-flight; persistent worker thread holds warm session + env; tasks tagged `Lane::GPU`); CPU lane (`QtConcurrent::mapped` with `QThreadPool` sized to `QThread::idealThreadCount()`); cross-stage pipelining helper (`stage(P+1) ‖ stage(P) ‖ stage(P-1)` via QFuture chaining). Add bounded scheduler to prevent OOM on large documents. Result queue ordered by page index; missing pages produce sentinel error results (not gaps). Unit tests prove warm-worker reuse + ordering + cancellation.
</objective>

<files_to_read>
src/engines/RenderCache.h + .cpp (existing concurrency pattern reference)
src/engines/ocr/OcrPipeline.cpp (existing ad-hoc concurrency to be replaced in M5)
src/engines/ConversionManager.cpp (batch loop pattern reference)
src/core/AppContext.h (where LaneScheduler will be wired)
src/app/Bootstrapper.cpp (where LaneScheduler will be constructed)
ROADMAP.md (WS1 architecture diagram + R10 risk: ordered result queue + sentinel error)
</files_to_read>

<deliverables>

### D1: ILaneScheduler interface
**Files (NEW):** `src/engines/scheduling/ILaneScheduler.h`
**Acceptance:**
- `enum class Lane { GPU, CPU, Any }`
- `enum class TaskPriority { Low, Normal, High }`
- `struct SchedulerOptions { Lane lane; TaskPriority priority; int pageIndex; QString taskName; }`
- `template<typename T> using SchedulerResult = QFuture<std::expected<T, SchedulerError>>` (or QFuture-of-variant if std::expected unavailable in MinGW Qt6)
- Pure-virtual interface: `submit<T>(SchedulerOptions, std::function<T()> work) -> SchedulerResult<T>`, `cancelAll()`, `inFlightCount(Lane)`, `setLaneCapacity(Lane, int)`.

### D2: LaneScheduler concrete implementation
**Files (NEW):** `src/engines/scheduling/LaneScheduler.h + .cpp`
**Acceptance:**
- QObject with `Q_DISABLE_COPY_MOVE`.
- GPU lane: persistent `QThread` worker holding queue + `QSemaphore` bounded to `gpuCapacity` (default 2). Tasks enqueued via `submit(Lane::GPU, ...)` block on semaphore acquire; worker drains queue sequentially within the GPU thread (so ONNX Runtime sessions don't race).
- CPU lane: `QThreadPool` (own pool, not global, to isolate from QtConcurrent::run elsewhere). `cpuCapacity = QThread::idealThreadCount()` default; configurable via setter.
- `submit` returns `SchedulerResult<T>` immediately; future fires when work completes or is cancelled.
- Result-ordering helper: `OrderedResultQueue<T>` — wraps N futures, emits results in page-index order via signal. Missing pages produce sentinel `SchedulerError{Code::Timeout, pageIndex}` not gaps.
- `cancelAll()` sets cancellation token (QAtomicInt); GPU worker checks between iterations; CPU pool calls `QtConcurrent::cancel()`. In-flight tasks complete their current operation then bail.

### D3: Cross-stage pipelining helper
**Files:** `src/engines/scheduling/LaneScheduler.h` (extend) + `src/engines/scheduling/PipelineStage.h` (NEW)
**Acceptance:**
- `template<typename Stage1, typename Stage2, typename Stage3> class CrossPagePipeline` — feed page index N, produces overlapped `stage1(N+1) ‖ stage2(N) ‖ stage3(N-1)` via QFuture chaining.
- `forEachPageOrdered(int pageCount, Stage1, Stage2, Stage3, ResultHandler)` runs the pipeline; results delivered in page order.
- Backpressure: if stage 3 (output) lags, stage 1 (input) blocks via semaphore to prevent unbounded queue growth.

### D4: AppContext wiring
**Files:** `src/core/AppContext.h`, `src/app/Bootstrapper.cpp`
**Acceptance:**
- `AppContext` adds `std::shared_ptr<LaneScheduler> scheduler`.
- Bootstrapper constructs it: `std::make_shared<LaneScheduler>(gpuCap=2, cpuCap=QThread::idealThreadCount())`.
- Stop on app shutdown via Bootstrapper destructor.

### D5: Tests
**Files (NEW):** `tests/TestLaneScheduler.cpp`. **Modify:** `tests/CMakeLists.txt`
**Acceptance:**
- `testGpuLaneSerializesExecution` — 10 tasks tagged Lane::GPU; verify only `gpuCapacity` (default 2) ever in-flight concurrently.
- `testCpuLaneParallelism` — 100 tasks tagged Lane::CPU; verify peak in-flight ≥ `idealThreadCount-1`.
- `testWarmWorkerReuse` — submit 1000 GPU tasks; assert single QThread persists (use QThread::currentThreadId() in task body; expect unique count == 1).
- `testCancellationStopsInFlightWork` — submit 100 long tasks; call `cancelAll()`; verify all pending tasks return cancelled-error; in-flight tasks complete then bail.
- `testCrossPagePipelining` — 20-page pipeline with stage1 (50ms) + stage2 (100ms) + stage3 (30ms); verify total runtime < 20 * 180ms (i.e., overlap actually happens) and results delivered in page-index order.
- `testOrderedResultQueueWithMissingPages` — submit tasks for pages [0,1,2,4,5]; verify result queue emits pages [0,1,2,SENTINEL(3),4,5].
- Headless via `QT_QPA_PLATFORM=offscreen`.

### D6: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`
**Acceptance:**
- CHANGELOG (Unreleased section): "LaneScheduler infrastructure (WS1 Phase 1/2 dependency) — GPU lane warm worker + CPU lane elastic pool + cross-page pipelining. Reused by OCR ensemble (M5), MRC pipeline (M7), and future GPU workloads."
- ROADMAP "WS1 expansion" + Risk Register R10: mark LaneScheduler as infrastructure-landed.

</deliverables>

<verification>
```powershell
cd build
$env:QT_QPA_PLATFORM='offscreen'
ctest -R TestLaneScheduler --output-on-failure --repeat-until-fail 5
# Expected: 6/6 cases pass 5/5 repetitions (catches scheduler flakiness)
ctest --output-on-failure -j4
# Expected: 15/15 pass overall (14 + new TestLaneScheduler)
```
</verification>

<constraints>
- DO NOT use Qt's global QThreadPool for the CPU lane — own pool prevents interference.
- DO NOT spawn fresh worker threads for GPU lane tasks — single persistent QThread mandatory (anti-spawn-per-page rule).
- DO NOT use blocking sleeps in tests as timing oracles — use QSignalSpy + QTest::qWait with generous margins.
- DO NOT make LaneScheduler depend on OCR or any specific engine — it's pure infrastructure.
- DO NOT expose raw QFuture<T> in the public API — wrap in SchedulerResult so callers can't bypass the cancellation token.
</constraints>

<success_criteria>
- [ ] Build clean
- [ ] 15/15 ctest pass (including TestLaneScheduler 5×5)
- [ ] AppContext + Bootstrapper wired
- [ ] LaneScheduler header documents the GPU warm-worker pattern + anti-spawn-per-page rule
- [ ] Commit: `feat(scheduling): LaneScheduler infrastructure with GPU warm worker + CPU pool + cross-page pipelining (WS1 P1/2 dep)`
</success_criteria>

---

## M2-PROMPT-4 — Real-crypto E2E test coverage expansion

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M2 security tier | Priority: MEDIUM (closes audit-acknowledged "TestSignatureValidation uses Mock" gap)
Depends on: M1 work (TestSignatureRealCrypto fixtures already generated)
Agents: /security, /testing
Estimated context: ~25% | Effort: 1 person-week
</session_metadata>

<role>
You are a senior security test engineer specializing in PAdES signature validation, real-world adversarial fixtures, and Qt Test framework. You're closing the long-standing gap where `TestSignatureValidation` uses `MockSignatureManager` and never exercises the real OpenSSL/PoDoFo crypto pipeline.
</role>

<project_context>
M1 added `TestSignatureRealCrypto` with 8 cases (sign/validate at all 4 PAdES levels, untrusted chain, byte-range overlap, OCSP unverified rejection). All 8 pass with the M1 fixtures. But `TestSignatureValidation` (the original test) still uses MockSignatureManager and provides zero real-crypto coverage. Multiple edge cases remain untested: revoked cert, expired cert, malformed signature, tampered byte range, RSA-1024 (weak), missing /ByteRange, multi-signature document, certified signature with subsequent edits.
</project_context>

<current_state>
- `tests/TestSignatureValidation.cpp` links MockSignatureManager — passes trivially.
- `tests/TestSignatureRealCrypto.cpp` covers happy paths + 3 negative cases.
- Adversarial fixtures (revoked cert OCSP response, expired cert, tampered PDF, multi-sig PDF) do NOT exist.
</current_state>

<objective>
Extend the signing-fixtures generator + TestSignatureRealCrypto with adversarial cases that exercise the M1 trust-policy + OCSP verification fixes from edge angles. Migrate TestSignatureValidation from MockSignatureManager to using the real SignatureManager + fixtures (keep mock-based test as a separate target for unit-level interface contract testing).
</objective>

<files_to_read>
tests/TestSignatureValidation.cpp
tests/TestSignatureRealCrypto.cpp
tests/mocks/MockSignatureManager.h
tests/fixtures/signing/generate.bat
src/engines/SignatureManager.cpp (M1 work — verify the trust + OCSP code paths)
</files_to_read>

<deliverables>

### D1: Adversarial fixture generation
**Files:** `tests/fixtures/signing/generate.bat` (extend), new fixtures committed
**Acceptance:**
- Generate `expired_cert.p12` (NotAfter in past).
- Generate `revoked_cert.p12` + a "revoked" OCSP response.
- Generate `weak_cert_rsa1024.p12` (deliberately weak — for rejection test).
- Generate `tampered.pdf` (a signed PDF with one byte changed after signing).
- Generate `multi_sig.pdf` (two signatures, second over first).
- Generate `cert_with_revoked_ocsp_signer.pdf` (signed where OCSP responder's own cert is revoked).
- All deterministic from `generate.bat`; outputs in `tests/fixtures/signing/`; `.gitignore` keeps the binaries out of the repo (script regenerates on demand).

### D2: New TestSignatureRealCrypto cases
**Files:** `tests/TestSignatureRealCrypto.cpp`
**Acceptance:** Add tests:
- `testExpiredCertRejected` — signing with expired cert returns false OR validation reports `CertExpired`.
- `testRevokedCertReportsRevoked` — validation against OCSP response showing "revoked" returns trustStatus="Revoked".
- `testRSA1024Rejected` — signing with RSA-1024 key fails (forced via cert policy).
- `testTamperedPdfInvalid` — validation of tampered PDF returns `integrityIntact=false`.
- `testMultipleSignatures` — both signatures validated independently; second signature includes first in its byte range.
- `testRevokedOcspSigner` — OCSP response signer cert revoked → OCSP rejected → signature degrades to B-T.

### D3: Migrate TestSignatureValidation away from MockSignatureManager
**Files:** `tests/TestSignatureValidation.cpp`, `tests/mocks/MockSignatureManager.h`
**Acceptance:**
- Rename current `TestSignatureValidation` → `TestSignatureValidationMock` (keep, retitled as unit test for the interface contract).
- New `TestSignatureValidation` uses the real `SignatureManager` + fixtures. Covers the validation API surface end-to-end including `setTsaUrl`, `setSignatureLevel(PAdESLevel)`, `validateSignatures` with all SignatureInfo fields populated correctly.
- Update `CMakeLists.txt` to add the new target.

### D4: Documentation
**Files:** `CHANGELOG.md`
**Acceptance:**
- CHANGELOG: "Expanded real-crypto signature test coverage: expired/revoked certs, tampered PDFs, multi-signature docs, weak-key rejection. `TestSignatureValidation` no longer uses MockSignatureManager — exercises real OpenSSL/PoDoFo pipeline."

</deliverables>

<verification>
```powershell
$env:QT_QPA_PLATFORM='offscreen'
cd build
ctest -R "TestSignature" --output-on-failure
# Expected: 2 test targets pass — TestSignatureValidation (real) + TestSignatureValidationMock (kept)
.\TestSignatureRealCrypto.exe 2>&1 | Select-String "Totals"
# Expected: at least 14 passed (8 original + 6 new)
```
</verification>

<constraints>
- DO NOT commit the generated fixture binaries (.p12, .pem etc.) — keep .gitignored; regenerate via script.
- DO NOT delete `MockSignatureManager` — it's still useful for unit tests that don't need real crypto.
- DO NOT introduce new fixtures that depend on a real CA or external trust anchor.
</constraints>

<success_criteria>
- [ ] 6 new adversarial test cases pass
- [ ] TestSignatureValidation runs against real crypto (no mock)
- [ ] CHANGELOG updated; "uses Mock" admission removed
- [ ] Commit: `test(signature): adversarial real-crypto coverage; migrate TestSignatureValidation off mock`
</success_criteria>

---

# MONTH 3 — Mode-page completions

Closes the 4 "preview banner" mode pages + the decorative InspectorWidget Properties tab. Each prompt is independent and can run in parallel.

---

## M3-PROMPT-1 — FormBuilderMode wired drag-and-drop field placement

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M3 | Priority: HIGH (closes major v1.0 UI gap)
Depends on: M1 done; FormManager already real
Agents: /frontend, /backend
Estimated context: ~30% | Effort: 1.5 person-weeks
</session_metadata>

<role>
You are a senior Qt UI engineer specializing in interactive canvas + form-field drag-and-drop, with deep familiarity of the PoDoFo AcroForm API and QGraphicsScene-style hit testing.
</role>

<project_context>
FormBuilderMode currently ships as a preview banner (`src/modes/FormBuilderMode.cpp:23-62`) with 9 disabled tool toggles and a blank PdfViewerWidget not connected to the user's document. FormManager (`src/engines/FormManager.cpp`) already implements real CRUD for all field types (text, checkbox, radio, combo, list, signature, date, numeric, calculated, button). M3-PROMPT-1 wires the UI to FormManager.
</project_context>

<current_state>
- FormBuilderMode renders a preview banner + 9 unconnected toggle buttons (TEXT FIELD, CHECKBOX, etc.) + a new blank PdfViewerWidget.
- All children `setEnabled(false)` in a loop after construction.
- FormManager + AddFormFieldCommand work end-to-end via the ribbon's `ToolId::TextField` path (placement hardcoded at `QRectF(100, 100, ...)`).
</current_state>

<objective>
Replace the preview banner with a functional mode: connect the canvas to the user's open document, enable the 9 field-type toggles, implement click-to-place workflow on the canvas, route placement to AddFormFieldCommand. Add field-properties side panel for renaming + setting required/placeholder/validation.
</objective>

<files_to_read>
src/modes/FormBuilderMode.h + .cpp
src/modes/ModeController.cpp (mode switching)
src/engines/FormManager.h + .cpp
src/commands/AddFormFieldCommand.h + .cpp
src/ui/PdfViewerWidget.h + .cpp (canvas; learn how it handles mouse events for existing tools)
src/core/PdfEnums.h (ToolMode enum)
</files_to_read>

<deliverables>

### D1: Remove preview banner; connect to active document
**Files:** `src/modes/FormBuilderMode.cpp`
**Acceptance:**
- Delete preview banner widget creation.
- Remove the disable-all-children loop.
- Replace the new blank PdfViewerWidget with reference to the existing main canvas via ModeController context.
- Mode-enter: switch active document into "form builder" overlay mode.

### D2: Click-to-place + drag-to-size workflow
**Files:** `src/modes/FormBuilderMode.cpp`, `src/ui/PdfViewerWidget.cpp`
**Acceptance:**
- Active toggle (e.g., TextField selected) → cursor changes to crosshair on canvas.
- Click-drag on canvas: rubber-band rectangle for field bounds.
- Release: emit `fieldPlaced(QRectF bounds, FieldType type)` signal.
- FormBuilderMode catches signal → pushes AddFormFieldCommand with real rect (no more hardcoded 100,100).
- ESC key cancels active placement.

### D3: Field-properties side panel
**Files (NEW):** `src/modes/FormFieldPropertiesPanel.h + .cpp`. **Modify:** FormBuilderMode.cpp
**Acceptance:**
- Right-sidebar panel shown when a field is selected.
- Editable: field name (must be unique per page), tooltip, required flag, default value, placeholder, validation regex (optional).
- "Apply" button pushes `EditFormFieldCommand` (new — add to commands/).
- Live preview of validation regex (red border if regex invalid).

### D4: Field manipulation (move, resize, delete)
**Files:** `src/modes/FormBuilderMode.cpp`, `src/commands/` (new `MoveFormFieldCommand`, `ResizeFormFieldCommand`, `DeleteFormFieldCommand`)
**Acceptance:**
- Existing fields shown with selection handles when in FormBuilder mode.
- Drag = move (snap to 4pt grid by default; Shift to disable snap).
- Resize handles on corners + edges.
- Delete key removes selected field via DeleteFormFieldCommand (undoable).

### D5: Tab order editor
**Files:** `src/modes/FormBuilderMode.cpp`
**Acceptance:**
- Toolbar button "Tab Order" toggles overlay showing field numbers (1, 2, 3, ...).
- Click-sequence on fields reorders them.
- Saves to PDF `/Tabs` entry via FormManager.

### D6: Tests
**Files:** `tests/TestFormBuilder.cpp` (new)
**Acceptance:**
- Place each of 9 field types via simulated drag → verify field appears in document.
- Move + resize + delete operations work + are undoable.
- Tab order persists across save/reload.

### D7: Documentation
**Files:** `CHANGELOG.md`
**Acceptance:**
- CHANGELOG: "FormBuilderMode wired end-to-end: drag-place + move/resize/delete + tab-order editor + field properties panel. No more preview banner."

</deliverables>

<verification>
```powershell
$env:QT_QPA_PLATFORM='offscreen'
cd build && ctest -R TestFormBuilder --output-on-failure
# Manual: open a PDF, switch to FormBuilder mode, place 3 fields, save, reopen, verify fields persist
```
</verification>

<constraints>
- DO NOT break the existing ribbon-path field placement (it still uses hardcoded coords; that's the quick-place path).
- DO NOT use absolute pixel coordinates — all geometry in PDF user-space.
- DO NOT make field names auto-generated — user-provided or auto with rename prompt.
</constraints>

<success_criteria>
- [ ] Build clean, 15/15 ctest pass
- [ ] Manual: 9 field types placeable + manipulable
- [ ] No "Preview — not wired in v1.0.0" banner remains
- [ ] Commit: `feat(forms): FormBuilderMode wired with drag-place + properties panel + tab order`
</success_criteria>

---

## M3-PROMPT-2 — BatchMode real execution loop

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M3 | Priority: HIGH
Depends on: M1
Agents: /backend, /frontend
Estimated context: ~25% | Effort: 1 person-week
</session_metadata>

<role>
You are a senior C++ engineer building batch processing UIs. You understand QtConcurrent + QFutureWatcher patterns for non-blocking progress reporting in Qt.
</role>

<project_context>
BatchMode ships as preview banner with hardcoded pipeline cards + a fake `QThread::msleep(200)` loop that does no real work. ConversionManager + PdfEditorEngine + SignatureManager all provide batch-friendly APIs.
</project_context>

<current_state>
- `src/modes/BatchMode.cpp:28-247` is the preview banner shell.
- `onRunClicked` exists at line 206-247 but loops over an always-empty `m_filesToProcess` and sleeps without doing work.
- ConversionManager has `convertTo(input, output, format)` ready.
</current_state>

<objective>
Replace preview banner with: file list (drag-drop + add/remove), operation selector (Convert / OCR / Compress / Watermark / Redact / Merge / Export PDF/A), per-operation config panel, sequential or parallel execution via QtConcurrent, per-file + overall progress bars, continue-on-failure with error log export (CSV/JSON via existing ErrorLog).
</objective>

<files_to_read>
src/modes/BatchMode.h + .cpp
src/engines/ConversionManager.h + .cpp
src/engines/PdfEditorEngine.h (batch-relevant methods)
src/core/ErrorInfo.h + .cpp (ErrorLog already supports export)
</files_to_read>

<deliverables>

### D1: File list widget with drag-drop
**Files:** `src/modes/BatchMode.cpp`
**Acceptance:**
- QListView with QStringListModel of file paths.
- Drag-drop from Windows Explorer adds files.
- "Add Files" + "Add Folder" + "Clear" + "Remove Selected" buttons.

### D2: Operation selector + config panel
**Files:** `src/modes/BatchMode.cpp`
**Acceptance:**
- Operation combo with 7 options. Each operation's config panel swaps in below: Convert (target format + quality), OCR (language + engine), Compress (target size), Watermark (text + opacity), Redact (search pattern), Merge (output name), Export PDF/A (conformance level).

### D3: Execution engine
**Files:** `src/modes/BatchMode.cpp`
**Acceptance:**
- Run button: spawns `QtConcurrent::mapped` over file list with chosen operation.
- Per-file progress (QListView delegate with progress bar in row).
- Overall progress bar + ETA.
- Continue-on-failure: failure populates ErrorLog; doesn't abort batch.
- Cancel button: requests cancellation via QFutureWatcher.

### D4: Error log + result export
**Files:** `src/modes/BatchMode.cpp`
**Acceptance:**
- Post-run: results panel shows N succeeded / M failed.
- "Export Log" button uses existing `ErrorLog::exportCsv()` / `exportJson()`.

### D5: Tests
**Files:** `tests/TestBatchMode.cpp` (new)
**Acceptance:**
- 3-file batch convert test → all succeed → log shows 3 success.
- 1 corrupt file in batch → 2 succeed, 1 fails, log captures.

### D6: Documentation
**Files:** `CHANGELOG.md`
**Acceptance:**
- CHANGELOG: "BatchMode wired with real execution loop — drag-drop file list, 7 operation types, per-file + overall progress, continue-on-failure with CSV/JSON error log export."

</deliverables>

<verification>
```powershell
$env:QT_QPA_PLATFORM='offscreen'
cd build && ctest -R TestBatchMode --output-on-failure
```
</verification>

<constraints>
- DO NOT block the GUI thread — all operations via QtConcurrent.
- DO NOT crash on file removal during running batch — handle the index invalidation.
- DO NOT allow output overwrites without confirmation.
</constraints>

<success_criteria>
- [ ] No preview banner; real execution loop runs
- [ ] 16/16 ctest pass (including new TestBatchMode)
- [ ] Commit: `feat(batch): BatchMode wired with real ConversionManager + progress + error log`
</success_criteria>

---

## M3-PROMPT-3 — PagesMode real split-form UI

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M3 | Priority: MEDIUM
Depends on: M1
Agents: /frontend
Estimated context: ~20% | Effort: 3-5 days
</session_metadata>

<role>
You are a Qt UI engineer building document-management dialogs. The page operations already work via the ribbon (`PagesController`); this prompt focuses on PagesMode's specialty: split + reorder UI.
</role>

<current_state>
PagesMode (`src/modes/PagesMode.cpp:25-84`) is a preview banner with 12 hardcoded fake page names ("Cover", "Contents", "Exec", etc.) and a QLabel placeholder ("Form layout: SPLIT AT radio, N PAGES input, ...") instead of a real form.
</current_state>

<deliverables>

### D1: Remove preview banner; show real page list
**Files:** `src/modes/PagesMode.cpp`
**Acceptance:**
- Real page-count from active document. Page list shows actual thumbnails (lazy load via PdfiumBackend renderPage at low DPI). Page numbers + size + has-text indicators.

### D2: Split form
**Files:** `src/modes/PagesMode.cpp`
**Acceptance:**
- "Split at page": numeric spinbox or page-range expression ("1-5,7,9-12").
- "Split every N pages": numeric spinbox.
- "Split at bookmark": dropdown of /Outlines top-level bookmarks.
- Output naming pattern with placeholders: `{stem}_{start}-{end}.pdf`, `{stem}_part{n}.pdf`.
- Output folder picker.
- "Preview" shows resulting filenames in a list.
- "Split" executes via `IPdfEditorEngine::extractPageAsBytes` loop.

### D3: Reorder UI (already partially works via thumbnail drag-drop in ThumbnailSidebar)
**Files:** `src/modes/PagesMode.cpp`
**Acceptance:**
- Two-column view: left = current order, right = target order (user drags between).
- "Apply" pushes ReorderPageCommand.

### D4: Tests + Documentation
**Files:** `tests/TestPagesMode.cpp`, `CHANGELOG.md`
**Acceptance:** Split + reorder regression tests; CHANGELOG entry.

</deliverables>

<success_criteria>
- [ ] No preview banner; real page list shown
- [ ] Split + reorder both work end-to-end
- [ ] Commit: `feat(pages): PagesMode wired with split form + reorder UI`
</success_criteria>

---

## M3-PROMPT-4 — RedactMode + pattern redaction backend

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M3 | Priority: HIGH (closes "Pattern redaction not implemented" CHANGELOG admission + completes RedactMode)
Depends on: M2-PROMPT-1 (Edact-Ray defense applies to pattern matches too)
Agents: /security, /backend, /frontend
Estimated context: ~30% | Effort: 1-1.5 person-weeks
</session_metadata>

<role>
You are a senior security engineer building pattern-based content redaction. You understand PDF text extraction with positional info (PDFium FPDFText APIs) and regex matching with proper Unicode handling.
</role>

<current_state>
- RedactMode (`src/modes/RedactMode.cpp:23-81`) is a preview banner.
- "Mark by Pattern ▾" pill exists in UI but has no backend.
- ToolId::PatternRedact + RegexRedact in SecurityController stub with "scheduled for future engine update" QMessageBox.
- The ribbon's Find & Replace + Redact-All path supports only literal strings.
</current_state>

<deliverables>

### D1: Remove preview banner; connect to live document
**Files:** `src/modes/RedactMode.cpp`
**Acceptance:** Like other mode-page closures.

### D2: Pattern redaction backend
**Files (NEW):** `src/engines/PatternRedactor.h + .cpp`. **Modify:** `src/engines/PdfEditorEngine.cpp`
**Acceptance:**
- `class PatternRedactor` with `QList<QRectF> findMatches(QString pdfPath, int pageIndex, QRegularExpression pattern)`.
- Built-in named patterns: `email`, `phone-us`, `phone-intl`, `ssn`, `credit-card`, `ipv4`, `ipv6`, `iban`, `uk-postcode`, `us-zip`, `date-iso`, `date-us`.
- Uses PDFium text extraction with per-character bounding boxes; matches regex against extracted text; returns aggregated bounding boxes per match.
- `IPdfEditorEngine::applyPatternRedactions(pattern, pageRange)` method that calls PatternRedactor then `applyRedactions` (which has Edact-Ray defense from M2-PROMPT-1).

### D3: RedactMode UI for pattern config
**Files:** `src/modes/RedactMode.cpp`
**Acceptance:**
- "Mark by Pattern" pill expanded into combo of named patterns + "Custom regex" option.
- Preview pane: shows matches highlighted on current page before applying.
- "Apply to all pages" / "Apply to current page" / "Apply to page range" options.

### D4: SecurityController wire-up
**Files:** `src/shell/controllers/SecurityController.cpp:80-87`
**Acceptance:**
- Remove the stub "scheduled for future engine update" QMessageBox for PatternRedact + RegexRedact.
- Wire to RedactMode pattern flow.

### D5: Tests
**Files:** `tests/TestPatternRedact.cpp` (new), extend TestRedaction
**Acceptance:**
- Test each built-in pattern against a synthetic PDF.
- Custom regex test.
- Edact-Ray defense still works on pattern-redacted output.

### D6: Documentation
**Files:** `CHANGELOG.md` (REMOVE the "Pattern redaction not implemented" admission)

</deliverables>

<success_criteria>
- [ ] No preview banner
- [ ] 12 built-in patterns + custom regex work
- [ ] Edact-Ray defense applies to pattern matches
- [ ] CHANGELOG admission removed
- [ ] Commit: `feat(redaction): RedactMode wired + pattern redaction backend (12 named patterns + custom regex)`
</success_criteria>

---

## M3-PROMPT-5 — InspectorWidget Properties tab fully bound

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M3 | Priority: MEDIUM
Depends on: none
Agents: /frontend
Estimated context: ~25% | Effort: 1 person-week
</session_metadata>

<role>
You are a Qt UI engineer building properties-panel binding patterns for inspector-style UIs.
</role>

<current_state>
InspectorWidget Properties tab (`src/ui/InspectorWidget.cpp:331-553`) is heavily decorative: 6 color swatches with no `connect()`, 6 align buttons no-op, opacity/blend/border rows hardcoded, X/Y/W/H grid not bound, Contents `QTextEdit` not bound to annotation.text, Reply Thread shows permanent "No replies yet".
</current_state>

<deliverables>

### D1: Identity fields bound to annotation
**Files:** `src/ui/InspectorWidget.cpp:331-368`
**Acceptance:**
- Type name, annotation ID, page number, author, creation/modification dates — all read from `AnnotationItem` (already populated by AnnotationLayer).
- Layer field: read from PDF /OCG association.
- Lock/unlock toggle: bound to annotation `Locked` flag.

### D2: Geometry fields editable
**Files:** `src/ui/InspectorWidget.cpp:432-438`
**Acceptance:**
- X/Y/W/H as QDoubleSpinBox (PDF units) bound bidirectionally to `annotation.rect`.
- Edit triggers `EditAnnotationCommand` (undoable).
- "Align" + "Distribute" buttons fire AnnotationLayer alignment helpers (left/center/right/top/middle/bottom; distribute horizontal/vertical for ≥3 selections).

### D3: Appearance fields editable
**Files:** `src/ui/InspectorWidget.cpp:388-410`
**Acceptance:**
- 6 color swatches: each click sets annotation.color + pushes EditAnnotationCommand. Custom color via "more..." opens QColorDialog.
- Opacity slider 0-100%.
- Blend mode combo (Normal/Multiply/Screen/Overlay/Darken/Lighten — map to PDF /BlendMode entries).
- Border width spinbox.

### D4: Contents editor bound
**Files:** `src/ui/InspectorWidget.cpp:483-500`
**Acceptance:**
- Bidirectional bind QTextEdit ↔ annotation.text.
- Char-count label updates live.
- Save on focus loss or Ctrl+Enter via EditAnnotationCommand.

### D5: Reply thread
**Files:** `src/ui/InspectorWidget.cpp:519-545`
**Acceptance:**
- Read replies from annotation.replies list (already exists in AnnotationItem).
- Display in scrollable list with author + timestamp + indent for nested replies.
- "+ Add Reply" opens inline editor; submit creates child AnnotationItem with `parentId` set, /IRT linking on save.

### D6: Tests + Documentation
**Files:** `tests/TestInspector.cpp` (new), `CHANGELOG.md`

</deliverables>

<success_criteria>
- [ ] All Properties tab fields read/write from real annotation
- [ ] EditAnnotationCommand fires on every change (undoable)
- [ ] Replies persist across save/load
- [ ] Commit: `feat(inspector): Properties tab fully bound (geometry, appearance, contents, replies)`
</success_criteria>

---

# MONTH 4 — 23 ribbon tools → real engine wiring

Eliminates every "scheduled for a future engine update" message box. Grouped by controller for parallel execution.

---

## M4-PROMPT-1 — View tools (TwoPage, EyeCare, Presentation)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M4 | Priority: MEDIUM | Effort: 4-6 days
</session_metadata>

<role>Qt UI engineer for viewer rendering modes.</role>

<deliverables>

### D1: TwoPage view via QGraphicsView renderer
**Files:** `src/ui/PdfViewerWidget.cpp`, `src/shell/controllers/ViewController.cpp:58-61`
**Acceptance:** Two-page side-by-side layout (odd left, even right, or facing-pages with first-page-cover option). Removes the "Two-Page view mode requires a custom QGraphicsView renderer and is scheduled..." dialog.

### D2: EyeCare (sepia/warm) filter
**Files:** `src/ui/PdfViewerWidget.cpp`, `src/shell/controllers/ViewController.cpp:69-71`
**Acceptance:** Sepia-tone shader applied to rendered page images (QImage color transform or QGraphicsEffect). Toggle persists in QSettings. Removes the stub status message.

### D3: Presentation mode (real slideshow)
**Files:** `src/shell/controllers/ViewController.cpp:62-65`
**Acceptance:** Full-screen + auto-advance timer (configurable interval) + click/space/arrow keys to advance + ESC to exit. Currently aliased to Fullscreen; replace with real slideshow.

### D4: Tests + CHANGELOG
**Acceptance:** Manual smoke per mode. CHANGELOG: "View modes: TwoPage, EyeCare, Presentation — all wired."

</deliverables>

<success_criteria>
- [ ] 3 View ribbon tools work
- [ ] No "scheduled for future" dialogs from View tab
- [ ] Commit: `feat(view): TwoPage, EyeCare, Presentation modes wired`
</success_criteria>

---

## M4-PROMPT-2 — Pages tools (Split, Reorder, Resize, Headers, Footers, Page Numbers, Bates)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M4 | Priority: HIGH | Effort: 1 person-week
</session_metadata>

<role>PDF page-operations engineer (PoDoFo experience).</role>

<context>
PagesController:65-81 has 7 stubbed ToolIds. Engine methods (cropPage/resizePage/addHeaderFooter/applyBatesNumbering) already exist on IPdfEditorEngine. Just wire them.
</context>

<deliverables>

### D1: Split (ribbon path)
Route to PagesMode (already real after M3-PROMPT-3) OR open dedicated SplitDialog.

### D2: Reorder (ribbon path)
Route to focus the ThumbnailSidebar (drag-drop already works) OR open ReorderDialog with explicit page-list editing.

### D3: Resize
Opens ResizeDialog: target paper size combo (A4/Letter/Legal/Custom) + scale mode (fit/fill/center) + apply-to-page-range. Calls `IPdfEditorEngine::resizePage`.

### D4: AddHeader / AddFooter / AddPageNumbers
Single HeaderFooterDialog: position (6 anchors), text template with `{page}`, `{total}`, `{date}`, `{filename}`, `{title}` placeholders, font + size + color, page range. Calls `addHeaderFooter` (M2-PROMPT-1's escaping prevents content-stream injection).

### D5: BatesNumber
BatesNumberingDialog: prefix, suffix, start number, digit count, position, font/size, page range. Calls `applyBatesNumbering`.

### D6: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] 7 Pages ribbon tools work via real engine methods
- [ ] Header/footer + Bates use M2-PROMPT-1 escaping (no injection)
- [ ] Commit: `feat(pages): wire Split/Reorder/Resize/Header/Footer/PageNumbers/Bates ribbon tools`
</success_criteria>

---

## M4-PROMPT-3 — Convert tools (ToHtml, ToText, Compress ribbon path, PDF→PPT)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M4 | Priority: MEDIUM | Effort: 1-1.5 person-weeks
</session_metadata>

<deliverables>

### D1: ToHtml + ToText
ConvertController routes to existing `IConversionEngine::convertTo(format=Html|Text)` — these targets exist in ConversionManager but ribbon path was stubbed. Just connect.

### D2: Compress (ribbon path)
Reroute to open CompressDialog (the screen-nav path already works). Eliminates the ribbon/screen-nav UX conflict.

### D3: PDF→PowerPoint export (NEW)
Add `ConversionManager::exportToPowerPoint(input, output, options)`. Use libppttx or write a minimal PPTX writer (each PDF page → 1 slide with embedded image at 150 DPI). Options: image DPI, slide size match-PDF-page-size or fixed 16:9.

### D4: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] 4 Convert ribbon tools work
- [ ] PPT export produces .pptx openable in PowerPoint
- [ ] Commit: `feat(convert): ToHtml/ToText/Compress ribbon wired + new PDF→PPT export`
</success_criteria>

---

## M4-PROMPT-4 — Forms tools (Button, SigField, AutoDetect, Tabs)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Button field
Add `ButtonField` type to FormManager + UI handler in FormBuilderMode (push-button with caption + action JavaScript-or-action-link).

### D2: SigField
Signature field placement that integrates with existing SignatureDialog/SignatureManager.

### D3: AutoDetect
Heuristic that scans page for form-like layouts (boxes, underlines, "Name: ___" patterns) and proposes field placements. User accepts/rejects per field.

### D4: Tabs (tab order editor)
Already covered in M3-PROMPT-1 D5. Wire the ribbon entry to that flow.

### D5: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] 4 Forms ribbon tools work
- [ ] Commit: `feat(forms): wire Button/SigField/AutoDetect/Tabs ribbon tools`
</success_criteria>

---

## M4-PROMPT-5 — Security tools (Permissions, RemoveSecurity, Certify, Timestamp, Pattern+Regex Redact already in M3)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Permissions
PermissionsDialog: print/copy/modify/annotate/fillforms/accessibility/assemble/printhigh checkboxes. Calls existing `encryptDocument` with the permission flags.

### D2: RemoveSecurity
Prompts for owner password; on success calls `IPdfEditorEngine::removeEncryption(ownerPassword)` — new method to add (PoDoFo supports decryption).

### D3: Certify (cert-level signing)
Like Sign but with cert-level (1/2/3) selection. Wires to `SignatureManager::certifyDocument` (new method extending sign with /DocMDP entry).

### D4: Timestamp (document-level timestamp without sign)
Adds `/DocTimeStamp` signature without a user-signed signature. Uses existing TSA URL setting.

### D5: PatternRedact + RegexRedact
Already wired in M3-PROMPT-4. Verify the ribbon path opens RedactMode in pattern mode.

### D6: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] 6 Security ribbon tools work
- [ ] Commit: `feat(security): wire Permissions/RemoveSecurity/Certify/Timestamp ribbon tools`
</success_criteria>

---

## M4-PROMPT-7 — WS2 Djot interchange foundation (Phase 1.5)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M4 / ROADMAP Phase 1.5 | Priority: HIGH (prerequisite for M5 OCR→Djot mapping, M6 annotation rich-text, M7 MRC sandwich text)
Depends on: M1 + M2-PROMPT-5 (LaneScheduler) + MSYS2 migration
Agents: /backend (C++ + Lua FFI), /security (provenance guard), /testing (round-trip property test)
Estimated context: ~45% | Effort: 2-3 person-weeks (largest single prompt)
</session_metadata>

<role>
You are a senior C++ + Lua FFI engineer building cross-format document interchange layers. You understand: (1) why Djot (designed by the CommonMark/Pandoc author John MacFarlane) was chosen over Markdown for unambiguous backtrack-free parsing in a security-critical document tool, (2) the dual-model architecture pattern (Structural = source of truth for layout/sign/redact; Semantic = editing/interchange model), (3) the explicitly-lossy boundary between Semantic and PDF (versus lossless Djot↔Semantic), and (4) the provenance guard that refuses edit-via-Djot-save-back for signed/born-PDF documents.
</role>

<project_context>
Per ROADMAP Phase 1.5 (Djot Interchange Foundation, WS2 Phase 1), this prompt stands up the two new libraries that anchor the dual-model architecture: `docmodel` (the Semantic model) and `pdfws_djot` (the Djot codec vendoring the Lua 5.4 reference parser). These libraries are prerequisites for: M5-PROMPT-4 OCR→Djot mapping (WS2 role 1), M6-PROMPT-4 annotation rich text (WS2 role 3), and M7-PROMPT-3 MRC pipeline (sandwich text alignment).
</project_context>

<current_state>
- Zero Djot / docmodel symbols exist in `src/` (verified by grep for `Djot|docmodel|SemanticDocument`).
- PoDoFo + PDFium + qpdf own the Structural model already.
- No provenance tracking exists; all edits go directly to PDF object graph regardless of origin.
- Lua interpreter not yet vendored.
</current_state>

<objective>
Build `docmodel` and `pdfws_djot` static libraries per ROADMAP Phase 1.5 spec. Vendor Lua 5.4 reference Djot parser (MIT). Implement ProvenanceGuard that refuses Djot→PDF save-back for born-PDF signed documents. Pin Djot spec version. Add round-trip property test (1000 random SemanticDocuments).
</objective>

<files_to_read>
ROADMAP.md §"Phase 1.5: Djot Interchange Foundation" (full spec)
ROADMAP.md §"Risk + Anti-Pattern Register" R7 (provenance guard) + R11 (Lua sandbox)
ROADMAP.md §"Third-Party License Matrix" (liblua MIT row)
ROADMAP.md §architecture diagram (Dual-Model Core section)
CMakeLists.txt (existing pdfws_core / pdfws_engines library structure)
src/core/AppContext.h (where new libraries get wired)
docs/djot-spec/ (if exists; otherwise download spec from djot.net)
</files_to_read>

<deliverables>

### D1: docmodel library (SemanticDocument tree)
**Files (NEW):** `src/docmodel/SemanticDocument.h`, `src/docmodel/Block.h`, `src/docmodel/Inline.h`, `src/docmodel/ProvenanceTag.h`, plus `.cpp` files. **CMakeLists.txt:** new target `docmodel` (STATIC, pure C++20, no Qt dep).
**Acceptance:**
- `SemanticDocument` tree: `Document → Section → Block (Paragraph / Table / Figure / List / Header / Footer / CodeBlock / RawBlock) → Inline (Span / Image / Link / Code / Math)`.
- No cycles in tree; ownership via `std::unique_ptr<Block>` for children, `std::weak_ptr` for cross-references.
- `ProvenanceTag` enum: `BornPDF`, `BornDjot`, `BornOCR`. Every node carries one (+ source-file path + page index + bbox).
- Pure C++20; no Qt types in `docmodel` interface (Qt types appear only in conversion utilities at the boundary).
- `DocumentFuzzer` class for property tests: generates random SemanticDocument with configurable depth/breadth/leaf types.

### D2: pdfws_djot library (Djot codec + mapper)
**Files (NEW):** `src/pdfws_djot/IDjotCodec.h`, `src/pdfws_djot/IDjotMapper.h`, `src/pdfws_djot/LuaDjotCodec.cpp`, `src/pdfws_djot/PdfStructureMapper.cpp`. **CMakeLists.txt:** new target `pdfws_djot` (STATIC, depends on `docmodel` + `liblua`).
**Acceptance:**
- Vendor Lua 5.4 reference Djot implementation: clone https://github.com/jgm/djot into `third_party/djot/` at pinned commit (record commit SHA in `pdfws_djot/DJOT_SPEC_VERSION` file).
- Vendor liblua 5.4.x as static library in `third_party/lua-5.4/` (MIT — no CLA / no dynamic dep). Build via CMake subdirectory.
- `IDjotCodec` interface: `QString encode(const SemanticDocument&)` / `SemanticDocument decode(const QString&)`. Implementation `LuaDjotCodec` invokes vendored Djot parser via embedded Lua VM.
- `IDjotMapper` interface: `SemanticDocument fromPdf(const PdfStructure&)` / `PdfStructure toPdf(const SemanticDocument&)`. Conversion utilities wrap Qt types here.
- PDF fidelity attributes on Djot nodes: `{pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}` per ROADMAP spec.
- Raw escape: `{=pdf}` opaque-blob blocks stored in sidecar `QHash<QString, QByteArray>` keyed by stable IDs.
- Lua sandbox per R11: disable `io`, `os`, `loadfile`, `require`, `dofile` before running user-supplied Djot; no network access.

### D3: ProvenanceGuard
**Files (NEW):** `src/pdfws_djot/ProvenanceGuard.h + .cpp`
**Acceptance:**
- `class ProvenanceGuard { ProvenanceViolation checkEditVia(ProvenanceTag origin, EditPath path); }`.
- `enum class EditPath { DirectStructural, DjotThenSave, DjotThenSaveAsCopy }`.
- For `BornPDF + signed` documents on `DjotThenSave` path: throws `ProvenanceViolation` (NOT a debug-only assertion — hard constraint).
- For `BornPDF + unsigned` documents on `DjotThenSave` path: warn dialog, offer "Save as copy" branch (route to `DjotThenSaveAsCopy`).
- For `BornDjot` / `BornOCR` documents: any edit path permitted.

### D4: Djot spec version pinning + Lua vendoring
**Files (NEW):** `pdfws_djot/DJOT_SPEC_VERSION`, `third_party/lua-5.4/` (vendored), `third_party/djot/` (vendored)
**Acceptance:**
- `DJOT_SPEC_VERSION` file records: Djot grammar commit SHA + Lua reference impl commit SHA + date pinned.
- README contributing guide notes the pinning policy: don't update Djot/Lua versions without round-trip test re-run + ADR.
- Build adds `third_party/lua-5.4/` as CMake subdirectory; produces static `liblua` target with sandbox-enforcing wrapper.

### D5: Round-trip property test
**Files (NEW):** `tests/TestDjotRoundtrip.cpp`. **Modify:** `tests/CMakeLists.txt`
**Acceptance:**
- 1000 random `SemanticDocument` instances generated via `docmodel::DocumentFuzzer`.
- For each: `original → encode → decode → reconstructed`; assert structural equality (AST node types + attribute maps; NOT naive string equality).
- Test categorically rejects naive `original_djot == roundtrip_djot` comparison (whitespace/ordering variation acceptable; semantic equivalence required).
- Test: born-PDF signed document → attempt `DjotThenSave` → ProvenanceViolation raised.
- Test: born-Djot document → `DjotThenSave` permitted; roundtrip preserves content.

### D6: AppContext integration
**Files:** `src/core/AppContext.h`, `src/app/Bootstrapper.cpp`
**Acceptance:**
- `AppContext` adds `std::shared_ptr<IDjotCodec> djotCodec` + `std::shared_ptr<IDjotMapper> djotMapper` + `std::shared_ptr<ProvenanceGuard> provenanceGuard`.
- Bootstrapper constructs them; LuaDjotCodec instantiated with sandboxed Lua VM.

### D7: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`, `LICENSE-3RD-PARTY.md`, `README.md`
**Acceptance:**
- CHANGELOG: "WS2 Phase 1.5 — Djot interchange foundation. `docmodel` (SemanticDocument tree with ProvenanceTag) + `pdfws_djot` (LuaDjotCodec via vendored liblua 5.4 reference parser; IDjotMapper PDF↔Semantic). ProvenanceGuard refuses Djot edit-save-back for born-PDF signed documents. Round-trip property test (1000 random docs)."
- ROADMAP Phase 1.5: mark ✅ DONE.
- LICENSE-3RD-PARTY: add `liblua 5.4.x — MIT — vendored at third_party/lua-5.4/`. Add `Djot reference impl — MIT — vendored at third_party/djot/`.
- README architecture section: add dual-model + ProvenanceGuard mention.

</deliverables>

<verification>
```powershell
cd build
$env:QT_QPA_PLATFORM='offscreen'
ctest -R TestDjotRoundtrip --output-on-failure
# Expected: PASS (1000/1000 roundtrips structurally equivalent; provenance violation correctly raised)
ctest --output-on-failure -j4
# Expected: 17/17 pass (15 from M2 + TestFormBuilder etc. from M3 + TestDjotRoundtrip)
```
</verification>

<constraints>
- DO NOT reimplement the Djot grammar — vendor the Lua reference parser (R11 risk if reimplemented).
- DO NOT make `docmodel` depend on Qt. Pure C++20 only.
- DO NOT skip the Lua sandbox setup; user-supplied Djot is untrusted input.
- DO NOT implement Djot→PDF save-back in this phase; that's M5 / M6 work.
- DO NOT bypass ProvenanceGuard at the API level. It's a hard constraint, not a polish.
- DO NOT use `git submodule` for third_party — direct vendoring with pinned commit recorded in `DJOT_SPEC_VERSION` is simpler and audit-friendly.
</constraints>

<success_criteria>
- [ ] `docmodel` + `pdfws_djot` libraries build cleanly
- [ ] 1000-doc roundtrip property test passes
- [ ] ProvenanceGuard rejects born-PDF + signed Djot save
- [ ] Lua sandbox confirmed (try `require 'io'` from a Djot block → fails)
- [ ] DJOT_SPEC_VERSION file present with pinned SHAs
- [ ] CHANGELOG + ROADMAP + LICENSE-3RD-PARTY updated
- [ ] Commit: `feat(djot): WS2 Phase 1.5 — docmodel + pdfws_djot foundation with ProvenanceGuard`
</success_criteria>

---

## M4-PROMPT-6 — Edge fixes (Strikeout/Squiggly real ToolModes, Share with attachment, click-to-place fields)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Real ToolMode::Strikeout + ToolMode::Squiggly
Currently aliased to Underline (EditController.cpp:70-71). Add to PdfEnums.h; wire AnnotationLayer to render strikethrough line and squiggly underline.

### D2: Share with attachment
HomeController:119-126 opens `mailto:` without attaching PDF. Use Windows MAPI (`MAPISendMail`) to compose an email with PDF attached.

### D3: Click-to-place form fields (instead of hardcoded QRectF(100,100))
FormsController:87-209 places every new field at hardcoded position. Change to: arm placement mode, next canvas click places field at that position. (Overlaps with M3-PROMPT-1 D2 — coordinate.)

### D4: Welcome widget: prune missing recent files
WelcomeWidget:256-269 dims missing files but keeps them. On click of missing file, prompt to remove from recents.

### D5: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] 4 edge UX issues closed
- [ ] Commit: `fix(ui): real Strikeout/Squiggly modes; Share-with-attachment via MAPI; click-to-place form fields; prune missing recents`
</success_criteria>

---

# MONTH 5 — OCR ensemble + Office import

---

## M5-PROMPT-1 — RapidOcrEngine real PP-OCRv5 implementation

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M5 | Priority: CRITICAL (closes "RapidOCR Mock" CHANGELOG admission)
Depends on: M1 (engine selector gated)
Agents: /backend (heavy)
Estimated context: ~45% | Effort: 2-3 person-weeks
</session_metadata>

<role>
You are a senior C++ ML inference engineer with ONNX Runtime expertise. You understand PaddleOCR PP-OCRv5 architecture: DBNet text detection → perspective transform per detection → SVTR text recognition → CTC decoding.
</role>

<current_state>
- `src/engines/ocr/RapidOcrEngine.cpp:112-127` is an explicit STUB returning hardcoded `"RapidOCR_Mock"`.
- 3 ONNX Sessions correctly constructed at lines 53-91 (det, cls, rec) and warm-kept.
- `isMockImplementation()` returns true (from M1 PROMPT-7); UI runtime-disables RapidOCR/ROVER selector.
</current_state>

<deliverables>

### D1: DBNet detection output decoding
**Files:** `src/engines/ocr/RapidOcrEngine.cpp` (replace stub block 112-127). New helpers in `src/engines/ocr/PpOcrDecoder.h/.cpp`.
**Acceptance:**
- Take DBNet session output tensor (1, 1, H, W) probability map.
- Threshold (default 0.3); morphological dilate.
- Extract polygons via connected components; minimum-area rectangle per polygon.
- Return `QList<QPolygonF>` of detection boxes.

### D2: Perspective transform per detection
**Files:** `src/engines/ocr/PpOcrDecoder.cpp`
**Acceptance:**
- Given QPolygonF + source QImage → cv::Mat-style warpPerspective to standard rect (e.g., 32×320 px for SVTR input).
- No OpenCV dependency — implement minimal 3x3 homography + bilinear sample (small helper, ~80 lines).

### D3: SVTR recognition + CTC decode
**Files:** `src/engines/ocr/PpOcrDecoder.cpp`
**Acceptance:**
- Run rec session per perspective-transformed crop.
- CTC decode output sequence using PP-OCRv5 vocabulary file (bundle in `models/ppocr_v5_keys.txt`).
- Return decoded text + confidence (mean of per-character softmax).

### D4: Wire into RapidOcrEngine::processImage
**Files:** `src/engines/ocr/RapidOcrEngine.cpp`
**Acceptance:**
- Replace stub; full pipeline det → cls (orientation) → perspective → rec → results.
- Return `QList<OcrResult>` with text + bbox + confidence per detection.
- `isMockImplementation()` returns false.

### D5: Tests
**Files:** `tests/TestRapidOcr.cpp` (new)
**Acceptance:**
- Recognize text from known test image (`tests/fixtures/ocr/printed_english.png`) → confidence > 80%, text matches expected ≥90%.
- CJK test image → recognizes (PP-OCRv5 multilingual).
- ROVER merge with Tesseract: combined accuracy ≥ either alone.

### D6: UI re-enables RapidOCR + ROVER
**Files:** `src/modes/OCRMode.cpp` (the gate added in M1 PROMPT-7)
**Acceptance:** Since `isMockImplementation()` now returns false, the selector auto-enables. Verify in UI.

### D7: CHANGELOG + ROADMAP
- CHANGELOG: REMOVE "RapidOCR PP-OCRv5 tensor pre/post processing is STUB" admission.
- ROADMAP: Session 9 D2 mark ✅ DONE.

</deliverables>

<success_criteria>
- [ ] RapidOcrEngine produces real text from images
- [ ] ROVER ensemble outperforms Tesseract-alone on test images
- [ ] CHANGELOG admission removed
- [ ] Commit: `feat(ocr): real PP-OCRv5 implementation in RapidOcrEngine (DBNet + perspective + SVTR + CTC)`
</success_criteria>

---

## M5-PROMPT-2 — Layout detector ensemble + OCR pipeline integration (WS1 completion)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M5 | Priority: HIGH (closes WS1) | Effort: 2-3 person-weeks
Depends on: M2-PROMPT-5 (LaneScheduler infrastructure already in place); M5-PROMPT-1 (RapidOCR real)
Agents: /backend (ML inference), /performance
Estimated context: ~40%
</session_metadata>

<role>
You are a senior ML pipeline engineer specializing in document-layout detection and multi-detector ensembling. You understand PP-DocLayoutV2 / PP-StructureV3 (PaddlePaddle layout models), Surya (transformer-based layout), and the IoU reconciliation pattern when two detectors disagree on region boundaries or types.
</role>

<context>
LaneScheduler infrastructure already landed in M2 (M2-PROMPT-5). M5-PROMPT-1 made RapidOCR real (PP-OCRv5 inference). This prompt adds the layout-first pass + ensemble reconciliation that feeds per-region OCR through LaneScheduler.
</context>

<deliverables>

### D1: ILayoutDetector interface + PP-DocLayoutV2 detector
**Files (NEW):** `src/engines/ocr/ILayoutDetector.h`, `src/engines/ocr/PpDocLayoutDetector.h + .cpp`
**Acceptance:**
- `enum class RegionType { Title, Paragraph, Table, Figure, List, Header, Footer, Equation, Reference, Caption, Other }`
- `struct LayoutRegion { QRectF bbox; RegionType type; int readingOrderIndex; double confidence; }`
- `class ILayoutDetector { virtual QList<LayoutRegion> detect(QImage page, Lane lane = Lane::GPU) = 0; }`
- `PpDocLayoutDetector` loads PP-DocLayoutV2 ONNX model (bundle in `models/pp_doclayout_v2.onnx`; verify Apache-2.0 model weight license at download); runs detection via `AppContext::scheduler->submit(Lane::GPU, ...)`.

### D2: SuryaDetector (conditional on license verification)
**Files (NEW):** `src/engines/ocr/SuryaDetector.h + .cpp` OR a documented stub
**Acceptance:**
- Surya license verification (visit github.com/VikParuchuri/surya): if GPL-3.0, do NOT integrate as in-process library — either invoke as subprocess (acceptable per veraPDF pattern) OR replace with a second Apache/MIT-licensed layout detector (e.g., LayoutLMv3 ONNX export).
- If license OK + integration straightforward: `SuryaDetector` as second ILayoutDetector impl.
- If license blocking: ship with PP-DocLayout-only and `SuryaDetector` as TODO stub with explanatory header comment; LayoutEnsemble (D3) falls back to single-detector mode gracefully.

### D3: LayoutEnsemble with IoU reconciliation
**Files (NEW):** `src/engines/ocr/LayoutEnsemble.h + .cpp`
**Acceptance:**
- Constructor takes 1-or-more `ILayoutDetector*`.
- `detect(QImage page)` runs each detector in parallel (via `AppContext::scheduler->submit(Lane::GPU)`); collects all region maps.
- IoU reconciliation: regions with IoU > 0.5 between detectors merge into one (averaged bbox); type votes with confidence tiebreak on disagreement; reading-order index re-computed from merged region centroids.
- Returns single `QList<LayoutRegion>` reconciled.
- Edge: single-detector mode (Surya unavailable) returns that detector's output directly.

### D4: OcrPipeline integration with cross-page pipelining
**Files:** `src/engines/ocr/OcrPipeline.h + .cpp`
**Acceptance:**
- New entry point: `recognizeDocument(QString pdfPath) -> QFuture<QList<PageOcrResult>>`.
- Uses `LaneScheduler::CrossPagePipeline` with three stages:
  - Stage 1 (GPU): LayoutEnsemble per page
  - Stage 2 (mixed): per-region OCR fanout — Tesseract on CPU lane + RapidOCR on GPU lane, parallel per region; word-level ROVER fusion (existing logic; gated so secondary engine is authoritative only when Tesseract MeanTextConf < 70).
  - Stage 3 (CPU): produce final `PageOcrResult` (regions + fused per-word text + confidence).
- Cross-page pipeline: layout(P+1) ‖ ocr(P) ‖ fusion(P-1) via `LaneScheduler::CrossPagePipeline` helper from M2-PROMPT-5.
- Results delivered in page order via OrderedResultQueue; missing pages produce sentinel error (not gaps).

### D5: OCRMode UI — fused per-word confidence + per-region redo
**Files:** `src/modes/OCRMode.cpp`
**Acceptance:**
- Per-word confidence overlay (green ≥ 90 / yellow 70-89 / red < 70) at `OCRMode.cpp:352` (the existing em-dash placeholder).
- Per-region redo button: right-click a region in the OCR preview → "Re-OCR this region" → re-submits just that region to `OcrPipeline::recognizeRegion(pageIndex, regionIndex)`.
- "Review before save" workflow: user can accept/reject per-region results before they're written back into the document.

### D6: Tests
**Files (NEW):** `tests/TestLayoutEnsemble.cpp`, extend `tests/TestRapidOcr.cpp`
**Acceptance:**
- LayoutEnsemble test: synthetic 2-page document with known regions; both detectors agree → single merged map; one detector returns extra region with confidence < other → suppressed in merge.
- OcrPipeline cross-page test: 5-page document; verify pipelining overlaps via LaneScheduler timing assertions (total time < sum of serial stage times).
- Per-region redo: trigger re-OCR on one region; verify only that region's text changes.

### D7: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`
**Acceptance:**
- CHANGELOG: REMOVE "OCR ensemble pipeline (PP-DocLayoutV2, Surya, LaneScheduler) not yet implemented" admission. ADD: "OCR ensemble pipeline (WS1): layout-first detection via PP-DocLayoutV2 (+ Surya when license permits), per-region Tesseract+RapidOCR fanout via LaneScheduler, word-level ROVER fusion, cross-page pipelining. Per-region redo + per-word confidence overlay in OCRMode."
- ROADMAP §"Session 9 — OCR Ensemble Pipeline (WS1)": mark ✅ DONE.

</deliverables>

<success_criteria>
- [ ] WS1 OCR ensemble pipeline complete per ROADMAP architecture diagram
- [ ] CHANGELOG admission removed
- [ ] Layout ensemble works with 1 or 2 detectors
- [ ] Cross-page pipelining via LaneScheduler verified by timing test
- [ ] Per-region redo + confidence overlay in OCRMode
- [ ] Commit: `feat(ocr): layout detector ensemble + cross-page pipelining via LaneScheduler (WS1 completion)`
</success_criteria>

---

## M5-PROMPT-3 — Office→PDF import + complete OOSS

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M5 | Priority: MEDIUM | Effort: 1-2 person-weeks
</session_metadata>

<deliverables>

### D1: HAS_LIBREOFFICE CMake option
**Files:** `CMakeLists.txt`, `src/engines/ConversionManager.cpp:339`
**Acceptance:** Define HAS_LIBREOFFICE when `find_program(LIBREOFFICE_SOFFICE soffice)` succeeds. Detection at configure time; if found, code path activates.

### D2: convertOfficeToPdf real implementation
**Files:** `src/engines/ConversionManager.cpp`
**Acceptance:**
- QProcess invoke: `soffice --headless --convert-to pdf:writer_pdf_Export --outdir <dir> <input.docx>`.
- Timeout 60s; on timeout: kill process tree (handle spawned children).
- Supports docx, xlsx, pptx, odt, ods, odp, rtf, txt, csv input.

### D3: Images→PDF
**Files:** `src/engines/ConversionManager.cpp` (new method `imagesToPdf`)
**Acceptance:** Take list of image paths → use PoDoFo PdfImage XObject embedding → one page per image at configurable DPI.

### D4: UI: add Office Import + Images Import to HomeController
**Files:** `src/shell/controllers/HomeController.cpp`
**Acceptance:** File menu / Welcome cards offer "Import Office Document" + "Images to PDF".

### D5: Tests + CHANGELOG + remove the "Office→PDF import not implemented" admission

</deliverables>

<success_criteria>
- [ ] Office docx/xlsx/pptx import to PDF works (when LibreOffice installed)
- [ ] Images to PDF works
- [ ] Commit: `feat(convert): Office→PDF import via LibreOffice subprocess + Images→PDF`
</success_criteria>

---

## M5-PROMPT-4 — WS2 OCR→Djot mapping (WS2 role 1)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M5 / WS2 role 1 | Priority: HIGH (closes WS2 role 1; feeds M7 MRC sandwich text)
Depends on: M4-PROMPT-7 (Djot foundation); M5-PROMPT-2 (OCR ensemble producing layout regions + fused word boxes)
Agents: /backend
Estimated context: ~20% | Effort: 4-6 days
</session_metadata>

<role>
You are an interchange-format engineer mapping fused OCR output (layout regions + per-word fused text with confidence + bboxes) into the docmodel SemanticDocument tree with PDF fidelity attributes.
</role>

<deliverables>

### D1: OcrDjotMapper helper
**Files (NEW):** `src/engines/ocr/OcrDjotMapper.h + .cpp`
**Acceptance:**
- `class OcrDjotMapper { SemanticDocument fromOcrResults(const QList<PageOcrResult>&); }`
- Reading-order index from LayoutEnsemble → block order in SemanticDocument.
- RegionType → Djot container class: Title → Header level 1, Paragraph → Paragraph, Table → Table with pipe-table cells, Figure → Figure block, List → List, Header/Footer → marked as page-header/page-footer attributes, etc.
- Per-word fused result → Inline span with attributes: `{pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}`.
- Tables: cell layout from LayoutEnsemble's table-region structure → Djot pipe-table syntax. Cell text from per-cell OCR words.
- ProvenanceTag: every node marked `BornOCR` with source PDF path + page index + bbox.

### D2: Integration into OcrPipeline
**Files:** `src/engines/ocr/OcrPipeline.cpp`
**Acceptance:**
- After fusion stage (M5-PROMPT-2 D4 stage 3), emit `SemanticDocument` via OcrDjotMapper.
- Existing per-page `PageOcrResult` retained (back-compat for other consumers).
- New API: `OcrPipeline::recognizeDocumentAsDjot(pdfPath) -> QFuture<SemanticDocument>`.

### D3: OCRMode review UI uses Djot
**Files:** `src/modes/OCRMode.cpp`
**Acceptance:**
- "Review before save" workflow renders SemanticDocument via docmodel → HTML preview.
- Edit-in-place: user can correct OCR errors via Djot-aware editor (same widget pattern as M6-PROMPT-4 annotation editor).
- Per-region accept/reject still works; rejected regions stay as raw OCR text without Djot mapping.

### D4: Tests
**Files (NEW):** `tests/TestOcrDjotMapper.cpp`
**Acceptance:**
- Mock OcrResults with known layout regions + words → SemanticDocument structurally matches expected tree.
- Roundtrip via M4-PROMPT-7 codec: encode SemanticDocument → Djot → decode → equivalent tree.
- Table region maps to Djot pipe table; cell text round-trips.

### D5: Documentation
**Files:** `CHANGELOG.md`
**Acceptance:** CHANGELOG: "OCR output now maps to SemanticDocument via OcrDjotMapper (WS2 role 1): layout regions → block structure; per-word fused text → Inline spans with `{pdf-page pdf-bbox pdf-font ocr-conf}` attributes; tables → Djot pipe tables. Feeds OCRMode review UI + M7 MRC sandwich text layer."

</deliverables>

<success_criteria>
- [ ] OcrDjotMapper produces valid SemanticDocument from OCR output
- [ ] Roundtrip preserves structure + attributes
- [ ] OCRMode review UI works with Djot
- [ ] Commit: `feat(ocr): map fused OCR output to Djot SemanticDocument (WS2 role 1)`
</success_criteria>

---

# MONTH 6 — Polish + integration

---

## M6-PROMPT-1 — DiffEngine LCS/Myers upgrade

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M6 | Priority: MEDIUM (closes "DiffEngine uses set-difference" CHANGELOG admission)
Effort: 1 person-week
</session_metadata>

<role>Diff algorithms engineer.</role>

<deliverables>

### D1: Myers LCS implementation
**Files (NEW):** `src/engines/MyersDiff.h + .cpp`
**Acceptance:** Pure-C++ Myers 1986 LCS implementation. Operates on token sequences. Returns edit script (insert/delete/keep) with positions.

### D2: Move detection
**Files:** `src/engines/MyersDiff.cpp`
**Acceptance:** Post-pass: identify deletes + inserts of identical token sequences → reclassify as moves with source + target positions.

### D3: Replace DiffEngine implementation
**Files:** `src/engines/DiffEngine.cpp:48-69`
**Acceptance:** Drop QSet-difference; route through MyersDiff. DiffResult gains `MoveOperation` type.

### D4: CompareWidget renders moves
**Files:** `src/ui/CompareWidget.cpp`
**Acceptance:** Moves rendered in third color (e.g., orange) distinct from add (green) / delete (red).

### D5: Tests
**Acceptance:** Legal-document pair with paragraph reordering → moves detected (not add+delete pairs).

### D6: CHANGELOG admission removed

</deliverables>

<success_criteria>
- [ ] Moves detected correctly
- [ ] Commit: `feat(diff): Myers LCS with move detection (replaces set-difference; legal/compliance fix)`
</success_criteria>

---

## M6-PROMPT-2 — Localization populate ar/fr/de

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M6 | Effort: variable (translator commission time dominates) | Engineering effort: 3-5 days
</session_metadata>

<deliverables>

### D1: Run lupdate to populate .ts files
**Acceptance:** `lupdate src/ -ts translations/glyphpdf_ar.ts translations/glyphpdf_fr.ts translations/glyphpdf_de.ts` populates all `tr()` strings.

### D2: Commission translators (vendor task; out of scope for Claude session)
**Acceptance:** Placeholder bullets in `translations/README.md` noting commissioning status + contact info for each language. Translators deliver back the populated .ts files.

### D3: lrelease + bundle .qm into resources
**Files:** `CMakeLists.txt`, `resources.qrc`
**Acceptance:** Translated .ts files compiled to .qm via `qt_add_translations()` and embedded via QRC.

### D4: Per-widget RTL audit for Arabic
**Files:** all UI widgets
**Acceptance:** Test in Arabic locale; flag widgets with broken RTL (mirrored buttons, text alignment, scroll direction). Fix or document.

### D5: Re-enable ar/fr/de in PreferencesDialog
**Files:** `src/ui/PreferencesDialog.cpp:46-58`
**Acceptance:** Language combo lists ar/fr/de as functional once .qm files are populated.

### D6: CHANGELOG admission removed

</deliverables>

<success_criteria>
- [ ] All UI strings translated for ar/fr/de
- [ ] RTL works correctly for Arabic
- [ ] Commit: `feat(i18n): ar/fr/de translations populated + RTL audit complete`
</success_criteria>

---

## M6-PROMPT-3 — AI backend wiring (Anthropic + OpenAI + Gemini + local Ollama)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M6 | Priority: MEDIUM (closes AIChatPanel canned-reply gap)
Effort: 1-1.5 person-weeks
</session_metadata>

<role>LLM API integration engineer.</role>

<deliverables>

### D1: IAiProvider interface
**Files (NEW):** `src/engines/ai/IAiProvider.h`
**Acceptance:** `class IAiProvider { virtual QFuture<QString> chat(QList<Message>, AiOptions) = 0; virtual bool isReady() = 0; virtual QString providerName() = 0; }`. Streaming via QFuture chained signals.

### D2: AnthropicProvider, OpenAiProvider, GeminiProvider, OllamaProvider
**Files (NEW):** `src/engines/ai/AnthropicProvider.cpp` (etc.)
**Acceptance:**
- Anthropic Claude API (default; uses CredentialManager-stored sk-ant-...).
- OpenAI ChatGPT API.
- Google Gemini API.
- Ollama local HTTP (default endpoint http://localhost:11434).
- Each provider handles its own auth + streaming format. All return streamed tokens via QFuture's progressUpdated.

### D3: Replace AIChatPanel canned reply
**Files:** `src/modes/AIChatPanel.cpp:71-79`
**Acceptance:**
- Read selected provider from QSettings.
- Compose context: current page text (via PDFium extract) + chat history.
- Stream response into the message list with typing-cursor indicator.
- Citations: detect page references in response ("page 5") and make them clickable.

### D4: PreferencesDialog provider selection
**Files:** `src/ui/PreferencesDialog.cpp:140-142`
**Acceptance:** Un-disable OpenAI / Gemini / Ollama entries. Test-key button does real round-trip (1-token ping) instead of format-only check.

### D5: Tests
**Acceptance:** Mock provider for tests (no real API calls in CI). Real round-trip test gated behind env-var ANTHROPIC_API_KEY etc.

### D6: CHANGELOG admission removed

</deliverables>

<success_criteria>
- [ ] AI Chat works end-to-end with at least Anthropic provider
- [ ] Streaming response with citations
- [ ] Commit: `feat(ai): AIChatPanel wired with Anthropic/OpenAI/Gemini/Ollama providers + streaming + citations`
</success_criteria>

---

## M6-PROMPT-4 — WS2 Djot annotation rich-text model (third Djot role)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M6 | Priority: HIGH (third WS2 role — annotation/comment rich text)
Depends on: M4-PROMPT-7 (WS2 Djot foundation already landed); M3-PROMPT-5 (InspectorWidget Properties bound)
Agents: /backend (annotation serialization), /frontend (InspectorWidget editor)
Estimated context: ~25% | Effort: 1 person-week
</session_metadata>

<role>
You are a senior PDF + annotation engineer who understands PDF's required rich-text formats: `/RC` (restricted XHTML subset for Acrobat/Foxit interop) and `/Contents` (plain text fallback). You know that storing arbitrary Djot in `/RC` violates the spec, but a `/PieceInfo` sidecar lets GlyphPDF preserve perfect round-trip while interoperating with other readers.
</role>

<context>
WS2 foundation (docmodel + pdfws_djot) landed in M4-PROMPT-7. M3-PROMPT-5 bound InspectorWidget contents to annotation.text. This prompt makes Djot the **internal authoring model** for annotation rich text — with transcoding on save for interop.
</context>

<deliverables>

### D1: AnnotationItem gains rich-text Djot field
**Files:** `src/core/AnnotationTypes.h`
**Acceptance:**
- Add `QString djotSource` field to `AnnotationItem` (existing `text` stays as plain-text fallback).
- Serializer round-trip: existing JSON serialization extended to preserve djotSource.

### D2: InspectorWidget Djot editor
**Files:** `src/ui/InspectorWidget.cpp` (Contents editor in Properties tab)
**Acceptance:**
- Replace plain QTextEdit with a Djot-aware editor: live render-preview pane on right showing the rendered output via docmodel → simple HTML preview.
- Toolbar: bold (`*x*`), italic (`_x_`), code (\`x\`), link (`[x](url)`), list, heading.
- Edits write to `annotation.djotSource`; auto-derive `annotation.text` as plain-text projection via `IDjotCodec::decode` + flatten.

### D3: Save-time transcoding
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (embedAnnotations)
**Acceptance:**
- When writing annotation to PDF:
  - `/Contents` ← plain-text projection of djotSource (spec-compliant plain text).
  - `/RC` ← XHTML-subset transcoding via `pdfws_djot::DjotToRichTextXhtml` (new helper) per ISO 32000 §12.5.6.4 — wraps in `<body>` with allowed inline tags only.
  - `/PieceInfo /GlyphPDF /DjotSource (...)` ← original Djot source, escaped per M2-PROMPT-1 content-stream injection rules.

### D4: Load-time round-trip
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (annotation loading)
**Acceptance:**
- When loading annotation: if `/PieceInfo /GlyphPDF /DjotSource` present, restore `djotSource` from it. Else fall back to deriving djotSource from `/Contents` (plain text → trivial djotSource).
- Perfect GlyphPDF→PDF→GlyphPDF round-trip preserves formatting.

### D5: Tests
**Files (NEW):** `tests/TestAnnotationDjot.cpp`
**Acceptance:**
- Author annotation with formatting (`*bold* and _italic_ and \`code\``) → save → reopen → djotSource matches original.
- Open same PDF in Adobe Reader (manual smoke) → annotation rich text visible per Adobe's /RC renderer.
- Plain-text-only annotation (no djotSource sidecar) → loads with djotSource = plain text.

### D6: Comment threading depth (consolidates M6-PROMPT-5 D1-D3)
**Files:** `src/ui/CommentsWidget.cpp` (covered in M6-PROMPT-5 — verify integration)
**Acceptance:** Comments now use Djot rich-text editor in reply thread; same /RC + /PieceInfo dual-write on save.

### D7: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`
**Acceptance:**
- CHANGELOG: "Annotation + comment rich text uses Djot as internal authoring model; transcodes to PDF /RC XHTML + /Contents plain text on save; original Djot stashed in /PieceInfo for perfect GlyphPDF round-trip while preserving Acrobat/Foxit interop (WS2 role 3)."
- ROADMAP Session 8 D5: mark ✅ DONE with Djot integration note.

</deliverables>

<success_criteria>
- [ ] Djot rich-text editor in InspectorWidget + CommentsWidget
- [ ] Save/load round-trip preserves formatting via /PieceInfo sidecar
- [ ] /RC XHTML transcoding interoperates with Adobe Reader
- [ ] Commit: `feat(annotations): Djot rich-text internal model with /RC + /Contents + /PieceInfo dual-write (WS2 role 3)`
</success_criteria>

---

## M6-PROMPT-5 — Comment threading depth (replies + /IRT)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M6 | Effort: 4-6 days
</session_metadata>

<deliverables>

### D1: Threaded view in CommentsWidget
**Files:** `src/ui/CommentsWidget.cpp`
**Acceptance:** Comments displayed hierarchically with indent per reply depth. Filter by status/author/date.

### D2: /IRT (In Reply To) linking on save
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (embedAnnotations)
**Acceptance:** When AnnotationItem has parentId, write `/IRT <parent annot ref>` to the PDF annotation dict per ISO 32000 §12.5.6.4.

### D3: changeReviewState wired
**Files:** `src/ui/CommentsWidget.cpp:364-367` (the empty stub)
**Acceptance:** Real implementation that updates AnnotationItem.reviewState + saves via EditAnnotationCommand. Context menu shows Open/Accepted/Rejected/Cancelled/Completed.

### D4: Tests + CHANGELOG

</deliverables>

<success_criteria>
- [ ] Replies threaded + persist across save/reload
- [ ] Review states roundtrip
- [ ] Commit: `feat(comments): threading depth (replies + IRT + reviewState)`
</success_criteria>

---

## M6-PROMPT-6 — Edge-case cleanup pass

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: SignaturesPanel real data
**Files:** `src/modes/SignaturesPanel.cpp:43-89`
**Acceptance:** Replace hardcoded "Elie Matta / GlobalSign CA" with real ISignatureManager::validateSignatures output for currently loaded doc. Place Signature button wired to ribbon's Sign flow.

### D2: ThumbnailSidebar real thumbnails
**Files:** `src/ui/ThumbnailSidebar.cpp:229-269`
**Acceptance:** Replace placeholder paper widgets with real PDFium-rendered thumbnails (cached via RenderCache at low DPI, e.g., 75 DPI for sidebar).

### D3: Sidebar Files attachment extract real bytes
**Files:** `src/shell/Sidebar.cpp:100-121`
**Acceptance:** Replace placeholder text-stub with real PoDoFo embedded-file byte extraction. Saves real attachment data, not stub.

### D4: StatusBar OCR/UTF-8 cells
**Files:** `src/shell/StatusBar.cpp:67, 69, 187, 193`
**Acceptance:** OCR language cell shows real selected OCR language from QSettings. Encoding cell removed (PDFs aren't UTF-8; meaningless cell).

### D5: PreferencesDialog Test Key does real round-trip (covered by M6-PROMPT-3 D4 — verify here).

### D6: BatesNumber + Resize legal-vertical polish
**Files:** packaging for Bates dialog (already wired in M4)
**Acceptance:** Bates dialog supports custom presets + range selection UI.

### D7: CHANGELOG

</deliverables>

<success_criteria>
- [ ] All previously-decorative panels show real data
- [ ] Commit: `fix(ui): SignaturesPanel/Thumbnails/AttachmentExtract/StatusBar real data`
</success_criteria>

---

# MONTH 7 — Hardening

---

## M7-PROMPT-1 — Third-party security audit prep + execution

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M7 | Priority: HIGH (legitimizes security claims for marketing)
Effort: 2-3 person-weeks (mostly waiting for auditor)
</session_metadata>

<role>Security engineer coordinating external audit.</role>

<deliverables>

### D1: Pick an auditor
**Acceptance:** Shortlist 2-3 firms (Trail of Bits, NCC Group, Cure53, PDF Association partners). Get quotes. ~$15-40K typical for a 1-2 week PDF tool audit.

### D2: Provide auditor with kit
**Acceptance:** Repo access (read-only commit signing key share OR public OSS repo), threat model doc, list of security claims (PAdES B-LTA, Edact-Ray defense, sanitization vectors, redaction surgery), test fixtures, build instructions.

### D3: Address audit findings
**Acceptance:** Track findings in GitHub Issues with severity labels. Fix CRITICAL + HIGH before launch. MEDIUM may slip to v1.1.

### D4: Audit-letter for marketing
**Acceptance:** Auditor provides letter summarizing scope + findings + remediation. Published on website + linked from README security section.

</deliverables>

<success_criteria>
- [ ] Audit complete; letter on file
- [ ] All CRITICAL findings fixed
- [ ] Public security disclosure page drafted
</success_criteria>

---

## M7-PROMPT-2 — Performance tuning + bug bash

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Performance baselines + regression suite
**Acceptance:** Measure: doc open time (target <3s for 100p), search latency (<1s), OCR throughput, RenderCache hit rate. Add to TestPerformance.

### D2: Profile hot paths
**Acceptance:** Use Tracy or Easy_Profiler; find top 5 hot functions; optimize.

### D3: Bug bash
**Acceptance:** Internal/beta-tester multi-day bug bash. Triage in GitHub; fix release-blockers.

### D4: Long-running stability
**Acceptance:** Open app, run for 48h with periodic doc loads + edits + saves; check for memory leaks (Heaptrack or similar). Fix leaks.

</deliverables>

<success_criteria>
- [ ] All performance targets met
- [ ] No memory leaks after 48h
- [ ] Critical bugs from beta closed
</success_criteria>

---

## M7-PROMPT-3 — WS3 MRC compression pipeline ("Modern DjVu" in PDF/A)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<session_metadata>
Phase: M7 / WS3 | Priority: HIGH (closes WS3; unlocks scan-archival use case for legal/medical persona)
Depends on: M5-PROMPT-2 (LayoutEnsemble + word boxes); M5-PROMPT-4 (OCR→Djot mapping for sandwich text); M2-PROMPT-5 (LaneScheduler for per-page concurrency)
Agents: /backend (image compression), /security (license verify JBIG2 encoder), /performance
Estimated context: ~45% | Effort: 2-3 person-weeks
</session_metadata>

<role>
You are a senior image-compression engineer specializing in Mixed Raster Content (MRC) layered compression for scanned-document PDFs. You understand: (1) JBIG2 as the standardized descendant of DjVu's JB2 for bitonal (text/line-art) compression, (2) JPEG2000 (via OpenJPEG, BSD-2) for continuous-tone backgrounds, (3) the invisible OCR text "sandwich" layer aligned to word boxes for searchability, (4) the 2013 Xerox JBIG2 pattern-matching incident (digit substitution bug — German BSI banned the pattern-matching mode), and (5) why DjVu OUTPUT is excluded (dead browser support; minimally maintained) while DjVu's MRC TECHNIQUE inside PDF/A is the right modern answer.
</role>

<project_context>
Per ROADMAP §"Phase 6 — Conversion + MRC Compression Pipeline (Session 13 WS3)" and §"Risk Register R6" (JBIG2 encoder license must be non-GPL/non-AGPL). MRC achieves 5-10× size reduction for scanned content vs naive image-PDF; remains universally supported PDF/A archival standard. No DjVu output (legacy); optional DjVu importer if legacy corpus ingestion needed.
</project_context>

<current_state>
- Standard compression (downsample, font subset, unused-object removal) exists via `IPdfEditorEngine::optimizeDocument` + `estimateOptimization`.
- LayoutEnsemble + OCR ensemble (M5) produce per-page layout regions + fused word boxes ready to feed MRC layer separation.
- OcrDjotMapper (M5-PROMPT-4) produces SemanticDocument that can carry MRC layer metadata.
- No JBIG2 encoder, no JPEG2000 encoder, no MRC pipeline exist in `src/`.
- ROADMAP R6 risk: must verify JBIG2 encoder license before integration. Options: `jbig2enc` (Apache-2.0 fork exists), `libjbig2` (BSD-like). Audit before merging.
</current_state>

<objective>
Build the MRC layer-separation + encoding pipeline: per-page layout/mask separation → JBIG2 bitonal foreground (text/line-art using layout regions to guide) + JPEG2000 background (continuous tone) + invisible OCR sandwich text aligned to fused word boxes → PDF/A-2b. Add DjVu importer (optional, gated on `HAS_DJVU` for legacy ingestion). Verify all encoder licenses Apache/BSD/MIT.
</objective>

<files_to_read>
ROADMAP.md §"Phase 6 — Conversion + MRC Compression Pipeline" (full spec)
ROADMAP.md §"Risk Register R6" (JBIG2 encoder license)
ROADMAP.md §"Forbidden Dependencies" (DjVu output excluded)
src/engines/PdfEditorEngine.cpp (optimizeDocument + exportPdfA)
src/engines/ocr/OcrPipeline.cpp (M5 — provides word boxes)
src/engines/ocr/LayoutEnsemble.cpp (M5 — provides region types for mask separation)
src/engines/scheduling/LaneScheduler.cpp (M2 — per-page concurrency)
CMakeLists.txt (add jbig2enc + OpenJPEG deps)
</files_to_read>

<deliverables>

### D1: License verification + encoder selection
**Files (NEW):** `docs/MRC-ENCODER-LICENSE-AUDIT.md`
**Acceptance:**
- Audit JBIG2 encoders: `jbig2enc` (https://github.com/agl/jbig2enc — original Apache-2.0; verify), `libjbig2` (BSD), Adobe JBIG2 Codec (proprietary; reject). Pin chosen encoder + license + version.
- Audit JPEG2000: OpenJPEG 2.x (BSD-2-Clause; already confirmed in ROADMAP license matrix).
- Audit OpenJPEG MSYS2 package: `mingw-w64-x86_64-openjpeg2`. Verify availability.
- Audit chosen JBIG2 encoder MSYS2 package availability OR plan source-build via FetchContent.
- Update `LICENSE-3RD-PARTY.md` with chosen versions + verified licenses.
- **Critical: STOP if no permissive JBIG2 encoder confirmed.** Report to user; defer MRC to v2.0.

### D2: MRC layer separation
**Files (NEW):** `src/engines/mrc/MrcPageProcessor.h + .cpp`, `src/engines/mrc/IPageMaskSeparator.h`
**Acceptance:**
- `class MrcPageProcessor { MrcLayers separatePage(QImage page, QList<LayoutRegion> regions, QList<OcrWord> words); }`
- `MrcLayers { QImage foregroundMask; QImage background; QList<OcrWord> sandwichText; }`
- Layout regions from M5 LayoutEnsemble guide separation: text-region pixels → foreground bitonal mask; figure/photo regions → background continuous tone; mixed regions → finer-grained per-pixel threshold.
- Foreground mask: 1-bit per pixel; text + line-art only.
- Background: 8-bit grayscale or 24-bit RGB; everything else.

### D3: JBIG2 + JPEG2000 encoding
**Files:** `src/engines/mrc/MrcPageProcessor.cpp` (extend)
**Acceptance:**
- Foreground mask → JBIG2 encoding via chosen encoder.
  - **NEVER pattern-matching mode** (2013 Xerox incident; German BSI banned). Use lossless OR symbol-distinct mode only.
  - Output: `.jb2` byte stream.
- Background → JPEG2000 lossy compression at configurable quality (default 30:1) via OpenJPEG.
  - Output: `.j2k` byte stream.

### D4: MRC sandwich assembly into PDF/A
**Files:** `src/engines/mrc/MrcPageProcessor.cpp` (extend), `src/engines/PdfEditorEngine.cpp` (extend `exportPdfA`)
**Acceptance:**
- Three-layer PDF page:
  - JPEG2000 background as `/XObject /Subtype /Image /Filter /JPXDecode`.
  - JBIG2 foreground mask as `/XObject /Subtype /Image /Filter /JBIG2Decode /ImageMask true`.
  - Invisible OCR text via `3 Tr` text operators positioned at word-box coordinates (from M5 OCR output, NOT hOCR round-trip).
- Page content stream composes them: draw background → apply mask → draw text.
- PDF/A-2b conformance: allows JPEG2000 + JBIG2 per ISO 19005-2.
- Validation step: invoke veraPDF subprocess (from M2-PROMPT-3) to verify output passes PDF/A-2b conformance.

### D5: API + UI: exportPdfA with MRC mode
**Files:** `src/core/interfaces/IPdfEditorEngine.h`, `src/engines/PdfEditorEngine.cpp`, `src/modes/CompressDialog.cpp`
**Acceptance:**
- `enum class MrcMode { Off, Lossless, Balanced, Aggressive }`.
- `exportPdfA(outputPath, conformanceLevel, MrcMode)` signature extended.
- CompressDialog: add MRC mode selector + size estimate ("Current: 45.2 MB → Estimated MRC: 4.8 MB (89% reduction)").
- Estimate based on image count/DPI + font analysis + MRC eligibility heuristic (any image > 1 MP eligible).

### D6: DjVu importer (optional, conditional)
**Files (NEW):** `src/engines/conversion/DjvuImporter.h + .cpp` (gated `HAS_DJVU`)
**Acceptance:**
- `HAS_DJVU` CMake option (default OFF; users opt-in).
- If enabled: import DjVu → parse structure → extract text + image layers → assemble as MRC PDF/A.
- Import-only (NEVER an exporter).
- License-verify DjVuLibre before enabling default-ON.

### D7: Tests
**Files (NEW):** `tests/TestMrcPipeline.cpp`
**Acceptance:**
- Synthetic scanned PDF (image + invisible OCR text) → MRC export → size reduction verified (≥ 5× for test fixture).
- MRC output passes veraPDF conformance (PDF/A-2b).
- Sandwich text searchable: extract text from MRC output → contains OCR'd text correctly.
- DjVu import test (skipped if HAS_DJVU=OFF): legacy DjVu test fixture imports to PDF/A.

### D8: Documentation
**Files:** `CHANGELOG.md`, `ROADMAP.md`, `LICENSE-3RD-PARTY.md`, `README.md`
**Acceptance:**
- CHANGELOG: REMOVE "MRC compression inside PDF/A not yet implemented" admission. ADD: "WS3 MRC layered compression in PDF/A (modern DjVu technique without DjVu format): JBIG2 foreground + JPEG2000 background + invisible OCR sandwich text from WS1 word boxes. 5-10× size reduction for scanned content; PDF/A-2b conformant via veraPDF validation. Optional DjVu importer (HAS_DJVU=OFF default; legacy corpus ingestion only)."
- ROADMAP Session 13 (WS3): mark ✅ DONE with chosen JBIG2 encoder named.
- LICENSE-3RD-PARTY: add jbig2enc + OpenJPEG + (optionally) DjVuLibre rows.
- README architecture section: add MRC pipeline mention.

</deliverables>

<verification>
```powershell
cd build
$env:QT_QPA_PLATFORM='offscreen'
ctest -R TestMrcPipeline --output-on-failure
ctest --output-on-failure -j4
# Manual: export a scanned PDF with MRC; open in Adobe Reader DC; verify visually identical;
# verify text searchable; verify PDF/A conformance via Acrobat Preflight or veraPDF.
```
</verification>

<constraints>
- DO NOT use pattern-matching JBIG2 mode under any circumstance (2013 Xerox digit-substitution bug; German BSI banned).
- DO NOT add DjVu as an output format (excluded per ROADMAP §"Forbidden Dependencies").
- DO NOT generate MRC sandwich text from hOCR (round-trip lossy; use M5 word boxes directly).
- DO NOT use a GPL/AGPL JBIG2 encoder — verify license before integration per R6.
- DO NOT apply MRC to signed PDFs without explicit user opt-in (re-compression changes byte ranges; invalidates signatures).
- DO NOT enable HAS_DJVU by default; off-by-default with documented enablement.
</constraints>

<success_criteria>
- [ ] JBIG2 encoder license verified Apache/BSD; pinned version recorded
- [ ] MRC pipeline produces 5-10× compressed PDF/A-2b for scanned fixture
- [ ] veraPDF confirms conformance
- [ ] Sandwich text searchable in output
- [ ] CHANGELOG admission removed; LICENSE-3RD-PARTY updated
- [ ] Commit: `feat(mrc): WS3 MRC layered compression in PDF/A (JBIG2 + JPEG2000 + sandwich text)`
</success_criteria>

---

# MONTH 8 — Launch

---

## M8-PROMPT-1 — Marketing prep

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Hero screenshots
**Acceptance:** 6-10 screenshots at 1920×1080 + 4K versions: signing flow, redaction with Edact-Ray defense visible, OCR with confidence overlay, mode switching, dark theme, accessibility view.

### D2: Demo video
**Acceptance:** 60-90s video showing: open PDF → annotate → sign with cert → redact → export PDF/A. Voiceover OR captions.

### D3: Website / landing page copy
**Acceptance:** Based on §5 positioning of the audit + §10 ladder. One-liner + sub-line + 3-pillar features + proof points + contribution invite.

### D4: Press kit
**Acceptance:** PNG logos + screenshots + 1-page product summary + founder photo (optional) in `press-kit.zip`.

</deliverables>

---

## M8-PROMPT-2 — Public release governance (LICENSE + CONTRIBUTING + CODE_OF_CONDUCT + SECURITY + GitHub repo + CI)

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

This is **PROMPT-7 D5-D8 from the original audit** — the OSS release prep. Reuse that prompt section.

<deliverables>

### D1: LICENSE file
- Apache-2.0 (recommended) text from canonical source.
- SPDX header added to every src/.h and src/.cpp via script.

### D2: Governance files
- CONTRIBUTING.md (build env + style + DCO sign-off)
- CODE_OF_CONDUCT.md (Contributor Covenant 2.1)
- SECURITY.md (90-day disclosure window, in-scope/out-of-scope)
- .github/ISSUE_TEMPLATE/ (bug + feature)
- .github/PULL_REQUEST_TEMPLATE.md
- .github/FUNDING.yml (GitHub Sponsors / OpenCollective)

### D3: README badges + positioning
- Apache-2.0 + build + release + sponsor badges
- OSS positioning text from §5 of audit

### D4: GitHub repo setup
- Public repo
- Branch protection on main
- CI workflow (.github/workflows/ci.yml): MSYS2 build + ctest
- Release workflow (.github/workflows/release.yml): triggers on v* tag, builds MSI, creates Release
- Labels: bug, enhancement, good first issue, help wanted, documentation, security, v1.x-roadmap
- Pre-populate issues from v1.1+ scope

</deliverables>

<success_criteria>
- [ ] Repo is public, governance files in place
- [ ] CI green on first PR
- [ ] Commit: `chore: OSS release governance (LICENSE/CONTRIBUTING/SECURITY/.github)`
</success_criteria>

---

## M8-PROMPT-3 — Tag v1.0.0 + sign MSI + package-manager submit + announce

> 🔒 **PHASE 0 + PHASE 6 mandatory** per STANDARD EXECUTION PROTOCOL at top of file. Before any code: Read `knowledge/agent-execution-anti-patterns.md` + `projects/glyphpdf/{06-non-negotiables, 08-lessons-learned, 01-current-state, 05-prompts-index}.md` + `infrastructure/system-environment.md`. After final verification: append session-log entry + state delta.

<deliverables>

### D1: Final verification gate
- Run full ctest one more time: 14/14+ pass.
- Manual smoke: every feature in the README works.
- Build fresh MSI via packaging/build-msi.bat.

### D2: Sign MSI
- Use EV code-signing certificate (if obtained) OR standard code-signing.
- `signtool sign /a /tr http://timestamp.digicert.com /td sha256 /fd sha256 dist\GlyphPDF-1.0.0-x64.msi`

### D3: Final CHANGELOG
- Promote `[1.0.0-internal]` → `[1.0.0]` (real public version).
- Remove `[INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]` annotation.
- Add release date.
- Final pass: remove every "Known Issue" that's been closed during M2-M7.

### D4: Tag + push
```bash
git tag -a v1.0.0 -m "Release v1.0.0 — open source under Apache-2.0"
git push origin v1.0.0
```

### D5: GitHub Release
- GitHub Actions release workflow auto-creates; OR `gh release create v1.0.0 dist\GlyphPDF-1.0.0-x64.msi --notes-file CHANGELOG-v1.0.0-excerpt.md`.
- Attach SHA-256 + GPG signature alongside MSI.

### D6: Package manager submissions
- winget: `winget submit` PR to microsoft/winget-pkgs.
- chocolatey: build .nupkg + `choco push`.
- scoop: PR to ScoopInstaller/Extras OR maintain own bucket.

### D7: Announce
- HackerNews: "Show HN: GlyphPDF — open-source privacy-first PDF workstation with PAdES B-LTA + Edact-Ray-defended redaction"
- r/PDF, r/opensource, r/privacy, r/degoogle, r/privacytoolsio
- PDF Association mailing list
- Twitter/Mastodon thread with screenshots + key features
- Blog post: "Why we built GlyphPDF" — positioning rationale + technical highlights

</deliverables>

<success_criteria>
- [ ] All tests pass; MSI signed
- [ ] v1.0.0 git tag exists
- [ ] GitHub Release published
- [ ] At least 1 package manager submission live
- [ ] HackerNews + r/PDF posts live
- [ ] **REAL PUBLIC v1.0.0 SHIPPED** — Branch C SCOPE LOCK fulfilled
</success_criteria>

---

# Post-launch v2.0 roadmap (separate planning effort)

After M8, the remaining roadmap is the v2.0 work that was deliberately deferred:
- **BATCH-1: Cloud Sync** (§4 of audit) — opt-in encrypted cloud, BYOSync OSS model.
- **macOS / Linux ports** — Qt 6 cross-platform; mostly DLL bundling + MSYS2-equivalent dep packaging for the target OS (Homebrew / pacman-style for Linux).
- **Mobile companion** — separate codebase; shares WS2 Djot SemanticDocument as the cross-platform model.
- **Local-only AI** — Ollama integration in v1.0 M6; v2.0 adds bundled small models (Llama 4B-class) for fully offline summarization + Q&A.
- **VLM-based extraction** (alternative to WS1 modular pipeline) — when local-GPU VLM inference is fast enough on consumer hardware (~2027); LaneScheduler's GPU lane is already prepared for it.
- **Surya** as second layout detector (if license verification in M5-PROMPT-2 D2 deferred to v2.0).

WS1 + WS2 + WS3 are ALL part of v1.0.0 (Branch C). They are not deferred — see M2/M4/M5/M6/M7 prompts above.

These plan in a fresh v2.0-roadmap document after v1.0.0 lands.

---

*End of M2-M8 prompts document. 29 prompts. ~6-7 months of execution. Use in dependency-aware order per the SCOPE LOCK schedule. Each prompt is a fresh Claude Code session.*
