# Local Send-for-Signing over PAdES — Implementation Plan (DESIGN ONLY, decision requests)

**Date:** 2026-09-09 · **Repo:** pdf-parity @ `a5840dc`, branch `feat/parity-glm`
**Status:** Design proposal for Tier-1 item T1-4 (`docs/research/synthesis.md` §2). No production
code and no build files are touched by this document. Section 7 carries **decision requests for
the user**; nothing here self-authorizes a standing network consent policy (RQ10 discipline of
`.context/RESEARCH-RECONCILIATION-2026-09-08.md`, extended to consent surfaces).

**Sources for status claims (all read at pinned revision `a5840dc` unless noted):**
`src/core/interfaces/ISignatureManager.h`, `src/engines/SignatureManager.{h,cpp}`,
`src/shell/controllers/SecurityController.{h,cpp}`, `src/ui/SignatureDialog.cpp`,
`src/core/ISecretStore.h`, `src/core/Capability.h`, `src/engines/ai/OllamaProvider.{h,cpp}`,
`src/engines/PdfEditorEngine.cpp`, `src/engines/qpdf/QpdfBackend.h`,
`third_party/podofo/install/include/podofo/main/{PdfSignerCms,PdfSigningContext,PdfSigner,PdfDeclarations}.h`,
`scripts/bootstrap-vendor-deps.sh`, `PRD.md` §9.7/§27/§28,
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, `docs/research/{synthesis,nitro,smallpdf,ilovepdf,bluebeam,acrobat,foxit,pdfxchange,form-js-implementation-plan}.md`,
`docs/research/acrobat.md` §8. Signature commits: `8a278db` (picker), `61fac01` (appearance),
`10efbd5`+`e2da993` (badges).

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE). Web
primaries verified 2026-09-09; engine facts cite code at `a5840dc` or the exact vendored/upstream
tag. **Premise corrections vs. the tasking brief are graded in §3.2 and the closing register.**

---

## 0. Executive summary (3 sentences)

GlyphPDF's signing *engine* is already the strongest in its class — B-B through B-LTA capable
(`ISignatureManager.h` PAdESLevel; PoDoFo `PdfSignerCms` CAdES + OpenSSL CMS/TSA/OCSP/DSS code
verified in `SignatureManager.cpp`) and moat M1 already claims 2/16-competitor exclusivity — but
the shipped product effectively signs **B-B only**, because `setTsaUrl`/`setSignatureLevel` have
**zero production call sites** (tests only) and the "Timestamp document" command cannot succeed
without a TSA URL that no UI can configure. The actual Tier-1 gap is therefore **workflow, not
crypto**: no multi-signer preparation (fields + recipients + order), no signing-request package,
no guided per-signer ceremony, no completion verification, no offline audit-trail artifact — the
exact surface every one of the 8+ cloud-gated competitor implementations monetizes
(`synthesis.md` T1-4). This plan ships that workflow locally on the existing engine in four
phases (P1 preparation+single-signer completion 2–3 wk; P2 consented-TSA B-T 1–1.5 wk; P3
LTV/DSS + honest level labels 1.5–2 wk; P4 B-LTA assessment 0.5–1 wk), requires **no new
dependency**, and leaves exactly three user decisions open: the TSA/OCSP egress consent policy,
opt-in P12-passphrase persistence, and confirmation of two scope cuts (never reminders/never
app-sent email; signing order is advisory, not cryptographic).

---

## 1. Existing-vs-gap analysis

### 1.1 What the signing stack already does (code-verified at `a5840dc`)

**Engine (`src/engines/SignatureManager.cpp`, 2 537 lines + `ISignatureManager.h`):**

