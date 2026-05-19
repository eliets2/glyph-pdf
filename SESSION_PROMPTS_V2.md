# GlyphPDF v1.0 — Enhanced Execution Session Prompts (v2)

**Generated:** 2026-05-19 | **Methodology:** 7-H Prompt Engineering (Lyra-Enhanced)
**Total sessions:** 20 | **Estimated total:** 28-36 weeks
**Optimization techniques applied:** Role Assignment, Task Clarity, Chain-of-Thought, Self-Verification, Conditional Logic, Delimiter Usage, Negative Instructions, Context Management, Agent Orchestration, Goal-Backward Verification

---

## STANDARD EXECUTION PROTOCOL (applies to ALL sessions)

Every session prompt below inherits this protocol. Do not skip steps.

```
PHASE 1 — ANALYZE (before writing any code)
  Read every file listed in <files_to_read>. Map the current implementation.
  Identify integration points and potential conflicts.

PHASE 2 — PLAN (before implementing)
  For each deliverable, write a brief approach in a comment block or mentally.
  Identify the order that minimizes rework.

PHASE 3 — IMPLEMENT + VERIFY (iterative)
  For each deliverable:
    a. Write the code
    b. Build: cmake --build build -- -j8
    c. If build fails → read error, fix, rebuild. Do NOT proceed with errors.
    d. Test: set QT_QPA_PLATFORM=offscreen && cd build && ctest --output-on-failure -j4
    e. If tests fail → isolate: ./FailingTest, diagnose, fix, re-run full suite.
    f. Git checkpoint: git add [changed files] && git commit -m "Session X.N: [description]"

PHASE 4 — CONTEXT GATE
  If context usage feels past 50% (you've read many files, written many changes):
    STOP. Write C:\Users\User\Projects\pdf\STATE.md with:
      - Session number + deliverable progress (D1: DONE, D2: DONE, D3: IN PROGRESS)
      - Files modified so far
      - Remaining deliverables with enough detail to continue cold
      - Build/test status at time of handoff
    Do NOT rush remaining work. Quality over completion.

PHASE 5 — FINAL VERIFICATION
  Run the session's <success_criteria> checks.
  Every criterion must pass before declaring the session complete.
```

**Environment constants (all sessions):**
- Project: `C:\Users\User\Projects\pdf`
- Qt: `C:\Qt\6.10.2\mingw_64` | MinGW: `C:\Qt\Tools\mingw1310_64`
- vcpkg: `C:\vcpkg` | Triplet: `x64-mingw-dynamic`
- Plugins: `set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins`
- Headless: `set QT_QPA_PLATFORM=offscreen`
- Build: `cmake --build build -- -j8`
- Test: `cd build && ctest --output-on-failure -j4`

---

## SESSION 1 of 20 — DI Modernization: AppContext shared_ptr + Bootstrapper

<session_metadata>
Phase: 1 (Architecture Hardening) | Priority: BLOCKING
Depends on: None (first session)
Agents: /refactor (primary — safe codebase migration), /backend (secondary — engine knowledge)
Estimated context: ~30% | Risk: MEDIUM — touches every file that uses AppContext
</session_metadata>

<role>
You are a senior C++17 engineer specializing in Qt 6 dependency injection patterns and safe ownership semantics. You are executing Session 1 of a 20-session pipeline to bring GlyphPDF (a native C++/Qt 6.10.2 PDF editor) to v1.0.
</role>

<project_context>
GlyphPDF is a native Windows desktop PDF editor at C:\Users\User\Projects\pdf.
Stack: C++17, Qt 6.10.2, PoDoFo 0.10.3, OpenSSL 3.x, MinGW 13.1.0, CMake, vcpkg.
Architecture: 4 static libraries (pdfws_core → pdfws_engines → pdfws_commands → pdfws_ui) + PdfWorkstation executable.
6 pure-virtual interfaces in src/core/interfaces/ decouple UI from engines.
AppContext struct passes engine pointers to all controllers and modes.
10 test targets all currently pass.
</project_context>

<current_state>
AppContext (src/core/AppContext.h) holds 8 RAW POINTERS:
  - IPdfEditorEngine*, IOcrEngine*, IFormManager*, ISignatureManager*
  - IConversionEngine*, ICollaboration*, QUndoStack*, DocumentSession*
These are created in main.cpp as stack/heap objects and passed by raw pointer.
Problem: No lifetime safety, no shared ownership, blocks multi-backend refactor.
</current_state>

<objective>
Replace all raw pointers in AppContext with std::shared_ptr, extract engine creation into a Bootstrapper class, and update every consumer — while keeping all 10 test targets green.
</objective>

<files_to_read>
src/core/AppContext.h
src/app/main.cpp
src/GpMainWindow.h
src/GpMainWindow.cpp
src/shell/controllers/HomeController.h (representative controller — check constructor signature)
src/modes/ModeController.h
tests/mocks/MockSignatureManager.h (representative mock)
CMakeLists.txt (library targets section)
</files_to_read>

<deliverables>

### D1: Refactor AppContext to shared_ptr
**Files:** src/core/AppContext.h
**Acceptance:** All 8 members are std::shared_ptr<T>. No raw pointer members remain.
**Implementation:**
- Replace `IPdfEditorEngine*` with `std::shared_ptr<IPdfEditorEngine>` (repeat for all 6 interfaces)
- Replace `QUndoStack*` with `std::shared_ptr<QUndoStack>`
- Replace `DocumentSession*` with `std::shared_ptr<DocumentSession>`
- Add `#include <memory>` if not present
- Keep struct layout — no builder pattern, no constructor, just public members

### D2: Create Bootstrapper class
**Files:** src/app/Bootstrapper.h (NEW), src/app/Bootstrapper.cpp (NEW)
**Acceptance:** `Bootstrapper::createContext()` returns a fully populated AppContext with all 8 shared_ptrs.
**Implementation:**
- Static method: `static AppContext createContext()`
- Moves all engine construction from main.cpp into this single method
- Uses `std::make_shared<ConcreteEngine>()` for each
- Add Bootstrapper.cpp to pdfws_engines target in CMakeLists.txt (it instantiates concrete engines)

### D3: Update main.cpp
**Files:** src/app/main.cpp
**Acceptance:** main.cpp calls `Bootstrapper::createContext()` and passes `const AppContext*` to MainWindow. No manual `new` for engines.
**Implementation:**
- Replace all manual engine construction with single Bootstrapper call
- `auto ctx = Bootstrapper::createContext();`
- `gp::MainWindow mainWindow(&ctx);`

### D4: Update all consumers to const AppContext*
**Files:** GpMainWindow.h/.cpp, all 7 controllers in src/shell/controllers/, ModeController.h/.cpp, all mode panels
**Acceptance:** Every consumer takes `const AppContext*` (never owning). No consumer stores a raw owning pointer.
**Implementation:**
- GpMainWindow constructor: `explicit MainWindow(const AppContext* ctx, QWidget* parent = nullptr)`
- Controllers: same pattern — `const AppContext* m_ctx`
- Access engines via `m_ctx->pdfEditor` (shared_ptr dereferences transparently with `->`)
- Grep all files for `AppContext*` and update signatures

### D5: Update test mocks
**Files:** All test files in tests/, tests/mocks/
**Acceptance:** All 10 test targets compile and pass. Mock objects created via make_shared.
**Implementation:**
- Test setup: `auto mockSig = std::make_shared<MockSignatureManager>();`
- AppContext in tests: populate with shared_ptr to mocks
- If any test directly constructs AppContext, update it

</deliverables>

<verification>
```bash
# Build
cmake --build build -- -j8 2>&1 | findstr /C:"error" /C:"warning"
# Expected: 0 errors. Warnings only from third-party headers.

# Test
set QT_QPA_PLATFORM=offscreen
set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins
cd build && ctest --output-on-failure -j4
# Expected: 10/10 pass

# Verify no raw interface pointers remain in AppContext
grep -n "IPdfEditorEngine\*\|IOcrEngine\*\|IFormManager\*\|ISignatureManager\*\|IConversionEngine\*\|ICollaboration\*\|QUndoStack\*\|DocumentSession\*" src/core/AppContext.h
# Expected: 0 matches

# Verify Bootstrapper exists and is linked
test -f src/app/Bootstrapper.h && test -f src/app/Bootstrapper.cpp && echo "OK"
grep -n "Bootstrapper" src/app/main.cpp
# Expected: at least 1 match
```
</verification>

<constraints>
- DO NOT use boost::di, kangaru, or any DI framework — manual DI for 8 members is sufficient
- DO NOT change any interface signatures (IPdfEditorEngine, IOcrEngine, etc.)
- DO NOT change engine implementation internals — this session is PURELY about ownership
- DO NOT break the public API of GpMainWindow (keep accepting pointer, not reference)
- DO NOT add unique_ptr — use shared_ptr for all, since multiple controllers share the same engine
- SCOPE: Only AppContext ownership + Bootstrapper. No other architectural changes.
</constraints>

<error_recovery>
- If build fails with "no matching function for call to make_shared": check that concrete class constructor is public
- If test fails with segfault: a consumer is storing raw pointer from shared_ptr.get() and shared_ptr was destroyed — ensure AppContext lifetime outlives all consumers
- If circular include: Bootstrapper.h includes concrete headers; AppContext.h includes only interface forward declarations
- If CMake error "Bootstrapper.cpp not found": verify it's added to the correct library target's SOURCES list
</error_recovery>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: 10/10 passing
- [ ] AppContext.h: 0 raw pointer members (grep confirms)
- [ ] Bootstrapper.h/.cpp exist and are in CMake build
- [ ] main.cpp uses Bootstrapper::createContext()
</success_criteria>

---

## SESSION 2 of 20 — ToolRegistry: ToolId Enum + QAction Dispatch

<session_metadata>
Phase: 1 (Architecture Hardening) | Priority: BLOCKING
Depends on: Session 1 completed (shared_ptr AppContext)
Agents: /refactor (primary — migration), /backend (QAction/Qt patterns)
Estimated context: ~35% | Risk: HIGH — touches all 7 controllers + ribbon + tests
</session_metadata>

<role>
You are a senior Qt 6 architect specializing in action-based dispatch patterns (QAction, KActionCollection-style registries) and strongly-typed enumerations. You are executing Session 2 of a 20-session pipeline for GlyphPDF.
</role>

<project_context>
GlyphPDF at C:\Users\User\Projects\pdf — C++17, Qt 6.10.2, 4 static libraries.
Session 1 completed: AppContext now uses shared_ptr. Bootstrapper creates engines.
</project_context>

