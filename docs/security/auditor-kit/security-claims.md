# GlyphPDF — Security Claims (for independent validation)

> **Deliverable:** M7-PROMPT-1 / D2
> **Date:** 2026-06-01
> **Audience:** Third-party security auditor
>
> Every claim below is **"to be independently validated."** For each, we point to
> the test(s) under `tests/` that currently exercise it and label the claim as:
>
> - **TESTS-VERIFIED** — an automated test asserts the behaviour today.
> - **PARTIALLY VERIFIED** — a test asserts *part* of the claim; gaps noted.
> - **ASPIRATIONAL** — the README states it but no test fully proves it; the
>   auditor should treat it as unproven.
>
> Source claims are quoted verbatim from `README.md` §Security and §Auto-Update.
> Test names map to `tests/*.cpp` (full inventory in `test-fixtures-list.md`).

---

## Claim 1 — "AES-256 password encryption"

- **Source:** README §Security.
- **Status:** **TESTS-VERIFIED.**
- **Implementation:** `PoDoFoBackend::encryptDocument` sets
  `PdfEncryptionAlgorithm::AESV3R6` (AES-256, revision 6).
- **Exercised by:** `TestEncryption` →
  `testAES256EncryptionApplied`, `testEmptyPasswordHandling` assert the saved
  document contains `/V 5` (AES-256) and `/R 6` (R6).
- **Auditor asks:** verify key derivation/permissions enforcement (owner vs user
  password), behaviour with empty passwords, and that permissions flags are
  actually enforced by readers — the test only checks the algorithm markers in
  output, not cryptographic strength of the KDF or real-world permission
  enforcement.

## Claim 2 — "Certificate-based encryption (multi-recipient)"

- **Source:** README §Security.
- **Status:** **PARTIALLY VERIFIED.**
- **Exercised by:** `TestEncryption` → `testCertificateEncryption` (builds an
  X.509 cert with OpenSSL, applies recipient encryption).
- **Auditor asks:** validate multi-recipient correctness, recipient key handling,
  and that only intended recipients can decrypt.

## Claim 3 — "PAdES B-LT/B-LTA digital signatures with DSS/VRI"

- **Source:** README §Security.
- **Status:** **PARTIALLY VERIFIED** (B-T and B-LT/DSS proven by real crypto;
  B-LTA archival timestamp path needs live TSA to fully prove).
- **Implementation:** `SignatureManager.cpp` — `PdfHashingAlgorithm::SHA256`,
  DSS dictionary build, VRI keyed by SHA-1 of `/Contents`, OCSP fetch+verify
  before DSS embedding, `/DocTimeStamp` for B-LTA. Code **degrades to B-T** and
  warns the caller when DSS/OCSP/TSA steps fail (honest fallback).
- **Exercised by:**
  - `TestSignatureRealCrypto` (real OpenSSL/PoDoFo) →
    `testBT_SignAndValidate` (sign + validate, integrity intact),
    `testBLT_VRIKeyMatchesOpenSSLSha1` (DSS present; VRI key matches OpenSSL CLI
    SHA-1 of `/Contents`).
  - `TestSignatureValidation` → field population, DSS presence/absence by level,
    trust-status population, byte-range integrity notes.
- **Auditor asks:** validate `ByteRange` covers the whole file (tamper after
  signing must fail); validate B-LTA archival timestamp against a real TSA;
  confirm OCSP/CRL freshness logic; confirm the B-T degradation is surfaced to
  the user (so the UI never falsely claims B-LT/B-LTA — code comments say callers
  MUST be informed).

## Claim 4 — "Secure redaction (content stream excision, never black rectangles)"

- **Source:** README §Security (verbatim).
- **Status:** **PARTIALLY VERIFIED — and materially overstated for non-text
  content. See `threat-model.md` §4 / T-RED-1.**
- **What is verified (text):** `TestRedaction` proves text excision —
  `testRedactionFormXObject` asserts `!data.contains("SECRET_FORM")` and a glyph
  test asserts "Redacted text must not be recoverable from content stream." Text
  show-operators within the rect are genuinely removed; struct-tree MCIDs scrubbed.
- **What is NOT verified (images/vectors):** **No test proves that raster-image
  or vector content under a redaction box is excised**, because the code does not
  excise it — it paints an opaque black rectangle over it. The "never black
  rectangles" claim is **false for images/vectors**: a black rectangle IS drawn,
  and the underlying image/vector bytes remain recoverable.
- **Auditor asks (priority):** (1) confirm the text-excision guarantee and its
  edge cases; (2) demonstrate recovery of image/vector content under a box (PoC);
  (3) recommend whether the claim should be narrowed to "text content" or the
  product should fail-closed on image/vector redaction until true excision ships.

## Claim 5 — "Document sanitization (15+ vectors)"

