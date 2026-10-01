# Font Subsetting for Compress — Investigation & Decision Plan (DESIGN ONLY)

**Date:** 2026-10-01 · **Worktree:** `D:/pdf/pdf-w2b-fontsubset` · **Branch:** `feat/font-subsetting-investigation` (base `a4cc1522`)
**Tasking:** PARITY-SCORECARD-2026-09-30.md §4 row 2 — *"Font subsetting for Compress … Converts the last inert
compression control into Adobe-parity function; large win on CJK/scanned-Unicode docs. Checkbox is already
honesty-gated so shipping it is purely additive. Effort L. Dispatch target: `PoDoFoBackend::optimizeDocument` new
pass + `CompressDialog` enable + `TestCompressDialogHonesty` flips"* (row read and confirmed at
`docs/audit/PARITY-SCORECARD-2026-09-30.md:307`; the underlying §3 row 90 is at `:239`).
No production code and no build files are touched by this document. This lane changes no product code.

**Sources (all read in this tree at `a4cc1522` unless marked):**
`src/modes/CompressDialog.{h,cpp}`, `src/core/Capability.{h,cpp}`, `src/core/interfaces/IPdfEditorEngine.h`,
`src/engines/podofo/PoDoFoBackend.{h,cpp}`, `tests/TestCompressDialogHonesty.cpp`, `tests/TestFindReplace.cpp`,
`third_party/podofo/install/` (vendored PoDoFo 1.1.0 headers + import library — this tree vendors the INSTALL
artifact only; sources are fetched by `scripts/bootstrap-vendor-deps.sh`), upstream PoDoFo `1.1.0` tag sources
(GitHub, fetched 2026-10-01, for semantics the headers alone do not show), host MSYS2 UCRT64 package state.

**Confidence legend:** TRUE (verified in this tree) · TRUE-UPSTREAM (verified in upstream 1.1.0 tag, not in this
tree — the vendored artifact is headers+binary) · UNCERTAIN (could not verify; do not plan on it without a probe).

---

## 0. Executive summary

The "Subset fonts" checkbox is real, visible, and permanently disabled: the capability does not exist in the
backend, and three honesty pins (`TestCompressDialogHonesty.cpp:64-73,104-122,126-144,149-169`) plus a
CapabilityRegistry probe (`Capability.cpp:561-570`) keep the UI from promising it. The vendored PoDoFo 1.1.0
**does contain a complete font-subsetting engine** — a TrueType sfnt subsetter (`FontTrueTypeSubset`) and an
Adobe-afdko-derived CFF subsetter (`SubsetFontCFF`), both exported from the shipped `libpodofo.dll` — **but its
public API cannot subset an existing document's fonts in place**: `PdfFont::InitImported` asserts
`!IsObjectLoaded()` (TRUE-UPSTREAM, `PdfFont.cpp` @ 1.1.0), subset CIDs are reassigned sequentially
(FSS-UTF-encoded, from 1), and there is no public call that takes font-file bytes plus a used-glyph set.
Therefore the decision is between:

- **(A) Zero new deps — RECOMMENDED — keep-CID "blank-glyph" subsetter written in this tree** (new pass in
  `PoDoFoBackend::optimizeDocument`): rewrite the embedded font **program** so unused glyphs lose their outlines
  while GID/CID numbering is preserved. Content streams, /ToUnicode, /W, /CIDToGIDMap all stay byte-identical —
  blast radius is confined to the font file itself, verifiable by render-diff. TrueType/`FontFile2`
  (incl. `CIDFontType2` — the CJK bulk) in v1; CFF/`FontFile3` second step; Type1 left untouched + disclosed
  (the CMYK-downsampling precedent, `PARITY-SCORECARD-2026-09-30.md:242`). **Effort M (TrueType-only v1), L
  (with CFF)** — the scorecard's L holds for full scope.
