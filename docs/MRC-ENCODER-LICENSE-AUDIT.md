# MRC Encoder License Audit (M7-P3 D1)

**Purpose:** Verify that every encoder used in the MRC compression pipeline
carries a permissive license compatible with GlyphPDF's Apache-2.0 project
license. A GPL/AGPL transitive dependency would force a relicense of the entire
project.

**Date:** 2026-06-02  
**Auditor:** Claude Sonnet 4.6 (M7-P3 automated audit)  
**Verdict at bottom of document — read Summary first.**

---

## 1. JBIG2 Encoder candidates

### 1.1 `jbig2dec` (MSYS2 ucrt64 package)

| Field | Value |
|---|---|
| MSYS2 package | `mingw-w64-ucrt-x86_64-jbig2dec` 0.20-1 |
| MSYS2 license field | `spdx:AGPL-3.0-or-later` |
| Role | **DECODER only** — decodes JBIG2 bitstreams |
| Verdict | **REJECTED** — two disqualifiers: (1) AGPL-3.0 is incompatible; (2) this is a decoder, not an encoder |

### 1.2 `jbig2enc` (agl/jbig2enc, Apache-2.0)

| Field | Value |
|---|---|
| GitHub | `https://github.com/agl/jbig2enc` |
| License file | `COPYING` — Apache License Version 2.0 (Google Inc., Adam Langley) |
| MSYS2 availability | **NOT in any MSYS2 repo** (pacman -Sl returns no results for jbig2enc) |
| Build system | `CMakeLists.txt` present — CMake 3.10+, C++17, produces `libjbig2enc` static library |
| Dependency | Leptonica ≥ 1.74 (BSD-2-Clause) — **already installed** in MSYS2 ucrt64 as `mingw-w64-ucrt-x86_64-leptonica` 1.87.0 |
| Encoding modes | Generic region (lossless), symbol extraction + text region (symbol-distinct), refinement |
| Pattern-matching mode | The symbol-extraction path clusters *visually similar* glyphs; refinement controls pixel tolerance per symbol. Contrast with "pattern-matching mode" in the Xerox 2013 incident (aggressive symbol deduplication across the document that substituted visually-similar digits). The safe usage is: **generic region (lossless)** or **symbol-distinct per page without cross-page deduplication**. |
| Patents note | `doc/PATENTS` notes JBIG2 compression processes may be patented in some countries; users directed to JBIG2 spec Annex I. This is an informational notice, not a license restriction — the COPYING file is Apache-2.0. |
| Verdict | **APPROVED** — Apache-2.0, CMake-buildable as subdirectory, Leptonica dep already present |
| Integration path | Vendor at `third_party/jbig2enc/` via `git subtree` or shallow clone; add `add_subdirectory(third_party/jbig2enc EXCLUDE_FROM_ALL)` |

### 1.3 `libjbig2dec` vs `libjbig` vs `libjbig2`

| Library | Role | License | Notes |
|---|---|---|---|
| `libjbig2dec` (Artifex) | DECODER | AGPL-3.0 | Used internally by Ghostscript; no encoder API |
| `libjbig` (Markus Kuhn) | T.82/JBIG1 encoder+decoder | BSD-like | JBIG1, not JBIG2 — **not applicable** |
| `libjbig2` | no such standalone encoder library exists under this name | — | Name sometimes confused with jbig2dec; do not use |
| Adobe JBIG2 | Proprietary | Commercial | Rejected — not open source |

---

## 2. JPEG2000 encoder

### 2.1 OpenJPEG 2.x

| Field | Value |
|---|---|
| MSYS2 package | `mingw-w64-ucrt-x86_64-openjpeg2` 2.5.4-2 — **already installed** |
| MSYS2 license field | `spdx:BSD-2-Clause` |
| pkg-config | `libopenjp2` — confirmed usable (`pkg-config --cflags --libs libopenjp2` returns valid flags) |
| Headers | `/c/msys64/ucrt64/include/openjpeg-2.5/openjpeg.h` present |
| Role | Full JPEG2000 encoder + decoder (ISO 15444-1) |
| Compression | Configurable quality via compression ratio or peak signal-to-noise ratio; default target 30:1 for MRC background |
| PDF/A-2b support | ISO 19005-2 (PDF/A-2b) explicitly allows JPEG2000 (JPX) compressed image streams — confirmed |
| Verdict | **APPROVED** — BSD-2-Clause, already in MSYS2 ucrt64, pkg-config accessible |

