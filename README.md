# GlyphPDF v1.0.0 — Professional PDF Workstation

[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![CI](https://github.com/eliets2/glyph-pdf/actions/workflows/ci.yml/badge.svg)](https://github.com/eliets2/glyph-pdf/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/eliets2/glyph-pdf)](https://github.com/eliets2/glyph-pdf/releases/latest)
[![Platform: Windows](https://img.shields.io/badge/platform-Windows-0078d4.svg)](https://github.com/eliets2/glyph-pdf/releases/latest)

A high-performance desktop PDF editor built with C++17 and Qt 6. Designed for professional environments with a focus on precision, security, and direct document manipulation — with no telemetry, no subscription, and no cloud dependency.

## Install

**You need nothing but the app itself.** No MSYS2, no Qt, no compilers, no runtime to install separately — every dependency (Qt 6, the PDF/OCR engines, the C++ and Visual C++ runtimes, the OCR models) is **bundled inside the download**. Just get it and run it.

| Option | Download | How to run |
|--------|----------|------------|
| **Installer** (recommended) | [`GlyphPDF-1.0.0-x64.msi`](https://github.com/eliets2/glyph-pdf/releases/latest) | Double-click → Next → Finish. Adds Start-menu & desktop shortcuts and a "PDF Document — GlyphPDF" Open-With entry. |
| **Portable** (no install) | [`GlyphPDF-1.0.0-x64-portable.zip`](https://github.com/eliets2/glyph-pdf/releases/latest) | Unzip anywhere — including a USB stick — and run `GlyphPDF.exe`. Nothing is written to the registry. |

Coming soon: `winget install Glyph.GlyphPDF` (pending Microsoft review).

**System requirements:** Windows 10 (version 1607+) or Windows 11, 64-bit. 4 GB RAM recommended for OCR on large documents. That's the entire list.

Every release is published with a `.sha256` file so you can verify the download integrity:
```powershell
Get-FileHash .\GlyphPDF-1.0.0-x64.msi -Algorithm SHA256
```

## Features

### Document Editing
- Text editing with inline support
- Full annotation suite (highlights, underlines, notes, stamps, shapes, pencil)
- Forms: text fields, checkboxes, radio buttons, dropdowns, date/numeric/calculated fields
- Page operations: rotate, crop, resize, reorder, insert, extract, split
- Headers, footers, page numbers, Bates numbering
- Text and image watermarks

### Security
- AES-256 password encryption
- Certificate-based encryption (multi-recipient)
- PAdES B-LT/B-LTA digital signatures with DSS/VRI
- Secure redaction (content stream excision, never black rectangles)
- Document sanitization (15+ vectors)
- SHA-256 only for signature hashing

### OCR
- Tesseract and RapidOCR PP-OCRv5 engines
- Preprocessing pipeline
- ROVER word-level multi-engine merge

### Conversion & Batch
- PDF to HTML, images, CSV, text
- Batch processing with inline error reporting
- Export presets (High Quality PDF/A, Web Optimized, Legal Archive)

### Print & Export
- Print preview with page setup (paper size, orientation, margins, scaling)
- Export presets panel with create/edit/delete support

### UI
- Ribbon toolbar with 7 controllers (Home, View, Edit, Pages, Convert, Forms, Security)
- Dark, Light, and High Contrast themes
- Drag-and-drop PDF opening
- Recent files (max 20)
- AI Chat panel (local Ollama only — no document content leaves the machine)
- Find & Replace with redact-all option
- Full keyboard accessibility (F6 region cycling, F1 help)

### Localization

GlyphPDF v1.0 ships in English. Arabic, French, and German translations are planned for a future release (translation scaffolding — 1394 strings each — is in place; human-translated packages are pending commissioning).

### Auto-Update
- JSON manifest-based update checker
- SHA-256 verified downloads
- User consent required at every stage

## Technical Architecture

```
pdfws_core (interfaces, ToolId, AppContext, commands base)
    pdfws_engines (PoDoFo, PDFium, qpdf, OCR, conversion)
        pdfws_commands (undo commands)
            pdfws_ui (MainWindow, controllers, modes, ribbon, dialogs)
                PdfWorkstation (main.cpp + Bootstrapper)
```

**Framework:** Qt 6.11.x (Core, Gui, Widgets, Pdf, PdfWidgets, PrintSupport, Svg, Network, Concurrent, Test) — installed via MSYS2 ucrt64 pacman

**Engines:**
- `PdfEditorEngine` — PoDoFo integration for structure manipulation, encryption, annotations
- `RenderCache` — PDFium-backed 3-tier LRU cache (256 MB, tiled rendering, prefetch)
- `SignatureManager` — PAdES signing, validation, TSA
- `UpdateChecker` — Async manifest-based auto-update with SHA-256 verification

**Dependencies:** PoDoFo, PDFium, qpdf, OpenSSL, Tesseract, Leptonica, LibXml2, Freetype, Zlib — all via MSYS2 ucrt64 pacman (except PDFium prebuilt + ONNX Runtime bundled)

## Building from Source (Developers only)

> **End users: skip this entire section.** Everything below is for compiling
> GlyphPDF from source. If you just want to *use* the app, see [Install](#install)
> above — the released MSI and portable ZIP already contain every dependency
> listed here. Nothing in this section is something a user ever installs.

### Prerequisites (build-time toolchain)
- **MSYS2** installed at `C:\msys64\` (https://www.msys2.org/)
- MSYS2 **ucrt64** environment with the following packages installed via pacman:
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

> **Note:** We use the **ucrt64** environment (not mingw64) because `qt6-pdf` (required for the PDF viewer) is only packaged for ucrt64. UCRT is the modern Universal C Runtime, system-native on all Windows 10+ installs.

### Build (Windows + MSYS2)
Open the **MSYS2 UCRT64** shell (`C:\msys64\ucrt64.exe`) or any shell with `C:\msys64\ucrt64\bin` on PATH:
```bash
cd /c/Users/User/Projects/pdf
mkdir -p build && cd build
cmake .. -G "Ninja"
cmake --build . --parallel 8
```
Or from PowerShell with MSYS2 ucrt64 on PATH:
```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cd C:\Users\User\Projects\pdf
cmake -B build -G "Ninja"
cmake --build build --parallel 8
```

### Optional external tools (two features)

Two **optional** features use external programs. Both are **detected at runtime** — the
app finds them automatically if they're on the machine (bundled alongside GlyphPDF, on the
PATH, in Program Files, or in the registry), with **no configuration**. Both degrade
gracefully: the app runs fine without them and offers a one-click download when you first
use the feature.

| Feature | External tool | How it's found | Without it |
|---------|--------------|----------------|------------|
| PDF/A conformance validation | [veraPDF](https://verapdf.org/home/#download) (AGPL-3.0, subprocess only) | bundled `verapdf/`, `GLYPHPDF_VERAPDF` env var, or PATH | In-app prompt to download veraPDF |
| Office → PDF import (`.docx`, `.xlsx`, `.pptx`, `.odt`) | LibreOffice (`soffice`) | bundled `libreoffice/`, PATH, Program Files, or registry | In-app prompt to download LibreOffice |

Neither is bundled in the default installer (veraPDF ships its own ~150 MB Java runtime;
LibreOffice is ~400 MB). To bundle veraPDF anyway, drop its CLI tree at `third_party/verapdf/`
before running `packaging/build-msi.bat` — `deploy.ps1` stages it and the app auto-detects it.

### Why MSYS2 ucrt64?
GlyphPDF migrated from a hybrid Qt-installer + vcpkg setup to fully MSYS2-native in v1.0.0 development. This eliminates the libstdc++/libwinpthread ABI mismatch that previously required carefully-chosen DLL mixes in the build directory. Single coherent toolchain (GCC 16.x + Qt 6.11 + all deps from pacman), single source of truth for dependency versions, easier maintenance via `pacman -Syu`.

### Building the installer + portable ZIP
To produce the distributable artifacts yourself (the same ones on the Releases page):
```bat
cd packaging
build-msi.bat
```
This runs the full pipeline — compile → `deploy.ps1` (stages **every** runtime DLL, the
VC++ runtime, ONNX models and tessdata into a self-contained tree) → WiX MSI **and**
portable ZIP, each with a SHA-256 checksum, written to `dist/`. The MSI registers `.pdf`
file associations via OpenWithProgids (it does **not** hijack the default handler).

## Testing

```bat
set QT_QPA_PLATFORM=offscreen
cd build
ctest --output-on-failure
```

**14 test targets:**

| Target | Category | What it tests |
|--------|----------|---------------|
| UnitTests | Unit | Core utility validation |
| TestInterfaces | Unit | Engine API contracts |
| SmokeTest | Integration | End-to-end load/save/modify |
| TestSanitization | Security | Metadata stripping vectors |
| TestSignatureValidation | Security | Byte-range and trust validation |
| TestRedaction | Security | Content stream excision, XObject redaction |
| TestThreadSafety | Concurrency | Mutex validation under concurrent access |
| TestEncryption | Security | AES-256 generation and enforcement |
| TestResourceLimits | Resilience | Page/buffer size boundaries |
| TestControllers | UI | Controller action dispatch |
| TestIntegration | E2E | Full workflow: open, edit, save, encrypt, rotate, redact |
| TestPerformance | Benchmark | Open/save timing, metadata ops, error overhead |

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open document |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+P | Print |
| Ctrl+F | Find |
| Ctrl+H | Find & Replace |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+, | Preferences |
| F1 | Keyboard shortcuts help |
| F6 / Shift+F6 | Cycle / reverse-cycle UI regions |
| F11 | Full screen |
| Alt+Left / Alt+Right | Navigate back / forward |
| Ctrl+0 | Actual size |
| Ctrl++ / Ctrl+- | Zoom in / out |

## Architecture

GlyphPDF v1.0.0 is publicly released (Apache-2.0). The architecture integrates three workstreams committed per `ROADMAP.md`:

- **Dual-Model Core** — Structural model (PDF object graph owned by PoDoFo + PDFium + qpdf — source of truth for sign/redact/forms/exact layout) ↔ Semantic model (`docmodel::SemanticDocument` — editing/interchange model). Djot ↔ Semantic = LOSSLESS; Semantic ↔ PDF = EXPLICITLY LOSSY both ways. `ProvenanceGuard` refuses Djot-edit-save-back for signed/born-PDF documents.
- **Heterogeneous LaneScheduler** — GPU lane (warm persistent worker, never spawn-per-page) + CPU lane (QtConcurrent, core-count) + cross-page pipelining (`layout(P+1) ‖ ocr(P) ‖ fusion(P-1)`). Reused by: OCR ensemble, MRC compression pipeline, future GPU workloads.
- **Parallel Layout + OCR Ensemble (WS1)** — PP-DocLayoutV2 layout detector (+ Surya when license permits) with IoU reconciliation → per-region Tesseract + RapidOCR PP-OCRv5 fanout via LaneScheduler → word-level confidence-weighted ROVER fusion. Per-region redo + per-word confidence overlay in OCRMode.
- **Djot Full Document Interchange (WS2)** — `docmodel` + `pdfws_djot` libraries; vendored Lua 5.4 reference parser (MIT); three roles: (a) OCR output mapping to SemanticDocument, (b) authoring input (Djot → Semantic → PoDoFo content stream), (c) annotation/comment rich text (Djot internal model, transcoded to /RC XHTML + /Contents plain text on save, original stashed in /PieceInfo for perfect GlyphPDF round-trip with Acrobat/Foxit interop).
- **MRC Layered Compression in PDF/A (WS3)** ✅ — Layout-region-guided mask separation (`MrcPageProcessor`) → JBIG2 lossless foreground (jbig2enc Apache-2.0; NEVER pattern-matching per 2013 Xerox incident) + JPEG2000 background (OpenJPEG 2.5.4 BSD-2, already in MSYS2) + invisible `3 Tr` OCR sandwich text from WS1 word boxes → PDF/A-2b assembly with XMP metadata + sRGB OutputIntent → veraPDF subprocess validation gate. **Achieved: 30.4× compression** (86 KB vs 2.64 MB raw pixels on A4 test page). Off/Lossless/Balanced/Aggressive modes in CompressDialog. Optional DjVu importer (`HAS_DJVU=OFF` default; import-only, no DjVu output).

```
              ┌────────────────────────────────────────────────┐
              │  Application Layer (GpMainWindow + Ribbon)     │
              └──────────────────────┬─────────────────────────┘
                                     │
              ┌──────────────────────▼─────────────────────────┐
              │             Dual-Model Core                    │
              │  Structural (PDF object graph)  │  Semantic    │
              │  PoDoFo / PDFium / qpdf         │  docmodel    │
              │  ProvenanceGuard ◄── boundary ──► pdfws_djot   │
              └──────────────────────┬─────────────────────────┘
                                     │
              ┌──────────────────────▼─────────────────────────┐
              │       Heterogeneous LaneScheduler              │
              │  GPU lane (warm worker)  │  CPU lane (pool)    │
              │  Cross-page pipeline: layout ‖ ocr ‖ fusion    │
              └──────────────────────┬─────────────────────────┘
                                     │
              ┌──────────────────────▼─────────────────────────┐
              │  OCR Ensemble (WS1)  │  MRC Pipeline (WS3)     │
              │  Layout+OCR fanout   │  JBIG2 + JP2K + Sandwich│
              └────────────────────────────────────────────────┘

FORBIDDEN: MuPDF (AGPL-3.0), Poppler (GPL-2.0+), DjVu output, veraPDF in-process linking.
```

## License

GlyphPDF is open source under **Apache-2.0**.

**Architectural constraints (per ROADMAP "Forbidden Dependencies"):**
- MuPDF (AGPL-3.0): **Never linked in-process** — CMake FATAL_ERROR guard enforces.
- Poppler (GPL-2.0+): **Never linked in-process** — CMake FATAL_ERROR guard enforces.
- DjVu: **Excluded as output format** (legacy; minimally maintained). Optional importer only for legacy corpus ingestion.
- veraPDF (AGPL-3.0): **Subprocess only, never linked in-process** (used for PDF/A conformance validation).

All third-party dependencies are documented in `LICENSE-3RD-PARTY.md`.
