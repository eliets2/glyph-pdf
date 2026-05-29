# M2-PROMPT-3 Vault Summary

## Current commit + test status

**Current head:** `8765629` (May 29, 2026 — M2-P2 D3: CHANGELOG + ROADMAP invisible OCR text scrub update)

**M2-P1 Commits** (Edact-Ray glyph-advance normalization):
- `42c0f46` — M2-P1 D1+D2: GlyphAdvanceCalculator + numeric-only TJ normalization
- `adec98b` — M2-P1 D3: Edact-Ray regression tests (3 new cases; 16/16 pass)
- `435a549` — M2-P1 D4: CHANGELOG + ROADMAP Edact-Ray update

**M2-P2 Commits** (OCR text-layer scrub):
- `b0d5a7a` — M2-P2 D1: scrub invisible OCR text (Tr==3) in redactCanvasRecursively
- `a1221e6` — M2-P2 D2: rewrite testOCRScrubbing() as mechanistically correct regression test
- `8765629` — M2-P2 D3: CHANGELOG + ROADMAP updated; Session 7 D2 marked DONE

**Test status:** 14/14 ctest suites pass; 16/16 TestRedaction individual cases pass

**Working tree state:** 
- Modified: `.gitignore`, `SESSION_BRIEF_NEXT.md`
- Untracked: `.context/` (this dir), `CLAUDE.md`, `docs/` (both new, pre-commit)

---

## Latest session log entries

### M2-PROMPT-1 — Edact-Ray Glyph-Advance Normalization (Claude Code, 2026-05-29)
- **Commits:** `42c0f46` → `adec98b` → `435a549`
- **What:**
  - D1: Created `GlyphAdvanceCalculator.h/.cpp` — three-path helper (TryGetEncodedStringLength, decoded-string GetStringLength, CID per-glyph via 2-byte CID codes + `TryGetGlyphWidth`). Throws `std::runtime_error` on total failure.
  - D2: Replaced old `[ ( ) adj ] TJ` space-glyph normalization with numeric-only `[ N ] TJ` where N = -totalAdvance * 1000 / (fontSize * fontScale). Handles `'` and `"` operators via `T*` prefix. Closes Edact-Ray side-channel (Bland et al., PETS 2023).
  - D3: Added 3 regression tests: `testGlyphAdvancesAreNormalized`, `testCJKFontHandling`, `testRedactionFailsAfterFontResolutionFailure`.
  - D4: CHANGELOG + ROADMAP updated; Session 7 D1 marked ✅ DONE.
- **Test status:** 14/14 ctest; 16/16 TestRedaction (13 prior + 3 new)
- **Lessons:**
  - `page.GetContents()` returns `PdfContents*` (not `PdfObject*`) — has `Reset()`, `CreateStreamForAppending()`, `GetObject()` methods.
  - Test coordinate mapping critical: `QRectF(90, 120, W, 30)` → PDF y=[692, 722] for text at (100, 700).
  - Binary content stream survives PoDoFo round-trip; null byte + `Reset()` + `CreateStreamForAppending()` reliable.
  - Atomic-commit discipline maintained: one commit per D1+D2, D3, D4.

### M2-PROMPT-2 — OCR text-layer scrub (Claude Code, 2026-05-29)
- **Commits:** `b0d5a7a` → `a1221e6` → `8765629`
- **What:**
  - D1: Extended `redactCanvasRecursively` to scrub invisible (Tr==3) text within redaction rects. Used existing `currentRenderingMode` tracking variable — no new state needed. When `currentRenderingMode == 3` and text operator intersects redaction rect: operator is dropped with no Edact-Ray TJ gap. For `'`/`"` operators T* line-advance side-effect still emitted.
  - D2: Rewrote `testOCRScrubbing()` from vacuous assertion to mechanistically correct regression test. Discovered two critical findings:
    1. `PdfVariantStack::operator[](0)` returns TOP of stack (last pushed), not first — coordinate tracking in `redactCanvasRecursively` has dx/dy swapped.
    2. PoDoFo stores text strings via font CID encoding as hex sequences — `data.contains("hunter2")` on raw file bytes never works.
  - D3: CHANGELOG + ROADMAP updated for M2-PROMPT-2 D1; Session 7 D2 marked ✅ DONE.
