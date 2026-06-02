# GlyphPDF Beta Tester Onboarding Guide

**Version under test:** GlyphPDF 1.0.0-beta  
**Time required:** ~45 minutes for full onboarding  
**Prereqs:** Windows 10/11 64-bit, 8 GB RAM, internet connection

---

## Step 1 — Install GlyphPDF

1. Download `GlyphPDF-1.0.0-beta-x64.msi` from the link provided by the bash lead.
2. Double-click the MSI. The installer will:
   - Copy files to `C:\Program Files\GlyphPDF\`
   - Register `.pdf` file association via OpenWithProgids (does NOT hijack your
     current default PDF viewer — your default remains unchanged)
   - Create a Start Menu shortcut under "GlyphPDF"
3. Launch GlyphPDF from the Start Menu. You should see the main window with a ribbon
   toolbar across the top containing seven tabs: Home, View, Edit, Pages, Convert,
   Forms, Security.
4. Confirm the version: Help menu -> About -> version should read "1.0.0-beta".

If the installer fails or the app does not launch, stop here and file a bug immediately
using the template in `bug-report-template.md` with severity = Critical.

---

## Step 2 — Download Your Test Corpus

You need at least 10 PDFs covering different types. Suggested download sources:

### Text-rich / Academic PDFs (2-3 files)
1. Go to https://arxiv.org
2. Search for any topic (e.g., "machine learning survey")
3. Click any result, then click "Download PDF" on the abstract page
4. Repeat for 2-3 papers of varying page counts (try one with 5 pages, one with 30+)

### Scanned / Image-only PDFs (2-3 files)
1. Go to https://archive.org
2. Search "government report scanned" or "annual report scan"
3. On a result page, click "PDF" under the Download Options on the right side
4. Download 2-3 files (ideally with varying scan quality — some crisp, some faded)

### Government Forms / Tables (2-3 files)
1. Suitable sources: USA.gov, IRS forms (irs.gov/forms), UK GOV.UK publications
2. Download any fillable form PDF (e.g., IRS Form W-4 or a standard government
   application form)
3. These exercise the Forms feature area

### Large Multi-page Document (1 file)
- Any PDF with 50+ pages. A government annual report or technical standard works well.
- Example: search "NIST special publication" on nvlpubs.nist.gov

### Self-generated test files (2 files)
- You will create these during testing: a password-protected PDF and a signed PDF.
  You do not need to download these in advance.

Store all downloaded PDFs in a single folder, e.g., `C:\BetaTestPDFs\`.

---

## Step 3 — Feature Area Walkthrough

Work through each area in order. The bash sheet (`bash-sheet.md`) contains the detailed
test matrix; this walkthrough gives you one representative task per feature area to
confirm basic operation before diving into edge cases.

### 3.1 Open and Navigate
1. Launch GlyphPDF. Drag and drop any PDF from your corpus onto the main window.
2. Confirm the document opens and pages render correctly.
3. Scroll through several pages. Use Ctrl+F to open Find, search for a word you know
   is in the document, confirm results highlight.
4. Press Ctrl+0 to fit to actual size. Use Ctrl+= and Ctrl+- to zoom in/out.
5. Press F6 to cycle through UI regions (tests keyboard accessibility).

### 3.2 Annotation
1. Open a text-rich PDF (an arxiv paper works well).
2. Home tab -> Highlight: drag to select a sentence. Confirm a yellow highlight appears.
3. Home tab -> Underline: select another sentence. Confirm underline annotation.
4. Home tab -> Note: click anywhere on the page. Type a comment. Press Escape or click
   away. Confirm the note icon appears.
5. Home tab -> Stamp: drop a stamp annotation (e.g., "DRAFT") onto the page.
6. Edit tab -> Pencil: draw a freehand mark. Confirm it renders.
7. Ctrl+Z to undo the last annotation. Confirm it disappears.
8. Ctrl+S to save. Reopen the file. Confirm annotations persisted.

### 3.3 Forms
1. Open one of your government form PDFs.
2. Forms tab -> if the form has interactive fields, click into text fields and type.
3. Click checkboxes and radio buttons. Confirm they toggle.
4. If a dropdown is present, open it and select an option.
5. Ctrl+S to save. Reopen and confirm form data persisted.
   If the PDF has no interactive fields: Forms tab -> Add Text Field, draw a field
   on the page, type into it, save, reopen.

### 3.4 Page Operations
1. Open any multi-page PDF.
2. Pages tab -> Rotate: rotate page 1 by 90 degrees. Confirm the page rotates.
3. Pages tab -> Reorder: drag page thumbnails in the sidebar to swap two pages.
4. Pages tab -> Extract: extract pages 1-2 to a new file. Confirm a new PDF is created.
5. Pages tab -> Split: split the document at page 2. Confirm two output files.
6. Ctrl+Z several times to undo. Confirm undo history works across page operations.

### 3.5 Security — Encryption
1. Open any PDF.
2. Security tab -> Encrypt: set an AES-256 password (e.g., "BetaTest1234!").
3. Confirm the document is saved as encrypted.
4. Close GlyphPDF. Reopen the encrypted PDF. Confirm a password dialog appears.
5. Enter the correct password. Confirm the document opens.
6. Try entering a wrong password. Confirm access is denied with a clear error.

### 3.6 Security — Digital Signature
1. Open a clean (unsigned) PDF.
2. Security tab -> Sign Document.
3. If you have a .pfx certificate file, load it. Otherwise use the "Self-signed
   test certificate" option if available, or note in your report that you skipped
   signing due to no test certificate.
4. Complete the signing workflow. Confirm a signature field appears in the document.
5. Security tab -> Validate Signatures. Confirm the signature shows as valid.

### 3.7 Security — Redaction
1. Open a text-rich PDF.
2. Security tab -> Redact: select a region of text or use "Find & Redact" (Ctrl+H,
   then use the "Redact All" option on a search term).
3. Security tab -> Apply Redactions. Confirm the redacted areas are permanently removed
   (black boxes or blank space — NOT just covered by a rectangle).
4. Attempt to select text in the redacted region in a PDF reader (e.g., Adobe Reader).
   Confirm no text is selectable there. This validates content-stream excision.
5. Security tab -> Sanitize Document. Choose all 15+ sanitization vectors.
   Confirm the document processes without error.

### 3.8 OCR
1. Open one of your scanned (image-only) PDFs.
2. Home tab or toolbar -> Run OCR (or via the OCR mode panel).
3. Select "Both Engines (ROVER merge)" if prompted for engine choice.
4. Wait for OCR to complete. Confirm text is now selectable/searchable on the page.
5. Use Ctrl+F to search for a word that should have been recognized.
6. If confidence overlay is available, enable it. Confirm words are color-coded by
   confidence level.
7. Right-click a low-confidence region -> "Re-run OCR on region". Confirm per-region
   redo works.

### 3.9 Conversion and Export
1. Open any PDF.
2. Convert tab -> Export to HTML. Save to desktop. Open the .html in a browser.
   Confirm readable content is present.
3. Convert tab -> Export to Text (.txt). Confirm the text file contains readable content.
4. Convert tab -> Export to Images. Confirm PNG or JPEG files are created, one per page
   (or as configured).
5. If you have a CSV-extractable document (tables): Convert tab -> Export to CSV.
6. Convert tab -> Export PDF/A. Confirm the file saves without error. If veraPDF is
   configured, confirm the validation result.

### 3.10 Batch Processing
1. Batch mode panel (accessible from Home tab or Mode switcher).
2. Add 3-5 PDFs from your corpus to the batch queue.
3. Set an operation: e.g., "Export to Text" or "Rotate all pages 90 degrees".
4. Run the batch. Confirm all files process and any errors are reported inline (not as
   silent failures).

### 3.11 AI Chat Panel
1. Open any PDF with readable text.
2. Click the AI Chat panel icon (right sidebar or View tab).
3. Select a provider (e.g., OpenAI or Anthropic). You will need an API key.
   If you do not have an API key, note this in your feedback but do not skip: verify
   that the panel shows a clear "API key required" message and how to configure it.
4. Ask a question about the document content. Confirm the response is contextually
   relevant.

### 3.12 Document Diff / Compare
1. Open two versions of a PDF (e.g., save a copy, make a change, save again).
2. Mode switcher -> Compare mode.
3. Load both files. Confirm differences are highlighted.

---

## Step 4 — How to Submit Bugs

All bugs go to the GitHub repository Issues tab.

1. Click "New Issue".
2. Select the "Bug Report" template (or paste from `bug-report-template.md`).
3. Fill in all fields — especially Steps to Reproduce. Incomplete reports slow down
   triage significantly.
4. Assign the "beta-feedback" label. Add "release-blocker" if you believe the bug
   is Critical or High severity.
5. Attach the PDF that triggered the bug if possible (redact any personal data first).
6. For crashes: attach the Windows Event Log entry. Go to Event Viewer ->
   Windows Logs -> Application, find the entry timestamped at the crash time,
   right-click -> Save Selected Events.

### Severity Quick Guide

| Severity | Use when |
|----------|---------|
| Critical | Crash, data loss, security bypass, file corruption |
| High | Feature completely non-functional; no workaround |
| Medium | Feature works but degrades significantly; workaround exists |
| Low | Cosmetic issue, minor wording, UI polish |

See the full severity rubric in `bug-report-template.md`.

---

## Step 5 — End-of-Day Wrap

At the end of each testing day, send a brief email to the bash lead with:
- How many sessions you ran
- How many bugs you filed
- Any blocking issues that need immediate attention (Critical bugs)
- General impressions of that feature area

Thank you for your time. Your testing directly determines whether GlyphPDF ships
as a trustworthy v1.0.0 for professional users.
