# GlyphPDF — Emergent Risk Analysis (R2-9 Capstone)

**Date:** 2026-06-10
**Branch:** `audit-remediation`
**Scope:** System-level risks that arise from the INTERACTION of individually-correct
components. Does NOT repeat per-component findings already catalogued in
`SEC-reverify.md`, `UX.md`, `CODE.md`, `REPO.md`. All four catastrophe chains
(CHAIN-1 through CHAIN-4) are taken as closed per R2-1 through R2-8; this
document asks what remains dangerous at the *junction* of those now-repaired parts.

---

## ER-1 — Incremental-Save + OCSP-Nonce Replay Window

**Title:** Incremental update preserves signed bytes but opens a time-window
for OCSP response replay across revision boundaries.

**Components (individually safe):**
- `PoDoFoBackend::writeUpdate` (R2-1 D2): correctly performs an incremental
  `SaveUpdate`, preserving all signed `/ByteRange` windows.
- `SignatureManager::validateSignatures`: correctly embeds OCSP responses from
  the DSS `/OCSPs` array and checks them for revocation status
  (`SignatureManager.cpp:1548-1572`).
- OCSP nonce insertion at signing time (`SignatureManager.cpp:374`): added to
  prevent replay.

**Scenario:**
A document is signed at time T0. The signer's certificate is revoked at T1.
A user opens the document at T2 > T1. The validation path reads the OCSP response
embedded in the DSS dictionary at signing time (T0), which pre-dates revocation.
Because `extractOcspFromDss` (`SignatureManager.cpp:598-650`) is a field-name
stub — it returns the *first* OCSP entry in the DSS regardless of whether it
matches the signer certificate's serial or issuer hash — an OCSP response for an
*unrelated* certificate (or an old response for the same certificate) may be
returned. The embedded revocation data is then accepted at face value. Result:
a document signed by a revoked certificate is presented as `ValidWithDSS`.

The nonce added at signing time (`OCSP_request_add1_nonce`) is never verified
on the validation side (`OCSP_check_nonce` is not called, `NF-6`). When the
document undergoes additional incremental updates (structural edits via
`writeUpdate`), new DSS revisions may be appended, but the old OCSP response
from revision 1 remains in all subsequent revisions and can be picked up by the
first-entry stub.

**Residual risk after R2:**
`NF-6` is documented but not closed. The OCSP certID-to-signer match is missing
(`extractOcspFromDss` is a stub). High confidence this is reachable with a test
CA: revoke the signer cert, append a second incremental revision, validate — the
first OCSP entry (pre-revocation) will be returned and the signature will show
`ValidWithDSS`.

**Falsification:**
Write a test that: (1) signs a document with the test CA, (2) revokes the signer
cert and embeds an OCSP response for a *different* cert in the DSS, (3) calls
`validateSignatures`. If `trustStatus != "Revoked"`, ER-1 is confirmed. Fix:
match `OCSP_id_issuer_cmp` + `ASN1_INTEGER_cmp(serial)` before accepting any
`OCSP_SINGLERESP`; implement per-field VRI correlation (M5).

---

## ER-2 — Redaction + Incremental Save Leaks Excised Bytes into Revision History

**Title:** Redaction physically neutralises image bytes in the current revision,
but incremental `SaveUpdate` preserves prior revisions, making excised content
recoverable by PDF revision extraction.

**Components (individually safe):**
- `PoDoFoBackend::neutralizeImageXObject` (R2-3): destructively overwrites the
  XObject stream with a 1x1 DeviceGray pixel, stripping Filter/SMask/Mask/Decode
  (`PoDoFoBackend.cpp:1031-1052`). This is physically correct within the current
  object generation.
- `PoDoFoBackend::writeUpdate` (R2-1): performs an incremental `SaveUpdate`
  which, by design, appends new object generations without rewriting the original
  bytes.

**Scenario:**
A user opens a signed document, applies redactions over sensitive image content,
then saves. The `onSave` path (`HomeController.cpp:148-150`) detects `isSigned`
and routes through `writeUpdate`. The `SaveUpdate` appends the neutralised
XObject (the 1x1 pixel) as a new indirect object generation, but the *original*
object bytes — containing the confidential image — remain in the file at their
original byte offsets, readable by any PDF revision extractor (e.g.,
`qpdf --show-xref`, `pdftk burst`). The digital signature over revision 1 is
preserved correctly (CHAIN-1 closed), but the redacted content is also preserved
correctly (it is part of the signed byte range).

This interaction is physically inevitable: incremental update and destructive
redaction are semantically incompatible on signed documents.

**Residual risk after R2:**
High. This is not a coding defect but an architectural conflict. GlyphPDF has
no user-visible warning that redacting a signed document via in-place save leaks
prior-revision content. The correct behaviour is to refuse in-place redaction of
signed documents and require a "Flatten + Save As new file" path that produces a
linearized output without revision history.

**Falsification:**
Sign a document containing a sensitive image. Apply redaction. Save (routes to
`writeUpdate`). Open the saved file with `qpdf --show-xref` or extract revision
1 bytes at the original `/ByteRange` offsets: the original image stream will be
readable at those offsets. If it is not, ER-2 is refuted.