<current_state>
Tool dispatch is string-based:
- Ribbon emits toolActivated(QString) with IDs like "zoom-in", "encrypt", "edit-text"
- GpMainWindow::onToolActivated loops 7 controllers calling handles(QString)/activate(QString)
- Each controller has if/else-if chains matching string literals
- Many tools have aliases: "zoomIn" AND "zoom-in", "saveAs" AND "save-as"
- ~84 unique tool IDs across 7 controllers
- String typos are silent failures
- TestControllers verifies no overlapping tool IDs (string-based assertions)
</current_state>

<objective>
Replace string-based tool routing with a ToolId enum class, ToolRegistry backed by QAction, and IToolController interface — eliminating string fragility while preserving all current functionality.
</objective>

<files_to_read>
src/shell/controllers/HomeController.h + .cpp
src/shell/controllers/ViewController.h + .cpp
src/shell/controllers/EditController.h + .cpp (largest — ~30 tool IDs)
src/shell/controllers/PagesController.h + .cpp
src/shell/controllers/ConvertController.h + .cpp
src/shell/controllers/FormsController.h + .cpp
src/shell/controllers/SecurityController.h + .cpp
src/GpMainWindow.cpp (onToolActivated dispatch loop)
src/shell/Ribbon.h + .cpp (signal emission)
src/shell/RibbonModel.h + .cpp (tool ID storage)
tests/TestControllers.cpp
</files_to_read>

<deliverables>

### D1: Create ToolId enum class
**Files:** src/core/ToolId.h (NEW)
**Acceptance:** One enum value per unique tool (~70 values). toString/fromString helpers handle aliases. Added to pdfws_core.
**Implementation:**
```cpp
// src/core/ToolId.h
#pragma once
#include <QString>

enum class ToolId {
    // Home
    Open, Save, SaveAs, Print, Share, Properties,
    // View
    ZoomIn, ZoomOut, ActualSize, FitWidth, FitPage,
    SinglePage, Continuous, TwoPage, Presentation, Fullscreen,
    DarkMode, EyeCare,
    // Edit
    Hand, Select, SelectObject, EditText, EditObject, EditImage,
    Search, Find, Ocr,
    Highlight, Underline, Strikeout, Squiggly,
    Note, Comment, Pencil, Freehand, TextBox, AddText,
    Line, Arrow, Rectangle, Oval,
    Signature, Image, MarkRedact,
    // Pages
    RotateCW, RotateCCW, DeletePage, InsertPage, Extract, Reorder,
    // Convert
    ToWord, ToExcel, Combine, Compress,
    // Forms
    TextField, Checkbox, Radio, Dropdown,
    // Security
    Encrypt, Password, Sign, ValidateSig, Validate,
    Sanitize, ApplyRedact, ExportAnno, ImportAnno,
    Cloud, Permissions, RemoveSecurity, Certify, Timestamp,
    PatternRedact, RegexRedact,

    COUNT // sentinel for array sizing
};

QString toolIdToString(ToolId id);
ToolId toolIdFromString(const QString& str); // handles aliases
bool isValidToolIdString(const QString& str);
```
- fromString maps BOTH "zoomIn" AND "zoom-in" to ToolId::ZoomIn (case-insensitive)
- Returns a sentinel or std::optional<ToolId> for unknown strings

### D2: Create IToolController interface
**Files:** src/core/interfaces/IToolController.h (NEW)
**Acceptance:** Pure virtual interface with handledTools(), activate(ToolId), isEnabled(ToolId).
**Implementation:**
```cpp
class IToolController {
public:
    virtual ~IToolController() = default;
    virtual QList<ToolId> handledTools() const = 0;
    virtual void activate(ToolId id) = 0;
    virtual bool isEnabled(ToolId id) const { return true; }
    virtual QString displayName(ToolId id) const { return toolIdToString(id); }
};
```

### D3: Create ToolRegistry
**Files:** src/shell/ToolRegistry.h (NEW), src/shell/ToolRegistry.cpp (NEW)
**Acceptance:** Registers IToolController instances per ToolId. Creates QAction per tool. Dispatches via activate(ToolId). Added to pdfws_ui.
**Implementation:**
```cpp
class ToolRegistry : public QObject {
    Q_OBJECT
public:
    explicit ToolRegistry(QObject* parent = nullptr);
    void registerController(IToolController* ctrl);  // registers all its handledTools()
    void activate(ToolId id);
    void activateFromString(const QString& str);     // alias-aware adapter for Ribbon
    QAction* actionFor(ToolId id) const;
    IToolController* controllerFor(ToolId id) const;
signals:
    void toolActivated(ToolId id);
private:
    QHash<ToolId, IToolController*> m_controllers;
    QHash<ToolId, QAction*> m_actions;
};
```

### D4: Migrate all 7 controllers to IToolController
**Files:** All 7 controller .h/.cpp pairs in src/shell/controllers/
**Acceptance:** Each implements IToolController. No string-based handles()/activate() remain. Switch/case on ToolId.
**Implementation:**
- HomeController: `QList<ToolId> handledTools() const override { return {ToolId::Open, ToolId::Save, ...}; }`
- activate(): `switch(id) { case ToolId::Open: ... case ToolId::Save: ... }`
- Remove old `bool handles(const QString&)` and `void activate(const QString&)`
- Repeat for all 7 controllers

### D5: Update GpMainWindow to use ToolRegistry
**Files:** src/GpMainWindow.h, src/GpMainWindow.cpp
**Acceptance:** Owns ToolRegistry. Registers all 7 controllers. Ribbon connects through ToolRegistry::activateFromString. Old dispatch loop removed.
**Implementation:**
- Add `ToolRegistry* m_toolRegistry` member
- In constructor: create registry, call registerController for each of 7 controllers
- Connect Ribbon::toolActivated(QString) → m_toolRegistry->activateFromString(QString)
- DELETE the old onToolActivated manual loop

### D6: Update Ribbon/RibbonModel (optional but recommended)
**Files:** src/shell/RibbonModel.h/.cpp, src/shell/Ribbon.h/.cpp
**Acceptance:** RibbonModel stores ToolId per button. Ribbon can emit toolActivated(ToolId) in addition to string signal.
**Implementation:**
- Add ToolId field to button model
- Keep string signal for backward compatibility during transition
- ToolRegistry::activateFromString serves as the adapter

### D7: Update TestControllers
**Files:** tests/TestControllers.cpp
**Acceptance:** Tests verify handledTools() returns correct ToolId lists. No-overlap test uses ToolId enum. All 11 test cases pass.
**Implementation:**
- Replace `ctrl.handles("open")` with `ctrl.handledTools().contains(ToolId::Open)`
- Overlap test iterates ToolId::Open through ToolId::COUNT, verifies exactly 1 controller per ID
- Unknown ID test verifies no controller claims invalid IDs

</deliverables>

<verification>
```bash
# Build
cmake --build build -- -j8

# Test
set QT_QPA_PLATFORM=offscreen
set QT_PLUGIN_PATH=C:\Qt\6.10.2\mingw_64\plugins
cd build && ctest --output-on-failure -j4
# Expected: 10/10 pass (or 11/11 if TestControllers counts differently)

# Verify no string-based handles remain in controllers
grep -rn "handles(\"" src/shell/controllers/
# Expected: 0 matches

# Verify ToolRegistry exists
test -f src/shell/ToolRegistry.h && test -f src/shell/ToolRegistry.cpp && echo "OK"

# Verify ToolId enum exists
grep -c "enum class ToolId" src/core/ToolId.h
# Expected: 1
```
</verification>

<constraints>
- DO NOT change Ribbon visual appearance — this is a wiring refactor only
- DO NOT remove alias support — fromString("zoomIn") and fromString("zoom-in") must both work
- DO NOT change how controllers interact with engines (activate() logic stays the same)
- DO NOT use QVariant or string-based property dispatch
- DO NOT use boost::any or std::variant for tool IDs
- SCOPE: Tool routing only. No engine or UI changes.
</constraints>

<error_recovery>
- If "ToolId undeclared" in controllers: ensure ToolId.h is included and pdfws_core links properly
- If Ribbon still emits strings: that's fine — ToolRegistry::activateFromString bridges the gap
- If TestControllers fails: the test likely still uses old handles(QString) — update test assertions
- If tool stops working after migration: check that the ToolId fromString mapping covers the alias
</error_recovery>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing (same count as before)
- [ ] ToolId.h has ~70 enum values
- [ ] ToolRegistry.h/.cpp exist
- [ ] IToolController.h exists
- [ ] All 7 controllers implement IToolController
- [ ] No string-based handles() in src/shell/controllers/ (grep: 0 matches)
- [ ] GpMainWindow no longer has manual dispatch loop
</success_criteria>

---

## SESSION 3 of 20 — IPdfBackend Abstraction + PoDoFoBackend + Licensing

<session_metadata>
Phase: 1 (Architecture Hardening) | Priority: BLOCKING
Depends on: Session 2 completed
Agents: /refactor (primary — extraction), /security (licensing review)
Estimated context: ~30% | Risk: MEDIUM — internal reorganization, no API change
</session_metadata>

<role>
You are a senior C++ architect specializing in PDF library abstraction layers and open-source license compliance. You are executing Session 3 of the GlyphPDF v1.0 pipeline.
</role>

<objective>
Introduce IPdfBackend abstraction interfaces, extract all PoDoFo usage into a PoDoFoBackend namespace, add MuPDF/AGPL license guard in CMake, and create LICENSE-3RD-PARTY.md.
</objective>

<deliverables>

### D1: Create IPdfBackend interface hierarchy
**Files:** src/core/interfaces/IPdfDocument.h, IPdfPage.h, IPdfRenderer.h, IPdfWriter.h, IPdfSearcher.h (all NEW)
**Acceptance:** 5 minimal pure-virtual interfaces abstracting document load/save, page access, rendering, writing, and search. Added to pdfws_core.
**Key interfaces:**
- `IPdfDocument`: loadDocument(path), saveDocument(path), pageCount(), metadata()
- `IPdfRenderer`: renderPage(pageIndex, dpi) -> QImage, renderTile(pageIndex, subRect, dpi) -> QImage
- `IPdfWriter`: writeDocument(path), writeUpdate(path) for incremental
- `IPdfSearcher`: searchText(pageIndex, query) -> QList<QRectF>

### D2: Create PoDoFoBackend
**Files:** src/engines/podofo/PoDoFoBackend.h (NEW), src/engines/podofo/PoDoFoBackend.cpp (NEW)
**Acceptance:** Implements IPdfDocument, IPdfWriter. Wraps existing PdfEditorEngine::Private PoDoFo logic. No PoDoFo headers leak into public API.
**Implementation:**
- Move PoDoFo-specific code from PdfEditorEngine::Private into PoDoFoBackend
- PoDoFoBackend uses pimpl — PoDoFo #includes only in .cpp file
- Create src/engines/podofo/ subdirectory

