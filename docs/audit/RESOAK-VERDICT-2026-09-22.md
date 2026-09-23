# RESOAK-VERDICT-2026-09-22 — R25b Reboot-Resilient 48 h Re-Soak (read 2026-09-22)

- **Candidate:** `b17106a3982fb79c5ddb67000151d4d407a5e9bf` (short `b17106a`,
  `feat/parity-glm` tip at re-soak start; the branch has since advanced, so this
  verdict applies to the pinned candidate). Setup/protocol:
  [RESOAK-2026-09-20.md](RESOAK-2026-09-20.md); precedent:
  [SOAK-VERDICT-2026-09-20.md](SOAK-VERDICT-2026-09-20.md).
- **Binary:** `build-soak/PdfWorkstation.exe`, Release,
  SHA-256 `509da2c8fc602411c6c1cfe45f7603ca1ede42f4d77af97058330b985f867853`
  (16,666,639 bytes) — **re-hashed 2026-09-22 after SOAK END, matches** both
  the log header and `D:\resoak-48h.exe.sha`; the binary on disk is the binary
  that soaked.
- **Inputs:** `D:\resoak-48h.log` (23,175,840 bytes, last write 2026-09-22
  20:56:23). **`D:\resoak-48h.log.1` does not exist** — the log never reached
  the 50 MB rotation threshold. Markers: `D:\resoak-48h.end`
  (`2026-09-22T20:49:07`, written once at first start, never reset),
  `D:\resoak-48h.done` (`2026-09-22T20:56:23`), `D:\resoak-48h.state`
  (`FIRSTSTART=2026-09-20T20:49:07`, `PASS=720`).
- **Method:** mechanical, scripted — `tools/soak_verdict_summary.py --resoak`
  (this lane's extension of the committed first-soak parser; same marker
  grammar, plus restart markers, crash blocks, per-pass durations, time
  buckets, and family classification). Failing-test extraction counts only
  pre-RESULT ctest lines (the bat re-echoes failing lines after RESULT; the
  echo hits 266 lines and are excluded). Integrity cross-checks, all passing:
  every one of the 242 FAIL passes has ≥ 1 pre-RESULT failing-test event
  (212 passes ×1, 28 ×2, 2 ×3 = 274 events); all FAIL exits are exactly 8;
  pass numbers are contiguous 1–720 with no duplicates.

## 1. Verdict

> **RESOAK VERDICT: PASS — the 48 h endurance claim is established for
> candidate `b17106a`.**
> The soak ran the full window: first start **2026-09-20T20:49:07+03** →
> `=== SOAK END after 720 passes total, failed passes this run: 242 ===` at
> **2026-09-22T20:56:23+03** (48.12 h first-to-last timestamp), ~7 min past
> the pinned end-by because the 720th pass (started 20:48:03) was allowed to
> finish — the loop records, it does not stop. 719 passes recorded results:
> **477 PASS (66.3 %) / 242 FAIL (33.7 %)**, every FAIL a clean ctest
> `exit=8` (no killed, no exotic exit, zero test timeouts in 720 passes).
> **No candidate-attributable recurrence:** no test failed in ≥ 30 % of
> passes with an unchanged signature (max: `TestLaneScheduler`, 21.8 % —
> below its own ~1/3 standalone pre-declaration, signature unchanged, so the
> RESOAK §2 escalation triggers never fired). **Zero app-crash suspects**
> (719/719 app cycles `KILLED-AFTER-60S`, none `EXITED-EARLY`).

Two honesty conditions ride on the PASS, both pre-declared:

1. **No real OS restart occurred in-window.** The reboot-resilience
   machinery (heartbeat + logon task + relaunch guard + continuing pass
   numbers) fired exactly once — the day-one controlled kill drill
   (RESTART DETECTED at 2026-09-20T21:04:24, resuming at pass 2, numbering
   continued cleanly at pass 3). A timestamp gap scan over all 2,402
   timestamped log lines found **zero gaps > 20 min** (a healthy pass is
   ≤ ~17 min), i.e. no hidden downtime and no unmarked restart: the loop
   simply never went down again. Windows Update — which killed the first
   soak at 4 h — did not fire this time. The 48 h window therefore completed
   by continuous execution, and reboot survival remains **proven by the
   drill plus design, not by an in-window real reboot** (RESOAK §5 residual,
   unchanged).
