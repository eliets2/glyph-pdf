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

**Framework:** Qt 6.10.x (Core, Gui, Widgets, Pdf, PdfWidgets, PrintSupport, Svg, Network, Concurrent, Test)

**Engines:**
- `PdfEditorEngine` — PoDoFo integration for structure manipulation, encryption, annotations
- `RenderCache` — PDFium-backed 3-tier LRU cache (256 MB, tiled rendering, prefetch)
- `SignatureManager` — PAdES signing, validation, TSA
- `UpdateChecker` — Async manifest-based auto-update with SHA-256 verification

**Dependencies:** PoDoFo, PDFium, qpdf, OpenSSL, Tesseract, Leptonica, LibXml2, Freetype, Zlib (via vcpkg)

## Build Instructions

### Prerequisites
- Qt 6.10+ (MinGW 13.1.0 64-bit)
- CMake 3.16+
- MinGW Makefiles
- vcpkg with `x64-mingw-dynamic` triplet

### Build (Windows + MinGW + vcpkg)
```bat
set "PATH=C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;%PATH%"

mkdir build && cd build
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build . --parallel
```

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

**12 test targets:**

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

## License

Architectural constraints:
- MuPDF (AGPL-3.0): **Never linked in-process**
- Poppler (GPL-2.0+): **Never linked in-process**

All third-party dependencies are documented in `LICENSE-3RD-PARTY.md`.
