# LANE REPORT — cmyk-downsample (parity scorecard §4 row 13)

- Lane: 13 of wave 2b — "Extend downsampling to CMYK/indexed (Gray done)"
- Worktree: `D:/pdf/pdf-w2b-cmyk`, branch `feat/cmyk-downsample`, base `ad77c29c`
- Date: 2026-10-01
- Scorecard anchor: `docs/audit/PARITY-SCORECARD-2026-09-30.md` §4 row 13 ← §3 row 93; guard site was cited as `PoDoFoBackend.cpp:6512-6513` (approximate) — located in the current tree inside `optimizeDocument` Phase 1 (DCT guard + raw/Flate colorspace qualification).

## Outcome

**Indexed: implemented. CMYK: honestly blocked (deliberate, pinned, reported)** — exactly the two-part contract the mission authorized for a tree without color management.

### 1. Indexed-color downsampling (the bounded part — shipped)

`optimizeDocument` Phase 1 now decodes `/ColorSpace [/Indexed /DeviceRGB|/DeviceGray <hival> <lookup>]` 8bpc images in the raw/Flate path:

- Palette parsed up front (before any stream decode): base must be `/DeviceRGB` or `/DeviceGray`; `hival` must be 0..255; lookup bytes obtained from an inline string, an indirect string, or an indirect stream (expanded); lookup must cover `(hival+1) × comps` bytes or the image is skipped untouched with a debug line.
- Indices expanded to pixels via `QImage::Format_Indexed8` + color table (row-wise copy — Qt pads scanlines, the buffer must not be wrapped); the table is padded to 256 entries so crafted indices above `/hival` resolve in-bounds (black), never an out-of-bounds read.
- The shared downsample + JPEG re-encode then proceeds unchanged; output is a real `/DCTDecode` `/DeviceRGB` (or `/DeviceGray`) JPEG with `/BitsPerComponent 8`, stale `/DecodeParms` removed.

**Re-index decision (documented, per mission authority):** staying EXPANDED is the contract. Smooth scaling produces interpolated pixels that are not in the palette; snapping them back re-introduces banding (i.e. re-encodes the very artifact downsampling removes), and the tree has no quantizer to build a fresh palette. Re-indexing is a net loss; expansion to RGB JPEG matches the tree's existing "re-encode as real JPEG" pattern for every other downsampled class.

### 2. CMYK (honestly blocked — no silent degradation)

Verified, not assumed:

- No CMYK decode path exists anywhere in the tree: `grep -ri cmyk src/` hits only (a) text-color parsing in `ContentSpans.cpp` (text extraction, not image decode), (b) the downsampling guard's own comment. The MRC pipeline (`src/engines/mrc/MrcPageProcessor.cpp`) and OCR pipeline (`src/engines/ocr/`) consume already-rendered RGB QImages — they never see CMYK pixels, contrary to the mission's hypothesis.
- No ICC transform engine: no lcms2/other CM library in `CMakeLists.txt` or `third_party/`. The only ICC artifact in the tree is the hand-built static sRGB profile blob embedded as PDF/A output intent (`PoDoFoBackend.cpp:3280+`) — a profile, not a transform engine. Pdfium bundles color management internally but exposes no public transform API usable here.
- Qt's JPEG read does a naive CMYK→RGB conversion (Adobe-convention inversion) with no source profile — applying it would silently recolor every CMYK image, which is precisely the failure mode the deliberate skip was written to prevent ("CMYK JPEGs would silently change colors"). Per the mission's decision rule, that makes CMYK "genuinely unavailable" rather than "genuinely decodable".

The CMYK block is pinned, both doors:

- DCT path: existing `/DeviceRGB|/DeviceGray` guard untouched; `malformedImagesAreSkippedSafely` (pre-existing) still passes unchanged.
- Raw path: new pin `rawCmykImageStaysSkippedUntilColorManagedDecode` asserts a genuine 4-bytes-per-pixel `/DeviceCMYK` stream stays byte-identical through the pass.
- Indexed door closed too: `malformedIndexedImagesAreSkippedSafely` asserts `/Indexed /DeviceCMYK` stays byte-identical (base colorspace check runs before any decode).

Lifting this later requires: an ICC-capable CMYK decode (vendored lcms2 or equivalent) plus flipping these pins — they are written to go RED if anyone lifts the skip without color management.

## File-by-file changes

1. `src/engines/podofo/PoDoFoBackend.cpp` — `optimizeDocument` Phase 1:
   - Phase-1 comment updated: indexed removed from the skip set; CMYK skip rationale documented with the row-13 rationale in-source.
   - Raw/Flate path: `/Indexed` colorspace detection (4-element array, first element `/Indexed`); bounded palette parse (base/hival/lookup validation, indirect lookup resolution, 256-entry table padding); new pixel-decode branch expanding indices → `Format_Indexed8` → RGB888/Grayscale8; `isGrayImage` propagates so gray-base stays grayscale JPEG.
   - No changes to the DCT path, dedup, estimate, strip, or dialog code.