---

## ER-3 — FEK Derivation + Incremental Update Breaks Multi-Recipient Re-Open

**Title:** AES encryption FEK derivation (`SHA256(seed||perms)`) is internally
consistent but does not follow the ISO 32000-2 `adbe.pkcs7.s5` algorithm;
combining this with incremental updates that append new `/Recipients` CMS
envelopes may leave existing recipients unable to decrypt after a second
encryption pass.

**Components (individually safe):**
- `PdfEditorEngine` FEK derivation (`src/engines/podofo/PdfEncryptPubSec.cpp:122-186`,
  `src/engines/PdfEditorEngine.cpp:776-818`): correctly AES-256-CBC encrypts
  streams with the FEK and round-trip-tests clean. But the FEK seed
  (`SHA256(sessionKey[24] || perms)`) is a bespoke derivation, not the literal
  `adbe.pkcs7.s5` seed extraction.
- `writeUpdate` incremental path: appends new object revisions without
  re-writing existing encrypted objects.

**Scenario:**
A document is encrypted for recipient A and recipient B (two CMS envelopes).
A user with recipient A's key opens the document, re-encrypts it (e.g., adds
a password), then saves incrementally. The incremental revision contains new
`/Encrypt` dictionary entries. Recipient B's original envelope, embedded in
revision 1's `/Recipients` array, was computed against the original FEK seed.
After re-encryption the new session key produces a different FEK. Recipient B
can no longer derive the correct FEK from their original envelope. Adobe Acrobat
(which implements the literal ISO 32000-2 algorithm) may accept the new envelope
for recipient B only if re-encryption also regenerated a CMS envelope for B —
which depends on whether the GlyphPDF UI exposes recipient management for
existing encrypted docs (currently unclear from the source).

**Residual risk after R2:**
Medium. The FEK derivation residual is explicitly documented in `SEC-reverify.md`
(A-01, Low residual). The interaction with incremental save elevates this from a
"Acrobat interop" concern to a potential silent data-loss scenario where recipient
B permanently loses access after another authorized user re-encrypts in place.
No test currently exercises multi-recipient re-encryption round-trips.

**Falsification:**
Create a document encrypted for two test recipients. Open as recipient A. Add a
metadata change and save (incremental). Attempt to open as recipient B. If
decryption fails, ER-3 is confirmed. Fix: prohibit in-place re-encryption of
multi-recipient documents; require Save As to a new file that generates fresh CMS
envelopes for all intended recipients.

---

## ER-4 — ProvenanceGuard Scope Gap: EditController SaveAs Bypasses the Gate

**Title:** The CHAIN-1 ProvenanceGuard blocks `DjotThenSave` on signed documents
in `HomeController::onSave`, but `EditController::onReplaceAllRequested` and the
`SaveAs` path bypass the guard entirely.

**Components (individually safe):**
- `ProvenanceGuard::checkEditVia` (`ProvenanceGuard.cpp:25-31`): correctly throws
  on `DjotThenSave + isSigned`.
- `HomeController::onSave` (`HomeController.cpp:125-140`): correctly calls the
  guard before saving.
- `EditController::onReplaceAllRequested` (`EditController.cpp:282-299`): routes
  to `writeUpdate` or `saveDocument` based on `isSigned`, correctly following
  the R2-2 silent-failure sweep.

**Scenario:**
A user opens a signed BornPDF. They use Find & Replace (not the Djot round-trip
path, but a structural text edit) and click "Replace All". `EditController`
routes through `writeUpdate` (correct, per R2-2). BUT: if the user then clicks
"Save As" (`HomeController::onSaveAs`, line 168-185), the ProvenanceGuard is
NOT called. `onSaveAs` calls `viewer->saveDocumentAs(fileName)` directly. That
method, depending on implementation, may use a full `saveDocument` rewrite on
the *current in-memory document state* — which, after a `writeUpdate`, has the
signature invalidated in its parsed object graph (PoDoFo re-parses the base plus
the incremental revision; after editing, the in-memory objects are the combined
state). The saved-as file will have an invalid signature with no user warning.

Additionally, the Djot export pipeline (`DjotThenSaveAsCopy`) is explicitly
listed as always safe in `ProvenanceGuard.cpp:18-19` because it writes a new
file. But if the "Save As" UI ever routes a Djot round-trip through the `onSaveAs`
handler (rather than the dedicated copy path), the guard will not catch it.

**Residual risk after R2:**
Medium. The `onSaveAs` path has no guard and no `isSigned` awareness. A user
can silently produce an invalid-signature file via Save As on a signed document
that has been edited in the current session. The file is not corrupted, but its
digital signature is silently broken.

**Falsification:**
Open a signed PDF. Make a structural edit. Choose File > Save As. Examine the
output with `TestSignatureRealCrypto::validateSignatures` — if the signature
reports `Invalid` or `UntrustedChain` without any user warning having been
shown, ER-4 is confirmed. Fix: add `isSigned` check and user warning/routing to
`onSaveAs`; alternatively block Save As on signed documents and offer Save As
Copy (unsigned flattened) or Save (incremental only).

