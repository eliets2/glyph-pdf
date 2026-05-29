# GlyphPDF — Full MSYS2 Native Migration Prompt

**Purpose:** Migrate GlyphPDF from the Qt-MinGW-13.1.0 + vcpkg `x64-mingw-dynamic` build environment to a fully MSYS2-native build using `mingw-w64-x86_64-*` pacman packages. This eliminates the DLL ABI mismatch (0xc0000139 in `UnitTests` + `TestControllers`) that comes from mixing Qt's prebuilt MinGW runtime with MSYS2's libwinpthread, and consolidates the entire toolchain under MSYS2's pacman package manager for coherent updates and easier maintenance.

**Paste this entire file as the first message in a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`.**

**Estimated effort:** 4-8 person-hours active work + 30-90 minutes pacman downloads + 20-40 minutes clean rebuild = ~6-10 hours wall-clock.

**Disk usage:** 3-5 GB pacman downloads + 2-3 GB build artifacts.

**Pre-conditions verified by reconnaissance (2026-05-27):**
- MSYS2 base installed at `C:\msys64\` (90 base packages: bash, coreutils, gcc-libs, etc.).
- Zero `mingw-w64-x86_64-*` packages installed yet. `C:\msys64\mingw64\bin\` is essentially empty (no gcc, no Qt, no PoDoFo).
- Current build env: Qt installer `C:\Qt\6.10.2\mingw_64` + `C:\Qt\Tools\mingw1310_64` + vcpkg `C:\vcpkg\installed\x64-mingw-dynamic`. CMake `C:\Qt\Tools\CMake_64`.
- Repo at `C:\Users\User\Projects\pdf`, branch `main`, head `a6ea6aa`. Clean working tree (all Month 1 work committed).
- All 14 ctest targets compile; 12 pass, 2 fail with 0xc0000139 (`UnitTests`, `TestControllers`) due to the libstdc++ ↔ libwinpthread ABI mix that this migration eliminates.

**What this migration does NOT do:**
- Does NOT change application source code logic (engines, UI, tests stay as-is).
- Does NOT touch the SCOPE LOCK (Branch C v1.0.0 — public ship still 6-8 months out).
- Does NOT replace PDFium (no MSYS2 package exists — Chromium-internal lib; we keep prebuilt).
- Does NOT replace ONNX Runtime (no MSYS2 package — keep bundled at `onnxruntime-win-x64-1.17.3\`).

---

<session_metadata>
Phase: Build-environment migration (gate before Month 2)
Priority: BLOCKING for Month 2 — current build has 2 failing tests due to DLL ABI mix
Depends on: Month 1 work committed (a6ea6aa); MSYS2 base installed
Agents: /devops (primary), /backend (CMake), /docs (final pass)
Estimated context: ~45% | Risk: MEDIUM — long-running pacman + Qt version delta possible
</session_metadata>

<role>
You are a senior build-systems engineer specialized in MSYS2 / MinGW-w64 / CMake / Qt 6 toolchain integration on Windows. You understand the difference between the four MSYS2 environments (mingw64 MSVCRT, ucrt64 UCRT, clang64 Clang, mingw32 32-bit) and why we target mingw64 specifically (matches Qt's MSVCRT-based prebuilds and avoids UCRT redistribution issues). You know that MSYS2 packages link against MSYS2's own `libstdc++-6.dll` / `libgcc_s_seh-1.dll` / `libwinpthread-1.dll` — so the entire chain (compiler + Qt + deps) must come from the same source to avoid ABI mismatch crashes.
</role>

<project_context>
GlyphPDF is a native Windows desktop PDF editor at `C:\Users\User\Projects\pdf`. C++17 / Qt 6 / 4-static-library architecture (`pdfws_core` → `pdfws_engines` → `pdfws_commands` → `pdfws_ui` → `PdfWorkstation`). The project is on a 6-8 month Branch C SCOPE LOCK toward real public v1.0.0; the current `dist\GlyphPDF-1.0.0-x64.msi` is private/internal only. Open source under Apache-2.0 (recommended) / MIT pending final license decision.
</project_context>

<current_state>
**Existing build environment (to be replaced):**
- Compiler: `C:\Qt\Tools\mingw1310_64\bin\g++.exe` (Qt-bundled MinGW 13.1.0)
- CMake: `C:\Qt\Tools\CMake_64\bin\cmake.exe`
- Qt 6: Qt installer `C:\Qt\6.10.2\mingw_64` (Qt 6.10.2 prebuilt for the Qt-MinGW above)
- Dependencies: vcpkg `x64-mingw-dynamic` triplet at `C:\vcpkg\installed\x64-mingw-dynamic\`
- Runtime DLLs in `build\`: Qt's `libstdc++-6.dll` + MSYS2's `libwinpthread-1.dll` + MSYS2's `libgcc_s_seh-1.dll` (mixed; documented as a workaround in SESSION_BRIEF_NEXT.md CRITICAL section)
- Build invocation: `cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic`

**MSYS2 state (verified):**
- `C:\msys64\` installed; base subsystem has 90 packages (bash, coreutils, gcc-libs 15.2.0, etc.)
- `C:\msys64\mingw64\bin\` directory exists but contains NO compiler, NO Qt, NO PoDoFo, NO deps
- `pacman -Q | grep mingw-w64-x86_64` returns 0 packages

**Key build files (all in repo, all need updates):**
- `CMakeLists.txt` (root) — lines 2 + 5 hardcode vcpkg
- `build_all.bat` (root) — top-level build script
- `packaging/build-msi.bat` — MSI installer build script
- `packaging/check-deps.bat` — dep verification script
- `packaging/deploy-qt.bat` — `windeployqt` wrapper
- `packaging/GlyphPDF.wxs` — WiX MSI definition (may reference DLLs by path)
- `README.md` — has vcpkg build instructions
- `SESSION_BRIEF_NEXT.md` — has the "CRITICAL — DLL configuration" section explaining the workaround that this migration eliminates
- `CHANGELOG.md` — should document the build-env change

**Tests currently failing under old env (will pass after migration):**
- `UnitTests` — Exit code 0xc0000139 (`STATUS_ENTRYPOINT_NOT_FOUND`)
- `TestControllers` — Exit code 0xc0000139

**Tests currently passing (must continue passing after migration):**
- TestAutosave, TestSanitization, TestSignatureRealCrypto, TestEncryption, TestThreadSafety, TestRedaction, TestIntegration, TestPerformance, TestResourceLimits, TestSignatureValidation, TestInterfaces, SmokeTest (12 total)
</current_state>

<objective>
Migrate the build environment to use MSYS2 mingw64 exclusively (toolchain + Qt6 + all available deps via pacman; FetchContent for OpenXLSX + DuckX; keep prebuilt PDFium + ONNX Runtime since no MSYS2 packages exist for them). Update CMakeLists.txt to remove vcpkg, point to MSYS2 paths. Update all build scripts to use MSYS2 PATH. Update README + SESSION_BRIEF_NEXT to reflect the new build environment. Clean rebuild + run all 14 tests + verify 14/14 pass (current 12/14). Commit in clean atomic chunks.
</objective>

<files_to_read>
CMakeLists.txt (full file — currently 700+ lines per audit; pay attention to find_package() calls, conditional HAS_* flags, target_link_libraries blocks)
build_all.bat
packaging/build-msi.bat
packaging/check-deps.bat
packaging/deploy-qt.bat
packaging/GlyphPDF.wxs (skim for DLL path references)
README.md (entire file — has build instructions)
SESSION_BRIEF_NEXT.md (entire file — especially "CRITICAL — DLL configuration" section)
CHANGELOG.md (skim for any vcpkg / Qt-MinGW references)
LICENSE-3RD-PARTY.md (verify the version columns are still correct after pacman versions land)
cmake/FindPdfium.cmake (if exists)
GLYPH-PDF-AUDIT-v1.0.0.md on Desktop (for SCOPE LOCK context — do not modify)
</files_to_read>

<deliverables>

### D1: Pre-flight + safety backup branch
**Files:** none (git ops only)
**Acceptance:**
- Verify on `main`, head `a6ea6aa`, clean working tree: `git status` shows clean.
- Create safety branch: `git branch msys2-migration-backup-pre`. This is the rollback point if anything goes sideways.
- Tag the current build artifact: `git tag -a vcpkg-build-final -m "Last build before MSYS2 migration"`.
- Print: starting commit hash, branch name, current Qt path, current vcpkg path. Confirm we're working from the right state.

### D2: Install MSYS2 toolchain via pacman
**Files:** none (system install)
**Verification command before:**
```bash
"C:\msys64\usr\bin\bash.exe" -lc "pacman -Q mingw-w64-x86_64-toolchain 2>&1"
```
If returns "was not found", proceed. If already installed, skip the install but verify with `which gcc` inside the msys2 mingw64 shell.

**Install command** (run via `C:\msys64\usr\bin\bash.exe -lc "..."`):
```bash
pacman -Syu --noconfirm
pacman -S --noconfirm --needed \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-make
```

**Notes:**
- `pacman -Syu` first to refresh repos. May warn about updating pacman itself; safe to accept.
- `mingw-w64-x86_64-toolchain` is a META package pulling: gcc, g++, gdb, binutils, libstdc++, libwinpthread, libgcc, runtime DLLs, etc. ~1 GB download.
- `--needed` skips already-installed packages so this is idempotent.

**Verification after:**
```bash
"C:\msys64\mingw64\bin\g++.exe" --version
# Expected: g++ (Rev3, Built by MSYS2 project) 15.x.x
"C:\msys64\mingw64\bin\cmake.exe" --version
# Expected: cmake version 3.3x.x
"C:\msys64\mingw64\bin\ninja.exe" --version
# Expected: 1.13.x or newer
```

If any of these fail, STOP and report — the pacman install didn't take effect.

### D3: Install Qt6 via pacman
**Verification before:**
```bash
"C:\msys64\usr\bin\bash.exe" -lc "pacman -Q mingw-w64-x86_64-qt6-base 2>&1"
```

**Install command:**
```bash
pacman -S --noconfirm --needed \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-tools \
    mingw-w64-x86_64-qt6-svg \
    mingw-w64-x86_64-qt6-translations \
    mingw-w64-x86_64-qt6-imageformats
