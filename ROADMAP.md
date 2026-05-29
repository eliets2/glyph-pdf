# GlyphPDF v1.0 — Full Engineering Roadmap (Amended)

**Last updated:** 2026-05-21 | **Total sessions:** 20 | **Estimated delivery:** 28–36 weeks from Session 1

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        GlyphPDF Application Layer                           │
│  GpMainWindow · Ribbon · ModeStrip · ThumbnailSidebar · AnnotationLayer    │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │
┌───────────────────────────────▼─────────────────────────────────────────────┐
│                      Dual-Model Core (NEW)                                  │
│                                                                             │
│  Structural Model (PDF object graph)   │  Semantic Model (SemanticDocument) │
│  PoDoFo 0.10.3+ ← sign/redact/forms   │  docmodel ← editing / interchange  │
│  PDFium (BSD-3)  ← render / search    │  pdfws_djot ← Djot ↔ Semantic      │
│  qpdf (Apache-2) ← linearize / repair │  liblua (MIT) ← Djot reference     │
│                                        │                    parser           │
│         ◄── EXPLICITLY LOSSY ──►      │                                     │
│    Structural ↔ Semantic boundary      │  Djot ↔ Semantic = LOSSLESS        │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │
┌───────────────────────────────▼─────────────────────────────────────────────┐
│               Heterogeneous Lane Scheduler (NEW — WS1)                      │
│                                                                             │
│  GPU Lane  ──────────────────────────────────────────────────────────────  │
│  Small concurrency · models loaded once into persistent warm worker        │
│  Reused ONNXRuntime session · long-lived sidecar with IPC                  │
│  NEVER spawn-per-page                                                       │
│                                                                             │
│  CPU Lane  ──────────────────────────────────────────────────────────────  │
│  Tesseract 5.5.x (core-count parallelism) · PP-layout detectors            │
│  Extended from existing QtConcurrent + QFutureWatcher (bounded scheduler)  │
│                                                                             │
│  Cross-page pipeline:  layout(P+1) ║ ocr(P) ║ fusion(P-1)                 │
└─────────────────────────────────────────────────────────────────────────────┘

FORBIDDEN:
  MuPDF  (AGPL-3.0) — will not be linked under any circumstance
  Poppler (GPL-2.0+) — will not be linked under any circumstance
