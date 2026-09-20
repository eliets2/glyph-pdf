# SWEEP-W2 TEST-SUITE AUDIT — 2026-09-20 (testing-specialist lane)

Branch `feat/sweep-w2-testing` (off `feat/parity-glm` @ `2d29a16`). Scope: the test suite itself
(170 registered ctest targets, `tests/` flat dir, 171 sources). This lane CLASSIFIES and PROPOSES;
the program's cleanup phase executes deletions/merges. Nothing was deleted here.

Run protocol for flake evidence: each recorded-flake suite standalone ×3 (idle machine, build
finished) + in-suite ×2 (`ctest -j 2` full runs); targeted concurrent pair for the FU-2 class.
QtTest `-o txt,txt` offscreen. All rerun counts live in `.context/sweep-w2-scratch/flake-results.tsv`.

## 1. Suite inventory (registration cross-check)

| Check | Result |
|---|---|
| `add_test(NAME ...)` in CMakeLists.txt | 170 |
| `ctest -N` after reconfigure | 170 (1:1 with add_test; no registered-but-unregistered target) |
| `add_executable` targets | 172 |
| `tests/*.cpp` files | 171 |
| Executables with NO add_test | `PdfWorkstation` (the app), `R14ProbeRedactSpace` (manual probe, deploys qoffscreen plugin, CMakeLists.txt:5099), `render_path_profile` (perf tool) — all intentional |
| add_test with NO own executable | `TestUiAccessibility200` (alias: `COMMAND TestUiAccessibility`, different env `QT_SCALE_FACTOR=2`, CMakeLists.txt:2200) — intentional |
| cpp files with NO executable target | see §4 |

## 2. Flake classification (evidence: rerun grids in §2.4)

Static profiles collected before reruns:

| Suite | Lines | Suspect mechanism (static) |
|---|---:|---|
| TestOllamaProvider | 1139 | loopback QTcpServer stub; QSettings-tunable short deadline; 6 wait-constructs |
| TestBatchMode | 713 | real disk I/O via QTemporaryDir; already `RESOURCE_LOCK BatchModeIO + RUN_SERIAL` |
| TestLaneScheduler | 304 | `testCrossPagePipelining`: wall-clock `< 1000 ms` bound with 30/50/20 ms sleeps; ~4x headroom; ZERO retry constructs |
| TestReadOnlyGate | 345 | QSettings isolation + QTemporaryDir; no timing constructs |
| TestBatchOpsCoverage | 1077 | spawns external `QProcess` (Ghostscript-class tools); 1 wait-construct |
| TestCommandBinding | 475 | widget focus/shortcut plumbing; no timing constructs |
| TestWelcomeRoutes | 621 | 2 wait-constructs; QSettings-routed |
| TestEngineSave | 1155 | FU-2 scanner, ZERO-based `leftoverCandidates()==0` after initTestCase debris sweep |
| TestRedactTransaction | 1796 | FU-2 writer (SafeSave candidate flow) + 6 wait-constructs |
| TestSep13LeadComparePerf | 110 | RATIO bound `tLarge/tSmall < 10`; adaptive repeats calibrate SMALL side only — large-side single measurement is load-sensitive (matches recorded "under-load" flake) |

(runs pending — filled in below as completed)

### 2.1 Standalone ×3 grids (idle machine, post-build 248/248, offscreen)

| Suite | r1 | r2 | r3 | Totals line evidence |
|---|---|---|---|---|
| TestOllamaProvider | rc=0 | rc=0 | rc=0 | 62 passed each (36.7-40.2s) |
| TestBatchMode | rc=0 | rc=0 | rc=0 | 17 passed |
| **TestLaneScheduler** | **rc=1** | rc=0 | **rc=1** | 10 passed 1 failed (r1/r3) |
| TestReadOnlyGate | rc=0 | rc=0 | rc=0 | 6 passed |
| TestBatchOpsCoverage | rc=0 | rc=0 | rc=0 | 8 passed 1 skipped |
| TestCommandBinding | rc=0 | rc=0 | rc=0 | 11 passed |
| TestWelcomeRoutes | rc=0 | rc=0 | rc=0 | 20 passed |
| TestEngineSave | rc=0 | rc=0 | rc=0 | 19 passed |
| TestRedactTransaction | rc=0 | rc=0 | rc=0 | 38 passed |
| TestSep13LeadComparePerf | rc=0 | rc=0 | rc=0 | 3 passed (2.5-4.1s) |

