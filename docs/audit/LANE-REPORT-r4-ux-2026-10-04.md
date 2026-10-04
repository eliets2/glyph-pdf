# LANE REPORT — r4-ux (DeepSeek UX-audit findings fixes)

- **Date:** 2026-10-04
- **Lane:** GlyphPDF round-4 UX fix lane (salvaged cross-model UX-audit findings)
- **Worktree:** `D:/pdf/pdf-r4-ux` — branch `feat/r4-ux`, base `9f7787ee`
  (NOTE: `main` has since moved to `4e65bcbb` (font-subsetting + Linux merge);
  per lane brief this lane did NOT rebase — the integrator reconciles.)
- **Context report:** `D:/pdf/verification/crossmodel/ux-specialist.part4.md`
  (DeepSeek UX audit, H=1 · M=6 · L=3; parts 2/3 are the earlier UI-consistency
  pass, out of this lane's scope)
- **Status:** COMPLETE — all 6 missioned findings fixed (none stale at verify
  time), RED-×1 → SERIAL-green-×3 recorded for every new pin, full serial
  suite green (201/201, 2 disabled-by-design — see §4)
- **Commits:** `fix(r4-ux)` — truthful 7-Zip resolution provenance, fallback-view
  screen-reader disclosure, refusal-wording + disabled-state pin hardening
  (code + pins); `docs(audit)` — this report + `docs/audit/evidence-r4-ux/`

## 1. Findings triage — every missioned finding verified against the CURRENT tree first

Each of the six missioned findings was re-verified against the base tree
(`git show HEAD:<file>`) before being accepted as still valid. **None were
stale.** Evidence for each:

| # | Finding (part4) | Verdict at verify time | Evidence |
|---|---|---|---|
| 1 [H] | `third_party/7zip/PROVENANCE.md` "Runtime resolution order" paragraph still advertised the removed `PATH` + `C:/Program Files/7-Zip/` fallback legs | VALID (stale doc) | base `PROVENANCE.md:67` — "application-owned directory first … then `PATH`, then the conventional `C:/Program Files/7-Zip/`"; the shipped code is `gp::SevenZipLocator::locateVerified` (`src/engines/SevenZipLocator.cpp`): **bundled-only** resolution, `TestSevenZipBundle::resolverDisclosesAbsenceHonestly` pins absence → empty, and r3-sec added runtime SHA-256 re-verification against compile-baked pins with a per-session verdict cache (mutex-guarded `QHash`), refusing a tampered/stale staged pair before launch |
| 2 [M] | `TestViewingModes::disabledControlsReadAsDisabledUnderEveryShippedSheet` asserted `QPalette::WindowText` for all families — a proxy that is the painted role of none of them (line edits/spin/combos paint `Text`, the button family paints `ButtonText`) | VALID | base `TestViewingModes.cpp:346,350` |
| 3 [M] | `loadedPageIsVisibleUnderTheShippedStylesheet` installed a hand-written one-liner (`QWidget { background-color: #1e1f22; }`), never a shipped sheet | VALID | base `TestViewingModes.cpp:197` |
| 4 [M] | Coverage gap: no `QComboBox`, `QToolButton`, `QRadioButton`; the `QCheckBox` indicator fill was never probed (only its label text) | VALID | base test held exactly four widgets, no indicator probe |
| 5 [M] | `TestRotateView::fallbackSwapsSurfacesAndReverts` pinned the pixmap swap/revert but nothing about disclosure; the pixmap-only `QLabel` (`rotatedPageView`) carried no accessible name/description | VALID | base `TestRotateView.cpp` had no `accessibleName`/`accessibleDescription` anywhere — and neither did `src/ui/PdfViewerWidget.cpp` |
| 6 [M] | `TestStampImageImport::importRefusesUnusableImagesWithTypedError` checked only `!error.isEmpty()` — a refusal message of literally `"error"` passed | VALID | base `TestStampImageImport.cpp:170,174,179,183` |

## 2. What was fixed

### Finding 1 [H] — truthful resolution provenance (`third_party/7zip/PROVENANCE.md`)

The "Runtime resolution order" paragraph is rewritten to describe the shipped
reality: resolution is the **application-owned copy ONLY**
(`gp::SevenZipLocator::locateVerified`, extracted from SafeSave by r3-api); the
`PATH` / `Program Files` legs were deliberately **REMOVED** in the wave-2b
security audit (F-02, CWE-427 — a planted `7z.exe` on those legs would receive
the document bytes AND the package password) and must **not** be reintroduced;
absence means "the vendored bundle is absent" and is disclosed honestly by the
encrypted-package dialog; since r3-sec (CWE-494) the locator re-verifies the
staged `7z.exe`/`7z.dll` SHA-256 against the pins compiled into the binary at
first use (per-session verdict cache) and refuses — never launches — a
tampered or stale staged copy. Verified line-by-line against
`src/engines/SevenZipLocator.cpp`.

Doc records that repeated the stale claim were amended in the same commit:
`LICENSE-3RD-PARTY.md` (7-Zip row: "resolution is the application-owned copy
ONLY") and `docs/audit/PARITY-SCORECARD-2026-09-30.md` item 75 (note amended
2026-10-04).

### Findings 2 + 4 + L8 — disabled-controls pin hardened to painted reality (`tests/TestViewingModes.cpp`)

`disabledControlsReadAsDisabledUnderEveryShippedSheet` now:

- **Probes the role each family actually paints with** (finding 2): `Text` for
  `QLineEdit` / `QSpinBox` / `QComboBox`, `ButtonText` for `QPushButton` /
  `QToolButton` / `QCheckBox` / `QRadioButton`. The earlier blanket
  `QPalette::WindowText` read was a palette proxy that is the painted role of
  none of these families.
- **Extends the matrix** (finding 4): `QComboBox`, `QToolButton` and
  `QRadioButton` join the original four — seven families per theme.
- **Pixel-pins the checkbox INDICATOR** (finding 4): the probe resolves
  `SE_CheckBoxIndicator`, grabs the widget, and asserts the **checked**
  indicator fill is the theme accent when enabled (`#ff8c42` dark /
  `#c25a18` light / `#ffff00` HC) and drops to the dim token when disabled
  (`#52555a` dark / `#9c9a90` light / `#666666` HC — matched to the sheets'
  `QCheckBox::indicator:checked:disabled` rules). A palette read cannot see
  indicator fills painted by sheet rules; pixels can.
- **Asserts enabled colours untouched** (audit L8): every family's enabled
  foreground must equal the sheet's base token (`#dfe1e5` dark / `#1a1b1e`
  light / `#ffffff` HC) — a sheet that dimmed the *enabled* foreground too now
  fails.

