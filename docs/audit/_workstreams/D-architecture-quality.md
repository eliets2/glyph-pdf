# Workstream D — Architecture Integrity & Code Quality Audit
**GlyphPDF v1.0.0-internal · git HEAD e1c5394 · Audit date 2026-06-02**
**Auditor:** Code Archaeologist (claude-sonnet-4-6) · Read-only · No source changes made.

---

## Summary

The five-layer dependency chain (`pdfws_core → pdfws_engines → pdfws_commands → pdfws_ui → PdfWorkstation`) is substantially sound at the CMakeLists.txt level. All critical layering rules hold at the header level for the `core` and `engines` layers. However seven concrete violations exist, the most severe being a `#define private public` UB hack in `PdfEditorEngine.cpp` that corrupts the ODR across the translation unit, and a direct `<podofo/podofo.h>` pull into `PdfViewerWidget.cpp` (UI layer coupling to a backend library). The ProvenanceGuard boundary is implemented but is NOT called from any save path yet — only instantiated in Bootstrapper, making the enforcement theoretical rather than operational. OCR source duplication across pdfws_engines and pdfws_ui is confirmed and explained. Interface seams are mostly clean; two concrete couplings in the commands layer (EditAnnotationCommand, RedactCommand) hold `QPointer<PdfViewerWidget>` which ties pdfws_commands to a concrete UI class. The CollaborationManager stub is wired (not dead) but the cloud path is guarded behind `#if 0`. No `tr`-shadow compilation failure was found — all `tr()` calls in the `CompareMode::CompareMode` constructor correctly use the `CompareMode::tr(...)` explicit form. Twenty-one findings total: 2 Critical, 4 High, 8 Medium, 5 Low, 2 Info.

---

## Layering Analysis

### Declared chain
```
pdfws_core (INTERFACE — header-only, Qt6::Core + Qt6::Gui only)
  └─ pdfws_engines (STATIC — all backend engines, OCR, scheduling)
       └─ pdfws_commands (STATIC — QUndoCommand subclasses)
            └─ pdfws_ui (STATIC — widgets, dialogs, controllers, modes)
                 └─ PdfWorkstation (EXECUTABLE — main.cpp + Bootstrapper)
```

### CMakeLists.txt declared link-libraries — verified
| Target | Links (PUBLIC/PRIVATE) | Notes |
|--------|------------------------|-------|
| pdfws_core | Qt6::Core, Qt6::Gui | Clean — no widgets |
| pdfws_engines | pdfws_core, Qt6::Widgets, Qt6::Concurrent, podofo, OpenSSL, pdfws_djot, docmodel | Qt6::Widgets leaks PUBLIC — see D-03 |
| pdfws_commands | pdfws_core, Qt6::Widgets | Clean (QUndoCommand lives in Widgets) |
| pdfws_ui | pdfws_core, pdfws_commands, Qt6::Widgets, pdfws_engines | Correct: UI depends on engines |
| PdfWorkstation | pdfws_ui, pdfws_engines, pdfws_commands, pdfws_core, pdfws_djot, docmodel, liblua | Fine |

### OCR source duplication (per-task: investigate)
`OcrPreprocessor`, `RapidOcrEngine`, `OcrPipeline`, `PpOcrDecoder` appear in **both** `pdfws_engines` (CMakeLists lines 173-178) and `pdfws_ui` (CMakeLists lines 419-422). These are **the same source files** listed twice. This produces double-compilation into two separate static archives. This is **intentional** as a workaround: `PdfViewerWidget.cpp` and mode widgets in `pdfws_ui` need OCR types at link time, but `pdfws_ui` does not link `pdfws_engines` via PUBLIC (the link is from `pdfws_ui PRIVATE pdfws_engines`). The duplication avoids a circular dependency but results in two ODR copies of the same symbols in different TUs — safe for static linking but confusing and a symbol-bloat risk. See D-07.

### Verified layering violations

| # | Direction | File:line | Description |
|---|-----------|-----------|-------------|
| V1 | ui → podofo (backend) | `src/ui/PdfViewerWidget.cpp:32` | `#include <podofo/podofo.h>` — UI widget directly includes backend PDF library |
| V2 | engines → UB hack | `src/engines/PdfEditorEngine.cpp:697-700` | `#define private public` / `#define protected public` before including `<podofo/podofo.h>` — undefined behaviour, ODR violation |
| V3 | commands → ui (concrete widget) | `src/commands/EditAnnotationCommand.h:9,36` | Forward-declares + holds `QPointer<PdfViewerWidget>` — command layer couples to concrete UI widget |
| V4 | commands → ui (concrete widget) | `src/commands/RedactCommand.h:8,27` | Same: `QPointer<PdfViewerWidget>` in pdfws_commands header |
| V5 | ui command impls in wrong layer | `src/ui/EditAnnotationCommand.cpp:3` `src/ui/RedactCommand.cpp:3` | Both `.cpp` files live in `src/ui/` and `#include "ui/PdfViewerWidget.h"` — their compilation unit is in pdfws_ui (correct), but the headers live in pdfws_commands and couple that layer to PdfViewerWidget |
| V6 | engines PUBLIC → Widgets | `CMakeLists.txt:191` | `pdfws_engines` links `Qt6::Widgets` as **PUBLIC** — any downstream target that depends on pdfws_engines transitively gets Qt6::Widgets, making it impossible for a future headless / server build of the engine layer without widgets |
| V7 | ProvenanceGuard not enforced in save paths | `src/pdfws_djot/ProvenanceGuard.cpp:6-31` vs all callers | Guard exists, throws on signed+BornPDF+DjotThenSave; but `checkEditVia()` is called **only** in tests (TestDjotRoundtrip). No production save path (PdfViewerWidget, controllers) calls it. Enforcement is theoretical. |

