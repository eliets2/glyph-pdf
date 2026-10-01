# Evidence — lane 17 wave 2b: OCR verify: disclose active preprocessing capability

Parity scorecard 2026-09-30 §4 row 17 (July §3 row 22): "A build without
Leptonica silently degrades preprocessing" — dispatch target: OCRMode checkbox
tooltip ← OcrPreprocessor capability query. Branch `feat/ocr-preproc-disclosure`,
base a4cc1522. All runs SERIAL on build-rel, ucrt64-first PATH with
`third_party/podofo/install/bin` + `third_party/pdfium/bin` PREPENDED (see
"DLL note").

## Environment

- Configure: `-G Ninja -DCMAKE_BUILD_TYPE=Release -DGLYPHPDF_ENABLE_TESSERACT=ON
  -DGLYPHPDF_ENABLE_LIBSECRET=ON -DGLYPHPDF_ENABLE_TEST_FIXTURES=OFF
  -DGLYPHPDF_FUZZ=OFF` (LTO omitted per lane instructions; small lane).
  Configure summary: `HAS_TESSERACT : TRUE` → `OcrPreprocessor::leptonicaAvailable()`
  returns true in every run below; the pin's Leptonica-branch assertions are the
  live ones, the no-Leptonica branch is compiled but skipped by the if/else.
- Build: `cmake --build build-rel --config Release -- -k 0 -j 2` (11-lane cap).
- DLL note: with ucrt64 alone first on PATH the test binaries died at load with
  0xc0000139 (STATUS_ENTRYPOINT_NOT_FOUND): `/c/msys64/ucrt64/bin/libpodofo.dll`
  is a stale May-9 4.0 MB copy while the freshly linked exe imports the vendored
  8.7 MB one; prepending `third_party/podofo/install/bin` and
  `third_party/pdfium/bin` (pdfium copies are byte-identical, md5
  01163893fb07567c3690b2ebc62cfc6d) fixes resolution. Recorded for future lanes.

## R7 sequence

1. **fail-before** — `fail-before-TestOcrPreprocessPrefs.log`: the new pin
   `tooltipsDisclosePreprocessingCapability` compiled against the UNWIRED UI
   (OCRMode.cpp reverted to base; `ocrmode-wiring.patch` is the change held
   back). Only the new pin is RED (`'!deskewTip.isEmpty()' returned FALSE`),
   all six pre-existing slots stay green. Never-weakened: no existing
   assertion touched.
2. **NC (once)** — `nc-hardcoded-tooltip-STILL-RED.log`: wiring restored but
   the tooltip HARDCODED to the degraded wording (ignoring the capability
   query — the exact anti-pattern row 17 names). The pin catches the lie in
   the HAS_TESSERACT=ON configuration: `'!deskewTip.contains(absence, ...)'
   returned FALSE` ("must not claim the capability is missing"). Proves the
   pin reads the query-driven surface, not merely tooltip presence. Mutation
   reverted after the run.
3. **pass-after ×3 SERIAL** — `pass-after-{1,2,3}-prefs.log` +
   `pass-after-{1,2,3}-preproc.log`, final tree, back-to-back, no parallelism:
   - TestOcrPreprocessPrefs: 7 passed, 0 failed (6 pre-existing + 1 new pin) ×3
   - TestOcrPreprocessor: 25 passed, 0 failed ×3 (touched suite —
     OcrPreprocessor.h/.cpp gained the static capability query)

No test was weakened, skipped-out or retried: every run above is first-try on
its named tree.
