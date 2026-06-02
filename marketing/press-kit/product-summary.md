# GlyphPDF v1.0.0 — Product Summary (Press Kit)

**Category:** Desktop productivity / document management
**Platform:** Windows 10 x64 and Windows 11 x64
**License:** Apache-2.0
**Price:** Free. No subscription. No account required.
**Website:** [URL — to be published at launch]
**Source code:** github.com/[ORG]/glyphpdf
**Contact:** [maintainer@email]

---

## What is GlyphPDF?

GlyphPDF is a professional PDF workstation for Windows, built in C++17 and Qt 6.
It is designed for professionals who handle sensitive documents — legal, financial,
or archival — and need to sign, redact, compress, and annotate PDFs with verifiable
security, entirely on their own machine.

No cloud upload. No telemetry by default. No subscription. The full source code
is available under Apache-2.0.

---

## Core capabilities

### Digital signing
GlyphPDF signs documents with PAdES B-LTA — the long-term archival profile
specified by ETSI EN 319 132. Signatures embed the full certificate chain, OCSP
revocation evidence, and a qualified timestamp, allowing validation years after
the signing certificate expires. Trust is verified against the Windows system
certificate store. RSA keys below 2048 bits are rejected. SHA-256 only.

### Secure redaction
Redaction removes text from the PDF content stream — it is not painted over with
a black rectangle. GlyphPDF implements Edact-Ray glyph-advance normalization
(Bland et al., PETS 2023): redacted positions emit a numeric gap rather than a
space glyph, eliminating the measurable glyph-width side channel that allows
character recovery from professionally-redacted PDFs. Invisible OCR text layers
are also scrubbed from redaction rectangles.

### Dual-engine OCR with confidence overlay
Two OCR engines run in parallel: Tesseract 5 and PaddleOCR PP-OCRv5. Results are
merged per-word using ROVER confidence weighting. A per-word color overlay
(green / yellow / red) shows confidence levels directly on the document page.
Users can re-run OCR on individual regions without reprocessing the whole document.

### MRC layered compression for PDF/A archives
The Mixed Raster Content pipeline separates scanned pages into text foreground
(compressed with JBIG2 lossless, Apache-2.0) and image background (compressed
with JPEG2000 via OpenJPEG). The invisible OCR text layer is embedded as a
searchable sandwich. Output is PDF/A-2b, validated by veraPDF. Measured
compression: 30x on A4 scanned content (86 KB vs 2.64 MB raw pixels).
JBIG2 pattern-matching mode is permanently disabled (Xerox 2013 digit-substitution
incident; German BSI guidance).

### Annotation and forms
Full annotation suite: highlights, underlines, notes, stamps, shapes, pencil.
Comment threading with ISO 32000 /IRT in-reply-to linking and five review states.
Form builder supports nine field types with a tab order editor. Annotation rich
text uses Djot internally, transcoded to /RC XHTML for Acrobat/Foxit interop.

### Document editing
Text editing, page operations (rotate, crop, reorder, split, insert, extract),
headers/footers, page numbers, Bates numbering, watermarks, batch processing with
error log export, and PDF/HTML/CSV/text conversion. Office import via LibreOffice
subprocess (optional). Images-to-PDF via PoDoFo XObject embedding.

### AI assistant
Built-in AI Chat panel supporting four backends: Anthropic Claude, OpenAI GPT-4o,
Google Gemini, and local Ollama. API key stays in local credential storage.

---

## Technical architecture

```
PdfWorkstation.exe
  pdfws_ui         — MainWindow, ribbon (7 tabs), modes, sidebars, dialogs
    pdfws_commands — Undo/redo command objects
      pdfws_engines — PoDoFo, PDFium, qpdf, OCR, MRC, signing, conversion
        pdfws_core  — Interfaces, AppContext, commands base
```

**Rendering:** PDFium (Chrome's PDF renderer) with 3-tier LRU cache, 256 MB,
tiled rendering, async prefetch.

**Document model:** Dual-model architecture — Structural (PDF object graph via
PoDoFo/PDFium/qpdf) and Semantic (docmodel::SemanticDocument via Djot interchange).
ProvenanceGuard prevents semantic edits from overwriting signed documents.

**Scheduling:** LaneScheduler with GPU lane (persistent warm worker) and CPU lane
(QtConcurrent pool). Cross-page pipeline overlaps layout, OCR, and fusion.

**Build:** MSYS2 ucrt64 (GCC 16 + Qt 6.11 + all deps via pacman). Single coherent
toolchain — no vcpkg, no Qt installer mixing.

---

## Test coverage

32 test targets including:
- Signature validation (real crypto + adversarial fixtures: expired cert, RSA-1024,
  revoked cert via OCSP stub, tampered PDF, multi-signature)
- Redaction (content stream excision, XObject recursion, Edact-Ray normalization,
  OCR layer scrub)
- MRC pipeline (layer separation, JBIG2, JPEG2000, sandwich text, 30x reduction,
  PDF/A-2b veraPDF gate)
- Thread safety (concurrent access, RenderCache LRU under contention)
- Forms, OCR ensemble, Djot roundtrip, Myers diff engine

---

## License and dependencies

GlyphPDF is **Apache-2.0**. No GPL or AGPL dependency is linked in-process.
CMake build guards enforce this: MuPDF (AGPL-3.0) and Poppler (GPL-2.0+) cause
a FATAL_ERROR if detected. veraPDF (AGPL-3.0) is invoked as a subprocess only,
never linked.

Key third-party licenses: PoDoFo (LGPL-2.0+), PDFium (BSD-3-Clause), qpdf
(Apache-2.0), OpenSSL (Apache-2.0), Tesseract (Apache-2.0), ONNX Runtime (MIT),
OpenJPEG (BSD-2-Clause), jbig2enc (Apache-2.0), Lua 5.4 (MIT).

Full license matrix: `LICENSE-3RD-PARTY.md` in the repository.

---

## Press kit contents

```
marketing/press-kit/
  product-summary.md       — this document
  logos/
    app_icon.png           — app icon (from resources/branding/)
    wordmark.png           — GlyphPDF wordmark (from resources/branding/)
    splash.png             — splash screen asset (from resources/branding/)
    installer_logo.png     — installer banner (from resources/branding/)
    tray_icon.png          — system tray icon (from resources/branding/)
  screenshots/             — PENDING: hero screenshots (see marketing/screenshots/)
```

### To assemble press-kit.zip (human action required)

Once hero screenshots are captured and placed in `marketing/screenshots/`:

```powershell
# From repo root
Copy-Item marketing/screenshots/*.png marketing/press-kit/screenshots/
Compress-Archive -Path marketing/press-kit/* -DestinationPath marketing/press-kit.zip
```

---

## Suggested press angles

1. **Security:** "The PDF editor that does redaction right" — content-stream
   excision + Edact-Ray side-channel defense is a differentiator no commercial
   tool advertises.
2. **Privacy:** "No telemetry, no cloud, no account" — strong angle for legal,
   healthcare, and government audiences.
3. **Open source:** "Apache-2.0 PDF workstation" — the free/OSS PDF landscape
   is thin on native Windows desktop tools with full feature parity.
4. **Technical depth:** "30x compression for scanned archives" — MRC + JBIG2 +
   PDF/A-2b is a niche feature with a measurable, publishable number.

---

*Document prepared: 2026-06-02 (M8-P1 D4)*
*Based on: README.md + CHANGELOG.md at HEAD c454a76*