- **(B) New dep — HarfBuzz `hb-subset`** (`HB_SUBSET_FLAGS_RETAIN_GIDS`, `hb-subset.h:98` on the host): the
  industry subsetter (Chromium's) does A's job, correctly, for all formats. Cost: a *new runtime DLL chain* —
  `libharfbuzz-subset-0.dll` (1.7 MB) + `libharfbuzz-0.dll` (1.5 MB) **plus glib/pcre2/intl** in the MSYS2
  package (verified by `ldd`) — and an owner's no-new-deps exception under the quickjs-ng precedent
  (`docs/research/form-js-implementation-plan.md:43-50`; `CMakeLists.txt:216-222`). Effort M.
- **(C) Keep the honesty-gated disclosure** (status quo): zero cost, the win stays on the table, scorecard row
  stays OPEN.

Owner decision requested: **A vs B vs C.** A is feasible today with no dependency, build-system, or licensing
change; B is strictly better engineering per-yen but requires the exception and ships ~3-4 MB more DLLs; C is
the safe default. The remaining sections carry the evidence.

---

## 1. What the current "Subset fonts" control actually does (honesty-gated state)

**UI state — disabled, unchecked, explained** (`src/modes/CompressDialog.cpp:162-179`):

- The comment block at `CompressDialog.cpp:162-169` states the contract verbatim: *"the backend does not
  implement font subsetting (no subsetter in this build). The checkbox stays visible but disabled and
  unchecked, with the availability explanation as tooltip/status tip, so the UI never promises a pass that
  would not run."*
- `CompressDialog.cpp:170-178`: the wording comes from the CapabilityRegistry probe
  (`caps->query(gp::CapId::CompressSubsetFonts).whyNot`), falling back to
  `CompressDialog::unsupportedPassExplanation()` (`CompressDialog.h:28`, `CompressDialog.cpp:31-38`) when no
  registry is present (tests).
- `CompressDialog.cpp:174-176`: `setChecked(false); setEnabled(false);` — hardcoded, not preset-driven.

**Canonical wording** (`src/core/Capability.cpp:29-34`): *"Not available in this build: the compression engine
does not implement font subsetting or unused-object removal, so these passes would not run."*
Note: the "or unused-object removal" clause is **stale** — the sweep shipped in §9.13 (commit `21a387c`;
`CompressDialog.cpp:181-185` retires that half of the placeholder) — but the string is pinned byte-exact by
`TestCompressDialogHonesty` and was left untouched. Any lane that touches the wording must fix both halves.

**Registry probe — UnavailableBuild** (`src/core/Capability.cpp:561-570`,
registration `:683`; enum comment `src/core/Capability.h:44`):
`probeCompressSubsetFonts` returns `Availability::UnavailableBuild` with
`detail = "R12: no font subsetter is implemented in the compression backend."` and the alternative
*"Use image downsampling and deduplication instead — those passes run in this build."*

**Options are pinned false at both seams** (`src/modes/CompressDialog.cpp:493` in `refreshEstimate()`,
`:596` in `onCompress()`): `opts.subsetFonts = false;` — required because the struct default is
`subsetFonts = true` (`src/core/interfaces/IPdfEditorEngine.h:91-99`, field at `:96`).

**Estimator refuses to claim savings** (`src/engines/podofo/PoDoFoBackend.cpp:6734-6742`): the
`options.subsetFonts && est.fontCount > 0` branch exists but the savings line is commented out
(`savings += est.fontCount * 15000;` at `:6741`) with the §9.13 rationale *"Claiming savings here would
overstate the estimate on every real PDF, so it is zeroed out until the pass actually runs."* The estimate
still counts fonts (`:6710-6716`) and surfaces them in the dialog detail line (`CompressDialog.cpp:518-520`).

**Why inert:** there is no subsetter anywhere in the write path. `optimizeDocument`
(`PoDoFoBackend.cpp:6842-7167`) runs exactly: Phase 1 image downsample/JPEG re-encode (`:6851+`), Phase 2
image dedup (`:7034`), Phase 3 sanitize when stripMetadata (`:7108-7114`), Phase 4 trailer-rooted garbage
collection (`:7116-7159`), then `commitMutation(outputPath)` (`:7161`). No phase touches font objects. The
only font-adjacent code is the PDF/A-1b /CIDSet *repair* for PoDoFo's own writer-created subsets
(`PoDoFoBackend.cpp:3269-3278`) — it derives CID populations from /W of fonts PoDoFo itself embedded; it is
not a subsetter (scorecard row 90 already says this, `PARITY-SCORECARD-2026-09-30.md:239`).

**Pins that lock the state** (`tests/TestCompressDialogHonesty.cpp`):

| Pin | Location | Asserts |
|---|---|---|
| `unsupportedPassExplanationIsHonest` | `:93-102` | explanation contains "not implemented"/"not available" |
| `subsetFontsStaysDisabledUncheckedWithExplanation` | `:104-122` | not enabled, not checked, tooltip AND statusTip == `unsupportedPassExplanation()` (`:64-73`) |
| `presetsNeverReEnableOrReCheckUnsupportedPasses` | `:126-144` | after clicking every preset card, stateViolations is empty |
| `estimateOptionsNeverRequestUnsupportedPasses` | `:149-169` | `!engine->lastEstimateOpts.subsetFonts` (`:163-165`) |
| `sizeRowStaysLabeledAsEstimate` | `:173-185` | estimate vocabulary stays on the prediction row |

---

## 2. Can the vendored PoDoFo 1.1.0 do the pass natively?

**Short answer: it has the engine, but not the API this pass needs.**

### 2.1 What the vendored tree ships

`third_party/podofo/` contains only the **install artifact** (`install/bin/libpodofo.dll`,
`install/include/podofo/**`, `install/lib/libpodofo.dll.a` + CMake config) — sources are not in-tree;
`scripts/bootstrap-vendor-deps.sh:52-53` pins `PODOFO_VER="1.1.0"` and `install_podofo`
(`:97-120`) does `git clone --depth 1 --branch 1.1.0` → cmake → build → install → `rm -rf` the build tree.
Consequences: (a) the headers + import library are the authoritative API surface; (b) a PoDoFo *patch* is
possible today only as a step inserted into `install_podofo` between clone and configure — a build-provisioning
change, not an in-tree code change.

### 2.2 The engine exists — TRUE

Symbol table of the shipped import library (`nm third_party/podofo/install/lib/libpodofo.dll.a`, demangled,
run 2026-10-01):

- `PoDoFo::FontTrueTypeSubset::BuildFont(PdfFontMetrics const&, span<PdfCharGIDInfo const>, charbuff&)` plus
  `init/initTables/writeGlyphTable/writeLocaTable/writeHmtxTable/loadGlyphMetrics/readGlyphCompoundData/…` —
  a complete TrueType sfnt subsetter (glyf/loca/hmtx rewrite).
- `afdko::SubsetFontCFF(PdfFontMetrics const&, span<PdfCharGIDInfo const>, PdfCIDSystemInfo const&, charbuff&)`
  and `afdko::ConvertFontType1ToCFF`, `FT::ExtractCFFFont(FT_Face*, charbuff&)` — CFF subsetting derived from
  Adobe's afdko (bundled inside PoDoFo's build; afdko is Apache-2.0 — license OK for this Apache-2.0 project;
  UNCERTAIN on the exact vendored file list inside the 1.1.0 tarball, verify at integration).
