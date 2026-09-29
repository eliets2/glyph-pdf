PROGRAM-CONSOLIDATION-2026-09-25 §1.7c — the INV-1 pin's once-on-CI flake

CI failure (run 36486624682, CX-14 artifact excerpt in
ci-artifact-run36486624682-inv1-flake-excerpt.txt): TestSignatureRealCrypto
testOwnBltDssRevisionNotDowngraded got ValidWithUnsignedChanges with
"S-1: Shadow attack detected: 'Shadow attack: modified non-catalog
object 14 0 R'". Object 14 IS /Info (PoDoFo writes it as
`14 0 obj<</ModDate(D:...)>>` for the signing fixture). Every other CI run
and every local run passed.

Root cause (probe-proven, probe-second-crossing-shadow-attack.txt +
.probe/probe_inv17c.cpp): Private::buildDssDictionary appended the B-LT DSS
revision with a BARE PdfMemDocument::SaveUpdate. PoDoFo's save-time metadata
refresh stamps /Info's /ModDate at save time; when the append lands in a
LATER wall-clock second than the signing write, /Info's content actually
changes and SaveUpdate re-emits it — a redefined BASE object outside the
catalog, which the INV-1 shadow-attack scan must and does flag, downgrading
the app's own legitimate revision. Probe variants: append delayed >=1.1s
=> re-emission (+73-byte tail) + "modified non-catalog object 14 0 R"; the
same delayed append with PdfSaveOptions::NoMetadataUpdate => tail unchanged,
no shadow attack. Load on CI runners makes the >=1s gap a one-in-many event
— exactly the observed flake distribution (once in a session, never solo).
Not load-race, not environment: a deterministic wall-clock condition on the
production bytes.

Fix: buildDssDictionary saves with PdfSaveOptions::NoMetadataUpdate — the
DSS revision is DSS-ONLY by construction, which is the very invariant
isLegitimateIncrementalAppend (INV-1) enforces; the same practice the
expiry path already follows (PdfEditorEngine "save with NoMetadataUpdate").
The PDF/A export reset (PoDoFoBackend convertToPdfA) is unrelated and keeps
its behavior.

Deterministic pin: TestSignatureRealCrypto
::testDssAppendDoesNotReemitInfoAcrossSecondBoundary — sign via the
production path, cross a 1100ms wall-clock gap, run the PRODUCTION append
through the test-only seam SignatureManager::appendDssRevisionForTesting,
then require: exactly one /Info object in the byte stream (no re-emission)
and the end-to-end verdict stays Valid/ValidWithDSS with isValid=true and
hasDss=true.

Files:
- ci-artifact-run36486624682-inv1-flake-excerpt.txt  the CI failure text
- probe-second-crossing-shadow-attack.txt  delayed append => re-emission +
  shadow attack; delayed + NoMetadataUpdate => clean (mechanism proof)
- nc-reverted-second-boundary-pin.txt  fix scoped back to the bare
  SaveUpdate: the pin fails deterministically — signedModDateCount(output)
  = 2 (the append re-emitted /Info) in a 1.1s run
- pass-after-TestSignatureRealCrypto.txt  full suite with the fix:
  29P/0F/1skip (was 28P before this pin — the new slot added one)
- pass-after-x3-additional-totals.txt  two further full-suite runs,
  29P/0F/1skip each (3/3 with the pass-after above)