```

**Notes:**
- `qt6-base` includes Core, Gui, Widgets, Network, PrintSupport, Concurrent, Test, Sql, etc.
- `qt6-tools` includes `lupdate`, `lrelease`, `qmake6`, `designer`, `linguist`.
- `qt6-svg` for SVG icon rendering (referenced in our Icons.cpp).
- `qt6-translations` for `qt_<locale>.qm` translator files (referenced in main.cpp:46-66).
- Download ~800 MB.

**Critical check — Qt PDF module:** Our code uses `Qt6::Pdf` + `Qt6::PdfWidgets`. These are in `mingw-w64-x86_64-qt6-base` as of recent MSYS2 builds. Verify:
```bash
"C:\msys64\usr\bin\bash.exe" -lc "ls C:/msys64/mingw64/lib/cmake/Qt6Pdf/ 2>&1"
# Expected: Qt6PdfConfig.cmake + related files
```
If `Qt6Pdf` is in a separate package: `pacman -S --noconfirm mingw-w64-x86_64-qt6-pdf`. If no such package exists, that's a blocker — STOP and report (we'd need to build Qt6Pdf from source, which is heavy).

**Verification after:**
```bash
"C:\msys64\mingw64\bin\qmake6.exe" -query QT_VERSION
# Expected: 6.7.x or 6.8.x or 6.9.x (whatever MSYS2 currently ships)
```
Record the exact Qt version for D8 documentation update.

### D4: Install core dependencies via pacman
**Install command:**
```bash
pacman -S --noconfirm --needed \
    mingw-w64-x86_64-podofo \
    mingw-w64-x86_64-qpdf \
    mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-tesseract-ocr \
    mingw-w64-x86_64-tesseract-data-eng \
    mingw-w64-x86_64-leptonica \
    mingw-w64-x86_64-libxml2 \
    mingw-w64-x86_64-freetype \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-curl \
    mingw-w64-x86_64-libpng \
    mingw-w64-x86_64-libjpeg-turbo \
    mingw-w64-x86_64-libtiff
