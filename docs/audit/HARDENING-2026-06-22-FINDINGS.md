# GlyphPDF Hardening — 2026-06-22 Findings

## Security fixes applied

Branch: `fuzz/redaction-rig` (one commit ahead of `main`; carries the fuzz rig +
reproductions used to verify F-02 and the Lua crash). All commits local-only.
Build: `cmake --build build`. Suite: `QT_QPA_PLATFORM=offscreen ctest -j4` —
**100% / 39** after every fix (TestLaneScheduler and TestBatchMode are known
parallel-repeat flakes; both pass single-pass).

| # | ID | File | Commit |
|---|----|------|--------|
| 1 | F-02 | `src/engines/podofo/PoDoFoBackend.cpp` | `1972901` |
| 2 | lua_tostring null-deref | `src/pdfws_djot/LuaDjotCodec.cpp` | `1ca69dd` |
| 3 | M-1 (Lua DoS) | `src/pdfws_djot/LuaDjotCodec.cpp` | `1ca69dd` |
| 4 | F-05 | `src/engines/podofo/PoDoFoBackend.cpp` + `tests/TestRedaction.cpp` | `05d303a` |
| 5 | H-1 | `src/engines/VeraPdfValidator.cpp` | `830770e` |
| 6 | M-3 | `src/engines/PatternRedactor.cpp` | `92af05d` |

---

### 1. F-02 — redaction ignored the text-matrix linear part (CONFIRMED leak)

- **File/lines:** `PoDoFoBackend.cpp` `redactCanvasRecursively`
  - text-matrix state added at `:1220` (`RedactCtm tm; RedactCtm tlm; double penX;`)
  - intersection rewritten at `:1272` (`isIntersectingSpan(penStart, penEnd)` + `mapTextToDevice`)
  - matrix maintenance in `BT` / `Tm` (`:1356`) / `Td` / `TD` / `T*` / `'` / `"` handlers
- **What changed:** Previously `Tm a b c d e f` was collapsed to the scalar
  translation (e,f) into `textX`/`textY`; the linear part (a,b,c,d) was discarded
  and the span was mapped through the CTM only — modeling rotated/scaled/skewed
  text as a horizontal span at the wrong device location, so it never intersected
  the redaction rect. Now a full text matrix `Tm` and text-line matrix `Tlm` are
  tracked per PDF 9.4.2, plus a horizontal pen offset; each baseline endpoint maps
  to device as `[t 0 1] × Tm × CTM` (full affine) and the axis-aligned bbox of the
  transformed endpoints is tested.
- **Verification (oracle):** rebuilt driver
  (`fuzz/build_clang/build_redaction_driver.sh`) + `fuzz/run_oracles.sh`:
  - `identity → CLEAN`, `rot90 → CLEAN`, `rot45 → CLEAN`, `scale3x → CLEAN`,
    `skewx → CLEAN` (the four non-identity cases were `LEAK` before the fix; the
    originally-confirmed 90° `VERTLEAK` case now scrubs — no residual `Tj`).
  - ctest 39/39 green.

### 2. lua_tostring null-deref (CONFIRMED crash)

- **File/lines:** `LuaDjotCodec.cpp` `:625` (require path) and `:644` (parse path).
- **What changed:** `std::string err = lua_tostring(L,-1)` → `const char* e =
  lua_tostring(L,-1); std::string err = e ? e : "unknown lua error";` at both
  sites. `lua_tostring` returns NULL for a non-string error value with no
  `__tostring`, and `std::string(NULL)` aborts under hardened libstdc++.