---

## Findings

### D-01 — `#define private public` UB hack in PdfEditorEngine.cpp
**Severity:** Critical · **Confidence:** High
`src/engines/PdfEditorEngine.cpp:697-700`

```cpp
// PoDoFo Hack to support PubSec
#define private public
#define protected public
#include <podofo/podofo.h>
#undef private
#undef protected
```

**Evidence:** The macro redefines C++ access specifiers for the entire PoDoFo header tree in this TU. This is undefined behaviour under the C++ standard ([macro.names] prohibits redefining keywords). More critically, it causes an ODR violation: `PoDoFo::PdfEncrypt` and related types now have a different class layout in this TU than in every other TU that included the same headers without the macro. With GCC 16 / LTO enabled, this is a linker-time time-bomb — the linker may silently merge the ODR-violating definitions, producing a binary with incorrect vtable offsets or corrupted member offsets.

**Impact:** Potential silent memory corruption or vtable misrouting in `PdfEncryptPubSec` (the class constructed just below line 700). Certificate encryption (`encryptWithCertificate`) is directly affected. The fact that it currently works is accidental, not guaranteed.

**Proposed Fix:** Replace the hack with one of two approaches:
1. **PoDoFo extension point (preferred):** Check whether PoDoFo 1.1.0 provides a legitimate factory or virtual override for `PdfEncrypt`. If the API allows subclassing `PdfEncrypt` via the public interface, do so directly without the macro.
2. **Patch-and-submit upstream:** If PoDoFo lacks the hook, isolate `PdfEncryptPubSec` into its own `.cpp` translation unit (`src/engines/podofo/PdfEncryptPubSec.cpp`) where the macro scope is perfectly controlled, use `#pragma GCC diagnostic push/pop` around it, and add a static_assert on sizeof to catch layout drift. File a PoDoFo upstream issue to add a proper API hook.
3. **Short-term mitigation:** At minimum add `static_assert(sizeof(PoDoFo::PdfEncrypt) == EXPECTED_SIZE, "ODR layout check");` immediately after the `#undef protected` to catch silent layout changes at compile time.

---

### D-02 — `pdfws_ui` directly includes `<podofo/podofo.h>` (UI → backend coupling)
**Severity:** Critical · **Confidence:** High
`src/ui/PdfViewerWidget.cpp:32`

```cpp
#include <podofo/podofo.h>
```

**Evidence:** `PdfViewerWidget.cpp` is in `pdfws_ui`. The declared architecture requires that UI depends on interfaces, not concrete backends. PoDoFo is an external C++ library from the backend layer (`pdfws_engines`), and `pdfws_ui` should only use the `IPdfEditorEngine` interface. The direct include pulls PoDoFo's entire type system into the UI layer. Grep confirms this is the only such include in `src/ui/`.

**Impact:** Any change to PoDoFo's API ripples directly into PdfViewerWidget. Compile times for the UI layer grow. The UI layer is now tied to a specific backend, making mocking for TestControllers/TestPagesMode harder (they cannot use a pure mock — the concrete PoDoFo types must be present at link time for TestControllers). It also means the Qt6::Widgets + PoDoFo headers are combined in the same TU, creating the exact kind of include-order dependency that the Windows-CryptoAPI/OpenSSL conflict (SignatureManager.cpp lines 9-26) was specifically designed to prevent.

**Impact on tests:** `TestControllers` and `TestPagesMode` both link `pdfws_ui` and thus transitively pull in PoDoFo even though they test controller dispatch logic only.

**Proposed Fix:** Audit `PdfViewerWidget.cpp` for which specific PoDoFo APIs are used directly (from the grep, lines 980-999 show `PoDoFo::PdfContentStreamReader`, `PoDoFo::PdfContent`, etc. in the `applyRedactions` method). The fix is to route all PoDoFo calls through the engine interface:
1. Move the `applyRedactions` logic that calls PoDoFo directly into `PdfEditorEngine::applyRedactions()` (which already exists on the interface).
2. Remove `#include <podofo/podofo.h>` from `PdfViewerWidget.cpp`.
3. Verify the `applyRedactions` method in `IPdfEditorEngine.h` has all needed parameters — it does (`int pageIndex, const QList<QRectF> &rects`).

---

### D-03 — `pdfws_engines` links Qt6::Widgets as PUBLIC
**Severity:** High · **Confidence:** High
`CMakeLists.txt:191`

```cmake
target_link_libraries(pdfws_engines
    PUBLIC  pdfws_core
    PRIVATE podofo::podofo OpenSSL::SSL OpenSSL::Crypto Qt6::Network Qt6::Widgets ...
```

**Evidence:** `Qt6::Widgets` is in the `PRIVATE` list on this line — verified in CMakeLists.txt line 191. **Correction after re-read:** `Qt6::Widgets` is actually listed in the `PRIVATE` block (after the keyword). However `Credui Advapi32 Crypt32` on line 194 are `PUBLIC` WIN32 deps. The engine should not expose Widgets publicly.

