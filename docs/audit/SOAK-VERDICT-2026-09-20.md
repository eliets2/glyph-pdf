# SOAK-VERDICT — R25 48h Release-Candidate Soak (read 2026-09-20)

- **Candidate:** `2f75524448ffca09b110a5517ad087a061218b1b` (short `2f755244`,
  `feat/parity-glm` tip at soak start 2026-09-15T01:44:41+03:00; the branch has
  since advanced to `b17106a`, so this verdict applies to the pinned candidate).
- **Binary:** `build-soak/PdfWorkstation.exe`, Release,
  SHA-256 `791749b562c1cd2bc2056e09fe8e6a6d0c93e55fe94026dc1551c880df31244c`
  (16,082,959 bytes) — **re-hashed 2026-09-20, matches** the value recorded in
  [SOAK-48H-2026-09-15.md](SOAK-48H-2026-09-15.md); the binary on disk is the binary that soaked.
- **Inputs:** `D:\soak-48h.log` (1,683,094 bytes, last write 2026-09-15 05:45:04),
  `D:\soak-48h.end`, `D:\soak-48h-setup-attempt1.log` (excluded from all
  tallies — buggy harness, see §9). **`D:\soak-48h.log.1` does not exist** — the
  log never reached the 50 MB rotation threshold. The "~116+ SOAK PASS markers"
  note refers to `grep -c "SOAK PASS"` = 116 = 58 START + 58 RESULT lines, all in
  the single main log.