- `PdfFont::AddSubsetCIDs / TryAddSubsetGID / SupportsSubsetting / embedFontSubset`,
  `PdfFontCIDTrueType::embedFontFileSubset`, `PdfFontCIDCFF::embedFontFileSubset`,
  `PdfFontSimple::embedFontSubset`, `PdfFontManager::GenerateSubsetPrefix`.

The create-path subsetting is battle-tested inside this very app: GlyphPDF's own PDF/A export relies on
PoDoFo emitting `FontFile3/CIDFontType0C` **subsets** for standard-14 fonts (the /CIDSet repair comment,
`PoDoFoBackend.cpp:3269-3278`). So the machinery works when PoDoFo *creates* fonts.

Public headers corroborate: `PdfFont.h:218-222` (`AddSubsetCIDs`), `:281` (`SupportsSubsetting`),
`:313-316` (`IsSubsettingEnabled`), `:406` (`InitImported(bool wantEmbed, bool wantSubset, bool isProxy)`),
`:418/:426-428/:458/:463-469` (subset bookkeeping); `PdfDeclarations.h:337-343` (`PdfFontCreateFlags`:
subsetting is **on by default** — you must pass `DontSubset = 2` to disable); `PdfFontCID.h:25,34`,
`PdfFontCIDTrueType.h:31` (`embedFontFileSubset`).

