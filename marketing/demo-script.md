# GlyphPDF v1.0.0 — 60-90s Demo Video Production Script

**Target runtime:** 75 seconds (aim for 65-80s; do not exceed 90s)
**Resolution:** 1920 x 1080 minimum; 4K preferred for YouTube
**Format:** MP4 (H.264 or H.265), 60 fps
**Audio:** Voiceover narration (recommended) OR lower-third captions
**Theme:** Dark throughout (most visually striking in recordings)
**Voiceover tone:** Calm, professional, slightly fast-paced (no filler pauses)

---

## Pre-production checklist

- [ ] Build the release binary (`cmake --build build --parallel 8`).
- [ ] Prepare a real multi-page PDF test document: a 4-6 page scanned contract
  or technical report (ideally with a signature block and some tabular content).
- [ ] Generate a test signing certificate:
  `cd tests/fixtures/signing && generate.bat`
  Use the generated test cert throughout; never use a production certificate.
- [ ] Open the app and run OCR on the test document once before recording so the
  OCR results are cached. The on-screen run should be fast for demo purposes.
- [ ] Set window to 1920 x 1080, Dark theme, AI Chat panel closed.
- [ ] Disable OS notifications that might pop over the window.
- [ ] Use OBS Studio or Camtasia for recording (record cursor movements).

---

## Scene-by-Scene Script

### SCENE 1 — Cold open (0:00 – 0:04)
**Action:** App launches. Splash screen fades in (GlyphPDF wordmark + logo).
The welcome screen appears with the ribbon and recent files grid visible.
**No voiceover yet** — let the splash/logo speak.
**On-screen text (lower third):** "GlyphPDF v1.0.0"
**Timing:** 4 seconds.

---

### SCENE 2 — Opening hook (0:04 – 0:10)
**Action:** Drag a real PDF onto the welcome screen. Document opens instantly
in the viewer. The ribbon is visible at top; thumbnail sidebar appears on left.
**Voiceover:**
> "GlyphPDF is a professional PDF workstation for Windows — open source,
> no subscription, no telemetry. Everything runs locally."

**Timing:** 6 seconds. Drag must be clearly visible.

---

### SCENE 3 — Annotation (0:10 – 0:22)
**Action:**
1. Click the Edit ribbon tab.
2. Click the Highlight tool. Drag to highlight a sentence.
3. Click the Notes tool. Click on the page to add a sticky note; type "Review this clause."
4. The Comments sidebar on the left updates to show the new annotation.

**Voiceover:**
> "Annotate with highlights, notes, stamps, and shapes. Everything is stored
> inside the PDF — no sidecar files."

**Timing:** 12 seconds. Keep mouse movements deliberate and smooth.

---

### SCENE 4 — Forms (0:22 – 0:33)
**Action:**
1. Click the Forms ribbon tab.
2. Switch to Form Builder mode.
3. Draw a Text Field on the page (rubber-band gesture).
4. The FormFieldPropertiesPanel appears on the right with name/tooltip/required fields.
5. Fill the Name field with "Signature Name". Press Enter.
6. Switch to Fill mode. Click the text field and type a name.

**Voiceover:**
> "Build and fill interactive forms — text fields, checkboxes, radio buttons,
> dropdowns, date pickers, and calculated fields."

**Timing:** 11 seconds.

---

### SCENE 5 — Signing (0:33 – 0:47)
**Action:**
1. Click the Security ribbon tab.
2. Click "Sign Document". The signing dialog opens.
3. Select the test certificate from the dropdown. TSA URL field shows
   a timestamp authority URL.
4. Click Sign. A brief progress indicator. Document saves.
5. The Signatures sidebar panel opens, showing the signature with a green
   checkmark and "Valid · B-LTA · Trusted chain."

**Voiceover:**
> "Sign with PAdES B-LTA — long-term archival signatures verified against
> the system certificate store. SHA-256. No MD5, no SHA-1."

**Timing:** 14 seconds. The green checkmark in the Signatures panel is the key visual.

