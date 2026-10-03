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

## Native-Linux artifact (L03, 2026-10-04)

The SAME bblanchon release supplies the Linux binary — same version pin
discipline as Windows, different platform artifact:

- **Artifact**: `pdfium-linux-x64.tgz` (from the identical
  `chromium%2F7834` release tag)
- **Version**: 150.0.7834 (artifact `VERSION`: MAJOR=150 MINOR=0 BUILD=7834
  PATCH=0 — the same 150.0.7834 line as the Windows pin)
- **Build flags** (artifact `args.gn`, committed here as the build record):
  `pdf_enable_v8 = false`, `pdf_enable_xfa = false`, `pdf_is_standalone =
  true`, `is_component_build = false`, `is_debug = false`,
  `target_cpu = "x64"`, `target_os = "linux"` — the non-V8, non-XFA shape the
  Windows DLL is built with, so the two platforms expose the same API surface.
- **SHA-256 (archive `pdfium-linux-x64.tgz`)**:
  `e10b18234af3e988b3021547786e574b8905a24511067f14773f29c9cac12365`
  — cross-verified against the **GitHub release API asset digest** for this
  asset (`sha256:e10b1823...`, an upstream-published checksum), observed
  2026-10-04.
- **SHA-256 (`lib/libpdfium.so`, the extracted link/runtime object)**:
  `246872bdd5e05843b70051e6378216cc584535a1f4a7248b9f88059715d70f7c`
  (only libc/libm/libpthread/libgcc_s dependencies — a plain glibc build;
  `ldd` clean in the lane container)
- **Staging**: `scripts/bootstrap-vendor-deps.sh` (Linux branch) downloads
  the archive (SHA-256-pinned, with a download-once cache under
  `GLYPHPDF_ARTIFACT_CACHE`, default `/opt/glyphpdf-artifact-cache`), verifies
  BOTH hashes (G18 two-object rule) and stages the `.so` to
  `third_party/pdfium/lib/libpdfium.so` — gitignored, exactly like
  `bin/pdfium.dll` on Windows. The pin is enforced at configure time by
  `cmake/FindPdfium.cmake` (platform-selective expected hash: the Windows
  `.dll.a` pin above on WIN32, the `.so` pin on every other platform).
- **Headers**: the tracked `third_party/pdfium/include/` tree was diffed
  against this artifact's `include/` on 2026-10-04 — byte-identical (the
  artifact additionally ships `include/cpp/` C++ wrapper headers and
  `fpdfview.h.orig`, which GlyphPDF does not use and which were NOT staged;
  no tracked header differs). The committed headers therefore remain the
  platform-independent provenance pin for BOTH platforms.
