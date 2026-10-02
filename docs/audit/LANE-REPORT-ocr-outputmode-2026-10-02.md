# LANE REPORT — OCR OutputMode (searchable vs editable) + import/export UI exposure

- Lane: wave 2b #5 (RELAUNCH — prior instance died before committing anything)
- Date: 2026-10-02
- Worktree: D:/pdf/pdf-w2b-ocrmode — branch `feat/ocr-outputmode`, base `7066534c`
- Scorecard target: PARITY-SCORECARD-2026-09-30 §4 row 5 (July rows 20, 27, 111)
- Feature commit: **b7736465** `feat(ocr): searchable-vs-editable OCR OutputMode + exposure in the OCR toolbar and the text-format export dialogs`
- Report commit: this file (docs(audit))

## RELAUNCH FINDINGS

The prior instance died AFTER writing the implementation but BEFORE
building, capturing evidence, or committing. `git status` at pickup showed
12 modified files (~930 insertions) + untracked `tests/TestOcrOutputMode.cpp`
(332 lines, 12 pin slots) + an already-reconfigured `build-rel` (CTest
already knew `TestOcrOutputMode`). This instance audited every hunk of the
inherited diff against the scorecard row and the surrounding code
(APIs, link deps, includes, mock overrides, default-arg back-compat with
TestOcrAcceptScope's byte-pinned strings), found it complete and coherent,
and adopted it rather than rewriting. All building, evidence, and commits
below were done by this instance.

## WHAT SHIPPED

1. **`OcrOutputMode` (src/core/OcrTypes.h)** — `Searchable` / `Editable`
   enum + `ocr/outputMode` pref key + total parser: only an explicit
   case-insensitive `"editable"` selects Editable; unknown/empty/legacy
   values are the documented Searchable default (a corrupted stored value
   can never silently flip the mode).
2. **`PdfEditorEngine::exportEditableTextPdf`** — the editable writer: per
   page, visible black text at the recognized word boxes on a blank page.
   NO image XObject, NO invisible `3 Tr` layer, NO PDF/A OutputIntent
   (plain PDF 1.6). pageImages supplies geometry only (150 dpi assumption
   mirroring the MRC writer) and is never embedded. Word emission mirrors
   the MRC writer (Tz fit, Td placement, R08 UTF-16BE + Identity-H +
   ToUnicode CMap; unembedded Type0 `/GlyphOcrSans` — viewer substitutes,
   the honest trade for a text-only copy). Honest refusals: empty
   pageImages, and an all-empty payload (a blank "editable copy" is not a
   copy) — no file left behind on refusal.
3. **`EditController::onOcrAcceptRequested` mode branch** — reads the pref
   at accept time (`outputModeFromSettings` static seam) and selects the
   writer. No silent fallback: a failed editable write reports the editable
   failure message, a failed searchable write the searchable one. Dialog
   title + success status name the copy kind for both modes, single- and
   multi-page; the Searchable strings are byte-identical to §9.4 history.
4. **OCRMode UI** — `ocrOutputModeCombo` in the OCR toolbar (two entries,
   item data = canonical pref value), tooltip explaining both modes,
   accessible name/description, persisted through the shared
   `ocr/outputMode` key, restored in fresh panels. `setOutputMode` is the
   single writer (user, hosts, tests).
5. **ConvertController export dialogs (July row 27)** — Word/Excel/CSV/Text
   exports probe the first pages for extractable text
   (`probeDocumentText`: HasText / Scanned / Unknown; fresh PdfiumBackend;
   Unknown never invents an answer — the export itself fails honestly
   downstream). A Scanned probe triggers a three-way prompt (Run OCR /
   Export as-is / Cancel) with a "Don't ask again" master switch
   (`ocr/scannedExportOffer`, default ON). Run-OCR builds a whole-document
   temporary SEARCHABLE MRC copy (fresh OcrEngine + OcrPipeline per call;
   QSettings read on the GUI thread) and converts THAT, so the exported
   text is the recognized text; the temp copy is removed after conversion
   either way. OCR-stage failure aborts the export with the stage named
   (E-2 wording discipline when machine policy manages the tessdata
   download) and is NEVER silently retried without OCR; conversion failure
   after OCR is reported as such.
6. **Interface + mock** — `IPdfEditorEngine::exportEditableTextPdf` pure
   virtual, implemented by `PdfEditorEngine` and `MockPdfEditorEngine`
   (records writer choice for controller-level assertions).

## R7 EVIDENCE (docs/audit/evidence-ocr-outputmode/)

| Step | File | Result |
|---|---|---|
| Fail-before RED | `01-RED-fail-before-{full.log,summary.txt}` | Scoped revert of the 10 src files + mock to base `7066534c` (CMakeLists + new test kept) → `cmake --build --target TestOcrOutputMode -k 0 -j 2` → **BUILD_RC=1, 56 `error:` diagnostics** (`OcrOutputMode has not been declared`, `ocrOutputModeFromPref/PrefValue not declared`, …). Restored by byte-identical file copies (no stash used). |
| Negative control (once) | `02-NEGATIVE-CONTROL-scoped-revert.txt` | Injected `3 Tr` (invisible text mode) into `exportEditableTextPdf` → rebuild clean → **TestOcrOutputMode: 11 passed, 1 failed** — `FAIL! editableWriterReplacesContentWithVisibleText: '!bytes.contains("3 Tr")' returned FALSE (the editable copy must not carry an invisible text layer)`. Reverted; `git diff` grep confirms zero `NEGATIVE CONTROL` remnants. |
| Pass-after ×3 | `03/04/05-PASS-run{1,2,3}.log` | SERIAL ctest of the 9 touched suites (TestOcrOutputMode, TestOcrAcceptScope, TestOcrAcceptSeam, TestOcrReviewLifecycle, TestMrcPipeline, TestExportPathBadge, TestOcrLanguage, TestOcrPreprocessPrefs, TestBatchOcrLanguage): **100% tests passed, 0 failed out of 9 — three consecutive runs**, RC=0 each. |
| Extra no-regression sweep | `06-SWEEP-referencing-suites.log`, `07-SWEEP-TestInterfaces.log` | 25 other suites that include the modified headers: **24/24 passed** + `TestInterfaces` (the registered name of tests/TestPdfEditorInterface.cpp, initially missed by regex) **1/1 passed**. |

No existing test was weakened; no test changed at all (the new
`OcrOutputMode` parameters are default-argument additions, so
TestOcrAcceptScope's pinned Searchable strings compile and pass untouched).

## GATE RESULTS

- Configure: inherited from the prior instance, verified to match the
  runbook exactly (`Release`, Ninja, LTO=ON, TESSERACT=ON, LIBSECRET=ON,
  TEST_FIXTURES=OFF, FUZZ=OFF; vendored podofo + pdfium DLLs present).
- Build: **BUILD_RC=0** (`cmake --build build-rel --config Release -- -k 0 -j 2`,
  ucrt64-first PATH). Plus two intermediate full rebuilds for the RED dance
  and the NC revert, both RC=0, zero compile errors.
- Tests: SERIAL only (no `-j`); one transient infrastructure note below.

## DEVIATIONS / NOTES (honest ledger)

1. **Doctrine path**: the mission cited
   `C:\Users\User\.claude\agents\gsd-executor.md`; the file actually lives
   at `C:\Users\User\.claude\agents\gsd-executor\gsd-executor.md` — read
   and applied (verification gate: every claim above carries its captured
   output; no stash/merge/gc; no pushes; cleanup left to the integrator).
2. **Killed background build**: the post-NC full rebuild was killed by the
   harness after ~25 min (656 ninja edges — the RED dance re-dirtied the
   whole dependent set). Ninja state is crash-safe; the rebuild was
   restarted with logging and completed RC=0. No retry-after-LTO-flake was
   needed; every recorded RC is a clean run's RC.
3. **Sweep regex miss**: the first sweep matched 24 of 25 intended suites
   because `tests/TestPdfEditorInterface.cpp` is registered under the name
   `TestInterfaces` (CMakeLists.txt:1261-1280). It was run separately and
   passed (07). Both the miss and the correction are recorded.
4. **Searchable-only OCR in the export path**: the ConvertController
   "Run OCR, then export" flow always produces the SEARCHABLE temp copy —
   the editable mode is the interactive Accept flow's choice, not a
   text-format-export option. This matches the row's dispatch targets
   ("ConvertController export dialogs" = exposure + OCR offer, not a
   second mode surface) and avoids inventing a third UI.
5. **Adopted inherited work**: per the RELAUNCH brief ("continue or restart
   cleanly"), the implementation was adopted after a full hunk-by-hunk
   audit; every gate result was produced fresh by this instance.

## SCORECARD ROWS THIS CLOSES

- §4 row 5 — OCR OutputMode (searchable vs editable) + exposure in
  import/export UI: **DONE**.
- July row 20 (searchable-vs-editable output choice) — **DONE**.
- July row 27 (P1 OCR-mode toggle in PDF→Word/Excel/Text/CSV dialogs) —
  **DONE** as a scanned-document OCR offer wired to the real pipeline
  (superset of a bare toggle: it only appears when it can matter and says
  exactly what it will do).
- July row 111 residual ("the OutputMode choice does not exist, so there is
  nothing to surface") — **DONE**: OCRMode now surfaces it.
