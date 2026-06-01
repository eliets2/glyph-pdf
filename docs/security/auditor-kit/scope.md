# GlyphPDF — Audit Scope

> **Deliverable:** M7-PROMPT-1 / D2
> **Date:** 2026-06-01
> **Audience:** Third-party security auditor
> **Engagement target:** focused 1–2 week white-box source-code audit (see
> `../audit-vendor-shortlist.md`).

This document bounds the engagement so the quote stays inside the ~$15–40K
planning envelope. Keeping scope tight is what separates a $25K engagement from a
$80K one.

---

## In scope

The audit covers **GlyphPDF's own code** and **its use of third-party libraries**,
focused on these surfaces (priority order):

1. **PDF parser robustness / fuzzing (PRIMARY).** Memory safety of parsing
   untrusted, attacker-supplied PDFs through PoDoFo / PDFium / qpdf and through
   GlyphPDF's own content-stream parser (`redactCanvasRecursively` and related in
   `src/engines/podofo/PoDoFoBackend.cpp`). Heap/stack overflow, use-after-free,
   integer overflow, type confusion, OOB read/write. **Fuzzing harnesses are
   in scope and expected.**
2. **Redaction.** Correctness and irreversibility of redaction —
   `applyRedactions` / `redactCanvasRecursively`. **Must validate the known
   image/vector limitation (T-RED-1 in `threat-model.md`).** Text excision,
   form-XObject text, struct-tree MCID scrubbing, inline-image abort path.
3. **AES-256 encryption.** `encryptDocument` (`AESV3R6`), password/permission
   handling, certificate-based multi-recipient encryption, `removeEncryption`.
4. **PAdES signatures.** `SignatureManager.cpp` — B-T/B-LT/B-LTA, SHA-256 digest,
   DSS/VRI, OCSP fetch+verify, TSA `/DocTimeStamp`, **signature `ByteRange`
   validation** (post-sign tamper detection), the documented B-T degradation
   behaviour.
5. **Sanitization vectors.** `sanitizeDocument` — completeness of the ~20-vector
   strip (metadata, JavaScript, OpenAction, AA, embedded files, optional-content,
   struct-tree alt-text, outlines, etc.) and **content-stream injection escaping**
   (PDF literal-string escaping; watermark/annotation/header-footer/Bates
   injection prevention).
6. **AI provider network + key handling.** `src/engines/ai/*` — outbound TLS
   calls to third-party LLM endpoints (data egress boundary), API-key storage in
   Windows Credential Manager **and the plaintext QSettings fallback** (T-API-2),
   credential persistence scope. *Note: providers are currently orphaned/unused
   in the shipping UI — see `threat-model.md` §5 — but the network + secrets code
   is in scope regardless.*
7. **Subprocess invocation.** `soffice` (Office import) and `veraPDF` (PDF/A
   validation) — argument/command injection, temp-file handling, timeouts.
8. **Auto-update integrity.** `UpdateChecker` — SHA-256 verification provenance,
   TLS, consent gating.
9. **Credential storage.** `src/core/CredentialManager.cpp` — Windows Credential
   Manager usage, persistence scope.

### Files of primary interest

| Area | Path |
| :-- | :-- |
| Redaction / sanitize / encrypt | `src/engines/podofo/PoDoFoBackend.cpp` |
| Signatures | `src/engines/SignatureManager.cpp` |
| Credential storage | `src/core/CredentialManager.cpp` |
| AI providers (network + keys) | `src/engines/ai/{Anthropic,OpenAi,Gemini,Ollama}Provider.cpp` |
| Security tests / fixtures | `tests/` (see `test-fixtures-list.md`) |

---

## Out of scope

