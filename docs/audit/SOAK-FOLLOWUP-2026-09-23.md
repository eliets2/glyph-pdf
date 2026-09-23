# SOAK-FOLLOWUP-2026-09-23 — crash/security follow-up lane (feat/soak-followups, from feat/parity-glm @ 26c9a415)

Closes the two follow-ups the RESOAK-VERDICT-2026-09-22 left open (its §9 items 1–2):
the TestSanitization SegFault recurrences (§4.5) and the new
TestRedactionProof::proofFailsOnXmpSurvivor family (§4.6). Branch:
`feat/soak-followups` (cut from `feat/parity-glm` tip `26c9a415`). Runs are
Debug, offscreen, serial, QtTest `-o txt`; transcripts under `.context/`
(gitignored) are quoted inline here.

## ITEM 1 — TestSanitization SegFault recurrences: EXPLAINED by the pre-fix soak binary; closed

**Reconciliation.** The re-soak binary
(`build-soak/PdfWorkstation.exe`, sha `509da2c8…`, candidate `b17106a`) was
built BEFORE the E-2 fix `ed04426` ("scrub /Info and /Outlines in place,
CollectGarbage never frees a live wrapper's object") merged into
`feat/parity-glm` (~2026-09-21T00:45; re-soak started 2026-09-20T20:49).
The re-soak therefore exercised the PRE-FIX sanitize path for its whole
window. Its recurrences — 8× `PdfDataContainer::AssertMutable` + 1× NEW
`PdfName::GetRawData` (pass 14), all in `testSanitizeGeneratesUniqueTrailerID`,
all `0xc0000005` — are the documented E-2 use-after-free of the cached
`PdfDocument::m_Info` wrapper reaching DIFFERENT downstream frames depending
on heap state: the second `Save()` stamps `/Info/ModDate` through the freed
wrapper and dies either in `AssertMutable` (freed `m_Owner`) or, one frame
earlier/later, in `PdfName::GetRawData` (freed name storage compared/hashed
during the dictionary update). Both symbols are frames of the SAME E-2
defect; `GetRawData` is not a second bug.

**Post-fix verification (this branch, includes `ed04426`).**

- `TestSanitization` standalone ×10 → **10/10 PASS** (`Totals: 19 passed,
  0 failed` every run; rc=0 every run; zero crashes).
- `TestSanitizeTrailerUaf` (the E-2 pin) standalone ×10 → **10/10 PASS**
  (`Totals: 3 passed, 0 failed` every run).

**GetRawData exposure audit (sanitize / trailer-ID path).** Full grep of
`src/` + `tests/`: exactly two GlyphPDF-side `GetRawData` call sites, both
OUTSIDE the sanitize path —

1. `src/engines/podofo/PoDoFoBackend.cpp:1449` — the E-1 redaction width
   helper: `PdfString::FromRaw(str.GetRawData(), str.IsHex())`. The view is
   only read to build an INDEPENDENT buffer (E-1's own fix); the content
   stack's shared operand is never mutated.
2. `src/engines/podofo/GlyphAdvanceCalculator.cpp:42` — CID per-glyph
   fallback: `encodedStr.GetRawData()` consumed synchronously in-scope,
   read-only, never stored, never forces the lazy COW evaluation.
   (Test-only uses in TestCertEncryptPicker/TestEncryption read freshly
   loaded documents; not the sanitize path.)

`sanitizeDocumentContents` itself contains NO GlyphPDF-side `GetRawData`
call: the trailer /ID randomization block writes
`idArr[1] = PdfObject(PdfString::FromRaw(bufferview))` — an in-place array
element replacement (`PdfArray` is not a `PdfDocument`-cached wrapper) with
independent storage. Every `GetRawData` inside the crash path is
PoDoFo-internal name lookup/comparison (`PdfName.h:180-220` operators) over
the dictionary state the E-2 defect freed. **Verdict: PoDoFo-internal only
→ no second-shape fixture added; explained and closed.** `TestSanitizeTrailerUaf`
already pins the underlying class.

## ITEM 2 — TestRedactionProof::proofFailsOnXmpSurvivor: FLAKE (test-fixture defect), fixed; the proof machinery was never wrong

**Classification: FLAKE, not a real false PASS.** On every captured failure
the tampered file GENUINELY CONTAINED NO SECRET — the proof's PASS was
correct for the actual bytes; the test's tamper helper had silently lost its
own plant. No product/proof-scan change is warranted (the XMP surface and
the generic decoded-streams surface both already scan `catalog /Metadata`
and caught the secret on every run where it survived in the file).

**Root cause (mechanized, captured twice under load).**
`tamperXmp()` plants the XMP with
`GetCatalog().SetMetadataStreamValue(xmp)` then saves with DEFAULT options.
PoDoFo 1.1.0's default `Save` stamps `/Info/ModDate` with the current time
and re-synchronizes the `/Catalog/Metadata` packet from its metadata store
(upstream `PdfMetadata::TrySyncXMPMetadata`: rewrite fires when a packet
exists AND any setter actually changed the cached store — `SetTitle`-class
setters mark it dirty only when the value differs; PDF dates carry second
granularity). Whether the ModDate stamp "differs" depends on a SECOND
BOUNDARY between the redaction save (which wrote the loaded file's ModDate)
and the tamper save — hence:

- unloaded/standalone: saves land in the same second → plant survives →
  slot passes (observed ×10 standalone clean at this tip);
- under load: seconds straddled → packet REGENERATED from the store →
  plant discarded → slot fails ~1-in-4–5.

Captured evidence (6-way parallel-test load, DIAG build):

- failing run: tampered /Metadata = **719-byte PoDoFo-GENERATED packet**
  (`<?xpacket begin="\ufeff"`, indented, `xmlns:pdf` — the store's packet),
  `has-secret-raw=false`; attribution HEALTHY (entry `removedStrings =
  ("TopSecretAlpha bare secrets")`, status Verified, `runsBefore=2`);
  all 8 surfaces incl. XmpMetadata (items 1) and DecodedStreams (items 8)
  verdict Clean; sha of file on disk == sha verify read;
- contrast runs under identical load: **32 PASS** (planted packet verbatim,
  336 bytes, `begin=""`, secret present) vs **8 FAIL** (719-byte generated
  packet, secret absent) — `dest-key=0` in both, i.e. the /Metadata packet
  existed only because the tamper created it;
- standalone ×10 + full suite ×3: all green (pre-fix), matching the soak's
  low in-suite rate (10/719 = 1.4 %) and W2c's 1-in-4 load-sensitive
  observation.

**Fix (test-only).** `tests/TestRedactionProof.cpp` `tamperXmp`: save with
`PdfSaveOptions::NoMetadataUpdate` — PoDoFo's documented option for exactly
this ("…if you want to manually handle the manipulation of the XMP packet") —
plus a post-save reload asserting the plant survived (any future PoDoFo
behavior change now fails the tamper step loudly instead of surfacing as a
confusing proof PASS). DIAG scaffolding removed; the file's include set is
back to baseline.

**Fail-before / pass-after / negative control.**

- FAIL-BEFORE: pre-fix helper under load → 8/40 slot failures with the soak
  signature (`'!proof.proofPassed' returned FALSE`), incl. two full DIAG
  captures; fail-before ×10 standalone = silent (the defect needs the second
  boundary), which is why it hid from standalone repro lanes.
- PASS-AFTER (fix): standalone ×10 → 10/10; under identical 6-way load →
  **30/30** (`.context/fix-loaded.log`, `TOTAL-FAILS=0`).
- NEGATIVE CONTROL (scoped revert of the fix back to plain Save, rebuilt):
  under identical load → **1/25 failures** captured with the exact soak
  signature; fix restored, rebuilt → restore-loaded ×10 **10/10** green.

**Family green (serial, offscreen, post-fix build).** See §Runs below —
TestRedactionProof, TestSep13LeadRedactionProof, TestRedactTransaction,
TestExcisionCorruption, TestRotate270PageSpace, TestRedactSanitizeBundle.

## Runs (2026-09-23, machine-local times; all `-o txt` transcripts in `.context/`)

| Gate | Result |
|---|---|
| TestSanitization standalone ×10 (ITEM 1) | 10/10 rc=0, 19/19 each |
| TestSanitizeTrailerUaf standalone ×10 (ITEM 1) | 10/10 rc=0, 3/3 each |
| proofFailsOnXmpSurvivor standalone ×10 (pre-fix) | 10/10 pass (second-boundary blind) |
| full TestRedactionProof ×3 (pre-fix) | 21/21 each |
| proofFailsOnXmpSurvivor under 6-way load (pre-fix, contrast) | 32 pass / 8 fail (DIAG captured) |
| proofFailsOnXmpSurvivor standalone ×10 (post-fix) | 10/10 |
| proofFailsOnXmpSurvivor under identical load (post-fix) | **30/30** |
| NEGATIVE CONTROL: fix reverted, identical load | 1 fail / 25 (soak signature captured), restored |
| proofFailsOnXmpSurvivor after restore, under load | 10/10 |
| Family (post-fix, serial offscreen) | TestRedactionProof **21/21**, TestSep13LeadRedactionProof **11/11**, TestRedactTransaction **38/38**, TestExcisionCorruption **4/4**, TestRotate270PageSpace **7/7**, TestRedactSanitizeBundle **3/3** — all rc=0 |

## Residuals

- PoDoFo 1.1.0's Save-time XMP re-synchronization is a vendored-dependency
  behavior any future in-repo caller of `SetMetadataStreamValue` must know
  about; the fixed helper documents it in place. A repo-wide grep shows no
  other `SetMetadataStreamValue` caller outside PoDoFo's own PDF/A path,
  which goes through the sanctioned `SyncXMPMetadata` API.
- The soak families RESOAK §4.2–4.4/§9.4–9.5 (TestLaneScheduler budget,
  readExpiryDate roundtrip, FU-2 tempdir) are other lanes' items — unchanged.
