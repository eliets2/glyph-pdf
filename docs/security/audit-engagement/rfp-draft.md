# Request for Proposal — Third-Party Security Audit
## GlyphPDF v1.0.0 Professional PDF Workstation

> **Deliverable:** M7-PROMPT-1 / D3
> **Date issued:** 2026-06-02
> **Issued by:** GlyphPDF (project owner — contact details TBD at contracting)
> **Addressed to:** Trail of Bits / NCC Group / Cure53 (shortlisted per
> `docs/security/audit-vendor-shortlist.md`)
> **Response deadline:** [INSERT — recommend 10 business days from issue]

---

## 1. Executive summary

GlyphPDF is a C++17 / Qt 6 desktop PDF workstation shipping on Windows 10/11
(MSYS2 ucrt64 toolchain). It handles attacker-supplied PDFs and performs
security-critical operations — redaction, AES-256 encryption, PAdES digital
signatures, document sanitization, and optional AI-provider integration. The
product is approaching public release under an open-source licence.

We are soliciting proposals for a **focused, white-box security assessment** of
the security surface described in Section 3 below. We expect to select one firm
and begin engagement within [INSERT target month]. The engagement budget
envelope is **$20,000–$40,000 USD** for a standard 1–2 week scope; candidates
proposing a fuzzing harness extension should quote that separately.

This RFP is addressed to three firms simultaneously. We will award to a single
firm based on fit (technical capability), timeline, and price. We are happy to
schedule a 30-minute scoping call before written proposals are submitted.

---

## 2. Project background

| Item | Detail |
| :-- | :-- |
| Product | GlyphPDF v1.0.0 |
| Language / platform | C++17, Qt 6.11, Windows 10/11, MSYS2 ucrt64 (GCC 16) |
| Licence (planned) | Apache-2.0 or MIT (finalized at public launch) |
| Repo | Private at audit time; read-only access granted to selected firm |
| MSI installer | `GlyphPDF-1.0.0-x64.msi` (buildable from source) |
| Test suite | 14 test targets (CTest); security-relevant: TestRedaction, TestEncryption, TestSignatureRealCrypto, TestSignatureValidation, TestSanitization |
| Third-party engines | PoDoFo 0.10.4, PDFium (vendored prebuilt), qpdf 12.3.2, OpenSSL 3.6.2, Tesseract 5.5.2, Qt 6.11.0 — all pinned |

We have prepared a complete **auditor kit** in `docs/security/auditor-kit/`
covering: a STRIDE threat model, security claims with test-verification status,
and a scoped audit boundary document. These will be shared immediately on
engagement.

---

## 3. Technical scope

The audit is bounded to **GlyphPDF's own code and its use of third-party
libraries** (not internals of those libraries, except where GlyphPDF misuses
them). Nine surfaces are in scope, priority-ordered:

### 3.1 PDF parser robustness — PRIMARY, fuzzing expected

Memory safety of parsing untrusted, attacker-supplied PDFs through PoDoFo /
PDFium / qpdf and through GlyphPDF's own content-stream parser
(`redactCanvasRecursively` and related code in
`src/engines/podofo/PoDoFoBackend.cpp`).

Target vulnerability classes: heap/stack overflow, use-after-free, integer
overflow, type confusion, out-of-bounds read/write.

**Fuzzing harnesses are in scope and expected** — the auditor should build at
least one libFuzzer or AFL++ harness targeting the PDF-open path and one
targeting the redaction content-stream parser. Corpora and harnesses should be
delivered as part of the engagement.

### 3.2 Redaction correctness

Validate the correctness and irreversibility of `applyRedactions` /
`redactCanvasRecursively`.

There is a **known, honestly-disclosed limitation (threat-model T-RED-1)** that
image/vector content under a redaction box is not excised — it is covered by a
painted black rectangle. We ask the auditor to:

1. Confirm the exact boundary of what is and is not excised.
2. Demonstrate recovery of image/vector content under a redaction box (PoC).
3. Recommend the fix surface (excise image XObjects / rasterize-and-flatten /
   fail-closed).
4. Validate coverage of all text-show operators, nested form XObjects, and
   struct-tree MCID scrubbing.

### 3.3 AES-256 encryption

`PoDoFoBackend::encryptDocument` using `PdfEncryptionAlgorithm::AESV3R6`.
Validate key derivation, password/permission handling, multi-recipient
certificate encryption, and `removeEncryption`.