2. **The crash-class follow-up (first-soak §10.2) is still open and now has
   evidence on this candidate too:** `TestSanitization` SegFaulted 8×
   (§4.5). Crash-class beats the low rate: it is the top follow-up item,
   not a PASS blocker (unit-test process only; the application itself never
   crashed in 719 launches).

## 2. Pass accounting (all 720 passes)

| Metric | Value |
|---|---|
| Pass START markers | 720 (unique, contiguous 1–720, in order) |
| Pass RESULT markers | 719 (477 `PASS`, 242 `FAIL`) |
| RESULT-less passes | 1 — pass 2, the documented §1 drill abort (START 20:58:31, killed by the drill; RESOAK §1/§5) |
| ctest exit = 0 | **477 passes** (66.3 % of 719 with results) |
| ctest exit = 8 (≥1 test failed) | **242 passes** (33.7 %) |
| Other exit codes / timeouts | none (no `1073807364`-class event ever; zero `***Timeout` kinds in 720 passes) |
| Failing-test events | 274 across the 242 failed passes (212×1 + 28×2 + 2×3) |
| ctest duration per pass | min 155 s / median 159 s / p90 165 s / max 980 s (n=719) |
| First pass | START 2026-09-20T20:49:09 |
| Last pass | 720: START 2026-09-22T20:48:03 → RESULT PASS 20:55:10 → app cycle KILLED-AFTER-60S 20:56:12 |
| SOAK END | 2026-09-22T20:56:23 (`D:\resoak-48h.done` written) |

Time-bucketed FAIL rates (24 h buckets from first start) — the load signal
is visible but no test approaches the finding threshold in either half:

| Window | Passes | FAIL rate | TestLaneScheduler | TestReadOnlyGate | TestCommandBinding |
|---|---|---|---|---|---|
| h 0–24 | 345 (1–346) | 138 = 40.0 % | 95 = 27.5 % | 20 = 5.8 % | 13 = 3.8 % |
| h 24–48 | 374 (347–720) | 104 = 27.8 % | 62 = 16.6 % | 29 = 7.8 % | 12 = 3.2 % |

## 3. Per-failing-test tally (719 result passes; 242 FAIL passes)

| Test | Count | Rate (719) | Rate (242 FAIL) | Kinds | Classification |
|---|---|---|---|---|---|
| TestLaneScheduler | 157 | 21.8 % | 64.9 % | Failed ×157 | **Pre-declared load-era timing guard** (RESOAK §2) — §4.1 |
| TestReadOnlyGate | 49 | 6.8 % | 20.2 % | Failed ×49 | Known readExpiryDate roundtrip family (verdict §4.1) — §4.2 |
| TestCommandBinding | 25 | 3.5 % | 10.3 % | Failed ×25 | Same roundtrip family + UI follow-ons (verdict §4.4) — §4.2 |
| TestBatchMode | 17 | 2.4 % | 7.0 % | Failed ×17 | FU-2 shared-tempdir / output-visibility family (verdict §4.2) — §4.4 |
| TestRedactionProof | 10 | 1.4 % | 4.1 % | Failed ×10 | **NEW vs first-soak families** — §4.6 |
| TestSanitization | 9 | 1.3 % | 3.7 % | **SegFault ×8**, Failed ×1 | **Crash-class recurrence** (verdict §4.5 / §10.2) — §4.5; the 1 Failed (pass 55) is the FU-2 compare family |
| TestSignatureRealCrypto | 5 | 0.7 % | 2.1 % | Failed ×5 | Known 1.7 % OCSP trust-status transient (verdict §5) — noise |
| TestOllamaProvider | 1 | 0.1 % | 0.4 % | Failed ×1 | Async result-visibility one-off under load — one-off bucket |
| TestEncryptedPackageSafeWrite | 1 | 0.1 % | 0.4 % | Failed ×1 | FU-2 shared-package-count family (verdict §4.6) — one-off bucket |

Protocol application (same rule as the first soak, threshold 30 % of passes,
same test + same signature): **no test comes close to 30 % of result passes**
(max 21.8 %, and that one is the RESOAK §2 pre-declaration with an unchanged
signature and an in-soak rate *below* its ~1/3 standalone rate). All 274
failing-test events map to the classified families above — zero unexplained
failing events. The 33.7 % FAIL-pass rate is therefore dominated by the
pre-declared timing guard (present in 157 of the 242 FAIL passes) plus the
known flake families; it is a **suite-stability/load-sensitivity picture**,
not a candidate regression.

