# LANE REPORT — #11 wave 2b, Page Labels writer + UI (2026-10-01)

Lane: `feat/page-labels-writer` @ worktree `D:/pdf/pdf-w2b-pagelabels`, base `fe6ca6f2`.
Mission: PARITY-SCORECARD-2026-09-30 §4 row 11 — "Page Labels writer + UI
(groundwork landed)".

## Headline: CHECK FIRST fired — the mission was already landed at the base

Per the mission's CHECK FIRST clause, the implementation content was found
**already complete at `fe6ca6f2`**, and not merely as "groundwork":

- **Writer half** — `src/core/PageLabels.cpp` `writeNumberTree(PdfMemDocument&, startValue, style, pageCount)` and the `writeNumberTree(pdfPath, startValue, style)` file overload: builds the catalog `/PageLabels` `/Nums` number tree (flat, single range starting at page 0), all five `/S` styles D/r/R/a/A, `/St` written explicitly, pre-existing stale tree removed before the new one is added, invalid input touches nothing. The file overload runs the G13/R01 SafeSave transaction (distinct candidate → validate by re-read → atomic commit; never saves a lazy-loaded document over itself).
- **PagesMode entry point** — `src/modes/PagesMode.cpp` `onApplyPageLabels()` (declared `PagesMode.h:116`), wired as "Apply Page Labels…" in the thumbnail-grid context menu (`fillGridContextMenu`): start-value spinbox + style combo covering all five styles, ARC07 read-only gate, dirty-document refusal, staged SafeSave candidate → write → commit, then engine re-sync (`pdfEditor` reload + `markReload`).
- **Round-trip pins** — `tests/TestPageLabels.cpp` ships its own reader (`readNumberTree`, decoding `/Nums` back into `PageLabelNumEntry`) and pins write → re-read → identical semantics in six writer tests (decimal/roman readback, stale-tree replacement, start-value offset, invalid args, G13 content-bearing survival, refused-commit byte-identity, and the exact PagesMode candidate flow end-to-end).

Proof this is not just in the worktree but **shipped in the release**: the
`v1.5.0` tag itself contains the writer and UI (`git grep -c` on the tag:
`writeNumberTree` ×8 in `PageLabels.cpp`, `onApplyPageLabels` ×4 in
`PagesMode.cpp`). Everything landed 2026-09-23 in consolidation commit
`1991d9c1` — **before** the CHANGELOG [1.5.0] entry (09-29) and the scorecard
(09-30) were written. Those two documents are stale; the code predates them.
Evidence: `docs/audit/evidence-page-labels/checkfirst-v150-tag-contains-writer.txt`.

Per CHECK FIRST, the landed work was **not** re-implemented. The lane's delta
is the one genuinely-missing deliverable: the CHANGELOG [1.5.0] scoping note.

## File-by-file changes (this lane)

| File | Change |
|---|---|
| `CHANGELOG.md` | [1.5.0] bullet "Page Labels groundwork: pure seams + tests; writer/UI deferred with a scoping note" → honest shipped-state note: writer + UI shipped in this release; "Apply Page Labels…" writes the catalog `/PageLabels` number tree via the SafeSave candidate transaction; five /S styles with explicit /St; stale tree replaced; pinned by `TestPageLabels` round trips; scope kept honest (one uniform range per document; per-range UI, /Kids branching, /P prefixes remain open). (+7/−2 lines; the deferral claim was factually false for the tagged release.) |
| `docs/audit/evidence-page-labels/` (new) | R7-adapted evidence: CHECK-FIRST tag/commit proof, the record-level fail-before (stale note quoted from `fe6ca6f2:CHANGELOG.md`), negative-control N/A statement, 3× clean serial gate runs. |
| `docs/audit/LANE-REPORT-page-labels-writer-2026-10-01.md` (new) | This report. |

No product source file was touched (the only edits are `.md` documents), so no
existing test needed to change and none was weakened.

## Build + gate results (Release, ucrt64-first PATH, this worktree's build-rel)

