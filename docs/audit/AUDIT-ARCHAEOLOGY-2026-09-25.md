# CODEBASE ARCHAEOLOGY — 2026-09-25 (Wave 1, code-archaeologist)

**Status:** COMPLETE
**Scope:** entire `src/` tree (379 files, ~98.9k LOC) plus build system, `third_party/`, `tests/`
on branch `audit/sweep-all` @ `7eb5c67b`.
**Method:** deterministic-first mapping (file inventory → include matrix → targeted grep → manual
verification of every candidate). Every claim below is backed by a captured command output;
candidates that failed verification (5 "dead" classes, 130+ "orphan" headers) were re-checked and
REJECTED rather than reported. Chesterton's Fence applied throughout — nothing deleted, nothing
assumed.

**Severity scale:** HIGH (actively blocks evolution / silent-failure risk) ·
MEDIUM (structural debt, should fix) · LOW (consistency, hygiene) ·
INFO (observation or positive finding).

**Rules honored:** AUDIT-ONLY. No production code changed. This document is the only artifact.

---

## Architecture Map

```
                      ┌────────────────────────────┐
                      │  app/ (Bootstrapper, main) │
                      └─────────────┬──────────────┘
                      ┌─────────────▼──────────────┐
                      │  shell/ (GpMainWindow,     │
                      │  controllers/, MenuBar,    │
                      │  EditPolicy*, StatusBar)   │
                      └─────────────┬──────────────┘
        ┌───────────────┬───────────▼───────────────┐
        │ modes/ (Batch,│  ui/ (PdfViewerWidget,    │
        │ Pages, OCR,   │  AnnotationLayer, panels) │
        │ Compare, …)   └───────────┬───────────────┘
        │                           │
        └───────────┬───────────────┘
        ┌───────────▼───────────────────────────────┐
        │ engines/ (PdfEditorEngine, Signature-     │
        │  Manager, FormManager, Conversion-        │
        │  Manager, OcrEngine, Accessibility, …)    │
        │   ├── podofo/ (PoDoFoBackend 6.7k!)       │
        │   ├── pdfium/ (render/search)             │
        │   ├── qpdf/, ocr/, mrc/, formjs/, ai/,    │
        │   │   conversion/, scheduling/            │
        └───────────┬───────────────────────────────┘
        ┌───────────▼──────────────┐  ┌──────────────┐
        │ commands/ (QUndoCommand  │  │ docmodel/    │
        │ + helpers, header-only)  │  │ (semantic    │
        └───────────┬──────────────┘  │ djot model)  │
        ┌───────────▼──────────────┐  └──────────────┘
        │ core/ (interfaces/I*.h,  │  ┌──────────────┐
        │ Capability, BatchPreset, │  │ pdfws_djot/  │
        │ RedactionProof, …)       │  │ (Lua codec)  │
        └──────────────────────────┘  └──────────────┘
  third_party: pdfium 150.0.7834 · podofo 1.1.x · lua 5.4.7 ·
               verapdf 1.30.2 (+JRE) · jbig2enc · djot · onnxruntime (local)
```

\* EditPolicy physically lives in `shell/` but is consumed by `commands/` and `modes/` (see C-05).

Note on compile reality: `pdfws_core` is an INTERFACE library (headers only). Every
`src/core/*.cpp` is compiled as the **first members of `pdfws_engines`**
(CMakeLists.txt:376-379). The layering between core and engines therefore exists only by
convention — the compiler enforces nothing (context for C-01).

---

## Findings

### 1 · Dead code

**A-01 · INFO · no orphaned headers — verified clean.**
All 158+ headers have at least one non-self includer in `src/`. An initial pass reported 130
"orphans"; the pattern required a quote/angle-bracket immediately before the basename while the
codebase uses directory-prefixed includes (`core/Capability.h`). Corrected pattern
(`<basename>"` or `<basename>>`) reports **zero** orphans, and a stricter self-only pass (header
included solely by its own .cpp) also reports **zero**. This is unusually disciplined header
hygiene for a 99k-LOC tree.

