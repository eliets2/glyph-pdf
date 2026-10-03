# LANE REPORT — findings-tests (wave-2c round 2: test quality + docs), 2026-10-02

- Worktree `D:/pdf/pdf-f-tests`, branch `feat/findings-tests`, base `406dfa25`.
- Inputs: `D:/pdf/verification/testing-specialist-wave2b.md` (H-2, M-1, §4
  coverage gaps) and `D:/pdf/verification/code-archaeologist-wave2b.md`
  (4.1/4.2 QSS, 1.1 scorecard NOTE).
- Scope of this lane: the five items dispatched in the round-2 prompt. Items
  from the reports that were ALREADY folded by round 1 (`406dfa25`: H-1
  wait predicate, H-3 TIMEOUT, docs integrity) were not re-touched.
- Build: Release + LTO + Tesseract (`build-rel`); tests SERIAL. Final builds
  rc=0; no transient LTO retry needed on the final passes.

## What landed

### 1. H-2 — the missing core pin for row 17 (pipeline consumes persisted prefs)

Seam (behavior-neutral extraction; the reads moved VERBATIM — same keys,
defaults, order):
- `EditController::ocrPreprocessPrefsFromSettings()` (public static, the
  `outputModeFromSettings` idiom), consumed by `runOcrRegion` — its only
  caller — which feeds `OcrPipeline::setPreprocessing`.
- Pin: `TestOcrPreprocessPrefs::pipelineConsumesPersistedPrefs` — absent keys
  → the shipped all-OFF default (F5-F2); each persisted true reaches EXACTLY
  its matching field and never a sibling; explicit false reads as false.
  Suite cleanup() now also sweeps `ocr/orientDetect`.

### 2. M-1 — tautological pin replaced with a product assertion

- Seam: `ConvertController::scannedOfferEnabledByPref()` — the PRODUCT read
  of `ocr/scannedExportOffer`; both consumers (the prompt's early-out and
  `gateScannedExportChoice`'s gate) now read through it, so the shipped-ON
  default lives in product code.
- The tautological tail (`QSettings().value(key, true)` with the default
  supplied BY the test) is deleted. Replacement pin:
  `scannedOfferPrefReadIsTheProductDefaultOn` — absent → true, explicit
  false → false, explicit true → true (permission WITH its denial).

### 3. Coverage gaps — the three named pins

- **Row 5 honest abort** (a): `buildSearchableOcrCopy` hoisted from the
  anonymous namespace to a public static seam (body UNCHANGED — the row's
  honest-abort contract is exactly its empty-result-plus-typed-message
  shape, which the export workers consume via `source.isEmpty()`). Two pins:
  unusable language data → empty + typed "OCR failed … unavailable" naming
  the language; a real recognize-nothing run (genuine blank scan, real
  Tesseract) → empty + typed "recognized no text". NOT covered (residual,
  see below): driving the modal 3-way prompt itself.
- **Row 7 re-entry refusal** (b): `TestCompareIntegration::reentryWhileRunningIsRefused`
  — parks run #1 at the first extraction boundary (heap-shared semaphores
  captured by value, so a mid-pin red can never leave the hook dangling),
  fires `compareFiles` #2, and pins the refusal three ways: one progress
  dialog (not two), `cmpFilesLabel` still names the first pair, and the
  applied `lastResult()` is the first pair's shape (2 pages, no structural
  rows) — not the insertion pair's (3 pages, one PageAdded). Note: the first
  pair must NOT be byte-identical — the engine's streaming-hash short-circuit
  resolves before the first extraction boundary and the park never fires
  (first draft failed exactly that way; corrected before the NC).
