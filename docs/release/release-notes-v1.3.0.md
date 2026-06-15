# GlyphPDF v1.3.0 — Feature Release

**Release date:** 2026-06-15
**Type:** Minor feature release (backward-compatible with v1.2.x)

---

## Summary

v1.3.0 closes nine PRD gaps across annotation, forms, batch automation, document
comparison, secure sharing, and accessibility. It builds directly on the v1.2.1
stability fix and the v1.2.0 security hardening — no regressions to existing
workflows.

---

## What's New

### Annotation & navigation
- **OCR Verify screen restored** to the screen navigation strip — the dedicated
  confidence-review screen, hidden since v1.0.0, is wired again. (§9.3)
- **Stamp, Callout, and Erase** annotation tools promoted from planned to live.
  Stamp and Callout are backed by the existing canvas annotation modes; comment
  annotations now carry a **file-attachment** field. (§9.3)

### Forms
- **Calculated form field** — a 10th AcroForm field type. Drops a read-only text
  field whose value is computed by a JavaScript/AcroForm calculation
  (`/AA /C` action), registered in the AcroForm `/CO` calculation-order array.
  Drive it from Form Builder's new *Calculated* type with an expression input. (§9.6)

### Batch processing
- **Batch OCR, Merge, and Redact** are no longer stubs:
  - *Merge* combines all input files into a single `<first>_merged.pdf`.
  - *OCR* renders each page and writes a searchable MRC PDF/A.
  - *Redact* excises comma-separated regex patterns and flattens the result.
- **Hot folder watching** — watch a directory and auto-ingest new PDFs (debounced),
  with an optional auto-run on arrival. (§9.12)

### Compare
- **Export comparison reports** to a self-contained HTML report (two-column
  removed/added table with summary header) or a plain-text unified diff. (§9.10)
- **Page reorder detection** — whole pages that moved position are detected via a
  page-level LCS over page fingerprints and reported as
  "Page N moved to position M". (§9.10)

### Security & sharing
- **Secure sharing** — share a document as an AES-256 encrypted ZIP package
  (in addition to the existing email attachment path). (§9.11)
- **Document expiry** — embed an expiry date in the PDF's XMP metadata
  (`glyph:ExpiryDate`); expired documents open in read-only mode with a warning. (§9.11)

### Accessibility
- **Tagged-PDF reading-order check** in the PDF/A panel — walks the structure tree
  and flags elements whose structural order diverges from their visual position. (§9.14)

---

## Notes & limitations

- **Encrypted package** sharing uses a 7-Zip executable (PATH, standard install
  location, or bundled). When 7-Zip is unavailable, the action explains how to
  proceed rather than failing silently.
- **Annotation Erase** is exposed in the ribbon but its canvas hit-testing is not
  yet implemented; selecting it shows an explicit "not yet implemented" notice.

---

## Installation

### Installer

1. Download `GlyphPDF-1.3.0-x64.msi`
2. Verify SHA-256:
   ```powershell
   (Get-FileHash "GlyphPDF-1.3.0-x64.msi" -Algorithm SHA256).Hash
   # Expected: B7859F33C78917BAFD6866818A1C44BFF66960369E90FDAAF32C7ED21821E4F1
   ```
3. Run the installer — upgrades in-place from v1.0.x and v1.2.x.

### Portable

Download `GlyphPDF-1.3.0-x64-portable.zip`, verify its SHA-256, and extract.

---

## Files

| File | SHA-256 |
|------|---------|
| `GlyphPDF-1.3.0-x64.msi` | `B7859F33C78917BAFD6866818A1C44BFF66960369E90FDAAF32C7ED21821E4F1` |
| `GlyphPDF-1.3.0-x64-portable.zip` | `9671BEE77C572CA6588B0925FE2AE8ED4864124DB0D703B9F85E9CC15C211B27` |

---

## Included from prior releases

- All v1.2.1 stability fixes (startup-crash / podofo DLL staging).
- All v1.2.0 security fixes (SECFIX-1 through SECFIX-5).