### D3: Refactor PdfEditorEngine to delegate to backend
**Files:** src/engines/PdfEditorEngine.h, src/engines/PdfEditorEngine.cpp
**Acceptance:** PdfEditorEngine holds IPdfDocument* backend pointer. Editing operations delegate to it. Still implements IPdfEditorEngine (UI-facing interface unchanged).
**Implementation:**
- PdfEditorEngine::Private holds `std::unique_ptr<PoDoFoBackend> backend`
- loadDocumentForEditing → backend->loadDocument
- saveDocument → backend->saveDocument (or writeUpdate for incremental)
- All editing ops (editTextInline, applyRedactions, encrypt, etc.) still call PoDoFo through the backend

### D4: Create BackendRouter stub
**Files:** src/engines/BackendRouter.h (NEW), src/engines/BackendRouter.cpp (NEW)
**Acceptance:** Declarative router mapping operations to backends. Currently all routes to PoDoFoBackend. Ready for PDFium (Session 4) and qpdf (Session 5).
**Implementation:**
- `IPdfRenderer* rendererFor(...)` → currently nullptr (no renderer backend yet; Qt's QPdfView still handles rendering)
- `IPdfDocument* documentBackendFor(...)` → PoDoFoBackend
- `IPdfWriter* writerFor(...)` → PoDoFoBackend

### D5: Add MuPDF license guard in CMake
**Files:** CMakeLists.txt
**Acceptance:** CMake FATAL_ERROR if MuPDF/libmupdf is detected. Comment explains AGPL incompatibility.
**Implementation:**
```cmake
# FORBIDDEN: MuPDF is AGPL-3.0 — linking it forces GlyphPDF into AGPL.
# Use PDFium (BSD-3) for rendering instead.
if(TARGET mupdf OR TARGET libmupdf)
    message(FATAL_ERROR "MuPDF/libmupdf detected. AGPL-3.0 license is incompatible with this project. Remove it and use PDFium.")
endif()
```

### D6: Create LICENSE-3RD-PARTY.md
**Files:** LICENSE-3RD-PARTY.md (NEW, project root)
**Acceptance:** Lists every dependency with name, version, license, and compatibility note.
**Content:**
| Library | Version | License | Compatible |
|---------|---------|---------|------------|
| Qt 6 | 6.10.2 | LGPL-3.0 / commercial | Yes |
| PoDoFo | 0.10.3+ | LGPL-2.0+ OR MPL-2.0 | Yes |
| OpenSSL | 3.x | Apache-2.0 | Yes |
| PDFium | (future) | BSD-3-Clause | Yes |
| qpdf | (future) | Apache-2.0 | Yes |
| Tesseract | 5.x | Apache-2.0 | Yes |
| Leptonica | 1.x | BSD-2-Clause | Yes |
| OpenXLSX | 1.x | BSD-3-Clause | Yes |
| DuckX | 1.x | MIT | Yes |
| MuPDF | ANY | **AGPL-3.0** | **FORBIDDEN** |
| Poppler | ANY | **GPL-2.0+** | **FORBIDDEN** |

</deliverables>

<verification>
```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# Verify no PoDoFo includes in public headers
grep -rn "#include.*podofo" src/core/ src/shell/ src/ui/ src/commands/
# Expected: 0 matches (all PoDoFo includes must be in src/engines/)

# Verify LICENSE-3RD-PARTY.md
test -f LICENSE-3RD-PARTY.md && echo "OK"

# Verify backend files exist
test -f src/engines/podofo/PoDoFoBackend.h && echo "OK"
test -f src/engines/BackendRouter.h && echo "OK"
```
</verification>

<constraints>
- DO NOT change PdfEditorEngine's public interface (IPdfEditorEngine)
- DO NOT move SignatureManager or FormManager into PoDoFoBackend yet — only PdfEditorEngine's core ops
- DO NOT actually link PDFium or qpdf — those are Sessions 4 and 5
- DO NOT add MuPDF or Poppler under any circumstances
- SCOPE: Internal engine reorganization + licensing only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] PoDoFoBackend exists in src/engines/podofo/
- [ ] BackendRouter exists with stub routing
- [ ] No PoDoFo #includes outside src/engines/
- [ ] LICENSE-3RD-PARTY.md exists at project root
- [ ] CMake has MuPDF FATAL_ERROR guard
</success_criteria>

---

## SESSION 4 of 20 — PDFium Backend: Rendering + 3-Tier Cache + Tiled Rendering

<session_metadata>
Phase: 2 (Multi-Backend Foundation) | Priority: HIGH
Depends on: Session 3 completed (IPdfBackend exists)
Agents: /backend (primary — PDFium integration), /performance (cache design)
Estimated context: ~35% | Risk: HIGH — new external dependency + rendering pipeline change
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF rendering pipelines, GPU-accelerated tiling, and cache-aware memory management. You understand PDFium's C API (fpdfview.h, fpdf_text.h, fpdf_formfill.h).
</role>

<objective>
Integrate PDFium as a rendering backend via pre-built binaries, implement PdfiumBackend for rendering/text/search, create a 3-tier LRU render cache with viewport prefetch, and add tiled rendering for pages exceeding 64 megapixels.
</objective>

<deliverables>

### D1: Integrate PDFium pre-built binary
**Files:** CMakeLists.txt, cmake/FindPdfium.cmake (NEW)
**Acceptance:** PDFium links to pdfws_engines. find_package(Pdfium) or manual target. Conditional: HAS_PDFIUM guard.
**Source:** Download from github.com/AnnulusGames/pdfium-binaries or bblanchon/pdfium-binaries — the MinGW-compatible build. Place in C:\vcpkg\installed\x64-mingw-dynamic or a local third-party folder.

### D2: Create PdfiumBackend
**Files:** src/engines/pdfium/PdfiumBackend.h (NEW), src/engines/pdfium/PdfiumBackend.cpp (NEW)
**Acceptance:** Implements IPdfRenderer and IPdfSearcher. FPDF_InitLibrary in constructor, FPDF_DestroyLibrary in destructor. Thread-safe via mutex.
**Key methods:**
- `QImage renderPage(int pageIndex, qreal dpi)` — FPDF_LoadPage, FPDF_RenderPageBitmap, FPDF_ClosePage
- `QImage renderTile(int pageIndex, QRectF subRect, qreal dpi)` — clipped render
- `QString extractText(int pageIndex)` — FPDFText_LoadPage, FPDFText_GetText
- `QList<QRectF> searchText(int pageIndex, const QString& query)` — FPDFText_FindStart loop

### D3: Update BackendRouter to route rendering
**Files:** src/engines/BackendRouter.h/.cpp
**Acceptance:** renderPage/renderTile → PdfiumBackend (if available, else fallback to QPdfView). extractText/searchText → PdfiumBackend.

### D4: Implement 3-tier RenderCache
**Files:** src/engines/RenderCache.h (NEW), src/engines/RenderCache.cpp (NEW)
**Acceptance:** Thread-safe LRU cache. Tier 1: metadata (always resident). Tier 2: rendered pages keyed by (page, scale), max 256 MB. Tier 3: text-layer (kept after first parse).
**Implementation:**
- `QReadWriteLock` for concurrent reader access
- LRU eviction by total byte size (not entry count)
- `QImage getOrRender(int page, qreal scale, IPdfRenderer* renderer)`
- Cache hit/miss counters for performance monitoring

### D5: Implement viewport prefetch
**Files:** src/engines/RenderCache.cpp (extension)
**Acceptance:** Background thread pre-renders pages within +-3 of current viewport at current zoom level. Cancels stale prefetch on scroll.
**Implementation:** QtConcurrent::run for prefetch. CancellationToken pattern via QAtomicInt.

### D6: Implement tiled rendering for large pages
**Files:** src/engines/RenderCache.cpp (extension), src/ui/PdfViewerWidget.cpp (compositor)
**Acceptance:** Pages where rendered raster > 64 MP at current zoom are split into 1024x1024 tiles. Tiles cached individually. PdfViewerWidget composites tiles in paintEvent.

### D7: Wire PdfViewerWidget to RenderCache
**Files:** src/ui/PdfViewerWidget.h/.cpp
**Acceptance:** Old m_pageCache (20-page QHash) replaced by RenderCache. All page rendering routes through cache.

</deliverables>

<verification>
```bash
cmake --build build -- -j8
cd build && ctest --output-on-failure -j4

# Verify PDFium is conditionally available
grep -n "HAS_PDFIUM" CMakeLists.txt
# Expected: conditional guard present

# Verify PdfiumBackend exists
test -f src/engines/pdfium/PdfiumBackend.h && echo "OK"

# Verify RenderCache exists
test -f src/engines/RenderCache.h && echo "OK"
```
</verification>

<constraints>
- DO NOT link MuPDF — use PDFium only (BSD-3-Clause)
- DO NOT break the app if PDFium binary is not available — keep QPdfView as fallback
- DO NOT use PDFium for editing — PoDoFo handles all content modification
- DO NOT exceed 256 MB default cache size — make it configurable via QSettings
- SCOPE: Rendering pipeline + cache only. No editing changes.
</constraints>

<success_criteria>
- [ ] Build: 0 errors (with or without PDFium — conditional compilation)
- [ ] Tests: all passing
- [ ] PdfiumBackend implements IPdfRenderer + IPdfSearcher
- [ ] RenderCache with 3 tiers
- [ ] Viewport prefetch working (+-3 pages)
- [ ] Tiled rendering for pages > 64 MP
</success_criteria>

---

## SESSION 5 of 20 — qpdf Backend: Linearize + Repair + Save Pipeline

<session_metadata>
Phase: 2 (Multi-Backend) | Priority: HIGH
Depends on: Session 3 completed
Agents: /backend (primary)
Estimated context: ~25% | Risk: MEDIUM — post-processor pattern, well-scoped
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF structural operations, linearization (ISO 32000 Annex F), and file repair strategies. You understand qpdf's C++ API (QPDF, QPDFWriter classes).
</role>

<objective>
Add qpdf as a post-processing backend for linearization and structural repair. Implement a save pipeline that routes through PoDoFo for editing and qpdf for optimization, with guards preventing qpdf from touching signed documents.
</objective>

<deliverables>

### D1: Integrate qpdf library
**Files:** CMakeLists.txt
**Acceptance:** find_package(qpdf QUIET) with HAS_QPDF conditional. Links to pdfws_engines.

