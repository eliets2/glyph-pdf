# GlyphPDF v1.0.0 Beta Bash Sheet

**Version under test:** GlyphPDF 1.0.0-beta  
**Instructions:** Work through each row. Fill in Actual Result and Pass/Fail during testing.
Severity only required when result = FAIL.

Legend: P = Pass | F = Fail | S = Skip (note reason)

---

## Section 1 — Annotation

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| A-01 | Highlight | Open text PDF. Home tab -> Highlight. Drag over a sentence. | Yellow highlight annotation appears over selected text. | | | |
| A-02 | Underline | Home tab -> Underline. Drag over a sentence. | Blue underline annotation appears. | | | |
| A-03 | Note (sticky) | Home tab -> Note. Click blank area on page. Type text. Press Esc. | Note icon appears on page. Click icon to re-read text. | | | |
| A-04 | Stamp | Home tab -> Stamp. Choose a stamp type. Click to place on page. | Stamp image appears at click location. | | | |
| A-05 | Shape — Rectangle | Edit tab -> Shape -> Rectangle. Draw a rectangle on page. | Rectangle annotation rendered with border. | | | |
| A-06 | Shape — Ellipse | Edit tab -> Shape -> Ellipse. Draw an ellipse. | Ellipse annotation rendered. | | | |
| A-07 | Shape — Line / Arrow | Edit tab -> Shape -> Line or Arrow. Draw across page. | Line/arrow annotation rendered. | | | |
| A-08 | Pencil / Freehand | Edit tab -> Pencil. Draw a freehand stroke. | Freehand ink annotation follows mouse path. | | | |
| A-09 | Annotation persist | After creating 3+ annotations, Ctrl+S. Close. Reopen file. | All annotations present and unchanged. | | | |
| A-10 | Undo annotation | Create an annotation. Press Ctrl+Z. | Annotation removed immediately. | | | |
| A-11 | Redo annotation | After undo (A-10), press Ctrl+Y. | Annotation restored. | | | |
| A-12 | Annotation color / properties | Right-click any annotation -> Properties. Change color. | Color updates in document. | | | |
| A-13 | Delete annotation | Click annotation to select. Press Delete key. | Annotation removed. | | | |

---

## Section 2 — Forms

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| F-01 | Text field — fill | Open interactive form PDF. Click a text field. Type text. | Text appears in field. | | | |
| F-02 | Text field — persist | After F-01, Ctrl+S. Close. Reopen. | Typed text present in field. | | | |
| F-03 | Checkbox — toggle | Click a checkbox in an interactive form. | Checkbox toggles checked/unchecked. | | | |
| F-04 | Radio button | Click radio buttons in a group. | Only one radio in group is selected at a time. | | | |
| F-05 | Dropdown | Click a dropdown field. Select an option from list. | Selected option displayed in field. | | | |
| F-06 | Date field | Click a date field. Enter a date. | Date accepted and displayed. | | | |
| F-07 | Numeric / calculated field | Enter a numeric value; if calculated field exists, verify auto-calculation. | Numeric value accepted; calculated field updates. | | | |
| F-08 | Add text field (Forms tab) | Open any PDF. Forms tab -> Add Text Field. Draw field. Type into it. | New interactive text field created and editable. | | | |
| F-09 | Add checkbox (Forms tab) | Forms tab -> Add Checkbox. Draw checkbox on page. | New checkbox annotation created and toggleable. | | | |
| F-10 | Tab order through fields | Press Tab repeatedly on an interactive form. | Focus moves through form fields in logical order. | | | |
| F-11 | Form data export | Forms tab -> Export form data (if available). | Form data exported as FDF or JSON without error. | | | |

---