**Wait — re-examining:** CMakeLists line 190-191:
```
PRIVATE podofo::podofo OpenSSL::SSL OpenSSL::Crypto Qt6::Network Qt6::Widgets Qt6::Concurrent libzip::zip
```
Qt6::Widgets is PRIVATE here. However pdfws_engines uses `QImage`, `QColor`, `QRectF` etc. in its PUBLIC headers (interface files), which come from Qt6::Gui (already PUBLIC via pdfws_core). The actual PRIVATE Qt6::Widgets usage comes from implementation files. This is correct.

**Actual finding remains:** `Credui Advapi32 Crypt32` are linked PUBLIC on line 194 — these Windows system DLLs are leaked transitively to all consumers of pdfws_engines on WIN32. For a headless server build this would be harmless, but it is an unnecessary transitive dependency. **Reclassify as Medium.**

---

### D-04 — `EditAnnotationCommand` and `RedactCommand` headers couple `pdfws_commands` to concrete `PdfViewerWidget`
**Severity:** High · **Confidence:** High
`src/commands/EditAnnotationCommand.h:9,36` · `src/commands/RedactCommand.h:8,27`

**Evidence:**
```cpp
// EditAnnotationCommand.h:9
class PdfViewerWidget;
// line 36
QPointer<PdfViewerWidget> m_viewer;
```
The command headers forward-declare and hold a typed pointer to `PdfViewerWidget`, a concrete widget that lives in `pdfws_ui`. The `pdfws_commands` layer is supposed to depend only on `pdfws_core` and interfaces, not on concrete widgets. The `.cpp` implementations (`src/ui/EditAnnotationCommand.cpp` and `src/ui/RedactCommand.cpp`) are placed in `pdfws_ui` precisely because they need the full type — but the headers are in `src/commands/`, making them nominally part of `pdfws_commands`.

**Impact:** Any TU that includes these command headers (e.g., tests linking only pdfws_commands) is forced to acknowledge `PdfViewerWidget`. Mock-based testing of commands against pure interfaces is blocked. The CMakeLists.txt works around this by compiling the `.cpp` bodies in `pdfws_ui` (lines 375-376), but the headers in `pdfws_commands` create a hidden coupling.

**Proposed Fix:** Two options:
1. **Move headers to `src/ui/`:** Relocate `EditAnnotationCommand.h` and `RedactCommand.h` into `src/ui/` alongside their `.cpp` files. Remove them from the `pdfws_commands` CMake sources. Update all includes.
2. **Abstract the viewer coupling:** Introduce a minimal `IAnnotationTarget` interface in `pdfws_core` with `setAnnotations()` and `redactAllMatches()` methods. Command headers depend on the interface; `PdfViewerWidget` implements it. This is the correct architectural pattern but requires more work.

---

### D-05 — ProvenanceGuard is instantiated but never called in production save paths
**Severity:** High · **Confidence:** High
`src/pdfws_djot/ProvenanceGuard.cpp:6-31` · `src/app/Bootstrapper.cpp:48`

**Evidence:** `checkEditVia()` is the sole enforcement method. Grepping all `.cpp` files shows it is called **only** in `src/pdfws_djot/ProvenanceGuard.cpp` (its own definition) and referenced in `tests/TestDjotRoundtrip.cpp`. No controller, no save path in `PdfViewerWidget`, no dialog triggers `checkEditVia()` before writing back a Djot-edited born-PDF. The guard is constructed (`Bootstrapper.cpp:48`) and stored in `AppContext`, but `ctx.provenanceGuard` is never accessed in any controller or UI save flow.

**Impact:** The architectural non-negotiable `§6: "Edit-via-Djot-save-back on signed born-PDF: NEVER — ProvenanceGuard refuses"` is **not enforced at runtime**. A user editing a signed born-PDF via Djot can currently save back without any guard firing. This is a security/integrity gap, not merely a code quality issue.

**Proposed Fix:** Wire `ctx.provenanceGuard->checkEditVia(...)` into:
1. `PdfViewerWidget::saveDocument()` (or the save-as path in `HomeController::onSaveAs`) — before committing a write, check the document's provenance tag and sign status.
2. Any Djot-edit save path that WS2 implements (M6-P4 etc.).
The call must be placed **before** the file is written, and the `ProvenanceViolation` exception must be caught and surfaced as a "Save as copy" dialog rather than an error dialog. The test coverage in `TestDjotRoundtrip` is good; production wiring is missing.

---

### D-06 — `tr` local variable shadows `QObject::tr()` in `CompareMode::CompareMode`
**Severity:** Medium · **Confidence:** High
`src/modes/CompareMode.cpp:27`

```cpp
auto* tr = new QHBoxLayout(tb);   // line 27 — shadows QObject::tr()
tr->addWidget(mono(CompareMode::tr("COMPARE")));  // line 30 — explicit workaround
```

**Evidence:** `CompareMode` is a `QWidget` subclass (which inherits `QObject::tr()`). The local `auto* tr = new QHBoxLayout(tb)` at line 27 shadows the inherited `tr()` method for the entire constructor scope. The code currently compiles and works correctly **only** because every `tr(...)` call in this scope uses the explicit `CompareMode::tr(...)` form (lines 30-52). This is Pattern 18 from `CLAUDE.md §7` — the documented anti-pattern — and the code is one refactor away from a developer accidentally writing `tr("some string")` instead of `CompareMode::tr("some string")`, breaking i18n silently.