## 4. Dedicated sections

### 4.1 TestLaneScheduler — 157× (21.8 %) — pre-declared, not a finding

Single function, single signature, all 157 events:

```
FAIL!  : TestLaneScheduler::testCrossPagePipelining() 'elapsed < 10 * 100' returned FALSE. (Cross-page pipeline took NNNNms ...
```

Observed over-budget spread: min 1000 ms / median 1022 ms / max 1090 ms
against the 1000 ms budget — the same ~0–9 % overshoot class as the
pre-soak standalone observations (1004–1014 ms). RESOAK-2026-09-20 §2
pre-declared exactly this: *"machine-load sensitivity of a marginal
assertion … not a soak finding — unless it degrades from ~1/3 toward 'most
passes', or starts failing with a different signature."* Neither trigger
fired: 21.8 % overall (27.5 % in the loaded first half, 16.6 % in the
quieter second half) never approached "most passes", and the signature never
varied. It is recorded as the suite's dominant **machine-load-sensitive
marginal assertion**, and the ~1 % budget margin itself remains a legitimate
test-hardening item (follow-up §9.4).

### 4.2 TestReadOnlyGate 49× + TestCommandBinding 25× — known roundtrip family

Same `readExpiryDate(dest).isValid()` temp-file write/read-back flake as
verdict §4.1/§4.4, with the usual UI-enablement follow-ons
(`'!menuSave->isEnabled()'`, `'dialogSeen'`, `'isReadOnly()'`) and varying
functions (`readOnlyBlocksPageMutations…`, `readOnlyBlocksReplaceAll…`,
`menuItemsShareTheRegistryPredicate`, `ribbonButtonsMirror…`,
`sampledEnabledActionsHaveRealEffects`). Rates *improved* vs the first soak:
TestReadOnlyGate 15.5 % → 6.8 %, TestCommandBinding 5.2 % → 3.5 %. Same
binary, passes in ~93 %/96 % of passes — environment-side, unchanged
classification.

### 4.3 TestBatchMode 17× — known, improved

`testCompressOpProducesOutput` / `testBatchConvert` /
`testBatchWithOneBadFile`, all "Compared values are not the same" — the
verdict §4.2 output-write/visibility-under-load family (first seen as
`successCount(): 0` vs 1). Rate improved 12.1 % → 2.4 %; a third function of
the same suite joined the family. Environment-side.

### 4.4 FU-2 shared-tempdir family — residual, low rate

The RESOURCE_LOCK set (commit `ae636a5`) was known partial at this candidate
(RESOAK brief; TestSanitization's clean-save and shared-candidates
interactions called out as candidates). Re-soak evidence: TestBatchMode 17×
(§4.3), TestEncryptedPackageSafeWrite 1× (`toolFailurePreservesExistingPackage`,
compare mismatch — same suite family as verdict §4.6), and the single
non-crash TestSanitization event (pass 55,
`'engine.sanitizeDocument(outPdf2)' returned FALSE`). Notably TestEngineSave's
all-10-functions `leftoverCandidates(): 1` family from the first soak did
**not** recur at all (0×/719). Total family pressure dropped sharply;
the shared-`%TEMP%/glyphpdf-candidates` hardening (verdict §10.3) stays on
the list at lower priority.

### 4.5 TestSanitization — 8× SegFault — TOP FOLLOW-UP (crash-class recurrence)

The first soak's §4.5/§10.2 crash **recurred at `b17106a`** — 8 times in 719
passes (1.1 %, vs 2×/58 = 3.4 % at `2f755244`), always the same test
function, always `0xc0000005`:

```
A crash occurred in C:\Users\User\Projects\pdf-keyC\build-soak\TestSanitization.exe.
While testing testSanitizeGeneratesUniqueTrailerID
Exception code   : 0xc0000005
Nearby symbol    : ZNK6PoDoFo16PdfDataContainer13AssertMutableEv   (7×)
Nearby symbol    : ZNK6PoDoFo7PdfName10GetRawDataEv                (1×, NEW symbol)
```

The second nearby symbol (`PdfName::GetRawData`, pass 14) is new evidence:
the UB-class defect in the vendored PoDoFo 1.1.0 sanitize/trailer-id path
has now been observed under two distinct call-site symbols. Passes:
14, 25, 102, 251, 335, 370, 445, 680. The pre-soak ×3 standalone check at
this tip was 19/19 clean three times, and the crash is non-deterministic
in-suite (711/719 green) — same classification as before:
**flake-source-with-a-real-bug-suspicion, crash-class, top follow-up**;
the §10.2 rule stands — *do not ship the next candidate without an
explanation* (the separate pdf-sec fix lane owns it).

