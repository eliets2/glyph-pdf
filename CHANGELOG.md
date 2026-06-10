# Changelog

All notable changes to GlyphPDF are documented in this file.

## [1.0.0] — 2026-06-10

First public release. The 2026-06-02 audit release-gate is clear: the WP-0…WP-8a
remediation, R2/R3 verification rounds, and the R4/R5 launch-gate passes
(multi-agent audit, 37/37 tests, MSI packaging with bundled OCR models) are
complete — see `docs/audit/round2/SUMMARY.md`. The items that de-promoted the
premature 2026-06-02 heading are resolved or honestly disclosed: i18n is
declared English-only for v1.0, FormBuilder delete/move/resize persist via
AcroForm, OCSP revocation is enforced with certID matching, and all security
blockers (incl. the incremental-saving-attack detection and image-dimension
caps) are closed with tests.

### R4/R5 launch-gate highlights (2026-06-10)
- **Security:** image XObject dimension cap (OOB read), incremental saving
  attack detection (`ValidWithUnsignedChanges`), recursion depth guards in
  redaction walkers, annotation Launch/URI/SubmitForm action stripping in
  sanitize, honest `Revoked` OCSP verdict with a real matching fixture.
- **OCR:** ROVER ensemble (Tesseract + RapidOCR) is the default engine with
  automatic Tesseract fallback; all ONNX models and tessdata ship in the MSI —
  OCR is fully offline, no first-run downloads.
- **Packaging:** WiX MSI with license dialog, objdump-derived DLL closure,
  pinned ProductCode, SHA-256 checksum, validated winget manifests.
- **Honesty pass:** release-notes claims corrected (Ollama-only AI, 7 form
  field types), update check now opt-in, undo/redo/watermark/compare wired,
  unfinished OCR-Verify screen hidden, Type3/pattern redaction gap disclosed
  in SECURITY.md (T-RED-2).

## Security — 2026-06-10
- [B-01] Private key material (ca.key, signer.key, test_signer.p12) purged from git history via
  git filter-repo. These files were present in commit a6ea6aa and are now absent from all refs.
  Collaborators must re-clone. Fresh test-only fixtures regenerated (R2-7).

### WP-1: Remove cloud upload paths (2026-06-03) — audit-remediation branch

#### Removed
- **Cloud Sync / CollaborationManager**: deleted `src/engines/CollaborationManager.*`,
  `src/core/interfaces/ICollaboration.h`, `AppContext::collab` field, and
  `AppContext::DefaultCloudSyncEndpoint`. Annotation export/import inlined
  into SecurityController using AnnotationSerializer (local file I/O only).
- **Fake SYNCED/NOT SYNCED indicator** (C-01): removed `setSyncStatus("SYNCED·v.1")`
  and `setSyncStatus("NOT SYNCED")` calls from GpMainWindow; removed
  `ModeStrip::setSyncStatus` slot entirely. Status bar now always shows LOCAL ONLY.
- **Send for Signature ribbon button** (C-05): removed `sendSign` tool from the Home
  ribbon Share group; no handler existed and it implied a cloud e-sign upload.
- **Cloud Sync ribbon button**: removed `cloud` tool from the Home ribbon Share group.
- **Cloud AI providers** (B-02): deleted `AnthropicProvider.*`, `OpenAiProvider.*`,
  `GeminiProvider.*` — all three send document/chat content to external servers.
  AI Chat is now **Ollama-only** (localhost, no internet). Updated AIChatPanel,
  PreferencesDialog (AI tab now shows Ollama endpoint + model fields), and
  TestAiProvider (removed Anthropic/OpenAI/Gemini cases; 32/32 ctest passes).

### MRC Compression Pipeline — WS3 (M7-PROMPT-3 — 2026-06-02)

Mixed Raster Content layered compression pipeline for scanned PDFs: JBIG2 (Apache-2.0, agl/jbig2enc) lossless foreground + JPEG2000 (OpenJPEG 2.5.4, BSD-2) background + invisible `3 Tr` OCR sandwich text from WS1+WS2 word boxes. PDF/A-2b conformant. **30× compression ratio** measured on synthetic A4 page (86 KB vs 2.64 MB raw pixels). veraPDF validation gate integrated. Optional DjVu importer (HAS_DJVU=OFF default, GPL-2.0+ accepted when enabled).

