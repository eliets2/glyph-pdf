# Security Policy

## Reporting a Vulnerability

**Contact:** security@glyphpdf.com

If you have discovered a security vulnerability in GlyphPDF, please report it
privately before public disclosure. We follow a coordinated disclosure model and
will work with you to understand, fix, and disclose the issue responsibly.

**What to include:**
- Description of the vulnerability and its impact
- GlyphPDF version affected (Help > About or MSI installer version)
- Steps to reproduce, including any proof-of-concept PDF if applicable
- Your name/handle for the acknowledgements section (optional)

**Response time:** We will acknowledge within **3 business days** and provide
an initial severity assessment within **7 business days**.

We will not pursue legal action against researchers who report in good faith and
follow this policy.

---

## Coordinated Disclosure Timeline (90-day window)

| Phase | Timeline | Description |
|-------|----------|-------------|
| Acknowledgment | Day 0–3 | Receipt confirmed |
| Initial assessment | Day 0–7 | Severity triaged; fix timeline communicated |
| Fix development | Per SLA below | Fix developed, tested, reviewed |
| Fix release | Per SLA below | Patched version released |
| Public disclosure | Day 90 max | Disclosure with or without fix |

**Severity SLAs (from acknowledgment):**

| Severity | CVSS | Fix SLA |
|----------|------|---------|
| Critical | 9.0+ | 7 days |
| High | 7.0–8.9 | 14 days |
| Medium | 4.0–6.9 | 30 days |
| Low | < 4.0 | Next release |

---

## Scope

**In scope:**
- GlyphPDF application (MSI and source)
- GlyphPDF's *use* of third-party libraries (PoDoFo, PDFium, qpdf, OpenSSL, Tesseract, Qt)
- The redaction pipeline, signing/verification, encryption, sanitization, OCR, and AI provider integrations

**Out of scope:**
- Vulnerabilities *inside* third-party libraries not caused by GlyphPDF's usage
- Denial-of-service against external services (TSA, OCSP, AI endpoints)
- Social engineering or physical attacks

---

## Known Limitations (pre-disclosed)

The following limitations are known and documented; duplicate reports are
appreciated but will be triaged as informational:

| ID | Area | Description |
|----|------|-------------|
| T-RED-1 | Redaction | Image/vector XObjects under a redaction box are covered by a black rectangle but not excised from the content stream. Text redaction performs genuine content-stream excision. |
| T-RED-2 | Redaction | Text painted via Type3 font glyphs (CharProcs content streams) or rendered through tiling-pattern fills is not recursed into during content-stream excision. Such text under a redaction box may survive as graphical marks. Standard (Type1/TrueType/CID) text redaction is unaffected. Mitigation: rasterize-and-re-OCR the page before redacting documents that use Type3 fonts or pattern-filled text. |
| T-API-2 | AI providers | API keys fall back to plaintext QSettings when Windows Credential Manager has no entry. |

---

## CVE Process

For Critical or High findings, GlyphPDF will request a CVE identifier from
MITRE and include it in the public advisory. Reporters are credited in the
advisory and in `CHANGELOG.md` unless they request anonymity.

---

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.x (current) | Yes |
| < 1.0 | No |