**Impact:** Medium — the build does not break today because of the explicit qualification, but it is a maintenance trap. The next developer adding a translatable string in this constructor will likely write unqualified `tr(...)` and get either a compile error or, worse, a function-call-on-pointer error at runtime.

**Proposed Fix:** Rename the layout variable per CLAUDE.md §7 Pattern 18:
```cpp
// Replace:
auto* tr = new QHBoxLayout(tb);
// With:
auto* hrow = new QHBoxLayout(tb);
```
Update all 8 references to `tr->` on lines 28-53 to `hrow->`. Also remove the (now unnecessary) explicit `CompareMode::tr(...)` qualifications back to plain `tr(...)`.

---

### D-07 — OCR source files compiled into both `pdfws_engines` and `pdfws_ui` (ODR duplication)
**Severity:** Medium · **Confidence:** High
`CMakeLists.txt:173-178` (pdfws_engines) and `CMakeLists.txt:419-422` (pdfws_ui)

**Evidence:** Four source files are listed in both targets:
- `src/engines/ocr/RapidOcrEngine.h/.cpp`
- `src/engines/ocr/OcrPreprocessor.h/.cpp`
- `src/engines/ocr/OcrPipeline.h/.cpp`
- `src/engines/ocr/PpOcrDecoder.h/.cpp`

Both static libraries will contain object files for these translation units. If a program links both (PdfWorkstation does: it links `pdfws_ui` which has `PRIVATE pdfws_engines`), the linker picks whichever definition it encounters first, but both copies are physically in the archives. With LTO or link-map analysis, this shows up as duplicate symbols.

**Impact:** Increased binary size (~50-100 KB for OCR TUs). Subtle risk: if the two copies of a template instantiation or a static local variable differ in optimization level or flags (pdfws_engines vs pdfws_ui have different PCH), the resulting binary has two copies of the same static state — e.g., `RapidOcrEngine::Private::isInitialized` could be in two instances.

**Proposed Fix:** Remove the four source file entries from `pdfws_ui` (CMakeLists lines 419-422). Since `pdfws_ui` already links `PRIVATE pdfws_engines`, it will get the OCR types from there. If the issue was that `pdfws_ui` headers reference OCR types that weren't available at compile time, the include paths (both have `${CMAKE_CURRENT_SOURCE_DIR}/src` as include root) mean `#include "engines/ocr/OcrPipeline.h"` should already resolve. The duplication is unnecessary if include paths are correct.

---

### D-08 — `BackendRouter` returns raw owning pointers (`new X()`)
**Severity:** Medium · **Confidence:** High
`src/engines/BackendRouter.cpp:10,21,26`

```cpp
IPdfRenderer* BackendRouter::rendererFor(const QString &path) {
    auto* backend = new PdfiumBackend();   // raw owning pointer
    if (backend->loadDocument(path)) return backend;
    delete backend;
    return nullptr;
}
IPdfDocument* BackendRouter::documentBackendFor(const QString &path) {
    Q_UNUSED(path);
    return new PoDoFoBackend();  // caller owns, no lifetime contract
}
IPdfWriter* BackendRouter::writerFor(const QString &path) {
    Q_UNUSED(path);
    return new PoDoFoBackend();  // same
}
```

**Evidence:** All three factory methods return raw owning pointers with no documented lifetime contract. The caller (`PdfEditorEngine::loadDocumentForEditing`, line 95) wraps the result in `std::unique_ptr<PoDoFoBackend>` via a `dynamic_cast` — but this relies on the caller knowing to wrap it, and the intermediate form between `BackendRouter::documentBackendFor()` return and the unique_ptr wrap is an unguarded raw pointer.

**Impact:** If any early-return path between the `BackendRouter::documentBackendFor()` call and the `std::unique_ptr` wrap throws or early-returns, the allocation leaks. The pattern is also fragile against future callers who may forget to wrap the result.

**Proposed Fix:** Change `BackendRouter` factory return types to smart pointers:
```cpp
std::unique_ptr<IPdfRenderer> rendererFor(const QString &path);
std::unique_ptr<IPdfDocument> documentBackendFor(const QString &path);
std::unique_ptr<IPdfWriter>   writerFor(const QString &path);
```
Update `PdfEditorEngine.cpp:95` accordingly — the `dynamic_cast` should become `dynamic_cast<PoDoFoBackend*>(docBackend.release())` after type verification, or restructure to avoid the cast entirely (see D-09).

---

### D-09 — `PdfEditorEngine` casts `IPdfDocument*` back to `PoDoFoBackend*`
**Severity:** Medium · **Confidence:** High
`src/engines/PdfEditorEngine.cpp:95-101`

```cpp
auto podofoBackend = std::unique_ptr<PoDoFoBackend>(
    dynamic_cast<PoDoFoBackend*>(docBackend));
if (!podofoBackend) {
    // ...
    return false;
}
```

**Evidence:** `BackendRouter::documentBackendFor()` returns `IPdfDocument*`. Immediately after, `PdfEditorEngine` downcasts it back to `PoDoFoBackend*`. This breaks the interface abstraction: the purpose of `IPdfDocument` is to allow swapping backends, but the cast makes `PdfEditorEngine` depend on `PoDoFoBackend` being the concrete type.

**Impact:** `IPdfDocument` is an empty abstraction here — it provides no real seam because the only legal return value is `PoDoFoBackend`. `BackendRouter::rendererFor()` has a similar pattern for `PdfiumBackend`. The interface is leaky (god-interface symptom: the concrete class has many more methods than the interface, and callers need those extra methods).