#### Added
- **`MrcPageProcessor`** (`src/engines/mrc/MrcPageProcessor.{h,cpp}`):
  - `separatePage(QImage, QList<LayoutRegion>, QList<MergedOcrWord>) → MrcLayers`.
  - Layout-guided separation: Title/Paragraph/List/Table/Header/Footer/Equation/Reference/Caption → foreground mask; Figure → background. Unclassified pixels: Otsu threshold.
  - Foreground → JBIG2 lossless generic-region (HAS_JBIG2ENC; falls back to Flate/1bpp when jbig2enc not yet vendored). **NEVER pattern-matching mode** (Xerox 2013 incident; German BSI ban).
  - Background → JPEG2000 at configurable ratio (Lossless: 10:1 / Balanced: 30:1 / Aggressive: 50:1) via OpenJPEG 2.5.4 (BSD-2-Clause, already in MSYS2 ucrt64). Custom write-stream callbacks (no opj_stream_create_default_memory_stream dependency).
  - `estimateCompressedSize(QImage, MrcMode)` → size hint for CompressDialog UI.
  - Thread-safe: `separatePage()` is re-entrant (CPU-lane safe).
- **`MrcMode` enum** (`src/core/interfaces/IPdfEditorEngine.h`): `Off | Lossless | Balanced | Aggressive`.
- **`PdfEditorEngine::exportMrcPdfA`**: raw PDF/A-2b writer (PDF 1.6, JPEG2000 bg XObject, JBIG2 mask XObject `/ImageMask true`, invisible 3 Tr text layer from OCR word boxes, XMP metadata stream, sRGB OutputIntent). veraPDF subprocess validation gate (warning if CLI unavailable).
- **`CompressDialog` MRC selector** (`src/modes/CompressDialog.{h,cpp}`): QComboBox with Off/Lossless/Balanced/Aggressive + live size estimate label.
- **`DjvuImporter`** (`src/engines/conversion/DjvuImporter.{h,cpp}`): optional DjVu → page images + embedded text via DjVuLibre. `HAS_DJVU=OFF` default; import-only (no DjVu output per ROADMAP).
- **`TestMrcPipeline`** (`tests/TestMrcPipeline.cpp`): 9 tests — foreground mask, JPEG2000 encoding, layer separation, estimate ordering, **≥5× size reduction** (achieved 30.44×), sandwich text searchable, veraPDF QSKIP (CLI not configured), DjVu QSKIP (HAS_DJVU=OFF), mode ordering monotonic.
- **`docs/MRC-ENCODER-LICENSE-AUDIT.md`**: D1 audit gate — jbig2enc Apache-2.0 approved; jbig2dec AGPL-3.0 rejected; OpenJPEG BSD-2 approved; DjVuLibre GPL-2.0+ conditional (HAS_DJVU guard).

#### Removed (known-issues)
- Removed "MRC compression inside PDF/A not yet implemented" admission. WS3 is now complete for v1.0.0.

---

### OCR→Djot Mapping — WS2 Role 1 (M5-PROMPT-4 — 2026-06-02)

OCR output maps to SemanticDocument via OcrDjotMapper (WS2 role 1) — layout regions → block structure; per-word fused text → Inline spans with `{pdf-page pdf-bbox pdf-font ocr-conf}` attributes; tables → Djot pipe tables. Feeds OCRMode review UI + M7-P3 MRC sandwich text layer.

#### Added
- **`OcrDjotMapper`** (`src/engines/ocr/OcrDjotMapper.{h,cpp}`): stateless pure-function
  `fromOcrResults(QList<PageOcrResult>, pdfPath) → SemanticDocument`. No hidden state;
  re-entrant and safe to call from multiple CPU lane workers simultaneously.
  - Reading-order (`LayoutRegion::readingOrderIndex`) → block order within each Section.
  - RegionType mapping: `Title` → `Heading(level 1)`, `Paragraph` → `Paragraph`,
    `Table` → `ContainerBlock(Table)`, `Figure` → `Figure`, `List` → `List`
    (ContainerBlock with ListItem children per Y-line), `Header` → `Paragraph` with
    `{page-header}` attr in provenance, `Footer` → `Paragraph` with `{page-footer}` attr.
    `Equation`, `Reference`, `Caption`, `Other` → `Paragraph` with typed attr annotation.
  - Per-word fused `MergedOcrWord` → `TextInline` with provenance encoding:
    `{pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}|path/to/file.pdf`
    stored in `Provenance::source_file` so M7-P3 MRC sandwich text can align words to tiles.
  - Every block node carries `BornOCR` `ProvenanceTag` + source PDF path + page index + bbox.
  - **Table cells**: X-gap analysis clusters row words into cells;
    `ContainerBlock(Table)` → rows (`ListItem`) → cells (`Paragraph`).
