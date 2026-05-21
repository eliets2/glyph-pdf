# GlyphPDF — Next Claude Code Session Brief

**Updated:** 2026-05-21 | **Tests:** 10/10 GREEN | **Branch:** master
**Project root:** `C:\Users\User\Projects\pdf`
**Full session prompts:** `SESSION_PROMPTS_V2.md`

---

## What has been built (confirmed committed + tested)

| Commit | Sessions | What it contains |
|--------|----------|-----------------|
| `a5d9ec9` | 1 | `AppContext` shared_ptr DI + `Bootstrapper` wiring |
| `307b09b` | 2–5 | `ToolId`/`ToolRegistry`, `IPdfBackend`/`IPdfRenderer`/`IPdfSearcher` abstractions, `PoDoFoBackend`, `PdfiumBackend`, `RenderCache` (LRU, 256 MB, tiled), `QpdfBackend`, save pipeline, repair-on-open |
| `bc00648` | 6 | `ISignatureManager` + `PAdESLevel` enum, PAdES B-LT/B-LTA, DSS/VRI, cert encryption |
| `d0cb76a` | 7–11 | Page ops (crop/resize/reorder/header-footer/Bates), forms (radio/combobox/listbox/date/numeric/FDF), OCR pipeline (Tesseract+RapidOCR+ROVER), diff/conversion engine, batch/compare modes |
| `8ddb588` | – | Untracked `Testing/Temporary/LastTest.log` (gitignore fix) |
| `ad9c1ab` | – | **DLL ABI fix + qoffscreen deploy (see CRITICAL section below)** |

**10 test targets:** UnitTests, TestThreadSafety, TestControllers, TestRedaction, TestEncryption, TestResourceLimits, TestSanitization, TestSignatureValidation, TestInterfaces, SmokeTest — **all PASS**.

> ⚠️ Sessions 7–11 code exists in the repo but **depth of implementation is unverified**.
> Some deliverables may be real implementations; others may be interface stubs or scaffolding.
> **DO NOT assume any Session 7–11 feature works end-to-end without reading the source and
> running the relevant test target.**

---

## CRITICAL — DLL configuration (DO NOT CHANGE)

The build directory (`build/`) contains a carefully chosen mix of MinGW runtime DLLs that
solves a MSYS2 ↔ Qt MinGW 13.1.0 ABI mismatch. Changing any of these DLLs will break tests.

### Why each DLL was chosen

| DLL in `build/` | Chosen from | Why — what breaks if you change it |
|-----------------|-------------|-------------------------------------|
| `libstdc++-6.dll` | **Qt's MinGW** (2.24 MB) | `Qt6Pdf.dll` imports `__emutls_v._ZSt11__once_call` and `__emutls_v._ZSt15__once_callable` — emulated TLS for `std::call_once`. MSYS2's libstdc++ uses a completely different implementation (`_ZSt15__get_once_callv`); those symbols are absent → 0xC0000139 crash on Qt6Pdf load. |
| `libwinpthread-1.dll` | **MSYS2** (63 KB) | `libtesseract-5.5.dll` imports `nanosleep64`. Qt's libwinpthread (53 KB) does NOT export `nanosleep64` → 0xC0000139 crash on tesseract load. |
| `libgcc_s_seh-1.dll` | **MSYS2** (148 KB) | MSYS2's version is a strict superset of Qt's (130 exports vs 126; 4 extras are GCC hardening symbols that no DLL in the chain actually needs). Safe for all. |

**Backup files present:**
- `libstdc++-6.dll.qt-bak` — Qt's original (current in use = Qt's)
- `libwinpthread-1.dll.qt-bak` — Qt's original (MSYS2's is active)

### Why `build/platforms/qoffscreen.dll` must exist

`TestControllers` is the only test that uses `QTEST_MAIN` (creates `QApplication`), which
requires a Qt platform plugin at runtime. All other tests use `QTEST_GUILESS_MAIN`
(`QCoreApplication` — no platform needed). `QT_QPA_PLATFORM=offscreen` requires
`qoffscreen.dll` in `build/platforms/`.

**CMake fix:** `CMakeLists.txt` now has a `POST_BUILD` custom command on `TestControllers`
that copies `${Qt6_DIR}/../../../plugins/platforms/qoffscreen.dll` into `$<TARGET_FILE_DIR>/platforms/`
automatically on every build. If you re-run cmake from scratch, the plugin deploys automatically.

---

## Test runner

Run tests with:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
$env:PATH = 'C:\Users\User\Projects\pdf\build;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.10.2\mingw_64\bin;C:\vcpkg\installed\x64-mingw-dynamic\bin;' + $env:PATH
Set-Location 'C:\Users\User\Projects\pdf\build'
ctest --output-on-failure -j4
```

Or use `C:\tmp\run_tests.ps1` (runs all 10 sequentially, shows PASS/FAIL + exit codes).

**Note on TestControllers specifically:** it uses `Start-Process -NoNewWindow` (not stdout
redirection). If you redirect its stdout to a pipe and try to read it from a background task,
it will appear to hang because Qt Test writes to stdout and the pipe buffer fills up before
the background task drains it. Use `$p.ExitCode` from `Start-Process -Wait` or run ctest
directly.

---

## Architecture

```
pdfws_core (interfaces, ToolId, AppContext, commands base)
    └── pdfws_engines (PoDoFoBackend, PdfiumBackend, QpdfBackend, OCR, conversion)
        └── pdfws_commands (undo commands: Redact, EditAnnotation, RotatePage, etc.)
            └── pdfws_ui (GpMainWindow, controllers, modes, ribbon, dialogs)
                └── PdfWorkstation (main.cpp + Bootstrapper)
