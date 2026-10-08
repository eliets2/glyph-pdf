# GlyphPDF code review — 6 September 2026

Historical snapshot report. The [fetched-remote row-by-row review at `4761443`](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md) contains the current package dispositions and full-suite evidence.

**The packages have made substantial progress, but code acceptance still needs fixes.** This review covers committed `feat/parity-glm` snapshot **`cf5ddc789f95059257455a267f7a7bf58600ed36`** from `C:\Users\User\Projects\pdf-parity`, including the delta after `06b542d`. It supersedes the older report's progress counts; the earlier defect descriptions remain available for implementation details.

Per your instruction, this is a code review with automated headless tests and disposable fixtures. No further computer-use actions or installation were performed. Live UI acceptance is deferred until the code is accepted and the newest app is reinstalled. The older installed app does not establish how these packages behave.

## Progress and verification

- **All 12 R packages now have committed implementations.** R05/R06 landed as `87d98f8` and `8e503b4`; their expanded preprocessing suite passes in the Tesseract-enabled configuration.
- **Seven distinct UI packages, U01–U07, have committed implementations.** This means code is present, not that their live workflows have passed acceptance. U08 was still represented by uncommitted capability-related changes at the snapshot check and is excluded from this review.
- The application and **23 selected test executables built successfully** from an isolated archive of `cf5ddc7`. The build used Qt 6.11/GCC 16.1, the existing vendored dependencies, and Debug configuration with `-g0` to conserve disk space. No source checkout or another session's build directory was modified.
- **23/23 targeted CTest suites passed in 50.00 seconds.** There are 100 registered tests in this snapshot; the other 77 were not run. This is not a full-suite or sanitizer certification.
- Independent production-code probes reconfirmed V02/V03/V04 below, reproduced the sanitize-retry defect, and exposed the OCR overlay-coordinate defect through a synthetic offscreen paint test. A separate compiler check failed in the supported Qt-only preprocessing path.

The passing suites were: `TestSignatureAppearance`, `TestOcrReviewLifecycle`, `TestOcrVerifyNavigation`, `TestOcrPreprocessor`, `TestControllers`, `TestCompareEntry`, `TestPagesMode`, `TestTaskNavRegistry`, `TestScreenStateSync`, `TestRibbonCollapse`, `TestStatusBarSlim`, `TestCommentsReview`, `TestWelcomeLayout`, `TestSignatureBadges`, `TestCompressDialogHonesty`, `TestFormSafety`, `TestFormUndo`, `TestExportPathBadge`, `TestRedactTransaction`, `TestConversionExtraction`, `TestRedactMarkAll`, `TestOllamaProvider`, and `TestDiffEngine`.

Good structural changes include the shared checked `SafeSave` commit, explicit redaction partial outcomes, actual OCR source-image widgets, a shared confidence classifier, and central task-state synchronization. The remaining problems show why callers, retries, lifetime, alternative build configurations, and saved artifacts need tests beyond the new happy paths.

## New findings and implementation instructions

Paths and lines below refer to `cf5ddc7` in the active worktree. Implement one bounded patch at a time, preserving concurrent changes.

### D01 — P1: “Retry Sanitize” receives an empty output path

**Runtime confirmed.** `src/engines/RedactOperation.cpp:339–348` fills `RedactResult::sanitizedDestination` only on success. On partial failure it remains empty, but `src/modes/RedactApplyDialog.cpp:301–307` passes that field directly to `sanitizeCommittedFile` when the user retries.

Using the existing sanitize-failure seam, the probe produced `PartialRedactedOnly` with an empty result destination. After clearing the fault, retry with the presenter's arguments returned false. Retry with the original request's intended destination returned true. The redacted artifact and original source remained intact.

**Fix:** Preserve the intended sanitize destination separately from the successfully committed destination, or carry the request/recovery plan into the presenter. Retry must use the intended path without claiming a sanitized artifact already exists. Update the result/banner after a successful retry; the current callers still build their banner from the original partial result.

**Acceptance:** Inject sanitize failure, capture the partial result, clear the failure, and execute the same recovery path used by the presenter. Assert a readable sanitized file at the originally selected path, correct completion text, and unchanged original/redacted artifacts. Repeat a failed retry and preserve recoverable state.

### D02 — P1: the redaction worker does not own the state it uses

**Source trace; no deliberate crash induced.** `src/engines/RedactOperation.cpp:140–147` captures a `QPointer`, checks it once, and calls `self->run()` on a worker. The operation is parented to UI-owned objects in `RedactMode.cpp:496` and `SecurityController.cpp:548`; its destructor at `RedactOperation.cpp:89` has no worker coordination. Once `run()` begins, it repeatedly reads members such as `m_request`, `m_cancelRequested`, and `m_pageBoundaryHook`. Closing the owner can destroy the operation while that code is still executing. `QPointer` observes deletion; it does not keep the object alive during the call.

**Fix:** Give the worker durable ownership of its request, cancellation state, and execution state through completion. Use a worker-owned operation with a properly managed lifetime, or a plain/shared job state independent of the UI object's lifetime; queue only the result back to a guarded receiver. Ensure start cannot create overlapping runs of one mutable operation. Do not add another weak-pointer check and assume it makes a whole member-function call safe.

**Acceptance:** Hold a synthetic job at a deterministic page/stage boundary, destroy the initiating owner, then release/cancel it. Verify safe termination and no callback to destroyed UI. Cover owner destruction before dispatch and during processing, repeated start, and normal completion. Run a sanitizer-enabled lifetime check where supported.

### D03 — P1: preprocessing fails to compile without Tesseract

