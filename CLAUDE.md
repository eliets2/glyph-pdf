# GlyphPDF — Claude Code Project Memory

This file auto-loads in every Claude Code session opened in `C:\Users\User\Projects\pdf`. **Read it. Trust it. Update it when state changes.** Sessions that re-derive context from scratch waste tokens.

---

## 0. Quick orient

**What:** Open-source native Windows desktop PDF workstation. C++17 + Qt 6.11. Privacy-first positioning: PAdES B-LTA local signing + Edact-Ray-defended redaction + no cloud + no AI sending docs to a server + no subscription.

**Status:** Internal `[1.0.0-internal]` build exists at `dist\GlyphPDF-1.0.0-x64.msi` (May 27, 2026). **NOT for public distribution.** Real public v1.0.0 ships after **Branch C SCOPE LOCK** execution completes (~8-9 months from M2 start, M8 target launch).

**License (decided):** Apache-2.0 (or MIT — finalized at M8). Donations optional via GitHub Sponsors / OpenCollective. No commercial tier. No telemetry. No CLA.

**Build environment (current):** MSYS2 **ucrt64** native — GCC 16.1.0, Qt 6.11.0, CMake 4.3.3, PoDoFo 1.1.0 (vendored). NOT mingw64; NOT Qt installer; NOT vcpkg.

**Repo state:** branch `main`, head `0d9fdc9` (2026-05-30 — M5-P3 D7 walkthrough). M1 `a6ea6aa`; MSYS2 `45807de`–`9ac0c2f`; M2 `42c0f46`–`c3eb22a`; M3 `faac7f2`–`5bc2fbe`; M4+catchup `8bb8f95`–`d54f4a1`; M5-P3 `09b0cfc`–`0d9fdc9`. See vault `01-current-state.md` for commit-by-commit map.

**Tests:** 24 ctest targets. All should pass under MSYS2 ucrt64 build (verify with `ctest --output-on-failure -j4 --repeat-until-fail 3`). TestOfficeImport: 5 tests (3 active without LibreOffice; 2 QSKIP when soffice absent). TestDjotRoundtrip: 7 tests (encode stub documented, ProvenanceGuard 4 cases). TestPatternRedact: 11 tests (PDFium-gated, pass without PDFium). TestBatchMode: RESOURCE_LOCK added to prevent parallel-run race.

---

## 1. Where to read for more

| Topic | Primary source | Backup |
|---|---|---|
| Comprehensive audit + 11 release-blockers + decision tree + positioning | `docs/planning/AUDIT-v1.0.0.md` | Obsidian `projects/glyphpdf/00-overview.md` |
| All 34 launch prompts for M2-M8 (Branch C scope) | `docs/planning/MONTHS-2-8-PROMPTS.md` | Obsidian `projects/glyphpdf/05-prompts-index.md` |
| MSYS2 migration prompt (already executed) | `docs/planning/MSYS2-MIGRATION.md` | Obsidian `projects/glyphpdf/03-build-environment.md` |
| Month 1 remediation prompt (already executed) | `docs/planning/M1-REMEDIATION.md` | Obsidian `projects/glyphpdf/07-sessions-log.md` |
| Antigravity session history (9 sessions reconstructed) | Obsidian `projects/glyphpdf/07-sessions-log.md` | `.gemini/antigravity/brain/<uuid>/` |
| Lessons learned (pattern-categorized) | Obsidian `projects/glyphpdf/08-lessons-learned.md` | — |
| Architectural non-negotiables | This file §6 | Obsidian `projects/glyphpdf/06-non-negotiables.md` |
| PRD (product requirements) | `PRD.md` | — |
| Engineering roadmap (Sessions 1-20 + WS1 + WS2 + WS3) | `ROADMAP.md` | — |
| Version history + INTERNAL-BUILD marker | `CHANGELOG.md` | — |
| Dep license matrix | `LICENSE-3RD-PARTY.md` | — |
| Session brief for next CC session | `SESSION_BRIEF_NEXT.md` | — |
| Public-facing build + features | `README.md` | — |

**Obsidian vault entry:** `C:\Users\User\.claude\memory\projects\glyphpdf\00-overview.md`. The full glyphpdf/ branch contains: 00-overview, 01-current-state, 02-architecture, 03-build-environment, 04-scope-lock, 05-prompts-index, 06-non-negotiables, 07-sessions-log, 08-lessons-learned, 09-license-policy.

---

## 2. Stack + architecture (one-paragraph version)

C++17 / Qt 6.11.0 / MinGW UCRT 16.1.0 (MSYS2). 4 static library layers: `pdfws_core` → `pdfws_engines` → `pdfws_commands` → `pdfws_ui` → `PdfWorkstation` (executable + Bootstrapper). Dependencies via MSYS2 pacman: PoDoFo 1.1.0 (vendored in `third_party/podofo_build/`), qpdf, OpenSSL 3.x, Tesseract 5, Leptonica, libxml2, freetype, zlib, curl, libpng, libjpeg-turbo, libtiff. PDFium + ONNX Runtime are prebuilt vendored binaries.

