# GlyphPDF — Test & Fixture Inventory (for auditor)

> **Deliverable:** M7-PROMPT-1 / D2
> **Date:** 2026-06-01
> **Audience:** Third-party security auditor
>
> Enumerates the existing tests an auditor can reuse and the fixtures they need.
> All paths are relative to the repo root (`C:\Users\User\Projects\pdf`).
> Run with `QT_QPA_PLATFORM=offscreen` then `ctest` from `build/` (see
> `build-instructions.md`).

---

## Security-relevant test targets

These are the tests that directly exercise the audited surface. Reuse them as
oracles; extend them for fuzzing.

| Test file (`tests/`) | Category | Exercises | Notes for auditor |
| :-- | :-- | :-- | :-- |
| `TestRedaction.cpp` | **Security** | Text excision, Form-XObject text redaction, glyph-level "not recoverable from content stream", redaction-log before/after SHA-256 | **Does NOT prove image/vector excision** — that is the T-RED-1 gap. Add image-XObject recovery PoC here. |
| `TestPatternRedact.cpp` | Security | Pattern/regex-driven "redact all" matches | Verify pattern coverage; ReDoS on user patterns. |
| `TestEncryption.cpp` | **Security** | AES-256 markers `/V 5` `/R 6`, empty-password handling, certificate (multi-recipient) encryption | Checks output markers, not KDF strength or permission enforcement. |
| `TestSignatureValidation.cpp` | **Security** | Field population, DSS presence/absence by level, trust-status, ByteRange integrity notes | Mock-leaning validation paths. |
| `TestSignatureValidationMock.cpp` | Security | Validation logic with mocked signature manager | Pairs with the above. |
| `TestSignatureRealCrypto.cpp` | **Security** | **Real OpenSSL/PoDoFo** B-T sign+validate; B-LT DSS present + VRI key == OpenSSL CLI SHA-1 of `/Contents` | Needs `tests/fixtures/signing/*` (below). Best target for tamper-after-sign tests. |
| `TestSanitization.cpp` | **Security** | Metadata removal, unique trailer ID, integration sanitize, and **injection prevention**: watermark / annotation / header-footer / Bates, PDF literal-string escape invariants | Fuzz for vectors *not* covered. |
| `TestResourceLimits.cpp` | **Resilience** | Page-count / buffer-size boundaries | Extend with decompression bombs, deep nesting, malformed xref. |
| `TestThreadSafety.cpp` | Concurrency | Mutex validation under concurrent access | Data races around the shared PoDoFo document. |
| `TestAiProvider.cpp` | Security-adjacent | AI provider behaviour (network/key paths) | No assertion that keys are never persisted in plaintext (T-API-2). |
| `TestVeraPdf.cpp` | Integration | veraPDF **subprocess** invocation | Subprocess arg/temp-file handling (B3). |
| `TestOfficeImport.cpp` | Integration | `soffice` Office→PDF import | Subprocess command-injection surface (B3). |
| `TestAutosave.cpp` | Integration | Autosave path | Temp-file leakage of document content. |

## Non-security test targets (context only)

`UnitTests.cpp`, `TestInterfaces.cpp` / `TestPdfEditorInterface.cpp`,
`smoke_test.cpp`, `TestControllers.cpp`, `TestIntegration.cpp`,
`TestPerformance.cpp`, `TestFormBuilder.cpp`, `TestPagesMode.cpp`,
`TestInspector.cpp`, `TestLaneScheduler.cpp`, `TestBatchMode.cpp`,
`TestDjotRoundtrip.cpp`, `TestDiffEngine.cpp`.

> The README lists "14 test targets"; the tree currently contains more `Test*.cpp`
> files than that (some are newer than the README table). Treat the table in
> README §Testing as indicative; this file is the authoritative current list.

---

## Fixtures present in-repo

### Signing fixtures — `tests/fixtures/signing/`

**All test-only and disposable. No production secrets.** Used by
`TestSignatureRealCrypto`.

| Fixture | Purpose |
| :-- | :-- |
| `test_signer.p12` (pass: `test`) | Valid signer cert + chain (PKCS#12) |
| `test_ca.pem`, `test_ca.srl` | Test root CA |
| `signer.key` / `signer.csr` / `signer.crt` | Signer key material |
| `ca.key`, `ext.cnf` | CA key + extension config |
| `expired.{key,csr,crt}`, `expired_cert.p12` | **Expired** cert (negative test) |
| `revoked.{key,csr,crt}`, `revoked_cert.p12`, `revoked_ocsp_response.der` | **Revoked** cert + OCSP response (negative test) |
| `weak.{key,csr,crt}`, `weak_cert_rsa1024.p12` | **Weak RSA-1024** cert (negative test) |
| `test_input.pdf` | One-page input PDF to sign |
| `generate.bat`, `generate_test_input.py`, `generate_revoked_ocsp.py` | Regenerate the fixtures |

> The negative fixtures (expired / revoked / weak-key) are valuable — the auditor
> should confirm each is actually **rejected** by the validation path.

### Other fixtures

| Fixture | Used by |
| :-- | :-- |
| `tests/test_autosave_input.pdf` | `TestAutosave` |
| `tests/mocks/MockFormManager.h`, `MockPdfEditorEngine.h`, `MockSignatureManager.h` | Mock-based tests |

---

## Fixtures the auditor will need to ADD

The current corpus is functional-test-oriented, **not** adversarial. For a real
audit the auditor should bring/build:

1. **Malformed / adversarial PDF corpus** for parser fuzzing (B1): truncated
   xref, circular references, deeply nested objects, integer-overflow object
   counts, malformed stream lengths, decompression bombs, hostile fonts/CMaps.
   Seed from public PDF fuzzing corpora + GlyphPDF's own output.
2. **Image/vector redaction PoC PDFs** (T-RED-1): pages whose sensitive content
   is a raster image or vector drawing under a redaction box, to demonstrate
   recoverability.
3. **Sanitization-evasion PDFs**: hidden data in annotation `/RC`, form-field
   values, image XMP/EXIF, `/Launch` actions, indirectly-referenced attachments.
4. **Tamper-after-sign PDFs**: signed docs with byte-range-violating incremental
   updates, to confirm integrity detection (T-TMP-2 / T-SPF-1).
5. **Malicious update manifests** (B4): wrong-hash, downgraded, attacker-supplied
   hash alongside binary.
6. **Subprocess-injection inputs** (B3): file names / paths with shell
   metacharacters for `soffice` / `veraPDF` invocation.

---

## How to run

```bat
set QT_QPA_PLATFORM=offscreen
cd build
ctest --output-on-failure
```

To run a single security target, e.g. redaction:

```bat
ctest -R TestRedaction --output-on-failure
```

> `TestSignatureRealCrypto` self-skips if the signing fixtures are missing; run
> the fixture generators in `tests/fixtures/signing/` first if needed.
