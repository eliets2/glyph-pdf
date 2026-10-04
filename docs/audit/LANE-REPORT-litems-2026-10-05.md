# LANE REPORT — r5-litems (r4-ux forwarded findings 7/9/10)

- **Date:** 2026-10-05
- **Lane:** GlyphPDF round-5 fix lane (three findings forwarded by the r4-ux
  verifier from `docs/audit/LANE-REPORT-r4-ux-2026-10-04.md` §5, originally
  DeepSeek UX-audit findings 7/9/10)
- **Worktree:** `D:/pdf/pdf-litems` — branch `feat/litems`, base `2c09393b`
  (no rebase/merge onto main, per lane rules; the integrator reconciles)
- **Resume note:** the first lane instance died on provider infrastructure
  mid-lane. It left the full WIP (4 dirty files) and a complete evidence dir
  (`docs/audit/evidence-litems/`) but no PROGRESS.md. The resumed instance
  **judged the evidence honest and reused it** (every RED names the new pin
  and is paired with a scoped restore build; every green log is a real ctest
  run), then **re-verified the whole chain independently** on 2026-10-05
  (§4). `PROGRESS.md` in the evidence dir records both timelines.
- **Status:** COMPLETE — 3/3 forwarded findings fixed, fail-before RED
  (scoped, restored, recorded once) → pass-after ×3 SERIAL recorded for every
  new pin, full serial suite green twice (202/202 executed, 2
  disabled-by-design — §4)
- **Commits:** `b629896b` `fix(r5-litems)` — captured modal-text pins,
  resolved-state HC parity, thumbnail accessibleName (code + pins);
  `docs(audit)` — this report + `docs/audit/evidence-litems/`

## 1. Findings triage — every forwarded finding verified against the CURRENT tree first

