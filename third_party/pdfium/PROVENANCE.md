# PDFium Provenance

- **Source URL**: https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F7834
- **Artifact**: `pdfium-win-x64.tgz`
- **Version**: 150.0.7834.0 (Chromium branch 7834)
- **Build Environment**: bblanchon/pdfium-binaries prebuilt (win-x64)
- **SHA-256 (`lib/libpdfium.dll.a`)**: `0fcd45dca1cb20e73f2335046d63f630afc13430e2d5e8309d08f6e940b0de03`
- **SHA-256 (`bin/pdfium.dll`)**: `A487E1D2A18F164ADC3A17AACEE158787FA86049E6D91D3712B0A43F745E6905`

Verified 2026-06-11: the local `pdfium.dll` (FileVersion 150.0.7834.0) is
byte-identical to the `bin/pdfium.dll` inside the chromium/7834 release
artifact, which pins the previously-unknown provenance.

The runtime DLL is NOT committed to git. CI downloads it from the release URL
above and verifies the SHA-256 before use (see `.github/workflows/`).
Locally it lives at `third_party/pdfium/bin/pdfium.dll` (gitignored) and is
staged into the build directory by the `stage_runtime_dlls` CMake target.
