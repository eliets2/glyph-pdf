# M7-PROMPT-2 Walkthrough: Performance Tuning + Bug Bash

**Date:** 2026-06-02  
**Commits:** 76264d5 (D1), e2dd0e8 (D2), this commit (D5-MEMORY)  
**Agent:** Claude Sonnet 4.6 (claude-sonnet-4-6)  
**Scope guard:** Did NOT touch CMakeLists.txt, src/engines/ocr/, src/engines/scheduling/

---

## Honest deliverable status

Per the prompt's honest scope assessment — reproduced here to satisfy anti-pattern #10
(Universal "everything passes" syndrome).

| Deliverable | Status | Notes |
|---|---|---|
| D1 — Performance baselines + regression suite | DONE | 6 benchmarks added, 28/28 ctest pass |
| D2 — Profile hot paths | DONE (analysis) | Tracy not in MSYS2; code-review analysis written |
| D3 — Bug bash | DEFERRED — HUMAN-GATED | Requires real beta testers |
| D4 — 48h soak test | DEFERRED — HUMAN-GATED | Requires 48 wall-clock hours |
| D5-MEMORY — Walkthrough + vault text | DONE | This file |

---

## D1 — Performance baselines + regression suite

**Commit:** `76264d5` — `test(perf): performance baselines + regression suite (M7-P2 D1)`  
**File changed:** `tests/TestPerformance.cpp` (330 insertions, 111 deletions)

### What was done

Extended TestPerformance.cpp with 6 regression-locked benchmarks, replacing the 4 original
tests (which had no named threshold constants and used scattered magic numbers).

**Changes:**
- Added `namespace PerfTarget{}` with 6 named threshold constants — single-point adjustment.
- Fixed the `createMultiPagePdf()` helper: the original xref offset calculation was
  incorrect (object ordering in the xref table did not match actual byte positions), which
  caused PoDoFo 1.1.0 to emit hundreds of "Treating object N as unavailable" warnings on
  every test run. Rewritten with an explicit `QVector<int> xrefOffset` recorded during the
  forward pass.
- Added 6 test slots:
  - `benchOpenDocument` — 100-page open `< PerfTarget::OpenMs` (3 000 ms)
  - `benchTextSearch` — RenderCache text-layer scan for 100 pages `< PerfTarget::SearchMs`
    (1 000 ms). Note: exercises the RenderCache text layer (getOrExtractText + insertText)
    since the engine-level text search API is not yet exposed through PdfEditorEngine's
    facade (wired in M6-P4 / M6-P5 for find-and-replace). This is an honest proxy.
  - `benchRenderCacheHitRate` — 50-entry warm-cache hit rate `>= PerfTarget::HitRatePct`
    (95 %). Tests that LRU eviction does not kick out freshly inserted entries.
  - `benchMetadataOperations` — 100 getMetadata/setMetadata round-trips `< PerfTarget::MetadataMs`
    (2 000 ms)
  - `benchSaveDocument` — 50-page save `< PerfTarget::SaveMs` (5 000 ms)
  - `benchErrorHandlingOverhead` — 1 000 error cycles `< PerfTarget::ErrorCycleMs`
    (5 000 ms)

### Tests added

```
TestPerformance::benchOpenDocument       PASS
TestPerformance::benchTextSearch         PASS
TestPerformance::benchRenderCacheHitRate PASS
TestPerformance::benchMetadataOperations PASS
TestPerformance::benchSaveDocument       PASS
TestPerformance::benchErrorHandlingOverhead PASS
```

### ctest result (verbatim)

```
test 16
    Start 16: TestPerformance
16: Test command: C:\Users\User\Projects\pdf\build\TestPerformance.exe
16: Environment variables:
16:  QT_QPA_PLATFORM=offscreen
1/1 Test #16: TestPerformance ..................   Passed    0.10 sec

100% tests passed, 0 tests failed out of 1
```

