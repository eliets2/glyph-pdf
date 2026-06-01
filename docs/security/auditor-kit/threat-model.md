# GlyphPDF — Threat Model (for external auditor)

> **Deliverable:** M7-PROMPT-1 / D2
> **Date:** 2026-06-01
> **Audience:** Third-party security auditor (white-box, source-available)
> **Status:** Draft — describes the *intended* security posture and *known*
> weaknesses. Claims in `security-claims.md` are to be **independently
> validated**; this document deliberately flags unsolved issues for the auditor
> rather than asserting they are solved.

GlyphPDF is a C++17 / Qt 6 desktop PDF workstation (Windows, MSYS2 ucrt64). It
parses, edits, redacts, encrypts, signs (PAdES), sanitizes, and OCRs PDF
documents. This model uses **STRIDE** over the assets and trust boundaries below.

---

## 1. Assets

| ID | Asset | Sensitivity | Where it lives |
| :-- | :-- | :-- | :-- |
| A1 | **User documents** (PDFs being edited) | High — may contain PII, legal, financial content | In-memory (PoDoFo/PDFium object graph), on disk |
| A2 | **Redacted content** (text/regions the user intends to permanently remove) | **Critical** — redaction failure = data leak | Content streams; must be *excised*, not merely covered |
| A3 | **Signing keys / certificates** (PAdES) | **Critical** | User-supplied PKCS#12 (`.p12`) / OS cert store; loaded for signing |
| A4 | **Stored AI provider API keys** (Anthropic / OpenAI / Gemini) | High — billable secret, identity | Windows Credential Manager (primary); **plaintext QSettings fallback** (see T-API-2) |
| A5 | **Document passwords** (AES encryption user/owner passwords) | High | Transient in memory during encrypt/decrypt |
| A6 | **Redaction audit log** (`*.redaction-log.json` sidecar) | Medium — records what/where was redacted, with SHA-256 before/after | On disk next to the source file |
| A7 | **Auto-update integrity** (update manifest + downloaded binary) | High — RCE vector if subverted | Network fetch + SHA-256 verify |

---

## 2. Trust boundaries

```
   [ Untrusted PDF file ]                 [ Untrusted/attacker-controlled web ]
            │                                          │
            ▼  (B1 parse)                              ▼  (B4 update manifest)
 ┌───────────────────────────────────────────────────────────────────────┐
 │  GlyphPDF process (user-privilege, native C++)                        │
 │   PoDoFo / PDFium / qpdf   ── B2 ──►  AI providers (api.anthropic.com,│
 │   redaction / sanitize / encrypt        api.openai.com, Gemini)       │
 │   SignatureManager (OpenSSL)  ── B3 ─►  soffice / veraPDF subprocesses│
 └───────────────────────────────────────────────────────────────────────┘
            │                                          │
            ▼  (signing keys A3)                       ▼  (API keys A4)
   [ PKCS#12 / OS cert store ]            [ Windows Credential Manager / QSettings ]
```

| ID | Boundary | Crossing | Direction of trust |
| :-- | :-- | :-- | :-- |
| **B1** | **Untrusted PDF → native parsers** | Attacker-supplied bytes enter PoDoFo / PDFium / qpdf | Untrusted → trusted process. **Primary attack surface.** |
| **B2** | **Process → AI provider network** | Document text + prompts sent over TLS to third-party LLM APIs; API key attached | Confidential data leaves the machine |
| **B3** | **Process → subprocesses** | `soffice` (Office import), `veraPDF` (PDF/A validation) invoked as child processes | Arguments/paths/temp files cross to external binaries |
| **B4** | **Process → update server** | Auto-update fetches JSON manifest + binary | Remote bytes become executable code |
| **B5** | **Process → secret stores** | Read/write of API keys (A4) and signing keys (A3) | Secrets cross to/from OS facilities |
| **B6** | **In-app model boundary** | `ProvenanceGuard` blocks lossy Djot-edit-save-back on signed/born-PDF docs | Integrity boundary inside the app |

---

## 3. STRIDE analysis

Severity is the **drafter's pre-audit estimate**; the auditor should re-rate.

### 3.1 Spoofing

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| T-SPF-1 | Forged / tampered signature accepted as valid | B1 | High | Validation must enforce signature `ByteRange` covers the whole file; DSS/VRI and OCSP must be checked. Auditor to verify byte-range logic in `SignatureManager.cpp`. |
| T-SPF-2 | Malicious update binary impersonates a real release | B4 | High | SHA-256 verification of download + user consent at each stage. Auditor to confirm the hash is from a trusted/authenticated manifest, not attacker-controlled alongside the binary (TLS + provenance of the manifest). |
| T-SPF-3 | TSA / OCSP responder spoofing during signing | B2/B4 | Medium | TSA token + OCSP response are verified before embedding in DSS; on failure the signature **degrades to B-T** (code does this honestly). Auditor to verify the verification path. |

