# LANE REPORT — reocr-region (wave 2b, lane #12)

Date: 2026-10-01
Branch: `feat/reocr-region` (base `ad77c29c`), worktree `D:/pdf/pdf-w2b-reocr`
Mission: PARITY-SCORECARD-2026-09-30 §4 row 12 — "Re-OCR region scoping —
Honesty fix shipped; the feature (map click → `LayoutRegion` bbox) is in-code
future work. Effort M. Dispatch target: `OCRMode.cpp:745`."
Status: **COMPLETE** — feature shipped, R7 gate green (RED → NC → pass ×3).

## CHECK-FIRST (prior aborted instance)

`git log ad77c29c..feat/reocr-region` was EMPTY at lane start (HEAD =
`ad77c29c`, working tree clean) — the provider-usage-cap abort left no
commits, no partial edits, no evidence dirs. Lane started clean from base.

## What shipped

The whole gap was real: `OCRMode::onImagePaneContextMenu` reset
`m_contextRegionBbox = QRectF()` on every menu open (before any action could
use it), and `GpMainWindow`'s `ocrReRunRegionRequested` lambda **discarded**
the bbox and called whole-page `runOcr()`. The menu label said "Re-OCR
entire page" with a "until region OCR ships" disclosure.

- `src/ui/OcrScanCanvas.{h,cpp}` — drag-select on the source page image
  commits a region in **pageImage pixel space** (the exact coordinate system
  `LayoutRegion::bbox` uses): ≥4 px movement turns a press into a region drag
  (a plain click stays the word-selection click, unchanged); new pure seam
  `imageRegionFor(widgetRect, image, pane)` maps widget → image space derived
  from the same `imageRectFor` source of truth as painting/hit-testing,
  normalized FIRST (right-to-left drags are legitimate — an `isEmpty()` check
  before `normalized()` threw them away; caught by a pin during the gate),
  clamped to the image, **empty when the drag misses the image** (garbage
  stays a refusal, never widened into whole-page). The committed region paints
  dashed; a new page image invalidates it (stale pixel space = garbage).
  `regionSelected(QRectF)` / `selectedRegion()` / `clearSelectedRegion()`.
- `src/modes/OCRMode.{h,cpp}` — `onScanRegionSelected` stores the bbox
  verbatim; the context menu no longer resets it and gains
  **"Re-OCR this region"** when a selection is committed; `onReOcrRegion`
  refuses a region request without a selection with a TYPED message
  (falling through would silently re-run the whole page, since empty bbox =
  whole page downstream); new `onReOcrWholePage` is the explicit whole-page
  entry (same SEP13 `ReviewReady` guard, always dispatches the empty bbox,
  never consumes a stored region); BOTH fresh-delivery paths
  (`setReviewSession`, `setOcrResults`) invalidate the stored region. The
  stale "Regional actions act on the whole page until region OCR ships" note
  now teaches the drag gesture (or confirms the committed scope) instead of
  denying the feature.
- `src/shell/controllers/EditController.{h,cpp}` — `runOcrRegion(regionBbox)`
  (separate slot: a PMF connect cannot adapt a wider slot signature to
  `ocrRunRequested()` — the default-arg overload form fails a Qt static
  assert; `runOcr()` delegates with the empty bbox). The user drag is treated
  as the `LayoutRegion` it is (bbox verbatim, `RegionType::Other`,
  confidence 1.0 — human-drawn); pure seams `ocrRegionCropRect` (normalize →
  snap to pixels → clamp to the rendered page; garbage = empty crop + typed
  reason) and `ocrRegionWordsToPageSpace` (crop-space boxes → pageImage
  space). The dispatch path: typed refusal BEFORE any dispatch for a region
  that cannot yield a crop (`_ocrRunning` restored, `ocrRunFailed` emitted);
  the worker runs the **EXISTING pipeline over the crop only** (same engine
  selection, same preprocessing prefs — `PreprocessedImage::inverseTransform`
  maps boxes into crop space, the seam translates them back), delivers the
  SAME review session/records as the whole-page path (full page image +
  page-space boxes — identical shape and coordinates), treats a region run
  recognizing NOTHING as a typed `ocrRunFailed` (the user's existing review
  stays intact, message says what to do) instead of a silent empty delivery,
  and reports scoped status ("Re-OCR region complete: N text blocks…").
  Whole-page empty-delivery contract unchanged.