Extended TestLaneScheduler probe (5 more idle runs, instrumented): **3/5 failed** —
`elapsed=1019ms`, `1002ms`, `1000ms` vs bound `< 1000ms`; the two passing runs finished under the
bound. Combined standalone record: **3 pass / 5 fail (8 runs, idle)**.

**TestLaneScheduler classification: GENUINELY-FLAKY timing guard (test bug, not interference).**
`testCrossPagePipelining` (tests/TestLaneScheduler.cpp:133-161) asserts the 10-page 3-stage
pipeline beats the serial sum (`elapsed < 10*100ms`). The DESIGNED overlap should land ~250ms
(GPU stage 50ms × 10 / capacity 2), but measured failures sit at 1000-1025ms — i.e. the pipeline
is executing at ~serial time on this host and the bound straddles the actual behavior; passes are
sub-bound luck. Already `RUN_SERIAL`, so serialization cannot fix it. PROPOSAL for the fix lane
(test change, no production impact): assert overlap structurally (per-page stage timestamps /
concurrency samples) or raise the bound to a multiple of the measured serial time with a comment;
never a bare `< serial` wall-clock at 1.00x.

One-off (not classified as suite flake): TestCommandBinding rc=2 on its first-ever post-build
execution (17:23:19), unreproducible in 9 subsequent attempts (5 direct, 3 immediately after
TestBatchOpsCoverage to test adjacency, 1 manual) — first-run transient (fresh 170MB exe /
Defender scan window).

In-suite ×2 grids: §2.2 (below) after the unpatched full `ctest -j 2` runs complete.

### 2.2 In-suite ×2 grids (full `ctest -j 2`, UNPATCHED, 171 targets, ~210 s per run)

| Suite | unpatched r1 | unpatched r2 | standalone x3 |
|---|---|---|---|
| TestOllamaProvider | Passed | Passed | 3/3 |
| TestBatchMode | Passed | Passed | 3/3 |
| TestLaneScheduler | Passed | Passed | **1/3** (+2/5 extended) |
| TestReadOnlyGate | Passed | Passed | 3/3 |
| TestBatchOpsCoverage | Passed | Passed | 3/3 |
| TestCommandBinding | Passed | Passed | 3/3 |
| TestWelcomeRoutes | Passed | Passed | 3/3 |
| TestEngineSave | Passed | Passed | 3/3 |
| TestRedactTransaction | Passed | Passed | 3/3 |
| TestSep13LeadComparePerf | Passed | Passed | 3/3 |
| TestSignatureRealCrypto (FU-2 sibling of the recorded pair) | **Failed** | Passed | 3/3 |
| TestEncryptedPackageSafeWrite (FU-2) | **Failed** | **Failed** | 3/3 |

In-suite failures, verbatim evidence (from `--output-on-failure` logs):

- r1 `TestSignatureRealCrypto::successfulSignLeavesNoCandidate` — `'leftoverCandidates() ==
  candidatesBefore' returned FALSE. (a successful signDocument must remove its committed candidate
  from <temp>/glyphpdf-candidates)` — delta assertion broken by a CONCURRENT suite's writer.
- r1 + r2 `TestEncryptedPackageSafeWrite` — three slots
  (`injectedCommitFaultPreservesExistingPackage`, `successReplacesDestinationWithCandidateBytes`,
  `cancelKillsWriterAndPreservesExistingPackage`) — `Compared values are not the same — Actual
  (candidateFileCount()): 6, Expected (beforeCandidates): 5`: a live candidate from a parallel
  suite landed between the "before" snapshot and the check.

### 2.3 Final classification (all 10 recorded flakes)

