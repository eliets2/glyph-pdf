# M2-PROMPT-4 Vault Summary

## Current commit + test count

**Head:** `c3eb22a` (May 29, 2026) — M2-P5 D6 CHANGELOG/ROADMAP update; LaneScheduler infrastructure (WS1 dep) landed

**Last 8 commits:**
```
c3eb22a docs: update CHANGELOG/ROADMAP for M2-P5 LaneScheduler infrastructure (WS1 dep landed)
c1157de feat(scheduling): LaneScheduler infrastructure with GPU warm worker + CPU pool + cross-page pipelining (WS1 P1/2 dep)
1585155 fix(security): correct dx/dy swap in redactCanvasRecursively Td + Tm operator handling
74fbb5f docs: update CHANGELOG/ROADMAP/README for M2-P3 veraPDF validation (S19 done, R8 closed)
2200aca feat(pdfa): VeraPdfValidator + PdfAValidationPanel wired to real veraPDF subprocess (S19, R8)
8765629 docs: update CHANGELOG/ROADMAP for M2-P2 invisible OCR text scrub (S7 D2 done)
a1221e6 test(security): D2 invisible OCR text scrub regression test (S7 D2)
b0d5a7a feat(security): D1 scrub invisible OCR text (Tr==3) in redactCanvasRecursively (S7 D2)
```

**Test status:** 14/14 ctest suites passing (post-MSYS2 migration). Notable recent result: TestRedaction last run 2026-05-29 03:19:13 AM.

## Git status

**Working tree:** CLEAN (2 modified, 4 untracked)
- **Modified:** `.gitignore`, `SESSION_BRIEF_NEXT.md`
- **Untracked:** `.context/`, `CLAUDE.md`, `docs/`

No uncommitted code changes.

## Test result mtimes

| File | Last Modified |
|------|---|
| TestRedaction_results.txt | 2026-05-29 03:19:13 AM |

Only TestRedaction results found in build directory; other test result files not present or older.

## OpenSSL version

**OpenSSL 3.6.2** (released 7 Apr 2026)

Strong crypto support available. SHA-256 hardening rules well-supported.

## Existing fixtures in tests/fixtures/signing/

| Filename | Size (bytes) |
|----------|---|
| ca.key | 1704 |
| test_ca.pem | 1107 |
| test_ca.srl | 41 |
| signer.key | 1704 |
| signer.crt | 1115 |
| signer.csr | 891 |
| test_signer.p12 | 3363 |
| ext.cnf | 34 |
| generate.bat | 1394 |
| test_input.pdf | 357 |

**Key materials:**
- CA certificate: `test_ca.pem` (self-signed, 3650-day validity)
- Signer key+cert: `signer.key` + `signer.crt` (RSA-2048, 365-day validity)
- PKCS#12 bundle: `test_signer.p12` (password: "test", includes full chain)
- Empty test PDF: `test_input.pdf` (minimal PDF-1.4 structure)

All certificates generated with openssl; none appear to be synthetic adversarial fixtures.

## generate.bat: does it already have adversarial targets?

**NO** — current implementation generates only standard fixtures:
- RSA-2048 keys (strong; no RSA-1024 alternatives)
- X509 certificates with 365/3650-day validity (no expired/revoked variants)
- extendedKeyUsage=emailProtection (basic EKU; no mismatched or missing EKUs)
- PKCS#12 export with static password "test"

**No adversarial coverage for:**
- Expired certificates
- Revoked certificates (no CRL generation)
- RSA-1024 signing (M2-PROMPT-4 target: crypto downgrade rejection)
- Tampered signature byte-range
- Multi-signature chain scenarios
- Certificate chain breaks (untrusted signer, weak hash algorithms)

These gaps exist because M1 B14 crypto hardening focused on real OpenSSL integration; M2-PROMPT-4 must expand fixture coverage for adversarial rejection testing.

## Crypto non-negotiables (from 06-non-negotiables)

**Signature hashing:**
- SHA-256 only — no MD5, SHA-1 for signing (hardcoded in `SignatureManager::signDocument`)

**Signature validation:**
- VRI key computed from raw `/Contents` bytes (not hex-decoded) per ETSI EN 319 132
- CMS_verify uses real trust policy: Windows system root + X509_VERIFY_PARAM + EKU + CRL
- OCSP responses verified before DSS embedding; failed responses degrade sig to B-T
- Incremental save (WriteUpdate) required when signatures exist (full rewrite invalidates ByteRange)
- qpdf NEVER used in signing path (flattens xref, invalidates ByteRange)

**Content-stream injection prevention:**
- All user-input string operators escape via `pdfEscapeLiteralString` (5 hardening sites: watermark, annotation, header/footer, Bates, editTextInline)
- Prevents operator injection via literal-string escape characters: `)`, `\`, `(`

**Redaction correctness:**
- Black-rectangle redaction forbidden (visual overlay is recoverable)
- Content-stream excision required + Edact-Ray glyph-advance normalization (M2-PROMPT-1 defense against PETS 2023 attack)

**Trailer randomization:**
- `/ID[1]` randomization persists across saves (removed `qpdf_set_static_ID` in M1 B10)

**Production shipping:**
- No `TODO(audit-*)` / `FIXME` self-flagged defects in v1.0.0
- All correctness gaps must be tracked as release blockers, not shipped comments

**Concurrency:**
- RenderCache hit-update under single WriteLockGuard (prevents TOCTOU race in LRU eviction)
- Prefetch lambdas use weak_ptr (UAF risk if cache destroyed mid-task)

**OCR limits:**
- Word-level ROVER only (no char-level voting; correlated engine errors amplify)
- Spawn-per-page ONNX process forbidden (ONNX init cost dominates; use persistent warm session via LaneScheduler)

**Format integrity:**
- JBIG2 pattern-matching mode forbidden (digit-substitution risk; BSI banned 2013)
- Djot grammar reimplementation forbidden (vendor Lua 5.4 reference parser only)
- DjVu output format excluded (legacy, dead browser support; importer only with HAS_DJVU=OFF)
- PoDoFo headers confined to engine layer (opaque IPdf* interfaces in public API)

---

**Assessment:** M2-PROMPT-4 must add adversarial fixture variants (expired, RSA-1024, tampered ByteRange, chain breaks) to `generate.bat` for M1 crypto hardening regression testing. All non-negotiable policies are documented and architecturally enforced; no shipping gaps detected.