---

## 3. DjVu importer (D6)

| Field | Value |
|---|---|
| Library | DjVuLibre 3.5.30 |
| MSYS2 package | `mingw-w64-ucrt-x86_64-djvulibre` 3.5.30-1 — available but **not installed by default** |
| MSYS2 license field | `GPL` (GPL-2.0+) |
| Verdict | **CONDITIONAL** — GPL-2.0+ is incompatible for static/dynamic linking. Acceptable via subprocess invocation OR dynamic loading with explicit GPL separation. Since D6 uses `HAS_DJVU=OFF` default and is a build-time opt-in, the CMake guard keeps the main binary clean. When `HAS_DJVU=ON`, the user explicitly accepts the GPL-2.0+ dependency. This matches the veraPDF pattern (AGPL-3.0 subprocess is legal under AGPL §13). |
| Integration | `HAS_DJVU` CMake option (default OFF). Guard in CMakeLists.txt. Import-only, no DjVu output. |

---

## 4. Summary and verdict

| Component | Library | License | MSYS2 | Verdict |
|---|---|---|---|---|
| JBIG2 encoder | jbig2enc (agl) | **Apache-2.0** | Not in repos — vendor as subdirectory | **APPROVED** |
| JBIG2 decoder (not needed) | jbig2dec | AGPL-3.0 | ucrt64 | REJECTED (AGPL + decoder only) |
| JPEG2000 codec | OpenJPEG 2.5.4 | **BSD-2-Clause** | Already installed | **APPROVED** |
| DjVu importer (optional) | DjVuLibre | GPL-2.0+ | Available | APPROVED with HAS_DJVU=OFF guard |

**D1 VERDICT: CLEARED — proceed to D2-D9.**

Both required encoders have permissive licenses. jbig2enc (Apache-2.0) is not in MSYS2 but has a CMakeLists.txt and can be vendored as a `third_party/jbig2enc/` subdirectory using Leptonica (BSD-2-Clause, already installed). OpenJPEG2 (BSD-2-Clause) is already installed in MSYS2 ucrt64.

---

## 5. Safety constraint — JBIG2 pattern-matching mode

The 2013 Xerox WorkCentre incident involved JBIG2 **symbol deduplication across the entire document**, where visually-similar digits (6 and 8) were substituted when re-rendering. The JBIG2 standard calls this "symbol coding with cross-page dictionary sharing."

This MRC implementation uses **lossless generic region encoding only** for the foreground mask:

- `jbig2_add_page()` called per page with lossless generic region (not symbol-extraction path)
- No cross-page symbol dictionary — each page is encoded independently
- No `jbig2_finalise()` cross-page symbol table compilation
- Consistent with ISO 19005-2 (PDF/A-2b) JB2 stream requirements

This is enforced at the API call level in `MrcPageProcessor::encodeForegroundJbig2()`.

---

## 6. Integration plan

### jbig2enc vendor steps
```bash
# In project root (one-time setup):
git subtree add --prefix=third_party/jbig2enc https://github.com/agl/jbig2enc master --squash
# Or shallow clone for smaller footprint:
git clone --depth=1 https://github.com/agl/jbig2enc third_party/jbig2enc
```

### CMakeLists.txt additions
```cmake
# In root CMakeLists.txt:
option(HAS_JBIG2ENC "Enable JBIG2 encoding for MRC pipeline" ON)
if(HAS_JBIG2ENC)
    add_subdirectory(third_party/jbig2enc EXCLUDE_FROM_ALL)
    target_compile_definitions(GlyphPDF PRIVATE HAS_JBIG2ENC)
endif()

# OpenJPEG2 (already installed):
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENJPEG REQUIRED libopenjp2)
target_include_directories(GlyphPDF PRIVATE ${OPENJPEG_INCLUDE_DIRS})
target_link_libraries(GlyphPDF PRIVATE ${OPENJPEG_LIBRARIES})
```

---

*This document satisfies the D1 pre-work gate for M7-PROMPT-3. No blocking license issues found.*