**Branch-C-committed architecture (per ROADMAP):** dual-model core (Structural PDF object graph owned by PoDoFo/PDFium/qpdf = source of truth ↔ Semantic `docmodel::SemanticDocument` = editing/interchange model; lossless Djot↔Semantic; explicitly lossy Semantic↔PDF; ProvenanceGuard refuses Djot-save-back for born-PDF signed docs). Heterogeneous LaneScheduler (GPU lane warm persistent worker + CPU lane elastic pool + cross-page pipelining). Three workstreams: WS1 (parallel layout + OCR ensemble), WS2 (Djot full document interchange), WS3 (MRC layered compression in PDF/A).

---

## 3. Build + test

### Setup (one-time)
Install MSYS2 at `C:\msys64\`. Then in MSYS2 UCRT64 shell:
```bash
pacman -Syu --noconfirm
pacman -S --noconfirm --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-tools \
    mingw-w64-ucrt-x86_64-qt6-svg \
    mingw-w64-ucrt-x86_64-qt6-pdf \
    mingw-w64-ucrt-x86_64-qt6-translations \
    mingw-w64-ucrt-x86_64-qt6-imageformats \
    mingw-w64-ucrt-x86_64-qpdf \
    mingw-w64-ucrt-x86_64-openssl \
    mingw-w64-ucrt-x86_64-tesseract-ocr \
    mingw-w64-ucrt-x86_64-tesseract-data-eng \
    mingw-w64-ucrt-x86_64-leptonica \
    mingw-w64-ucrt-x86_64-libxml2 \
    mingw-w64-ucrt-x86_64-freetype \
    mingw-w64-ucrt-x86_64-zlib \
    mingw-w64-ucrt-x86_64-curl \
    mingw-w64-ucrt-x86_64-libpng \
    mingw-w64-ucrt-x86_64-libjpeg-turbo \
    mingw-w64-ucrt-x86_64-libtiff
