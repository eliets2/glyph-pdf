# PROGRESS — font-subset CFF follow-up lane (feat/fs-cff, worktree D:/pdf/pdf-fs-cff)

Resumed 2026-10-05 after prior instances died on provider storms. Uncommitted WIP found in the
worktree (7 dirty files, no PROGRESS.md — prior instance died before writing one). This file is
the resume ledger; every future resume appends here.

## State found on resume (reconstructed from the worktree)

* Base `2c09393b`, branch `feat/fs-cff`. Dirty: `src/core/Capability.cpp`,
  `src/engines/podofo/FontSubsetter.{h,cpp}`, `src/modes/CompressDialog.cpp`,
  `tests/TestCompressDialogHonesty.cpp`, `tests/TestFontSubset.cpp`; untracked `build_red.log`.
* `build_red.log` (2185 lines): a FULL build, 920/920 targets linked, zero errors — only
  pre-existing warnings (lua memcpy -Wstringop-overflow from msys string.h, LTRANS notes).
  Name suggests it was meant to be the RED-phase log but contains a green build.
* Implementation state (all diffs read in full this session):
  - Layer 1b CFF surgery complete in FontSubsetter.cpp: `parseCffIndex`, `parseCffDict`,
    `rebuildCffDict` (fixed 29-form offset re-encoding → two-pass layout),
    `parseCffCharset` (0/1/2, ISOAdobe identity, Expert refused), `parseCffEncodingCustom`
    (0/1, supplements refused), `parseCffFdSelect` (0/3), exact-coverage tiling in
    `parseCffLayout` (any unmodeled byte ⇒ refuse), `scanType2ForSeac` (Type 2 token space,
    hintmask accounting), `blankUnusedCffCharstrings` (kept bytes verbatim, blanks → bare
    endchar, charset/FDSelect/subrs/Private verbatim, Top DICT/FDArray rebuilt),
    `cffCidsToGids`, `cffCodeToGidBuiltIn` (custom encoding or ASCII 32..126 Standard
    Encoding), `cffNameToGid` (standard SIDs 1..95 + String INDEX), `rebuildSfntReplacingTable`
    (OTTO wrapper, checksum discipline), `cffSeacGate` (kept charstrings AND all subrs).
  - `parseSfntDirectoryEx` (allowOtto) added; cmap/post gained allowOtto; strict
    `parseSfntDirectory` OTTO rejection preserved (TT-lane pin intact).
  - Walker: FontKind gained CidCffProgram / CffNameProgram / CffOpenTypeProgram; classifyFont
    reworked (decided-flag); program-kind skew across shared programs ⇒ counted skip;
    CIDFontType0 Identity-H/-V/-UCS2 over bare CIDFontType0C; Type1/OpenType over FontFile3
    (Type1C bare, OpenType wrapper); /Length1 written only for FontFile2 (FontFile3 carries
    no Length keys); rewrite dispatch by ProgKind; new counter `skippedCffUnsupported` (seac).
  - Estimator `estimateSubsetSavings` already kind-agnostic (uses usable + blankableBytes).
  - Disclosure: `subsetFontsScopeDisclosure()` wording now "TrueType and CFF font programs
    (/FontFile2, /FontFile3 Type1C and CIDFontType0C, and OpenType-wrapped CFF)… Type1 … left
    untouched" (single source of truth; CompressDialog + probe + honesty-test comment updated).
  - TestFontSubset: hermetic synthetic CFF builders (makeCidCff 6 glyphs, CID≠GID charset,
    FDSelect/FDArray/local-subr; makeNameCff 4 glyphs SID 34..36; makeOttWrapper cmap (3,10)
    fmt 12), doc fixtures (CFF CID, simple CFF, OpenType), test-side INDEX/sfnt readers, and
    11 new/changed slots: cffLayoutParsesSyntheticFonts, cffBlankingPreservesNumberingAndKeptBytes,
    roundTripCidFontType0C…, simpleCffBuiltinEncodingRoundTrip, openTypeWrapperCffRoundTrip,
    nonCidKeyedCffUnderType0IsUntouched, simpleCffNamedEncodingIsUntouched,
    cffSeacCharstringSkipsFont, corruptCffProgramSkipsSafely; old cffProgramIsLeftUntouched
    renamed unparseableCffProgramIsLeftUntouched (degradation contract kept).
