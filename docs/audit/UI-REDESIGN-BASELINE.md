# UI redesign: baseline on the untouched BASE (Phase 0, no-regression protocol step 1)

**What this is.** The recorded "before" state that every later UI-redesign phase is compared against.
- **The spec:** plan 06 §4.14 and §7.5. The execution brief is the UI-redesign prompt §4.
- **The rule:** any later difference from these numbers must be listed and justified in that phase's handoff. An unexplained difference is a regression.

| Field | Value |
|---|---|
| BASE | `99219846`: `main` = v1.5.0 (tag `v1.5.0` → `75d90453`) + the Release Gate `shell: pwsh` fix |
| Branch / worktree | `feat/ui-redesign-p0` in an isolated worktree created from BASE |
| Vendored deps | podofo 1.1.0 install, pdfium chromium/7834, onnxruntime 1.17.3, quickjs-ng 0.15.1; `bootstrap-vendor-deps.sh check` = OK |
| Configure | `-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGLYPHPDF_QUICKJS_PIN=0.15.1`. The log shows "Using vendored podofo 1.1.0 from …/third_party/podofo/install" and "quickjs-ng version pin satisfied: 0.15.1". |
| Build | fresh `build-ui`: **1062/1062 steps, 0 errors** |
| **ctest, serial** | **188/188 passed (100 %)**. There are 189 registered suites; R14ProbeBatchSkip is disabled by design. |
| ctest `-j6` | 183/188. It ran while four heavy review agents shared the machine. TestDiffEngine exited with `0xc000012d` (STATUS_COMMITMENT_LIMIT: the machine's memory commit limit was reached), and TestLaneScheduler, TestBatchMode, TestRedactMarkAll and TestWelcomeRoutes failed in the same window. All five pass in the serial run on the same binaries. The `-j6` baseline is to be re-recorded on a quiet machine. |
| **TestViewParity** | **15 passed, 0 failed, 1 XFAIL** (`zoomInAfterFitLeavesFitMode`: the recorded Zoom-after-Fit defect) |

## What TestViewParity pins on BASE
Every one of these capabilities must behave the same through every later phase:
- **Zoom:** In, Out, Actual, Fit Width, Fit Page.
- **Page layout:** Single, Continuous, Two-Page.
- **Presentation** (auto-advance timer) and **Full Screen**, with enter and exit restoring the layout.
- **Dark Mode:** the app theme toggle, which works with no document open.
- **Page filters:** Eye Care and Night Mode, mutually exclusive.
- **RTL:** application-wide, persisted in `QSettings ui/rtl`.
- **Panes:** Thumbnails, Bookmarks, Comments, Layers, Files.
- **Window menu:** Compare works; Split and New window are planned, disabled with a reason.
- **Viewer:** rotation, hyperlinks, page labels.
- **Task screens:** all 13 TaskNav screens are reachable.

## UI sweep baseline
The R17 matrix: 1366×768 and 1920×1080 × `QT_SCALE_FACTOR` 1.0 / 1.5 / 2.0. It is recorded in the follow-up commit that adds `docs/audit/evidence-ui-redesign/baseline/uisweep/`.
