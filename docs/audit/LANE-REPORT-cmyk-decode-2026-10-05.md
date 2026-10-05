# LANE REPORT — cmyk-decode (parity scorecard §4 row 13, second half: CMYK)

- Lane: color-managed CMYK downsampling — Option A of `docs/research/cmyk-lcms2-plan-2026-10-04.md` (Qt 6.8+ `QColorSpace` CMYK decode, zero new dependencies)
- Worktree: `D:/pdf/pdf-cmyk`, branch `feat/cmyk-decode`, base `0441df7c` (the plan's own docs commit)
- Date: 2026-10-05
- Contract: lift the CMYK skip **only** for images that carry their own ICC profile (DCT-embedded APP2 or PDF-side `/ICCBased /N 4`), colorimetrically, via Qt's color-managed decode; profile-less CMYK stays skipped (PDF 2.0 Annex B defines no default CMYK→RGB); flip the three downsample-lane tripwire pins per contracted discipline; add the profile-less-stays-skipped tripwire, colorimetric render pins, and the no-profile refusal pin; disclosure on the Downsample checkbox.
- Resume note: three prior instances of this lane died on provider infrastructure storms mid-flight (not on work defects). The PROGRESS ledger (`docs/audit/evidence-cmyk-decode/PROGRESS.md`) carried the state; this instance verified the WIP against the plan, found the source reverted mid-fail-before (saved patch intact, apply-check clean), and ran the remaining R7 sequence to completion.

## Outcome

**Shipped.** CMYK images with an embedded ICC profile are now decoded color-managed (Qt `Format_CMYK8888` + `QColorSpace::fromIccProfile` + CLUT `colorTransformed`), downsampled, and re-encoded as real `/DeviceRGB` JPEGs — with Adobe-inversion semantics handled by Qt's reader (0 = 100 %-ink inversion undone on decode, per plan §3.2). Profile-less CMYK stays byte-identical through the pass, on all three doors (raw `/DeviceCMYK`, profile-less CMYK JPEG, `/Indexed /DeviceCMYK` base) — now pinned as the new tripwire. Zero new dependencies; everything gated `#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)` with the pre-6.8 `#else` restoring the former skip verbatim.

The downsample lane's owner item ("vendored lcms2 or keep the skip") is resolved: the pinned Qt 6.11.0 already contains a color-managed CMYK engine, runtime-verified colorimetric against lcms2 within ≤5/255 (plan §3.4) and now pinned end-to-end by the FOGRA39 render pin on real press-profile values.

## Implementation (file-by-file)

1. `src/engines/podofo/PoDoFoBackend.cpp` (+219 lines, `optimizeDocument` Phase 1 only):
   - `cmykIccBasedProfileBytes()` — bounded `/ICCBased` resolution: 2-element array, indirect stream, `/N 4` exactly, ≤4 MiB, `PdfError`-contained; anything else returns empty ⇒ skip.
   - `cmyk8888ToSRgb()` — refuses missing/non-CMYK/identity transforms (identity would misread CMYK bytes as RGB); otherwise `colorTransformed(tr, Format_RGB888)`.
   - **DCT door**: non-RGB/Gray JPEG is decoded; only a genuine `Format_CMYK8888` decode with a resolvable profile (embedded APP2, else PDF-side `/ICCBased /N 4` fallback) is transformed and falls through to the shared downsample/re-encode; everything else — RGB payload behind a `/DeviceCMYK` label, profile-less CMYK JPEG, undecodable garbage — is skipped untouched with a debug line.
   - **Raw/Flate door**: `/ICCBased /N 4` qualifies; plain `/DeviceCMYK` does NOT (no profile ⇒ no colorimetric transform ⇒ Annex B skip stands). Samples wrap row-wise into `Format_CMYK8888` (little-endian `QCmyk32` layout = PDF sample order; stride 4·W, no Qt padding), size-guarded, transformed, `isGrayImage = false`.
   - **Indexed door**: base `[/ICCBased <stream>]` CMYK — the ≤256-entry palette is transformed ONCE through the profile, then the existing palette-expansion path runs unchanged; a parser-refused profile skips the image untouched.
   - All shared guards untouched: ImageMask/SMask/Mask/Decode-array, dimension cap, predictor, media-filter, per-image `PdfError` containment. CMYK + SMask stays skipped (mask-desync rationale unchanged).
2. `src/core/Capability.{h,cpp}` + `src/modes/CompressDialog.cpp` — `downsampleScopeDisclosure()`, the canonical scope wording (which CMYK classes are recolored colorimetrically, which stay untouched and why), carried on the Downsample checkbox tooltip + status tip — same honesty seam as "Subset fonts".
3. `tests/TestCompressJpegReencode.cpp` — 15 → 20 slots, fixture machinery in the anonymous namespace:
   - Synthetic but genuine ICC v2 lut16 (mft2) A2B0 CMYK profile with a 4³ CLUT whose vertex mapping is exact (RGB = (C, M, Y)); banded CMYK8888 fixtures at CLUT vertices; expected-value and **naive-conversion** helpers (the naive values are asserted to NOT match — the failure signature of a naive swap).
   - `rawCmykImageStaysSkippedUntilColorManagedDecode` superseded 1:1 by `profileLessCmykStaysSkipped` (the surviving half of the old guard, now the tripwire: raw `/DeviceCMYK` and profile-less CMYK JPEG stay byte-identical, colorspaces kept, below-threshold twin untouched).
   - Positive pins: `cmykJpegWithEmbeddedProfileIsDownsampledColorimetrically` (DCT door: re-encode `/DCTDecode` `/DeviceRGB` 8bpc, dims ≈72 dpi, band colors within 15/255 of the profile-defined mapping, per-channel distance > 15 from the naive values; below-threshold twin byte-identical), `rawCmykIccBasedImageIsDownsampledColorimetrically`, `indexedCmykBaseWithProfileIsDownsampled` (palette transformed once).
   - Refusal pin: `cmykWithUnusableIccProfileStaysSkipped` — (a) mangled lut16 input-channel count (parser-refused, asserted invalid up front), (b) `/N 3` non-CMYK profile claim, (c) refused PDF-side profile over a profile-less CMYK JPEG — all byte-identical.
   - Render pin: `cmykDownsampleMatchesFogra39Reference` — Windows' real `CoatedFOGRA39.icc` driven through the full chain against the plan §3.4 lcms2-verified reference table, tolerance 8/255, loud QSKIP when the profile is absent (on this machine it RAN and PASSED).
   - Re-scoped (not weakened): `malformedImagesAreSkippedSafely`'s CMYK case (now an RGB-payload-behind-CMYK-label instance of the profile-less class) and `malformedIndexedImagesAreSkippedSafely` case (b) — both keep their byte-identical skip guarantees.
4. `tests/TestCompressDialogHonesty.cpp` — `downsampleCarriesCmykScopeDisclosure` slot + `stateViolations` extension: tooltip/statusTip must carry the canonical wording; the disclosure must name ICC, say "colorimetric", say "left untouched", and must not explain an implemented pass with an availability excuse.

## Evidence summary (R7 contract) — all gates met (`docs/audit/evidence-cmyk-decode/`)

1. **Fail-before RED** (`RED-fail-before.txt`): implementation reverted (source at HEAD), all 20 slots present, rebuild `BUILD_RC=0` → **exactly the 4 lift pins failed** (`cmykJpegWithEmbeddedProfile…` — dims kept 1240×1754; `rawCmykIccBased…` and `indexedCmykBase…` — no `/DCTDecode`; `cmykDownsampleMatchesFogra39Reference` — "got rgb(255,0,0), want rgb(227,6,20)", i.e. the pin failed ON the naive-conversion signature it exists to catch); 16 passed, all guard pins green.
2. **Negative control, recorded ONCE** (`NC-src-revert.patch`, `NC-run.txt`): scoped revert of `PoDoFoBackend.cpp` only, rebuild `BUILD_RC=0` → identical 4-pin RED with byte-identical failure text; 5/6 gate slots green (83%). Patch re-applied, restore verified.
3. **Pass-after ×3, SERIAL** (`PASS-after-run{1,2,3}.txt`): six-suite gate `TestCompressJpegReencode | TestImageDedup | TestDedupSMask | TestOptimizeEstimate | TestCompressStripSanitize | TestCompressDialogHonesty`, ctest serial (no `-j`): **3 × 100 % passed, 0 failed out of 6**.
4. **Final tree confirm** (`PASS-after-final-tree-confirm.txt`, `ctest -V` after the last full rebuild `BUILD_RC=0`): TestCompressJpegReencode **20 passed, 0 failed, 0 skipped** — FOGRA39 actively passed (not skipped).
5. **Full suite, SERIAL** (`FULL-suite-final.txt`): **99 % — 201 passed, 1 failed out of 202** (2 disabled probes Not Run). See the pre-existing-failure finding below.

Builds: runbook `build-rel` reused; every build `cmake --build D:/pdf/pdf-cmyk/build-rel --config Release -- -k 0 -j 2` with ucrt64-first PATH → `BUILD_RC=0` each time (only the pre-existing third-party lua `-Wstringop-overflow` warnings). Infra-storm note for reproducibility: two of the relink waves (~200 LTO links each) survived provider-kill storms via a surviving orphan ninja plus a foreground resume of the final edges; no build was left half-applied (verified by `ninja -n` dry-run counts and rebuild `BUILD_RC=0`).

## Pre-existing failure found and root-caused (NOT this lane's diff)

`TestOfficeImport` times out (120 s) in `testOfficeToPdf_realConversion` on this machine — full-suite run + two identical re-runs recorded (`FULL-suite-final.txt`, `FULL-suite-officeimport-rerun.txt` with the analysis appended). Root cause, proven by direct probe: `ConversionManager.cpp:655` launches soffice with `--env:UserInstallation=…` (double dash); LibreOffice requires `-env:` (single dash). Same machine, same input: single dash → RC 0 + valid PDF; double dash → hang. Prior lanes' environments had LibreOffice absent, so this slot always QSKIPped (`CLEANUP-LEDGER-2026-09-09.md:56`) and the defect never executed. Zero code-path overlap with this lane. Deliberately left unfixed here (out of scope; would need its own R7 discipline) — the evidence file hands the owner a one-character fix.

## Constraints honored

- Work confined to `D:/pdf/pdf-cmyk`; no pushes, merges, rebases, stash, gc; no worktree/branch deletion; patches applied only from saved snapshots (`git apply NC-src-revert.patch`).
- No new dependencies (plan Option A); `LICENSE-3RD-PARTY.md` untouched; no CMake changes.
- No test weakened: the two adapted pins keep their skip guarantees; the single superseded pin (`rawCmykImageStaysSkippedUntilColorManagedDecode` → `profileLessCmykStaysSkipped`) is the contracted tripwire flip, 1:1, with the flip rationale in-source.
- Estimate honesty: no estimate-modeling change; estimates never claimed a per-colorspace behavior (`TestOptimizeEstimate` green in all runs; plan §4.3 last bullet).
- Profile-less CMYK lift remains out of scope (bundling a redistributable CMYK profile is the owner's separate sub-decision, plan §4.4).

## Owner items

1. Fold row 13 into the scorecard endgame: CMYK-with-profile DONE colorimetrically; profile-less CMYK stays skipped by PDF-spec necessity (not by missing tooling) — the skip rationale moved from "no engine in-tree" to "no default CMYK→RGB in PDF 2.0 Annex B".
2. `ConversionManager.cpp:655`: `--env:` → `-env:` (one character, evidence in `evidence-cmyk-decode/FULL-suite-officeimport-rerun.txt`).
3. Optional Qt-robustness note (plan §4.5): Qt < 6.8 builds compile back to the honest skip via the `#else` branches; the pinned 6.11.0 in the build env keeps the feature alive.
