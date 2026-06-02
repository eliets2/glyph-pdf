# Twitter / X Thread

---

**Tweet 1 (lead):**
GlyphPDF v1.0.0 is live — open-source PDF workstation for Windows.

No telemetry. No cloud. No subscription. Apache-2.0.

[screenshot: app with a PDF open]

github.com/YOUR_USERNAME/glyphpdf

---

**Tweet 2:**
Most PDF redactors paint a black box over text. That's not secure.

The Edact-Ray attack (Bland et al., PETS 2023) shows glyph-advance deltas in the content stream can reconstruct redacted text without touching the visual layer.

GlyphPDF normalizes all advances and scrubs invisible OCR layers.

---

**Tweet 3:**
Scanned PDFs are huge. GlyphPDF compresses them with MRC:

- JBIG2 lossless foreground (pattern-matching disabled — Xerox 2013)
- JPEG2000 background (configurable: 10x / 30x / 50x)
- Invisible sandwich text from OCR for searchability

Measured: 86 KB vs 2.64 MB raw. PDF/A-2b conformant.

---

**Tweet 4:**
OCR stack:
- PP-DocLayoutV2 layout detection (11 region types, ONNX)
- Tesseract 5.5.2 + RapidOCR PP-OCRv5 dual-engine fanout
- ROVER word-level fusion for multi-engine consensus
- Cross-page pipeline overlaps layout / OCR / fusion in parallel

All local. No cloud API.

---

**Tweet 5:**
Digital signatures: PAdES B-LT/B-LTA with:
- Trust-chain verification against Windows system root store
- OCSP response verification before DSS embedding
- ByteRange overlap detection (PDF shadow attack prevention)
- RSA < 2048 bit key rejection before any output

Real crypto, not cosmetic.

---

**Tweet 6:**
Djot for PDF annotation rich text.

Internal authoring model is Djot. Save path writes:
- /RC XHTML (Acrobat/Foxit interop)
- /Contents plain text (spec fallback)
- /PieceInfo Djot sidecar (perfect GlyphPDF roundtrip)

Comments are threaded with /IRT in-reply-to linking per ISO 32000.

---

**Tweet 7:**
AI assistant with four providers:
- Anthropic Claude 3.5 Sonnet
- OpenAI GPT-4o
- Google Gemini 1.5 Flash
- Ollama local (llama3 — completely offline)

Keys in Windows Credential Manager. Real async calls.

---

**Tweet 8:**
Tech stack:
C++17 / Qt 6.11 (LGPL) / PoDoFo 0.10.4 / PDFium / qpdf 12 / Tesseract 5 / OpenJPEG 2.5.4 / ONNX Runtime / OpenSSL

MSYS2 ucrt64 build. 32 test targets, all green.

Apache-2.0. Contributions welcome.

github.com/YOUR_USERNAME/glyphpdf

---

**Mastodon version (single toot):**
GlyphPDF v1.0.0 is live — open-source PDF workstation for Windows. No telemetry. No cloud. Apache-2.0.

Highlights:
- Edact-Ray secure redaction (closes arXiv 2206.02285 side-channel)
- MRC compression 30x (JBIG2 + JPEG2000 + OCR sandwich text)
- PAdES B-LT/B-LTA signatures with real OCSP verification
- OCR ensemble: PP-DocLayoutV2 + Tesseract + RapidOCR + ROVER
- AI via local Ollama (no cloud required)

https://github.com/YOUR_USERNAME/glyphpdf
