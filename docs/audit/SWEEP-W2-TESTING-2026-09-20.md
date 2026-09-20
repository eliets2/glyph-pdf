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
- Top-3 pins added (§6.1): mode-pill click switching (5 pills, one suite), toggle-ai, task-chooser.

## 7. Harness hygiene findings

| # | Finding | Severity | Action taken |
|---|---|---|---|
| H1 | `AA_DontUseNativeDialogs`: only TestCertEncryptPicker lacks it among dialog-adjacent suites — but it drives the documented seam (`RecipientPickerDialog::addRecipientPaths`, line 238; comment explains native QFileDialog is undrivable offscreen). Verified safe. | none | none |
| H2 | QSettings isolation: 25/29 suites redirect org/app. `TestEngineSave::initTestCase/cleanupTestCase` does `QSettings settings; settings.remove("export/linearizeOnSave")` on the REAL user store (default org/app) — deletes a developer's actual preference if set. | low | flagged; propose INI-redirect (cleanup phase) |
| H3 | QT_QPA_PLATFORM: 169/170 suites carry it. Only `TestMeasureCore` lacks it — uses `QTEST_GUILESS_MAIN` (core-only, legitimately QPA-free). | none | property added anyway for uniformity (harmless, included in patch §3.1) |
| H4 | Assertion quality: tautological QVERIFY2 in TestSignatureValidation (§5). | medium | merge proposal |

## 8. Commits (this lane)

(filled at end)
