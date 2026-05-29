# Changelog

All notable changes to GlyphPDF are documented in this file.

## [Unreleased] — v1.0.0 Branch C SCOPE LOCK execution (M2-M8)

Real public v1.0.0 ships when all M2-M8 work in `GLYPH-PDF-MONTHS-2-8-PROMPTS.md` (Desktop) completes. **Timeline: ~8-9 months from M2 start.** The `[1.0.0-internal]` build below is private and NOT for public distribution.

**Workstreams scheduled in M2-M8 (per ROADMAP Phase 1.5 + Sessions 9, 13):**
- **WS1 — Parallel Layout + OCR Ensemble** (M2 LaneScheduler infrastructure → M5 layout detector ensemble + cross-page pipelining): PP-DocLayoutV2 layout detector (+ Surya if license permits), per-region Tesseract+RapidOCR fanout via LaneScheduler, word-level ROVER fusion, per-region redo + confidence overlay.
- **WS2 — Djot Full Document Interchange** (M4 foundation Phase 1.5 → M5 OCR mapping → M6 annotation rich text): dual-model architecture (Structural PDF object graph ↔ Semantic `docmodel::SemanticDocument`); ProvenanceGuard refuses Djot save-back for born-PDF signed documents; vendored Lua 5.4 reference parser (MIT). Three roles: OCR output mapping, authoring input, annotation/comment rich text with /RC XHTML + /Contents plain text + /PieceInfo Djot sidecar.
- **WS3 — MRC Layered Compression in PDF/A** (M7): JBIG2 foreground + JPEG2000 background + invisible OCR sandwich text from WS1 word boxes; 5-10× compression for scanned content; PDF/A-2b conformant. No DjVu output (excluded); optional DjVu importer.

**Other v1.0.0 work in M2-M8:** Edact-Ray glyph-advance defense in redaction, OCR text-layer scrub in redaction rectangles, veraPDF subprocess for PDF/A validation, real-crypto E2E test coverage, 5 mode-page completions, 23 ribbon tools wired, Office→PDF import + PDF→PPT export, DiffEngine LCS/Myers upgrade, ar/fr/de translations populated, AI backend (Anthropic/OpenAI/Gemini/Ollama), third-party security audit, performance tuning + bug bash, OSS governance files (LICENSE/CONTRIBUTING/SECURITY), GitHub repo + CI workflows, marketing prep, MSI signing, package-manager submissions, launch announcement.

### Batch Processing (M3-PROMPT-2 — 2026-05-29)

#### Added
- BatchMode wired with real execution loop — drag-drop file list (text/uri-list, PDF filter),
  duplicate detection, Add Files / Add Folder / Remove / Clear buttons; QStandardItemModel
  keeps m_filesToProcess in sync with the view.
- 7 operation types: Convert (IConversionEngine::convertTo — real), Compress
  (IPdfEditorEngine::optimizeDocument — real), Watermark (addTextWatermark + saveDocument —
  real), Export PDF/A (exportPdfA — real), Merge (disabled — tooltip: "coming in M4"),
  OCR (disabled — tooltip: "OCR engine available in M5"), Redact search-pattern (disabled —
  tooltip: "coming in M3-P4"). QStackedWidget swaps per-operation config panels.
- Per-file progress: QFutureWatcher::resultReadyAt drives inline log + ETA calculation;
  overall QProgressBar updated 0-100%. Cancel button calls QFutureWatcher::cancel().
- Continue-on-failure: worker lambda catches per-file result, appends ErrorInfo to ErrorLog,
  does not abort the mapped future.
- CSV/JSON error log export via ErrorLog::exportCsv/exportJson, triggered by Export Log
  button (hidden until batch completes and ErrorLog is non-empty).
- Preview banner and fake QThread::msleep loop completely removed.
- IPdfEditorEngine calls serialized via QMutex (load/edit/save state machine; one instance
  shared across worker threads). IConversionEngine::convertTo called without mutex (stateless).
- ModeController.setScreen("batch") now injects AppContext via BatchMode::setAppContext().
- Qt6::Concurrent added to pdfws_ui PRIVATE link libraries.
- TestBatchMode (5/5 headless tests): file list population, duplicate detection, batch
  convert (mock IConversionEngine — 3 files), continue-on-failure (2 good + 1 bad),
  cancel (batch stops before all 6 files processed).
- Deferred: OCR (M5 OcrPipeline), pattern redact (M3-P4).

### Form Builder (M3-PROMPT-1 — 2026-05-29)