### 3.4 PAdES digital signatures

`src/engines/SignatureManager.cpp` — B-T / B-LT / B-LTA levels, SHA-256 digest,
DSS/VRI dictionary construction, OCSP fetch and verification, TSA
`/DocTimeStamp`. Specifically:

- Verify that `ByteRange` covers the whole file (post-sign tamper must be
  detectable).
- Validate the documented B-T degradation behaviour when DSS/OCSP/TSA steps
  fail.
- Validate OCSP/CRL freshness logic and TSA token embedding.
- Verify SHA-1 appears only where PAdES mandates it (VRI keying), not in the
  signature digest.

Note: `SignatureManager.cpp` opens with a deliberate Windows CryptoAPI /
OpenSSL include-order guard (`#undef X509_NAME`, etc.) to prevent macro
stomping — auditor should confirm this guard is complete and correct.

### 3.5 Document sanitization

`PoDoFoBackend::sanitizeDocument` — completeness of the ~20-vector strip
(metadata, JavaScript, OpenAction, AA, embedded files, optional-content,
struct-tree alt-text, outlines, collection portfolios, etc.) and
content-stream injection escaping (PDF literal-string escaping;
watermark/annotation/header-footer/Bates injection prevention).

Fuzz for vectors not on the list: annotation `/RC` rich text, form-field
values, image EXIF/XMP, `/Launch` actions, indirect attachment references.

### 3.6 AI provider network and key handling

`src/engines/ai/` — outbound TLS calls to api.anthropic.com, OpenAI, and
Gemini endpoints; API key storage in Windows Credential Manager and the
**plaintext QSettings fallback** (`ai/keyAnthropicCached`) in
`AnthropicProvider::resolveKey` (threat-model T-API-2). Credential persistence
scope (`CRED_PERSIST_LOCAL_MACHINE`).

Note: these providers are currently orphaned/unused in the shipping UI but the
network and secrets code is live and must be treated as in scope.

### 3.7 Subprocess invocation

`soffice` (Office import) and `veraPDF` (PDF/A validation) — argument/command
injection via crafted file paths, temp-file permissions and cleanup, subprocess
timeout enforcement.

### 3.8 Auto-update integrity

`UpdateChecker` — SHA-256 verification provenance, TLS certificate validation,
user-consent gating, manifest-and-binary delivery channel trust.

### 3.9 Credential storage

`src/core/CredentialManager.cpp` — Windows Credential Manager usage
(`CredWriteW`, DPAPI-backed), persistence scope assessment.

### Out of scope

- Internals of third-party libraries (PoDoFo, PDFium, qpdf, OpenSSL, Tesseract,
  Leptonica) except where GlyphPDF misuses them.
- Non-Windows platforms.
- OCR model accuracy or ML behaviour.
- Physical / social-engineering attacks.
- Business logic of non-security features (annotations, forms layout, theming).

---

## 4. Deliverables expected

| # | Deliverable | Notes |
| :-- | :-- | :-- |
| D-1 | **Findings report** | CRITICAL / HIGH / MEDIUM / LOW / Informational per CVSS v3.1. Each finding: title, severity, affected file:line, reproduction steps, root-cause analysis, remediation recommendation. |
| D-2 | **Proof-of-concept for Critical/High findings** | Especially T-RED-1 image-recovery PoC and any parser memory-corruption PoC. |
| D-3 | **Remediation guidance** | Written guidance for each finding; available for one round of remediation Q&A after report delivery. |
| D-4 | **Signed audit letter** | A brief (1–2 page) signed summary of scope, methodology, and findings status, suitable for public reference on the product website and README. This is a hard requirement — we need a citable, signed letter. |
| D-5 | **Fuzzing harnesses and corpora** | At least one harness targeting PDF-open and one targeting the redaction content-stream parser; seed corpus; instructions for reproduction. |

---

## 5. Timeline

| Milestone | Target date |
| :-- | :-- |
| RFP response deadline | [INSERT — recommend 10 business days from issue] |
| Scoping call (if requested) | [INSERT — within 5 business days of RFP issue] |
| Vendor selection + SOW execution | [INSERT] |
| Repo / build access granted | [INSERT — Day 0 of engagement] |
| Kick-off call | [INSERT — Day 1] |
| **Engagement duration** | **2 weeks (10 business days)** |
| Preliminary findings shared (verbal/draft) | End of week 1 |
| Final report delivered | End of week 2 (Day 10) |
| Remediation window | 4 weeks post-report |
| Re-test (if included in SOW) | After remediation window |
| Signed audit letter issued | After re-test / remediation sign-off |

