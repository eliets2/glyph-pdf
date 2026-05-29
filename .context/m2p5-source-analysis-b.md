# M2-PROMPT-5 Source Analysis B: CMake + ROADMAP + Test Patterns

## CMakeLists.txt: Qt6::Concurrent linked?

**YES — Qt6::Concurrent is explicitly required and linked to both pdfws_engines and test targets.**

- **Line 27:** `find_package(Qt6 COMPONENTS ... Concurrent REQUIRED)` — mandatory component
- **Line 152:** `pdfws_engines` links `Qt6::Concurrent` as PRIVATE dependency
- **Line 460:** TestAutosave links `Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent`
- **Line 532:** TestThreadSafety links `Qt6::Test Qt6::Widgets Qt6::Concurrent podofo::podofo`

**Ready for LaneScheduler:** No additional Qt6::Concurrent linkage needed; infrastructure is in place.

---

## CMakeLists.txt: pdfws_engines source list pattern

The `pdfws_engines` STATIC library uses an **explicit hand-maintained file list** (lines 119–146), not a glob:

```cmake
add_library(pdfws_engines STATIC
    src/core/AnnotationSerializer.h  src/core/AnnotationSerializer.cpp
    src/core/ErrorInfo.h             src/core/ErrorInfo.cpp
    src/core/TempFileManager.h       src/core/TempFileManager.cpp
    ...
    src/engines/podofo/PdfStringEscape.h src/engines/podofo/PdfStringEscape.cpp
    src/engines/podofo/GlyphAdvanceCalculator.h src/engines/podofo/GlyphAdvanceCalculator.cpp
    ...
    src/engines/qpdf/QpdfBackend.h     src/engines/qpdf/QpdfBackend.cpp
    ...
    src/core/interfaces/IPdfDocument.h
    src/core/interfaces/IPdfPage.h
    src/core/interfaces/IPdfRenderer.h
    src/core/interfaces/IPdfWriter.h
    src/core/interfaces/IPdfSearcher.h
)
```

**Pattern:** Explicit pairs `(Header.h, Header.cpp)` for moc-requiring classes; interface headers alone for pure abstractions.

---

## CMakeLists.txt: how to add scheduling/ subdirectory sources

**Option A: Explicit file list (matches current style)**
```cmake
# After line 146, before closing paren:
    src/engines/scheduling/LaneScheduler.h  src/engines/scheduling/LaneScheduler.cpp
    src/engines/scheduling/GpuLaneWorker.h  src/engines/scheduling/GpuLaneWorker.cpp
    src/engines/scheduling/CpuLanePool.h    src/engines/scheduling/CpuLanePool.cpp
```

**Option B: add_subdirectory (cleaner for multi-file subsystem)**
```cmake
# After pdfws_engines target definition (after line 255):
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/engines/scheduling/CMakeLists.txt")
    add_subdirectory(src/engines/scheduling)
    target_link_libraries(pdfws_engines PRIVATE scheduling_objs)
endif()
```

Then `src/engines/scheduling/CMakeLists.txt`:
```cmake
add_library(scheduling_objs OBJECT
    LaneScheduler.h LaneScheduler.cpp
    GpuLaneWorker.h GpuLaneWorker.cpp
    CpuLanePool.h   CpuLanePool.cpp
)
target_include_directories(scheduling_objs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(scheduling_objs PUBLIC Qt6::Concurrent)
```

**Recommendation:** Use **Option A** (explicit list) to keep the project's minimalist hand-maintained style; only resort to add_subdirectory if scheduling grows beyond 5 files.

---

## CMakeLists.txt: test registration pattern (full example)

From **TestThreadSafety** (lines 525–540):

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestThreadSafety.cpp")
    add_executable(TestThreadSafety
        tests/TestThreadSafety.cpp
    )
    target_include_directories(TestThreadSafety PRIVATE src tests)
    target_link_libraries(TestThreadSafety PRIVATE
        pdfws_engines pdfws_core
        Qt6::Test Qt6::Widgets Qt6::Concurrent podofo::podofo
    )
    add_test(NAME TestThreadSafety COMMAND TestThreadSafety)
    set_tests_properties(TestThreadSafety PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
        LABELS "unit;threads;qt;headless"
    )