2. `tests/TestCompressJpegReencode.cpp` — 4 new slots + helpers (no existing slot modified):
   - helpers `makeIndexedImage`, `bandedIndices`, `downsampleOptions` (anonymous namespace); includes `<algorithm>`, `<cstring>`.
   - `indexedRgbImageIsDownsampledToRgbJpeg` — 4-color-palette banded image: downsampled to `/DCTDecode` `/DeviceRGB`, dims ≈72dpi, all four band colors survive the decode+downsample+JPEG round-trip within tolerance 15; a small indexed image below the DPI threshold stays byte-identical (per-image scoping).
   - `indexedGrayImageBecomesGrayscaleJpeg` — gray-base palette → real grayscale JPEG (`/DeviceGray`, `Format_Grayscale8`), band values within tolerance 12.
   - `malformedIndexedImagesAreSkippedSafely` — short lookup / CMYK base / 4bpc indices all stay byte-identical; pass succeeds.
   - `rawCmykImageStaysSkippedUntilColorManagedDecode` — characterization pin for the CMYK block (byte-identical, colorspace kept).
3. `docs/audit/evidence-cmyk-downsample/` — R7 evidence: `R7-EVIDENCE.md`, `RED-fail-before.txt`, `NC-src-revert.patch`.

## Evidence summary (R7 contract) — all gates met

1. **Fail-before**: with implementation absent and pins present, the two behavior pins fail with the exact expected text (`filterIs(... "DCTDecode") returned FALSE`), all pre-existing slots pass. `evidence-cmyk-downsample/RED-fail-before.txt`.
2. **Negative control** (recorded ONCE): scoped revert of `PoDoFoBackend.cpp` only (`NC-src-revert.patch`), rebuild clean, exactly the 2 behavior pins RED, same failure text, guard pins and all other slots green; patch re-applied.
3. **Pass-after ×3**: serial ctest (no parallelism) over the six touched suites —
   `TestCompressJpegReencode | TestImageDedup | TestDedupSMask | TestOptimizeEstimate | TestCompressStripSanitize | TestCompressDialogHonesty` —
   run 1: 100% (6/6), run 2: 100% (6/6), run 3: 100% (6/6).

Builds: fresh-tree configure per runbook (vendored podofo 1.1.0 confirmed via `podofo_dir:…/third_party/podofo/install/...`; `libpodofo.dll` + `pdfium.dll` present, no fallback), full baseline build `[1085/1085]` no FAILED lines; post-change full-tree build `-k 0` → `[190/190]`, no FAILED targets (only the pre-existing third-party lua `-Wstringop-overflow` warnings); final incremental rebuild after a comment-only whitespace edit → ninja exit 0, fully up to date. ucrt64-first PATH used for every build/test invocation.

Gate tally: the ×3 contract runs were followed by three more file-captured clean runs and one final-tree confirmation run — **7 consecutive clean serial runs** of the six touched suites (`evidence-cmyk-downsample/PASS-after-*.txt`).

No existing test was weakened, removed, or modified; the pre-existing DCT-CMYK pin in `malformedImagesAreSkippedSafely` now double-serves as the guard for the honest CMYK block.

## Known limits (all deliberate, bounded-scope)

- Indexed scope: 8bpc indices only (1/2/4bpc unpacking not covered — consistent with the raw path's existing 8bpc-only contract); `hival > 255` or a shorter lookup skips untouched; non-RGB/Gray bases (incl. CMYK, and array bases like CalRGB/ICCBased) skip untouched.
- Indexed images re-encode expanded (RGB/Gray JPEG) — never re-indexed (rationale above, in-source comment + test comment).
- `/Indexed` with a media-filter chain or PNG/TIFF predictor still skips untouched (shared guards run before decode) — unchanged behavior, still pinned by `mediaFilterChainImageIsSkipped`/`predictorImageIsSkippedSafely`.
- CMYK (DCT or raw, named or indexed-base) remains skipped by design; lifting requires a color-managed decode the tree does not have.
- Not claimed: font subsetting, SMask downsampling (pre-existing upgrade path), size-estimate modeling of the new pass (estimates only claim what runs; this pass already had honest labeling).

## Fixture constraints discovered (for future lane archaeology)

- `PdfImage::SetData` reads RGB24 rows on 4-byte-aligned strides: fixture widths must be divisible by 4 (widths ≡ 2 mod 4 throw `PdfErrorCode::UnexpectedEOF` from `InputStream.cpp`); cost ~2 diagnostic cycles here.
- Binary palette bytes must be written with `PdfString::FromRaw(view, /*hex=*/true)` and read with `GetRawData()` — `GetString()` returns UTF-8-re-encoded text contents (a Latin-1-ish byte string round-trips through save as UTF-8, silently recoloring the palette), and a plain `std::string_view` ctor throws `utf8::invalid_utf8` on non-ASCII bytes. Both engine read path and test fixtures use the raw contract.

## Owner items

1. CMYK downsampling remains OPEN by design: vendor an ICC engine (lcms2) or take a product decision to accept naive Adobe-convention conversion; the three CMYK-adjacent pins are the gate.
2. Non-8bpc indexed indices (1/2/4bpc) are an easy follow-on if telemetry shows real-world demand.
3. (Pre-existing, untouched) `/SMask` downscale-with-base upgrade path remains noted in the Phase-1 comment.

## Commits

- `feat(compression): …` — implementation + tests (single commit; see git log on `feat/cmyk-downsample`).
- Evidence + lane report under `docs/audit/`.
