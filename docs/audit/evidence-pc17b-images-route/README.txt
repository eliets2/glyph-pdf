PROGRAM-CONSOLIDATION-2026-09-25 §1.7b — the images route always produces its artifact

CI failure (run 36538793928, CX-14 artifact excerpt in
ci-artifact-run36538793928-images-route-excerpt.txt): TestWelcomeRoutes
imagesRouteProducesAndOpensTheOutput — 'QFileInfo::exists(out)' returned
FALSE after the full 30s QTRY; load-dependent (also failed once under -j6 on
the LTO release build, passed on rerun).

Root cause: HomeController::onImagesToPdf wired
QProgressDialog::canceled → QFutureWatcher::cancel. convertImagesToPdf is
one uninterruptible paint+save, so the wiring is a lie in both directions:
a cancel after the task started changes nothing (the file appears while the
UI said "canceled"), and — the CI-observed direction — a cancel landing
BEFORE the pool thread picks the task up makes QtConcurrent skip the
runnable entirely. Probe (probe-qtconcurrent-cancel-before-start.txt, 3/3):
cancel-before-start ⇒ runnableRan=0. The offscreen plugin's spurious-
canceled quirk (documented in this suite since the images test was written,
"pinned as an env residual") supplies exactly that on the loaded runner:
the whole route silently produced nothing.

Files:
- probe-qtconcurrent-cancel-before-start.txt  the mechanism, this Qt build:
  cancel before start ⇒ the runnable never executes (3 consecutive runs)
- ci-artifact-run36538793928-images-route-excerpt.txt  the CI failure text
- nc-reverted-imagesRoute.txt  the fix scoped back (cancel button + canceled
  wiring restored): the new structural pin fails — the busy dialog offers a
  Cancel (2P/1F). The behavioral race itself is stochastic by nature; the
  structural pin is the deterministic lock on the only canceled source.
- pass-after-TestWelcomeRoutes.txt  full suite with the fix, 20P/0F
- pass-after-x2-totals.txt  second run, 20P/0F

The fix: the images-route busy dialog has no Cancel button
(setCancelButton(nullptr)) and no canceled→cancel wiring — the artifact
contract is unconditional: the route always produces its output and always
reports its result. The isCanceled guard in the completion handler stays as
a defensive no-op for any future cancellation-capable conversion.
