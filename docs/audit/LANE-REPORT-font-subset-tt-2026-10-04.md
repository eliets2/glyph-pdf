# LANE REPORT — font-subset-tt (Option A TrueType core)

**Date:** 2026-10-04 · **Worktree:** `D:/pdf/pdf-fs-tt` · **Branch:** `feat/font-subset-tt` (base `39c32506`)
**Scope:** owner-approved Option A — the keep-CID "blank-glyph" TrueType subsetter from
`docs/research/font-subsetting-plan-2026-10-01.md` (the decision-grade plan on this base; the
mission's `font-subsetting-plan-2026-10-02.md` filename does not exist in the tree — the -10-01
document IS the plan). TrueType/`FontFile2` only; CFF/Type1/signed-doc fonts skipped + disclosed.

---

## 1. What shipped

### 1.1 The subsetter — `src/engines/podofo/FontSubsetter.{h,cpp}` (new)

Two layers:

**Pure sfnt surgery** (no PoDoFo, directly unit-tested): `parseSfntDirectory` (rejects `ttcf`
collections and `OTTO`/CFF), `expandCompositeClosure` (transitive composite-component closure,
`.notdef` always kept), `blankUnusedGlyphs` (unused GIDs → zero-length `loca` spans, kept glyph
outline bytes verbatim, all other tables byte-identical, `loca` rebuilt in the same
`indexToLocFormat`, `head.checkSumAdjustment` per ISO 14496-6 — directory checksums taken over the
zeroed adjustment, adjustment written last, directory never touched again), `blankableGlyfBytes`
(exact estimator input), `cmapLookup` (formats 0/4/6/12, best-subtable scoring) and
`postNameToGid` (post v2.0 + the 258 Macintosh standard-order names, verified entry-for-entry
against fontTools' `standardGlyphOrder` — the industry reference for the same table).

**Document walk** (PoDoFo): one shared analysis (`analyzeFontPrograms`) behind both the pass and
the estimator. Enumerates every `/Type /Font` dict; classifies `CIDFontType2` (Identity /
absent / stream `/CIDToGIDMap`, 2-byte Identity-H/-V/-UCS2 code width) and simple `/TrueType`
(no `/Encoding` → built-in cmap path; `/Encoding /Differences` → every shown code must resolve
through a glyph NAME via the post table); walks every canvas — page /Contents (with /Pages-tree
resource inheritance), recursive Form XObjects, tiling patterns (`/PatternType 1`), transparency
`/ExtGState /SMask /G`, annotation `/AP /N /R /D`, Type3 `/CharProcs` — collecting shown codes
per font via `PdfContentStreamReader` (Tf tracking, Tj/TJ/'/" strings, raw byte codes).

**Safety posture (the plan's worst-failure-mode rule: never blank a used glyph):**
* Usage that cannot be PROVEN ⇒ skip + counted disclosure, never a guessed keep-set:
  unreadable/unparseable canvases, a Tf naming a font outside its resource scope, AcroForm `/DR`
  fonts (viewer-side appearance regeneration), CIDs beyond a `/CIDToGIDMap` stream, ragged
  (odd-byte) CID strings, named `/Encoding` simple fonts, unresolvable `/Differences` names,
  post v3.0 fonts (no names).
* One hostile font degrades to a counted skip; the pass never aborts the run (§9.13 F5
  containment pattern). `FontSubsetStats` carries the full skip ledger (CFF / Type1 / Type3 /
  unknown-usage / encoding-ambiguous / corrupt / unsupported stream params / already-subset /
  no-gain), logged per run.
* Commit-or-restore: a rewritten program replaces the stream only when the re-encoded (Flate)
  stream is smaller than the old one; otherwise the original bytes are restored exactly. Only
  `{absent, /FlateDecode}` old filters qualify (`/DecodeParms` and LZW-class streams skip —
  never half-restored). `/Length1` is rewritten to the new uncompressed length (32000 §9.8).
* GID/CID numbering is never reassigned ⇒ content streams, /ToUnicode, /W, /CIDToGIDMap stay
  byte-identical by construction; the pass touches ONLY `FontFile2` stream bytes.
* Already subset-prefixed (`AAAAAA+`) programs below 1 MB encoded are left untouched (dense
  PoDoFo subsets — churn without gain; plan §3.2 note).

### 1.2 Pass wiring — `PoDoFoBackend::optimizeDocument` Phase 3.5

Behind the existing `options.subsetFonts`. The signature-field inspection is hoisted once and
shared by Phases 3.5 and 4: signed documents skip both (the writeUpdate incremental path cannot
shrink — the plan's guard, mirroring the Phase-4 sweep).

### 1.3 Estimator honesty — `PoDoFoBackend::estimateOptimization`

The zeroed §9.13 branch is retired: `options.subsetFonts` now adds
`gp::fontsubset::estimateSubsetSavings(doc, signedDoc)` — nonzero only for FontFile2 programs the
pass will actually rewrite in an unsigned document (same eligibility walk as the pass), claiming
50 % of the raw blankable bytes (conservative flate haircut) capped at the programs' encoded
size, and nothing for signed documents. `TestOptimizeEstimate`'s old zero-claim pin still passes
unchanged — its standard-14 fixture has no embedded program, which is exactly the honest result.

### 1.4 Honesty surfaces — CompressDialog + CapabilityRegistry (plan §5.1)

* `probeCompressSubsetFonts`: `UnavailableBuild` → **Available** with the canonical scope
  disclosure (what runs: TrueType `/FontFile2`; what is left untouched: CFF, Type1, OpenType,
  unprovable-usage fonts, signed documents) and the plan's alternative wording ("Fonts in Type1
  or other unsupported programs are left untouched …").
* The retired R12 string ("does not implement font subsetting or unused-object removal") is
  deleted — both halves have shipped. `CompressDialog::unsupportedPassExplanation()` is
  re-scoped to the only remaining not-available-here compress capability (MRC, delegating to
  `mrcWhyNot()`), per the plan's "re-scope to MRC-only wording; fix the stale clause".
* "Subset fonts" checkbox: ENABLED, default UNCHECKED (it mutates font programs — explicit
  opt-in), tooltip/statusTip carry the scope disclosure via the new
  `CompressDialog::subsetScopeExplanation()` (byte-identical to the probe detail — one source of
  truth, pinned round-trip).
* `refreshEstimate()`/`onCompress()` honor the checkbox (`opts.subsetFonts = checked`); presets
  no longer disable or uncheck it — they never touch the user's choice (same contract as
  Remove-unused).

### 1.5 TestFontSubset suite (plan §5.2) — `tests/TestFontSubset.cpp` (new, 14 pins, all green)

* sfnt surgery (hermetic synthetic program): directory rejects `ttcf`/`OTTO`; blanking preserves
  numbering + kept-glyph bytes + untouched tables, rebuilds loca and the checksum identity;
  composite closure pulls component chains.
* Round-trip CIDFontType2 (hand-built Type0/Identity-H fixture over a real host TTF, full
  program embedded): estimate claims (honesty both ways), output smaller, program shrunk,
  content stream + /ToUnicode + /W byte-identical, used GIDs keep outlines, the largest unused
  glyph blanked, numGlyphs preserved, PoDoFo + pdfium reopen, **render-diff < 0.1 %** differing
  pixels, extracted text identical.
* `/CIDToGIDMap` stream variant honored (CIDs 1..3 → GIDs through the 2-byte map).
* Simple /TrueType with built-in cmap round-trip (render + extraction identity).
* Composite-glyph closure in-document: candidate selection requires a mapped, outlined,
  genuinely-composite glyph; drawn glyph survives AND the never-drawn component survives.
* Scope disclosure: CFF (`/FontFile3`) untouched + zero estimate claim; Type1 (`/FontFile`)
  untouched; corrupt (truncated) program skips safely, document intact; signed document (real
  signing fixtures + SignatureManager) claims no savings.
* Estimator honesty: named-`/Encoding` simple font claims nothing and is left untouched.
* Registration: `CMakeLists.txt` block beside the other compression tests (offscreen, serial
  house rules; `SOURCE_DIR` for the signing fixtures).

### 1.6 Contracted honesty-pin flips (each justified by the plan's test plan §5.1)

| Test | Flip | Why |
|---|---|---|
| `TestCompressDialogHonesty::unsupportedPassExplanationIsHonest` | asserts MRC-only wording, forbids the retired "font subsetting" text | plan: re-scope to MRC-only; both named passes are implemented |
| `TestCompressDialogHonesty::subsetFontsStaysDisabledUncheckedWithExplanation` | → `subsetFontsFollowsUserChoiceWithScopeDisclosure`: enabled, default unchecked, scope tooltip | plan: "becomes a follows-user-choice pin" |
| `TestCompressDialogHonesty::presetsNeverReEnableOrReCheckUnsupportedPasses` | → `presetsNeverTouchTheSubsetChoice`: no preset checks OR disables it; user choice survives | plan: keep the preset pin; the disable is the violation now |
| `TestCompressDialogHonesty::estimateOptionsNeverRequestUnsupportedPasses` | → `estimateOptionsHonorCheckbox`: `lastEstimateOpts.subsetFonts == checkbox` | plan: "drop the hardcoded-false pin" |
| `TestCapabilityRegistry::engineProbesR12PassesAreBuildUnavailable…` | → `engineProbesCompressPassesAreAvailableWithScopeDisclosure` | plan: probe → Available with scope detail + alternative |
| `TestExportPathBadge::compressDialogWordingRoundTripsThroughRegistry` | scope-disclosure round-trip (probe detail == dialog seam), MRC seam pinned to `mrcWhyNot` | the retired R12 string no longer exists |

Untouched: `sizeRowStaysLabeledAsEstimate`, the three completion-report pins,
`TestOptimizeEstimate`, all TestCompressJpegReencode / TestDedupSMask / TestOptimizeSignedGuard
pins (they set `subsetFonts=false` explicitly, which remains a valid user choice).

## 2. Deferred (disclosed, not silently missing)

* **CFF lane (follow-up)**: `/FontFile3` (Type1C / CIDFontType0C / OpenType) programs are counted
  (`skippedCffProgram`) and left byte-untouched; the estimator claims nothing for them. The plan's
  keep-numbering CFF recipe (CharStrings INDEX rewrite, subrs untouched) is the next lane.
* **Type1** (`/FontFile`) and **Type3** (no font program): counted skips, per the CMYK-downsampling
  residual precedent.
* **pdfium-side acceptance**: `/CIDSet` (PDF/A-1b) is never updated by the pass; blanking unused
  glyphs only over-reports CID presence (superset = valid). Noted as a residual for the CFF lane
  to revisit if a validator ever complains.
* Estimator cost: the honest (walker-based) subset estimate decodes eligible programs per
  estimate refresh — acceptable for the dialog, noted for any future batch-mode reuse.

## 3. Evidence summary

* Build: configure + `cmake --build build-rel --config Release` → **BUILD_RC=0** (record: two
  transient LTO link kills — one "Cannot open libpdfws_ui.a", one external process kill —
  resumed incrementally to green; final full pass `-k 0 -j 2` clean).
* `TestFontSubset`: **14 passed, 0 failed, 0 skipped** (twice — confirming run).
* Flipped/new honesty surfaces: `TestCompressDialogHonesty` **10/0/0**, `TestCapabilityRegistry`
  **22/0/1** (pre-existing environment skip), `TestExportPathBadge` **18/0/0**.
* Regression net: `ctest -R "TestFontSubset|TestCompress|TestOptimize|TestDedup|TestCapabilityRegistry|TestExportPathBadge"`
  → **9/9 passed**; plus `TestOptimizeEstimate 3/0`, `TestOptimizeSignedGuard 4/0`,
  `TestEngineSave 22/0`, `TestFindReplace 30/0`, `TestExcisionCorruption 5/0`, `TestRedaction
  22/0/2`, `TestSanitization 21/0`, `TestPdfACidSetSafety 6/0`, `TestLinkBookmarkRoundTrip 4/0`.
* Measured effect on the round-trip fixture (full `simhei.ttf` program embedded, 4 glyphs used):
  FontFile2 1,045,720 → 355,560 raw bytes (427,253 encoded bytes saved on the committed write),
  render-identical, extraction-identical.
* Bugs found and fixed during verification (all pre-merge, caught by the pins):
  1. `checkSumAdjustment` ordering (directory must not be rewritten after the adjustment).
  2. Encoded-stream snapshot skipped when the decoded program was already cached by the cmap
     path — made every rewrite "no-gain" and the restore write empty bytes.
  3. `encodingUnprovable` was classified but never enforced in the walker (WinAnsi simple fonts
     would have been "subset" on an unprovable mapping).
  4. Walker error-detection ran after the reader loop, where PoDoFo clears the flag — now fails
     fast on any `HasErrors` / `UnexpectedKeyword` item (this is what makes the
     mangled-high-byte-literal case a disclosed skip instead of a silent under-keep).

## 4. Files

* New: `src/engines/podofo/FontSubsetter.{h,cpp}`, `tests/TestFontSubset.cpp`,
  `docs/audit/LANE-REPORT-font-subset-tt-2026-10-04.md` (this file).
* Modified: `src/engines/podofo/PoDoFoBackend.cpp` (Phase 3.5 + estimator), `src/core/Capability.{h,cpp}`,
  `src/modes/CompressDialog.{h,cpp}`, `CMakeLists.txt` (sources + test target),
  `tests/TestCompressDialogHonesty.cpp`, `tests/TestCapabilityRegistry.cpp`,
  `tests/TestExportPathBadge.cpp`.

*R7: no other worktree touched; no push/merge/rebase; no CLAUDE.md/SECURITY.md; existing tests
flipped only where the plan's test plan contracts the flip (justified in §1.6 and the commits).*