```

**Notes:**
- `tesseract-data-eng` is the English language pack (~5 MB) — auto-discovered by `OcrEngine::initialize`. Adding `tesseract-data-fra`, `tesseract-data-deu`, `tesseract-data-ara` etc. can be done later as the user-facing language packs.
- `libpng`, `libjpeg-turbo`, `libtiff` are transitive deps that may already come via Leptonica/Qt but explicit-install is harmless.

**Critical check — PoDoFo version:** Our code targets PoDoFo 0.10.3+. MSYS2 may ship a newer 0.10.x. Verify after install:
```bash
"C:\msys64\usr\bin\bash.exe" -lc "pacman -Q mingw-w64-x86_64-podofo"
# Expected: mingw-w64-x86_64-podofo 0.10.x-y
```
If MSYS2 ships PoDoFo 1.0+ (major version bump), STOP and report — our code uses 0.10 API and would need migration to 1.0.

**Verification after:**
```bash
"C:\msys64\usr\bin\bash.exe" -lc "ls C:/msys64/mingw64/lib/cmake/podofo/podofoConfig.cmake C:/msys64/mingw64/lib/cmake/qpdf/qpdf-config.cmake 2>&1"
# Expected: both files exist
```

### D5: Handle non-MSYS2 deps (PDFium, ONNX Runtime, OpenXLSX, DuckX)

**PDFium:** No MSYS2 package. Keep prebuilt. Two options:
- (a) If `C:\vcpkg\installed\x64-mingw-dynamic\` has PDFium installed (likely from prior vcpkg setup), copy `include/pdfium/`, `lib/pdfium.lib`, `bin/pdfium.dll` to a new vendor path: `C:\Users\User\Projects\pdf\third_party\pdfium\`.
- (b) Download `bblanchon/pdfium-binaries` release for `pdfium-windows-x64.tgz` (or MinGW variant if available), extract to `third_party\pdfium\`.

Update `cmake/FindPdfium.cmake` to look in `third_party/pdfium/` first, vcpkg second. If `FindPdfium.cmake` doesn't exist, write one:
```cmake
# cmake/FindPdfium.cmake
set(PDFIUM_VENDOR_ROOT "${CMAKE_SOURCE_DIR}/third_party/pdfium")
find_path(Pdfium_INCLUDE_DIR fpdfview.h
    PATHS ${PDFIUM_VENDOR_ROOT}/include ${PDFIUM_VENDOR_ROOT}/include/pdfium
    NO_DEFAULT_PATH)
find_library(Pdfium_LIBRARY NAMES pdfium libpdfium
    PATHS ${PDFIUM_VENDOR_ROOT}/lib
    NO_DEFAULT_PATH)