Full baseline:
```
100% tests passed, 0 tests failed out of 28
```

### Limitation noted

The `benchTextSearch` test exercises `RenderCache::getOrExtractText` rather than a
document-level `searchText()` API call, because `PdfEditorEngine` does not yet expose a
unified text-search method through its public interface. The `IPdfSearcher::searchText()`
method exists on `PdfiumBackend` but is not routed through the engine facade. When M6-P4
(find-and-replace wiring) lands, this test should be upgraded to call the real search path.

### TODOs inserted

None. No `TODO(audit-*)` or `FIXME` comments were added.

---

## D2 — Hot-path analysis + profiling setup notes

**Commit:** `e2dd0e8` — `perf: hot-path analysis + profiling setup notes (M7-P2 D2)`  
**File created:** `docs/performance/hot-path-analysis.md` (370 lines)

### Tracy / Easy_Profiler availability

**Tracy:** NOT available in MSYS2 ucrt64 as of 2026-06-02.
- Command: `pacman -Ss tracy` → exit code 1, no matches.

**Easy_Profiler:** NOT available in MSYS2 ucrt64 as of 2026-06-02.
- Command: `pacman -Ss easy_profiler` → exit code 1, no matches.

**Samply** IS available: `mingw-w64-ucrt-x86_64-samply` — a sampling profiler that
produces Firefox Profiler flamegraphs. No recompilation required.

### Hot paths identified (code-review analysis, not fabricated profiler output)

| Rank | Function | File | Dominant cost | Priority |
|---|---|---|---|---|
| 1 | `RenderCache::getOrRender` | `RenderCache.cpp:126` | O(N) `QList::removeOne` on every cache hit | HIGH — called every scroll tick |
| 2 | `redactCanvasRecursively` | `PoDoFoBackend.cpp:972` | O(T×R) rect-scan + 2 zlib cycles per XObject | HIGH — user-perceptible latency |
| 3 | `OcrPipeline::roverMerge` | `OcrPipeline.cpp:88` | O(P×S×W) text-similarity scan | MEDIUM — low-confidence OCR path only |
| 4 | `SignatureManager::buildDssDictionary` | `SignatureManager.cpp:347` | Full file re-read + redundant CMS parse | MEDIUM — PAdES B-LT/LTA only |
| 5 | `PpOcrDecoder` preprocessing | `PpOcrDecoder.cpp` | Scalar float normalise 200×3×48×W per page | LOW for now (M5 still blocked on models) |

For each: full complexity table + specific optimisation recommendation in
`docs/performance/hot-path-analysis.md`.

---

## D3 — Bug bash (HUMAN-GATED — deferred)

**Status:** Cannot be performed by an agent. Requires real beta testers.

**Why it is deferred:** A multi-day interactive bash with internal/beta testers requires:
- Human-driven UI workflows (the agent cannot operate the GUI interactively)
- Real user document corpora (cannot be synthesised)
- Human judgment on crash reproducibility and severity triage

**Instructions for the human executing D3:**

1. Build and distribute the v1.0.0-beta MSI from HEAD:
   ```bash
   export PATH="/c/msys64/ucrt64/bin:$PATH"
   cd /c/Users/User/Projects/pdf/build
   cmake --build . --parallel 8 --target package
   # Distribute the resulting .msi from build/packages/
   ```

2. Provide testers with a structured bash sheet covering:
   - Document open (native PDF, scanned PDF, password-protected PDF)
   - Text search + find-and-replace
   - Manual + pattern redaction
   - Signature placement + validation
   - Form filling (text fields, checkboxes, dropdowns)
   - OCR on scanned documents
   - Batch export (PDF → HTML, PDF → text, PDF → images)
   - Page operations (rotate, reorder, delete, insert blank)

3. Triage reported issues:
   - **Release-blocker:** Data loss, security regression, crash on standard workflow
   - **P1:** Functional regression versus stated feature
   - **P2:** UX friction, unexpected behaviour, cosmetic defect
   - **P3:** Cosmetic / nice-to-have

