# Statement of Work — Security Assessment
## GlyphPDF v1.0.0

> **Deliverable:** M7-PROMPT-1 / D3
> **Version:** 1.0 (template — fill placeholders before execution)
> **Date:** 2026-06-02
> **Instructions for use:** Replace every `[PLACEHOLDER]` with the agreed value.
> Sections marked `[FIRM TO COMPLETE]` must be filled by the selected auditing
> firm before countersignature.

---

## 1. Parties

| Role | Entity | Signatory |
| :-- | :-- | :-- |
| **Client** | GlyphPDF (project owner) | [INSERT name, title] |
| **Service Provider** | [FIRM NAME] | [INSERT name, title] |

This Statement of Work is entered into under and subject to the mutual
Non-Disclosure Agreement executed between the parties on [INSERT NDA date]
(the "NDA"), which governs confidentiality obligations for information exchanged
under this engagement.

---

## 2. Engagement dates

| Milestone | Date |
| :-- | :-- |
| SOW execution (both parties signed) | [INSERT] |
| Repo / build access granted to firm | [INSERT — Day 0] |
| Kick-off call | [INSERT — Day 1] |
| Engagement start (billable days begin) | [INSERT] |
| Engagement end (billable days end) | [INSERT] |
| Preliminary findings (draft / verbal) | [INSERT — end of week 1] |
| Final report delivered | [INSERT — end of week 2] |
| Remediation window closes | [INSERT — 4 weeks post-report] |
| Re-test period (if elected) | [INSERT] |
| Signed audit letter issued | [INSERT — after re-test sign-off] |

**Total engagement duration:** [INSERT — target 10 business days / 2 weeks]

**Audited revision:** The source-code revision to be assessed is fixed at git
commit SHA `[INSERT SHA]`. No changes to the audited codebase will be made
during the engagement window without written agreement from both parties.

---

## 3. Consultant names and roles

[FIRM TO COMPLETE]

| Name | Role | Seniority |
| :-- | :-- | :-- |
| [Name] | Lead — memory safety / fuzzing | [Senior / Principal] |
| [Name] | Cryptography reviewer | [Senior / Principal] |
| [Name] | [Additional role if applicable] | [Seniority] |

The firm agrees not to substitute key personnel without written notice and
client approval.

---

## 4. Testing methodology

[FIRM TO COMPLETE — describe the methodology to be applied to each surface]

### 4.1 PDF parser robustness (memory safety + fuzzing)

[FIRM TO COMPLETE: describe static code review approach, fuzzing tooling
(libFuzzer / AFL++ / Honggfuzz), harness construction plan, corpus sources,
coverage targets.]

### 4.2 Redaction

[FIRM TO COMPLETE: describe approach to validating text-excision correctness,
image/vector recovery PoC methodology (T-RED-1), and struct-tree MCID scrubbing
coverage.]

### 4.3 AES-256 encryption and certificate encryption

[FIRM TO COMPLETE]

### 4.4 PAdES signatures (ByteRange, DSS/VRI, OCSP, TSA)

[FIRM TO COMPLETE]

### 4.5 Sanitization vectors

[FIRM TO COMPLETE]

### 4.6 AI provider network and key handling

[FIRM TO COMPLETE]

### 4.7 Subprocess invocation (soffice, veraPDF)

[FIRM TO COMPLETE]

### 4.8 Auto-update integrity

[FIRM TO COMPLETE]

### 4.9 Credential storage

[FIRM TO COMPLETE]

**Out of scope (as defined in `docs/security/auditor-kit/scope.md`):** Library
internals of PoDoFo, PDFium, qpdf, OpenSSL, Tesseract, and Leptonica (except
misuse by GlyphPDF); non-Windows platforms; physical attacks; business-logic
features outside the security surface.

---

## 5. Non-disclosure and confidentiality

### 5.1 Client information

All source code, build artifacts, test fixtures, and business information shared
by the Client are confidential under the NDA. The firm will:

- Restrict access to personnel named in Section 3 plus any vetted subcontractors
  disclosed and approved in writing.
