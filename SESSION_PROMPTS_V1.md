# GlyphPDF v1.0 — Execution Session Prompts

Generated: 2026-05-19
Based on: Technical Validation & v1.0 Roadmap (research output)
Total sessions: 10
Estimated total: 22-28 weeks of work

---

## SESSION 1 of 10 — DI Modernization: AppContext shared_ptr + Bootstrapper

**Scope:** Replace all raw pointers in AppContext with `std::shared_ptr<I…>` injection from a new Bootstrapper class. This is the foundation for safe multi-backend integration.

**Agents:** `/backend` (primary), `/refactor` (secondary for safe migration)

---

### Context

You are working on GlyphPDF, a C++17/Qt 6.10.2 desktop PDF editor at `C:\Users\User\Projects\pdf`.

**Current problem:** `AppContext` (at `src/core/AppContext.h`) holds 6 raw interface pointers + 2 raw object pointers. These are created in `main.cpp` as stack/heap objects and passed by raw pointer. This is unsafe — no lifetime guarantees, no shared ownership, and blocks the multi-backend refactor coming next.

**Architecture:**
- 4 static libraries: `pdfws_core` (INTERFACE) → `pdfws_engines` (STATIC) → `pdfws_commands` (STATIC) → `pdfws_ui` (STATIC)
- 6 interfaces: `IPdfEditorEngine`, `IOcrEngine`, `IFormManager`, `ISignatureManager`, `IConversionEngine`, `ICollaboration`
- Plus: `QUndoStack*` and `DocumentSession*`
- 7 controllers in `gp::` namespace consume AppContext
- 10 test targets use MockXxx implementations via AppContext
- Build: MinGW 13.1.0, CMake, vcpkg x64-mingw-dynamic

### Deliverables

1. **Refactor `AppContext`** (`src/core/AppContext.h`):
   - Replace all 6 `I*` raw pointers with `std::shared_ptr<IFoo>`
   - Replace `QUndoStack*` with `std::shared_ptr<QUndoStack>`
   - Replace `DocumentSession*` with `std::shared_ptr<DocumentSession>`
   - Keep the struct layout simple — no builder pattern needed for 8 members

2. **Create `Bootstrapper`** (new file: `src/app/Bootstrapper.h/.cpp`):
   - Single class that constructs all engines and returns a fully populated `AppContext`
   - `AppContext Bootstrapper::createContext()` method
   - Encapsulates the engine creation logic currently scattered in `main.cpp`
   - Add to `pdfws_engines` library target in CMakeLists.txt (it creates engine instances)

3. **Update `main.cpp`** (`src/app/main.cpp`):
   - Replace the 8 manual `new`/stack constructions with `Bootstrapper::createContext()`
   - Pass `const AppContext&` (or `const AppContext*`) to `gp::MainWindow`

