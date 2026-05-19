# PDF Workstation Pro — Corporate Document Platform

A professional, high-performance desktop PDF editor built with C++17 and Qt 6. Designed for enterprise environments, this platform focuses on precision, stability, security, and direct document manipulation.

## Core Features

- **Glyph UI**: Pixel-perfect "Ultra-Pro" Dark Obsidian aesthetic. Native ribbon UI, mode strips, scalable SVG icons, and reactive sidebar panels.
- **Advanced Annotation & Redaction**: Apply secure, non-extractable redactions and inline edits using PoDoFo object manipulation.
- **Data Security**: Secure document sanitization, byte-range signature validation, and AES-256 encryption.
- **Engine-Driven Architecture**: Clean separation between `gp::` shell layer and core `pdfws_engines` logic via `AppContext`.

## Technical Architecture

- **Framework**: Qt 6.10.x (Core, Gui, Widgets, Pdf, PdfWidgets, PrintSupport, Svg, Concurrent, Test)
- **Engines**:
  - `PdfEditorEngine`: Core PoDoFo integration for structure manipulation and encryption.
  - `SignatureManager`: Document certification and cryptographic trust validation.
  - `CollaborationManager`: Thread-safe metadata parsing and comment tracking.
- **Shell**: The new `gp::` namespace containing `gp::MainWindow`, `gp::Ribbon`, `gp::Sidebar`, `gp::ModeController`, and isolated `controllers/`.
- **Dependencies**: PoDoFo (0.10.3), OpenSSL, LibXml2, Freetype, Zlib (All managed via vcpkg).

## Build Instructions

### Prerequisites
- Qt 6.10+ (Mingw64)
- CMake 3.16+
- Ninja or Mingw32-make
- vcpkg (with `x64-mingw-dynamic` triplet configured)

### Compilation (Windows + MinGW + vcpkg)
```bat
# Setup Qt Environment
set "PATH=C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;%PATH%"

# Standard CMake build using vcpkg toolchain
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build . -j8
```
A convenience script `build_all.bat` is provided in the project root.

## Testing

The project contains a comprehensive suite of unit and integration tests.
To run the tests, build the project and execute CTest from the build directory:
```bat
cd build
ctest --output-on-failure -j8
```

**Test Targets:**
- `SmokeTest`: Validates end-to-end load/save/modify capability.
- `UnitTests`: Core utility validation.
- `TestInterfaces`: Contract validation for Engine APIs.
- `TestSanitization`: Verifies sensitive metadata stripping.
- `TestSignatureValidation`: Byte-range and cryptographic trust validation tests.
- `TestRedaction`: Verifies text extraction defense and XObject redaction.
- `TestEncryption`: AES-256 generation and enforcement tests.
- `TestThreadSafety`: Mutex validation during concurrent engine usage.
- `TestResourceLimits`: Page and buffer size enforcement boundaries.

## GSD Roadmap
Detailed project milestones and maintenance logs are tracked in [ROADMAP.md](./ROADMAP.md).

---
*Created by Antigravity Systems — Orchestrator-refreshed v1.0.0*
