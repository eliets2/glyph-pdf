# PROGRESS — feat/cmyk-decode (resumed 2026-10-05, third instance)

Worktree: D:/pdf/pdf-cmyk, branch feat/cmyk-decode, base 0441df7c (docs commit).
Prior instances died on provider infra, not on work defects. This file is the
resume ledger; update it as steps complete.

## Fourth instance (2026-10-05, resumed from step 6)

Resume-state finding: PoDoFoBackend.cpp was found AT HEAD (reverted) — the
third instance died mid-step-6 after `git checkout` of the source but before
recording RED. Snapshots verified: NC-src-revert.patch byte-identical to
wip-podofo-backend.patch, `git apply --check` clean.

6. DONE. Rebuild on reverted source: BUILD_RC=0. RED run recorded in
   RED-fail-before.txt — EXACTLY the 4 lift pins failed:
   - cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically (DCT door shut:
     kept 1240x1754, no downsample)
   - rawCmykIccBasedImageIsDownsampledColorimetrically (no /DCTDecode)
   - indexedCmykBaseWithProfileIsDownsampled (no /DCTDecode)
   - cmykDownsampleMatchesFogra39Reference — failure text is the naive
     signature itself: got rgb(255,0,0) want rgb(227,6,20) (pure magenta
     decoded NAIVE, plan §1.2's quantified drift). 16 passed / 4 failed;
     every guard pin green (profileLessCmykStaysSkipped,
     cmykWithUnusableIccProfileStaysSkipped, both re-scoped malformed pins).
7. DONE. NC-src-revert.patch applied; 6-file WIP restored; rebuild
   BUILD_RC=0 (full relink wave, 203 targets); TestCompressJpegReencode
   single-target confirm: 100% passed, 0 failed.
8. DONE. pass-after x3 SERIAL (6-target gate, ctest, no -j):
   run1/run2/run3 all "100% tests passed, 0 tests failed out of 6"
   (PASS-after-run{1,2,3}.txt). FOGRA39 pin executes on this machine (the
   RED run failed it with real color values), so it ran and passed in all
   three green runs; slot-level -V detail captured on the final confirm run.
9. DONE. NC recorded ONCE (NC-run.txt): git checkout HEAD -- PoDoFoBackend.cpp,
   rebuild BUILD_RC=0 (NOTE: the infra storm killed the background build
   wrapper ~50 min in — a surviving orphan ninja plus a foreground resume of
   the last 4 edges finished it; all edges green). 6-target gate:
   83% passed (5/6 slots green); TestCompressJpegReencode failed with EXACTLY
   the same 4 lift pins and byte-identical failure text as RED-fail-before.
   Restored via git apply NC-src-revert.patch; final rebuild BUILD_RC=0;
   PASS-after-final-tree-confirm.txt (ctest -V): 20 passed, 0 failed,
   0 skipped — cmykDownsampleMatchesFogra39Reference actively PASSED (not
   QSKIP): the real CoatedFOGRA39 press-profile pin ran and validated the
   plan §3.4 reference values through the full chain.
   Snapshot hygiene: wip-tests.patch was stale (predated the step-4 i3
   fixture fix) — refreshed from the dirty tree; wip-disclosure.patch
   verified current; NC-src-revert.patch == wip-podofo-backend.patch
   (byte-identical, apply-check clean).
10. DONE. Full suite SERIAL (FULL-suite-final.txt): 99% — 201 passed,
   1 failed out of 202 (2 disabled probes Not Run). The 1 failure is
   TestOfficeImport (Timeout 120s) in testOfficeToPdf_realConversion —
   re-run twice more (FULL-suite-officeimport-rerun.txt), identical
   signature; ROOT-CAUSED PRE-EXISTING, not this lane's diff:
   ConversionManager.cpp:655 passes "--env:UserInstallation=" to soffice;
   LibreOffice requires "-env:" (probe: single dash RC 0 + PDF; double dash
   HANGS). Prior lanes' machines had LibreOffice absent → the slot QSKIPped
   (CLEANUP-LEDGER-2026-09-09.md:56), so the defect never ran there. One-
   character owner fix documented in the evidence file; deliberately NOT
   fixed in this lane (out of scope, own R7 discipline would be required).
11. DONE. LANE-REPORT-cmyk-decode-2026-10-05.md written; two commits on
    feat/cmyk-decode: 43788467 (feat: implementation+tests+disclosure),
    then evidence+report (this file's final state amended in). Working
    tree clean; nothing pushed; worktree/branch untouched.

## State on resume (verified)

- WIP dirty diff (6 files) = complete Option-A implementation per
  docs/research/cmyk-lcms2-plan-2026-10-04.md:
  - src/engines/podofo/PoDoFoBackend.cpp — CMYK lift: DCT door (Qt CMYK8888 +
    embedded APP2 ICC, or PDF-side /ICCBased /N 4 fallback), raw /ICCBased /N 4
    door (row-wise wrap into Format_CMYK8888), /Indexed /ICCBased /N 4 base
    (palette transformed once). Profile-less CMYK (plain /DeviceCMYK,
    profile-less CMYK JPEG, /Indexed /DeviceCMYK) stays SKIPPED. All guarded
    by `#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)`.
  - src/core/Capability.{cpp,h} + src/modes/CompressDialog.cpp —
    `downsampleScopeDisclosure()` on the Downsample checkbox (tooltip +
    status tip).
  - tests/TestCompressJpegReencode.cpp — synthetic CMYK ICC fixture (lut16
    A2B0, CLUT-vertex-exact mapping RGB=(C,M,Y)); flipped pins:
    malformedImages (CMYK case re-scoped to profile-less payload),
    malformedIndexed (b) re-scoped, rawCmykImageStaysSkipped... REPLACED by
    profileLessCmykStaysSkipped; new positive pins
    cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically,
    rawCmykIccBasedImageIsDownsampledColorimetrically,
    indexedCmykBaseWithProfileIsDownsampled; refusal pin
    cmykWithUnusableIccProfileStaysSkipped; FOGRA39 render pin
    cmykDownsampleMatchesFogra39Reference (QSKIP-conditional).
  - tests/TestCompressDialogHonesty.cpp — downsample disclosure pin.
- Evidence snapshots: wip-podofo-backend.patch (= NC-src-revert.patch,
  verified byte-identical to dirty diff), wip-disclosure.patch,
  wip-tests.patch (all refreshed against current tree).

## Done this instance

1. Analyzed WIP vs plan §4.2/§4.3 — coherent, no new deps, no test weakened.
2. Refreshed patch snapshots (prior wip-disclosure.patch was stale).
3. BUILD_RC=0 (tree was already compiled by prior instance; incremental build
   clean).
4. Probe run of 6-target gate: 5/6 targets green, TestCompressJpegReencode
   19 passed / 1 failed — FAIL was a FIXTURE bug in
   cmykWithUnusableIccProfileStaysSkipped: placeholder i3 (mkImage(W3), never
   converted to CMYK, stays plain /DeviceRGB) collided on dims W3=1248 with
   the case-(c) profile-less CMYK JPEG, so findImages(W3,H)[0] captured the
   placeholder's stream; the placeholder legitimately re-encodes (plain RGB
   above threshold). Product code correct. Fixed by removing the i3
   placeholder from that slot only (the other i3 use, in
   malformedIndexedImagesAreSkippedSafely, IS mutated and stays).
5. Rebuild BUILD_RC=0; 6-target gate: 100% passed, 0 failed out of 6.

## Remaining (R7 + delivery)

6. fail-before RED: revert ONLY src/engines/podofo/PoDoFoBackend.cpp to HEAD
   (patch saved), rebuild, run TestCompressJpegReencode → expect RED on the 3
   positive lift pins (+ FOGRA39). Record RED-fail-before.txt.
7. Restore (git apply NC-src-revert.patch), rebuild → pass.
8. pass-after x3 SERIAL (6-target gate) → PASS-after-run{1,2,3}.txt.
9. NC once: revert PoDoFoBackend.cpp again, rebuild, run gate → same RED
   signature, all other slots green → NC-run.txt; restore, rebuild, final
   confirm run → PASS-after-final-tree-confirm.txt.
10. Full suite SERIAL → FULL-suite-final.txt.
11. Commits (implementation+tests, evidence+report) on feat/cmyk-decode;
    LANE-REPORT-cmyk-decode-2026-10-05.md.

## Constraints honored

- No new deps; estimate modeling untouched (estimate makes no
  per-colorspace claim — plan §4.3 last bullet).
- No stash/merge/gc; patches via git apply of saved snapshots only.
- No test weakened: the two adapted pins keep their skip guarantees (scoped
  to the profile-less class, which is the surviving half of the guard);
  rawCmykImageStaysSkippedUntilColorManagedDecode is superseded 1:1 by
  profileLessCmykStaysSkipped per contracted discipline.
