# GlyphPDF Parity Scorecard — reconciliation of the July audit against main @ `2ccfd5ba`

**What this file is.** The completed parity record: every claim in
[`COMPETITIVE-PARITY-AUDIT-2026-07-01.md`](COMPETITIVE-PARITY-AUDIT-2026-07-01.md)
(16 domains, 53 P0 + 60 P1 + 31 P2 items) is dispositioned against current
`main` with code-level evidence — the artifact (function / `connect()` /
writer) **and** the pinning test. A status label from any September record was
never accepted as proof; each DONE below names code this lane read.

- **Base:** `main` @ `2ccfd5baaabf032ddb1902641bc3d7c32a31567d` (v1.5.0 lineage,
  2026-09-30). **Branch:** `feat/parity-reconcile` (worktree `D:/pdf/pdf-feat-parity`).
- **Verification standard:** EXISTS → SUBSTANTIVE → WIRED (3-level), with the
  test pin read at source. "git log says so" alone was never accepted.
- **Ledger population:** the July file contains **53 P0 + 59 P1 + 31 P2 =
  143 items** (the briefing's "~45/~60" was approximate; §3 counts every one
  of the 112 P0+P1 rows).
- **Disposition counts** (§3 ledger): **DONE-IN-MAIN 83 · PARTIAL 8 ·
  STILL-OPEN 13 · OBSOLETE 1 · OWNER-DECISION 0 · MARKETING-GROUP 7**
  (2 P0 + 5 P1 "market…" rows batched into §3.4).

---

## 1. Updated scorecard

Same /10 scale as July. "New" is this lane's verified grade at `2ccfd5ba`.

| § | Feature | Jul | New | One-line justification (evidence anchor) |
|---|---|---|---|---|
| §9.1 | Document Viewing | 4.5 | **9.0** | Rotation is engine-side `/Rotate` + reload (`requestPageRotation` → `GpMainWindow.cpp:678`), links navigate with a scheme allow-list (`PdfViewerWidget::handleLinkClick`), two-page mode composites annotations + search highlights, Night Mode is a real RGB-inverting `QGraphicsEffect`; `TestViewParity` locks every view mode. Residual: two-page is still a composited spread, not a live layout. |
| §9.2 | Text & Object Editing | 3.5 | **8.5** | Image rotate/replace/delete/restack/opacity live in a gated right-click menu driving the command classes (`EditController.cpp:1330-1445`), Cut/Copy/Delete wired (`EditController.cpp:1464+`, `TestControllers.cpp:81`), eraser real via `deleteObjectAt` (`:1491`), text opacity/letter/line spacing shipped (`EditToolBar` + `TestTextEditStyle`). Residual: no in-document paste; text-edit undo still page-snapshot. |
| §9.3 | Annotation & Markup | 4.0 | **8.5** | Shapes/ink persist as `/Square /Circle /Line /Ink` with inverse mapping (`PoDoFoBackend.cpp:4962-4979`, `:5504-5540`; `TestShapeInkPersistence`), one authoritative markup surface (ribbon Comment tab, 13 tools, `TestAnnotationToolBar`), dynamic stamp library (`StampLibrary`, `TestDynamicStamps`). Residual: no QuadPoints text-anchored markup anywhere; stamp tool takes no imported image. |
| §9.4 | OCR | 5.0 | **8.5** | Language wired to both engines from `ocr/language` (`EditController.cpp:997`), Accept exports a searchable MRC PDF/A (`:1618 exportMrcPdfA`), 0/90/180/270 orientation detection via Leptonica (`OcrPreprocessor`, `TestOcrPreprocessor`), preprocessing prefs honest-by-default. Residual: no searchable-vs-editable OutputMode choice; region re-OCR honestly labeled page-scoped. |
| §9.5 | Conversion | 5.0 | **8.5** | In-house real-OOXML .docx/.xlsx writers replaced the mislabeled HTML/CSV fallback (`ConversionManager.cpp:323-380`), engine tracking + honest fallback warning (`ConvertController.cpp:152-167`; `TestExportPathBadge` writes and parses real packages), PPTX overlay at ~1% alpha (`:1365`; `TestPptxOverlayAlpha`). Residual: batch convert still lacks Text/PowerPoint; no OCR toggle in export dialogs. |
| §9.6 | Forms | 5.5 | **8.0** | `fillForm` split into fill vs fill+lock (`FormManager.cpp:620`; `TestFillFormLock`), unsupported fields surface an "Import Incomplete" dialog (`FormsController.cpp:209-218`), /TU + /Ff-required persist (`:345-355`; `TestFieldMetadata`), CalcField in the ribbon (`FormsController.cpp:40`, `MenuBar.cpp:128`), auto-detect is a real disclosed heuristic with review-before-commit (`AutoDetectPlacement`; `TestAutoDetectHeuristic::plainPageYieldsNoFakeFields`), tab order moved to /Annots + `/Tabs W`, never /CO (`:1776+`). Residual: Signature form field still maps to Text; CSV/FDF import parser still naive. |
| §9.7 | E-signatures | 5.5 | **9.0** | Draw/Type/Upload/Initials picker (`SignaturePicker.cpp:242+`; `TestSignaturePicker` 10 pins), visible ETSI-layout appearance auto-fit (`SignatureManager.cpp:1044+`; `TestSignatureAppearance`), per-signature badges incl. two-page + Validate-All summary (`TestSignatureBadges`, `TestValidateAllSignatures`), session cache, outcome dialog. Deepest parity turnaround in the audit. |
| §9.8 | Redaction | 5.5 | **9.0** | Clear Marks removes marks (`RedactMode.cpp:601`; `TestRedactClearMarks`), Mark-Region/Mark-All with page lists in-mode (`TestRedactMarkAll`), sanitize default-ON bundle on both paths (`TestRedactSanitizeBundle`), one transaction + SHA-256 source check, Cancel/Back, overlay text burn-in, word-list import, named presets; PGR-46 mixed-space mark flip fixed (`99dd7b67`). |
| §9.9 | Page Management | 5.5 | **8.5** | Thumbnail-grid drag reorder is live (`PagesMode.cpp:440-453` InternalMove on the IconMode grid; `TestPagesMode::internalMovePushesAtomicPermutation`), merge honesty pinned (`TestMergeSuccess`), one reorder command (`cb635a84`), Merge grouped in the Organize ribbon (`RibbonModel.cpp:157`), multi-part split `_part{n}` (`:561-582`), Bates cross-document (`TestBatesCrossDoc`). Residual: Page Labels writer/UI shipped since 2026-09-23 (§3 row 60 DONE — the earlier "deferred" note was stale). |
| §9.10 | Document Comparison | 3.0 | **9.0** | "&Compare Documents…" → dual file pickers → `compareFiles` (`MenuBar.cpp:370`, `CompareMode.cpp:796-806`), change-type filters gate tree + panes (`CompareWidget::setChangeFilter`; `TestCompareEntry` 20 pins), added/removed pages as structural rows, deterministic mid-insert alignment, memoized anchors (`TestSep13LeadComparePerf`). Residual: no progress/cancel. |
| §9.11 | Security | 6.5 | **8.5** | Watermark honors font family + real glyph metrics (`PoDoFoBackend.cpp:1442`; `TestWatermarkFont::testCenteringUsesRealFontMetrics`), Set Expiry Date UI + SafeSave transaction + XMP-survival pins (`SecurityController.cpp:451`; `TestExpiryInterface`), expiry gates the whole mutating tool set incl. Encrypt/Sanitize/ApplyRedact/CertEncrypt (`EditPolicy::isMutatingTool`), certificate-encryption recipient picker (N17). Residual: no selective-sanitize mode or pre-commit summary; encrypted packages now use the vendored 7-Zip (§4 row 14, closed 2026-10-01 — no longer a system-7z dependency). |
| §9.12 | Batch & automation | 5.0 | **8.5** | Multi-pattern redaction is one load/find/apply/save (`applyPatternRedactionsMulti`, `BatchMode.cpp:2251`; `508f86e9`), OCR language combo + 12-language mapping (`:459-468`; `TestBatchOcrLanguage`), low-confidence notes (`TestBatchOcrConfidence`), presets U1–U7 (Bates lane, rename-on-conflict, stop-on-failure, import/export, reports, hot-folder ingest, multi-step editor), configurable DPI presets, mutex now only where PDFium genuinely demands it (`:1957-1985` with stated rationale; `TestLaneScheduler`). Residual: hot-folder watch is non-recursive, no polling fallback; processed-set unbounded. |
| §9.13 | Compression | 2.5 | **8.0** | Real JPEG decode/re-encode honoring quality+DPI (`PoDoFoBackend.cpp:6439-6513`; `TestCompressJpegReencode::qualityIsHonored`), dedup rewiring done incl. SMask-aware fingerprints (`TestImageDedup::duplicatesRewiredToCanonical`, `TestDedupSMask`), unused-object sweep implemented (`21a387c`; `CompressDialog.cpp:182`), estimates only claim passes that run (`TestOptimizeEstimate`), strip path = full sanitize (`TestCompressStripSanitize`), measured before/after readout (`CompressDialog.cpp:604`; `TestCompressDialogHonesty::completionReportCarriesMeasuredFiguresAndDelta`), signed-doc refusal. Residual: font subsetting absent (checkbox disabled with explanation); CMYK/indexed downsampling deliberately skipped. |
| §9.14 | Accessibility | 3.0 | **8.5** | /Pg inheritance walk-up per ISO 32000-2 §14.7.2 (`PdfAValidationPanel.cpp:423-427,470`; `TestReadingOrderInheritance::inheritedPgIsNotFlagged`), checker off the UI thread (`TestReadingOrderAsync`), named tolerance constant (2) with pin (`TestReadingOrderThreshold`), jump buttons on the same panel's issue list, plus the program-level checker + Tag Document. Residual: MCID spans still skipped (`:462`); no exportable results panel; 60-level depth cap fixed and unwarned. |
| §9.15 | Search & Navigation | 6.0 | **8.5** | Match Case / Whole Words / Regex shape a shared matcher used by search and redact-all (`EditController::pageTextPattern` → `TextMatchFinder::buildPattern`; `TestSearchFlags` 7 pins) with an honest page-level note when flags are set (`:323-331`), thumbnail zoom buttons wired (`ThumbnailSidebar.cpp:92+`; `TestThumbnailZoom`), RenderCache concurrency + cancellation pinned (`TestThreadSafety`). Residual: flag-filtered matches highlight at page level only; bookmark/comment scopes don't cycle. |
| §9.16 | File import/export | 5.5 | **8.5** | Export-path honesty: engine tracking + fallback warning dialog + local-processing notice on every export (`ConvertController.cpp:152-167`; `TestExportPathBadge`), linearize preset actually linearizes via qpdf with `qpdf_is_linearized` verification (`QpdfBackend.cpp:19-65`; `TestExportPresets::linearizedPresetRequestsLinearize`), unified Open routing + drop plans (`GpMainWindow::planDrop:1612`; `TestOpenRouting`), bookmark/link round-trip tests (`TestLinkBookmarkRoundTrip`). Residual: OCR output-mode not exposed in import UI; no binary Office fixtures. |