**A-02 · INFO · no commented-out code, no `#if 0` blocks.**
`#if 0` count in src/: 0. Commented-out-code heuristic (`^\s*//\s*(if|for|return|void|class|emit|connect…)`)
flags at most 9 lines in `src/ui/PdfViewerWidget.cpp` — inspection shows they are explanatory
comment fragments, not disabled code.

**A-03 · MEDIUM · three interfaces declared but never consumed — the ports are aspirational.**
- `src/core/interfaces/IPdfDocument.h` — only includer: `src/engines/podofo/PoDoFoBackend.h`
- `src/core/interfaces/IPdfSearcher.h` — only includer: `src/engines/pdfium/PdfiumBackend.h`
- `src/core/interfaces/IPdfWriter.h` — only includer: `src/engines/podofo/PoDoFoBackend.h`

No client code depends on these abstractions; each is implemented by exactly one backend and
referenced by nobody else (grep for basename across `src/ tests/ tools/ fuzz/`). By contrast
`IPdfEditorEngine.h` has 42 consumers and `ISignatureManager.h`/`IToolController.h` ~11-12 each.
The three orphan ports are either (a) dead weight to delete, or (b) scaffolding for a planned
multi-backend swap — the ledger does not say which. Decide and record before anyone "cleans" them.
Also thin: `IConversionEngine.h` (3 consumers).

**A-04 · LOW · `pdfws_engines` links `Qt6::Widgets` PRIVATE with zero QtWidgets usage.**
CMakeLists.txt:465 links `Qt6::Widgets` into `pdfws_engines`; `grep -rn "QtWidgets|QFileDialog|
QMessageBox|QWidget|QApplication|QInputDialog" src/engines/` returns **0** hits. The link is very
likely removable (engine layer stays on Core/Gui/Network/Concurrent). Verify with a link-time
build before removing — Chesterton's Fence; transitive toolchain requirements can hide here.

**A-05 · INFO · five suspect classes verified ALIVE (negative finding, recorded to stop re-litigation).**
`BackendRouter`, `TextMatchFinder`, `MyersDiff`, `ReviewSummaryWriter`, `HeadingOutlineDetector`
each showed 0 hits under a `\b` grep that ugrep mishandles; direct greps confirm all five are
used (e.g. `BackendRouter` ← PdfEditorEngine.cpp, PagesMode.h, SetOutlineCommand.h;
`MyersDiff` ← DiffEngine + tests/TestDiffEngine.cpp). Methodology note: on this machine
`grep` is ugrep and `\b` is unreliable — use fixed-string greps for dead-code sweeps.

### 2 · God files (decomposition candidates)

**B-01 · HIGH · `src/engines/podofo/PoDoFoBackend.cpp` — 6,756 lines, 67 method definitions, ≥20 named sections.**
Largest file in the tree by 1.9×. Internal section banners map a natural decomposition that is
already sitting in the file: annotation round-trip (§9.3, lines ~5314-5825), find & replace
excision core (T2-2, ~2598, ~3121), clickable links (§9.1, ~5825), watermarking (Session 13,
~5919), optimization (Session 13, ~6197), outlines (T2-9, ~5315), measurement dictionaries (T1,
~5081), image stacking/opacity (Wave 2B/2C, ~4682), repair (E-1, ~3255), review-state mapping
(M6-P5 D2, ~4870). Splitting into per-concern TUs (PoDoFoAnnotations.cpp, PoDoFoWatermark.cpp,
PoDoFoOptimize.cpp, …) is mechanical: one class, same TU, no header changes required.

