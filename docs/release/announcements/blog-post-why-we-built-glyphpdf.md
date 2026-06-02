# Why We Built GlyphPDF

*Published 2026-06-02 — the day v1.0.0 ships.*

---

## The problem with "secure" PDF tools

In 2022, Bland et al. published "Edact-Ray: Uncovering PDF Redaction Flaws" (arXiv 2206.02285,
presented at PETS 2023). Their finding: when a PDF editor redacts text by overpainting it with a
black rectangle, the original text can often still be recovered — not by lifting the paint, but
by analysing the glyph-advance deltas that the PDF content stream still contains. The cursor
positions that the PDF renderer uses to lay out characters create a fingerprint of the underlying
text even when the glyphs themselves are removed.

This was not a new attack. Court documents, government reports, and legal filings had been leaking
redacted content this way for years before anyone formally named the attack.

Every commercial PDF editor we evaluated — Adobe Acrobat, Foxit PhantomPDF, Nitro PDF — was
vulnerable. Most free tools were worse.

We built GlyphPDF to fix this. The redaction engine in GlyphPDF:

1. Removes glyph operators from the content stream (does not overpaint them).
2. Replaces every removed glyph's advance width with a numeric-only `[N] TJ` gap — the exact
   sum of the original advances, in units, with no identity information.
3. Scrubs invisible text (Tr==3 rendering mode) separately — OCR sandwich layers in scanned
   PDFs use Tr==3 to position invisible text over rendered glyphs. That invisible text is
   equally redactable by a black rectangle paint, and equally recoverable from the content
   stream without one.

This is not a feature we bolted on. It was the reason GlyphPDF exists.

---

## Why we chose Apache-2.0

The PDF security community suffers from a fragmentation problem. Research tools are one-off
Python scripts. Commercial tools are closed. Open-source tools tend to be GPL, which limits
adoption in enterprise environments where legal review of copyleft provisions can take months.

Apache-2.0 means GlyphPDF can be:
- Embedded in commercial products without license negotiation
- Forked by governments and regulated industries that cannot use copyleft software
- Audited publicly — which is the only way to actually trust a security tool

We retained LGPL dependencies (Qt, PoDoFo) at arm's length via dynamic linking. The GlyphPDF
application code itself is Apache-2.0.

---

## The three workstreams

GlyphPDF shipped three major technical workstreams in v1.0.0:

### WS1 — Parallel OCR ensemble