## Section 3 — Page Operations

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| PG-01 | Rotate page 90 CW | Pages tab -> Rotate -> 90 clockwise. Apply to page 1. | Page 1 rotated 90 degrees clockwise in viewer. | | | |
| PG-02 | Rotate page 180 | Pages tab -> Rotate -> 180. Apply to page 1. | Page 1 rotated 180 degrees. | | | |
| PG-03 | Rotate all pages | Pages tab -> Rotate -> Apply to all pages. | Every page in document rotated. | | | |
| PG-04 | Crop page | Pages tab -> Crop. Draw crop region. Apply. | Page rendered at cropped dimensions. | | | |
| PG-05 | Resize / scale page | Pages tab -> Resize. Set new page size. | Page dimensions updated. | | | |
| PG-06 | Reorder pages (drag) | Open thumbnail sidebar. Drag page 3 thumbnail before page 1. | Page order changes; page 3 now first. | | | |
| PG-07 | Insert page (blank) | Pages tab -> Insert -> Blank page after page 2. | New blank page inserted at position 3. | | | |
| PG-08 | Insert page (from file) | Pages tab -> Insert -> From File. Select another PDF. | Pages from second PDF inserted. | | | |
| PG-09 | Extract pages | Pages tab -> Extract. Select pages 1-3. Save as new file. | New PDF containing only pages 1-3 created. | | | |
| PG-10 | Split document | Pages tab -> Split. Split at page 3. | Two PDFs created: pages 1-2 and pages 3-end. | | | |
| PG-11 | Delete page | Select page in thumbnail sidebar. Press Delete or Pages tab -> Delete. | Page removed from document. | | | |
| PG-12 | Headers and footers | Pages tab -> Header/Footer. Add page number to footer. | Page numbers appear in footer on all pages. | | | |
| PG-13 | Bates numbering | Pages tab -> Bates Numbering (if exposed). Apply. | Bates numbers rendered on pages. | | | |
| PG-14 | Watermark — text | Edit tab -> Watermark -> Text. Type "DRAFT". Apply. | "DRAFT" text watermark appears on all pages. | | | |
| PG-15 | Watermark — image | Edit tab -> Watermark -> Image. Select a PNG. Apply. | Image watermark appears on pages. | | | |
| PG-16 | Page op undo chain | Perform rotate + insert + delete in sequence. Ctrl+Z three times. | Each undo reverses the last operation in correct order. | | | |

---

## Section 4 — Security

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| SC-01 | AES-256 encrypt | Security tab -> Encrypt. Set user password "BetaTest1!". Set owner password. Save. | File saved as AES-256 encrypted. | | | |
| SC-02 | Decrypt — correct password | Close and reopen encrypted file. Enter correct password. | Document opens successfully. | | | |
| SC-03 | Decrypt — wrong password | Reopen encrypted file. Enter wrong password. | Access denied with clear error message. No crash. | | | |
| SC-04 | Encrypt — no password set | Try to apply encryption with empty password field. | Error or validation message prevents saving without a password. | | | |
| SC-05 | Certificate-based encrypt | Security tab -> Certificate Encryption. Select a certificate recipient. Encrypt. | File encrypted for recipient only. | | | |
| SC-06 | PAdES B-LT sign | Open unsigned PDF. Security tab -> Sign. Use test certificate. Complete workflow. | Signature field appears. Document shows as signed. | | | |
| SC-07 | Signature validation | After SC-06, Security tab -> Validate Signatures. | Signature reported as valid (or invalid with explanation if cert is self-signed). | | | |
| SC-08 | Redact by selection | Security tab -> Redact. Draw redaction region over text. Apply Redactions. | Region permanently removed from content stream. Text NOT selectable in that region in any PDF reader. | | | |
| SC-09 | Redact by search pattern | Security tab -> Redact -> Pattern. Enter a regex or search term. Redact all matches. Apply. | All matches permanently removed. | | | |
| SC-10 | Redact verification | Open redacted file in Adobe Reader or another viewer. Attempt to select text in redacted area. | No text selectable. Content is excised, not covered. | | | |
| SC-11 | Sanitize — all vectors | Security tab -> Sanitize Document. Enable all 15+ sanitization vectors. Run. | Document processed. Metadata, JS, hidden layers, embedded files removed per selected vectors. | | | |
| SC-12 | Sanitize — selective | Security tab -> Sanitize. Enable only metadata removal. Run. | Only metadata removed; other content unchanged. | | | |
| SC-13 | SHA-256 signature hash | After signing (SC-06), inspect signature properties. | Hash algorithm shown as SHA-256 (not SHA-1 or MD5). | | | |

---