4. Fix release-blockers before M8 ship gate. Each fix must have a regression test.
   Commit format: `fix(<scope>): <description> (M7-P2 bug-bash)`

5. After all release-blockers are closed, update CHANGELOG `[Unreleased]` section with
   `### Fixed` entries for each bug-bash fix.

---

## D4 — 48-hour soak test (HUMAN-GATED — deferred)

**Status:** Cannot be performed by an agent. Requires 48 wall-clock hours + human monitoring.

**Why it is deferred:** 48 wall-clock hours cannot complete in any agent session. Memory
leak detection also requires Heaptrack (Linux) or Dr. Memory / VLD (Windows) to be set up
and the agent cannot interpret multi-hour memory growth data.

**Instructions for the human executing D4:**

1. Build a stress-test binary based on `TestIntegration` that loops indefinitely:
   ```
   open PDF → search for text → redact 1 region → save → close  [repeat]
   ```
   Use a corpus of at least 3 different PDFs (simple, scanned, signed).

2. Run for 48 hours with memory monitoring:
   ```powershell
   # PowerShell: log working set every 5 minutes to a CSV
   $proc = Start-Process .\stress_test.exe -PassThru
   while (-not $proc.HasExited) {
       $proc.Refresh()
       "$([DateTime]::Now),$($proc.WorkingSet64)" | Out-File -Append soak_memory.csv
       Start-Sleep 300
   }
   ```

3. Alternatively, use Dr. Memory for leak detection:
   ```bash
   # Download Dr. Memory from https://drmemory.org
   drmemory.exe -leaks_only -- ./stress_test.exe
   ```

4. After 48 hours: plot `soak_memory.csv`. If working set grows > 10 % after 1 000 cycles,
   attach a profiler snapshot and investigate with Samply:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-samply
   samply record ./stress_test.exe
   ```

5. Fix any confirmed leaks. Commit format: `fix(memory): <description> (M7-P2 D4 soak)`

---

## CHANGELOG update required

Add to `CHANGELOG.md` under `[Unreleased] → ### Changed`:
```
- Performance regression suite: 6 benchmarks added to TestPerformance (open <3s,
  search <1s, RenderCache hit rate >=95%, metadata 100 cycles <2s, save <5s,
  error overhead <5s) — locked by PerfTarget{} constants (M7-P2 D1)
- Hot-path analysis written for top-5 performance-sensitive functions (M7-P2 D2)
- Bug bash (D3) and 48h soak (D4) deferred to human-driven M7 beta phase
```

---

## Vault update text (SUGGESTED — do NOT write to vault from this session)

Add to `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` under the
commit-by-commit map section:

```markdown
| `76264d5` | 2026-06-02 | Claude Sonnet 4.6 | **M7-P2 D1** — 6 regression-locked perf benchmarks in TestPerformance; PerfTarget{} constants; xref bug fix in createMultiPagePdf |
| `e2dd0e8` | 2026-06-02 | Claude Sonnet 4.6 | **M7-P2 D2** — docs/performance/hot-path-analysis.md: top-5 hot paths + algorithmic analysis + Samply/Tracy profiling setup notes |
| `<this>` | 2026-06-02 | Claude Sonnet 4.6 | **M7-P2 D5-MEMORY** — walkthrough m7-prompt-2-walkthrough.md; D3/D4 deferred with explicit human instructions |
```

Also update `## Tests` status line:
```
- **Tests:** 28/28 ctest (unchanged — TestPerformance extended, no new targets)
```

Also note under `## Known limitations`:
```
- **D3 bug bash:** Requires M7 beta program — not yet started
- **D4 48h soak:** Requires 48h wall time + human monitoring — not yet started
- **benchTextSearch** proxy: uses RenderCache text layer, not engine-level searchText()
  (upgrade when M6-P4 find-and-replace wiring lands)
```