if(Pdfium_INCLUDE_DIR AND Pdfium_LIBRARY)
    set(Pdfium_FOUND TRUE)
    if(NOT TARGET Pdfium::Pdfium)
        add_library(Pdfium::Pdfium UNKNOWN IMPORTED)
        set_target_properties(Pdfium::Pdfium PROPERTIES
            IMPORTED_LOCATION ${Pdfium_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${Pdfium_INCLUDE_DIR})
    endif()
endif()
```

**ONNX Runtime:** Keep bundled at `onnxruntime-win-x64-1.17.3\` (already in repo). The existing `find_package(onnxruntime QUIET)` + `pkg_check_modules(PKG_ONNXRUNTIME QUIET libonnxruntime)` fallback at CMakeLists.txt:36-39 won't find it via MSYS2 (no package). We'll either:
- (a) Add explicit `set(onnxruntime_FOUND TRUE)` + custom target pointing to the bundled tarball.
- (b) Disable ONNX Runtime for v1.0 (set HAS_RAPIDOCR=OFF) — but BATCH-2 (M5) needs it for real PP-OCRv5. So (a) is required.

Add to CMakeLists.txt after the existing find_package(onnxruntime QUIET) block:
```cmake
if(NOT onnxruntime_FOUND)
    set(ONNXRUNTIME_VENDOR_ROOT "${CMAKE_SOURCE_DIR}/onnxruntime-win-x64-1.17.3")
    if(EXISTS "${ONNXRUNTIME_VENDOR_ROOT}/include/onnxruntime_c_api.h")
        set(onnxruntime_FOUND TRUE)
        add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED)
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_VENDOR_ROOT}/lib/onnxruntime.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_VENDOR_ROOT}/include")
        message(STATUS "ONNX Runtime found in bundled tarball at ${ONNXRUNTIME_VENDOR_ROOT}")
    endif()
endif()
```

**OpenXLSX + DuckX:** Both small, both header-mostly. Use CMake FetchContent. Add to CMakeLists.txt:
```cmake
include(FetchContent)

