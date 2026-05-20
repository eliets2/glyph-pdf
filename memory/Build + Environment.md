# Build + Environment

## Toolchain

| Component | Version | Path |
|-----------|---------|------|
| Qt | 6.10.2 | `C:\Qt\6.10.2\mingw_64` |
| MinGW | 13.1.0 | `C:\Qt\Tools\mingw1310_64` |
| CMake | (Qt bundled) | `C:\Qt\Tools\CMake_64\bin` |
| vcpkg | classic mode | `C:\vcpkg` |
| Triplet | x64-mingw-dynamic | |
| C++ Standard | C++17 | |

## Required Dependencies (always linked)

| Package | Purpose |
|---------|---------|
| podofo | PDF parsing/editing engine |
| OpenSSL | Digital signatures, encryption |
| Qt6 Core/Gui/Widgets/Pdf/PdfWidgets/PrintSupport/Svg/Network/Test | UI + PDF rendering |

## Optional Dependencies (conditional compilation)

| Package | Guard | Feature | Status |
|---------|-------|---------|--------|
| Tesseract + Leptonica | `HAS_TESSERACT` | OCR text recognition | Available via MSYS2 |
| RapidOCR (ONNXRuntime) | `HAS_RAPIDOCR` | Secondary OCR engine | Available via MSYS2 |
| qpdf | `HAS_QPDF` | PDF linearization | Not installed |
| OpenXLSX | `HAS_OPENXLSX` | Excel export | Available |
| duckx | `HAS_DUCKX` | Word export | Available |

## Build Commands

### MSYS2 Dependencies (OCR)
If compiling with OCR support, ensure you have the MSYS2 Mingw-w64 packages installed:
```bat
pacman -S mingw-w64-x86_64-tesseract-ocr mingw-w64-x86_64-leptonica mingw-w64-x86_64-onnxruntime
```
*Note: Make sure MSYS2 `mingw64/bin` and `mingw64/lib/pkgconfig` are accessible in your environment or `CMAKE_PREFIX_PATH`.*

### Configure (first time)
```bat
set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.10.2\mingw_64\bin;C:\Users\User\msys64\mingw64\bin;%PATH%
set PKG_CONFIG_PATH=C:\Users\User\msys64\mingw64\lib\pkgconfig;%PKG_CONFIG_PATH%

cmake -S . -B build -G "MinGW Makefiles" ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
  -DVCPKG_MANIFEST_MODE=OFF ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64;C:/vcpkg/installed/x64-mingw-dynamic"
```

### Build
```bat
cmake --build build -- -j8
```

### Run Tests
```bat
set QT_QPA_PLATFORM=offscreen
cd build
ctest --output-on-failure -j4
```

Or use `build_all.bat` for a one-shot configure + build.

## Test Targets

| Target | File | Labels | Tests |
|--------|------|--------|-------|
| `UnitTests` | `tests/UnitTests.cpp` | unit, qt | Core engine unit tests |
| `TestInterfaces` | `tests/TestPdfEditorInterface.cpp` | unit, interfaces, commands | Mock-based interface + command tests |
| `TestSanitization` | `tests/TestSanitization.cpp` | unit, security, sanitization | SanitizeDocumentHelper edge cases |
| `TestSignatureValidation` | `tests/TestSignatureValidation.cpp` | unit, security, signatures | MockSignatureManager validation |
| `SmokeTest` | `tests/smoke_test.cpp` | smoke, integration | End-to-end PoDoFo operations |

All 5 tests pass. `SmokeTest::testLinearize` gracefully skips when qpdf is not compiled in.

## Known Environment Notes


- Tesseract/Leptonica/qpdf made optional via `find_package(... QUIET)` + conditional guards
- Qt 6.9.3 also installed but project uses 6.10.2
- `vcpkg_installed/` at project root contains pre-built dependencies (103 MB, keep to avoid rebuild)