---

### SCENE 6 — Secure redaction (0:47 – 0:58)
**Action:**
1. Still in Security tab. Click the Redact tool.
2. Draw 3 redaction rectangles over text (names, a dollar amount, an address).
   The pending-redaction red-bordered boxes appear.
3. Click "Apply Redaction". Brief progress.
4. The text under the rectangles is gone (white/blank space; no black boxes).
5. Show a small tooltip or caption: "Content stream excision — not black rectangles."

**Voiceover:**
> "Redaction removes text from the content stream — not just overlaid with black.
> Our Edact-Ray normalization prevents glyph-width side-channel recovery."

**Timing:** 11 seconds. The contrast with "black rectangle" PDF editors is the
key selling point; consider a two-frame cut showing BEFORE (text visible) and
AFTER (content gone).

---

### SCENE 7 — OCR (0:58 – 1:08)
**Action:**
1. Open a scanned (image-only) PDF.
2. Click "Run OCR" in the OCR mode or ribbon.
3. After completion, enable the confidence overlay toggle.
4. The page lights up with green (high confidence) and yellow word highlights.
5. Info strip shows: "AVG CONFIDENCE: 94% | LOW-CONFIDENCE WORDS: 3."
6. Right-click a yellow word to show the "Re-OCR this region" menu.

**Voiceover:**
> "Built-in OCR with Tesseract and PP-OCRv5, merged via word-level ROVER
> fusion. Per-word confidence overlays. Per-region redo."

**Timing:** 10 seconds. This scene can be cut to 8s if runtime is long.

---

### SCENE 8 — Closing call to action (1:08 – 1:15)
**Action:** Return to Welcome screen. Fade to wordmark on black.
**On-screen text:**
  Line 1: "GlyphPDF v1.0.0"
  Line 2: "Apache-2.0 · Windows · Free forever"
  Line 3: "github.com/[YOUR-ORG]/glyphpdf"

**Voiceover:**
> "GlyphPDF — the professional PDF workstation. Apache-2.0. Free forever.
> Download at [your URL]."

**Timing:** 7 seconds.

---

## Total runtime breakdown

| Scene | Start | End | Duration |
|-------|-------|-----|----------|
| 1 — Cold open | 0:00 | 0:04 | 4s |
| 2 — Opening hook | 0:04 | 0:10 | 6s |
| 3 — Annotation | 0:10 | 0:22 | 12s |
| 4 — Forms | 0:22 | 0:33 | 11s |
| 5 — Signing | 0:33 | 0:47 | 14s |
| 6 — Redaction | 0:47 | 0:58 | 11s |
| 7 — OCR | 0:58 | 1:08 | 10s |
| 8 — CTA | 1:08 | 1:15 | 7s |
| **Total** | | | **75s** |

If recording runs over 80s, trim Scene 7 to 8s (cut the right-click menu step)
and trim Scene 3 to 10s (drop the shape annotation).

---

## Audio / Caption notes

**Voiceover option (recommended):**
- Record VO at 44.1 kHz, mono or stereo.
- Recommend a neutral American English accent (widest international audience).
- Mix at -6 dB relative to music bed (if using one).

**Caption option (alternative):**
- Use auto-captioning in DaVinci Resolve or Premiere, then manually proof.
- Minimum caption font size: 28pt at 1080p.
- White text with black drop shadow or dark background bar.

---

## Post-production notes

- Export as MP4, H.264, CRF 18 (high quality), 60 fps.
- Also export a 30-fps version for social media (Twitter/X, LinkedIn).
- Thumbnail frame: Shot from Scene 5 (green checkmark in Signatures panel) — 
  the most distinctive visual differentiator vs. competitors.
- Upload to YouTube as "GlyphPDF v1.0.0 — Professional PDF Workstation Demo".
  Set as Unlisted until launch day, then switch to Public at announcement time.

---

*Script written: 2026-06-02*
*Production: [DATE — human to fill]*
*Produced by: [NAME — human to fill]*
