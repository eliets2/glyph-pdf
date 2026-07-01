# GlyphPDF Fix-and-Elevate Implementation Plan (2026-07-01)

Source: `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md`. That report scored 16 already-shipped
feature domains against Adobe Acrobat/Foxit/DocuSign/etc and found the gap is mostly **dead,
disconnected, or lying code**, not missing features. This plan turns that report's P0/P1 list into
an executable sequence.

## Objective

- **Phase 1** — fix every dead/disconnected/lying-code defect (P0). Nothing here requires new
  product decisions; it's making already-built code actually reachable and correct.
- **Phase 2** — close the named feature-parity gaps against best-in-class competitors (P1). This is
  the direct "comparison to best-in-class" work: real capability additions, not bug fixes.
- **Phase 3 (optional, parallel track)** — P2 polish + the cross-cutting privacy/offline marketing
  bundle (every domain flagged one; batch them into a single push instead of 16 piecemeal ones).

## Execution mechanics

- This is a real C++17/Qt6/CMake codebase with a 39-target ctest suite (currently 100% green,
  per project memory). Every work package must **build clean and pass full ctest** before merging —
  no item is "done" without that gate.
- **One git worktree per domain**, not per item. Items within a domain usually touch the same
  files (e.g. §9.9 Page Management's thumbnail-DnD, merge-error-surfacing, and reorder-command
  consolidation all touch the page-management/reorder-command area) — batching by domain avoids
  merge conflicts and repeated context-loading.
- Merge each domain's branch onto an integration branch (`feature-elevation` off `audit-remediation`
  or `main`), rebuild + full ctest after each merge, then proceed to the next wave.
- Domains with no file overlap run fully in parallel within a wave. The two known cross-domain
  overlaps to sequence, not parallelize: §9.2 Editing and §9.4 OCR both touch `EditController`;
  §9.8 Redaction and §9.13 Compression both call into `sanitizeDocument()`.
- Add new ctest coverage for any previously-untested path touched by a fix (the audit flagged
  several: OOXML conversion paths, Bates/header-footer/crop/resize/merge, DiffEngine integration,
  reading-order analysis) — don't just fix the bug, close the test gap that let it ship.

## Suggested first three (highest value per unit of effort, per the audit's own recommendation)

1. §9.10 — wire `CompareMode::compareFiles()` to a real menu entry (S). Unlocks a fully-built,
   already-tested feature that is currently 100% unreachable.
2. §9.4 — wire the language selector into the OCR engines, and route Accept through `exportMrcPdfA`
   so OCR text actually persists on save (S/M). Fixes the headline OCR promise silently failing.
3. §9.13 — real JPEG re-encoding for the Quality/DPI controls (L, biggest single lift, but the
   worst-scored domain in the audit at 2.5/10).

---

## PHASE 1 — Fix the dead/disconnected/lying code (P0)

### Wave 1A — Quick wins (S-effort, isolated wiring — no design decisions)