### 2.3 Why the engine is unreachable for this pass — TRUE (header) + TRUE-UPSTREAM (semantics)

1. **No public header exposes the subsetters.** `FontTrueTypeSubset` appears exactly once in the installed
   headers — as `PODOFO_PRIVATE_FRIEND(class FontTrueTypeSubset)` in
   `third_party/podofo/install/include/podofo/main/PdfFontMetrics.h:44`. `SubsetFontCFF` appears in none.
   Calling the exported symbols would require re-declaring internal classes against an unversioned ABI —
   rejected as a route (fragile against any PoDoFo rebuild).
2. **Object-loaded fonts can never enter the subset pipeline.** `PdfFont::InitImported` begins with
   `PODOFO_ASSERT(!IsObjectLoaded());` and sets
   `m_SubsettingEnabled = wantEmbed && wantSubset && SupportsSubsetting();`
   (TRUE-UPSTREAM, `PdfFont.cpp` @ tag 1.1.0, fetched 2026-10-01). `PdfFont::TryCreateFromObject`
   (`PdfFont.h:82-83`) — the only public way to wrap an *existing* document font — produces an object-loaded
   font, which can therefore never subset. Base `SupportsSubsetting()` unconditionally returns `false`
   (TRUE-UPSTREAM, same file).
3. **The one public bridge — `TryCreateProxyFont` (`PdfFont.h:119-120`) — reassigns codes.** It builds a
   *replacement* font ("rendering or font program embedding", header comment `:115-117`); upstream, it passes
   create-flags through and uses merged/system metrics (TRUE-UPSTREAM `PdfFont.cpp` @ 1.1.0; for fonts with an
   embedded file it goes through `metrics.CreateMergedMetrics` — exact embedded-vs-system resolution per font
   type UNCERTAIN, needs a compile probe before relying on it). Subset CIDs are allocated
   **sequentially from 1** (`tryAddSubsetGID`: `cid = PdfCID(m_SubsetCIDMap->size() + 1,
   FSSUTFEncode(size + 1))`, "CID 0 reserved for fallbacks") and char codes via
   `AddCharCodeSafe`/FSS-UTF (TRUE-UPSTREAM). **The original content-stream byte codes therefore do not
   survive** a proxy-font substitution.

### 2.4 The PoDoFo-native pass shape (if forced through public API) and its risk surface

Input → subset → write-back sketch (route "A1", listed for completeness, **not recommended**):

1. Enumerate fonts: walk every page's /Resources /Font **and every Form XObject / Tiling pattern /
   Type3 CharProcs / annotation appearance / AcroForm /DA** (missing any one corrupts that consumer).
2. `PdfFont::TryCreateFromObject` per font → `TryCreateProxyFont(default flags)` (subset-on) per font.
3. Walk every content stream with `PdfContentStreamReader` (already used 4× in this backend:
   `PoDoFoBackend.cpp:1401, 2131, 2358-2476, 4593`), collect each font's encoded strings;
   `AddSubsetCIDs` them into the proxy.
4. Re-encode every Tj/TJ/'/" string old-decode→unicode→proxy-encode and rewrite the stream
   (precedent for byte-level stream rewriting exists: the redaction walker at `:2358-2476`).
