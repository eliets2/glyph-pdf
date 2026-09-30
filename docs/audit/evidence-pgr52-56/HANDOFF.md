# PGR-52/54/55/56 lane — handoff (feat/pgr-52-56, 2026-09-30)

The four recorded presets-review owner items, all FIXED with R7 evidence.
Branch `feat/pgr-52-56`, worktree `D:/pdf/pdf-feat-pgr`, linear on `main`
(base `8315e390`), 4 fix commits + this evidence record, 0 merges. FF-ready.

Cherry-pick list (R13): `723476e1` (PGR-52) -> `4ec51db0` (PGR-54) ->
`678dd645` (PGR-55) -> `b1ceb1bf` (PGR-56). 1 and 4 are independent of 2/3;
2 and 3 touch only `src/modes/BatchMode.*` + `tests/TestBatchPresetsP2.cpp`
sequentially (2 before 3 is the landed order; both pins are independent).

| ID | Verdict | Pin (RED -> GREEN) |
|---|---|---|
| PGR-52 | FIXED `723476e1` | TestPatternRedact: 16P/1F (fail-before, seam-only tree) -> 17P/0F |
| PGR-54 | FIXED `4ec51db0` | TestBatchPresetsP2: 42P/1F -> 43P/0F |
| PGR-55 | FIXED `678dd645` | TestBatchPresetsP2: 43P/1F -> 43/44P/1F lineage -> 44P/0F |
| PGR-56 | FIXED `b1ceb1bf` | TestBatchPresetsP2: 44P/1F -> 45P/0F |

Fail-before protocol per item (single-writer tree, no stash): the pin was
built and run on a tree containing the SEAM but NOT the fix (the fix files
restored from HEAD in the working tree, staged fix kept in the index); the
RED run is archived verbatim; the fix was then restored from the index and
the pass-after run archived. Every RED is a single failing test — the new
pin only.

Gates: 17/17 active touched-surface suites x3 consecutive green
(`5-touched-suites-run{1,2,3}.txt`; R14ProbeBatchSkip is DISABLED by design,
18th in the regex, "Not Run (Disabled)" — never a silent skip). Suites:
R14ProbeSep13Fixes, TestGsdW2Probe, TestBatchMode, TestBatchOcrConfidence,
TestBatchOcrLanguage, TestBatchOcrSkipText, TestBatchOpsCoverage,
TestBatchPresets, TestBatchPresetsP2, TestPatternRedact,
TestPgr35BatchCollision, TestPgr37PageSpaceLaw, TestRedactClearMarks,
TestRedactMarkAll, TestSep13LeadBatchMerge, TestSweepW1PresetAdversary,
TestSweepW3UxFlows.

Build notes for fresh worktrees (unchanged from the presets-review lane
handoff, plus): `pcre2-16` links from ucrt64 (`-lpcre2-16` ->
`libpcre2-16-0.dll`, already in the ucrt64 PATH ctest runs under; it is the
SAME DLL Qt6Core imports its pcre2_*_16 symbols from — no new runtime
dependency ships).