- **Source:** README §Security.
- **Status:** **PARTIALLY VERIFIED.**
- **Implementation:** `PoDoFoBackend::sanitizeDocument` removes ~20 vectors:
  `Info`, XMP `Metadata`, `PieceInfo`, `MarkInfo`, `OutputIntents`,
  `EmbeddedFiles`, `JavaScript`, `OpenAction`, `AA`, struct-tree
  `ActualText`/`Alt`/`E`, `OCProperties`, `Outlines`, Collection portfolio, and
  more; refuses in-place sanitize; regenerates a unique trailer ID.
- **Exercised by:** `TestSanitization` →
  `testMockSanitizeRemovesMetadata`, `testIntegrationSanitization`,
  `testSanitizeGeneratesUniqueTrailerID`, plus **injection-prevention** vectors:
  `testWatermarkInjectionIsPrevented`, `testAnnotationInjectionIsPrevented`,
  `testHeaderFooterInjectionIsPrevented`, `testBatesInjectionIsPrevented`,
  `testPdfEscapeLiteralStringInvariants` (PDF string-escape correctness — the
  "content-stream injection escape" claim from the prompt's acceptance list).
- **Auditor asks:** fuzz for vectors **not** on the list (annotation `/RC` rich
  text, form-field values, image EXIF/XMP, `/Launch` actions, attachments
  referenced indirectly). Confirm "15+" is a floor, not a ceiling.

## Claim 6 — "SHA-256 only for signature hashing"

- **Source:** README §Security.
- **Status:** **TESTS-VERIFIED (indirectly).**
- **Implementation:** `params.Hashing = PdfHashingAlgorithm::SHA256`.
- **Exercised by:** `TestSignatureRealCrypto` (real signatures validate; VRI key
  derivation uses SHA-1 of `/Contents` per PAdES spec — note: VRI **key** is
  SHA-1 by spec, the signature **digest** is SHA-256; do not conflate).
- **Auditor asks:** confirm no weaker digest is selectable; confirm SHA-1 only
  appears where PAdES mandates it (VRI keying), not in the signature digest.

## Claim 7 — "Auto-Update: SHA-256 verified downloads, user consent at every stage"

- **Source:** README §Auto-Update.
- **Status:** **ASPIRATIONAL for security purposes** (no dedicated security test
  found for the update path in the current `tests/` inventory).
- **Implementation:** `UpdateChecker` — async manifest fetch, SHA-256 verify,
  staged user consent.
- **Auditor asks:** verify the SHA-256 is delivered over an authenticated channel
  and is not attacker-controllable alongside the binary; verify TLS validation;
  verify there is no auto-execute without consent. **Treat as unproven until
  reviewed.**

## Claim 8 — "AI provider keys stored securely"

- **Source:** Implied by `CredentialManager` (Windows Credential Manager) usage;
  not an explicit README §Security bullet.
- **Status:** **ASPIRATIONAL / CONTRADICTED.** See `threat-model.md` T-API-2.
- **Implementation:** `CredentialManager::storeKey` uses Windows Credential
  Manager (`CredWriteW`, DPAPI-backed) at `CRED_PERSIST_LOCAL_MACHINE` scope.
  **However**, `AnthropicProvider::resolveKey` falls back to **plaintext
  `QSettings` (`ai/keyAnthropicCached`)** when no credential-manager entry exists.
- **Exercised by:** `TestAiProvider` (provider behaviour). No test asserts that
  keys are *never* persisted in plaintext.
- **Auditor asks:** confirm/deny the plaintext fallback path is reachable in
  shipping builds; recommend removing the QSettings fallback or encrypting it;
  reassess `CRED_PERSIST_LOCAL_MACHINE` vs current-user scope.

---

## Summary table

| # | Claim | Status | Primary test |
| :-- | :-- | :-- | :-- |
| 1 | AES-256 password encryption | TESTS-VERIFIED | `TestEncryption` |
| 2 | Certificate-based encryption | PARTIAL | `TestEncryption::testCertificateEncryption` |
| 3 | PAdES B-LT/B-LTA + DSS/VRI | PARTIAL | `TestSignatureRealCrypto`, `TestSignatureValidation` |
| 4 | Secure redaction "never black rectangles" | **PARTIAL — overstated for images/vectors (T-RED-1)** | `TestRedaction` |
| 5 | Sanitization (15+ vectors) | PARTIAL | `TestSanitization` |
| 6 | SHA-256 only for signature hashing | TESTS-VERIFIED (indirect) | `TestSignatureRealCrypto` |
| 7 | Auto-update SHA-256 + consent | ASPIRATIONAL (no security test) | — |
| 8 | AI key storage | ASPIRATIONAL / CONTRADICTED (plaintext fallback) | `TestAiProvider` |

**Headline for the auditor:** the two claims most likely to mislead a user are
**#4 (redaction "never black rectangles" — untrue for images/vectors)** and
**#8 (key storage — plaintext QSettings fallback)**. Both are flagged honestly
here for validation rather than presented as solved.