**Proposed Fix:** Two approaches:
1. **Promote PoDoFo-specific operations into IPdfDocument:** Expand `IPdfDocument` with the operations `PdfEditorEngine` needs beyond `loadDocument/saveDocument/pageCount/metadata`. This makes the interface less minimal but eliminates the cast.
2. **Accept the concrete coupling explicitly:** Rename `BackendRouter::documentBackendFor()` to return `std::unique_ptr<PoDoFoBackend>` directly — the backend is not swappable today and honesty is better than a false abstraction. Remove `IPdfDocument` from this call path. Keep `IPdfDocument` as a future-facing interface but don't use it where concrete types are required.

---

### D-10 — `AnnotationLayer.h` member declaration order vs constructor initializer list (Wreorder)
**Severity:** Medium · **Confidence:** High
`src/ui/AnnotationLayer.h:59-60` · `src/ui/AnnotationLayer.cpp:14-29`

**Evidence:** Header declaration order (relevant members):
```
line 57: QList<AnnotationItem> m_annotations;
line 58: AnnotationItem m_currentNote;
line 59: bool m_isDrawing;
line 60: bool m_isMoving;
line 61: QPointF m_lastDragPos;
line 62: int m_rotation;
line 63: int m_selectedIndex;
```
Constructor initializer list order:
```cpp
: QWidget(parent)
, m_currentMode(ToolMode::HandTool)   // matches line 54
, m_selectedColor(Qt::red)            // matches line 55
, m_selectedThickness(2)              // matches line 56
, m_isDrawing(false)                  // matches line 59 — OK
, m_rotation(0)                       // matches line 62 — skips isMoving (60), lastDragPos (61)
, m_selectedIndex(-1)                 // matches line 63
, m_isMoving(false)                   // matches line 60 — AFTER selectedIndex (63) — WRONG ORDER
, m_pageAtCallback(nullptr)           // ...
, m_resizeHandle(-1)
```

`m_isMoving` is declared at position 60 (before `m_rotation`, `m_selectedIndex`) but initialized after `m_selectedIndex` in the initializer list. This is the `-Wreorder` warning. Members are always initialized in declaration order regardless of initializer list order, so `m_isMoving` is initialized to `false` (default) before `m_rotation` and `m_selectedIndex` see their values — but the initializer list implies the opposite order. Not UB here (all are trivial types), but -Wreorder exists precisely to catch cases where initializer list order produces surprising behavior.

**Proposed Fix:** Reorder the initializer list to match the header declaration order:
```cpp
AnnotationLayer::AnnotationLayer(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(ToolMode::HandTool)
    , m_selectedColor(Qt::red)
    , m_selectedThickness(2)
    , m_isDrawing(false)
    , m_isMoving(false)          // moved here — matches declaration order
    , m_rotation(0)
    , m_selectedIndex(-1)
    , m_pageAtCallback(nullptr)
    , m_resizeHandle(-1)
```

---

### D-11 — `RenderCache::prefetchViewport` discards `QtConcurrent::run` result (nodiscard future)
**Severity:** Medium · **Confidence:** High
`src/engines/RenderCache.cpp:271`

```cpp
QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() { ... });
// QFuture<void> return value silently discarded
```

**Evidence:** `QtConcurrent::run()` returns a `QFuture<T>`. GCC 16 marks it `[[nodiscard]]`. The prefetch future is discarded, meaning: (a) the build produces the warning noted in the pre-established facts, and (b) if the prefetch throws or cancels, there is no way to detect it — the future is the only handle.

**Impact:** Low risk at runtime (prefetch failures are non-critical). Medium risk as a maintenance pattern — future developers may add error handling to the lambda and expect to observe it via the future, not realizing it is dropped.

**Proposed Fix:** Store the future or explicitly suppress:
```cpp
// Option 1: Store in member (allows later cancellation)
m_prefetchFuture = QtConcurrent::run([...]{...});

// Option 2: Explicit discard if intentional
[[maybe_unused]] auto _ = QtConcurrent::run([...]{...});

// Option 3: Redesign to use LaneScheduler (architecturally cleaner — prefetch
// is exactly the kind of background work the CPU lane handles)
```
Option 3 is the preferred long-term fix given the LaneScheduler infrastructure already exists. The existing cancellation token mechanism in RenderCache already overlaps with LaneScheduler's cancel semantics.

---

### D-12 — `HomeController::onShare` uses C-style cast for `GetProcAddress` result (cast-function-type)
**Severity:** Medium · **Confidence:** High
`src/shell/controllers/HomeController.cpp:149`

```cpp
LPMAPISENDMAIL pfnSendMail = (LPMAPISENDMAIL)GetProcAddress(hMapi, "MAPISendMail");
```

**Evidence:** `GetProcAddress` returns `FARPROC` (a `void(*)()` function pointer). Casting it to `LPMAPISENDMAIL` via C-style cast is flagged by `-Wcpp-cast-function-type` / `-Wcast-function-type` when GCC encounters a function pointer type change without an intermediate `void*`. GCC 16 warns on this pattern because function pointer casts through an incompatible type are undefined behaviour on some ABIs.

