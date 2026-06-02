# GlyphPDF Hot-Path Analysis (M7-P2 D2)

**Date:** 2026-06-02  
**Method:** Code-review-based analysis (static complexity + algorithmic reasoning).  
**Data source:** Source inspection + wall-clock timings from TestPerformance.cpp benchmarks.  
**Profiler availability:** Tracy — NOT available in MSYS2 ucrt64 (`pacman -Ss tracy` returned no results).  
Easy_Profiler — NOT available in MSYS2 ucrt64 (`pacman -Ss easy_profiler` returned no results).  
Available MSYS2 profilers: `mingw-w64-ucrt-x86_64-samply` (sampling profiler) and KCacheGrind.  
See [Profiling Setup Notes](#profiling-setup-notes) at the bottom for exact install commands.

This document identifies the five most performance-sensitive functions in the GlyphPDF
codebase, explains why each is hot, states its algorithmic complexity, and proposes
concrete optimisations for future implementation.

> **Honesty note:** All complexity analysis and timing observations below are derived from
> reading the source code and running the TestPerformance regression suite. No fabricated
> profiler output is present. Timings marked "(TestPerformance)" come from real ctest runs.

---

## Hot Path 1 — `RenderCache::getOrRender` + LRU management

**File:** `src/engines/RenderCache.cpp` lines 126–185  
**Called by:** `PdfViewerWidget`, `ThumbnailSidebar`, `CompareWidget` on every page-view event

### Why it is hot

Every time the user scrolls, zooms, or switches pages, `getOrRender()` is called once per
visible page (and up to N+2 more times via `prefetchViewport`). On a 300-DPI display showing
a 2-page spread, this means 6-10 calls per scroll tick. The function is the single most
frequently invoked non-trivial method in the rendering stack.

### Algorithmic complexity

| Operation | Complexity | Notes |
|---|---|---|
| Cache lookup (`QHash::contains`) | O(1) average | Hash of `{page, scale, isTile, subRect}` |
| LRU update (`QList::removeOne`) | **O(N)** | N = number of cached entries (can be hundreds) |
| LRU prepend (`QList::prepend`) | O(1) amortised | Qt QList uses an array; prepend may realloc |
| `evictIfNeeded()` | O(k) — k = evicted entries | Called inside write lock; each eviction is O(N) removeOne |

The dominant cost is `m_lruList.removeOne(key)` — a linear scan through the LRU list every
hit and every miss. For a 256 MB cache holding ~64 rendered pages, N is small and this is
acceptable. If the cache grows to 500+ entries (e.g., a user who tiles at 400 DPI and
navigates a 200-page document), the O(N) scan becomes measurable.

### What optimisation would help

Replace `QList<RenderCacheKey>` with a doubly-linked list whose nodes are stored in a
`QHash<RenderCacheKey, LruNode*>`. Both removeOne and prepend become O(1) pointer
manipulations. This is the standard LRU-cache data structure used by `std::list` +
`std::unordered_map`. Expected benefit: removes the O(N) scan entirely.

Secondary: the `WriteLockGuard` blocks all concurrent readers during LRU update. With the
O(1) node swap, the critical section is short enough to consider a single `QMutex` instead
of `QReadWriteLock`, which has lower acquisition overhead on Windows when there is no reader
contention.

---

## Hot Path 2 — `redactCanvasRecursively` in `PoDoFoBackend::applyRedactions`

**File:** `src/engines/podofo/PoDoFoBackend.cpp` lines 972–1285  
**Called by:** `PdfEditorEngine::applyRedactions` and `applyPatternRedactions`

### Why it is hot

Redaction is a full content-stream parse-and-rewrite: the function deserialises the page's
compressed byte stream into a `PdfContentStreamReader` event sequence, applies bounding-box
intersection tests against each text operator's implied position, and rewrites the entire
stream into a new `std::ostringstream`. For a dense legal document (50+ text operators per
page) applied to 30 redaction regions, this is the single most CPU-intensive user-initiated
operation in the app.

The function is also called recursively for every Form XObject on the page (line 1255), so a
page with 10 embedded XObjects triggers 11 stream parse/rewrite cycles.

### Algorithmic complexity

| Operation | Complexity | Notes |
|---|---|---|
| Stream decompression (`CopyTo`) | O(S) — S = compressed stream size | zlib inflate |
| Content-stream token scan | O(T) — T = token count | Linear scan, no indexing |
| Per-operator rect intersection | O(T × R) | R = number of redaction rects; inner loop `isIntersectingSpan` |
| Stream rewrite into ostringstream | O(T) | |
| Stream compression on write-back | O(S) | zlib deflate |
| Recursive XObject descent | O(X × above) | X = XObject count |

For R = 30 rects and T = 5 000 tokens (dense page), this is ~150 000 rect-test iterations
per page. Not individually expensive, but the decompression + recompression pass (two zlib
cycles per XObject) dominates wall time.

### What optimisation would help

1. **Spatial index for rect tests.** Build a 1D interval tree or sorted array of `pdfRects`
   ordered by Y, then by X. Replace the O(R) linear scan in `isIntersectingSpan` with an
   O(log R) binary-search range query. For R <= 10 this doesn't matter; for R = 100+ (batch
   redaction), it shaves a measurable amount.