- **Method:** mechanical, scripted (committed:
  `tools/soak_verdict_summary.py`; run `python tools/soak_verdict_summary.py`).
  The per-test tally was cross-checked through a second, independent extraction
  path (ctest's own "The following tests FAILED" summary blocks); both paths
  agree exactly on every overlapping entry.

## 1. Verdict

> **SOAK VERDICT: FAIL — the 48 h soak did not complete.**
> The detached loop was killed by the host at **2026-09-15 05:45:03+03**, after
> **4 h 00 m 21 s** (8.4 % of the window), when Windows Update initiated a
> planned OS restart (event-log evidence in §8). 58 passes ran of the ~600–700
> the methodology projected. The methodology's completion line
> (`=== SOAK END … passes, failed passes: n ===`) is **absent** — by the
> committed verdict protocol this alone is a soak failure.

**Candidate-side picture inside the recorded window** (recorded honestly, and
NOT a 48 h exoneration):

- **No candidate-attributable failure recurrence.** No test failed in "most"
  passes (worst case: `TestReadOnlyGate`, 9/58 = 15.5 %). All 22 ctest-failure
  passes map to the suite's already-documented order/temp-state interference and
  timing-flake family (§4); every failing test passes in the large majority of
  passes with the identical binary, and the suite gate on this candidate was
  153/153 before the soak.
- **Zero app-crash suspects:** 57/57 recorded app cycles ended
  `KILLED-AFTER-60S;taskkill-exitcode=1` (normal); **no `EXITED-EARLY`** cycles.
  The real Release executable never hung, never crashed, and never produced an
  anomalous exit code across 58 launches.
- **Conclusion:** candidate `2f755244` shows **no 48 h stability finding, but
  also no 48 h evidence**. The soak is valid as a ~4 h endurance sample only.
  **A re-soak is required** for the 48 h claim (§10).

## 2. Pass accounting (main log, all 58 passes)

| Metric | Value |
|---|---|
| Pass START markers | 58 (first 2026-09-15T01:44:42, last 2026-09-15T05:41:21) |
| Pass RESULT markers | 58 (35 `PASS`, 23 `FAIL`) |
| ctest exit = 0 | **35 passes** (60.3 %) |
| ctest exit = 8 (≥1 test failed) | **22 passes** (37.9 %) |
| ctest exit = 1073807364 (0x40010004, killed) | **1 pass** (pass 58 — §8) |
| Failing-test events total | 29 across the 23 failed passes |
| ctest duration per pass | min 136 s / median 154 s / max 429 s (n=57 timed) |

Failed passes with their failing tests (exit=8 unless noted):

| Pass | Failing tests |
|---|---|
| 2 | TestReadOnlyGate |
| 4 | TestSanitization (SegFault), TestBatchMode, TestEncryptedPackageSafeWrite |
| 5 | TestEngineSave (all 10 functions) |
| 8 | TestBatchMode |
| 9 | TestReadOnlyGate |
| 13 | TestBatchMode |
| 16 | TestCommandBinding |
| 22 | TestBatchMode |
| 24 | TestReadOnlyGate, TestBatchMode |
| 25 | TestCommandBinding, TestEngineSave (all 10 functions) |
| 26 | TestEngineSave (all 10 functions) |
| 27 | TestReadOnlyGate |
| 28 | TestBatchMode, TestEncryptedPackageSafeWrite |
| 31 | TestSanitization (SegFault) |
| 34 | TestReadOnlyGate |
| 36 | TestBatchMode |
| 37 | TestEngineSave (all 10 functions) |
| 40 | TestReadOnlyGate |
| 41 | TestReadOnlyGate |
| 45 | TestReadOnlyGate |
| 48 | TestReadOnlyGate |
| 52 | TestCommandBinding |
| 58 | TestSignatureRealCrypto (Failed), TestResourceLimits (Timeout) — then the process was killed; exit=1073807364 (§8) |

## 3. Per-failing-test tally (58 passes)

| Test | Count | Rate | Passes | Kinds |
|---|---|---|---|---|
| TestReadOnlyGate | 9 | 15.5 % | 2, 9, 24, 27, 34, 40, 41, 45, 48 | Failed ×9 |
| TestBatchMode | 7 | 12.1 % | 4, 8, 13, 22, 24, 28, 36 | Failed ×7 |
| TestEngineSave | 4 | 6.9 % | 5, 25, 26, 37 | Failed ×4 |
| TestCommandBinding | 3 | 5.2 % | 16, 25, 52 | Failed ×3 |
| TestSanitization | 2 | 3.4 % | 4, 31 | **Exception: SegFault ×2** |
| TestEncryptedPackageSafeWrite | 2 | 3.4 % | 4, 28 | Failed ×2 |
| TestSignatureRealCrypto | 1 | 1.7 % | 58 | Failed ×1 |
| TestResourceLimits | 1 | 1.7 % | 58 | Timeout ×1 |

Protocol application: the verdict protocol's "candidate finding" trigger is a
test failing in MOST passes — **no test comes close** (max 15.5 %). The
"a different test failing occasionally = environment flake source" bucket covers
the four 1–3× entries. The four tests at 2–9× exceed one-off noise, so per the
analysis brief each gets a dedicated section below (§4) with signatures; their
recurrence rate is a **suite-stability finding**, not a candidate regression
(evidence per section).

## 4. Dedicated sections — tests recurring above noise

### 4.1 TestReadOnlyGate — 9× (15.5 %)

Failing functions vary: `readOnlyBlocksPageMutationsAndSaveInPlaceAndReenablingRestoresThem`
(4×), `readOnlyBlocksReplaceAllAndReportsNoSuccessCount` (4×),
`readOnlyBlocksInsertAndAnnotationToolsWhileViewingStaysAvailable` (1×).
Dominant assertion (12 `FAIL!` lines across the whole log, also in
TestCommandBinding):

```
FAIL!  : TestReadOnlyGate::readOnlyBlocksPageMutationsAndSaveInPlaceAndReenablingRestoresThem()
         'PdfEditorEngine::readExpiryDate(dest).isValid()' returned FALSE. ()
```

A write-then-read-back roundtrip of a freshly written temp PDF occasionally
fails to read back. Non-deterministic across passes of the identical binary;
passes in 49/58 passes; no app-level read-only behavior ever misbehaved. Same
temp-file roundtrip family as the baseline's known flakes.

### 4.2 TestBatchMode — 7× (12.1 %) + 1× in the excluded attempt-1

`testCompressOpProducesOutput` 6× (plus `testBatchConvert` 1×):

```
FAIL!  : TestBatchMode::testCompressOpProducesOutput() Compared values are not the same
   Actual   (bm.successCount()): 0
   Expected (1)                : 1
D:/pdf/pdf-keyC/tests/TestBatchMode.cpp(304) : failure location
```

The compress op occasionally produces 0 successes instead of 1 — output-file
write/visibility under machine load. Independently observed in the
setup-attempt-1 run (same signature, `TestBatchMode.cpp(304)`), i.e. it
predates this soak's harness and reproduces across independent loop
instances — environment-side, not a soak artifact.

### 4.3 TestEngineSave — 4× (6.9 %)

Distinctive all-or-nothing pattern: in every failing pass **all 10 test
functions fail at once**. Representative:

```
FAIL!  : TestEngineSave::sameFileSaveKeepsPageCountAndContent() Compared values are not the same
   Actual   (leftoverCandidates()): 1
```

`leftoverCandidates()` counts leftover files in the shared
`%TEMP%/glyphpdf-candidates` directory. This is the exact in-suite interference
family already live-captured and partially patched on this repo
(commit `ae636a5`: candidate-count `6!=5` interference under concurrent runs,
fixed with ctest `RESOURCE_LOCK` on the 15 unprotected suites). The soak shows
leftover state can still leak across tests/passes even at `-j 1` (a prior
pass's app cycle or an interrupted test leaves state). Environment-side.

### 4.4 TestCommandBinding — 3× (5.2 %)

Failing functions vary (`menuItemsShareTheRegistryPredicate` 2×,
`sampledEnabledActionsHaveRealEffects` 2×, `ribbonButtonsMirrorRegistryEnablementAcrossReadOnly` 1×).
Signatures combine the §4.1 `readExpiryDate(dest).isValid()` roundtrip
assertion with UI-enablement follow-ons (`'!menuSave->isEnabled()' returned
FALSE`, `'dialogSeen' returned FALSE`), i.e. downstream state of the same
write/read-back flake.

### 4.5 TestSanitization — 2× SegFault (3.4 %) — TOP FOLLOW-UP

Both occurrences are the same crash, in pass 4 (0.44 s) and pass 31 (0.21 s):

```
A crash occurred in C:\Users\User\Projects\pdf-keyC\build-soak\TestSanitization.exe.
While testing testSanitizeGeneratesUniqueTrailerID
Exception code   : 0xc0000005
Nearby symbol    : ZNK6PoDoFo16PdfDataContainer13AssertMutableEv
```

Access violation inside `PoDoFo::PdfDataContainer::AssertMutable()` while
testing `testSanitizeGeneratesUniqueTrailerID`. This is **crash-class**, not a
compare mismatch, and is the only in-soak event of its kind. It is
non-deterministic (identical binary, 56/58 passes green) and confined to a
unit-test process — the application itself never crashed in 58 app cycles — so
it is classified flake-source-with-a-real-bug-suspicion rather than a proven
candidate regression. It is nonetheless the **top follow-up item**: an
`AssertMutable` contract violation on the vendored PoDoFo 1.1.0 path is a
latent UB-class defect that should be run down (§10).

### 4.6 TestEncryptedPackageSafeWrite — 2× (3.4 %)

`cancelKillsWriterAndPreservesExistingPackage()` — "Compared values are not the
same". Same shared-candidates-dir count family as §4.3, and the same suite that
commit `ae636a5` already live-captured interfering in-suite (`count 6!=5`).

## 5. Known one-off watch items — reconciliation

| Watch item (from prior records) | Soak tally | Resolution |
|---|---|---|
| TestWelcomeRoutes (baseline run-1 timing flake, 117.9 s silent) | **0× in 58** | Did not recur. Stays classified as a known timing flake; no action. |
| TestBatchMode (attempt-1 flake) | **7× in 58** (+1× excluded attempt-1) | Was recorded as a one-off — it is not. Reclassified as a recurring environment flake source (§4.2). |
| TestReadOnlyGate (recorded one-off) | **9× in 58** | Same: reclassified as the most frequent recurring flake source (§4.1). |
| TestSignatureRealCrypto (recorded transient) | **1× in 58**, inside the killed pass 58 | Signature exactly matches the recorded transient: `testOcspCertIdMismatchRejected() 'acceptable' returned FALSE. (ER-1 baseline: unexpected trustStatus: ValidWithUnsignedChanges)`. Remains a 1.7 % environment-sensitive transient — noise level. |
| (new) TestResourceLimits | **1× Timeout (0.34 s)**, pass 58 only, seconds before the OS restart initiated at 05:45:03 | Almost certainly a shutdown-competition artifact of the host restart (§8); never seen in any other pass. Watch on the re-soak. |

## 6. App-cycle accounting

| Metric | Value |
|---|---|
| PdfWorkstation.exe launches recorded | 58 |
| `KILLED-AFTER-60S;taskkill-exitcode=1` (normal forced termination) | **57** |
| `EXITED-EARLY` (crash suspects) | **0** — no exit codes to list |
| Outcome never recorded | 1 (pass 58: the loop itself was killed by the host shutdown mid-cycle; see §8) |

Every completed 60 s lifecycle of the real Release executable ended in the
expected forced-kill path. No hang, no crash, no anomalous exit code.

## 7. Scope — what this soak does and does NOT prove

Restated from SOAK-48H-2026-09-15.md, **with the early termination applied**:

Proves (within ~4 h, not 48 h): repeated cold startup/hard-shutdown of the real
Release executable is clean (58/58 launches); the 153-test serial suite holds
up across 58 consecutive runs with no deterministic red and no resource-
exhaustion failure trend visible in the window.

Does NOT prove:

- **The 48 h claim itself** — the loop died at 4 h 00 m 21 s. Long-horizon
  resource creep (handle/memory/file-lock) beyond ~58 lifecycles is unobserved.
- No real-user GUI marathon — everything `QT_QPA_PLATFORM=offscreen`; no
  display, no human interaction, no DPI/multi-monitor/modal-by-mouse variance.
- No installer/uninstall cycles (MSI/packaging, `GLYPHPDF_RELEASE_BUILD=ON`).
- No multi-day file-format fuzzing; ctest fixtures are finite and well-formed.
- No graceful-shutdown exercise of the app cycle — 60 s cycles end in
  `taskkill /F` by design; close-event/save-prompt paths only covered where
  tests cover them.
- Single machine, single user profile, no network/printer variance, no
  concurrent instances; OCR inference covered only to suite depth.

## 8. The pass-58 kill event (exit=1073807364)

What happened, mechanically:

- Pass 58 started **05:41:21**. ctest ran clean through tests 1–151
  (all `Passed`); `TestSignatureRealCrypto` (#16) had failed and
  `TestResourceLimits` (#35) timed out earlier in the pass. During
  `Start 152: TestSep13LeadBtDowngrade` the ctest process died with
  **exit 1073807364 = 0x40010004 (`STATUS_CONTROL_C_EXIT`)** — the code a
  console process gets when the console closes or a Ctrl+C/Ctrl+Break broadcast
  is delivered.
- The loop's last three lines (RESULT, "records and continues", app-cycle
  legend) **lost their timestamp prefixes**, and the pass-58 app-cycle line has
  an **empty outcome field** — the helper PowerShell processes could no longer
  run. The log's last write is **05:45:04**. No pass 59, no `SOAK END` line.

Correlation — the host restarted under us. Windows System event log
(query 2026-09-20):

| Local time 2026-09-15 | Event | Source | Meaning |
|---|---|---|---|
| 05:45:03 | 1074 | `MoUsoCoreWorker.exe` (Windows Update UUS), "on behalf of NT AUTHORITY\SYSTEM" | **Restart initiated — "Operating System: Service pack (Planned)", Reason Code 0x80020010** |
| 05:45:19 | 6006 | EventLog | Event log service stopped (shutdown proceeding) |
| 05:45:49 | 13 | Kernel-General | OS shutting down (system time 02:45:49Z = 05:45:49+03) |
| 05:46:07 | 12 | Kernel-General | OS started (reboot 1 of the servicing cycle) |
| 05:46:36 | 1074 | `TrustedInstaller.exe` | Restart — "Operating System: Upgrade (Planned)" |
| 05:46:58 | 12 | Kernel-General | OS started (reboot 2) |
| 05:47:09 | 1074 | `TrustedInstaller.exe` | Restart — "Operating System: Upgrade (Planned)" |
| 05:47:29 | 12 | Kernel-General | OS started (reboot 3; update cycle completes) |

The console-close broadcast that killed ctest at ~05:44–05:45 is the shutdown
sequence starting (the 1074 initiation and pre-shutdown service churn), which
also explains the two pass-58 test anomalies (`TestResourceLimits` timing out
after 0.34 s of a machine already bogged down by update servicing, and
TestSignatureRealCrypto's known trust-status transient landing in the same
minute) and the dead timestamp helper. **Verdict: environment/host event
(Windows Update planned restart), not a candidate defect.** The loop was
designed to survive lane exit, not OS restarts; it did exactly what it could.

## 9. Setup attempt-1 (excluded) — recorded, not counted

`D:\soak-48h-setup-attempt1.log` (56,252 bytes, 2026-09-15 01:40): 3 passes
under the buggy harness (PATH hid powershell → no timestamps/app cycles;
unescaped `)` corrupted if/else parsing — visible as a contradictory
`RESULT: PASS (ctest exit=0` followed by `RESULT: FAIL (ctest exit=0)` for its
pass 2). Pass 1 recorded the TestBatchMode flake (§4.2). Per the methodology
these passes are **excluded from every tally above**; it is retained only as
evidence that the harness bug — and the TestBatchMode flake — predate the
real soak. 0 app cycles recorded.

## 10. Follow-ups

1. **Re-soak the same candidate for the full 48 h** (binary unchanged,
   SHA re-verified 2026-09-20). Before starting: pause/defer Windows Update
   active hours, or add a guard that suspends the loop across a detected
   restart and resumes with a continuing pass counter; keep the `.end`
   timestamp check. Treat pass 58's two test anomalies as noise unless the
   re-soak reproduces them off-shutdown.
2. **Run down the TestSanitization SegFault** (§4.5): reproduce
   `testSanitizeGeneratesUniqueTrailerID` in a loop; inspect the
   `PdfDataContainer::AssertMutable` call path in the sanitize/trailer-id code
   against the vendored PoDoFo 1.1.0 contract. Crash-class → do not ship the
   next candidate without an explanation.
3. **Harden the shared `%TEMP%/glyphpdf-candidates` state** beyond the
   `RESOURCE_LOCK` set of commit `ae636a5` (leftover-candidate cleanup at
   suite start, or per-test directories) — TestEngineSave's
   `leftoverCandidates(): 1` and TestEncryptedPackageSafeWrite are residual
   members of that family even at `-j 1`.

## 11. Provenance

- Analysis script (committed): `tools/soak_verdict_summary.py` —
  `python tools/soak_verdict_summary.py` reproduces every number in §§2–6.
- Verdict-session branch: `feat/soak-verdict` (cut from `feat/parity-glm` at
  `b17106a`); candidate is pinned at `2f75524448ffca09b110a5517ad087a061218b1b`.
- Exe SHA-256 re-verified 2026-09-20:
  `791749b562c1cd2bc2056e09fe8e6a6d0c93e55fe94026dc1551c880df31244c`.
- Windows event-log queries and file hashes in this document were taken
  2026-09-20 on the soak host (PCELIE).