The first workstream was infrastructure: a LaneScheduler with a GPU lane (single warm QThread,
bounded by QSemaphore to prevent page-spawn storms) and a CPU lane (QThreadPool, isolated from
Qt's global pool). On top of that: PP-DocLayoutV2 layout detection via ONNX Runtime, Tesseract 5
and RapidOCR PP-OCRv5 in parallel per region, and ROVER word-level fusion that reconciles the two
OCR engines' output.

The key constraint: every inference session is warm (loaded once at startup, reused per page).
This is the single biggest performance factor in OCR applications. Per-page session creation costs
hundreds of milliseconds in model warm-up overhead that accumulates linearly with document length.

Cross-page pipelining overlaps `stage1(P+1) || stage2(P) || stage3(P-1)` with a backpressure
semaphore so that layout detection for the next page runs while OCR runs on the current page and
fusion runs on the previous one.

### WS2 — Djot document interchange

The second workstream was the semantic document model. PDFs are structurally opaque: they describe
where to paint glyphs, not what those glyphs mean. To do anything semantic — extract structure,
produce accessible output, round-trip through an editor — you need a parallel representation.

We chose Djot as the interchange format because:
- It is simpler than Markdown (unambiguous grammar)
- It has a reference parser (Lua 5.4, MIT) we could vendor without reimplementing the grammar
- It is expressive enough for annotation rich text (bold/italic/code/link/list/heading) while
  being parseable with a simple recursive descent

The hardest part was the annotation save path. PDF annotations carry rich text in /RC (XHTML,
ISO 32000-1 §12.5.6.4), a plain-text fallback in /Contents, and we added a /PieceInfo Djot
sidecar. The triple-write ensures interoperability with Acrobat and Foxit (they read /RC) while
preserving GlyphPDF's authoring model for a perfect roundtrip.

ProvenanceGuard is the safety catch: it refuses Djot save-back to signed born-PDF documents
because editing the content stream would invalidate the signature's ByteRange coverage. The user
gets a "Save as copy" route instead.

### WS3 — MRC layered compression

The third workstream was compression. MRC (Mixed Raster Content) is the ISO 16485 standard for
layered scanned-page compression: separate foreground (text, line art) from background (photos,
shading), compress each with the optimal codec, and sandwich an invisible OCR text layer in between.

GlyphPDF's implementation:
- **Foreground:** JBIG2 lossless generic-region via agl/jbig2enc (Apache-2.0). Pattern-matching
  mode is disabled unconditionally. The Xerox 2013 incident, where JBIG2 pattern substitution
  silently altered document content, and the subsequent German BSI guidance, make
  pattern-matching mode unacceptable in a document workstation regardless of compression gains.
- **Background:** JPEG2000 via OpenJPEG 2.5.4 (BSD-2-Clause, already in MSYS2 ucrt64). Three
  quality tiers: Lossless (10:1), Balanced (30:1), Aggressive (50:1).
- **Sandwich text:** The word-box output of WS1 OCR, with each word's PDF coordinate provenance
  encoded in Djot attributes, is placed as an invisible Tr==3 text layer so the output is
  searchable and copy-pasteable.
- **PDF/A-2b conformance:** The raw PDF writer emits a proper XMP metadata stream, sRGB
  OutputIntent, and the veraPDF CLI gate validates before finalization.

Measured on a synthetic A4 page: 86 KB compressed vs 2.64 MB raw — a 30.44x reduction.

---

## What we got wrong (and fixed)

**Pattern-matching in JBIG2.** We shipped jbig2enc with the pattern-matching flag as a
configurable option in the first internal build. We removed it after reviewing the BSI guidance
more carefully. The compression gain (maybe 20-30% on certain documents) is not worth the risk
of silent content alteration. It is now unconditionally disabled with a comment in the code.

**VRI key computation.** The initial PAdES B-LT implementation computed the VRI dictionary key
as a hex string of the signer certificate's serial number. ETSI EN 319 132 specifies the key
as the SHA-1 of the raw /Contents bytes. This was a spec non-conformance that would have
caused interoperability failures with strict PAdES validators. Fixed before v1.0.0 shipped.

**OCSP verification.** We initially embedded OCSP responses in the DSS /OCSPs array without
verifying them first. An unverified OCSP response that claims a certificate is valid but is
itself forged or stale provides no security guarantee. We added `OCSP_basic_verify` with the
responder's certificate before any OCSP response is accepted for embedding.

**The RapidOCR stub.** The internal v1.0.0-internal build had a RapidOCR stub that returned
synthetic word boxes. We shipped a real DBNet detect → orientation classifier → perspective warp
→ SVTR recognition → CTC decode pipeline against genuine PP-OCRv5 mobile ONNX weights before the
public release. A `isMockImplementation()` guard enforces this — if it ever returns true in a
non-test context, that is a bug.

---

## What's next

v1.0.0 ships the core. The honest backlog:

- **v1.1:** Remote signing workflow (signing order, reminders, audit trails). Installer dialog
  branding. Missing-recent-files pruning. Richer Djot comment composer toolbar.
- **v1.x:** Human-translated Arabic/French/German UI (commissioning in progress). macOS and Linux
  ports (Qt is cross-platform; the PDFium and MSYS2 packaging are not yet).
- **v2.0:** Real-time collaboration. Cloud-optional sync for teams who want it.

The security roadmap continues. The Edact-Ray defense is necessary but not sufficient. We plan
to add font-subsetting analysis (glyph-presence side-channel for short redacted words), metadata
stripping UI that surfaces every metadata field rather than silently removing them, and a
formal PAdES conformance test suite using the PDF Association's reference validator.

---

*GlyphPDF is Apache-2.0. The source is at https://github.com/YOUR_USERNAME/glyphpdf.
If you find a security issue, please follow SECURITY.md — responsible disclosure, 90-day window.*
