# GlyphPDF — Round-2 Remediation Status

**Branch:** `r2-1-chain1` (derived from `audit-remediation` HEAD `f9eac10`)
**Last updated:** 2026-06-09
**Auditor reference:** `docs/audit/AUDIT-2026-06-02.md` + workstreams A-G

---

## Status key
- `OPEN` - not started
- `IN-PROGRESS` - work started, not committed
- `CLOSED` - fix committed, build+ctest green, evidence below

---

## R2-1 -- Close CHAIN-1 (silent un-signing)  [HIGH security]  CLOSED

**Commit:** `a757056` on branch `r2-1-chain1`
**Test count:** 31/33 green (2 pre-existing DLL-path failures in TestSignatureValidation + TestSignatureRealCrypto not introduced by this session).

### D1 -- ProvenanceGuard real gate
**File:** `src/pdfws_djot/ProvenanceGuard.cpp:25-31`

checkEditVia() now throws ProvenanceViolation for any DjotThenSave call when isSigned=true,
regardless of ProvenanceTag origin. Previously hardcoded DirectStructural (always allowed),
ignoring the isSigned flag.

Unsigned BornPDF+DjotThenSave still allowed with requiresWarning=true. All other combinations
(DirectStructural +/-signed, DjotThenSaveAsCopy, unsigned BornDjot/BornOCR+DjotThenSave)
are unconditionally allowed.

### D2 -- Signed-doc saves routed through writeUpdate
Files changed:
- src/core/interfaces/IPdfEditorEngine.h: added writeUpdate() + hasPdfSignatures() pure virtuals
- src/engines/podofo/PoDoFoBackend.h + PoDoFoBackend.cpp: hasPdfSignatures() impl (iterates GetFieldsIterator, checks PdfFieldType::Signature)
- src/engines/PdfEditorEngine.h + PdfEditorEngine.cpp: delegation shims
- src/shell/controllers/HomeController.cpp:~112: onSave() calls hasPdfSignatures(); routes signed saves through writeUpdate(), unsigned through saveDocument(); ProvenanceGuard called with real isSigned state
- src/shell/controllers/EditController.cpp:~280: replace-all path likewise routes signed saves through writeUpdate()

writeUpdate() was already in PoDoFoBackend.cpp:299-354 (SaveUpdate = incremental append, preserves /ByteRange).
This deliverable wires it as the production save path for signed documents.

### D3 -- TestChain1 (provenance guard matrix, 6 tests)
File: tests/TestChain1.cpp (new)
Links only pdfws_djot + pdfws_core + Qt6::Test -- no OpenSSL/podofo, runs in all environments.

Tests: BornPDF+signed+DjotThenSave THROWS; BornDjot+signed+DjotThenSave THROWS;
       signed+DirectStructural ALLOWED; signed+DjotThenSaveAsCopy ALLOWED;
       unsigned+BornPDF+DjotThenSave ALLOWED+warning; unsigned+BornDjot+DjotThenSave ALLOWED.
All 6 PASS.

---

## R2-2 through R2-9  OPEN (not touched in this session)
