# GlyphPDF — Build Instructions (for auditor)

> **Deliverable:** M7-PROMPT-1 / D2
> **Date:** 2026-06-01
> **Audience:** Third-party security auditor
>
> Distilled from `README.md` §Build Instructions. The auditor needs a buildable
> tree to construct fuzzing harnesses and reproduce findings. **Windows-only**
> (MSYS2 ucrt64). Authoritative source is the README; this is the short path.

---

## Toolchain

- **OS:** Windows 10/11 (UCRT is system-native on all Windows 10+).
- **MSYS2** installed at `C:\msys64\` — https://www.msys2.org/
- **Environment: ucrt64** (NOT mingw64). Reason: `qt6-pdf` (the PDF viewer) is
  only packaged for ucrt64. Single coherent toolchain: GCC 16.x + Qt 6.11 + all
  deps from `pacman` (avoids the libstdc++/libwinpthread ABI mismatch the old
  hybrid Qt-installer + vcpkg setup had).
- **Generator:** Ninja. **Build system:** CMake.

## Install dependencies (MSYS2 UCRT64 shell)

```bash
pacman -Syu --noconfirm
pacman -S --noconfirm --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-tools \
    mingw-w64-ucrt-x86_64-qt6-svg \
    mingw-w64-ucrt-x86_64-qt6-translations \
    mingw-w64-ucrt-x86_64-qt6-imageformats \
    mingw-w64-ucrt-x86_64-qt6-pdf \
    mingw-w64-ucrt-x86_64-podofo \
    mingw-w64-ucrt-x86_64-qpdf \
    mingw-w64-ucrt-x86_64-openssl \
    mingw-w64-ucrt-x86_64-tesseract-ocr \
    mingw-w64-ucrt-x86_64-tesseract-data-eng \
    mingw-w64-ucrt-x86_64-leptonica \
    mingw-w64-ucrt-x86_64-libxml2 \
    mingw-w64-ucrt-x86_64-freetype \
    mingw-w64-ucrt-x86_64-zlib \
    mingw-w64-ucrt-x86_64-curl \
    mingw-w64-ucrt-x86_64-libpng \
    mingw-w64-ucrt-x86_64-libjpeg-turbo \
    mingw-w64-ucrt-x86_64-libtiff
```

> PDFium (prebuilt) and ONNX Runtime are bundled/vendored, not from pacman.

## Build

**Option A — MSYS2 UCRT64 shell** (`C:\msys64\ucrt64.exe`):

```bash
cd /c/Users/User/Projects/pdf
mkdir -p build && cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
```

**Option B — PowerShell with MSYS2 ucrt64 on PATH:**

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cd C:\Users\User\Projects\pdf
cmake -B build -G "Ninja"
cmake --build build --parallel 8
```

## Run the test suite

```bat
set QT_QPA_PLATFORM=offscreen
cd build
ctest --output-on-failure
```

Single security target:

```bat
ctest -R TestRedaction --output-on-failure
```

(`offscreen` Qt platform is required — tests are GUI-less but link Qt Widgets.)

## Optional integrations (security-relevant subprocesses)

These are **subprocess** integrations (B3 in `threat-model.md`), not in-process
links — relevant to the auditor's subprocess-injection review.

- **veraPDF** (PDF/A validation, AGPL-3.0, **subprocess only**):
  ```bash
  cmake -B build -DVERAPDF_CLI_PATH=/path/to/verapdf
  ```
  Without it, `PdfAValidationPanel` shows "validator unavailable"; all other
  features work.

- **LibreOffice** (Office→PDF import — `.docx/.xlsx/.pptx/.odt`):
  ```bash
  pacman -S --noconfirm mingw-w64-ucrt-x86_64-libreoffice-fresh
  # OR install system LibreOffice (adds soffice.exe to PATH)
  ```
  CMake auto-detects `soffice` at configure time. Without it, Office import shows
  "LibreOffice not installed"; all other features work.

## Packaging (context only)

```bat
cd packaging
build-msi.bat
```

The MSI registers `.pdf` associations via `OpenWithProgids` (does **not** hijack
the default handler).

---

## Notes for the auditor

- **Pin the audited revision.** Fix the commit SHA at engagement start; record it
  in the SOW and in any findings.
- **Fuzzing harness target.** The highest-value harness wraps the PDF-load path
  and `applyRedactions`/`sanitizeDocument` over the adversarial corpus described
  in `test-fixtures-list.md`. Build with sanitizers where the MinGW/ucrt64
  toolchain supports them (AddressSanitizer/UBSan availability varies on MinGW —
  confirm; a Clang/MSVC ASan side-build of the parsing core may be needed for
  full memory-safety coverage).
- **Architectural guards to verify at build time:** CMake `FATAL_ERROR` guards
  forbid in-process linking of MuPDF (AGPL-3.0) and Poppler (GPL-2.0+); veraPDF
  must be subprocess-only. Confirm these guards actually trigger.
- **Forbidden in-process deps** (per README/ROADMAP): MuPDF, Poppler, in-process
  veraPDF. Their absence is part of the security/licensing posture.
