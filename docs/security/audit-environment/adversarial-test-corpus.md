# Adversarial PDF Test Corpus

> **Deliverable:** M7-PROMPT-1 / D4
> **Date:** 2026-06-02
> **Audience:** Third-party security auditor
> **Purpose:** Catalogue of adversarial PDF categories and corpus sources that
> the auditor should generate or obtain for GlyphPDF security testing.
> Cross-reference with threat IDs from `docs/security/auditor-kit/threat-model.md`.

---

## How to use this document

For each category:
1. Obtain or generate samples using the method described.
2. Open in GlyphPDF (ASAN build where possible — see `test-environment-setup.md`).
3. For redaction-specific categories: apply a redaction to a region of the file
   after opening; then inspect the saved PDF with iText RUPS or a hex editor.
4. Record any crash, ASAN report, unexpected behaviour, or redacted-content
   recovery as a candidate finding.

---

## Category 1 — Malformed cross-reference table

**Threat IDs:** T-TMP-1, T-DOS-1
**Risk:** Parser confusion, integer overflow, UAF in PoDoFo xref reconstruction.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 1a | Missing `xref` section entirely (PDF 1.5+ cross-reference stream only, malformed) |
| 1b | xref with negative object offsets |
| 1c | xref entries that point past end of file |
| 1d | Hybrid-reference PDF (both traditional xref and cross-reference stream, contradictory) |
| 1e | xref table with duplicate object numbers |
| 1f | Linearized PDF with corrupt primary hint stream |
| 1g | xref with overflow values (`0xFFFFFFFF` generation number) |

### Sources

- Corkami PDF PoCs: https://github.com/corkami/pocs/tree/master/pdf
  (many xref-malformed samples directly available)
- Generate variants using `qpdf --qdf` to extract and hand-mutate.
- Radamsa mutation of clean PDFs: `radamsa clean.pdf > mutated.pdf`

---

## Category 2 — Circular and infinitely recursive references

**Threat IDs:** T-TMP-1, T-DOS-1
**Risk:** Stack overflow in recursive object resolution (PoDoFo object graph
traversal); infinite loop in GlyphPDF's `redactCanvasRecursively` form-XObject
recursion.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 2a | Catalog → Pages → Page → Resources → XObject → (back to Page) |
| 2b | Form XObject that references itself via `/Do` |
| 2c | Form XObject chain A → B → C → A |
| 2d | Named-destinations dictionary with circular reference |
| 2e | Deeply nested array: array of arrays, 1000 levels deep |
| 2f | Deeply nested dict: dict of dicts, 1000 levels deep |

### Generation

```python
# Minimal circular XObject (pseudocode)
# Object 5: Form XObject that invokes itself
# 5 0 obj << /Type /XObject /Subtype /Form /Resources << /XObject << /F5 5 0 R >> >> >> stream
# /F5 Do
# endstream endobj
```

Hand-craft with a PDF hex editor or use Python's `fpdf2` / `pypdf` for object
construction.

**Note for redaction:** The circular form-XObject case directly exercises the
recursion guard (or lack thereof) in `redactCanvasRecursively`. Confirm the
application does not stack-overflow when the user attempts to redact a page
containing a self-referential XObject.

---

## Category 3 — Deeply nested content streams

**Threat IDs:** T-TMP-1, T-DOS-1
**Risk:** Stack exhaustion or combinatorial explosion in the content-stream
parser (`redactCanvasRecursively`) when it recurses into nested form XObjects.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 3a | Form XObject nesting 50 levels deep (each calls the next via `/Do`) |
| 3b | Content stream with 100,000 text show operators (`Tj`) on one page |
| 3c | Page with 10,000 tiny form XObjects each containing a `Tj` |
| 3d | Content stream with a single `TJ` array containing 50,000 elements |
| 3e | Page array (`/Contents`) with 1,000 stream references |

### Generation

Use `fpdf2` or a minimal C++ PDF writer to construct these programmatically.

---

## Category 4 — Polyglot files