4. **Update all consumers** — every file that receives `AppContext*`:
   - `GpMainWindow.h/.cpp` — change constructor and member to `const AppContext*` or `const AppContext&`
   - All 7 controllers in `src/shell/controllers/`
   - `ModeController` in `src/modes/ModeController.h/.cpp`
   - All mode panels that receive AppContext
   - Ensure consumers never take ownership (read through shared_ptr, don't store raw)

5. **Update all test mocks**:
   - `tests/mocks/` — MockSignatureManager, and any other mocks
   - Test setup functions that create AppContext must now use `std::make_shared<MockFoo>()`
   - All 10 test targets must compile and pass

### Verification

```bash
# Build
cd C:\Users\User\Projects\pdf
cmake --build build -- -j8 2>&1 | tee build_s1.log

# Test
set QT_QPA_PLATFORM=offscreen
set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins
cd build && ctest --output-on-failure -j4

# Check: zero raw pointer usage of interface types in AppContext
grep -rn "IPdfEditorEngine\*\|IOcrEngine\*\|IFormManager\*\|ISignatureManager\*\|IConversionEngine\*\|ICollaboration\*" src/core/AppContext.h
# Expected: 0 matches (all should be shared_ptr now)
```

### Anti-patterns to avoid
- Do NOT use `boost::di` or `kangaru` — manual DI is sufficient for 8 members
- Do NOT change interface signatures (only the pointer types in AppContext)
- Do NOT touch engine implementation internals — this session is purely about ownership
- Do NOT break the test suite — if any test fails, fix the mock setup, not the test logic

---

## SESSION 2 of 10 — ToolRegistry: ToolId Enum + QAction Dispatch + Controller Migration

**Scope:** Replace the fragile string-based `handles(QString)`/`activate(QString)` tool routing with a strongly-typed `ToolRegistry` + `ToolId` enum + `QAction` pattern. This is the second blocking architecture fix.

**Agents:** `/backend` (primary), `/refactor` (migration)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. The current tool dispatch works like this:
- Ribbon emits `toolActivated(QString toolId)` with string IDs like `"zoom-in"`, `"encrypt"`, `"edit-text"`
- `GpMainWindow::onToolActivated(QString)` loops through 7 controllers calling `handles(id)` then `activate(id)`
- Each controller has a big if/else-if chain matching strings
- Tool IDs are duplicated across controllers, tests, and ribbon model — no single source of truth
- Typos are silent failures (string `"signaure"` would just do nothing)

**Current controllers and their approximate tool ID counts:**
- HomeController: 7 IDs (open, save, saveAs, save-as, print, share, properties)
- ViewController: 17 IDs (zoomIn, zoom-in, zoomOut, zoom-out, actual, actual-size, fitWidth, fit-width, fitPage, fit-page, single, single-page, continuous, two, two-page, presentation, fullscreen, darkMode, eyeCare)
- EditController: ~30 IDs (edit-text, highlight, underline, strikeout, note, pencil, freehand, line, arrow, rect, oval, squiggly, signature, image, search, find, ocr, hand, select, selectObj, editObject, editText, textbox, addText, comment, markRedact)
- PagesController: 6 IDs (rotate-cw, rotate-ccw, delete-page, insert-page, extract, reorder)
- ConvertController: 4 IDs (to-word, to-excel, combine, compress)
- FormsController: 4 IDs (text-field, checkbox, radio, dropdown)
- SecurityController: ~16 IDs (encrypt, password, sign, validateSig, validate, sanitize, applyRedact, exportAnno, importAnno, cloud, permissions, removeSec, certify, timestamp, patternRedact, regexRedact)

Note: Many IDs have camelCase + kebab-case aliases (e.g., `zoomIn` and `zoom-in`). The new system should have ONE canonical ID per tool with aliases handled at the ribbon layer.

### Deliverables

1. **Create `ToolId` enum class** (new file: `src/core/ToolId.h`):
   - One enum value per unique tool (no duplicate aliases)
   - ~60-70 values covering all controllers
   - Group by category with comments
   - Add `ToolId_COUNT` sentinel for array sizing
   - Add `toString(ToolId)` and `fromString(QString)` helpers (the fromString handles aliases)
   - Add to `pdfws_core` target

2. **Create `ToolRegistry`** (new file: `src/shell/ToolRegistry.h/.cpp`):
   ```cpp
   class ToolRegistry : public QObject {
       Q_OBJECT
   public:
       void registerController(ToolId id, IToolController* ctrl);
       IToolController* controllerFor(ToolId id) const;
       QAction* actionFor(ToolId id) const;
       void activate(ToolId id);
   signals:
       void toolActivated(ToolId id);
   private:
       QHash<ToolId, IToolController*> m_controllers;
       QHash<ToolId, QAction*> m_actions;
   };
   ```
   - Each tool gets a `QAction*` (enables keyboard shortcut customization, accessibility, menu integration)
   - Add to `pdfws_ui` target

3. **Create `IToolController` interface** (new file: `src/core/interfaces/IToolController.h`):
   ```cpp
   class IToolController {
   public:
       virtual ~IToolController() = default;
       virtual QList<ToolId> handledTools() const = 0;
       virtual void activate(ToolId id) = 0;
       virtual bool isEnabled(ToolId id) const { return true; }
   };
   ```

4. **Migrate all 7 controllers** to implement `IToolController`:
   - Replace `bool handles(const QString&)` with `QList<ToolId> handledTools() const`
   - Replace `void activate(const QString&)` with `void activate(ToolId id)`
   - Remove all string matching; use switch/case on ToolId enum
   - Controllers register themselves with ToolRegistry in GpMainWindow constructor

5. **Update GpMainWindow**:
   - Own a `ToolRegistry*` member
   - In constructor: create registry, register all controllers
   - Connect Ribbon's signal through `fromString()` → `ToolRegistry::activate()`
   - Remove the old manual dispatch loop in `onToolActivated`

6. **Update RibbonModel** (`src/shell/RibbonModel.h/.cpp`):
   - Store `ToolId` per button (not bare strings)
   - Emit `toolActivated(ToolId)` signal (or keep string emission with `fromString()` adapter)

7. **Update TestControllers** (`tests/TestControllers.cpp`):
   - Test via `handledTools()` returning `QList<ToolId>` instead of string handles()
   - The no-overlap test should verify each ToolId appears in exactly one controller
   - All 11 tests must pass

### Verification

```bash
# Build
cmake --build build -- -j8

# Test all 10 targets
set QT_QPA_PLATFORM=offscreen
set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins
cd build && ctest --output-on-failure -j4

# Verify no remaining string-based handles()
grep -rn "handles(\"" src/shell/controllers/
# Expected: 0 matches
```

### Anti-patterns to avoid
- Do NOT add `boost::any` or variant-based dispatch — enum switch is simpler and faster
- Do NOT break the Ribbon visual/behavior — this is a wiring refactor only
- Do NOT remove alias support — `fromString()` must map both `"zoomIn"` and `"zoom-in"` to `ToolId::ZoomIn`
- Do NOT change how controllers interact with engines — only the dispatch path changes

---

## SESSION 3 of 10 — IPdfBackend Abstraction + PoDoFoBackend Extraction + Licensing

**Scope:** Introduce the `IPdfBackend` abstraction layer, extract all PoDoFo usage into a `PoDoFoBackend` class, add licensing safeguards, and upgrade PoDoFo to 0.10.6.

**Agents:** `/backend` (primary), `/refactor` (extraction), `/security` (licensing)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. Currently PoDoFo is used directly throughout `PdfEditorEngine`, `SignatureManager`, and `FormManager`. The research validated a triple-backend design:

- **PoDoFo** (LGPL/MPL) — content stream editing, AST manipulation, signing, encryption, AcroForm
- **PDFium** (BSD-3) — rendering, text extraction, search, form display (Session 4)
- **qpdf** (Apache-2.0) — linearization, repair, JSON inspection (Session 5)
- **MuPDF** — **AGPL-3.0, FORBIDDEN** (forces all downstream into AGPL)

Before adding PDFium and qpdf, we need a clean abstraction layer so backends can be swapped/composed.

### Deliverables

1. **Create `IPdfBackend` interface hierarchy** (new files in `src/core/interfaces/`):
   ```
   IPdfDocument.h    — loadDocument, saveDocument, pageCount, metadata, incremental save
   IPdfPage.h        — textContent, annotations, dimensions
   IPdfRenderer.h    — renderPage (returns QImage), renderTile
   IPdfWriter.h      — writeDocument, writeUpdate (incremental)
   IPdfSearcher.h    — searchText, searchRegex
   ```
   Keep these minimal — just enough to abstract current PoDoFo usage.

2. **Create `PoDoFoBackend`** (new files: `src/engines/podofo/PoDoFoBackend.h/.cpp`):
   - Implements `IPdfDocument`, `IPdfWriter`
   - Wraps existing `PdfEditorEngine::Private` logic
   - Moves PoDoFo `#include` statements OUT of any public header
   - All PoDoFo types are pimpl'd behind the .cpp file

3. **Refactor `PdfEditorEngine`** to delegate to `PoDoFoBackend`:
   - `PdfEditorEngine` becomes a facade that holds an `IPdfDocument*` backend
   - Editing operations (editTextInline, applyRedactions, encryptDocument, etc.) call through the backend
   - `PdfEditorEngine` still implements `IPdfEditorEngine` — the interface to the UI layer doesn't change
   - This is an internal reorganization, not an API change

4. **Add `BackendRouter`** (new file: `src/engines/BackendRouter.h/.cpp`):
   - Declarative routing: which backend handles which operation
   - For now, everything routes to PoDoFoBackend (single backend)
   - Structure ready for PDFium (Session 4) and qpdf (Session 5) to plug in

5. **Add MuPDF/AGPL guard in CMakeLists.txt**:
   ```cmake
   # FORBIDDEN: MuPDF (AGPL-3.0) — would force GlyphPDF into AGPL
   if(TARGET mupdf OR DEFINED MUPDF_FOUND)
       message(FATAL_ERROR "MuPDF detected — AGPL license is incompatible. Use PDFium instead.")
   endif()
   ```

6. **Create `LICENSE-3RD-PARTY.md`** at project root listing all dependencies with their licenses.

7. **Upgrade PoDoFo to 0.10.6** in vcpkg/CMake (latest stable with security fixes).

### Verification

```bash
# Build
cmake --build build -- -j8

# All tests pass (no API changes visible to tests)
set QT_QPA_PLATFORM=offscreen
cd build && ctest --output-on-failure -j4

# Verify no PoDoFo includes leak into public headers
grep -rn "#include.*podofo" src/core/ src/shell/ src/ui/ src/commands/
# Expected: 0 matches (only in src/engines/podofo/)

# Verify LICENSE-3RD-PARTY.md exists
test -f LICENSE-3RD-PARTY.md && echo "OK"
```

---

## SESSION 4 of 10 — PDFium Backend: Rendering Pipeline + 3-Tier Cache + Tiled Rendering

**Scope:** Add PDFium as a rendering backend via `bblanchon/pdfium-binaries`, implement `PdfiumBackend` for rendering/text/search, create a 3-tier render cache with viewport prefetch, and add tiled rendering for large pages.

**Agents:** `/backend` (primary), `/performance` (cache design)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. Currently rendering goes through Qt's `QPdfView` which wraps PDFium internally. But `QPdfView` has limited API — no sub-rect tiling, no direct access to PDFium's text extraction or form rendering. The research found that linking PDFium directly (BSD-3-Clause) gives us:
- Sub-rect rendering for tiled display of engineering drawings
- Direct text extraction with character positions
- Form field rendering
- Faster than going through Qt's abstraction

**Performance targets (from commercial benchmarks):**
- Open/first-page: < 1 s
- Navigate to page 500: < 250 ms
- Render one page at 150 DPI: < 80 ms
- RAM working set: < 350 MB for 1000-page PDF

### Deliverables

1. **Integrate PDFium** via `bblanchon/pdfium-binaries` (pre-built, BSD-3):
   - Add to CMakeLists.txt as an external dependency
   - Download and cache the MinGW-compatible binary
   - Link `pdfium` to `pdfws_engines`

2. **Create `PdfiumBackend`** (new files: `src/engines/pdfium/PdfiumBackend.h/.cpp`):
   - Implements `IPdfRenderer`, `IPdfSearcher`
   - `renderPage(int pageIndex, qreal dpi) → QImage`
   - `renderTile(int pageIndex, QRectF subRect, qreal dpi) → QImage`
   - `extractText(int pageIndex) → QString`
   - `searchText(int pageIndex, QString query) → QList<QRectF>`
   - Initialize PDFium library once (`FPDF_InitLibrary` in constructor, `FPDF_DestroyLibrary` in destructor)

3. **Update `BackendRouter`** to route rendering through PdfiumBackend:
   - `renderPage` → PdfiumBackend
   - `extractText` → PdfiumBackend
   - `searchText` → PdfiumBackend
   - All editing operations still go to PoDoFoBackend

4. **Implement 3-tier render cache** (new file: `src/engines/RenderCache.h/.cpp`):
   - Tier 1: Page metadata cache (always full — page count, dimensions, rotation)
   - Tier 2: Rendered page LRU keyed by `(pageIndex, scale)`, bounded by total bytes (default 256 MB, configurable)
   - Tier 3: Text-layer cache (once parsed, kept for search)
   - Thread-safe via `QReadWriteLock`

5. **Implement viewport prefetch**:
   - Background thread renders pages ±3 from current viewport
   - Uses `QtConcurrent::run` into the render cache
   - Cancels prefetch when user scrolls past prefetched range

6. **Implement tiled rendering** for pages > 64 MP at current zoom:
   - Detect when rendered raster would exceed threshold
   - Split into 1024×1024 tiles using `renderTile()`
   - Cache per-tile
   - `PdfViewerWidget` composites tiles in paint event

7. **Update `PdfViewerWidget`** to use `RenderCache` instead of its current `m_pageCache`:
   - Remove the old 20-page `m_pageCache`
   - Route through RenderCache for all page rendering
   - Keep `QPdfView` for scrolling/navigation UI but feed it cached images

### Verification

```bash
# Build
cmake --build build -- -j8

# All tests pass
cd build && ctest --output-on-failure -j4

# Manual test with a large PDF (if available):
# Open a 100+ page PDF, verify smooth scrolling, zoom in on a page, check tile boundaries
```

---

## SESSION 5 of 10 — qpdf Backend: Linearize + Repair + Save Pipeline

**Scope:** Add qpdf 12.x as a backend for linearization, structural repair, and the post-save pipeline. Upgrade PoDoFo to 0.10.6 if not done in Session 3.

**Agents:** `/backend` (primary)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. PoDoFo removed its linearization code (it was non-spec-compliant). qpdf (Apache-2.0) is the standard tool for:
- **Linearization** (web-optimized PDF): `QPDFWriter::setLinearization(true)`
- **Structural repair**: qpdf auto-repairs damaged xref tables on load
- **JSON inspection**: qpdf can dump PDF structure as JSON for debugging

**Critical constraint:** qpdf **cannot** write incremental updates. It flattens appended xref sections on save. Therefore:
- qpdf is a **post-processor**, never used in the signing path
- Save pipeline: PoDoFo writes → qpdf linearizes/repairs → final output
- For signed documents: PoDoFo incremental save ONLY (no qpdf post-processing)

### Deliverables

1. **Integrate qpdf** via vcpkg or pre-built:
   - Add `find_package(qpdf QUIET)` with `HAS_QPDF` guard
   - Link to `pdfws_engines`

2. **Create `QpdfBackend`** (new files: `src/engines/qpdf/QpdfBackend.h/.cpp`):
   - `linearize(inputPath, outputPath) → bool`
   - `repair(inputPath, outputPath) → bool` (auto-fix damaged xref)
   - `inspectJson(inputPath) → QJsonDocument` (debug tool)
   - `isLinearized(inputPath) → bool`

3. **Update `BackendRouter`**:
   - `linearize` → QpdfBackend
   - `repair` → QpdfBackend
   - All editing/signing still → PoDoFoBackend
   - All rendering → PdfiumBackend

4. **Implement save pipeline** in `PdfEditorEngine::saveDocument`:
   ```
   User clicks Save:
   1. PoDoFo writes to temp file (preserves structure, incremental if applicable)
   2. If user requested linearization AND document is NOT signed:
      → qpdf linearizes temp → final output
   3. If document has signatures:
      → PoDoFo incremental update ONLY (never qpdf)
   4. Atomic rename temp → target
   ```

5. **Implement repair-on-open** in document loading:
   - When PoDoFo fails to parse, attempt qpdf repair first, then retry PoDoFo on repaired copy
   - Show user a warning: "Document was damaged and has been repaired"

6. **Add tests**:
   - Test linearization produces valid output (if qpdf available)
   - Test repair pathway for a deliberately corrupted PDF
   - Test that signed documents skip qpdf post-processing

### Verification

```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# If qpdf is installed, verify linearization:
# Open a PDF → Save with linearization → verify with qpdf --check
```

---

## SESSION 6 of 10 — PAdES B-LT / B-LTA + Certificate Encryption

**Scope:** Upgrade the digital signature implementation from PAdES B-T to B-LT (long-term valid) and B-LTA (long-term archival). Implement certificate-based PDF encryption as an alternative to password-based.

**Agents:** `/security` (primary), `/backend` (crypto implementation)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. Current signature state:
- `SignatureManager` implements PAdES B-T (basic CMS + RFC 3161 timestamp)
- Missing: DSS dictionary, VRI sub-dictionary, document timestamp
- Adobe Reader only shows "Signature is LTV enabled" at B-LT level and above
- Without B-LT, signatures become unverifiable after the signing cert expires

**PAdES levels (ETSI EN 319 142-1):**
- **B-B**: Basic CMS signed data (have this)
- **B-T**: B-B + RFC 3161 timestamp token (have this)
- **B-LT**: B-T + DSS dictionary with VRI (certs, OCSP responses, CRLs) — **NEED THIS**
- **B-LTA**: B-LT + document timestamp re-sealing the DSS — **NEED THIS**

**Certificate encryption (separate from signing):**
- PDF supports `/Filter /PubSec` with AES key wrapped per recipient's RSA public key
- PoDoFo's `PdfEncrypt` doesn't support public-key encryption end-to-end
- Must implement via OpenSSL `CMS_encrypt()`

### Deliverables

1. **Implement PAdES B-LT** in `SignatureManager`:
   - After signing, fetch OCSP responses for the signer cert chain
   - Fetch CRL if OCSP unavailable
   - Create `/DSS` dictionary in the PDF catalog:
     - `/Certs` array — DER-encoded certificates (full chain)
     - `/OCSPs` array — OCSP response bytes
     - `/CRLs` array — CRL bytes
   - Create `/VRI` sub-dictionary keyed by uppercase-hex SHA-1 of the signature `/Contents`:
     - Per-signature `/Cert`, `/OCSP`, `/CRL` arrays
   - Write as incremental update (do NOT rewrite full document)

2. **Implement PAdES B-LTA**:
   - After DSS is written, add a separate document timestamp signature:
     - Signature type: `/DocTimeStamp`
     - SubFilter: `/ETSI.RFC3161`
     - ByteRange covers the entire DSS-augmented file
   - Write as another incremental update

3. **Add OCSP client** (new method in SignatureManager private class):
   - Parse Authority Information Access (AIA) extension from cert for OCSP URL
   - HTTP POST to OCSP responder
   - Parse and validate OCSP response (good/revoked/unknown)
   - Cache responses for DSS embedding

4. **Implement certificate-based encryption** (new methods in PdfEditorEngine):
   - `encryptWithCertificate(inputPath, outputPath, QList<QSslCertificate> recipients)`
   - Per-document random AES-256 key
   - Per-recipient: wrap AES key with recipient's RSA public key via `CMS_encrypt()`
   - Write `/Filter /PubSec`, `/SubFilter /adbe.pkcs7.s5` (AES-256)
   - Start with RSA-2048/3072; ECC behind feature flag

5. **Update ISignatureManager interface**:
   - Add `setSignatureLevel(PAdESLevel level)` — B_B, B_T, B_LT, B_LTA
   - Add `addLtvData(filePath)` for post-hoc LTV augmentation

6. **Add interop tests**:
   - Test PAdES B-LT output validates in Adobe Reader (manual check, document process)
   - Test DSS dictionary structure with qpdf JSON inspection
   - Test certificate encryption round-trip (encrypt → decrypt with private key)
   - Update MockSignatureManager for new interface methods

### Verification

```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# Manual interop check:
# 1. Sign a PDF with B-LT level
# 2. Open in Adobe Reader → verify "Signature is LTV enabled"
# 3. Verify with EU DSS demo validator (https://ec.europa.eu/digital-building-blocks/DSS/webapp-demo/validation)
```

---

## SESSION 7 of 10 — Redaction Hardening + Extended Metadata Sanitization

**Scope:** Fix the glyph-advance side channel in redaction (Edact-Ray attack), add OCR-layer scrub, structure-tree sanitization, and redaction audit logging. Extend the metadata sanitization to cover 15+ additional vectors.

**Agents:** `/security` (primary), `/backend` (implementation)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. Current redaction state:
- `PdfEditorEngine::applyRedactions` uses recursive AST content stream parsing
- Excises text operators from reconstructed command buffer
- Rejects binary/unparseable streams (safe-abort)
- **Vulnerability (Edact-Ray, USENIX Security '22):** Even properly excised redactions leak via glyph horizontal-advance widths. An attacker can measure the x-advance gaps left by removed characters and reconstruct the redacted text.

Current metadata sanitization (6 passes):
1. Trailer `/Info` removal
2. Catalog `/Metadata` (XMP) removal
3. `/EmbeddedFiles` and `/JavaScript` from Names
4. `/OpenAction` and `/AA` from Catalog
5. Page-level `/AA` and `/A`
6. AcroForm `/XFA`

**Missing sanitization targets (from research):**
- Structure tree (`/StructTreeRoot`) — carries `/ActualText`, `/Alt`, `/E`
- `/PieceInfo` — private application data per page
- Page thumbnails (`/Thumb`)
- Optional Content Groups (`/OCProperties`) — hidden layers with text
- Bookmarks (`/Outlines`) referencing redacted destinations
- RichMedia annotations
- Embedded color profiles (workplace identification)
- Trailer `/ID` array (forensic fingerprint)
- Form field values matching redacted text
- Annotations overlapping redacted regions
- Embedded font full character sets (should re-subset)

### Deliverables

1. **Fix glyph-advance side channel** in `applyRedactions`:
   - After excising text operators, normalize x-advances in the affected text run
   - Option A (simpler): Replace the gap with a fixed-width space glyph of appropriate width
   - Option B (better): Re-shape the entire text line so advances don't encode character widths
   - Add a "paranoid mode" flag that re-rasterizes the affected page region (pixel-burn)

2. **Add OCR text layer scrub**:
   - After visual redaction, check if pages have an invisible text layer (from prior OCR)
   - Remove text content from the invisible layer that falls within redaction rectangles
   - Log: "OCR text layer scrubbed in redacted region on page X"

3. **Add structure tree scrub**:
   - Walk `/StructTreeRoot` and all structure elements
   - Clear `/ActualText`, `/Alt`, `/E` (expansion text) that contains redacted content
   - Remove structure elements whose marked content falls within redaction regions

4. **Add redaction audit log**:
   - For each redaction operation, generate a JSON log entry:
     ```json
     {
       "timestamp": "ISO8601",
       "page": 5,
       "region": {"x": 100, "y": 200, "w": 300, "h": 20},
       "textRemoved": true,
       "ocrLayerScrubbed": true,
       "structureTreeScrubbed": true,
       "metadataSanitized": true,
       "hashBefore": "sha256:...",
       "hashAfter": "sha256:..."
     }
     ```
   - Write to sidecar `.redaction-log.json` file

5. **Extend `sanitizeDocument`** to cover all 15+ vectors:
   - Structure tree (`/StructTreeRoot`) — strip `/ActualText`, `/Alt` globally
   - `/PieceInfo` per page — remove private application data
   - Page thumbnails (`/Thumb`) — regenerate or remove
   - Optional Content Groups — flatten (render visible state, remove OCG structure)
   - Bookmarks referencing removed content — strip destinations
   - RichMedia annotations — remove
   - Output intents (can leak printer profiles) — remove
   - `/Collection` (portfolios) — remove
   - Trailer `/ID` — randomize second element
   - Form field values if sanitization mode = "full"
   - Annotations with redacted `/Contents` or `/RC` — clean or remove

6. **Add tests**:
   - Test that glyph advances don't leak character widths (compare advance patterns before/after)
   - Test OCR layer scrub on a document with invisible text
   - Test extended sanitization removes all listed vectors
   - Test redaction audit log is generated correctly

### Verification

```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# Verify extended sanitization:
# 1. Create a test PDF with XMP, thumbnails, structure tree, JavaScript, etc.
# 2. Run sanitize
# 3. Inspect with qpdf --json to verify all vectors removed
```

---

## SESSION 8 of 10 — Forms Completion + Text Editing Controls + Annotation Expansion

**Scope:** Complete AcroForm field types (radio, combo, list, date, numeric, calculated), add text editing font/size/color controls, and implement missing annotation subtypes (strikeout rendering, squiggly, stamps, callouts, comment threads).

**Agents:** `/backend` (forms engine), `/frontend` (UI controls)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`.

**Forms current state:**
- FormManager can extract and fill text fields and checkboxes via PoDoFo AcroForm
- FormBuilderMode UI exists with basic text-field and checkbox creation
- Missing: radio buttons, dropdowns, list boxes, date/numeric fields, calculated fields, validation rules, tab order, auto-detect, form data import/export, flattening

**Text editing current state:**
- `PdfEditorEngine::editTextInline` does basic text replacement via PoDoFo
- No font picker, size control, color control, or alignment control
- EditController routes "edit-text" tool ID but the editing dialog is minimal

**Annotations current state:**
- AnnotationLayer handles: freehand, rectangle, ellipse, line, arrow, text box, sticky note, highlight, underline
- Missing renderings: strikeout, squiggly underline, stamp, callout
- No comment threads (ISO 32000 `/IRT` — In Reply To)
- No comment status (open/resolved/rejected)

### Deliverables

1. **Complete AcroForm field types** in FormManager + FormBuilderMode:
   - **Radio buttons**: PoDoFo `PdfRadioButton`, group via shared parent, `RadiosInUnison` flag
   - **Combo box (dropdown)**: PoDoFo `PdfComboBox`, `/Opt` array, Edit flag
   - **List box**: PoDoFo `PdfListBox`, MultiSelect flag
   - **Date field**: text field + format action `/AA /F` (built-in `AFDate_FormatEx`)
   - **Numeric field**: text field + keystroke validation `/AA /K` (`AFNumber_Keystroke`)
   - **Form validation**: Required flag (Ff bit 2), MaxLen, format/validate actions
   - **Tab order**: `/Tabs /S` (structure order) on page, field `/TU` tooltips
   - **Form flattening**: render appearance streams to page content, remove AcroForm

2. **Add form data import/export**:
   - Export to FDF (PDF Forms Data Format)
   - Export to CSV (one row per form, columns = field names)
   - Import from FDF/CSV to fill fields

3. **Add text editing controls**:
   - Font family picker (list available PDF fonts + system fonts)
   - Font size selector
   - Color picker for text color
   - Bold/italic/alignment controls
   - Wire these into `editTextInline` — when editing, user can change properties
   - Add a floating toolbar that appears during text edit mode

4. **Complete annotation subtypes**:
   - **Strikeout**: render horizontal line through text midpoint (same as underline but offset)
   - **Squiggly underline**: render wavy line below text baseline
   - **Stamp annotations**: predefined stamps (Approved, Rejected, Confidential, Draft, Final) + custom image stamp
   - **Callout**: text box with a leader line pointing to a location
   - Always generate `/AP` (appearance stream) for each annotation

5. **Implement comment threads**:
   - ISO 32000 `/IRT` (In Reply To) linking
   - `AnnotationItem` gains `replies` list and `reviewState` enum
   - `CommentsWidget` (already exists) shows threaded view
   - Status: Open, Accepted, Rejected, Cancelled, Completed
   - Filter by status, author, date

6. **Tests**: form field creation/read-back, form flattening, FDF export/import, annotation appearance generation

### Verification

```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# Manual: open FormBuilderMode, create radio group, dropdown, date field
# Manual: open Edit mode, select text, change font/size/color
# Manual: add a stamp annotation, verify appearance renders
```

---

## SESSION 9 of 10 — OCR Multi-Engine: Tesseract MSYS2 + PP-OCRv5 + ROVER Merge

**Scope:** Get Tesseract 5 building via MSYS2 (not Visual Studio), integrate RapidOCR-Onnx with PP-OCRv5 as a secondary engine, implement word-level confidence-weighted ROVER merging, and add preprocessing pipeline.

**Agents:** `/backend` (OCR engines), `/performance` (pipeline optimization)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. Current OCR state:
- `OcrEngine` wraps Tesseract 5 behind `#ifdef HAS_TESSERACT`
- Interface: `IOcrEngine` with `processImage()` → `QList<OcrResult>` and `getRawText()`
- Tesseract was marked "needs VS toolchain" — **this is wrong**. MSYS2/mingw-w64 packages work.

**Research findings:**
- Tesseract 5.5.x via MSYS2 `mingw-w64-x86_64-tesseract-ocr` — native MinGW build
- RapidOCR-Onnx wrapping PP-OCRv5 (released 2025-05-20, Apache-2.0) via ONNXRuntime C++
- Word-level confidence-weighted ROVER for merging results
- Only invoke secondary engine on low-confidence Tesseract spans (MeanTextConf < 70)
- Preprocessing: DPI normalize, orientation detect, deskew, denoise, binarize via Leptonica

### Deliverables

1. **Get Tesseract 5 working via MSYS2**:
   - Document the MSYS2 install: `pacman -S mingw-w64-x86_64-tesseract-ocr mingw-w64-x86_64-leptonica`
   - Update CMakeLists.txt to find Tesseract from MSYS2 paths
   - Remove all "needs VS toolchain" comments from build docs
   - Verify `OcrEngine` compiles and runs with MSYS2 Tesseract
   - Bundle `tessdata_fast` for English (default) + download script for other languages

2. **Integrate RapidOCR-Onnx** (new files: `src/engines/ocr/RapidOcrEngine.h/.cpp`):
   - Clone/vendor `RapidAI/RapidOcrOnnx` CMake project
   - Implements `IOcrEngine` interface
   - Bundle det/cls/rec ONNX model files (~30 MB)
   - Conditional compilation: `HAS_RAPIDOCR` guard
   - Link ONNXRuntime C++ API

3. **Implement preprocessing pipeline** (new file: `src/engines/ocr/OcrPreprocessor.h/.cpp`):
   - DPI detection: if < 200, upscale to 300 DPI via Leptonica `pixScale`
   - Orientation/OSD: Tesseract `--psm 0`, auto-rotate
   - Deskew: Leptonica `pixDeskew` (±15° range)
   - Denoise: Leptonica median filter or optional OpenCV non-local-means
   - Binarize: Sauvola (`pixSauvolaBinarize`) for variable lighting, Otsu for clean scans
   - Border crop: remove dark scan borders
   - Input: QImage → Output: preprocessed QImage + transformation matrix (for coordinate mapping back)

4. **Implement OcrPipeline with ROVER merge** (new file: `src/engines/ocr/OcrPipeline.h/.cpp`):
   ```cpp
   class OcrPipeline {
   public:
       enum MergeStrategy { PrimaryOnly, ConfidenceWeighted, RoverVote };
       void addEngine(std::shared_ptr<IOcrEngine> engine, int priority);
       OcrResult run(const QImage& page, MergeStrategy strategy);
   };
   ```
   - Default: PrimaryOnly (Tesseract)
   - ConfidenceWeighted: run Tesseract first; for words with `MeanTextConf < 70`, run RapidOCR on the word's bounding box region; accept the higher-confidence result
   - RoverVote: run both engines on full page; align words by IoU > 0.5; pick per-word by confidence; use ROVER alignment for disagreements

5. **Expose confidence in OCR results**:
   - `OcrResult` gains `float confidence` per word (0.0–1.0 normalized)
   - `OcrMode` UI shows confidence coloring (green ≥80%, yellow 50-80%, red <50%)
   - Per-region "re-OCR" button for low-confidence zones

6. **Update OcrMode UI**:
   - Preprocessing options panel (auto/manual deskew, language selection)
   - Engine selection (Tesseract / RapidOCR / Both+ROVER)
   - Confidence overlay on recognized text
   - "Review before save" workflow: user confirms/corrects before text layer is committed

### Verification

```bash
cmake --build build -- -j8

# Test with Tesseract available:
cd build && ctest --output-on-failure -j4

# Manual: open a scanned PDF, run OCR, verify confidence colors
# Manual: enable ROVER mode, compare results quality
```

---

## SESSION 10 of 10 — Conversion + Diff + Batch + CI/CD + Distribution

**Scope:** Complete the conversion pipeline (Word/Excel/HTML/image/PDF-A), implement visual+textual document diff, wire up batch mode, set up GitHub Actions CI/CD, and prepare distribution (crashpad, code signing readiness).

**Agents:** `/backend` (conversion, diff), `/devops` (CI/CD), `/performance` (batch)

---

### Context

You are working on GlyphPDF at `C:\Users\User\Projects\pdf`. This is the final session bringing everything together for v1.0.

**Conversion current state:**
- ConversionManager: PDF→Word via DuckX, PDF→Excel via OpenXLSX
- Missing: PDF→HTML, PDF→image, PDF→CSV, Office→PDF, batch conversion, quality presets

**Comparison current state:**
- CompareMode + CompareWidget exist with basic side-by-side layout
- No diff algorithm (visual or textual)

**Batch current state:**
- BatchMode panel exists with file list UI
- No execution loop

**CI/CD:** None. Build is manual via `build_all.bat`.

### Deliverables

#### Part A: Conversion pipeline
1. **PDF→HTML**: PDFium text extraction + positional CSS layout
2. **PDF→Image**: PDFium render at configurable DPI (PNG, JPEG, TIFF)
3. **PDF→CSV**: table detection heuristic (horizontal/vertical line clustering) + text extraction
4. **Office→PDF**: headless LibreOffice invocation (`soffice --headless --convert-to pdf`) — optional, gated by `HAS_LIBREOFFICE`
5. **PDF/A export**: verify with structure checks, embed fonts, set output intent
6. **Quality presets**: High Fidelity (300 DPI, no downsample), Balanced (150 DPI), Small File (72 DPI, aggressive compress)

#### Part B: Visual + textual diff
1. **DiffEngine** (new: `src/engines/DiffEngine.h/.cpp`):
   - Stage 1: SHA-256 file hash comparison (exit if identical)
   - Stage 2: page count comparison
   - Stage 3: text diff — extract text per page via PDFium, normalize whitespace, Myers diff per word stream
   - Stage 4: pixel diff — render both at 150 DPI via PdfiumBackend, `pixelmatch`-style XOR with antialiasing threshold
   - Output: `DiffResult` with per-page text changes + pixel change regions
2. **Update CompareWidget**:
   - Side-by-side view with synchronized scrolling
   - Text changes highlighted (red = removed, green = added)
   - Pixel diff overlay toggle
   - Summary: "X pages changed, Y text blocks modified, Z visual differences"

#### Part C: Batch mode
1. Wire `BatchMode` execution loop:
   - File list + operation selector (convert, OCR, compress, redact, watermark, merge)
   - Sequential or parallel execution via `QtConcurrent::mappedReduced`
   - Progress bar per file + overall progress
   - Error handling: continue on failure, log errors

#### Part D: CI/CD (GitHub Actions)
1. Create `.github/workflows/build.yml`:
   - Matrix: Debug + Release
   - Install Qt 6.10.2 via `jurplel/install-qt-action` or `aqtinstall`
   - Install MinGW 13.1.0
   - Cache vcpkg binary cache via `actions/cache`
   - Configure + build + ctest
   - `windeployqt` for Release artifacts
   - Upload artifacts

2. Create `.github/workflows/release.yml`:
   - Triggered on tag push (`v*`)
   - Build Release + windeployqt + WiX MSI
   - Upload MSI as release asset

#### Part E: Distribution readiness
1. **Crashpad via sentry-native** (optional, gated):
   - Download sentry-native SDK
   - Initialize in `main.cpp` with minidump path `%APPDATA%\GlyphPDF\crashes\`
   - Opt-in "Send crash report" dialog on next launch after crash
2. **Code signing readiness**:
   - Document SignPath Foundation OSS application process
   - Prepare `signtool` integration point in CI release workflow
   - Add placeholder for cert thumbprint
3. **Accessibility basics**:
   - Ensure every custom widget has `accessibleName()` and `accessibleDescription()`
   - Test keyboard navigation through ribbon tabs and tool panels
   - Add `QAccessible::Role` to custom widgets

### Verification

```bash
# Local build
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# CI: push to GitHub, verify workflow passes
# Manual: convert a PDF to HTML, open in browser
# Manual: compare two PDFs, verify diff overlay
# Manual: batch convert 5 PDFs to Word
```

---

## Execution Order & Dependencies

```
Session 1 (DI modernization)
    ↓
Session 2 (ToolRegistry)
    ↓
Session 3 (IPdfBackend + PoDoFoBackend)
    ↓
    ├── Session 4 (PDFium backend) ──→ Session 10 Part B (diff uses PDFium)
    ├── Session 5 (qpdf backend)
    ├── Session 6 (PAdES B-LT/LTA)
    ├── Session 7 (Redaction hardening)
    ├── Session 8 (Forms + annotations)
    └── Session 9 (OCR multi-engine)
                                    ↓
                         Session 10 (Conversion + CI/CD + distribution)
```

Sessions 1→2→3 are **strictly sequential** (each depends on the previous).
Sessions 4–9 can run in **any order** after Session 3.
Session 10 should run **last** (it integrates everything).

---

## Quick Start

1. Copy the session prompt text above
2. Start a new Claude Code session in `C:\Users\User\Projects\pdf`
3. Paste the prompt
4. Let the agent execute
5. Verify the deliverables
6. Move to the next session

Each session is designed to be completable in a single Claude Code context window (2-3 major tasks). If a session runs long, the agent should write a STATE.md handoff before context exhaustion.