**B-02 · HIGH · six more files exceed 2,000 lines.**
`src/modes/BatchMode.cpp` 3,576 (+ `BatchMode.h` 549 — interface bloat), `src/engines/SignatureManager.cpp`
3,153, `src/engines/AccessibilityTagger.cpp` 2,483, `src/engines/PdfEditorEngine.cpp` 2,083,
`src/engines/FormManager.cpp` 2,016, `src/GpMainWindow.cpp` 1,968.

**B-03 · MEDIUM · 46 files exceed 500 lines** (full `wc -l` sweep). Beyond the above:
PagesMode.cpp 1,863, RedactionProof.cpp 1,827, PdfViewerWidget.cpp 1,783, EditController.cpp
1,634, ConversionManager.cpp 1,509, OCRMode.cpp 1,413, InspectorWidget.cpp 1,389,
SecurityController.cpp 1,167, AnnotationLayer.cpp 1,146, BatchPreset.cpp 1,111,
CommentsWidget.cpp 1,067. Aggregate: 98,889 LOC across 379 files (mean 261).

### 3 · Module boundary violations

**C-01 · HIGH · core → engines: 6 files, 11 upward includes.**
- `src/core/Capability.cpp:5-7` → ConversionManager.h, VeraPdfValidator.h, RapidOcrEngine.h
- `src/core/ErrorInfo.cpp:4` → ConversionManager.h
- `src/core/RedactionProof.cpp:16` → engines/pdfium/PdfiumBackend.h
- `src/core/BatchPreset.cpp:6-7` → PatternRedactor.h, SafeSave.h
- `src/core/PageLabels.cpp:10` → SafeSave.h
- `src/core/SigningRequestRunner.cpp:4-6` → SignatureFieldCreator.h, SignatureManager.h, SafeSave.h

`pdfws_core` is the bottom INTERFACE layer (links only Qt6::Core/Gui, CMakeLists.txt:357-364) yet
six of its files reach the top of the engine stack. It compiles only because core .cpps ride
inside `pdfws_engines` (see Architecture Map note). The dependency arrow is inverted: capability
probing, error normalization, and save-idiom usage belong behind small interfaces in
`core/interfaces/` (the I*.h pattern this codebase already trusts elsewhere), with engines
registering implementations at boot.

**C-02 · MEDIUM · core → ui (dialog inversion).**
`src/core/NetworkTouchpoints.cpp:19` includes `ui/OcspConsentDialog.h`. A core-layer module
invoking a Qt dialog directly. Should be a consent callback/`IUserConsent` port supplied by the
shell at boot (the app already has `ISecretStore` as the model for exactly this pattern).

**C-03 · MEDIUM · commands → ui and commands → shell.**
- `src/commands/SetOutlineCommand.h:9` → `ui/PdfViewerWidget.h`
- `src/commands/EditFormFieldCommand.h:9` → `shell/EditPolicy.h`

`pdfws_commands` links only `pdfws_core` + Qt6::Widgets (CMakeLists.txt:946-948), so these two
headers can only be consumed from within pdfws_ui — the commands library's own boundary is
unenforceable and its CMake linkage is fiction for these files.

**C-04 · LOW · ui → shell and ui → modes.**
- `src/ui/PdfViewerWidget.cpp:9` → `shell/StatusBar.h`
- `src/ui/OcrScanCanvas.{h:8,cpp:7}` → `modes/OcrReviewSession.h`, `modes/OcrConfidence.h`

Widget layer reaching the shell and a feature mode. The OcrScanCanvas↔modes coupling is the
strongest of the two (a header-to-header cycle risk if a mode ever includes the canvas back —
currently it does not).

**C-05 · MEDIUM · two shared classes live in the wrong layer.**
- `shell/EditPolicy.h` is included by `commands/` (EditFormFieldCommand.h:9), `modes/`
  (AccessibilityPanel.cpp:5, FormFieldPropertiesPanel.cpp:8, …) and shell itself — it is a domain
  policy, not shell furniture. Belongs in `core/` or `commands/`.
