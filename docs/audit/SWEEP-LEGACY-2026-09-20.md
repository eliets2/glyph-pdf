# SWEEP-LEGACY — 2026-09-20

Legacy-surface evaluate-and-FIX lane of the END-PHASE sweep. Scope: the July-era
parity surfaces and the core modes the September waves only touched at the edges —
conversion (ConversionManager export paths), compare engine + compare UI flows, OCR
scan/review canvas, forms editing (FormManager core), annotations editing
(move/resize/appearance), page operations (merge/split/rotate/extract/insert/delete),
measurement toolset, stamps, outlines/bookmarks user flows.

Lane: `feat/sweep-legacy-fix` (from feat/parity-glm @ 8f62a17). Build `build-leg`
(Debug, Ninja, UCRT64, -j 2, vendored podofo 1.1.0 verified at configure), offscreen
QtTest, serial. WIP handoff: `.context/sweep-legacy-wip.md`; evidence:
`.context/evidence-sweep-legacy/`. Method per defect: failing pin at tip (real saved
artifacts, independent read paths) → minimal root-cause fix at the shared boundary →
pin green → negative control (scoped `git checkout feat/parity-glm -- <paths>`, pin
fails again, restore, capture) → one commit per defect. NO defect-pin → NO fix.

## Verdict summary

| Surface | Verdict |
|---|---|
| Annotations editing (move/resize/appearance commit + read-back) | **3 defects fixed** (db18f5e) — origin class |
| Forms editing (FormManager core: field creation/update) | **defects fixed** (db18f5e + 82e661c) — origin class |
| Conversion (ConversionManager export paths) | **defect fixed** (23bc970) — transactional class; leftovers reviewed-clean/residual (below) |
| Compare engine + compare UI flows | reviewed-clean at tip (R06/PERF-02/PERF-03/R13/U04 hold; no new failing pins found) |
| OCR scan/review canvas (outside the fixed guards) | reviewed-clean at tip (review-state + scan-stack guards; honest empty-state wording) |
| Measurement toolset | reviewed-clean at tip after Fix 1 (write paths law-mapped; T1 pins green) |
| Stamps | reviewed-clean at tip (T2-6 pinned; placement rides the Fix 1 boundary); AP-aspect residual |
| Outlines/bookmarks user flows | reviewed-clean at tip (validate-before-mutate, UTF-8 titles, committed write) |
| Page operations | reviewed-clean at tip for bounds/transactional resident ops (WP-R02/R03); file-level PdfPageOps direct-write residual |

## Defects fixed (SHA + evidence)

### L1 — origin class: annotation embed/extract + form-field rects ignored /Rotate and the MediaBox origin — commit **db18f5e**

`PoDoFoBackend::applyAnnotationsToDoc` (annotation move/resize/appearance commit),
`PoDoFoBackend::extractAnnotations` (read-back), `FormManager`'s 9 `CreateField`
sites and `updateFieldRect` (write + reopen-verify) flipped viewer Y with the
MediaBox HEIGHT alone: the MediaBox lower-left origin was dropped and /Rotate was
ignored. On any rotated or offset-origin page the saved /Rect (and /InkList, /L,
/Vertices) landed where the user did not draw, while GlyphPDF's own overlay (fed by
the inverse-wrong read-back) kept showing the mark in the right place — the same
silent-misplacement class SEP13 L5/L8 fixed for redaction marks and F1 (R14) fixed
for the redaction consumers of annotation rects. Measured repro: embedding display
rect (100,150,80x40) on MediaBox [0 200 612 842] + /Rotate 90 stored
/Rect [100 652 180 692]; the law requires [150 300 190 380].

Fix at the shared boundary: NEW `core/ItemSpaceTransform.h` adds the EXACT inverse of
the ledger-verified `gp::PageSpace::viewerToUser` (the W1-owned PageSpaceTransform.h
is included, not modified; a round-trip identity slot pins both directions against
drift). The writer maps display-space bounds/geometry through the law and stores the
raw user rect VERBATIM via `SetRectRaw` / raw `AddKey("Rect")` — an empirical probe
against PoDoFo 1.1.0 (`.context/evidence-sweep-legacy/podofo_rect_probe.cpp`) proved
`CreateAnnot`/`CreateField` rect parameters are /Rotate-View-space and would transform
AGAIN. On /Rotate 0 pages the new code stores exactly the legacy numbers (all
pre-existing fixtures byte-stable). The reader maps raw /Rect + geometry into display
space, so foreign annotations on rotated/offset pages surface at the drawn spot.