- **`OcrPipeline::recognizeDocumentAsDjot`** (`src/engines/ocr/OcrPipeline.{h,cpp}`):
  new API `QFuture<SemanticDocument> recognizeDocumentAsDjot(pdfPath, pageImages)`.
  Chains `recognizeDocument()` → `OcrDjotMapper::fromOcrResults()` in a single
  `QtConcurrent` CPU-lane worker thread (CPU-bound mapping stays off the UI thread).
  Back-compat: `QFuture<QList<PageOcrResult>> recognizeDocument()` is unchanged.
- **`OCRMode::setSemanticDocument`** (`src/modes/OCRMode.{h,cpp}`):
  Djot-aware "Review before save" pane. Scan pane renders inline-styled HTML via a direct
  SemanticDocument tree-walker (Heading/Paragraph/Figure/List/Table blocks, no external CSS).
  Text pane populates with Djot source text via `LuaDjotCodec::documentToDjot` (C++ emitter —
  no Lua runtime required for encode direction) for Djot-aware edit-in-place.
  Per-region accept/reject buttons (M5-P2 D6) remain active after doc load.
- **`Block::Type`** extended with `Table` and `Figure` types (`src/docmodel/Block.h`).
- **`LuaDjotCodec::emitBlock`** updated: `Table` emits Djot pipe-table syntax with header
  separator row; `Figure` emits `::: figure` fenced div.
- **`TestOcrDjotMapper`** (14 tests): empty input, Paragraph/Title/Header/Footer/Figure/List/
  Table region mapping, per-word BornOCR provenance with attribute encoding, block order from
  readingOrderIndex, encode roundtrip (paragraph text in Djot output), table pipe-table syntax
  (`|` + all cell texts), multi-page sections, failed page error section. **31/31 ctest pass.**

#### WS2 Status
- WS2 Role 1 (OCR→Djot mapping): **COMPLETE** (this prompt)
- WS2 Role 3 (annotation rich text): **COMPLETE** (M6-PROMPT-4, 2026-06-01)
- WS2 Role 2 (authoring Djot→PDF): implicit in M4-PROMPT-7 IDjotMapper

### Edge-case cleanup + branding (M6-PROMPT-6 — 2026-06-01)
#### Changed (decorative panels → real data)
- **SignaturesPanel** renders real `ISignatureManager::validateSignatures` output (was hardcoded "Elie Matta / GlobalSign CA"). (D1)
- **ThumbnailSidebar** shows real PDFium-rendered page thumbnails via RenderCache (was placeholder blocks). (D2)
- **Sidebar** attachment extraction reads real embedded-file bytes via PoDoFo (was a text stub). (D3)
- **StatusBar** OCR cell reflects the real selected language; removed the meaningless UTF-8 encoding cell. (D4)
- **Bates numbering** gains a preset selector + All/Custom page-range UI (from/to spin boxes bounded to the document); the backend stamps only the chosen range (`firstPage`/`lastPage`, backward-compatible all-pages default). (D5)
#### Added (branding)
- App-wide window/taskbar **icon**, a startup **splash screen**, and a **system-tray icon** (Show/Quit + tooltip) wired in `main.cpp`; 5 brand assets bundled under `resources/branding/` via the Qt resource system.
- `PdfWorkstation.exe` now carries the brand icon (windres `.rc`, `enable_language(RC)`); the WiX installer's icons (ARP, shortcuts, `.pdf` file-association DefaultIcon) are rebranded via a regenerated multi-size `glyphpdf.ico`.
#### Verified
- **PdfAValidationPanel** already exposes real veraPDF report data — no hardcoded violations (D6). A Pattern-5 grep across `src/ui|src/modes|src/shell` returns **zero** mock-masquerading-as-real surfaces.
#### Known follow-ups
- The **installer-logo** asset is bundled but not yet shown in a WiX UI dialog (needs a WixUI dialog set + license.rtf + BMP banners; deferred to M8 packaging — requires the WiX toolset to build/verify).
- Brand PNGs are full-resolution (≈1–2 MB each); they are downscaled at runtime, so embedding optimized copies would shrink the binary (follow-up).
- The rich Djot **comment composer toolbar** remains deferred (TODO(M6-P5) seam), unchanged by this pass.

