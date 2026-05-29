# M2-PROMPT-5 Source Analysis A: Concurrency Patterns + Integration Points

## RenderCache concurrency pattern

**Lock type used:** `QReadWriteLock` (line 6 of RenderCache.h, member variable `m_lock` at line 99)

**Key characteristics:**
- Read-write lock via `WriteLockGuard` helper (RenderCache.cpp lines 17-28) with thread-local state `t_writeLocked` to assert preconditions
- TOCTOU fix from M1: Lines 148–158 in RenderCache.cpp show the critical refactor:
  - **Before M1:** separate Read → Check → Write operations left race windows
  - **After M1:** Combined `WriteLockGuard` block (lines 149–158) that checks cache, updates LRU, and increments hit/miss stats **under write lock** before rendering
  - Same pattern applies to tile rendering (lines 189–199) and prefetch insertion (lines 294–304)
  - Prefetch tasks exit early if `m_prefetchCancelToken` changes, preventing stale writes (lines 276, 290)

**Prefetch lambda capture pattern:** Lines 268–307
- Captures `weak_from_this()` (line 268) as `std::weak_ptr<RenderCache>` to avoid circular ref
- Lock at entry: `auto self = weakThis.lock()` (line 271) safely handles stale ptr
- Runs via `QtConcurrent::run()` (global pool, no explicit QThreadPool)

**QFuture usage:** Yes — `std::shared_future<QSizeF>` (line 113) for lazy page-size resolution (promise-based, lines 67–69, 91–93)

**QThread usage:** No direct QThread; relies on QtConcurrent global pool.

**Stats:** Uses `QAtomicInt` for lock-free hit/miss counters (lines 105–106, accessed via `.loadRelaxed()` / `.fetchAndAddRelaxed()` in lines 66–67, 152, 161, etc.)

---

## OcrPipeline current concurrency

**QtConcurrent usage:** None currently in OcrPipeline.cpp itself.

**Important note:** OcrPipeline::run() (line 151–208) is **designed to be re-entrant and can be called concurrently**:
- Line 37–39 comment explicitly states: "OcrPreprocessor is stateless; OcrPipeline::run may be called concurrently. If preprocessor ever becomes stateful, add explicit synchronization."
- No internal locks; thread safety depends on IOcrEngine implementations (Tesseract, RapidOCR) being thread-safe or guarded externally

**QThreadPool:** None referenced. OcrEngine itself uses `QRecursiveMutex` (OcrEngine.cpp line 10, though not fully shown in truncated output).

**OnnxSession / ONNX Runtime:** Not referenced in OcrPipeline.cpp; likely in RapidOcrEngine or future secondary engines.

**What LaneScheduler will REPLACE here:**
- Current ad-hoc concurrency: When UI calls OcrPipeline::run(), it's typically wrapped in QtConcurrent::run() at the caller (e.g., BatchMode.cpp lines 18, 82–86 show `QtConcurrent::run([file1, file2]() { ... })` for DiffEngine; similar pattern would apply to OCR batch jobs)
- **M5 refactor:** LaneScheduler will be the centralized dispatcher for these async pipeline steps, replacing scattered QtConcurrent::run() + QFutureWatcher patterns

---

## AppContext.h current state

**Existing shared_ptr members:**
```
std::shared_ptr<IPdfEditorEngine>   pdfEditor;
std::shared_ptr<IOcrEngine>         ocr;
std::shared_ptr<IFormManager>       forms;
std::shared_ptr<ISignatureManager>  signing;
std::shared_ptr<IConversionEngine>  conversion;
std::shared_ptr<ICollaboration>     collab;
std::shared_ptr<QUndoStack>         undoStack;
std::shared_ptr<DocumentSession>    document;
std::shared_ptr<AutosaveManager>    autosave;
```

**Where to add LaneScheduler:**
- After line 24 (before closing brace), insert:
  ```
  std::shared_ptr<LaneScheduler>     scheduler;
  ```

