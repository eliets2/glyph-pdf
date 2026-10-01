# R7 evidence — parity row 13: extend downsampling to indexed (CMYK honestly blocked)

Lane: feat/cmyk-downsample, worktree D:/pdf/pdf-w2b-cmyk, base ad77c29c, date 2026-10-01.
Touched production code: src/engines/podofo/PoDoFoBackend.cpp (optimizeDocument Phase 1 downsample pass).
Touched suite: tests/TestCompressJpegReencode.cpp (4 new slots, no existing slot modified).

## 1. Fail-before pin (RED before the fix)

Command: ctest --test-dir D:/pdf/pdf-w2b-cmyk/build-rel -C Release -R '^TestCompressJpegReencode$' --output-on-failure
Baseline tree: implementation NOT applied (git checkout of PoDoFoBackend.cpp at ad77c29c state),
new test slots present. Full output: RED-fail-before.txt (this directory).

    FAIL!  : TestCompressJpegReencode::indexedRgbImageIsDownsampledToRgbJpeg()
             'filterIs(*big->obj, "DCTDecode")' returned FALSE.
             (indexed image must be re-encoded to /DCTDecode)
    FAIL!  : TestCompressJpegReencode::indexedGrayImageBecomesGrayscaleJpeg()
             'filterIs(*outImages[0].obj, "DCTDecode")' returned FALSE.
             (indexed gray image must be re-encoded to /DCTDecode)
    Totals: 10 passed, 2 failed (pre-fix interim runs; final RED run: 13 passed, 2 failed)

The two guard pins (malformedIndexedImagesAreSkippedSafely,
rawCmykImageStaysSkippedUntilColorManagedDecode) PASS before the change by design:
they pin skip behavior that must not regress (bounded scope edges + the CMYK block).
An earlier RED attempt additionally crashed the slot in PoDoFo fixture code
(PdfImage::SetData stride quirk + PdfString UTF-8 text re-encoding of binary palette
bytes — see LANE-REPORT, "fixture constraints"), fixed inside the test only.

## 2. Negative control (scoped revert, recorded ONCE)

git apply of the saved src patch reversed ONLY src/engines/podofo/PoDoFoBackend.cpp
(tests kept). Patch preserved as NC-src-revert.patch. Rebuild (0 FAILED targets), then:

    FAIL!  : TestCompressJpegReencode::indexedRgbImageIsDownsampledToRgbJpeg()
             'filterIs(*big->obj, "DCTDecode")' returned FALSE.
    FAIL!  : TestCompressJpegReencode::indexedGrayImageBecomesGrayscaleJpeg()
             'filterIs(*outImages[0].obj, "DCTDecode")' returned FALSE.
    Totals: 13 passed, 2 failed

Exactly the 2 behavior pins went RED, with the same failure text as the fail-before
run; every other slot (incl. both guard pins) stayed green. Patch then re-applied
(git apply NC-src-revert.patch) and rebuilt.

## 3. Pass-after — 3 consecutive clean SERIAL runs

Command (serial, no -j, single ctest invocation):
ctest --test-dir D:/pdf/pdf-w2b-cmyk/build-rel -C Release -R
  '^TestCompressJpegReencode$|^TestImageDedup$|^TestDedupSMask$|^TestOptimizeEstimate$|^TestCompressStripSanitize$|^TestCompressDialogHonesty$'
  --output-on-failure

    run 1: 100% tests passed, 0 tests failed out of 6
    run 2: 100% tests passed, 0 tests failed out of 6
    run 3: 100% tests passed, 0 tests failed out of 6

Additionally captured to files (three further consecutive runs on the same tree,
then one confirmation run after the final full-tree rebuild with the last
comment-only edit compiled in):

    PASS-after-run1.txt / run2 / run3: 100% tests passed, 0 tests failed out of 6
    PASS-after-final-tree-confirm.txt: 100% tests passed, 0 tests failed out of 6

Builds after the change: full-tree `cmake --build … -- -k 0` → `[190/190]`,
no FAILED targets (only the pre-existing third-party lua -Wstringop-overflow
warnings); a final incremental rebuild after a comment-only whitespace edit →
ninja exit 0 ("[1/1] Staging runtime DLLs", i.e. fully up to date).

No existing test weakened or removed; no existing slot needed modification
(malformedImagesAreSkippedSafely's DCT-CMYK pin still holds — the CMYK skip remains).