- **Row 12 nothing-recognized** (c):
  `TestOcrRegionReocr::regionRunRecognizingNothingIsTypedFailureAndReviewStaysIntact`
  — END-TO-END once: real `GpMainWindow` (Bootstrapper context), real open
  route, a delivered 2-word review session, then `runOcrRegion` over the
  blank bottom half of a genuinely blank scan → real Tesseract recognizes
  nothing → the typed `ocrRunFailed` ("no text recognized in the selected
  region") arrives through the SHIPPED host wiring, the review session's
  records survive verbatim, the panel lands in the retryable RecoverableError
  with Run re-armed, and `ocrResultsReady` never fires (no silent empty
  delivery). QSKIPs honestly when no OCR engine is probed (flow5 gate).
  CMake: suite now links Bootstrapper + Qt6::Pdf/PdfWidgets, TIMEOUT
  120→600, `QTEST_FUNCTION_TIMEOUT=600000` (the pin's 300s typed-failure
  budget must be the thing that decides, not QtTest's 300s function
  watchdog).

### 4. Dead QSS selectors (archaeologist 4.1/4.2)

- Deleted the dead `#thumbPaperTitle` / `#thumbPaperText` rules (no live
  widgets since the `2dabb42d` rename) from ALL THREE sheets.
- Retargeted `#thumbPaperImg` → `#thumbPaperImage` (the live
  `ThumbnailSidebar` placeholder) in ALL THREE sheets — the live selector now
  exists in dark, light and high-contrast (parity with the live name).
- Deleted `#canvasMargin` from all three sheets (matches no widget anywhere
  in src; pre-existing staleness the polish wave perpetuated).
- Machine check (selector-position `#id` regex vs every `setObjectName` /
  `setProperty` literal in src): zero dead selectors remain in any sheet;
  selector sets are byte-identical across the three sheets; `leftSidebar`
  verified live (ternary form). The archaeologist's further recommendation —
  extending `TestViewingModes.cpp:328` to assert selector-reality in CI —
  is left as dispatched future work (not in this lane's five items).

### 5. Scorecard row-16 NOTE reword (archaeologist 1.1)

`PARITY-SCORECARD-2026-09-30.md` row 16: the NOTE "compare-progress (row 7)
can adopt `renderPageAsync` instead of inventing a worker" is reworded to
record the mapped difference so no future lane attempts the impossible
adoption: token-epoch for cacheable page images keyed by page+scale
(document-change staleness) vs QPromise boundary-poll for a DiffResult with
per-stage progress and USER cancel; the actual reusable seam for future
off-GUI workers is the QPromise boundary-poll idiom + by-value hook
discipline (shared plumbing only if a fourth worker appears).

## R7 discipline (evidence: `docs/audit/evidence-findings-tests-2026-10-02/`)

Five named regressions applied simultaneously (disjoint functions), ONE NC
build, ONE red run per suite, then all restored, rebuilt, ×3 serial green:

| Pin | NC result |
|---|---|
| `pipelineConsumesPersistedPrefs` (H-2) | RED: "persisted Deskew=on must reach the pipeline" |
| `scannedOfferPrefReadIsTheProductDefaultOn` (M-1) | RED: absent-pref read false — the exact regression the tautological pin could never see |
| `scannedExportOcrStage…LanguageDataMissing` (row 5) | RED: stage yielded the ORIGINAL input (silent fallback) |
| `scannedExportOcrStage…RecognizingNothingAborts` (row 5) | RED: same |
| `reentryWhileRunningIsRefused` (row 7) | RED: 2 progress dialogs vs 1 |
| `regionRunRecognizingNothing…` (row 12) | RED: typed failure never arrived (function timed out at the 300s budget) — 18 pre-existing pins stayed green |

Every pre-existing pin stayed green under every regression (no weakening):
NC totals 7+1, 12+3, 11+1, 18+1 failures only in the NEW pins.
Green-after: ×3 consecutive serial `ctest` runs, 4/4 suites each (55/55
functions), distinct log MD5s `06e85c0e708a` / `b1079b3f0671` /
`657a09172e2f`. Ripple gate (TestControllers, TestOcrAcceptScope,
TestOcrAcceptSeam, TestOcrReviewLifecycle, TestOcrLanguage, TestDiffEngine,
TestCompareEntry): 7/7 serial. No flakes, no re-runs (the contention
re-run clause was never needed).

### Restore incident (recorded for honesty)

The NC restore used `git checkout --` on the three regressed product files,
which also reverted the lane's own (uncommitted) seam edits in
EditController.cpp / ConvertController.cpp (CompareMode.cpp had NC-only
edits and was restored correctly). The seams were re-applied immediately and
verified byte-equivalent (only two comment lines needed the `§` prefix
restored after the scripted re-application); the ×3 green campaign ran on
the restored build (rc=0). No other worktree was touched at any point.

## Residuals (for the next dispatch, not done here)

1. The 3-way modal prompt itself (Run OCR / Export as-is / Cancel) and the
   export workers' completion paths remain driven only through seams — a
   full modal-driving e2e (SweepW3 `capturePromptAndClick` idiom through
   `activate(ToolId::ToText)`) is the honest next pin for row 5.
2. Archaeologist 4.1 follow-up: extend `TestViewingModes.cpp:328` to assert
   every QSS `#id` selector has a live `setObjectName` (this lane did the
   cleanup + a manual machine check only).
3. Testing-specialist M-2/M-3/M-4..M-7 and L-1..L-8 remain open (out of this
   lane's dispatched five items).

## Commits (this lane, on feat/findings-tests)

1. `feat(ocr)`: the two behavior-neutral observable seams (+ hoist) —
   `ocrPreprocessPrefsFromSettings`, `scannedOfferEnabledByPref`,
   `ConvertController::buildSearchableOcrCopy`.
2. `test(audit)`: the five pins + CMake (TestOcrRegionReocr e2e wiring,
   timeouts) + NC/green evidence.
3. `fix(theme)`: dead-selector cleanup + live-name parity in the three sheets.
4. `docs(audit)`: scorecard row-16 NOTE reword + this lane report.