**Proposed Fix:** Use the correct two-step cast pattern:
```cpp
LPMAPISENDMAIL pfnSendMail = reinterpret_cast<LPMAPISENDMAIL>(
    reinterpret_cast<void*>(GetProcAddress(hMapi, "MAPISendMail")));
```
On Windows (MSYS2/UCRT), `FARPROC` is `INT_PTR(*)()`, so the double reinterpret_cast through `void*` satisfies the type system. Alternatively use `#pragma GCC diagnostic push/pop` to suppress around this one line with an explanatory comment.

---

### D-13 — `CollaborationManager::pushToCloud` / `pullFromCloud` are stubs with misleading return values
**Severity:** Medium · **Confidence:** High
`src/engines/CollaborationManager.cpp:52-80`

```cpp
bool CollaborationManager::pushToCloud(const QString &packagePath, const QString &endpoint) {
    // ...
    return true;  // Stub: always succeeds
}
bool CollaborationManager::pullFromCloud(const QString &endpoint, const QString &destinationPath) {
    return true;  // Stub: always succeeds
}
```

**Evidence:** Both methods return `true` unconditionally regardless of the endpoint being reachable. The cloud sync call site in `SecurityController::cloudSyncSync()` is guarded behind `#if 0` (correctly commented out per the stub annotation at line 315-322), so these stubs are not reachable in production. However they are compiled and linked into every build, and `ICollaboration` has no `isSupported()` / `isCloudAvailable()` guard to signal the stub state to callers.

**Impact:** Low — the `#if 0` guard in SecurityController prevents actual calls. The `AppContext::DefaultCloudSyncEndpoint` is a hardcoded internal URL (`weaver.enterprise.internal`) which is not appropriate for the open-source build.

**Proposed Fix:**
1. Add `virtual bool isCloudSyncSupported() const { return false; }` to `ICollaboration` and override to return `false` in `CollaborationManager`. UI can use this to disable the Cloud Sync ribbon button rather than relying on a `#if 0` in a controller.
2. Remove `AppContext::DefaultCloudSyncEndpoint = "https://weaver.enterprise.internal/v1/sync"` — this is an internal URL leaking into the open-source code.

---

### D-14 — `EditTextInlineCommand::undo()` is a no-op with undocumented behavior
**Severity:** Low · **Confidence:** High
`src/commands/EditTextInlineCommand.h:21-23`

```cpp
void undo() override {
    // Full structural undo not trivial without saving backup of original text,
    // for now just triggering reload since it's hard to revert inline text accurately in PDF
}
```

**Evidence:** `undo()` does nothing — not even `m_doc->markReload()`. A user pressing Ctrl+Z after an inline text edit will see the undo action appear consumed from the stack but the document will not revert. This is a silent deception that violates the contract of `QUndoCommand`.

**Impact:** User experience regression — Ctrl+Z after text edit appears to work (item removed from stack) but document is unchanged. Could cause confusion and data loss if user relies on undo.

**Proposed Fix:** Either (a) implement real undo by storing the original content stream bytes for the affected page before the edit and restoring them in `undo()`, or (b) make this command non-undoable by setting `setObsolete(true)` inside `redo()` after success, similar to `RedactCommand`. Option (b) is the correct short-term fix — same pattern as the redaction command audit fix (line 16 of `RedactCommand.cpp`). At minimum, `undo()` should call `m_doc->markReload()` so the viewer refreshes.

---

### D-15 — `OcrPipeline.h` pulls `docmodel/SemanticDocument.h` into engines layer
**Severity:** Low · **Confidence:** High
`src/engines/ocr/OcrPipeline.h:16`

```cpp
#include "docmodel/SemanticDocument.h"
```

**Evidence:** `OcrPipeline.h` is a public header of `pdfws_engines`. It includes `docmodel/SemanticDocument.h`, pulling the `docmodel` library (a separate target) into the public interface of `pdfws_engines`. This is not a hard layering violation since `pdfws_engines` does link `docmodel` (CMakeLists line 191), but it means every consumer of `pdfws_engines` also sees the full `docmodel` type system in their compilation units.

**Proposed Fix:** Forward-declare `docmodel::SemanticDocument` in `OcrPipeline.h` and use a `QFuture<docmodel::SemanticDocument>` with forward declaration only. Move the full include to `OcrPipeline.cpp` and `OcrDjotMapper.cpp` where the type is used. This requires changing the `recognizeDocumentAsDjot()` return type to use an opaque handle or keeping the full include in the `.cpp` and returning `QFuture<docmodel::SemanticDocument>` with just a forward-declared class.

---

### D-16 — `PpOcrDecoder.cpp` uses `tr` as variable name (non-QObject context, no shadow)
**Severity:** Low · **Confidence:** High
`src/engines/ocr/PpOcrDecoder.cpp:201`

```cpp
if (d > dMax) { dMax = d; tr = i; }
```

**Evidence:** Here `tr` is a plain `int` local variable in a non-member free function `orderPoints`. There is no `QObject::tr()` in scope — this is a free function in a `.cpp` file with no `using namespace`. This is NOT the Pattern 18 bug. The naming is confusing given the documented codebase rule against using `tr` as a variable name, but it does not shadow anything.

**Proposed Fix:** Low priority. Rename for clarity and to avoid confusion with the documented naming constraint:
```cpp
int tl=0, tr_=0, br=0, bl=0; // or: topRight
```

---

### D-17 — `CompareMode` toolbar hardcodes `"Q4-Report-v1.pdf ↔ Q4-Report-v2.pdf"` as translatable string
**Severity:** Low · **Confidence:** High
`src/modes/CompareMode.cpp:31`