### D2: Create QpdfBackend
**Files:** src/engines/qpdf/QpdfBackend.h (NEW), src/engines/qpdf/QpdfBackend.cpp (NEW)
**Acceptance:** linearize(), repair(), isLinearized(), inspectJson() — all with QPDF C++ API. Conditional compilation with #ifdef HAS_QPDF.

### D3: Implement save pipeline in PdfEditorEngine
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** Save flow: PoDoFo writes to temp → if linearize requested AND no signatures → qpdf linearizes → atomic rename. Signed docs NEVER touch qpdf.
**Critical constraint:** qpdf flattens xref on save, which invalidates signatures. The save pipeline MUST check for existing signatures before routing through qpdf.

### D4: Implement repair-on-open
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** When PoDoFo fails to parse, attempt qpdf repair → retry PoDoFo on repaired copy → show user warning.

### D5: Add tests
**Files:** tests/ (extend SmokeTest or new TestLinearization)
**Acceptance:** Linearization test (if qpdf available), repair test, signed-doc-skip test. Use QSKIP if qpdf not compiled.

</deliverables>

<constraints>
- DO NOT use qpdf for incremental saves — it flattens xref sections
- DO NOT use qpdf in the signing path — signatures become invalid
- DO NOT require qpdf — it must be optional (HAS_QPDF guard)
- SCOPE: Post-processing pipeline only
</constraints>

<success_criteria>
- [ ] Build: 0 errors (with and without qpdf)
- [ ] Tests: all passing
- [ ] QpdfBackend conditionally compiles
- [ ] Save pipeline skips qpdf for signed documents
- [ ] Repair-on-open works when PoDoFo fails
</success_criteria>

---

## SESSION 6 of 20 — PAdES B-LT/B-LTA + Certificate Encryption

<session_metadata>
Phase: 3 (Critical Security) | Priority: BLOCKING
Depends on: Session 3 completed
Agents: /security (primary — crypto correctness), /backend (OpenSSL API)
Estimated context: ~35% | Risk: HIGH — cryptographic correctness is critical
</session_metadata>

<role>
You are a senior cryptographic engineer specializing in PAdES digital signatures (ETSI EN 319 142-1), X.509 PKI, OCSP, and CRL processing via OpenSSL 3.x. You understand the PDF signature model (ISO 32000-2 section 12.8) and incremental update mechanics.
</role>

<objective>
Upgrade SignatureManager from PAdES B-T to B-LT (DSS dictionary with VRI, OCSP, certs) and B-LTA (document timestamp). Implement certificate-based PDF encryption via OpenSSL CMS_encrypt. All signature operations MUST use incremental updates to preserve earlier signatures.
</objective>

<deliverables>

### D1: PAdES B-LT — DSS dictionary with VRI
**Files:** src/engines/SignatureManager.cpp
**Acceptance:** After signing, creates /DSS in catalog with /Certs, /OCSPs, /CRLs arrays + /VRI sub-dict keyed by uppercase-hex SHA-1 of signature /Contents. Written as incremental update.
**Key steps:**
1. Fetch OCSP response for signer cert chain (parse AIA extension)
2. Fetch CRL as fallback
3. Build DSS dictionary with DER-encoded certs + OCSP + CRL
4. VRI key = uppercase hex SHA-1 of the /Contents hex string
5. Write as incremental update (PoDoFo WriteUpdate)

### D2: OCSP client
**Files:** src/engines/SignatureManager.cpp (internal to Private class)
**Acceptance:** HTTP POST to OCSP responder URL (from cert AIA extension). Parse and validate OCSP response. Cache for DSS embedding.
**Implementation:** OpenSSL OCSP_request_new + OCSP_sendreq_bio or Qt QNetworkAccessManager.

### D3: PAdES B-LTA — document timestamp
**Files:** src/engines/SignatureManager.cpp
**Acceptance:** After DSS written, adds /DocTimeStamp signature with /SubFilter /ETSI.RFC3161. ByteRange covers entire DSS-augmented file. Written as another incremental update.

### D4: Signature level selection
**Files:** src/core/interfaces/ISignatureManager.h, src/engines/SignatureManager.h/.cpp
**Acceptance:** New enum PAdESLevel { B_B, B_T, B_LT, B_LTA }. setSignatureLevel() method. Default: B_T (backward compatible).

### D5: Certificate-based encryption
**Files:** src/engines/PdfEditorEngine.cpp (new method)
**Acceptance:** encryptWithCertificate(input, output, recipients) uses /Filter /PubSec, /SubFilter /adbe.pkcs7.s5 (AES-256). Per-recipient RSA-wrapped AES key via CMS_encrypt().

### D6: Update mocks and tests
**Files:** tests/mocks/MockSignatureManager.h, tests/TestSignatureValidation.cpp
**Acceptance:** MockSignatureManager implements new interface methods. Tests verify DSS structure.

</deliverables>

<constraints>
- DO NOT use MD5 or SHA-1 for signature hashing — SHA-256 only
- DO NOT rewrite the full document after signing — incremental updates only
- DO NOT skip OCSP/CRL fetch — B-LT requires validation data embedded at sign time
- DO NOT support ECC encryption yet — RSA-2048/3072 only for v1.0
- SCOPE: Signing upgrades + certificate encryption only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] PAdES B-LT: DSS dictionary created with VRI
- [ ] PAdES B-LTA: document timestamp present
- [ ] Certificate encryption: round-trip test passes
- [ ] Mock updated for new interface
</success_criteria>

---

## SESSION 7 of 20 — Redaction Hardening + Extended Sanitization

<session_metadata>
Phase: 3 (Critical Security) | Priority: BLOCKING
Depends on: Session 3 completed
Agents: /security (primary), /backend (implementation)
Estimated context: ~30% | Risk: HIGH — security-critical, Edact-Ray defense
</session_metadata>