endif()
```

**Anatomy:**
1. **Existence guard:** `if(EXISTS ...)` — test file is optional
2. **Executable:** single test source file; no resource files
3. **Include paths:** `PRIVATE src tests` — tests can #include from both src and tests/
4. **Linkage:** pdfws_engines + pdfws_core + Qt6::Test + Qt6::Widgets + Qt6::Concurrent (for async tests) + optional podofo::podofo (direct PDF fixture creation)
5. **add_test:** registers the executable as a CTest test
6. **set_tests_properties:** sets env var `QT_QPA_PLATFORM=offscreen` (headless GUI), TIMEOUT (in seconds), and LABELS (for ctest filtering: `-L unit`, `-L threads`, etc.)

---

## scheduling/ directory

**Does NOT exist yet.** Current `src/engines/` contains:

```
AutosaveManager.{h,cpp}
BackendRouter.{h,cpp}
CollaborationManager.{h,cpp}
ConversionManager.{h,cpp}
DiffEngine.{h,cpp}
DocumentSession.{h,cpp}
FormManager.{h,cpp}
OcrEngine.{h,cpp}
PdfEditorEngine.{h,cpp}
RenderCache.{h,cpp}
SignatureManager.{h,cpp}
VeraPdfValidator.{h,cpp}

Subdirs:
  ocr/           (RapidOcrEngine, OcrPreprocessor, OcrPipeline — Session 9 WS1)
  pdfium/        (PdfiumBackend, tiled rendering — Session 4)
  podofo/        (PdfStringEscape, GlyphAdvanceCalculator, PoDoFoBackend — Sessions 3, 7)
  qpdf/          (QpdfBackend, save pipeline — Session 5)
```

**Recommendation:** Create `src/engines/scheduling/` before Session 9 M2-PROMPT-5 work; initialize with stub `LaneScheduler.h` and empty `.cpp` file in CMakeLists.txt.

---

## ROADMAP.md WS1 LaneScheduler section

**Architecture (lines 28–40):**

```
Heterogeneous Lane Scheduler (NEW — WS1)

GPU Lane  ──────────────────────────────────────────────────────────────
Small concurrency · models loaded once into persistent warm worker
Reused ONNXRuntime session · long-lived sidecar with IPC
NEVER spawn-per-page

CPU Lane  ──────────────────────────────────────────────────────────────
Tesseract 5.5.x (core-count parallelism) · PP-layout detectors
Extended from existing QtConcurrent + QFutureWatcher (bounded scheduler)

Cross-page pipeline:  layout(P+1) ║ ocr(P) ║ fusion(P-1)
```

**D5 Deliverable spec (lines 331–341):**

```
D5: Lane Scheduler
- src/engines/ocr/LaneScheduler.h/.cpp
- GPU lane: QSemaphore-bounded queue (default: 2 concurrent ONNX inferences); persistent
  worker thread holds warm OrtSession + warm ONNXRuntime environment
- CPU lane: QtConcurrent::mapped with QThreadPool sized to QThread::idealThreadCount()
- Task tagging: enum class Lane { GPU, CPU, Any }
- Bounded scheduler: max-in-flight cap per lane to prevent OOM on large documents
- Cross-page pipelining: scheduler issues layout(P+1) while ocr(P) is running and
  fusion(P-1) is writing; implemented as a 3-stage pipeline with QFuture chaining
- Architecture note: This scheduler is a Phase 1/2 infrastructure dependency, not OCR-only.
  Reused for WS3 (MRC per-page compression) and any future GPU-accelerated workload.
```

**Note:** The ROADMAP places LaneScheduler in `src/engines/ocr/` but recommends moving to `src/engines/scheduling/` for reusability.

---

## ROADMAP.md R10 risk entry

**R10 (lines 544):**

```
| R10 | Cross-page pipeline layout(P+1) ║ ocr(P) produces out-of-order results on error |
      | MEDIUM | LaneScheduler result queue is ordered by page index; missing pages
      | produce a sentinel error result, not a gap |
```

**Implication for LaneScheduler design:**
- Result queue must be **keyed by page index** (not insertion order)
- When a task fails or is skipped, emit a **sentinel error result** with that page index
- Consumer (OcrPipeline) checks each result; on sentinel, records error but doesn't gap the page list
- Prevents silent truncation if one page's layout detection fails

---

## TestThreadSafety.cpp timing patterns

**No QTest::qWait, QSignalSpy, or QTRY_ found.** Instead:

1. **std::async for concurrent tasks** (lines 90–104, 118–126, 139–147):
   ```cpp
   auto future = std::async(std::launch::async, [&]() { /* task */ });
   future.wait();
   QVERIFY(future.get());
   ```
   — Launches task on thread pool; waits indefinitely; gets result and verifies it.

2. **QFuture::waitForFinished** for bulk waits (lines 61–63):
   ```cpp
   QList<QFuture<void>> futures;
   for (...) futures.append(QtConcurrent::run([...] { /* work */ }));
   for (auto &f : futures) f.waitForFinished();
   ```
   — Blocks until all async tasks finish. Used for race-condition stress tests.

3. **Debug assertions embedded in the tested class** (lines 66–73):
   ```
   // m_totalBytes == sum(value.image.sizeInBytes() for v in m_renderedPages.values())
   // This assertion runs during evictIfNeeded; if violated, test crashes
   // Successfully completing means assertions passed.
   ```
   — Relies on `#ifndef QT_NO_DEBUG Q_ASSERT` inside RenderCache; no explicit timeout.