- **Verification:** repro `fuzz/findings/lua_tostring_nullderef/` reproduces the
  old crash (exit 127). A product-linked driver (built ad-hoc, then removed)
  pointed the real `djotToDocument` at a `djot.lua` raising
  `error(setmetatable({},{}))`: it now **throws cleanly** ("failed to require
  djot: unknown lua error") instead of aborting; normal djot still parses. ctest
  djot/codec/roundtrip + full 39/39 green.

### 3. M-1 — Lua djot parse DoS (no instruction/memory cap)

- **File/lines:** `LuaDjotCodec.cpp` — bounded allocator `boundedAlloc` (`:92`),
  state created via `lua_newstate(boundedAlloc, &mem)` (`:594`), instruction-count
  hook installed via `lua_sethook(..., LUA_MASKCOUNT, kHookInterval)` (`:604`).
- **What changed:** A `LUA_MASKCOUNT` hook raises a Lua error once a per-call
  instruction budget is exhausted (surfaced as a normal parse failure via the
  surrounding `lua_pcall`), and a custom `lua_Alloc` enforces a 256 MiB live-bytes
  ceiling so a pathological input cannot hang the VM or OOM the process. The budget
  guard is cleared on every exit path (including exceptions) via a small RAII guard.
- **Verification:** djot round-trip / codec tests pass (normal parse finishes well
  within budget); full 39/39 green.

### 4. F-05 — redaction audit sidecar leaked regions + pre-redaction hash

- **File/lines:** `PoDoFoBackend.cpp` `applyRedactions` — opt-in flag at `:1679`,
  rewritten log block at `:1811`+; test `tests/TestRedaction.cpp::testAuditLogSidecar`.
- **What changed:** The log was written unconditionally to
  `<currentFile>.redaction-log.json` beside the source with exact region
  coordinates and `before_sha256` (a hash of the UN-redacted file) under the
  source's default ACL. Now the audit log is **opt-in and OFF by default**
  (`GLYPHPDF_REDACTION_AUDIT=1`); when enabled it is written to the per-user
  `AppDataLocation` (never beside the source) and records only the region COUNT,
  the after-hash, page, filename and timestamp — **no `before_sha256`, no
  coordinates**. The pre-redaction hash is no longer even computed.
- **Test change (strengthened, not weakened):** `testAuditLogSidecar` previously
  asserted the insecure behavior (sidecar present, contains `before_sha256`); it
  now asserts (a) default-off → no sidecar beside source, (b) opt-in → log in
  app-data with neither `before_sha256` nor `regions`.
- **Verification:** TestRedaction + full 39/39 green.

### 5. H-1 — veraPDF cmd.exe metachar injection

- **File/lines:** `VeraPdfValidator.cpp` `:64`–`:78` (the path-validation block;
  forbidden set at `:72`).
- **What changed:** When the CLI is a `.bat`/`.cmd` it runs via
  `cmd.exe /c call <bat> ... <pdfPath>`, and the old blocklist only rejected
  `& | < >` — missing `%VAR%` expansion, the `^` escape, and `( ) "`. Replaced with
  a **strict allowlist** rejecting any `pdfPath` containing `% ^ " & | < > ( )`.
  exitCode / waitForStarted / 30 s timeout handling unchanged.
- **Verification:** TestVeraPdf + full 39/39 green.

### 6. M-3 — custom-regex ReDoS

- **File/lines:** `PatternRedactor.cpp` `findMatches` `:258`–`:300`
  (`kMaxRegexInput` at `:271`, time guard in the match loop).
- **What changed:** A user-supplied custom regex run via `globalMatch` over full
  page text on the UI path could catastrophically backtrack. Now the input is
  capped at 256 KiB (truncated with a warning) and the match loop aborts after a
  1500 ms wall-clock budget, returning partial results. **Documented residual:** a
  single PCRE2 match step is not interruptible mid-step, so the input cap is what
  bounds that case; a fully cancellable worker thread is out of scope for this
  minimal hardening. Built-in named patterns are linear and unaffected.
- **Verification:** TestPatternRedact + full 39/39 green.

---

All six fixes built clean and left the suite at 100% / 39. No `main` push performed.

---

## UI-wiring fixes applied

Scope: MenuBar dead/silent menu items + a durable integrity gate, plus the
EncryptionDialog permissions-restriction logic. Files: `src/shell/MenuBar.{h,cpp}`,
`src/core/ToolId.{h,cpp}`, `src/ui/EncryptionDialog.cpp`,
`tests/TestMenuBarIntegrity.cpp` (+ its `CMakeLists.txt` registration).

Background (emergence H-2): MenuBar dispatched toolId strings through
`onToolActivated → ToolRegistry::activateFromString → toolIdFromString`, but
several menu IDs resolved to no ToolId/alias/handler and silently no-op'd. Unlike
the ribbon, the menu had no integrity test and was not gated by `plannedTools()`.

Root-cause structural fix: the MenuBar now declares a single source of truth —
`MenuBar::actionSpecs()` — classifying every actionable item as
`Registry` (routed to a controller), `Local` (handled inline in MenuBar), or
`Disabled` (intentionally greyed-out / planned). The constructor consults this
table to auto-disable `Disabled` items (greyed with a "Planned for a future
release." tooltip), and `TestMenuBarIntegrity` enumerates the same table so a
future dead item cannot ship silently.

1. **Dead Edit menu — DISABLED.** `MenuBar.cpp` Edit menu
   `cut`/`copy`/`paste`/`delete`/`select-all` had no ToolId/handler. The viewer
   exposes no clipboard slots (verified — no copy/cut/paste/selectAll in
   `PdfViewerWidget.h`), so these are genuinely unshipped. Classified
   `MenuDispatch::Disabled` → rendered greyed with a planned tooltip rather than
   faked. Hide-don't-delete.

2. **Forms aliases — WIRED.** `ToolId.cpp` added kebab aliases `"date-field"`→
   `DateField`, `"num-field"`→`NumField`, `"signature-field"`→`SigField`
   (FormsController already handles all three). Menu items
   `date-field`/`num-field`/`signature-field` now dispatch correctly. Note:
   `calc-field` resolves to `ToolId::CalcField` but FormsController has **no**
   handler for it (PRD gap), so the `Add Calculated Field` item is classified
   `Disabled` rather than left silently dead.

3. **Document aliases — WIRED.** `ToolId.cpp` added `"extract-page"`→`Extract`
   and `"headers-footers"`→`AddHeader` (PagesController handles both). Menu items
   `extract-page` / `headers-footers` now dispatch correctly.

4. **Measure items — DISABLED.** `measure-dist` / `measure-area` had no handler
   and the ribbon already hides their equivalents via `plannedTools()`
   (`"measure"`,`"distance"`,`"area"`). Classified `Disabled` to match the
   ribbon's planned-tool treatment.

5. **Window/misc stubs.**
   - `close` — **WIRED** (Local) → `MainWindow::close()`.
   - `save-copy` — **WIRED** (Local) → routes to the Save As flow via
     `onToolActivated("saveAs")`.
   - `find-replace` — **WIRED** (Local) → `toggleFindBar()` (the FindBar already
     contains the replace inputs/buttons, so it is the same bar as Find; this
     item previously no-op'd).
   - `new-window`, `polyline`, `custom-stamp`, `toggle-comments`, `tile`,
     `updates` — **DISABLED** (unshipped; ribbon equivalents `newWin`/`poly`/
     `customStamp`/`comments` are already in `plannedTools()`; `updates` opt-in
     lives under Preferences ▸ Updates). Greyed with a planned tooltip.

6. **TestMenuBarIntegrity — ADDED (durable gate).** `tests/TestMenuBarIntegrity.cpp`
   mirrors TestRibbonIntegrity: every `Registry` spec must resolve to a known
   ToolId AND have a controller handler; every `Local` spec must be declared in
   `MenuBar::localHandlerIds()`; specs must be non-empty and free of conflicting
   dispatch classes. This would have caught findings 1–4. Registered in the root
   `CMakeLists.txt` alongside TestRibbonIntegrity (offscreen, `LABELS
   "unit;menubar;integrity;ui;qt;headless"`).

7. **EncryptionDialog inverted logic (R-04) — FIXED.**
   `EncryptionDialog.cpp:137-138`. The accept handler computed
   `hasRestrictions = !print || !copy || !modify`. With the safe defaults
   (print=on, copy=on, modify=**off**) the `!modify` term was always true, so
   accepting with default permissions spuriously demanded an owner password.
   Corrected to `!print || !copy || modify` — allowing modification is the
   loosening that an owner password gates, matching the existing reset semantics
   in the owner-password `textChanged` handler (`:86-88`). No dialog unit test
   exists; the permissions group is also disabled until an owner password is
   entered, so the regression path was specifically default-perms + no owner
   password. Verified by inspection against the dialog's intended semantics.

8. **(Optional) dead signals** — NOT TOUCHED. `navigationChanged` /
   `pageOperationFinished` / `screenChanged` rewiring was judged out of the
   smallest-correct-diff envelope for this pass and skipped (the listed items 1–7
   were the silent-no-op and correctness issues). Left cleanly for a follow-up.

**Verification:** `cmake --build build` clean (new TestMenuBarIntegrity target
links via `pdfws_ui`). `ctest -j4` → 40/40 (was 39 + the new test); a parallel
run intermittently shows the known TestBatchMode parallel-repeat flake, which
passes single-pass (`ctest -R TestBatchMode` → 1/1 Passed). TestMenuBarIntegrity
and TestRibbonIntegrity both Passed. No `main` push performed.

## Performance fixes applied

Branch: `main` (commits local-only, no push). Build: `cmake --build build`.
Suite after every fix: `QT_QPA_PLATFORM=offscreen ctest -j4` → **100% / 40**
(TestBatchMode is a known parallel-repeat flake; passes single-pass). Each fix
is behavior-preserving and was kept to the smallest correct diff; no test was
weakened, and the M-3 ReDoS input cap in PatternRedactor and the redaction
text-matrix logic were left intact.

| # | ID | File | Commit |
|---|----|------|--------|
| 1 | P1 redaction full-parse-per-page | `src/engines/PatternRedactor.{h,cpp}`, `src/engines/PdfEditorEngine.cpp` | `e7d6e69` |
| 2 | P4 OCR engine re-init per call | `src/shell/controllers/EditController.{h,cpp}` | `9ebd810` |
| 3 | P9 viewer cache O(n) re-sum per insert | `src/ui/PdfViewerWidget.{h,cpp}` | `6d3377e` |
| 4 | P12 OCR worker reloads open document | `src/shell/controllers/EditController.cpp` | `1625d62` |
| 5 | P8 7-Zip packaging blocks GUI thread | `src/shell/controllers/HomeController.cpp` | `d316ffe` |

### P1 — PatternRedactor parsed the whole PDF once per page per pattern

`PatternRedactor::findMatches(path, pageIndex, pattern)` → `extractCharsWithPositions()`
issued a full `FPDF_LoadDocument()`/`FPDF_CloseDocument()` on every call.
`PdfEditorEngine::applyPatternRedactions` (`PdfEditorEngine.cpp:~1292`) called it
once per page, and BatchMode calls `applyPatternRedactions` once per pattern, so
the full-document parse cost was **O(N_pages × N_patterns)**.

**Change:** added a batch overload `findMatches(path, pages, pattern) →
QHash<int, QList<QRectF>>` (PatternRedactor.cpp:~330) that opens the
`FPDF_DOCUMENT` exactly once and loops pages over the single open handle. The
per-page extraction was refactored into `extractCharsFromOpenDoc(void* doc, …)`
(PatternRedactor.cpp:~117, PDFium type kept behind `void*` so the header stays
backend-agnostic), and the regex/M-3 logic into `matchChars()` so both code paths
share one ReDoS bound. `applyPatternRedactions` now collects all matches in a
single pass (`PdfEditorEngine.cpp:~1292`).

**Cost:** document parse drops from **O(N_pages)** to **O(1)** per
`applyPatternRedactions` call → from **O(N_pages × N_patterns)** to
**O(N_patterns)** full parses across a BatchMode run. The single-page
`findMatches` API is unchanged (still used by RedactMode preview and
TestPatternRedact). **ctest: 40/40** (TestPatternRedact, TestRedaction,
TestBatchMode all Passed).

### P4 — OCR engines re-initialised (3 ONNX sessions + Tesseract) every run

`EditController::runOcr` (`EditController.cpp:~421,425`) built a fresh `OcrEngine`
and `RapidOcrEngine` per OCR action. `RapidOcrEngine::initialize` constructs three
ONNX sessions (det/cls/rec) from disk and `OcrEngine::initialize` spins up the
Tesseract API; because the instance was thrown away each call, each engine's own
"already initialized" guard (RapidOcrEngine.cpp:54, OcrEngine.cpp:167) never fired.

**Change:** cache one Tesseract + one RapidOCR instance on the controller
(`EditController.h` members `_ocrTesseract`/`_ocrRapid` + tracked langs) and reuse
them across runs; reinit only on a language change. Engine/strategy selection and
the honest-availability checks are unchanged. Access from the OCR worker thread is
race-free because runs are serialised by `_ocrRunning`.

**Cost:** model/session load drops from **once per OCR run** to **once per
process** (per engine). **ctest: 40/40** (TestRapidOcr, TestRover, TestControllers
Passed).

### P9 — viewer page-cache eviction re-summed all bytes per insert

`PdfViewerWidget::renderPage` (`PdfViewerWidget.cpp:~622`) summed every cached
pixmap's bytes on every insert (an **O(n)** re-walk on the UI thread) before the
LRU victim scan.

**Change:** store each entry's byte size on `CachedPage` and maintain a running
`m_cacheTotalBytes`, updated on insert / same-page replace / evict / clear. The
per-eviction linear scan for the LRU victim is kept (eviction is rare and removes
few pages). 256 MB budget and LRU policy unchanged.

**Cost:** per-insert accounting drops from **O(n)** to **O(1)** amortised.
**ctest: 40/40.**

### P12 — OCR worker reloaded the already-open document

The OCR worker did a fresh `QPdfDocument::load(filePath)` + `render()`
(`EditController.cpp:~406`) although the viewer already had the document loaded
and the page rendered/cached.

**Change:** render the page on the GUI thread via `viewer->renderPage(page, 2.0)`
(reusing the viewer's `QPdfDocument` and render cache) and pass the `QImage` into
the worker, which now only runs OCR. `renderPage(page, 2.0)` reproduces the
worker's previous scale (`pageSize * 2.0`) exactly. This also removes a latent
correctness bug: `QPdfDocument` is not thread-safe, so rendering it on the worker
thread (separate from its owning GUI thread) was unsafe.

**Cost:** eliminates a redundant full-document parse + render per OCR run and
benefits from the viewer cache. **ctest: 40/40.**

### P8 — 7-Zip packaging blocked the GUI thread

`HomeController::createEncryptedPackage` (`HomeController.cpp:~377`) called
`proc.waitForFinished(-1)` on the GUI thread, freezing the event loop for the
entire 7-Zip run.

**Change:** move the `QProcess` into a `QtConcurrent::run` worker and report via
`QFutureWatcher` + `QProgressDialog` — the exact pattern the Office/Images-to-PDF
converters in this file already use (`HomeController.cpp:~576,632`). Only a small
result struct crosses back to the GUI thread; the success/failure dialogs are
unchanged.

**Cost:** GUI stays responsive (with a modal progress dialog) during packaging.
**ctest: 40/40.**

### Deferred (not attempted — reported for focused follow-up)

- **P2 — thumbnails render on the UI thread during scroll** (`ThumbnailSidebar.cpp:~313`,
  `m_renderCache->getOrRender(...)` inside `createThumbnailWidget`).
  **Blocker:** `RenderCache` has no internal locking (verified — no mutex in
  `RenderCache.{h,cpp}`) and the renderer is a GUI-thread object, so it cannot be
  called from a worker as-is. A correct offload needs (a) making RenderCache
  thread-safe, and (b) placeholder-then-swap via a queued signal keyed by page
  index, guarded against the virtualized widgets being recycled/destroyed before
  the render lands (stale render must not write to a reused widget). That is a
  correctness-sensitive change needing new threading tests — out of the
  smallest-correct-diff, stay-green envelope for this pass.
- **P7 — print-preview render loop on the GUI thread** (`HomeController.cpp:~459-471`).
  **Blocker:** `QPrintPreviewDialog::paintRequested` requires painting onto the
  `QPrinter` synchronously inside the signal handler; `QPainter`-on-`QPrinter`
  cannot move to a worker. Only the `doc->render()` calls are offloadable, and
  only by pre-rendering all pages before the modal `preview.exec()` and caching
  them — a larger restructure with its own lifetime concerns and marginal benefit.
  Left for a focused pass.
- **P3 — async document open.** As instructed, not attempted: it is a larger
  load-flow change and is noted here for a dedicated follow-up.

**Verification:** `cmake --build build` clean after every fix. `ctest -j4` →
**40/40** after each of the five commits (TestBatchMode parallel-repeat flake
passes single-pass: `ctest -R TestBatchMode` → 1/1). CMakeLists release flags and
`.github/` were not touched (devops owns LTO); the PatternRedactor M-3 ReDoS cap
and the redaction text-matrix logic were preserved. No `main` push performed.

---

## DevOps/CI fixes applied

Branch: `main` (commits local-only, no push). Build: `cmake --build build`. Suite
after changes: `QT_QPA_PLATFORM=offscreen ctest -j4` → **100% / 40 green** (same
count as before — the fuzz guard is OFF by default and adds zero test targets).

| # | Fix | Files | Commit |
|---|-----|-------|--------|
| 1 | P5 — LTO enabled in release CI | `.github/workflows/release.yml` line ~128 | see below |
| 2 | Fuzz CI leg — copy workflow + CMakeLists guard | `.github/workflows/glyphpdf-fuzz.yml` (new), `CMakeLists.txt` | see below |
| 3 | Release gate coherence verified | `.github/workflows/release.yml` | no change needed |

### 1. P5 — LTO never enabled in the shipped binary

**File/line:** `.github/workflows/release.yml`, the "Configure" step (~line 128).

**What changed:** added `-DGLYPHPDF_ENABLE_LTO=ON` to the `cmake -B build -G Ninja
...` invocation. The root `CMakeLists.txt` at line 76 already declares
`option(GLYPHPDF_ENABLE_LTO ... OFF)` with the comment "release CI sets it ON", and
lines 79-80 wire `-flto=auto` into both `add_compile_options` and `add_link_options`
when the flag is true and the build type is not Debug. The release YAML configure
step previously passed only `-DCMAKE_BUILD_TYPE=Release -DGLYPHPDF_RELEASE_BUILD=ON`
— the `GLYPHPDF_ENABLE_LTO=ON` flag was simply never passed, so the published MSI
was not LTO-optimised despite the comment claiming otherwise. The local dev default
remains OFF for fast iteration builds. LTO is gated to non-Debug configurations by
`$<$<NOT:$<CONFIG:Debug>>:-flto=auto>` generator expressions in CMakeLists.txt, so
this change cannot regress debug builds.

**Local build effect:** zero — the normal `cmake --build build` directory already
has `GLYPHPDF_ENABLE_LTO=OFF` (it was configured without `-DGLYPHPDF_ENABLE_LTO=ON`).
Re-running configure locally does not flip it unless explicitly passed. ctest: 40/40.

### 2. Fuzz CI leg — workflow copy + CMakeLists guard

**Files changed:**
- `CMakeLists.txt` — inserted `option(GLYPHPDF_FUZZ ...)` + guarded
  `add_subdirectory(fuzz)` block immediately before the Translations section
  (after the last test target block). Default is `OFF`.
- `.github/workflows/glyphpdf-fuzz.yml` — copied verbatim from
  `fuzz/ci/clusterfuzzlite.yml` (the fuzz-harness agent wrote it there for the
  owner to copy; it was not previously wired into GitHub Actions).

**Why the guard is safe for the normal build:** `GLYPHPDF_FUZZ` defaults `OFF`, so
the `add_subdirectory(fuzz)` line is never reached during a plain `cmake -B build`
or the release gate configure. The fuzz `CMakeLists.txt` itself guards
`redaction_driver` on `if(TARGET pdfws_engines)` and `djot_fuzzer` on
`if(GLYPHPDF_FUZZ_LIBFUZZER AND ...)`, so even if someone accidentally sets only
`-DGLYPHPDF_FUZZ=ON` without a clang toolchain, the libFuzzer target is silently
skipped. A separate build directory (`fuzz/build/`) is required when
`GLYPHPDF_FUZZ=ON` to prevent sanitizer flags from contaminating the test binaries.

**Clang/libFuzzer availability:** per `fuzz/MANIFEST.md`, `clang` and `compiler-rt`
are absent from the local MSYS2 ucrt64 toolchain. The `clusterfuzzlite.yml` workflow
handles this correctly:
- `redaction-oracles` job: runs on `windows-latest` with `msys2/ucrt64` + `gcc`
  only — no clang required. The redaction driver is a plain g++ build.
- `djot-libfuzzer` job: runs on `ubuntu-latest`, installs `clang` via `apt-get`.
  The build step is `bash fuzz/build_clang/build_djot_clang.sh || echo "scaffolded;
  see MANIFEST"` — the `|| echo` ensures the step does not hard-fail if the clang
  recipe is still scaffolded. The fuzz run step is guarded by `if [ -x
  fuzz/bin/djot_fuzzer ]`, so a scaffolded (non-built) harness produces a no-op
  rather than a broken required check.
- Both jobs trigger on `pull_request` (paths-filtered to `src/engines/podofo/**`,
  `src/pdfws_djot/**`, `fuzz/**`), nightly `schedule`, and manual
  `workflow_dispatch` — never a blocking required check on unrelated PRs.

**YAML validation:** both `.github/workflows/release.yml` and
`.github/workflows/glyphpdf-fuzz.yml` validated with `python3 -c "import yaml;
yaml.safe_load(open(...))"` — both parse without error.

**Local ctest after CMakeLists change:** `cmake --build build --parallel` rebuilt
cleanly (the `option(GLYPHPDF_FUZZ)` line adds one option and the `if(GLYPHPDF_FUZZ)`
block is never entered — zero new targets, zero new build steps). `QT_QPA_PLATFORM=offscreen
ctest -j4` → **100% tests passed, 0 tests failed out of 40** (12.70 s wall time).
Test count is unchanged: the fuzz guard adds nothing to the CTest registry when
`GLYPHPDF_FUZZ=OFF`.

### 3. Release gate coherence — verified, no change needed

Confirmed the following by reading `.github/workflows/release.yml` in full:

- The "Test" step (last step, ~line 151) runs `QT_QPA_PLATFORM=offscreen ctest
  --output-on-failure -j4` from `build/`. This covers all 40 registered tests
  including the new `TestMenuBarIntegrity` (added in the UI-wiring session) and
  any security regression tests registered in `CMakeLists.txt`. No manual
  exclusion list exists, so every test that is present in the build is gated.
- `GLYPHPDF_RELEASE_BUILD=ON` is passed at configure time (line 129), which
  triggers the AR-11 D5 hard-fail block if any of HAS_PDFIUM / HAS_TESSERACT /
  HAS_RAPIDOCR / HAS_QPDF is absent.
- The "Assert GLYPH_TESTING not defined" step checks `CMakeOutput.log` to ensure
  the OCSP test-fixture bypass (`GLYPH_TESTING`) is not present in the release
  binary (only `TestUpdateChecker` and `TestSignatureRealCrypto` define it, and
  only when `GLYPHPDF_ENABLE_TEST_FIXTURES=ON`, which is never passed in the
  release configure step).
- `TestMenuBarIntegrity` and all other new tests require no special flags beyond
  `QT_QPA_PLATFORM=offscreen` and `ctest` invocation — they are covered by the
  existing Test step without any release.yml change.

No changes were made to the release gate. It is coherent as-is, with P5 (LTO) being
the only release.yml modification in this session.

**All three fixes committed atomically. Local build: green. No push performed.**