```

---

## 🔒 Branch C SCOPE LOCK (decided 2026-05-26)

> **Team decision:** Real public v1.0.0 does not ship until every claim in PRD + this roadmap + the marketing copy is implemented. Estimated timeline: **~8-9 months** from M2 start (Sept 2026 target launch).
>
> All WS1 + WS2 + WS3 work below is **v1.0.0 scope** (not deferred to v1.1+). The current `dist\GlyphPDF-1.0.0-x64.msi` is **private/internal only** — NOT publishable to any public channel (GitHub Releases, winget, chocolatey, scoop, website) until M8 launch.
>
> Full execution plan: 34 prompts in `C:\Users\User\Desktop\GLYPH-PDF-MONTHS-2-8-PROMPTS.md` organized into 7 sprints (M2-M8). See `GLYPH-PDF-AUDIT-v1.0.0.md` SCOPE LOCK section for the realistic month-by-month schedule.

---

## Progress: Sessions 1–11 + M1 — Committed (commit a6ea6aa)

| Session | Phase | Description | Status |
|---------|-------|-------------|--------|
| 1 | 1 | DI modernization: AppContext shared_ptr + Bootstrapper | ✅ |
| 2 | 1 | ToolRegistry: ToolId enum + QAction dispatch | ✅ |
| 3 | 1 | IPdfBackend abstraction + PoDoFoBackend + licensing | ✅ |
| 4 | 2 | PDFium backend: rendering + 3-tier cache + tiled rendering | ✅ |
| 5 | 2 | qpdf backend: linearize + repair + save pipeline | ✅ |
| 6 | 3 | PAdES B-LT/B-LTA + certificate encryption | ✅ |

**Remediation note (May 2026):** A 7-session cleanup phase resolved all accumulated technical debt
before this roadmap was formalized: MainWindow modularization, `gp::` namespace migration, thread
data races, ineffective redactions, missing signature validation, buffer overflows (resolved via
PoDoFo/OpenSSL), and test-suite expansion. The project entered formal roadmap tracking in a clean
state.

**Key interfaces now committed:**
- `IPdfEditorEngine` — full page-ops surface including `cropPage`, `resizePage`, `reorderPages`,
  `addHeaderFooter`, `applyBatesNumbering`, `embedAnnotations`, image manipulation, and `applyRedactions`
- `ISignatureManager` — `PAdESLevel` enum (B_B/B_T/B_LT/B_LTA), `setTsaUrl`, `setSignatureLevel`,
  `signDocument`, `validateSignatures`
- `HeaderFooterOptions` and `BatesNumberingOptions` structs live in `IPdfEditorEngine.h`

---

## Implementation Roadmap

---

### Phase 1: Architecture Hardening (Sessions 1–3) — COMPLETE ✅

**Goal:** Establish a clean DI container, typed tool dispatch, and the three-backend abstraction.
All subsequent sessions build on this foundation.

- Session 1 ✅: `AppContext` converted to `shared_ptr`; `Bootstrapper` wires all singletons
- Session 2 ✅: `ToolId` enum + `QAction` dispatch; ribbon actions routed via `ToolRegistry`
- Session 3 ✅: `IPdfBackend` + `IPdfRenderer` + `IPdfSearcher` abstractions; `PoDoFoBackend`
  implements editing surface; license-compatibility audit embedded in `LICENSE-3RD-PARTY.md`

---

### Phase 1.5: Djot Interchange Foundation (NEW — Session 3.5 / Workstream 2)

**Goal:** Stand up `docmodel` + `pdfws_djot` as the semantic layer and establish the
Structural ↔ Semantic boundary contract. This phase is a prerequisite for Sessions 9 and 10
(OCR output → Djot → review UI) and Session 8 (annotation rich-text model).

**Rationale:** Djot (https://djot.net) was designed by the CommonMark/Pandoc author to be
unambiguous and backtrack-free. Its formal grammar eliminates the parsing inconsistencies that
make Markdown unsuitable as an interchange format in a security-critical document tool.

#### Session 3.5 Deliverables

**D1: `docmodel` library**
- `SemanticDocument` tree: Document → Section → Block (Paragraph/Table/Figure/List/Header/Footer/
  CodeBlock/RawBlock) → Inline (Span/Image/Link/Code/Math)
- No cycles; ownership model: `std::unique_ptr` for children, `std::weak_ptr` for cross-references
- `ProvenanceTag` on every node: `{BornPDF | BornDjot | BornOCR}` + source file + page + bbox
- Pure C++20, no Qt dependency (Qt types appear only in conversion utilities)

**D2: `pdfws_djot` library**
- Vendored Lua 5.4 reference implementation (MIT) as `third_party/lua/`; static link
- `IDjotCodec` interface: `QString encode(const SemanticDocument&)` / `SemanticDocument decode(const QString&)`
- `IDjotMapper` interface: `SemanticDocument fromPdf(const PdfStructure&)` / `PdfStructure toPdf(const SemanticDocument&)`
- PDF fidelity attributes on Djot nodes: `{pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}`
- Raw escape: `{=pdf}` opaque-blob blocks stored in sidecar `QHash<QString, QByteArray>`
- Djot spec version pinned in `pdfws_djot/DJOT_SPEC_VERSION` (commit the spec snapshot)

**D3: Provenance guard**
- `ProvenanceGuard::checkEditVia(ProvenanceTag, EditPath::DjotThenSave)` — throws
  `ProvenanceViolation` for `BornPDF + signed` documents
- For unsigned born-PDF: warn dialog, offer "Save as copy" branch
- Ships as a hard constraint: the guard is not a debug-only assertion

**D4: Round-trip property test**
- `tests/TestDjotRoundtrip.cpp`: generate 1000 random `SemanticDocument` instances via
  `docmodel::DocumentFuzzer`; encode→decode; assert structural equality
- Reject naive string-equality comparison: compare AST node types and attribute maps

**Success criteria:**
- [ ] `docmodel` and `pdfws_djot` compile and link under MinGW x64
- [ ] Round-trip property test passes (1000 docs)
- [ ] Provenance guard rejects born-PDF+signed edit path
- [ ] Djot spec version pinned + Lua vendored

**Constraint:** Do NOT implement Djot→PDF save-back in this phase. That is a Phase 6 deliverable.

---

### Phase 2: Multi-Backend + Rendering (Sessions 4–5) — COMPLETE ✅

- Session 4 ✅: PDFium backend; `PdfiumBackend` implements `IPdfRenderer` + `IPdfSearcher`;
  3-tier `RenderCache` (256 MB default, LRU-by-bytes); viewport prefetch ±3 pages;
  tiled rendering for pages > 64 megapixels
- Session 5 ✅: `QpdfBackend`; save pipeline (PoDoFo → qpdf linearize → atomic rename);
  signed-document guard (never route signed docs through qpdf); repair-on-open fallback

---

### Phase 3: Security (Sessions 6–7)

**Session 6 — PAdES B-LT/B-LTA + Certificate Encryption** ✅

- DSS dictionary with VRI (certs + OCSP + CRL), written as incremental update
- OCSP client (OpenSSL `OCSP_request_new`; cache for DSS embedding)
- `/DocTimeStamp` document-level timestamp → B-LTA
- `PAdESLevel` enum committed to `ISignatureManager`
- `/PubSec` certificate encryption (`CMS_encrypt`, AES-256, RSA-wrapped per recipient)
- Mock + tests updated

**Session 6 — Security Hardening (v1.0.0-PROMPT-2 audit)** ✅

- ✅ **VRI key spec-conformance** (B3 closed): VRI key is now SHA-1 of raw `/Contents` bytes
  (not hex-decoded bytes); removed `TODO(audit-2026-05-23)` hex round-trip
- ✅ **Real CMS trust policy** (B4 closed): `validateSignatures` now uses Windows system
  trust store via `CertOpenSystemStoreA` (or custom `signing/trustStorePath` QSettings key);
  adds `X509_VERIFY_PARAM` with CRL-check and SMIME-sign purpose; signing-time check;
  EKU enforcement; `UntrustedChain` trustStatus; removes self-admission `qWarning`
- ✅ **OCSP response verification** (B5 closed): `OCSP_basic_verify` called before embedding
  any OCSP response in DSS; malformed/unverified responses rejected with `qWarning`;
  signature degrades to B-T rather than embedding unverified revocation data
- ✅ **ByteRange overlap rejection**: overlapping ranges detected before shadow-attack check;
  `ByteRangeOverlap` trustStatus set
- ✅ **TSA token buffer**: 16 KB → 32 KB to accommodate multi-cert TSA chains
- ✅ **Exception propagation**: `extractSignatureContentsRaw` no longer swallows all exceptions
- ✅ **Size-validation hardening**: `i2d_X509`, `i2d_PrivateKey`, `BIO_new_mem_buf` return
  values checked; early returns with logged OpenSSL error strings on failure
- ✅ **New test**: `TestSignatureRealCrypto.cpp` linked against real engine (not mock);
  QSKIP if fixtures absent

**Session 7 — Redaction Hardening + Extended Sanitization** (in progress — M2-PROMPT-1 complete)

- **D1: Glyph-advance normalization** ✅ DONE (M2-P1, 2026-05-29) — Edact-Ray (Bland et al.,
  PETS 2023 / arXiv 2206.02285) defense. `GlyphAdvanceCalculator` computes sum-of-advances via
  three encoding paths; redaction surgery emits numeric-only `[N] TJ` gap (exact advance sum)
  — NO space glyph, so attacker cannot reconstruct removed characters from gap widths.
  D2 (PoDoFoBackend numeric-only TJ) + D3 (3 regression tests) also complete. Total: 16/16
  TestRedaction cases passing.
- **D2: OCR text-layer scrub** ✅ DONE (M2-P2, 2026-05-29) — invisible text from prior OCR
  operations falling within redaction rectangles removed from the invisible text stream.
  `redactCanvasRecursively` now uses existing `currentRenderingMode` (Tr) tracking to suppress
  Tj/TJ/'/\" operators with Tr==3 that intersect redaction rects. No Edact-Ray gap emitted for
  invisible scrubs. Regression test `testOCRScrubbing` rewritten to be mechanistically correct
  (checks content stream operators, not raw bytes of compressed stream).
- **D3: Structure-tree scrub** — walk `/StructTreeRoot`; clear `/ActualText`, `/Alt`, `/E`
  entries containing redacted content; remove marked-content structure elements in redacted regions
- **D4: Redaction audit log** — JSON sidecar (`.redaction-log.json`) per operation:
  timestamp, page, region, operations performed, before/after SHA-256 hashes
- **D5: Extended `sanitizeDocument`** — adds 15+ vectors to the existing 6-pass sanitizer:
  `/StructTreeRoot`, `/PieceInfo`, `/Thumb` (thumbnails), `/OCProperties` (flatten),
  `/Outlines` (bookmarks to removed content), RichMedia annotations, output intents,
  `/Collection`, trailer `/ID` (randomize second element), form field values, annotations
  with redacted `/Contents`
- **D6: Tests** — glyph-advance normalization, OCR layer scrub, extended sanitization coverage

**Constraint:** NEVER draw black rectangles as redaction — always excise from content stream.

---

### Phase 4: Content Tools (Sessions 8, 11, 12, 13)

**Session 8 — Forms Completion + Text Editing + Annotations**

- Complete AcroForm field types: radio (group + `RadiosInUnison`), ComboBox (Edit flag),
  ListBox (MultiSelect), date/numeric (format+validate actions) — all via PoDoFo
- FDF and CSV form data import/export; form flattening
- Floating text-editing toolbar: font picker, size selector, color picker, bold/italic/alignment
- Missing annotation subtypes: strikeout, squiggly underline, stamp
  (Approved/Rejected/Confidential/Draft + custom image), callout with leader line — all generate
  `/AP` appearance streams
- Comment threading: `AnnotationItem` gains `replies` list + `ReviewState` enum;
  `CommentsWidget` threaded view with filtering; ISO 32000 `/IRT` linking
- **WS2 integration:** `AnnotationItem` rich-text model backed by Djot internally; transcoded to
  PDF spec's XHTML subset (`/RC`) and plain text (`/Contents`) on save; original Djot stashed in
  `/PieceInfo`

**Session 11 — Page Operations: Crop, Resize, Headers, Bates, Drag-Drop**

Interface already committed in `IPdfEditorEngine.h`. Implementation deliverables:

- `cropPage` → sets `/CropBox` via PoDoFo; `CropPageCommand` for undo/redo
- `resizePage` → sets `/MediaBox`; stores before/after boxes
- `addHeaderFooter` — content-stream injection with `{page}`, `{total}`, `{date}`,
  `{filename}` template placeholders; positions: 6 header/footer anchors
- `applyBatesNumbering` — sequential counter injection; format `PREFIX-000001-SUFFIX`
- Drag-and-drop page reordering in `ThumbnailSidebar`: custom `QMimeData`, insertion-line
  indicator, `ReorderPageCommand`
- Selection rectangle tool in `PdfViewerWidget` for crop region definition

**Session 12 — Search & Navigation**

- Find-and-replace: PDFium for highlighting, PoDoFo content-stream for replace; regex via
  `QRegularExpression`; match options (case/whole-word/regex); results counter
- Bookmark panel (`BookmarkPanel`): parses `/Outlines` hierarchy; click → navigate; tab in sidebar
- Search scope selector: Document Text / Comments / Bookmarks / All
- Jump-to-page field in status bar
- Back/forward page history (Alt+Left, Alt+Right)

**Session 13 — Watermark + MRC Compression Pipeline (EXPANDED by WS3)**

See Phase 6 below for the full MRC expansion.

---

### Phase 5: OCR Ensemble Pipeline (Session 9 — EXPANDED by Workstream 1)

**Goal:** Upgrade from a simple dual-engine OCR to a layout-first, dual-ensemble,
hardware-lane-scheduled pipeline. The lane scheduler established here is a shared
infrastructure component reused by all subsequent concurrent workloads.

#### Architecture

```
Page image
    │
    ▼