### Added
- Forms tools: Button field, signature field placement, auto-detect form fields, tab order editor - all wired.

GlyphPDF v1.0.0 is the first public open-source release. All workstreams (WS1–WS3) and M2–M8 deliverables are complete. Apache-2.0 licensed.

### Comment Threading Depth (M6-PROMPT-5 — 2026-06-01)

Replies, ISO 32000 `/IRT` in-reply-to linking, and non-modal review-state changes
for the comment thread (CommentsWidget + PoDoFoBackend).

#### Added
- **Threaded view depth indent + filters** (`src/ui/CommentsWidget.cpp` — D1): nested replies render with a depth-scaled `↳` guide and progressive dimming so reply depth is legible even with word-wrapped multi-line text; a reply whose parent is filtered out is promoted to top-level so it is never hidden. New **date filter** (All / Today / Last 7 days / Last 30 days, computed against the ISO-8601 `creationDate`) alongside the existing **status** (now incl. Cancelled) and **author** filters. Review-state color tints each top-level row + state tooltip.
- **`/IRT` in-reply-to linking on save** (`PoDoFoBackend::embedAnnotations` — D2): when an `AnnotationItem` has `parentId`, write `/IRT` as an indirect reference to the parent annotation + `/RT /R` per ISO 32000-1 §12.5.6.4. Review state is now written per-annotation via `/State` + `/StateModel /Review` for **both** top-level comments and replies (previously partial, reply-only, via a non-standard `/Subj "State"`). `ReviewState::Open` → `/State (None)`; `ReviewState::None` writes no `/State`.
- **Thread + review-state load-back** (`PoDoFoBackend::extractAnnotations` — D2): restores `parentId` from `/IRT` → parent `/NM` and `reviewState` from `/State`, then rebuilds each parent's `replies` list — so replies stay threaded and review states roundtrip across save/reload.
- **`changeReviewState` wired** (`src/ui/CommentsWidget.cpp` — D3): new `applyReviewState(id, state)` updates `AnnotationItem.reviewState` + `modificationDate` and pushes an `EditAnnotationCommand` onto the shared `QUndoStack` (undoable; marks the `DocumentSession` dirty). The tree context menu (Open / Accepted / Rejected / Cancelled / Completed) routes through it and stays **non-modal** (QMenu, no blocking dialog). `CommentsWidget` gains `setContext(AppContext*)`; `Sidebar` now also hands the widget the viewer (it was previously never given one, so its menu + composer were inert).
- **`TestAnnotationDjot`** +4 cases (now 14): reply threading persists through the PDF roundtrip; every review state (Open/Accepted/Rejected/Cancelled/Completed) roundtrips; `ReviewState::None` omits `/State`; `.ann` serializer preserves `parentId` + `reviewState`.

#### Replaced
- Non-standard `/Subj "State"` partial review-state marker → spec `/State` + `/StateModel /Review` on the annotation, written for top-level comments and replies alike.

#### Deferred / Known Issue
- The comment composer is **still plain text**. The richer Djot composer toolbar + live preview for comments/replies (the `TODO(M6-P5)` seam left by M6-PROMPT-4) is **not** delivered in this sprint — M6-PROMPT-5's D1 scope is the threaded *view* (indent + filters), not composer markup authoring. Entered comment text is still treated as trivial Djot source and dual-writes correctly. The richer composer remains a future polish item.

### Djot Annotation Rich Text (M6-PROMPT-4 — 2026-06-01)

Annotation + comment rich text uses Djot as internal authoring model; transcodes to PDF /RC XHTML + /Contents plain text + /PieceInfo Djot sidecar for perfect GlyphPDF roundtrip + Acrobat/Foxit interop (WS2 role 3 complete).

