# evidence-page-labels — lane #11 (wave 2b), 2026-10-01

CHECK FIRST result: the mission's implementation content was **already landed at the
lane base fe6ca6f2** — `core/PageLabels` writer (`writeNumberTree`, both overloads,
all five /S styles D/r/R/a/A, explicit /St, stale-tree replacement, SafeSave
candidate transaction), the PagesMode entry point ("Apply Page Labels…" context
menu + dialog, read-only gate, dirty refusal, engine re-sync), and the round-trip
pins (`TestPageLabels`: write → `readNumberTree` → identical semantics). All of it
landed 2026-09-23 in consolidation commit `1991d9c1` and is contained in the
**v1.5.0 tag** — while CHANGELOG [1.5.0] and PARITY-SCORECARD-2026-09-30 both
still claimed writer/UI "deferred". Those two documents are stale, not the code.

## What this lane therefore changed

One thing only: the CHANGELOG [1.5.0] note, replaced with an honest shipped-state
note (plus this evidence and the lane report). **No behavior change was authored
by this lane**, so the R7 fail-before / negative-control pins have no code target;
what is recorded instead:

| File | Role |
|---|---|
| `checkfirst-v150-tag-contains-writer.txt` | CHECK FIRST proof: tag grep, landing commit/dates, pin inventory |
| `record-fail-before-changelog-stale-note.txt` | The red state of the RECORD: the false deferral note as shipped at fe6ca6f2 |
| `pass-after-run1.txt` … `run3.txt` | 3 consecutive clean **serial** ctest runs of the touched suite `TestPageLabels` at the lane tree (all Passed, 100%) |

## Negative control

Not applicable — scoped-revert semantics only exist for a code fix; the sole
product edit is a documentation correction whose "before" state is preserved in
git (`fe6ca6f2:CHANGELOG.md`, quoted in `record-fail-before-…txt`). Stated
explicitly rather than simulated.

## Gate results (serial, Release, this worktree's build-rel)

- Run 1: `Test #61: TestPageLabels … Passed 2.04 sec` — 100% tests passed
- Run 2: `Test #61: TestPageLabels … Passed 3.07 sec` — 100% tests passed
- Run 3: `Test #61: TestPageLabels … Passed 1.65 sec` — 100% tests passed

## Known limits surfaced (owner items, not lane work)

- `PageLabelNumEntry` / writer do **not** emit `/P` (prefix) — excluded by the
  landed code's documented scope decision ("one uniform labeling range per
  document"); the mission prose mentions prefixes, the scorecard row does not.
- Page Labels is absent from the Organize ▸ Numbering ribbon group (its neighbors
  Page Numbers / Header/Footer / Bates live there); entry is via the Pages
  thumbnail context menu. Ribbon placement would touch `src/shell/RibbonModel.*`,
  a file the in-flight ui-polish lane may own — left to the integrator.
- PARITY-SCORECARD-2026-09-30 §3 row 60, §4 row 11 and the §9.9 residual remain
  stale; the scorecard is the integrator's document and was not edited by this lane.
