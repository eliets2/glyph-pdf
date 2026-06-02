# GlyphPDF Bug Report Template

Copy this template when filing a new GitHub Issue. Delete the italicised guidance text
before submitting.

---

## Title

_One sentence: what goes wrong and where. Examples:_
- _"Crash when applying redaction on PDF with embedded XObjects"_
- _"AES-256 encryption dialog accepts empty password without error"_
- _"Highlight annotation lost after save-reopen cycle"_

---

## Feature Area

_Select the closest match:_

- [ ] Annotation (highlight / underline / note / stamp / shapes / pencil)
- [ ] Forms (text fields / checkboxes / radio / dropdown / date / numeric / calculated)
- [ ] Page Operations (rotate / crop / resize / reorder / insert / extract / split)
- [ ] Page Decoration (headers / footers / Bates / watermarks)
- [ ] Security — Encryption (AES-256 / certificate-based)
- [ ] Security — Digital Signature (PAdES / validation / DSS)
- [ ] Security — Redaction (selection / pattern / apply)
- [ ] Security — Sanitization
- [ ] OCR (Tesseract / RapidOCR / ROVER / confidence overlay / per-region redo)
- [ ] Conversion (HTML / text / CSV / images / PDF/A)
- [ ] Export Presets
- [ ] Batch Processing
- [ ] Office -> PDF Import
- [ ] AI Chat Panel
- [ ] Diff / Compare
- [ ] UI / Ribbon / Themes
- [ ] Keyboard Accessibility
- [ ] Find and Replace / Redact-all
- [ ] Auto-Update
- [ ] Install / Uninstall
- [ ] Performance / Responsiveness
- [ ] Other: _____________

---

## Severity

_Choose one. See rubric below._

- [ ] Critical
- [ ] High
- [ ] Medium
- [ ] Low

---

## Steps to Reproduce

_Number each step. Be specific — include menu paths, file types, and any prerequisite
state. A report that cannot be reproduced cannot be fixed._

1.
2.
3.
4.
5.

---

## Expected Behavior

_What should have happened?_

---

## Actual Behavior

_What actually happened? Include the exact text of any error message._

---

## Reproducibility

- [ ] Always (100%)
- [ ] Often (> 50% of attempts)
- [ ] Sometimes (< 50% of attempts)
- [ ] Once (could not reproduce again)

---

## PDF Attached

- [ ] Yes — attached to this issue (redact personal data before attaching)
- [ ] No — the PDF is confidential / cannot be shared; I can describe its structure
- [ ] No — the issue is not PDF-specific

_If the bug only triggers on a specific PDF, a reproduction case is essential for Critical
and High bugs. Please attach or provide an anonymized minimal reproduction PDF._

---

## Environment

| Field | Value |
|-------|-------|
| Windows version | _e.g., Windows 11 22H2, Windows 10 21H2_ |
| GlyphPDF build version | _Help menu -> About, copy version string_ |
| Display resolution | _e.g., 1920x1080_ |
| RAM | _e.g., 16 GB_ |
| LibreOffice installed? | _Yes / No (version if yes)_ |
| Internet connected? | _Yes / No_ |

---

## Additional Context

_Any other information: screenshots, screen recordings, Windows Event Log entries
(paste the Application log entry from Event Viewer if this was a crash)._

---

---

# Severity Rubric

Use this rubric to assign severity consistently. When in doubt, assign the higher severity
and let the triage lead adjust.

## Critical

Any of:
- **Data loss or corruption**: a document is silently corrupted or data is permanently
  lost as a result of a normal operation (save, export, redaction).
- **Security bypass**: redacted content is recoverable from the output file; an
  encrypted file is openable without a password; a digital signature is created or
  shown as valid when it is not.
- **Crash (unrecoverable)**: the application crashes to the desktop (access violation,
  assertion failure, unhandled exception) during a routine user operation on a
  well-formed PDF.
- **Installer failure**: the MSI fails to install or leaves the system in a broken
  state on a clean Windows 10/11 machine.

Examples:
- "Apply Redactions crashes on PDFs with Form XObjects"
- "Sanitizer claims success but embedded JavaScript survives in output"
- "Saved file is a 0-byte file when saving to a path with non-ASCII characters"

## High

Any of:
- **Feature completely non-functional**: a documented feature produces no output, no
  effect, or an error on every attempt with no workaround.
- **Data integrity issue with workaround**: data is affected but the user can recover
  by using an alternative workflow.
- **Regression from expected baseline**: a feature that worked during internal testing
  is broken in the beta build.

Examples:
- "OCR produces no output on any scanned PDF"
- "Batch mode silently skips all files without error"
- "PDF/A export always fails with 'veraPDF not found' even when veraPDF is installed"

## Medium

Any of:
- **Degraded functionality**: the feature works but is significantly harder to use,
  slower than expected, or produces a lower-quality result than documented.
- **Missing UI affordance**: an action is possible but the UI path is confusing,
  hidden, or inconsistent with the documented workflow.
- **Intermittent functional failure**: the feature fails on some inputs or some of the
  time, but works in most cases.

Examples:
- "Confidence overlay in OCR mode does not update after per-region redo"
- "Batch progress bar does not update during processing (appears frozen)"
- "High Contrast theme makes some ribbon icons invisible"

## Low

Any of:
- **Cosmetic issue**: incorrect label text, misaligned UI element, missing icon,
  minor visual glitch that does not affect functionality.
- **Workaround is obvious and trivial**: one extra click resolves the issue.
- **Edge case with negligible user impact**: affects a very specific file type or
  workflow combination that typical users will not encounter.

Examples:
- "Tooltip for 'Bates Numbering' button is missing"
- "Splash screen appears briefly behind the main window on first launch"
- "Keyboard shortcut help dialog (F1) does not sort shortcuts alphabetically"

---

# Labels to Apply

| Label | When to use |
|-------|------------|
| `beta-feedback` | All bugs filed during the beta bash |
| `release-blocker` | Critical or High severity bugs that must be fixed before M8 ship |
| `regression` | Worked in a previous internal build, broken now |
| `ui-polish` | Low severity cosmetic / UX issues |
| `needs-repro` | Cannot reproduce without more info — assigned by triage lead |
| `duplicate` | Filed more than once — add link to canonical issue |