| # | Finding (r4-ux §5 → ux-specialist) | Verdict at verify time | Evidence |
|---|---|---|---|
| 7 [M] | `TestPersistenceOutcomes::closeAfterFailedSaveKeepsDocumentOpen` dismissed the failed-save box with a generic `dismissNextModal()`; the box text was never captured or asserted — a regression to a generic "An error occurred" sailed through | VALID (test-side). The **product half was already satisfied at base**: `HomeController.cpp:339-345` (on-save failure path, this test's exact scenario) already shows `QMessageBox::critical("Save Failed", "Could not save '%1'. Check that the disk is not full and the file is not write-protected.")` — the UX-14/R2-2 wording from earlier rounds. What was missing was the pin. | base `TestPersistenceOutcomes.cpp:132-137` (`dismissNextModal`, text dropped on the floor); product wording present at base via `git show 2c09393b:src/shell/controllers/HomeController.cpp` |
| 9 [L] | HC parity asserted as raw `sheet.contains(selector)` string presence — presence of a selector does not prove the rule WINS; a later override keeps the string check green | VALID | base `TestViewingModes.cpp:361-366` — two `QVERIFY2(sheet.contains(...))` checks |
| 10 [L] | Placeholder thumbnails had no `accessibleName` pin — and the product thumbnail surface was an unnamed `QWidget`, so a screen reader walked an anonymous client area | VALID | base `TestThumbnailOffGui.cpp` had no `accessibleName` anywhere; base `ThumbnailSidebar.cpp::createThumbWidget` set only `pageIndex`/`current` properties on the interactive surface |

No missioned item was verified-stale. The one nuance (7's product half) is
recorded above and in §2.

## 2. What was fixed

### Finding 7 [M] — the failed-save box text is captured and pinned truthful (`tests/TestPersistenceOutcomes.cpp`)

- The generic closer is replaced by **`captureNextModalText`**
  (`tests/TestPersistenceOutcomes.cpp:140-157`): same budgeted-retry idiom as
  the harness's other modal drivers (the box registers as
  `QApplication::activeModalWidget` only after a nested-loop turn), but it
  grabs `windowTitle()` / `text()` / `informativeText()` off the
  `QMessageBox` before closing it. A regression degrades into a qWarning and
  an empty capture, not a hang.
- `closeAfterFailedSaveKeepsDocumentOpen` now pins the honesty contract
  (`:321-346`): title must be **"Save Failed"** (names the event — a save
  that did NOT persist — not a generic "Error"), and the body must be the
  exact typed wording **"Could not save '<doc>'. Check that the disk is not
  full and the file is not write-protected."** — naming the document and the
  cause that really applies in this scenario (the harness made the
  destination read-only; the RED run's `QCRITICAL` line shows the engine's
  observed refusal is exactly the not-writable case). The retry half of the
  contract stays pinned by the pre-existing assertions (window open, work
  dirty, history intact, retry persists).
- **Product side unchanged, deliberately:** the dialog already carries the
  truthful UX-14 wording at base. The pin's bite is proven by the RED, which
  scopes the product wording back to the generic shape — see §3.

### Finding 9 [L] — HC parity asserted on the RESOLVED state (`tests/TestViewingModes.cpp`)

Inside `disabledControlsReadAsDisabledUnderEveryShippedSheet`, the two raw
`sheet.contains(...)` checks are replaced by the same computed-style approach
the disabled-controls matrix uses — the sheet is applied first
(`qApp->setStyleSheet(sheet)` moved before the parity block), then for
HighContrast:

- **Paper preview at the pixel level** (`:376-391`): a plain `QWidget`
  named `#thumbPaper` is polished and `grab()`bed — its background is
  painted by the sheet rule (`resources/theme_highcontrast.qss:356`), so a
  palette read cannot see it — and the center pixel must be the paper token
  **`#e8e6df`**. If the rule stops winning, the preview paints black and
  vanishes against the black sidebar; the pin now catches that.
- **Mono input font read back after polish** (`:392-399`): a
  `QLineEdit` with `mono=true` must resolve `font().family()` to
  **"JetBrains Mono"** (sheet rule `theme_highcontrast.qss:349`).

Both rules verified present in the shipped HC sheet at base and at commit
time, so green is due to real, load-bearing rules — not to a weakened check.
The old string checks are gone, not weakened: the replacement asserts a
strictly stronger fact (the rule wins on the resolved state).

### Finding 10 [L] — thumbnails named for their page, on and off the GUI

- **Product** (`src/ui/ThumbnailSidebar.cpp:300-309`): `createThumbWidget`
  names each interactive thumbnail surface `accessibleName(tr("Page %1"))`
  for the page it shows and carries the interaction contract as
  `accessibleDescription(tr("Thumbnail preview. Click to navigate, drag to
  reorder."))` — the same contract the sidebar-level description already
  disclosed (`:80-81`), now on the surface that actually receives the
  interaction.
- **Pin** (`tests/TestThumbnailOffGui.cpp:301-339`, new slot
  `placeholderThumbnailsExposeAccessiblePageNames`): a fully-visible 3-page
  document must yield exactly 3 `thumbItem` widgets with no duplicate page;
  each slot's `accessibleName` must be **its own** `"Page %1"` (a constant
  name reused across slots fails exactly as an empty one), and the
  accessible description must disclose the interaction contract.

## 3. RED → green protocol (per pin; scoped, restored, recorded once)

The product side for 7 and 9 already carried the correct behavior at base,
so the fail-before state was produced by **temporarily scoping the shipped
truth out** (never by weakening a test), rebuilding only the affected
targets, capturing the failure once, and restoring (each capture paired with
a ninja log naming exactly the reverted + restored targets):