- `shell/FlowToolbarLayout.h` is the toolbar layout utility consumed by four+ modes
  (CompareMode.cpp:3, FormBuilderMode.cpp:4, …) — belongs in `ui/`.
Their current placement is what *creates* the C-03/C-04 edges; move the files and the violations
disappear without code changes.

**C-06 · INFO · engines → docmodel is a declared, intentional edge.**
`target_link_libraries(pdfws_engines PRIVATE … docmodel)` (CMakeLists.txt:463-467);
consumers are the OCR→djot pipeline (`OcrDjotMapper`, `OcrPipeline`). Not a violation — recorded
so a future sweep does not re-flag it.

### 4 · Dependency health

**D-01 · INFO · pdfium provenance is exemplary.**
`third_party/pdfium/PROVENANCE.md`: bblanchon prebuilt win-x64, chromium/7834, version
150.0.7834.0, SHA-256 recorded for both `.dll` and import lib, byte-identity verified 2026-06-11.
The DLL is **not** committed; CI downloads and hash-verifies (`.github/workflows/`). This is the
pattern every vendored binary should follow.

**D-02 · INFO · vendored source versions.**
Lua 5.4.7 (`third_party/lua-5.4/src/lua.h:19-21`) — current 5.4.x patch line; low risk.
PoDoFo 1.1.x (`third_party/podofo/install/include/podofo/auxiliary/podofo_config.h:6-7`) —
prebuilt under `install/`, no PROVENANCE.md (unlike pdfium) — version pin and hash should be
recorded the same way (LOW). jbig2enc and djot vendored with their own licenses; see
`LICENSE-3RD-PARTY.md`.

**D-03 · INFO · veraPDF 1.30.2 + bundled JRE is license-safe aggregation, documented.**
`third_party/verapdf/LICENSE.txt` states subprocess-only invocation, never in-process; GlyphPDF
remains Apache-2.0. Correct approach.

**D-04 · LOW · onnxruntime 1.17.3 lives at repo root, local-only.**
`onnxruntime-win-x64-1.17.3/` is gitignored (0 tracked files) — like the pdfium DLL it is a local
runtime, not a repo artifact. 1.17.3 (2024) is two-plus years old in Sept 2026; whether an upgrade
matters depends on the OCR model requirements — flagging for the dependency-health ledger, not a
defect. Root placement (vs `third_party/`) is inconsistent with the pdfium convention.

### 5 · Build system health

**E-01 · HIGH · root `CMakeLists.txt` is a 6,173-line / 306KB monolith.**
Exactly one CMake file defines: 4 libraries, the app, **199 `add_executable` occurrences**
(2 top-level + ~197 test targets), 190 `add_test` registrations, the CX-14 logger override, and
the R8 temp-roots block. ~4,900 lines (≈80%, from ~line 1230 to 6158) are per-test blocks.

**E-02 · HIGH · ~190 hand-rolled, copy-pasted test-target blocks.**
Each block repeats the same 6-call shape, e.g. CMakeLists.txt:1237-1257
(`if(EXISTS)` → `add_executable` → `target_include_directories` → `target_link_libraries` →
`add_test` → `set_tests_properties`). Divergence already exists between blocks (different link
sets, some embedding extra sources like `src/engines/DocumentSession.cpp` at line ~1251). A single
`function(glyphpdf_test name …)` with keyword options (LINK, SOURCES, LABELS, TIMEOUT) would
delete thousands of lines and make policy changes (e.g. the CX-14 capture) one-line diffs instead
of 190-place edits.

**E-03 · MEDIUM · `if(EXISTS …)` guards make silent test dropout possible.**
228 `if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/…")` guards: if a test file is renamed or
moved, CMake **silently skips** the target — no error, no `ctest` entry, no CI signal. (Verified:
all 201 current test .cpp files ARE referenced, so nothing is dropped today.) The guard was
evidently added for partial checkouts; the cost is that the guard defeats the guarantee it
protects. Prefer `if(NOT EXISTS …) message(FATAL_ERROR …) endif()` or drop guards under a
`glyphpdf_test()` function that asserts file existence.