5. Point all /Font resources at the proxy fonts; save (full save for unsigned docs —
   `commitMutationImpl` `PoDoFoBackend.cpp:828`, no-signatures branch `:914-918` uses `saveDocument`;
   signed docs take SaveUpdate `:923+`, which **appends** and can never shrink — same reason Phase 4
   skips signed docs, `:7124-7130`; a subset pass must skip them identically).

Risk surface — why this route grades L→XL rather than L:

- **CID reassignment** (§2.3.3) forces step 4; every text-showing operator in the document is rewritten.
  Downstream consumers of those streams in this app — search, extraction, compare, redaction proofs
  (`RedactionProof.cpp`), text-width math that is deliberately byte-preserving on subset Tj strings
  (`PoDoFoBackend.cpp:1555-1564`) — all inherit the rewrite's fidelity.
- **ToUnicode degradation**: fonts without /ToUnicode (common in scanned/CMap-encoded CJK) decode through
  /Encoding CMaps; any codepoint the old font cannot decode cannot be handed to `AddSubsetCIDs`, so that
  font must be skipped and disclosed. /ActualText, ligature splits, and Type3 glyph procedures are edge cases.
- **Existing-user documents re-saved**: content rewrites change extraction/compare baselines for documents
  users already rely on; the /W arrays and identity mappings that the PDF/A /CIDSet repair logic depends on
  (`:3274-3278`) are rewritten by the proxy, so that interaction needs re-pinning.

### 2.5 The alternative PoDoFo-native route (route "A-patch")

Insert a small patch into `install_podofo` (clean slot between clone and configure,
`bootstrap-vendor-deps.sh:99-119`) exposing an internal keep-GID subset helper. Note, however, that upstream's
own engines **renumber** glyphs (BuildFont writes a dense font ordered by the `PdfCharGIDInfo` list — symbol
signatures above; `tryAddSubsetGID` renumbers CIDs) — so even the patch route cannot reuse them for a
keep-CID subset without new upstream-side code. A patch that merely *exposes* `BuildFont` still leaves this
tree with renumbered fonts and thus the content-rewrite problem of §2.4. This kills the "cheap patch" hope:
the keep-CID property has to be implemented either in-tree (route A below) or upstream (new API — slow,
outside our control).

---

## 3. In-tree options vs external options

### 3.1 What the tree already vendors / links (TRUE)

| Component | State | Subsetting capability |
|---|---|---|
| PoDoFo 1.1.0 (vendored install) | linked (`CMakeLists.txt:150-189`) | engine only, not reachable for existing docs (§2) |
| FreeType | **already a runtime dep of the app** — `libpodofo.dll` links `libfreetype-6.dll` (verified `ldd`, 2026-10-01); `podofo-config.cmake:18` `find_dependency(Freetype)`; host UCRT64 package present | **No subsetter.** FreeType is a font-access/rasterizer library; it can read/measure (`PdfFontMetricsFreetype`), map charcode→GID via `FT_Get_Char_Index`/`FT_Load_Glyph`, but ships no subset writer |
| HarfBuzz | **not** a project dep; present on the host only (`/c/msys64/ucrt64/lib/pkgconfig/harfbuzz.pc`; `hb-subset.h` with `hb_subset_or_fail` `:262`, `HB_SUBSET_FLAGS_RETAIN_GIDS` `:98`) | **Yes — full subsetter**, TrueType+CFF+OT, GID-preserving mode |
| quickjs-ng 0.15.1 | vendored-in via MSYS2 package pin (`CMakeLists.txt:216-222, 650-720`) | irrelevant (JS engine) |
| pdfium | vendored runtime for rendering (`third_party/pdfium`, `CMakeLists.txt:227, 643-647`) | contains a subsetter internally but exposes none publicly; render-only for us |
| jbig2enc, lua-5.4, djot | vendored | irrelevant |

### 3.2 Route A — zero new deps: in-tree keep-CID "blank-glyph" subsetter (RECOMMENDED)