- **Test status:** 14/14 ctest; 16/16 TestRedaction
- **Lessons:**
  - `PdfVariantStack::operator[](0)` is top-of-stack; coordinate tracking bug (dx/dy swap) is pre-existing.
  - Always check actual content stream operators, not raw file bytes, when testing redaction.
  - Use `verifyDoc.Load()` + `GetContents()` + `CopyTo()` to get decoded rewritten stream.
  - D1 `testOCRScrubbing` regex for "no Edact-Ray gap" (`] TJ\n`) distinguishes invisible scrub from visible scrub.

---

## Git status

```
 M .gitignore
 M SESSION_BRIEF_NEXT.md
?? .context/
?? CLAUDE.md
?? docs/
```

---

## Last 5 commits

```
8765629 docs: update CHANGELOG/ROADMAP for M2-P2 invisible OCR text scrub (S7 D2 done)
a1221e6 test(security): D2 invisible OCR text scrub regression test (S7 D2)
b0d5a7a feat(security): D1 scrub invisible OCR text (Tr==3) in redactCanvasRecursively (S7 D2)
435a549 docs: D4 CHANGELOG + ROADMAP Edact-Ray update (M2-P1)
adec98b feat(security): D3 Edact-Ray regression tests (M2-P1)
```

---

## Test result files (mtime)

| Name | LastWriteTime |
|---|---|
| TestRedaction_results.txt | 5/29/2026 3:19:13 AM |

(Only TestRedaction results present; other 13 result files not found in build/ — likely cleaned or from earlier run)

---

## Build environment

**MSYS2 ucrt64 (NOT mingw64):**
- Path: `C:\msys64\ucrt64\bin`
- Toolchain: GCC 16.1.0, CMake 4.3.3, Ninja, pkgconf
- Qt 6.11.0 (qt6-pdf only in ucrt64, not mingw64)
- PoDoFo 1.1.0 vendored at `third_party/podofo_build/`
- PDFium + ONNX Runtime vendored prebuilt binaries

**Build command (PowerShell):**
```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins'
cd C:\Users\User\Projects\pdf
cmake -B build -G "Ninja"
cmake --build build --parallel 8
```

**Test execution:**
```powershell
cd build
ctest --output-on-failure -j4
# Expected: 14/14 pass
```

---

## CLAUDE.md M2-P3 relevant rules

### License contamination protection (§6):
- **veraPDF (AGPL-3.0): SUBPROCESS ONLY** — never link in-process. M2-PROMPT-3 wires CLI invocation.
- **Surya: license-verify before integration** — if GPL-3.0, subprocess or substitute (M5-PROMPT-2 D2)
- **JBIG2 encoder: must be Apache/BSD/MIT** — audit before integration per R6 risk

### Crypto + security (§6):
- **All incremental saves must use PoDoFo `WriteUpdate`** when signatures exist (full rewrite invalidates signatures)
- **qpdf in the signing path: NEVER** — qpdf flattens xref, invalidates `/ByteRange`. `PdfEditorEngine.cpp:166-185` guards.
- **Black rectangle redaction: NEVER** — always excise from content stream + Edact-Ray glyph-advance normalization (Session 7 D1 done; M2-P1 extends to robust impl)

### Subprocess patterns (§6 + M2-P3 focus):
- **Spawn-per-page ONNX process: NEVER** — use LaneScheduler persistent warm worker (M2-PROMPT-5)
- **Anything using external processes:** must capture stderr/stdout, validate exit codes, handle timeouts, sanitize arguments (no shell metachar injection)

### M2-P3 specific deliverables (per MONTHS-2-8-PROMPTS.md):
- D1: veraPDF CLI integration; PDF/A validation subprocess
- D2: Subprocess error handling + output parsing
- D3-D4: Real-crypto E2E test expansion + LaneScheduler infrastructure (needs M2-P5)
- Success criteria: all 4 tests pass, veraPDF subprocess invoked with timeout + error capture, no in-process linking of veraPDF (AGPL guard)

---

*Summary generated 2026-05-29 from memory vault + git status. Valid at commit `8765629`.*