**E-04 · INFO · the good parts — keep these.**
Per-test temp roots (R8, CMakeLists.txt:6158-6173) give every test its own TMP/TEMP/TMPDIR — this
is why parallel ctest is trustworthy here. The CX-14 per-test capture override
(:1214-1235) is clever but shadows a CMake builtin command conditionally (`function(add_test)`
inside an `if()`) — fragile under CMake policy changes; fold it into the future
`glyphpdf_test()` function rather than keeping the shadow. Staging (`stage_runtime_dlls`),
Release strip (AR-11 D4, :1153-1160), and PCH for engines/ui (:1170-1187) are all sound.

### 6 · Code duplication

**F-01 · LOW · sha256-hex helper duplicated 3×.**
Near-identical 1-2 line wrappers: `src/core/SigningRequestRunner.cpp:40`
(`hash.result().toHex()`), `src/core/RedactionProof.cpp:51`
(`QString::fromLatin1(hash.result().toHex())`), `src/engines/SignatureManager.cpp:88`
(`data.toHex().toUpper()`). One shared `gp::sha256Hex()` in core would do. Trivial, fix in passing.

**F-02 · INFO · no algorithm re-implementation found — verified clean.**
base64 and hex are Qt built-ins everywhere they appear (EncryptedFileSecretStore.cpp:448/485,
AnnotationSerializer.cpp:53/128, SigningRequestRunner.cpp:40, SignatureManager.cpp:88/986 —
`toBase64/fromBase64/toHex`, no hand-rolled loops). Exactly one diff algorithm
(MyersDiff, used by DiffEngine; no competing impl). One temp-file naming idiom shared via
SafeSave/TempFileManager rather than re-invented per call site.

**F-03 · MEDIUM · the largest duplication in the repo is the CMake test blocks themselves**
(≈190 × ~25 lines — see E-02). It dwarfs all source-level duplication combined.

### 7 · Technical debt markers

**G-01 · INFO · near-zero TODO/FIXME/HACK debt — remarkable for 99k LOC.**
Raw grep says 16 hits, but 14 are false positives (`XXXXXX` inside `QTemporaryFile`/
`QTemporaryDir` name templates, e.g. TempFileManager.cpp:172/191/291, SafeSave.cpp:80,
BatchMode.cpp:1345, PoDoFoBackend.cpp:3997, PdfViewerWidget.cpp:1579, HeadingOutlineDetector.cpp:15
comment about PCRE2 `\uXXXX`). Genuine markers: **one** —
`src/engines/podofo/PdfEncryptPubSec.cpp:23` `TODO(WP-7)` tracking an upstream PoDoFo macro.
Nothing else.

**G-02 · INFO · decisions are encoded as comment ledger tags, not debt markers.**
The dominant inline convention is `R16`, `N2`, `T2-2`, `M6-P5 D2`, `§1.7`,
`PROGRAM-CONSOLIDATION-2026-09-25` (examples: GpMainWindow.cpp:46-48, PoDoFoBackend.cpp:2598,
BatchPreset.h:164). These reference ledgers under `docs/audit/` and `docs/planning/`. This is a
strength **only while** every tag remains resolvable; tags whose ledger rows are lost become
noise. `docs/SPEC-TRACEABILITY.md` exists and should be the canonical tag→doc index.

### 8 · Architecture assessment

**H-01 · Strengths.**
(1) Interface-first core: 10 `I*.h` ports, top ones heavily adopted (IPdfEditorEngine.h: 42
consumers). (2) Backend routing (BackendRouter) instead of scattered engine if-else. (3) Header
hygiene: zero orphan headers, zero commented-out code, near-zero TODO debt (A-01/02, G-01).
(4) Provenance discipline for vendored binaries (D-01). (5) Test infrastructure engineering:
per-test temp roots, per-test capture, 190 registered suites for 99k LOC (~1 suite per 520 LOC).
(6) Single diff algorithm, single save-commit idiom (SafeSave) shared by callers.