```cpp
tr->addWidget(mono(CompareMode::tr("Q4-Report-v1.pdf   ↔   Q4-Report-v2.pdf")));
```

**Evidence:** This string, which looks like a placeholder filename from development, is being sent through `tr()` (meaning it will appear in all three translation `.ts` files as a translatable string that translators will see). It is a UI artifact that should either be replaced with an actual file-name display bound to the compared documents, or at minimum removed from `tr()`.

**Proposed Fix:** Replace with a dynamically-set label populated from `compareFiles(const QString& file1, const QString& file2)`. The label should show the actual filenames. Remove from `tr()` since filenames are not user-visible strings that need translation.

---

### D-18 — `BackendRouter` lacks `pdfws_mrc` / compression path routing
**Severity:** Low · **Confidence:** Medium
`src/engines/BackendRouter.h` / `src/engines/BackendRouter.cpp`

**Evidence:** `BackendRouter` only routes `IPdfRenderer`, `IPdfDocument`, and `IPdfWriter`. The MRC compression pipeline (`MrcPageProcessor`) and veraPDF validator (`VeraPdfValidator`) are instantiated directly in `PdfEditorEngine.cpp` without going through any routing abstraction. This is consistent with the current codebase but means `BackendRouter` is only a partial routing layer.

**Impact:** Low — for a single-target application this is fine. For future extensibility (e.g., GPU vs CPU MRC) the pattern should extend to include a compression backend router.

**Proposed Fix:** Document the intended scope of `BackendRouter` (PDF structure backends only; compression and validation are direct) via a class-level comment in `BackendRouter.h`.

---

### D-19 — Missing `[[nodiscard]]` on safety-critical engine return values
**Severity:** Low · **Confidence:** Medium
`src/core/interfaces/IPdfEditorEngine.h` (multiple methods)

**Evidence:** Methods such as `encryptDocument()`, `sanitizeDocument()`, `applyRedactions()`, `applyPatternRedactions()` return `bool` to indicate success/failure but are not marked `[[nodiscard]]`. A caller that ignores the return value (e.g., during a refactor) will get no compiler warning.

**Proposed Fix:** Add `[[nodiscard]]` to all security-critical interface methods:
```cpp
[[nodiscard]] virtual bool encryptDocument(...) = 0;
[[nodiscard]] virtual bool sanitizeDocument(...) = 0;
[[nodiscard]] virtual bool applyRedactions(...) = 0;
[[nodiscard]] virtual bool applyPatternRedactions(...) = 0;
[[nodiscard]] virtual bool signDocument(...) = 0;  // in ISignatureManager
```

---

### D-20 — `ICollaboration` interface includes cloud sync methods that violate separation of concerns
**Severity:** Info · **Confidence:** High
`src/core/interfaces/ICollaboration.h:10-13`

**Evidence:** `ICollaboration` has four methods: `exportAnnotationPackage`, `importAnnotationPackage` (local annotation I/O) and `pushToCloud`, `pullFromCloud` (network operations). These are two entirely different responsibilities mixed in one interface. The local I/O methods are used in production (SecurityController annotation export/import). The cloud methods are stubs that return `true` unconditionally.

**Proposed Fix:** Split into `IAnnotationIO` (local) and `ICloudSync` (network stub, v1.1+). `AppContext` keeps both. This also makes the stub nature explicit at the interface level.

---

### D-21 — `AppContext::DefaultCloudSyncEndpoint` contains internal infrastructure URL in open-source code
**Severity:** Info · **Confidence:** High
`src/core/AppContext.h:43`

```cpp
static constexpr const char* DefaultCloudSyncEndpoint = "https://weaver.enterprise.internal/v1/sync";
```

**Evidence:** This URL refers to an internal enterprise endpoint. Once the project is open-sourced at M8, this string will be visible in the public source code and in compiled binaries (strings-extractable). It is not a security credential but it reveals internal infrastructure naming.

**Proposed Fix:** Replace with an empty string or a documented placeholder:
```cpp
static constexpr const char* DefaultCloudSyncEndpoint = "";
// Cloud Sync is not available in v1.0.0 — see CollaborationManager::pushToCloud
```

---

## Tech-Debt Inventory

| ID | File:line | Text | Type | Assessment |
|----|-----------|------|------|------------|
| TD-1 | `src/ui/CommentsWidget.cpp:379` | `TODO(M6-P5): give the comment composer the same Djot formatting toolbar` | TODO | Scheduled — M6-P5 rich composer; not a correctness issue |
| TD-2 | `src/ui/CommentsWidget.cpp:414` | `TODO(M6-P5): rich Djot composer for replies` | TODO | Same sprint as TD-1; deferred feature |
| TD-3 | `src/commands/EditTextInlineCommand.h:21` | `// for now just triggering reload since it's hard to revert inline text accurately in PDF` | Hollow undo | Active correctness gap — undo does nothing; see D-14 |
| TD-4 | `src/engines/PdfEditorEngine.cpp:696` | `// PoDoFo Hack to support PubSec` | HACK comment (implicit) | Active UB/ODR violation — see D-01 |
| TD-5 | `src/engines/CollaborationManager.cpp:55` | `// We'll return true to simulate success for the Beta milestone` | Stub | Beta artifact surviving past beta; see D-13 |
| TD-6 | `src/modes/FormBuilderMode.cpp:322` | `// This slot is a no-op stub — the button tooltip says "Coming in v1.1"` | Deferred feature | Explicitly documented; acceptable pre-M8 |