```
PoDoFo 1.1.0 is vendored at `third_party/podofo_build/` (not in MSYS2 packages). PDFium + ONNX Runtime are vendored at `third_party/pdfium/` + `onnxruntime-win-x64-1.17.3/`.

### Build (PowerShell)
```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins'
cd C:\Users\User\Projects\pdf
cmake -B build -G "Ninja"
cmake --build build --parallel 8
```

### Test
```powershell
cd build
ctest --output-on-failure -j4
# Expected: 14/14 pass
```

### Run the app
```powershell
.\PdfWorkstation.exe
```

### Why MSYS2 UCRT64 (not mingw64)?
`qt6-pdf` package is only available in MSYS2 ucrt64 (not mingw64). UCRT (Universal C Runtime) is system-native on Windows 10+. Single coherent toolchain (no Qt-installer + vcpkg + MSYS2 mix causing 0xC0000139 ABI errors).

---

## 4. Repo structure (annotated)

```
C:\Users\User\Projects\pdf\
├── CLAUDE.md                     ← you are here; auto-loads in every CC session
├── README.md                     ← public-facing; OSS-positioned
├── PRD.md                        ← product requirements
├── ROADMAP.md                    ← engineering roadmap (Sessions 1-20 + WS1/WS2/WS3 + Phase 1.5)
├── CHANGELOG.md                  ← version history; [1.0.0-internal] marker; [Unreleased] M2-M8
├── LICENSE-3RD-PARTY.md          ← dep license matrix
├── SESSION_BRIEF_NEXT.md         ← state-of-the-art for next CC session
├── CMakeLists.txt                ← MSYS2 ucrt64 build config; MuPDF + Poppler FATAL_ERROR guards
├── docs/
│   └── planning/                 ← Branch C execution artifacts
│       ├── AUDIT-v1.0.0.md       ← 1,982 lines: 11 blockers + decision tree + appendices A-E
│       ├── M1-REMEDIATION.md     ← Month 1 prompts (already executed)
│       ├── MSYS2-MIGRATION.md    ← MSYS2 migration prompt (already executed)
│       └── MONTHS-2-8-PROMPTS.md ← 34 launch prompts for M2-M8 Branch C work
├── src/
│   ├── app/                      ← main.cpp + Bootstrapper
│   ├── core/                     ← interfaces, AppContext, ToolId, ErrorInfo, TempFileManager,
│   │   │                          CredentialManager, UpdateChecker, AnnotationTypes
│   │   └── interfaces/           ← 12 abstract interfaces (IPdfEditorEngine, ISignatureManager,
│   │                              IFormManager, IOcrEngine, IConversionEngine, ICollaboration,
│   │                              IPdfDocument, IPdfRenderer, IPdfWriter, IPdfSearcher,
│   │                              IPdfPage, IToolController)
│   ├── engines/                  ← real engine implementations
│   │   ├── PdfEditorEngine       ← facade over PoDoFoBackend
│   │   ├── SignatureManager      ← PAdES B-B/B-T/B-LT/B-LTA + cert encryption
│   │   ├── FormManager           ← AcroForm CRUD + FDF/CSV import/export + flatten
│   │   ├── ConversionManager     ← PDF↔Office/Image/HTML/CSV
│   │   ├── OcrEngine             ← Tesseract wrapper
│   │   ├── DiffEngine            ← Myers LCS (M6-PROMPT-1 upgraded from set-difference)
│   │   ├── RenderCache           ← 3-tier LRU, 256 MB cap, tiled rendering
│   │   ├── BackendRouter         ← chooses backend per operation
│   │   ├── CollaborationManager  ← Cloud Sync stub (disabled in UI; v2.0)
│   │   ├── UpdateChecker         ← Session 18 auto-update (manifest + SHA-256 + msiexec)
│   │   ├── AutosaveManager       ← (NEW M1) interval autosave + .autosave.pdf
│   │   ├── DocumentSession       ← path + dirty + lastAutosave state
│   │   ├── ocr/                  ← RapidOcrEngine, OcrPipeline (ROVER), OcrPreprocessor
│   │   ├── podofo/               ← PoDoFoBackend + PdfStringEscape (NEW M1)
│   │   ├── pdfium/               ← PdfiumBackend
│   │   └── qpdf/                 ← QpdfBackend
│   ├── commands/                 ← QUndoCommand subclasses + helper structs
│   ├── shell/                    ← Ribbon, RibbonModel, ToolRegistry, ModeStrip, MenuBar,
│   │   │                          StatusBar, ScreenNav, Sidebar
│   │   └── controllers/          ← 7 tool controllers (Home, View, Edit, Pages, Convert,
│   │                              Forms, Security)
│   ├── modes/                    ← 11 mode panels (OCRMode, PagesMode, BatchMode, CompareMode,
│   │                              FormBuilderMode, AIChatPanel, WatermarkDialog, EncryptionDialog,
│   │                              SignatureDialog, RedactMode, SignaturesPanel,
│   │                              PdfAValidationPanel)
│   ├── ui/                       ← widgets + dialogs (PdfViewerWidget central; ThumbnailSidebar,
│   │                              BookmarkPanel, CommentsWidget, InspectorWidget, FindBar,
│   │                              PreferencesDialog, ErrorDialog, RecoveryDialog (NEW M1),
│   │                              etc.)
│   └── util/                     ← GpTheme, Icons, Badge, Slider
├── tests/                        ← 14 test executables
│   ├── fixtures/
│   │   └── signing/              ← (NEW M1) generate.bat + ca.key + signer.crt + test_signer.p12 + test_input.pdf
│   ├── mocks/                    ← MockPdfEditorEngine, MockSignatureManager, MockFormManager
│   ├── UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation,
│   ├── TestRedaction, TestThreadSafety, TestEncryption, TestResourceLimits, TestControllers,
│   ├── TestIntegration, TestPerformance,
│   └── TestAutosave (NEW M1), TestSignatureRealCrypto (NEW M1)
├── packaging/                    ← WiX v4 MSI installer (GlyphPDF.wxs + build-msi.bat)
├── translations/                 ← glyphpdf_en/ar/fr/de.ts (ar/fr/de are 6-line empty shells;
│                                  M6-PROMPT-2 will populate via lupdate + commission translators)
├── third_party/
│   ├── podofo_build/             ← (NEW MSYS2) vendored PoDoFo 1.1.0 install tree
│   ├── pdfium/                   ← isolated PDFium headers
│   ├── lua-5.4/                  ← (NEW M4) vendored Lua 5.4 (MIT) reference parser
│   └── djot/                     ← (NEW M4) vendored Djot reference parser (Lua files; no .git)
├── src/docmodel/                 ← (NEW M4) SemanticDocument tree + ProvenanceTag
├── src/pdfws_djot/               ← (NEW M4) Djot codec + IDjotMapper + ProvenanceGuard
├── models/                       ← OCR model files (Tesseract data + PP-OCRv5 ONNX)
├── resources/                    ← icons, fonts, QSS themes, .qrc
├── dist/                         ← built MSI artifacts (gitignored)
└── build/                        ← CMake build dir (gitignored)
```

---

## 5. Branch C SCOPE LOCK execution model

**The team has rejected a fast-ship v1.0.0.** Real public v1.0.0 does not ship until every claim in PRD + ROADMAP + marketing copy is implemented. No preview banners. No mock OCR. No canned AI replies. No empty translations. No "scheduled for a future engine update" dialogs.

### Already done (M1 — commit `a6ea6aa`, May 27, 2026)
- B1+B2: Interval autosave timer + crash-recovery dialog (`AutosaveManager` + `RecoveryDialog`)
- B3: SignatureManager DSS/VRI key spec-conformance fix (SHA-1 of raw `/Contents` bytes)
- B4: `CMS_verify` real trust policy (Windows system root + X509_VERIFY_PARAM + EKU + CRL)
- B5: OCSP responses verified with `OCSP_basic_verify` before DSS embedding
- B6: Content-stream injection fix (`pdfEscapeLiteralString` in 5 sites)
- B7: RenderCache TOCTOU race fix (single WriteLockGuard)
- B8: `encryptWithCertificate` real `/Filter /PubSec` output (was UAF + raw CMS envelope)
- B9: CMake Poppler + MuPDF FATAL_ERROR guards (4 detection methods each)
- B10: `qpdf_set_static_ID` removed (trailer /ID randomization now persists)
- B11: RapidOCR `isMockImplementation()` runtime gate in OCRMode

### Already done (MSYS2 migration — commits `45807de` + `6e7c8aa` + `9ac0c2f`, May 28, 2026)
- Migrated from Qt-installer + vcpkg to MSYS2 ucrt64 native (single ABI; eliminates 0xC0000139)
- Upgraded PoDoFo 0.10.3 → 1.1.0 (API adaptations in PoDoFoBackend.cpp + commands)
- Vendored PoDoFo 1.1.0 install tree at `third_party/podofo_build/`
- Updated CMakeLists.txt + README + CHANGELOG to MSYS2 ucrt64

### Already done (M2 sprint — 5/5 prompts, May 29, 2026)
- M2-P1: Edact-Ray glyph-advance normalization (GlyphAdvanceCalculator + numeric TJ)
- M2-P2: OCR invisible text scrub in redaction (Tr==3 tracking in redactCanvasRecursively)
- M2-P3: veraPDF subprocess for PDF/A conformance (VeraPdfValidator + PdfAValidationPanel wired)
- M2-P4: Real-crypto E2E test expansion (adversarial fixtures, RSA key-size enforcement)
- M2-P5: LaneScheduler infrastructure (GPU warm worker + CPU pool + CrossPagePipeline)

### Already done (M3 sprint — 5/5 prompts, May 29-30, 2026)
- M3-P1: FormBuilderMode wired drag-and-drop (field placement + 9 types + tab order)
- M3-P2: BatchMode real execution loop (QtConcurrent + error log + 5 tests; RESOURCE_LOCK added in catchup)
- M3-P3: PagesMode real split-form UI (real page list + split/reorder + TestPagesMode)
- M3-P4: RedactMode + PatternRedactor (12 named patterns + applyPatternRedactions; TestPatternRedact registered in catchup)
- M3-P5: InspectorWidget Properties tab fully bound (all 5 tabs; TestInspector 6 tests)

### Already done (M4 sprint — 6/7 prompts + catchup, May 29-30, 2026)
- M4-P1: View tools (TwoPage, EyeCare, Presentation Mode)
- M4-P2: Pages tools (Resize, HeaderFooter, BatesNumbering — engine methods pre-existing, UI wired)
- M4-P3: Convert tools (ToHtml, ToText, Compress, PDF→PPT structural PPTX export)
- M4-P4: Forms tools (Button, SigField, AutoDetect, TabOrder)
- M4-P5: Security tools (Permissions, RemoveSecurity, Certify, Timestamp, PatternRedact)
- M4-P7: WS2 Djot foundation (docmodel + pdfws_djot + Lua vendor + ProvenanceGuard + TestDjotRoundtrip)
- M4-P6: NOT executed (D1+D2 verified-in-place; D4 prune-recents not implemented; prompt skeletal)

### Already done (M5-P3 — 2026-05-30)
- M5-P3: Office→PDF import (HAS_LIBREOFFICE, real soffice subprocess, tree-kill) + Images→PDF (PoDoFo PdfImage) + HomeController ToolId::ImportOffice/ImagesToPdf + WelcomeWidget cards + TestOfficeImport (24/24)

### Remaining (M2-M8) — 34 prompts in `docs/planning/MONTHS-2-8-PROMPTS.md`

| Month | Sprint | Prompts | Notes |
|---|---|---|---|
| **M2** | Security tier completion + LaneScheduler infrastructure (WS1 Phase 1/2 dep) | 5 | Edact-Ray glyph-advance, OCR text-layer scrub, veraPDF subprocess, real-crypto E2E expansion, LaneScheduler |
| **M3** | Mode-page completions | 5 | FormBuilder, Batch, Pages, Redact + pattern redaction, Inspector Properties |
| **M4** | 23 ribbon tools + **WS2 Djot foundation (Phase 1.5)** | 7 | docmodel + pdfws_djot + ProvenanceGuard + 1000-doc roundtrip test |
| **M5** | OCR ensemble (WS1) + Office import + WS2 OCR→Djot mapping | 4 | RapidOCR real PP-OCRv5, LayoutEnsemble + cross-page pipelining, Office→PDF + PPT export, OcrDjotMapper |
| **M6** | DiffEngine LCS + i18n + AI + WS2 Djot annotation rich text + comments + edge cleanup | 7 | Myers LCS, translations populated, AI backend (Anthropic/OpenAI/Gemini/Ollama), Djot internal model + /RC XHTML + /PieceInfo, comment threading depth |
| **M7** | Hardening + **WS3 MRC compression pipeline** | 3 | Third-party security audit, perf tuning + bug bash, JBIG2 + JPEG2000 + sandwich text in PDF/A-2b |
| **M8** | Launch | 3 | Marketing + screenshots, governance files (LICENSE + CONTRIBUTING + SECURITY + GitHub repo + CI), tag v1.0.0 + sign MSI + winget/chocolatey/scoop + announce |

**Three workstreams (WS1 + WS2 + WS3 all in v1.0.0):**
- **WS1 — Parallel Layout + OCR Ensemble.** M2 LaneScheduler infrastructure → M5 layout detector ensemble (PP-DocLayoutV2 + Surya if license permits) + cross-page pipelining (`layout(P+1) ‖ ocr(P) ‖ fusion(P-1)`) + per-region redo + per-word confidence overlay. Word-level ROVER fusion.
- **WS2 — Djot Full Document Interchange.** M4 Phase 1.5 foundation → M5 OCR→Djot mapping → M6 annotation/comment rich text. Dual-model: Structural ↔ Semantic. ProvenanceGuard refuses Djot-save-back on born-PDF signed. Vendored Lua 5.4 reference parser.
- **WS3 — MRC Layered Compression in PDF/A.** M7. JBIG2 foreground (lossless or symbol-distinct — NEVER pattern-matching) + JPEG2000 background + invisible OCR sandwich text from WS1 word boxes. 5-10× size reduction. **No DjVu output** (legacy).

### How to execute a prompt
1. Open a fresh Claude Code session rooted at this directory.
2. Open `docs/planning/MONTHS-2-8-PROMPTS.md`. Find the prompt for the task (e.g., `M2-PROMPT-1` for Edact-Ray).
3. Copy the entire `<session_metadata>` ... `</success_criteria>` block.
4. Paste as the first message in the new CC session. Do NOT add commentary.
5. The agent executes following the STANDARD EXECUTION PROTOCOL (analyze → plan → implement → verify → context-gate at 50% → final verification + atomic commit).
6. After completion, **run ctest yourself** and verify the test result files refresh (timestamps newer than before run). DO NOT trust the agent's self-reported success — see §7 lessons-learned.
7. Close the session. Start a fresh one for the next prompt. One prompt = one session.

### Dependency-aware execution order (NOT prompt number order)
- **M2:** PROMPT-1 (Edact-Ray) → PROMPT-2/3/4/5 in parallel
- **M3:** all 5 parallel
- **M4:** PROMPT-1 through PROMPT-6 parallel by controller; PROMPT-7 (Djot foundation) parallel — independent new library
- **M5:** PROMPT-1 (RapidOCR real) → PROMPT-2 (LayoutEnsemble — needs M2-PROMPT-5 LaneScheduler) → PROMPT-4 (OCR→Djot mapping — needs M4-PROMPT-7 Djot foundation). PROMPT-3 (Office import) independent.
- **M6:** PROMPT-1/2/3/5/6/7 parallel; PROMPT-4 (Djot annotation rich text) needs M4-PROMPT-7 + M3-PROMPT-5.
- **M7:** PROMPT-3 (MRC) needs M5 outputs + M4-PROMPT-7 Djot. PROMPT-1 (audit) + PROMPT-2 (bug bash) independent.
- **M8:** sequential (D5 governance → D7 release → D9 smoke → D10 announce).

---

## 6. Architectural non-negotiables (enforce every session)

**License contamination protection:**
- **MuPDF (AGPL-3.0): NEVER link in-process** — `CMakeLists.txt:43-45` FATAL_ERROR guard (4 detection methods: target, find_package, pkg-config, target alias)
- **Poppler (GPL-2.0+): NEVER link in-process** — symmetric FATAL_ERROR guard (M1 PROMPT-6)
- **veraPDF (AGPL-3.0): SUBPROCESS ONLY** — never link in-process (M2-PROMPT-3 wires CLI invocation)
- **DjVu: NEVER as output format** — import-only, gated `HAS_DJVU=OFF` default (M7-PROMPT-3 D6)
- **Surya: license-verify before integration** — if GPL-3.0, subprocess or substitute (M5-PROMPT-2 D2)
- **JBIG2 encoder: must be Apache/BSD/MIT** — audit before integration per R6 risk

**Crypto + security:**
- **SHA-256 only for signature hashing** — no MD5 or SHA-1 (except VRI key spec-required SHA-1 per ETSI EN 319 132)
- **All incremental saves must use PoDoFo `WriteUpdate`** when signatures exist (full rewrite invalidates signatures)
- **qpdf in the signing path: NEVER** — qpdf flattens xref, invalidates `/ByteRange`. `PdfEditorEngine.cpp:166-185` guards (skip qpdf when `hasSignatures==true`)
- **Black rectangle redaction: NEVER** — always excise from content stream + Edact-Ray glyph-advance normalization (Session 7 D1 done; M2-PROMPT-1 extends to robust impl)
- **OCSP unverified embedding in DSS: NEVER** — `OCSP_basic_verify` runs before DSS append (M1 B5)
- **CMS_verify without trust policy: NEVER** — Windows system root via `CertOpenSystemStoreA` + X509_VERIFY_PARAM (M1 B4)
- **DSS/VRI key from hex-decoded inner bytes: NEVER** — SHA-1 over raw `/Contents` byte stream per spec (M1 B3)

**Performance + concurrency:**
- **Spawn-per-page ONNX process: NEVER** — use LaneScheduler persistent warm worker (M2-PROMPT-5)
- **Character-level OCR majority vote: NEVER** — word-level ROVER only (OcrPipeline already correct)
- **Layout detector ensemble end-to-end VLM: NEVER for v1.0** — modular pipeline (layout → OCR → fusion) per ROADMAP WS1

**Format integrity:**
- **JBIG2 pattern-matching mode: NEVER** — 2013 Xerox digit-substitution incident; German BSI banned; lossless or symbol-distinct only (M7-PROMPT-3 D3)
- **Djot grammar reimplementation: NEVER** — vendor the Lua 5.4 reference parser (M4-PROMPT-7 D2)
- **Storing raw Djot where PDF spec requires XHTML: NEVER** — transcode on save to /RC XHTML; original Djot in /PieceInfo only (M6-PROMPT-4)
- **Edit-via-Djot-save-back on signed born-PDF: NEVER** — ProvenanceGuard refuses; route to "Save as copy" (M4-PROMPT-7 D3)
- **DjVu as output format: EXCLUDED** — legacy, dead browser support
- **Content-stream operators with raw user strings: NEVER** — always escape via `pdfEscapeLiteralString` (M1 B6 done)

**Build + distribution:**
- **Anything labeled v1.0.0 published before M8 ships: NEVER** — current `dist/GlyphPDF-1.0.0-x64.msi` is internal/private only
- **Telemetry without opt-in: NEVER** — default OFF; clear local toggle in Preferences

---

## 7. Lessons learned (read before doing anything destructive)

These patterns recurred during Antigravity execution (May 19-27) + Claude Code follow-up (May 25-28). Future sessions should not re-make these mistakes.

**Cross-project version (applies to all projects in workspace):** `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 17 patterns total, generalized to be tool-and-stack agnostic.