```

**Static libraries only** — no shared libs inside the project.
`pdfws_engines` links tesseract/leptonica/pdfium/qpdf/podofo **PUBLIC** (transitive).
`pdfws_ui` links Qt6::Pdf, Qt6::PdfWidgets, Qt6::PrintSupport, Qt6::Svg, podofo PRIVATE.

Interfaces in `src/core/interfaces/`:
- `IPdfEditorEngine.h` — page ops, rendering, search, annotation, crop/resize/Bates (stubs may exist)
- `ISignatureManager.h` — PAdES signing, validation, TSA
- `IOcrEngine.h`, `IFormManager.h`, `IConversionEngine.h`, `ICollaboration.h`

---

## What is NOT yet started (Sessions 12–20)

Per `SESSION_PROMPTS_V2.md`, these sessions are confirmed unstarted:

| Session | Feature |
|---------|---------|
| 12 | Find-and-replace with regex, bookmark/outline navigation panel |
| 13 | Watermark rendering + image compression / PDF optimization (MRC) |
| 14 | Accessibility (screen reader, keyboard nav, focus order, WCAG) |
| 15 | Localization framework + RTL support |
| 16 | Error handling hardening (structured error codes, recovery dialogs) |
| 17 | Installer / packaging (WiX MSI) |
| 18 | Auto-update mechanism |
| 19 | Print polish + integration test suite |
| 20 | Final QA pass, performance profiling, v1.0 release tag |

Also unstarted (new workstreams in ROADMAP.md):
- **Session 3.5** — `docmodel` + `pdfws_djot` Djot interchange layer (upstream of Sessions 8/9/10/13)
- **WS1 extension** — OCR ensemble pipeline (PP-DocLayoutV2 / Surya layout, ROVER word merge, LaneScheduler)
- **WS3 extension** — MRC compression inside PDF/A (JBIG2 bitonal + JPEG2000 background + OCR text sandwich)

---

## How to start the next session

1. **Paste this entire file as the opening message.**
2. Immediately run the test suite and confirm 10/10 green:
   ```
   powershell -File C:\tmp\run_tests.ps1
   ```
3. Decide which session to work on. Recommended order:
   - **Verify Sessions 7–11** depth first (read source files + run targeted tests) before
     starting Session 12, so you know what's real vs. scaffolded.
   - Then proceed to Session 12 per `SESSION_PROMPTS_V2.md`.
4. Follow the **STANDARD EXECUTION PROTOCOL** in `SESSION_PROMPTS_V2.md`:
   ANALYZE → PLAN → IMPLEMENT+VERIFY (build after each deliverable) → CONTEXT GATE at 50% → FINAL VERIFICATION.
5. Write `STATE.md` at 50% context usage — always.

---

## Architectural non-negotiables (enforce every session)

- **MuPDF (AGPL-3.0): NEVER link in-process**
- **Poppler (GPL-2.0+): NEVER link in-process**
- **qpdf in the signing path: NEVER** — flattens xref, invalidates ByteRange
- **Black rectangle redaction: NEVER** — always excise from content stream
- **Spawn-per-page ONNX process: NEVER** — use `LaneScheduler` persistent warm worker
- **Character-level OCR majority vote: NEVER** — word-level ROVER only
- **DjVu as output format: EXCLUDED** (import-only, if ever needed)
- **Djot grammar reimplementation: NEVER** — vendor the Lua reference parser (MIT)
- **All incremental saves must use PoDoFo `WriteUpdate` (not full rewrite) when signatures exist**
- **SHA-256 only for signature hashing** — no MD5 or SHA-1
- **Never regenerate/overwrite signed byte ranges** — validate first, sign only if clean

---

## Open problems from this session (already fixed — for reference)

These problems are SOLVED. Recorded here so the next session agent does not re-investigate.

1. **MSYS2 libstdc++ missing `__emutls_v.*` symbols** → UnitTests/TestThreadSafety crashed on
   startup (0xC0000139). Fixed: restored `libstdc++-6.dll` from `.qt-bak` (Qt's version).

2. **Qt's libwinpthread missing `nanosleep64`** → tesseract DLL failed to load. Fixed: kept
   MSYS2's `libwinpthread-1.dll` in build directory.

3. **`qoffscreen.dll` not in `build/platforms/`** → `TestControllers` exited with -1 before
   any test ran (QApplication initialization failure). Fixed: CMakeLists.txt POST_BUILD
   custom command auto-deploys the plugin.

4. **TestControllers stdout-redirect hang** → Not a real hang. Qt Test + piped stdout in a
   background PowerShell task appears stuck because the pipe write blocks when the reading
   end isn't draining fast enough. Use `Start-Process -NoNewWindow -Wait` (no redirect)
   or run via `ctest` which handles this correctly.
