# Auditor Onboarding Checklist

> **Deliverable:** M7-PROMPT-1 / D3
> **Date:** 2026-06-02
> **Owner:** Security coordination (Client side)
> **Use:** Work through this list sequentially on Day 0–1 of the engagement.
> Check off each item and record the timestamp. Send the completed checklist
> to the audit firm lead as confirmation that setup is done.

---

## Pre-engagement (before Day 0)

- [ ] SOW executed by both parties; NDA in place.
- [ ] Audited revision SHA confirmed and recorded in SOW Appendix B:
      `SHA: _______________________________________________`
- [ ] Budget milestone invoice schedule agreed.
- [ ] Kick-off call scheduled: `Date/time: __________________`
- [ ] Firm has returned consultant names per SOW Section 3.
- [ ] Client contact for the engagement named:
      `Name / email / Slack/Signal: _______________________`

---

## Step 1 — Share source code access

- [ ] **Determine access mode** (decide based on whether the repo is public at
      audit time — see `docs/security/auditor-kit/scope.md` §Repo access):
  - [ ] **(a) Public repo:** Confirm the repo URL and pin the audited commit SHA
        in the SOW. No special access steps needed; proceed to Step 2.
  - [ ] **(b) Private repo:** Send a GitHub read-only collaborator invite to the
        auditor's GitHub username(s): `__________________________`
        Confirm they have accepted the invite before proceeding.
  - [ ] **(c) Source tarball:** Generate a signed tarball of the audited commit:
        ```
        git archive --format=tar.gz --prefix=glyphpdf/ <SHA> -o glyphpdf-audit-<SHA>.tar.gz
        ```
        Deliver via [INSERT secure file-transfer method]. Include the SHA and
        the tarball checksum (SHA-256) in the cover email.

- [ ] Auditor confirms they can view the repo / extract the tarball.
- [ ] Auditor confirms they can see the audited commit (not HEAD if HEAD has
      moved).

---

## Step 2 — Provide MSI build for installed-product testing

- [ ] Build `GlyphPDF-1.0.0-x64.msi` from the audited revision:
      ```bat
      cd packaging
      build-msi.bat
      ```
- [ ] Upload MSI to [INSERT secure transfer] and share the download link plus
      the MSI SHA-256:
      `MSI SHA-256: ___________________________________________`
- [ ] Auditor confirms download and SHA-256 match.

---

## Step 3 — Share the auditor kit

Share the following documents from the repository (or as exported PDFs if the
repo is private and not yet accessible):

- [ ] `docs/security/auditor-kit/threat-model.md` — STRIDE model with all
      known issues honestly flagged (especially T-RED-1 image/vector redaction
      gap and T-API-2 plaintext QSettings key fallback).
- [ ] `docs/security/auditor-kit/security-claims.md` — eight README security
      claims annotated with test-verification status (TESTS-VERIFIED /
      PARTIALLY VERIFIED / ASPIRATIONAL / CONTRADICTED).
- [ ] `docs/security/auditor-kit/scope.md` — precise in-scope / out-of-scope
      boundary, including third-party library version pins.
- [ ] `docs/security/audit-environment/test-environment-setup.md` — VM setup
      guide and tool installation steps.
- [ ] `docs/security/audit-environment/adversarial-test-corpus.md` — adversarial
      PDF categories and corpus sources.

- [ ] Auditor lead confirms receipt and has read the threat model.

---

## Step 4 — Clarify out-of-scope items (prevent scope creep)

Walk through the following at the kick-off call and confirm the auditor agrees:

- [ ] **Library internals out of scope** — bugs inside PoDoFo, PDFium, qpdf,
      OpenSSL, Tesseract, Leptonica are out of scope *except* where GlyphPDF
      misuses them. Library-internal crashes surfaced via fuzzing *are* in scope
      as GlyphPDF DoS/RCE findings (we ship the library).
- [ ] **Non-Windows out of scope** — GlyphPDF is Windows-only.
- [ ] **OCR accuracy out of scope** — correctness of Tesseract / RapidOCR output
      is not a security question.
- [ ] **Business logic out of scope** — annotations, forms layout, theming,
      print preview, unless they touch a security asset.
- [ ] **Forbidden / never-linked libraries** — MuPDF and Poppler are never
      linked in-process (CMake `FATAL_ERROR` guard). veraPDF is subprocess-only.
      Auditor should verify the CMake guard holds; they need not audit these
      libraries internally.
- [ ] **AI providers: orphaned in shipping UI** — the Anthropic/OpenAI/Gemini
      providers are present in source but currently unwired in the shipped UI.
      They are **still in scope** for secrets handling and network code review
      (T-API-1, T-API-2). Auditor should confirm reachability in the audited
      build.
- [ ] **Test fixtures are disposable** — signing certs/keys in `tests/fixtures/`
      are test-only; no production secrets are in the repo.

- [ ] Out-of-scope items verbally confirmed with audit lead.
      Notes from call: `________________________________________`

---

## Step 5 — Schedule kick-off call

- [ ] Kick-off call held: `Date / time: ___________________________`
- [ ] Client attendees: `_________________________________________`
- [ ] Firm attendees: `___________________________________________`
- [ ] Auditor's preliminary question list received and answered.
- [ ] Week-1 check-in scheduled: `Date / time: ____________________`
- [ ] Preliminary findings call (end of week 1) scheduled:
      `Date / time: ___________________________________________`

---

## Step 6 — Communication and escalation channels

- [ ] Primary channel agreed: [email / Slack / Signal — INSERT]
- [ ] Emergency escalation (Critical finding mid-engagement): [INSERT — e.g.
      phone number of security lead]
- [ ] Vulnerability embargo understood: firm will not disclose findings until
      coordinated release (see `docs/security/findings-triage/disclosure-policy-draft.md`).
- [ ] Reporting cadence agreed: [INSERT — e.g. brief daily status email, full
      debrief on Day 5 and Day 10]

---

## Step 7 — Build environment verification (for fuzzing)

This step is for the auditor to complete; Client is available to assist.

- [ ] Auditor has set up Windows 10/11 VM per
      `docs/security/audit-environment/test-environment-setup.md`.
- [ ] Auditor has successfully built GlyphPDF from source on MSYS2 ucrt64.
- [ ] Auditor has confirmed MSI installs and all features are accessible.
- [ ] Auditor confirms they can build with AddressSanitizer:
      ```bash
      cmake .. -G Ninja -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
      cmake --build . --parallel 8
      ```
- [ ] Build with ASAN confirmed by auditor: `Date: _________________`

---

## Step 8 — Findings tracking

- [ ] Triage protocol shared: `docs/security/findings-triage/triage-protocol.md`
- [ ] Remediation template shared: `docs/security/findings-triage/remediation-template.md`
- [ ] Agreed format for interim findings submission (e.g. rolling Google Doc /
      private GitHub issue / encrypted email):
      `Format: ________________________________________________`
- [ ] Client engineering contacts per area confirmed:
  - Redaction: `src/engines/podofo/PoDoFoBackend.cpp` → [INSERT owner]
  - Signatures: `src/engines/SignatureManager.cpp` → [INSERT owner]
  - Parsing: `PoDoFoBackend` / PDF parse path → [INSERT owner]
  - AI / credentials: `src/engines/ai/`, `src/core/CredentialManager.cpp` →
    [INSERT owner]

---

## Onboarding complete

- [ ] All steps above checked.
- [ ] Client security lead sign-off: `Name: _________ Date: _______`
- [ ] Audit firm lead acknowledgment received: `Date: ______________`
- [ ] Engagement is live — Day 1 billable day starts: `Date: ________`