**H-02 · Weaknesses (the debt that actually compounds).**
(1) The core layer's independence is fiction — it compiles inside engines and reaches upward in
6 files (C-01); every new engine feature erodes it further. (2) Engine-layer god files
(B-01/B-02) concentrate the highest-risk logic (signing, redaction, content excision) in TUs too
large to review effectively. (3) The build is one 6k-line file whose test section is copy-paste
(E-01/02) with silent-dropout guards (E-03). (4) Two misplacement hot-spots (EditPolicy,
FlowToolbarLayout) generate most boundary edges (C-05). None of these are big-bang fixes; all
four have Strangler-Fig paths.

---

## Top-10 priority list

| # | ID | Sev | Item | Effort |
|---|----|-----|------|--------|
| 1 | E-01/02 | HIGH | Decompose 6,173-line CMakeLists: introduce `glyphpdf_test()` function; collapses ~4,900 lines | Medium (mechanical) |
| 2 | B-01 | HIGH | Split PoDoFoBackend.cpp (6,756 lines) into per-concern TUs along its own section banners | Medium |
| 3 | C-01 | HIGH | Invert 6 core→engines includes behind `core/interfaces/` ports | Medium |
| 4 | E-03 | MED | Replace 228 `if(EXISTS)` guards with fail-loud existence checks | Small |
| 5 | C-05 | MED | Move EditPolicy.h → core/ (or commands/), FlowToolbarLayout.h → ui/ | Small |
| 6 | C-02 | MED | Extract IOcspConsent port; NetworkTouchpoints must not open dialogs | Small |
| 7 | C-03 | MED | Decouple SetOutlineCommand from PdfViewerWidget; EditFormFieldCommand from shell | Small |
| 8 | A-03 | MED | Decide fate of IPdfDocument/IPdfSearcher/IPdfWriter (adopt or delete); record in ledger | Small |
| 9 | B-02 | MED | BatchMode.cpp (3.6k) and SignatureManager.cpp (3.2k) decomposition plans | Large |
| 10 | A-04 | LOW | Drop Qt6::Widgets from pdfws_engines after link-verify | Trivial |

## Safe refactoring plan (Strangler Fig order)

1. Characterization is already in place — 190 registered test suites; confirm `ctest` green at
   `7eb5c67b` before any move.
2. Build system first (E-01→E-03): the `glyphpdf_test()` function is behavior-preserving by
   construction and unlocks cheap policy changes; diff `ctest -N` output before/after (must be
   identical list of 190).
3. Move-file fixes (C-05): pure `git mv` + include-path updates; C-03/C-04 edges disappear.
4. Interface inversions (C-01/C-02): introduce ports one at a time, engines register at
   Bootstrapper; each port ships with its own sweep showing the include removed.
5. God-file splits (B-01/B-02) last: mechanical TU splits per section banner, no header changes,
   one section per commit.

## Residuals and methodology limits

- Function-level dead code: no reachable cases found, but the sweep was class/include-granular;
  a compiler-assisted pass (`-Wunused-function`, clang-analysis, or cross-referencing every
  public method) was out of scope for a grep-based audit. UNVERIFIED: exhaustive dead-method census.
- The pre-existing uncommitted change to `tests/TestSignatureRealCrypto.cpp` in the worktree is
  **not mine** and was left untouched/uncommitted.
- PoDoFo patch version digit and per-dependency CVE sweeps belong to the security wave
  (`AUDIT-SECURITY-2026-09-25.md`); only provenance/version pinning was checked here.
- Verification gate: every count above was produced by a captured command run in this session;
  rejected hypotheses (130 orphan headers, 5 dead classes) are recorded in A-01/A-05 rather than
  silently dropped.
