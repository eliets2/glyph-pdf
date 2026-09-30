evidence-quadpoints/ — PARITY-SCORECARD-2026-09-30 §4 row 1 (July audit §9.3
P1 row 15): QuadPoints text-anchored Highlight/Underline/Strikeout/Squiggly.
Lane: feat/quadpoints-markup, base 02d1a898 (origin = main only).

fail-before-quadpoints-pins.txt
    Verbatim run of tests/TestQuadPointsMarkup.cpp against the tree with the
    data-model fields present (AnnotationItem.quads, TextMatch.lineRects) but
    NO behavior: writer / reader / renderer / seam all unimplemented.
    7 failed / 3 passed — the 3 passes are the free-rect guard
    (rectOnlyMarkupWritesNoQuadPoints: a drag-rect item must NOT gain a
    QuadPoints key) plus the Qt harness slots. Every R7 pin RED:
      writerEmitsQuadPointsPerLine        — "got rect-only" (no QuadPoints key)
      quadPointsRoundTripPreservesLines   — 0 quads on extract
      foreignAcrobatQuadPointsFixtureLoads— fixture quads ignored (rect fallback)
      doubleRoundTripIsStable             — chained save loses quads
      paintDrawsQuadsNotUnionBlank        — union blank band painted rgb(255,255,155)
      wrappedMatchReportsPerLineRects     — lineRects empty (union-only)
      sidecarRoundTripPreservesQuads      — .ann drops quads

nc-writer-reverted-pins-kept.txt
    Negative control: the /QuadPoints WRITER branch in
    PoDoFoBackend::applyAnnotationsToDoc scoped out (the exact hunk reverted),
    pins kept. TestQuadPointsMarkup 4P/5F — failures exactly the writer-dependent
    pins (writer / roundtrip / foreign double-save); the reader-only fixture pin
    and the rendering/seam/sidecar pins keep their own fail/pass identity.
    Reverted after capture; tree carries no source diff (see nc-restore).

pass-after-run{1,2,3}.txt
    Three consecutive serial ctest runs of the touched-surface gate on the
    final tree (see gate.txt for the exact test list), all green.
