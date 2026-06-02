# GlyphPDF v1.0.0 — First Public Release

GlyphPDF is an open-source, privacy-first PDF workstation for Windows. No telemetry. No cloud
upload. No subscription. Your documents stay on your machine.

Licensed under [Apache-2.0](LICENSE).

---

## What's in v1.0.0

### Core PDF Engine
- **Three-backend architecture:** PoDoFo for editing/annotations/forms/encryption,
  PDFium for high-quality rendering (3-tier LRU tile cache, 256 MB), qpdf for
  linearization and repair-on-open.
- **Incremental save** preserves digital signatures.
- **Corruption recovery** with graceful repair-on-open warnings.

### Security & Redaction
- **Edact-Ray defense:** glyph-advance normalization closes the side-channel attack
  (Bland et al., PETS 2023 / arXiv 2206.02285) — redacted text cannot be reconstructed
  from cursor-position gaps.
- **Invisible OCR text scrub:** Tr==3 invisible text within redaction rectangles is
  excised from the content stream, not merely painted over.
- **PAdES B-LT/B-LTA digital signatures** with real DSS/VRI embedding, trust-chain
  verification against the Windows system root store, OCSP response verification, and
  ByteRange overlap detection (PDF shadow attack prevention).
- **15+ sanitization vectors:** metadata, JavaScript, embedded files, and more.
- **Content-literal injection fixed:** `pdfEscapeLiteralString` hardens watermark,
  annotation, header/footer, and Bates numbering against injection.

### OCR — Workstream 1
- **PP-DocLayoutV2 layout detector** (ONNX, Apache-2.0): 11-region-type detection
  (Title/Paragraph/Table/Figure/List/Header/Footer/Equation/Reference/Caption/Other).
- **Tesseract 5.5.2 + RapidOCR PP-OCRv5** dual-engine fanout via LaneScheduler.
- **ROVER word-level fusion** for multi-engine consensus.
- **Per-region confidence overlay** (green/yellow/red) with re-OCR on demand.
- **Cross-page pipeline:** layout ‖ OCR ‖ fusion overlap via `CrossPagePipeline`.

### Djot Document Interchange — Workstream 2
- **Dual-model architecture:** Structural PDF object graph ↔ Semantic `SemanticDocument`.
- **OCR→Djot mapping** (`OcrDjotMapper`): reading-order layout regions → block structure,
  per-word provenance encoding for MRC sandwich-text alignment.
- **Annotation rich text** in Djot with /RC XHTML + /Contents plain text + /PieceInfo
  Djot sidecar — perfect GlyphPDF roundtrip plus Acrobat/Foxit interoperability.
- **Comment threading:** `/IRT` in-reply-to linking, review-state roundtrip, depth-indent
  view with date/status/author filters.
- **ProvenanceGuard** refuses Djot save-back to signed born-PDF documents.
- Vendored Lua 5.4 (MIT) + Djot reference parser for spec-conformant parsing.

### MRC Layered Compression — Workstream 3
- **30× compression ratio** measured on synthetic A4 scanned page.
- **JBIG2 lossless foreground** (agl/jbig2enc, Apache-2.0) — pattern-matching mode
  disabled (Xerox 2013 / German BSI ban).
- **JPEG2000 background** at configurable ratio (Lossless 10:1 / Balanced 30:1 /
  Aggressive 50:1) via OpenJPEG 2.5.4 (BSD-2-Clause).
- **Invisible sandwich text** from WS1 OCR word boxes — output is searchable PDF/A-2b.
- **veraPDF validation gate** before finalization.

### AI Assistant
- **Four providers:** Anthropic Claude 3.5 Sonnet, OpenAI GPT-4o, Google Gemini 1.5 Flash,
  local Ollama (llama3 default). Real async HTTPS calls — no canned responses.
- API keys stored in Windows Credential Manager or QSettings.
- "Test key" button performs a real 1-token ping before saving.