* NOT yet done on resume: R7 evidence (RED / NC / green×3 serial), full serial suite,
  commits, lane report. No test had been run (no logs).
* GAP vs mission: "sub-FontMatrix transforms" skip-disclosure is NOT implemented anywhere
  (grep FontMatrix: zero matches). Plan this session: PDF-dict /FontMatrix gate on the CFF
  kinds (non-default ⇒ counted skip) + refuse a CFF Top-DICT FontMatrix operator (12 7) in
  parseCffLayout + pin `cffSubFontMatrixIsUntouched`.

## Session log (append-only)

### 2026-10-05 (resume 1)
* Read all six dirty diffs + build_red.log + TT lane report + plan §CFF; wrote this file.
* First incremental build (pre-edit WIP) killed externally mid-flight; restarted after the
  FontMatrix work landed (below) — first log `build-resume1.log` is PARTIAL (died 168/206),
  resume log `build-resume1b.log` is the record that counts.
* **Sub-FontMatrix gap closed** (mission-listed unsafe case, missing from the WIP):
  - FontSubsetter.h: skip-list comment names sub-FontMatrix; new counter `skippedFontMatrix`.
  - FontSubsetter.cpp: `fontMatrixIsDefault` helper (absent or exactly [0.001 0 0 0.001 0 0]);
    classifyFont sets `FontInfo::nonDefaultFontMatrix` for CidCffProgram (Type0 wrapper AND
    descendant checked) and CffNameProgram/CffOpenTypeProgram; walker refuses the program
    (counted `skippedFontMatrix`); `parseCffLayout` refuses a FontMatrix operator (12 7) in
    the Top DICT and in FDArray Font DICTs; "not a parseable CFF" skipReason text and the
    run-summary log line updated; `<cmath>` include added.
  - TestFontSubset.cpp: `makeNameCff(..., topFontMatrix)` emits a BCD-real 0.001 FontMatrix
    op; hermetic pin added to cffLayoutParsesSyntheticFonts (parse must refuse);
    `buildSimpleCffFixture(..., subFontMatrix)` adds /FontMatrix [0.0005 …]; new document pin
    `cffSubFontMatrixIsUntouched` (estimate claims nothing + program byte-untouched).
* R7 protocol decided (follows r4-ux/r3-sec precedent):
  - RED (fail-before): scoped pre-CFF simulation patch in classifyFont — the three new kinds
    routed to `CffProgram` + fontFile=nullptr, marked `// RED STAGE` → expect exactly the 3
    round-trip pins RED (roundTripCidFontType0C…, simpleCffBuiltinEncodingRoundTrip,
    openTypeWrapperCffRoundTrip); skip/hermetic pins pass (the pure layer compiles-coupled;
    its teeth are proven by the NC + review — recorded honestly).
  - NC once (combined single run): 4 neutralizations marked `// NC STAGE` —
    (1) scanType2ForSeac short-circuit Clean → cffSeacCharstringSkipsFont RED;
    (2) encodingUnprovable forced false for Type1/OpenType → simpleCffNamedEncodingIsUntouched RED;
    (3) cffCidsToGids guesses (cidKeyed refusal + not-found → identity insert) → nonCidKeyedCffUnderType0IsUntouched RED;
    (4) walker FontMatrix enforcement removed → cffSubFontMatrixIsUntouched RED.
    Zero collateral reds expected (disjoint fixtures). Restore, grep "NC STAGE" = 0, rebuild.
  - pass-after ×3 SERIAL: TestFontSubset green ×3 + TestCompressDialogHonesty,
    TestCapabilityRegistry, TestExportPathBadge (wording surfaces), then full serial suite.

### 2026-10-05 (resume 1) — R7 RED captured
* Green build of full WIP + FontMatrix work: `build-resume1b.log` **BUILD_RC=0** (resume of
  the externally killed partial run — runbook incremental retry, recorded).