**Include path for LaneScheduler:**
- Add to Bootstrapper.cpp: `#include "engines/scheduling/LaneScheduler.h"`
- LaneScheduler.h goes in: `src/engines/scheduling/LaneScheduler.h`

---

## Bootstrapper.cpp construction pattern

**Current construction order (Bootstrapper.cpp lines 16–31):**
1. `ocr` (OcrEngine)
2. `pdfEditor` (PdfEditorEngine)
3. `forms` (FormManager)
4. `signing` (SignatureManager)
5. `conversion` (ConversionManager)
6. `collab` (CollaborationManager)
7. `undoStack` (QUndoStack) — with setUndoLimit(200)
8. `document` (DocumentSession)
9. `autosave` (AutosaveManager) — depends on `pdfEditor`, `document`

**Destructor pattern:** AppContext is a plain struct; destruction follows RAII via shared_ptr ordering. Bootstrapper has no destructor, so cleanup is automatic when context goes out of scope.

**How existing services are stopped:**
- AutosaveManager: has `.stop()` method (AutosaveManager.h line 21), which calls `m_timer->stop()`
- No explicit stop pattern elsewhere visible in brief; relies on RAII and Qt's deferred deletion

**Where to add LaneScheduler construction:**
- **After OcrEngine (line 19), before PdfEditorEngine (line 20)**, OR
- **Last (after autosave, line 28)** if it depends on other services
- **Likely position:** Right after line 19, because:
  - OcrPipeline tasks will be dispatched via LaneScheduler
  - Bootstrapper should construct base services first (scheduler = base infra)
  - Then engines that use the scheduler (ocr, pdfEditor, etc.)
  
  ```cpp
  ctx.ocr        = std::make_shared<OcrEngine>();
  ctx.scheduler  = std::make_shared<LaneScheduler>();  // NEW
  ctx.pdfEditor  = std::make_shared<PdfEditorEngine>();
  ...
  ```

**Shutdown pattern to add:**
- If LaneScheduler has a `.stop()` or `.shutdown()` method, no explicit cleanup needed in Bootstrapper (RAII handles it)
- If it spins threads, ensure destructor or stop() method quiesces them before task queue destruction

---

## Files that use QThreadPool or QtConcurrent (global pool)

These are current/future consumers of LaneScheduler:

1. **C:\Users\User\Projects\pdf\src\engines\RenderCache.cpp**
   - Uses: `QtConcurrent::run()` for prefetch (line 270)
   - Future role: Render tasks dispatched via LaneScheduler

2. **C:\Users\User\Projects\pdf\src\modes\BatchMode.cpp**
   - Uses: `QtConcurrent::map()` for multi-file batch processing (line with `.map(m_filesToProcess, ...)`)
   - Future role: Batch pipeline stages (OCR, compress, watermark) → LaneScheduler lanes

3. **C:\Users\User\Projects\pdf\src\modes\CompareMode.cpp**
   - Uses: `QtConcurrent::run()` for DiffEngine (line 82)
   - Future role: Diff tasks → LaneScheduler

4. **C:\Users\User\Projects\pdf\src\engines\AutosaveManager.cpp**
   - Uses: `QtConcurrent::run()` (likely in onTick() at line 75; see full impl for exact pattern)
   - Future role: Autosave tasks → LaneScheduler

5. **C:\Users\User\Projects\pdf\src\shell\controllers\* (ConvertController, EditController, SecurityController)**
   - Include `<QThread>` (not shown in full, but used for controller-level async tasks)
   - Future role: Controller-initiated pipelines → LaneScheduler

---

## std::expected availability

**Status:** No usage found in codebase.
- Grep: `grep -rn "std::expected\|expected<"` returned no matches
- Codebase uses traditional error handling:
  - Boolean returns (e.g., `bool initialize()` in OcrEngine.h line 18)
  - Exceptions (implied in interfaces, not shown)
  - Qt signals/slots for async errors (e.g., `void autosaveFailed(const QString &reason)` in AutosaveManager.h line 28)