[Layout Detector Ensemble]  ← PP-DocLayoutV2 + PP-StructureV3 OR Surya
    ├─ Detector A (parallel)
    └─ Detector B (parallel)
         │
         ▼ IoU reconciliation; confidence tiebreak on disagreement
[Region map: title / paragraph / table / figure / list / header / footer]
[Reading order established]
    │
    ├─────────────────────────────────────────────────────┐
    │  GPU Lane (serialized, warm worker)                  │  CPU Lane (core-count)
    │  ONNXRuntime PP-OCRv5 (Apache-2.0)                  │  Tesseract 5.5.x
    │  Persistent session — NEVER spawn-per-page           │  QFutureWatcher pool
    └─────────────────────────────────────────────────────┘
         │                                                  │
         ▼ Word-level ROVER merge                           │
    Confidence-weighted fusion                              │
    (secondary authoritative only when Tesseract            │
     MeanTextConf < 70 — NOT naive char-level majority)    │
         │◄──────────────────────────────────────────────────
         ▼
[Fused per-word result with confidence score]
    │
    ▼
[WS2] Map to Djot: reading-order → block-order, region-type → container class,
      bbox/confidence → attributes, tables → pipe tables
    │
    ▼
[WS3] Word boxes → invisible text sandwich layer (MRC pipeline)
```

#### Session 9 Deliverables (expanded)

**D1: Tesseract 5.5.x via MSYS2 MinGW packages**
- `HAS_TESSERACT` guard; MSYS2 `mingw-w64-x86_64-tesseract-ocr` package
- Build docs updated (remove "needs VS" comment)

**D2: RapidOcrEngine (PP-OCRv5 via ONNXRuntime)**
- `src/engines/ocr/RapidOcrEngine.h/.cpp` — implements `IOcrEngine`
- ONNXRuntime C++ API (MIT); `HAS_RAPIDOCR` guard; det/cls/rec ONNX models (~30 MB bundled)
- Session loaded once into `OrtSession`; kept warm across pages

**D3: OcrPreprocessor**
- `src/engines/ocr/OcrPreprocessor.h/.cpp`
- DPI normalize → orientation detect → deskew (Leptonica `pixDeskew`) → denoise →
  Sauvola binarize
- Returns preprocessed `QImage` + coordinate transform matrix for bbox re-projection

**D4: Layout Detector Ensemble**
- `src/engines/ocr/LayoutDetector.h/.cpp` — `ILayoutDetector` interface
- Two concrete detectors: `PP-DocLayoutV2Detector` + `SuryaDetector` (or a stub if model
  unavailable at compile time)
- IoU reconciliation: regions with IoU > 0.5 between detectors are merged; confidence tiebreak
  on remaining disagreements
- Output: `QList<LayoutRegion>` with type enum + bbox + reading-order index

**D5: Lane Scheduler**
- `src/engines/ocr/LaneScheduler.h/.cpp`
- GPU lane: `QSemaphore`-bounded queue (default: 2 concurrent ONNX inferences); persistent
  worker thread holds warm `OrtSession` + warm `ONNXRuntime` environment
- CPU lane: `QtConcurrent::mapped` with `QThreadPool` sized to `QThread::idealThreadCount()`
- Task tagging: `enum class Lane { GPU, CPU, Any }`
- Bounded scheduler: max-in-flight cap per lane to prevent OOM on large documents
- Cross-page pipelining: scheduler issues `layout(P+1)` while `ocr(P)` is running and
  `fusion(P-1)` is writing; implemented as a 3-stage pipeline with `QFuture` chaining
- **Architecture note:** This scheduler is a Phase 1/2 infrastructure dependency, not OCR-only.
  Reused for WS3 (MRC per-page compression) and any future GPU-accelerated workload.

**D6: OcrPipeline with word-level ROVER merge**
- `src/engines/ocr/OcrPipeline.h/.cpp`
- Strategies: `PrimaryOnly`, `ConfidenceWeighted` (secondary overrides only when Tesseract
  `MeanTextConf` < 70), `RoverVote` (full ROVER on both engines)
- Word-level alignment by IoU > 0.5
- Rejects naive character-level majority vote (documented anti-pattern: increases error rate
  when engines are correlated)

**D7: WS2 integration — OCR output maps to Djot**
- Post-fusion: `OcrDjotMapper::fromRegions(QList<FusedWord>, QList<LayoutRegion>)` →
  `SemanticDocument`
- Reading-order index → block order; region type → Djot container class
  (paragraph/header/figure/table/etc.)
- Per-word bbox + confidence → Djot attributes `{pdf-page pdf-bbox ocr-conf}`
- Tables → Djot pipe-table syntax

**D8: OcrMode UI updates**
- Engine selector (Tesseract only / PP-OCRv5 only / ROVER ensemble)
- Preprocessing options panel
- Per-word confidence coloring overlay (green ≥ 90 / yellow 70–89 / red < 70)
- Per-region redo: user can trigger re-OCR on a single region
- "Review before save" workflow using Djot SemanticDocument as editing model

**Constraints:**
- DO NOT use PaddlePaddle runtime — ONNXRuntime only for PP-OCRv5
- DO NOT spawn a new ONNX process per page — persistent warm worker mandatory
- DO NOT use character-level voting — word-level ROVER only
- DO NOT require both engines — Tesseract alone must work

---

### Phase 6: Conversion + MRC Compression Pipeline (Sessions 10, 13 — EXPANDED by WS2 + WS3)

**Session 10 — Conversion Pipeline + Visual Diff**

- PDF → HTML: PDFium text extraction + positional CSS layout; single HTML file per PDF
- PDF → image: configurable DPI; PNG / JPEG / TIFF; per-page or all-pages
- PDF → CSV: table detection via horizontal/vertical line clustering + text-alignment heuristic
- Office → PDF: headless LibreOffice (`soffice --headless`); `HAS_LIBREOFFICE` guard
- `DiffEngine`: SHA-256 → page count → Myers per-word text diff → pixel diff (XOR at 150 DPI)
- `CompareWidget`: synchronized side-by-side scrolling; text-change highlights; pixel-diff
  overlay toggle; change summary
- `BatchMode` execution loop: file list + operation selector; sequential or `QtConcurrent`
  parallel; per-file + overall progress bars; continue-on-failure with error log
- **WS2 integration:** `ConversionManager` can export as Djot (`IDjotCodec::encode`) for
  downstream processing; Djot→PDF content-stream generation available via `IDjotMapper::toPdf`

**Session 13 — Watermark + MRC Compression Pipeline (WS3 expanded)**

This session merges the original watermark + compression scope with the MRC pipeline from
Workstream 3. DjVu output is explicitly excluded; MRC runs inside PDF/A.

**D1: Text watermark rendering**
- `addWatermark(options)` injects semi-transparent diagonal text via ExtGState opacity operators
- Options: text, font, size, color, opacity, rotation, position, page range

**D2: Image watermark rendering**
- `addImageWatermark(imagePath, options)` — image XObject with opacity; tile/center/corner modes

**D3: MRC layer separation**
- `src/engines/mrc/MrcPageProcessor.h/.cpp` — `IPageMaskSeparator` interface
- Per-page: foreground mask (bitonal text + line-art) + background layer (continuous tone)
- Uses layout-region map from WS1 `LayoutDetector` to guide separation
- Foreground encoded as JBIG2 (verify chosen JBIG2 encoder is NOT GPL/AGPL — see license matrix)
- Background encoded as JPEG2000 via OpenJPEG (BSD-2, confirmed compatible)

**D4: MRC sandwich assembly**
- Invisible OCR text layer generated directly from WS1 fused word boxes (NOT hOCR round-trip)
- Text positioned to word-box coordinates from `OcrPipeline` output
- Three-layer PDF page: JBIG2 foreground + JPEG2000 background + invisible text overlay
- Target: 5–10× size reduction vs. naive image-PDF for scanned content

**D5: PDF/A export with MRC**
- `exportPdfA(outputPath, conformanceLevel)` extended to accept `MrcMode::{Off, Lossless, Balanced}`
- Conformance: PDF/A-2b (allows JPEG2000 + JBIG2)
- Validation step: reject output that fails PDF/A conformance constraints

**D6: DjVu importer (conditional, legacy ingestion only)**
- `HAS_DJVU` compile guard
- Import-only: parse DjVu structure → extract text + image layers → assemble as MRC PDF/A
- Never a DjVu exporter

**D7: Size estimation in CompressDialog**
- "Current: 45.2 MB → Estimated: 12.8 MB (72% reduction)" before user confirms
- Estimate based on image count/DPI analysis + font analysis + MRC eligibility heuristic

**D8: Standard compression (non-MRC path)**
- Downsample images above DPI threshold; hash-based XObject dedup; font subsetting;
  remove unused named destinations + old linearization data; metadata strip mode

**Constraints:**
- DO NOT add DjVu as an output format — import-only
- DO NOT generate MRC text layer from hOCR — use WS1 word boxes directly
- DO NOT use a GPL or AGPL JBIG2 encoder — verify license before integration
- DO NOT apply watermark to signed pages — warn user

---

### Phase 7: Quality + Polish (Sessions 14–19)

**Session 14 — Accessibility**
- `QAccessibleInterface` for all custom widgets (Ribbon/ModeStrip/ThumbnailSidebar/
  AnnotationLayer/FindBar/StatusBar/InspectorWidget)
- Tab order: Ribbon tabs → active panel → sidebar → central view → status bar
- 2px accent-color focus indicator in both dark and light themes
- Windows High Contrast API detection → high-contrast QSS (7:1 minimum contrast)
- `ShortcutHelpDialog` (F1) listing all shortcuts by category

**Session 15 — Localization**
- Qt Linguist integration; initial locales: en-US, fr-FR, ar-AE (RTL)
- RTL layout mirroring for Arabic; right-to-left PDF rendering direction
- Locale-aware date/number formatting in headers, Bates numbers, status bar

**Session 16 — Error Handling + Resilience**
- Structured error model: `GpError` hierarchy; no raw `QString` error returns
- Crash recovery: document auto-save every N minutes; restore on next launch
- Corrupted file diagnostics: classify PoDoFo parse errors; user-visible remediation steps
- Memory pressure handler: evict `RenderCache` tiers under OS memory pressure signals

**Session 17 — Installer + Packaging (WiX MSI)**
- WiX 4 MSI: per-user and per-machine install modes; Start Menu + Desktop shortcuts;
  file-type associations (.pdf); uninstaller
- MSYS2 MinGW runtime bundled; Qt plugins bundled; vcpkg DLLs bundled
- Code-signed MSI (self-signed for internal; real cert for distribution)

**Session 18 — Auto-Update Mechanism**
- Sparkle-style update check: HTTPS JSON feed; version comparison; silent download
- Delta-patch support (bsdiff); background download; user-visible progress + deferral
- Rollback: keep previous version MSI for one update cycle

**Session 19 — Print + Export Polish + Final UI Refinements**
- Print pipeline: `QPrinter` with per-page range, duplex, fit-to-page; print preview
- Export polish: PDF/A validation dialog; export progress for large documents
- Final UI: pixel-perfect icon audit; missing tooltips; keyboard shortcut gaps; theme
  consistency review; About dialog with third-party license viewer (reads `LICENSE-3RD-PARTY.md`)

---

### Phase 8: Release (Session 20)

**Session 20 — Integration Testing + Performance Benchmarks + v1.0 Freeze**

- End-to-end integration test suite: open → edit → sign → redact → export for 5 document
  archetypes (signed legal, scanned invoice, fillable form, large technical document, RTL)
- Performance benchmarks: render latency (p50/p95/p99 at 1×/2× DPI), OCR throughput
  (pages/sec on CPU-only and GPU-accelerated paths), MRC compression ratio vs. file size,
  memory high-water mark under 500-page document
- v1.0 freeze: tag `v1.0.0`; freeze `SESSION_PROMPTS_V2.md` as historical; publish
  `CHANGELOG.md` from commit history

---

## Architecture Dependency Graph

```
Session 1 (DI/AppContext)
    └─► Session 2 (ToolRegistry)
            └─► Session 3 (IPdfBackend/PoDoFo)
                    ├─► Session 3.5 (Djot/docmodel) [Phase 1.5 — WS2 Phase 1]
                    │       └─► Session 8  (Forms+Annotations — WS2 annotation model)
                    │       └─► Session 9  (OCR→Djot output — WS1+WS2)
                    │       └─► Session 10 (Djot export — WS2)
                    │       └─► Session 13 (Djot sandwich text — WS2+WS3)
                    ├─► Session 4 (PDFium/RenderCache)
                    │       └─► Session 10 (Conversion: PDF→HTML/image/CSV)
                    │       └─► Session 12 (Search: PDFium text extraction)
                    ├─► Session 5 (qpdf/save pipeline)
                    │       └─► Session 13 (MRC PDF/A export)
                    ├─► Session 6 (PAdES B-LT/B-LTA) ✅
                    │       └─► Session 7  (Redaction hardening)
                    ├─► Session 7  (Redaction/Sanitization) [Phase 3]
                    ├─► Session 8  (Forms+Annotations) [Phase 4]
                    ├─► Session 9  (OCR Ensemble+LaneScheduler) [Phase 5 — WS1]
                    │       └─► Session 10 (BatchMode OCR)
                    │       └─► Session 13 (MRC text layer)
                    ├─► Session 11 (Page Ops: Crop/Resize/Bates/DnD)
                    ├─► Session 12 (Search+Navigation)
                    └─► Session 13 (Watermark+MRC — WS3) [Phase 6]
                            └─► Sessions 14–19 (Quality/Polish)
                                    └─► Session 20 (v1.0 Freeze)
