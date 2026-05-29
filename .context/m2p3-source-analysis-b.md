# M2-PROMPT-3 Source Analysis B: CMakeLists + Docs

## CMakeLists.txt: existing CACHE option pattern

**No explicit CACHE options found.** The project uses:
1. `set(MSYS2_ROOT "C:/msys64/ucrt64")` — plain set, not CACHE
2. Environment variable patterns via `set(ENV{PKG_CONFIG_PATH} ...)`
3. `option()` macro is NOT used anywhere in the file

The project relies instead on:
- `find_package()` QUIET/REQUIRED patterns (Qt6, podofo, qpdf, OpenSSL, Tesseract, Leptonica, onnxruntime, Pdfium)
- Conditional `if(EXISTS "...")` guards around `add_executable` for test targets (no cache variables)

**For veraPDF integration recommendation:** Define a new boolean option at top of CMakeLists.txt:
```cmake
option(ENABLE_VERAPDF_CI "Enable veraPDF subprocess validation in CI (runs only in ctest, not app)" OFF)
```
If OFF, the test target `TestVeraPdfConformance` is not registered; users building locally skip it automatically.

---

## CMakeLists.txt: test registration pattern

**One complete test block (UnitTests, lines 372–387):**

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/UnitTests.cpp")
    add_executable(UnitTests
        tests/UnitTests.cpp
    )
    target_include_directories(UnitTests PRIVATE src)
    target_link_libraries(UnitTests PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets
    )
    add_test(NAME UnitTests COMMAND UnitTests)
    set_tests_properties(UnitTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;qt;headless"
    )
endif()
```

**Complete pattern breakdown:**
1. `if(EXISTS "path/to/source.cpp")` — conditional registration (optional test)
2. `add_executable(TestName source.cpp [additional_sources])` — single or multiple source files
3. `target_include_directories(PRIVATE src [tests])` — at least `src`, optional `tests`
4. `target_link_libraries(PRIVATE lib1 lib2 Qt6::Component1 Qt6::Component2)` — order: custom libs first, then Qt
5. `add_test(NAME TestName COMMAND TestName)` — register with ctest
6. `set_tests_properties(PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT N LABELS "...;...;...")` — set environment, timeout (60–300s), semicolon-delimited labels

**Labels used in existing tests:** `unit`, `integration`, `smoke`, `security`, `signatures`, `crypto`, `sanitization`, `redaction`, `threads`, `encryption`, `resources`, `controllers`, `ui`, `e2e`, `performance`, `benchmark`, `qt`, `headless`, `regression`

---

## CMakeLists.txt: pdfws_engines library target

Target name: **`pdfws_engines`** (STATIC library, lines 118–214)

Key definitions:
- Source files: src/engines/*, src/core/*, podofo backend, pdfium backend, qpdf backend, OCR engines, RenderCache, DiffEngine
- PUBLIC link libs: `pdfws_core`, `Tesseract::libtesseract`, `leptonica`, `OpenMP::OpenMP_CXX`, `qpdf::*`, `Pdfium::Pdfium`, `onnxruntime`
- PRIVATE link libs: `podofo::podofo`, `OpenSSL::SSL`, `OpenSSL::Crypto`, `Qt6::Network`, `Qt6::Widgets`, `Qt6::Concurrent`
- Compile definitions: `HAS_TESSERACT`, `HAS_RAPIDOCR`, `HAS_QPDF`, `HAS_PDFIUM`

**For veraPDF test:** Link `pdfws_engines` to access conversion/export APIs; link `Qt6::Test` for QTest framework.

---

## CMakeLists.txt: QProcess usage

**QProcess is NOT used anywhere in CMakeLists.txt or the project.**

Current design: all backends (PoDoFo, PDFium, qpdf, OCR engines) are linked libraries. The subprocess-only note in ROADMAP R8 ("Run veraPDF (AGPL; subprocess-only) in CI") refers to a future integration pattern (not yet implemented).

**For M2-PROMPT-3:** TestVeraPdfConformance will be the FIRST use of QProcess in this project if/when veraPDF subprocess invocation is added. The pattern would be:
1. Use `QProcess::execute()` or `QProcess qp; qp.start(...)` in the test to invoke external veraPDF CLI
2. Parse `qp.exitCode()` and stdout/stderr for validation results
3. No QProcess use in the application main code (CI test only)

---

## ROADMAP.md: S19 entry

**Session 19 — Print + Export Polish + Final UI Refinements** (lines 473–479):

```
**Session 19 — Print + Export Polish + Final UI Refinements**
- Print pipeline: `QPrinter` with per-page range, duplex, fit-to-page; print preview
- Export polish: PDF/A validation dialog; export progress for large documents
- Final UI: pixel-perfect icon audit; missing tooltips; keyboard shortcut gaps; theme
  consistency review; About dialog with third-party license viewer (reads `LICENSE-3RD-PARTY.md`)
