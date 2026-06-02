# GlyphPDF v1.0.0 — Professional PDF Workstation

A high-performance desktop PDF editor built with C++17 and Qt 6. Designed for professional environments with a focus on precision, security, and direct document manipulation.

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
- AI Chat panel
- Find & Replace with redact-all option
- Full keyboard accessibility (F6 region cycling, F1 help)

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

## Build Instructions

### Prerequisites
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

### Optional: PDF/A Validation (veraPDF)

Download [veraPDF](https://verapdf.org/home/#download) (AGPL-3.0 — invoked as subprocess only) and configure:

```bash
cmake -B build -DVERAPDF_CLI_PATH=/path/to/verapdf
```

Without this, `PdfAValidationPanel` shows "validator unavailable" — all other features work normally.

**Optional: LibreOffice** for Office→PDF import (`.docx`, `.xlsx`, `.pptx`, `.odt`, etc.):
```bash
# Via MSYS2 (ucrt64)
pacman -S --noconfirm mingw-w64-ucrt-x86_64-libreoffice-fresh
# OR install the system LibreOffice installer (adds soffice.exe to PATH automatically)
```
CMake auto-detects soffice at configure time. Without LibreOffice, Office→PDF import shows
a "LibreOffice not installed" message — all other features work normally.

### Why MSYS2 ucrt64?
GlyphPDF migrated from a hybrid Qt-installer + vcpkg setup to fully MSYS2-native in v1.0.0 development. This eliminates the libstdc++/libwinpthread ABI mismatch that previously required carefully-chosen DLL mixes in the build directory. Single coherent toolchain (GCC 16.x + Qt 6.11 + all deps from pacman), single source of truth for dependency versions, easier maintenance via `pacman -Syu`.

### Installation (MSI)
```bat
cd packaging
build-msi.bat
```
The MSI installer registers `.pdf` file associations via OpenWithProgids (does not hijack the default handler).

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

## Architecture (post-Branch C SCOPE LOCK)

GlyphPDF v1.0.0 is on Branch C SCOPE LOCK (real public v1.0.0 ships in M8; current MSI is private/internal). The architecture integrates three workstreams committed per `ROADMAP.md`:

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

GlyphPDF is open source under **Apache-2.0** (or MIT — license decision finalized at M8 public launch).

**Architectural constraints (per ROADMAP "Forbidden Dependencies"):**
- MuPDF (AGPL-3.0): **Never linked in-process** — CMake FATAL_ERROR guard enforces.
- Poppler (GPL-2.0+): **Never linked in-process** — CMake FATAL_ERROR guard enforces.
- DjVu: **Excluded as output format** (legacy; minimally maintained). Optional importer only for legacy corpus ingestion.
- veraPDF (AGPL-3.0): **Subprocess only, never linked in-process** (used for PDF/A conformance validation).

All third-party dependencies are documented in `LICENSE-3RD-PARTY.md`.