```

**Lane scheduler note:** The `LaneScheduler` introduced in Session 9 is an infrastructure
component; Sessions 10, 13, and any future GPU workload should adopt it rather than
implementing ad-hoc concurrency.

---

## Risk + Anti-Pattern Register

| # | Risk | Severity | Mitigation |
|---|------|----------|------------|
| R1 | qpdf flattens xref, invalidating signatures | HIGH | Save pipeline guard: check `SignatureManager::hasSignatures()` before routing through qpdf; test with signed fixture |
| R2 | Edact-Ray glyph-advance side channel in redaction | ✅ MITIGATED | M2-P1 D1/D2: numeric-only `[N] TJ` gap (no space glyph); 3 regression tests added |
| R3 | PAdES B-LTA archive timestamp drift (TSA clock skew) | MEDIUM | Validate TSA response `genTime` against system clock ±5 min; log skew |
| R4 | ONNXRuntime session not warm → latency spike on first page | HIGH | `LaneScheduler` initializes GPU lane worker at application startup; model files loaded once |
| R5 | Layout ensemble IoU < 0.5 on complex pages (mixed RTL/LTR) | MEDIUM | Fallback: use higher-confidence detector's output alone; surface low-confidence flag to UI |
| R6 | JBIG2 encoder chosen has GPL/AGPL license | HIGH | Audit before integration; acceptable options: jbig2enc fork (Apache-2.0 fork exists) or libjbig2 (BSD-like); update license matrix before merging WS3 |
| R7 | Djot→PDF save-back corrupts signed documents | HIGH | `ProvenanceGuard` blocks this path at the API level; integration test with signed born-PDF |
| R8 | MRC PDF/A output fails conformance validation | MEDIUM | Run veraPDF (AGPL; subprocess-only) in CI; never link it in-process |
| R9 | PP-OCRv5 model weights license not Apache-2.0 for commercial use | MEDIUM | Verify PaddleOCR model license at model download time; add to license matrix |
| R10 | Cross-page pipeline `layout(P+1) ║ ocr(P)` produces out-of-order results on error | MEDIUM | `LaneScheduler` result queue is ordered by page index; missing pages produce a sentinel error result, not a gap |
| R11 | Lua interpreter surface area in `pdfws_djot` (arbitrary code via Djot input) | MEDIUM | Lua sandbox: disable `io`, `os`, `loadfile`, `require` before running user-supplied Djot; no network |
| R12 | Large documents exhaust `RenderCache` (256 MB default) at 2× DPI | MEDIUM | Cache is LRU-by-bytes with configurable cap; expose `QSettings` key `RenderCacheMB` |

**Anti-patterns explicitly prohibited:**

| Anti-pattern | Reason |
|--------------|--------|
| Spawn ONNX process per page | Startup cost > inference cost; defeats GPU warm-up |
| Naive character-level OCR majority vote | Increases error rate when engines are correlated; ROVER is word-level |
| Black rectangle redaction | Visually obstructs but does not excise from content stream; Edact-Ray recovers text |
| qpdf in the signing path | Flattens xref, invalidates `/ByteRange` |
| Linking MuPDF in-process | Forces entire application to AGPL-3.0 |
| Linking Poppler in-process | Forces entire application to GPL-2.0+ |
| Running veraPDF as a linked library | veraPDF is AGPL; invoke as subprocess in CI only |
| DjVu output format | Out of scope; import-only if legacy corpus ingestion needed |
| End-to-end VLM for layout | Rejected in favor of modular pipeline (layout → OCR → fusion) |
| Djot grammar reimplementation | Vendor Lua reference parser; do not reimplement |

---

## Third-Party License Matrix

| Library | Version | License | Compatible | Role | Notes |
|---------|---------|---------|:---:|------|-------|
| Qt 6 | 6.10.2 | LGPL-3.0 / commercial | Yes | UI framework | Dynamic linking, LGPL compliant |
| PoDoFo | 0.10.3+ | LGPL-2.0+ OR MPL-2.0 | Yes | PDF editing/signing | Dynamic linking |
| OpenSSL | 3.x | Apache-2.0 | Yes | Crypto/OCSP/CMS | |
| PDFium | current | BSD-3-Clause | Yes | Rendering/text/search | Pre-built binaries |
| qpdf | current | Apache-2.0 | Yes | Linearize/repair | Optional (HAS_QPDF) |
| Tesseract | 5.5.x | Apache-2.0 | Yes | OCR engine (CPU) | MSYS2 package |
| Leptonica | 1.x | BSD-2-Clause | Yes | Image preprocessing | Tesseract dependency |
| ONNXRuntime | current | MIT | Yes | PP-OCRv5 inference | GPU + CPU execution providers |
| RapidOCR-Onnx models (PP-OCRv5) | current | Apache-2.0 | Yes | Secondary OCR | Verify at download; ~30 MB |
| PP-DocLayoutV2 / PP-StructureV3 weights | current | Apache-2.0 | Yes | Layout detection | Verify at download |
| Surya (if used) | current | GPL-3.0 | **VERIFY** | Layout detection | **If GPL: subprocess only or replace with Apache model** |
| liblua (Lua 5.4 reference) | 5.4.x | MIT | Yes | Djot grammar parser | Vendored in `third_party/lua/` |
| OpenJPEG | 2.x | BSD-2-Clause | Yes | JPEG2000 (MRC background) | |
| JBIG2 encoder | TBD | **TBD — must not be GPL/AGPL** | TBD | MRC foreground | Audit before integration |
| OpenXLSX | 1.x | BSD-3-Clause | Yes | XLSX export | |
| DuckX | 1.x | MIT | Yes | DOCX export | |
| MuPDF | ANY | **AGPL-3.0** | **FORBIDDEN** | — | Linking forces app to AGPL |
| Poppler | ANY | **GPL-2.0+** | **FORBIDDEN** | — | Linking forces app to GPL |
| veraPDF | ANY | AGPL-3.0 | Subprocess only | PDF/A validation (CI) | Never link in-process |

**Action items before merging Workstream 3:**
1. Identify and audit JBIG2 encoder candidate; update this table before Session 13 begins
2. Confirm PP-OCRv5 and PP-DocLayoutV2 model weight licenses at download time (Apache-2.0 claimed)
3. If Surya is selected as a layout detector, confirm it is run as an isolated subprocess or
   replaced with a non-GPL alternative

---

## Forbidden Dependencies

```
MuPDF  (AGPL-3.0)       — FORBIDDEN: never link in-process
Poppler (GPL-2.0+)      — FORBIDDEN: never link in-process
DjVu output format      — EXCLUDED: import-only path is the only permitted use
veraPDF (AGPL-3.0)      — CI subprocess only: never link in-process
```

```
ANTI-PATTERNS — prohibited in all sessions:

  Spawn-per-page model loading   → use persistent warm LaneScheduler worker
  Naive char-level OCR vote      → word-level ROVER only
  Black rectangle redaction      → always excise from content stream
  qpdf in signing path           → flattens xref, invalidates ByteRange
  End-to-end VLM for layout      → modular pipeline: layout → OCR → fusion
  Djot grammar reimplementation  → vendor Lua reference parser
```

---

*Reference: SESSION_PROMPTS_V2.md (detailed per-session deliverables + success criteria)*
*Methodology: GSD Pipeline + 7-H Prompt Engineering*
*Amended: 2026-05-21 — incorporated Workstream 1 (OCR Ensemble), Workstream 2 (Djot),
Workstream 3 (MRC Compression); Phase 1.5 added; Sessions 9 and 13 expanded*