**GlyphPDF-specific full detail:** Obsidian `projects/glyphpdf/08-lessons-learned.md`.

Below is the abridged version of the patterns most relevant to GlyphPDF Branch C execution. Read these 10; deep-dive in the linked notes for the full 17.

### Pattern 1: Agent self-reports are unreliable — verify on disk
**4 occurrences.** Both Antigravity walkthroughs (consistently report "all tests pass") and Claude Code agents (M1 remediation session reported "100% tests passed" when result files showed 14 pass / 3 fail) have hallucinated success. **Rule:** after any agent claims tests pass, independently check `build/*_results.txt` timestamps (must be newer than start of agent run) AND tail the actual result content. Don't move forward on agent reports alone.

### Pattern 2: Stale test result files = false confidence
**3 occurrences.** `build/*_results.txt` only refresh when ctest writes them. Source edits without `cmake --build` + `ctest` = stale results. **Rule:** the only proof a fresh test pass happened is the result file's `mtime` being newer than the source edits.

### Pattern 3: Scratch file pollution
**~7 files cleaned during M1.** Agents leave `scratch_*.py`, `test_*.txt`, intermediate `.csv` etc. in repo root. **Rule:** `.gitignore` now contains scratch patterns; agents should clean after themselves but verify with `git status` before commit.