<role>
You are a security engineer specializing in PDF content removal, forensic analysis defense, and metadata sanitization. You are familiar with the Edact-Ray (USENIX Security '22) glyph-advance side-channel attack on PDF redactions.
</role>

<objective>
Fix glyph-advance side channel in redaction, add OCR text-layer scrub, structure-tree sanitization, redaction audit logging, and extend metadata sanitization to cover 15+ additional information leakage vectors.
</objective>

<deliverables>

### D1: Glyph-advance normalization after redaction
**Files:** src/engines/PdfEditorEngine.cpp (applyRedactions)
**Acceptance:** After excising text operators, remaining x-advances in the line are normalized to fixed-width substitutions. An attacker cannot reconstruct removed characters from gap widths.
**Implementation:** Replace the gap left by removed glyphs with a space glyph whose width equals the sum of removed advances (preserving line layout without leaking per-character widths).

### D2: OCR text-layer scrub
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** After visual redaction, invisible text layers (from prior OCR) are scrubbed: text falling within redaction rectangles is removed from the invisible text stream.

### D3: Structure tree scrub
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** /StructTreeRoot walked; /ActualText, /Alt, /E entries containing redacted content are cleared. Structure elements with marked content in redacted regions are removed.

### D4: Redaction audit log
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** JSON sidecar file (.redaction-log.json) generated per redaction operation with timestamp, page, region, operations performed, before/after SHA-256 hashes.

### D5: Extended sanitizeDocument (15+ vectors)
**Files:** src/engines/PdfEditorEngine.cpp (sanitizeDocument)
**Acceptance:** In addition to existing 6 passes, now sanitizes: /StructTreeRoot (/ActualText, /Alt), /PieceInfo, /Thumb (thumbnails), /OCProperties (optional content groups — flatten), /Outlines (bookmarks to removed content), RichMedia annotations, output intents, /Collection, trailer /ID (randomize second element), form field values (in full mode), annotations with redacted /Contents.

### D6: Tests
**Acceptance:** Test glyph-advance normalization, OCR layer scrub, extended sanitization vector removal.

</deliverables>

<constraints>
- DO NOT draw black rectangles as redaction — always excise from content stream
- DO NOT skip OCR layer check — scanned+OCR'd PDFs are a documented redaction failure mode
- DO NOT modify structure tree without checking if document is tagged first
- SCOPE: Redaction + sanitization only. No signing or encryption changes.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Glyph advances normalized post-redaction
- [ ] OCR text layer scrubbed in redacted regions
- [ ] Extended sanitization covers 15+ vectors
- [ ] Redaction audit log generated as JSON sidecar
</success_criteria>

---

## SESSION 8 of 20 — Forms Completion + Text Editing + Annotations

<session_metadata>
Phase: 4 (Content Tools) | Priority: HIGH
Depends on: Session 3 completed
Agents: /backend (forms engine), /frontend (text editing UI, annotation rendering)
Estimated context: ~40% | Risk: MEDIUM — broad scope, may need STATE.md handoff
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF AcroForm implementation (ISO 32000-2 section 12.7), Qt widget-based text editing controls, and PDF annotation rendering with appearance streams.
</role>

<objective>
Complete all AcroForm field types (radio, combo, list, date, numeric, calculated), add text editing font/size/color controls, implement missing annotation subtypes, and add comment threading.
</objective>

<deliverables>

### D1: Complete AcroForm field types in FormManager
**Files:** src/engines/FormManager.h/.cpp, src/modes/FormBuilderMode.cpp
**Acceptance:** Radio buttons (group + RadiosInUnison), ComboBox (dropdown with Edit flag), ListBox (MultiSelect), date/numeric fields (format/validate actions). All via PoDoFo API.

### D2: Form data import/export
**Files:** src/engines/FormManager.h/.cpp
**Acceptance:** Export to FDF and CSV. Import from FDF/CSV to fill fields. Form flattening (render appearances to page content, remove AcroForm).

### D3: Text editing controls
**Files:** src/ui/EditToolBar.h/.cpp (extend), src/shell/controllers/EditController.cpp
**Acceptance:** Floating toolbar with font family picker, size selector, color picker, bold/italic/alignment. Wired to editTextInline with property parameters.

### D4: Missing annotation subtypes
**Files:** src/ui/AnnotationLayer.cpp, src/core/AnnotationTypes.h
**Acceptance:** Strikeout (horizontal line through text midpoint), squiggly underline (wavy line), stamp annotations (predefined: Approved/Rejected/Confidential/Draft + custom image), callout (text box with leader line). All generate /AP appearance streams.

### D5: Comment threading
**Files:** src/core/AnnotationTypes.h, src/ui/CommentsWidget.cpp
**Acceptance:** AnnotationItem gains replies list + ReviewState enum (Open/Accepted/Rejected/Cancelled/Completed). CommentsWidget shows threaded view with filtering by status/author/date. ISO 32000 /IRT linking.

</deliverables>

<constraints>
- DO NOT implement JavaScript-based calculated fields yet — save for post-v1.0
- DO NOT change existing annotation serialization format — extend it
- DO NOT break existing form fill/extract functionality
- CONTEXT GATE: This is a large session. If you reach 50% context, write STATE.md and stop.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Radio, ComboBox, ListBox field creation works
- [ ] FDF export/import round-trips correctly
- [ ] Text editing toolbar appears in edit mode
- [ ] Stamp annotation renders with appearance stream
- [ ] Comment replies show threaded in CommentsWidget
</success_criteria>

---

## SESSION 9 of 20 — OCR Multi-Engine: Tesseract + PP-OCRv5 + ROVER

<session_metadata>
Phase: 5 (OCR) | Priority: HIGH
Depends on: Session 3 completed
Agents: /backend (OCR engines), /performance (pipeline optimization)
Estimated context: ~35% | Risk: HIGH — external library integration, MSYS2 path
</session_metadata>

<role>
You are a senior C++ engineer specializing in OCR pipeline architecture, Tesseract 5 API (TessBaseAPI), ONNXRuntime C++, and confidence-weighted text recognition merging (ROVER algorithm).
</role>

<objective>
Get Tesseract 5 building via MSYS2 MinGW packages, integrate RapidOCR-Onnx with PP-OCRv5 as secondary engine, implement preprocessing (deskew/denoise/binarize via Leptonica), and create OcrPipeline with word-level confidence-weighted ROVER merging.
</objective>

<deliverables>

### D1: Tesseract via MSYS2
**Files:** CMakeLists.txt, build docs
**Acceptance:** HAS_TESSERACT enabled via MSYS2 mingw-w64 packages. OcrEngine compiles and runs. Build docs updated (remove "needs VS" comment).

### D2: RapidOcrEngine (PP-OCRv5 via ONNXRuntime)
**Files:** src/engines/ocr/RapidOcrEngine.h (NEW), src/engines/ocr/RapidOcrEngine.cpp (NEW)
**Acceptance:** Implements IOcrEngine. Uses RapidOcrOnnx with det/cls/rec ONNX models. HAS_RAPIDOCR guard. Bundle ~30 MB model files.

### D3: OcrPreprocessor
**Files:** src/engines/ocr/OcrPreprocessor.h (NEW), src/engines/ocr/OcrPreprocessor.cpp (NEW)
**Acceptance:** DPI normalize, orientation detect, deskew (Leptonica pixDeskew), denoise, Sauvola binarize. Returns preprocessed QImage + coordinate transform matrix.

### D4: OcrPipeline with ROVER merge
**Files:** src/engines/ocr/OcrPipeline.h (NEW), src/engines/ocr/OcrPipeline.cpp (NEW)
**Acceptance:** Strategies: PrimaryOnly, ConfidenceWeighted (secondary only on low-confidence spans where MeanTextConf < 70), RoverVote (full ROVER on both engines). Word-level alignment by IoU > 0.5.

### D5: OcrMode UI updates
**Files:** src/modes/OCRMode.cpp
**Acceptance:** Engine selector, preprocessing options, confidence coloring overlay (green/yellow/red), "review before save" workflow.

</deliverables>

<constraints>
- DO NOT use PaddlePaddle runtime — ONNXRuntime only for PP-OCRv5
- DO NOT require both engines — Tesseract alone must work
- DO NOT use character-level voting — word-level only (char-level decreases accuracy when engines correlate)
- SCOPE: OCR pipeline only. No rendering or editing changes.
</constraints>

<success_criteria>
- [ ] Build: 0 errors (with Tesseract from MSYS2)
- [ ] OcrEngine recognizes text from test image
- [ ] RapidOcrEngine conditionally compiles
- [ ] Preprocessing improves recognition on skewed test image
- [ ] OcrPipeline routes through correct strategy
</success_criteria>

---

## SESSION 10 of 20 — Conversion Pipeline + Visual Diff

<session_metadata>
Phase: 6 (Conversion & Diff) | Priority: HIGH
Depends on: Session 4 completed (PDFium for text extraction + rendering)
Agents: /backend (conversion), /performance (batch execution)
Estimated context: ~35% | Risk: MEDIUM
</session_metadata>

<role>
You are a senior C++ engineer specializing in document format conversion (PDF to Office, HTML, images), visual difference algorithms, and Qt-based batch processing with progress reporting.
</role>

<objective>
Complete the conversion pipeline (PDF to HTML/image/CSV + Office-to-PDF via LibreOffice), implement visual+textual diff for CompareMode, and wire up BatchMode execution loop.
</objective>

<deliverables>

### D1: PDF to HTML conversion
**Files:** src/engines/ConversionManager.cpp
**Acceptance:** PDFium text extraction + positional CSS layout. Output: single HTML file per PDF with positioned text divs.

### D2: PDF to image export
**Files:** src/engines/ConversionManager.cpp
**Acceptance:** PDFium render at configurable DPI. Formats: PNG, JPEG, TIFF. Per-page or all-pages output.

### D3: PDF to CSV (table extraction)
**Files:** src/engines/ConversionManager.cpp
**Acceptance:** Table detection via horizontal/vertical line clustering + text alignment heuristic. Export detected tables as CSV.

### D4: Office to PDF (optional)
**Files:** src/engines/ConversionManager.cpp
**Acceptance:** Invoke headless LibreOffice (soffice --headless --convert-to pdf). Gated by HAS_LIBREOFFICE. Handles docx/xlsx/pptx/odt.

### D5: DiffEngine — visual + textual diff
**Files:** src/engines/DiffEngine.h (NEW), src/engines/DiffEngine.cpp (NEW)
**Acceptance:** Pipeline: SHA-256 compare → page count → text diff (Myers per-word) → pixel diff (XOR with antialiasing threshold at 150 DPI). Output: DiffResult with per-page changes.

### D6: Update CompareWidget
**Files:** src/ui/CompareWidget.cpp, src/modes/CompareMode.cpp
**Acceptance:** Side-by-side synchronized scrolling. Text changes highlighted. Pixel diff overlay toggle. Change summary.

### D7: BatchMode execution loop
**Files:** src/modes/BatchMode.cpp
**Acceptance:** File list + operation selector (convert/OCR/compress/merge). Sequential or parallel execution via QtConcurrent. Per-file + overall progress bars. Continue-on-failure with error log.

</deliverables>

<constraints>
- DO NOT link GPL pdf2htmlEX in-process — invoke as subprocess if needed
- DO NOT require LibreOffice — HAS_LIBREOFFICE conditional
- DO NOT block UI during batch — all operations via QtConcurrent
- SCOPE: Conversion + diff + batch only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] PDF to HTML produces valid output
- [ ] PDF to image at 150/300 DPI works
- [ ] DiffEngine detects text and visual differences
- [ ] CompareWidget shows synchronized diff view
- [ ] BatchMode executes file list with progress
</success_criteria>

---

## SESSION 11 of 20 — Page Operations: Crop, Resize, Headers, Bates, Drag-Drop

<session_metadata>
Phase: 4 (Content Tools) | Priority: HIGH
Depends on: Session 3 completed
Agents: /backend (page manipulation via PoDoFo), /frontend (thumbnail DnD UI)
Estimated context: ~30% | Risk: MEDIUM
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF page geometry (CropBox, MediaBox, TrimBox, BleedBox per ISO 32000-2 section 7.7.3), content stream injection for headers/footers, and Qt drag-and-drop in custom widgets.
</role>

<objective>
Implement page crop/resize via box manipulation, headers/footers/page numbering via content stream injection, Bates numbering for legal workflows, and drag-and-drop page reordering in ThumbnailSidebar.
</objective>

<deliverables>

### D1: Page crop and resize
**Files:** src/engines/PdfEditorEngine.h/.cpp, src/commands/ (NEW CropPageCommand)
**Acceptance:** cropPage(pageIndex, QRectF cropRect) sets /CropBox. resizePage(pageIndex, QSizeF) sets /MediaBox. Both via QUndoCommand for undo/redo.
**Implementation:**
- PoDoFo: page->GetMediaBox(), page->SetMediaBox(). Same for CropBox, TrimBox, BleedBox.
- CropPageCommand stores before/after boxes for undo.
- UI: selection rectangle tool in PdfViewerWidget to define crop region.

### D2: Headers, footers, and page numbering
**Files:** src/engines/PdfEditorEngine.h/.cpp
**Acceptance:** addHeaderFooter(options) injects text into every page's content stream at specified position (top-left/center/right, bottom-left/center/right). Options: text template with {page}, {total}, {date}, {filename} placeholders. Font/size configurable.
**Implementation:**
- For each page: prepend content stream with Tm/Tj operators positioning text
- Use existing document font or embed a standard base-14 font (Helvetica)
- Page number formatting: "Page {page} of {total}", custom prefix/suffix

### D3: Bates numbering
**Files:** src/engines/PdfEditorEngine.h/.cpp
**Acceptance:** applyBatesNumbering(options) stamps sequential numbers on every page. Options: prefix, suffix, start number, digit count (zero-padded), position, font size.
**Implementation:**
- Extends header/footer injection with sequential counter
- Bates number format: PREFIX + zero-padded number + SUFFIX (e.g., "ACME-000001-CONF")
- Position: any of the 6 header/footer positions

### D4: Drag-and-drop page reordering in ThumbnailSidebar
**Files:** src/ui/ThumbnailSidebar.h/.cpp
**Acceptance:** User can drag a thumbnail to reorder pages. Drop inserts the page at the new position. Reorder propagates to PdfEditorEngine and creates a QUndoCommand.
**Implementation:**
- Enable drag: setDragEnabled(true) on thumbnail items
- Custom QMimeData with page index
- dropEvent: calculate target index, call engine reorderPages, push ReorderPageCommand
- Visual feedback: insertion line indicator during drag

### D5: ReorderPageCommand + CropPageCommand
**Files:** src/commands/ReorderPageCommand.h (NEW), src/commands/CropPageCommand.h (NEW)
**Acceptance:** QUndoCommand subclasses for crop and reorder. Undo reverses the operation cleanly.

### D6: PagesController tool wiring
**Files:** src/shell/controllers/PagesController.h/.cpp
**Acceptance:** New tool IDs: Crop, Resize, AddHeader, AddFooter, AddPageNumbers, BatesNumber. All registered in ToolRegistry.

</deliverables>

<constraints>
- DO NOT modify page content for crop — only box dictionaries (crop is non-destructive)
- DO NOT inject headers on pages that already have them (detect and warn)
- DO NOT break existing page rotation/insertion/deletion
- SCOPE: Page geometry + headers + reorder only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Page crop modifies /CropBox (verify with qpdf --json)
- [ ] Header/footer text visible on rendered page
- [ ] Bates numbering sequential across all pages
- [ ] Drag-drop reorder works in thumbnail sidebar
- [ ] Both new commands support undo/redo
</success_criteria>

