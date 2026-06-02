# GlyphPDF — Security Disclosure Policy (Draft)

> **Deliverable:** M7-PROMPT-1 / D5
> **Date:** 2026-06-02
> **Status:** Draft — will be published as `SECURITY.md` in the repository
> root at public launch (M8-P2). Do not publish until M8-P2 clears.
> **Audience:** Security researchers, external reporters, the audit firm.

---

## Reporting a vulnerability

**Contact:** security@glyphpdf.com

If you have discovered a security vulnerability in GlyphPDF, please report it
to us privately before public disclosure. We follow a coordinated disclosure
model and will work with you to understand, fix, and disclose the issue
responsibly.

**What to include in your report:**

- A description of the vulnerability and its impact.
- The GlyphPDF version(s) affected (check Help > About or the MSI installer
  version).
- Steps to reproduce, including any PoC file or script if applicable.
- Your name/handle for the acknowledgements section (optional).

**Preferred format:** Encrypted email (PGP key: [INSERT — to be published at
launch]) or the GitHub private vulnerability report feature once the repo is
public.

**Response time:** We will acknowledge receipt within **3 business days** and
provide an initial assessment (severity + fix timeline) within **7 business
days**.

---

## What we ask of reporters

- **Do not** publish or share the vulnerability details with others before we
  have released a fix or agreed a disclosure date.
- **Do not** exploit the vulnerability beyond what is necessary to demonstrate
  the issue.
- **Do not** access or modify user data other than your own.

We follow the 90-day coordinated disclosure window described below. We will not
pursue legal action against researchers who follow this policy in good faith.

---

## Coordinated disclosure timeline (90-day window)

GlyphPDF follows a **90-day coordinated disclosure policy**, aligned with
industry standards (Google Project Zero, CERT/CC).

| Phase | Days from report | Description |
| :-- | :-- | :-- |
| Acknowledgment | Day 0–3 | Receipt confirmed. |
| Initial assessment | Day 0–7 | Severity triaged; fix timeline communicated to reporter. |
| Fix development | Day 1–[SLA per severity] | Fix developed, tested, and reviewed. |
| Fix release | Per SLA below | Patched version released. |
| Disclosure | Day 90 max | Public disclosure, with or without fix, at or before Day 90. |

**Severity-specific response SLA (from acknowledgment of the report):**

| Severity | Fix SLA | Disclosure |
| :-- | :-- | :-- |
| Critical (CVSS 9.0+) | 7 days | Coordinated immediately after fix deployment |
| High (CVSS 7.0–8.9) | 14 days | Coordinated after fix deployment, within 90 days |
| Medium (CVSS 4.0–6.9) | 30 days | Coordinated after fix deployment, within 90 days |
| Low (CVSS <4.0) | Next release | At or before 90-day window |
| Informational | No SLA | At GlyphPDF's discretion |

**Extension:** If a fix requires more time than the SLA allows due to complexity
or upstream library dependency, we will communicate this to the reporter with a
revised estimate. We will not let the 90-day window expire without disclosure
even if a fix is not yet available — we will disclose with a documented
interim mitigation.

**Early disclosure:** If a reporter chooses to disclose before 90 days and we
have not yet released a fix, we ask for 7 days of advance notice to coordinate
a simultaneous fix release where possible.

---

## Scope

This policy covers vulnerabilities in:

- The GlyphPDF application (`GlyphPDF-*.msi` and source tree).
- GlyphPDF's use of third-party libraries (PoDoFo, PDFium, qpdf, OpenSSL,
  Tesseract, Leptonica, Qt) — vulnerabilities in GlyphPDF's *use* of these
  libraries are in scope.

**Out of scope:**

- Vulnerabilities *inside* third-party libraries that are not caused or
  exacerbated by GlyphPDF (report those directly to the upstream project).
- Denial-of-service attacks against external services (TSA providers, OCSP
  responders, AI endpoints, update server) — GlyphPDF is a client of these.
- Social engineering, phishing, or physical attacks.
- Vulnerabilities in operating systems or hardware.

---

## Third-party library coordination

If a vulnerability is root-caused in a third-party library (PoDoFo, PDFium,
qpdf, OpenSSL), GlyphPDF will:

1. Implement a local mitigation (workaround, feature gate, or fail-closed
   behavior) within the applicable SLA.
2. Coordinate upstream disclosure with the library maintainers within 90 days.
3. Publish a GlyphPDF-specific advisory crediting the reporter once the upstream
   fix is available or the 90-day window closes.

---

## CVE process

For vulnerabilities that meet CVE eligibility criteria (exploitable, unique,
publicly fixable), GlyphPDF will:

1. Request a CVE from MITRE (https://cveform.mitre.org/) or the appropriate
   CNA for the affected component.
2. Include the CVE ID in the release notes, SECURITY.md update, and any
   public advisory.
3. Share the draft CVE description with the reporter for accuracy review before
   submission.

Reporters who prefer to request the CVE themselves are welcome to do so; please
coordinate with us to avoid duplicate CVE assignments.

---

## What we will do

- Fix confirmed vulnerabilities promptly per the SLAs above.
- Notify you when the fix is released.
- Credit you in the acknowledgements section (see below) unless you prefer
  to remain anonymous.
- Coordinate public disclosure at a mutually agreed time, typically immediately
  after the patched version is released.
- Publish a public security advisory for Critical and High findings.

---

## What we will not do

- Publicly disclose your personal information without your consent.
- Take legal action against you for good-faith research under this policy.
- Delay fixes or disclosures beyond the 90-day window without reporter agreement.

---

## Known limitations and pre-disclosed issues

The following limitations are **known and honestly disclosed** as of GlyphPDF
v1.0.0. They are in active remediation:

| Issue | Affected feature | Status |
| :-- | :-- | :-- |
| Image/vector content under a redaction box is not excised — only covered by a visual black rectangle. Text redaction is correct. | Secure Redaction | Actively being fixed. Redaction of image content will fail-closed (operation refused) until true excision is implemented. |
| AI provider API key may fall back to plaintext storage in Windows registry (`QSettings`) if Windows Credential Manager has no entry. | AI provider key storage | Fix in progress: remove plaintext fallback path. |

Reporting these specific issues is welcome; please note they are already
tracked internally.

---

## Acknowledgements

We are grateful to the security researchers who have responsibly disclosed
vulnerabilities to us. Contributors will be credited here with their permission:

| Researcher | Firm | Vulnerability | Version fixed |
| :-- | :-- | :-- | :-- |
| [Auditor name(s)] | [Trail of Bits / NCC Group / Cure53] | Third-party security audit (all findings) | [1.0.1] |
| — | — | — | — |

---

## Contact

**Email:** security@glyphpdf.com
**PGP key:** [INSERT — fingerprint and key to be published at M8 launch]
**GitHub:** Private vulnerability reports via the Security tab (once the repo
is public)
**Response:** 3 business days for acknowledgment; 7 business days for initial
assessment.

---

*This policy is effective as of the GlyphPDF v1.0.0 public launch date.*
*It will be published as `SECURITY.md` in the repository root at M8-P2.*
