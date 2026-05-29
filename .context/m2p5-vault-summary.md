# M2-PROMPT-5 Vault Summary

## Current commit + test status

**HEAD:** `74fbb5f` (May 29, 2026) — "docs: update CHANGELOG/ROADMAP/README for M2-P3 veraPDF validation (S19 done, R8 closed)"

**M2 Commits (May 29, 2026):**
- `42c0f46` — M2-P1 D1+D2: GlyphAdvanceCalculator + numeric-only TJ normalization (Edact-Ray defense)
- `adec98b` — M2-P1 D3: 3 Edact-Ray regression tests (16/16 TestRedaction pass)
- `435a549` — M2-P1 D4: CHANGELOG + ROADMAP Edact-Ray update
- `b0d5a7a` — M2-P2 D1: scrub invisible OCR text (Tr==3) in redactCanvasRecursively
- `a1221e6` — M2-P2 D2: rewrite testOCRScrubbing() as mechanistically correct regression test
- `8765629` — M2-P2 D3: CHANGELOG + ROADMAP updated; Session 7 D2 marked DONE
- `2200aca` — M2-P3 D1-D4: VeraPdfValidator + PdfAValidationPanel wired; TestVeraPdf (skips without CLI); 15/15 ctest pass
- `74fbb5f` — M2-P3 D5: CHANGELOG/ROADMAP/README updated; S19 done + R8 closed

**Test count:** 14 ctest suites (all passing)
- 16/16 TestRedaction individual cases pass (13 prior + 3 new M2-P1)
- 15/15 ctest suites pass (one new TestVeraPdf with skip-on-no-CLI behavior)

## Git status

```
 M .gitignore
 M SESSION_BRIEF_NEXT.md
?? .context/
?? CLAUDE.md
?? docs/
```

**Working tree:** Mostly clean. Untracked: `.context/` directory (new vault summary output), `CLAUDE.md` (project memory), `docs/` (planning files from Desktop migration).

## Last 6 commits (oneline)

```
74fbb5f docs: update CHANGELOG/ROADMAP/README for M2-P3 veraPDF validation (S19 done, R8 closed)
2200aca feat(pdfa): VeraPdfValidator + PdfAValidationPanel wired to real veraPDF subprocess (S19, R8)
8765629 docs: update CHANGELOG/ROADMAP for M2-P2 invisible OCR text scrub (S7 D2 done)
a1221e6 test(security): D2 invisible OCR text scrub regression test (S7 D2)
b0d5a7a feat(security): D1 scrub invisible OCR text (Tr==3) in redactCanvasRecursively (S7 D2)
435a549 docs: D4 CHANGELOG + ROADMAP Edact-Ray update (M2-P1)
```

## Test result mtimes

| Filename | LastWriteTime |
|---|---|
| TestRedaction_results.txt | 2026-05-29 03:19:13 AM |

Only one results file found in `build/` dir (most recent test run = TestRedaction on 2026-05-29).

## Build environment

| Property | Value |
|---|---|
| **MSYS2 shell** | ucrt64 (NOT mingw64) |
| **GCC version** | 16.1.0 (Rev5, Built by MSYS2 project) |
| **Qt version** | 6.11.0 (via `qmake` from C:/msys64/ucrt64/lib) |
| **CMake** | 4.3.3 |
| **C++ standard** | CMAKE_CXX_STANDARD = **17** |
| **Compiler flags** | `-Wall -Wextra -Wno-unused-parameter` |
| **PoDoFo** | 1.1.0 (vendored at `third_party/podofo_build/`) |
| **PDFium** | vendored Windows binary at `third_party/pdfium/` |
| **ONNX Runtime** | 1.17.3 vendored at `onnxruntime-win-x64-1.17.3/` |

**CMakeLists.txt snippet:**
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

## C++ standard note

**CMAKE_CXX_STANDARD = 17** (required, not optional). This is C++17, NOT C++20 or C++23.

**std::expected caveat:** `std::expected<T, E>` requires **C++23** per the C++ standard library. GCC 16.1.0 supports C++23 via `-std=c++23`, but the project is locked at C++17.

**Current usage:** GlyphPDF does NOT use `std::expected`. Error handling uses:
- `QVariant` (fallback; see CLAUDE.md architecture notes)
- `std::runtime_error` (e.g., GlyphAdvanceCalculator throws on glyph width resolution failure)
- Qt signals/slots for async errors
- Custom result types (`PdfAValidationReport` with violations vector)

**If M2+ introduces std::expected:** Either:
1. Upgrade to C++20 or C++23 in CMakeLists.txt (requires audit of all code for C++17→C++20 compatibility issues, e.g., `<=>` three-way comparison, structured bindings, concepts)
2. Use a backport library (gsl::expected, boost::outcome, tl::expected) that compiles in C++17
3. Stick with QVariant + Qt error patterns (current safe path)

**Recommendation for M5+ (LaneScheduler):** Lock decision now. If GPU lane scheduling requires `std::expected` (for type safety), bump to C++20 preemptively in a dedicated commit before LaneScheduler work begins.

## Key rules for LaneScheduler

**DO NOT:**
- Use Qt global `QThreadPool::globalInstance()` (legacy; blocks reusable worker creation)
- Spawn fresh `QThread` per GPU task (resource leak; context switch thrash)
- Block the main thread waiting for async results
- Mix OS threads with Qt's `QThread` pooling without explicit lifeline management

**DO:**
- Use `QFuture`-based API (`QtConcurrent::map`, `QtConcurrent::run`) for transparent task queuing + result chaining
- Return `QFuture<T>` from all async lane methods so callers can `.waitForFinished()` or chain `.then()`
- Track lane worker lifetime via `shared_ptr<WorkerThread>` or `unique_ptr<QThread>` (not leaked globals)
- Document lane semantics in method comments (GPU lane = persistent warm worker; CPU lane = elastic pool per task)
- Emit Qt signals on completion (not callbacks) for QObject-integrated error handling

**Result handling:**
- Prefer `QFuture<T>` + `.result()` for synchronous wait
- Or `QFuture<QVariant>` + type-check `.toMap()` / `.toList()` for variant heterogeneous results
- If `std::expected<T, SchedulerError>` is introduced, first bump C++20 (as above); then wrap in `QFuture` so Qt integrates cleanly

**See also:**
- M5-PROMPT-2 (LaneScheduler spec, Surya layout detector gate)
- M5-PROMPT-3 (OCR ensemble: Tesseract + real PP-OCRv5 + LayoutDetector + LaneScheduler wiring)
- `MONTHS-2-8-PROMPTS.md` M5 section (full spec with architecture diagram reference)