**July average ~4.9/10 → verified average 2026-09-30: 8.6/10** (137/16).
The four domains that anchored the July bottom (Compression 2.5, Comparison 3.0,
Accessibility 3.0, Text & Object Editing 3.5) now grade 8.0–9.0 with pins.

The remaining distance to 10 is not "dead code" — it is named feature gaps
(§3 below): QuadPoints markup, MCID-level reading order, font subsetting,
selective sanitize, OutputMode choice, hot-folder recursion.

---

## 2. What changed since July (program map)

The reconciliation is not one commit; it is three waves, each independently
recorded:

1. **Early targeted fixes (2026-08-25…27)** — the July P0 spine, one commit per
   audit item with the §-ID in the message: `d3dbde4f` (§9.4 language),
   `082f7926` (§9.8 Clear Marks), `21d826fa` (§9.3 shapes/ink),
   `88e9517e` (§9.14 inheritance), `7ecc1aba` (§9.1 rotation),
   `872986bf` (§9.11 watermark), `508f86e9` (§9.12 single-pass),
   `40bb3624` (§9.15 flags), `9f033c49` (§9.16 presets/linearize seam),
   `e5a5f012` (§9.3 markup surface pin).
2. **Consolidation (PR #2, `1991d9c1` + Phase C)** — twenty parity lines
   cherry-picked onto main; PGR-01…46 security/quality findings fixed
   (43 of 46; PGR-33 owner-as-is, PGR-40/41 deferred+disclosed);
   CX-01…17 + N1/N2 + INV-1; K5 deadlock; the Set Expiry Date data-loss fix.
3. **September packages F01–F12 / U01–U08 / R-series / Q-series** — the
   v1.5.0 release body (CHANGELOG [1.5.0]); evidence ledger
   `CURRENT-EVIDENCE-LEDGER-2026-09-05.md`; open work moved to
   [`PROGRAM-CONSOLIDATION-2026-09-25.md`](PROGRAM-CONSOLIDATION-2026-09-25.md).

---

## 3. Per-item disposition ledger (P0 + P1)

Legend: **DONE** = implemented, wired, and pinned in main (evidence names both).
**PARTIAL** = the user-visible core shipped, a stated remainder exists.
**OPEN** = verified absent in current code. **OBSOLETE** = superseded; the
target changed meaning. **GROUP** = marketing/positioning item, batched in §3.4.

### §9.1 Document Viewing (P0 ×3, P1 ×1)

| # | July item | Status | Evidence |
|---|---|---|---|
| 1 | P0 Rotate the real page bitmap, not just the overlay | **DONE** | `PdfViewerWidget::rotateClockwise/CounterClockwise` emit `requestPageRotation(±90)` (`PdfViewerWidget.cpp:541-557`); connected to `PagesController::onPageRotateRequested` at `GpMainWindow.cpp:678`; engine writes `/Rotate` and reloads (`PdfEditorEngine::rotatePage:1282`, `PdfPageOps::rotatePages:88`). Pins: `TestViewerRotation::rotateEmitsEngineRequest` (asserts ±90), `reloadResetsRotationState`. Carrier `7ecc1aba`. The view-local bitmap route was rejected as unachievable in this Qt build — the honest alternative satisfies the item's intent. |
| 2 | P0 Real hyperlink URI + GoTo navigation | **DONE** | `PdfViewerWidget::setLinkReader/refreshPageLinks/handleLinkClick` (`:595-648`): URI → `QDesktopServices::openUrl` gated by `isSafeLinkScheme` (http/https/mailto only), GoTo → `goToPage`. Pins: `TestHyperlinkNavigation::extractsUriAndGoToLinks`, `TestViewerLinkSeam`, `TestLinkSchemeSafety` (unsafe schemes consumed, never opened). |
| 3 | P0 Two-page mode: restore annotation visibility + search-highlight sync (stated minimum bar) | **DONE** (minimum bar) | `AnnotationLayer` composites into the spread via `paintTwoPageOverlays` (`PdfViewerWidget.cpp:123,237-246`). Pins: `TestTwoPageOverlay::twoPageShowsAnnotationsOnLeftAndRightPages`, `twoPageShowsSearchHighlights`, `modeTogglingPreservesAnnotations`, `unpairedLastPageShowsItsAnnotation`. Residual (still-open, below): the spread is still static images + overlay compositing, not a live `QPdfView` layout — the July "effort L" full version. |
| 4 | P1 Content-level Night Mode inversion | **DONE** | `ui/NightModeEffect.h` — `QGraphicsEffect` inverting RGB (`invertPixels(InvertRgb)`), applied via `ViewController.cpp:87-92` → `viewer->toggleNightMode()`. Pins: `TestViewingModes::nightModeEffectInvertsPixels`, `nightModeCoversBothPageSurfaces`, `nightModeChangesTheRenderedViewer`. |

### §9.2 Text & Object Editing (P0 ×4, P1 ×3)

| # | July item | Status | Evidence |
|---|---|---|---|
| 5 | P0 Wire Image Rotate to the UI | **DONE** | Right-click image menu (Rotate CW/CCW/180/custom-angle) → `RotateImageCommand` (`EditController.cpp:1330-1368`), read-only gate at the top (ARC07). Pins: `TestImageEditWiring::rotateRedoUndoAndMerge` (incl. merged-inverse −180°), `TestImageAppearance::customAngleRotationUndoesExactly`, `moveResizeRotateReachTheRealImage`. PGR-29/31 independently fixed the engine math. |
| 6 | P0 Wire Image Delete and Replace | **DONE** | Same menu: Replace… → `ReplaceImageCommand` (`:1409`), Delete (with confirm) → `DeleteImageCommand` (`:1430`), both under the restorable-page-backup contract. Pin: `TestImageEditWiring::deleteAndReplaceCommandsExecute`. |
| 7 | P0 Minimal Cut/Copy/Delete for the EditController selection | **DONE** | `ToolId::Cut/Copy/DeleteSelection` handled (`EditController.cpp:169-177`); Copy places a raster snapshot of the selected object's region on the system clipboard (`:1464-1467`, in-document paste explicitly out of scope, as July scoped it). Pin: `TestControllers.cpp:81-88` ("§9.2 P0: minimal clipboard editing is wired"). |
| 8 | P0 Real eraser via the `deleteObjectAt` pipeline | **DONE** | `ToolId::Erase` arms `ToolMode::Erase` and connects `AnnotationLayer::eraseRequested` → `EditController::onEraseRequested` → `_ctx->pdfEditor->deleteObjectAt(pageIndex, pos)` (`EditController.cpp:161-167, 1491-1501`). Pins: `TestEraseWiring`, `TestEraserEngine`, `TestEraseSignedGuard` (signed-doc refusal). |
| 9 | P1 Opacity control in the inline text and image toolbars | **DONE** | Text: `EditToolBar::opacityCombo` → `textStyleChanged` → `EditController::onTextStyleChanged` (connect `:1236`); image: context-menu "Opacity…" → `ImageAppearanceCommand::Kind::Opacity` (`:1373-1390`). Pins: `TestTextEditStyle::opacityNeverLetsTheReplacedTextShowThrough`, `TestImageAppearance::opacityBlendsAndUpdatesInPlace`. |
| 10 | P1 Letter-spacing and line-spacing in EditToolBar | **DONE** | `EditToolBar::letterSpacingCombo/lineSpacingCombo` (`EditToolBar.cpp:109-119`) on the same `textStyleChanged` signal. Pins: `TestTextEditStyle::lineSpacingScalesTheBaselineGap`, `letterSpacingWidensTheLine`, `defaultsKeepThePreviousOutput`. |
| 11 | P1 Z-order (bring-to-front / send-to-back) for images | **DONE** | Menu entries → `ImageAppearanceCommand::Kind::BringToFront/SendToBack` — a content-stream restack that moves only the image block. Pins: `TestImageAppearance::restackMovesOnlyTheImageBlock`, `bringToFrontChangesTheVisibleImageAndPersists`, `sendToBackChangesTheVisibleImage`, `commandUndoRestoresTheStacking`. |

### §9.3 Annotation & Markup (P0 ×3, P1 ×2)

| # | July item | Status | Evidence |
|---|---|---|---|
| 12 | P0 Fix the false "file attachment" CHANGELOG claim | **DONE** | CHANGELOG v1.3.0 corrected in place (now at `CHANGELOG.md:296-300`: "file attachments are NOT implemented", correction dated 2026-07-01); dead `attachmentPath` field **removed from the model** in `cb635a84` (verified: zero `attachmentPath` matches under `src/`). The correction record itself is `CORRECTIONS-2026-07-01.md`. |
| 13 | P0 Persist shapes/freehand as real PDF subtypes | **DONE** | `applyAnnotationsToDoc` maps DrawRectangle→`/Square`, DrawEllipse→`/Circle`, Line/Arrow→`/Line`, Freehand/Signature→`/Ink` (+ measure →`/PolyLine`//`/Polygon`) at `PoDoFoBackend.cpp:4962-4979` ("§9.3 P0" comment); inverse in `extractAnnotations` at `:5504-5540` (reads subtype name + `InkList`). Pins: `TestShapeInkPersistence::shapesRoundTripAsRealSubtypes`, `inkRoundTripsWithPoints`. Carrier `21d826fa`. |
| 14 | P0 Consolidate the two annotation toolbars into one surface | **DONE** — **and the July premise was already stale when written** (see §5, misaudit M-1) | The floating `AnnotationToolBar` was removed from the build on 2026-06-22 (`6aac22ca` — before the July audit; verified: no CMakeLists/GpMainWindow reference at the July-1 tree). The ribbon Comment tab is the single surface exposing all 13 markup tools incl. Strikeout/Squiggly/Stamp/Callout, pinned by `TestAnnotationToolBar` (`e5a5f012`). The dead class is deliberately retained as a revival base — its header (`AnnotationToolBar.h:4-11`) requires parity-lane sign-off to delete: **this lane signs off**; delete class + header + .cpp at the next purge, keeping the test. |
| 15 | P1 QuadPoints-based text-anchored Highlight/Underline/Strikeout/Squiggly | **OPEN** | Verified absent: zero matches for `QuadPoints`/`QuadPointsArray` in `src/` and `tests/`; highlight/underline still persist as drag-rect annotations (`applyAnnotationsToDoc` writes the drag `/Rect` only). Dispatch target: `PoDoFoBackend::applyAnnotationsToDoc` + `AnnotationLayer` placement; needs a text-hit → quad-set seam (the `TextMatchFinder` already produces rects). |
| 16 | P1 Predefined stamp set + custom image-as-stamp import | **PARTIAL** | Text half done well: `StampLibrary` (five built-ins Approved/Draft/Confidential/Received/Reviewed, custom stamps persisted to stamps.json, dynamic `${author}/${date}/…` placeholders substituted at apply time). Pins: `TestDynamicStamps::builtInLibraryMatchesResearchRow`, `customStampsPersistAndReload`, `savedAnnotationCarriesResolvedText`. **Remaining:** no image-as-stamp import — `StampTemplate` has no image field (`StampLibrary.h:19-24`); the signature Upload mode is the only image-as-`/Stamp` path. |

### §9.4 OCR (P0 ×3, P1 ×4)

| # | July item | Status | Evidence |
|---|---|---|---|
| 17 | P0 Wire the language selector into the engines | **DONE** | `EditController.cpp:997-1002` reads `ocr/language`, maps via `ocrEngineLanguageCode`, passes to both engines (`RapidOcrEngine::initialize(lang)` at `:1068`; Tesseract error paths name the resolved code `:1101-1106`). Verified the July bug existed: July-1 tree had `QStringLiteral("eng")` hardcoded at `EditController.cpp:427`. Pins: `TestOcrLanguage` (12-language table, unique codes, eng fallback); batch twin `TestBatchOcrLanguage`. Carrier `d3dbde4f`. |
| 18 | P0 Accept must persist the searchable layer (exportMrcPdfA) | **DONE** | `EditController::onOcrAcceptRequested` → `exportMrcPdfA(outPath, {pageImage}, {pageResult})` (`:1618`), with the R08 original-image guarantee, read-only write-blocker, and per-page honest status (`:1533-1633`). Pins: `TestMrcPipeline`, `TestOcrAcceptSeam`, `TestOcrAcceptScope`, `TestOcrReviewLifecycle` (34P). |
| 19 | P0 Page-level orientation detection (0/90/180/270) | **DONE** | `OcrPreprocessor` `orientDetect` (0/90/180/270 via Leptonica, inverse-mapped boxes) — `OcrPreprocessor.h:21,27`; wired to the persisted `ocr/orientDetect` pref at `EditController.cpp:1005-1006` (the July "dead orientDetect flag" is now read). Pins: `TestOcrPreprocessor` (orientation-only options, inverse transform). |
| 20 | P1 OutputMode choice (searchable image vs editable text) | **OPEN** | No `OutputMode` anywhere (`grep -rni outputmode src/ tests/` = 0). The only export path is the searchable MRC copy (`:1618`); the review screen delivers editable text into the session but no user-facing mode toggle. Partial overlap with the archived FineReader lane (owner decision FOLD-2, PROGRAM-CONSOLIDATION §2.6). |
| 21 | P1 Make Re-OCR-this-region region-scoped | **PARTIAL** | Feature still page-scoped — `OCRMode::onImagePaneContextMenu` sets `m_contextRegionBbox = QRectF()` ("empty = whole current page", `OCRMode.cpp:745`) — but the lying label was fixed instead: the menu now reads "Re-OCR entire page" with a U03 disclosure row ("Regional actions act on the whole page until region OCR ships", `:751-757`). Region mapping to `LayoutRegion` bboxes remains in-code future work. |
| 22 | P1 Surface which binarization/deskew path is active in the Verify UI | **PARTIAL** | The preprocessing checkboxes persist and are honored (`ocr/preprocessDeskew/Binarize/Denoise` → `OcrPreprocessOptions`, `EditController.cpp:1007-1014`; pins `TestOcrPreprocessPrefs` incl. shipped-off defaults F5-F2). **Remaining:** no disclosure when the Leptonica-backed path is absent in a build (`HAS_TESSERACT` gating is invisible in the UI). |
| 23 | P1 Automated tests for language pass-through and orientation | **DONE** | `TestOcrLanguage` (mapping table), `TestBatchOcrLanguage` (combo→engine code), `TestOcrPreprocessor` (orientation/deskew/inverse), `TestOcrPreprocessPrefs`. |

### §9.5 Conversion (P0 ×3, P1 ×4)

| # | July item | Status | Evidence |
|---|---|---|---|
| 24 | P0 Land real OOXML (never mislabel fallback output) | **DONE** (by in-house writers — the July "land duckx/OpenXLSX" mechanism was superseded by a better one) | `exportToWord/exportToExcel` write real OOXML in both builds: duckx/OpenXLSX when present (`ConversionManager.cpp:323-330,348-363`), else the in-house WordprocessingML/SpreadsheetML writers (`:340-344,368-372` — "produce REAL OOXML in-house instead of the old mislabeled HTML-as-.docx fallback"). The never-mislabel guarantee is the invariant July actually demanded. Pins: `TestExportPathBadge::wordDocxIsRealOoxmlPackage`, `excelXlsxIsRealOoxmlPackage` (writes and unzips the package), `TestOfficeExport`. |
| 25 | P0 Automated tests for the real-OOXML paths | **DONE** | `TestOfficeExport` (non-empty output, content matches active backend, missing-input failures), `TestExportPathBadge` (package structure + engine tracking + control-char stripping), `TestPptxOverlayAlpha` (slide XML). The "regression could silently reintroduce the fallback" risk is additionally covered by the `ExportEngine::Fallback` warning path (`ConvertController.cpp:156,224`). |
| 26 | P0 PPTX text-overlay opacity ~invisible | **DONE** | `ConversionManager.cpp:1365` writes `<a:alpha val="1000"/>` (~1%) on the overlay run. Pins: `TestPptxOverlayAlpha::slideXmlCarriesAlpha` (asserts the element and the 1000 value), hostile-character round-trips. |
| 27 | P1 OCR-mode toggle in PDF→Word/Excel/Text/CSV dialogs | **OPEN** | No OCR reference in `ConvertController`/`ExportPresetsPanel` (grep = 0). The OCR pipeline exists to wire (§9.4 P0s landed). |
| 28 | P1 Batch format list parity with single-document path | **PARTIAL** | Single-doc ConvertController: Word/Excel/Csv/Html/Text/PowerPoint/Image (`ConvertController.cpp:149-626`). Batch: Word/Excel/Html/Image/Csv only (`BatchMode.cpp:252-256`) — **Text and PowerPoint still batch-missing**. |
| 29 | P1 Batch Compress uses DPI+quality pairing, not hardcoded DPI=150 | **DONE** | `m_dpiPresetCombo` (Low 72 / Medium 150 / …, `BatchMode.cpp:294-305`, "§9.12 P1" comment), schema range-checked 36–600 (`:74-78` R26 static_assert). The single-doc dialog pairs quality+DPI (`CompressDialog`). Pins: `TestBatchPresets`/`TestBatchPresetsP2` schema + `TestBatchMode`. |
| 30 | P1 Market the local-first conversion pipeline | **GROUP** (delivered in-product; see §3.4) | `localProcessingNotice()` appended to every export success message (`ConvertController.cpp:28,165,223`). |

### §9.6 Forms (P0 ×5, P1 ×3)

| # | July item | Status | Evidence |
|---|---|---|---|
| 31 | P0 Split fillForm's unintended SetReadOnly side effect | **DONE** | `fillForm(path, data, out, lockFields, unsupported, jsFailures)` — an explicit `lockFields` parameter (`FormManager.cpp:620`); default-value path passes `false`. Pins: `TestFillFormLock::nonLockingFillKeepsFieldEditable`, `defaultFillStillLocks`. |
| 32 | P0 Make Radio/PushButton fillForm no-ops visible | **DONE** | Unapplied keys (unknown names and Radio/PushButton) are appended to `unsupportedFields` (`FormManager.cpp:811-816`), and the import path surfaces them in an "Import Incomplete" warning listing every dropped field (`FormsController.cpp:209-218`). Pin: `TestFillFormNoOp::unknownFieldNameIsReported`. |
| 33 | P0 Persist Required flag and Tooltip (/Ff bit + /TU) | **DONE** | Snapshot/commit writes `/TU` and `Ff |= 2` (`FormManager.cpp:345-355`) with removal-on-clear. Pins: `TestFieldMetadata::requiredAndTooltipPersist`, `clearingMetadataWorks`. |
| 34 | P0 Move Calculated field into the standard Forms surface | **DONE** | `FormsController::handledTools()` includes `ToolId::CalcField` (`:40`, case `:91`); `MenuBar.cpp:128` routes the `calc-field` alias through the CommandRegistry (the old hard-disable is gone). Pins: `TestMenuBarIntegrity`, `TestCommandRegistry`/`TestControllers`. |
| 35 | P0 Real content-aware auto-detect or honest gating | **DONE** (real heuristic + review-before-commit) | `AutoDetectPlacement` + label-pattern detector: "Detect → preview (heuristic disclosed, rows editable) → undoable commit" (`EditController.cpp:672`); detections join one compound undo (CHANGELOG [1.5.0]). Pins: `TestAutoDetectHeuristic::labelPatternYieldsSuggestions`, `plainPageYieldsNoFakeFields` (the July "3 hardcoded dummy fields" behavior is now a named failure), `suggestionSitsUnderTheLabelNotMirrored`. |
| 36 | P1 Move tab order off /CO onto a spec-correct mechanism | **DONE** | `FormManager::setTabOrder` rewritten (R18(b), `FormManager.cpp:1776-1800`): reorders the page's `/Annots` widget order + writes `/Tabs /W` only when no author declaration exists; **never touches /CO** (calculation order left to the form-JS cascade). Pin: `TestFormSafety::setTabOrderSameFileNoLeftovers` (the old /CO pin was deliberately flipped). |
| 37 | P1 A real Digital Signature field type distinct from Text | **DONE** (wave 2, `38102a23`) | `FormBuilderMode.cpp:584`: `case ToolMode::FormAddSignature: return FieldType::Signature` — real `/FT /Sig` field (unsigned, spec-basic /SigFieldLock, dedicated save path, `IFormManager.h:92`); the `// Sig uses text box` mismatch is gone (Button keeps the text-box stand-in by its own row). |
| 38 | P1 Harden the CSV/FDF import parser to export-side rigor | **OPEN** | `FormManager::importFormData` (`:1447-1477`) still uses an FDF regex + `split("\",\"")` CSV scan; no quoting/escape rigor; zero test coverage for malformed imports (grep for `importFormData` in tests = 0). Mitigation present but partial: imports route through the R01 transaction (`:1480-1482`) and unknown/unsupported names are reported (row 32). |

### §9.7 E-signatures (P0 ×3, P1 ×3)

| # | July item | Status | Evidence |
|---|---|---|---|
| 39 | P0 Typed + image-upload signature modes | **DONE** | `ToolMode::AddSignatureTyped/AddSignatureUpload` (`PdfEnums.h:45-46`); persisted as `/Stamp` annotations with a real image appearance stream (`PoDoFoBackend.cpp:4981-4984`); `SignaturePicker` 3-tab UI. Pins: `TestSignaturePicker::typedSignatureRendersInkAndPersists`, `uploadedSignatureSurvivesRoundTrip`, `visibleTabsDispatchTheirOwnKind`, `hugeUploadedImageIsCapped`. |
| 40 | P0 Signature appearance/design step for the cryptographic path | **DONE** | `SignatureManager.cpp:1044+` ("§9.7 P0 — visible signature appearance, ETSI EN 319 142-6 §5.2"): identity/CSR/date/reason/location lines, auto-fit, drawn before Sign so it lands inside the signed revision. Pins: `TestSignatureAppearance::roomyRectKeepsAllLines`, `autoFitDropsReasonFirstThenLocation`, `signAddsFormAppearanceWithAllLines`, `signWithoutReasonLocationOmitsLines`. |
| 41 | P0 On-page validity badges + Validate All Signatures | **DONE** | Badges anchored to actual field rects, incl. two-page mode and tooltip hit-test (`TestSignatureBadges::badgePaintedAtFieldCorner_singlePageMode`, `badgePaintedInTwoPageMode`, `badgeTooltipPayloadAndHitTest`, panel-mapping quartet); Validate-All summary (`TestValidateAllSignatures::summaryCountsValidAndListsEach`, `unknownSignerAndTrustAreHonest`). |
| 42 | P1 Initials variant | **DONE** | `SignaturePicker.cpp:242-262` Initials tab (§9.7 P1 marker in code), compact point size, dispatched through the typed path. Pins: `initialsVariantRendersCompactInk`, `initialsTabAndPlacementUseTypedPath`. |
| 43 | P1 Session signature cache across placements | **DONE** | Cache follows document switches and reuse re-enables signing (CHANGELOG [1.5.0]). Pins: `TestSignatureSessionCache::storeAndReuseLastSignature`, `sessionSwitchAtoBtoAClearsWithoutPicker`, `reuseGateEnablesOkOnEmptyTab`. |
| 44 | P1 Surface SignOutcome degradation at signing time | **DONE** | `SignatureDialog::attainmentWord(SignOutcome, requestedLevel)` (`SignatureDialog.cpp:154-170`); the SecurityController sign path carries the outcome and the UI names exactly what is missing. Pins: `TestSignatureBadges::signingOutcomeWarningNamesExactMissingPieces`, `signingOutcomeWarningVerbAndNonDegradation`. |

### §9.8 Redaction (P0 ×4 incl. 1 marketing, P1 ×5)

| # | July item | Status | Evidence |
|---|---|---|---|
| 45 | P0 Wire "Clear Marks" | **DONE** | `RedactMode.cpp:601` ("Audit 9.8 P0: make Clear Marks actually remove placed redaction marks"); button built at `:151`. Pins: `TestRedactClearMarks::clearRemovesOnlyRedactMarks` (non-redact marks survive), `clearWithNoMarksIsSafe`. Carrier `082f7926`. |
| 46 | P0 Expose Mark Region / Mark All in RedactMode | **DONE** | In-mode marking with explicit page lists: `TestRedactMarkAll::markAllPlacesMatchRects`, `markRegionSetsRedactToolMode`, `rangeMarksExactlyTheListedPages`, `invalidRangeMarksNothing` (an invalid range marks nothing instead of falling back to all pages). |
| 47 | P0 Bundle an optional sanitize pass into Apply | **DONE** | Sanitize-on-save checkbox, default ON, shared by both entry paths (`SecurityController.cpp:56` `static_assert(kDefaultSanitizeOn)`; `TestRedactSanitizeBundle::sanitizedCopyIsClean`, `TestRedactMarkAll::sanitizeCopyCheckboxProducesCleanOutput`, `sanitizeUncheckedKeepsMetadata`). The operation runs as one transaction with a SHA-256 check that the source is unchanged (CHANGELOG [1.5.0]; `TestRedactTransaction` 42P). |
| 48 | P0 Market the fully-local redaction pipeline | **GROUP** | Delivered in-product: `TestRedactMarkAll::redactPanelShowsLocalClaim` pins the panel's local-processing claim; see §3.4. |
| 49 | P1 Cancel/Back control in RedactMode | **DONE** | `RedactMode.h:48` ("§9.8 P1: the panel's Cancel/Exit control — the mode-exit contract"). Pins: `TestRedactMarkAll::cancelControlEmitsExitRequestedAndKeepsMarks`, `exitRequestedDisarmsMarkingToolAndKeepsMarks`. |
| 50 | P1 Optional overlay text / reason on the black box | **DONE** | `RedactApplyDialog.h:77` (`m_overlayEdit`, "§9.8 P1") → `RedactOperation::overlayText` (`RedactOperation.h:67`), carried through both entry paths (the dropped-field regression is itself named in `RedactApplyDialog.h:85`). Pin: `TestRedactMarkAll::applyDialogOverlayTextIsBurnedIntoSavedPdf`. |
| 51 | P1 Word-list import for pattern redaction | **DONE** | `RedactMode.cpp:196-257` (`onImportWordList`, `.txt` → escaped alternation, review before mark). Pins: `TestRedactMarkAll::wordListImportBuildsEscapedAlternation`, `wordListImportRefusesOversizedFile`, `importListButtonSitsNextToRegexEdit`. |
| 52 | P1 Per-file confirmation before batch OpRedact overwrites | **DONE** (superseded by a stronger design) | U2 rename-on-conflict: staging de-conflicts to the first free `stem-N` so a run "never [produces] a silent overwrite" (`BatchMode.cpp:1007-1033`); the explicit-overwrite path asks "Output file already exists … Overwrite it?" (`:1048-1051`). Cross-file collision regressions pinned by `TestPgr35BatchCollision` (PGR-42). |
| 53 | P1 Audit log records which pattern/category matched | **OBSOLETE** (superseded by the F-05 privacy decision) | The opt-in log exists (`PoDoFoBackend.cpp:2924-2932, 3050+`, `GLYPHPDF_REDACTION_AUDIT=1`, app-data location) but **deliberately records only the region count + after-hash** — coordinates and pre-image hash were removed because they fingerprint the original (F-05). Recording "which pattern matched" would reintroduce exactly that leak; a category-count variant would need a fresh privacy review. |

### §9.9 Page Management (P0 ×4 incl. 1 marketing, P1 ×5)

| # | July item | Status | Evidence |
|---|---|---|---|
| 54 | P0 Drag-and-drop reorder on the thumbnail grid | **DONE** | `m_pageList` is a `PagesGridWidget` in `IconMode` with `setDragDropMode(InternalMove)` (`PagesMode.cpp:440-453`); the drop is converted into the atomic permutation command. Pins: `TestPagesMode::internalMovePushesAtomicPermutation`, `keyboardMoveUsesDragCommandPath`, `selectionAndCurrentRowRestoredAfterUndo`. (CHANGELOG [1.5.0]: "fixed drag-reorder (silently dead in the live path)".) |
| 55 | P0 Surface real merge success/failure | **DONE** | `TestMergeSuccess::mergeReturnsFalseOnBadInputs`, `mergeReturnsTrueAndWritesOutput`; batch twin `TestSep13LeadBatchMerge` (PGR-12 CRITICAL: batch merge reported success with no file — fixed on line). |
| 56 | P0 Consolidate the two reorder commands into one | **DONE** | Only `ReorderPermutationCommand` remains (`cb635a84` retired `ReorderPageCommand` sources); `TestPagesMode::testMovePermutationConsolidation` asserts a drag move pushes the atomic path ("the legacy ReorderPageCommand path is retired", `TestPagesMode.cpp:591`). |
| 57 | P0 Market the offline/local-first page-management surface | **GROUP** | In-product notices shipped (Welcome card tooltips, export notices); see §3.4. |
| 58 | P1 Move Merge under the Pages/Organize group | **DONE** | `RibbonModel::makeOrganize()` — the Organize tab's "Document" group holds Split/Merge/Extract (`RibbonModel.cpp:155-157`). |
| 59 | P1 Multiple output parts from one split-by-range | **DONE** | `"1-3,4-6,7"` → `<stem>_part1..3.pdf` (`PagesMode.cpp:561-582, 965-982`); real execution writes the parts (the production path previously wrote none — CHANGELOG [1.5.0]). Pin: `TestPagesMode::verifySplitPart` helper + split tests. |
| 60 | P1 Page Labels mode (styles/sections) | **DONE** | Groundwork shipped: `core/PageLabels.{h,cpp}` (style parser incl. Roman, pure seams) + `TestPageLabels`; writer and UI were NOT deferred — `writeNumberTree` + `PagesMode::onApplyPageLabels` landed 2026-09-23 (`1991d9c1`, in the v1.5.0 tag); the CHANGELOG [1.5.0] scoping note was factually false and was corrected (`bbfd858b`, lane #11 CHECK-FIRST 2026-10-01). Scope kept honest: one uniform range, no /P prefix yet. |
| 61 | P1 Bates numbering across document batches | **DONE** | The presets Bates lane runs ordered across files with run-start pinning and truthful failure semantics. Pins: `TestBatesCrossDoc::twoDocBatchContinuity`, `threePageDocContinuity`, `emptyRangeKeepsCounterSafe`; `TestBatesBatchSafety`, `TestBatchPresetsP2::batesFaultAtFileTwoKeepsFileOneCommitted`. |
| 62 | P1 Unit tests for Bates/header-footer/crop/resize/merge | **DONE** | `TestBatesCrossDoc`+`TestBatesBatchSafety`, header/footer and crop/resize/merge exercise through `TestPagesMode` (`testSplitAtPage`, `testReorderPages`, `testAtomicReorder`), `TestMergeSuccess`, plus `TestPdfEditorInterface`. (Not exhaustive per-op matrices, but the July "zero automated coverage" state is closed.) |

### §9.10 Document Comparison (P0 ×2, P1 ×4)

| # | July item | Status | Evidence |
|---|---|---|---|
| 63 | P0 Wire `compareFiles()` into a file-picker entry point | **DONE** | `MenuBar.cpp:370` "&Compare Documents…"; `CompareMode.cpp:796-806` dual `QFileDialog` → `compareFiles(a, b)` (with same/missing-file guards at `:793`). Verified the July bug was real: at the July-1 tree `compareFiles` had zero callers in `src/`. Pins: `TestCompareEntry::distinctExistingFilesAreComparable`, `sameFileIsRejected`, `missingFileIsRejected`. |
| 64 | P0 Change-type filter on the CHANGES tree + text-diff panel | **DONE** | `CompareWidget::setChangeFilter` (`CompareWidget.cpp:134-141`), filters gate both panes. Pins: `TestCompareEntry::changeTypeFilterSeamCountsVisibleRows`, `filterTogglesExistCheckedByDefaultAndWired`, `togglingFiltersHidesRowsImmediatelyWithoutMutatingResult`, `filteredChangeCountMatchesRowsVisibleForFiltersForAllCombinations`, `zeroFilterSaysNoMatchNeverIdentical`. |
| 65 | P1 Progress bar + Cancel for long comparisons | **OPEN** | No progress/cancel in `CompareMode.cpp` (grep for progress/cancel = 0 matches). The run is synchronous behind the entry point. |
| 66 | P1 Integration tests for `DiffEngine::compare` end-to-end | **DONE** | `TestDiffEngine`, `TestCompareIntegration` (8P), `TestCompareEntry` (26P), plus the perf regression pin `TestSep13LeadComparePerf` (16× separation, negative control; CHANGELOG [1.5.0]). |
| 67 | P1 Explicit rows for added/removed pages | **DONE** | Structural change rows with side names, filter tag, status totals, navigation, reports (CHANGELOG [1.5.0] Compare block). Pins: `TestCompareEntry::structuralRowsAppearWithOwnFilterTagAndSideNames`, `structuralFilterSeamCountsAddedRemovedRows`, `statusTotalsCountStructuralChanges`, `treeSelectionJumpsToStructuralChangeInSharedSequence`. |
| 68 | P1 Market the offline/local-first comparison guarantee | **GROUP** | see §3.4. |

### §9.11 Security (P0 ×3, P1 ×4)

| # | July item | Status | Evidence |
|---|---|---|---|
| 69 | P0 Watermark font family must apply | **DONE** | `PoDoFoBackend.cpp:1442-1443` honors `fontFamily` (empty → Helvetica fallback documented at `:1475`). Pins: `TestWatermarkFont::testCourierFamilyIsAppliedAndReferenced`, `testEmptyFamilyFallsBackToHelvetica`. Carrier `872986bf`. |
| 70 | P0 Center with real font metrics, not char-count | **DONE** | `GlyphAdvanceCalculator` (PoDoFo glyph-width API) used by the watermark writer (`PoDoFoBackend.cpp:10,1387+`). Pin: `TestWatermarkFont::testCenteringUsesRealFontMetrics`. |
| 71 | P0 Wire `setExpiryDate` into the UI | **DONE** | `SecurityController::setExpiryDocument` — "Set Expiry Date" dialog → `engine->setExpiryDate(inputPath, date, inputPath)` (`SecurityController.cpp:444-475`), dispatched from the menu (`:444`). Pins: `TestExpiryInterface::writesAndReadsBackExpiryMarker`, plus the v1.5.0 SafeSave fix `markerSurvivesTheSaveTimeModDateRefresh` and the §1.3 XMP pin `expiryMarkerSurvivesADocumentPropertyEdit` — i.e. PROGRAM-CONSOLIDATION §1.3 is closed by this pin too. |
| 72 | P1 "Selective remove" mode for Sanitize Document | **OPEN** | `sanitizeDocument()` is all-or-nothing: file dialog → progress → `engine->sanitizeDocument(outPath)` (`SecurityController.cpp:684-715`); `SanitizeDocumentHelper` has no category selection; no selective UI anywhere. |
| 73 | P1 Pre-commit summary/count of what Sanitize will remove | **OPEN** | Same flow — no found-items summary dialog before commit. |
| 74 | P1 Deepen expiry enforcement beyond the viewer flag | **DONE** | `EditPolicy::isMutatingTool` includes Encrypt, Password, Sign, Sanitize, ApplyRedact, Permissions, RemoveSecurity, ExpiryDate, CertEncrypt ("re-encrypts the session document"), Certify, Pattern/RegexRedact (`EditPolicy.h:104-129`); the session flips read-only from the recovered/loaded expiry (`GpMainWindow.cpp:805, 1073`); every direct mutation entry calls `mutationBlocked()` (the header names each route). Pins: `TestReadOnlyGate`, `TestCommandBinding`. Deliberate carve-out, documented in the same header: SaveAs/export/print stay available (an escape hatch, not an omission). |
| 75 | P1 Bundle a static/vendored 7z library | **DONE** (2026-10-01) | 7-Zip 26.02 x64 committed at `third_party/7zip/bin/` (pinned SHA-256 `83967f1b…`/`69fd4df0…`, configure-time FATAL on drift; installer `6745fa76…` identical from www.7-zip.org and github/ip7z; provenance + LGPL/BSD/unRAR license records committed). Runtime resolves app-owned-first (`SafeSave::locateSevenZip`), system install only as fallback; no system 7z.exe required. Invoked subprocess-only (LGPL aggregation, veraPDF pattern). Pins: `TestSevenZipBundle` (hash+license record, resolution preference, honest absence, bundled-binary stdin-password end-to-end); existing `TestControllers`/`TestEncryptedPackageSafeWrite` real-7z legs now run the bundled copy. 26.03 bump = deliberate owner follow-up (its CVE-2026-58052 fix is extraction-side; our surface is `7z a`/`7z t`). |

### §9.12 Batch & automation (P0 ×4, P1 ×6 incl. 1 marketing)

| # | July item | Status | Evidence |
|---|---|---|---|
| 76 | P0 Fix engine-mutex false parallelism | **DONE** (as "no unjustified serialization"; full per-file engine isolation was not the chosen route) | Convert runs mutex-free (`BatchMode.cpp:1955-1964`, "convertTo is specified as stateless — no mutex needed"); only OCR + the PDFium-probe stay serialized with a stated thread-safety rationale (`:1970-1985`, shared `OcrEngine` is not thread-safe); the preset chain holds the mutex only around the candidate-validation probe (`:1467-1472`). Scheduling is the `LaneScheduler` (GPU lane serializes, CPU lane parallel, warm reuse, cancellation — `TestLaneScheduler` 8 pins). The July complaint was waste-without-benefit; what remains is benefit-with-rationale. |
| 77 | P0 Single load/redact/save pass for multi-pattern redaction | **DONE** | `applyPatternRedactionsMulti` — `BatchMode.cpp:2251-2255` ("§9.12 P0: collapse N patterns into a single load/find/apply/save"). Carrier `508f86e9`. Pin: `TestBatchMode` redact ops; `TestPatternRedact` 8P. |
| 78 | P0 Low-confidence-word flagging in batch OCR output | **DONE** | Per-file notes count and list low-confidence words/pages. Pins: `TestBatchOcrConfidence::noNoteWhenAllConfident`, `noteCountsLowConfidenceWords`, `noteListsAffectedPages`, `noteIsOneBased`, `emptyResultsProduceNoNote`. |
| 79 | P0 OCR language selection in Batch UI | **DONE** | `m_ocrLanguage` combo, persisted-pref default, 12 languages (`BatchMode.cpp:459-468`, "§9.12 P0" comment). Pins: `TestBatchOcrLanguage::ocrPanelHasLanguageCombo`, `comboListsAllTwelveLanguages`, `comboDefaultsToPersistedPreference`, `uiCodeMapsToEngineCode`. |
| 80 | P1 Named redaction pattern presets (PII quick-picks) | **DONE** | Email / Phone (US) / SSN built-ins resolved through `PatternRedactor::namedPattern` (`BatchMode.cpp:538-540, 2860-2873`). Pins: `TestBatchMode` preset rows; `TestPatternRedact` covers the named patterns. |
| 81 | P1 Compress/Optimize target DPI user-configurable with presets | **DONE** | Same artifact as row 29 (`m_dpiPresetCombo` + R26 schema). Cross-referenced: one implementation closes both the §9.5 and §9.12 rows. |
| 82 | P1 Merge onto the async pipeline with real progress | **DONE** | Merge runs on the QtConcurrent pool via `startMergeWorker` with per-file cancellation polls and PoDoFo-based per-file loop for counter accounting (`BatchMode.cpp:1591-1598, 1857-1872, 2390-2398, 2430`). Pins: `TestBatchMode`, `TestBatesBatchSafety`, `TestBatchPresetsP2::batesLaneCancelAtBoundaryIsTruthful`. |
| 83 | P1 Automated batch test coverage (Merge/OCR/Redact/Compress/Watermark/Export-PDF/A) | **DONE** | `TestBatchMode`, `TestBatchOpsCoverage`, `TestBatchPresets`+`P2`, `TestBatchOcrLanguage/Confidence/SkipText`, `TestBatesBatchSafety`, `TestPgr35BatchCollision`, `TestPgr45*` progress pins. |
| 84 | P1 Recursive hot-folder watching + polling fallback | **OPEN** | `m_hotFolderWatcher` is a plain `QFileSystemWatcher::addPath(dir)` (`BatchMode.cpp:886-890`) — non-recursive, no polling fallback anywhere in the file. |
| 85 | P1 Market the offline hot-folder + zero-cloud-fee story | **GROUP** | see §3.4. |

### §9.13 Compression (P0 ×4, P1 ×5 incl. 1 marketing)

| # | July item | Status | Evidence |
|---|---|---|---|
| 86 | P0 Real JPEG quality/DPI re-encoding behind the sliders | **DONE** | `optimizeDocument` Phase 1 decodes `/DCTDecode` streams via Qt and re-encodes honoring `options.jpegQuality` + target DPI, with mask/decode-array/CMYK safety guards (`PoDoFoBackend.cpp:6439-6513`). Pins: `TestCompressJpegReencode::qualityIsHonored`, `perImageScopingOnMultiPageDoc`, `malformedImagesAreSkippedSafely`, `mediaFilterChainImageIsSkipped`. |
| 87 | P0 Finish the duplicate-image dedup rewiring | **DONE** | The "last mile" is closed: references rewired to the canonical XObject, object removed. Pins: `TestImageDedup::duplicatesRewiredToCanonical`, `TestDedupSMask::identicalBytesDifferentSMaskAreNotMerged` (SMask-aware fingerprints), `danglingXobjectRefDoesNotFailDedup` (`TestCompressJpegReencode:462`). |
| 88 | P0 Compress metadata-strip must call the full sanitize logic | **DONE** | The strip path is the full sanitize (hidden-data removal incl. struct-tree cycle safety). Pins: `TestCompressStripSanitize::stripMetadataRemovesHiddenData`, `cyclicStructTreeDoesNotBreakSanitize`. |
| 89 | P0 Fix/remove size estimates that don't match the write path | **DONE** | Estimates never claim unimplemented passes and never mutate (`PoDoFoBackend.cpp:6207, 6324`; `TestOptimizeEstimate::estimateDoesNotClaimUnimplementedPassSavings`); the dialog disables unsupported passes with explanations and keeps "estimate" labeling distinct from the measured result (`TestCompressDialogHonesty::estimateOptionsNeverRequestUnsupportedPasses`, `sizeRowStaysLabeledAsEstimate`, `unsupportedPassExplanationIsHonest`). |
| 90 | P1 Real font subsetting behind the "Subset fonts" checkbox | **OPEN** (honest-disabled) | `CompressDialog.cpp:162-178`: checkbox visible, disabled, unchecked, with a "no subsetter in this build" tooltip; `TestCompressDialogHonesty::subsetFontsStaysDisabledUncheckedWithExplanation` + `presetsNeverReEnableOrReCheckUnsupportedPasses` lock the honesty. The capability itself does not exist (PDF/A export repairs existing subsets at `:3270-3276` but writes no subsetter). |
| 91 | P1 Unused/unreferenced object removal | **DONE** | "the unused-object sweep is now implemented (21a387c)" (`CompressDialog.cpp:181-185`), checked by default; estimates account for it. Pins: `TestOptimizeEstimate`, `TestCompressDialogHonesty::completionReportCarriesMeasuredFiguresAndDelta`. |
| 92 | P1 Real before/after size readout | **DONE** | Both sizes read from disk after the write, with delta, and an honesty note when not smaller (`CompressDialog.cpp:604+`; `TestCompressDialogHonesty::completionReportNotesWhenNotSmallerOrEqual`, `completionReportStaysDistinctFromEstimateLabeling`). |
| 93 | P1 Extend downsampling to Gray/CMYK/indexed raw streams | **PARTIAL** | Gray JPEG re-encode is handled (re-encode "must stay /DeviceRGB or /DeviceGray", `:6512`); **CMYK JPEGs are deliberately skipped** with a color-shift rationale (`:6512-6513`) and legacy uncompressed images convert to real JPEG; indexed raw streams are not covered. |
| 94 | P1 Market the offline compression angle | **GROUP** | see §3.4. |

### §9.14 Accessibility (P0 ×3, P1 ×4)

| # | July item | Status | Evidence |
|---|---|---|---|
| 95 | P0 Fix /Pg inheritance walk-up in pageIndexOf | **DONE** | `collectStructElems` threads `inheritedPage` down the tree; explicit `/Pg` wins, otherwise the nearest ancestor's page ("§9.14 P0", ISO 32000-2 §14.7.2 cited in-source, `PdfAValidationPanel.cpp:423-427, 470-472`). Carrier `88e9517e`. Pin: `TestReadingOrderInheritance::inheritedPgIsNotFlagged` (+ `untaggedPdfIsReported`, `issuePagesParallelArray`). |
| 96 | P0 Jump-to-page on each reported mismatch | **DONE** | The reading-order analyzer lives in the same panel as the veraPDF list and reuses its row pattern: per-finding JUMP button → `goToPage` with 1-based→0-based mapping (`PdfAValidationPanel.cpp:43-54`). Page parallelism pinned by `TestReadingOrderInheritance::issuePagesParallelArray`. |
| 97 | P0 Move analyzeReadingOrder off the UI thread | **DONE** | Checker runs via the panel's async worker (QFuture-style delivery). Pins: `TestReadingOrderAsync::checkRunsAsynchronouslyAndDelivers`, `noDocumentShowsMessageWithoutWorker`. |
| 98 | P1 Named, documented position-slots threshold + honest framing | **DONE** | The heuristic constant is named and equals 2 (`TestReadingOrderThreshold::toleranceConstantIsNamedAndEqualsTwo`, `displacementOfExactlyTwoIsNotFlagged`, `displacementOfExactlyThreeIsFlagged`). |
| 99 | P1 Extend BBox/position extraction to MCID spans | **OPEN** | `collectStructElems` still skips bare MCID integers ("e.g. a bare MCID integer — skip", `PdfAValidationPanel.cpp:462`); no marked-content BBox extraction anywhere. Cross-ref: PROGRAM-CONSOLIDATION §4 "Accessibility P3" (tables, lists, ActualText, multi-column) is the bigger umbrella. |
| 100 | P1 Persistent, exportable results panel | **OPEN** | `AccessibilityPanel` has no export/save of results (grep export/save/csv in the panel = tag-run and gate code only); results remain a session popup. |
| 101 | P1 Unit tests for analyzeReadingOrder/collectStructElems | **DONE** | `TestReadingOrderInheritance`, `TestReadingOrderThreshold`, `TestReadingOrderAsync`, `TestAccessibilityChecker`, `TestAccessibilityPanel`, `TestAccessibilityTagger`, `TestAccessibilityFixes`. |

### §9.15 Search & Navigation (P0 ×2, P1 ×3 incl. 1 marketing)

| # | July item | Status | Evidence |
|---|---|---|---|
| 102 | P0 Wire Match Case / Whole Words / Regex into document search | **DONE** (with an honest, disclosed limitation) | `FindBar` passes all three flags (`FindBar.cpp:180-195`) → `EditController::pageTextPattern` → `TextMatchFinder::buildPattern` (`:20-30`, `\b` guards, CaseInsensitiveOption), the shared matcher for search **and** redact-all. When any flag is set, QPdfSearchModel (case-insensitive substring only) is bypassed and matching pages are navigated with an explicit status note ("Page-level navigation … inline highlight unavailable", `EditController.cpp:323-331`). Pins: `TestSearchFlags` 7 slots (matchCase on/off, wholeWords rejects substrings, regex honored, invalid regex reported, wholeWords wraps regex). Carrier `40bb3624`. |
| 103 | P0 Wire the dead thumbnail zoom buttons | **DONE** | `ThumbnailSidebar` zoom in/out buttons built (`:92-95`) and connected; Pins: `TestThumbnailZoom::buttonsExistAndAreWired`, `zoomStepsChangeFactor`, `zoomClampsToBoundsAndDisablesButtons`. |
| 104 | P1 Thumbnails off the GUI thread + RenderCache locking | **PARTIAL** | Locking + concurrency: done and pinned (`TestThreadSafety::testRenderCacheConcurrency`, `testRenderCacheKeyHashEqualityInvariant`, `testRenderCacheNoDuplicateTilesUnderScroll`) with background prefetch + cancellation (`RenderCache::drainPrefetches`, `prefetchViewport`, lowest-priority pin `testPrefetchRunsAtLowestPriority`). **Remaining:** the visible thumbnail paint itself still renders synchronously through `viewer->renderPage` (`ThumbnailSidebar.cpp:35-46`) — large-document first-paint jank persists. |
| 105 | P1 Minimal automated coverage for search/thumbnail/bookmark/jump wiring | **DONE** | `TestSearchFlags`, `TestThumbnailZoom`, `TestOcrVerifyNavigation`, `TestHyperlinkNavigation`, `TestAutoBookmarks`, `TestTaskNavRegistry`, `TestScreenStateSync`. (The 1.3.2.2-era thumbnail-click dead signal class is guarded by `TestThumbnailZoom` + view pins.) |
| 106 | P1 Market the local search/navigation stack | **GROUP** | see §3.4. |

### §9.16 File import/export (P0 ×3, P1 ×3)

| # | July item | Status | Evidence |
|---|---|---|---|
| 107 | P0 Runtime badge: which export path actually ran | **DONE** | Engine tracking (`ConversionManager::lastWordExportEngine/lastExcelExportEngine`, `ConversionManager.h:77-79`) consumed at `ConvertController.cpp:156,224`; a `Fallback` result raises an explicit "Export Format Notice" warning naming the repair-prompt risk; success messages carry `localProcessingNotice()`. Pins: `TestExportPathBadge::wordExportTracksEngineUsed`, `excelExportTracksEngineUsed`, `capabilityQueriesReflectInHouseWriters`. (With the in-house writers the fallback no longer occurs in shipped builds — the badge is a tripwire.) |
| 108 | P0 Remove or wire up the dead "linearized" preset checkbox | **DONE** | Root cause at July-1 was a settings-key mismatch (panel stored `linearized` in the preset JSON; the save path read `export/linearizeOnSave`) — the qpdf implementation itself already existed (`PdfEditorEngine.cpp:158-192` at July-1), so the audit's "implement via qpdf" was half-done already; only the wiring was dead. Now: `planForExport` (pure, unit-tested) maps the preset onto the save steps (`HomeController.cpp:700-745`); `QpdfBackend::linearize` verifies with `qpdf_is_linearized` (`QpdfBackend.cpp:19-65`). Pins: `TestExportPresets::linearizedPresetRequestsLinearize`, `webOptimizedDefaultPresetIsLinearized`; capability probe `Capability.cpp:382-386,678`. Carrier `9f033c49`. |
| 109 | P0 "No internet, no upload" messaging on export/import | **DONE** (in-product copy; broader marketing grouped) | `localProcessingNotice()` on every export success (`ConvertController.cpp:28,165`); Welcome cards carry "Processed 100% locally — no internet, no upload." (`WelcomeWidget.cpp:301`); the redaction panel's local claim is test-pinned (`TestRedactMarkAll::redactPanelShowsLocalClaim`). |
| 110 | P1 Unify Office/image import into Open + drag-and-drop | **DONE** | One routing matrix with drop plans: `GpMainWindow::planDrop` (`:1612`) + connect at `:1568`; Office/images route through the same open coordinator. Pins: `TestOpenRouting::routingMatrix`, `routingIsCaseInsensitive`, `routingFullExtensionSets`, `dropPlanPdfWins`, `dropPlanImagesCombine`, `dropPlanMixedAndUnsupported`; Welcome cards wired (`WelcomeWidget.cpp:244-334`). |
| 111 | P1 OCR output-mode + language picker in the OCR/scan-import UI | **PARTIAL** | Language: done — `OCRMode` language combo (`ocrLangCombo`, `OCRMode.cpp:112-115`, shared `ocr/language` key) + ribbon entry (`RibbonModel` OCR tab). **Remaining:** the OutputMode choice (row 20) does not exist, so there is nothing to surface. |
| 112 | P1 Round-trip tests for bookmark and hyperlink preservation on Save | **DONE** | `TestLinkBookmarkRoundTrip::plainSaveRoundTripPreservesBookmarksAndLinks`, and the known deliberate strip is pinned as intentional and visible: `sanitizeStrippingIsIntentionalAndVisible`. |

### 3.4 Marketing / positioning batch (the 7 GROUP rows: §9.8-P0, §9.9-P0, §9.5-P1, §9.10-P1, §9.12-P1, §9.13-P1, §9.15-P1)

July's per-domain "market this explicitly" rows are tracked as one workstream.
(The §9.16 P0 "no-upload badge" row is NOT in this batch — its in-product half
shipped and is test-pinned, so it is DONE as row 109.)

Delivered in-product (verified):
- `ConvertController::localProcessingNotice()` on every export success (`:28,165,223`);
- Welcome cards: "Processed 100% locally — no internet, no upload." (`WelcomeWidget.cpp:301`);
- Redaction panel local-processing claim, test-pinned (`TestRedactMarkAll::redactPanelShowsLocalClaim`);
- Capability-registry disclosure consumed by Compress/Convert/Batch/signature picker (`core/Capability.*`, `TestCapabilityRegistry`);
- `marketing/` exists (website-copy.md, press-kit/product-summary.md, logos, demo-script, screenshots).

Remaining for the marketing workstream (copy tasks, tracked separately):
the About-dialog badge set, release-notes parity callouts, and a
landing-page comparison table are **copy-completeness tasks**, not
engineering — OUT OF SCOPE for code verification; not graded here.

---

## 4. The definitive still-open list

Ranked by user-visible value. Cross-references to
PROGRAM-CONSOLIDATION-2026-09-25 (§1 code fixes, §2 owner decisions,
§4 backlog) mark duplicates so future lanes dispatch once.

| # | Item | Source | Value / why first | Effort | Dispatch target |
|---|---|---|---|---|---|
| 1 | **QuadPoints text-anchored highlight/underline/strikeout/squiggly** | §3 row 15 (July P1) | The single biggest remaining *quality* gap vs every competitor: free-rect markup still highlights blank space and misses wrapped lines. Everything else it needs exists (`TextMatchFinder` rects, `applyAnnotationsToDoc` writer). | L | `PoDoFoBackend::applyAnnotationsToDoc`/`extractAnnotations` (new /QuadPoints write+read), `AnnotationLayer` placement seam; pins in `TestShapeInkPersistence` pattern |
| 2 | **Font subsetting for Compress** — INVESTIGATION COMPLETE, owner decision pending | §3 row 90 (July P1) | Decision-grade plan landed: `docs/research/font-subsetting-plan-2026-10-01.md` (`f3c7cb88`, wave-2b investigation lane). Key finding: PoDoFo 1.1.0's public API cannot subset an existing document's fonts without renumbering CIDs (content-stream rewrite blast radius). **Owner picks: (A)** zero-new-deps in-tree keep-CID blank-glyph subsetter (M for TrueType/FontFile2 — the CJK bulk; L with CFF; Type1/signed skipped+disclosed), **(B)** HarfBuzz hb-subset with RETAIN_GIDS (better-tested; NEW MIT dep ~3.5-4.5 MB incl. glib — needs the quickjs-style owner exception), **(C)** keep the honesty gate (S: fix the stale "or unused-object removal" tooltip wording). | L | see the plan's route sketches + test plan (honesty-pin flips, TestFontSubset round-trip + render-diff) |
| 3 | **MCID-level reading-order extraction** | §3 row 99 (July P1; dup of PROG-CONSOL §4 "Accessibility P3") | Makes the a11y checker detect true text-level order problems, not just figure/table reorder; the checker/Tag-Document stack is otherwise landed. | L | `PdfAValidationPanel.cpp:462` (stop skipping MCID ints), `AccessibilityChecker`; BBox via marked-content |
| 4 | **Selective Sanitize + pre-commit summary** | §3 rows 72-73 (July P1 ×2, one work item) | Acrobat/Foxit parity for the single most compliance-sensitive dialog; `sanitizeDocument` already walks every category — this is classification + UI. | M | `PoDoFoBackend::sanitizeDocument` (category params), `SecurityController::sanitizeDocument` (summary dialog + category checkboxes) |
| 5 | **OCR OutputMode (searchable vs editable) + exposure in import/export UI** | §3 rows 20, 111 (July P1 ×2) | Closes the PRD-called-out gap; the MRC writer and the review-doc pipeline both exist — it is a mode switch + two dialogs. | M | `EditController::onOcrAcceptRequested` (mode branch), `OCRMode` UI, `ConvertController` export dialogs |
| 6 | **Signature form field type distinct from Text** | §3 row 37 (July P1) | A customer-visible functional mismatch (`// Sig uses text box`); the cryptographic machinery (`SignatureFieldCreator`) already exists to model it on. | M | `FormBuilderMode.cpp:569`, `AddFormFieldCommand::FieldType`, `FormManager::addFormField` (/FT /Sig), `TestFormBuilder` |
| 7 | **Compare progress + cancel** — **DONE** | §3 row 65 (July P1) | Lane folded `2cb94e9c`/`a7833a25`/`276514a3` (wave 2b; the original lane died mid-implementation, a finisher continued its coherent design): `DiffEngine::compare` gains a default-off progress probe (extraction + page-pair boundaries, batch file-boundary semantics); `CompareMode::compareFiles` runs on the QtConcurrent pool (QPromise overload) behind a per-run NON-MODAL QProgressDialog whose Cancel drives the watcher (worker polls isCanceled at the engine's own boundary probes — BatchMode idiom); cancelled runs discard partial results by construction; re-entry refused; the dialog is destroyed after completion/cancel (finisher upgraded two raw-pointer pins to QPointer — they were a real UAF). R7: NC compile-red at the hook seam once, ×3 serial across 4 touched suites (83/83 final gate). | M | done |
| 8 | **CSV/FDF form-import parser hardening** | §3 row 38 (July P1) | Import-side parser is regex/split while every export path was hardened (PGR-16/17 class); malformed FDF is attacker-reachable input. | M | `FormManager::importFormData:1447-1477`; reuse `ConversionManager::csvFormulaSafeCell` discipline; new pins beside `TestFillFormNoOp` |
| 9 | **Recursive hot-folder watch + polling fallback** | §3 row 84 (July P1) | Enterprise watched-folder robustness (network shares, nested dirs) on an otherwise-differentiating feature; PROG-CONSOL §4 lists `HotFolderController` (needs the `TestHotFolder` characterization pin first). | M | `BatchMode.cpp:886-890` → extract `HotFolderController` per PROG-CONSOL §4 |
| 10 | **Batch format list parity (Text, PowerPoint)** — **DONE** | §3 row 28 (July P1) | Lane folded `9ccc2d82`/`e43ecb9e` (wave 2b, integrator-finished — the agent completed its R7 campaign but died pre-commit): Text/PowerPoint combo rows appended position-stably after the existing five, extension tables extended, same `IConversionEngine::convertTo` path (no parallel impl); pins cover both formats end-to-end through the real worker. | S | done |
| 11 | ~~Page Labels writer + UI (groundwork landed)~~ **CLOSED — already shipped in v1.5.0** | §3 row 60 (July P1) | Lane #11 CHECK-FIRST (2026-10-01): `writeNumberTree` both overloads + `PagesMode::onApplyPageLabels` dialog + round-trip pins all landed 2026-09-23 (`1991d9c1`, present in the v1.5.0 tag); scorecard row and the CHANGELOG [1.5.0] "deferral" were stale. Honest scope kept: one uniform range, no /P prefix (owner follow-up). | M | none — folded as docs corrections `bbfd858b`/`3a6a044f`; stale-row sweep this commit |
| 12 | Re-OCR region scoping — **DONE** | §3 row 21 (July P1) | Lane folded `392c57d9`/`f64777cd`/`a0c68823` (wave 2b): drag a rectangle on the OCR review source-image canvas → bbox in `LayoutRegion::bbox` pageImage pixel space → "Re-OCR this region" runs the EXISTING pipeline over the crop only, word boxes mapped back to page space, same `OcrReviewSession` records as whole-page; honest failures (degenerate/outside region refused pre-dispatch, nothing-recognized = typed `ocrRunFailed`, review intact, never a silent whole-page fallback); stale regions invalidated on fresh deliveries. R7: RED 11F/8P, NC exact-reproduction once, ×3 serial 19/19, 15/15 OCR-suite touched gate. The old "until region OCR ships" disclosure now teaches the gesture. | M | done |
| 13 | Extend downsampling to CMYK/indexed (Gray done) — **indexed DONE; CMYK honestly blocked** | §3 row 93 (July P1) | Lane folded `af2604bd`/`2af5bdf7` (wave 2b): `/Indexed` 8bpc over RGB/Gray bases decodes (palette pre-parsed, OOB-guarded on crafted /hival), downsamples, re-encodes as true DeviceRGB/Gray DCTDecode JPEG; stay-expanded (no in-tree quantizer; documented). CMYK stays skipped on BOTH doors with verified cause — no color-managed decode exists in-tree (no lcms2 anywhere; Qt JPEG read is a naive conversion = the exact silent recolor the guard prevents) — and pins go RED if the skip is lifted without color management. **Owner item:** vendored lcms2 (new-dep exception) or a product decision to keep the skip. | M | `PoDoFoBackend.cpp` optimizeDocument Phase-1 guard site |
| 14 | ~~Vendored/static 7z~~ **CLOSED — vendored 7-Zip 26.02 committed with pinned SHA-256 + app-owned-first resolver (`d41979c1`, lane 2026-10-01)** | §3 row 75 (July P1) | Removes the last external-binary dependency contradicting the offline pitch. | L | `SafeSave::locateSevenZip` (bundled → PATH → Program Files; empty = honest disclosure), `third_party/7zip/` (committed binaries, dual-source-verified installer hash, provenance + license recorded, configure-time `file(SHA256)` pin), `TestSevenZipBundle`; evidence `docs/audit/evidence-vendored-7z/`. Owner follow-ups: 26.03 bump procedure (CVE-2026-58052 is extraction-side; unused by our `7z a`/`t` surface), CapabilityRegistry `EncryptedPackage` row, `.ts` rewording |
| 15 | Exportable accessibility results panel — **DONE** | §3 row 100 (July P1) | Lane folded `7f1b70ed`/`4845a142` (wave 2b): `A11yReportWriter` (CSV via the shared `csvFormulaSafeCell`+RFC-4180 contract; print-ready PDF summary through SafeSave with sheet footers), panel rows armed only for the current document identity, honesty pinned (failed scans export as failed; "accessible"/"conformant" never appear on zero findings; truncation disclosed). W1-04 standard-14 toolkit hoisted to `Standard14Text.h` (anti-drift, neighbors green). Limits: checker's existing check set (no new detection); Helvetica non-WinAnsi → "?" with in-artifact note. | S-M | done |
| 16 | Thumbnail render off the GUI thread (locking done) — **DONE** | §3 row 104 (July P1) | Lane folded `93e222ea`/`a602fa55` (wave 2b): `RenderCache::renderPageAsync` opens the prefetch machinery (token-epoch staleness, in-flight registry, lowest priority) as a general API — it had ZERO callers at base; `ThumbnailSidebar` renders async with honest blank placeholders, queued GUI-thread delivery; `renderPageUncached` thread-safe seam (pixel-identical, pinned); retirement reordered to EC06 (closes a latent UAF). Fail-before pinned workers in a semaphore to prove the old sync render; R7 ×3 + 4-suite ripple green. NOTE: compare-progress (row 7) can adopt `renderPageAsync` instead of inventing a worker. | M | done |
| 17 | OCR verify: disclose active preprocessing capability | §3 row 22 (July P1) | A build without Leptonica silently degrades preprocessing. | S | `OCRMode` checkbox tooltip ← `OcrPreprocessor` capability query |
| 18 | Stamp tool: image-as-stamp import — **DONE** | §3 row 16 (July P1) | Lane folded `2101f6e5`/`3c7838ee`/`88c01163` (wave 2b): "Import Image…" in `StampLibraryDialog` (pick → name → library; typed refusal for unusable images, no half imports — catalog owns a fully-decoded PNG copy under `stamp-images/`); `StampLibrary` image-variant slot with exactly-one-carrier validation + absolute/traversal tamper refusal (the lane's own tamper pin caught and fixed a carrier-validation bug mid-flight); `armDynamicStamp` reuses the signature-Upload `/Stamp`+image-AP writer unchanged; typed "re-import it" failure if the image is later deleted. | S-M | done |
| 19 | Depth-60 struct-walk cap configurable + truncation warning | July P2 (§9.14) | Ordered here because the a11y program is otherwise landed and this caps correctness on deeply-tagged docs. | S | `PdfAValidationPanel.cpp:465` (`depth > 60`) |
| 20-23 | P2 engineering tail (each verified individually; 31 P2 items = 9 marketing (grouped, §3.4 discipline) + 22 engineering: 4 DONE, 3 PARTIAL, 15 OPEN) | July P2 | **OPEN (15):** text-edit undo scoped diff (`PoDoFoBackend` still page-snapshots) · viewer dual-path consolidation note (absent; `PdfViewerWidget` QPdfDocument vs `PdfiumBackend`) · per-annotation-type visibility toggle (absent) · hot-folder processed-set TTL (absent) · Compress-to-target-size (absent) · space-usage breakdown (absent) · redact-box color config (absent) · extract options "delete after"/"one file per page" (absent) · header/footer Helvetica-fallback warning (absent) · flatten fidelity beyond hardcoded Helvetica (`FormManager.cpp:1493-1495`) · jump-to-page out-of-range feedback (absent; `goToPage` silently clamps) · bookmark/comment scope cycling (absent; each search restarts) · Office binary fixtures (`tests/fixtures/` has none — suites synthesize or fake soffice) · LibreOffice bundle-or-detect dead end (still required) · pixel-diff registration (text-fingerprint page alignment exists via R11; per-pixel registration does not). **PARTIAL (3):** interactive Accept low-confidence warning (`OcrConfidence` bands + legend and the batch note exist; no Accept-time warning) · images-import DPI/page-size UI (`ImageImportOptions` exists engine-side; the Welcome route passes defaults). **DONE (4):** OCSP test hook gated behind `GLYPHPDF_TESTING` (`SignatureManager.cpp:380,1785`; compiled out of shipped binaries per the 1.3.2 record) · multi-recipient cert-encryption recipient picker (N17: `CertEncryptController`, `RecipientPickerDialog`, `TestCertEncryptPicker`) · Open-vs-Permissions password labels (`EncryptionDialog.cpp:21-25`) · 2-step batch presets (U7 preset manager + multi-step editor, `TestBatchPresetsP2`) | S–L each | named inline |

Not duplicates — already closed elsewhere (do not re-dispatch):
PROGRAM-CONSOLIDATION §1.3 (XMP custom-entity drop) is closed by
`TestExpiryInterface::expiryMarkerSurvivesADocumentPropertyEdit`;
§1.4 (zoom-after-fit) remains open as the pinned expected failure
`TestViewParity::zoomInAfterFitLeavesFitMode`; §1.1 (3 CSV sinks) and §1.2
(OLE signature) remain open per that file and are NOT July items.

---

## 5. Method, coverage, and the July misaudits

### How this was verified
- **Code reading at `2ccfd5ba`** for every disposition: the producing function
  or `connect()` was read, not grepped-for (e.g. rotation, link-click, MRC
  accept, watermark metrics, tab-order rewrite, OOXML writers, dedup,
  reading-order walk).
- **Pins read at source:** every DONE names the test file and the slot names;
  assertions were read where the claim was load-bearing (rotation degrees,
  package structure, alpha value, threshold constants, false-positive
  negatives like `plainPageYieldsNoFakeFields`).
- **Git archaeology:** `git log -S` per artifact for carrier SHAs
  (`7ecc1aba`, `082f7926`, `21d826fa`, `d3dbde4f`, `88e9517e`, `508f86e9`,
  `872986bf`, `40bb3624`, `9f033c49`, `cb635a84`, `6aac22ca`, `e5a5f012`),
  plus the July-1 tree (`fd74f5dc`) probed directly for six dead-control
  claims (below).
- **Not run:** no build/test execution in this lane (docs-only mandate; the
  branch tip's gate — full serial 189/189 — is recorded in
  POST-CONSOLIDATION-ENDGAME-2026-09-30, and the v1.5.0 tag was verified
  188/188 serial + `-j6` before release). Pin *existence and semantics* are
  source-verified; pin *execution* is inherited from those recorded gates.
- **Not verifiable from code:** the outward marketing copy completeness
  (§3.4), and the CI-only flakes (§1.7 of PROGRAM-CONSOLIDATION) — left to
  their owners.

### July claims found already-true or misdirected in July (misaudits)
- **M-1 (§9.3 P0 toolbar consolidation):** the July item's premise — "users on
  the floating toolbar cannot discover 4 of 10 markup types" — was stale at
  writing time: the floating `AnnotationToolBar` had been removed from the
  build on 2026-06-22 (`6aac22ca`), verified absent from CMakeLists and
  `GpMainWindow` at the July-1 tree. The consolidation already existed; the
  September work (`e5a5f012`) added the *test* that pins it.
- **M-2 (§9.16 P0 linearize, partially):** the audit called the linearized
  preset "a UI control implying a real capability that does nothing" and
  recommended implementing via qpdf — but the qpdf path was already
  implemented and reachable at July-1 (`PdfEditorEngine.cpp:158-192` →
  `QpdfBackend::linearize`); the *checkbox* was dead due to a settings-key
  mismatch (`linearized` vs `export/linearizeOnSave`). The audit's framing
  overstated the gap; the real bug was one key name.
- **Confirmed accurate in July (checked, not misaudits):** OCR `eng` hardcode
  (`EditController.cpp:427` at July-1), zero `compareFiles()` callers, image
  commands include-only, `setExpiryDate` with zero shell callers, thumbnail
  zoom buttons present-but-unconnected, no two-page overlay compositing.

### Honesty notes
- Two DONEs are "done by a better mechanism than July specified" and say so:
  OOXML (in-house writers instead of landing duckx/OpenXLSX — the invariant
  "never mislabel" is what was owed) and the batch engine mutex (justified
  serialization + LaneScheduler instead of per-file engine instances).
- One DONE carries a disclosed functional limitation: flag-filtered document
  search navigates at page level ("inline highlight unavailable" status).
- Two open items are owner-gated *adjacent* but not blocked: FOLD-2
  (FineReader port, §2.6) would fold in OCR depth work relevant to rows 5/12;
  no July item is marked OWNER-DECISION because none is decided by §2 D1–D7.