- Not retain copies of Client source code beyond [INSERT — recommended: 90 days
  after final report delivery] without written permission.
- Not disclose findings or Client information to any third party without written
  consent, except as required by law.

### 5.2 Coordinated disclosure

Any findings that may constitute exploitable vulnerabilities in third-party
libraries (PoDoFo, PDFium, qpdf, OpenSSL) surfaced during the engagement will
be coordinated with the Client before upstream disclosure. The Client will
initiate upstream disclosure for any library-level findings within 90 days of
the final report, per the coordinated disclosure policy in
`docs/security/findings-triage/disclosure-policy-draft.md`.

### 5.3 Findings embargo

The firm agrees not to publish, present, or otherwise disclose findings
(including vulnerability details, PoCs, and severity assessments) from this
engagement until:

(a) The Client has released a remediated version or public patch, **and**
(b) The Client has issued or authorized public disclosure in writing.

For CRITICAL findings, the embargo expires at the later of: (i) 7 days after
Client confirms a fix is deployed, or (ii) the date the Client requests
coordinated public disclosure. This aligns with the 90-day coordinated
disclosure policy in `disclosure-policy-draft.md`.

---

## 6. Deliverables format and acceptance

| Deliverable | Format | Acceptance criteria |
| :-- | :-- | :-- |
| Findings report | PDF + Markdown | Includes all findings with CVSS v3.1 scores, affected file:line, reproduction steps, root-cause analysis, remediation recommendation |
| PoC for Critical/High findings | Working script / PDF sample | Reproduces the finding on the agreed test environment per `test-environment-setup.md` |
| Remediation guidance | Written section in report or separate document | Specific, actionable fix guidance per finding |
| Fuzzing harnesses | Source code (libFuzzer / AFL++ compatible) + README | Harness builds and runs against the audited revision with provided seed corpus |
| Signed audit letter | PDF, on firm letterhead, signed by lead consultant | Covers: scope, methodology, finding count by severity, remediation status; suitable for public reference |

Preliminary report (draft / verbal) at end of week 1 is a **non-negotiable**
milestone — it allows the Client to begin critical-path remediation while the
engagement is still live.

---

## 7. Payment milestones

[FIRM TO COMPLETE — standard structure shown; adjust as negotiated]

| Milestone | Amount |
| :-- | :-- |
| SOW execution / engagement start | 50% of total fee |
| Final report delivery (all deliverables in Section 6) | 50% of total fee |

**Total fee:** [INSERT — to be agreed at scoping; see RFP Section 6 for budget
envelope]

**Currency:** USD

**Payment terms:** Net [INSERT — 30 days] from invoice date.

**Expenses:** [INSERT — travel (if any), tooling licenses — typically not
applicable for remote-first firms; confirm.]

---

## 8. Intellectual property

The findings report, audit letter, and fuzzing harnesses (Section 6) are
**work-for-hire** delivered to the Client upon final payment. The Client grants
the firm permission to cite "GlyphPDF" in the firm's public client list and to
reference the engagement (in general terms, without disclosing findings) in
marketing materials, after public launch of the product.

---

## 9. Limitation of liability

[FIRM TO COMPLETE — standard professional-services limitation; align with
firm's standard terms]

The firm's liability under this SOW is limited to the fees paid under this
SOW. The firm is not liable for any indirect, consequential, or punitive damages
arising from the audit or reliance on its findings.

---

## 10. Signatures

| | Client | Service Provider |
| :-- | :-- | :-- |
| **Name** | [INSERT] | [FIRM TO COMPLETE] |
| **Title** | [INSERT] | [FIRM TO COMPLETE] |
| **Date** | [INSERT] | [FIRM TO COMPLETE] |
| **Signature** | ________________ | ________________ |

---

*Appendix A: Audited file list — attach `docs/security/auditor-kit/scope.md`*
*Appendix B: Audited revision SHA — confirm at engagement start*
*Appendix C: NDA reference — attach executed NDA*
