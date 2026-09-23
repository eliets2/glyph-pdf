GlyphPDF **v1.4.0** — a large feature-parity and hardening release. The theme: features that were advertised but only partially wired are now **real, end-to-end, and test-backed** — plus a substantial dead-code/clarity cleanup pass. 82/82 tests green.

## Downloads
| File | What it is |
|------|-----------|
| **GlyphPDF-1.4.0-x64.msi** | Installer (recommended). Major-upgrades any prior install; Start-menu + desktop shortcuts. |
| **GlyphPDF-1.4.0-x64-portable.zip** | No-install portable edition — unzip anywhere, run `GlyphPDF.exe`. |

Each artifact ships with a matching `.sha256`. Fully self-contained: Qt runtime, PDFium, PoDoFo, ONNX OCR models, tessdata, and veraPDF are bundled — **zero network access required**.

## What's new (now actually works end-to-end)
- **Viewing** — real hyperlink navigation (URI → browser, internal GoTo → page jump); page rotation now rotates the *rendered bitmap*, not just an overlay; two-page mode composites annotations + search highlights; live reload on rotate/resize/Bates/reorder/crop.
- **Editing** — real eraser; Cut/Copy/Delete of the selected object; image Rotate/Replace/Delete.
- **Annotations** — shapes & freehand persist as real `/Square` `/Circle` `/Line` `/Ink` subtypes (no longer invisible on save); consolidated 13-tool markup surface.
- **Redaction** — Mark Region drag-placement and Mark All Occurrences place *real* marks; Apply burns them in; optional Sanitize Document pass bundled into Apply; Clear Marks works.
- **OCR** — Accept persists a searchable MRC PDF/A copy; language selector wired into the engines; page-level orientation detection (0/90/180/270); batch-mode language selection + low-confidence flagging.
- **Forms** — content-aware auto-detect (label/underscore heuristics); Required flag + Tooltip persisted as real PDF metadata (`/Ff`, `/TU`); Calculated fields enabled; silently-dropped values surfaced.
- **Pages** — drag-and-drop thumbnail reorder via an atomic permutation command; honest merge success/failure.
- **Compression** — real JPEG re-encode (Quality/DPI actually decode→downsample→re-encode); duplicate-image dedup; strip-metadata runs the full sanitize scrub; refuses signed documents.
- **Search** — Match Case / Whole Words / Regex now genuinely drive document search.
- **Signatures** — Validate All Signatures surfaces per-signature validity/trust.
- **Compare** — Document Comparison entry point wired.
- **Accessibility** — reading-order mismatches render as jump-able issue rows; ISO 32000-2 §14.7.2 `/Pg` inheritance honored; analysis moved off the UI thread.
- **Security** — link URIs restricted to http/https/mailto; Tools ▸ Set Expiry Date; watermark honors font family + real metrics.

## Quality
- Large dead-code / clarity cleanup: unused members, no-op slots, misleading field names, and stray includes removed across the UI layer.
- Hardening across compression (media-filter chains, degenerate DPI, sanitize depth caps) and redaction (explicit page-list handling).

## ⚠️ Unsigned build
Like prior releases, this build is **not code-signed** (no EV certificate yet). Windows SmartScreen will warn on first launch — choose **More info → Run anyway**. Verify integrity against the published **SHA-256** before running.

## Privacy
100% local. No telemetry, no cloud. Your documents never leave your machine.
