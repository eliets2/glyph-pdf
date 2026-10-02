# R7 EVIDENCE — Rotate View port (Wave 2b tail) — 2026-10-02

Branch `feat/rotate-view`, base `2af5bdf7`, feature commit `ae48c87e`.

## Contract

Session-only view rotation (0/90/180/270): the rendered page bitmap genuinely
turns; the document's /Rotate is never touched, nothing is pushed on the undo
stack, state resets on document (re)load. Distinct from the persisted
Document ▸ Rotate (ToolId::RotateCW/RotateCCW → RotatePageCommand).

## Pins (which test proves what)

| Pin | Suite::test | Proves |
|---|---|---|
| P1 | TestViewerRotation::rotateViewToolsResolveInRegistry | RotateViewCW/CCW resolve from the dispatch strings, round-trip, and are DISTINCT from the persisted rotate tools (string-level so it also RUNS red on a tree lacking the tool) |
| P2 | TestRotateView::rotateViewToolResolvesAndRoundTrips | enum-level presence + round-trip + distinctness |
| P3 | TestRotateView::viewRotationIsSessionOnly | full cycle 90→180→270→0 AND CCW; requestPageRotation NEVER emitted (no engine write, no undo entry) |
| P4 | TestRotateView::renderPageReflectsViewRotation | the BITMAP turns: dims swap, top-left marker lands top-right, former corner is paper, 4×cw returns the upright render |
| P5 | TestRotateView::fallbackSwapsSurfacesAndReverts | native QPdfView stands down under rotation, fallback label (rotatedPageView) shows the swapped aspect, revert at 0 |
| P6 | TestRotateView::viewRotationResetsOnReload | reload() (the close/reload reset path) resets rotation + restores the native surface |

## RED — fail-before (base src, tests present)

File: `RED-before-TestViewerRotation.txt` — TestViewerRotation run against
base `2af5bdf7` src (only tests + CMake applied; src reverted via patch,
re-applied after capture):

    FAIL!  : TestViewerRotation::rotateViewToolsResolveInRegistry() 'cw.has_value()' returned FALSE.
    Totals: 4 passed, 1 failed
    RUN_RC=1

Exactly the NEW pin red; the 4 pre-existing pins stayed green (nothing
weakened).

File: `RED-before-TestRotateView-compile.txt` — TestRotateView cannot compile
against base: 38 errors, `'class PdfViewerWidget' has no member named
'rotateViewClockwise'` etc. — the session-only API is absent on base
(`git grep RotateView 2af5bdf7 -- src/` → RC=1).

## NC — once

Same invocations as the pass rounds, first green run after the implementation
landed (session log): all 7 suites 0 failed.

## PASS-after — ×3 SERIAL (consecutive, first-try green)

Files: `pass{1,2,3}-<Suite>.txt` — 7 suites × 3 rounds, every run RC=0:

| Suite | Totals (pass3) | Notes |
|---|---|---|
| TestRotateView (new) | 7 passed, 0 failed | |
| TestViewerRotation | 5 passed, 0 failed | 4 pre-existing + P1 |
| TestViewingModes | 11 passed, 0 failed | reading filters cover the new surface |
| TestTwoPageOverlay | 7 passed, 0 failed | paintTwoPageOverlays signature extended, behavior unchanged |
| TestMenuBarIntegrity | 5 passed, 0 failed | proves the new Registry menu items resolve to a wired controller |
| TestControllers | 15 passed, 0 failed | controller-uniqueness over the enlarged ToolId set |
| TestViewParity | 15 passed, 0 failed | full-shell view characterization |

## Build

File: `BUILD-LOG.txt` — full-tree Release (LTO ON, ninja -j2):
run1 externally killed at [133/142] after a transient LTO `lto1.exe: Cannot
open ...ltrans1.o` (machine contention); run2 RC=1 = lane test bug
(`qRed(const QColor&)` → fixed to `QColor::red()`; RED evidence unaffected —
the base failure was the missing viewer API); run3 **BUILD_RC=0**.

## Env note

ucrt64/bin ships a stale libpodofo.dll — third_party DLL dirs prepended when
running test exes (same 0xc0000139 note as the ocr-preproc lane).
