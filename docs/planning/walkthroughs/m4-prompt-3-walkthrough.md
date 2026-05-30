# M4-PROMPT-3 Walkthrough — Convert Tools (ToHtml, ToText, Compress, PDF→PPT)

**Date:** 2026-05-29
**Result:** Shipped (subagent + additional PPTX commit)
**Commits:** b38217b (Convert Tools) · 0e364cb (merge) · 66ca5ba (PPTX structural export)

---

## COMPLETED

### D1 — ToHtml tool wired
- `ToolMode::ExportToHtml` handler calls `IConversionEngine::convertTo(path, outputDir, Format::Html)`.
- File-save dialog defaults to output dir + stem + `.html`.

### D2 — ToText tool wired
- `ToolMode::ExportToText` handler calls `IConversionEngine::convertTo(path, outputDir, Format::Text)`.

### D3 — Compress tool wired
- `ToolMode::Compress` handler opens `CompressDialog` (quality slider + DPI select).
- Calls `IPdfEditorEngine::optimizeDocument(output, CompressionOptions)`.

### D4 — PDF→PPT export (PPTX structural export) — commit 66ca5ba
- New `IPdfEditorEngine::exportToPowerPoint(inputPath, outputPath)` method.
- PPTX generation: each PDF page → one slide; page text extracted via PDFium; slide background set to PDF page thumbnail image.
- File format: Open XML `.pptx` written via the `pptx-writer` helper (minimal Office XML without OpenXML SDK).
- `ToolMode::ExportToPpt` handler calls the new method.

---

## DEFERRED

None.

---

## Notes

- PPTX export (D4) landed separately in 66ca5ba as an additional commit beyond the initial b38217b. The initial commit wired D1-D3; 66ca5ba added D4.
- Engine methods `convertTo` + `optimizeDocument` were pre-existing; exportToPowerPoint is new code added in this sprint.
- No walkthrough was produced at execution time (Pattern 11). Reconstructed from commit history.

---

## CHANGELOG confirmation

Covered under `[Unreleased]` "23 ribbon tools wired" scope. PDF→PPT noted as "Office→PDF import + PDF→PPT export" in the scope description.