Pins: NEW `tests/TestLegacyOriginSpace.cpp` (9 slots) over a 4-page fixture
({plain, offset, rotated, rotated+offset}); expected /Rects verified through raw
dictionary arrays AND PDFium `FPDFAnnot_GetRect` (second engine), with hardcoded
literals for the decisive shapes. FAIL-FIRST 6 failed / 3 passed
(`origin-PREFIX-failfirst.txt`), 9/9 post-fix (`origin-POSTFIX.txt`), scoped revert
re-fails the same 6 anchors (`origin-NEGCTRL-PREFIX.txt`). Adjacent regression sweep
23/23 (forms, annotations, measure, redaction-proof, history, two-page, export).

### L2 — transactional class: convertOfficeToPdf destroyed the destination on a failed run — commit **23bc970**

`ConversionManager::convertOfficeToPdf` removed the caller's destination
(`QFile::remove(outputPath)`) BEFORE renaming the soffice product into place: any
rename failure after the remove (converter exits 0 but writes nothing — the
phantom-success shape, an open handle on the source, a cross-volume move) DESTROYED
the previous output while reporting false — the same destructive class WP-R04 (A03)
fixed for the encrypted-package flow, still present in the July-era office-import
path.

Fix: validate the converter's product FIRST (readable + `%PDF` header — also upgrades
an exit-0-garbage phantom success into an honest failure), then commit through
`SafeSave::commitFileToDestination` (atomic replace; destination never touched before
commit), candidate removed on every outcome; in-place case keeps the validation.

Pins (`TestOfficeImport` +3 slots; R04's re-exec'd fake-writer pattern — the test
binary copied to `soffice.exe` and planted on PATH, child mode from a mode file beside
argv[0], no LibreOffice install involved):
`officeConvertNoOutputKeepsPreviousDestination` (runtime data-loss repro — pre-fix
the sentinel was destroyed: `office-PREFIX-failfirst.txt`),
`officeConvertCommitFaultKeepsDestinationByteIdentical` (pre-fix the conversion
"succeeded" over the sentinel with FailBeforeCommit armed — the tail never routed
through the transaction boundary), `officeConvertSuccessReplacesDestinationAndCleansCandidate`
(control: PoDoFo-verified output, candidate consumed). Negative control (scoped
revert): both anchors fail again (`office-NEGCTRL-PREFIX.txt`); post-fix 10/10
(`office-POSTFIX.txt`).

### L3 — origin class: auto-detected form fields mirrored to the opposite side of the page — commit **82e661c**

`FormManager::autoDetectFields` walks the content stream in RAW USER space (baseline
Y up) but emitted `FieldSuggestion::rect` directly in that space, while the rect
contract (and the add*Field consumers, post-L1) is DISPLAY space: a double flip that
mirrored every suggestion — a "Name:" label at baseline y=700 (near the TOP of a
Letter page) produced a field stored at user y≈72 (near the BOTTOM). Every
auto-detected field landed on the opposite side of the page from its label once
placed, on ANY page (no rotation needed).

Fix: build the suggestion rect in user space (existing in-page clamp arithmetic kept,
user-space), then map through `gp::ItemSpace::userToViewer` — the same single mapping
as L1. Pin `TestAutoDetectHeuristic::suggestionSitsUnderTheLabelNotMirrored`:
suggestion must be at display (108, 75.2) for the `72 700 Td (Name: ) Tj` fixture and
placing it must store the widget /Rect at user y≈700..716.8 (read back RAW).
FAIL-FIRST `autodetect-PREFIX-failfirst.txt` ("suggestion y 700 != 75.2"); post-fix
5/5; scoped revert re-fails (`autodetect-NEGCTRL-PREFIX.txt`); TestFormSafety 12/12.

## Coverage table — what the legacy sweep actually touched