#### Added
- **`AnnotationItem::djotSource`** (`src/core/AnnotationTypes.h`): Djot rich-text source as the internal authoring model. Plain-text `text` retained as the PDF /Contents fallback. `AnnotationSerializer` preserves djotSource through the `.ann` JSON roundtrip.
- **`pdfws_djot::DjotToRichTextXhtml`** (NEW `src/pdfws_djot/DjotToRichTextXhtml.{h,cpp}`): bounded transcoder for the annotation authoring subset (strong/emph/code/link/list/heading). `djotToXhtml` emits the restricted XHTML rich-text string per ISO 32000-1 §12.5.6.4; `djotToPlainText` strips markup for the /Contents projection; `djotToHtmlFragment` backs the live preview. NOT a Djot grammar reimplementation — the full-document path keeps using the vendored Lua reference parser.
- **InspectorWidget Djot editor** (`src/ui/InspectorWidget.cpp`): the Contents editor is now a Djot source editor with a formatting toolbar (bold/italic/code/link/list/heading) and a live read-only HTML preview pane. Edits write djotSource and auto-derive `text` as the plain-text projection.
- **Save-time dual-write** (`PoDoFoBackend::embedAnnotations`): `/Contents` = plain-text projection (spec-required fallback Acrobat/Foxit read); `/RC` = restricted XHTML via `DjotToRichTextXhtml` (raw Djot is never stored in /RC); `/PieceInfo /GlyphPDF /Private /DjotSource` = original Djot escaped via `pdfEscapeLiteralString`.
- **Load-time restore** (NEW `PoDoFoBackend::extractAnnotations`): restores djotSource from the /PieceInfo sidecar when present (via new `pdfUnescapeLiteralString`, the exact inverse of `pdfEscapeLiteralString`), else derives a trivial djotSource from /Contents — a perfect GlyphPDF→PDF→GlyphPDF rich-text roundtrip.
- **Comment integration** (`src/ui/CommentsWidget.cpp`): top-level comments and replies (and the InspectorWidget reply composer) populate djotSource, so the shared dual-write emits /RC + /PieceInfo for comments identically to annotations. (PROMPT-10 comment threading already shipped; the rich Djot composer toolbar for replies is left as a clean seam with `TODO(M6-P5)`.)
- **`TestAnnotationDjot`** (NEW, 12 cases): markup→save→reopen→djotSource match; plain-text-only→djotSource=plain text; UTF-8 markup sidecar roundtrip; transcoder structure + XML-escaping + plain projection; escape/unescape lossless (ASCII+UTF-8); serializer preserves djotSource.

#### Note
The InspectorWidget gets the full Djot toolbar + live preview; the CommentsWidget composer remains plain text (its entered text is treated as trivial Djot source and still dual-writes correctly) — a richer Djot composer for comments/replies is deferred to M6-PROMPT-5.

### AI Backend Wiring (M6-PROMPT-3 — 2026-06-01)

#### Added
- **`IAiProvider`** interface (`src/engines/ai/IAiProvider.h`): async `QFuture<AiResult> chat(history, opts)`; `isReady()` and `isPlausibleKey()` per provider.
- **`AnthropicProvider`** (Anthropic Claude 3.5 Sonnet): reads key from `CredentialManager("Anthropic")` or `QSettings("ai/keyAnthropicCached")`; real HTTPS call via `QNetworkAccessManager` in `QtConcurrent::run` worker thread (non-blocking GUI).
- **`OpenAiProvider`** (OpenAI GPT-4o): real HTTPS; key from `QSettings("ai/keyOpenAICached")`.
- **`GeminiProvider`** (Google Gemini 1.5 Flash): real HTTPS; key from `QSettings("ai/keyGeminiCached")`.
- **`OllamaProvider`** (local Ollama): connects to `http://localhost:11434`; no API key required; model from `QSettings("ai/ollamaModel", "llama3")`.
- **`AIChatPanel` real wiring**: replaces canned reply with `activeProvider()->chat(history)` via `QFutureWatcher`. Chat history maintained across turns; typing-cursor placeholder replaced by response on completion. Suggestion chips submit pre-filled prompts.
- **PreferencesDialog AI tab**: all 4 providers selectable (Anthropic, OpenAI, Gemini, Ollama — previously OpenAI/Gemini were "coming v1.1" placeholders). "Test key" button now performs a real 1-token API ping; result shown inline. Ollama shows informational message (no key needed).
- **`TestAiProvider`** (10 tests): mock provider echo, empty history, key format validation for all 4 providers, real round-trip QSKIP-gated behind `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` env vars.