- **Internals of third-party libraries.** Bugs *inside* the following are **out
  of scope** except where GlyphPDF **misuses** them (wrong API usage, missing
  validation, unsafe defaults, ignoring error returns). Versions are pinned below
  and in `LICENSE-3RD-PARTY.md`:

  | Library | Version (pinned) | License |
  | :-- | :-- | :-- |
  | Qt 6 | 6.11.0 (MSYS2 ucrt64) | LGPL-3.0 / commercial |
  | PoDoFo | 0.10.4 (MSYS2 ucrt64) | LGPL-2.0+ OR MPL-2.0 |
  | OpenSSL | 3.6.2 (MSYS2 ucrt64) | Apache-2.0 |
  | PDFium | prebuilt (vendored) | BSD-3-Clause |
  | qpdf | 12.3.2 (MSYS2 ucrt64) | Apache-2.0 |
  | Tesseract | 5.5.2 (MSYS2 ucrt64) | Apache-2.0 |
  | Leptonica | 1.87.0 (MSYS2 ucrt64) | BSD-2-Clause |
  | OpenXLSX | 2025-07-14 | BSD-3-Clause |
  | DuckX | 1.2.2 | MIT |
  | ONNX Runtime | 1.17.3 | MIT |

  > Finding a 0-day *inside* PoDoFo/PDFium/OpenSSL is valuable and welcome, but the
  > engagement is scoped to GlyphPDF's attack surface; deep library-internal
  > fuzzing is a separate, larger effort. If the auditor's fuzzing surfaces a
  > library-internal crash reachable from GlyphPDF input, that **is** in scope as
  > a GlyphPDF DoS/RCE finding (we ship the library), with upstream coordination
  > handled separately.

- **OCR accuracy / ML model behaviour** (Tesseract, RapidOCR, ONNX models) —
  correctness, not security, unless model files are an untrusted-input vector.
- **Non-Windows platforms.** GlyphPDF ships Windows-only (MSYS2 ucrt64);
  `CredentialManager` and crypto paths are Windows-specific. Linux/macOS are not
  in scope.
- **Physical / social-engineering / phishing** attacks.
- **Denial of service against external services** (TSA, OCSP, LLM endpoints,
  update server) — we are a *client* of these.
- **Business logic of non-security features** (annotations, forms layout, theming,
  print preview) except where they touch a security asset.
- **Forbidden/never-linked dependencies.** MuPDF (AGPL-3.0) and Poppler (GPL-2.0+)
  are **not linked in-process** (CMake `FATAL_ERROR` guard); veraPDF (AGPL-3.0) is
  **subprocess-only**. These are out of scope as in-process code because they are
  not in-process code — but the auditor may verify the CMake guards actually hold.

---

## Repo access for the auditor

- **Mode:** white-box, source-available. Provide the auditor read access to the
  full source tree (the audited revision SHA to be fixed at engagement start).
- **Options (decide at contracting):**
  - **(a) Public OSS repo** — GlyphPDF is slated to be open source under
    **Apache-2.0 (or MIT — finalized at M8 launch)**. If the repo is public at
    audit time, no special access is needed; pin the commit SHA in the SOW.
  - **(b) Private read-only access** — if auditing before public launch, grant a
    read-only repo invite (or a tagged source tarball at a fixed SHA) plus a
    **read-only commit-signing public key share** so the auditor can verify the
    provenance of the audited revision. Do **not** share any signing **private**
    key or production secrets.
- **Build:** see `build-instructions.md` (MSYS2 ucrt64). The auditor needs a
  buildable tree to construct fuzzing harnesses.
- **Out of band:** test fixtures (signing certs/keys are **test-only**, see
  `test-fixtures-list.md`) ship in-repo under `tests/fixtures/` — these are
  disposable and contain no production secrets.

---

## Deliverables expected from the auditor

1. Findings report with **CRITICAL / HIGH / MEDIUM / LOW** severities (feeds D3).
2. PoCs for any redaction-recovery (T-RED-1) and parser-memory-safety findings.
3. A short **signed summary letter** of scope + findings + remediation status,
   suitable for public reference on the website / README (feeds D4).
4. Reproduction steps + fuzzing harnesses/corpora where applicable.

> **Constraint (from the prompt):** do not publish audit findings until the
> auditor's letter is received; CRITICAL findings must be fixed before launch.
