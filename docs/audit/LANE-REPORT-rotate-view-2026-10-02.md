# LANE REPORT — Rotate View port (Wave 2b tail) — 2026-10-02

Lane: `feat/rotate-view` in worktree `D:/pdf/pdf-w2b-rotateview`, base `2af5bdf7`.
Feature commit: `ae48c87e`. Evidence: `docs/audit/evidence-rotate-view/`.
Source: archive `archive/cleanup-20260913-closure/heads/feature/viewing-parity`
commit `cd82d701` (session-only rotate + bitmap fallback), branch tip `de1fa268`
(tool wiring).

## CHECK FIRST result

Confirmed absent on base: `git grep RotateView 2af5bdf7 -- src/` → RC=1
(re-verified in this worktree; the integrator's RC=1 holds). The session-only
rotation did NOT exist on current main — the only rotation shipped was the
PERSISTED one (Document ▸ Rotate → `RotatePageCommand`), plus a dangling
`requestPageRotation` path inside `PdfViewerWidget::rotateClockwise` (no UI
caller).

## The adaptation (this was NOT a cherry-pick)

1. **The primary paint path is QPdf, not Pdfium.** Scorecard 2026-09-30 line
   325 lists "viewer dual-path consolidation (absent; PdfViewerWidget
   QPdfDocument vs PdfiumBackend)" as an OPEN P2 note — PdfiumBackend serves
   conversion/OCR/bookmarks (`BackendRouter`, `AutoBookmarkDialog`), while the
   viewer paints exclusively through `QPdfView` (single page) and
   `QPdfDocument::render` via `renderPage()` (two-page spread, thumbnails,
   snapshots, print). Rotation was therefore wired into the path that actually
   paints: `renderPage()` maps `m_viewRotation` into
   `QPdfDocumentRenderOptions::setRotation()` with a dimension swap for
   90/270 (the R12 bounded-size check runs on the swapped dims). No QPdf-only
   logic was bolted onto any Pdfium-painted view because the viewer has no
   Pdfium paint path. Requirement 5's cleaner alternative (rotate natively in
   the primary path) does not exist in this Qt build — QPdfView still exposes
   no rotation API (the scorecard's own row 90 records the view-local route as
   unachievable there) — so the archive's **disclosed bitmap fallback** stays,
   now scoped to the session-only tool only.
2. **The archive's `rotateClockwise` was the session rotation; on main it is
   the persisted one.** The port therefore ADDED `rotateViewClockwise/`
   `rotateViewCounterClockwise`/`viewRotation()`/`resetViewRotation()` and
   `m_viewRotation` instead of touching the existing engine-request path.
   `rotateClockwise/CounterClockwise` keep emitting `requestPageRotation(±90)`
   unchanged (with `m_viewRotation == 0` their behavior is byte-identical).
3. **Two rotation states, one overlay.** `AnnotationLayer`'s
   rotate-around-center paint/hit-test transform now mirrors the TOTAL display
   rotation `(m_rotation + m_viewRotation) % 360` — the transient engine
   offset plus the session rotation. With no view rotation this is exactly the
   old `m_rotation` value.
4. **Cache correctness without forced eviction.** The archive cleared the
   page cache on every rotation change because its key was scale-only; this
   tree's cache was already rotation-aware (§9.1 P0), so the key simply gained
   `viewRotation` — a pre-rotation bitmap can never be served stale, and no
   eviction is needed.
5. **RenderPage consumers rotate too — same contract as the archive.** At
   `viewRotation == 0` every existing consumer is untouched; while rotated,
   two-page spread, thumbnails, snapshots and print genuinely show the rotated
   render (the archive's "every bitmap consumer gets a genuinely rotated
   render").

## Session-only contract (requirement 2)

- No document mutation: `rotateView*` touches no engine, writes no /Rotate,
  and (pinned) never emits `requestPageRotation` — hence no
  `RotatePageCommand`, nothing on the undo stack.
- Reset: `loadDocument()` — the path every open and reload crosses — resets
  `m_viewRotation` and restores the native view (pinned by
  `viewRotationResetsOnReload`). `reload()` inherits the reset.
- Read-only sessions: view-only rotation is deliberately allowed (it mutates
  nothing); ViewController's RotateView tools are not mutating, matching
  DarkMode/EyeCare/NightMode.

## Honest UI (requirement 4)

The fallback's scroll limitation is disclosed the way the source does — in the
code comments AND at the point of use: every rotation shows
"View rotated N° (session only, not saved to the file; the page shows
fit-to-window while rotated — Next/Previous page still works)."; resetting
shows "View rotation reset — upright view restored.". No silent mode switch.