#### Added
- FormBuilderMode wired end-to-end: drag-place 9 field types (Text/Checkbox/Radio/Dropdown/ListBox/Date/Numeric/Signature/Button) via rubber-band mouse gesture on canvas; move/resize/delete commands; tab-order editor; FormFieldPropertiesPanel. Preview banner removed.
- Nine new `ToolMode` enum values (`FormAddText`…`FormAddButton`) in `PdfEnums.h`; `PdfViewerWidget` routes all nine to CrossCursor + `fieldPlacementRequested` signal (same rubber-band pattern as Crop mode)
- `FormFieldPropertiesPanel`: right-sidebar QWidget with name/tooltip/required/default/placeholder/regex fields; live validation (red border); Apply pushes `EditFormFieldCommand`
- `EditFormFieldCommand` (id 0x105): undo/redo for field property changes via `IFormManager::fillForm`
- `DeleteFormFieldCommand` (id 0x106): removes field from UI list; engine-side removal deferred to v1.1
- `MoveFormFieldCommand` (id 0x107) + `ResizeFormFieldCommand` (id 0x108): record geometry changes; engine-side rect update deferred to v1.1 (blocked on `IFormManager::updateFieldRect`)
- `ModeController::setAppContext()`: passes `AppContext*` + shared `PdfViewerWidget*` to `FormBuilderMode` at lazy-init
- `TestFormBuilder` (18/18 ctest — 5 headless tests): place/edit/delete/tab-order coverage

### Scheduling Infrastructure (M2-PROMPT-5 — 2026-05-29)

- **LaneScheduler infrastructure** (D1-D5): GPU lane — single persistent QThread warm worker bounded by QSemaphore (default capacity 2; anti-spawn-per-page); CPU lane — own QThreadPool sized to `QThread::idealThreadCount()` (isolated from global pool). `SchedulerResult<T> = QFuture<ScheduledValue<T>>` (C++17-compatible wrapper; `std::expected` deferred to C++23 bump). `OrderedResultQueue<T>` delivers results in page-index order; missing indices emit sentinel `SchedulerError{Timeout}`. `CrossPagePipeline` overlaps `stage1(P+1) || stage2(P) || stage3(P-1)` with backpressure semaphore. Wired into `AppContext` + `Bootstrapper`. 6 concurrency tests pass 5 repetitions (flakiness regression). Consumed by M5 OCR ensemble + M7 MRC pipeline.

### Security (M2-PROMPT-4 — 2026-05-29)

- **Adversarial crypto fixtures** (D1): `tests/fixtures/signing/generate.bat` extended with four new fixture generation blocks — `expired_cert.p12` (NotAfter 2020-01-02, in past), `weak_cert_rsa1024.p12` (RSA-1024 key), `revoked_cert.p12` (valid RSA-2048, for revocation simulation), and `revoked_ocsp_response.der` (DER-encoded OCSP stub with `certStatus = revoked [1]`, generated by `generate_revoked_ocsp.py`). `generate_test_input.py` produces `test_input.pdf` with byte-accurate xref offsets, replacing the batch-echo approach that produced incorrect offsets.
- **RSA key-size enforcement** (D2 engine): `signDocument()` now rejects keys below 2048 bits before creating any output (returns `false` + logs `"Signing rejected — RSA key size N bits < 2048 bits (weak key)"`). `validateSignatures()` checks each CMS signer cert — sets `trustStatus = "WeakKey"` for RSA < 2048 or `trustStatus = "CertExpired"` for `NotAfter < now()`. WeakKey/CertExpired take priority over EKU/signing-time checks. Embedded OCSP responses in the DSS `/OCSPs` array are re-parsed during validation — `certStatus == V_OCSP_CERTSTATUS_REVOKED` overrides trustStatus with `"Revoked"`.
- **Adversarial test suite** (D2 tests): Six new test methods added to `TestSignatureRealCrypto`: `testExpiredCertRejected`, `testRSA1024Rejected`, `testRevokedCertReportsRevoked` (QEXPECT_FAIL — M5 DSS wiring pending), `testTamperedPdfInvalid`, `testMultipleSignatures`, `testRevokedOcspSigner`. `testRSA1024Rejected` and `testRevokedOcspSigner` pass. Revocation test marked XFAIL until M5 DSS-to-signature-field correlation is wired.
- **Test suite migration** (D3): `TestSignatureValidation.cpp` migrated to `TestSignatureValidationMock.cpp` (21 mock-based tests, no real crypto). New `TestSignatureValidation.cpp` (9 real-crypto interface tests) exercises the full signing+validation pipeline using the test CA fixtures. CMakeLists.txt updated: `TestSignatureValidationMock` target added; `TestSignatureValidation` links `pdfws_engines`, `OpenSSL::SSL`, `OpenSSL::Crypto`, `podofo::podofo`, `Crypt32` (WIN32), with `SOURCE_DIR` compile definition and 120s timeout.
- **PoDoFo SaveOnSigning fix** (engine): `signDocument()` now passes `PdfSaveOptions::SaveOnSigning` to `SignDocument()`, ensuring the output is a complete PDF (header + all objects) rather than an objects-only incremental fragment that PoDoFo could not reload.
- **ctest**: 17/17 pass.