* RED build (pre-CFF simulation, `// RED STAGE` ×5 markers): `build-red.log` BUILD_RC=0.
* RED run: `red-testfontsubset-precff.log` — **21 passed, 3 failed** (RED_RC=8), and the 3
  failures are EXACTLY the round-trip capability pins:
  - roundTripCidFontType0CPreservesUsedAndBlanksUnused — "an eligible CIDFontType0C program
    must yield a real estimate" FALSE;
  - simpleCffBuiltinEncodingRoundTrip — same shape (Type1C);
  - openTypeWrapperCffRoundTrip — same shape (OpenType/CFF).
  All skip-disclosure, hermetic-surgery, TrueType-core and signed-doc pins PASS under the
  simulation (the skip behavior of the pre-CFF lane is what those pins assert — expected).
* NC stage applied on top of the restored tree (`// NC STAGE` ×4 markers; `RED STAGE` grep = 0):
  (1) scanType2ForSeac short-circuit Clean; (2) encodingUnprovable forced false on
  Type1/OpenType; (3) cffCidsToGids guesses (cidKeyed refusal commented + absent-CID identity
  insert); (4) walker FontMatrix enforcement `if (false && …)`. Expect exactly 4 slots RED:
  cffSeacCharstringSkipsFont, simpleCffNamedEncodingIsUntouched,
  nonCidKeyedCffUnderType0IsUntouched, cffSubFontMatrixIsUntouched; zero collateral.
  NC build: `build-nc.log`; NC run log: `nc-testfontsubset.log` (next step).

### 2026-10-05 (resume 1) — R7 NC captured + one fixture bug found
* NC run: `nc-testfontsubset.log` — **19 passed, 5 failed** (NC_RC=8):
  - EXACTLY the 4 predicted skip pins RED: cffSeacCharstringSkipsFont,
    simpleCffNamedEncodingIsUntouched, nonCidKeyedCffUnderType0IsUntouched,
    cffSubFontMatrixIsUntouched (each "Compared values are not the same" = the
    untouched-assert; the guessed rewrite went through).
  - 1 collateral red: openTypeWrapperCffRoundTrip "the wrapped program must
    shrink" — ROOT CAUSE a WIP fixture bug, not an implementation bug: the test
    built the wrapper over `makeNameCff()` whose unused glyph only frees 15 raw
    bytes; inside the OTTO wrapper that delta is inside deflate noise → the
    pass correctly took the no-gain restore path → decoded stream unchanged →
    assert failed. FIX: the builder already had the `bulkUnused` pad (+180 raw
    bytes) for exactly this case (comment said so) — the prior instance forgot
    to use it. Test now `makeNameCff(/*seacGlyph=*/false, /*bulkUnused=*/true)`;
    assert unchanged (fixture strengthened, not weakened). NC-side note: all 4
    neutralizations are disjoint from the OTTO fixture, so the assert sequence
    it exposed (estimate claims → no-gain restore) is the GREEN behavior too —
    the bug would have failed pass-after anyway; NC surfaced it early.
* All RED/NC markers restored: grep "RED STAGE" = 0, "NC STAGE" = 0 (verified).
* Green rebuild 1: `build-green.log` killed externally mid-link (exit 137, second storm kill);
  resumed → `build-green2.log` **BUILD_RC=0**.
* Green1 run: `green1-testfontsubset.log` — **23 passed, 1 failed**: only
  openTypeWrapperCffRoundTrip, "Compared values are not the same", Actual -152 / Expected -368
  at the per-table compare. ROOT CAUSE #2 (WIP test bug, not implementation):
  `readSfntTables` returned (offset, length) pairs while the test treats .second as an end
  offset (span.second - span.first). First time this code ever executed (RED stopped the slot
  at the estimator, NC at the shrink assert). FIX: reader now returns [start, end) spans —
  same convention as readCffIndexAt (test-side only; implementation already copies non-CFF
  tables verbatim, and the fixed reader now genuinely verifies that). bytesSaved debug in
  green1 proves the pass commits on all three CFF round-trips (21 / 27 / 10 encoded bytes).
* Next: rebuild (`build-green3.log`) → pass-after ×3 SERIAL from a clean slate (green1 does
  not count toward the three; it is recorded as the bug-finder), missioned honesty suites,
  then the full serial suite.