---

## SESSION 12 of 20 — Search & Navigation: Find-Replace, Regex, Bookmarks

<session_metadata>
Phase: 4 (Content Tools) | Priority: HIGH
Depends on: Session 4 completed (PDFium text extraction)
Agents: /backend (text search), /frontend (UI panels)
Estimated context: ~25% | Risk: LOW — well-defined features
</session_metadata>

<role>
You are a senior C++ engineer specializing in text search algorithms, Qt QRegularExpression integration, and tree-view navigation widgets.
</role>

<objective>
Implement find-and-replace with regex support, bookmark navigation panel, search in comments/bookmarks, and jump-to-page functionality.
</objective>

<deliverables>

### D1: Find-and-replace
**Files:** src/ui/FindBar.h/.cpp, src/engines/PdfEditorEngine.h/.cpp
**Acceptance:** FindBar gains a "Replace" field + "Replace" / "Replace All" buttons. Replace edits the content stream text via PoDoFo. Regex toggle uses QRegularExpression.
**Implementation:**
- Find: PDFium text search (fast) for highlighting
- Replace: PoDoFo content stream text substitution (same as editTextInline but automated)
- Match options: case-sensitive, whole word, regex
- Results counter: "3 of 17 matches"

### D2: Bookmark navigation panel
**Files:** src/ui/BookmarkPanel.h (NEW), src/ui/BookmarkPanel.cpp (NEW), src/shell/Sidebar.h/.cpp
**Acceptance:** Tree view showing PDF bookmark hierarchy (/Outlines). Click navigates to destination page. Shows in left sidebar as a tab alongside thumbnails.
**Implementation:**
- Parse /Outlines from PoDoFo: PdfOutlines, PdfOutlineItem hierarchy
- QTreeWidget with expand/collapse
- Each item: title + page destination
- Double-click scrolls PdfViewerWidget to target page

### D3: Search in comments and bookmarks
**Files:** src/ui/FindBar.cpp
**Acceptance:** "Search scope" dropdown: Document Text / Comments / Bookmarks / All. When "Comments", searches annotation text content. When "Bookmarks", searches outline titles.

### D4: Jump-to-page
**Files:** src/shell/StatusBar.cpp (or dedicated widget)
**Acceptance:** Page number input field in status bar. Type a number + Enter → scrolls to that page. Shows "Page X of Y" label.

### D5: Recent pages history
**Files:** src/ui/PdfViewerWidget.h/.cpp
**Acceptance:** Back/forward navigation for visited pages (like browser history). Keyboard shortcuts: Alt+Left, Alt+Right.

</deliverables>

<constraints>
- DO NOT modify find-bar visual design significantly — extend it
- DO NOT implement bookmark editing (add/delete/rename) — read-only for now
- SCOPE: Search + navigation only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Find-and-replace with regex works
- [ ] Bookmark panel shows outline hierarchy
- [ ] Jump-to-page navigates correctly
- [ ] Back/forward page history works
</success_criteria>

---

## SESSION 13 of 20 — Watermark & PDF Optimization

<session_metadata>
Phase: 4 (Content Tools) | Priority: MEDIUM
Depends on: Session 3 completed
Agents: /backend (PDF manipulation), /performance (size optimization)
Estimated context: ~25% | Risk: LOW
</session_metadata>

<role>
You are a senior C++ engineer specializing in PDF content stream manipulation for watermarking and document size optimization (image compression, font subsetting, object deduplication).
</role>

<objective>
Implement watermark rendering to PDF pages (text and image watermarks) and PDF size optimization (image downsample, font subset, remove unused objects, with before/after size estimates).
</objective>

<deliverables>

### D1: Text watermark rendering
**Files:** src/engines/PdfEditorEngine.h/.cpp, src/modes/WatermarkDialog.cpp
**Acceptance:** addWatermark(options) injects semi-transparent text diagonal across each page. Options: text, font size, color, opacity, rotation angle, position (center/diagonal), page range.
**Implementation:**
- Inject content stream operators with ExtGState for opacity (ca/CA keys)
- Text positioned with Tm matrix at center, rotated 45 degrees
- Font: embedded base-14 or user-selected
- Applied to each page in range

### D2: Image watermark rendering
**Files:** src/engines/PdfEditorEngine.h/.cpp
**Acceptance:** addImageWatermark(imagePath, options) embeds an image XObject and draws it on each page with opacity. Options: position (center/corner/tile), scale, opacity, page range.

### D3: PDF size optimization / compression
**Files:** src/engines/PdfEditorEngine.h/.cpp, src/modes/CompressDialog.cpp
**Acceptance:** optimizeDocument(options) reduces file size. Options with toggles:
- Downsample images above threshold DPI (e.g., reduce 600 DPI images to 150 DPI)
- Remove duplicate image XObjects (hash-based dedup)
- Subset embedded fonts (remove unused glyphs)
- Remove unused named destinations, old linearization data
- Strip metadata (calls sanitizeDocument in "size" mode)
- Before/after size estimate shown in CompressDialog before user confirms

### D4: Size estimation
**Files:** src/modes/CompressDialog.cpp
**Acceptance:** Before applying optimization, show: "Current: 45.2 MB → Estimated: 12.8 MB (72% reduction)". Estimate based on image count/size analysis + font analysis.

</deliverables>

<constraints>
- DO NOT apply watermark to signed pages — warn user
- DO NOT lossy-compress images below user-selected quality threshold
- DO NOT remove fonts that are still referenced — only subset unused glyphs
- SCOPE: Watermark + compression only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Text watermark visible on rendered page at correct angle/opacity
- [ ] Image watermark renders with transparency
- [ ] CompressDialog shows before/after size estimate
- [ ] Optimization reduces file size measurably
</success_criteria>

---

## SESSION 14 of 20 — Accessibility: Screen Reader, Keyboard Nav, Focus Order

<session_metadata>
Phase: 7 (Quality) | Priority: HIGH
Depends on: Sessions 1-3 completed (stable architecture)
Agents: /frontend (accessibility UI), /testing (NVDA verification)
Estimated context: ~25% | Risk: MEDIUM — requires testing with screen reader
</session_metadata>

<role>
You are a senior Qt accessibility engineer specializing in QAccessibleInterface, MSAA/UIA bridge, keyboard navigation patterns, and WCAG 2.1 AA compliance for desktop applications.
</role>

<objective>
Make GlyphPDF accessible: every custom widget exposes correct QAccessible::Role and accessible name/description, keyboard navigation works through all panels, focus order is logical, and high-contrast mode works.
</objective>

<deliverables>

### D1: Accessible names and roles for all custom widgets
**Files:** Every custom widget in src/ui/, src/shell/, src/modes/
**Acceptance:** Every QWidget subclass has accessibleName() and accessibleDescription() returning meaningful strings. Custom widgets override QAccessibleInterface if they have non-standard roles.
**Widgets to audit:**
- Ribbon (tabbed toolbar) → QAccessible::ToolBar with tab names
- ModeStrip → QAccessible::MenuBar
- ThumbnailSidebar → QAccessible::List with page number items
- AnnotationLayer → QAccessible::Canvas
- FindBar → QAccessible::SearchBox
- StatusBar → QAccessible::StatusBar with page/zoom info
- InspectorWidget → QAccessible::PropertyPage
- All dialogs → standard roles, labeled buttons

### D2: Keyboard navigation
**Files:** All widgets, src/GpMainWindow.cpp
**Acceptance:** Tab cycles through: Ribbon tabs → active panel → sidebar → central view → status bar. Escape closes dialogs/panels. F6 cycles major regions. Arrow keys navigate within ribbon/lists.
**Implementation:**
- Set tab order explicitly with QWidget::setTabOrder
- Add keyboard shortcuts for all major actions (document in help)
- Ribbon tabs: Left/Right arrows cycle tabs
- Thumbnail list: Up/Down arrows + Enter to navigate

### D3: Focus indicators
**Files:** QSS theme files (style.qss, light.qss)
**Acceptance:** Every focusable widget has a visible focus indicator (2px accent-color outline). Works in both dark and light themes.

### D4: High-contrast mode
**Files:** src/util/GpTheme.h, QSS files
**Acceptance:** System high-contrast detection (Windows High Contrast API). When active, switch to a high-contrast QSS with minimum 7:1 contrast ratio on all text.

### D5: Keyboard shortcut documentation
**Files:** src/ui/ (new ShortcutHelpDialog or within existing help)
**Acceptance:** Dialog listing all keyboard shortcuts, accessible via F1 or Help menu. Grouped by category (File, View, Edit, Navigate, etc.).

</deliverables>

<constraints>
- DO NOT break existing visual design — accessibility additions must be additive
- DO NOT rely on color alone for any information — use icons/text as well
- DO NOT change keyboard shortcuts that match industry standard (Ctrl+S, Ctrl+Z, etc.)
- SCOPE: Application UI accessibility only. Tagged PDF export (PDF/UA) is a separate task.
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Every custom widget has accessibleName (grep audit)
- [ ] Tab cycles through all major regions
- [ ] Focus indicators visible in both themes
- [ ] Shortcut help dialog accessible via F1
</success_criteria>

---

## SESSION 15 of 20 — Localization: Qt Linguist + RTL + Locale Formats

<session_metadata>
Phase: 7 (Quality) | Priority: MEDIUM
Depends on: Sessions 1-3 completed
Agents: /frontend (UI localization), /backend (locale-aware formatting)
Estimated context: ~25% | Risk: LOW — Qt has excellent i18n support
</session_metadata>

<role>
You are a senior Qt internationalization engineer specializing in Qt Linguist workflows, .ts/.qm translation files, RTL layout adaptation (Arabic/Hebrew), and locale-aware number/date/currency formatting.
</role>

<objective>
Set up the complete localization infrastructure: wrap all user-facing strings in tr(), configure Qt Linguist workflow, add RTL layout support, and implement locale-aware formatting in forms.
</objective>

<deliverables>

### D1: Wrap all user-facing strings in tr()
**Files:** All .cpp files with user-visible strings
**Acceptance:** Every hardcoded user-visible string wrapped in tr() or QObject::tr(). Context set via Q_DECLARE_TR_FUNCTIONS or class name.
**Implementation:**
- Audit all .cpp files for bare string literals in UI calls (setText, setToolTip, setTitle, addTab, etc.)
- Replace: `label->setText("Page")` → `label->setText(tr("Page"))`
- Do NOT translate: internal identifiers, debug messages, file paths

