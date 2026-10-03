# R7 evidence — r3-sec security lane (finished 2026-10-03)

Mission: round-3 security fix lane for (1) the security-auditor finding 2
(runtime 7-Zip hash verification, security M, CWE-494) and (2) the
native-adversary wave-2b finding F-6 (watcher fan-out backstop). Base
`f6d9ca1f`, branch `feat/r3-sec`, worktree `D:/pdf/pdf-r3-sec`, build dir
`build-rel` (in-worktree only).

All test runs SERIAL (ctest default single-process), offscreen where the
suite requires it, from `build-rel`. Per behavior change: fail-before RED,
NC once (fix neutralized → its own pins RED, guards green), pass-after
×3 SERIAL.

## Files

| File | What it is |
|---|---|
| `build-configure.log` | Runbook configure (Release, LTO, Tesseract, libsecret, no fixtures, no fuzz). CONFIGURE_RC=0. |
| `build-stageA-full.log` | Full-tree build with the new pins + seam plumbing staged (behavior NOT yet changed): externally KILLED at [994/1108] (exit 137, the prior lanes' documented contention class). 0 FAILED lines up to the kill. |
| `build-stageA-resume.log` | Incremental resume of the killed build: completed exit 0 (BUILD_RC=0 recorded from the runner stream). |
| `red-sevenzipbundle.log` | **Fix-1 fail-before RED**: 7 passed, 2 failed — `tamperedBundledPairIsRefusedWithIntegrityDisclosure` (the byte-flipped 7z.exe RESOLVES pre-fix: "a tampered bundled 7z.exe must be REFUSED at resolution") and `integrityCheckRunsOncePerSession` (control leg: "the tamper is not being detected at all"). All pre-existing pins green, including the fixture-strengthened `resolverPrefersAppOwnedBinary` (real bytes) and the end-to-end M-1 round trip. |
| `red-hotfolder.log` | **Fix-2 fail-before RED**: 25 passed, 4 failed — the four new F-6 pins (cap simulated via the addPaths hook: no polling engagement, no disclosure; the BatchMode wiring pin fails at `isPolling()`). All 25 pre-existing pins green (F-5/F-7 guards, recursive/polling pins). |
| `build-fix-reconf.log` | Reconfigure — picks up the new `GLYPHPDF_7ZIP_{EXE,DLL}_SHA256` compile definitions on `pdfws_engines`. Clean. |
| `build-fix.log` | Fix-stage builds, three appended segments: (1) first build externally KILLED mid-relink (exit 137, same documented class); (2) incremental resume BUILD_RC=0; (3) rebuild after the disclosure-wording alignment (see lane report §3.2) BUILD_RC=0. |
| `pass-1.log` | **Pass-after ×1**: 2/2 suites green (TestSevenZipBundle 9/9 pins, TestHotFolder 29/29). |
| `build-nc.log` | NC stage build (both fixes neutralized behind `NC STAGE` markers): BUILD_RC=0. |
| `nc.log` | **NC once per fix**: TestSevenZipBundle 7/2 (fix-1 neutralization caught by exactly its own two pins — tamper accepted again, control leg red); TestHotFolder 25/4 (fix-2 neutralization caught by exactly the four F-6 pins). Guards green under NC: `goodBundledPairPassesRuntimeVerification`, `resolverPrefersAppOwnedBinary`, absence pin, `vendoredBinariesAreCommittedAndHashPinned`, `bundledBinaryEndToEndStdinPassword`, and all 25 pre-existing hot-folder pins. Zero collateral reds. |
| `build-restore.log` | Fixes restored (`grep -rn "NC STAGE" src/ tests/` → 0 matches, verified in the same command): BUILD_RC=0. |
| `pass-2.log` | **Pass-after ×2** (post-restore): 2/2 suites green. |
| `pass-3.log` | **Pass-after ×3** (serial): 2/2 suites green. |
| `build-final.log` | Closing full-tree incremental build: BUILD_RC=0, 0 FAILED. |
| `full-gate.log` | Closing full serial ctest gate: **201 passed, 0 failed** out of 201 run (the 202nd entry, R14ProbeBatchSkip, "Not Run (Disabled)" — the same disabled-by-design state as the wave-2b/2c closing gates). GATE_RC=0. |

## Test-count deltas (no test weakened)

- TestSevenZipBundle: 7 → 9 test functions. `resolverPrefersAppOwnedBinary`
  kept its name and assertion; its FIXTURE changed from a fake tool pair to
  byte-exact copies of the committed bundle (the resolution contract now
  includes runtime verification — a fake pair must be REFUSED, which the new
  tamper pin asserts). The absence pin gained one extra assertion
  (absence must not be reported as an integrity failure).
- TestHotFolder: 25 → 29 test functions (four F-6 pins). No existing pin
  touched.