### Finding 3 — the page-visible pin runs against the REAL shipped sheets (`tests/TestViewingModes.cpp`)

`loadedPageIsVisibleUnderTheShippedStylesheet` loops Dark / Light /
High-Contrast, loads each through the same `loadThemeSheet()` resolve helper
the disabled-controls pin uses (resource path, then the injected source dir),
and asserts the composited viewer still shows paper pixels (> 5000) under each
sheet. A shipped sheet with a broader/different base rule that hides pages now
fails this pin instead of sailing past a hand-written one-liner.

### Finding 5 — the fallback view discloses itself (`src/ui/PdfViewerWidget.cpp`, `tests/TestRotateView.cpp`)

Production: the `rotatedPageView` fallback `QLabel` now carries
`accessibleName` ("Rotated page view") and an `accessibleDescription`
disclosing the paused free scrolling, the surviving page navigation, and the
session-only contract — the same honest-disclosure contract the Rotate View
status message tells sighted users, delivered to screen readers on exactly the
surface where scrolling pauses (previously an unlabeled image).

Pin: `fallbackSwapsSurfacesAndReverts` asserts the fallback label names
itself ("Rotated", case-insensitive), discloses the paused scrolling
("scrolling" + "paus"), and points at the surviving page navigation
("page navigation"). Existing swap/aspect/revert assertions unchanged — no
test was weakened.

### Finding 6 — refusal wording pinned, not just non-emptiness (`tests/TestStampImageImport.cpp`)

`importRefusesUnusableImagesWithTypedError` now pins the EXACT user-facing
strings (verified byte-equal against `src/core/StampLibrary.cpp:181-195`):

- garbage bytes / truncated PNG → `"Could not read <name> as an image (unsupported or corrupt file)."`
- missing file → `"Choose an image file first."`
- empty name → `"Enter a name for the stamp."`

A regression to a generic `"error"` fails the pin (this exact shape is the
recorded RED). The catalog-untouched / no-copied-image assertions stay.

## 3. RED → GREEN record (protocol: RED once against a scoped, since-restored regression simulation; then ×3 SERIAL green)

RED evidence (one run each, `docs/audit/evidence-r4-ux/red-*.log`) — each new
pin was shown to fail against its named regression, recorded once, and the
scoped mutation restored before the green runs:

