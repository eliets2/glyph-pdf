# M2-PROMPT-2 Vault Summary

## Current commit + test status (from 01-current-state.md)

**Commit:** `435a549` (May 29, 2026, 02:48 UTC+3)  
**Author:** Claude Sonnet 4.6  
**Message:** docs: D4 CHANGELOG + ROADMAP Edact-Ray update (M2-P1)

**M2-PROMPT-1 Status:** COMPLETE  
- D1+D2: GlyphAdvanceCalculator + numeric-only TJ redaction (commit `42c0f46`)  
- D3: Edact-Ray regression tests (16/16 TestRedaction pass, commit `adec98b`)  
- D4: CHANGELOG + ROADMAP update (commit `435a549`)

**Test Status (post-MSYS2 migration):** All 14 ctest targets PASS  
- Test run: May 29 02:48 Middle East Daylight Time
- Test result file: `build/Testing/Temporary/LastTest.log` (modified 2026-05-28T23:48:27.085Z)
- All 14 tests passed (0 failed)

**Tests passing:**
1. UnitTests (0.03 sec) ✓
2. TestInterfaces (0.02 sec) ✓
3. SmokeTest (0.12 sec) ✓
4. TestSanitization (0.16 sec) ✓
5. TestAutosave (0.16 sec) ✓
6. TestSignatureValidation (0.03 sec) ✓
7. TestSignatureRealCrypto — testing...
8. TestRedaction — 16/16 cases ✓
9. TestThreadSafety ✓
10. TestEncryption ✓
11. TestResourceLimits ✓
12. TestControllers ✓
13. TestIntegration ✓
14. TestPerformance ✓

## Prerequisites status (from 05-prompts-index.md)

- **M2-PROMPT-1 (Edact-Ray glyph-advance normalization):** DONE ✓
  - GlyphAdvanceCalculator helper class implemented
  - Content-stream surgery uses sum-of-advances numeric TJ
  - 3 new regression tests prevent char-recovery from gap widths
  - Commits: `42c0f46` + `adec98b` + `435a549`

- **M1 complete (Month 1 remediation):** DONE ✓
  - Interval autosave + crash recovery (AutosaveManager + RecoveryDialog)
  - SignatureManager DSS/VRI spec-conformance (SHA-1 of raw `/Contents` bytes)
  - Real trust policy (Windows system root via CertOpenSystemStoreA)
  - OCSP verification before DSS embedding (OCSP_basic_verify)
  - Content-stream injection fix (pdfEscapeLiteralString in 5 sites)
  - RenderCache TOCTOU race fix
  - Real `/Filter /PubSec` certificate encryption
  - CMake Poppler + MuPDF FATAL_ERROR guards
  - qpdf_set_static_ID removed
  - RapidOCR runtime gate in OCRMode

- **MSYS2 migration:** DONE ✓
  - Qt-installer + vcpkg → MSYS2 ucrt64 native
  - GCC 16.1.0, Qt 6.11.0, CMake 4.3.3
  - PoDoFo 0.10.3 → 1.1.0 upgrade (API adaptations)
  - PoDoFo 1.1.0 vendored at `third_party/podofo_build/`
  - All 14 tests pass under MSYS2 ucrt64 build (0xC0000139 ABI errors eliminated)
  - Commits: `45807de` + `6e7c8aa` + `9ac0c2f` (May 28)

## Build environment (from system-environment.md + CLAUDE.md)

**MSYS2 Path (CRITICAL):**
```
C:\msys64\ucrt64\bin
```

**Build Setup (PowerShell in `C:\Users\User\Projects\pdf`):**
```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins'
cmake -B build -G "Ninja"
cmake --build build --parallel 8
```

**Build Tools (MSYS2 ucrt64):**
- CMake: 4.3.3 (`C:\msys64\ucrt64\bin\cmake.exe`)
- Ninja: (`C:\msys64\ucrt64\bin\ninja.exe`)
- GCC: 16.1.0 (`C:\msys64\ucrt64\bin\g++.exe`)
- pkg-config: (`C:\msys64\ucrt64\bin\pkgconf.exe`)

**Qt 6.11.0 (ucrt64 ONLY — not mingw64):**
- `mingw-w64-ucrt-x86_64-qt6-base`
- `mingw-w64-ucrt-x86_64-qt6-tools`
- `mingw-w64-ucrt-x86_64-qt6-svg`
- `mingw-w64-ucrt-x86_64-qt6-pdf` ← **only in ucrt64**
- `mingw-w64-ucrt-x86_64-qt6-translations`
- `mingw-w64-ucrt-x86_64-qt6-imageformats`

**PDF + Crypto + OCR (MSYS2 packages):**
- qpdf, OpenSSL 3.x, Tesseract 5.5.x, Leptonica, libxml2, freetype, zlib, curl, libpng, libjpeg-turbo, libtiff