2. **Decompression-free scan for no-op pages.** Before paying the zlib inflate cost, check
   the compressed-stream header: if the stream's `/Filter` is `/FlateDecode` and the page's
   bounding box does not overlap any redact rect, skip the page entirely. This short-circuit
   saves the full decompression cost for untouched pages in multi-page batch redaction.

3. **Single-pass stream.** The current implementation concatenates into an `ostringstream`
   then writes to PoDoFo. Using PoDoFo's `PdfContentStreamWriter` to write incrementally
   avoids the O(S) allocation of a second in-memory string.

---

## Hot Path 3 — `OcrPipeline::roverMerge` (word-level ROVER)

**File:** `src/engines/ocr/OcrPipeline.cpp` lines 88–200  
**Called by:** `OcrPipeline::processImage` when secondary engine is invoked (low-confidence path)

### Why it is hot

ROVER merge runs after both Tesseract and RapidOCR have processed the same image. The merge
performs a word-level alignment between two word sequences (primary and secondary). For a
dense OCR page with 500 words per engine, and treating each word as an independent token,
the word-alignment loop iterates O(primary.size() × secondary.size()) comparisons in the
worst case (when no direct-position match is found and the fallback text-similarity scan
runs).

### Algorithmic complexity

From the implementation (lines 88–160):

- **Primary pass:** O(P) — iterate primary words, look up a matching secondary word in a
  text-similarity scan: O(P × S) worst case where P = primary word count, S = secondary.
- **Text similarity:** `contains()` substring check — O(W) where W = word length.
- **Overall:** O(P × S × W) per page in the worst case. For P=S=500, W=10 average chars:
  ~2.5M character comparisons per page pair.

For most pages the match rate is high (same word recognized by both engines) and the cost
stays near O(P) via early exit. The worst case is scanned pages with low OCR confidence
where secondary has few matches, forcing the full quadratic scan.

### What optimisation would help

Replace the linear text-similarity scan with a hash map indexed by normalised word text.
Build `QHash<QString, int> secondaryByText` before the merge loop. For each primary word,
look up the secondary map in O(1). This reduces the overall merge from O(P × S × W) to
O(P + S). For 500-word pages, the improvement is ~500×.

Secondary: for position-based matching (words overlap by bounding box), sort secondary words
by X-coordinate and use a binary search to find candidates in the X-range rather than a
linear scan.

---

## Hot Path 4 — `SignatureManager::buildDssDictionary` (byte-range + DSS embedding)

**File:** `src/engines/SignatureManager.cpp` lines 347–596  
**Called by:** `signDocument` when PAdES B-LT or B-LTA level is requested

### Why it is hot

`buildDssDictionary` is the PAdES Long-Term Validation embedding step. It:
1. Reloads the signed PDF from disk (full file read: O(F) bytes).
2. Walks all signature fields and extracts `/Contents` (raw CMS blobs).
3. Parses each CMS blob to extract embedded certificates and OCSP responses.
4. Calls `OCSP_basic_verify` for each response (network + crypto: dominant cost when TSA is
   reachable, but in unit-test context it is a local verify).
5. Encodes DER for all certs + OCSPs + CRLs.
6. Writes back the entire PDF with the new `/DSS` dictionary appended.

Step 6 is an incremental-update save — it appends an xref increment rather than rewriting
the entire file — so it scales with the size of the DSS data, not the full PDF size. However
steps 1 and 2 are unconditional full-file reads.

### Algorithmic complexity

| Step | Complexity | Notes |
|---|---|---|
| Full file read | O(F) | F = file size; unavoidable |
| CMS blob parse (OpenSSL d2i_CMS) | O(B) | B = CMS blob size |
| OCSP_basic_verify | O(C × N) | C = cert chain length, N = OCSP response count |
| DER encode all certs | O(C × cert_size) | |
| Incremental PDF write | O(DSS_size) | Only appends; does not rewrite |