**Compiler confirmed.** `src/engines/ocr/OcrPreprocessor.cpp:38` defines `carryResolution` inside `#ifdef HAS_TESSERACT`, while the Qt-only binarization branch at line 356 and unconditional denoise path at line 375 call it. A syntax check without that macro fails twice with `carryResolution was not declared in this scope`. The normal Tesseract-enabled build passes, so its tests conceal this configuration regression.

**Fix:** Move the Qt-only metadata helper outside the Leptonica conditional while retaining Leptonica types/functions inside it. Keep the correct polarity mapping and deskew transform.

**Acceptance:** Compile `OcrPreprocessor.cpp` with and without `HAS_TESSERACT`. Check fallback denoise/binarize preserve image dimensions and DPI, and rerun the enabled configuration's polarity, skew, and transform fixtures. No new dependency is required.

### D04 — P2: OCR word overlays are offset from the displayed scan

**Headless paint probe confirmed.** `src/ui/OcrScanCanvas.cpp:130–133` subtracts the letterboxed image's origin from image-space word boxes before scaling. Painting needs `widgetPosition = imageOrigin + scale × imagePosition`; `wordIdAt` uses the corresponding correct inverse, so drawn overlays and click selection can disagree.

For a 100×100 image in a 200×100 widget, the image begins at (50,0). A word box at (10,10,20,20) should appear at (60,10,20,20). The current code paints it at (-40,10,20,20), off the widget. The probe's expected box-center pixel at (70,20) stayed white instead of receiving the confidence overlay.

**Fix:** Use one image-to-widget transform for overlay painting and its inverse for hit testing. Add the image origin after scaling image-space coordinates; keep border thickness intentional if using a painter transform.

**Acceptance:** Use synthetic image/word geometry to check actual painted box positions for horizontal and vertical letterboxing, zoom/resize, selection borders, and removed-word styling. Assert clicking the visible painted box selects the same word. This is an automated coordinate contract, not a substitute for the later installed-app UI walkthrough.

### D05 — P1: sanitization still bypasses the safe replacement boundary

**Source trace; disk/rename failure not induced.** The new transaction's sanitize step calls `sanitizeCommittedFile`, which calls `engine->sanitizeDocument` with the final destination (`RedactOperation.cpp:123`). The backend passes that destination directly to `qpdf_init_write` at `src/engines/podofo/PoDoFoBackend.cpp:2241`. Its fallback removes an existing output before renaming the temporary file at lines 2256–2262 and 2266–2272. A later failure can therefore lose or damage a previously existing sanitized output. The redacted source artifact is protected, but the requested second output does not receive the same atomic-replacement guarantee.

**Fix:** Produce the fully sanitized candidate first, validate it, then commit it through `SafeSave`. Ensure both qpdf and non-qpdf branches write only operation-owned candidates until commit. Carry failure and cleanup through the partial-result path.

**Acceptance:** Seed an existing sanitized destination with known bytes. Force candidate, qpdf/write, validation, and commit failures; preserve those bytes and the committed redacted artifact. On success, reopen and inspect the sanitized output. Remove all delete-before-rename replacement paths from this operation.

## Earlier findings remain relevant

The detailed instructions are in the [previous branch review](PARITY-BRANCH-REVIEW-2026-09-05.md). These are not fixed merely because their parent R package has landed:

| Finding | Status on `cf5ddc7` |
|---|---|
| V01: form-import temporary path deletion/self-rename | Relevant controller code unchanged; source-traced persistence defect remains. |
| V02: failed form undo advances history | Runtime reconfirmed: index 1→0, `canUndo=false`, edited value remains. |
| V03: spreadsheet columns collapse | Runtime reconfirmed: only A1/A2 contain joined two-column text. |
| V04: content edit also reported as page removal/addition | Runtime reconfirmed on the one-page Apple→Orange fixture. |
| V05: OCR revision proxy misses same-path/same-count mutations | Path/count guard remains; U03's new source-image widgets do not repair it. |
| V06: auto-detect bypasses undo history | Relevant controller code unchanged; undo/partial-success messaging remains inaccurate. |

## Ledger corrections and boundaries

The ledger now correctly records R05/R06 and R09/R10 as implemented-awaiting-review. It still has an “all open” UI heading, two conflicting U07 rows, and an obsolete 85-test count. Reconcile these against the pinned implementation and generated registration count. Keep independent test results scoped to the exact tested configuration and workflows.

The ledger's E-1 same-stream redaction-corruption report remains an unresolved investigation item. I tested both a handwritten PDF and a PoDoFo-generated same-stream two-line PDF: the unredacted `PUBLIC_KEEP_TEXT` survived in both on this snapshot. That does not disprove the reported case, but I cannot independently certify it as reproduced. Preserve the exact failing fixture, mark rectangle, decoded stream, command, and commit before asserting its root cause or marking it fixed. The existing second-page survival assertion does not prove same-stream preservation.

This review concentrates on the changed correctness boundaries and meaningful automated coverage. It does not claim exhaustive inspection of every line in the 73-file delta, full model-based OCR evaluation, every feature configuration, every signature/font case, or live visual acceptance. Uncommitted U08 and other concurrent work are outside this pinned review.

## Recommended next work

Address D02 and the persistence/history findings V01/V02/D01/D05 first. D03 is a small build repair. Then resolve V03/V05 and the comparison/overlay issues V04/D04, followed by V06. Preserve existing improvements and extend the narrow boundaries rather than replacing large subsystems.

Run the complete required regression suite on the final consolidated commit after these fixes, then reinstall that exact build and perform the deferred UI acceptance scenarios. Record the installed version/commit so the UI review cannot accidentally evaluate the pre-package application.

[GLM implementation and UI guide](GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md) · [Code-review evidence](code-review-evidence-2026-09-06.zip)