**Implication for LaneScheduler:**
- Can use `std::expected<T, ErrorInfo>` in M5 (M2 doesn't require it; M5 may benefit for error propagation)
- ErrorInfo struct exists (referenced in BatchMode.h line 2: `#include "core/ErrorInfo.h"`)

---

## Key design constraints from codebase

**1. Naming convention:**
- Classes: `PascalCase` (RenderCache, OcrEngine, AutosaveManager, DiffEngine, etc.)
- Files: `PascalCase.h` / `PascalCase.cpp` (RenderCache.h, OcrEngine.cpp, etc.)
- Namespaces: lowercase (e.g., `gp` in BatchMode/CompareMode)
- LaneScheduler should follow: `class LaneScheduler { ... }` in `src/engines/scheduling/LaneScheduler.h`

**2. Include style:**
- Qt includes without `<>` suffix: `#include <QReadWriteLock>` (not `QtCore/QReadWriteLock`)
- Forward declares: Use in .h for decoupling (e.g., AppContext.h line 5–9)
- Private impl pattern: `std::unique_ptr<Private> d;` (OcrEngine.h line 28, OcrPipeline.h line 61)

**3. Qt signal patterns:**
- No signals in engine classes (RenderCache, OcrEngine, OcrPipeline) — they're library-like
- Signals in UI/manager classes: AutosaveManager::autosaveCompleted(), autosaveFailed()
- LaneScheduler should probably use signals for task completion/error:
  ```cpp
  signals:
      void taskCompleted(const TaskId& id, const QVariant& result);
      void taskFailed(const TaskId& id, const QString& reason);
  ```

**4. Thread safety pattern:**
- Prefer `QReadWriteLock` for readers-heavy code (RenderCache: many getOrRender calls, few cache insertions)
- Use `QMutex` + `QMutexLocker` for write-heavy or simple sync (OcrEngine, PdfiumBackend)
- Atomic int for lock-free stats (QAtomicInt in RenderCache)
- Weak ptr + lock pattern for async callbacks (RenderCache prefetch)

**5. Lifecycle pattern:**
- Services constructed in Bootstrapper::createContext() as shared_ptr
- No explicit start() required in most cases; AutosaveManager::start() is exception (optional 5-min interval)
- Destruction automatic via scope or explicit reset
- LaneScheduler should follow similar pattern: construct in Bootstrapper, stop/destroy on app exit

**6. Error handling:**
- Mixture of boolean returns, Qt signals, and exception (likely)
- ErrorInfo struct used in BatchMode (core/ErrorInfo.h)
- Future: std::expected<T, ErrorInfo> can unify these

---

## Summary: Integration readiness for M2-PROMPT-5

| Aspect | Status | Notes |
|--------|--------|-------|
| **RenderCache** | Ready | Uses QtConcurrent globally; weak_ptr prefetch pattern can guide LaneScheduler callback design |
| **OcrPipeline** | Ready | Stateless, reentrant; add LaneScheduler dispatch logic post-M4 |
| **AppContext** | Ready | Adding `std::shared_ptr<LaneScheduler> scheduler;` is straightforward |
| **Bootstrapper** | Ready | Simple make_shared pattern; construct LaneScheduler early (after ocr or as base service) |
| **QtConcurrent users** | Candidates | RenderCache, BatchMode, CompareMode, AutosaveManager, controllers — all can migrate to LaneScheduler lanes |
| **std::expected** | Optional | Not currently used; M5 can introduce for error types |
| **Thread safety** | Proven patterns | QReadWriteLock (RenderCache), QMutex (engines), QAtomicInt (stats) — LaneScheduler can reuse |
| **Naming/style** | Aligned | LaneScheduler fits PascalCase/private-pimpl conventions |

**M2 scope:** LaneScheduler infrastructure (lanes, lanes registry, work item protocol, lifecycle, metrics).
**M3 scope:** Lane implementations (render lane, OCR lane, etc.).
**M4 scope:** Integration points in AppContext, Bootstrapper, consumer refactors.
**M5 scope:** Migrate QtConcurrent callers (RenderCache prefetch, BatchMode, CompareMode, etc.) to LaneScheduler.