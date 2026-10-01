# LANE REPORT — ocr-preproc-disclosure (wave 2b, lane 17) — 2026-10-01

- **Row:** PARITY-SCORECARD-2026-09-30.md §4 row 17 — "OCR verify: disclose
  active preprocessing capability. A build without Leptonica silently degrades
  preprocessing." (July §3 row 22). Effort S.
- **Worktree/branch:** D:/pdf/pdf-w2b-ppdisc — `feat/ocr-preproc-disclosure`,
  base a4cc1522. Isolated; no other worktree touched; nothing pushed/merged.
- **Status:** COMPLETE.

## CHECK-FIRST

No disclosure existed on the base: `grep setToolTip src/modes/OCRMode.cpp`
showed tooltips only on review-pane controls (word edit, buttons, suggestions);
the four preprocessing checkboxes (`ocrChkDeskew/Binarize/Denoise/OrientDetect`)
carried none, and `OcrPreprocessor` had no capability query at all — the
HAS_TESSERACT gating lived only in `#ifdef`s with silent fallbacks (deskew and
orientation documented no-ops, binarize a quiet fixed-threshold fallback).

## What was built (surgical: 4 files, +150/−0)

1. `src/engines/ocr/OcrPreprocessor.h/.cpp` — new static capability query
   `OcrPreprocessor::leptonicaAvailable()` (true under `HAS_TESSERACT`).
   Documented contract: without Leptonica, deskew/orientation are no-ops and
   binarize degrades to a fixed threshold; denoise and DPI normalization are
   Qt-only and always present. The UI must ask this query, never re-derive it.
2. `src/modes/OCRMode.cpp` — all four preprocessing checkboxes now carry
   tooltips composed through `preprocessingToolTip(available, missing)` from
   the capability query:
   - Deskew / Auto-Rotate without Leptonica: plainly "NOT available in this
     build … has no effect on recognition", plus what the user can do (OCR-
     capable build; straighten in an image editor / rotate on the Pages screen).
   - Binarize without Leptonica: discloses the degradation ("falls back to a
     simple fixed threshold instead of adaptive Sauvola — expect worse results
     on uneven lighting").
   - Denoise: states it is Qt-only and available in every build (no false
     absence claim in either configuration).
   With Leptonica present the tooltips describe the real behavior (pixFindSkew,
   pixOrientDetect, Sauvola) and claim no degradation. Checkboxes stay enabled
   and the persisted-pref semantics are untouched — disclosure only, zero
   behavior change (scope discipline; EditController/BatchMode pref readers
   unaffected).
3. `tests/TestOcrPreprocessPrefs.cpp` — new pin
   `tooltipsDisclosePreprocessingCapability`: every preprocessing checkbox must
   carry a tooltip; denoise must never claim absence; and per configuration —
   no-Leptonica builds must contain the absence/fallback wording, Leptonica
   builds must NOT claim it. Dual-config discipline (runs green under both
   HAS_TESSERACT=ON/OFF; each branch only where its wording is the truth).
   No existing test weakened.

## R7 evidence (docs/audit/evidence-ocr-preproc-disclosure/, SERIAL only)

- **fail-before:** pin compiled against the unwired UI (wiring held in
  `ocrmode-wiring.patch`) — only the new pin RED (`'!deskewTip.isEmpty()'`
  FALSE), 6/6 pre-existing slots green. `fail-before-TestOcrPreprocessPrefs.log`
- **NC ×1:** tooltip hardcoded to the degraded wording (query ignored — the
  named anti-pattern) — pin catches the lie: "must not claim the capability is
  missing" RED. `nc-hardcoded-tooltip-STILL-RED.log`. Mutation reverted.
- **pass-after ×3 SERIAL, final tree:** TestOcrPreprocessPrefs **7 passed,
  0 failed** ×3; TestOcrPreprocessor **25 passed, 0 failed** ×3
  (`pass-after-{1,2,3}-{prefs,preproc}.log`). First-try every run; no flakes,
  no retries.
- Full-tree build on the final state: `cmake --build build-rel -- -k 0 -j 2`,
  BUILD_RC=0.

## Environment findings (for future lanes)

- With ucrt64 alone first on PATH, freshly linked test binaries die at load
  with 0xc0000139 (STATUS_ENTRYPOINT_NOT_FOUND): `/c/msys64/ucrt64/bin/libpodofo.dll`
  is a stale 2026-05-09 4.0 MB copy vs the vendored 8.7 MB one. Prepend
  `third_party/podofo/install/bin` + `third_party/pdfium/bin` (pdfium copies
  byte-identical) when running test executables directly or via ctest.

## Residual (honest)

- Disclosure is tooltip-carried per the dispatch target; no non-visual
  (accessibleDescription) or checkbox-disable channel was added — a screen
  reader user gets the text only on focus/hover. The no-Leptonica branch of the
  pin is compiled but not exercised on this machine (no HAS_TESSERACT=OFF build
  was run; the branch's logic is symmetric to the NC-proven positive branch).
- EditController/BatchMode honor the same prefs but expose no UI of their own;
  out of dispatch scope.

## Commit

`feat(ocr): disclose preprocessing capability in OCR checkbox tooltips — parity §4 row 17; OcrPreprocessor::leptonicaAvailable() query drives Deskew/Auto-Rotate no-op + Binarize Sauvola-fallback disclosure (Denoise stays Qt-only); pin dual-config in TestOcrPreprocessPrefs; R7: fail-before, NC (hardcoded tooltip caught), pass-after ×3 serial (7+25 green ×3)`