### D2: Qt Linguist workflow setup
**Files:** CMakeLists.txt, translations/ directory (NEW)
**Acceptance:** `lupdate` extracts strings to .ts files. `lrelease` compiles to .qm. CMake target for translation workflow.
**Implementation:**
- Create translations/ directory
- Add `qt_add_translations(PdfWorkstation TS_FILES translations/glyphpdf_ar.ts translations/glyphpdf_fr.ts translations/glyphpdf_de.ts)` to CMake
- Generate initial .ts files with lupdate
- Load translations in main.cpp via QTranslator

### D3: RTL layout support
**Files:** src/app/main.cpp, QSS files, layout-critical widgets
**Acceptance:** When Arabic/Hebrew locale is active, entire UI mirrors: ribbon, sidebar swap sides, text alignment flips. Qt handles most of this automatically with layoutDirection.
**Implementation:**
- `QApplication::setLayoutDirection(Qt::RightToLeft)` when locale is RTL
- Audit hardcoded margins/padding in QSS — replace with `margin-inline-start` patterns
- Test: set LANG=ar and verify layout mirrors

### D4: Locale-aware formatting in forms and status bar
**Files:** src/modes/FormBuilderMode.cpp, src/shell/StatusBar.cpp
**Acceptance:** Date fields use locale format (QLocale::dateFormat). Number fields use locale decimal separator. File sizes use locale number format.
**Implementation:** Replace manual formatting with QLocale::system() calls.

### D5: Language switcher in Settings
**Files:** src/ui/ (Settings or Preferences dialog — create if none exists)
**Acceptance:** User can select language from dropdown. Change takes effect on restart (or immediately for most strings). Stored in QSettings.

</deliverables>

<constraints>
- DO NOT translate in this session — only set up infrastructure and wrap strings
- DO NOT break layouts by making strings too long — use elide where needed
- DO NOT hardcode English strings in logic — only in tr() calls
- SCOPE: i18n infrastructure + RTL + formatting only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] lupdate extracts strings to .ts files successfully
- [ ] RTL layout mirrors correctly (manual visual check)
- [ ] Status bar uses locale number formatting
- [ ] Language switcher saves selection to QSettings
</success_criteria>

---

## SESSION 16 of 20 — Error Handling & Resilience

<session_metadata>
Phase: 7 (Quality) | Priority: HIGH
Depends on: Sessions 1-3 completed
Agents: /backend (error handling), /frontend (error UX)
Estimated context: ~25% | Risk: MEDIUM
</session_metadata>

<role>
You are a senior C++ engineer specializing in defensive programming, error recovery patterns, and user-friendly error presentation in Qt desktop applications.
</role>

<objective>
Implement comprehensive error handling: plain-language error messages, retry/skip/export-logs UX for batch operations, graceful PDF corruption handling, and memory safety for very large files.
</objective>

<deliverables>

### D1: Error message framework
**Files:** src/core/ErrorInfo.h (NEW), src/ui/ErrorDialog.h/.cpp (NEW)
**Acceptance:** ErrorInfo struct with: user message (plain language), technical details (expandable), suggested action, severity (Info/Warning/Error/Critical). ErrorDialog shows these with "Retry" / "Skip" / "Export Log" / "OK" buttons as appropriate.

### D2: Engine error wrapping
**Files:** All engine .cpp files
**Acceptance:** Every engine method that can fail returns or signals ErrorInfo instead of silently failing. PoDoFo exceptions caught and wrapped. OpenSSL errors translated to human messages.
**Examples:**
- "Could not open this PDF. The file may be corrupted or password-protected." (instead of "PdfError: unexpected EOF")
- "Digital signature could not be verified. The signing certificate has expired." (instead of "X509_V_ERR_CERT_HAS_EXPIRED")

### D3: Batch error handling
**Files:** src/modes/BatchMode.cpp
**Acceptance:** When a file in batch fails: show error inline, mark as failed, continue with next. At end: summary with "X succeeded, Y failed". Export log button writes detailed CSV/JSON.

### D4: Graceful PDF corruption handling
**Files:** src/engines/PdfEditorEngine.cpp
**Acceptance:** When loading a corrupted PDF: attempt qpdf repair (if available) → retry load → if still fails, offer partial load (load pages that parse, skip corrupted ones) → show warning with details.

### D5: Memory guards for large files
**Files:** src/engines/PdfEditorEngine.cpp, src/engines/RenderCache.cpp
**Acceptance:** Before loading a PDF > 500 MB, warn user about memory usage. RenderCache monitors total allocation and evicts aggressively when system memory < 500 MB free. Render operations for pages > 50 MP use tiled rendering automatically.

### D6: Temp file cleanup
**Files:** src/app/main.cpp (shutdown), src/engines/PdfEditorEngine.cpp
**Acceptance:** All temp files are cleaned on normal exit and crash recovery. Use QTemporaryFile/QTemporaryDir consistently. Register cleanup in atexit() handler.

</deliverables>

<constraints>
- DO NOT suppress errors — every error must be visible to the user
- DO NOT show stack traces to users — keep technical details in expandable section
- DO NOT block UI for error dialogs during batch — queue them
- SCOPE: Error handling + resilience only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] ErrorInfo struct and ErrorDialog exist
- [ ] Engine errors produce human-readable messages
- [ ] Batch failure doesn't stop remaining files
- [ ] Corrupted PDF shows repair dialog instead of crash
</success_criteria>

---

## SESSION 17 of 20 — Installer & Packaging: WiX MSI

<session_metadata>
Phase: 7 (Distribution) | Priority: HIGH
Depends on: All feature sessions completed
Agents: /devops (packaging), /backend (deployment config)
Estimated context: ~20% | Risk: MEDIUM — WiX tooling
</session_metadata>

<role>
You are a senior Windows deployment engineer specializing in WiX v4 MSI authoring, windeployqt bundling, file association registration, and enterprise-grade Windows installers.
</role>

<objective>
Create a production WiX MSI installer: bundle all Qt/MinGW runtime DLLs via windeployqt, register .pdf file association, add Start Menu entries, and ensure clean upgrade/uninstall behavior.
</objective>

<deliverables>

### D1: windeployqt bundling script
**Files:** scripts/deploy.bat (NEW or update)
**Acceptance:** Runs windeployqt --release on PdfWorkstation.exe. Copies all required Qt plugins, platforms, imageformats, iconengines. Output: deploy/ folder with complete standalone app.

### D2: WiX MSI project
**Files:** installer/GlyphPDF.wxs (NEW or update enterprise_installer.wxs)
**Acceptance:** WiX v4 MSI that:
- Installs to Program Files\GlyphPDF\
- Includes all files from deploy/ folder
- Registers .pdf file association (optional, user choice during install)
- Creates Start Menu shortcut
- Creates Desktop shortcut (optional)
- Sets Add/Remove Programs entry with icon and version
- Supports per-machine or per-user install
- Upgrade: handles in-place upgrade (same UpgradeCode)
- Uninstall: clean removal of all files and registry entries

### D3: Runtime dependencies check
**Files:** installer/GlyphPDF.wxs
**Acceptance:** MSI checks for VC++ runtime (if needed), and required .NET (if WiX needs it). MinGW runtime DLLs (libgcc, libstdc++, libwinpthread) bundled in deploy/.

### D4: File associations
**Files:** installer/GlyphPDF.wxs
**Acceptance:** .pdf files can be opened with GlyphPDF. Registry: HKCR\.pdf\OpenWithProgids\GlyphPDF.Document. Shell verb "Open with GlyphPDF" in context menu. Does NOT hijack default PDF viewer unless user chooses.

### D5: Build automation
**Files:** scripts/build-installer.bat (NEW)
**Acceptance:** Single script: cmake build → windeployqt → wix build → output GlyphPDF-Setup-v0.2.0.msi.

</deliverables>

<constraints>
- DO NOT hijack the system default PDF viewer — offer as optional during install
- DO NOT require admin for per-user install
- DO NOT bundle debug symbols in release MSI — ship separate PDB package
- SCOPE: Installer only. No code changes to the application.
</constraints>

<success_criteria>
- [ ] deploy/ folder runs standalone (no Qt install required)
- [ ] MSI installs and uninstalls cleanly
- [ ] Start Menu shortcut launches app
- [ ] .pdf file association works (right-click → Open with GlyphPDF)
- [ ] In-place upgrade preserves user settings
</success_criteria>

---

## SESSION 18 of 20 — Auto-Update Mechanism

<session_metadata>
Phase: 7 (Distribution) | Priority: MEDIUM
Depends on: Session 17 completed (installer exists)
Agents: /backend (update logic), /security (signature verification)
Estimated context: ~20% | Risk: MEDIUM — security-sensitive update path
</session_metadata>

<role>
You are a senior Windows desktop engineer specializing in software auto-update mechanisms, code-signed update packages, and safe atomic update application.
</role>

<objective>
Implement a lightweight auto-update system: check for updates on startup (with user consent), download update package, verify code signature, and apply via MSI upgrade.
</objective>

<deliverables>

### D1: Update manifest system
**Files:** src/core/UpdateChecker.h (NEW), src/core/UpdateChecker.cpp (NEW)
**Acceptance:** On startup (if user opted in), fetch update manifest JSON from a configurable URL. Compare version numbers. If newer version available, show non-blocking notification.
**Manifest format:**
```json
{
  "version": "1.0.0",
  "releaseDate": "2026-07-01",
  "downloadUrl": "https://..../GlyphPDF-Setup-1.0.0.msi",
  "sha256": "abc123...",
  "releaseNotes": "Bug fixes and performance improvements.",
  "minVersion": "0.2.0"
}
```

### D2: Download with progress
**Files:** src/core/UpdateChecker.cpp
**Acceptance:** QNetworkAccessManager downloads the MSI to temp directory. Progress bar in status bar or notification area. SHA-256 verification after download.

### D3: Update application
**Files:** src/core/UpdateChecker.cpp
**Acceptance:** Launch MSI installer in silent upgrade mode (msiexec /i ... /qb). Application closes gracefully before launching installer. On next launch, verify version updated.

### D4: Settings integration
**Files:** Settings/Preferences dialog
**Acceptance:** Checkbox: "Check for updates on startup" (default: on). "Check Now" button. Update channel selector: Stable / Beta (if applicable).

### D5: Rollback safety
**Acceptance:** If update fails (download corrupted, MSI fails), app continues running current version. Error shown with "Try again later" option. No partial state.

</deliverables>

<constraints>
- DO NOT auto-download without user consent — notification only, user clicks "Download"
- DO NOT auto-install without user confirmation
- DO NOT trust unsigned update packages — SHA-256 at minimum, code signature when available
- DO NOT block app startup for update check — async, non-blocking
- SCOPE: Update mechanism only
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Update check fetches manifest and compares versions
- [ ] Download shows progress + verifies SHA-256
- [ ] Settings has update preference checkbox
- [ ] Failed update leaves current version intact
</success_criteria>