Insight: a Compress pass does not need the densest possible subset — it needs the **biggest safe win with
zero content-stream fallout**. Preserving GID/CID numbering achieves that:

- **Used-glyph collection** (new code, reuses existing walker idiom): for `CIDFontType2` with
  `/CIDToGIDMap /Identity` (the dominant CJK case) the 2-byte codes in `Tj/TJ/'/"` strings **are** GIDs —
  collect them by walking content streams per font resource (PdfContentStreamReader, as at
  `PoDoFoBackend.cpp:2358-2476`). For simple /TrueType fonts, map charcode→GID via the font's cmap — FreeType
  (`FT_Get_Char_Index`) through `/Encoding` + `/Differences`, or in-tree cmap parsing. Composite-glyph
  closure: keep any GID referenced by a kept composite glyph's `glyf` component chain (transitive walk).
- **Program rewrite** (new code, ~600-900 lines total with tests): parse the sfnt table directory; rewrite
  `glyf` (unused GIDs → empty glyph: `loca[i] == loca[i+1]`, zero-length outline) and rebuild `loca`; keep
  every other table byte-identical (`cmap`, `hmtx`, `maxp.numGlyphs` unchanged, `head` checksum fixed).
  Conservative v1 prunes nothing else. Expected shrink: glyph outline data dominates CJK `FontFile2`
  programs (tens of MB → hundreds of KB when a few hundred glyphs of 20k are used).
- **CFF step 2** (`FontFile3` /Type1C and CIDFontType0C): same keep-numbering trick — rewrite the CharStrings
  INDEX replacing unused charstrings with bare `endchar`, recompute INDEX offsets, keep **all** global/local
  subrs and FDArray/FDSelect untouched (avoids subroutine-closure analysis; subrs are small relative to CJK
  charstrings). GID↔CID charset stays valid because numbering is untouched.
- **Write-back**: replace only the `FontFile2`/`FontFile3` stream bytes; /W, /ToUnicode, /CIDToGIDMap,
  FontDescriptor keys other than the file stream untouched → every other consumer (pdfium rendering,
  extraction, compare, redaction proofs) sees identical structure. Skip: Type1 `FontFile` (rare, small),
  Type3 (no font program), signed docs (incremental-update path cannot shrink — same guard as Phase 4,
  `PoDoFoBackend.cpp:7124-7130`), fonts already subset-prefixed (`AAAAAA+`) unless they still exceed a
  threshold, undecodable/corrupt programs (skip + disclose).
- **Disclosure scope in v1**: TrueType/`FontFile2` + CFF/`FontFile3` covered; Type1 uncovered — the exact
  deliberate-skip pattern the CMYK downsampling residual uses (`PARITY-SCORECARD-2026-09-30.md:242`).
- Effort: **M** for the TrueType v1 (walker + sfnt rewriter + honesty flips + pins), **L** with CFF and
  cmap-based simple fonts. No build-system change, no license change, no new DLL.
- Risk notes: correctness bugs here produce visible glyph damage (blanked glyph that was actually used),
  so the pin suite must include a **render-diff** (pdfium re-render before/after, raster tolerance) and a
  composite-glyph fixture. Hostile-input hardening follows the Phase-1 containment pattern
  (`PoDoFoBackend.cpp:6911-6914`: one bad font degrades to "skip this font", never aborts the run).
- Honest impact note: the scorecard's "scanned-Unicode docs" framing only partially holds — scanned PDFs
  carry small invisible-OCR fonts (little to shrink); the large win is **CJK Office/LaTeX exports with full
  embedded `FontFile2` programs** (frequently 5-30 MB each). The plan targets exactly that case first.

### 3.3 Route B — new dep: HarfBuzz hb-subset