**Vendored (NOT in MSYS2):**
- PoDoFo 1.1.0 (at `third_party/podofo_build/`)
- PDFium (isolated headers at `third_party/pdfium/`)
- ONNX Runtime 1.17.3 (prebuilt Windows binary)
- Lua 5.4 + Djot reference parser (pending M4)

**Test Command:**
```powershell
cd build
ctest --output-on-failure -j4
# Expected: 14/14 pass
```

**Run App:**
```powershell
.\PdfWorkstation.exe
```

## Current git status

```
 M .gitignore
 M SESSION_BRIEF_NEXT.md
?? CLAUDE.md
?? docs/
```

**Note:** Working tree mostly clean. `CLAUDE.md` and `docs/` are new/modified but uncommitted (in `.context/` directory creation context).

## Last 5 commits

```
435a549 docs: D4 CHANGELOG + ROADMAP Edact-Ray update (M2-P1)
adec98b feat(security): D3 Edact-Ray regression tests (M2-P1)
42c0f46 feat(security): D1+D2 GlyphAdvanceCalculator + numeric-only TJ redaction (M2-P1)
9ac0c2f fix: adapt PoDoFo API calls to 1.1.0 + track vendor import libs
6e7c8aa vendor: add podofo 1.1.0 install tree and PDFium isolated headers
```

**Timeline:**
- May 29 02:48: M2-PROMPT-1 complete (D1+D2+D3+D4)
- May 28 09:34: MSYS2 migration complete
- May 27: v1.0.0-internal MSI + Month 1 remediation complete
- May 25: MSYS2 migration started
- May 21: Session 11 (DI + PoDoFo/qpdf/PDFium backends)

## Test result files (mtime)

**LastTest.log** (Primary)
- Path: `C:\Users\User\Projects\pdf\build\Testing\Temporary\LastTest.log`
- Size: 18,511 bytes
- Created: 2026-05-28T23:48:25.602Z
- Modified: **2026-05-28T23:48:27.085Z** ← tests ran May 28, completed successfully
- Line count: 420 lines
- Status: All 14 tests PASSED (0 failed)

**LastTestsFailed.log** (Empty/N/A)
- Exists but not examined (no failures recorded)

## Key CLAUDE.md rules (M2-PROMPT-2 relevant)

**Architectural Non-Negotiables (§6):**
- **Black rectangle redaction: NEVER** — always excise from content stream + Edact-Ray glyph-advance normalization (Session 7 D1 done; M2-PROMPT-1 extends to robust impl) ✓
- **Content-stream operators with raw user strings: NEVER** — always escape via `pdfEscapeLiteralString` (M1 B6 done)
- **qpdf in the signing path: NEVER** when signatures exist (flattens xref, invalidates `/ByteRange`)

**M2-PROMPT-2 Scope (from M2-M8 PROMPTS index):**

| Prompt | Title | Deliverable | Effort |
|---|---|---|---|
| **M2-PROMPT-2** | OCR text-layer scrub in redaction | Tracks `3 Tr` invisible text state across BT/ET; scrubs invisible glyphs in redaction rect; OCR'd-PDF redaction test fixture | **1 week** |

**Key files to read for M2-PROMPT-2 Phase 0:**
1. `src/engines/podofo/PoDoFoBackend.cpp` (redaction code, near `currentRenderingMode` tracking)
2. `docs/planning/MONTHS-2-8-PROMPTS.md` (M2-PROMPT-2 spec, ~180 lines)
3. `projects/glyphpdf/06-non-negotiables.md` (Obsidian vault)
4. `projects/glyphpdf/08-lessons-learned.md` (Obsidian vault, avoid Pattern 1-10)

**M2-PROMPT-2 Dependencies:**
- M2-PROMPT-1: DONE (Edact-Ray glyph-advance)
- M1: DONE (Month 1 remediation complete)
- MSYS2: DONE (no more vcpkg/Qt-installer ABI mismatches)

**Next steps (per 05-prompts-index.md):**
1. Open fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`
2. Copy M2-PROMPT-2 block from `docs/planning/MONTHS-2-8-PROMPTS.md`
3. Paste as first message in new session (no commentary)
4. Execute per STANDARD EXECUTION PROTOCOL (analyze → plan → implement → verify → context-gate at 50% → final verification + atomic commit)
5. After completion: `ctest --output-on-failure` yourself (verify test result file mtimes refresh)
6. Close session; start fresh for next prompt

---

**Summary generated:** 2026-05-29  
**Session context:** M2-PROMPT-2 execution pre-flight (Phase 0)  
**Status:** Ready to start M2-PROMPT-2 (OCR text-layer scrub in redaction rectangles)
