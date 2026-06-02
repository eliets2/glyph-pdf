# Reddit Posts

---

## r/PDF

**Title:** GlyphPDF v1.0.0 — open-source PDF editor for Windows with real redaction security (Apache-2.0)

**Body:**

Hi r/PDF,

I shipped GlyphPDF v1.0.0 today — a free, open-source PDF workstation for Windows with no telemetry and no subscription.

**Why I built it:** Most PDF editors claim "secure redaction" but don't address the Edact-Ray attack (Bland et al., 2022) — where glyph-advance deltas in the PDF content stream can reconstruct what was redacted without touching the visible black box. GlyphPDF normalizes all glyph advances after redaction and scrubs invisible OCR text layers (Tr==3), closing both vectors.

**Key features:**
- Secure redaction with Edact-Ray defense + invisible OCR text scrub
- PAdES B-LT/B-LTA digital signatures with real OCSP verification
- MRC compression: JBIG2 + JPEG2000 + searchable OCR text — 30x ratio on scanned pages
- OCR: PP-DocLayoutV2 + Tesseract 5 + RapidOCR PP-OCRv5 + ROVER fusion
- PDF/A-2b export with veraPDF validation
- Batch processing, form builder, compare mode with Myers LCS diff
- AI assistant (Anthropic Claude, OpenAI, Gemini, local Ollama)
- Apache-2.0 open source

Windows 10/11 x64. MSI installer.

GitHub + downloads: https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0

---

## r/opensource

**Title:** GlyphPDF v1.0.0 — Apache-2.0 PDF workstation for Windows, no telemetry, no cloud

**Body:**

Releasing GlyphPDF today — eight months of work, finally v1.0.0.

It's a full PDF editor and workstation for Windows: edit, redact, sign, OCR, compress, compare, batch process. Everything runs locally. Apache-2.0. No SaaS.

Tech highlights for the open-source crowd:
- Three-backend PDF engine: PoDoFo (editing) + PDFium (rendering) + qpdf (linearize/repair)
- Edact-Ray secure redaction (arXiv 2206.02285 side-channel defense)
- MRC layered compression with JBIG2 + JPEG2000 + OCR sandwich text
- ONNX Runtime ML inference for layout detection and OCR
- Vendored Lua 5.4 + Djot reference parser for document interchange
- 32 test targets, all pass

GitHub: https://github.com/YOUR_USERNAME/glyphpdf
Releases: https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0

Contributions welcome. See CONTRIBUTING.md.

---

## r/privacy

**Title:** GlyphPDF — open-source PDF editor that runs 100% locally (no telemetry, no cloud, no subscription)

**Body:**

Just released GlyphPDF v1.0.0 — an Apache-2.0 PDF workstation for Windows that:

- Runs 100% locally — no telemetry, no cloud upload, no phone-home
- Secure redaction that actually works: closes the Edact-Ray side-channel attack where content-stream glyph-advance deltas can reconstruct redacted text without touching the visual layer. Also scrubs invisible OCR text layers (Tr==3) that other tools miss.
- Digital signatures verified against your local Windows trust store — no external key escrow
- AI assistant works with local Ollama (llama3 default) — keep your documents off cloud APIs
- Open source (Apache-2.0) so you can audit every line

Free download (MSI): https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0

---

## r/degoogle

**Title:** GlyphPDF — local PDF workstation (no Google Docs, no Adobe, no cloud)

**Body:**

For those replacing Google Docs / Adobe Acrobat with local tools — I just released GlyphPDF v1.0.0.

What it does locally without any cloud service:
- PDF editing (text, annotations, forms, watermarks)
- Secure redaction (closes Edact-Ray side-channel)
- Digital signatures (PAdES B-LT/B-LTA, local cert store)
- OCR (Tesseract + RapidOCR + layout detection — all local, all ONNX)
- Compression (30x MRC ratio for scanned pages)
- AI assistant via Ollama (local LLM, no API key)

Apache-2.0. Windows 10/11 x64.

https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0

---

## r/privacytoolsio (if still active / use r/PrivacyGuides)

**Title:** GlyphPDF — open-source, privacy-respecting PDF workstation for Windows

**Body:**

Sharing GlyphPDF v1.0.0 — a local-first open-source PDF editor for Windows. Apache-2.0.

Privacy properties:
- Zero telemetry (no analytics, no crash reporter that phones home)
- Zero cloud dependency — all processing is on-device
- Open source — auditable
- Digital signatures verified against local Windows CertStore
- AI assistant supports local Ollama — no data leaves your machine if you choose

Full feature list and download: https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0