---

## ER-5 — CMS Integrity Pass / Chain Pass Ordering Enables Downgrade Oracle

**Title:** `validateSignatures` runs a two-pass CMS verification (integrity-only
first, then full chain); the result of the first pass is stored in `integrityOk`
and used as a fallback oracle when the chain pass fails, enabling an attacker to
construct a forged-chain-but-valid-integrity signature that is surfaced to the
user as `UntrustedChain` (not `Invalid`).

**Components (individually safe):**
- First CMS pass (`SignatureManager.cpp:1353-1362`): `CMS_NOVERIFY` flag, no
  chain — tests only the cryptographic integrity of the signed data. Legitimate
  use: distinguish "the document was tampered" from "the cert is just untrusted".
- Second CMS pass (`SignatureManager.cpp:1365-1366`): full chain verification.
- Fallback logic (`SignatureManager.cpp:1534-1535`):
  `if (isUntrustedChain || integrityOk) { info.trustStatus = "UntrustedChain"; }`

**Scenario:**
An attacker obtains a self-signed or stolen certificate. They sign a document
(or re-sign an existing document) using a certificate whose chain does not
terminate in a trusted root. The first CMS pass (integrity-only) succeeds because
the signing key is valid for the CMS structure. The second pass fails (untrusted
chain). The fallback condition `integrityOk = true` fires, and the verdict is
`UntrustedChain` — displayed to the user as a yellow warning, not a red Invalid.

This is the intended and documented behavior for legitimate self-signed documents.
The emergence risk is that it becomes indistinguishable from a deliberate attack:
a sophisticated attacker who can produce a structurally valid CMS blob (easily
done with OpenSSL) over an arbitrarily modified document will always get
`UntrustedChain` from GlyphPDF's validator, not `Invalid`. The user sees a
warning, not a failure, and may proceed.

The interaction with the OCSP check (ER-1) amplifies this: if `trustStatus` is
`UntrustedChain` and an OCSP entry happens to exist in the DSS (placed there by
the original signer), the OCSP check may not even run (line 1546: check skips
`WeakKey`/`CertExpired`, but `UntrustedChain` is not excluded, so OCSP does
run). An attacker who controls the DSS can inject an OCSP response for the forged
cert from a rogue OCSP responder; if certID matching is absent (ER-1), the
injected response upgrades the verdict to `ValidWithDSS`.

**Residual risk after R2:**
The two-pass strategy is the correct approach for self-signed document workflows.
The residual risk is documentation and UI: the `UntrustedChain` status must be
visually distinct enough that users do not treat it as "valid but foreign cert",
and the OCSP certID matching (ER-1 fix) must be in place before DSS-embedded
OCSP can safely be used to upgrade `UntrustedChain` results. Without the ER-1
fix, ER-5 + ER-1 compose into a practical path from "untrusted cert" to
`ValidWithDSS`.

**Falsification:**
Create a forged CMS blob over a tampered document using a self-signed cert.
Inject a DSS `/OCSPs` entry for that cert from a local OCSP responder.
Validate with `SignatureManager::validateSignatures`. If `trustStatus == "ValidWithDSS"`
rather than `UntrustedChain` or `Invalid`, ER-5 + ER-1 compound is confirmed.
Fix path: require OCSP certID match (ER-1); never upgrade `UntrustedChain` to
`Valid*` solely on embedded OCSP without chain verification.

---

## Summary Table

| ID | Title | Components | Severity | Closed by R2? |
|----|-------|-----------|----------|----------------|
| ER-1 | OCSP nonce replay across incremental revisions | writeUpdate + extractOcspFromDss stub + OCSP nonce (NF-6) | High | No (NF-6 open) |
| ER-2 | Redaction + incremental save leaks excised bytes | neutralizeImageXObject + writeUpdate + PDF revision model | High | No (architectural) |
| ER-3 | FEK derivation + incremental update breaks multi-recipient | PdfEncryptPubSec FEK + writeUpdate | Medium | No (A-01 residual) |
| ER-4 | ProvenanceGuard scope gap: SaveAs bypasses gate | ProvenanceGuard (CHAIN-1) + onSaveAs + EditController | Medium | Partial (onSave gated; onSaveAs not) |
| ER-5 | CMS two-pass ordering enables downgrade oracle | CMS_NOVERIFY pass + integrityOk fallback + DSS OCSP (ER-1) | Medium (High with ER-1) | No (amplified by NF-6) |

**ER-1 and ER-5 compose into a practical attack path** and should be treated as
a single High finding until NF-6 (OCSP certID match + nonce check) is closed.

**ER-2 is architectural and cannot be fixed by a code patch alone** — it requires
a UX policy decision: block in-place redaction of signed documents and force the
user through a flatten+save-as-new workflow.

**ER-4 is the highest-leverage code fix**: adding `isSigned` detection and a user
warning/block to `onSaveAs` (`HomeController.cpp:168-185`) closes the scope gap.

---

*Analysis: Claude (audit) | Co-authored for R2-9 capstone | 2026-06-10*