### Pattern 4: Forgetting commits after edits
**Worst: 37 files uncommitted at one point.** Agents edit source + write tests + add files without committing — one `git checkout` away from data loss. **Rule:** atomic commits per deliverable. If a prompt has D1-D11, expect 11 commits, not 1.

### Pattern 5: Mock data masquerading as real
**Multiple sites discovered:** RapidOCR returned literal `"RapidOCR_Mock"`, ThumbnailSidebar used fake paper widgets, AIChatPanel canned reply, SignaturesPanel hardcoded "Elie Matta / GlobalSign CA", PdfAValidationPanel 5 fake rules, fake `⤺ SYNCED · v.X` indicator, Sidebar Files writing placeholder text instead of attachment bytes. **Rule:** every UI surface must either be wired to real engine OR disclosed (banner, disabled control, "future release" dialog). The audit-flagged Mock surfaces ALL got runtime gates (M1 B11) or are scheduled for completion in M3-M6.

### Pattern 6: Misleading UI labels
**`AUTOSAVED · HH:MM:SS` was a lie** — label updated only on manual save. PRD §13 promised real interval autosave. **Rule:** UI text must describe actual behavior. Use `setEnabled(false)` + tooltip rather than fake-functional labels.

### Pattern 7: Audit-flagged TODOs that shipped
**`SignatureManager.cpp:323` had `TODO(audit-2026-05-23): remove hex round-trip in VRI key construction` — shipped in v1.0.0-internal MSI.** Self-flagged correctness defects belong in blockers, not in production. **Rule:** before any release tag, grep source for `TODO(audit-*)`, `FIXME`, `XXX`, `HACK` — these must be either closed or explicitly accepted with rationale.