```

**Not S19, but closest veraPDF reference:** Session 13 (Watermark + MRC) passes "Run veraPDF in CI" to Sessions 14–19. The actual PDF/A validation is mentioned in Session 19 export polish, but veraPDF subprocess is documented as a Phase 6 (Session 13) infrastructure dependency.

---

## ROADMAP.md: R8 risk entry

**R8 — MRC PDF/A output fails conformance validation** (lines 540–542):

```
| R8 | MRC PDF/A output fails conformance validation | MEDIUM | Run veraPDF (AGPL; subprocess-only) in CI; never link it in-process |
```

Also cited in anti-patterns (line 600):
```
Running veraPDF as a linked library | veraPDF is AGPL; invoke as subprocess in CI only
```

---

## CHANGELOG.md: [1.0.0-internal] structure

**[1.0.0-internal] section (lines 42–199) structure:**

```markdown
## [1.0.0-internal] - 2026-05-23 [INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]

> **⚠ Scope-lock note:** [long quotation block with context]

### Build Environment (v1.0.0 internal)
- Bullet list of environment/toolchain changes

### Architecture
- Bullet list of design decisions

### PDF Engine
- Bullet list of backend details

### Security
- Bullet list of security features

[... more subsections: Content, OCR, Conversion & Batch, Page Operations, Watermark & Optimization, etc.]

### Known Issues
- Bullet list of unimplemented or stubbed features
```

**For M2-PROMPT-3 entry:** Add a new subsection under `[Unreleased] — v1.0.0 Branch C SCOPE LOCK execution (M2-M8)`, ABOVE the existing "### Security (M2-PROMPT-2)" entry:

```markdown
### PDF/A Validation (M2-PROMPT-3 — 2026-05-XX)

- **veraPDF subprocess integration for CI** (D1): `TestVeraPdfConformance` test target added;
  invokes external veraPDF CLI via QProcess in test suite only (never linked in-process per
  AGPL licensing constraint). Environment variable `VERAPDF_PATH` configures veraPDF binary
  location; test skipped if binary absent or returns error. Validates MRC PDF/A output from
  WS3 compression pipeline against PDFBOX veraPDF rules (PDF/A-2b). Test fixture includes
  scanned document, OCR-enriched MRC output, and expected validation pass/fail signatures.
```

Or more concisely:
```markdown
- **veraPDF CI subprocess** (D1): `TestVeraPdfConformance` validates MRC PDF/A output via
  external veraPDF CLI (AGPL subprocess-only, never linked). Test skipped if `VERAPDF_PATH`
  absent. Fixture: scanned doc → MRC PDF/A → veraPDF validation rules (PDF/A-2b).