### 3.2 Tampering

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| T-TMP-1 | Memory-corruption via crafted PDF leads to code execution / object overwrite | B1 | **Critical** | Native parsing of untrusted input is the dominant risk. **Fuzzing in scope.** Heap/stack overflow, UAF, integer overflow, type confusion in PoDoFo/PDFium/qpdf glue and our content-stream parser (`redactCanvasRecursively`). |
| T-TMP-2 | Signed document modified post-signature without detection | B1/B6 | High | `ProvenanceGuard` refuses lossy Djot-edit-save-back for signed/born-PDF docs. Auditor to test incremental-update tampering detection. |
| T-TMP-3 | Redaction audit log (A6) altered to hide a redaction | filesystem | Low | Sidecar JSON is unauthenticated; before/after SHA-256 recorded but the log itself can be edited. Auditor to note this is informational, not tamper-proof. |

### 3.3 Repudiation

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| T-RPD-1 | User denies a signature/redaction action | n/a | Low | PAdES signatures provide non-repudiation when keys are sound; redaction log records timestamp + regions + before/after hashes. |

### 3.4 Information Disclosure — *highest-priority category for this product*

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| **T-RED-1** | **Image / vector content under a redaction box is NOT excised — only covered by a painted black rectangle — and remains recoverable.** | B1/A2 | **Critical (KNOWN LIMITATION — validate, do not assume solved)** | **See §4 for full detail.** This is a known, honestly-disclosed gap, not a solved claim. The README states "content stream excision, never black rectangles"; that holds for **text** but **not** for raster/vector imagery. Auditor MUST validate the exact boundary of what is and isn't excised. |
| T-RED-2 | Text redaction misses content due to content-stream parser edge cases (unusual `Tm`/`Td`/`TJ` patterns, nested XObjects, fonts) | B1/A2 | High | `redactCanvasRecursively` excises text operators that intersect the rect. Coverage of all positioning/show-text operators, nested form XObjects, and Type3 fonts must be validated. |
| T-RED-3 | Redacted content survives in metadata / structure tree (`ActualText`, `Alt`, tagged-PDF MCIDs) | B1/A2 | High | Redaction scrubs struct elements by MCID and sanitize removes `ActualText`/`Alt`/`E`. Auditor to confirm no redacted text remains in alternate-text or accessibility paths. |
| T-SAN-1 | Sensitive metadata / hidden data survives "sanitization" | B1 | Medium | `sanitizeDocument` strips ~20 vectors (Info, XMP `Metadata`, `PieceInfo`, `MarkInfo`, `OutputIntents`, `EmbeddedFiles`, JavaScript, `OpenAction`, `AA`, `OCProperties`, `Outlines`, Collection, struct `Alt`/`ActualText`/`E`, …). Auditor to fuzz for vectors not on the list (e.g. annotations' `/RC`, form field values, image EXIF). |
| **T-API-1** | **Document text is sent to third-party LLM APIs over the network** | B2 | High | The AI providers POST document/prompt content to `api.anthropic.com` / OpenAI / Gemini. This is an intentional feature but a confidentiality boundary. **NOTE: the AI providers are currently present but orphaned/unused in the shipping UI** — see §5. Auditor to confirm no AI path is reachable unless the user explicitly opts in, and that Ollama (local) is the only fully-offline option. |
| **T-API-2** | **API key stored in plaintext via QSettings fallback** | A4/B5 | High | `CredentialManager` stores keys in Windows Credential Manager (DPAPI-backed). **But** `AnthropicProvider::resolveKey` falls back to **plaintext `QSettings` (`ai/keyAnthropicCached`)** if the Credential Manager has no entry. The README/claims imply credential-manager storage; the plaintext fallback contradicts that and must be flagged. |
| T-API-3 | Credential blob persisted at machine scope | A4/B5 | Medium | `storeKey` uses `CRED_PERSIST_LOCAL_MACHINE` — readable by other processes of the same machine context. Auditor to assess whether `CRED_PERSIST_LOCAL_MACHINE` vs `CRED_PERSIST_CURRENT_USER` is the right scope. |
| T-IDS-1 | Temp files from `soffice`/`veraPDF` leak document content | B3 | Medium | Office import and PDF/A validation write temp files. Auditor to check temp-file permissions, cleanup, and predictable-path issues. |

### 3.5 Denial of Service

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| T-DOS-1 | Decompression bomb / deeply-nested objects / huge page count | B1 | Medium | `TestResourceLimits` exercises page/buffer boundaries. Auditor to test zip-bomb streams, recursive XObjects, malformed xref. |
| T-DOS-2 | Subprocess hang (soffice/veraPDF never returns) | B3 | Low | Auditor to confirm timeouts on child processes. |

### 3.6 Elevation of Privilege

| ID | Threat | Boundary | Pre-audit severity | Notes / control |
| :-- | :-- | :-- | :-- | :-- |
| T-EOP-1 | Argument/command injection into `soffice`/`veraPDF` via crafted file paths | B3 | Medium | Auditor to verify subprocess invocation uses argument vectors (not shell string concatenation) and sanitizes paths. |
| T-EOP-2 | DLL search-order hijack at startup (MinGW/ucrt64 runtime + bundled PDFium/ONNX DLLs) | process load | Medium | Windows desktop app loading bundled DLLs. Auditor to check for unsafe DLL loading. |
| T-EOP-3 | PDF JavaScript / launch actions execute | B1 | Low–Medium | GlyphPDF does not run PDF JS, and sanitize strips `/JavaScript`, `/OpenAction`, `/AA`, `/Launch`. Auditor to confirm no execution path exists when *viewing* a malicious PDF.

---

## 4. KNOWN LIMITATION — redaction of images/vectors (T-RED-1)

**This is disclosed honestly for the auditor to validate. It is NOT presented as
solved.**

- **What works (text):** When a redaction rectangle is applied, `applyRedactions`
  → `redactCanvasRecursively` parses the page content stream and **excises the
  text-show operators** (and their operands) that fall within the rectangle, then
  scrubs the corresponding tagged-PDF structure-tree MCIDs. For text, the content
  is genuinely removed from the content stream — consistent with the README claim
  "content stream excision, never black rectangles."
- **What does NOT work (images/vectors):** For **raster images (image XObjects /
  inline images) and vector path content** sitting under the rectangle, the code
  does **not** remove or alter the underlying pixel/path data. The image branch in
  `redactCanvasRecursively` performs only a *position check*
  (`isIntersectingSpan(textX, textX, textY)` — labelled in-code as a "Simple
  check for image position") and does not excise image data. The final step of
  `applyRedactions` always paints an opaque black rectangle
  (`PdfPainter::DrawRectangle`, `PdfPathDrawMode::Fill`) over the region.
- **Consequence:** A black box drawn over an image/vector region is a **visual
  overlay**. The original image/vector bytes remain in the PDF and are
  **recoverable** (e.g. by removing the overlay operator, or extracting the image
  XObject directly). For documents whose sensitive content is an *image* (scanned
  pages, photos, signatures, charts), redaction is **not** guaranteed to be
  irreversible.
- **Partial mitigation already present:** If a content stream contains inline
  images (`ID`/`EI`) or binary data, the code **aborts** the redaction (returns
  `false`, logs a `qCritical` "SECURITY" message) rather than silently applying a
  visual-only overlay for that case. This protects *inline-image* streams but does
  **not** cover image **XObjects**, which are the common case.

**Asks for the auditor:**
1. Confirm the exact set of content types that are / are not excised.
2. Demonstrate recovery of image/vector content under a redaction box (PoC).
3. Recommend the fix surface (e.g. excise image XObjects intersecting the rect /
   re-encode the page region / rasterize-and-flatten the redacted area).
4. Advise whether the product should **block** image/vector redaction (fail
   closed) until true excision is implemented, mirroring the inline-image abort.

---

## 5. Note on AI providers (orphan / currently-unused code)

The four AI providers (`AnthropicProvider`, `OpenAiProvider`, `GeminiProvider`,
`OllamaProvider`) are **present in the source tree but currently
orphaned/unused** in the shipping product (a "AI Chat panel" exists in the UI
inventory, but the providers are not wired into a reachable, shipped workflow at
this milestone). They are in scope for the threat model because they:

- **read secrets** (API keys from Credential Manager / plaintext QSettings),
- **make outbound network calls** (live HTTPS POST to third-party LLM endpoints),
- would **exfiltrate document content** if reachable.

The auditor should (a) confirm whether any AI path is reachable in the audited
build, (b) treat the network + key-handling code as live risk regardless, and
(c) flag the plaintext-key fallback (T-API-2) and the data-egress boundary
(T-API-1) even if the feature is gated off. If AI is shipped later, this code
must be re-reviewed with the feature enabled.

---

## 6. Out of scope (see `scope.md`)

Internals of third-party libraries (PoDoFo, PDFium, qpdf, OpenSSL, Tesseract,
Leptonica) are **out of scope** except where GlyphPDF's *use* of them creates a
vulnerability (wrong API usage, missing validation, unsafe defaults). Versions are
pinned in `scope.md` / `LICENSE-3RD-PARTY.md`.