| Domain | Item |
|---|---|
| §9.2 Editing | Wire Image Rotate to UI (drag handle + right-click Rotate) |
| §9.2 Editing | Wire Image Delete and Image Replace to UI |
| §9.3 Annotation | Fix/remove false "file attachment" CHANGELOG claim; TODO-mark dead `attachmentPath` |
| §9.3 Annotation | Consolidate the two annotation toolbars (add missing Strikeout/Squiggly/Stamp/Callout buttons or an overflow) |
| §9.4 OCR | Wire the existing language selector into Tesseract + RapidOCR (`EditController::runOcr()` hardcodes "eng") |
| §9.5 Conversion | Add automated tests for the real-OOXML code paths |
| §9.5 Conversion | Fix PPTX text-overlay opacity (solid black instead of intended low-alpha) |
| §9.6 Forms | Fix `fillForm`'s unintended `SetReadOnly` side effect on default-value edits |
| §9.6 Forms | Make Radio/PushButton `fillForm` no-op visible instead of a silent `qDebug` skip |
| §9.6 Forms | Move Calculated field into the standard Forms ribbon/menu (remove MenuBar.cpp hard-disable) |
| §9.7 E-signatures | Add on-page validity badge overlay + "Validate All Signatures" action (data already computed) |
| §9.8 Redaction | Wire "Clear Marks" to actually remove placed redaction marks |
| §9.9 Page Mgmt | Surface real merge success/failure instead of always reporting "Successfully merged" |
| §9.9 Page Mgmt | Consolidate the two parallel reorder commands into one (retire the legacy single-swap path) |
| §9.10 Compare | **Wire `CompareMode::compareFiles()` into a real file-picker entry point** — the blocker |
| §9.11 Security | Fix `addTextWatermark` to honor `fontFamily` instead of hardcoded Helvetica |
| §9.11 Security | Replace watermark text-centering heuristic with real font metrics |
| §9.11 Security | Wire `setExpiryDate` into the UI (date picker → already-implemented engine method) |
| §9.12 Batch | Collapse multi-pattern batch redaction into one load/redact/save pass per file |
| §9.13 Compression | Reuse existing `sanitizeDocument()` in the Compress flow instead of a thin subset |
| §9.13 Compression | Fix/remove size-estimate heuristics that don't match what the write path actually does |
| §9.14 Accessibility | Fix page-inheritance walk-up in `pageIndexOf` (ISO 32000-2 §14.7.2) — kills false-positive reorder flags |
| §9.14 Accessibility | Add jump-to-page/highlight-on-click for each reported mismatch |
| §9.15 Search | Wire the dead thumbnail zoom +/- buttons (missing `connect()` calls) |
| §9.16 Import/Export | Add a runtime badge showing which export path was actually used (real OOXML vs fallback) |
| §9.16 Import/Export | Remove or wire up the dead "linearized" export preset checkbox (qpdf already linked) |