- Configure: runbook flags exactly (`GLYPHPDF_ENABLE_LTO/TESSERACT/LIBSECRET=ON`, fixtures OFF, fuzz OFF). Configured clean.
- Build: `cmake --build build-rel --config Release -- -k 0` — reached `[1081/1081]`; the last target (`TestFileHandleCoordination.exe`, unrelated to this lane) failed its LTO link on the first attempt under concurrent lane load; an incremental retry linked it and a subsequent no-op rebuild returned **BUILD_RC=0** (true cmake exit code, all targets up to date). No other target ever failed.
- Runtime DLLs: `third_party/podofo/install/bin/libpodofo.dll` was absent from the fresh worktree (gitignored build artifact); restored from the main tree per runbook; CMake POST_BUILD deploys it beside the test exe automatically.
- Gate (touched suite `TestPageLabels`, **serial** ctest, three consecutive clean runs, outputs in `evidence-page-labels/pass-after-run{1,2,3}.txt`):
  - Run 1: `Test #61: TestPageLabels … Passed 2.04 sec` — 100% tests passed, 0 failed
  - Run 2: `Test #61: TestPageLabels … Passed 3.07 sec` — 100% tests passed, 0 failed
  - Run 3: `Test #61: TestPageLabels … Passed 1.65 sec` — 100% tests passed, 0 failed

## R7 evidence-contract adaptation (stated, not papered over)

R7 (fail-before pin, negative control, pass-after ×3) applies to behavior
changes. This lane authored **no behavior change** — CHECK FIRST converted the
lane into a record-correction + verification lane. Applied as:

1. **Fail-before** — pinned at the record level: the false deferral note as shipped at `fe6ca6f2` (`record-fail-before-changelog-stale-note.txt`), contradicted by the tag grep (`checkfirst-….txt`).
2. **Negative control** — N/A, and stated so in `evidence-page-labels/README.md`: there is no lane code fix to scoped-revert; the before-state lives in git history. Simulating one would have been theater.
3. **Pass-after ×3** — done literally: 3 consecutive clean serial runs of `TestPageLabels` (above).

## Known limits (shipped scope, documented in code + now in CHANGELOG)

- One uniform labeling range per document (single `/Nums` pair at page 0); no per-range UI, no `/Kids` branching.
- Roman labels beyond 3999 are honestly blank (unrepresentable).
- **`/P` (prefix) is not emitted.** The mission prose mentions prefixes; the scorecard row 11 dispatch target does not; the landed seam's header documents their exclusion as a deliberate scope decision. Extending `PageLabelNumEntry` + writer + UI for prefixes would reverse a documented decision and touch PagesMode UI mid-wave — left as an owner item.

## Owner items for the integrator

1. **Scorecard rows are stale and should be corrected at fold time**: §9.9 row 37 residual ("Page Labels writer/UI deferred by scoping note"), §3 row 60 (PARTIAL claim, "still stamps literal numbers only"), §4 row 11 (this lane's dispatch row) — the writer + UI + pins have shipped since 2026-09-23/`v1.5.0`. This lane did not edit the scorecard (integrator's document, shared with in-flight lanes).
2. **Prefix (/P) support** — small follow-up lane if wanted: struct field + `/P` key + dialog line-edit + round-trip pins; ~half a day including the R7 gates.
3. **Ribbon placement** — Page Labels lives in the Pages thumbnail context menu only; its neighbors (Page Numbers, Header/Footer, Bates) are grouped under Organize ▸ Numbering (`src/shell/RibbonModel.cpp:158`). Adding a ribbon entry touches `src/shell/`, which the in-flight ui-polish lane may own — sequenced deliberately after that lane folds.
4. **Unrelated first-attempt LTO link failure** on `TestFileHandleCoordination.exe` under concurrent lane load (clean on retry) — worth knowing for wave-end full-build verification on this machine.

## Commits

- `docs(changelog)`: replace stale [1.5.0] Page-Labels deferral note with the shipped-state truth.
- `docs(audit)`: lane report + `evidence-page-labels/` (CHECK-FIRST proof, record fail-before, 3× gate runs).