## Section 5 — OCR

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| OC-01 | Tesseract OCR — single page | Open scanned (image-only) PDF. Run OCR with Tesseract engine only. | Text recognized and made searchable. | | | |
| OC-02 | RapidOCR (PP-OCRv5) — single page | Run OCR with RapidOCR engine only on same page. | Text recognized and made searchable. | | | |
| OC-03 | ROVER dual-engine merge | Run OCR with "Both Engines" option. | ROVER merged result applied. Higher confidence than either engine alone on noisy scan. | | | |
| OC-04 | OCR full document | Run OCR across entire multi-page scanned PDF. | All pages processed; progress indicator shown; completion reported. | | | |
| OC-05 | Confidence overlay | After OCR, enable confidence overlay (OCR mode panel). | Words color-coded by confidence (green=high, red=low or similar). | | | |
| OC-06 | Per-region redo | In OCR mode, right-click a low-confidence region -> Re-run OCR. | That region re-processed; result updates without re-running full document. | | | |
| OC-07 | Preprocessing pipeline | OCR on a skewed / low-contrast scan. | Preprocessing corrects skew/contrast before recognition. Output quality better than raw engine. | | | |
| OC-08 | Search OCR output | After OCR, use Ctrl+F. Search for a word present in recognized text. | Search finds and highlights match. | | | |
| OC-09 | OCR text selection | After OCR, attempt to select/copy text on page. | Text selectable and copyable. | | | |
| OC-10 | OCR on clean PDF | Run OCR on a born-digital (non-scanned) PDF. | App handles gracefully: either skips (already has text) or runs without error. | | | |

---

## Section 6 — Conversion and Batch

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| CV-01 | Export to HTML | Convert tab -> Export to HTML. Save. Open in browser. | HTML file created with readable, structured content. | | | |
| CV-02 | Export to Text | Convert tab -> Export to Text. Save. Open .txt file. | Plain text extracted; readable. | | | |
| CV-03 | Export to CSV | Convert tab -> Export to CSV (on a PDF with tables). | CSV file with table data. Columns roughly aligned. | | | |
| CV-04 | Export to Images (PNG) | Convert tab -> Export to Images. Choose PNG. | One PNG per page (or as configured) created. | | | |
| CV-05 | Export to Images (JPEG) | Convert tab -> Export to Images. Choose JPEG. | JPEG files created per page. | | | |
| CV-06 | PDF/A export | Convert tab -> Export PDF/A. | File saved in PDF/A format. If veraPDF configured, validation result shown. | | | |
| CV-07 | Export preset — High Quality PDF/A | Export Presets panel -> "High Quality PDF/A" preset. Apply. | Document exported using preset settings without error. | | | |
| CV-08 | Export preset — Web Optimized | Export Presets panel -> "Web Optimized" preset. Apply. | Web-optimized PDF created (linearized/smaller). | | | |
| CV-09 | Export preset — create custom | Export Presets -> New. Set custom settings. Save preset. Apply to document. | Custom preset saved and applied correctly. | | | |
| CV-10 | Export preset — delete | Delete the custom preset created in CV-09. | Preset removed from list. No error. | | | |
| CV-11 | Batch — multiple files, one op | Batch mode. Add 3+ PDFs. Set operation = Export to Text. Run. | All files converted. Output files created. Any failures reported inline. | | | |
| CV-12 | Batch — mixed op types | Batch mode. Add 3 PDFs. Apply different operations to each. Run. | Each file processed with its assigned operation. | | | |
| CV-13 | Batch — error reporting | Batch mode. Include a corrupted PDF in the queue. Run. | Corrupted file reported as failed with error message; other files continue. | | | |
| CV-14 | Office -> PDF import (LibreOffice) | If LibreOffice installed: File -> Open a .docx or .xlsx file. | Document imported and rendered as PDF. | | | |
| CV-15 | Office import — no LibreOffice | With LibreOffice NOT installed: attempt to open a .docx. | Clear "LibreOffice not installed" message. No crash. | | | |

---

## Section 7 — Diff / Compare

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| DC-01 | Compare two PDFs — text diff | Mode switcher -> Compare. Load original and modified PDF. | Text differences highlighted in the diff view. | | | |
| DC-02 | Compare identical PDFs | Compare two identical copies of the same PDF. | "No differences found" or empty diff result. | | | |
| DC-03 | Compare visual diff | If visual diff is available: compare PDFs with layout changes. | Visual differences overlaid on pages. | | | |