- API: `hb_subset_or_fail(hb_face_t*, hb_subset_input_t*)` (`hb-subset.h:262`) with
  `HB_SUBSET_FLAGS_RETAIN_GIDS` (`hb-subset.h:98`, "glyph indices will not be modified") — same keep-CID
  design as A, but implemented by the library Chromium ships, covering TrueType, CFF/OT, and the table
  bookkeeping (cmap/hmtx/vmtx/MVAR pruning) A hand-rolls.
- License: HarfBuzz is **Old MIT / MIT** (permissive; UNCERTAIN on the exact LICENSE text of the current
  version — verify at integration like the form-js plan did).
- Cost: two new runtime DLLs **plus their chain**: `libharfbuzz-subset-0.dll` 1.74 MB + `libharfbuzz-0.dll`
  1.55 MB, and the MSYS2 package links `libglib-2.0-0.dll`, `libpcre2-8`, `libintl-8` on top (verified
  `ldd`, 2026-10-01; freetype/png/zlib/bz2 already ship via podofo). Net new: **~3.5-4.5 MB** installed,
  unless a glib-free HarfBuzz build is vendored (HarfBuzz builds without glib upstream; the UCRT64 package
  as shipped links it — UNCERTAIN whether a configure switch exists in that package).
- Process cost: an owner's no-new-deps exception under the quickjs-ng precedent — the research note requests,
  the owner authorizes (`docs/research/form-js-implementation-plan.md:43-50`, "RQ10 forbids research notes
  from self-authorizing it"); then the enforced-pin discipline (`CMakeLists.txt:650-720`: pinned version,
  header-advertised-version check, goldens) and `bootstrap-vendor-deps.sh` staging like pdfium's
  hash-pinned flow (`:122+`).
- Effort: **M** (integration is small; the pin/vendoring/licensing apparatus is the work).

### 3.4 Route C — keep honesty-gated disclosure

Zero cost; the checkbox stays disabled with the (stale-clause) explanation; scorecard row stays OPEN and the
"last inert compression control" framing in §4 row 2 remains accurate. Choose C if neither A's M/L
implementation budget nor B's dependency exception is acceptable now.

---

## 4. Recommendation (owner-decision framing)

**Recommend A (zero new deps, keep-CID blank-glyph subsetter, TrueType/`FontFile2`-first), with B
(hb-subset + RETAIN_GIDS) pre-named as the fallback if the owner prefers proven engine code over ~600-900
in-tree lines, and C as the do-nothing baseline.**

Why A over B: A ships no new dependency, no licensing question, no new DLL chain, and no bootstrap/pin
apparatus; its blast radius (font-program bytes only) is strictly smaller than the PoDoFo-proxy route's
(whole-document content rewrites, §2.4) and is independently verifiable by render-diff pins. The engineering
risk of A is concentrated in sfnt/CFF table surgery, which is bounded, fixture-testable, and degrades safely
(skip + disclose per font). Why A is still honest about B: hb-subset does the same job with better-tested
code and broader table coverage; if the owner grants the exception, B replaces A's rewriter wholesale — the
pass plumbing, honesty flips, and pins in §5 are identical for both.

Effort vs scorecard: scorecard says L for the row; A's v1 (TrueType only) is **M**, A full (with CFF) is
**L** — consistent once scope is explicit. B is **M** + the exception. C is **S** (fix the stale
"unused-object removal" clause of the pinned wording only).

---

## 5. Test plan (identical skeleton for A or B; C needs only the wording fix)

