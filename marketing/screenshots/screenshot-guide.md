# GlyphPDF v1.0.0 — Hero Screenshot Guide

This guide instructs a human operator on how to capture the 8 hero screenshots
for the v1.0.0 launch. Each shot must use a REAL PDF document — no Lorem Ipsum,
no generated filler. Suggested source documents are noted per shot.

All screenshots must be taken at **1920 x 1080** (100% DPI, no scaling).
Export 4K versions (3840 x 2160) where flagged.

---

## Pre-flight checklist

1. Build and launch `PdfWorkstation.exe` from the release MSI or from
   `build/PdfWorkstation.exe` (after `cmake --build build --parallel 8`).
2. Set window size to exactly 1920 x 1080:
   - Open PowerShell and run:
     ```powershell
     Add-Type -TypeDefinition @"
     using System;using System.Runtime.InteropServices;
     public class Win32{[DllImport("user32.dll")]public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int ht,bool r);}
     "@
     $hwnd=(Get-Process PdfWorkstation -ErrorAction Stop).MainWindowHandle
     [Win32]::MoveWindow($hwnd,0,0,1920,1080,$true)
     ```
   - Or use your OS window-resize utility / ShareX "resize window" feature.
3. Apply the **Dark theme** (View ribbon > Theme > Dark) for shots 1-5.
   Apply the **Light theme** for shot 6.
   Apply the **High Contrast** theme for shot 7.
4. Dismiss the splash screen and close the AI Chat panel before each shot.
5. Use a real multi-page PDF. Suggested test documents:
   - A scanned legal contract (3-5 pages, ideally with a signature block).
   - A technical report or academic paper (enables OCR demo).
   - A multi-page invoice or form (enables form demo).

---

## Shot 1 — Home / Welcome screen (Dark theme)

**Filename:** `01-welcome-dark.png` (+ `01-welcome-dark-4k.png`)
**Feature shown:** First-run experience, ribbon, recent files grid, drag-and-drop.
**State to reach:**
1. Launch with NO document open (close any auto-restored session).
2. Ensure Recent Files shows at least 3-4 real PDF entries (open some real PDFs
   first so they appear in history).
3. The WelcomeWidget action cards should be fully visible:
   "Open PDF", "Import Office Document", "Images to PDF", etc.
**Composition target:** WelcomeWidget centered; recent files visible in grid below;
full ribbon visible at top (Home tab active).
**Capture:** F11 to ensure normal (non-full-screen) mode. Screenshot full window.
**4K:** Yes.

---

## Shot 2 — Annotation + ribbon (Dark theme)

**Filename:** `02-annotation-dark.png` (+ `02-annotation-dark-4k.png`)
**Feature shown:** Annotation suite, highlight, note balloon, ribbon Edit tab.
**State to reach:**
1. Open a real multi-page PDF (e.g., a legal contract or a research paper).
2. Activate the **Edit** ribbon tab.
3. Select Highlight tool. Highlight 2-3 sentences on page 1.
4. Add a sticky note annotation (Note tool) near the top-right of the page.
5. Add a rectangle shape annotation somewhere on the page.
6. Open the Comments sidebar (left side) to show the annotation list.
7. Zoom to ~125% so page content is clearly legible.
**Composition target:** Ribbon (Edit tab active), annotated PDF page, Comments
sidebar visible on left.
**Capture:** Full window.
**4K:** Yes.

---

## Shot 3 — Digital signing flow (Dark theme)

