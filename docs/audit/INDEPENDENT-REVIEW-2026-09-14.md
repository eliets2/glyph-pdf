# Independent Review — 2026-09-14 (R14 reviewer)

Reviewer: R14 independent reviewer. Candidate: `feat/parity-glm` @ `39aaca3ebdbae47589004db13cd3e42031418abe`
(review branch `feat/parity-glm-review` in worktree `C:\Users\User\Projects\pdf-r18`).
Build: `build-r18-noeng` (Debug/Ninja, full 365-target rebuild from the branch switch, `-j 2`,
`QT_QPA_PLATFORM=offscreen`). Protocol per package: implementer claim → falsifiable contract →
committed suite → own probe → negative control → independent read path on SAVED artifacts → verdict.
Ledger flips made in the same docs commit as this file. Handoff: `.context/review-r14-2026-09-14.md`.

Reviewer probe TUs (committed under `tests/`, temporarily registered during review; CMake
registration blocks removed after the review — re-add an `if(EXISTS)` block modeled on the
TestSep13Lead* registrations to rebuild them):

- `tests/R14ProbeRedactSpace.cpp` — Queue 1 (L5/L7/L8 composed)
- `tests/R14ProbeSep13Fixes.cpp` — Queue 2 (L1/L2/L13/M7/L9)
- `tests/R14ProbeBatchSkip.cpp` — Queue 3 (Q1/Q3/Q4)
- standalone: `C:\Users\User\scratch-r14\r14-keystroke-probe.cpp` (+ `-POST.txt`) — Queue 4 (R18(f))