**Threat IDs:** T-TMP-1, T-EOP-1
**Risk:** Parser confusion at file type boundaries; DLL confusion if GlyphPDF
inspects file headers incorrectly before routing to a parser.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 4a | PDF+ZIP (central directory appended after `%%EOF`) |
| 4b | PDF+PE (Windows executable embedded after `%%EOF`) |
| 4c | PDF with a valid JPEG magic bytes at offset 0, PDF header at offset 8 |
| 4d | PDF+HTML (valid HTML page before `%PDF-1.x`) |
| 4e | PDF with a null byte at offset 0 (binary content check in `applyRedactions`) |

### Sources

- Corkami polyglot collection:
  https://github.com/corkami/pocs/tree/master/pdf
- PolyFile tool: https://github.com/trailofbits/polyfile

---

## Category 5 — Crafted JBIG2 streams (future / MRC)

**Threat IDs:** T-TMP-1 (future — relevant when MRC ships)
**Risk:** JBIG2 has a history of complex parser vulnerabilities (CVE-2023-3019
in Ghostscript; Evince historical issues). Although JBIG2 decoding is currently
delegated to PDFium / PoDoFo's filter stack, crafted JBIG2 streams should be
in the fuzzing corpus for completeness.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 5a | JBIG2 stream with malformed segment headers |
| 5b | JBIG2 arithmetic-coded stream with invalid probability table |
| 5c | JBIG2 with claimed height/width larger than actual data |

### Source

- Google Clusterfuzz JBIG2 corpus (public): search GCS `clusterfuzz-builds`
  buckets for JBIG2 seeds.
- Mutate clean JBIG2 streams from PDF/A archive samples using Radamsa.

---

## Category 6 — Inline image bombs

**Threat IDs:** T-TMP-1, T-DOS-1
**Risk:** `applyRedactions` in `PoDoFoBackend.cpp` aborts on inline images (the
`hasInlineImage` path at lines ~1319-1343 of the source) and returns `false`
to prevent insecure visual-only overlay. Test that this abort is reliable and
not bypassable by varying the `ID`/`EI` marker placement.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 6a | Inline image with 1x1 pixel but declared dimensions 32768x32768 |
| 6b | Inline image with FlateDecode claiming decompressed size 1 GB |
| 6c | Inline image where `ID` appears in a string literal (false positive test) |
| 6d | Inline image with `ID` preceded by a space (tests the `" ID "` detection) |
| 6e | `BI ... ID ... EI` with zero-length data |
| 6f | Inline image with no `EI` marker (truncated stream) |
| 6g | Multiple inline images on one page: first handled correctly, second triggers abort |

### Generation

Construct minimal PDFs with hand-crafted content streams:
```
%PDF-1.4
1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj
2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj
3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792]
  /Contents 4 0 R >> endobj
4 0 obj << /Length 50 >>
stream
BI /W 32768 /H 32768 /CS /RGB /BPC 8
ID
EI
endstream
endobj
```

---

## Category 7 — JavaScript-containing PDFs (sanitization bypass attempt)

**Threat IDs:** T-SAN-1, T-EOP-3
**Risk:** The sanitizer strips `/JavaScript`, `/OpenAction`, and `/AA`. Verify
completeness by presenting PDFs where JS is embedded through indirect paths not
covered by the sanitizer's explicit dictionary-key walk.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 7a | JS in `/OpenAction` with `/Action /JavaScript` |
| 7b | JS in page `/AA` (additional-actions: `/O`, `/C`, `/E`, `/X`, `/Fo`, `/Bl`) |
| 7c | JS in form field `/AA` (text field with keystroke validation JS) |
| 7d | JS triggered by annotation (`/A` with `/S /JavaScript`) |
| 7e | JS in named actions via `/Names >> /JavaScript` dictionary |
| 7f | `RichMedia` annotation with embedded script (PDF 2.0) |
| 7g | JS stored as an indirect object referenced only from a non-top-level dict |

**Expected behavior:** After sanitization, opening the resulting PDF in
GlyphPDF (or any reader) must not execute JavaScript. Verify with a PDF viewer
that reports JS execution (Adobe Acrobat with JS enabled; or a JS-capable
reader in a sandboxed environment).

---

## Category 8 — Overlapping annotation rectangles

**Threat IDs:** T-SAN-1, T-RED-2
**Risk:** Annotations with `/Rect` values that overlap other content regions,
or that are designed to visually cover redacted text with appearance streams
that reproduce the redacted text (redaction bypass via annotation overlay).

### Variants to test