```

---

## LICENSE-3RD-PARTY.md: current veraPDF row

**veraPDF is listed in the license matrix (line 587):**

```
| veraPDF | ANY | AGPL-3.0 | Subprocess only | PDF/A validation (CI) | Never link in-process |
```

**For M2-PROMPT-3:** This row is ALREADY CORRECT and documents the intended usage. No changes needed unless additional notes are required (e.g., VERAPDF_PATH environment variable documentation).

**Table header + context (lines 5–6, 567):**
```
| Library | Version | License | Compatible | Compatibility Notes / Rationale |
| :--- | :--- | :--- | :---: | :--- |
```

Example row (line 7):
```
| **Qt 6** | 6.11.0 (MSYS2 ucrt64) | LGPL-3.0 / commercial | **Yes** | Permitted under LGPL-3.0 dynamic linking rules. |
```

---

## README.md: Build Instructions section

**Build Instructions section (lines 71–130):**

### Prerequisites
- Lists environment (MSYS2 at C:\msys64\)
- Code block with full pacman install command (multiline bash)
- Note callout: "We use the **ucrt64** environment..." explains why

### Build (Windows + MSYS2)
- Two code blocks: one bash (from MSYS2 shell), one PowerShell (alternative)
- Both follow same pattern: cd → mkdir build → cmake → cmake --build

### Why MSYS2 ucrt64?
- Prose paragraph explaining migration rationale

### Installation (MSI)
- Short code block: cd packaging && build-msi.bat

### Testing
- Environment variable setup: `set QT_QPA_PLATFORM=offscreen`
- ctest invocation with --output-on-failure
- Table of 14 test targets with Category | What it tests columns

---

## tests/TestVeraPdf.cpp: exists?

**NO — TestVeraPdf.cpp does NOT exist.**

Search result: `NOT_FOUND`

The test target is not yet registered in CMakeLists.txt. M2-PROMPT-3 must create:
1. `C:\Users\User\Projects\pdf\tests\TestVeraPdf.cpp` (or `TestVeraPdfConformance.cpp`)
2. CMakeLists.txt registration block following the UnitTests pattern (lines 372–387 as template)
3. Environment variable guard + optional `ENABLE_VERAPDF_CI` option

---

## Implementation notes

### No existing CACHE options
The project has NOT defined any CMake CACHE variables (no `option()` or `set(...CACHE...)`). It uses:
- QUIET/REQUIRED find_package patterns (auto-skip if not found, unless REQUIRED)
- `if(EXISTS "test_file.cpp")` to conditionally register test targets
- Condition variables like `Tesseract_FOUND`, `qpdf_FOUND`, `onnxruntime_FOUND`

**Recommendation:** Introduce a simple boolean option for the veraPDF test:
```cmake
option(ENABLE_VERAPDF_CI "Enable veraPDF subprocess validation in CI" OFF)
```
Then wrap the test registration:
```cmake
if(ENABLE_VERAPDF_CI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestVeraPdf.cpp")
    add_executable(TestVeraPdf ...)
endif()
```

### FATAL_ERROR guards for licensing
Lines 81–106 in CMakeLists.txt show the pattern:
- Check for MuPDF/Poppler via multiple methods (TARGET, find_package, pkg_check_modules)
- FATAL_ERROR on any detection (AGPL-3.0 / GPL-2.0+ incompatibility)

**veraPDF guard not present** (it's subprocess-only, not linked, so no guard needed).

### Test labeling convention
Existing labels: `unit;qt;headless`, `security;signatures;qt;headless`, `integration;e2e;qt;headless`, etc.
**For TestVeraPdf:** Recommend labels like `unit;pdf-a;validation;ci;headless` or `integration;pdf-a;ci;headless`.

### No QProcess use yet
This is the FIRST test to use QProcess. Ensure:
1. Qt6::Core linked (already done in all test targets)
2. #include <QProcess> in test source
3. Proper error handling for missing VERAPDF_PATH environment variable (QSKIP)

### Office import is still stubbed
CHANGELOG line 194 notes: "Office→PDF import not implemented (only PDF→Office conversion paths exist)." Relevant to M2-PROMPT-3 scope? (May require data format conversions before veraPDF validation.)

### No RapidOCR real implementation yet
CHANGELOG line 197: "RapidOcrEngine: PP-OCRv5 tensor pre/post processing is a stub. The engine is runtime-disabled in the OCR mode selector."
**Implication:** MRC PDF/A output for veraPDF validation will use Tesseract-only output or pre-recorded test fixtures, not live PP-OCRv5, in M2-PROMPT-3 scope.

### veraPDF external invocation pattern
The ROADMAP (R8) constraint is clear: subprocess-only, never in-process. The test will:
1. Export a sample PDF/A to disk
2. Call `QProcess::execute(veraPdfPath, {args})` to validate
3. Parse stdout/stderr or exit code for PASS/FAIL
4. QSKIP if VERAPDF_PATH environment variable not set or binary not found

---

## Summary for M2-PROMPT-3 implementation

### Files to create/modify:
1. **Create** `tests/TestVeraPdf.cpp` — QProcess-based validation test
2. **Modify** `CMakeLists.txt`:
   - Add optional `ENABLE_VERAPDF_CI` option (line ~16)
   - Add test registration block (after line 625)
3. **Modify** `CHANGELOG.md`:
   - Add M2-PROMPT-3 entry under [Unreleased] section (before "### Security (M2-PROMPT-2)")

### CMakeLists.txt test block template:
```cmake
if(ENABLE_VERAPDF_CI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestVeraPdf.cpp")
    add_executable(TestVeraPdf
        tests/TestVeraPdf.cpp
    )
    target_include_directories(TestVeraPdf PRIVATE src tests)
    target_link_libraries(TestVeraPdf PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
    )
    add_test(NAME TestVeraPdf COMMAND TestVeraPdf)
    set_tests_properties(TestVeraPdf PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
        LABELS "unit;pdf-a;validation;ci;headless"
    )
endif()
```

### Key constraint:
- **NO veraPDF linking in-process** — subprocess via QProcess only
- **VERAPDF_PATH environment variable** — configures external binary location
- **Test skips gracefully** if veraPDF binary absent (QSKIP, not FAIL)