## Edge cases handled

- §9.7 P0 badges: the badge overlay's math assumes QPdfView's viewport+zoom;
  under the fallback it hides and badges/search highlights join the pixmap
  composite via `paintTwoPageOverlays(..., includeAnnotations=false)` —
  annotations stay ONLY on the visible layer above (no double paint).
- Reading filters: Night/Eye Care cover the fallback surface (including the
  lazy-creation case — the filter is re-applied when the surface first
  appears).
- Mode interplay: two-page mode replaces the fallback (and hides it);
  returning to single-page with rotation active re-raises it; zoom/resize/
  page-change refresh it; a failed render keeps the last good surface.
- `rotateView*` no-ops with no document loaded.

## R7 evidence (evidence-rotate-view/)

- **RED (fail-before)** on base src with tests+CMake present: 
  `RED-before-TestViewerRotation.txt` — exactly the NEW pin
  `rotateViewToolsResolveInRegistry` FAIL (rotateViewCW unresolved), the 4
  pre-existing pins green (RC=1); `RED-before-TestRotateView-compile.txt` —
  38 compile errors proving the session-only API is absent on base.
- **NC ×1**: first green run of all 7 suites after the implementation.
- **PASS-after ×3 SERIAL** (`pass{1,2,3}-*.txt`): 7 suites × 3 consecutive
  rounds, 21/21 runs RC=0 — TestRotateView 7/7, TestViewerRotation 5/5,
  TestViewingModes 11/11, TestTwoPageOverlay 7/7, TestMenuBarIntegrity 5/5
  (Registry→handler resolution for the new items), TestControllers 15/15
  (controller uniqueness over the enlarged ToolId set), TestViewParity 15/15.
  No existing test was weakened (TestViewerRotation only gained a slot).
- **Build**: full-tree Release (LTO ON, `-j2`) **BUILD_RC=0** (run3). Recorded:
  run1 was externally killed at [133/142] after a transient LTO
  `lto1.exe: Cannot open ...ltrans1.o`; run2 RC=1 was a lane test bug
  (`qRed(const QColor&)` → `QColor::red()`; the RED evidence is unaffected —
  base failed on the missing viewer API, not on the helper). Env note:
  ucrt64's stale libpodofo.dll — third_party DLL dirs prepended for test exes
  (same as the ocr-preproc lane).

## Disclosed limitations (shipped behavior, not defects)

1. Free pixel-scrolling pauses while a view rotation is active (QPdfView has
   no rotation API; the fallback is fit-to-window one page at a time). Page
   navigation, goToPage, keyboard PageUp/Down/Home/End all keep working.
   Disclosed in the status message and code comments.
2. While rotated, zoom is not visually applied inside the fallback (fit scale
   rules); the zoom level takes visible effect again the moment rotation
   returns to 0 — same as the archive.
3. Out of scope, unchanged: the P2 "viewer dual-path consolidation" note
   (PdfiumBackend is not a viewer paint path); the pre-existing dangling
   `requestPageRotation` UI wiring (no caller in the shipped UI) was neither
   fixed nor relied upon.

## Integrator notes

- Files touched: `src/core/ToolId.{h,cpp}`, `src/shell/MenuBar.cpp`,
  `src/shell/controllers/ViewController.cpp`,
  `src/ui/PdfViewerWidget.{h,cpp}`, `tests/TestRotateView.cpp` (new),
  `tests/TestViewerRotation.cpp` (+1 slot), `CMakeLists.txt` (+TestRotateView
  registration), evidence dir, this report.
- No merges, no branch switches, no pushes; worktree and branch left in place
  for cleanup.