#### Replaced
- `AIChatPanel` canned reply `"AI: (v1.1) AI responses will appear here once real LLM calls are wired."` replaced with real async LLM calls.

### Localization ar/fr/de (M6-PROMPT-2 — 2026-05-30)

#### Added
- **`lupdate` populated** all three locale files (`glyphpdf_ar.ts`, `glyphpdf_fr.ts`, `glyphpdf_de.ts`) — 1394 source strings each, all marked `type="unfinished"` pending human translation.
- **`.qm` binary files embedded** as Qt resources at `:/translations/` via `qt_add_translations RESOURCE_PREFIX "/translations"` in CMakeLists.txt. Loaded at startup via `QTranslator::load(locale, "glyphpdf", "_", ":/translations")`.
- **`translations/README.md`**: documents commissioning status, vendor tracker table, lupdate/lrelease workflow.
- **RTL audit** for Arabic: `main.cpp` sets `app.setLayoutDirection(Qt::RightToLeft)` when Arabic locale is selected; standard Qt widgets (QFormLayout, QHBoxLayout, QListWidget, QTreeWidget) auto-mirror. No critical widget-level RTL fixes required. AnnotationLayer painter coordinates intentionally locale-independent (fixed PDF coordinate system).

#### Note
Translations remain untranslated pending commission. Switching to Arabic/French/German in
Preferences currently displays English source strings (graceful fallback). The engineering
scaffolding is complete; translator delivery gates real multilingual UI.

### DiffEngine LCS/Myers Upgrade (M6-PROMPT-1 — 2026-05-30)

#### Added
- **`MyersDiff`** (`src/engines/MyersDiff.h/.cpp`): Myers 1986 O((N+M)D) LCS algorithm for token sequences. Produces a fully ordered edit script (Keep / Insert / Delete). `MyersDiff::detectMoves()` post-pass identifies Delete+Insert pairs of identical tokens and reclassifies them as `MoveOperation`.
- **`DiffResult::PageDiff::moves`**: new `QList<MoveOperation>` field exposing moved tokens alongside existing `textAdded` / `textRemoved`.
- **`CompareWidget` text diff panel**: new `QTextBrowser` below the two PDF viewers. Renders the diff with color coding — green insertions, red deletions, **orange moves** (with underline + position annotation). "NEXT →" / "← PREV" toolbar buttons in `CompareMode` now active and navigate through changes.
- **`TestDiffEngine`** (10 tests): Myers correctness (empty, insert, delete, identical, complex LCS, edit-script ordering), move detection (no-moves, single-move, legal-document paragraph reorder), `DiffResult.moves` field smoke test.

#### Replaced
- `DiffEngine` per-word `QSet` set-difference replaced with `MyersDiff::compute()`. The previous implementation treated order changes as unrelated add+delete pairs; the new LCS correctly minimises edit distance and enables move detection. Existing `textAdded` / `textRemoved` fields remain populated for backward compatibility with `CompareMode` change counts.

### Office→PDF Import + Images→PDF (M5-PROMPT-3 — 2026-05-30)

#### Added
- **Office→PDF import via LibreOffice subprocess** (`HAS_LIBREOFFICE` CMake flag; `find_program(soffice)`). `ConversionManager::convertOfficeToPdf` replaces dead `#ifdef HAS_LIBREOFFICE` stub with real subprocess invocation: validates input extension, spawns `soffice --headless --convert-to pdf:writer_pdf_Export`, applies timeout (default 120s), kills process tree on Windows via `taskkill /F /T /PID` on timeout. Supported formats: `.docx`, `.xlsx`, `.pptx`, `.odt`, `.ods`, `.odp`, `.rtf`, `.csv`, `.txt`.
- **Images→PDF** via PoDoFo PdfImage XObject embedding (`ConversionManager::convertImagesToPdf`). Accepts `QStringList` of PNG/JPEG/TIFF paths; one page per image; fit-to-page or natural DPI sizing; `ImageImportOptions { dpi, fitToPage, pageSize }`.
- **Import Office Document** and **Images to PDF** action cards in `WelcomeWidget`. New signals: `importOfficeRequested()`, `imagesToPdfRequested()`.
- `ToolId::ImportOffice` + `ToolId::ImagesToPdf` added to enum; `HomeController` handles both via `QtConcurrent::run` + `QProgressDialog` (non-blocking GUI).
- `TestOfficeImport` (4 tests): images→PDF page count + XObject presence; empty-list guard; OfficeToPdf with real RTF (QSKIP without LibreOffice); unsupported-extension rejection. 24/24 ctest.
- README "Build Instructions" updated with optional LibreOffice install note.

