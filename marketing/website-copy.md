# GlyphPDF v1.0.0 — Website / Landing Page Copy

All claims in this document are grounded in README.md and CHANGELOG.md as of v1.0.0.
Do not add any feature not in those files.

---

## Hero Section

### Primary headline (A)
**The PDF workstation that doesn't phone home.**

### Primary headline (B — variant)
**Professional PDF editing. Open source. No subscription.**

### Primary headline (C — variant for security-forward audiences)
**Sign. Redact. Archive. Without trusting a cloud.**

### Sub-headline (pairs with any primary)
GlyphPDF is a full-featured PDF editor for Windows — digital signing, secure
redaction, OCR ensemble, and PDF/A archival. Apache-2.0. Free forever.

### Hero CTA buttons
- Primary: **Download v1.0.0 for Windows** (→ /download)
- Secondary: **View on GitHub** (→ github.com/[ORG]/glyphpdf)

---

## One-liner (for taglines, social bio, README badge)
> Professional PDF workstation — sign, redact, OCR, archive. Apache-2.0. Free forever.

---

## Feature Pillars

### Pillar 1 — Security You Can Verify

**Headline:** Sign with archival-grade PAdES B-LTA. Redact without residue.

**Body:**
GlyphPDF signs documents with PAdES B-LTA digital signatures — the long-term
archival profile that embeds the certificate chain, OCSP responses, and a
trusted timestamp directly into the PDF. Verified against the Windows system
root certificate store. SHA-256 only; no MD5, no SHA-1.

Redaction removes text from the content stream — it is never a black-painted
overlay. The Edact-Ray normalization (Bland et al., PETS 2023) prevents
glyph-advance side-channel recovery: redacted glyphs emit only a numeric gap,
leaving no measurable width that could reconstruct the original characters.

Document sanitization removes 15+ metadata vectors: JavaScript, embedded files,
annotations, hidden layers, and more.

**Proof points:**
- PAdES B-LT/B-LTA with DSS/VRI embedding (ISO 32000 + ETSI EN 319 132)
- Certificate trust chain verified via CertOpenSystemStoreA + X509_VERIFY_PARAM + CRL
- RSA key-size enforcement: keys below 2048 bits are rejected before signing
- OCSP responses verified with OCSP_basic_verify before DSS embedding
- Content-stream excision redaction (PoDoFoBackend::redactCanvasRecursively)
- Edact-Ray glyph-advance normalization — closes PETS 2023 side-channel
- Invisible OCR text layer scrubbed from redaction rectangles (Tr==3 operator)
- AES-256 password encryption + certificate-based multi-recipient encryption
- 15+ sanitization vectors (metadata, JS, embedded files, hidden data)

---

### Pillar 2 — OCR That Reads Like a Human

**Headline:** Two engines. One consensus. Word-level confidence you can see.

**Body:**
GlyphPDF runs Tesseract and PaddleOCR PP-OCRv5 in parallel — one CPU worker
per engine, pipelined across pages so layout detection, OCR, and result fusion
all run concurrently. The engines vote on each word using ROVER word-level
confidence weighting; the highest-confidence interpretation wins.

After OCR, a confidence overlay colors every recognized word: green (≥ 90%),
yellow (70-89%), or red (< 70%). You can right-click any region to re-run OCR
on it individually. OCR output maps to a structured semantic document —
headings, paragraphs, tables, figures — ready for export or annotation.

For scanned archives, the MRC pipeline compresses OCR output into PDF/A-2b:
JBIG2 for the text foreground, JPEG2000 for backgrounds, and an invisible OCR
text sandwich for searchability. Measured compression: **30x** on A4 scanned
content (86 KB vs 2.64 MB raw pixels), verified by veraPDF.

**Proof points:**
- Tesseract 5.5.2 + RapidOCR PP-OCRv5 (real ONNX inference, not a stub)
- Word-level ROVER fusion (correlated error anti-pattern documented + avoided)
- LaneScheduler cross-page pipeline: layout(P+1) || ocr(P) || fusion(P-1)
- Per-word confidence overlay: green / yellow / red heatmap
- Per-region redo via right-click context menu
- MRC compression: JBIG2 (Apache-2.0) foreground + JPEG2000 background
- 30.4x compression ratio measured on synthetic A4 (TestMrcPipeline)
- JBIG2 lossless only — NEVER pattern-matching mode (Xerox 2013 / BSI ban)
- PDF/A-2b output with veraPDF subprocess validation gate

---

### Pillar 3 — A Real Desktop App, Not a Wrapper

**Headline:** C++17 core. Qt 6. No Electron. No cloud dependency.

**Body:**
GlyphPDF is a native Windows desktop application built in C++17 and Qt 6.11,
with a layered static-library architecture that keeps the PDF engine fully
decoupled from the UI. The rendering backend uses PDFium (the same engine
as Chrome) with a three-tier LRU cache (256 MB, tiled rendering, prefetch)
for smooth scrolling through large documents.

