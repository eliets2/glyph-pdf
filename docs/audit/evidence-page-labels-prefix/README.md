# Evidence — page-labels /P prefix lane (2026-10-01)

Branch `feat/page-labels-prefix`, base `606397b1` (writer landed at `1991d9c1`).
R7 discipline: fail-before RED → NC once → pass-after ×3 serial.

| File | Stage | What it proves |
|---|---|---|
| `build-red.log` | fail-before | Builds with the prefix API surface (entry field, default-param overloads, test-reader /P decode) but WITHOUT /P emission. BUILD_RC=0. |
| `test-red.log` | fail-before RED | 3/4 new pins RED for the right reason (written tree prefix "" ≠ expected "Fig."/"App-"/"pref-"); `writeNumberTree_emptyPrefixNotWritten` green by design (guard pin: a writer that never writes /P trivially omits it for empty input — its value is NC detection of an always-write regression); 20 pre-existing pins green (API extension is behavior-neutral). Totals: 20 passed, 3 failed. |
| `build-green.log` | fix | /P emission + file-overload /P validation added. BUILD_RC=0. |
| `test-green-1.log` | (superseded) | First green attempt: 22/23 — exposed a wrong expectation inside the new `writeNumberTree_prefix_readback` pin itself (startValue 4 in Roman is "IV","V","VI", not "I","II","III"); the structural /P round-trip assert had already passed. Fixed the pin's own expectation. |
| `build-pass.log` | pass-after | Final build. BUILD_RC=0. |
| `test-pass-1.log` | pass-after 1/3 | Totals: 23 passed, 0 failed. |
| `test-pass-2.log` | pass-after 2/3 | Totals: 23 passed, 0 failed. |
| `test-pass-3.log` | pass-after 3/3 | Totals: 23 passed, 0 failed. |
| `build-nc.log` | NC | /P emission commented out, rebuilt. BUILD_RC=0. |
| `test-nc.log` | NC RED | Same 3 prefix pins RED again — 2 of them via the file overload's /P-matches-request validation ("returned FALSE"), proving the validation gate catches a non-emitting writer. Totals: 20 passed, 3 failed. No false alarms elsewhere. |
| `build-full.log` | full tree | Whole-project build (all targets), `-k 0 -j 2`. Cumulative log of the final resume attempt (BUILD_RC=0, no FAILED steps in it) appended after attempt 2's tail; the 2 FAILED lines it contains belong to attempt 2, preserved verbatim. |
| `build-full-attempt1-killed.log` | full tree | Attempt 1: killed externally (exit 137) mid-LTO-link, 0 FAILED build steps. |
| `build-full-attempt2-killed.log` | full tree | Attempt 2 (incremental resume): killed externally (exit 137) at the last ~33 steps; 2 transient LTO link failures (`lto-wrapper failed`, TestTextEditStyle + TestCheckedMutationCoverage) — the runbook's transient-LTO class. |
| *(attempt 3 → `build-full.log`)* | full tree | Incremental resume: BUILD_RC=0; both previously failing exes linked. |

All test runs SERIAL, single process, offscreen platform, third_party DLL dirs
prepended (ucrt64 ships a stale libpodofo.dll — 0xc0000139 guard).

Notes:
- One test-authoring defect was caught and fixed during RED staging: the raw
  catalog check in `writeNumberTree_emptyPrefixNotWritten` asserted
  `labels->IsReference()`, but PoDoFo 1.1 `FindKey` can return the resolved
  dictionary; the check now accepts both shapes (mirrors the production
  validation). The recorded RED run is post-fix.
- `build-red.log` also contains an earlier intermediate compile error of the
  test reader (`PdfString` accessor chain — one `.GetString()` short) from the
  first RED build attempt; fixed before the recorded RED run.
