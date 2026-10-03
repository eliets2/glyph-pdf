# LANE REPORT — r3-hygiene (build/test registration hygiene)

- **Date:** 2026-10-03
- **Worktree:** `D:/pdf/pdf-r3-hygiene` — branch `feat/r3-hygiene`, base `f6d9ca1f`
- **Scope:** CMakeLists.txt test-registration hygiene — DocumentSession dedup consistency (code-archaeologist crossmodel #1-#5, #8-#10), expected-fail comment reconciliation + registration properties (testing-specialist crossmodel #1-#9, #11), dead target (M4), GsdW2Probe structure (M9).
- **Findings sources:** `D:/pdf/verification/crossmodel/testing-specialist.md` (18 findings; H1-H3 = #1-#3, M4-M9 = #4-#9), `D:/pdf/verification/crossmodel/code-archaeologist.md` (10 findings).
- **Verdict: PASS** — every finding dispositioned below as fixed / verified-already-accurate / removed-with-reason; no suite lost; `ctest -N` 202 → 203 (+1 deliberate DISABLED registration).

---

## 1. Configure / build / test evidence

- Configure: runbook flags verbatim (`Release`, `GLYPHPDF_ENABLE_LTO=ON`, `TESSERACT=ON`, `LIBSECRET=ON`, `TEST_FIXTURES=OFF`, `FUZZ=OFF`), Ninja, into `D:/pdf/pdf-r3-hygiene/build-rel` only.
- **BUILD_RC=0.** Honesty note: pass 1 of the cold build logged one transient link failure (`TestAutoBookmarks.exe`, LTO 115-LTRANS link killed — memory pressure under `-j 2` parallel LTO links, same OOM class that killed the background build shell with exit 137). The incremental resume relinked it cleanly; the final full-graph pass (post-comment-edits) completed `216/216` with **FINAL_BUILD_RC=0**. No source or CMake semantic cause; no other target ever failed.
- `ctest -N`: **202 before → 203 after** (the +1 is the deliberate `R14ProbeRedactSpace` DISABLED registration, §2.4). No existing test removed or renamed.
- Suites executed this lane (serial, offscreen via their registered properties), all green:

| Suite(s) | Result |
|---|---|
| TestSep13Lead{Capability,BatchMerge,OcrGuards,ConversionExport,RedactionProof,BtDowngrade,ComparePerf} | 7/7 passed, 31.8 s total |
| TestSweepW1PresetAdversary / SigningAdversary / SummaryPolicyAdversary / TestSweepW1SecProbe / TestGsdW2Probe | 5/5 passed, 3.0 s |
| TestFormBuilder / TestFormSafety / TestInterfaces / TestDjotFuzz / TestSendForSigning | 5/5 passed |
| TestBatchOpsCoverage | 1/1 passed (1.4 s) |
| TestViewParity / TestSignatureRealCrypto / R14ProbeSep13Fixes | 3/3 passed |
| R14ProbeRedactSpace | `ctest`: **Not Run (Disabled)** — DISABLED property holds even when named with `-R`; binary run directly: **7 passed / 0 failed** (see §2.4) |

---

## 2. Per-item disposition

### 2.1 DocumentSession dedup consistency (task item 1 + 6; archaeologist #1-#5, #8-#10)

Link-graph fact established first: `pdfws_engines` is a STATIC archive containing `src/engines/DocumentSession.cpp` exactly once; `pdfws_ui` links it **PRIVATE** (D2 comment) — for a static lib that still hands the archive to consumers' final links (LINK_ONLY), so any test linking `pdfws_ui` or `pdfws_engines` receives DocumentSession transitively.

- **TestFormBuilder — VERIFIED already deduped; comment FIXED to canonical convention.** The explicit `DocumentSession.cpp` was already removed by the earlier build-system dedup (406dfa25-era, on main before this lane's base). The in-source-list comment ("duplicates its moc symbols" — archaeologist #9: mechanism understated) was replaced by the canonical one-liner family above the `add_executable`, naming the exact edges: links `pdfws_engines` explicitly **and** via `pdfws_ui`; duplicate would clash on staticMetaObject/vtable (ODR); do not re-add. (Archaeologist #4's "which link line is the authority" ambiguity resolved in the comment.)
- **TestInterfaces — kept standalone, comment ADDED.** It does NOT link `pdfws_engines`/`pdfws_ui` (only `pdfws_core pdfws_commands`), so its explicit `DocumentSession.cpp` is the legitimate "standalone targets compile their own" case (archaeologist #1's option (b)). The new comment states the convention, the reason for the local copy, and the trap: if `pdfws_engines`/`pdfws_ui` is ever added to the link line, the source MUST be removed first. Also records the target-name ≠ source-name mismatch (TestInterfaces vs TestPdfEditorInterface.cpp) that has caused regex-sweep misses before (archaeologist #5 — addressed by comment, not rename: a rename would churn the registered ctest name for no behavioral gain).
- **TestFormSafety — comment aligned** to the same canonical wording (archaeologist #2: one shared phrasing across the fleet; #9: staticMetaObject/vtable mechanism), preserving the S2-2 link-rationale line.
- One-line convention, now stated at all three sites: *"DocumentSession is owned by pdfws_engines; test targets link it, never compile it — standalone targets compile their own."*

### 2.2 Expected-fail comment reconciliation + registration properties (task item 2; H1-H3 = findings #1-#3)

- **Registration properties — VERIFIED COMPLETE (no additions needed).** Every flagged block already carries the tree convention: `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"`, an explicit TIMEOUT (60-300 s per suite class), and LABELS including the family tag (`sep13leads` / `sweep-w1` / `security-sweep-w1`). The report's "registered as an ordinary add_test with no marker" referred to the absence of DISABLED/WILL_FAIL markers — i.e. the stale-comment problem below, not missing properties. Evidence table (all verified in-tree): Capability 60/BatchMerge 180 (+BatchModeIO lock)/OcrGuards 60/ConversionExport 60/RedactionProof 180/BtDowngrade 300/ComparePerf 180 (raised, §2.6)/PresetAdversary 120/SigningAdversary 240/SummaryPolicyAdversary 300/SecProbe 240.
- **SEP13-LEADS section header (H2) — FIXED to current truth.** Old: "expected failures are the confirmation evidence." The sources themselves say the repros were written to fail on candidate 83be3c2 and "a fix flips them green without any edit here"; the fixes landed (lead 3: Capability.cpp Degraded branch now reverses the registry-owned disable; lead 12: CompareWidget memoization, "FIXED (follow-ups lane, 2026-09-15)" in the suite header). New comment records the history, states the suites now PASS as regression guards, and pins the invariant: a failure means a lead regressed — fix the code, never the tests. **Verified by execution: 7/7 green.**
- **SWEEP-W1 ADVERSARY header (H3) — FIXED.** Same class: PresetAdversary/SigningAdversary were failing repros on candidate a3a9317, now green regression guards; SummaryPolicyAdversary was a probe "expected GREEN unless a probe finds a real defect" (per its own header) — the comment now distinguishes the two roles honestly. **Verified: 3/3 green.**
- **SWEEP-W1 SECURITY header (H1, TestSweepW1SecProbe) — FIXED.** Old comment claimed "Slots FAIL until the finding is remediated (S4/F1 is the documented design item)". Verified current truth: the S4 slot was REMOVED from the source (its header: "S4 (removed) … the W1-05 honesty pins in TestPolicyController/TestSupportBundle carry the disclosure contract"), F1 was fixed in 71891494, and the suite passes. Comment now records exactly that. **Verified: green (0.09 s).**
- The wave-closing "202/202" record and these suites are now consistent — the contradiction the cross-model auditor flagged (#15) is resolved on the comment side.

### 2.3 Stale comments (task item 4; M5-M8 = findings #5-#8, plus #11 same class)

- **#6 TestDjotFuzz — FIXED (verified stale).** Source check: `djotToDocument` walks the Lua AST (M5 implemented long ago); section-count round-trip is a HARD QCOMPARE; grep finds **no QEXPECT_FAIL** in the suite (the only mention is "this is a HARD assertion (no QEXPECT_FAIL)"). CMake comment rewritten to that truth, including the `<= original` block-count pin being an emitter limitation, not an expected failure. Suite green.
- **#7 TestSignatureRealCrypto — VERIFIED ALREADY ACCURATE / MOOT (no change).** The quoted "Revocation test marked XFAIL until M5 DSS-to-signature-field correlation is wired" comment **no longer exists anywhere in CMakeLists.txt** (grep empty); the source states "No QEXPECT_FAIL — this is a hard assertion" and the revocation pins are hard (`AD-01: isValid must be false…`). Suite run green. Nothing to fix.
- **#8 TestSendForSigning — FIXED (verified stale premise).** The old comment justified RUN_SERIAL by "the suite counts the SHARED %TEMP%/glyphpdf-candidates dir". Current truth: the per-test temp-roots block (bottom of CMakeLists) gives every test its own `TMP/TEMP/TMPDIR`, and the suite resolves candidates via `QDir::tempPath()` (`TestSendForSigning.cpp:183`) — the directory is PER TEST PROCESS now, and the count pins are delta-based (`candidatesBefore`/after) besides. Comment rewritten; **RUN_SERIAL + RESOURCE_LOCK GlyphpdfCandidates deliberately retained** as belt-and-braces for deterministic real-crypto timing (removal would be a scheduling-behavior change with no hygiene payoff).
- **#11 TestBatchOpsCoverage — FIXED (verified stale, same class).** Old: "plus one expected-fail characterization finding for the 2U/3U conformance fall-through." Source check: the level matrix now carries first-class `L2U`/`L3U` rows asserting exact `pdfaid:part`+conformance for ALL five levels (1B/2B/2U/3B/3U), and there is no QEXPECT_FAIL in the suite. Comment rewritten; suite green (1.4 s).
- **#5 TestSep13LeadComparePerf — PARTIAL (in-scope part fixed; code ceiling noted).** CTest TIMEOUT raised 120 → 180 (tree default ceiling; perf-ratio suite). The in-test `kCeiling = 100.0` the auditor flagged was **left unchanged**: it is already the "relative" shape the auditor asked for (two samples measured back-to-back in the same run, 100 ms floor per sample, so uniform load inflation cancels), and the source documents the headroom rationale ("100x is the geometric midpoint: >= 2.1x headroom above the worst fixed measurement"). Widening it further without fresh multi-machine perf evidence would be guesswork — handed to a future perf-measurement lane in the notes below.
- **#10 TestViewParity — VERIFIED already resolved (no change).** The QEXPECT_FAIL marker was removed pre-redesign per its own instruction (PROGRAM-CONSOLIDATION 1.4); the suite now pins the fixed behavior directly and has no known-red pin. Suite green.

### 2.4 Dead target R14ProbeRedactSpace (task item 3; M4) — REGISTERED DISABLED, with a corrected history

- Old state: `add_executable` with no `add_test` — compiled dead weight on every build, invisible to ctest, with a comment saying registration was removed because the probe "now fails BY DESIGN since the F1 fix (7189149) landed".
- Disposition per task preference: registered as a **DELIBERATE DISABLED probe** (same class as the documented `R14ProbeBatchSkip`), with full convention properties (`offscreen`, `TIMEOUT 180`, `LABELS "R14-probe;probe;redaction;qt;headless"`, `DISABLED TRUE`). `ctest -N` 202 → 203; nothing removed.
- **Honesty finding during verification:** running the binary directly shows it **PASSES 7/7 today** — the inherited "fails BY DESIGN" claim was itself stale. Git archaeology: the probe source was amended post-F1-fix by `a5618f52` (REDACTION-RESEARCH-2026-09-21 §1.6 Plan 1-A) to assert the fixed behavior. The comment now states the verified truth: it stays DISABLED because its coverage **duplicates TestSep13LeadRedactionProof** (which carries the permanent pins), not because it is red. Also documented: ctest will not execute a DISABLED test even when named with `-R` (lists "Not Run (Disabled)") — the old comment's "run it explicitly with ctest -R" instruction (also present, still wrong, on R14ProbeBatchSkip's older comment) does not work on this CMake; run the binary under `QT_QPA_PLATFORM=offscreen` instead. (R14ProbeBatchSkip's own block was left textually intact apart from the re-nesting below; its DISABLED rationale is unchanged.)

### 2.5 TestGsdW2Probe structure (task item 5; M9) — FIXED

- `add_executable(TestGsdW2Probe …)` sat at column 0 while its body was indented inside `if(EXISTS TestGsdW2Probe.cpp)` — the misleading indent is fixed (properly nested now).
- The `R14ProbeBatchSkip` block was **hoisted out of the TestGsdW2Probe EXISTS guard** to top level, so removing `TestGsdW2Probe.cpp` can no longer silently drop the BatchSkip probe (the auditor's coupling complaint). Its mis-indented comment block (first line at 4 spaces, rest at column 0) is fixed, and the de-nesting rationale is recorded in the block.
- Both registrations verified intact after the restructure: `#197: R14ProbeBatchSkip (Disabled)`, `#198: TestGsdW2Probe`.

### 2.6 Registration-hygiene extras found in-pass (same class, fixed)

- Duplicated link items `pdfws_engines … pdfws_ui … pdfws_engines` deduped in TestSweepW1SigningAdversary, TestSweepW1SecProbe, W2BProbeSigning, W2BProbeSummaryPolicy; doubled `Qt6::Pdf` deduped in TestSep13LeadComparePerf. Pure no-ops semantically (CMake dedups link items); they were the same "which line is the authority" drift the archaeologist flagged on TestFormBuilder.

---

## 3. Out-of-scope notes (recorded, not changed)

- **Finding #12** (TestUpdateChecker's unconditional `GLYPH_TESTING=1`, unlike every other target's `GLYPHPDF_ENABLE_TEST_FIXTURES` gate): left as-is. Gating it under this lane's `TEST_FIXTURES=OFF` configure would change the binary's behavior; needs its own verified change.
- **Finding #13/#14** (evidence dirs / scorecard verifiability): the evidence directories exist in this tree (`docs/audit/evidence-*`); the auditor's concern applied to the subset of tree contents they were given.
- **Finding #16** (TestSweepW3UxFlows 900 s timeout, documented load flake): known/disclosed; unchanged.
- **Finding #17** (TestGsdW2Probe "probe" wording vs normal registration): auditor's own verdict "fine as-is; the label carries the intent".
- **Finding #18** (add_test override detects Qt6::Test only in direct LINK_LIBRARIES): fragility acknowledged; every current QtTest suite links it directly. Future-lane hardening candidate.
- **W2BProbe\* / SWEEP-W2C tail executables without add_test**: unlike M4, these carry an explicit block comment ("not part of the default ctest gates, run manually per docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md") — documented deliberate non-registrations, left alone.
- **Archaeologist #6/#7** (duplicate `#include "core/Capability.h"` + include-grouping drift in `src/GpMainWindow.cpp`): src residue, outside this lane's CMake registration scope; flagged for the next src-hygiene lane.
- **Archaeologist #8** (TestFormBuilder's `pdfws_commands` link justification): the suite's pins drive FormBuilderMode command flows; link retained, now covered by the rewritten comment's explicitness.

## 4. Hard-rule compliance

- Work confined to `D:/pdf/pdf-r3-hygiene`; build dir `D:/pdf/pdf-r3-hygiene/build-rel` only; no pushes, merges, rebases, stashes, gc; worktree/branch intact; no CLAUDE.md/SECURITY.md created; every pre-existing registration preserved (202 → 203, the +1 is the explicit M4 disposition).