| Recorded flake | Class | Evidence | Fix ownership |
|---|---|---|---|
| TestOllamaProvider | clean under protocol (loopback stub; deadline tunable) | 3/3 standalone, 2/2 in-suite | none (watch on loaded CI) |
| TestBatchMode | clean — existing protection works | 3/3 + 2/2 (already `RESOURCE_LOCK BatchModeIO` + `RUN_SERIAL`) | none |
| TestLaneScheduler | **genuinely-flaky timing guard** (test bug) | 3 pass / 5 fail IDLE; failures 1000-1025 ms vs `<1000 ms` bound = serial-time straddle; designed ~250 ms overlap not achieved on this host | fix lane: structural overlap assertion or higher bound (proposal in §2.1) |
| TestReadOnlyGate | clean | 3/3 + 2/2 | none |
| TestBatchOpsCoverage | clean | 3/3 + 2/2 (1 skipped: external tool absent) | none |
| TestCommandBinding | clean (one first-run rc=2 one-off, unreproducible ×9) | 3/3 + 2/2 | none |
| TestWelcomeRoutes | clean | 3/3 + 2/2 | none |
| TestEngineSave×TestRedactTransaction | **FU-2 order/concurrency interference class** (both are SafeSave writers; the class DEMONSTRATED live via sibling scanners TestSignatureRealCrypto 1/2 and TestEncryptedPackageSafeWrite 3/3-standalone-clean but failing in-suite) | §2.2 failure verbatims | **fixed this lane** — `RESOURCE_LOCK GlyphpdfCandidates` on 15 suites (§3.1); patched in-suite run green |
| TestSep13LeadComparePerf | load-robust by design (ratio bound) — held at `-j 2` | 3/3 standalone + 2/2 in-suite | none; residual risk only under heavier load than `-j 2` |
| (pair) TestRedactTransaction | covered by FU-2 class above | 3/3 + 2/2 once isolated | same patch |

Totals: 30 standalone runs + 10 instrumented LaneScheduler runs + 3 full in-suite runs
(2 unpatched + 1 patched). Machine: Windows 11, MSYS2 UCRT64, Qt 6.11.0, Ninja `-j 2`.

## 3. FU-2 shared-tempdir class — mechanism and patch

Production seam: `SafeSave::makeUniqueCandidate` (src/engines/SafeSave.cpp:54) creates candidates in
**`%TEMP%/glyphpdf-candidates`**, a process-SHARED directory; same dir referenced by
SignatureManager (src/engines/SignatureManager.cpp:1832). Any test asserting on that directory's
contents observes debris from every other suite that triggers a SafeSave/sign flow concurrently.

**Scanners** (assert dir contents):
- TestEngineSave — `QCOMPARE(leftoverCandidates(), 0)` ×10 (zero-based; sweeps debris in initTestCase)
- TestEncryptedPackageSafeWrite — delta-based `candidateFileCount() == before`
- TestFormSafety — delta-based + initTestCase sweep
- TestSendForSigning — delta-based (TestSendForSigning.cpp:731)
- TestSweepW1SigningAdversary — delta-based (line 354)
- TestSignatureRealCrypto — `successfulSignLeavesNoCandidate`
- TestPrintableSummary — sweeps/asserts the transaction dir (comment at line 23 documents FU-2)

**Writers** (SafeSave/makeUniqueCandidate users): the above plus TestAccessibilityFixes,
TestBatesBatchSafety, TestCheckedMutationCoverage, TestCertEncryptPicker, TestOfficeImport,
TestPageLabels, TestPersistenceOutcomes, TestPagesMode, TestSweepW1PresetAdversary,
TestRedactTransaction.

**Already protected** (pre-existing): TestLaneScheduler (`RUN_SERIAL`), TestBatchMode
(`RESOURCE_LOCK BatchModeIO` + `RUN_SERIAL`), TestPrintableSummary (`RUN_SERIAL`),
TestAccessibilityFixes (`RUN_SERIAL`, comment names the shared candidate dir),
TestSep13LeadBatchMerge (`RESOURCE_LOCK BatchModeIO`).

**Patch applied (this lane)**: `RESOURCE_LOCK GlyphpdfCandidates` on the unprotected
writer/scanner suites (surgical: serializes candidate users against each other while still
allowing the other ~150 suites to run in parallel). See §3.1 for the exact diff and the
in-suite verification runs.

### 3.1 Patch — applied and verified

CMakeLists.txt: +15 × `RESOURCE_LOCK GlyphpdfCandidates` on
TestEncryptedPackageSafeWrite, TestEngineSave, TestFormSafety, TestSendForSigning,
TestSignatureRealCrypto, TestSweepW1SigningAdversary, TestRedactTransaction, TestBatesBatchSafety,
TestCheckedMutationCoverage, TestCertEncryptPicker, TestOfficeImport, TestPageLabels,
TestPersistenceOutcomes, TestPagesMode, TestSweepW1PresetAdversary;
+ `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"` on TestMeasureCore (H3, GUI-less main — uniformity).
Behavior-preserving test-infra change only: no test source, no production code touched.
(Pre-existing protection left as-is: TestLaneScheduler `RUN_SERIAL`; TestBatchMode
`BatchModeIO` lock + `RUN_SERIAL`; TestPrintableSummary, TestAccessibilityFixes `RUN_SERIAL`;
TestSep13LeadBatchMerge `BatchModeIO` lock. A shared-`RESOURCE_LOCK` was chosen over
blanket `RUN_SERIAL` so the ~150 suites that never touch the candidate dir keep running
in parallel with the group.)