| RED | Scoped-out truth | Recorded failure | Restore |
|---|---|---|---|
| `red-f7-generic-save-failure.txt` | dialog wording → generic `"Error"` shape (`HomeController.cpp`) | `the failed-save box title is "Error", expected "Save Failed"` | `build-red1-restore.log` |
| `red-f9a-hc-thumbpaper-rule-lost.txt` | HC `#thumbPaper` rule removed | `the thumbnail paper preview paints #000000, expected ... #e8e6df` | before f9b stage |
| `red-f9b-hc-mono-overridden.txt` | HC `QLineEdit[mono="true"]` font rule removed (paper restored) | `a mono input resolves to ... "Segoe UI", expected ... "JetBrains Mono"` | `build-red3.log` region |
| `red-f10-thumbnail-accessible-name.txt` | `ThumbnailSidebar.cpp` accessible-name edit scoped out | `thumbnail page slot 1 exposes accessibleName "", expected "Page 1"` | `build-red3.log` |

Build logs: `build-red1.log`, `build-red1-restore.log`, `build-red3.log`;
final full build of the restored tree `build-final.log`.

## 4. Verification evidence

**Prior instance (2026-10-04), reused after judgment:**

- Focused gate ×3 SERIAL (`green1.log`/`green2.log`/`green3.log`):
  `TestThumbnailOffGui` + `TestViewingModes` + `TestPersistenceOutcomes` —
  100% passed, 0 failed out of 3, three consecutive passes.
- Full serial suite (`serial-full-suite.log`, whole-project `ctest`, no
  `-j`, 324 s): **100% tests passed, 0 tests failed out of 202** (204
  registered; `R14ProbeRedactSpace` and `R14ProbeBatchSkip` are the
  deliberate disabled probes documented since r4-ux — untouched).

**Resumed instance (2026-10-05), independent re-verification — no re-capture
needed, all runs reproduced:**

- Incremental rebuild of the as-left tree (`build-resume-20261005.log`):
  **BUILD_RC=0**.
- Focused gate ×3 SERIAL (`green-resume-20261005.log`): 100% out of 3,
  three passes (9/9).
- Full serial suite (`serial-full-suite-resume-20261005.log`): **SERIAL_RC=0,
  100% tests passed, 0 tests failed out of 202**.

No flakes in any run (prior or resumed); no re-run was triggered. The
base lane commit's "final serial 203/203" counted the same suite at its
registration; the current honest count is 202 executed / 202 passed with 2
disabled-by-design, as recorded in both serial logs.

## 5. Remaining findings

None missioned remain. Of the r4-ux §5 forwarded list, 7/9/10 are closed by
this lane; the r4-ux report's other §5 entry (audit L8) was already folded
into that lane's finding-2 pin and required no action here.

Coupling notes for future changes (the pins are load-bearing by design):

- The finding-7 pins are coupled to the shipped `HomeController` tr()
  wording ("Save Failed" + the write-protected body). Any rewording of that
  dialog must update the pin in the same commit — that coupling is the
  point.
- The finding-9 pins are coupled to the HC sheet tokens (`#e8e6df` paper,
  "JetBrains Mono" mono family). A sheet token change must update the pin.
- The finding-10 pin is coupled to the `tr("Page %1")` name shape and the
  "Click to navigate" contract phrase in `ThumbnailSidebar`.

## 6. Handoff notes for the integrator

- Lane sits on base `2c09393b`; per lane rules it did NOT rebase or merge.
  Conflict surface is minimal: one hunk in
  `src/ui/ThumbnailSidebar.cpp::createThumbWidget` (pure addition inside the
  function) and three test files (`TestPersistenceOutcomes.cpp` — helper +
  one slot; `TestThumbnailOffGui.cpp` — one new slot; `TestViewingModes.cpp`
  — one block inside `disabledControlsReadAsDisabledUnderEveryShippedSheet`).
- `docs/audit/evidence-litems/` holds all lane evidence including both
  timelines (`PROGRESS.md`); nothing in it is generated at build time.
- The finding-7 RED protocol (product wording scoped out, restored) is the
  idiom to reuse whenever a pin guards shipped text that predates the pin:
  it proves the pin bites without pretending the product regressed.