Flexibility: we can accommodate a phased approach (fuzzing week 1, code review
week 2) if the firm finds it operationally preferable.

---

## 6. Budget envelope

| Scenario | Estimate |
| :-- | :-- |
| Tight scope, 1 week, 1–2 consultants, no fuzzing harness | ~$15K–$25K |
| **Standard scope, 1–2 weeks, fuzzing harnesses included** | **~$25K–$40K (target)** |
| Premium: custom fuzzing harness + extended re-test | $40K+ |

The planning budget for this engagement is **$20,000–$40,000 USD**. Proposals
materially above $40,000 for the core scope will require additional justification.
Please quote fuzzing harness construction as a separate line item if applicable.

Payment milestones will be negotiated in the SOW (see `sow-template.md`); a
typical structure is 50% on engagement start, 50% on report delivery.

---

## 7. Access requirements

### 7.1 Source code

- **Mode:** white-box, source-available.
- A read-only GitHub invite (or signed source tarball at a fixed commit SHA)
  will be provided on engagement start. The audited revision SHA will be fixed
  in the SOW and must appear in the final report.
- The auditor will need a buildable tree to construct fuzzing harnesses (see
  build instructions in `README.md` — MSYS2 ucrt64 environment).

### 7.2 Build environment

- Windows 10 or 11, 64-bit.
- MSYS2 ucrt64 toolchain (GCC 16, Qt 6.11 — installed via pacman; full
  instructions in `README.md`).
- MSI installer (`GlyphPDF-1.0.0-x64.msi`) for black-box / installed-product
  testing in parallel with source review.

### 7.3 Test fixtures

- Signing certificates/keys in `tests/fixtures/` are test-only (disposable;
  no production secrets).
- Additional adversarial PDF test corpus: see
  `docs/security/audit-environment/adversarial-test-corpus.md`.

### 7.4 Auditor kit

On engagement start, we provide:
- `docs/security/auditor-kit/threat-model.md` — STRIDE model with known issues
  flagged.
- `docs/security/auditor-kit/security-claims.md` — eight claims with
  test-verification status.
- `docs/security/auditor-kit/scope.md` — precise in/out-of-scope boundary.
- `docs/security/audit-environment/test-environment-setup.md` — VM setup guide.
- `docs/security/audit-environment/adversarial-test-corpus.md` — adversarial
  PDF categories.

### 7.5 NDA

A mutual NDA covering the source code and findings will be executed before repo
access is granted. Template language is in `sow-template.md` Section 5.

### 7.6 What we do NOT share

- Production signing keys, production API keys, or any user data.
- Any build secrets not in the repo (there are none — all build dependencies
  come from public MSYS2 pacman packages).

---

## 8. Proposal requirements

Please respond with:

1. **Firm overview** — relevant C/C++ security audit experience; prior PDF-parser
   or document-format audit references (public reports preferred).
2. **Proposed methodology** — how you approach the nine surfaces; whether you
   include fuzzing harness construction and what tooling (libFuzzer, AFL++,
   Honggfuzz).
3. **Team composition** — number of consultants, seniority, named leads if
   possible.
4. **Itemized quote** — base scope + fuzzing harness as separate line item +
   optional re-test.
5. **Proposed timeline** — start date, end date, preliminary/final report
   delivery dates.
6. **Signed audit letter** — confirm you can and will provide a signed public
   summary letter on completion.
7. **References** — one or two public audit reports of comparable C/C++ or
   document-format scope.

---

## 9. Evaluation criteria

| Criterion | Weight |
| :-- | :-- |
| C/C++ memory-safety audit capability | 30% |
| Parser fuzzing capability (harness authoring, tooling) | 25% |
| Cryptography review depth (PAdES, AES, OCSP, TSA) | 20% |
| Quality and citability of published audit letter | 15% |
| Price relative to scope | 10% |

---

## 10. Contact and submission

Proposals should be submitted to: [INSERT contact email]

Questions and scoping-call requests: [INSERT contact email]

Response format: PDF or Markdown. Scoping calls available [INSERT availability].