4. **No timing margins in the test itself** — asserts don't use elapsed time; they check invariants post-hoc.

**Pattern for LaneScheduler tests:** Use `QFuture::waitForFinished()` for bounded-time waits on GPU/CPU lane tasks; embed internal asserts in LaneScheduler for queue-order invariants.

---

## TestPerformance.cpp timing pattern

**QElapsedTimer with explicit < threshold assertions:**

```cpp
QElapsedTimer timer;
timer.start();

// ... operation ...

qint64 elapsed = timer.elapsed();
qDebug() << "Operation:" << elapsed << "ms";
QVERIFY2(elapsed < 2000, qPrintable(
    QString("Op took %1 ms (target: < 2000 ms)").arg(elapsed)));
```

**Pattern details:**
- **QElapsedTimer:** high-precision timer (nanosecond precision internally)
- **timer.elapsed():** returns milliseconds as `qint64`
- **QVERIFY2:** takes boolean + diagnostic message (second arg is fallback error text)
- **qPrintable() + QString::arg():** formats detailed message including actual vs. target
- **Thresholds used:** 2000 ms (open), 5000 ms (save), various others

**Margins employed:**
- Open: < 2000 ms
- Save: < 5000 ms
- Metadata cycles: < 2000 ms for 100 ops (20 ms per op allowed)
- Error cycles: < 5000 ms for 1000 ops (5 ms per op)

**For LaneScheduler cross-page pipeline test:**
- Time from issuing layout(P+1) to completing fusion(P-1)
- Suggested threshold: < 500 ms for 10-page batch on modern hardware
- Example: `QVERIFY2(pipelineElapsed < 500, ...)` with actual ms logged

---

## Key decisions for LaneScheduler tests

### 1. GPU Lane Serialization Test (timing-sensitive)
**Recommendation:** Use `QTest::qWait(N)` style for bounded wait; **NOT found in current codebase**, so implement via `QEventLoop`:

```cpp
void testGpuLaneSerializes() {
    LaneScheduler scheduler;
    
    // Submit 5 tasks to GPU lane
    QList<QFuture<...>> gpuResults;
    for (int i = 0; i < 5; ++i) {
        gpuResults.append(scheduler.submitToGpuLane(task_i));
    }
    
    // Verify queue size bounded (default: 2)
    QVERIFY(scheduler.gpuLaneQueueSize() <= 2);
    
    // Wait for all to finish (no timeout; GPU lane is serial anyway)
    for (auto &f : gpuResults) f.waitForFinished();
    QVERIFY(gpuResults.size() == 5);
}
```

**Timing margin:** None; GPU serialization is deterministic. Just verify queue cap.

### 2. Cross-Page Pipeline Timing Assertion
**Recommendation:** Use `QElapsedTimer` for end-to-end throughput:

```cpp
void testCrossPagePipelineLatency() {
    LaneScheduler scheduler;
    QElapsedTimer timer;
    timer.start();
    
    // Submit 10 pages to pipeline: layout → ocr → fusion
    QList<QFuture<OrderedResult>> results = scheduler.processPagesInPipeline(
        pages, LaneScheduler::Lane::Any);
    
    for (auto &f : results) f.waitForFinished();
    qint64 elapsed = timer.elapsed();
    
    qDebug() << "Pipeline 10 pages:" << elapsed << "ms";
    QVERIFY2(elapsed < 3000, qPrintable(
        QString("Pipeline took %1 ms (target: < 3000 ms)").arg(elapsed)));
}
```

**Suggested margin:** < 3000 ms for 10 pages = 300 ms/page (conservative; real performance typically 50–100 ms/page with GPU warm).

### 3. Sentinel Error Result Queue Ordering
**Recommendation:** Use direct invariant check on result map:

