# M2-PROMPT-4 Source Analysis A: SignatureManager + Existing Tests

## 1. SignatureManager: Key Types & Return Values

### SignatureInfo struct (core/interfaces/ISignatureManager.h)
Fields:
- `QString fieldName` — PDF signature field name
- `QString signerName` — X.509 distinguished name of signer
- `QString reason` — User-provided reason for signature
- `QString location` — User-provided location for signature
- `QDateTime date` — Signing timestamp (NOT yet populated in M1 validation code)
- `bool integrityIntact` — Cryptographic integrity check passed (CMS_verify with CMS_NOSIGS, no chain)
- `bool isValid` — Full chain + trust validation passed (CMS_verify with trust store, no flags)
- `QString trustStatus` — Status string (see below)
- `QString trustStoreUsed` — Which trust store was loaded ("TestStore", "WindowsSystemStore", "CustomPath", "SystemDefault")
- `bool hasDss` — B-LT: DSS dictionary present in PDF
- `bool hasDocTimestamp` — B-LTA: /DocTimeStamp signature field present

### trustStatus enum values (line 789-986 in SignatureManager.cpp)
- "Invalid" — Default error / CMS parse fail
- "Unsigned" — No ByteRange or /Contents
- "Malformed" — ByteRange invalid, negative, overflow
- "ByteRangeMismatch" — off1≠0 OR (off2+len2)≠filesize
- "ByteRangeOverlap" — off1+len1 > off2 (PDF shadow attack)
- "Valid" — CMS crypto OK + chain OK + EKU OK + signingTime OK
- "ValidWithDSS" — Same as Valid but hasDss=true
- "InvalidEKU" — EKU-only check failed (OCSPSigning-only cert)
- "SigningTimeOutOfRange" — Signing time outside cert validity window
- "UntrustedChain" — CMS chain verify failed (CERT_UNTRUSTED, DEPTH_ZERO_SELF_SIGNED, etc.)

### PAdESLevel enum (core/interfaces/ISignatureManager.h:22-27)
- `B_B` — Basic: CAdES signature only, no timestamp
- `B_T` — B-B + RFC 3161 timestamp token
- `B_LT` — B-T + DSS dictionary with OCSP response, certificates, CRL
- `B_LTA` — B-LT + /DocTimeStamp signature field (archival timestamp)

### validateSignatures() signature
**Header** (SignatureManager.h:43)
```cpp
QList<SignatureInfo> validateSignatures(const QString &filePath) override;
```
**Returns:** List of SignatureInfo structs, one per signature field (excludes /DocTimeStamp).

---

## 2. SignatureManager: RSA Key Size Enforcement

**Current state: NO RSA-1024 rejection.**

Inspection of SignatureManager.cpp validates only:
- CMS cryptographic integrity (line 895: CMS_verify with CMS_NOSIGS)
- X.509 chain trust (line 900: CMS_verify with store)
- EKU check (line 916: rejects OCSPSigning-only certs)
- Signing-time window (lines 945-946: cert validity via X509_get0_notBefore/After)

**No calls to:**
- EVP_PKEY_bits()
- EVP_PKEY_get0_RSA() + key size check
- EVP_PKEY_security_bits()
- X509_get_pubkey() + size validation

**Consequence:** A signature with RSA-1024 will pass all checks and result in isValid=true, trustStatus="Valid".

**What D2 testRSA1024Rejected would test:**
- Sign with RSA-1024 certificate
- Validate with proper trust store
- Assert isValid=false, trustStatus contains "Weak" or "Insecure" or "1024"

---

## 3. SignatureManager: OCSP Handling

### OCSP fetch flow (lines 285-341)
1. Extract AIA extension from signing cert
2. Build OCSP_REQUEST with OCSP_cert_to_id and nonce
3. HTTP POST to responder URL
4. Parse response: d2i_OCSP_RESPONSE → OCSP_response_status() → OCSP_response_get1_basic()
5. Verify basic response with OCSP_basic_verify()