### Office & Image Import
- **Office→PDF** via LibreOffice subprocess (docx/xlsx/pptx/odt/ods/odp/rtf/csv/txt).
- **Images→PDF** via PoDoFo PdfImage XObject embedding (PNG/JPEG/TIFF).

### Modes & Tools
- **7 mode tabs:** Home, View, Edit, Pages, Convert, Forms, Security
- **Pages mode:** split (by-page / every-N / range expression), reorder with drag-drop.
- **Batch mode:** 7 operation types, per-file progress, continue-on-failure, CSV/JSON
  error log export.
- **Form Builder:** 9 field types (Text/Checkbox/Radio/Dropdown/ListBox/Date/Numeric/
  Signature/Button) with tab-order editor and undo/redo.
- **Compare mode:** Myers LCS diff with move detection (green/red/orange), NEXT/PREV
  navigation.
- **Signature mode:** PAdES B-LT/B-LTA signing + validation with trust indicators.
- **PDF/A mode:** veraPDF subprocess validation, conformance levels, export.
- **Compress mode:** MRC Off/Lossless/Balanced/Aggressive selector + live size estimate.

### Branding & UI
- App icon, splash screen, and system-tray icon (Show/Quit).
- Dark, Light, and High Contrast themes.
- WCAG-aligned accessibility: screen reader names, F6 region cycling, F1 help dialog.
- Autosave every 5 minutes (configurable 1-30 min); crash recovery on next launch.

### Localization
- Translation framework wired for Arabic (RTL), French, German.
- 1394 source strings each; human-translated packages pending commissioning.

---

## Installation

**System requirements:** Windows 10/11 x64, ~500 MB disk space.

1. Download `GlyphPDF-1.0.0-x64.msi` below.
2. Verify the SHA-256 checksum (see SHA-256 Verification below).
3. Run the MSI installer — no admin rights required for per-user install.
4. Launch GlyphPDF from the Start Menu or by double-clicking any `.pdf` file.

**Optional dependencies (not bundled):**
- LibreOffice (for Office→PDF import)
- veraPDF CLI (for PDF/A live validation in the PDF/A panel)
- Ollama (for local AI assistant without an API key)

---

## SHA-256 Verification

```
SHA-256: REPLACE_WITH_ACTUAL_HASH_AFTER_SIGNING
File:    GlyphPDF-1.0.0-x64.msi
```

**PowerShell verification:**
```powershell
(Get-FileHash "GlyphPDF-1.0.0-x64.msi" -Algorithm SHA256).Hash
# Compare with the hash above — must match exactly
```

**Command prompt:**
```cmd
certutil -hashfile GlyphPDF-1.0.0-x64.msi SHA256
```

---

## Build From Source

```bash
# MSYS2 ucrt64 toolchain (GCC 16.1.0, Qt 6.11.0)
pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-podofo \
          mingw-w64-ucrt-x86_64-tesseract-ocr mingw-w64-ucrt-x86_64-openjpeg2

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 8
QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4
```

Full package list in `README.md` → "Build Instructions".

---

## Acknowledgements

GlyphPDF would not exist without these open-source projects:

| Library | License | Use |
|---------|---------|-----|
| Qt 6.11 | LGPL-3.0 | Application framework |
| PoDoFo 0.10.4 | LGPL-2.0 | PDF editing + annotations |
| PDFium | BSD-3-Clause | PDF rendering |
| qpdf 12.3.2 | Apache-2.0 | PDF linearization + repair |
| Tesseract 5.5.2 | Apache-2.0 | OCR engine |
| OpenJPEG 2.5.4 | BSD-2-Clause | JPEG2000 encoding |
| agl/jbig2enc | Apache-2.0 | JBIG2 lossless compression |
| Lua 5.4 | MIT | Djot reference parser host |
| ONNX Runtime | MIT | ML inference (layout + OCR) |
| OpenSSL | Apache-2.0 | Cryptographic operations |

Full dependency audit: `docs/MRC-ENCODER-LICENSE-AUDIT.md`

---

*Contributions welcome — see `CONTRIBUTING.md` for the contribution guide and `SECURITY.md` for responsible disclosure.*