**Filename:** `03-signing-dark.png`
**Feature shown:** PAdES B-LT/B-LTA signing dialog, Signatures panel.
**State to reach:**
1. Open a real unsigned PDF contract.
2. Navigate to the Security ribbon tab.
3. Click "Sign Document". The signing dialog opens.
4. Select a test certificate (generate one via `tests/fixtures/signing/generate.bat`
   if you don't have a real one; never use production certificates for screenshots).
5. Leave the dialog OPEN showing the certificate picker and timestamp authority field.
   Do NOT complete the signing yet (to keep the UI interactive in the screenshot).
6. In the background, ensure the Signatures sidebar panel is visible on the right,
   showing "No signatures" or a previously signed document with a green checkmark.
**Composition target:** Signing dialog in foreground, Security ribbon tab active,
Signatures panel visible.
**4K:** No (dialog-centric shot; 1920 x 1080 sufficient).

---

## Shot 4 — Redaction with Edact-Ray defense (Dark theme)

**Filename:** `04-redaction-dark.png`
**Feature shown:** Secure redaction (content stream excision, not black rectangles).
**State to reach:**
1. Open a PDF containing visible text (an invoice, letter, or contract works well).
2. Activate the Security ribbon tab > Redact tool.
3. Draw redaction rectangles over 3-5 text regions (names, addresses, amounts).
   The regions should show the red-bordered "pending redaction" overlay in the UI.
   Do NOT apply the redaction yet.
4. Activate the Find & Replace bar (Ctrl+H) and type a pattern (e.g., email regex)
   in the redact field to show the pattern-redact feature.
**Composition target:** PDF with multiple pending redaction rectangles visible
(red borders over text), Find & Replace bar visible at top, Security ribbon active.
**4K:** No.

---

## Shot 5 — OCR with confidence overlay (Dark theme)

**Filename:** `05-ocr-confidence-dark.png` (+ `05-ocr-confidence-dark-4k.png`)
**Feature shown:** OCR ensemble, per-word confidence heatmap overlay.
**State to reach:**
1. Open a scanned PDF (image-only, no embedded text layer). A scanned contract
   or printed article works perfectly.
2. Navigate to OCR mode (mode strip or View > OCR).
3. Run OCR on the document (click "Run OCR" or equivalent).
4. After OCR completes, enable the **confidence overlay** toggle.
5. The page should show word-level color coding:
   - Green words (confidence >= 90%)
   - Yellow words (confidence 70-89%)
   - Red words (confidence < 70%)
6. Show the info strip at the bottom: "AVG CONFIDENCE: N% | LOW-CONFIDENCE WORDS: N".
7. Optionally right-click a low-confidence word to show the "Re-OCR this region"
   context menu.
**Composition target:** OCR mode active, confidence-colored overlay on scanned page,
info strip visible, confidence legend visible.
**4K:** Yes.

---

## Shot 6 — Forms mode (Light theme)

**Filename:** `06-forms-light.png`
**Feature shown:** Form builder, field placement, FormFieldPropertiesPanel.
**State to reach:**
1. Switch to Light theme (View > Theme > Light).
2. Open a real fillable PDF form (or a blank contract with form fields).
3. Activate the Forms ribbon tab.
4. Switch to Form Builder mode.
5. Place 2-3 form fields on the page (Text Field, Checkbox, Dropdown).
6. Select one field to show the FormFieldPropertiesPanel on the right sidebar,
   with Name/Tooltip/Required/Default fields visible.
7. Show the tab order editor panel if space permits.
**Composition target:** Forms ribbon active, form builder canvas with placed fields,
FormFieldPropertiesPanel sidebar visible on right.
**4K:** No.

---

## Shot 7 — High Contrast accessibility (High Contrast theme)

**Filename:** `07-accessibility-hc.png`
**Feature shown:** High Contrast theme, full keyboard accessibility, F6 region cycling.
**State to reach:**
1. Switch to High Contrast theme (View > Theme > High Contrast).
2. Open any real PDF document.
3. Press F6 until focus ring is on the ribbon tab bar (visible focus rectangle).
4. Open the Keyboard Shortcuts dialog (F1) and position it so both the dialog
   and the underlying ribbon are visible.
**Composition target:** High Contrast theme throughout, visible focus rectangle
on ribbon, Keyboard Shortcuts dialog open.
**4K:** No.

---

## Shot 8 — MRC compression (Dark theme)

**Filename:** `08-mrc-compression-dark.png`
**Feature shown:** Mixed Raster Content compression dialog, 30x compression ratio.
**State to reach:**
1. Switch back to Dark theme.
2. Open a scanned PDF (preferably large, 10+ pages of scanned content).
3. Navigate to Compress mode or open the CompressDialog (Convert ribbon > Compress).
4. In the MRC selector, choose "Balanced" mode.
5. The live size estimate label should show the estimated compressed size.
   If the document is large enough, the label might show something like
   "2.6 MB -> 86 KB (30x)" — use a document that triggers a meaningful ratio.
6. Show the Off / Lossless / Balanced / Aggressive radio buttons clearly.
**Composition target:** CompressDialog open, MRC mode selector visible, size
estimate label showing a meaningful compression ratio.
**4K:** No.

---

## Post-capture checklist

- [ ] All 8 PNGs are named exactly as above.
- [ ] 1920 x 1080 versions captured for all 8 shots.
- [ ] 4K versions captured for shots 01, 02, 05.
- [ ] No Lorem Ipsum, placeholder text, or fake data visible.
- [ ] No test certificate CN or private key path visible in shot 03.
- [ ] Splash screen not visible in any shot.
- [ ] AI Chat panel closed in all shots.
- [ ] Place all files in `marketing/screenshots/` in the repo.
- [ ] Update this guide with "Captured: YYYY-MM-DD" once done.

---

## Tools recommended for capture

- **ShareX** (free, open source): supports exact window resize + region capture.
- **Windows Snipping Tool**: Ctrl+Shift+S, "Window" mode.
- **OBS Studio**: for 4K capture if monitor is 1080p (virtual canvas).

---

*Captured: [DATE — human to fill]*
*Captured by: [NAME — human to fill]*