### Pattern 8: DLL ABI mismatch (Qt installer + MSYS2 mix)
**0xC0000139 STATUS_ENTRYPOINT_NOT_FOUND** when UnitTests + TestControllers ran. Root cause: hybrid Qt-installer (Qt MinGW 13.1.0) + vcpkg (MSYS2-style libwinpthread + libstdc++) produced binaries with mixed ABI. The carefully-chosen mix in `build/*.dll` plus `*.qt-bak` backup files was a fragile workaround. **Fix shipped May 28:** full MSYS2 ucrt64 migration — single coherent toolchain, problem permanently gone. **Rule:** do not mix toolchain sources. Either fully MSYS2 OR fully Qt installer + vcpkg. The MSYS2 path won for Branch C because `qt6-pdf` is only packaged in ucrt64.

### Pattern 9: Antigravity built engines, didn't wire UI
**The pattern of Antigravity sessions (per `07-sessions-log.md`):** strong real engine work (PoDoFo content-stream surgery, real FDF/CSV, real PAdES B-LTA, real DiffEngine Myers) but the corresponding UI integration was often left as stubs or preview banners. The audit caught this. **Rule for M3-M4 sessions:** every engine feature must also have a working UI path with no "scheduled for future engine update" message box.

### Pattern 18: `tr` local variable shadows `QObject::tr()`
**Discovered in M3-P1.** In any QObject subclass, naming a local variable `tr` (e.g., `auto* tr = new QHBoxLayout(...)`) shadows `QObject::tr()`. All `tr(...)` calls in the same function fail to compile with "tr cannot be used as a function." **Rule:** never use `tr`, `lay`, or other Qt-reserved identifiers as local variable names. Use `trow`, `hbox`, `row`, etc.

