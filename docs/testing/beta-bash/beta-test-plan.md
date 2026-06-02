# GlyphPDF v1.0.0 Beta Test Plan

**Milestone:** M7-PROMPT-2, D3  
**Document status:** Ready for distribution  
**Version under test:** GlyphPDF 1.0.0-beta (internal MSI build from HEAD)  
**Date issued:** 2026-06-02

---

## 1. Objectives

The primary goal of the beta bash is to discover and triage release-blocking defects
before the public v1.0.0 launch (M8 ship gate). Specific objectives:

1. Surface any crash, data-loss, or security-bypass defect in the production MSI build
   under real user workflows on real document corpora.
2. Validate that every major feature listed in README.md functions correctly end-to-end
   for power users and PDF professionals.
3. Confirm that the installer, auto-update, and file-association flows work on both
   Windows 10 and Windows 11.
4. Generate a triaged backlog distinguishing release-blockers (must fix before M8) from
   post-launch candidates (v1.1 deferred).

The ship gate is: **zero Critical or High severity bugs outstanding** at the close of the
bash period. Medium/Low bugs are tracked but do not block ship.

---

## 2. Tester Profiles

We want three distinct tester cohorts:

### Cohort A — PDF Professionals (target: 3-5 testers)
- Legal secretaries, paralegals, or compliance officers who work with PDF forms,
  redaction, and digital signatures daily.
- Primary focus: Forms, Security (signing/redaction/encryption), document fidelity.
- Representative corpus: dense legal agreements, government forms, redacted filings.

### Cohort B — Power Users / Knowledge Workers (target: 3-5 testers)
- Technical writers, researchers, or analysts who annotate, export, and batch-process
  large PDF collections.
- Primary focus: Annotation suite, Batch processing, Conversion (HTML/text/CSV),
  OCR on scanned documents.
- Representative corpus: academic papers (arxiv), scanned government reports, long
  technical manuals.

### Cohort C — Developers / QA Engineers (target: 2-3 testers)
- Developers comfortable with edge cases, adversarial inputs, and reading error messages.
- Primary focus: Edge cases, error recovery, keyboard-only navigation, accessibility,
  malformed PDFs, auto-update flow.
- Representative corpus: stress-test PDFs, 0-page files, password-protected archives,
  PDFs with embedded JavaScript.

---

## 3. Testing Environment Requirements

### Hardware
- Windows 10 (21H2 or later) or Windows 11 (22H2 or later) — 64-bit only
- Minimum 8 GB RAM (16 GB recommended for large PDF workflows)
- Minimum 2 GB free disk space (for test corpus + exports)
- Display: 1920x1080 or higher (ribbon layout depends on minimum width)

### Software
- Fresh install of GlyphPDF-1.0.0-beta-x64.msi (provided by project team)
- No prior GlyphPDF installation on the machine (to test fresh-install path)
- Optional: a valid code-signing certificate (PKCS#12 .pfx) for testing PAdES
  signature workflows
- Optional: LibreOffice (any recent version) installed system-wide for Office->PDF
  import testing

### Network
- Internet connectivity for: AI Chat panel (all 4 providers), auto-update check,
  TSA timestamp during signing, and OCSP validation.
- At least one soak session must be run offline to validate offline-mode graceful
  degradation.

### Test PDF Corpus
Each tester should prepare or download a corpus of at least 10 representative PDFs:

| Corpus type | Suggested source |
|-------------|-----------------|
| Academic papers (text-rich, multi-column) | arxiv.org — download open-access PDFs directly |
| Government reports (forms, tables) | usa.gov publications, NHS UK reports |
| Scanned documents (image-only, OCR needed) | Internet Archive (archive.org) — search "scanned report" |
| Legal filings with redactions | PACER free opinions, court public records |
| Office-origin PDFs (Word/Excel export) | Generate from own Office files or download sample .docx templates |
| Password-protected PDFs | Generate one with GlyphPDF itself during testing |
| Large file (50+ pages) | Any long government report or technical standard |

---

## 4. Timeline — 5-Day Bash

| Day | Focus | Activities |
|-----|-------|-----------|
| Day 1 | Install + Onboarding | Install MSI, onboarding walkthrough, first impressions, basic open/save |
| Day 2 | Core features | Annotation, Forms, Page operations, text search, Find & Replace |
| Day 3 | Security features | AES-256 encrypt/decrypt, digital signing (PAdES), redaction, sanitization |
| Day 4 | OCR + Conversion + Batch | OCR on scanned PDFs, ROVER confidence overlay, HTML/CSV/image export, batch mode |
| Day 5 | Edge cases + Wrap-up | Malformed PDFs, keyboard-only navigation, AI Chat, diff/compare, final bug sweep |

Testers should budget 2-4 hours per day. Day 5 wrap-up includes a 30-minute sync call
to triage outstanding reports.

---

## 5. Success Criteria (Ship Gate)

The beta bash is considered passed when ALL of the following are true at the close of
Day 5:

| Criterion | Threshold |
|-----------|-----------|
| Critical severity bugs | 0 open |
| High severity bugs | 0 open (or explicitly accepted as known limitation with workaround documented) |
| Medium severity bugs | Triaged and assigned to v1.0.0 or v1.1 milestone |
| Low severity bugs | Logged, no fix required for ship |
| Tester satisfaction | >= 2 of 3 cohorts report "usable for professional workflows" |
| Crash-free session rate | >= 95% of reported testing sessions completed without a crash |

---

## 6. Out of Scope

- Performance micro-benchmarks (covered by M7-P2 D1/D2 separately)
- 48-hour soak / memory-leak validation (covered by D4 soak-test)
- Public-facing security audit / penetration testing (separate M8 workstream)
- Localization testing (English only for v1.0.0)

---

## 7. Contacts and Escalation

| Role | Responsibility |
|------|---------------|
| Bug bash lead | Triage incoming reports daily, assign severity, update milestone |
| Developer on-call | Respond to Critical bugs within 4 hours; reproduce and acknowledge within 24h |
| Testers | Submit reports via GitHub Issues using bug-report-template.md; respond to
developer follow-up questions within 24h |

All bugs go to the GitHub repository under the "v1.0.0 Beta Bash" milestone.
See `github-milestone-setup.md` for milestone and label setup instructions.