| Surface | Files inspected | Verdict | Action |
|---|---|---|---|
| ConversionManager export paths | src/engines/ConversionManager.cpp/.h (all writers: Word/Excel in-house OOXML, HTML, Text, CSV, PPTX, Image, officeToPdf, imagesToPdf; zip writers; soffice discovery) | 1 defect fixed (L2) | fix + 3 pins + NEGCTRL (23bc970) |
| Conversion leftovers | exportToImage page option (only API/batch passes it; UI renders all), CSV UTF-8-no-BOM (valid; Excel mojibake = consumer note), duckx/OpenXLSX branches (not compiled here) | reviewed-clean / residual | reported, no pin → no fix |
| Compare engine + UI | DiffEngine, MyersDiff, CompareMode, CompareWidget export paths | reviewed-clean at tip | none (prior lanes' rows hold) |
| OCR scan/review canvas | OCRMode (review-state guards, honest empty-state), OcrEngine seams | reviewed-clean at tip | none |
| Forms core | FormManager (9 CreateField sites, updateFieldRect, autoDetectFields, runFormSaveTransaction) | 2 defect classes fixed (L1, L3) | fix + pins + NEGCTRL (db18f5e, 82e661c) |
| Annotations editing | PoDoFoBackend applyAnnotationsToDoc/extractAnnotations (all /Rect + geometry arrays + AP streams), EditAnnotationCommand, AnnotationLayer rotation mapping | 1 defect fixed (L1); AP-aspect residual | fix + pins + NEGCTRL (db18f5e) |
| Page ops | PdfPageOps (extract/delete/insertBlank/rotate/merge/writeDocumentFromPages), PoDoFoBackend resident page ops | reviewed-clean at tip; file-level direct-write residual | reported |
| Measurement | MeasureCore, MeasureMode, PoDoFoBackend measure write/read paths (post-L1) | reviewed-clean after L1 (TestMeasureRoundTrip/TestMeasurePanelHonesty green) | covered by Fix 1 |
| Stamps | applyAnnotationsToDoc stamp branches, StampLibrary consumers | reviewed-clean; AP-aspect residual on /Rotate pages | covered by Fix 1 boundary |
| Outlines/bookmarks | PoDoFoBackend getOutline/replaceOutline, SetOutlineCommand flows | reviewed-clean at tip | none |
| Same-class residuals for owner lanes | extractLinks link-rect reader (PoDoFoBackend ~4992); T2-2 Find&Replace replacement writer (~2751) | NOT touched (outside lane scope) | exact fix recipe = L1's mapping; owner lanes to pick up |

## Suites run (serial, offscreen)

- Baseline at tip (10 suites): 10/10.
- TestLegacyOriginSpace: NEW, 9 slots — FAIL-FIRST 6F/3P → 9/9 → NEGCTRL 6F.
- TestOfficeImport: 7 slots → +3 = 10 slots — FAIL-FIRST 2 anchors → 10/10 → NEGCTRL 2F.
- TestAutoDetectHeuristic: 4 → +1 = 5 slots — FAIL-FIRST 1 anchor → 5/5 → NEGCTRL 1F.
- Adjacent regression sweeps: 23/23 (db18f5e), 6/6 (82e661c), 3/3 conversion
  (23bc970) — TestFormSafety, TestFormBuilder, TestFormUndo, TestFormStaleDisclosure,
  TestFormJsCalc, TestFormKeystroke, TestFormPersistence, TestFieldMetadata,
  TestAnnotationDjot, TestShapeInkPersistence, TestMeasureRoundTrip,
  TestMeasurePanelHonesty, TestAnnotationToolBar, TestCommentsReview,
  TestSep13LeadRedactionProof, TestHistoryIntegrity, TestTwoPageOverlay,
  TestAutoBookmarks, TestExportPathBadge, TestControllers, TestFillFormNoOp,
  TestConversionExtraction, TestPdfEditorInterface.
- Full-suite serial gate: see final line of this file (and
  `.context/evidence-sweep-legacy/final-gate-ctest.txt`).

## Residuals (explicit)

1. Appearance-stream (/AP) BBox aspect on /Rotate pages: the annotation's position
   (/Rect + geometry) is now correct, but image/stamp appearance streams are
   letterboxed in item-space aspect without an AP /Matrix rotation — appearance
   content may be stretched on /Rotate pages. Deferred (requires AP matrix work).
2. `extractLinks` link-rect reader and the T2-2 Find&Replace replacement writer still
   use the height-only flip (same class as L1, different surfaces/lanes). Recipe: the
   Fix 1 mapping (`PageSpace::viewerToUser` / `ItemSpace::userToViewer`).
3. File-level page ops (`PdfPageOps::mergeDocuments/extractPages/deletePages/
   rotatePages`) write the destination directly (PoDoFo Save) rather than through a
   SafeSave candidate — mitigated by explicit Save-As file dialogs; same upgrade
   shape as L2 if the program wants transactional semantics there.
4. `exportToImage` with an out-of-range `page` option silently renders ALL pages
   (API/batch-only; the shipped UI passes no page option).
5. Conversion CSV is valid UTF-8 without BOM — Excel shows mojibake when a BOM-less
   CSV is double-clicked (consumer rendering concern; possible enhancement).
6. Negative /Rotate values (`(cur + angle) % 360` can go negative on repeated
   counter-clockwise rotates) — spec-legal; PDFium/Acrobat normalize. Not pinned.

## Ledger

A `sweep-legacy` section was appended to
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` (rows implemented-awaiting-review).

## Full-suite gate

See `.context/evidence-sweep-legacy/final-gate-ctest.txt` — filled at lane end.