### Pattern 19: TestBatchMode flaky under parallel ctest
**Discovered in M3-P2.** TestBatchMode passes in isolation but fails intermittently under `ctest -j4`. Root cause: concurrent tests share I/O resources. **Fix (M4 catchup):** `RESOURCE_LOCK BatchModeIO` added to `set_tests_properties(TestBatchMode)`. Verify with `--repeat-until-fail 3`.

### Pattern 10: Universal "everything passes" syndrome
**Every Antigravity walkthrough ends with "all deliverables completed, builds successfully, all tests pass."** None mention skipped tests, deferred sub-tasks, or known limitations. The CHANGELOG `Known Issues` section was honest, but session walkthroughs were not. **Rule:** walkthroughs should explicitly list (a) what's complete, (b) what's deferred + why, (c) what tests were added vs skipped, (d) any TODO comments inserted.

---

## 8. Critical paths for common tasks

### "I need to make a change to the engines"
Read `src/core/interfaces/IPdfEditorEngine.h` first. Most engine changes propagate from there. Then look at `src/engines/podofo/PoDoFoBackend.cpp` for PDF object graph operations.

### "I need to add a new ToolId"
`src/core/ToolId.h` (enum) → `src/core/ToolId.cpp` (alias-aware string mapping) → register in the appropriate controller's `handledTools()` in `src/shell/controllers/` → add to RibbonModel in `src/shell/RibbonModel.cpp` for visibility.

### "I need to fix a UI bug"
Find the controller for the affected tool (e.g., SecurityController for sign/encrypt/redact). Trace from ToolRegistry::activate(ToolId) → controller switch → engine call.

### "I need to add a new test"
Add `.cpp` to `tests/` → register in the **root** `CMakeLists.txt` test section (there is no `tests/CMakeLists.txt`; all tests in root CMakeLists) → `add_executable` + `target_link_libraries` + `add_test` + `set_tests_properties`. Deploy qoffscreen plugin if test creates QApplication. Use `RESOURCE_LOCK` if test shares I/O resources (e.g., TestBatchMode).

### "I need to debug a signature issue"
The signing path: `SignDocumentHelper::execute` (in `src/commands/`) → `SignatureManager::signDocument` → PoDoFo + OpenSSL → `buildDssDictionary` if level≥B_LT → `applyDocTimeStamp` if B_LTA. Test with `tests/TestSignatureRealCrypto.cpp` (real crypto, real fixtures). Mock-based `TestSignatureValidationMock` is for interface contract testing only.

### "I need to update what the next CC session sees"
Edit `SESSION_BRIEF_NEXT.md`. The next session reads it.

### "I need to update memory across all CC sessions"
Edit (a) this `CLAUDE.md` file (project-scoped, auto-loads) AND (b) `C:\Users\User\.claude\memory\projects\glyphpdf\00-overview.md` (vault entry). Keep both in sync.