| Pin | RED log | Observed failure |
|---|---|---|
| painted-role probe (f2) | `red-f2-painted-role.log` | `disabledControls…`: "disabled **QPushButton** still paints in the enabled foreground" — with the button-family dim withdrawn, the painted-role pin catches it |
| shipped-sheet probe (f3) | `red-f3-sheet-hides-page.log` | `loadedPageIsVisible…`: `paperPixels > 5000` never satisfied under a page-hiding sheet (QTRY timeout, 18.4 s run) |
| family matrix (f4) | `red-f4-combobox-coverage.log` | full-slot run, 10 passed / `disabledControls…` failed: "disabled **QComboBox** still paints in the enabled foreground" |
| indicator probe (f4b) | `red-f4b-indicator-accent.log` | "a disabled checked indicator must not keep the accent fill" (`disabledFill != enabledFill` FALSE) |
| fallback disclosure (f5) | `red-f5-fallback-accessible-disclosure.log` | `TestRotateView`: `accessibleName` was `""` → "fallback accessibleName must name the rotated surface" |
| refusal wording (f6) | `red-f6-generic-refusal-wording.log` | `TestStampImageImport`: actual error literally `"error"` vs the pinned typed string |

GREEN (serial `ctest -R "^(TestViewingModes|TestRotateView|TestStampImageImport)$"`,
`BUILD/build-rel`, Windows, Release):

| Pass | Log | Result |
|---|---|---|
| 1 | `green1.log` | 3/3 passed, RC=0 (TestStampImageImport 0.47 s, TestRotateView 0.31 s, TestViewingModes 0.73 s) |
| 2 | `green2.log` | 3/3 passed, RC=0 |
| 3 | `green3.log` | 3/3 passed, RC=0 |

No flakes occurred; the re-run-on-flake rule never triggered.

Finding 1 is a documentation fix — the RED protocol does not apply (the stale
paragraph was the defect; the fix is the rewrite above, verified against the
shipped locator code and the standing `TestSevenZipBundle` pins).

## 4. Build and gate record

- Build: `PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH cmake --build
  D:/pdf/pdf-r4-ux/build-rel --config Release -- -k 0 -j 2` → **BUILD_RC=0**
  first pass (no LTO retry needed); log `evidence-r4-ux/build-final.log`.
  The resumed lane's prior partial build was completed to up-to-date; the
  rebuilt `TestViewingModes.exe` was verified to carry the new pin strings
  before the green runs (guards against a stale-binary false green).
- Focused serial gate ×3: 9/9 passed (above).
- Full serial suite (whole-project `ctest`, no `-j`, 337.9 s): 
  `serial-full-suite.log` — **201/201 passed, 0 failed** (FULL_RC=0); the two
  "did not run" entries (`R14ProbeRedactSpace`, `R14ProbeBatchSkip`) are
  DELIBERATE DISABLED probes registered in the base `CMakeLists.txt:6121`
  ("same class as R14ProbeBatchSkip") — the documented prior state, untouched
  by this lane.

## 5. Remaining audit findings — verified, NOT missioned, left for the integrator

Verified against the current tree for completeness; all three are still valid
but were **not in this lane's brief**, so they were deliberately not fixed
here (no scope creep; handing forward with evidence):

- **Finding 7 [M]** — `TestPersistenceOutcomes::closeAfterFailedSaveKeepsDocumentOpen`
  (`tests/TestPersistenceOutcomes.cpp:278-313`) still dismisses the failed-save
  error box with the generic `dismissNextModal()`; the box text is never
  captured or asserted. Fixing it needs a capture-next-modal-text helper in
  that harness (none exists today — only click/close/file-pick drivers).
- **Finding 9 [L]** — HC parity is still asserted as raw `sheet.contains(...)`
  string presence (`tests/TestViewingModes.cpp:361-366`), not resolved state.
- **Finding 10 [L]** — `TestThumbnailOffGui` placeholder thumbnails still have
  no `accessibleName` pin (no occurrence in the file).

Audit L8 [L] ("enabled colours never checked") was folded into the finding-2
pin (enabled-token assertions for all seven families) as part of the missioned
work — no separate lane action needed.

## 6. Handoff notes for the integrator

- This lane sits on base `9f7787ee` per brief; `main` has moved to `4e65bcbb`
  (font-subsetting + Linux). Expected conflict surface is minimal: the lane
  touches `third_party/7zip/PROVENANCE.md`, `LICENSE-3RD-PARTY.md`,
  `docs/audit/PARITY-SCORECARD-2026-09-30.md` (item 75), one hunk in
  `src/ui/PdfViewerWidget.cpp::updateRotatedPageView`, and three test files
  (`TestViewingModes.cpp`, `TestRotateView.cpp`, `TestStampImageImport.cpp`).
  The audit's finding-8 referenced `CompressDialog`/font-subsetting code was
  NOT touched by this lane.
- The QSS expectation table in `TestViewingModes.cpp` is coupled to the theme
  sheets' tokens (base/accent/dim + `::indicator:checked:disabled` fills);
  any future sheet token change must update both.
