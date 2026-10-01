evidence-quadpoints/ — PARITY-SCORECARD-2026-09-30 §4 row 1 (July audit §9.3
P1 row 15): QuadPoints text-anchored Highlight/Underline/Strikeout/Squiggly.
Lane: feat/quadpoints-markup. Original base 02d1a898; rebased twice as main
advanced mid-lane (181247b2, then df2e7f94 — the 05:27 rebase appears in the
reflog as an external/integrator action on this branch — final base 7066534c,
tip 69a02311, FF-ready). Feature commits: 718df532 (pins+data model),
40d14f48 (writer+reader), 5ec495c9 (seam+rendering+sidecar), 69a02311
(pin guards + NC).

fail-before-quadpoints-pins.txt
    Verbatim run of tests/TestQuadPointsMarkup.cpp with the data-model fields
    present (AnnotationItem.quads, TextMatch.lineRects) but NO behavior.
    7 failed / 3 passed — the 3 passes are the free-rect guard
    (rectOnlyMarkupWritesNoQuadPoints) plus the Qt harness slots. Every R7
    pin RED:
      writerEmitsQuadPointsPerLine         — "got rect-only" (no QuadPoints key)
      quadPointsRoundTripPreservesLines    — 0 quads on extract
      foreignAcrobatQuadPointsFixtureLoads — fixture quads ignored (rect fallback)
      doubleRoundTripIsStable              — quads lost across generations
      paintDrawsQuadsNotUnionBlank         — union blank band painted rgb(255,255,155)
      wrappedMatchReportsPerLineRects      — lineRects empty (union-only)
      sidecarRoundTripPreservesQuads       — .ann drops quads

nc-writer-reverted-pins-kept.txt
    Negative control: the /QuadPoints writer branch in
    PoDoFoBackend::applyAnnotationsToDoc scoped out (`if (false && …)`), pins
    kept verbatim — exactly the 3 writer-dependent pins RED (writer, our-save
    round-trip, double generation), 7P/3F; the reader-only fixture pin and the
    rendering/seam/sidecar pins keep their identity (their branches were not
    touched — their fail-before proofs live in the file above, from before
    those branches existed). Bypass reverted after capture; no source diff.

pass-after-run1.txt / pass-after-run2.txt
    Two CONSECUTIVE full serial ctest gates on the rebased tree
    (181247b2 + this lane, rebuilt RC=0): 191/191, 191/191 (334 s / 278 s).

pass-after-touched-run{1,2,3}.txt
    Three consecutive serial runs of the touched-surface gate on the FINAL
    FF-ready tree (7066534c + this lane, rebuilt RC=0): 19/19 × 3.
    Surface: QuadPoints markup, shape/ink persistence, annotation djot,
    annotation toolbar surface, find&replace (TextMatchFinder), text
    extraction coords, view parity/controllers/viewing/rotation/two-page
    (paintShape consumers), measure panel (AnnotationLayer state machine),
    sidecar reopen + persistence outcomes, page-space law consumers
    (LegacyOriginSpace / Pgr37 / Rotate270), Inspector, CommentsReview,
    TextEditStyle. (TestAnnotationToolBar no longer exists as a ctest target
    on this main — absorbed by the UI wave; the other 19 cover the surface.)

pass-after-run3-attempt{2..8}-flakes.txt
    Six further full-serial attempts, each blocked by a ROTATING cast of
    environment flakes while co-tenant processes pinned the CPU at 100%
    (measured; run totals swung 278 s → 2519 s for the identical tree):
      attempt 2: TestSecretStore (ctest 60 s TIMEOUT)
      attempt 3: TestSecretStore, TestWelcomeRoutes, TestLaneScheduler (timing pin 1002 ms vs <1000 ms)
      attempt 4: TestFileHandleCoordination (SafeSave K4 child-process race probe)
      attempt 5: TestSecretStore, TestSignatureRealCrypto (timeouts), TestSweepW3UxFlows, TestFormJsAdversarial
      attempt 6: TestPerformance, TestFormBuilder (timeout), TestFindReplace, TestFormJsAdversarial, TestRedactTransaction (timeout), TestRedactMarkAll
      attempt 7: TestFileHandleCoordination (same slot as attempt 4)
      attempt 8: TestWelcomeRoutes, TestSweepW3UxFlows (0xc0000602), TestLaneScheduler, TestBatchMode, TestPersistenceOutcomes, TestRenderGuards
    None is on this lane's surface, and the flake identity was proven on
    trees WITHOUT this lane's change: TestSweepW3UxFlows failed (different
    slot, flow8) on the base build-rel binaries, TestSidecarReopenState::
    saveOnSwitchEmbedsAnnotationsAndReopenIsClean failed 1-of-4 on the base
    build-rel binaries, and every named suite passes in isolation on this
    tree (TestFindReplace 30P/0F, TestFileHandleCoordination 7P/0F ×2,
    TestSecretStore 25P/0F ×3, TestWelcomeRoutes 20P/0F, TestBatchMode 18P/0F,
    TestLaneScheduler 13P/0F).

flake-classification-isolation.txt
    Isolation runs taken WHILE the gate was saturating the machine —
    LaneScheduler/BatchMode fail there too; kept as the concurrency-sensitivity
    exhibit (the clean isolation numbers are in the README above).