### "I want to record a new lesson learned"
Add to §7 of this file AND:
- Project-specific: `projects/glyphpdf/08-lessons-learned.md` (numbered sequentially Pattern 18+)
- Cross-project (if applies to other stacks): `knowledge/agent-execution-anti-patterns.md` (also tool/stack-agnostic; applies to every project in workspace)

### "I want to understand what the executor agent was thinking"
Read `.context/<prompt-id>-source-analysis.md` and `.context/<prompt-id>-vault-summary.md`. These are per-prompt working memos written before execution. They're NOT authoritative (git log + vault notes are), but they explain what the agent found when it first read the codebase. Coverage: M2-P2 through M3-P5 (M4 subagent sessions didn't produce them). See `.context/README.md`.

### "I'm executing a Branch C prompt — what do I read first?"
Per the STANDARD EXECUTION PROTOCOL **PHASE 0** in `docs/planning/MONTHS-2-8-PROMPTS.md` (and the inline reminder at the top of each individual prompt):

1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 17 cross-project failure patterns (Patterns 1-4 minimum)
2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` — GlyphPDF-specific lessons
3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` — architectural constraints
4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — current commit + test status
5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` — dependency-aware order
6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` — installed tools + MSYS2 packages + filesystem layout (don't re-query `pacman -Q`, `where`, `pip list`)

This CLAUDE.md auto-loads; the 6 above don't (they live outside the project). Reading them = ~6-12K tokens. Skipping them = ~5-10K wasted on re-derivation + risk of repeating documented failure modes. **Net: read them.**

### "I want to know what's installed on this machine"
Don't run `pacman -Q` / `pip list` / `npm ls -g` / `where <tool>` cold. Read `C:\Users\User\.claude\memory\infrastructure\system-environment.md` first — it has:
- Windows version + key tool versions/paths (git, gh, python 3.14.4, node 24, npm 11, java 21, docker)
- Tools NOT in default PATH (cmake/ninja/g++ in MSYS2 ucrt64)
- Python pip packages, npm global packages
- MSYS2 ucrt64 packages (Qt 6.11, PoDoFo deps, all GlyphPDF reqs)
- Vendored binaries (PoDoFo 1.1.0, PDFium, ONNX Runtime, planned Lua/Djot/JBIG2)
- Filesystem layout (Projects/, D:\prompts\data\, .claude/)
- Vertex AI / GWS / Apify / Mermaid config
- Re-verification commands (if you suspect the inventory is stale, run those + update the note)

---

## 9. Pre-flight checklist for any new CC session

Before executing a Branch C prompt:
- [ ] Working tree clean (`git status --short` returns nothing or only known build-artifact dirs)
- [ ] Last commit is what you expect (`git log -1`)
- [ ] All 24 tests pass (`ctest --output-on-failure -j4 --repeat-until-fail 3` — the repeat flag catches flaky tests; TestBatchMode has RESOURCE_LOCK for parallel runs)
- [ ] No stale STATE.md left over from a previous context-gated session
- [ ] Disk space for build (~5 GB headroom)
- [ ] You have the prompt file path: `docs/planning/MONTHS-2-8-PROMPTS.md`
- [ ] You know which prompt number to execute (dependency-aware order in §5 above)

Before tagging anything in git:
- [ ] All tests pass against current source (verify result file `mtime` newer than last source change)
- [ ] CHANGELOG updated for the change
- [ ] No `TODO(audit-*)` / `FIXME` / `XXX` introduced
- [ ] `grep -rn "scheduled for.*future engine update" src/` returns zero (or fewer than before)

Before declaring v1.0.0 release-ready (M8):
- [ ] All 34 M2-M8 prompts complete + committed
- [ ] No `Mock` returns in any IOcrEngine / ISignatureManager / IFormManager implementation
- [ ] No `setEnabled(false)` + "Coming in v1.1" tooltips
- [ ] All `.ts` translation files populated (lupdate output + translator delivery)
- [ ] Third-party security audit complete with no CRITICAL findings open
- [ ] EV code-signing cert obtained + MSI signed
- [ ] CHANGELOG `[1.0.0-internal]` → promoted to `[1.0.0]` with release date

---

## 10. When this file is wrong, fix it

State drifts. If you discover this CLAUDE.md is out of date (mentions a file that no longer exists, claims work is done that isn't, lists a commit that's not the head), **edit it in the same session**. The next session relying on stale memory wastes more time than the edit takes.

If you're not sure whether to update CLAUDE.md vs the Obsidian vault: update CLAUDE.md for facts that auto-load is high-leverage (build env, current state, file paths, non-negotiables). Update the Obsidian vault for deeper detail (full session histories, decision rationale, link networks).

---

*Last meaningful update: 2026-05-30 — consolidated catchup (M3+M4 sprints + 8 walkthroughs + test registrations + CHANGELOG fixes + vault sync). Next: read `docs/planning/NEXT-SESSION-HAND-OFF.md` for the recommended next prompt (M5-PROMPT-3, Office import).*