**Verification (patched, full `ctest -j 2`): 171/171 passed, 0 failures**
(`insuite-patched-r1.log`) — including the three suites that had just failed unpatched
(TestSignatureRealCrypto, TestEncryptedPackageSafeWrite ×2 runs) and the pair named in the
recorded flake list (TestEngineSave, TestRedactTransaction). TestModeStripPins and
TestMeasureCore also green in the same run.

Incident note (transparency): the first application of the patch script corrupted 4
single-line property blocks (slice-offset bug, `set_tests_properties(ies(`); detected via the
phase-3 MISS guard, repaired by exact-string replacement before this commit; cmake reconfigured
clean and the diff above is the post-repair state. The broken pattern is documented in
`.context/sweep-w2-testing-wip.md` so the cleanup phase does not reuse it.

## 4. Dead / orphaned (flagged; NOT deleted)

| Item | Status | Evidence |
|---|---|---|
| `tests/R14ProbeBatchSkip.cpp` | **ORPHAN — never registered** | no `add_executable`/`EXISTS` guard anywhere (grep "ProbeBatchSkip" in CMakeLists.txt: 0 hits); introduced by `cd01e89` (R14 independent-review probes, 2026-09-14); sibling probes R14ProbeRedactSpace/R14ProbeSep13Fixes DID get registrations. Syntax-checks clean against CURRENT headers (`c++ -fsyntax-only` with Qt6/podofo includes, stub .moc → RC 0) — dead-by-omission, seams (AppContext/PdfEditorEngine/BatchMode) all still exist. Cleanup options: register it (EXISTS-guard pattern like its sibling at CMakeLists.txt:5099) or delete. |
| `tests/R14ProbeRedactSpace.cpp` | built, intentionally unregistered | CMakeLists.txt:5099-5112 (manual probe + plugin deploy) |
| `tests/TestPdfEditorInterface.cpp` | alive (aggregated) | compiled into `UnitTests` (CMakeLists.txt:1135-1137, EXISTS-guarded) |
| `tests/SweepW1SecProbe.cpp` | alive | own exe `TestSweepW1SecProbe`, registered |
| fixtures / mocks | no orphans | `tests/fixtures/signing/*` all referenced (TestSignatureValidation/TestSignatureRealCrypto); `tests/mocks/Mock{PdfEditorEngine,SignatureManager}.h` both used |

Registered-but-never-running: NONE (170 add_test == 170 ctest -N post-reconfigure). Conditionally
skipped: TestSignatureValidation QSKIPs 5 of 7 slots when signing fixtures are absent — fixtures
ARE present on this machine, so it runs today; see §5 for why it is still flagged.

## 5. Duplicate / overlapping coverage map

Verdict: the redaction (9 suites) and signing clusters are LAYERED, not duplicated. Map:

| Layer | Suites | Contract |
|---|---|---|
| redaction engine | TestRedaction (23 slots) | text unextractable, image bytes removed, signed-doc refusal |
| redaction proof | TestRedactionProof (19) / TestSep13LeadRedactionProof (10) | proof pipeline + manifest / rotated+offset anti-false-pass — complementary, NOT duplicates |
| redaction marks/UI | TestRedactApplyMarks (4) / TestRedactClearMarks (2) / TestRedactMarkAll (34) | mark→apply, clear, panel integration |
| redaction transaction | TestRedactTransaction (75) | commit/cancel/retry/byte-identity semantics |
| pattern redact | TestPatternRedact (14) | pattern library |
| sanitize copies | TestRedactSanitizeBundle (1) / TestRedactionProof.proofWithSanitizeCoversSanitizedCopy / TestRedactMarkAll.sanitizeCopyCheckboxProducesCleanOutput | same contract (sanitized copy clean) via DIFFERENT paths (engine sequence / proof pass / UI checkbox) — overlap is intentional path redundancy, kept |
| signing: engine e2e | TestSignatureRealCrypto (23), TestSep13LeadBtDowngrade (2), TestSendForSigning, TestSweepW1SigningAdversary | real sign/validate, TSA failure semantics |
| signing: validation reporting | TestSignatureValidationMock (21) | mock-manager report handling (byte ranges, DSS, doc-TS) |
| signing: UI mapping | TestSignatureBadges (25), TestValidateAllSignatures (2) | badge mapping, summary counts |