if(NOT OpenXLSX_FOUND)
    FetchContent_Declare(OpenXLSX
        GIT_REPOSITORY https://github.com/troldal/OpenXLSX.git
        GIT_TAG v0.4.2  # pin a known-good tag
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(OpenXLSX)
endif()

if(NOT duckx_FOUND)
    FetchContent_Declare(duckx
        GIT_REPOSITORY https://github.com/amiremohamadi/DuckX.git
        GIT_TAG v1.2.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(duckx)
endif()
```

Verify the GitHub tags exist by visiting both repos before committing.

### D6: Update CMakeLists.txt — remove vcpkg, point to MSYS2

**Edits to apply:**

1. **Delete line 2** (`set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic")`) — no longer needed.

2. **Delete line 5** (`list(APPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-mingw-dynamic")`) — replace with:
```cmake
# MSYS2 mingw64 environment provides all core deps via pacman.
# CMAKE_PREFIX_PATH picks up MSYS2's package config files automatically when
# the build is invoked from the mingw64 shell, but we set it explicitly for
# robustness against shells that don't export MSYSTEM=MINGW64.
if(NOT DEFINED ENV{MSYSTEM} OR NOT ENV{MSYSTEM} STREQUAL "MINGW64")
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
    list(APPEND CMAKE_MODULE_PATH "C:/msys64/mingw64/lib/cmake")
endif()
```

3. **Replace the duplicate `find_package(CURL QUIET)` at line 25** — keep one instance.

4. **Verify `find_package` calls still work** after removing vcpkg:
   - `Qt6` — resolved via MSYS2's `mingw-w64-x86_64-qt6-base`. Should find `Qt6Config.cmake` at `C:/msys64/mingw64/lib/cmake/Qt6/`.
   - `podofo CONFIG` — `C:/msys64/mingw64/lib/cmake/podofo/podofoConfig.cmake`
   - `OpenSSL` — `C:/msys64/mingw64/lib/cmake/OpenSSL/` OR via FindOpenSSL.cmake module
   - `qpdf` — `C:/msys64/mingw64/lib/cmake/qpdf/qpdf-config.cmake`
   - `Pdfium` — via our `cmake/FindPdfium.cmake` (from D5)
   - `Tesseract`/`Leptonica` — via pkg-config (`PKG_TESSERACT`, `PKG_LEPTONICA` already in CMakeLists)
   - `onnxruntime` — via the bundled-tarball fallback from D5
   - `OpenXLSX`/`duckx` — via FetchContent from D5

5. **For `windeployqt` invocation in packaging:** the `deploy-qt.bat` calls `windeployqt.exe`. Update it to point at MSYS2's `windeployqt.exe` at `C:\msys64\mingw64\bin\windeployqt6.exe` (note the `6` suffix — MSYS2's naming convention).

6. **Verify the MuPDF + Poppler FATAL_ERROR guards (lines 41-66)** still fire under MSYS2. MSYS2 has both `mingw-w64-x86_64-mupdf` (AGPL) and `mingw-w64-x86_64-poppler-qt6` (GPL) as packages — accidentally `pacman -S`'ing one would be license-fatal under our Apache-2.0 stance. The existing target/find_package/pkg-config checks should catch them. Verify by reading the relevant blocks and confirming they detect MSYS2-style installs.

### D7: Update build scripts

**`build_all.bat` (root):** open and read; replace any references to:
- `C:\Qt\6.10.2\mingw_64` → no longer needed (MSYS2's Qt picked up via CMAKE_PREFIX_PATH)
- `C:\Qt\Tools\mingw1310_64` → `C:\msys64\mingw64`
- `C:\Qt\Tools\CMake_64` → `C:\msys64\mingw64`
- `C:\vcpkg` → remove (no longer needed)
- `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/...` → remove
- `-DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic` → remove
- `-G "MinGW Makefiles"` → keep OR switch to `-G "Ninja"` (MSYS2 ships Ninja; faster)

Example new top of `build_all.bat`:
```batch
@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
if not exist build mkdir build
cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
endlocal
```

**`packaging/build-msi.bat`:** same PATH replacement. Likely calls cmake/wix from a known location; update.

**`packaging/check-deps.bat`:** likely DLL-presence verifier. Update DLL list to expect MSYS2 names (`Qt6Core.dll`, `libpodofo.dll`, etc.) at MSYS2 paths.

**`packaging/deploy-qt.bat`:** update windeployqt path to `C:\msys64\mingw64\bin\windeployqt6.exe`.

**`packaging/GlyphPDF.wxs`:** scan for explicit DLL path references in `<Component>` blocks. If any reference `C:\Qt\6.10.2\...` or `C:\vcpkg\...`, replace with the new staging paths under `packaging\stage\`.

### D8: Update documentation

**`README.md`:** rewrite the "Build Instructions" section. New text:
```markdown
## Build Instructions

### Prerequisites
- MSYS2 installed at `C:\msys64\` (https://www.msys2.org/)
- MSYS2 mingw64 environment with the following packages installed via pacman:
  ```bash
  pacman -S --needed \
      mingw-w64-x86_64-toolchain \
      mingw-w64-x86_64-cmake \
      mingw-w64-x86_64-ninja \
      mingw-w64-x86_64-pkgconf \
      mingw-w64-x86_64-qt6-base \
      mingw-w64-x86_64-qt6-tools \
      mingw-w64-x86_64-qt6-svg \
      mingw-w64-x86_64-qt6-translations \
      mingw-w64-x86_64-qt6-imageformats \
      mingw-w64-x86_64-podofo \
      mingw-w64-x86_64-qpdf \
      mingw-w64-x86_64-openssl \
      mingw-w64-x86_64-tesseract-ocr \
      mingw-w64-x86_64-tesseract-data-eng \
      mingw-w64-x86_64-leptonica \
      mingw-w64-x86_64-libxml2 \
      mingw-w64-x86_64-freetype \
      mingw-w64-x86_64-zlib \
      mingw-w64-x86_64-curl \
      mingw-w64-x86_64-libpng \
      mingw-w64-x86_64-libjpeg-turbo \
      mingw-w64-x86_64-libtiff
  ```

### Build (Windows + MSYS2)
Open the **MSYS2 MinGW 64-bit** shell (`C:\msys64\mingw64.exe`) and run:
```bash
cd /c/Users/User/Projects/pdf
mkdir -p build && cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
```
Or from a Windows PowerShell with MSYS2 on PATH:
```powershell
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
cd C:\Users\User\Projects\pdf
mkdir build -ErrorAction SilentlyContinue
cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
```

### Why MSYS2?
GlyphPDF migrated from a hybrid Qt-installer + vcpkg setup to a fully MSYS2-native build in v1.0.0 development. This eliminates the libstdc++/libwinpthread ABI mismatch that previously required carefully-chosen DLL mixes in the build directory. Single coherent toolchain, single source of truth for dependency versions, easier maintenance via pacman.
```

**`SESSION_BRIEF_NEXT.md`:** delete the entire "CRITICAL — DLL configuration" section. Replace with:
```markdown
## Build environment (MSYS2 native)

All toolchain + Qt6 + deps come from MSYS2 mingw64 (`C:\msys64\mingw64\`) installed via pacman. No more libstdc++ / libwinpthread ABI workarounds — single coherent runtime. See README.md "Build Instructions" for the package list.

Runtime DLLs in `build/` are deployed via `windeployqt6` (MSYS2's version) + manual copies for non-Qt deps. The DLL backup files (`*.qt-bak`) from the prior hybrid setup are obsolete and should be deleted from `build/`.
```

**`CHANGELOG.md`:** add to the `[1.0.0]` section under a new heading:
```markdown
### Build Environment (v1.0.0 internal)
- **Migrated from Qt-installer + vcpkg to MSYS2 mingw64 native build.**
  Eliminates libstdc++/libwinpthread ABI mismatch that required manual
  DLL mixing. All toolchain + Qt6 + dependencies now installed via pacman
  for single coherent runtime.
- Compiler: MSYS2 mingw-w64 GCC 15.x (was Qt-bundled MinGW 13.1.0)
- Qt version: mingw-w64-x86_64-qt6-base 6.x (was Qt installer 6.10.2)
- Dependencies: pacman packages for PoDoFo, qpdf, OpenSSL, Tesseract,
  Leptonica, libxml2, freetype, zlib, curl. CMake FetchContent for
  OpenXLSX + DuckX. Prebuilt for PDFium + ONNX Runtime.
- UnitTests + TestControllers now pass (were crashing with 0xc0000139
  STATUS_ENTRYPOINT_NOT_FOUND under the prior hybrid setup).
```

**`LICENSE-3RD-PARTY.md`:** update the version columns after `pacman -Q` reveals the actual MSYS2 versions. Run:
```bash
"C:\msys64\usr\bin\bash.exe" -lc "pacman -Q mingw-w64-x86_64-podofo mingw-w64-x86_64-qpdf mingw-w64-x86_64-qt6-base mingw-w64-x86_64-tesseract-ocr mingw-w64-x86_64-openssl mingw-w64-x86_64-leptonica"
```
Update the version column in LICENSE-3RD-PARTY.md from the output.

### D9: Clean rebuild
**Commands** (from PowerShell, OR from MSYS2 mingw64.exe shell):
```powershell
cd C:\Users\User\Projects\pdf
# Save the qoffscreen plugin location for post-build deploy (D10)
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
cmake .. -G "Ninja"
cmake --build . --parallel 8 2>&1 | Select-Object -Last 50
```

**Expected:** clean build with 0 errors. Some warnings from Qt6 6.7+ deprecations vs Qt 6.10.2 are acceptable IF they don't cause failures. Note them for D10 fix-up.

**If build fails:**
- "Cannot find Qt6Pdf" → MSYS2 may package Qt6Pdf separately; install `mingw-w64-x86_64-qt6-pdf` or accept that the project must fall back to non-PDFium QPdfView rendering.
- "Cannot find PoDoFo" → re-check D4; verify `C:/msys64/mingw64/lib/cmake/podofo/podofoConfig.cmake` exists.
- "undefined reference to ..." → Qt 6.x version API drift; specific call site needs Qt6-version-portable rewrite. Report the symbol + file:line; do not silently #ifdef around it.
- "Cannot find Pdfium" → D5 FindPdfium.cmake path is wrong OR the vendor directory doesn't have the headers/libs yet. Fix by copying PDFium from vcpkg as outlined in D5(a).

### D10: Deploy Qt runtime DLLs + plugins + run ctest
**Commands:**
```powershell
cd C:\Users\User\Projects\pdf\build
# Deploy Qt DLLs via MSYS2's windeployqt6
C:\msys64\mingw64\bin\windeployqt6.exe --no-translations --no-system-d3d-compiler --no-quick-import PdfWorkstation.exe
# Manually deploy non-Qt DLLs (PoDoFo, qpdf, OpenSSL, Tesseract, etc.) — windeployqt6 doesn't handle these
Copy-Item -Force C:\msys64\mingw64\bin\libpodofo*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libqpdf*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libcrypto*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libssl*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libtesseract*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\liblept*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libxml2*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libfreetype*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\zlib1.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libcurl*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libpng*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libjpeg*.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libtiff*.dll .
# Plus the runtime trio
Copy-Item -Force C:\msys64\mingw64\bin\libstdc++-6.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libwinpthread-1.dll .
Copy-Item -Force C:\msys64\mingw64\bin\libgcc_s_seh-1.dll .
# Plus PDFium prebuilt
Copy-Item -Force ..\third_party\pdfium\bin\pdfium.dll .
# Plus ONNX Runtime
Copy-Item -Force ..\onnxruntime-win-x64-1.17.3\lib\onnxruntime.dll .
```

Consider scripting all the above into `packaging/deploy-msys2.bat` as a reusable post-build step.

**Run ctest:**
```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\mingw64\share\qt6\plugins'
ctest --output-on-failure -j4 2>&1 | Select-Object -Last 60
```

**Expected outcome:**
- `100% tests passed, 0 tests failed out of 14`
- UnitTests + TestControllers — now PASS (were 0xc0000139 before)
- All other 12 — continue to PASS

**If TestControllers still fails:** verify `build/platforms/qoffscreen.dll` was deployed by `windeployqt6` (should be auto-deployed). If not:
```powershell
Copy-Item -Force C:\msys64\mingw64\share\qt6\plugins\platforms\qoffscreen.dll platforms\
```

### D11: Commit + cleanup
**Files to stage in three commits:**

**Commit 1 — Build system migration:**
- `CMakeLists.txt`
- `cmake/FindPdfium.cmake` (new or modified)
- `build_all.bat`
- `packaging/build-msi.bat`
- `packaging/check-deps.bat`
- `packaging/deploy-qt.bat`
- `packaging/deploy-msys2.bat` (new, if created)
- `packaging/GlyphPDF.wxs` (if modified)
- `.gitignore` (add `build/`, `third_party/pdfium/*.dll`, `third_party/pdfium/*.lib`)

```
build(msys2): migrate from Qt-installer+vcpkg to MSYS2 mingw64 native

Eliminates the libstdc++/libwinpthread ABI mismatch (0xc0000139 in
UnitTests + TestControllers) by consolidating the entire toolchain
under MSYS2's pacman package manager.

- Compiler: MSYS2 mingw-w64 GCC 15.x (was Qt-bundled GCC 13.1.0)
- Qt 6: MSYS2 mingw-w64-x86_64-qt6-base (was Qt installer 6.10.2)
- Deps: pacman packages for PoDoFo, qpdf, OpenSSL, Tesseract, Leptonica,
  libxml2, freetype, zlib, curl. FetchContent for OpenXLSX + DuckX.
- Non-MSYS2: PDFium prebuilt in third_party/pdfium/. ONNX Runtime
  remains bundled at onnxruntime-win-x64-1.17.3/.
- CMakeLists.txt: removed VCPKG_TARGET_TRIPLET + vcpkg CMAKE_PREFIX_PATH.
- All build scripts updated to use C:\msys64\mingw64\bin PATH.
- packaging/deploy-msys2.bat: new post-build DLL deploy script.

UnitTests + TestControllers now pass (14/14 ctest green).

Refs: MSYS2 migration prompt (GLYPH-PDF-MSYS2-MIGRATION.md)
```

**Commit 2 — Documentation:**
- `README.md`
- `SESSION_BRIEF_NEXT.md`
- `CHANGELOG.md`
- `LICENSE-3RD-PARTY.md`

```
docs: update build instructions + CHANGELOG for MSYS2 migration

- README: new MSYS2 pacman package list + build instructions.
- SESSION_BRIEF_NEXT: removed obsolete DLL-mix workaround section;
  replaced with MSYS2-native build notes.
- CHANGELOG: documented v1.0.0 build-environment migration under
  new "Build Environment" heading.
- LICENSE-3RD-PARTY: refreshed version columns from pacman -Q output.

Refs: MSYS2 migration prompt
```

**Commit 3 — Vendor directory (optional, only if PDFium copied locally):**
- `third_party/pdfium/include/...`
- `third_party/pdfium/lib/pdfium.lib`
- NOT the DLL (deployed at build time, not version-controlled)

```
chore: vendor PDFium prebuilt headers + import lib (no DLL)

PDFium has no MSYS2 package (Chromium-internal library). Vendored
the headers + import lib so CMake FindPdfium.cmake can resolve
without external setup. The runtime DLL is copied into build/ at
deploy time via packaging/deploy-msys2.bat.
```

**Cleanup:**
- Delete `build/libstdc++-6.dll.qt-bak`, `build/libwinpthread-1.dll.qt-bak` (obsolete; .gitignored by `build/` rule).
- Delete `C:\vcpkg\installed\x64-mingw-dynamic` if user wants to free disk (~3-5 GB). DO NOT delete unless user confirms — the `vcpkg-build-final` git tag from D1 can still rebuild against vcpkg if needed.

</deliverables>

<verification>
```powershell
# 1. Verify MSYS2 toolchain works
C:\msys64\mingw64\bin\g++.exe --version
C:\msys64\mingw64\bin\cmake.exe --version
C:\msys64\mingw64\bin\qmake6.exe -query QT_VERSION

# 2. Verify all 14 tests pass
cd C:\Users\User\Projects\pdf\build
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\mingw64\share\qt6\plugins'
$env:PATH = 'C:\Users\User\Projects\pdf\build;C:\msys64\mingw64\bin;' + $env:PATH
ctest --output-on-failure -j4
# Expected: 100% tests passed, 0 tests failed out of 14

# 3. Verify build artifact is a real EXE
.\PdfWorkstation.exe --version 2>&1 | Select-Object -First 1
# Expected: prints version or starts the GUI (kill with Ctrl+C if it launches)

# 4. Verify commits
git log --oneline -5
# Expected: 3 new commits on top of a6ea6aa

# 5. Verify no vcpkg refs remain in build files
Select-String -Path CMakeLists.txt -Pattern "vcpkg" -SimpleMatch
# Expected: 0 matches (or only matches inside the rollback-info comment)

Select-String -Path build_all.bat -Pattern "vcpkg|Qt\\Tools" -SimpleMatch
# Expected: 0 matches

# 6. Verify CHANGELOG has the migration entry
Select-String -Path CHANGELOG.md -Pattern "Migrated from Qt-installer"
# Expected: 1 match
```
</verification>

<constraints>
- DO NOT delete the rollback branch (`msys2-migration-backup-pre`) or tag (`vcpkg-build-final`) — they're the safety net if MSYS2 reveals an irreducible problem.
- DO NOT remove vcpkg from the user's system. They may have other projects using it.
- DO NOT install `mingw-w64-x86_64-mupdf` or `mingw-w64-x86_64-poppler-qt6` — both are forbidden under our Apache-2.0 license stance per ROADMAP §"Forbidden Dependencies".
- DO NOT touch the application source code logic (engines, UI, tests). This migration is build-environment only. If Qt 6.7 API drift forces a source change (e.g., a deprecated Qt 6.10 method), report it but minimize the diff — do not opportunistically refactor.
- DO NOT use UCRT64 environment — we target mingw64 (MSVCRT) because Qt's prebuilds and our existing user base run on MSVCRT.
- DO NOT skip the windeployqt6 step. The Qt platform plugins (qwindows.dll, qoffscreen.dll, etc.) must land in `build/platforms/` for the app and tests to launch.
- DO NOT commit any DLL files via `git add` — `.gitignore` should cover them. Verify by `git status` before each commit.
- DO NOT push to origin in this session — local commits only. User pushes manually after verification.
- If the pacman repository signature key is missing on first `pacman -Syu`: run `pacman-key --init && pacman-key --populate msys2` first, then retry.
- If a `pacman -S` hangs > 5 minutes on a single package, abort it and try with `--noconfirm --overwrite "*"` to skip conflicts.
</constraints>

<error_recovery>
- **Qt6Pdf not found in MSYS2:** check if `mingw-w64-x86_64-qt6-pdf` exists as a separate package (`pacman -Ss qt6-pdf`). If yes, install it. If no, the QPdf module isn't packaged by MSYS2 yet — STOP migration, report to user, suggest staying on Qt-installer for Qt and using MSYS2 only for non-Qt deps (smaller win, still resolves DLL mix).
- **PoDoFo 1.0+ in MSYS2:** the major version bump breaks our API usage. STOP migration. Options to discuss with user: (a) stay on vcpkg PoDoFo 0.10.x, (b) migrate code to PoDoFo 1.0 API (large rewrite of PoDoFoBackend.cpp ~1700 LOC).
- **Build fails with "undefined reference to symbol used by Qt 6.10":** check if the symbol exists in Qt 6.7+ via Qt docs. If yes — likely include / link missing; add the right Qt6:: target. If no — find the Qt 6.7-compatible equivalent (usually a 2-line edit per call site) and apply minimally.
- **ctest still shows 0xc0000139 after migration:** verify that ALL DLLs in `build/` come from `C:\msys64\mingw64\bin\` and NONE remain from Qt installer or vcpkg. Run `Get-ChildItem build\*.dll | Select-Object Name, @{N='Size';E={$_.Length}}, @{N='Source';E={(Get-Item ('C:\msys64\mingw64\bin\' + $_.Name) -ErrorAction SilentlyContinue).Length -eq $_.Length}}` to compare sizes — mismatched sizes indicate a non-MSYS2 DLL got mixed in.
- **windeployqt6 not found:** the binary may be named just `windeployqt` (no `6` suffix) on some MSYS2 versions. Try `Get-ChildItem C:\msys64\mingw64\bin\windeployqt*` to find the right name.
- **TestSignatureRealCrypto fails because tests/fixtures/signing/generate.bat uses CMD syntax** that doesn't work in MSYS2 bash: that's fine; the .bat is only run via Windows cmd.exe to generate fixtures once. After fixtures exist, ctest uses them as-is.
- **PDFium prebuilt incompatible with MSYS2 GCC 15.x ABI:** the prebuilt was likely compiled with MSVC or older GCC. If linking fails, options: (a) use a more recent bblanchon/pdfium-binaries build that targets MSYS2-compatible runtime, (b) compile PDFium from source (heavy: requires depot_tools + 30 GB disk + 3+ hours), (c) ship without PDFium (HAS_PDFIUM=OFF, falls back to QPdfView rendering — loses 3-tier cache + search but app still works).
</error_recovery>

<success_criteria>
- [ ] MSYS2 mingw-w64 toolchain installed (`gcc.exe --version` returns 15.x)
- [ ] MSYS2 Qt6 installed (`qmake6 -query QT_VERSION` returns 6.7+)
- [ ] All core deps installed via pacman (PoDoFo, qpdf, OpenSSL, Tesseract, Leptonica, libxml2, freetype, zlib, curl, libpng, libjpeg-turbo, libtiff)
- [ ] PDFium vendored to `third_party/pdfium/`
- [ ] ONNX Runtime resolved via bundled-tarball fallback in CMakeLists.txt
- [ ] OpenXLSX + DuckX integrated via FetchContent
- [ ] CMakeLists.txt has zero `VCPKG_TARGET_TRIPLET` / vcpkg path references
- [ ] build_all.bat + packaging/*.bat all use `C:\msys64\mingw64\bin` PATH
- [ ] Clean rebuild succeeds (0 errors)
- [ ] `windeployqt6` + manual DLL copies deploy all required runtime libs to `build/`
- [ ] ctest: **14/14 passing** (UnitTests + TestControllers now green; the previously-passing 12 continue to pass)
- [ ] README.md has new MSYS2 build instructions
- [ ] SESSION_BRIEF_NEXT.md old DLL-mix section removed
- [ ] CHANGELOG.md has migration entry under [1.0.0]
- [ ] LICENSE-3RD-PARTY.md has refreshed version columns
- [ ] 3 atomic commits exist with descriptive Conventional Commits messages
- [ ] Safety branch `msys2-migration-backup-pre` + tag `vcpkg-build-final` exist
- [ ] Final report (under 250 words) covers: pacman packages count, Qt version installed, tests pass/fail, any unexpected issues
</success_criteria>

---

**After this migration completes successfully, the build environment will be:**
- ✅ Single coherent toolchain (MSYS2 mingw64)
- ✅ No DLL ABI mix
- ✅ 14/14 ctest passing
- ✅ Easier maintenance via pacman
- ✅ Unblocked for Month 2 work (Edact-Ray + OCR text-layer scrub + veraPDF)

**Reversibility:** if anything goes wrong, `git checkout msys2-migration-backup-pre` + delete build/ + rebuild via the old `cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/...` invocation restores the prior state. The `vcpkg-build-final` tag is the immutable anchor.