The overall cost is dominated by the full file read on step 1, which is unavoidable given
that the signing step writes the signed PDF to disk and the DSS embedding step must read it
back. On a 50 MB legal PDF this is ~50 MB of I/O.

### What optimisation would help

1. **In-memory handoff.** If `signDocument` and `buildDssDictionary` are called in the same
   process, the signed PDF could be held in a `QBuffer` (in-memory byte stream) rather than
   written and re-read from disk. This eliminates the full-file I/O for the DSS pass.
   PoDoFo 1.1.0 supports `PdfMemDocument::LoadFromBuffer`.

2. **Cache the CMS parse result.** `signDocument` already constructs the CMS object;
   instead of re-parsing it in `buildDssDictionary`, pass the already-parsed cert chain and
   OCSP responses as parameters. Eliminates one redundant `d2i_CMS_ContentInfo` call.

---

## Hot Path 5 — `PpOcrDecoder` tensor preprocessing (DBNet + SVTR inference)

**File:** `src/engines/ocr/PpOcrDecoder.cpp` (D2/D4 sections)  
**Called by:** `RapidOcrEngine::processImage` on every OCR invocation

### Why it is hot

`PpOcrDecoder` runs two ONNX Runtime sessions back-to-back per page:
1. **DBNet detection** — takes the full page image (rescaled to a max side of 960 px),
   runs a convolutional detection network, returns bounding polygon candidates.
2. **SVTR recognition** — for each detected text region, crops the region, normalises it
   to a fixed 48-pixel height, runs the recognition network, decodes CTC output.

The preprocessing step for each recognition crop involves:
- Bilinear resize of the cropped region to 48 × W (W varies by text length).
- Per-pixel normalisation: `pixel = (pixel / 255.0 - mean) / std`.
- Transposition from HWC to CHW layout for ONNX.

For a dense page with 200 text regions, step 2 runs 200 times. The transpose + normalise
loop is a tight float-arithmetic inner loop over (3 × 48 × W) values per region.

### Algorithmic complexity

| Step | Complexity | Notes |
|---|---|---|
| DBNet resize | O(H × W) | Full page resize — runs once per page |
| DBNet inference | O(model) | Fixed per ONNX graph; GPU-accelerated if available |
| Per-region crop | O(48 × W_i) | W_i = region width |
| SVTR normalise + transpose | O(3 × 48 × W_i) | Inner float loop |
| SVTR inference | O(model × W_i / 32) | Time-steps proportional to width |
| CTC decode | O(T × vocab_size) | T = time-steps |
| **Total per page** | O(H × W + Σ_i 3×48×W_i) | i = 0..200 regions |

For 200 regions averaging 200 px wide: ~200 × 3 × 48 × 200 = 5.76M float operations just in
the preprocessing loop — before any inference.

### What optimisation would help

1. **SIMD-accelerated normalise + transpose.** Replace the scalar `(pixel/255 - mean)/std`
   loop with `QImage`-native operations or an explicit AVX2 intrinsic loop. Qt 6 does not
   accelerate float normalisation; an explicit SIMD pass (or using `std::transform` with
   pre-computed reciprocals) cuts memory bandwidth by 4×.

2. **Batch recognition.** Rather than running one SVTR inference call per region, batch
   all regions from the same page into a single ONNX `Run()` call. ONNX Runtime supports
   dynamic batch dimensions; batching 200 regions into one call typically achieves 4-8×
   throughput improvement on GPU and ~2× on CPU due to reduced kernel-launch overhead.

3. **DBNet result caching.** If the same page is OCR'd twice (e.g., after a small UI
   update), re-use the DBNet region proposals from the first run and skip straight to SVTR.
   This requires adding a per-page detection cache keyed by (page, image_hash).

---

## Summary table

| Rank | Function | File | Dominant Cost | Best Fix |
|---|---|---|---|---|
| 1 | `RenderCache::getOrRender` | `RenderCache.cpp:126` | O(N) LRU scan on every hit | Doubly-linked O(1) LRU + hash pointer map |
| 2 | `redactCanvasRecursively` | `PoDoFoBackend.cpp:972` | O(T×R) rect-scan + 2 zlib cycles per XObject | Interval tree for rects; no-op page skip |
| 3 | `OcrPipeline::roverMerge` | `OcrPipeline.cpp:88` | O(P×S×W) text-similarity scan | Hash map indexed by normalised word text |
| 4 | `SignatureManager::buildDssDictionary` | `SignatureManager.cpp:347` | Full file read + redundant CMS parse | In-memory handoff; pass parsed cert chain |
| 5 | `PpOcrDecoder` preprocessing | `PpOcrDecoder.cpp` | Scalar float normalise 200 × 3×48×W per page | SIMD normalise + batched SVTR inference |