1. **Honesty flips in `TestCompressDialogHonesty.cpp`** (dispatch target confirmed):
   - `subsetFontsStaysDisabledUncheckedWithExplanation` (`:104-122`) → becomes a follows-user-choice pin:
     checkbox **enabled**, default **unchecked**, tooltip discloses *scope* (TrueType/CFF covered, Type1
     skipped; signed docs skipped) instead of unavailability.
   - `stateViolations` (`:55-77`) → remove the disabled/unchecked violations, add "checkbox enabled but
     engine never subsets" and "options do not honor checkbox" violations.
   - `estimateOptionsNeverRequestUnsupportedPasses` (`:149-169`) → `lastEstimateOpts.subsetFonts` must
     **equal** the checkbox state (drop the hardcoded-false pin at `:163-165`).
   - `presetsNeverReEnableOrReCheckUnsupportedPasses` (`:126-144`) → keep: no preset may silently check it.
   - `unsupportedPassExplanationIsHonest` (`:93-102`) → re-scope to MRC-only wording; fix the stale
     "or unused-object removal" clause (`Capability.cpp:29-34`).
   - Registry: `probeCompressSubsetFonts` (`Capability.cpp:561-570`) → `Available` with scope detail +
     alternative ("Fonts in Type1/unsupported programs are left untouched"); `Capability.h:44` comment
     updated; `SupportBundle.cpp:66` string unchanged.
2. **Backend pins (new `TestFontSubset.cpp`), fixture style follows `TestCompressJpegReencode.cpp`
   (synthesized in-test documents) + the `C:/Windows/Fonts/…` with `QSKIP` precedent
   (`TestFindReplace.cpp:109, 328`):**
   - **Round-trip CIDFontType2**: build a doc embedding a real system CJK-capable TTF via PoDoFo's painter
     with `DontSubset` (full program embedded); run `optimizeDocument` with `subsetFonts=true`; assert:
     output smaller; `FontFile2` stream bytes shrunk; /ToUnicode, /W, /CIDToGIDMap byte-identical; content
     streams byte-identical; document reopens in PoDoFo AND renders in pdfium without error; extracted text
     identical before/after.
   - **Render-diff pin**: pdfium render of the text page before vs after within pixel tolerance (blanked
     used-glyph bug class).
   - **Composite-glyph closure fixture**: draw a string hitting composite glyphs (e.g. accented Latin);
     assert component GIDs survive (render-diff covers it; an explicit GID-set assertion documents it).
   - **Scope disclosure pins**: a `FontFile3` (CFF) doc in v1-TrueType scope → untouched, estimate does not
     claim its savings; a Type1 doc → untouched; corrupt font program → skipped, run succeeds.
   - **Signed doc**: fixture with signature field → subset pass skipped (mirrors Phase 4 guard,
     `PoDoFoBackend.cpp:7124-7130`), report still measured (`TestCompressDialogHonesty.cpp:194-244` pins
     the report format).
   - **Estimator honesty**: `estimateOptimization` claims subset savings **only** for fonts the pass will
     actually rewrite (the zeroed-out branch at `PoDoFoBackend.cpp:6734-6742` becomes real, with the same
     signed-doc conservatism as `:6744-6768`).
3. **Regression net**: full `TestCompress*`, `TestOptimizeEstimate`, `TestEngineSave`,
   `TestPdfACidSetSafety` (CIDFontType2 fixture dicts at `tests/TestPdfACidSetSafety.cpp:65-66`),
   `TestLinkBookmarkRoundTrip`; serial-gate protocol per house rules.

## 6. Residuals & uncertainties

- UNCERTAIN: exact boolean of `PdfFontCID::SupportsSubsetting()` / `PdfFontType3::SupportsSubsetting()` at
  1.1.0 (overrides exist — `PdfFontCID.h:25`, `PdfFontType3.h:42` — irrelevant to route A).
- UNCERTAIN: whether the UCRT64 harfbuzz package can be used glib-free (route B sizing assumes it cannot).
- UNCERTAIN: the file list/license headers of the afdko subset sources inside the PoDoFo 1.1.0 tarball
  (route A does not use them; route B does not either — only a hypothetical A-patch would; verify then).
- The pinned explanation string carries the stale "unused-object removal" clause (`Capability.cpp:29-34`)
  — fix opportunistically in whichever route lands.
- Route A v1 leaves Type1 (`FontFile`) fonts unsubsetted by design; disclose in tooltip + probe detail.

---

*Prepared by the wave-2b font-subsetting investigation lane, 2026-10-01. Committed on
`feat/font-subsetting-investigation` @ base `a4cc1522`. No product code changed.*