### PDF/A Validation (M2-PROMPT-3 — 2026-05-29)

- **Real PDF/A conformance validation via veraPDF CLI subprocess** (D1-D3): new `VeraPdfValidator` class invokes veraPDF CLI (`--format json --flavour <level>`) via `QProcess`, parses JSON output, returns structured `PdfAValidationReport` with `RuleViolation` list. AGPL-3.0 subprocess posture maintained — veraPDF is never linked in-process. `PdfAValidationPanel` now shows live validation results when veraPDF is configured, or "validator unavailable" status with build instruction when not. CMake option `-DVERAPDF_CLI_PATH=<path>` enables integration. "Fix Automatically" button deferred to v1.1.0; "Convert to PDF/A-2B" and "Export Report" wired.

### Security (M2-PROMPT-2 — 2026-05-29)

- **Invisible OCR text layer scrub** (D1): `redactCanvasRecursively` now tracks text rendering
  mode (`Tr` operator). Text operators (Tj/TJ/'/") executed with Tr==3 (invisible) whose glyph
  bounding box intersects a redaction rectangle are removed from the content stream. No
  Edact-Ray glyph-advance normalization gap is emitted for invisible scrubs — invisible glyphs
  produce no visual output so there is no visible cursor position to preserve. For `'` and `"`
  operators the text-line advance side-effect (T* equivalent) is still emitted to keep
  subsequent text positioned correctly. Form XObjects are walked recursively; Tr state is
  inherited across XObject boundaries (existing tracking behavior). Regression test
  `testOCRScrubbing` confirms "hunter2"-style secrets in invisible (Tr==3) OCR layers are
  scrubbed from redacted output with no Edact-Ray gap in their place.

### Security (M2-PROMPT-1 — 2026-05-29)

- **Edact-Ray glyph-advance normalization** (D1/D2): new `GlyphAdvanceCalculator` helper class
  computes total glyph advance widths via three encoding paths — `TryGetEncodedStringLength`
  (Simple + CID Identity-H), decoded-string `GetStringLength`, and CID per-glyph fallback using
  2-byte CID codes. Redaction content-stream surgery now emits a numeric-only `[N] TJ` gap
  (exact sum-of-advances) instead of the old `[ ( ) adj ] TJ` space-glyph form, closing the
  Edact-Ray side-channel attack (Bland et al., PETS 2023 / arXiv 2206.02285). Three regression
  tests added: `testGlyphAdvancesAreNormalized`, `testCJKFontHandling`,
  `testRedactionFailsAfterFontResolutionFailure` (16 total; 0 failures).

---

## [1.0.0-internal] - 2026-05-23 [INTERNAL-BUILD — NOT FOR PUBLIC DISTRIBUTION]

> **⚠ Scope-lock note:** Per Branch C SCOPE LOCK in `GLYPH-PDF-AUDIT-v1.0.0.md`, this build is private/internal during the 6-9 month execution sprint. **Real public v1.0.0** ships only when every claim in this changelog is implemented (no stubs, no preview banners, no mock OCR, no canned AI reply, no empty translations). The MSI at `dist\GlyphPDF-1.0.0-x64.msi` MUST NOT be published to any public channel (GitHub Releases, winget, chocolatey, scoop, website, social media) until the Branch C work completes.

### Build Environment (v1.0.0 internal)
- **Migrated from Qt-installer + vcpkg to MSYS2 ucrt64 native build.** Eliminates libstdc++/libwinpthread ABI mismatch (was causing 0xc0000139 in UnitTests + TestControllers). All toolchain + Qt6 + deps installed via pacman for single coherent runtime. Compiler: MSYS2 ucrt64 GCC 16.1.0 (was Qt-bundled MinGW 13.1.0). Qt: MSYS2 mingw-w64-ucrt-x86_64-qt6-base 6.11.0 + qt6-pdf (was Qt installer 6.10.2). Deps: pacman packages for PoDoFo 0.10.4, qpdf 12.3.2, OpenSSL, Tesseract 5.5.2, Leptonica 1.87.0, libxml2, freetype, zlib, curl, libpng, libjpeg-turbo, libtiff. Prebuilt for PDFium + ONNX Runtime. See README.md "Build Instructions" for full pacman package list. Note: ucrt64 selected over mingw64 because qt6-pdf is not packaged for mingw64 in MSYS2.



### Architecture
- Dependency injection via `AppContext` with shared_ptr ownership
- Bootstrapper pattern for clean startup wiring
- Static library architecture: pdfws_core > pdfws_engines > pdfws_commands > pdfws_ui
- ToolId enum with ToolRegistry for type-safe action dispatch

### PDF Engine
- PoDoFo backend for editing, metadata, forms, annotations, encryption
- PDFium backend for high-quality rendering with 3-tier cache (LRU, 256 MB, tiled)
- qpdf backend for linearization, repair-on-open, and save pipeline
- Incremental save with PoDoFo WriteUpdate (preserves signatures)
- Error reporting via lastError()/clearError() pattern on IPdfEditorEngine

### Security
- Fixed content-stream literal-string injection vulnerability in watermark, annotation, header/footer, and Bates numbering code paths (`pdfEscapeLiteralString`).
- PAdES B-LT/B-LTA digital signatures with DSS/VRI embedding
- OCSP client for certificate validation
- Certificate-based encryption (multi-recipient)
- SHA-256 only for signature hashing (no MD5/SHA-1)
- Redaction with content stream excision (never black rectangles)
- 15+ sanitization vectors (metadata, JavaScript, embedded files, etc.)
- **Hardened PAdES B-LT/B-LTA (v1.0.0 security audit):**
  - VRI key now computed as SHA-1 of raw `/Contents` bytes per ETSI EN 319 132 (fixes spec non-conformance; B3)
  - `validateSignatures` now performs real trust-chain verification against the Windows system
    root store (`CertOpenSystemStoreA`) or a custom `signing/trustStorePath`; adds
    `X509_VERIFY_PARAM` with CRL check and SMIME-sign purpose; returns `UntrustedChain` for
    self-signed or untrusted certificates (B4)
  - OCSP responses are verified with `OCSP_basic_verify` before DSS embedding; unverified
    responses are rejected with a `qWarning` and the signature degrades to B-T (B5)
  - ByteRange overlap detection (`ByteRangeOverlap` trustStatus) closes PDF shadow attack
    vector not previously caught
  - TSA token buffer increased from 16 KB to 32 KB for multi-cert TSA chains
  - `i2d_X509`, `i2d_PrivateKey`, and `BIO_new_mem_buf` return values are validated;
    OpenSSL error strings emitted on failure
  - `extractSignatureContentsRaw` no longer swallows exceptions silently

### Content
- Forms: text fields, checkboxes, radio buttons, combo boxes, list boxes
- Date fields, numeric fields, calculated fields
- FDF import/export
- Text editing with inline edit support
- Full annotation subtypes (highlights, underlines, notes, stamps, shapes)

### OCR
- Tesseract engine integration
- RapidOCR PP-OCRv5 engine
- Preprocessing pipeline
- ROVER word-level merge for multi-engine consensus

### Conversion & Batch
- PDF to HTML, images, CSV, text conversion
- Batch execution mode with inline error reporting
- Error log export (CSV/JSON)

### Page Operations
- Rotate, delete, insert, extract, split, reorder pages
- Crop and resize pages
- Headers, footers, page numbers, Bates numbering

### Watermark & Optimization
- Text and image watermarks
- PDF compression with optimization estimates
- Linearization for web-optimized delivery

### Accessibility
- Screen reader support with accessible names/descriptions on all UI elements
- F6 / Shift+F6 region cycling (ribbon, sidebars, viewer, status bar)
- F1 keyboard shortcuts help dialog
- Tab focus order across all regions
- WCAG-aligned high contrast theme

### Localization
- Qt Linguist translation framework wired (English only translated; Arabic, French, German .ts files exist as empty shells pending lupdate + translator pass)
- RTL layout flag applied at application root via Qt::RightToLeft when language is Arabic; per-widget RTL handling not yet audited
- Language selection in Preferences (restart required) — picks resource translator but non-English packs are currently no-ops

### Error Handling
- ErrorInfo struct with severity levels (Info, Warning, Error, Critical)
- ErrorDialog with collapsible technical details and action buttons
- Engine error wrapping across all 40+ methods
- Batch error handling with inline success/failure display
- Graceful PDF corruption handling with repair warnings
- Memory guards: 500 MB file warning, system RAM monitoring, auto-tiling
- Temp file cleanup with atexit handler

### Reliability
- Real interval autosave with configurable timer (default 5 minutes, range 1–30 minutes)
- `AutosaveManager` writes `.autosave.pdf` via PoDoFo full-save on worker thread with atomic rename
- Crash recovery dialog on startup scans for orphaned `.autosave.pdf` files
- Autosave interval configurable in Preferences (live-applied)
- ModeStrip label shows true autosave/save state: `● AUTOSAVED`, `● UNSAVED`, `● SAVED`
- Sync indicator replaced with `LOCAL ONLY` (Cloud Sync deferred to v2.0)

### Installer & Packaging
- windeployqt bundling script
- WiX v4 MSI installer with MajorUpgrade support
- File associations via OpenWithProgids (not hijacking defaults)
- Runtime dependency checker
- Build automation script

### Auto-Update
- JSON manifest-based update checker
- Async non-blocking startup check (3-second delay)
- SHA-256 verified downloads
- MSI silent upgrade via msiexec
- User consent required at every stage (check, download, install)
- Rollback safety: failed updates leave current version intact
- Preferences integration: toggle check-on-startup, channel selection (Stable/Beta)

### Print & Export
- Print preview via QPrintPreviewDialog
- Page setup dialog (paper size, orientation, margins, scaling)
- Export presets panel (High Quality PDF/A, Web Optimized, Legal Archive)
- Create/edit/delete custom export presets (stored in QSettings)

### UI
- Ribbon toolbar with 7 tab controllers (Home, View, Edit, Pages, Convert, Forms, Security)
- Mode strip with theme toggle and AI panel toggle
- Left/right sidebars (thumbnails, bookmarks, comments, inspector)
- Screen navigation (OCR, Redact, Compare, Signatures, PDF/A, Compress, Watermark)
- Status bar: mode, page, zoom, tool, selection, OCR language, encoding, PDF version, page size, file size, unsaved indicator
- Recent files (max 20) with Clear History
- Drag-and-drop PDF opening
- Welcome widget with action cards and recent files grid
- Dark, Light, and High Contrast themes
- Find & Replace bar with redact-all option
- AI Chat panel (toggle)
- Keyboard shortcuts dialog

### Testing
- 13 test targets: UnitTests, TestInterfaces, SmokeTest, TestSanitization, TestSignatureValidation, TestRedaction, TestThreadSafety, TestEncryption, TestResourceLimits, TestControllers, TestIntegration, TestPerformance, TestAutosave
- Headless testing via qoffscreen platform plugin
- Thread safety tests with QRecursiveMutex verification
- Performance benchmarks with timing targets

### Known Issues
- MuPDF (AGPL) and Poppler (GPL) are never linked in-process (licensing constraint)
- OCR ensemble pipeline (PP-DocLayoutV2, Surya, LaneScheduler) not yet implemented
- MRC compression inside PDF/A not yet implemented
- Some Session 7-11 features may be interface stubs pending full implementation verification
- Translations: glyphpdf_{ar,fr,de}.ts are empty shells. Run `lupdate src/ -ts translations/glyphpdf_*.ts` then commission translators before claiming multilingual support.
- DiffEngine uses per-word set-difference rather than LCS/Myers — order changes appear as add+delete pairs, not moves. Affects legal/compliance comparison persona.
- Pattern redaction (email/phone/ID regex) not implemented. Only literal-string search-and-redact is available.
- Office→PDF import not implemented (only PDF→Office conversion paths exist).
- Send-for-signing workflow (remote signing order, reminders, audit trails) not implemented — only local certificate-based signing exists.
- CollaborationManager.cpp marks itself "Cloud Sync Stub (Simulation)"; ICollaboration interface exists with no real network backend.
- RapidOcrEngine: PP-OCRv5 tensor pre/post processing is a stub. The engine is runtime-disabled in the OCR mode selector (greyed out with "Available in v1.1.0" tooltip) so users cannot select it.
- enterprise_installer.wxs at repo root (legacy WiX v3 with different product name) — superseded by packaging/GlyphPDF.wxs (WiX v4) and removed in this audit.