---

## Profiling Setup Notes

Tracy and Easy_Profiler are NOT available in MSYS2 ucrt64 as of 2026-06-02
(`pacman -Ss tracy` → no results; `pacman -Ss easy_profiler` → no results).

### Option A — Samply (available in MSYS2 ucrt64)

`samply` is a low-overhead sampling profiler available in ucrt64:

```bash
# Install
pacman -S mingw-w64-ucrt-x86_64-samply

# Profile a test run (sampling mode, no recompilation required)
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd C:/Users/User/Projects/pdf/build
samply record ./TestPerformance.exe
# Opens a Firefox Profiler view at localhost:3000
```

Samply outputs to the Firefox Profiler format — searchable flamegraphs showing wall-clock
attribution per function. No source-level instrumentation needed.

### Option B — Tracy (via vcpkg or manual build)

If Tracy instrumentation is needed (frame markers, named zones, memory tracking):

```bash
# Build Tracy server (Windows, manual — not via pacman)
git clone https://github.com/wolfpld/tracy
cd tracy/profiler && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Add to GlyphPDF CMakeLists.txt (DO NOT add until a dedicated profiling build is set up):
# option(GLYPH_TRACY "Enable Tracy instrumentation" OFF)
# if(GLYPH_TRACY)
#   add_subdirectory(third_party/tracy)
#   target_link_libraries(pdfws PRIVATE Tracy::TracyClient)
#   target_compile_definitions(pdfws PRIVATE TRACY_ENABLE)
# endif()

# Instrument hot paths:
# #include <tracy/Tracy.hpp>
# void RenderCache::getOrRender(...) {
#     ZoneScoped;   // marks the function in Tracy timeline
#     ...
# }
```

Tracy instrumentation is zero-overhead when `TRACY_ENABLE` is not defined, so it can be
kept in the source permanently behind the CMake option.

### Option C — GCC `-pg` + gprof (zero-install, lowest resolution)

Requires recompiling with `-pg`:

```bash
# In CMakeLists.txt add temporarily:
# target_compile_options(pdfws PRIVATE -pg)
# target_link_options(pdfws PRIVATE -pg)

# Run test to generate gmon.out
QT_QPA_PLATFORM=offscreen ./build/TestPerformance.exe

# Analyse
gprof ./build/TestPerformance.exe gmon.out > profile.txt
```

gprof gives flat + call-graph profiles. Useful for first-pass identification of hot
functions without installing anything beyond what GCC already provides.

---

## What D3/D4 require from a human

The following deliverables from the M7-P2 prompt spec **cannot be completed by an agent**:

### D3 — Bug bash (HUMAN-GATED)

Requires real beta testers running the application interactively. An agent cannot:
- Discover UI-layer bugs that only surface during human-driven workflows
- Evaluate whether a crash is reproducible under a user's specific document corpus
- File nuanced bug reports capturing the user's steps to reproduce

**Instructions for executing D3:**
1. Distribute the v1.0.0-beta MSI (built from HEAD) to at least 3 internal testers.
2. Provide a structured bash sheet covering: document open, text search, redaction,
   signature placement, form filling, OCR on a scanned PDF, batch export.
3. Triage reported issues by severity: release-blocker, P1, P2, cosmetic.
4. Fix release-blockers before M8 ship gate. P1 fixes are strongly recommended.
5. Commit each fix with message: `fix(<scope>): <description> (M7-P2 bug-bash)`

### D4 — 48-hour soak test (HUMAN-GATED)

Requires 48 wall-clock hours of continuous execution. An agent cannot:
- Run a test that takes 48 hours of real time within a session
- Use Heaptrack (a Linux tool; Windows equivalent is DrMemory or Application Verifier)
- Evaluate memory growth trends across hours of data

**Instructions for executing D4:**
1. Build a stress-test script (can be based on TestIntegration) that loops:
   open → search → redact-1-page → save → close, for a random 20-page PDF.
2. Run it with `--repeat-until-fail` or a time-limited loop for 48 hours.
3. Use Dr. Memory (`drmemory.exe -leaks_only -- ./stress_test.exe`) or Windows
   Task Manager's "Commit" column to track working set growth.
4. If working set grows > 10 % after 1000 cycles, attach a profiler snapshot.
5. Commit any leak fixes with: `fix(memory): <description> (M7-P2 D4 soak)`
