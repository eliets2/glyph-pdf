# HackerNews — Show HN Post

**Title:**
Show HN: GlyphPDF — open-source privacy-first PDF workstation for Windows (Apache-2.0)

---

**Body:**

Hi HN,

I've been building GlyphPDF for the past eight months and today it's finally v1.0.0 and open source.

**The short version:** it's a full PDF editor that runs entirely locally, with no telemetry, no cloud upload, and no subscription. Apache-2.0.

**What makes it different:**

1. **Edact-Ray defense in redaction.** Most PDF redactors black-box over text, but Bland et al. (PETS 2023, arXiv 2206.02285) showed that glyph-advance deltas in the content stream can reconstruct redacted text without touching the visual layer. GlyphPDF normalizes all glyph advances to a numeric TJ gap after redaction, closing this side-channel. Invisible OCR text layers (Tr==3) are also scrubbed, not painted over.

2. **MRC layered compression, 30x ratio.** Scanned PDFs are massive. GlyphPDF uses JBIG2 lossless foreground + JPEG2000 background + invisible OCR sandwich text — same technique as PDF/A-2b compliant archival scanners. Pattern-matching mode in JBIG2 is disabled (Xerox 2013 incident). On a synthetic A4 page: 86 KB vs 2.64 MB raw.

3. **Real OCR ensemble.** PP-DocLayoutV2 layout detection (ONNX, 11 region types) runs in parallel with Tesseract 5 + RapidOCR PP-OCRv5 (full DBNet + SVTR pipeline, not a stub). ROVER word-level fusion combines the two OCR engines. Results feed the MRC sandwich text layer automatically.

4. **PAdES B-LT/B-LTA digital signatures.** Trust-chain verification against Windows system root store. OCSP response verification. ByteRange overlap detection to catch PDF shadow attacks. Weak-key (RSA < 2048) and expired-cert rejection before any output is produced.

5. **Djot document interchange.** The internal document model is dual: structural PDF object graph and semantic SemanticDocument. Annotations use Djot as the authoring model, with /RC XHTML + /Contents plain text + /PieceInfo Djot sidecar for perfect roundtrip plus Acrobat/Foxit interop. ProvenanceGuard blocks save-back to signed born-PDF documents.

**Tech stack:** C++17, Qt 6.11 (LGPL), PoDoFo 0.10.4 (LGPL), PDFium (BSD-3), qpdf 12.3.2 (Apache-2.0), Tesseract 5, OpenJPEG 2.5.4, ONNX Runtime, OpenSSL. MSYS2 ucrt64 build on Windows.

**Things it does NOT do (honest limitations):**
- Remote/cloud signing workflow (deferred to v1.1)
- Real-time collaboration (deferred to v2.0)
- Human-translated UI strings for Arabic/French/German (scaffolding done, commission pending)
- The comment-composer Djot toolbar (the view is threaded, the composer is plain text for now)

GitHub: https://github.com/YOUR_USERNAME/glyphpdf
MSI installer: https://github.com/YOUR_USERNAME/glyphpdf/releases/tag/v1.0.0
Docs: https://glyphpdf.com

Happy to answer questions about the Edact-Ray implementation, the MRC pipeline, or anything else.
