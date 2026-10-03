# R7 evidence — findings-tests lane (wave-2c round 2), 2026-10-02

Worktree `D:/pdf/pdf-f-tests`, branch `feat/findings-tests` (base `406dfa25`).
Build: Release, LTO ON, Tesseract ON (`build-rel`, Ninja). Tests SERIAL
(`ctest -j 1`, `QT_QPA_PLATFORM=offscreen`).

Five new/strengthened pins; each demonstrated RED once against its NAMED
regression (all five regressions applied simultaneously — they live in
disjoint functions — then restored in one pass), then ×3 consecutive serial
green. No existing test was weakened; the NC logs show every pre-existing
pin staying green under each regression.

## Fail-before (NC) — one red per pin, verbatim from the logs

| Pin | Named regression applied | NC log + failing assertion |
|---|---|---|
| `TestOcrPreprocessPrefs::pipelineConsumesPersistedPrefs` (H-2) | the four persisted-pref reads DELETED from `EditController::ocrPreprocessPrefsFromSettings` (pipeline stops consuming prefs) | `NC-red-TestOcrPreprocessPrefs.log`: `FAIL! ... 'prefs.deskew' returned FALSE. (persisted Deskew=on must reach the pipeline)` — Totals 7 passed, **1 failed** |
| `TestOcrOutputMode::scannedOfferPrefReadIsTheProductDefaultOn` (M-1 replacement) | product default flipped `true`→`false` in `ConvertController::scannedOfferEnabledByPref` ("the offer exists" regresses) | `NC-red-TestOcrOutputMode.log`: `FAIL! ... 'ConvertController::scannedOfferEnabledByPref()' returned FALSE. (with the pref absent the scanned-export offer must exist ...) ` — the old tautological pin (`QSettings().value(key, true)` with the default supplied BY the test) cannot produce this red |
| `TestOcrOutputMode::scannedExportOcrStageFailsHonestlyWhenLanguageDataMissing` (row 5, engine leg) | OCR-stage failure paths return `inputPath` instead of `{}` (the silent un-OCR'd fallback) | same log: `FAIL! ... 'source.isEmpty()' returned FALSE. (an OCR stage that cannot run must NOT yield a source (the worker would export it): got '.../scan.pdf')` |
| `TestOcrOutputMode::scannedExportOcrStageRecognizingNothingAborts` (row 5, recognize-nothing leg) | same fallback regression | same log: `FAIL! ... 'source.isEmpty()' returned FALSE. (recognizing nothing must NOT yield a source: got '.../blank-scan.pdf')` — 12 passed, **3 failed** |
| `TestCompareIntegration::reentryWhileRunningIsRefused` (row 7) | `if (m_watcher.isRunning()) return;` DELETED from `CompareMode::compareFiles` | `NC-red-TestCompareIntegration.log`: `FAIL! ... Compared values are not the same — Actual (cmpProgressDialog count): 2, Expected (1)` — the guard's own documented harm (orphaned second dialog); Totals 11 passed, **1 failed** |
| `TestOcrRegionReocr::regionRunRecognizingNothingIsTypedFailureAndReviewStaysIntact` (row 12) | the `isRegionRun && mergedWords.isEmpty()` typed-failure block DELETED from EditController's completion lambda | `NC-red-TestOcrRegionReocr.log`: `QFATAL ... Test function timed out` after the full 300s typed-failure budget — the typed `ocrRunFailed` NEVER arrived (the empty payload went down the success path instead); Totals 18 passed, **1 failed**, rc=127 |

Restore: `git checkout -- src/shell/controllers/EditController.cpp
src/shell/controllers/ConvertController.cpp src/modes/CompareMode.cpp`
(seam edits re-applied byte-identically afterwards — see the lane report's
"restore incident" note), rebuild rc=0.

## Green-after — ×3 consecutive SERIAL runs, all four touched suites

`GREEN-run1.log` / `GREEN-run2.log` / `GREEN-run3.log`:
`100% tests passed, 0 tests failed out of 4` each
(TestOcrRegionReocr 20 fns, TestOcrOutputMode 15, TestCompareIntegration 12,
TestOcrPreprocessPrefs 8 = 55/55 per run). Log MD5s distinct:
`06e85c0e708a` / `b1079b3f0671` / `657a09172e2f`.

Ripple gate (neighbors consuming the touched controllers), serial, 7/7:
TestControllers, TestOcrAcceptScope, TestOcrAcceptSeam, TestOcrReviewLifecycle,
TestOcrLanguage, TestDiffEngine, TestCompareEntry — `100% tests passed`.

## Honesty notes

- The row-12 e2e pin exercises the REAL pipeline (Tesseract, staged eng
  traineddata; `QSKIP` when no engine is probed — the flow5 gate idiom) and
  asserts through the SHIPPED host wiring (EditController::ocrRunFailed →
  OCRMode::notifyOcrFailed → lifecycle label + RecoverableError + Run re-armed).
- The suite's QtTest per-function watchdog was raised via
  `QTEST_FUNCTION_TIMEOUT=600000` (ctest ENVIRONMENT) so the pin's 300s
  typed-failure budget decides reds honestly instead of the watchdog racing it.
- No flakes observed; no re-runs were needed (contention clause unused).