- `src/GpMainWindow.cpp` — the bbox travels through to `runOcrRegion`.
- `tests/TestOcrRegionReocr.cpp` + CMake target (offscreen, serial) — 19 pins.

## R7 gate (docs/audit/evidence-reocr-region/)

| Gate | Result | File |
|---|---|---|
| Fail-before (pins commit `392c57d9`, seams-only) | **11F / 8P** — every new-capability pin RED at runtime; controls (SEP13 guard, whole-page guard, ReviewReady harness) green | `fail-before-red-run1.txt` |
| Negative control (scoped revert of `f64777cd` source files → rebuild → 1 serial run) | **8P / 11F — exact reproduction of the fail-before set (11 FAIL lines)**; recorded once; implementation restored byte-identical afterwards (`git checkout HEAD --`) | `negative-control-scoped-revert.txt` |
| Pass-after ×3 consecutive SERIAL on `f64777cd` | 19/19, 19/19, 19/19 (runs 2–3 after the restore rebuild) | `pass-after-run1.txt` / `-run2.txt` / `-run3.txt` |
| Final-tree touched gate (15 OCR suites, serial) | **15/15** (gate file holds 14; `TestOcrAcceptScope` run separately, also green) | `final-tree-touched-gate.txt` |

No existing test was weakened: `TestSep13LeadOcrGuards` pins pass unchanged
(the `onReOcrRegion` ReviewReady guard is preserved verbatim).

## Honest deviations / environmental record

- The default-arg `runOcr(const QRectF& = QRectF())` form was abandoned after
  the first build: Qt static-asserts ("slot requires more arguments than the
  signal provides") on GpMainWindow's PMF connect. Resolved with the
  dedicated `runOcrRegion` slot (documented in the header). One wasted full
  compile pass (~530/1089 targets before the error surfaced).
- Two compile-fix rounds in lane code: most-vexing-parse `QColor fill(...)`,
  and the normalization-order bug above (caught by the pins, not shipped).
- External kills: FOUR background builds were SIGKILLed by the environment
  (exit 137 — eleven co-tenant lanes share this machine). Ninja's
  incrementality preserved all work; every retry resumed cleanly. Recorded
  build attempts: pins tree ×5 (one transient LTO `lto-wrapper failed` after
  a kill, clean on retry), implementation ×5, NC ×1, restore ×1.
  Final `BUILD_RC=0` on each configured gate tree.
- One transient `libpdfws_ui.a` archive lock (`cmake -E rm -f` failed twice —
  fresh 34 MB archive held, most plausibly by AV scan racing ar; no test
  process was holding it, manual removal succeeded, next build clean).

## Known limits (honest scope notes)

- Region drag exists on the **source-image canvas** only. The rich-text
  fallback label (words-only deliveries, Djot preview) has no drag surface;
  there the menu offers whole-page re-OCR and the drag-gesture note, which is
  honest (no image = no region to scope to).
- The scoped run reuses the whole-page 2.0× page render, cropped — identical
  preprocessing/orientation/engine prefs, so a region re-run cannot disagree
  with the page run it amends. Very small drags (<1 px after snapping) are
  refused with the typed garbage-region message rather than silently widened.
- The region action lives in the right-click context menu (existing surface).
  A dedicated toolbar button was NOT added (not in the row's scope).

## Owner/integrator notes

- Branch tip: `f64777cd` (impl) on `392c57d9` (pins) on `ad77c29c` (base).
  FF-ready; no merges, no pushes, worktree/branch left in place per contract.
- Scorecard row 12 and §9.4's residual ("region re-OCR honestly labeled
  page-scoped") can now read SHIPPED; the §3 row 21 PARTIAL resolves to done.
- `TestOcrRegionReocr` is registered LABELS `unit;ocr;reocr-region;review;qt;headless`,
  TIMEOUT 120, offscreen — include it in integrator OCR sweeps.