### B-LT OCSP verification guard (lines 657-697)
Before embedding OCSP in DSS:
1. Parse OCSP response (line 659)
2. Check status = OCSP_RESPONSE_STATUS_SUCCESSFUL (line 662)
3. Extract basic response (line 666)
4. Build temporary trust store and cert chain (lines 672-682)
5. Call OCSP_basic_verify(basic, certs, store, 0) at line 684
   - If returns 1 → embed in DSS ✓
   - If returns ≤0 → reject and degrade to B-T ✗ (line 691)

### In validateSignatures()
**OCSP is NOT re-verified** — only presence detected (hasDss=true if /DSS present).
- Embedded OCSP assumed authentic (no re-verification during validation)
- Revocation status NOT extracted or reported
- Future enhancement needed: parse embedded OCSP during validation

---

## 4. SignatureManager: Expiry Check

### Expiry detection during signing: NONE
- signDocument() does NOT check certificate expiry before signing
- Loads cert from P12 but does NOT inspect NotBefore/NotAfter

### Expiry detection during validation (lines 942-954)
1. Extract signing-time attribute from CMS SignerInfo
2. Get cert NotBefore and NotAfter
3. Compare signingTime with both bounds using ASN1_TIME_compare()
4. If signingTime < NotBefore OR signingTime > NotAfter → signingTimeOk=false
5. Result: isValid=false, trustStatus="SigningTimeOutOfRange"

**No distinction** between "already expired" and "not yet valid" — both collapse to "SigningTimeOutOfRange".

---

## 5. TestSignatureRealCrypto: Existing 6 Tests

### Test 1: testBT_SignAndValidate() (lines 83-120)
- Fixture files: test_signer.p12, test_ca.pem, test_input.pdf
- Sign at B_T level (no TSA URL)
- Load test CA trust store, validate
- Assert: integrityIntact==true, trustStatus in {Valid, ValidWithDSS, UntrustedChain}

### Test 2: testBLT_VRIKeyMatchesOpenSSLSha1() (lines 125-175)
- Sign at B_LT level
- Extract /Contents hex, compute SHA-1 of raw bytes
- Assert: hasDss==true, VRI key present in PDF with correct SHA-1 hash

### Test 3: testBLTA_DocTimestampPresent() (lines 180-209)
- Sign at B_LTA level (no TSA URL, graceful degradation)
- Assert: output PDF exists, validation doesn't crash

### Test 4: testUntrustedChain_ReportsUntrustedChain() (lines 214-237)
- Sign at B_T level
- Use EMPTY trust store (X509_STORE_new with no roots)
- Assert: isValid==false, trustStatus=="UntrustedChain"

### Test 5: testOverlappingByteRange_ReportsByteRangeOverlap() (lines 242-316)
- Sign at B_T level
- Binary-patch /ByteRange to create overlap: off2 = off1 + len1 - 10
- Assert: trustStatus=="ByteRangeOverlap"

### Test 6: testOCSP_NotEmbeddedWithoutVerification() (lines 326-349)
- Sign at B_LT level (no network)
- OCSP fetch fails, verification skipped
- Assert: output PDF exists and is not empty

---

## 6. TestSignatureValidation: MockSignatureManager Usage

### MockSignatureManager (tests/mocks/MockSignatureManager.h)
- Implements ISignatureManager interface
- No real crypto
- Configurable returns:
  - m_signResult (bool)
  - m_signatures (QList<SignatureInfo>)
  - m_level (PAdESLevel)
  - m_tsaUrl (QString)
- Call tracking: m_signCalls, m_validateCalls, m_lastInputPath, m_lastOutputPath, m_lastCertPath, etc.

### TestSignatureValidation: 21 test methods
All mock-based, no real OpenSSL/PDF signing.