### 4.6 TestRedactionProof — 10× — NEW vs first-soak families

Not in the first soak's tally (0×/58 at `2f755244`; the test itself existed —
added in `15f3f1cc` — but never failed there), absent from the baseline and
pre-soak gates. Single function, single signature, all 10 events:

```
FAIL!  : TestRedactionProof::proofFailsOnXmpSurvivor() '!proof.proofPassed' returned FALSE. (a redacted string planted in XMP must FAIL the proof)
D:/pdf/pdf-keyC/tests/TestRedactionProof.cpp(514) : failure location
```

The test plants a survivor string into the redacted file's XMP and requires
the machine-verifiable proof to FAIL; in these passes the proof wrongly
PASSED. Non-deterministic (709/719 green), spread across the whole window
(passes 30, 33, 135, 154, 206, 389, 442, 482, 609, 624 — not concentrated in
the loaded first half), single function while sibling proof functions pass.
Mechanically this is a **new recurrence at 1.4 % — far under the 30 %
finding threshold, hence not candidate-attributable per protocol** — but it
is a security-adjacent assertion and unexplained: in-suite order/state
interference is suspected, and it needs a dedicated repro lane (standalone
loop of the one test, then ordered pairs) before it can be called
environment-side with the same confidence as §4.2–§4.4.

### 4.7 One-offs

| Event | Pass | Resolution |
|---|---|---|
| TestOllamaProvider::endpointPolicyHttps(https:default-allowlist:localhost) `resultCount(): 0` vs 1 | 7 | Async dispatch result-visibility one-off under the day-one load; 1×/719; noise bucket, watch. |
| TestEncryptedPackageSafeWrite | 68 | §4.4 family residual. |
| TestSignatureRealCrypto `testOcspCertIdMismatchRejected` trust-status transient | 3, 40, 192, 230, 275 | Same signature as the recorded transient (verdict §5); 0.7 % vs 1.7 % first soak — noise level. |
| TestWelcomeRoutes / TestResourceLimits | — | **0× in 719** — first soak's one-offs did not recur (TestResourceLimits was the shutdown-competition artifact; no shutdown occurred). |

## 5. App-cycle accounting

| Metric | Value |
|---|---|
| PdfWorkstation.exe launches recorded | 719 (every pass with a RESULT; pass 2 was drill-aborted before its cycle) |
| `KILLED-AFTER-60S;taskkill-exitcode=1` (normal forced termination) | **719** |
| `EXITED-EARLY` (crash suspects) | **0** |
| Outcome never recorded | 0 |

Every completed 60 s lifecycle of the real Release executable ended in the
expected forced-kill path. No hang, no crash, no anomalous exit code across
719 launches — consistent with the first soak's 57/57.

## 6. Restart / resume accounting

| Metric | Value |
|---|---|
| `RESTART DETECTED` markers | 1 — 2026-09-20T21:04:24, resuming at pass 2, first start and end-by intact |
| Numbering continuity | marker at pass 2 → next START = pass 3 (clean); passes contiguous 1–720, no duplicates, no reordering |
| Timestamp gaps > 20 min (hidden downtime) | **0** across all 2,402 timestamped lines (healthy pass ≤ ~17 min) |
| Real OS reboots in-window | **0** — the single marker is the §1 drill; Windows Update never fired |

The reboot-resilience proof therefore stands exactly as RESOAK §1 stated it:
the resume path executed live (state read, end-by preserved, pass numbers
continued, append-only log intact) under a controlled kill; what remains
untested is only the OS's own logon → task invocation after a *real* restart
(§7).

## 7. Load-context disclosure (window overlap)

The brief for this verdict states the 48 h window overlapped a **heavy
parallel agent/build period** on this host (5+ concurrent build lanes;
multiple D:-to-100 % disk events). That context is disclosed and is visible
mechanically in the log: the 15 slowest passes are dominated by the first
day (passes 1–24 at 327–980 s vs a 159 s median; worst p8 = 980 s, p7 = 964 s
during the first hours), and day-1 FAIL/TestLaneScheduler rates run ~1.5–1.7×
day-2 rates (§2 table). Two consequences for reading this verdict:

- Part of the 33.7 % FAIL-pass rate is load-era elevated; the quieter second
  half still fails at 27.8 % — dominated by the pre-declared marginal timing
  assertion (16.6 %) — so the flake families are persistent
  machine-sensitivity, not purely a load artifact.
- No classification in §3 changes under load adjustment: every family is
  either pre-declared, known-from-first-soak, or far under the finding
  threshold in BOTH halves.

## 8. Scope — what this re-soak does and does NOT prove

Carried over unchanged from SOAK-48H-2026-09-15 / SOAK-VERDICT §7: offscreen
only (no display, no human interaction, no DPI/modal-by-mouse variance); no
installer/uninstall cycles; no multi-day file-format fuzzing; app cycles end
in `taskkill /F` by design (no graceful-shutdown marathon); single machine,
single user profile, no network/printer variance, no concurrent app
instances; OCR covered only to suite depth.

Specific to THIS re-soak:

- **A real OS restart never happened in-window.** The 48 h claim is
  established as *48 h of continuous execution*; survival-of-a-real-reboot
  remains drill-proven + by-design (logon task + guard + resume semantics),
  not demonstrated end-to-end in this window. Windows Update active hours
  were never reconfigured (needs elevation) — the design survives a restart
  rather than preventing one.
- The candidate moved six waves after the first soak; first-soak conclusions
  transfer only where this document re-establishes them — which §3/§4 do per
  family (4 improved/vanished, the crash-class persisted, 1 new family
  appeared).
- Long-horizon resource creep is bounded by what ctest+app-cycle lifecycles
  observe: 720 suite runs + 719 app launches with zero degradation trend
  (median pass duration 159 s at start and at end of window; no timeout
  drift; app cycles uniformly clean).

## 9. Follow-ups (priority order)

1. **Run down the TestSanitization SegFault (crash-class, now 10 total
   occurrences across two candidates, two PoDoFo symbols)** —
   `testSanitizeGeneratesUniqueTrailerID`, 0xc0000005 in
   `PdfDataContainer::AssertMutable` (7×) and `PdfName::GetRawData` (1×);
   reproduce in a loop against the vendored PoDoFo 1.1.0
   sanitize/trailer-id path. pdf-sec lane; blocks the next candidate ship
   without an explanation (unchanged from verdict §10.2).
2. **Repro TestRedactionProof::proofFailsOnXmpSurvivor** (new, §4.6):
   standalone loop, then ordered-pair runs to isolate in-suite interference;
   security-adjacent assertion must not stay unexplained even at 1.4 %.
3. **Complete the reboot chain with a real restart** when one happens
   naturally (or a supervised one): confirm the `GlyphPDFResoak` logon task
   fires unattended and the guard resumes — the only untested link.
4. **Widen `TestLaneScheduler::testCrossPagePipelining`'s budget or make it
   load-aware** (currently ~1 % margin, 1000 ms, fails 17–27 % under this
   host's load, 21.8 % overall) — or move it out of the serial soak gate;
   it alone explains ~2/3 of failed passes.
5. **FU-2 tempdir hardening** (verdict §10.3, unchanged, lower rate now):
   per-test directories or suite-start cleanup for the shared
   `%TEMP%/glyphpdf-candidates` surface; TestBatchMode 17× is the residual
   member.

## 10. Provenance

- Analysis script (committed, this branch): `tools/soak_verdict_summary.py`
  — `python tools/soak_verdict_summary.py --resoak` reproduces every number
  in §§2–7 (first-soak mode without the flag is behavior-unchanged; the
  `/153` suite-size hardcode was generalized).
- Verdict-session branch: `feat/resoak-verdict` (cut from `feat/parity-glm`
  tip; protocol docs carried forward from `feat/soak-48h-resume`/`feat/soak-verdict`,
  one commit, docs+tools only).
- Candidate pinned `b17106a3982fb79c5ddb67000151d4d407a5e9bf`; exe SHA-256
  `509da2c8fc602411c6c1cfe45f7603ca1ede42f4d77af97058330b985f867853`
  re-verified 2026-09-22 post-SOAK-END against `build-soak/PdfWorkstation.exe`,
  the log header, and `D:\resoak-48h.exe.sha`.
- Raw evidence untouched: `D:\resoak-48h.log`, `.state`, `.end`, `.done`,
  `.heartbeat`, `.exe.sha` (read-only in this lane).
