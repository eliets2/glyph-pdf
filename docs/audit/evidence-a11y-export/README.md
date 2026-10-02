# Evidence — lane #15 wave 2b, exportable accessibility results (a11y-export)

Tree under test: `feat/a11y-export` @ worktree `D:/pdf/pdf-w2b-a11yexport`,
base `a4cc1522`. Mission: PARITY-SCORECARD-2026-09-30 §4 row 15.

R7 contract applied to a NEW-feature lane (the pins target new APIs, so the
fail-before is the honest compile-absence state):

1. **fail-before** — `fail-before-build.log`: with the two new pin suites
   added and the implementation absent, the build is RED with exactly the
   new-API errors (BUILD_RC=1):
   - `TestA11yReportWriter.cpp:29:10: fatal error: engines/A11yReportWriter.h: No such file or directory`
   - `TestAccessibilityPanel.cpp:870/871/884/895: 'class gp::AccessibilityPanel' has no member named 'exportCsvTo'/'exportSummaryPdfTo'`
2. **NC once** — `nc-once.log`: the first single-shot run after the
   implementation compiled was 10 passed / 1 failed; the failing pin was
   correct and the fix was test-READER-side only (whitespace normalization
   of extracted PDF text). The pin was never weakened.
3. **pass-after ×3 SERIAL** — `pass-after-3x-serial.log`: three consecutive
   clean serial ctest runs of both suites
   (`TestAccessibilityPanel` 18/18, `TestA11yReportWriter` 11/11).
4. **Neighbor suites** — `neighbor-suites-after-hoist.log`: the lane hoisted
   the W1-04 standard-14 toolkit out of `ReviewSummaryWriter.cpp` into
   `engines/Standard14Text.h` (single-contract discipline, mirrors
   `csvFormulaSafeCell`); `TestReviewSummary`, `TestPrintableSummary`,
   `TestAccessibilityChecker`, `TestAccessibilityFixes`,
   `TestAccessibilityTagger` all green after the hoist (5/5).

Build: runbook configure exactly; BUILD_RC=0 on the full tree
(`build-final2.log` tail kept below). Environmental incidents recorded, all
resolved by retry per runbook: two background builds killed externally
(exit 137, co-tenant load), one transient LTO link failure
(`TestBatchOcrConfidence.exe`, 29 LTRANS jobs, clean on incremental retry),
and one `libpdfws_engines.a` remove-collision caused by an earlier
still-running background build holding the lock (not a transient flake; it
cleared when that build finished).