---

## Section 8 — AI Chat Panel

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| AI-01 | AI Chat — provider config | View tab -> AI Chat panel (or sidebar icon). Configure API key for one provider. | Key accepted; provider selectable. | | | |
| AI-02 | AI Chat — query on document | With a text PDF open, ask "Summarize this document". | Contextually relevant response returned. | | | |
| AI-03 | AI Chat — all 4 providers | Attempt to configure and query each of the 4 supported providers. | Each provider connects and responds (or gives clear auth error if key invalid). | | | |
| AI-04 | AI Chat — no API key | Open panel without configuring a key. | Clear "API key required" message with instructions. No crash. | | | |
| AI-05 | AI Chat — offline | Disconnect from internet. Attempt to query. | Clear network error message. No crash. | | | |

---

## Section 9 — UI, Themes, and Accessibility

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| UI-01 | Dark theme | View tab -> Theme -> Dark (or Preferences -> Theme). | UI switches to dark color scheme. All text readable. | | | |
| UI-02 | Light theme | View tab -> Theme -> Light. | UI switches to light color scheme. | | | |
| UI-03 | High Contrast theme | View tab -> Theme -> High Contrast. | High contrast theme applied. All UI elements visible. | | | |
| UI-04 | Keyboard navigation (F6) | Press F6 repeatedly. | Focus cycles through UI regions: ribbon, document, sidebar, status bar. | | | |
| UI-05 | F1 help | Press F1. | Keyboard shortcuts help dialog opens. | | | |
| UI-06 | Find and Replace | Ctrl+H. Enter find term and replace term. Replace all. | All instances of find term replaced in document. | | | |
| UI-07 | Find and Redact | Ctrl+H. Enter find term. Choose "Redact All". | All instances marked for redaction. Apply Redactions to confirm permanent removal. | | | |
| UI-08 | Recent files | Open 5+ different PDFs. Close all. File menu -> Recent. | Up to 20 recently opened files listed. | | | |
| UI-09 | Drag and drop | Drag a PDF file from Windows Explorer and drop onto main window. | Document opens. | | | |
| UI-10 | Print preview | Ctrl+P. | Print preview dialog opens with paper size, orientation, margin, scaling controls. | | | |
| UI-11 | Full screen | Press F11. | Application enters full-screen mode. Press F11 again to exit. | | | |
| UI-12 | Auto-update check | Help menu -> Check for Updates (or wait for startup check). | Update checker runs; if no update, clear "up to date" message. No crash. | | | |
| UI-13 | Ribbon tab switching | Click each of the 7 ribbon tabs: Home, View, Edit, Pages, Convert, Forms, Security. | Each tab displays its controller's buttons without errors or blank areas. | | | |

---

## Section 10 — Edge Cases and Error Handling

| # | Feature | Test Steps | Expected Result | Actual Result | P/F/S | Severity if Fail |
|---|---------|-----------|----------------|---------------|-------|-----------------|
| EC-01 | Open corrupted PDF | Download or create a truncated PDF. Attempt to open. | Graceful error message. No crash. | | | |
| EC-02 | Open 0-byte file | Create empty file named test.pdf. Attempt to open. | Graceful error message. No crash. | | | |
| EC-03 | Open very large PDF | Open a 100+ MB or 200+ page PDF. | Opens within reasonable time (see performance targets in README). No crash. | | | |
| EC-04 | Open PDF with JS | Open a PDF known to contain embedded JavaScript. | JavaScript not executed. If sanitizer detects JS, it offers to remove it. | | | |
| EC-05 | Open password-protected PDF without password | Attempt to open an AES-encrypted PDF without entering a password. | Password prompt shown. No crash. Cancel dismisses dialog. | | | |
| EC-06 | Save to read-only path | Try to save a document to a read-only location or network path that is inaccessible. | Clear error message. Original file not corrupted. | | | |
| EC-07 | Undo past beginning | Open a file. Make one change. Ctrl+Z twice. | Second Ctrl+Z is a no-op. No crash. No error message required (acceptable: greyed-out undo). | | | |
| EC-08 | Cancel long operation | Start OCR on a large scanned PDF. Immediately click Cancel. | OCR stops cleanly. Document state unchanged. No crash. | | | |