*(Marketing-flavored P0 items — "market the local-first X advantage" bullets in §9.8/§9.9/§9.16 — are
folded into Phase 3, not Wave 1A, since they're copy/positioning work, not code fixes.)*

### Wave 1B — Correctness fixes (M-effort — real logic/data-model changes)

| Domain | Item |
|---|---|
| §9.1 Viewing | Fix page-rotation to rotate the actual rendered bitmap, not just the annotation overlay |
| §9.1 Viewing | Add real hyperlink (URI + internal GoTo) click-navigation |
| §9.2 Editing | Ship minimal Cut/Copy/Delete for the currently-selected object |
| §9.2 Editing | Replace the eraser placeholder with a real erase via the existing `deleteObjectAt` pipeline |
| §9.3 Annotation | Persist Shapes and Freehand ink as real PDF annotation subtypes (`/Square /Circle /Line /Ink`) — highest-severity single finding in the audit |
| §9.4 OCR | Make interactive Run OCR → Accept call `exportMrcPdfA` so the searchable layer actually saves |
| §9.4 OCR | Implement page-level orientation detection (Tesseract OSD, already linked) |
| §9.5 Conversion | Land real OOXML dependencies (OpenXLSX + duckx) in the shipped build; verify `HAS_DUCKX`/`HAS_OPENXLSX` branches are taken |
| §9.6 Forms | Persist Required flag and Tooltip as real PDF metadata (`/Ff` bit, `/TU`) |
| §9.6 Forms | Give form auto-detect a real content-aware heuristic, or clearly relabel as experimental (currently returns 3 hardcoded dummy fields always) |
| §9.7 E-signatures | Add typed-font and image-upload modes to the signature tool (currently draw-only) |
| §9.7 E-signatures | Build a signature appearance/design step for the cryptographic signing path (currently invisible, no appearance stream) |
| §9.8 Redaction | Expose "Mark Region"/"Mark All Occurrences" inside RedactMode itself (currently split across disconnected subsystems) |
| §9.8 Redaction | Bundle an optional "Sanitize Document" pass into the redaction Apply flow |
| §9.9 Page Mgmt | Enable drag-and-drop reordering directly on the page thumbnail grid (remove `NoDragDrop`) |
| §9.10 Compare | Add a change-type filter to the CHANGES tree/text-diff panel |
| §9.12 Batch | Fix engine-wide mutex false parallelism (5 of 7 "parallel" ops are secretly serialized) |
| §9.12 Batch | Add low-confidence-word highlighting/flagging to batch OCR output |
| §9.12 Batch | Expose OCR language selection in the Batch UI |
| §9.13 Compression | Finish the duplicate-image dedup rewiring (XObject-reference rewiring + object removal — "last mile" only) |
| §9.14 Accessibility | Move `analyzeReadingOrder` off the UI thread (reuse the QFutureWatcher pattern already used by the PDF/A validator nearby) |
| §9.15 Search | Wire Match Case / Whole Words / Use Regex checkboxes into `searchDocument()` |

### Wave 1C — Big lifts (L-effort P0)

| Domain | Item |
|---|---|
| §9.1 Viewing | Make two-page mode a true continuous live layout (keep `QPdfView`/`AnnotationLayer` active instead of static spread images) |
| §9.13 Compression | Real JPEG re-encoding for the Quality/DPI controls (decode/downsample/re-encode `DCTDecode` streams) |

---

## PHASE 2 — Close the best-in-class comparison gaps (P1)

### Wave 2A — Quick parity wins (S-effort)

| Domain | Item |
|---|---|
| §9.4 OCR | Make "Re-OCR this region" actually region-scoped (currently re-runs the whole page) |
| §9.4 OCR | Surface which binarization/deskew path is active in OCR Verify UI |
| §9.4 OCR | Add test coverage for language pass-through + orientation correction |
| §9.5 Conversion | Bring batch format list to parity with the single-document ConvertController |
| §9.5 Conversion | Make batch Compress use the full DPI+quality preset pairing instead of hardcoded DPI=150 |
| §9.7 E-signatures | Add an Initials variant of the signature tool |
| §9.7 E-signatures | Cache/reuse the adopted signature across placements in a session |
| §9.7 E-signatures | Surface `SignOutcome` degradation as a user-visible dialog at signing time |
| §9.8 Redaction | Add a Cancel/Back control to RedactMode's panel |
| §9.8 Redaction | Add optional overlay text/redaction reason codes on the black box |
| §9.8 Redaction | Support multi-term/word-list import for pattern redaction |
| §9.8 Redaction | Add per-file confirmation before BatchMode `OpRedact` silently overwrites existing outputs |
| §9.8 Redaction | Upgrade the opt-in audit log to record which pattern/category matched |
| §9.9 Page Mgmt | Move Merge under the Pages/Organize tool group (or cross-link it) |
| §9.10 Compare | Generate explicit rows for pages added/removed entirely when page counts differ |
| §9.12 Batch | Add named redaction pattern presets (PII quick-picks) |
| §9.12 Batch | Make Compress/Optimize target DPI user-configurable with named presets |
| §9.13 Compression | Add a real post-compression before/after size readout |
| §9.14 Accessibility | Replace the ">2 position slots" heuristic with a named, documented threshold |
| §9.14 Accessibility | Add automated unit tests for `analyzeReadingOrder`/`collectStructElems` |
| §9.16 Import/Export | Add automated round-trip tests for bookmark/hyperlink preservation on save |

### Wave 2B — Feature-parity additions (M-effort)

| Domain | Item |
|---|---|
| §9.1 Viewing | Add real content-level Night Mode (color inversion), distinct from chrome Dark Mode |
| §9.2 Editing | Add opacity control to inline text-edit and image-edit toolbars (reuse watermark's ExtGState code) |
| §9.2 Editing | Add letter-spacing and line-spacing controls to EditToolBar |
| §9.3 Annotation | Ship a small predefined stamp set + custom image-as-stamp import |
| §9.4 OCR | Add a lightweight OutputMode choice (searchable image vs editable text) |
| §9.5 Conversion | Add explicit OCR-mode toggle to PDF→Word/Excel/Text/CSV export dialogs |
| §9.6 Forms | Move tab order off the `/CO` array onto a spec-correct mechanism; resolve collision with calculated fields |
| §9.6 Forms | Add a real Digital Signature field type distinct from Text |
| §9.6 Forms | Harden the CSV/FDF import parser to match the export side's rigor |
| §9.9 Page Mgmt | Support multiple output parts from one split-by-range operation |
| §9.9 Page Mgmt | Add a distinct Page Numbering (Page Labels) mode with numbering-style/section support |
| §9.9 Page Mgmt | Extend Bates numbering to cross-document batches |
| §9.9 Page Mgmt | Add unit test coverage for Bates, header/footer, crop, resize, and merge |
| §9.10 Compare | Add a progress bar with page-by-page status and a Cancel button |
| §9.10 Compare | Add automated integration tests for `DiffEngine::compare` end-to-end |
| §9.11 Security | Add a "Selective remove" checkbox mode to Sanitize Document |
| §9.11 Security | Show a pre-commit summary/count of what Sanitize Document will remove |
| §9.11 Security | Deepen expiry enforcement beyond the viewer tool-mode flag (gate re-encrypt/sanitize/redact/save-as) |
| §9.12 Batch | Move Merge onto the async QtConcurrent pipeline with real progress |
| §9.12 Batch | Add recursive hot-folder watching + a polling fallback |
| §9.13 Compression | Implement unused/unreferenced object removal (mark-and-sweep) |
| §9.13 Compression | Extend downsampling coverage to Gray/CMYK/indexed raw streams |
| §9.14 Accessibility | Add a persistent, exportable results panel |
| §9.15 Search | Move thumbnail rendering off the GUI thread with proper `RenderCache` locking |
| §9.15 Search | Add automated test coverage for search/thumbnail/bookmark/jump-to-page wiring |
| §9.16 Import/Export | Unify Office/image import into the main Open flow + drag-and-drop |
| §9.16 Import/Export | Surface OCR output-mode/language picker in the OCR/scan-import UI |

### Wave 2C — Larger parity investments (L-effort)

| Domain | Item |
|---|---|
| §9.2 Editing | Add basic z-order (bring-to-front/send-to-back) for images |
| §9.3 Annotation | Add QuadPoints-based text-anchored Highlight/Underline/Strikeout/Squiggly (replace free-rectangle drag) — clearest quality gap vs every competitor studied |
| §9.11 Security | Bundle a static/vendored 7z library instead of shelling to system-installed `7z.exe` |
| §9.12 Batch | Add automated test coverage for Merge/OCR/Redact/Compress/Watermark/Export-PDF-A (6 of 7 batch ops have zero CI safety net) |
| §9.13 Compression | Implement real font subsetting for the existing "Subset fonts" checkbox |
| §9.14 Accessibility | Extend BBox/position extraction to marked-content (MCID) spans |

---

## PHASE 3 (optional, parallel track) — Polish + privacy/offline marketing bundle

Every domain in the audit independently flagged a "market the local-first/offline advantage
explicitly" item (badges, About-dialog copy, release notes, comparison callouts). Rather than
16 piecemeal copy changes, bundle these into one cross-cutting pass, run any time in parallel
with Phase 1/2 engineering since it touches copy/UI-badges, not core logic. Remaining P2 items
(per-domain polish: black-box color config, page-extract options, "Compress to target size" mode,
etc.) are lower priority and listed in full in the audit report §4 P2 section — pull from there
as capacity allows.

## Next step

Launch the implementation workflow wave-by-wave, starting with **Wave 1A**. Each domain gets one
agent in its own worktree, scoped to that domain's Wave 1A items, required to build clean and pass
full ctest before its branch is considered mergeable.