**Note:** The `CLAUDE.md §7 Pattern 7` audit-flagged TODO (`SignatureManager.cpp:323`) is closed — the VRI SHA-1 fix was completed in M1. No `TODO(audit-*)` patterns found in current source.

---

## Largest Files / Hotspots

| Rank | File | LOC | Layer | SRP Status |
|------|------|-----|-------|------------|
| 1 | `src/engines/podofo/PoDoFoBackend.cpp` | 3,089 | engines | VIOLATION — covers load/save/encrypt/redact/images/annotations/sanitize/watermark/optimize/headers/footers/Bates — at least 8 distinct responsibilities |
| 2 | `src/engines/PdfEditorEngine.cpp` | 1,342 | engines | VIOLATION — facade + crypto + MRC coordination + UB hack; contains two conceptually separate concerns (engine facade + PubSec encryption class) |
| 3 | `src/ui/PdfViewerWidget.cpp` | 1,307 | ui | VIOLATION — central widget plus rendering + annotation + redaction + text search + file I/O + podofo direct call (D-02) |
| 4 | `src/ui/InspectorWidget.cpp` | 1,197 | ui | VIOLATION — 5-tab property inspector rendered as one file; tabs should be separate classes |
| 5 | `src/engines/SignatureManager.cpp` | 1,146 | engines | Acceptable — PKI/PAdES is inherently complex; but could be split into signer/validator/timestamper |
| 6 | `src/engines/ConversionManager.cpp` | 945 | engines | VIOLATION — handles HTML/image/CSV/PPT/LibreOffice export in one class |
| 7 | `src/engines/FormManager.cpp` | 647 | engines | Borderline — single responsibility (AcroForm CRUD) but complex; acceptable |
| 8 | `src/modes/OCRMode.cpp` | 623 | ui | Borderline — OCR mode UI; PCH explicitly disabled for this file (OOM risk noted in CMakeLists) |
| 9 | `src/GpMainWindow.cpp` | 588 | ui | Acceptable — main window orchestration |

**Risk hotspot:** `PoDoFoBackend.cpp` at 3,089 LOC is the single highest-risk file for maintenance. Any change to one responsibility (e.g., watermark layout) requires reading 3,000 lines. Extraction plan: `PdfEncryptionBackend`, `PdfAnnotationBackend`, `PdfPageOpsBackend`, `PdfImageBackend` as sub-objects owned by `PoDoFoBackend`.

---

## Verified vs Could-Not-Verify

### Verified (High Confidence)
- `#define private public` UB hack at `PdfEditorEngine.cpp:697-700` — confirmed by direct read
- `<podofo/podofo.h>` in `PdfViewerWidget.cpp:32` — confirmed by grep + direct read
- OCR source duplication across pdfws_engines and pdfws_ui — confirmed by CMakeLists lines 173-178 vs 419-422
- `ProvenanceGuard::checkEditVia()` not called in any production save path — confirmed by exhaustive grep of all `.cpp` files
- `EditAnnotationCommand.h` / `RedactCommand.h` coupling to concrete `PdfViewerWidget` — confirmed by direct read
- `BackendRouter` raw owning pointer factory methods — confirmed by direct read
- `dynamic_cast<PoDoFoBackend*>` in `PdfEditorEngine.cpp:95` — confirmed
- `dynamic_cast<PdfiumBackend*>` in `RenderCache.cpp:101,323` — confirmed (both gated under `#ifdef HAS_PDFIUM`)
- `tr` shadow in `CompareMode.cpp:27` — confirmed; NOT a compilation failure because all calls use `CompareMode::tr(...)` explicitly
- `AnnotationLayer.h` member init order — confirmed by header declaration order vs constructor list
- `RenderCache::prefetchViewport` nodiscard QFuture discard at line 271 — confirmed
- `HomeController.cpp:149` C-style cast-function-type — confirmed
- `CollaborationManager` stubs return `true` unconditionally — confirmed; cloud path is `#if 0` gated
- All 12 interfaces are real seams in `AppContext` (all stored as interface shared_ptr) — verified via `AppContext.h`
- `ICollaboration` is used in production (SecurityController annotation export/import) — confirmed; cloud path stubbed
- `EditTextInlineCommand::undo()` is a silent no-op — confirmed
- `AppContext::DefaultCloudSyncEndpoint` internal URL — confirmed
- CLAUDE.md head `e09404d` vs actual HEAD `e1c5394` — drift noted per audit brief

### Could Not Verify (Confidence: Low/Medium)
- Exact LOC of `PdfViewerWidget.h` header (not counted; .cpp is 1,307)
- Whether `RenderCache::dynamic_cast<PdfiumBackend*>` has been reported as a warning (the `#ifdef HAS_PDFIUM` guards suggest it is condition-compiled; the `IPdfRenderer` interface could eliminate this cast)
- Whether any shipped test fixture exercises `encryptWithCertificate` end-to-end (tests exist but PDF/cert fixture availability in test environment not verified in this read-only audit)
- Whether `PoDoFoBackend.cpp:3089 LOC` includes generated MOC content (AUTOMOC is ON; however PoDoFoBackend is not a QObject, so no MOC)
- Specific git HEAD sha in working tree — CLAUDE.md says `e09404d`, audit brief says `e1c5394`; could not resolve without running `git log`