---

## SESSION 19 of 20 — Print & Export Polish + Final UI Refinements

<session_metadata>
Phase: 7 (Quality) | Priority: MEDIUM
Depends on: Sessions 1-10 completed
Agents: /frontend (UI polish), /backend (print pipeline)
Estimated context: ~25% | Risk: LOW — polish pass
</session_metadata>

<role>
You are a senior Qt UI engineer specializing in print pipeline (QPrinter/QPrintDialog), export presets, and UI polish for desktop applications approaching v1.0 release.
</role>

<objective>
Polish the print pipeline (print preview, page setup), add export presets panel, improve recent files, and complete remaining UI refinements for v1.0 quality.
</objective>

<deliverables>

### D1: Print preview
**Files:** src/shell/controllers/HomeController.cpp, src/ui/ (new PrintPreviewWidget or use QPrintPreviewDialog)
**Acceptance:** Print menu shows Print Preview. QPrintPreviewDialog (or custom) renders pages via PDFium, shows page layout, margins, scaling options. User confirms or cancels.

### D2: Page setup dialog
**Files:** src/ui/PageSetupDialog.h (NEW)
**Acceptance:** Paper size, orientation, margins, scaling (fit-to-page, actual size, custom %). Saved per-document.

### D3: Export presets panel
**Files:** src/modes/ or src/ui/ (new ExportPresetsPanel)
**Acceptance:** Panel showing saved export configurations: "High Quality PDF/A" (300 DPI, PDF/A-2b), "Web Optimized" (150 DPI, linearized, compressed), "Legal Archive" (PDF/A-3b, Bates numbered). User can create/edit/delete presets. Stored in QSettings.

### D4: Recent files enhancement
**Files:** src/shell/MenuBar.cpp, src/ui/WelcomeWidget.cpp
**Acceptance:** Recent files list (max 20). Shows file name, path, last opened date, thumbnail preview on hover. Pin files to keep them in the list. Clear history option. WelcomeWidget shows recent files grid with thumbnails.

### D5: Drag-and-drop improvements
**Files:** src/GpMainWindow.cpp, src/ui/PdfViewerWidget.cpp
**Acceptance:** Drag PDF file onto app → opens it. Drag PDF onto open document → merge dialog (append/insert at page). Drag image onto page → insert as annotation. Visual drop target indicators.

### D6: Status bar information
**Files:** src/shell/StatusBar.cpp
**Acceptance:** Shows: current page / total pages, zoom percentage, file size, document dirty indicator, selection info (when text selected: word count).

</deliverables>

<constraints>
- DO NOT redesign the Ribbon or overall layout — polish within existing structure
- DO NOT add features not in the PRD — this is refinement only
- SCOPE: Print + export + UI polish
</constraints>

<success_criteria>
- [ ] Build: 0 errors
- [ ] Tests: all passing
- [ ] Print preview shows pages correctly
- [ ] Export presets save and load from QSettings
- [ ] Recent files show thumbnails on WelcomeWidget
- [ ] Drag-drop file opening works
</success_criteria>

---

## SESSION 20 of 20 — Integration Testing + Performance Benchmarks + v1.0 Freeze

<session_metadata>
Phase: 8 (Release) | Priority: BLOCKING
Depends on: ALL previous sessions completed
Agents: /testing (primary — test suite), /performance (benchmarks), /devops (release prep)
Estimated context: ~35% | Risk: LOW — testing and verification, no new features
</session_metadata>

<role>
You are a senior QA engineer and release manager. You specialize in end-to-end test suites, performance benchmarking for desktop applications, and release readiness checklists.
</role>

<objective>
Create an integration test suite testing real-world PDF workflows end-to-end, benchmark performance against targets (1000-page PDF), execute release checklist, bump version to 1.0.0, and generate CHANGELOG.md.
</objective>

<deliverables>

### D1: End-to-end integration test suite
**Files:** tests/TestIntegration.cpp (NEW)
**Acceptance:** Tests that exercise full workflows through the engine layer:
- Open → edit text → save → reopen → verify edit persisted
- Open → encrypt → save → reopen with password → verify
- Open → add annotation → save → reopen → verify annotation
- Open → redact region → save → verify text removed from content stream
- Open → sign → add DSS → save → validate signature
- Open → add form field → fill → flatten → verify flat output
- Open → OCR (if available) → verify text layer added
- Open → batch convert 3 PDFs → verify outputs exist

### D2: Performance benchmarks
**Files:** tests/TestPerformance.cpp (NEW)
**Acceptance:** Timed tests against target metrics:
- Open 100-page PDF: < 2 seconds
- Navigate to page 50: < 500 ms
- Full text search (100 pages): < 3 seconds
- Render one page at 150 DPI: < 200 ms
- RAM working set with 100-page PDF: < 200 MB
- Use QBENCHMARK macro for timing

### D3: Real-world PDF corpus test
**Files:** tests/corpus/ (test PDFs — create programmatically or document how to obtain)
**Acceptance:** Test against diverse PDFs: native text, scanned image, mixed, encrypted, signed, form-heavy, 1000+ page, non-standard page sizes. Document results and any failures.

### D4: Release checklist execution
**Acceptance:** Verify each item:
- [ ] All 10+ test targets pass (ctest)
- [ ] Integration tests pass
- [ ] Performance meets targets
- [ ] Build with -Wall -Wextra produces no new warnings
- [ ] windeployqt creates standalone package
- [ ] MSI installs and uninstalls cleanly
- [ ] File association works
- [ ] All conditional features gracefully degrade when optional deps missing
- [ ] LICENSE-3RD-PARTY.md is accurate
- [ ] No TODO/FIXME/HACK comments in critical paths

### D5: Version bump and CHANGELOG
**Files:** CMakeLists.txt (version), CHANGELOG.md (NEW)
**Acceptance:** Version set to 1.0.0 in CMakeLists.txt project() directive. CHANGELOG.md lists all features, fixes, and known issues organized by category.

### D6: README update
**Files:** README.md
**Acceptance:** Updated with: feature list, screenshots (describe where to add), build instructions, installation instructions, keyboard shortcuts summary, license information.

</deliverables>

<constraints>
- DO NOT add new features — this is testing and release prep only
- DO NOT ignore test failures — every failure must be investigated and fixed or documented as known issue
- DO NOT ship with any CRITICAL or HIGH severity known bugs
- SCOPE: Testing + benchmarks + release freeze only
</constraints>

<success_criteria>
- [ ] All existing tests: passing
- [ ] Integration tests: 8/8 workflows pass
- [ ] Performance: meets targets for 100-page PDF
- [ ] Release checklist: all items checked
- [ ] Version: 1.0.0 in CMakeLists.txt
- [ ] CHANGELOG.md exists with complete feature list
- [ ] README.md updated
</success_criteria>

---

## Execution Order & Dependencies (Full 20-Session Map)

```
SESSION 1 (DI modernization) ─── BLOCKING
    │
SESSION 2 (ToolRegistry) ─── BLOCKING
    │
SESSION 3 (IPdfBackend + licensing) ─── BLOCKING
    │
    ├── SESSION 4 (PDFium backend + cache) ──┐
    ├── SESSION 5 (qpdf backend + save)      │
    ├── SESSION 6 (PAdES B-LT/LTA)          │
    ├── SESSION 7 (Redaction hardening)      │
    ├── SESSION 8 (Forms + annotations)      ├── Can run in ANY order
    ├── SESSION 9 (OCR multi-engine)         │   after Session 3
    ├── SESSION 11 (Page ops: crop/Bates)    │
    ├── SESSION 13 (Watermark + optimize)    │
    ├── SESSION 14 (Accessibility)           │
    ├── SESSION 15 (Localization)            │
    └── SESSION 16 (Error handling)          │
                                             │
    SESSION 10 (Conversion + diff) ──────────┘ (needs Session 4 for PDFium)
    SESSION 12 (Search + bookmarks) ─────────── (needs Session 4 for PDFium text)
    SESSION 17 (Installer) ──────────────────── (needs all features complete)
    SESSION 18 (Auto-update) ────────────────── (needs Session 17)
    SESSION 19 (Print + polish) ─────────────── (needs Sessions 1-10)
    SESSION 20 (Integration test + release) ─── (needs ALL sessions)
```

**Parallel execution windows:**
- After Session 3: up to 11 sessions can run independently (4-9, 11, 13-16)
- After Session 4: Sessions 10 and 12 unblock
- After all features: Sessions 17 → 18 → 19 → 20 (sequential)

---

## Quick Reference: Session-to-Feature Matrix

| Session | Phase | Features Covered |
|---------|-------|------------------|
| 1 | Arch | shared_ptr DI, Bootstrapper |
| 2 | Arch | ToolId enum, ToolRegistry, QAction dispatch |
| 3 | Arch | IPdfBackend, PoDoFoBackend, MuPDF guard, licensing |
| 4 | Backend | PDFium rendering, 3-tier cache, tiled rendering, prefetch |
| 5 | Backend | qpdf linearize/repair, save pipeline, repair-on-open |
| 6 | Security | PAdES B-LT/B-LTA, OCSP client, certificate encryption |
| 7 | Security | Redaction glyph-advance defense, OCR scrub, 15+ sanitization vectors |
| 8 | Content | Forms (radio/combo/calc), text editing controls, annotation subtypes, comment threads |
| 9 | OCR | Tesseract MSYS2, RapidOCR PP-OCRv5, preprocessing, ROVER merge |
| 10 | Convert | PDF→HTML/image/CSV, Office→PDF, visual diff, batch execution |
| 11 | Content | Page crop/resize, headers/footers, Bates numbering, drag-drop reorder |
| 12 | Content | Find-replace with regex, bookmark panel, search scope, page history |
| 13 | Content | Watermark (text + image), PDF compression/optimization, size estimates |
| 14 | Quality | Accessibility: screen reader, keyboard nav, focus order, high contrast |
| 15 | Quality | Localization: tr(), Qt Linguist, RTL, locale formats, language switcher |
| 16 | Quality | Error handling: plain messages, retry/skip, corruption recovery, memory guards |
| 17 | Distro | WiX MSI installer, windeployqt, file association, Start Menu |
| 18 | Distro | Auto-update: manifest check, download, SHA-256 verify, MSI upgrade |
| 19 | Polish | Print preview, page setup, export presets, recent files, drag-drop |
| 20 | Release | Integration tests, performance benchmarks, release checklist, v1.0 freeze |