Raw evidence: `C:\Users\User\scratch-r14\results\` (q1-*, q2-*, q3-*, q4-*, q5-*, NC-* files
referenced per package below). Environment limitations: the Codex G-gate probe workspace
(`Documents\Codex\2026-09-05\...`) is empty at review time — G-negatives were exercised via the
full-suite gate instead; `docs/audit/R22-LINUX-INSTALLED-RESOURCES-2026-09-14.md` does NOT exist
on this branch (nothing to review; no flip).

## Verdict table

| Package | Contract | Committed suite | Own probe | Negative control | Verdict |
|---|---|---|---|---|---|
| SEP13 redaction L5 (proof attribution origin+/Rotate) | offset/rotated viewer marks attribute content runs; no false VerifiedNoTextInRegion | TestSep13LeadRedactionProof 8/8 (`q1-TestSep13LeadRedactionProof-POST.txt`) | combined MediaBox [0 200 612 1042]+/Rotate 90 fixture; own mark math; attribution + honest failure (`q1-R14Probe-POST-full.txt`) | NC-L5: proof `viewerToUser` → Height-only flip → 4 failures incl. both committed L5 slots (`q1-NC-L5-probe.txt`, `q1-NC-L5-suite.txt`); restored, green | **verified** |
| SEP13 redaction L6 (disclaimer wording) | disclaimer claims only what attribution+extraction deliver; overclaim phrase banned | TestRedactionProof 20/20 incl. wording pin (`q1-TestRedactionProof-POST.txt`) | — (wording pin read; correct-contract assert verified in suite) | lane's fail-before evidence accepted (pin bans a concrete phrase; wording is text) | **verified** (caveat: wording inherits F1 scope on /Rotate pages) |
| SEP13 redaction L7 (annotation/form attribution) | viewer mark over displayed annot attributes the string on ANY page orientation | committed L7 slots 8/8 — but rotate-0 pages ONLY | `rotatedPageAnnotOnlySecretMustNotFalsePass` + `rotatedAnnotMarkFalsePassDemonstration`: proof certifies `passed=true, removed=[], failures=[]` over surviving `AnnotSecretZebra` on /Rotate 90 offset page; rotate-0 control attributes correctly (`q1-R14Probe-FINDING.txt`) | negative control not applicable — the FIXED build itself demonstrates the gap (probe FAILS on fixed, control isolates /Rotate) | **PARTIAL — FINDING F1** |
| SEP13 redaction L8 (excision + overlay origin) | real redaction excises secret on offset/rotated page; overlay label inside mark band; honest Completed only when excised | committed overlay slot 8/8 (label baseline 895.19 ∈ [876..918]) | `realRedactionExcisesSecretOnOffsetRotatedPage`: secret gone from SAVED artifact via PDFium + raw bytes + PoDoFo; benign line survives; proof PASSes the really-excised artifact | NC-L8: excision flip disabled → secret SURVIVES with outcome=Completed (`q1-NC-L8-probe.txt`); restored, green | **verified** |
| L1 B-T downgrade honesty | dead TSA ⇒ outcome≠plain-Success AND `timestampMissing`; artifact honestly carries NO timestamp; no-TSA control = Success | TestSep13LeadBtDowngrade 4/4 (`q2-TestSep13LeadBtDowngrade-POST.txt`) | `l1_deadTsaDisclosedAndArtifactHonest`: outcome=3 (PartialLtvMissing), flag true, own PoDoFo object walk of the SIGNED artifact = 0 DocTimeStamp objects; control Success (`q2-R14ProbeSep13Fixes-POST3.txt`) | combined NC @1100285: anchor fails with plain-Success signature (`q2-NC-TestSep13LeadBtDowngrade.txt`); restored | **verified** |
| L2 HTML font-name injection | hostile BaseFont name cannot break out of `style="..."` | TestSep13LeadConversionExport 6/6 | own name `A}'; color:red; x='(u)` + own attribute-boundary scan (2 attrs, no breakout) | combined NC: raw `color:red` re-injected (`q2-NC-TestSep13LeadConversionExport.txt`) | **verified** |
| L3 Degraded reversal | unavailable→Degraded reverses registry-owned disable | TestSep13LeadCapability 5/5 | — (registry seam; suite anchor asserts `w.isEnabled()` after Degraded) | combined NC: stuck-disabled fail (`q2-NC-TestSep13LeadCapability.txt`) | **verified** |
| L9/L10 batch merge honesty | failed/cancelled merge: 0 successes, ≤1 result per input, no output | TestSep13LeadBatchMerge 5/5; TestBatchMode 17/17 | own failure injection (output dir = an existing FILE): success=0, fail=2, no phantom, no output | combined NC: 3-results-for-2-files + cancel-counted-1-success (`q2-NC-TestSep13LeadBatchMerge.txt`) | **verified** |
| L11 OCR reject/reOcr guards | reject/reOcr ignored outside ReviewReady | TestSep13LeadOcrGuards 5/5; TestOcrReviewLifecycle 30/30 | — (state-machine seam; anchors assert state + signal spy) | combined NC: reject-during-Saving + reOcr-from-Idle both reproduce (`q2-NC-TestSep13LeadOcrGuards.txt`) | **verified** |
| L13+M7 columns/line-join | ragged right-aligned column stays one column; small line under big heading stays separate | TestSep13LeadConversionExport 6/6; TestConversionExtraction 17/17 (in gate) | own 3-row ragged fixture + own 18pt/7pt fixture (`q2-R14ProbeSep13Fixes-POST3.txt`); BOUNDARY PINNED: strictly-past-extent start continues the column, inside-extent start opens a new anchor (documented V03 preservation — now empirically pinned by the probe) | combined NC: `","5"` split + merged `BOLDHEAD tiny detail` reproduce | **verified** |
| M1/M3/M5 (static-LOW) | encryptionPassword cleared on release; cleanupCandidate on both new fail paths; word edits refresh canvas | TestEngineSave 19/0, TestSignatureRealCrypto 23/0/1, TestOcrReviewLifecycle 30/30 (gate) | code-state counts at HEAD: cleanupCandidate()=7 (base 5+2), encryptionPassword.clear()=4 (base 3+1), setWords(m_reviewWords)=3 (base 1+2) — match ledger | code-state negctl (base counts) accepted for STATIC leads per confirmation doc | **verified** |
| Q1 batch OCR skip (work + kept-page extraction) | pages WITH text never rendered/OCRed under skip-pages; kept pages = ORIGINAL page objects | TestBatchOcrSkipText 8 slots (10 pass total; see flake note) | own 3-page fixture + own CountingOcr: calls==1; SAVED artifact kept-page content streams byte-identical to source; OCRed page differs (`q3-R14ProbeBatchSkip-FINAL.txt`) | NC-Q1: pre-loop skip disabled → probe counts 3 calls, committed slot fails (`q3-NC-Q1-probe.txt`, `q3-NC-Q1-suite.txt`); restored, green | **verified** |
| Q2 batch harness idempotency | run-2 on outputs is a no-op without modal/race | TestBatchOcrSkipText full runs green (this session: 4/5 standalone, 2/2 ctest — flake is in the Q3 persistence slot, not Q2's) | — (test-harness-only fix; production untouched — verified by ledger + slot read) | lane's live hang/race observations (300 s kill; success 0 vs complete log) accepted | **verified** |
| Q3 settings write-on-change | toggles persist to the preference store immediately; fresh instance reads them | `skipOptionsPersistAcrossRestart` (green 4/5 standalone runs) | own probe: toggle → sync → independent `reg query` dump carries all three keys = true; fresh BatchMode reads them checked (`q3-R14ProbeBatchSkip-FINAL.txt`) | lane NC (writer disabled → fresh harness unchecked) accepted; lane's own failing-first observation | **verified** (suite flake noted below) |
| Q4 skip-pages preservation + XFAIL | extracted-text equality + object-level page preservation; /AcroForm gap surfaced, never silently weakened | `skipPagesPreservesPageObjectsOnSavedFile` 10-pass run incl. 1 documented XFAIL | `q4_acroFormGapIsRealAndXfailHonest`: SAVED output keeps the Widget annot, catalog /AcroForm ABSENT — the documented gap reproduces independently (`q3-R14ProbeBatchSkip-FINAL.txt`) | lane NC (forced kept-page extraction failure → slot fails at annots) accepted | **verified**; XFAIL honest: QEXPECT_FAIL(Continue) with reason, XPASS would surface; CSV overclaim corrected |
| R18(f) keystroke tier | /AA /K gates typing: reject reverts, transform rewrites, failure fails closed, no script = typing stands | TestFormKeystroke 8/8; TestFormJsCalc 49/49 (`q4-*.txt`) | standalone probe vs tip formjs: 6 own vectors ALL PASS (merge+transform→ABC, rc=false, splice abXYef, hostile loop honest Timeout, event shape `false|X|1|2`, AFNumber_Keystroke installed) (`scratch-r14/r14-keystroke-probe-POST.txt`) | lane revert controls (AFormShim alone / panel alone → documented slot failures) accepted; sandbox-layer deadline verified by my probe | **verified** |
| R19 settings end-to-end pin | real PreferencesDialog Save → persisted bytes → readSigningConfig | TestSignatureBadges 24/24 incl. modal-driver dismissal + section-aware INI parser (`q4-TestSignatureBadges.txt`) | slot mechanics read: real dialog, real Save click, INI bytes parsed as text independent of QSettings | pin-bites evidence: two named first-draft test defects each demonstrated the pin failing | **verified** (pin-only row) |
| r18-review F1/F2/F3 | verify-at-tip of da3aac0 | TestFormStaleDisclosure 14/14, TestFormUndo 12/12, TestHistoryIntegrity 15/15, TestFormBuilder 10/10 (`q4-*.txt`) | — | lane surgical-sever mutation (exactly 4 feed pins fail) accepted | **verified-at-tip** (da3aac0 itself remains un-reviewed history) |

## FINDING F1 (the one partial; severity: data-loss class on a narrower composition)

Claim broken: L7's annotation/form attribution composes with L5's /Rotate handling.
Demonstrated on the FIXED build (`39aaca3`): a spec-correct viewer mark over the DISPLAYED
FreeText annotation on a MediaBox [0 200 612 1042] + /Rotate 90 page attributes NOTHING; the
proof certifies a clean PASS (`passed=true, removed=[], failures=[]`) while `AnnotSecretZebra`
survives verbatim in the output (raw bytes + object strings). Root cause:
`src/core/RedactionProof.cpp` `collectAnnotStrings` uses PoDoFo `PdfAnnotation::GetRect()`,
which applies its own /Rotate adjustment at read time (in-memory (100,650,200×30) → reloaded
(450,512,30×200); raw `/Rect` in the file per qpdf = [100 650 300 680]) — inconsistent with the
shared PageSpace display law that the L5 fix itself established (and that my probe verifies for
CONTENT attribution/excision/overlay on the same fixture). ISO 32000-1 §12.5.2: /Rect is default
user space; /Rotate rotates the whole presentation, so the spec-correct viewer mark maps via
`viewerToUser` onto the RAW /Rect. Control on a rotate-0 offset page attributes correctly — the
gap is exactly the /Rotate composition. Scope: rotated (90/270) pages, annot-only or
mark-over-annot-only secrets, decodable page stream for the pure false PASS. Suggested repair
direction: attribute from the raw `/Rect` (dict read), not `GetRect()`; add a rotated L7 repro.
The committed L7 slots only cover rotate-0 pages, and their `!proofPassed` assertion is also
satisfiable via UNSWEPT stream problems — both worth tightening.

## TestBatchOcrSkipText flake characterization (owner = batch/OCR lane; NOT fixed by reviewer)

This worktree, this session: standalone ×5 → 4 green / 1 FAILED
(`skipOptionsPersistAcrossRestart`, "skip-files choice must persist across restart";
`q3-standalone-3.txt`); in-suite ctest ×2 → green. One signature observed (Q3 persistence slot),
matching the r18f-gate report of order/environment sensitivity. The fix verdicts above rest on
my own probe, not on this suite.

## Full-suite gate (spot-verify scope, one run + instructed rerun)

`ctest -j 2` from `build-r18-noeng` @ `39aaca3` (+ my probe registrations): **152/154 passed**
(151.97 s; `q5-ctest-full.log`). Exceptions:
- `R14ProbeRedactSpace` — my own probe's F1 demonstration slots (expected).
- `TestEncryptedPackageSafeWrite` — in-suite transient; standalone rerun 11/11 green
  (`q5-TestEncryptedPackageSafeWrite-rerun.txt`); not on the known-flake list, noted not chased.
All named spot-verify targets passed inside the gate: TestOllamaProvider (37.1 s), TestExportPathBadge
(R09), TestFormUndo (R02), TestPersistenceOutcomes (Pack A real replacement), TestCommandBinding /
TestWelcomeRoutes / TestUiAccessibility / TestUiAccessibility200 (R15–R17), TestReadOnlyGate,
TestSecretStore, TestPdfACidSetSafety, TestAutosave, TestThreadSafety, TestBatchOpsCoverage,
TestLaneScheduler. Preserved R05/P1 form-JS deadline probe (pdf-inst review-r14, copied to
scratch, compiled against tip): 8/8 modes, signatures identical to the recorded postfix results
(`scratch-r14/r14-formjs-tip-results.json`).

## Skips

- R22 Linux audit doc: absent from this branch — no claims reviewed, no flip.
- G01–G23 negatives: exercised via the full gate (the preserved Codex G-probe workspace is empty);
  no per-G bespoke probe beyond the gate.
- L4/M4 (static/security-store leads outside my queue): not re-verified; no flip requested.
- veraPDF/external validators: not available in the loop env (matches lane notes).