### Djot Interchange Foundation (M4-PROMPT-7 — 2026-05-30)

#### Added
- `pdfws_djot` and `docmodel` foundational static libraries for the dual-model architecture.
- Vendor Lua 5.4 (MIT) + Djot reference parser at `third_party/lua-5.4/` + `third_party/djot/`. No reimplementation — spec-conformant parsing via the reference parser (non-negotiable per architecture).
- `ProvenanceGuard` enforces a hard constraint: semantic (Djot) edits cannot be saved back to a signed, born-PDF document to prevent signature invalidation. Warns on unsigned born-PDFs with a "Save as copy" route.
- `IDjotCodec` and `IDjotMapper` interfaces wired into `AppContext`. `LuaDjotCodec` implements `djotToDocument` (decode path).
- TestDjotRoundtrip: 6 tests (decode, encode-stub documentation, ProvenanceGuard 4 cases).

#### Known Issue (M4-PROMPT-7)
- `LuaDjotCodec::documentToDjot` encode path is **stubbed** (`LuaDjotCodec.cpp:54` TODO, returns empty string). M5-PROMPT-4 (OCR→Djot mapping) is blocked until encode is implemented.

### Edge Fixes (M4-PROMPT-6 — partial, not fully executed)

#### Verified in-place from prior sessions
- `ToolMode::Strikeout` and `ToolMode::Squiggly` are distinct enum values (not aliased to Underline) in `EditController` and render correctly in `AnnotationLayer` (D1 verified).
- MAPI "Share with Attachment" wired via `GetProcAddress` at `HomeController.cpp:136` (D2 verified).

#### Not yet implemented
- Prune missing recent files from the recent-files list (D4) — no `pruneRecent`/`removeMissingRecent` logic found in source.
- M4-PROMPT-6 in `docs/planning/MONTHS-2-8-PROMPTS.md` is skeletal (missing session_metadata, role, files_to_read, verification, constraints, error_recovery sections). Must be expanded to full 7-H format before execution.

### Pages Mode (M3-PROMPT-3 — 2026-05-29)

#### Added
- PagesMode wired: real page list with lazy thumbnails (gray placeholder + page number; real PDFium render deferred until IPdfRenderBackend is exposed via AppContext), split form (by-page / by-N-pages / by-range expression), reorder UI. Preview banner removed.
- Split form: QSpinBox (split at page N), QSpinBox (split every N pages), QLineEdit (range expression "1-3,5,7-9" parser), output naming pattern with `{stem}`/`{n}` tokens, output folder Browse button, filename-preview QListWidget, QProgressDialog for large splits, overwrite confirmation via QMessageBox.
- Split implementation: `extractPageAsBytes` + `insertPageFromBytes` loop per output part; no `splitDocument()` engine method required. Each output starts from a minimal stub PDF; stub page deleted after real pages inserted.
- Reorder panel: drag-drop QListWidget, Apply button calls `IPdfEditorEngine::reorderPages` via sequential-move algorithm, Reset button restores original order.
- `PagesMode::parsePageRange(expr, pageCount)` — static parser for comma-separated range expressions; returns sorted, deduplicated 0-based index list.
- `PagesMode::executeSplit(sourcePath, groups, outputDir, stemPattern)` — public test-seam; returns paths of produced files.
- `PagesMode::writeMinimalPdf(path)` — static helper; writes a minimal valid 1-page PDF stub used as the split output bootstrap.
- ModeController now injects AppContext into PagesMode (same pattern as BatchMode).
- TestPagesMode (4 headless tests): testPageRangeParser, testSplitAtPage (5-page → 2 parts), testSplitEveryNPages (6-page → 3 parts), testReorderPages (4-page [3,0,1,2] order).

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

## [1.0.0-internal] - 2026-05-23

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

GlyphPDF v1.0 ships in English. Arabic, French, and German translations are planned for a future release.

Qt Linguist translation framework wired. All three locales (ar/fr/de) lupdate-populated (1394 strings each, 0 translated — all `type="unfinished"`). `.qm` files embedded at `:/translations` via `qt_add_translations RESOURCE_PREFIX`. RTL audit complete for Arabic. Language selector in Preferences is present but non-English packs are no-ops in v1.0.

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

