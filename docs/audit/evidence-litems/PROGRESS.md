# r5-litems lane — progress log (worktree D:/pdf/pdf-litems, branch feat/litems)

## 2026-10-04 (prior instance, resumed)

Recovered from the worktree + evidence dir; timeline reconstructed from file
timestamps and log contents. No PROGRESS.md was left by the prior instance
(it died on provider infrastructure before writing one); the evidence dir is
complete and internally consistent, so the runs were reused, not re-captured.

- 20:57 `configure.log`, `build-initial.log` — configure + full build of the
  WIP tree (product fix + all four test edits applied).
- 22:02 `build-red1.log` + `red-f7-generic-save-failure.txt` — FAIL-BEFORE RED
  for finding 7: product wording temporarily scoped back to the generic
  `QMessageBox("Error", ...)` shape, `HomeController.cpp` + the test rebuilt
  (ninja log shows exactly those two targets), `TestPersistenceOutcomes` run
  once: `closeAfterFailedSaveKeepsDocumentOpen` FAILED with
  `the failed-save box title is "Error", expected "Save Failed"`.
  Recorded once, then restored.
- 22:05 `build-red1-restore.log` —HomeController.cpp restored and rebuilt.
- 22:05–22:06 `red-f9a-hc-thumbpaper-rule-lost.txt`,
  `build-red3.log`, `red-f9b-hc-mono-overridden.txt` — FAIL-BEFORE REDs for
  finding 9, captured in two scoped stages (paper rule scoped out first —
  the paper pin fails at `#000000`; then paper restored and the mono rule
  scoped out — the mono pin fails at `Segoe UI`). Restored afterwards.
- 22:06 `red-f10-thumbnail-accessible-name.txt` — FAIL-BEFORE RED for
  finding 10: the ThumbnailSidebar.cpp product edit scoped out,
  `TestThumbnailOffGui` run: new slot FAILED with
  `thumbnail page slot 1 exposes accessibleName "", expected "Page 1"`.
  Restored (build-red3.log shows ThumbnailSidebar.cpp rebuilt).
- 22:53 `build-final.log` — full rebuild of the restored tree, BUILD_RC=0.
- 22:54 `green1.log`/`green2.log`/`green3.log` — pass-AFTER ×3 SERIAL:
  the three focused binaries, 9/9 each pass, 100% out of 3 ×3.
- 22:59 `serial-full-suite.log` — whole-project ctest, serial, no -j:
  **100% tests passed, 0 tests failed out of 202** (204 registered; the two
  "did not run" entries, `R14ProbeRedactSpace` and `R14ProbeBatchSkip`, are
  the deliberate disabled probes documented since the r4-ux lane).

## 2026-10-05 (resumed instance — verification, no re-work needed)

Judged the prior evidence honest and complete: every RED names the new pin,
fails at the new assertion, and is paired with a scoped restore build; every
green log is a real ctest invocation. Re-verified independently rather than
re-capturing:

- `build-resume-20261005.log` — incremental rebuild of the as-left tree:
  **BUILD_RC=0**.
- `green-resume-20261005.log` — focused gate re-run ×3 SERIAL (today):
  **100% tests passed, 0 tests failed out of 3**, three times (9/9 total).
- `serial-full-suite-resume-20261005.log` — full serial suite re-run (today):
  **SERIAL_RC=0, 100% tests passed, 0 tests failed out of 202** (same two
  deliberate disabled probes).

No flakes observed in any run; no re-run was needed.

## Mission state

1. Modal-text-capture helper (`captureNextModalText`) + truthful-text pins in
   `TestPersistenceOutcomes::closeAfterFailedSaveKeepsDocumentOpen` — DONE
   (test-side only; the product dialog already carried the truthful
   "Save Failed" wording from the earlier R2-2/UX-14 fix, so the RED proves
   the pins bite by scoping the wording out, not by changing the product).
2. HC-parity upgrade to resolved-state assertions (pixel grab of
   `QWidget#thumbPaper` == `#e8e6df`; polished `QLineEdit[mono="true"]`
   font family == "JetBrains Mono") — DONE, replacing the raw
   `sheet.contains(...)` checks.
3. Thumbnail accessibleName pins — product side in
   `src/ui/ThumbnailSidebar.cpp::createThumbWidget` (accessibleName
   "Page %1" + interaction-contract accessibleDescription), off-GUI pin
   `placeholderThumbnailsExposeAccessiblePageNames` in `TestThumbnailOffGui`
   — DONE.

Remaining: commits on feat/litems + `docs/audit/LANE-REPORT-litems-2026-10-05.md`.