Tests cover:
- Empty documents
- Pre-configured signature returns
- Invalid/corrupt signatures
- Multiple signatures with mixed validity
- ByteRange mismatch/overlap/zero-length attacks
- Untrusted roots
- Sign success/failure/empty paths
- Call counting
- PAdES level setting (B_B, B_T, B_LT, B_LTA)
- DSS/DocTimestamp flags

**Safe to rename to TestSignatureValidationMock?**
Yes — clarifies these are mock-based unit tests (interface contract), not integration tests.

---

## 7. Key Integration Points for D2 Adversarial Tests

### Which field holds revocation status?
**Currently: NONE.** SignatureInfo has no revocation field.
- validateSignatures() does NOT extract/report revocation status
- OCSP responses embedded during signing, NOT checked during validation
- Future: add QString revocationStatus ("Good", "Revoked", "Unknown", "NotChecked")

### Failure mode API calls

| Failure | API Call | Code Path |
|---------|----------|-----------|
| ByteRangeOverlap | validateSignatures() | Lines 829-834 |
| ByteRangeMismatch | validateSignatures() | Lines 821-826 |
| UntrustedChain | validateSignatures() | Lines 900-981 |
| SigningTimeOutOfRange | validateSignatures() | Lines 945-964 |
| InvalidEKU | validateSignatures() | Lines 903-922 |
| Malformed | validateSignatures() | Lines 812-844 |
| Unsigned | validateSignatures() | Lines 794-799, 852-855 |

### Trust store behavior
- signDocument() does NOT use trust store
- validateSignatures() fetches via getTrustStore() (lines 881-882)
  - If testTrustStore set → use it
  - Else: Windows system store or system defaults
  - Configurable via QSettings signing/trustStorePath

### Timestamp behavior
- setTsaUrl(url) configures RFC 3161 TSA
- signDocument() fetches token only if level >= B_T AND url not empty (line 625)
- validateSignatures() does NOT re-verify timestamps (only detects /DocTimeStamp presence)

---

## 8. D1/D2/D3 Test Design Summary

### D1: Mock-based (interface contract)
File: TestSignatureValidationMock (rename from TestSignatureValidation)
- Verify MockSignatureManager implements ISignatureManager correctly
- Test UI code paths with predefined SignatureInfo states
- No real crypto, no PDFs required

### D2: Real crypto, signing-side attacks
File: TestSignatureRealCryptoAdversarial (new)
- RSA-1024 rejection: sign with weak RSA → assert isValid=false
- Expired cert rejection: sign with expired cert → assert trustStatus="SigningTimeOutOfRange"
- Bad OCSP rejection: inject malformed OCSP → assert not embedded in DSS
- Weak hash: sign with MD5/SHA-1 → graceful degradation

### D3: Real crypto, validation-side attacks
File: TestSignatureRealCryptoAdversarial (extended)
- Revoked cert detection: pre-populate mock OCSP "revoked" → validate
- Timestamp manipulation: modify signingTime outside cert validity
- Shadow attack: craft PDF with overlapping ByteRange
- Chain injection: modify cert chain in DSS → re-verify

---

## 9. Open Questions / Future Work

1. **RSA key size enforcement:** No current implementation
   - Option A: signDocument() rejects weak keys upfront
   - Option B: validateSignatures() detects and reports weak keys
   - Decision: B-LT spec accepts any RSA; D2 requires stricter policy

2. **Revocation status:** Not in SignatureInfo struct
   - Need: parse embedded OCSP response during validation
   - Add: QString revocationStatus ("Good", "Revoked", "Unknown", "NotChecked")

3. **Certificate expiry at signing:** Not checked
   - Should signDocument() reject already-expired certs?
   - Current: validation catches it if signing-time is set correctly

4. **CRL support:** Infrastructure exists but never populated
   - buildDssDictionary() has CRL arrays (lines 389-399)
   - No fetchCrlResponse() implementation
   - CRL embed logic present but never triggered