### M5-PROMPT-2 — Layout ensemble + cross-page pipelining (2026-06-02)
#### Added
- **ILayoutDetector** interface (`src/engines/ocr/ILayoutDetector.h`): `RegionType` enum (11 values), `LayoutRegion` struct (bbox/type/readingOrderIndex/confidence), pure-virtual `detect(QImage, Lane)`.
- **PpDocLayoutDetector** (`PpDocLayoutDetector.{h,cpp}`): PP-DocLayoutV2 ONNX via `models/pp_doclayout/pp_doclayout_v2.onnx` (Apache-2.0, PaddlePaddle official HF repo). Warm session (never spawned per-page). Input `[1,3,800,800]` float32 CHW + `scale_factor [1,2]`; output pred_logits + pred_boxes (RT-DETR); 25-class → `RegionType` mapping; sigmoid threshold 0.5.
- **SuryaDetector** (`SuryaDetector.{h,cpp}`): Clearly-named stub gated by `HAS_SURYA`. Surya code Apache-2.0; model weights Open Rail-M (commercial restriction for >$5M; redistribution not covered). Stub documents the license decision. When `HAS_SURYA=1`, dispatches `surya-detect` as QProcess subprocess (weights never link into binary, mirrors veraPDF AGPL pattern).
- **LayoutEnsemble** (`LayoutEnsemble.{h,cpp}`): Runs 1+ ILayoutDetector* in parallel via LaneScheduler GPU lane; IoU > 0.5 merges regions; confidence-weighted type voting; reading-order re-computed from centroids (top-to-bottom, left-to-right). Single-detector mode works without Surya.
- **OcrPipeline::recognizeDocument** (`OcrPipeline.cpp`): New `recognizeDocument(QList<QImage>) → QFuture<QList<PageOcrResult>>`. Uses `CrossPagePipeline` (stage1 layout ‖ stage2 per-region OCR fanout ‖ stage3 fusion). Layout regions captured via mutex per-page and threaded to `PageOcrResult.layoutRegions` without re-detection. Falls back to `QtConcurrent::run` sequential when no scheduler.
- **OCRMode confidence overlay** (`OCRMode.{h,cpp}`): `setOcrResults(QList<MergedOcrWord>)` populates real confidence-colored HTML overlay (green ≥90 / yellow 70-89 / red <70). `updateInfoStrip()` shows real AVG CONFIDENCE N% and LOW-CONFIDENCE WORDS N. Right-click context menu: "Re-OCR this region" emits `reOcrRegionRequested(bbox)`; per-region Accept/Reject workflow.
- **TestLayoutEnsemble** (10 tests): IoU computation, single/multi-detector merge, type voting, confidence threshold, reading-order.
- **TestOcrPipeline** (9 tests): ROVER merge, IoU, recognizeDocument page count, cross-page correctness (structural: page indices, words non-empty, no crash).
#### Removed (known-issues)
- Removed "OCR ensemble pipeline (PP-DocLayoutV2, Surya, LaneScheduler) not yet implemented" admission. WS1 is now complete for v1.0.0.

### Known Limitations (v1.0.0)
- MuPDF (AGPL) and Poppler (GPL) are never linked in-process (licensing constraint; by design).
- Send-for-signing workflow (remote signing order, reminders, audit trails) is not implemented — only local certificate-based signing exists. Deferred to v1.1.
- Cloud collaboration (`CollaborationManager`) is a stub; real-time multi-user sync is deferred to v2.0.
- Arabic/French/German UI translations are engineering scaffolding only (1394 strings each, marked unfinished); human-translated `.qm` files pending commissioning. English fallback active.
- Prune-missing-recent-files cleanup (M4-PROMPT-6 D4) not yet implemented.
- The rich Djot comment-composer toolbar for the CommentsWidget replies pane is deferred to a post-v1.0 polish release; entered comment text correctly dual-writes /RC + /PieceInfo.
- Installer logo dialog (WixUI banner BMP) is bundled but not shown during MSI install wizard — deferred to v1.1 packaging pass.
- Brand PNG assets are full-resolution (~1-2 MB each); runtime downscaling is acceptable for v1.0.0; optimized assets deferred.
