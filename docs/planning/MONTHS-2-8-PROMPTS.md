# GlyphPDF — Pending Branch C Execution Prompts

**Status (post-catchup):** HEAD `cd2e3e6` on `main` · 23 ctest targets passing · M1+M2+M3+M4(6/7+catchup) shipped · 16 prompts shipped + deleted from this file · **18 prompts remain in chronological execution order below**.

**File history:** previously contained 34 prompts (M2-M8). Catchup completed 2026-05-30 (commits `bc00e6a`→`cd2e3e6`). This rewrite deletes the 16 shipped prompts, reorders the 17 remaining + adds 1 mini-prompt (LuaDjotCodec encode), and expands every prompt to full 7-H structure.

---

## 🔒 PROTOCOL ENFORCEMENT (every prompt below)

Every prompt below inherits the **STANDARD EXECUTION PROTOCOL**. PHASE 0 (read vault) and PHASE 6 (update vault + add `Dn-MEMORY` deliverable) are **non-negotiable**.

### PHASE 0 vault reads — MANDATORY before any code

Read these 6 vault notes IN ORDER before reading any source file (saves ~5-10K tokens per session):

1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 19 cross-project failure patterns (Patterns 1-4 minimum)
2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` — GlyphPDF-specific (incl. Pattern 18 `tr`-shadow + Pattern 19 TestBatchMode race)
3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` — architectural constraints (MuPDF/Poppler/DjVu forbidden; etc.)
4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — current commit + test status (don't assume stale)
5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` — dependency-aware order + sprint status
6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` — installed tools + MSYS2 packages + filesystem (don't re-query `pacman -Q`/`where`/`pip list`)

Project `CLAUDE.md` auto-loads in CC sessions; the 6 vault notes don't. Skipping PHASE 0 = re-deriving context = ~5-10K tokens wasted.

### Standard execution protocol (inherited)

```
PHASE 0 — READ VAULT MEMORY (6 notes above)
PHASE 1 — ANALYZE: read every file in <files_to_read>; map current implementation; identify conflicts
PHASE 2 — PLAN: brief approach per deliverable; identify rework-minimizing order
PHASE 3 — IMPLEMENT + VERIFY per deliverable:
  a. Write the code
  b. Build: cmake --build build --parallel 8
  c. If build fails → fix, rebuild. Don't proceed with errors.
  d. Test: $env:QT_QPA_PLATFORM='offscreen'; cd build; ctest --output-on-failure -j4
  e. If tests fail → isolate, diagnose, fix, re-run full suite.
  f. INDEPENDENT verification per Pattern 1:
     - build/*_results.txt mtime > start-of-deliverable
     - tail -30 result file; check Totals line
     - DO NOT claim "tests pass" without these two checks
  g. Atomic commit per Pattern 4: git add <specific files> && git commit -m "feat(scope): D# description"
PHASE 4 — CONTEXT GATE: at 50% context, write STATE.md, stop. Quality > completion.
PHASE 5 — FINAL VERIFICATION: <success_criteria> checks; walkthrough.md with explicit complete/deferred/skipped lists per Pattern 10
PHASE 6 — UPDATE VAULT (MANDATORY; ENFORCED VIA Dn-MEMORY DELIVERABLE):
  - CLAUDE.md: bump HEAD + test count + Next pointer
  - SESSION_BRIEF_NEXT.md: refresh
  - vault projects/glyphpdf/07-sessions-log.md: append session entry
  - vault projects/glyphpdf/01-current-state.md: append commit row
  - vault projects/glyphpdf/05-prompts-index.md: tick checkbox
  - If new failure pattern: add Pattern 20+ to vault knowledge/agent-execution-anti-patterns.md
  - If new architectural constraint: add to vault 06-non-negotiables.md
  - Walkthrough at docs/planning/walkthroughs/<prompt-id>-walkthrough.md
```

### Environment (post-MSYS2 ucrt64 + post-catchup)

- PATH: `C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;$PATH`
- QT_PLUGIN_PATH: `C:\msys64\ucrt64\share\qt6\plugins`
- QT_QPA_PLATFORM: `offscreen`
- Build: `cmake --build build --parallel 8` (Ninja)
- Test: `$env:QT_QPA_PLATFORM='offscreen'; cd build; ctest --output-on-failure -j4 --repeat-until-fail 3`
- Branch: `main`. Current HEAD: `cd2e3e6`. Test count: 23.

---

## Index — chronological execution order

| # | Prompt | Sprint | Status | Depends on | Effort |
|---|---|---|---|---|---|
| **1** | **M5-PROMPT-3 — Office→PDF import + complete OOSS** | M5 | **READY (next)** | none | 1-2 days |
| 2 | **MINI-PROMPT — LuaDjotCodec encode stub closure** | mini | READY | none | 2-4 hr |
| 3 | M4-PROMPT-6 — Edge fixes (D4 only; D1/D2/D3 already in-place) | M4 catchup | READY (smaller than original scope) | none | 1-1.5 days |
| 4 | M5-PROMPT-1 — RapidOCR real PP-OCRv5 | M5 | **BLOCKED** — `models/ppocrv5/` has v4 weights; resolve per `models/ppocrv5/STATUS.md` | none (after model resolution) | 2-3 weeks |
| 5 | M5-PROMPT-2 — Layout detector ensemble + cross-page pipelining | M5 | needs #4 | M5-P1 + M2-P5 LaneScheduler (shipped) | 2-3 weeks |
| 6 | M5-PROMPT-4 — WS2 OCR→Djot mapping (WS2 role 1) | M5 | needs #2 + #5 | #2, #5 | 4-6 days |
| 7 | M6-PROMPT-1 — DiffEngine LCS/Myers upgrade | M6 | Independent | none | 1 week |
| 8 | M6-PROMPT-2 — ar/fr/de translations populate | M6 | Independent | none | 3-5 days eng (commission gates real completion) |
| 9 | M6-PROMPT-3 — AI backend (Anthropic + OpenAI + Gemini + Ollama) | M6 | Independent | none | 1-1.5 weeks |
| 10 | M6-PROMPT-5 — Comment threading depth (replies + /IRT) | M6 | Independent | M3-P5 InspectorWidget (shipped) | 4-6 days |
| 11 | M6-PROMPT-6 — Edge-case cleanup pass | M6 | Independent | none | 4-5 days |
| 12 | M6-PROMPT-4 — Djot annotation rich-text (WS2 role 3) | M6 | needs #2 | #2 + M3-P5 (shipped) | 1 week |
| 13 | M7-PROMPT-1 — third-party security audit prep + execution | M7 | Independent (auditor-timeline-gated) | none | 2-3 weeks wall |
| 14 | M7-PROMPT-2 — performance tuning + bug bash | M7 | Independent | none | 1-2 weeks |
| 15 | M7-PROMPT-3 — WS3 MRC compression pipeline | M7 | needs #2 + #4 + #5 + #6 | #2, #4, #5, #6 | 2-3 weeks |
| 16 | M8-PROMPT-1 — marketing prep (screenshots, demo video, copy) | M8 | After M7 | M7 done | 1 week |
| 17 | M8-PROMPT-2 — public release governance (LICENSE/CONTRIBUTING/SECURITY/GitHub repo/CI) | M8 | After M7 | M7 done | 1 week |
| 18 | M8-PROMPT-3 — tag v1.0.0 + sign MSI + package-manager submit + announce | M8 | **FINAL** | everything | 4-5 days |

**Total remaining effort: ~22-30 weeks wall time** (~5-7 months from now to M8 launch).

---

# PROMPT 1 — M5-PROMPT-3: Office→PDF import + complete OOSS

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens vs re-derivation):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 19 cross-project failure patterns
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` — GlyphPDF-specific
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` — architectural constraints
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — current commit + test status (don't assume stale)
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` — dependency-aware order
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` — installed tools + MSYS2 packages (don't re-query `pacman -Q` / `where` / `pip list`)
>
> **PHASE 6 — Vault refresh enforced via D8-MEMORY deliverable.** Bump CLAUDE.md HEAD/test-count, refresh SESSION_BRIEF_NEXT.md, append entries to vault `01-current-state.md` + `07-sessions-log.md`, tick `05-prompts-index.md` checkbox. If new failure pattern: add Pattern 20+ to `knowledge/agent-execution-anti-patterns.md` AND `projects/glyphpdf/08-lessons-learned.md`.

<session_metadata>
Phase: M5 sprint | Priority: HIGH (next recommended next; no blockers) | Sprint position: 1 of 18
Depends on: nothing (independent)
Agents: /backend (subprocess + ConversionManager extension), /testing
Estimated context: ~25% | Effort: 1-2 person-days
</session_metadata>

<role>
You are a senior C++17 + Qt + cross-process IPC engineer specializing in `QProcess` subprocess orchestration on Windows. You understand: (a) the LibreOffice headless conversion contract (`soffice --headless --convert-to pdf:writer_pdf_Export --outdir <dir> <input>`), (b) Windows process-tree termination patterns (parent kill doesn't always kill children — need `JobObject` or `taskkill /T`), (c) Qt `QProcess` timeout + cancellation patterns, and (d) format-detection via file extension + magic bytes.
</role>

<project_context>
GlyphPDF v1.0.0 supports PDF→Office conversion (Word/Excel/HTML/CSV/Text via ConversionManager) but NOT the reverse direction. PRD §9.5 + §9.17 require Office→PDF import for student/office-worker personas. CHANGELOG `Known Issues` lists "Office→PDF import not implemented (only PDF→Office conversion paths exist)" — close that admission with this prompt.

Currently `src/engines/ConversionManager.cpp:339` has dead `convertOfficeToPdf` code gated `#ifdef HAS_LIBREOFFICE` — but `HAS_LIBREOFFICE` is never defined in CMakeLists. This prompt activates the path properly with subprocess hardening + add images→PDF as bonus.
</project_context>

<current_state>
- `ConversionManager::convertOfficeToPdf` at `src/engines/ConversionManager.cpp:339-362` is `#ifdef HAS_LIBREOFFICE` gated; flag never defined.
- `IConversionEngine` interface (`src/core/interfaces/IConversionEngine.h`) has `enum class TargetFormat { Word, Excel, Html, Image, Csv, OfficeToPdf }` — `OfficeToPdf` enum value exists but no real path.
- `HomeController` / `WelcomeWidget` lack "Import Office Document" + "Images to PDF" entry points.
- No tests for office import.
- LibreOffice may or may not be installed on dev machine; need detection + graceful skip.
</current_state>

<objective>
1. Add `HAS_LIBREOFFICE` CMake detection via `find_program(LIBREOFFICE_SOFFICE soffice)`.
2. Replace dead `convertOfficeToPdf` with real subprocess invocation + timeout + process-tree kill on cancel.
3. Add `ConversionManager::convertImagesToPdf` (bonus — closes "images→PDF" PRD §9.17 gap).
4. Wire HomeController + WelcomeWidget entries.
5. Add `TestOfficeImport` (skipped if soffice not available).
6. Remove CHANGELOG admission.
7. Update vault per PHASE 6.
</objective>

<files_to_read>
src/engines/ConversionManager.h + .cpp (lines 300-400 specifically)
src/core/interfaces/IConversionEngine.h
src/shell/controllers/HomeController.cpp + .h (where to add import-office entry)
src/ui/WelcomeWidget.cpp + .h (where to add cards)
CMakeLists.txt (find_program patterns + HAS_* flag conventions)
tests/CMakeLists.txt (if separate; else inline) — for new TestOfficeImport registration
CHANGELOG.md (locate "Office→PDF import not implemented" admission)
ROADMAP.md §"Session 10 — Conversion" for context
docs/planning/walkthroughs/m4-prompt-3-walkthrough.md (Convert tools template)
</files_to_read>

<deliverables>

### D1 — CMake HAS_LIBREOFFICE detection
**Files:** `CMakeLists.txt`
**Acceptance:**
- `find_program(LIBREOFFICE_SOFFICE soffice DOC "LibreOffice headless soffice binary for Office→PDF conversion")`
- If found: `target_compile_definitions(pdfws_engines PRIVATE HAS_LIBREOFFICE=1 LIBREOFFICE_SOFFICE_PATH="${LIBREOFFICE_SOFFICE}")` + `message(STATUS "LibreOffice found at ${LIBREOFFICE_SOFFICE} — Office→PDF import enabled")`
- If not: `message(STATUS "LibreOffice not found — Office→PDF import will gracefully no-op at runtime")` (no FATAL_ERROR; optional dep)
- Document in README under Build Instructions: "Optional: install LibreOffice (`pacman -S mingw-w64-ucrt-x86_64-libreoffice-fresh` or system installer) for Office→PDF import."

**Commit:** `build: detect LibreOffice via HAS_LIBREOFFICE for Office→PDF import (M5-P3 D1)`

### D2 — Real convertOfficeToPdf subprocess
**Files:** `src/engines/ConversionManager.cpp` (replace lines 339-362)
**Acceptance:**
- Take input path + output dir + timeout (default 120s).
- Validate input is a supported format (`.docx/.xlsx/.pptx/.odt/.ods/.odp/.rtf/.txt/.csv`) via extension check + magic bytes.
- `QProcess soffice;` invoke `soffice --headless --convert-to pdf:writer_pdf_Export --outdir <outdir> <input>`.
- On timeout: `soffice.kill()` PLUS `taskkill /F /T /PID <pid>` on Windows to kill spawned-child tree (soffice often spawns `oosplash.exe` + child processes).
- Return `ConversionResult { success: bool, outputPath: QString, errorInfo: ErrorInfo }`.
- Populate `lastError()` on failure with diagnostic (timeout vs format unsupported vs soffice crashed).
- Compile-time guard: `#ifndef HAS_LIBREOFFICE` returns `{false, "", ErrorInfo::error("LibreOffice not configured at build time")}` immediately.

**Commit:** `feat(convert): real convertOfficeToPdf via LibreOffice subprocess with timeout + tree-kill (M5-P3 D2)`

### D3 — convertImagesToPdf method
**Files:** `src/core/interfaces/IConversionEngine.h` (add method), `src/engines/ConversionManager.cpp` (impl)
**Acceptance:**
- `virtual bool convertImagesToPdf(const QStringList& imagePaths, const QString& outputPath, ImageImportOptions options) = 0;`
- `struct ImageImportOptions { int dpi = 150; bool fitToPage = true; QPageSize::PageSizeId pageSize = QPageSize::A4; }`
- Uses PoDoFo `PdfImage` XObject embedding — one page per image, sized per options.
- Supports PNG, JPEG, TIFF (use Leptonica for TIFF if needed).
- Atomic + tested.

**Commit:** `feat(convert): convertImagesToPdf via PoDoFo PdfImage embedding (M5-P3 D3)`

### D4 — UI entry points
**Files:** `src/shell/controllers/HomeController.cpp + .h`, `src/ui/WelcomeWidget.cpp + .h`
**Acceptance:**
- HomeController: add `ToolId::ImportOffice` + `ToolId::ImagesToPdf` handling (add to `src/core/ToolId.h` enum if not present; rebuild ribbon).
- WelcomeWidget: add 2 action cards "Import Office Document" + "Images to PDF" alongside existing "Open PDF" / "Recent Files".
- Wire to QFileDialog → `ConversionManager::convertOfficeToPdf` / `convertImagesToPdf` calls.

**Commit:** `feat(ui): Import Office Document + Images→PDF entries in HomeController + WelcomeWidget (M5-P3 D4)`

### D5 — Tests
**Files (NEW):** `tests/TestOfficeImport.cpp` + register in `tests/CMakeLists.txt`
**Acceptance:**
- Test 1: convertImagesToPdf with 3 PNG inputs → 3-page PDF, page count matches, each page has 1 image XObject.
- Test 2: convertOfficeToPdf with a known `.docx` fixture → PDF output exists, opens with PoDoFo, has expected page count. **QSKIP if `HAS_LIBREOFFICE` undefined or soffice not on PATH at runtime.**
- Test 3: timeout test — convertOfficeToPdf with `--timeout 100ms` on a real .docx → returns false with timeout ErrorInfo, no zombie soffice process.
- Headless via `QT_QPA_PLATFORM=offscreen`.
- Register following the pattern catchup D3 established (TestPatternRedact, TestDjotRoundtrip, TestInspector all properly registered now — same pattern).

**Commit:** `test(convert): TestOfficeImport covering soffice subprocess + images→PDF + timeout (M5-P3 D5)`

### D6 — CHANGELOG closure
**Files:** `CHANGELOG.md`
**Acceptance:**
- Locate "Office→PDF import not implemented" admission (likely in Known Issues section).
- Delete it.
- Add under `[Unreleased]` Conversion section: "M5-PROMPT-3: Office→PDF import via LibreOffice subprocess (HAS_LIBREOFFICE CMake flag); images→PDF via PoDoFo PdfImage embedding; HomeController + WelcomeWidget entry points."

**Commit:** `docs(CHANGELOG): close Office→PDF import admission (M5-P3 D6)`

### D7 — Walkthrough
**Files (NEW):** `docs/planning/walkthroughs/m5-prompt-3-walkthrough.md`
**Acceptance:**
- Match format of `walkthroughs/m4-prompt-3-walkthrough.md` (Convert tools template).
- Sections: Overview / D1-D6 status / Deferred (if any — e.g., specific soffice flags for redactions etc.) / Outstanding work (none if D1-D6 closed) / Commits / Lessons captured.

**Commit:** `docs: M5-PROMPT-3 walkthrough (Pattern 11)`

### D8-MEMORY — Vault + memory layer refresh (PHASE 6 enforcement)
**Files:** `CLAUDE.md`, `SESSION_BRIEF_NEXT.md`, vault `01-current-state.md`, `07-sessions-log.md`, `05-prompts-index.md`, `00-overview.md`
**Acceptance:**
- `CLAUDE.md`: bump HEAD → current after D1-D7 commits; test count 23 → 24 (TestOfficeImport added); "Next prompt" → PROMPT 2 (LuaDjotCodec encode mini-prompt).
- `SESSION_BRIEF_NEXT.md`: refresh sprint table with M5-P3 row; "Next" pointer → PROMPT 2; bump `Updated:` frontmatter date.
- vault `01-current-state.md`: append commit-table rows for M5-P3 (D1-D7 commits).
- vault `07-sessions-log.md`: append session entry (date, prompt, deliverables, commits).
- vault `05-prompts-index.md`: tick checkbox for prompt #1; update status column.
- vault `00-overview.md`: bump `last_updated:` + refresh "Current state" section.
- If anything new emerged worth a Pattern 20+: add to `knowledge/agent-execution-anti-patterns.md` AND `projects/glyphpdf/08-lessons-learned.md`.
- If new architectural constraint: add to `06-non-negotiables.md`.

**Commit:** `docs(memory): M5-P3 vault + CLAUDE.md + SESSION_BRIEF refresh (D8-MEMORY)`

</deliverables>

<verification>
```powershell
cd C:\Users\User\Projects\pdf
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
cmake --build build --parallel 8 2>&1 | Select-Object -Last 15
cd build
ctest --output-on-failure -j4 --repeat-until-fail 3 2>&1 | Select-Object -Last 30
cd ..
git log --oneline -10
grep -in "Office.*PDF.*not implemented" CHANGELOG.md  # expect 0
where soffice  # informational
```
</verification>

<constraints>
- DO NOT make `HAS_LIBREOFFICE` a hard build requirement. Optional dep with graceful runtime-skip.
- DO NOT bundle the LibreOffice binary. Detect installed version.
- DO NOT use single `process.kill()` on Windows — soffice spawns child processes; use `taskkill /F /T /PID <pid>` for tree kill.
- DO NOT block GUI thread — `QProcess` is async; UI feedback via QFutureWatcher.
- DO NOT skip D8-MEMORY. Catchup audit showed shipping without PHASE 6 is the #1 systematic failure mode.
- DO NOT add new ToolId entries without rebuilding the ribbon to include them.
- DO NOT use cmd's `start /WAIT` — Qt's QProcess handles process lifetimes correctly.
</constraints>

<error_recovery>
- If `soffice --convert-to` exits with code 81 (lock file): a prior soffice instance is still running; kill it via `taskkill /F /IM soffice.bin /IM soffice.exe`.
- If output PDF is empty / corrupt: the input may have password protection or macros LibreOffice rejected; document in errorInfo + return false.
- If TIFF support fails: Leptonica's libtiff may need explicit `pacman -S mingw-w64-ucrt-x86_64-libtiff`; already in MSYS2 deps but verify.
- If TestOfficeImport hangs in CI: timeout must be reduced (~5s for tests); ensure tests don't depend on real .docx fixtures larger than necessary.
- If you discover NEW gaps beyond D1-D8: do NOT close them in this prompt. Document in `docs/planning/M5-P3-FOLLOWUP.md` for a follow-up session.
</error_recovery>

<success_criteria>
- [ ] D1: `HAS_LIBREOFFICE` CMake flag + detection
- [ ] D2: `convertOfficeToPdf` real subprocess with timeout + tree-kill
- [ ] D3: `convertImagesToPdf` via PoDoFo
- [ ] D4: HomeController + WelcomeWidget entries
- [ ] D5: TestOfficeImport registered + passing (or QSKIPed cleanly without LibreOffice)
- [ ] D6: CHANGELOG admission removed
- [ ] D7: Walkthrough at `docs/planning/walkthroughs/m5-prompt-3-walkthrough.md`
- [ ] D8-MEMORY: CLAUDE.md + SESSION_BRIEF + vault notes refreshed; commit message includes "D8-MEMORY"
- [ ] 24/24 ctest pass `--repeat-until-fail 3`
- [ ] 8 atomic commits (D1-D8) in git log
</success_criteria>

---

# PROMPT 2 — MINI: LuaDjotCodec encode stub closure

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D4-MEMORY deliverable below: refresh CLAUDE.md + SESSION_BRIEF + vault `{01-current-state, 07-sessions-log, 05-prompts-index, 00-overview}.md`.

<session_metadata>
Phase: M4-P7 follow-up (WS2 encode path) | Priority: HIGH (unblocks M5-P4, M6-P4, M7-P3) | Sprint position: 2 of 18
Depends on: nothing
Agents: /backend (C++ + Lua FFI), /testing
Estimated context: ~15% | Effort: 2-4 hours
</session_metadata>

<role>
You are a C++ engineer with Lua FFI experience implementing AST→Djot syntax serialization. You understand Djot's grammar (CommonMark-derived, backtrack-free), the SemanticDocument tree shape produced by `docmodel`, and the vendored Lua reference Djot parser's encode-side conventions.
</role>

<project_context>
M4-PROMPT-7 (Djot foundation) shipped `docmodel` + `pdfws_djot` + vendored Lua 5.4 + Djot reference parser. The DECODE path (Djot text → SemanticDocument via Lua AST) works. The ENCODE path (SemanticDocument → Djot text) is stubbed:

```cpp
// src/pdfws_djot/LuaDjotCodec.cpp:75
QString LuaDjotCodec::documentToDjot(const SemanticDocument& doc) {
    // TODO: implement once docmodel → djot serialisation is written
    return QString();
}
```

This stub blocks the round-trip property test (TestDjotRoundtrip can decode but not re-encode, so the 1000-doc round-trip is currently checking decode-only). It also blocks M5-P4 (OCR→Djot mapping needs encode), M6-P4 (annotation rich-text needs encode for `/PieceInfo` Djot sidecar), and M7-P3 (MRC sandwich text uses Djot intermediate).
</project_context>

<current_state>
- `src/pdfws_djot/LuaDjotCodec.cpp:75` — single TODO in `src/` per deep-dive audit
- `src/docmodel/SemanticDocument.h` — tree shape: `Document → Section → Block (Paragraph/Table/Figure/List/Header/Footer/CodeBlock/RawBlock) → Inline (Span/Image/Link/Code/Math)`
- `tests/TestDjotRoundtrip.cpp` — registered + passing decode-side; would fail round-trip equality if encode worked but produced different output. Currently passes because encode returns empty + decode of empty = empty doc (trivially equal).
- Djot reference parser at `third_party/djot/djot.lua` has both decode (`djot.parse`) and encode (`djot.render`) sides — the encode side can be invoked via Lua FFI same as decode is currently.
</current_state>

<objective>
Implement `LuaDjotCodec::documentToDjot` by either: (a) marshalling SemanticDocument → Lua table → invoking `djot.render` from the vendored parser, OR (b) writing direct C++ emitter that walks SemanticDocument and produces Djot syntax. Then expand TestDjotRoundtrip to actually verify round-trip equality on 1000 random docs.
</objective>

<files_to_read>
src/pdfws_djot/LuaDjotCodec.h + .cpp (current decode impl; mirror its FFI pattern for encode)
src/docmodel/SemanticDocument.h
src/docmodel/Block.h + Inline.h (tree node types to walk)
third_party/djot/djot.lua (reference parser; check `djot.render` API signature)
tests/TestDjotRoundtrip.cpp (extend round-trip section)
docs/planning/walkthroughs/m4-prompt-7-walkthrough.md (foundation context)
</files_to_read>

<deliverables>

### D1 — Implement documentToDjot
**Files:** `src/pdfws_djot/LuaDjotCodec.cpp` (replace TODO at line 75)
**Acceptance:**
- **Recommended approach:** marshall `SemanticDocument` → Lua table matching what `djot.parse` produces → invoke `djot.render(ast)` via FFI → return resulting Djot string.
- Marshal function: `static int pushDocumentToLua(lua_State* L, const SemanticDocument& doc)` walks the tree, pushes Lua tables matching Djot AST schema (block types: `para`, `table`, `figure`, `list`, `heading`, etc.; inline types: `str`, `emph`, `strong`, `code`, `link`, `image`, `math`).
- Handle PDF fidelity attributes (`{pdf-page=N pdf-bbox=... pdf-font=... ocr-conf=...}`) — render as Djot attribute syntax.
- Handle raw blocks (`{=pdf}...{=pdf}`) — emit literal content from sidecar store.
- Handle preservation of source positions if available (for round-trip stability).
- Remove the TODO comment.

**Commit:** `feat(djot): LuaDjotCodec::documentToDjot encode via vendored Lua djot.render (M4-P7 D1 follow-up)`

### D2 — Expand TestDjotRoundtrip to real round-trip
**Files:** `tests/TestDjotRoundtrip.cpp`
**Acceptance:**
- Existing 1000-doc DocumentFuzzer-generated test: now actually verifies `decode(encode(doc)) == doc` for structural equality (NOT naive string equality).
- Acceptable equality: AST node types match, attribute maps match (modulo ordering), text content matches. Whitespace + ordering variations OK.
- Add a "spec round-trip" test: take canonical Djot input strings → decode → encode → re-decode → assert equal AST.
- Add edge-case tests: empty doc, doc with only raw blocks, doc with deeply nested lists, doc with pipe tables, doc with all PDF fidelity attributes.

**Commit:** `test(djot): TestDjotRoundtrip now verifies real round-trip on 1000 fuzz + canonical samples (M4-P7 D5 follow-up)`

### D3 — Document encode design
**Files (NEW):** `docs/djot-encode-design.md`
**Acceptance:**
- 1-page explanation: which approach chosen (Lua FFI vs C++ emitter), why, performance characteristics, known limitations (e.g., "Djot attribute ordering not deterministic — round-trip equality uses set comparison").
- Cross-link from `docs/planning/walkthroughs/m4-prompt-7-walkthrough.md` (append follow-up section).

**Commit:** `docs: Djot encode design + M4-P7 walkthrough follow-up (encode path closure)`

### D4-MEMORY — Vault refresh (PHASE 6)
**Files:** `CLAUDE.md`, `SESSION_BRIEF_NEXT.md`, vault `01-current-state.md` + `07-sessions-log.md` + `05-prompts-index.md` + `00-overview.md`
**Acceptance:**
- CLAUDE.md: bump HEAD; "Next prompt" → PROMPT 3 (M4-P6).
- SESSION_BRIEF: remove "WS2 Djot encode path (blocking M5-P4)" from PENDING section.
- Vault: log this mini-prompt as a session entry; tick prompt #2 in index.

**Commit:** `docs(memory): LuaDjotCodec encode closure + vault refresh (D4-MEMORY)`

</deliverables>

<verification>
```powershell
cmake --build build --parallel 8 2>&1 | Select-Object -Last 10
cd build
ctest -R TestDjotRoundtrip --output-on-failure --repeat-until-fail 3
ctest --output-on-failure -j4 --repeat-until-fail 3 2>&1 | Select-Object -Last 20
grep -rn "TODO" ../src/pdfws_djot/  # expect 0
```
</verification>

<constraints>
- DO NOT add new TODO/FIXME comments — close this one; don't replace it.
- DO NOT implement a second Djot encoder from scratch when vendored Lua reference parser already has `djot.render`.
- DO NOT change SemanticDocument shape — encoder must work with what M4-P7 D1 defined.
- DO NOT skip the spec-round-trip test in D2 — fuzz-only testing has false-pass risk.
</constraints>

<error_recovery>
- If `djot.render` from the vendored parser is incomplete: fall back to C++ emitter approach in D1; document in D3.
- If round-trip fails on some fuzz cases: investigate the specific failing cases; may indicate a SemanticDocument shape that has no canonical Djot representation (e.g., empty paragraphs); document in D3 + fuzzer's "exclude" set.
- If Lua FFI marshalling is slow (>1s per 1000 docs): profile + optimize; consider C++ emitter.
</error_recovery>

<success_criteria>
- [ ] `LuaDjotCodec.cpp` has 0 TODO/FIXME comments
- [ ] TestDjotRoundtrip verifies real round-trip equality (not vacuous)
- [ ] 1000-doc fuzz + canonical samples pass
- [ ] 23+/23+ ctest pass `--repeat-until-fail 3`
- [ ] D4-MEMORY: vault + CLAUDE.md + SESSION_BRIEF refreshed
- [ ] M5-P4, M6-P4, M7-P3 unblocked (verify by checking their Depends-on lists no longer mention encode stub)
</success_criteria>

---

# PROMPT 3 — M4-PROMPT-6: Edge fixes (D4 only — D1/D2/D3 already in-place)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D3-MEMORY deliverable below.

<session_metadata>
Phase: M4 catchup completion | Priority: MEDIUM | Sprint position: 3 of 18
Depends on: nothing
Agents: /frontend, /backend
Estimated context: ~20% | Effort: 1-1.5 days
**Reduced scope vs original prompt:** D1 (Strikeout/Squiggly real ToolModes) + D2 (Share via MAPI) already verified in-place per catchup; D3 (click-to-place form fields) collided with M3-P1 D2 (already shipped there). **Only D4 (prune missing recent files) is genuinely unimplemented.**
</session_metadata>

<role>
You are a Qt UI engineer maintaining a desktop file-recents system. You understand QSettings persistence, `QFile::exists()` validation, and graceful UX for missing files (vs hiding silently).
</role>

<project_context>
WelcomeWidget + Recent Files menu show files the user previously opened. When a file is moved/deleted/disk-unmounted, the entry still shows but click does nothing useful. Original M4-P6 D4 spec: "WelcomeWidget:256-269 dims missing files but keeps them. On click of missing file, prompt to remove from recents."

Verification confirms D1+D2 from original M4-P6 ARE present in current source (Strikeout/Squiggly ToolModes added during M3 wave; Share via MAPI in HomeController). D3 (click-to-place) collided with M3-P1 D2 work which shipped first — keep there, remove from this prompt.
</project_context>

<current_state>
- `src/ui/WelcomeWidget.cpp:256-269` — Recent Files grid; missing files dimmed but click is no-op (or worse, error popup with no recovery).
- HomeController has `loadRecentFiles()` reading `QSettings("recent/files")` — list grows unbounded until manual clear.
- No "prune all missing" entry in File menu.
</current_state>

<objective>
Add: (a) on-click prompt when user clicks a missing recent file ("File no longer exists at X. Remove from recents?"), (b) "Prune Missing" entry in File → Recent Files submenu, (c) automatic prune-on-startup option in PreferencesDialog (default OFF).
</objective>

<files_to_read>
src/ui/WelcomeWidget.cpp + .h (lines 240-280 — recent-files grid)
src/shell/controllers/HomeController.cpp (loadRecentFiles + Recent Files menu construction)
src/ui/PreferencesDialog.cpp + .h (where to add auto-prune toggle)
src/shell/MenuBar.cpp (File → Recent submenu)
</files_to_read>

<deliverables>

### D1 — Click-prompt on missing file
**Files:** `src/ui/WelcomeWidget.cpp`
**Acceptance:**
- On click of dimmed (missing) recent entry: show modal `QMessageBox::question("File not found", "<path>\nThis file no longer exists. Remove from recents?", Yes|No)`.
- Yes → call `HomeController::removeFromRecents(path)`.
- No → leave entry.
- Wired via existing click signal; check `QFileInfo::exists()` before opening flow.

**Commit:** `feat(welcome): prompt-to-prune missing recent files on click (M4-P6 D4 partial)`

### D2 — "Prune Missing" menu entry
**Files:** `src/shell/MenuBar.cpp`, `src/shell/controllers/HomeController.cpp` (add `pruneMissingRecents()` method)
**Acceptance:**
- File → Recent Files → "Prune Missing" entry (visible always; greyed if none missing).
- Click → iterate recents, `QFileInfo::exists()` each, remove all missing; show toast "Removed N missing entries."
- Persists to QSettings.

**Commit:** `feat(menu): File → Recent → Prune Missing entry (M4-P6 D4 menu)`

### D3-MEMORY — Vault + memory refresh + PreferencesDialog auto-prune toggle
**Files:** `src/ui/PreferencesDialog.cpp + .h`, `CLAUDE.md`, `SESSION_BRIEF_NEXT.md`, vault notes (per pattern)
**Acceptance:**
- PreferencesDialog → General tab: add `QCheckBox` "Auto-prune missing recent files on startup" (default unchecked). Persists to `QSettings("recent/autoPrune")`.
- HomeController constructor (or post-load): if setting true, call `pruneMissingRecents()` silently.
- CLAUDE.md / SESSION_BRIEF / vault notes updated per PHASE 6.
- Walkthrough at `docs/planning/walkthroughs/m4-prompt-6-walkthrough.md` (NEW) — explicitly note that D1/D2/D3 of original prompt were already shipped; this prompt only added D4 work split into 3 deliverables.

**Commit:** `feat(prefs): auto-prune setting + D3-MEMORY vault refresh (M4-P6 D4 final + completion)`

</deliverables>

<verification>
```powershell
cmake --build build --parallel 8
cd build
ctest --output-on-failure -j4 --repeat-until-fail 3
# Manual smoke: create recent entry pointing to non-existent file, click → prompt appears.
grep -in "pruneMissingRecents" ../src
```
</verification>

<constraints>
- DO NOT silently delete recent entries without user confirmation (D1) OR explicit menu/setting action (D2/D3).
- DO NOT add D1/D2/D3 from original prompt — already shipped or moved.
- DO NOT touch HomeController paths beyond `recents/` namespace in QSettings.
- DO NOT skip the walkthrough — closes Pattern 11 for M4-P6.
</constraints>

<error_recovery>
- If QSettings read returns malformed list (corrupted ini): catch + reset to empty; log warning.
- If user has 10000+ recents (pathological): cap recents list at 100 via existing convention.
</error_recovery>

<success_criteria>
- [ ] D1: click on missing file prompts, removes on Yes
- [ ] D2: "Prune Missing" menu entry works
- [ ] D3-MEMORY: PreferencesDialog auto-prune option + vault refresh
- [ ] Walkthrough written
- [ ] M4-P6 fully closed; CHANGELOG admission (if any) removed
- [ ] 23+/23+ ctest pass
</success_criteria>

---

# PROMPT 4 — M5-PROMPT-1: RapidOcrEngine real PP-OCRv5 implementation

> 🔒 **PRE-EXECUTION BLOCKER:** read `models/ppocrv5/STATUS.md` first; current files are PP-OCRv4 weights. Decide: download real PP-OCRv5 ONNX models OR amend this prompt to use PP-OCRv4. Don't start until resolved.
>
> **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D9-MEMORY deliverable below.

<session_metadata>
Phase: M5 (WS1 OCR ensemble) | Priority: CRITICAL (closes RapidOCR Mock CHANGELOG admission) | Sprint position: 4 of 18
Depends on: PP-OCRv5 model weight availability (currently BLOCKED)
Agents: /backend (heavy ML inference), /performance
Estimated context: ~45% | Effort: 2-3 person-weeks
</session_metadata>

<role>
You are a senior C++ ML inference engineer with ONNX Runtime expertise. You understand PaddleOCR PP-OCRv5 architecture: DBNet text detection → perspective transform per detection → cls (orientation classification) → SVTR text recognition → CTC decoding. You know the PaddleOCR vocabulary file format and the input tensor shapes per model.
</role>

<project_context>
RapidOcrEngine at `src/engines/ocr/RapidOcrEngine.cpp:112-127` is a hardcoded STUB returning `OcrResult{ text: "RapidOCR_Mock", confidence: 85 }`. M1-PROMPT-7 added runtime-gating via `isMockImplementation()` so the selector is disabled in OCRMode UI — this prompt removes that gate by making the implementation real.

3 ONNX Sessions correctly constructed at lines 53-91 (det, cls, rec) and warm-kept (compliant with anti-spawn-per-page rule from ROADMAP). Just need real pre/post processing.
</project_context>

<current_state>
- STUB at `src/engines/ocr/RapidOcrEngine.cpp:112-127` returning Mock.
- Models in `models/ppocrv5/` are mis-labeled — actually PP-OCRv4 per audit. STATUS.md documents this.
- `isMockImplementation()` returns true; OCRMode UI gates selector.
- TestRapidOcr would need to be added (none exists yet).
</current_state>

<objective>
Implement full PP-OCRv5 (or v4) inference pipeline. Remove STUB. Make `isMockImplementation()` return false. Verify ROVER ensemble with Tesseract actually outperforms Tesseract-alone on test images.
</objective>

<files_to_read>
src/engines/ocr/RapidOcrEngine.h + .cpp
src/engines/ocr/OcrPipeline.cpp (ROVER merge logic; consumer of RapidOcrEngine)
src/modes/OCRMode.cpp (runtime gate added in M1 PROMPT-7)
models/ppocrv5/STATUS.md (read FIRST — confirm v4 vs v5 decision)
models/ppocrv5/ — list .onnx files actually present
</files_to_read>

<deliverables>

### D1 — Resolve model file question (PRE-WORK; NOT engineering)
**Files:** `models/ppocrv5/STATUS.md` (update with decision)
**Acceptance:**
- Read STATUS.md. Choose:
  - (a) Download real PP-OCRv5 ONNX from PaddleOCR repo / RapidOcrOnnx releases, replace files in `models/ppocrv5/`. Document download source + SHA-256.
  - (b) Rename directory to `models/ppocrv4/` + amend this prompt + RapidOcrEngine to use v4 (acceptable; v4 is still SOTA-class for many use cases).
- Decision recorded in STATUS.md + commit.

**Commit:** `vendor: PP-OCRv5 (or v4) ONNX model weights + provenance (M5-P1 D1 pre-work)`

### D2 — DBNet detection decoding
**Files (NEW):** `src/engines/ocr/PpOcrDecoder.h + .cpp`. **Modify:** `src/engines/ocr/RapidOcrEngine.cpp`
**Acceptance:**
- Take DBNet session output tensor (1, 1, H, W) probability map.
- Threshold (default 0.3); morphological dilate (3×3 kernel).
- Extract polygons via connected components; minimum-area rectangle per polygon.
- Return `QList<QPolygonF>` of detection boxes.
- Unit-tested with synthetic prob map.

**Commit:** `feat(ocr): PpOcrDecoder DBNet polygon extraction (M5-P1 D2)`

### D3 — Perspective transform per detection
**Files:** `src/engines/ocr/PpOcrDecoder.cpp` (extend)
**Acceptance:**
- Given QPolygonF + source QImage → 3×3 homography matrix → bilinear-sample warp to standard rect (32 × 320 px for SVTR input).
- No OpenCV dep — implement minimal homography solver + bilinear sample (~80 lines).
- Tested with axis-aligned rect (homography = identity) and rotated rect (verifiable rotation result).

**Commit:** `feat(ocr): perspective transform for SVTR rec input (M5-P1 D3)`

### D4 — Cls (orientation) + SVTR (recognition) + CTC decode
**Files:** `src/engines/ocr/PpOcrDecoder.cpp` (extend), `src/engines/ocr/RapidOcrEngine.cpp` (orchestrate)
**Acceptance:**
- Cls session per crop: 4-class output (0°/90°/180°/270°); rotate crop accordingly.
- Rec session: forward perspective-warped crop; CTC decode using PP-OCRv5 (or v4) vocabulary file (`models/ppocrv5/ppocr_keys.txt`).
- Return decoded text + per-char confidence (mean of softmax).
- Bundle vocabulary file (verify Apache-2.0 license per ROADMAP license matrix).

**Commit:** `feat(ocr): cls orientation + SVTR recognition + CTC decode (M5-P1 D4)`

### D5 — Wire pipeline into processImage; remove STUB
**Files:** `src/engines/ocr/RapidOcrEngine.cpp` (replace stub block 112-127)
**Acceptance:**
- Replace stub; full pipeline: preprocess → det → cls → perspective → rec → results.
- Return `QList<OcrResult>` with text + bbox + confidence per detection.
- Make `isMockImplementation()` return false.

**Commit:** `feat(ocr): RapidOcrEngine real inference pipeline; remove STUB (M5-P1 D5)`

### D6 — Tests
**Files (NEW):** `tests/TestRapidOcr.cpp` + register in tests CMake
**Acceptance:**
- Recognize printed English from `tests/fixtures/ocr/printed_english.png` → confidence > 80%, text ≥ 90% match expected.
- Recognize CJK from `tests/fixtures/ocr/cjk_test.png` → text correctly decoded.
- ROVER merge: combined Tesseract + RapidOCR accuracy ≥ either alone on test set.
- Need to commit test fixtures (small images).

**Commit:** `test(ocr): TestRapidOcr real inference + ROVER merge ensemble verification (M5-P1 D6)`

### D7 — UI auto-enable + CHANGELOG closure
**Files:** `src/modes/OCRMode.cpp` (verify runtime gate auto-disables since isMockImplementation now false), `CHANGELOG.md`
**Acceptance:**
- Verify M1 PROMPT-7's runtime gate code at `src/modes/OCRMode.cpp:65-78` now enables RapidOCR + ROVER selector items (since `isMockImplementation()` now returns false).
- Remove `[Unreleased]` admission "RapidOCR PP-OCRv5 tensor pre/post processing is STUB" — replace with new entry under Features.

**Commit:** `feat(ocr): UI re-enables RapidOCR + ROVER + CHANGELOG closure (M5-P1 D7)`

### D8 — Walkthrough
**Files (NEW):** `docs/planning/walkthroughs/m5-prompt-1-walkthrough.md`
**Commit:** `docs: M5-PROMPT-1 walkthrough (Pattern 11)`

### D9-MEMORY — Vault refresh (PHASE 6)
Same pattern as PROMPT-1 D8-MEMORY. Bump CLAUDE.md HEAD + test count + Next pointer (→ PROMPT 5, M5-PROMPT-2).

**Commit:** `docs(memory): M5-P1 vault refresh (D9-MEMORY)`

</deliverables>

<verification>
```powershell
cmake --build build --parallel 8 2>&1 | Select-Object -Last 15
cd build
ctest -R TestRapidOcr --output-on-failure
ctest --output-on-failure -j4 --repeat-until-fail 3
# Manual: open scanned PDF in OCRMode, select RapidOCR engine, verify real text out
```
</verification>

<constraints>
- DO NOT use PaddlePaddle runtime. ONNX Runtime only.
- DO NOT spawn ONNX process per page. Persistent warm worker (already done; verify M2-P5 LaneScheduler is the orchestrator).
- DO NOT use char-level voting. Word-level ROVER only (already in OcrPipeline).
- DO NOT make RapidOCR required. Tesseract-alone must still work if RapidOCR fails to load models.
- DO NOT skip D1. Running this prompt without resolving model question = guaranteed Pattern 5 (Mock masquerading as real).
- CONTEXT GATE: 2-3 weeks of work. Expect ≥2 sessions with STATE.md handoffs.
</constraints>

<error_recovery>
- If ONNX Runtime can't load model: check ONNX Runtime version vs model opset version; may need model export from PaddleOCR at compatible opset.
- If CTC decoding produces garbage: vocabulary file mismatch; verify keys file matches the rec model's vocabulary.
- If ROVER merge doesn't improve over Tesseract alone: investigate confidence-weighting threshold; may need tuning.
</error_recovery>

<success_criteria>
- [ ] D1: model question resolved + recorded
- [ ] D2-D5: real inference pipeline (no STUB)
- [ ] D6: real recognition + ROVER tests pass
- [ ] D7: UI auto-enables; CHANGELOG admission removed
- [ ] D8: walkthrough
- [ ] D9-MEMORY: vault refresh
- [ ] `isMockImplementation()` returns false
- [ ] 24+/24+ ctest pass
</success_criteria>

---

# PROMPT 5 — M5-PROMPT-2: Layout detector ensemble + cross-page pipelining (WS1 completion)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D7-MEMORY deliverable below.

<session_metadata>
Phase: M5 (WS1 OCR ensemble completion) | Priority: HIGH | Sprint position: 5 of 18
Depends on: M5-P1 (real RapidOCR) + M2-P5 LaneScheduler (shipped)
Agents: /backend (ML pipeline), /performance
Estimated context: ~40% | Effort: 2-3 person-weeks
</session_metadata>

<role>
You are an ML pipeline engineer specializing in document-layout detection + multi-detector ensembling. You understand PP-DocLayoutV2 / PP-StructureV3, Surya layout transformer, and IoU reconciliation when detectors disagree.
</role>

<project_context>
Per ROADMAP WS1, OCR pipeline upgrades from "OCR per page" to "layout-first, region-fanout, ROVER-merged". M5-P1 provides real RapidOCR. M2-P5 LaneScheduler provides GPU warm-worker + CPU pool + cross-page pipelining helper. This prompt adds the layout-detection pass that feeds per-region OCR.
</project_context>

<current_state>
- LaneScheduler shipped (M2-P5).
- RapidOcrEngine real (after M5-P1).
- OcrPipeline at `src/engines/ocr/OcrPipeline.cpp` has ROVER merge but no layout-aware fanout.
- No ILayoutDetector interface, no PP-DocLayout impl, no Surya impl, no LayoutEnsemble.
</current_state>

<objective>
Build ILayoutDetector + PP-DocLayoutV2 + (conditional) Surya detector + LayoutEnsemble with IoU reconciliation + integrate into OcrPipeline with cross-page pipelining via LaneScheduler.
</objective>

<files_to_read>
src/engines/ocr/OcrPipeline.h + .cpp
src/engines/ocr/RapidOcrEngine.h + .cpp (post-M5-P1)
src/engines/scheduling/LaneScheduler.h + .cpp (M2-P5)
src/engines/scheduling/PipelineStage.h (cross-page helper)
src/modes/OCRMode.cpp (UI integration)
ROADMAP.md WS1 section
</files_to_read>

<deliverables>

### D1 — ILayoutDetector interface + LayoutRegion type
**Files (NEW):** `src/engines/ocr/ILayoutDetector.h`
**Acceptance:** `enum class RegionType { Title, Paragraph, Table, Figure, List, Header, Footer, Equation, Reference, Caption, Other }`; `struct LayoutRegion { QRectF bbox; RegionType type; int readingOrderIndex; double confidence; }`; pure-virtual `detect(QImage page, Lane lane = Lane::GPU) -> QList<LayoutRegion>`.
**Commit:** `feat(ocr): ILayoutDetector interface + LayoutRegion type (M5-P2 D1)`

### D2 — PpDocLayoutDetector
**Files (NEW):** `src/engines/ocr/PpDocLayoutDetector.h + .cpp`. Bundle `models/pp_doclayout_v2.onnx` (verify Apache-2.0 license).
**Acceptance:** Loads ONNX model; runs detection via `AppContext::scheduler->submit(Lane::GPU, ...)`; returns LayoutRegions.
**Commit:** `feat(ocr): PpDocLayoutDetector PP-DocLayoutV2 impl (M5-P2 D2)`

### D3 — SuryaDetector (license-conditional)
**Files (NEW):** `src/engines/ocr/SuryaDetector.h + .cpp` OR documented stub.
**Acceptance:** Verify Surya license at https://github.com/VikParuchuri/surya. If GPL-3.0 → subprocess approach (acceptable per veraPDF pattern). If GPL blocking + subprocess too slow → ship as TODO stub with explanation; LayoutEnsemble falls back to single-detector mode.
**Commit:** `feat(ocr): SuryaDetector (license-resolved approach) (M5-P2 D3)`

### D4 — LayoutEnsemble with IoU reconciliation
**Files (NEW):** `src/engines/ocr/LayoutEnsemble.h + .cpp`
**Acceptance:** Constructor takes 1+ ILayoutDetector*; runs each in parallel via LaneScheduler; IoU > 0.5 merges regions; type votes with confidence tiebreak; reading-order re-computed from centroids; single-detector mode supported.
**Commit:** `feat(ocr): LayoutEnsemble + IoU reconciliation (M5-P2 D4)`

### D5 — OcrPipeline cross-page pipelining
**Files:** `src/engines/ocr/OcrPipeline.cpp`
**Acceptance:** New `recognizeDocument(QString pdfPath) -> QFuture<QList<PageOcrResult>>`. Uses LaneScheduler::CrossPagePipeline (M2-P5): stage1 (GPU LayoutEnsemble) ‖ stage2 (mixed-lane per-region OCR fanout via existing ROVER merge) ‖ stage3 (CPU fusion). Results in page order via OrderedResultQueue.
**Commit:** `feat(ocr): OcrPipeline cross-page pipelining via LaneScheduler (M5-P2 D5)`

### D6 — OCRMode UI: per-word confidence + per-region redo
**Files:** `src/modes/OCRMode.cpp`
**Acceptance:** Per-word confidence overlay (green ≥90 / yellow 70-89 / red <70). Right-click region → "Re-OCR this region" → submits just that region. "Review before save" workflow per-region accept/reject.
**Commit:** `feat(ocr): per-word confidence overlay + per-region redo in OCRMode (M5-P2 D6)`

### D7-MEMORY — Vault + tests + CHANGELOG
**Files:** Tests + walkthrough + CHANGELOG + vault refresh (per Dn-MEMORY pattern)
**Acceptance:**
- `tests/TestLayoutEnsemble.cpp` + register: synthetic 2-page doc, both detectors agree → single merged map; one returns extra region with lower confidence → suppressed in merge.
- `tests/TestOcrPipeline.cpp` extend for cross-page pipelining timing.
- CHANGELOG: REMOVE "OCR ensemble pipeline (PP-DocLayoutV2, Surya, LaneScheduler) not yet implemented" admission.
- Walkthrough at `walkthroughs/m5-prompt-2-walkthrough.md`.
- Vault refresh per standard pattern.

**Commit:** `test+docs(ocr): LayoutEnsemble + OcrPipeline tests + M5-P2 walkthrough + vault refresh (D7-MEMORY)`

</deliverables>

<verification>
```powershell
cmake --build build --parallel 8
cd build
ctest -R "TestLayoutEnsemble|TestOcrPipeline|TestRapidOcr" --output-on-failure
ctest --output-on-failure -j4 --repeat-until-fail 3
```
</verification>

<constraints>
- DO NOT spawn per-page ONNX. Use LaneScheduler warm worker.
- DO NOT char-level vote. Word-level ROVER.
- DO NOT make Surya a hard dep. Single-detector mode must work.
- CONTEXT GATE: 2-3 weeks. Expect STATE.md handoffs.
</constraints>

<error_recovery>
- Cross-page pipelining slower than serial: profile; may indicate LaneScheduler tuning needed.
- IoU reconciliation merges too aggressively: lower IoU threshold (0.5 → 0.3).
</error_recovery>

<success_criteria>
- [ ] D1-D6 deliverables shipped
- [ ] CHANGELOG ensemble admission removed
- [ ] D7-MEMORY: walkthrough + vault refresh
- [ ] Cross-page pipelining verified by timing test (total < serial sum)
- [ ] 25+/25+ ctest pass
</success_criteria>

---

# PROMPT 6 — M5-PROMPT-4: WS2 OCR→Djot mapping (WS2 role 1)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M5 (WS2 integration) | Priority: HIGH | Sprint position: 6 of 18
Depends on: PROMPT 2 (LuaDjotCodec encode) + M5-P1 + M5-P2 (real OCR ensemble output)
Agents: /backend (interchange-format engineer)
Estimated context: ~20% | Effort: 4-6 days
</session_metadata>

<role>
You are an interchange-format engineer mapping fused OCR output (layout regions + per-word fused text + bboxes + confidence) into the `docmodel::SemanticDocument` tree with PDF fidelity attributes.
</role>

<project_context>
After M5-P2 + PROMPT-2, OCR pipeline produces layout regions + fused word results, AND `LuaDjotCodec::documentToDjot` works. This prompt produces the bridge: `OcrDjotMapper` that turns OCR output into SemanticDocument with `{pdf-page pdf-bbox pdf-font ocr-conf}` attributes.
</project_context>

<current_state>
- M5-P2 produces `QList<PageOcrResult>` with fused words + layout regions.
- `docmodel::SemanticDocument` exists (M4-P7 D1) with full encode/decode (after PROMPT-2).
- No mapper between them.
</current_state>

<objective>
Implement OcrDjotMapper; wire into OcrPipeline; update OCRMode "Review before save" UI to render via SemanticDocument; close WS2 role 1.
</objective>

<files_to_read>
src/engines/ocr/OcrPipeline.cpp (M5-P2 output shape)
src/docmodel/SemanticDocument.h + Block.h + Inline.h
src/pdfws_djot/LuaDjotCodec.cpp (post-PROMPT-2)
src/modes/OCRMode.cpp
</files_to_read>

<deliverables>

### D1 — OcrDjotMapper class
**Files (NEW):** `src/engines/ocr/OcrDjotMapper.h + .cpp`
**Acceptance:**
- `class OcrDjotMapper { SemanticDocument fromOcrResults(const QList<PageOcrResult>&); }`
- Reading-order index → block order in SemanticDocument.
- RegionType→Djot container: Title→Heading level 1; Paragraph→Paragraph; Table→Table with pipe-table cells; Figure→Figure block; List→List; Header/Footer→marked with `page-header`/`page-footer` attributes; etc.
- Per-word fused result → Inline span with attributes `{pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}`.
- Tables: cell layout from LayoutEnsemble's table-region structure → Djot pipe-table syntax; cell text from per-cell OCR.
- ProvenanceTag: every node `BornOCR` with source PDF path + page index + bbox.

**Commit:** `feat(ocr): OcrDjotMapper — fused OCR → SemanticDocument (M5-P4 D1)`

### D2 — Integration into OcrPipeline
**Files:** `src/engines/ocr/OcrPipeline.cpp`
**Acceptance:**
- After fusion stage (M5-P2 stage 3), emit `SemanticDocument` via OcrDjotMapper.
- Existing `PageOcrResult` retained (back-compat).
- New API: `OcrPipeline::recognizeDocumentAsDjot(pdfPath) -> QFuture<SemanticDocument>`.

**Commit:** `feat(ocr): OcrPipeline::recognizeDocumentAsDjot integration (M5-P4 D2)`

### D3 — OCRMode review UI uses Djot
**Files:** `src/modes/OCRMode.cpp`
**Acceptance:**
- "Review before save" renders SemanticDocument via docmodel → HTML preview (simple stylesheet).
- Edit-in-place: Djot-aware editor (same pattern as M6-PROMPT-4 annotation editor will use).
- Per-region accept/reject preserved.

**Commit:** `feat(ocr): OCRMode Djot-aware review UI (M5-P4 D3)`

### D4 — Tests
**Files (NEW):** `tests/TestOcrDjotMapper.cpp` + register
**Acceptance:**
- Mock OcrResults with known regions + words → SemanticDocument structurally matches expected tree.
- Roundtrip via PROMPT-2 codec: encode SemanticDocument → Djot → decode → equivalent tree.
- Table region maps to pipe table; cell text round-trips.

**Commit:** `test(ocr): TestOcrDjotMapper structural + roundtrip (M5-P4 D4)`

### D5-MEMORY — Vault + CHANGELOG + walkthrough
**Files:** CHANGELOG + walkthrough + vault per standard pattern
**Acceptance:**
- CHANGELOG: "OCR output maps to SemanticDocument via OcrDjotMapper (WS2 role 1) — layout regions → block structure; per-word fused text → Inline spans with `{pdf-page pdf-bbox pdf-font ocr-conf}` attributes; tables → Djot pipe tables. Feeds OCRMode review UI + M7-P3 MRC sandwich text layer."
- Walkthrough at `walkthroughs/m5-prompt-4-walkthrough.md`.
- Vault refresh.

**Commit:** `docs(memory): M5-P4 OCR→Djot + vault refresh (D5-MEMORY)`

</deliverables>

<verification>
```powershell
cmake --build build --parallel 8
cd build
ctest -R "TestOcrDjotMapper|TestDjotRoundtrip" --output-on-failure
ctest --output-on-failure -j4 --repeat-until-fail 3
```
</verification>

<constraints>
- DO NOT bypass LaneScheduler for OcrDjotMapper (CPU-bound mapping work; use Lane::CPU).
- DO NOT lose PDF fidelity attributes during mapping — they're how M7-P3 MRC sandwich text aligns later.
- DO NOT introduce hidden state in OcrDjotMapper — must be stateless / pure-function.
</constraints>

<success_criteria>
- [ ] OcrDjotMapper produces valid SemanticDocument
- [ ] Roundtrip preserves structure + attributes
- [ ] OCRMode review UI uses Djot
- [ ] WS2 role 1 complete
- [ ] D5-MEMORY vault refreshed
- [ ] 26+/26+ ctest pass
</success_criteria>

---

# PROMPT 7 — M6-PROMPT-1: DiffEngine LCS/Myers upgrade

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M6 polish | Priority: MEDIUM (closes "DiffEngine uses set-difference" admission) | Sprint position: 7 of 18
Depends on: nothing
Agents: /backend (algorithms)
Estimated context: ~25% | Effort: 1 week
</session_metadata>

<role>
Diff algorithms engineer with Myers 1986 LCS knowledge + move-detection post-pass design.
</role>

<project_context>
DiffEngine at `src/engines/DiffEngine.cpp:48-69` uses `QSet<QString>` per-word set-difference — moved paragraphs appear as add+delete pairs, not moves. Legal/compliance persona (PRD §9.10) wants move-detection for redlines of reordered contracts.
</project_context>

<current_state>
- `src/engines/DiffEngine.cpp:48-69` set-difference impl.
- `DiffResult` struct lacks MoveOperation variant.
- CompareWidget renders only add (green) / delete (red).
</current_state>

<objective>
Replace set-difference with Myers LCS; add move-detection post-pass; CompareWidget renders moves in third color (orange).
</objective>

<files_to_read>
src/engines/DiffEngine.h + .cpp
src/ui/CompareWidget.cpp
tests/TestDiffEngine.cpp (if exists; else create)
</files_to_read>

<deliverables>

### D1 — MyersDiff implementation
**Files (NEW):** `src/engines/MyersDiff.h + .cpp`
**Acceptance:** Pure C++ Myers 1986 LCS on token sequences; edit script (insert/delete/keep) with positions.
**Commit:** `feat(diff): MyersDiff LCS implementation (M6-P1 D1)`

### D2 — Move detection post-pass
**Files:** `src/engines/MyersDiff.cpp`
**Acceptance:** Post-pass: identify delete+insert pairs of identical token sequences → reclassify as moves with source + target positions.
**Commit:** `feat(diff): Move-detection post-pass (M6-P1 D2)`

### D3 — DiffEngine replacement
**Files:** `src/engines/DiffEngine.cpp:48-69`
**Acceptance:** Drop QSet-difference; route through MyersDiff. DiffResult gains MoveOperation variant.
**Commit:** `feat(diff): DiffEngine routed through MyersDiff with moves (M6-P1 D3)`

### D4 — CompareWidget renders moves
**Files:** `src/ui/CompareWidget.cpp`
**Acceptance:** Moves rendered in third color (orange) distinct from add (green) / delete (red). "Next move" / "Prev move" nav.
**Commit:** `feat(ui): CompareWidget renders moves in third color (M6-P1 D4)`

### D5-MEMORY — Tests + CHANGELOG + walkthrough + vault
**Files:** Tests + CHANGELOG + walkthroughs + vault
**Acceptance:**
- Test: legal-document pair with paragraph reordering → moves detected (not add+delete).
- CHANGELOG: REMOVE "DiffEngine uses per-word set-difference rather than LCS/Myers" admission.
- Walkthrough.
- Vault refresh.

**Commit:** `test+docs(diff): Myers LCS + move tests + walkthrough + vault (M6-P1 D5-MEMORY)`

</deliverables>

<verification>Standard build + ctest --repeat-until-fail 3.</verification>

<constraints>
- DO NOT use exponential-worst-case naive LCS. Myers is O((N+M)D).
- DO NOT change DiffResult public API beyond adding MoveOperation variant.
</constraints>

<success_criteria>
- [ ] Moves correctly detected on legal-doc test
- [ ] CHANGELOG admission removed
- [ ] D5-MEMORY vault refreshed
- [ ] All tests pass
</success_criteria>

---

# PROMPT 8 — M6-PROMPT-2: ar/fr/de translations populate

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M6 polish | Priority: MEDIUM | Sprint position: 8 of 18
Depends on: nothing (engineering; commission gates real completion)
Agents: /devops (lupdate workflow), /docs (translator commissioning)
Estimated context: ~15% | Effort: 3-5 days engineering + N weeks for translators
</session_metadata>

<role>
Qt i18n engineer + translation coordinator.
</role>

<project_context>
ar/fr/de translation `.ts` files in `translations/` are 6-line empty XML shells. RTL flag flip works for Arabic (main.cpp); zero strings translated. CHANGELOG admits this.
</project_context>

<deliverables>

### D1 — lupdate populate
**Files:** `translations/glyphpdf_{ar,fr,de}.ts`
**Acceptance:** Run `lupdate src/ -ts translations/glyphpdf_ar.ts translations/glyphpdf_fr.ts translations/glyphpdf_de.ts`. All `tr()` strings extracted with `<source>...<translation type="unfinished">` placeholders.
**Commit:** `i18n: lupdate populate ar/fr/de translation stubs (M6-P2 D1)`

### D2 — Translator commissioning placeholder
**Files (NEW):** `translations/README.md`
**Acceptance:** Document commissioning status per language: vendor contact, dates, accepted vs pending. Out-of-scope for Claude session — leave for human follow-up.
**Commit:** `docs(i18n): translator commissioning README (M6-P2 D2)`

### D3 — lrelease + QRC bundle
**Files:** `CMakeLists.txt`, `resources.qrc`
**Acceptance:** `qt_add_translations()` compiles .ts → .qm; embed via QRC.
**Commit:** `build(i18n): lrelease + QRC bundle for ar/fr/de (M6-P2 D3)`

### D4 — RTL per-widget audit for Arabic
**Files:** All UI widgets
**Acceptance:** Test in Arabic locale (force via QSettings); flag widgets with broken RTL (mirrored buttons, text alignment, scroll direction). Fix or document. Most Qt auto-RTL handles; custom widgets may need explicit `setLayoutDirection(Qt::RightToLeft)`.
**Commit:** `i18n: RTL per-widget audit + fixes for Arabic (M6-P2 D4)`

### D5-MEMORY — PreferencesDialog re-enable + CHANGELOG + walkthrough + vault
**Files:** `src/ui/PreferencesDialog.cpp:46-58`, CHANGELOG, walkthrough, vault
**Acceptance:**
- PreferencesDialog language combo lists ar/fr/de as functional (un-disable per M1 wave).
- CHANGELOG: REMOVE "Translation .ts files for ar/fr/de are empty shells" admission.
- Walkthrough.
- Vault refresh.

**Commit:** `feat+docs(i18n): re-enable ar/fr/de in Preferences + vault refresh (M6-P2 D5-MEMORY)`

</deliverables>

<verification>Standard + manual: launch in Arabic locale, verify RTL renders correctly.</verification>

<constraints>
- DO NOT translate strings inside Claude session. Engineering only; translators are humans.
- DO NOT skip RTL audit. Arabic is a fidelity-critical persona.
</constraints>

<success_criteria>
- [ ] .ts files populated with all tr() strings
- [ ] .qm files build + embed
- [ ] RTL works for Arabic
- [ ] D5-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 9 — M6-PROMPT-3: AI backend wiring (Anthropic + OpenAI + Gemini + Ollama)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D6-MEMORY deliverable below.

<session_metadata>
Phase: M6 polish | Priority: MEDIUM (closes AIChatPanel canned-reply gap) | Sprint position: 9 of 18
Depends on: nothing
Agents: /backend (LLM API integration), /security (key handling)
Estimated context: ~30% | Effort: 1-1.5 weeks
</session_metadata>

<role>
LLM API integration engineer with QNetworkAccessManager + streaming-token experience across multiple providers.
</role>

<project_context>
AIChatPanel `src/modes/AIChatPanel.cpp:71-79` returns canned reply `"AI: (v1.1) AI responses will appear here once real LLM calls are wired."` for every Send. CredentialManager.cpp:60 already supports key storage (M1).
</project_context>

<deliverables>

### D1 — IAiProvider interface
**Files (NEW):** `src/engines/ai/IAiProvider.h`
**Acceptance:** `class IAiProvider { virtual QFuture<QString> chat(QList<Message>, AiOptions) = 0; virtual bool isReady() = 0; virtual QString providerName() = 0; }`. Streaming via QFuture::progressUpdated.
**Commit:** `feat(ai): IAiProvider interface (M6-P3 D1)`

### D2 — Provider impls (Anthropic + OpenAI + Gemini + Ollama)
**Files (NEW):** `src/engines/ai/{AnthropicProvider,OpenAiProvider,GeminiProvider,OllamaProvider}.cpp`
**Acceptance:**
- Anthropic Claude API (default; uses CredentialManager `GlyphPDF.AI.Anthropic`).
- OpenAI ChatGPT API.
- Google Gemini API.
- Ollama local HTTP (default endpoint `http://localhost:11434`).
- Each handles auth + streaming format independently. Stream via QFuture progressUpdated signal.

**Commit:** `feat(ai): 4 provider impls (Anthropic/OpenAI/Gemini/Ollama) (M6-P3 D2)`

### D3 — AIChatPanel real wiring
**Files:** `src/modes/AIChatPanel.cpp:71-79`
**Acceptance:**
- Read selected provider from QSettings.
- Compose context: current page text (PDFium extract) + chat history.
- Stream response into message list with typing-cursor.
- Citations: detect page references ("page 5") → clickable jumps.

**Commit:** `feat(ai): AIChatPanel real backend wiring with streaming + citations (M6-P3 D3)`

### D4 — PreferencesDialog provider selection
**Files:** `src/ui/PreferencesDialog.cpp:140-142`, `:256-273`
**Acceptance:** Un-disable OpenAI/Gemini/Ollama entries (M1 had them gated). Test-key button does real round-trip (1-token ping), not format-only check.
**Commit:** `feat(ai): PreferencesDialog provider selection + real test-key (M6-P3 D4)`

### D5 — Tests
**Files (NEW):** `tests/TestAiProvider.cpp` + register
**Acceptance:** Mock provider for CI. Real round-trip test gated behind env vars (ANTHROPIC_API_KEY etc.); QSKIP if not set.
**Commit:** `test(ai): TestAiProvider with mock + env-gated real round-trip (M6-P3 D5)`

### D6-MEMORY — CHANGELOG + walkthrough + vault
**Files:** CHANGELOG + walkthrough + vault
**Acceptance:**
- CHANGELOG: REMOVE AIChatPanel canned-reply admission. Add Features entry.
- Walkthrough.
- Vault refresh.

**Commit:** `docs(ai): M6-P3 walkthrough + vault refresh (D6-MEMORY)`

</deliverables>

<verification>
```
ctest -R TestAiProvider --output-on-failure
# Manual: open AI Chat panel, ask "summarize page 1", verify real response
```
</verification>

<constraints>
- DO NOT hardcode API keys.
- DO NOT log responses (privacy positioning).
- DO NOT block GUI thread on API calls.
- DO NOT enable provider selectors without real impl backing them.
</constraints>

<success_criteria>
- [ ] AI Chat works end-to-end with at least Anthropic
- [ ] Streaming + citations
- [ ] D6-MEMORY vault refreshed
- [ ] CHANGELOG admission removed
</success_criteria>

---

# PROMPT 10 — M6-PROMPT-5: Comment threading depth (replies + /IRT)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D4-MEMORY deliverable below.

<session_metadata>
Phase: M6 polish | Priority: MEDIUM | Sprint position: 10 of 18
Depends on: M3-P5 InspectorWidget (shipped)
Agents: /backend (annotation), /frontend (CommentsWidget)
Estimated context: ~20% | Effort: 4-6 days
</session_metadata>

<role>
PDF annotation engineer + Qt widget engineer.
</role>

<project_context>
AnnotationItem already has `replies` list + ReviewState enum (M3-P5 model prep). Not surfaced in UI. PDF /IRT (In Reply To) ISO 32000 §12.5.6.4 not written on save.
</project_context>

<deliverables>

### D1 — Threaded view in CommentsWidget
**Files:** `src/ui/CommentsWidget.cpp`
**Acceptance:** Hierarchical display with indent per reply depth. Filter by status/author/date.
**Commit:** `feat(comments): threaded view with depth indent + filters (M6-P5 D1)`

### D2 — /IRT linking on save
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (embedAnnotations)
**Acceptance:** When AnnotationItem has parentId, write `/IRT <parent annot ref>` to annotation dict per spec.
**Commit:** `feat(annot): /IRT in-reply-to linking on save (M6-P5 D2)`

### D3 — changeReviewState wired
**Files:** `src/ui/CommentsWidget.cpp:364-367` (empty stub)
**Acceptance:** Real impl updates AnnotationItem.reviewState + saves via EditAnnotationCommand. Context menu shows Open/Accepted/Rejected/Cancelled/Completed.
**Commit:** `feat(comments): changeReviewState context menu wired (M6-P5 D3)`

### D4-MEMORY — Tests + CHANGELOG + walkthrough + vault
**Files:** Tests + CHANGELOG + walkthrough + vault
**Acceptance:**
- Test: replies threaded + persist across save/reload.
- Test: review states roundtrip.
- CHANGELOG entry.
- Walkthrough.
- Vault refresh.

**Commit:** `test+docs(comments): threading + IRT + reviewState + walkthrough + vault (M6-P5 D4-MEMORY)`

</deliverables>

<verification>Standard.</verification>

<constraints>
- DO NOT change AnnotationItem persistence format beyond adding /IRT.
- DO NOT use blocking modal for review-state change.
</constraints>

<success_criteria>
- [ ] Replies threaded + persist
- [ ] /IRT written correctly per ISO 32000
- [ ] Review states roundtrip
- [ ] D4-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 11 — M6-PROMPT-6: Edge-case cleanup pass

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D7-MEMORY deliverable below.

<session_metadata>
Phase: M6 polish | Priority: MEDIUM | Sprint position: 11 of 18
Depends on: nothing
Agents: /frontend (multiple panels)
Estimated context: ~25% | Effort: 4-5 days
</session_metadata>

<role>
Generalist Qt engineer fixing decorative-panel-to-real-data conversions identified in the original audit.
</role>

<deliverables>

### D1 — SignaturesPanel real data
**Files:** `src/modes/SignaturesPanel.cpp:43-89`
**Acceptance:** Replace hardcoded "Elie Matta / GlobalSign CA" with real `ISignatureManager::validateSignatures` output. Place Signature button wires to ribbon Sign flow.
**Commit:** `fix(ui): SignaturesPanel real signature data (M6-P6 D1)`

### D2 — ThumbnailSidebar real thumbnails
**Files:** `src/ui/ThumbnailSidebar.cpp:229-269`
**Acceptance:** Replace `// Fake content blocks on paper` placeholder widgets with real PDFium-rendered thumbnails (cached via RenderCache at 75 DPI).
**Commit:** `fix(ui): ThumbnailSidebar real PDFium-rendered thumbnails (M6-P6 D2)`

### D3 — Sidebar Files attachment extract real bytes
**Files:** `src/shell/Sidebar.cpp:100-121`
**Acceptance:** Replace placeholder text-stub with real PoDoFo embedded-file byte extraction.
**Commit:** `fix(ui): Sidebar attachment extract real bytes (M6-P6 D3)`

### D4 — StatusBar OCR/UTF-8 cells
**Files:** `src/shell/StatusBar.cpp:67,69,187,193`
**Acceptance:** OCR language cell shows real selected language from QSettings. Encoding cell removed (PDFs aren't UTF-8; meaningless).
**Commit:** `fix(ui): StatusBar OCR-real + remove meaningless encoding cell (M6-P6 D4)`

### D5 — BatesNumber + Resize legal-vertical polish
**Files:** BatesNumberingDialog (M4 catchup wired)
**Acceptance:** Custom presets + range selection UI for Bates.
**Commit:** `feat(bates): legal-vertical polish — presets + range UI (M6-P6 D5)`

### D6 — PdfAValidationPanel real data verification
**Files:** `src/modes/PdfAValidationPanel.cpp`
**Acceptance:** Verify catchup's veraPDF wiring exposes real data (no remaining hardcoded violations).
**Commit:** `fix(ui): PdfAValidationPanel real-data verification (M6-P6 D6)`

### D7-MEMORY — Walkthrough + vault refresh
**Files:** Walkthrough + vault
**Commit:** `docs(memory): M6-P6 edge-case cleanup walkthrough + vault refresh (D7-MEMORY)`

</deliverables>

<verification>Standard + manual smoke on each panel.</verification>

<constraints>
- DO NOT change panel layouts — only data sources.
- DO NOT leave Pattern 5 (Mock masquerading as real) anywhere after this prompt.
</constraints>

<success_criteria>
- [ ] All previously-decorative panels show real data
- [ ] D7-MEMORY vault refreshed
- [ ] Pattern 5 grep returns 0 unintentional Mock UI surfaces
</success_criteria>

---

# PROMPT 12 — M6-PROMPT-4: Djot annotation rich-text model (WS2 role 3)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D7-MEMORY deliverable below.

<session_metadata>
Phase: M6 (WS2 role 3) | Priority: HIGH (third WS2 role) | Sprint position: 12 of 18
Depends on: PROMPT 2 (LuaDjotCodec encode) + M3-P5 InspectorWidget (shipped)
Agents: /backend (annotation serialization), /frontend (InspectorWidget editor)
Estimated context: ~25% | Effort: 1 week
</session_metadata>

<role>
PDF + annotation engineer who understands PDF's required rich-text formats: `/RC` (restricted XHTML for Acrobat/Foxit interop) + `/Contents` (plain text fallback). Storing arbitrary Djot in `/RC` violates spec; `/PieceInfo` sidecar preserves perfect GlyphPDF round-trip while interop-ing with other readers.
</role>

<project_context>
WS2 foundation (M4-P7) + encode (PROMPT-2) + M3-P5 InspectorWidget Properties bound contents. This makes Djot the internal authoring model for annotation rich text with transcoding on save.
</project_context>

<deliverables>

### D1 — AnnotationItem gains djotSource field
**Files:** `src/core/AnnotationTypes.h`
**Acceptance:** Add `QString djotSource` field; existing `text` stays as plain-text fallback. Serializer roundtrip preserves djotSource.
**Commit:** `feat(annot): djotSource field on AnnotationItem (M6-P4 D1)`

### D2 — InspectorWidget Djot editor
**Files:** `src/ui/InspectorWidget.cpp` (Contents editor in Properties tab)
**Acceptance:** Replace plain QTextEdit with Djot-aware editor: live render-preview pane via docmodel → simple HTML. Toolbar: bold/italic/code/link/list/heading. Edits write djotSource; auto-derive `annotation.text` as plain-text projection.
**Commit:** `feat(ui): InspectorWidget Djot-aware editor with live preview (M6-P4 D2)`

### D3 — Save-time transcoding (/Contents + /RC + /PieceInfo)
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (embedAnnotations)
**Acceptance:**
- `/Contents` ← plain-text projection of djotSource (spec-compliant).
- `/RC` ← XHTML-subset transcoding via `pdfws_djot::DjotToRichTextXhtml` (NEW helper) per ISO 32000 §12.5.6.4.
- `/PieceInfo /GlyphPDF /DjotSource` ← original Djot source, escaped per PdfStringEscape (M1 PROMPT-3 pattern).

**Commit:** `feat(annot): save-time dual-write /Contents + /RC + /PieceInfo Djot (M6-P4 D3)`

### D4 — Load-time round-trip
**Files:** `src/engines/podofo/PoDoFoBackend.cpp` (annotation loading)
**Acceptance:** If `/PieceInfo /GlyphPDF /DjotSource` present, restore djotSource. Else fall back to deriving from /Contents (trivial djotSource). Perfect GlyphPDF→PDF→GlyphPDF roundtrip preserves formatting.
**Commit:** `feat(annot): load-time djotSource restore from /PieceInfo (M6-P4 D4)`

### D5 — Tests
**Files (NEW):** `tests/TestAnnotationDjot.cpp` + register
**Acceptance:**
- Author annotation with `*bold* and _italic_ and \`code\`` → save → reopen → djotSource matches.
- Plain-text-only annotation (no sidecar) → loads with djotSource = plain text.

**Commit:** `test(annot): TestAnnotationDjot roundtrip + /RC interop (M6-P4 D5)`

### D6 — Comment threading depth integration (consolidates with PROMPT-10)
**Files:** `src/ui/CommentsWidget.cpp` (already shipped in PROMPT-10)
**Acceptance:** Verify PROMPT-10's threading uses djotSource for reply rich-text. Same /RC + /PieceInfo dual-write.

**Commit:** `feat(comments): replies use djotSource rich-text (M6-P4 D6)`

### D7-MEMORY — Walkthrough + CHANGELOG + vault
**Files:** CHANGELOG + walkthrough + vault
**Acceptance:**
- CHANGELOG: "Annotation + comment rich text uses Djot as internal authoring model; transcodes to PDF /RC XHTML + /Contents plain text + /PieceInfo Djot sidecar for perfect GlyphPDF roundtrip + Acrobat/Foxit interop (WS2 role 3 complete)."
- Walkthrough.
- Vault refresh.

**Commit:** `docs(memory): M6-P4 Djot annotation + vault refresh (D7-MEMORY)`

</deliverables>

<verification>Standard + manual: open annotated PDF in Adobe Reader, verify /RC renders.</verification>

<constraints>
- DO NOT skip /Contents plain-text fallback. Acrobat/Foxit need it.
- DO NOT skip /PieceInfo sidecar. Perfect GlyphPDF roundtrip needs it.
- DO NOT skip the PROMPT-2 dependency (encode stub closure). Roundtrip can't work without encode.
</constraints>

<success_criteria>
- [ ] Djot editor in InspectorWidget + CommentsWidget
- [ ] Save/load roundtrip preserves formatting
- [ ] /RC interoperates with Adobe Reader
- [ ] D7-MEMORY vault refreshed
- [ ] WS2 role 3 complete; all 3 WS2 roles closed
</success_criteria>

---

# PROMPT 13 — M7-PROMPT-1: Third-party security audit prep + execution

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M7 hardening | Priority: HIGH (legitimizes public security claims) | Sprint position: 13 of 18
Depends on: nothing engineering-wise (auditor timeline-gated)
Agents: /security (coordination)
Estimated context: ~15% | Effort: 2-3 weeks wall (mostly waiting for auditor)
</session_metadata>

<role>
Security coordination engineer — shortlist auditors, prepare kit, triage findings.
</role>

<deliverables>

### D1 — Shortlist auditors + quotes
**Files (NEW):** `docs/security/audit-vendor-shortlist.md`
**Acceptance:** 2-3 firms (Trail of Bits / NCC Group / Cure53 / PDF Association partners). Quotes. ~$15-40K typical 1-2 wk PDF tool audit.
**Commit:** `docs(security): audit vendor shortlist + quotes (M7-P1 D1)`

### D2 — Auditor kit
**Files (NEW):** `docs/security/auditor-kit/{threat-model.md,security-claims.md,scope.md,test-fixtures-list.md,build-instructions.md}`
**Acceptance:**
- Threat model doc.
- Security claims (PAdES B-LTA, Edact-Ray defense, sanitization vectors, content-stream injection escape).
- Test fixtures.
- Build instructions.
- Repo access details (read-only commit signing key share OR public OSS repo).

**Commit:** `docs(security): auditor onboarding kit (M7-P1 D2)`

### D3 — Track audit findings (live during audit)
**Files (NEW):** GitHub Issues with severity labels
**Acceptance:** As findings come in: triage CRITICAL/HIGH/MEDIUM/LOW. Fix CRITICAL + HIGH before launch. MEDIUM may slip to v1.1.
**Commit:** Per-fix commits as findings come in.

### D4 — Audit-letter for marketing
**Files (NEW):** `docs/security/audit-letter-summary.md`
**Acceptance:** Auditor's letter summarizing scope + findings + remediation. Published on website + linked from README security section.
**Commit:** `docs(security): audit-letter summary (M7-P1 D4)`

### D5-MEMORY — Vault refresh
**Commit:** `docs(memory): M7-P1 audit prep + execution + vault refresh (D5-MEMORY)`

</deliverables>

<verification>Auditor sign-off letter.</verification>

<constraints>
- DO NOT claim audit findings publicly until auditor letter received.
- DO NOT skip CRITICAL findings; fix before launch.
</constraints>

<success_criteria>
- [ ] Audit complete; letter on file
- [ ] CRITICAL findings fixed
- [ ] Public security disclosure page drafted
- [ ] D5-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 14 — M7-PROMPT-2: Performance tuning + bug bash

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M7 hardening | Priority: HIGH | Sprint position: 14 of 18
Depends on: nothing
Agents: /performance, /testing
Estimated context: ~30% | Effort: 1-2 weeks
</session_metadata>

<role>
Performance engineer + bug-bash coordinator.
</role>

<deliverables>

### D1 — Performance baselines + regression suite
**Files:** Extend `tests/TestPerformance.cpp`
**Acceptance:** Measure doc open <3s for 100p, search <1s, OCR throughput, RenderCache hit rate. Regression-locked.
**Commit:** `test(perf): performance baselines + regression suite (M7-P2 D1)`

### D2 — Profile hot paths
**Files:** Profiling reports in `docs/performance/`
**Acceptance:** Tracy or Easy_Profiler; top-5 hot functions identified + optimized.
**Commit:** `perf: hot-path optimizations from profiling (M7-P2 D2)`

### D3 — Bug bash
**Files:** GitHub Issues
**Acceptance:** Internal/beta-tester multi-day bash. Fix release-blockers.
**Commit:** Per-fix.

### D4 — Long-running stability
**Files:** Stress test report
**Acceptance:** 48h soak test (open + edit + save loop). No memory leaks (Heaptrack). Fix leaks.
**Commit:** `fix(memory): leaks found in 48h soak (M7-P2 D4)`

### D5-MEMORY — Walkthrough + vault
**Commit:** `docs(memory): M7-P2 perf + bug bash + vault refresh (D5-MEMORY)`

</deliverables>

<verification>Performance targets met; no leaks.</verification>

<constraints>
- DO NOT optimize without profiling. Premature optimization is Pattern 7-adjacent.
</constraints>

<success_criteria>
- [ ] All performance targets met
- [ ] No memory leaks after 48h
- [ ] Critical bugs from beta closed
- [ ] D5-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 15 — M7-PROMPT-3: WS3 MRC compression pipeline ("Modern DjVu" in PDF/A)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D9-MEMORY deliverable below.

<session_metadata>
Phase: M7 (WS3) | Priority: HIGH (closes WS3 + scan-archival use case) | Sprint position: 15 of 18
Depends on: PROMPT 2 (LuaDjotCodec encode) + PROMPT 4 (M5-P1 OCR real) + PROMPT 5 (M5-P2 LayoutEnsemble) + PROMPT 6 (M5-P4 OCR→Djot mapping)
Agents: /backend (image compression), /security (JBIG2 license verify), /performance
Estimated context: ~45% | Effort: 2-3 person-weeks
</session_metadata>

<role>
Senior image-compression engineer specializing in MRC layered compression for scanned-document PDFs. You understand: JBIG2 as standardized descendant of DjVu's JB2 for bitonal; JPEG2000 (OpenJPEG, BSD-2) for continuous-tone; invisible OCR sandwich text aligned to word boxes for searchability; 2013 Xerox JBIG2 pattern-matching incident (digit substitution; German BSI banned); DjVu OUTPUT excluded (legacy) but technique INSIDE PDF/A is modern answer.
</role>

<project_context>
Per ROADMAP §"Phase 6 — MRC Compression Pipeline (WS3)" + Risk Register R6 (JBIG2 encoder license must be non-GPL/AGPL). MRC achieves 5-10× compression for scanned content; PDF/A archival standard; veraPDF validation gate.
</project_context>

<current_state>
- Standard compression exists (`IPdfEditorEngine::optimizeDocument`).
- LayoutEnsemble + OCR (M5) produce regions + word boxes (after PROMPTs 4+5).
- OcrDjotMapper (after PROMPT 6) produces SemanticDocument.
- No JBIG2 encoder, no JPEG2000 encoder, no MRC pipeline in src/.
- R6: must verify JBIG2 encoder license before integration.
</current_state>

<deliverables>

### D1 — License verification + encoder selection
**Files (NEW):** `docs/MRC-ENCODER-LICENSE-AUDIT.md`
**Acceptance:**
- Audit JBIG2: `jbig2enc` (Apache-2.0 fork; verify), `libjbig2` (BSD), Adobe (reject).
- Audit OpenJPEG 2.x (BSD-2 confirmed).
- MSYS2 ucrt64 package availability check.
- Update `LICENSE-3RD-PARTY.md`.
- **STOP if no permissive JBIG2 encoder.** Defer MRC to v2.0; report to user.

**Commit:** `docs(mrc): JBIG2 + JPEG2000 license audit (M7-P3 D1 pre-work)`

### D2 — MRC layer separation
**Files (NEW):** `src/engines/mrc/MrcPageProcessor.h + .cpp`
**Acceptance:** `separatePage(QImage page, QList<LayoutRegion> regions, QList<OcrWord> words) -> MrcLayers { foregroundMask, background, sandwichText }`. Layout regions guide separation: text → foreground mask; figure/photo → background; mixed → finer-grained threshold.
**Commit:** `feat(mrc): MrcPageProcessor layer separation (M7-P3 D2)`

### D3 — JBIG2 + JPEG2000 encoding
**Files:** `src/engines/mrc/MrcPageProcessor.cpp` (extend)
**Acceptance:**
- Foreground → JBIG2 (lossless OR symbol-distinct mode; **NEVER pattern-matching**).
- Background → JPEG2000 lossy at configurable quality (default 30:1) via OpenJPEG.

**Commit:** `feat(mrc): JBIG2 lossless + JPEG2000 background encoding (M7-P3 D3)`

### D4 — MRC sandwich assembly into PDF/A
**Files:** `src/engines/PdfEditorEngine.cpp` (extend exportPdfA)
**Acceptance:**
- 3-layer page: JPEG2000 background XObject + JBIG2 mask XObject (`/ImageMask true`) + invisible `3 Tr` text from M5 word boxes (NOT hOCR roundtrip).
- Content stream composes: draw background → apply mask → draw text.
- PDF/A-2b conformance (ISO 19005-2 allows JPX + JB2).
- veraPDF subprocess (M2-P3) validates output.

**Commit:** `feat(mrc): MRC sandwich PDF/A-2b assembly + veraPDF gate (M7-P3 D4)`

### D5 — API + CompressDialog MRC mode
**Files:** `src/core/interfaces/IPdfEditorEngine.h`, `src/engines/PdfEditorEngine.cpp`, `src/modes/CompressDialog.cpp`
**Acceptance:**
- `enum class MrcMode { Off, Lossless, Balanced, Aggressive }`.
- `exportPdfA(outputPath, conformanceLevel, MrcMode)` signature extended.
- CompressDialog: MRC mode selector + size estimate.

**Commit:** `feat(mrc): API + UI for MRC mode in CompressDialog (M7-P3 D5)`

### D6 — DjVu importer (optional)
**Files (NEW):** `src/engines/conversion/DjvuImporter.h + .cpp` gated `HAS_DJVU`
**Acceptance:** `HAS_DJVU` CMake option (default OFF). If enabled: import DjVu → parse → extract text + image layers → assemble as MRC PDF/A. **Import-only.** License-verify DjVuLibre.
**Commit:** `feat(djvu): optional DjVu importer (HAS_DJVU off default) (M7-P3 D6)`

### D7 — Tests
**Files (NEW):** `tests/TestMrcPipeline.cpp`
**Acceptance:**
- Synthetic scanned PDF (image + invisible OCR text) → MRC export → ≥5× reduction.
- MRC output passes veraPDF (PDF/A-2b).
- Sandwich text searchable.
- DjVu import test (QSKIP if HAS_DJVU off).

**Commit:** `test(mrc): TestMrcPipeline 5x reduction + PDF/A conformance + searchable (M7-P3 D7)`

### D8 — Documentation
**Files:** CHANGELOG, ROADMAP, LICENSE-3RD-PARTY, README
**Acceptance:**
- CHANGELOG: REMOVE "MRC compression inside PDF/A not yet implemented" admission. Add Features entry.
- ROADMAP Session 13 (WS3) → ✅ DONE.
- LICENSE-3RD-PARTY: jbig2enc + OpenJPEG (+ optional DjVuLibre).
- README: mention MRC pipeline.

**Commit:** `docs(mrc): close MRC admission + license matrix update (M7-P3 D8)`

### D9-MEMORY — Walkthrough + vault refresh
**Commit:** `docs(memory): M7-P3 MRC walkthrough + vault refresh (D9-MEMORY)`

</deliverables>

<verification>
```powershell
ctest -R TestMrcPipeline --output-on-failure
# Manual: scanned PDF MRC export → Adobe Reader visually identical + text searchable + PDF/A passes
```
</verification>

<constraints>
- **NEVER pattern-matching JBIG2 mode** (Xerox 2013 incident).
- **DO NOT add DjVu output format** (excluded per ROADMAP).
- DO NOT generate sandwich text from hOCR. Use M5 word boxes.
- DO NOT use GPL/AGPL JBIG2 encoder.
- DO NOT apply MRC to signed PDFs without explicit user opt-in (invalidates signatures).
- DO NOT enable HAS_DJVU by default.
</constraints>

<error_recovery>
- If JBIG2 encoder license blocking: STOP, document, defer to v2.0.
- If MRC output fails veraPDF: investigate JPX/JB2 conformance flags.
- If 5× reduction not achieved: tune JPEG2000 quality + JBIG2 mode.
</error_recovery>

<success_criteria>
- [ ] JBIG2 encoder license verified Apache/BSD
- [ ] MRC pipeline produces ≥5× compressed PDF/A-2b
- [ ] veraPDF confirms conformance
- [ ] Sandwich text searchable
- [ ] CHANGELOG admission removed
- [ ] D9-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 16 — M8-PROMPT-1: Marketing prep

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M8 launch | Priority: HIGH | Sprint position: 16 of 18
Depends on: M7 done (so all features real + audited)
Agents: /docs (copy), /design (screenshots)
Estimated context: ~20% | Effort: 1 week
</session_metadata>

<deliverables>

### D1 — Hero screenshots
**Files (NEW):** `marketing/screenshots/*.png`
**Acceptance:** 6-10 screenshots at 1920×1080 + 4K versions: signing flow, redaction with Edact-Ray defense visible, OCR with confidence overlay, mode switching, dark theme, accessibility view. **Use REAL PDFs only** (no Lorem Ipsum / fake data).
**Commit:** `marketing: hero screenshots for v1.0.0 launch (M8-P1 D1)`

### D2 — Demo video
**Files (NEW):** `marketing/demo-v1.0.0.mp4`
**Acceptance:** 60-90s: open PDF → annotate → sign with cert → redact → export PDF/A. Voiceover OR captions.
**Commit:** `marketing: 60-90s demo video (M8-P1 D2)`

### D3 — Website / landing page copy
**Files (NEW):** `marketing/website-copy.md`
**Acceptance:** Based on `docs/planning/AUDIT-v1.0.0.md` §5 positioning + §10 ladder. One-liner + sub-line + 3-pillar features + proof points + contribution invite.
**Commit:** `marketing: website + landing page copy (M8-P1 D3)`

### D4 — Press kit
**Files (NEW):** `marketing/press-kit/{logos/,screenshots/,product-summary.md,founder-photo.jpg}` (founder optional)
**Acceptance:** PNG logos + screenshots + 1-page product summary in `press-kit.zip`.
**Commit:** `marketing: press kit (M8-P1 D4)`

### D5-MEMORY — Walkthrough + vault refresh
**Commit:** `docs(memory): M8-P1 marketing prep + vault refresh (D5-MEMORY)`

</deliverables>

<verification>Manual review.</verification>

<constraints>
- DO NOT use stock photos pretending to be GlyphPDF screenshots.
- DO NOT show features that aren't actually implemented.
</constraints>

<success_criteria>
- [ ] 6-10 hero screenshots
- [ ] Demo video produced
- [ ] Website copy drafted
- [ ] Press kit assembled
- [ ] D5-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 17 — M8-PROMPT-2: Public release governance (LICENSE + CONTRIBUTING + SECURITY + GitHub repo + CI)

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **PHASE 6** via D5-MEMORY deliverable below.

<session_metadata>
Phase: M8 launch | Priority: HIGH | Sprint position: 17 of 18
Depends on: M7 done
Agents: /docs, /devops (CI workflows)
Estimated context: ~25% | Effort: 1 week
</session_metadata>

<deliverables>

### D1 — LICENSE file + SPDX headers
**Files:** `LICENSE` (NEW), every .h/.cpp in `src/`
**Acceptance:**
- LICENSE: full Apache-2.0 text from `https://www.apache.org/licenses/LICENSE-2.0.txt` (canonical) OR MIT if decided.
- Add `// SPDX-License-Identifier: Apache-2.0` header to every `.h`/`.cpp` in `src/` (script the bulk add).

**Commit:** `chore: LICENSE Apache-2.0 + SPDX headers (M8-P2 D1)`

### D2 — Governance files
**Files (NEW):** `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, `.github/ISSUE_TEMPLATE/{bug,feature}.md`, `.github/PULL_REQUEST_TEMPLATE.md`, `.github/FUNDING.yml`
**Acceptance:**
- CONTRIBUTING: build env + style + DCO sign-off + testing requirement.
- CODE_OF_CONDUCT: Contributor Covenant 2.1.
- SECURITY: 90-day disclosure window, in-scope (signing, redaction, encryption, sanitization, parsing) / out-of-scope (DoS unless leading to RCE).
- Issue/PR templates.
- FUNDING.yml: GitHub Sponsors or OpenCollective.

**Commit:** `docs: OSS governance files (CONTRIBUTING/CODE_OF_CONDUCT/SECURITY/templates) (M8-P2 D2)`

### D3 — README badges + positioning
**Files:** `README.md`
**Acceptance:** Badges (Apache-2.0 / build / release / sponsor) via shields.io. Positioning text matching audit §5.
**Commit:** `docs(README): badges + positioning for v1.0.0 (M8-P2 D3)`

### D4 — GitHub repo setup + CI/release workflows
**Files (NEW):** `.github/workflows/ci.yml`, `.github/workflows/release.yml`
**Acceptance:**
- Public repo. Branch protection on `main`.
- CI workflow: MSYS2 build + ctest on every PR.
- Release workflow: triggers on `v*` tag → build MSI → create Release with assets + SHA-256.
- Labels: bug, enhancement, good-first-issue, help-wanted, documentation, security, v1.1-roadmap.
- Pre-populate issues from v1.1+ scope (Cloud Sync, macOS port, mobile, etc.).

**Commit:** `ci+chore: GitHub Actions CI + release workflows + labels (M8-P2 D4)`

### D5-MEMORY — Walkthrough + vault refresh
**Commit:** `docs(memory): M8-P2 governance + vault refresh (D5-MEMORY)`

</deliverables>

<verification>CI green on first PR; release workflow tested with rc tag.</verification>

<constraints>
- DO NOT add CLA. Apache-2.0 §5 has implicit patent grant.
- DO NOT enable telemetry by default.
</constraints>

<success_criteria>
- [ ] LICENSE present + SPDX headers
- [ ] All governance files
- [ ] CI green
- [ ] Release workflow tested
- [ ] D5-MEMORY vault refreshed
</success_criteria>

---

# PROMPT 18 — M8-PROMPT-3: Tag v1.0.0 + sign MSI + package-manager submit + announce

> 🔒 **PHASE 0 — Read these 6 vault notes BEFORE any source code** (saves ~5-10K tokens):
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md`
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md`
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md`
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md`
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md`
>
> **This is the FINAL GATE — must follow PHASE 0 strictly even though tempted to skip.** PHASE 6 via D7-MEMORY deliverable below.

<session_metadata>
Phase: M8 launch FINAL | Priority: CRITICAL | Sprint position: 18 of 18 — **THE END**
Depends on: ALL prior 17 prompts complete
Agents: /release, /docs, /devops
Estimated context: ~25% | Effort: 4-5 days
</session_metadata>

<deliverables>

### D1 — Final verification gate
**Acceptance:**
- `ctest --output-on-failure -j4 --repeat-until-fail 5` — pass 5/5.
- Manual smoke: every feature in README works.
- Build fresh MSI via packaging/build-msi.bat.

### D2 — Sign MSI
**Acceptance:** EV code-signing cert (if obtained) OR standard. `signtool sign /a /tr http://timestamp.digicert.com /td sha256 /fd sha256 dist\GlyphPDF-1.0.0-x64.msi`.
**Commit:** `release: sign MSI for v1.0.0 (M8-P3 D2)`

### D3 — Final CHANGELOG
**Files:** `CHANGELOG.md`
**Acceptance:**
- Promote `[1.0.0-internal]` → `[1.0.0]`.
- Remove `[INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]` annotation.
- Add release date.
- Remove every Known Issue closed during M2-M7.

**Commit:** `docs(CHANGELOG): promote to [1.0.0] public release (M8-P3 D3)`

### D4 — Tag + push
**Acceptance:**
- `git tag -a v1.0.0 -m "Release v1.0.0 — open source Apache-2.0"`
- `git push origin v1.0.0`
- GitHub Actions release workflow auto-creates Release with MSI + SHA-256.

**Commit:** none (tag operation).

### D5 — GitHub Release
**Acceptance:** Release published with MSI + SHA-256 + GPG signature (if available) + CHANGELOG excerpt.

### D6 — Package-manager submissions
**Acceptance:**
- winget: PR to microsoft/winget-pkgs.
- chocolatey: build .nupkg + `choco push`.
- scoop: PR to ScoopInstaller/Extras OR maintain own bucket.

### D7-MEMORY — Announce + final vault refresh
**Files:** Multiple announcement channels + vault
**Acceptance:**
- HackerNews: "Show HN: GlyphPDF — open-source privacy-first PDF workstation".
- r/PDF, r/opensource, r/privacy, r/degoogle, r/privatetoolsio.
- PDF Association mailing list.
- Twitter/Mastodon thread with screenshots + key features.
- Blog post: "Why we built GlyphPDF" — positioning rationale + technical highlights.
- Vault: mark v1.0.0 SHIPPED in `01-current-state.md`; update `00-overview.md` status; final session log entry; if any new patterns from launch: add to anti-patterns.

**Commit:** `docs(memory): v1.0.0 public release vault + announce log (D7-MEMORY)`

</deliverables>

<verification>
- Tag exists: `git tag --list v1.0.0`
- MSI signed: `signtool verify /pa dist\GlyphPDF-1.0.0-x64.msi`
- GitHub Release live
- ≥1 package manager submission accepted
- Announcement posts live
</verification>

<constraints>
- DO NOT tag v1.0.0 if any test fails or any manual-smoke step fails.
- DO NOT push to remote without explicit user confirmation.
- DO NOT skip MSI signing (Pattern 6: misleading "verified publisher" without signature).
</constraints>

<success_criteria>
- [ ] All tests pass `--repeat-until-fail 5`
- [ ] MSI signed
- [ ] `git tag v1.0.0` exists
- [ ] GitHub Release published
- [ ] ≥1 package manager submission live
- [ ] HackerNews + r/PDF posts live
- [ ] D7-MEMORY vault refreshed with v1.0.0 SHIPPED status
- [ ] **REAL PUBLIC v1.0.0 SHIPPED — Branch C SCOPE LOCK FULFILLED 🚀**
</success_criteria>

---

# Post-launch v2.0 roadmap (separate planning effort)

After M8-PROMPT-3:
- **Cloud Sync** (opt-in encrypted, BYOSync OSS model)
- **macOS / Linux ports** (Qt 6 cross-platform; MSYS2-equivalent dep packaging)
- **Mobile companion** (separate codebase; shares WS2 Djot SemanticDocument)
- **Local-only AI** (bundled small models via Ollama; offline summarization/Q&A)
- **VLM-based extraction** (alternative to WS1 modular pipeline; ~2027 when local GPU VLM fast enough)
- **Surya as second layout detector** (if license verification deferred in M5-P2)

Plan in fresh v2.0-roadmap doc after v1.0.0 lands.

---

*End of M5-M8 prompts document. 18 prompts in chronological execution order. Each is self-contained with full 7-H structure + PHASE 0 banner + Dn-MEMORY deliverable. Catchup commits (`bc00e6a`→`cd2e3e6`) ensure HEAD/test-count/vault are accurate as of 2026-05-30.*