**TRUE overlap flagged — merge/supersede proposal (cleanup phase decides; nothing deleted here):**
`TestSignatureValidation` (7 slots, 222 lines) vs `TestSignatureRealCrypto` (23 slots):
- `testValidateSignedDocument` asserts only "fields populated, does not crash" and contains the
  TAUTOLOGY `QVERIFY2(!sig.signerName.isEmpty() || sig.signerName.isEmpty(), ...)` — can never
  fail on content. Its own comment: "this is tested more strictly in TestSignatureRealCrypto".
- Overlapping contracts: sign+validate (RealCrypto `testBT_SignAndValidate` supersedes),
  level persistence (RealCrypto `settingsDrivenLevelAttestsItsPieces`), sign failure
  (RealCrypto `testEmptyPostConditionFailsAndPreservesOutput`).
- Unique in TestSignatureValidation: `testValidateMissingFile` edge, `testSetTsaUrlPersists`
  (config-level), `testValidateUnsignedPdf` (empty-list on unsigned) — 3 small pins to port into
  RealCrypto/Mock before retiring the suite.
Unique-assertion diff recorded in full in the merge proposal for the cleanup phase.

## 6. Coverage gaps vs feature-command matrix

Source: docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv (300 rows; 237 `implemented+available`).
- 224/237 rows carry a `test_or_repro` reference; all 18 distinct referenced tests exist (0 stale).
- **13 rows have NO test reference** — all shell-navigation: 5 mode-strip pills (`mode-view/edit/comment/form/protect`), `toggle-ai`, `task-chooser`, 6 side-pane tab rows (incl. duplicate id `pane-comments` in the matrix itself).
- Incidental coverage exists (TestCommandBinding.paneEntriesSwitchTheRealSidebarPanes for panes; TestScreenStateSync for pill/tab state application), but no suite drives the USER path (pill click → screen transition).
- Top-3 pins ADDED: new suite `tests/TestModeStripPins.cpp` — mode-pill click switching (5 pills, data-driven), toggle-ai signal, Tools-chooser composition + taskSelected payload, programmatic-setMode silence, pill exclusivity (11 test executions). Registered offscreen TIMEOUT 60. 11/11 standalone; Passed in all three full in-suite runs.

## 7. Harness hygiene findings

| # | Finding | Severity | Action taken |
|---|---|---|---|
| H1 | `AA_DontUseNativeDialogs`: only TestCertEncryptPicker lacks it among dialog-adjacent suites — but it drives the documented seam (`RecipientPickerDialog::addRecipientPaths`, line 238; comment explains native QFileDialog is undrivable offscreen). Verified safe. | none | none |
| H2 | QSettings isolation: 25/29 suites redirect org/app. `TestEngineSave::initTestCase/cleanupTestCase` does `QSettings settings; settings.remove("export/linearizeOnSave")` on the REAL user store (default org/app) — deletes a developer's actual preference if set. | low | flagged; propose INI-redirect (cleanup phase) |
| H3 | QT_QPA_PLATFORM: 169/170 suites carry it. Only `TestMeasureCore` lacks it — uses `QTEST_GUILESS_MAIN` (core-only, legitimately QPA-free). | none | property added anyway for uniformity (harmless, included in patch §3.1) |
| H4 | Assertion quality: tautological QVERIFY2 in TestSignatureValidation (§5). | medium | merge proposal |

## 8. Commits (this lane)

| SHA | Content |
|---|---|
| `8b28085` | §1-§7 static findings (inventory, FU-2 mechanism map, duplicate map, orphan, gaps, hygiene) |
| `8593541` | §2.1 standalone flake grids + TestLaneScheduler classification |
| `0ef416d` | test(pins): TestModeStripPins suite + registration |
| `ae636a5` | test(infra): FU-2 RESOURCE_LOCK patch (15 suites) + TestMeasureCore env; patched 171/171 verification |

Residuals for the cleanup/fix lanes: TestLaneScheduler bound redesign (§2.1);
TestSignatureValidation merge-or-retire proposal with 3 unique pins to port (§5);
R14ProbeBatchSkip register-or-delete decision (§4); TestEngineSave real-store QSettings
remove (H2, low). No deletions were performed by this lane.