| Capability | Where | Notes |
|---|---|---|
| P12/PFX load + chain extraction | `loadP12` (`.cpp` ~275–340) | OpenSSL `PKCS12_parse`; leaf + issuer + extra-chain |
| Key-strength gate | `.cpp` ~1281–1295 | Signing **rejected** for RSA < 2048 bits (M2-P4 choice #1) |
| CMS signing | PoDoFo `PdfSignerCms`, SHA-256 (`signDocumentImpl` ~1319) | SubFilter `ETSI.CAdES.detached` (PAdES_B type); RFC 5652 SignedData via OpenSSL |
| B-T timestamp | `BtPdfSigner::ComputeSignature` ~1325–1341 | RFC 3161 token added as **unsigned attr** `id-smime-aa-timeStampToken` over the signature value; empty token ⇒ honest "downgrades to B-B" |
| B-LT DSS | `buildDssDictionary` ~434–556 | `/DSS` with `/Certs`, `/OCSPs`, `/CRLs`, `/VRI` as incremental update; OCSP fetched via cert AIA, **verified** (`OCSP_basic_verify`) before embedding |
| B-LTA | `TimestampSigner` + `addDocTimestamp` ~560–640 | Custom `PdfSigner` with SubFilter `ETSI.RFC3161`, `/DocTimeStamp` |
| TSA/OCSP egress guard | `httpPost` ~235–250 | **HTTPS-only enforced** — "HTTP URLs are forbidden for TSA/OCSP" (~238) |
| Certification (DocMDP) | `certifyDocument` → `signDocumentImpl(certLevel)` ~1432–1453 | `/DocMDP` transform levels 1–3; **fail-loud**: invalid level or failed transform aborts the operation (no silent approve-signature) |
| Validation | `validateSignatures` ~850+ | `CMS_verify`, trust store = Windows system store + optional PEM dir + test seam (`getTrustStore` ~131–192); OCSP nonce caveat NF-6; DSS-cert-match ER-1 (`NoCertMatch` → `UntrustedChain`); `/DocTimeStamp` entries skipped in the ordinary-signature walk (~875–880) |
| Tamper evidence | `isLegitimateIncrementalAppend` ~1724 (used ~2091) | Post-signature trailing bytes must parse as a legitimate incremental update |
| Degradation honesty | `SignOutcome` 4 states + `SignatureOutcomeDetail` + `lastSignOutcomeDetail` | UI names the **exact** missing B-LT/B-LTA piece with Continue/Retry (audit E-02, §9.7-c `22a7b66`) |
| Visible appearance | `planSignatureAppearance` (`61fac01`) | ETSI EN 319 142-6 §5.2 layout ladder, 6 pt floor, name-only fallback, optional image |
| Field anchors | `signatureFieldAnchors` (`e2da993`) | fieldName/page/rect per signature field — the basis of on-page badges and, below, of multi-signer field placement |
| Signed-doc protection | `PdfEditorEngine.cpp` 281–286, 1432–1439; ledger §9.13 `45aa606` | Redact/erase/optimize refuse signed docs (ER-2 class); signed saves keep incremental `writeUpdate` (EC01) |

**UI/shell:** `SignatureDialog` (P12 + passphrase + reason/location + optional appearance image),
visible-signature picker Draw/Type/Upload/Initials (`8a278db`, `be47cce`), on-page validity
badges, 4 states, view-layer only (`10efbd5`), session signature cache (`706a60c`),
`SecurityController::runSigning` with the restartable immutable `SigningRequest` (N06,
`.cpp`:63–169) — a `PartialLtvMissing` outcome re-runs the *identical* crypto request.

**Secrets:** `ISecretStore` (`ISecretStore.h`) — Windows Credential Manager (DPAPI-backed) with a
labelled AES-256-GCM encrypted-file fallback, **never silent** — already proven in the AI lane
(AR-10 D4 / EC04). The P12 passphrase today lives only in the dialog and the in-memory
`SigningRequest`.

**Encryption (the 2026-09-08 review's warning, confirmed):** DocMDP certification
(`certifyDocument`) and recipient-certificate encryption (`IEncryptor::encryptWithCertificate` +
ER-3 CMS-recipient counter, `IPdfEditorEngine.h:264–275`) **already have code paths**. This plan
plans **zero** duplicate work in either area; the workflow reuses them as-is where relevant.

**Test estate:** `TestSignatureRealCrypto` (real OpenSSL+PoDoFo, synthetic certs; B-B/B-LT/B-LTA
and degradation contracts), `TestSignatureValidation{,Mock}`, `TestValidateAllSignatures`,
`TestSignatureAppearance` (13), `TestSignatureBadges` (15), `TestSignaturePicker`,
`TestSignatureSessionCache`, `TestEraseSignedGuard`, `TestOptimizeSignedGuard`. Test-only
egress precedents: `GLYPHPDF_TESTING` loads a local `*_ocsp_response.der` instead of querying AIA
(`.cpp` ~341–371); tests run with no TSA URL and pin the resulting degradation behavior
(`TestSignatureRealCrypto.cpp:103,150,206–243`).

### 1.2 The honest current-shipment state (load-bearing finding)

- `Private::level` defaults to `B_T` (`SignatureManager.cpp:119`) but **`setTsaUrl` and
  `setSignatureLevel` are never called anywhere under `src/`** (grep-verified: only the
  interface, the implementation, and tests). With an empty TSA URL the B-T attribute branch is
  skipped, so every shipped signature is **effectively B-B** while the internal level knob says
  B_T.
- `SecurityController::timestampDocument()` (`.cpp:874`) calls `addDocTimeStamp`, whose first
  act is `if (tsaUrl.isEmpty()) return false` (`.cpp` ~566). **The "Timestamp document" menu
  action cannot succeed in the shipped app.** It is a dead command awaiting exactly the
  configuration + consent surface this plan's P2 adds.
- `SignatureDialog.cpp` contains **no** level or TSA selector (grep-verified) — there is no user
  path to any PAdES level above B-B.
- Consequence for honesty (moat M8): we must not market "PAdES B-LT/B-LTA" as a *running*
  capability until P2/P3 light it; the engine capability is real, the product surface is not.

### 1.3 The concrete gap to "send-for-signing"

| Workflow capability (T1-4) | State |
|---|---|
| Place **multiple** signature fields in one pass, each bound to a recipient | Missing. Fields exist as a form-field type; `signatureFieldAnchors` reads them; nothing assigns recipients or order |
| Signing-order definition + enforcement | Missing (PDF-XChange sells "Placeholder Tool" placement as its local analog, `pdfxchange.md` §8) |
| Signing-request package export/import (PDF + field map + recipients + state) | Missing. Nearest code idiom: annotation-package JSON export (`SecurityController.cpp:547`) |
| Routing handoff (filesystem/user's own channel — never an online service) | Missing by design; must be built as a pure export |
| Guided per-signer ceremony ("now signer 2 of 3: field Sig2, page 3") | Missing; current dialog is one-shot per signature |
| Completion verification (all fields signed, expected signers, level achieved) | Partial: `validateSignatures` + badges verify *signatures*, not *workflow completeness* |
| Offline audit-trail / completion certificate PDF | Missing (smallpdf.md synthesis: reviewers treat the artifact set as table stakes) |
| Status tracking | Missing locally (cloud tools keep it server-side — see §2.6) |
| Reminders/notifications | Out of scope permanently (cloud anti-recommendation, `synthesis.md` §3) |

PRD §27 row §9.7 marks exactly this: *"Not started: multi-party send-for-signing, signing order,
reminders, status tracking, audit trail"* — the largest remaining Phase-2 gap in GlyphPDF's own
PRD; the v1.5 roadmap names it (PRD §28).

---

## 2. Scope decision matrix — what "local send-for-signing" means for an offline-first app

### 2.1 What the competitors actually ship (and what survives local inversion)

| Competitor surface | Mechanism | Local equivalent |
|---|---|---|
| Nitro Sign: multi-party send, recipients/order, templates, bulk, reminders/status, audit trail with secure action record (nitro.md A2, verdict TRUE) | Cloud envelope; desktop uploads | **Keep:** field map, recipients, order, audit record. **Drop:** upload, server state, reminders. nitro.md's own build implication: "export a signing-request package… route by email, import completed copies, verify… generate an offline audit-trail PDF; ship signing order + status as state tracked in a local manifest" |
| Smallpdf/Sign.com: activity timeline, **Certificate of Completion**, signing order, Document IDs, access codes, digital sealing (smallpdf.md §8) | Cloud (2 docs/month free) | Keep all artifacts as **local files**; access codes/sealing are unnecessary when nothing is hosted — replaced by the package's file-level security (optional password via existing PDF encryption) |
| iLovePDF iLoveSign: signers list, required/optional fields, signing order, expiry, reminders, per-signer language (ilovepdf.md §1.8) | Cloud | Keep signers/fields/order as manifest data; expiry = optional prepared-copy read-only XMP expiry (already LANDED, §9.11); drop reminders |
| Acrobat Sign: send, order, reminders, status, audit trail (acrobat.md §8) | Cloud, anti-feature | Same local inversion |
| Bluebeam: **no send-for-signing at all** — "that space belongs to Studio partners" (bluebeam.md §8, TRUE-absence) | — | Confirms a local tool can ship the workflow without any SaaS and still exceed the AEC species |
| PDF-XChange Placeholder Tool (v11, Plus): *mark where signatures/initials go* (pdfxchange.md §8) | Local | Direct precedent for our P1 field-placement UX — but ours binds each placeholder to a recipient and an order |

Tool demand for the workflow class: 8+/16, every implementation cloud-gated (`synthesis.md`
T1-4). The cloud envelope backend is a standing anti-recommendation (`synthesis.md` §3.3;
nitro.md anti: "build the local package workflow instead; do not chase Nitro Sign's envelope
backend").

### 2.2 The signing-request package (new artifact, manifest-first)

A **file pair** handed across the filesystem — the app never transmits anything:

- `Contract.pdf` — the prepared document (signature fields already placed and named).
- `Contract.sigreq.json` — the manifest. JSON, versioned, in the exact idiom of the existing
  annotation-package export (`SecurityController.cpp:547`: `version` / `exported_at` / `source`
  / payload):

```
{
  "glyphpdf-signing-request": 1,
  "created": "2026-09-09T12:00:00Z",
  "source_pdf": "Contract.pdf",
  "source_pdf_sha256": "…",           // identity of the PREPARED (unsigned) bytes
  "pades": { "target_level": "B-T", "consent": { "tsa": "ask|granted|denied", "decided_at": … } },
  "signers": [
    { "id": "s1", "display_name": "A. Buyer", "contact": "", "order": 1,
      "reason": "Approval", "location": "Munich",
      "fields": [ { "field": "Sig_Buyer", "type": "signature", "page": 3, "required": true } ],
      "status": "pending" | "signed" | "declined", "signed_copy": "" }
  ],
  "timeline": [ { "at": "…", "event": "prepared|exported|imported|signed(s1)|completed", "detail": … } ]
}
```

Design rules: (a) the manifest is **advisory data with a cryptographic anchor** — `source_pdf_sha256`
binds it to the prepared bytes; (b) it is updated locally as the round-trip progresses (the
timeline rows are the offline "activity timeline"); (c) a *completed* package's audit-trail PDF
(§2.5) is generated from the manifest + `validateSignatures` output and is itself just a PDF —
no service holds custody (contrast iLovePDF's 5-year server custody, ilovepdf.md §1.8).
Optional hardening (not P1): distribute the pair inside the PRD-claimed §9.11 encrypted-ZIP
secure package — the code site was **not located in this pass** (PRD.md:385 row only; verify at
integration before relying on it — UNVERIFIED as a reusable seam).

### 2.3 Routing handoff (zero-egress)

The app's entire transport role is: **export the package to a folder the user chooses; open
nothing; send nothing.** The UI copy says so: "Hand this pair to signer 1 by your own means
(email, share, USB). GlyphPDF never uploads anything." Completed copies return the same way and
are imported via file picker. This is the nitro.md A2 handoff verbatim, minus email *sending* by
the app (mailto: or MAPI composition is a **user decision we explicitly do not take by default**;
see §7 D3).

### 2.4 Signing ceremony UX (per signer, on the signing machine)

1. Import package (or just open the PDF — the ceremony must degrade gracefully to a plain
   single-signer flow; the manifest is optional input, never a gate for the crypto path).
2. Guided state: "Package: Contract — signer 2 of 3 (A. Buyer). Your field: `Sig_Buyer`, page 3."
   → viewer navigates to the anchor (`signatureFieldAnchors` already returns page+rect; badges
   already overlay that rect).
3. The existing `SecurityController::runSigning` request executes against the anchor's field —
   no new crypto code; what's new is **which field** is pre-selected and the visible order state.
4. On success, the ceremony marks `status: signed` in the local manifest copy and appends a
   timeline row; `SignOutcome::PartialLtvMissing` keeps its existing honest Retry/Keep dialog.
5. Certification interplay (existing DocMDP path, reused): if the *preparer* wants a
   certification signature, it is applied at preparation time with `/DocMDP` level 2 (form-fill +
   allowed) so later recipient signatures remain possible — level 1 would lock the document and
   the ceremony must refuse further signing with the existing fail-loud behavior.

### 2.5 Completion verification + audit-trail artifact

- **Per return:** validate all signatures (`validateSignatures`), map each valid signature back
  to a manifest signer by **field name** (byte-range/revision identity via
  `isLegitimateIncrementalAppend`-style trailing checks already exists for shadow detection),
  verify order compliance *as data* (sequence of signatures in revision order vs. manifest
  order), and check "all required fields signed".
- **Honesty rules:** out-of-order signatures are flagged, not hidden ("signer 2 signed before
  signer 1 — verify this was agreed"); a signature that fails validation never silently
  completes the package; a signer whose certificate identity cannot be matched to the manifest
  display name yields an explicit discrepancy row.
- **Audit-trail PDF (completion certificate):** generated locally at completion — document
  identity (SHA-256 of prepared + each signed revision), signer certificate subjects +
  fingerprints, claimed time (`/M`) vs. trusted time (TSA token genTime, when present), achieved
  PAdES level **as measured** (§3.3), validation outcome per signature, and the manifest
  timeline. Writer choice (QPdfWriter vs. PoDoFo document build) is an integration decision, not
  a design fork; both are already in-stack.

### 2.6 Explicitly OUT (permanent scope cuts — requested for confirmation, §7 D3)

- Any online envelope/service, accounts, web signing URLs (`synthesis.md` §3.1/§3.3).
- Reminders/notifications (needs a server or a scheduler-with-egress; both anti).
- App-initiated email transmission (zero-egress; user's mail client is the transport).
- Cloud custody/retention of signed copies.
- Cryptographic *enforcement* of signing order (ISO 32000-2 signature fields do not carry an
  order constraint; DocMDP + field locks + manifest verification are the honest mechanism — we
  claim guidance and verification, not enforcement).

---

## 3. PAdES level strategy

### 3.1 The levels (ETSI EN 319 142-1 V1.2.1, 2024-01 — primary source, fetched this pass)

| Level | Requirement (standard) | Trusted time? | Revocation data travels with doc? | Network needed at sign time? |
|---|---|---|---|---|
| **B-B** | Baseline CAdES over PDF: content-type (`id-data`), message-digest, `SignedData.certificates` incl. signing cert (ESS signing-certificate[-v2]); **signingTime CMS attribute shall NOT be present** (claimed time lives in `/M`); MD5 shall not be used; SubFilter `ETSI.CAdES.detached`; single signer per PDF signature dict (EN 319 142-1 §5.1–5.2, req (a)) | No (claimed `/M`) | No | **No** |
| **B-T** | B-B + proof of existence of the signature from a TSP: **signature-time-stamp** unsigned attribute **or** a **document-time-stamp** (§5.3) | Yes (RFC 3161) | No | **Yes — TSA** (unless an offline token file is supplied) |
| **B-LT** | B-T + certificate and revocation values embedded so validation is possible later: the **DSS shall be present** (≥1); VRI **should not be used** (§5.4, req v) | Yes | **Yes** (`/Certs`, `/OCSPs`, `/CRLs`) | **Yes — OCSP responder/CRL DP** (or pre-fetched data) |
| **B-LTA** | B-T/LT + proof of existence of the *validation data*: at least one **document-time-stamp** after the DSS is complete — **including the TSA's own chain/revocation material**; repeated (periodic) archive timestamps extend validity (§5.6/§6) | Yes | Yes | **Yes — TSA, recurring** |

Normative plumbing used by these levels and already cited by the standard: RFC 3161
(`TimeStampReq.messageImprint/nonce/certReq`; `TSTInfo.genTime/serialNumber`; token = CMS
SignedData with eContent `id-ct-TSTInfo` 1.2.840.113549.1.9.16.1.4; TSA cert carries critical
EKU `id-kp-timeStamping`) and RFC 5816 (ESSCertIDv2). CAdES baseline attribute obligations come
via EN 319 122-1 (referenced normatively); PoDoFo's `PAdES_B` mode contributes the
signing-certificate-v2 attribute and the correct SubFilter (verified from the upstream 1.1.0
tag, §4.1).

### 3.2 Premise correction (graded)

The tasking brief names the levels "B-B, B-T, B-LT, B-A". **"B-A" is not a PAdES level** — the
PAdES archive level in EN 319 142-1 is **B-LTA** (the B-A abbreviation belongs to the CAdES/XAdES
families, EN 319 122-1/132-1). All "B-A" statements below are about B-LTA. Grade: correction from
the primary standard, TRUE.

### 3.3 What we claim at each level — and what we will never claim

- **B-B (today's effective behavior):** "Tamper-evident cryptographic signatures; time claimed
  by the signer, not certified." Honest about `/M`.
- **B-T (P2):** "Signature time certified by an independent timestamp authority you approved for
  this document." If the TSA call fails ⇒ existing honest downgrade path (B-B + notice), never a
  silent B-T claim.
- **B-LT (P3):** "Validation data (chain + revocation) travels inside the document — verifiable
  long after signing." Level labels in the UI are **derived from validation state** (DSS
  present? token present? DocTimeStamp present? — all three probes already exist as
  `SignatureInfo::hasDss/hasDocTimestamp` + subfilter checks), never from the *requested* level.
- **B-LTA (P4, assessment):** "Archival timestamping" only if we are willing to do it honestly —
  a single DocTimeStamp after LTV data is cheap (engine exists: `TimestampSigner`); a true
  archival policy needs **periodic re-timestamping**, i.e. recurring consented egress and a
  scheduler. Recommendation: ship the one-shot archive timestamp, **defer the recurring
  schedule** (§6 P4), and never label a document B-LTA unless a current (non-expired) archive
  timestamp chain exists.
- **Never claimed (any level):** eIDAS qualified signatures / QES; AATL/EUTL trust-list
  membership (we read Windows trust store + user-imported roots — displayed honestly as
  `trustStoreUsed`); legal-binding advice; cryptographic signing-order enforcement (§2.6);
  "PAdES B-LT/B-LTA" as a *running* product claim until P2/P3 land (§1.2).

### 3.4 The egress reality and the consent surface

B-T/B-LT/B-LTA are **impossible fully offline** — RFC 3161 tokens and fresh OCSP/CRL data come
from network services by definition (§3.1). The zero-egress app already owns the two precedent
patterns this surface must copy:

1. **Loopback/HTTPS guard:** the AI lane blocks cleartext HTTP to non-loopback hosts at the
   provider boundary (`OllamaProvider.cpp:21–94`, R04); the TSA/OCSP path is **already
   HTTPS-enforced** inside `httpPost` (`SignatureManager.cpp` ~238). P2/P3 reuse, never relax, it.
2. **Consent gates:** AI runs behind explicit user configuration with first-run consent
   semantics (AR-8 D6, default OFF, `GpMainWindow.cpp:1221`); the form-JS design specifies
   per-document consent with a remembered decision; telemetry is opt-in only.

**Design (P2):** a per-document consent dialog at first TSA/OCSP use: *"Signing can embed a
certified timestamp from `https://tsa.example…`. This is the only network call; the document
bytes are not uploaded — only a hash."* (True by construction: RFC 3161 transmits the
messageImprint **hash**, RFC 3161 §2.4 — the strongest privacy sentence in the whole design and
it is standard-mandated.) Buttons: Allow once / Allow for this document (remembered in the
manifest + session) / Deny (signs B-B, honest label). The default for a *fresh* document is
**ask**; a global "never network" switch forces B-B everywhere and is the CapabilityRegistry's
`whyNot` answer ("TSA/OCSP egress disabled in settings — signature stays B-B"). The current
`timestampDocument()` dead command (§1.2) becomes this dialog's first consumer.

---

## 4. Feasibility with current dependencies (result: **no new dependency required**)

### 4.1 PoDoFo 1.1.0 — verified from the vendored install + the upstream tag the bootstrap clones

`scripts/bootstrap-vendor-deps.sh:54` clones `podofo` tag 1.1.0 from GitHub; headers in
`third_party/podofo/install/include/podofo/main/` + upstream `PdfSignerCms.cpp` @ 1.1.0
(fetched 2026-09-09):

- **`PdfSignerCms`** (RFC 5652 CMS): `PdfSignerCmsParams.SignatureType = PAdES_B` ⇒ SubFilter
  `ETSI.CAdES.detached`, **adds signing-certificate-v2**, **suppresses** MIME-capabilities and
  signing-time — exactly the EN 319 142-1 §5.2 attribute posture (3.1). The file references **no
  signature-policy/commitment-type attribute**: EPES-style policies are not built in, but
  `PdfSignerCms::AddAttribute(nid, attr, SignedAttribute)` is the provided seam should they ever
  be wanted (out of scope here).
- Content-type + message-digest signed attributes: not referenced in `PdfSignerCms.cpp` itself;
  SignedData assembly is delegated to OpenSSL's CMS API, which adds both per RFC 5652 §11.1
  mandatory-attribute rules (MOSTLY_TRUE — inferred from OpenSSL CMS semantics + our
  signatures validating in `CMS_verify`; confirm once with a DER dump in a P1 test, already
  planned).
- **`PdfSigningContext`**: multi-signer configuration (`AddSigner` per signature field),
  **deferred signing** (Start/Finish, `FetchIntermediateResult` — hash-outsourcing, e.g. for
  future smartcard/HSM paths), and `DumpInPlace`/`Restore` of a signing context. GlyphPDF
  currently drives the simpler `SignDocument(doc, device, signer, signature)` one-signer path
  (`SignatureManager.cpp:1513,626`); the multi-signer *ceremony* does **not** need
  `PdfSigningContext` because each recipient signs a separate returned copy in a separate
  incremental update — the existing sequential path is correct and simpler.
- `PdfSignature`/widget creation (`CreateField<PdfSignature>`, hidden flags for DocTimeStamp —
  already used at `.cpp` ~572–580) suffices for P1 field placement; the visible-signature
  appearance planner is ours (`61fac01`).
- **Gaps in PoDoFo (accepted, worked around, not dependencies):** no DSS builder (ours,
  `buildDssDictionary`), no RFC 3161 client (ours, `fetchTimestampToken`), no OCSP client
  (ours, OpenSSL `OCSP_*`), no revocation-aware validator (ours, `CMS_verify` + `X509_STORE` +
  DSS extraction).

### 4.2 OpenSSL (in-stack) covers the rest

P12 parse, CMS sign/verify, RFC 3161 request building, OCSP request/basic-verify, CRL parsing,
X509_STORE with Windows system roots — all already exercised by `SignatureManager.cpp` (§1.1
table). **The OCSP/CRL "stack" some taskings fear must be added is already the OpenSSL code
above; no new library, no vendored crypto, no `LICENSE-3RD-PARTY.md` row.**

### 4.3 Other engines

- **qpdf** (`QpdfBackend.h`): linearize/repair/inspect only — no signing role; linearization is
  already *refused* for signed docs (EC01).
- **PDFium**: rendering/read path only; used for visual verification in tests (render signed
  pages to confirm appearance layers), unchanged.
- **Qt Network** (`QNetworkAccessManager`): already linked for the AI lane; reused for TSA/OCSP
  POSTs behind the consent gate — not a new dependency either.

**Conclusion: P1–P4 are buildable entirely on existing code + existing deps.** The only
"user-decision" items are policy (§7), not technology.

---

## 5. Security model

### 5.1 Private-key handling

- Today: P12 chosen per signing dialog; passphrase lives in the dialog and in the in-memory
  immutable `SigningRequest` (N06) for the duration of one restartable operation; nothing
  persists it. RSA < 2048 refused pre-sign (§1.1).
- P1 change (small, opt-in): offer **"remember passphrase for this certificate"** backed by
  `ISecretStore` (service key = certificate SHA-256 fingerprint), giving DPAPI-backed Windows
  Credential Manager storage with the labelled encrypted-file fallback and **no silent fallback**
  (`ISecretStore.h` contract). Default remains session-only. The ceremony never writes the
  passphrase into the manifest, the PDF, or settings. *(D2, §7.)*
- Multi-signer note: the *preparer* never holds recipients' private keys — the package contains
  public-field metadata only. This is what makes the local workflow privacy-superior to cloud
  envelopes by construction.

### 5.2 Trust assessment display (honest, already partly built)

- `SignatureInfo.trustStatus/trustStoreUsed`, badges (4 states), and the `NoCertMatch →
  UntrustedChain` mapping (ER-1) are LANDED. P1 extends the *wording*, not the crypto: a
  signature from a self-signed / non-chain-rooted certificate is displayed as **"chain not
  validated against a trusted root — signer identity is self-asserted"** rather than a bare red
  X, matching the honesty moat (M8) — the state is *real* and must be distinguishable from
  *tamper*. P3 adds the level label derived from measured state (§3.3).

### 5.3 Tamper evidence after each signature

- Post-sign incremental-append legitimacy check (`isLegitimateIncrementalAppend`) + `CMS_verify`
  + ByteRange coverage are LANDED and run on every `validateSignatures`.
- Workflow additions (P1): after each ceremony step and on every package import, (a) validate,
  (b) confirm every *earlier* signature still validates (an invalid earlier signature after a
  later signing = hard stop with a what-changed row), (c) record the revision's SHA-256 into the
  manifest timeline — the offline audit trail's spine.

### 5.4 Consent-gated egress policy (summary of §3.4)

| Action | Network | Gate |
|---|---|---|
| B-B sign, validate, package export/import, audit PDF | none | none |
| B-T sign or `addDocTimeStamp` | TSA (HTTPS POST, hash-only payload — RFC 3161) | per-document consent dialog, remember-for-document; global kill switch |
| B-LT DSS build | OCSP responder / CRL distribution point (HTTPS-only, existing guard) | same gate, same memory |
| B-LTA archive timestamp | TSA | same gate; *recurring* re-timestamping = standing consent → deferred (P4) |

Denied ⇒ degrade one level with the existing `SignOutcome` honesty machinery; never silently.
The RQ05 lesson (CapabilityRegistry is disclosure, not enforcement) is respected: the *enforcement
point* is the `httpPost` boundary (already HTTPS-only), the *disclosure point* is the
CapabilityRegistry + consent dialog.

---

## 6. Phased plan (anchors at `a5840dc`)

### P1 — Send-for-signing preparation + single-signer completion (2–3 weeks)

- **Preparation mode** (new, UI+manifest only): place N signature fields (reuse
  `FormBuilderMode`/auto-detect patterns and `CreateField<PdfSignature>`), name + assign
  recipients, set order/reason defaults; write `*.sigreq.json` beside the PDF (idiom:
  `SecurityController.cpp:547`); optional preparer DocMDP-2 certification via the **existing**
  `certifyDocument`.
- **Ceremony:** package import → guided next-signer state → pre-select the field via
  `signatureFieldAnchors` → existing `runSigning` path → manifest/timeline update.
- **Completion:** field-mapped verification + discrepancy rows + local audit-trail PDF.
- **Tests** (`tests/TestSigningPackage.cpp`, style of `TestSignaturePicker.cpp`): manifest
  round-trip; prepared-copy SHA-256 binding; ceremony state machine (pending→signed→complete,
  out-of-order flag, declined); two-signer sequential real-crypto round-trip (extend
  `TestSignatureRealCrypto` fixtures: two synthetic P12s, sign, re-validate, DocMDP-2 honored);
  tamper test (flip a byte outside the newest ByteRange ⇒ earlier signature invalid ⇒ completion
  refused); audit PDF contains the measured level + both certificate fingerprints.
- **Effort: 2–3 weeks.** Risk: field-naming collisions with existing AcroForms (mitigate: name
  lint + auto-uniquify, tested).

### P2 — B-T via consented TSA (1–1.5 weeks)

- TSA URL + level settings UI (Preferences + per-sign dialog), persisted in QSettings, **default
  empty = B-B**; per-document consent dialog with remember (`§5.4`); retire the dead
  `timestampDocument()` behavior by wiring it to the same consent surface.
- **Deterministic TSA stub for tests: loopback-only** — a tiny HTTPS server on `127.0.0.1` bound
  in-process for the test binary only, answering RFC 3161 requests with tokens signed by a
  synthetic TSA cert (EKU `id-kp-timeStamping`, per RFC 3161 §2.3). Precedents:
  `GLYPHPDF_TESTING` local OCSP DER loading (`SignatureManager.cpp` ~341) and the R04
  loopback-only endpoint guard (`OllamaProvider.cpp:87–94`) — the stub *is* a loopback literal,
  so the guard and the production HTTPS rule both stay intact. Token verification asserts
  `genTime` ≥ `/M`, imprint match, nonce echo (RFC 3161 §2.4 requester duties).
- **Tests:** consent matrix (allow-once / remember / deny ⇒ B-B + honest label + CapabilityRegistry
  wording); TSA failure ⇒ existing B-T downgrade path pinned (`TestSignatureRealCrypto.cpp:206–243`
  already pins the no-TSA degradation — extend to with-TSA success); stub round-trip; deny
  leaves zero outbound connections (mock-network assertions).
- **Effort: 1–1.5 weeks.** Risk: Windows TLS to real TSAs (Schannel via Qt) — test against a
  public TSA once, manually, not in CI.

### P3 — LTV/DSS embedding + validation display (1.5–2 weeks)

- Expose `setSignatureLevel(B_LT)` through the same consent gate (OCSP/CRL fetch already
  implemented + verified-before-embed); add **after-the-fact LTV augmentation** of an already
  signed package copy (DSS build is already an incremental update — `buildDssDictionary` runs
  post-sign today; factor it into a public "add validation data" op for completed packages).
- Honest level labels from measured state (§3.3) in badges + audit PDF.
- **VRI alignment item (found in primary source):** EN 319 142-1 §5.4 req (v) says VRI *should
  not be used*; our DSS writes `/VRI` (`SignatureManager.cpp` ~540). Keep writing it for
  reader compatibility in P3 but add a tracked task to make it optional behind the level setting,
  and validate a major-viewer sample (Acrobat/Foxit) reads the DSS — NF-6's deferred VRI work
  already anticipated this.
- **Tests:** DSS presence/absence ⇒ label matrix; OCSP-fetch-failure ⇒ `PartialLtvMissing` with
  `dssMissing` (existing contract, `TestSignatureRealCrypto.cpp:150`); augmentation round-trip on
  a P1 two-signer artifact (earlier signatures still valid after DSS append); DER-dump test that
  the CMS carries content-type + message-digest + signing-certificate-v2 (closes the §4.1
  MOSTLY_TRUE).
- **Effort: 1.5–2 weeks.**

### P4 — B-LTA assessment (0.5–1 week, likely defer the recurring part)

- Ship: one-shot `/DocTimeStamp` after complete DSS (engine exists: `TimestampSigner`; gating:
  TSA consent) — gives an honest "archive timestamp present" state.
- Assess + likely defer: **periodic re-timestamping** (background scheduler + standing consent +
  cert-expiry horizon math). Decision input: who runs archival for years offline without a
  service? If nobody in the target personas, defer with a CapabilityRegistry `whyNot`
  ("periodic re-timestamping not scheduled — document remains B-LT-valid; archive timestamp
  ages").
- **Effort: 0.5–1 week for the assessment + one-shot; recurring = separate decision.**

---

## 7. Decision requests (user-gated)

| # | Decision | Options | Recommendation |
|---|---|---|---|
| **D1** | **Standing TSA/OCSP consent policy** (the only policy question with security weight) | (a) per-document ask + remember-for-document + global never-network switch; (b) global opt-in once, silent afterwards; (c) never network — B-B forever | **(a)** — matches AR-8/consent precedent and R04 guard discipline; (c) remains available as the settings default for hardened installs |
| **D2** | P12 passphrase persistence | session-only (today) vs. opt-in `ISecretStore` per certificate fingerprint | **opt-in, session-only default** — reuses the proven EC04/AR-10 store, zero new storage surfaces |
| **D3** | Scope-cut confirmation | (i) no reminders/notifications ever; (ii) no app-initiated email (user's own channel transports the package); (iii) signing order = guidance + verification, never claimed as cryptographic enforcement | Confirm all three as permanent anti-features (they are the cloud species' distinguishing machinery, `synthesis.md` §3) |

No new dependency is requested in any phase (§4) — unlike the form-JS plan, this document asks
for **no `LICENSE-3RD-PARTY.md` row**.

---

## 8. TL;DR (10 lines) + recommendation

1. The signing engine is already B-B→B-LTA capable and honestly degrading — the moat's crypto is real.
2. The shipped product still signs effective B-B: no production code ever sets a TSA URL or level.
3. The "Timestamp document" command is dead until that configuration + consent exists (P2 fixes it).
4. The Tier-1 gap is the workflow: prepare fields→recipients→order, package, hand off, ceremony, verify, audit PDF.
5. Every competitor implementation of that workflow is cloud-gated (8+/16); the local inversion is the product.
6. Package = PDF + versioned JSON manifest in the existing export idiom; transport = the user's own hands.
7. B-T/B-LT/B-LTA require TSA/OCSP egress by definition; consent-gate them per-document (hash-only TSA payload, RFC 3161).
8. No new dependency: PoDoFo 1.1.0 (CAdES, verified from tag) + OpenSSL + Qt Network cover everything.
9. Four phases: P1 workflow 2–3 wk; P2 consented B-T 1–1.5 wk; P3 LTV + honest labels 1.5–2 wk; P4 B-LTA assess ≤1 wk.
10. Three decisions requested: consent policy (D1), passphrase persistence (D2), scope-cut confirmation (D3).

**Recommendation:** proceed with P1 immediately on the existing engine (no dependency, no
egress, pure workflow + UI + manifest over LANDED crypto), with P2's consent-gated TSA surface
designed now and built right after; adopt D1(a)/D2(opt-in)/D3 as specified. The honest-claim
rule is non-negotiable throughout: the UI states the PAdES level *measured from the document*,
never the level that was *requested*.

---

## 9. Source register

**Codebase (pinned `a5840dc`):** `ISignatureManager.h:33–50,56–59,61–108`; `SignatureManager.h:23–64,96–110,122–147`; `SignatureManager.cpp:115–130` (defaults), `:131–192` (trust store), `:199–250` (TSA + HTTPS guard), `:275–340` (P12), `:341–433` (OCSP + AIA + test fixtures), `:434–556` (DSS/VRI), `:560–640` (DocTimeStamp signer), `:875–880` (validation walk), `:917–947` (sign entries), `:1222–1341` (impl core, B-T attr), `:1432–1453` (DocMDP), `:1490–1526` (post-sign B-LT/B-LTA), `:1658–1697` (certify), `:1724,2091` (append legitimacy); `SecurityController.h:32–60`; `SecurityController.cpp:63–169` (SigningRequest/runSigning), `:874+` (timestampDocument), `:547–570` (package-export idiom); `ISecretStore.h:22–50`; `Capability.h:36–121`; `OllamaProvider.cpp:21–94`; `PdfEditorEngine.cpp:281–286,1432–1439`; `QpdfBackend.h:1–13`; `GpMainWindow.cpp:1221`; `IPdfEditorEngine.h:264–275`; `bootstrap-vendor-deps.sh:54,124`; `CMakeLists.txt:118–154,457–465`; tests as listed in §1.1; commits `8a278db`, `61fac01`, `10efbd5`, `e2da993`, `22a7b66`, `706a60c`, `be47cce`, `45aa606`.

**PRD/ledger/research:** `PRD.md` §9.7 (`:179–189`), §27 (`:381`), §28 (`:406`), §9.11 row (`:385`); `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md:44–46,80–82,110–111`; `docs/research/synthesis.md` §2 T1-4, §3.3, §4 M1/M8; `docs/research/{nitro.md A2, smallpdf.md §8, ilovepdf.md §1.8, bluebeam.md §8, acrobat.md §8, foxit.md (Signatures delta), pdfxchange.md §8}`; `docs/research/form-js-implementation-plan.md` (format, consent patterns, RQ10 discipline).

**Web (verified 2026-09-09, primary):**
- ETSI EN 319 142-1 **V1.2.1 (2024-01)** full text: <https://www.etsi.org/deliver/etsi_en/319100_319199/31914201/01.02.01_60/en_31914201v010201p.pdf> — levels §6, baseline attributes §5.2, B-T §5.3, B-LT/DSS/VRI §5.4 (req v: VRI should not be used), DocMDP note §5.4.2.3, encryption §5.5, extensions §5.6, B-LTA procedure §6/§6.2, normative refs RFC 3161 + RFC 5816. (Version index: <https://www.etsi.org/deliver/etsi_en/319100_319199/31914201/>; a V1.3.0 draft-stage entry dated 2026-08 exists — cited text is the published V1.2.1.)
- RFC 3161: <https://datatracker.ietf.org/doc/html/rfc3161> (TimeStampReq/TSTInfo/messageImprint/nonce, `id-ct-TSTInfo` 1.2.840.113549.1.9.16.1.4, TSA EKU §2.3, requester verification duties §2.2, HTTP transport §3).
- PoDoFo tag 1.1.0 `PdfSignerCms.cpp` (the exact tag `bootstrap-vendor-deps.sh` clones): <https://raw.githubusercontent.com/podofo/podofo/1.1.0/src/podofo/main/PdfSignerCms.cpp> — `PAdES_B` ⇒ `ETSI.CAdES.detached` + signing-certificate-v2, MIME-caps/signing-time suppressed, no policy/commitment attribute; `AddAttribute` seam. Vendored headers: `third_party/podofo/install/include/podofo/main/{PdfSignerCms,PdfSigningContext,PdfSigner,PdfDeclarations}.h` (deferred signing, `AddSigner`, `PdfSignatureType::PAdES_B`).

**Premise corrections vs. tasking brief (graded):** (1) "B-A" is not a PAdES level — the archive
level is **B-LTA** (EN 319 142-1; TRUE). (2) "DocMDP certification and recipient-certificate
encryption have code paths" — confirmed and treated as reuse-only (`certifyDocument`,
`IEncryptor::encryptWithCertificate`; TRUE). (3) "B-T and B-LT require RFC3161 TSA / OCSP-CRL
access" — confirmed with the refinement that B-LT's *validation-data* fetch is the egress; DSS
embedding itself is local (TRUE). (4) Tasking-implied "needs an OCSP/CRL stack or better crypto
lib as a possible new dependency" — **resolved FALSE**: the OpenSSL-based OCSP/CRL/CMS/TSA stack
is already in-tree and exercised (§4.2). (5) Synthesis moat M1's "PAdES B-LT/B-LTA shipped"
stands for the *engine*; this doc qualifies it as not-yet a *product surface* (§1.2) — an
honesty refinement, not a contradiction.