| Variant | Description |
| :-- | :-- |
| 8a | FreeText annotation whose appearance stream (`/AP /N`) reproduces the redacted text |
| 8b | Markup annotation with `/RC` (rich content) containing the redacted text |
| 8c | Widget annotation (form field) whose `/V` value contains the redacted text |
| 8d | Stamp annotation with an image XObject that reproduces the redacted image |
| 8e | Annotation with `/Rect [0 0 0 0]` (zero-area, hidden — survives sanitize?) |
| 8f | Annotation whose `/Contents` field holds the redacted text verbatim |
| 8g | Two overlapping FreeText annotations; outer one drawn over redaction box |

### Verification method

After applying redaction and sanitization, open the resulting PDF in iText RUPS
or `qpdf --qdf` and manually inspect all annotation dictionaries for surviving
sensitive text. Check `/Contents`, `/RC`, `/AP`, `/V` fields.

---

## Category 9 — Metadata and hidden-data survival

**Threat IDs:** T-SAN-1, T-RED-3
**Risk:** Residual sensitive data surviving sanitization through paths the
sanitizer does not walk.

### Variants to test

| Variant | Description |
| :-- | :-- |
| 9a | EXIF metadata embedded in an image XObject (JPEG/TIFF in PDF) |
| 9b | XMP metadata in an image stream (`/Metadata` on an image XObject) |
| 9c | ICC color profile stream with embedded author info |
| 9d | Producer / Creator string in the trailer `Info` dict (should be stripped) |
| 9e | `PieceInfo` (private application data) with sensitive payload |
| 9f | Form field `/V` value containing sensitive text (not cleared by sanitize) |
| 9g | `ActualText` on a struct element that was NOT in the redacted MCIDs |
| 9h | `Alt` text on a figure XObject |
| 9i | Outlines (`/Outlines`) with titles that reproduce document section names |
| 9j | Thumbnail image (`/Thumb`) on a page containing a redacted region |

---

## Category 10 — Crafted signing and encryption edge cases

**Threat IDs:** T-SPF-1, T-SPF-2, T-TMP-2

### Variants to test

| Variant | Description |
| :-- | :-- |
| 10a | PDF with a `/ByteRange [0 N M L]` that does NOT cover the whole file |
| 10b | PDF with incremental update appended after signature (post-sign modification) |
| 10c | PDF with two signatures where the second signature's ByteRange covers the first |
| 10d | PDF encrypted with an unknown `/V` (encryption version) value |
| 10e | PDF with `/R 6` (AES-256) but `/V 4` (inconsistent) |
| 10f | Empty password with a non-empty owner password (owner-only protection) |
| 10g | Certificate-encrypted PDF with a certificate whose key is 512-bit RSA (weak) |
| 10h | PDF with a PKCS#7 signature container but no actual signature bytes |

---

## Corpus source summary

| Source | URL | Priority |
| :-- | :-- | :-- |
| Corkami PDF PoCs | https://github.com/corkami/pocs/tree/master/pdf | High — immediate use |
| PDF.js test suite | https://github.com/mozilla/pdf.js/tree/master/test/pdfs | High — broad coverage |
| pdfium test data | https://pdfium.googlesource.com/pdfium/+/refs/heads/main/testing/ | High — edge cases |
| SafeDocs corpus | https://www.pdfa.org/safedocs/ (registration) | Medium — real-world PDFs |
| PolyFile tool | https://github.com/trailofbits/polyfile | Medium — polyglot generation |
| Radamsa mutation | https://gitlab.com/akihe/radamsa | High — mutation fuzzing |
| AFL++ dictionary | `dictionaries/pdf.dict` in AFL++ source | Medium — fuzzer seeds |

---

## Mutation strategy

For fuzzing harnesses, seed the corpus with 50–100 real PDFs from the above
sources, then apply structure-aware mutation:

1. **Radamsa** for raw byte mutation: `for i in $(seq 100); do radamsa seed.pdf > corpus/m$i.pdf; done`
2. **AFL++ `pdf.dict`** for dictionary-guided mutations of token sequences.
3. **Hand-crafted minimal PDFs** for each category above — these exercise
   specific parser paths that random mutation may not reach.
4. **Cross-seed with ASAN build** — any input that causes an ASAN report is a
   high-value seed; add it to the corpus immediately.