```cpp
void testOrderedResultsOnError() {
    LaneScheduler scheduler;
    
    // Page 1 succeeds, page 2 fails, page 3 succeeds
    QMap<int, OrderedResult> results;
    results[1] = scheduler.processPage(page1).result();  // OK
    results[2] = scheduler.processPage(page2_broken).result();  // Error sentinel
    results[3] = scheduler.processPage(page3).result();  // OK
    
    // Verify ordering: all keys are present; result[2].isError() == true
    QVERIFY(results.keys() == QList<int>{1, 2, 3});
    QVERIFY(results[2].isError());
    QVERIFY(!results[1].isError());
    QVERIFY(!results[3].isError());
}
```

**No timing margins needed:** Queue ordering is a structural property, not a timing property.

---

## CHANGELOG.md: where to insert LaneScheduler entry

**Top section: `## [Unreleased] — v1.0.0 Branch C SCOPE LOCK execution (M2-M8)` (line 4)**

Insert new subsection **after** "Redaction Hardening / Extended Sanitization (M2-PROMPT-1/2)" and **before** "PDF/A Validation", under a new heading:

```markdown
### OCR Ensemble Pipeline + Lane Scheduler Infrastructure (M2-PROMPT-5 — 2026-06-XX)

- **Lane Scheduler core infrastructure** (D1-D5):
  - New `src/engines/scheduling/LaneScheduler.h/.cpp` — heterogeneous hardware-aware task scheduler
  - GPU lane: `QSemaphore`-bounded queue (default: 2 ONNX inferences); persistent warm worker thread
  - CPU lane: `QtConcurrent::mapped` with `QThreadPool(QThread::idealThreadCount())`
  - `enum class Lane { GPU, CPU, Any }` task tagging; per-lane max-in-flight caps to prevent OOM
  - Cross-page pipelining: layout(P+1) ║ ocr(P) ║ fusion(P-1) via `QFuture` chaining
  - **Result queue ordered by page index; missing pages produce sentinel error results (R10 mitigation)**
  - CMake: explicit linkage `Qt6::Concurrent` in pdfws_engines; new test: `TestLaneScheduler`
- **Test coverage** (D6): concurrent GPU/CPU task submission, queue depth bounds, sentinel error handling, cross-page pipeline latency
```

**Rationale:** LaneScheduler is M2-PROMPT-5 scope (Session 9 WS1 infrastructure); insert chronologically after M2-PROMPT-1/2 (security) and before M2-PROMPT-3 (PDF/A).

---

## Summary Table

| Aspect | Finding |
|--------|---------|
| **Qt6::Concurrent linkage** | ✅ YES — required in find_package; linked to pdfws_engines + TestAutosave + TestThreadSafety |
| **pdfws_engines source style** | Explicit hand-maintained file list (no globs); header+source pairs for moc classes |
| **Add scheduling/ to CMakeLists.txt** | Option A (explicit list): append to pdfws_engines source list; Option B (add_subdirectory): if > 5 files. Prefer Option A per project style. |
| **Test registration pattern** | `if(EXISTS ...) add_executable(...) target_include_directories target_link_libraries add_test set_tests_properties` with `QT_QPA_PLATFORM=offscreen` env + TIMEOUT + LABELS |
| **scheduling/ dir exists?** | ❌ NO — create `src/engines/scheduling/` directory in M2-PROMPT-5 prep |
| **WS1 LaneScheduler arch** | GPU lane (QSemaphore 2-deep queue, warm ONNX), CPU lane (QtConcurrent, core-count pool), cross-page pipeline (layout→ocr→fusion) |
| **R10 risk** | Out-of-order results on error; mitigation: result queue keyed by page index, missing pages emit sentinel error results |
| **TestThreadSafety patterns** | std::async + future.wait(), QFuture::waitForFinished(), embedded Q_ASSERT, NO QTest::qWait or QSignalSpy |
| **TestPerformance patterns** | QElapsedTimer + timer.elapsed() (ms) + QVERIFY2(elapsed < threshold, ...) with qDebug logging; margins 2000–5000 ms |
| **LaneScheduler test margins** | GPU serialization: no margin (deterministic); cross-page pipeline: < 3000 ms for 10 pages (300 ms/page); sentinel ordering: structural, not timing |
| **CHANGELOG.md insertion point** | New subsection after M2-PROMPT-1/2 (redaction) and before M2-PROMPT-3 (PDF/A), titled "OCR Ensemble Pipeline + Lane Scheduler Infrastructure (M2-PROMPT-5)" |