The full document interchange format is Djot — a structured semantic layer
that sits alongside the PDF object graph. Annotations, comments, and OCR output
map to a semantic document that round-trips losslessly through GlyphPDF and
interoperates with Acrobat and Foxit via standard /RC XHTML + /Contents
fallback. Signed documents are protected: the ProvenanceGuard prevents semantic
edits from being saved back over a signed PDF, preserving ByteRange integrity.

No telemetry. No account required. No data leaves your machine. The auto-updater
requires explicit consent at every step and verifies downloads by SHA-256.

**Proof points:**
- C++17 + Qt 6.11 + PDFium (BSD-3-Clause, Chrome rendering engine)
- 3-tier LRU render cache: 256 MB, tiled, prefetch with weak_ptr safety
- PoDoFo 0.10.4 for PDF structure; qpdf 12.3.2 for linearization/repair
- Djot dual-model: Structural (PDF object graph) + Semantic (docmodel)
- ProvenanceGuard: refuses Djot save-back on signed/born-PDF documents
- Annotations: /RC XHTML + /Contents plain-text + /PieceInfo Djot sidecar
- Ribbon toolbar: 7 tabs, 40+ wired tools
- Dark / Light / High Contrast themes
- Full keyboard accessibility: F6 region cycling, WCAG High Contrast theme
- Telemetry default OFF (opt-in only, per project non-negotiable)
- Auto-update: SHA-256 verified, user consent at every stage
- MSI installer with WiX v4, file association via OpenWithProgids (non-hijacking)
- MSYS2 ucrt64 single-toolchain build (GCC 16 + Qt 6.11 + all deps via pacman)
- Apache-2.0 license — no GPL/AGPL transitive dependency in the binary

---

## Contribution invite section

**Headline:** Built in the open. Contributions welcome.

**Body:**
GlyphPDF is Apache-2.0. No CLA required — Apache's patent grant (§5) is
included automatically with every contribution. If you find a bug, open an
issue. If you want a feature, open a PR. The architecture is documented in
`docs/` and enforced by tests; new features follow the same layered
interface pattern.

**CTA:** [Contributing Guide] on GitHub — [LINK]

---

## Comparison callouts (for comparison table or "Why GlyphPDF?" section)

| GlyphPDF | Most free PDF editors |
|----------|----------------------|
| Content-stream excision redaction | Black-rectangle overlay |
| PAdES B-LTA (archival signatures) | Basic signature image / no long-term validation |
| Word-level ROVER OCR fusion | Single-engine OCR |
| 30x MRC compression with PDF/A-2b | No compression or lossy JPEG-only |
| Apache-2.0, no telemetry by default | Telemetry on, account required |
| Native C++/Qt, no Electron | Electron or browser-wrapped |

---

## SEO meta tags

```html
<title>GlyphPDF — Professional PDF Workstation for Windows | Free & Open Source</title>

<meta name="description"
  content="GlyphPDF is a free, open-source PDF editor for Windows. PAdES B-LTA
  digital signing, secure redaction, dual-engine OCR with ROVER fusion, PDF/A
  archival, and AES-256 encryption. No telemetry. Apache-2.0. Download free.">

<meta name="keywords"
  content="PDF editor Windows, PDF signing PAdES, secure PDF redaction,
  OCR PDF Windows, PDF/A archival, open source PDF editor, free PDF workstation,
  AES-256 PDF encryption">
```

---

## Open Graph / social preview

```html
<meta property="og:title" content="GlyphPDF — Professional PDF Workstation">
<meta property="og:description"
  content="Sign. Redact. OCR. Archive. Free forever. Apache-2.0.
  The PDF workstation that doesn't phone home.">
<meta property="og:image" content="https://[domain]/og-screenshot.png">
<!-- og:image should be the signing/green-checkmark screenshot from hero shot 03 -->
```

---

## Short copy variants (for HN, Reddit, ProductHunt, GitHub description)

**GitHub repo description (max 350 chars):**
> Professional PDF workstation for Windows. PAdES B-LTA signing, content-stream
> redaction (Edact-Ray normalized), dual-engine OCR (Tesseract + PP-OCRv5, ROVER
> fusion), 30x MRC PDF/A compression, AES-256 encryption. C++17 + Qt 6. Apache-2.0.

**Hacker News Show HN (first comment):**
> Show HN: GlyphPDF — an open-source PDF workstation (C++/Qt, Windows)
>
> I built GlyphPDF because I needed a PDF editor that could do archival-grade
> PAdES B-LTA digital signing, real content-stream redaction (not black overlays),
> and dual-engine OCR on scanned documents — all locally, with no telemetry.
>
> Technical highlights: Edact-Ray glyph-advance normalization to close PETS 2023
> side-channel, 30x MRC compression (JBIG2 + JPEG2000 + OCR sandwich), word-level
> ROVER OCR fusion, and ProvenanceGuard for signed-document integrity. Apache-2.0.
>
> Repo: [LINK]

**ProductHunt tagline:**
> Professional PDF editor. Sign. Redact. OCR. No cloud. No subscription. Apache-2.0.

---

*Copy drafted: 2026-06-02 (M8-P1 D3)*
*Based on: README.md + CHANGELOG.md as of HEAD c454a76*
*Review before publishing: confirm all feature claims still accurate at public launch.*