### 2026-10-05 (resume 1) — R7 pass-after: TestFontSubset 24/0/0, ×3 SERIAL green
* green3 shakeout found TWO more WIP test bugs in the OTTO pin (implementation correct
  throughout — the rewriter's verbatim-copy + checksum discipline held every time):
  1. readSfntTables returned (offset,length) while the pin treated .second as end → reader
     fixed to [start,end) (test-side only).
  2. Blanket byte-identity for `head` contradicted the ISO 14496-6 discipline the
     implementation correctly applies (checkSumAdjustment recomputed over the new layout):
     pin amended to head-bytes-identical-except-adjustment + the classic whole-font
     invariant `checksum(final) == 0xB1B0AFBA` (STRONGER than the old pin; test-side only).
  - One -Werror=shadow build break in the new test lambda (`byte` shadows rpcndr.h's
    typedef) — renamed to `octet`; build-green4.log final **BUILD_RC=0**.
* PASS-AFTER (final tree): green-serial-1/2/3.log — **3 × "100% tests passed"** (RC=0 each,
  SERIAL); green-final-verbose.log — **Totals: 24 passed, 0 failed, 0 skipped**.
* Missioned honesty suites: pass-missioned-honesty.log — TestCapabilityRegistry +
  TestCompressDialogHonesty + TestExportPathBadge **3/3 passed (RC=0)**.
* Next: full serial suite gate (ctest, all tests), then commits + lane report.

### 2026-10-05 (resume 1) — full gate + the TestOfficeImport investigation
* Full serial suite (detached run): `full-serial-suite.log` — **201/202 passed**, 2 Disabled
  probes, **1 failure: TestOfficeImport (Timeout at 120.04 s)** — an unrelated feature area
  (Office import), which the lane diff cannot influence.
* Isolation re-run (`flake-officeimport-isolation.log`): **reproduces** — not a flake.
* Root-caused to a PRE-EXISTING product bug outside the lane's scope, with a controlled A/B:
  `ConversionManager.cpp:655` launched soffice with `--env:UserInstallation=...` (double
  dash). LibreOffice bootstrap variables accept ONLY `-env:` (single dash); the double-dash
  spelling is silently ignored → soffice used the SHARED default profile instead of the
  private temp one → with the machine's default profile in a blocking state, conversion
  stalls → the 120 s internal timeout ≈ ctest's 120 s TIMEOUT → Timeout. (Yesterday's
  endgame 203/203 passed because the default profile was healthy then.)
  Probes (docs/audit/evidence-fs-cff/soffice-probes.md): same fresh profile, seconds apart —
  `-env` converts in 2.6 s; `--env` hangs >45 s. Five further probes bisect
  filter/path/profile-name variables — only the dash count matters.
* Fix applied (one token, disclosed as an out-of-mission drive-by per the TT lane's
  bugs-found-during-verification practice): `"--env:UserInstallation="` →
  `"-env:UserInstallation="` + explanatory comment. Rebuild: `build-soffice-fix.log`
  **BUILD_RC=0**.
* TestOfficeImport after the fix: `pass-officeimport-postfix.log` — **Passed 7.86 s** (was
  Timeout 120.04 s). Causal chain proven end-to-end.
* **FULL SERIAL GATE (final tree): `full-serial-suite2.log` — 202/202 passed, 0 failed
  (FULL_RC=0), 2 Disabled probes (R14ProbeRedactSpace, R14ProbeBatchSkip), 433.55 s real.**
* Remaining: commits (feat / fix / test / docs split) + lane report finalization.

### 2026-10-05 (resume 1) — COMMITS
* Committed on feat/fs-cff (see git log):
  1. feat(font-subset): CFF lane — FontSubsetter.{h,cpp} + Capability.cpp + CompressDialog.cpp
  2. fix(conversion): soffice `-env:` single-dash fix — ConversionManager.cpp
  3. test(font-subset): TestFontSubset CFF suite + TestCompressDialogHonesty comment flip
  4. docs(audit): LANE-REPORT-fs-cff-2026-10-05.md + evidence-fs-cff/







