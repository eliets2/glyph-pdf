# Spec Sheet: LibreOffice Draw (The Document Foundation) — as a PDF workstation

Research date: 2026-09-07/08. Product state: **fully free open source (MPL 2.0), no paid tiers.** Current release
line as of research date is **26.8 (Aug 2026) / 26.2 (Feb 2026)**, with **25.8 (Aug 2025) / 25.2 (Feb 2025)** still
maintained (TDF wiki version sidebar) — the task brief's "current 24.x/25.x" is stale; 24.2/24.8 are EOL.
This sheet covers the modern behavior (26.x) with version-dating back to the 2018 import rewrite where it matters.

**The one-paragraph architecture story (everything below follows from it):** LibreOffice is a drawing/office
suite that *exports* excellent PDFs (shared, mature filter with PDF/A, tags, signing, encryption, hybrid ODF
embedding) but *imports* PDFs non-destructively-but-non-editably: since LO 6.1 (2018) the old Poppler
"convert-everything-to-Draw-objects" import was REPLACED by a PDFium-based filter that drops each PDF page in as
**one graphic object wrapping the original PDF bytes** (VectorGraphicData::Pdf, rendered on demand by PDFium) —
so the file renders faithfully, but page text is NOT directly editable. Real round-tripping happens via **Hybrid
PDF** (ODF embedded in the PDF). Primary spec sources: official help (26.2/26.8), TDF release notes (6.1→26.8),
LibreOffice core source code and git history, TDF bug tracker, Ask LibreOffice/Reddit for sentiment.

---

## 1. PDF open/import fidelity (what survives opening a PDF in Draw)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Import engine | PDFium-based `SdPdfFilter`; Poppler editable-shape import removed | Commit "sd: import PDFs as images using Pdfium new SdPdfFilter" 2018-03-14, shipped LO 6.1 (2018). The legendary "everything becomes editable Draw objects (mangled)" import is **gone** — replaced by faithful-but-opaque import. GRADE: TRUE (git history + source). |
| Page model | One PDF page = ONE graphic object; original PDF stream kept inside (GfxLink), re-rendered on demand at zoom | Source: `vcl/source/filter/ipdf/pdfread.cxx` builds `Graphic(VectorGraphicData(..., VectorGraphicDataType::Pdf))` from the original bytes. Fidelity of rendering is high (it IS the original content); editability is ~zero without extra steps. GRADE: TRUE (source). |
| Text on imported pages | Not directly editable; no text frames from import | Complaint corroboration: r/programming thread "why has no one created a functional FOSS PDF editor" — "LibreOffice Draw opens PDF as an image file in its editor". Escapes: GUI Shape>Break on the graphic; headless `--convert-to fodg:{"DecomposePDF":true}` "explode pdf into odf elements" (commit 2025-09-12, LO 26.2, Miklos Vajna/Collabora). GRADE: TRUE. |
| Legacy text-fragment repair tools | "Consolidate Text" merges fragmented text boxes "primarily to simplify editing fragmented content from imported PDFs" (LO 6.4, tdf#118370) | An official admission of the fragmentation problem; dates from the old import era but still ships for metafile text. GRADE: TRUE (release notes). |
| Images & vector art | Survive as part of the page graphic (PDFium rendering); tiling pattern fills imported (24.8, tdf#113050); clipped stroke paths imported (25.2, tdf#85428) | Steady fidelity work on the render path; still inside the single page object, not separately selectable objects. GRADE: TRUE (release notes). |
| Fonts | Rendered by PDFium from embedded fonts; no font editing/substitution UI for imported PDFs | No evidence of any ToUnicode-repair or font-replacement feature (cf. PDF-XChange). GRADE: MOSTLY_TRUE (negative evidence on tooling). |
| Encrypted PDFs | Password-protected PDFs open (password collected at type detection, forwarded to import); encrypted **hybrid** PDFs importable since 25.8 (tdf#55425); password passed through version downgrade (commit 2026-07) | Source + release notes. GRADE: TRUE. |
| AcroForm fields on import | NOT imported as fields; widgets have no import path in `sdpdffilter.cxx` (annotation import handles Polygon/Square/Circle/Ink/Highlight/Line/FreeText/Stamp only) | Consequence: you cannot fill or edit fields of an imported PDF in Draw. GRADE: MOSTLY_TRUE (source-code negative; user reports consistent). |
| Attachments, portfolios, layers (OCG), embedded media on import | No import path found | Page content only + annotations + links. GRADE: MOSTLY_TRUE (negative evidence). |
| Hybrid PDF reopen | A PDF exported with "Hybrid PDF (embed ODF file)" reopens in LO with full editable ODF (since 7.6 the ODF is stored as a PDF-compatible file attachment, e172 stable path) | This — not the PDF import — is LibreOffice's real round-trip: fidelity is lossless because it isn't PDF at all, it's the embedded ODF source. GRADE: TRUE (help + 7.6 release notes). |

## 2. Editing model (text/object — "hybrid-PDF" world)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Native Draw editing | Full object model: text boxes, shapes, curves, fills/transparency, layers, guides/grid/snap, connectors, 3D-ish effects, styles | Genuine, mature vector editor — for Draw/ODG content. GRADE: TRUE. |
| Editing imported PDF page content | Move/scale/delete the page graphic; overlay new objects on top; Shape>Break to explode into shapes (text becomes outlines/shapes, reflow never happens) | No direct text surgery on PDF content; no content-stream editing anywhere in LO. GRADE: TRUE. |
| Headless PDF→editable conversion | `soffice --convert-to 'fodg:OpenDocument Drawing Flat XML:{"DecomposePDF":{"type":"boolean","value":"true"}}' sample.pdf` (26.2+) | Explodes PDF into ODF elements at conversion time — batch-capable, but output is FODG (further editing → re-export PDF), quality of decomposition undocumented. GRADE: TRUE (commit + syntax). |
| Hybrid PDF export | Embeds the ODF source inside the exported PDF; reopen = full fidelity editing; encrypted hybrid export also possible (25.8 import side) | The flagship round-trip trick. Downside: attachment survives in the published PDF unless stripped — a data-leak consideration competitors flag. GRADE: TRUE. |
| Page management | Reorder/duplicate/delete/insert pages in the imported doc (page panel); page sizes >200 in. exported via PDF 1.6 markup (7.0) | Works on the graphic-per-page model. GRADE: TRUE. |
| Undo | Full undo/redo for Draw operations | GRADE: TRUE. |

## 3. Export-to-PDF quality

| Feature | Sub-capabilities | Notes |
|---|---|---|
| PDF versions (current dialog) | PDF 1.7 (default since 7.6), **PDF 2.0 (25.8+, ISO 32000-2, Tomaž Vajngerl/Collabora)**, PDF/A-1b (PDF 1.4 base), PDF/A-2b (PDF 1.7 base), PDF/A-3b (PDF 1.7 base), **PDF/A-4 (PDF 2.0 base, 25.8+)** — single combined dropdown (25.8 UI rearrange) | Verified from `filter/uiconfig/ui/pdfgeneralpage.ui` (master): exactly those 6 entries. **No PDF/A "u" or "a" variants exist in the UI** — no 1a/2a/3a/2u/3u; GlyphPDF's 2U/3U have NO LibreOffice counterpart. PDF/A-2 added 6.3 (tdf#62728); PDF/A-3 "minimum support" 6.4; PDF/A-3 associated-files `/AF` enabled 25.2-era (tdf#160196). GRADE: TRUE. |
| PDF/A conformance quality | Fonts embedded, tags written; veraPDF used as PDF validator for automated tests (26.8, tdf#136822); periodic veraPDF-reported fix waves (6.4 tdf#113448) | Honest in-house conformance testing, but only "b" levels; no user-facing conformance validator (veraPDF is test-only). GRADE: TRUE. |
| Tagged PDF | "Tagged PDF (add document structure)" checkbox; warns of file-size increase; auto-forced when PDF/UA selected | GRADE: TRUE. |
| PDF/UA | "Universal Accessibility (PDF/UA)" checkbox (7.0+, initially experimental), ISO 14289; documented checks (title, language, alt text, table merge cells, heading order, contrast, no footnotes...); Tools>Check Accessibility pre-flight | The check tool is Writer-centric; the checks list dates to "January 2020" in current help. PDF/UA only with PDF 1.7/2.0 (commit 2025-02-20). GRADE: TRUE (feature), MOSTLY_TRUE (Draw-specific depth). |
| Image handling | Lossless or JPEG with quality slider; resample/downsample DPI; EPS embedded-preview caveat | GRADE: TRUE. |
| Range & views | All / page ranges (3-6;8;10) / selection; Writer outlines→bookmarks; Impress notes pages; Calc whole-sheet export (6.4); hidden slides; auto blank pages | Draw gets range + selection; bookmarks/outlines are Writer-centric. GRADE: TRUE. |
| Watermark | Centered vertical light-green text watermark ("Sign with watermark"); fixed position/size; TiledWatermark via CLI (7.4+) | Not stored in source doc. GRADE: TRUE. |
| Use reference XObjects | Re-embeds original PDF data when exporting PDFs that contain imported PDF graphics | Interesting round-trip optimization: imported PDF pages can ride through export as original PDF objects. GRADE: TRUE (help). |
| External links on export | Handle document external links (26.2, tdf#167490/170309) | GRADE: TRUE (release notes). |
| Fidelity reputation | Very good: LO's PDF export is one of the most mature non-Adobe filters (decades of bug-fix waves: CJK emphasis marks, underline positions, font subsets, variable fonts 7.6) | Loved workflow: "make my poster/letter in Draw/Writer, export PDF/A". GRADE: TRUE. |

## 4. OCR

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Built-in OCR | NONE — no OCR in any LibreOffice module | Negative finding across docs/release notes/extension catalog. Users glue external Tesseract/OCRmyPDF onto LO pipelines. GRADE: PARTLY_TRUE (negative evidence, multiple searches) — i.e., absence is credible but proven by absence. |
| OCR via extensions | No maintained mainstream OCR extension found; historical/stale items only | GRADE: UNVERIFIABLE→absent. |

## 5. Forms (AcroForm)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Author PDF forms | "Create PDF form" section of export dialog (all modules incl. Draw) + Form Controls toolbar (text box, checkbox, radio, list, combo, button, ...) | Field types = whatever LO form controls exist; no native signature/date-filed types like PDF-XChange. GRADE: TRUE (help). |
| Submit formats | FDF, PDF, HTML, XML; duplicate-field-name control | GRADE: TRUE. |
| Form-field export parity | Numeric/Currency/Date/Time form fields exported to PDF since 7.4 (tdf#105972) | GRADE: TRUE. |
| Filling forms | Fill its OWN exported forms; **cannot fill fields of an imported PDF** (widgets not imported) | Workflow-killing gap vs every real PDF editor. GRADE: TRUE (source-derived; consistent user reports). |
| Form JavaScript / calculations in PDF | No evidence of AcroForm JS calculation export | GRADE: PARTLY_TRUE (negative). |
| XFA | Not supported | GRADE: MOSTLY_TRUE (negative). |
| Form data import/export (FDF/CSV/XML tooling) | Not found as a user feature (submit formats only) | GRADE: PARTLY_TRUE (negative). |

## 6. Comments & annotation round-trip

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Import PDF annotations | Real sd annotations with author/date/position/text; types: Polygon, Square, Circle, Ink, Highlight, Line, FreeText, Stamp; **threaded comments**: /IRT reply-parent resolution, /State(/StateModel=Review) collapsed to Resolved flag; Acrobat-matching hidden-state-change skipping (commits 2026-04 → 26.2/26.8) | Surprisingly modern comment round-trip IN. Source: `sdpdffilter.cxx`. GRADE: TRUE. |
| Import links | Link annotations imported (`setLinkAnnotations`) | GRADE: TRUE. |
| Export comments to PDF | "Comments as PDF annotations" option documented for **Writer and Calc documents only**; "Comments in margin" Writer only (7.5, tdf#77650) | Draw/Impress comment→PDF-annotation export not documented in help; 7.6 added ink/freetext/polygon annotation export in the shared path (Collabora/LOKit context). GRADE: TRUE (help), PARTLY_TRUE (Draw-side export). |
| Comment UI | Reply/resolve threading in the Draw comment sidebar; navigation | Newer than most competitors assume. GRADE: MOSTLY_TRUE. |

## 7. Redaction — GRADED against GlyphPDF content-excision

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Redaction tool | Tools>Redact converts the document to a Draw drawing (lossy re-import!) and shows a Redaction toolbar: Rectangle Redaction, Freeform Redaction, black or white fill; introduced 6.3 (2019, Muhammet Kara/Collabora) | Marks are ordinary shapes; transparent gray until export. GRADE: TRUE (help + 6.3 notes). |
| Making redaction permanent | "Export as Redacted PDF (Black/White)" **rasterizes the ENTIRE document** to bitmaps ("pixellized PDF"); per 6.3 notes: "There will be no selectable text in it, and the redacted content will be non-existent"; help: redaction shapes are "exported as pixels" | Security result is real (content genuinely gone — not a black-box overlay), but it is **whole-document rasterization, not content-stream excision**: no text anywhere survives, file size balloons, zoom quality dies. GRADE: TRUE (help + release notes + source behavior). |
| Output quality | Fixed low rasterization resolution, **no resolution control**; confirmed bug tdf#128089 "Redacted documents are saved in low quality"; Ask LO threads + r/libreoffice "Redact PDF in LibreOffice Draw - low resolution" | Blunt instrument: legible-print quality not guaranteed; only workaround is manual black shapes + normal export — which (correctly) leaves text extractable underneath, i.e. insecure. GRADE: TRUE (bug tracker + threads). |
| Auto-Redaction | Tools>Auto-Redact: search-term targets with scope (complete words etc.); **image redaction targets ("All Images") added 25.8** (tdf#139331) | No documented regex/pattern presets (emails/SSN/phones), no code sets, no redaction log/certificate. GRADE: TRUE (feature), PARTLY_TRUE (no-regex claim = negative evidence). |
| Safety/assurance UX | No preview-before-apply (the Draw conversion IS the preview), no sanitize bundle offer, no verification report, no post-export hidden-text scan | Underlying-text removal is achieved by rasterization only; nothing audits that e.g. metadata or out-of-mark content is clean. GRADE: MOSTLY_TRUE (negative). |
| **Verdict vs GlyphPDF** | LO redaction = "flatten to pixels": secure-ish result, hostile artifacts. GlyphPDF = surgical stream excision + transaction + sanitize + pattern presets + overlay labels + word-list import | LibreOffice cannot match selective excision, nor honest post-conditions; it compensates with price and ubiquity. **Grade: PARTLY_TRUE that LO "has redaction" in the professional sense** — it has a redaction feature, not a redaction capability. |

## 8. Security (encryption, signing incl. GPG)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Open-password + permission-password export | Set Passwords dialog: view password + edit/print permission password | GRADE: TRUE. |
| Permission flags | Printing (none/150dpi/high); changes (none/insert-delete-rotate pages/fill forms/comment+fill/any except extract); enable copy; enable accessibility text access | Help honestly warns restrictions are honored only by PDF-1.5+ compliant readers. GRADE: TRUE. |
| Encryption algorithms | AES-128 historically; **modern AES-256 implemented for PDF 2.0 (mandatory there), 25.8**; SHA384/AES-192 open support 25.8 (tdf#166241) | From release notes/source; no user-facing algorithm picker. GRADE: MOSTLY_TRUE. |
| Digital signature at export | X.509 cert from default key store (NSS) or smartcard; cert password/PIN; Location/Contact/Reason fields; RFC 3161 TSA timestamp URL; single signature computation for timestamp since 25.8 (tdf#147452) | GRADE: TRUE (help). |
| **Signed-PDF compatibility** | "The format of PDF documents signed on export is now compatible with Adobe Acrobat Reader" — **25.8, tdf#121133 (bug open since 2019)** | Before Aug 2025, LibreOffice-signed PDFs could fail Acrobat validation — a huge practical caveat for any audit trail relying on older LO. GRADE: TRUE (release notes + bug ID). |
| Signing EXISTING PDFs | Draw: visible digital signatures added to existing PDF files (7.1, Miklos Vajna); signature verification based on PDFium (7.2); PIN prompt UX improvements | Rare free capability; appearance/visibility options basic vs GlyphPDF's picker/initials/appearance work. GRADE: TRUE. |
| GPG / OpenPGP | GPG keys sign **ODF documents and macros** (6.1+, allotropia); "GPG Certificate Manager" button renamed 25.2 to clarify GPG-only scope; **PDF signing is X.509 only** — help: "signed PDF export uses the keys and X.509 certificates already stored in your default key store" | No GPG-backed PAdES-style PDF signing found. GRADE: TRUE. |
| PAdES conformance labeling | No PAdES B-B/B-T/B-LT/B-LTA profile claims found anywhere | LTV-ish pieces exist (TSA) but no conformance story. GRADE: UNVERIFIABLE→absent. |
| Metadata sanitization on export | No user-facing sanitize/redact-metadata bundle for PDF export found | GRADE: PARTLY_TRUE (negative). |

## 9. Compare

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Document compare | NONE in Draw; no PDF-vs-PDF compare anywhere in LibreOffice | Writer has redline "Compare Document" for text formats only. Negative finding across docs/notes. GRADE: PARTLY_TRUE (negative evidence). |

## 10. Batch / CLI / scripting (their real superpower)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Headless conversion | `soffice --headless --convert-to pdf[:filter][:options] --outdir` — any loadable format → PDF, and PDF→many targets; wildcard input; infinite reuse in scripts/CI | Works on Windows/Linux/macOS; no license gates. GRADE: TRUE. |
| **Full PDF-export options from CLI (7.4+)** | JSON filter data: `pdf:draw_pdf_Export:{"PageRange":{"type":"string","value":"2-"}, "TiledWatermark":{"type":"string","value":"draft"}, "EncryptFile":true+DocumentOpenPassword, "SelectPdfVersion":15, "SignPDF":true+SignCertificateSubjectName:"CN=..."}` (Miklos Vajna/Collabora; tdf#141340 origin) | Encrypt/sign/range/watermark/version — the whole dialog, scriptable. Few paid tools expose signing from a bare CLI. GRADE: TRUE (vmiklos blog + release notes). |
| UNO API | Complete document model automation from Python/Java/Basic (com.sun.star.*); UNO sockets/listeners; PDF export filter DataProperties = every dialog option | GRADE: TRUE. |
| Macros & scripting UI | Tools>Macos (Basic IDE), Python scripts, event bindings, extension-packaged macros | GRADE: TRUE. |
| Server patterns | unoserver / unoconv (community), Collabora Online /cool/convert-to HTTP endpoint with same JSON options | De-facto standard for mass document→PDF conversion in FOSS land. GRADE: TRUE. |
| Hot-folder | No built-in watcher; achieved by scripting/OS tools on top of the CLI | Feature gap vs GlyphPDF's built-in hot folder, but trivially assembled. GRADE: TRUE. |
| Batch honesty | No pre-flight capability disclosure; failures surface as exit codes/stderr only | GRADE: MOSTLY_TRUE. |

## 11. Accessibility / tagging

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Tagged PDF export | Checkbox, auto-forced under PDF/UA | GRADE: TRUE. |
| PDF/UA export + checks | ISO 14289 checkbox; accessibility checker (Tools>Check Accessibility, Writer-centric); documented rule list (alt text, heading order, contrast, merged cells...) | No tag-structure editing pane, no reading-order tool for Draw objects. GRADE: TRUE/MOSTLY_TRUE. |
| Alt text | Per-object description/alt text fields (name/dialog) in Draw | Feeds PDF/UA checks. GRADE: MOSTLY_TRUE. |

## 12. Import/export formats (Draw-relevant)

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Import to Draw | ODG/OTG/FODG, **PDF** (PDFium), Adobe Illustrator (PDF-based), PostScript/EPS (preview-level), plus librevenge-family: Visio VSD/VSDX (libvisio), CorelDRAW CDR/CMX (libcdr), Publisher (libmspub), Freehand (libfreehand), PageMaker (libpagemaker), QuarkXPress (libqxp), Zoner Draw (libzmf), CGM, images (PNG/JPEG/GIF/TIFF/BMP/WMF/EMF/SVG) | Unmatched legacy-format net for a free tool. GRADE: TRUE (help/docs; per-format depth varies). |
| Export from Draw | PDF/FDF (full dialog), ODG/FODG, SVG, PNG/JPEG/TIFF/BMP/GIF/EPS/EMF/WMF, HTML gallery (Impress), XHTML | GRADE: TRUE. |
| PDF→Office/text export | NOT a Draw feature; Writer/Calc open PDFs? No — PDF text extraction to Writer not offered (PDF opens in Draw only) | No PDF→Word/Excel conversion at all. GRADE: TRUE (negative). |
| Metadata | Document properties dialog; no XMP editor for PDFs found | GRADE: MOSTLY_TRUE (negative). |

## 13. Extensions / ecosystem

| Feature | Sub-capabilities | Notes |
|---|---|---|
| Extension manager + catalog | extensions.libreoffice.com; templates; dictionaries; gallery themes | GRADE: TRUE. |
| PDF-relevant extensions | Legacy "PDF Import" (Sun era) is obsolete/built-in now; no active must-have PDF extensions found | GRADE: MOSTLY_TRUE (negative). |
| Derived ecosystem | Collabora Online/LibreOffice-Online (LOKit), unoserver, unoconv, countless document pipelines; TDF design/QA community | The ecosystem, not the app, is the moat for batch. GRADE: TRUE. |
| Release cadence | Feb/Aug feature releases, ~7-year-old-line support via derivatives (e.g. CIB, Collabora LTS) | GRADE: TRUE. |

---

## GlyphPDF delta (bidirectional; "GAP" = LibreOffice has it, GlyphPDF doesn't; "WEAK" = GlyphPDF has it, LibreOffice doesn't)

- **Import/round-trip fidelity: WEAK on their side is GlyphPDF's headline.** LO imports PDF as one opaque page graphic; no text surgery, no form filling, no font repair on imported PDFs. GlyphPDF's content-stream-level editing, safe-save transactions, and excision operate where LibreOffice cannot. (Their headless DecomposePDF→FODG, 26.2, is a partial nod — batch-only, lossy, wrong output format for round-trip.)
- **Batch/automation: GAP (their genuine differentiator).** `--headless --convert-to` with JSON filter options including **signing and encryption from the command line** (7.4+), UNO API, unoserver. GlyphPDF has batch + hot folder but no public scripting API — a UNO-like automation surface (CLI verbs for redact/sign/watermark/version) is the single most copyable LibreOffice advantage.
- **PDF/A coverage: mixed.** GAP: they ship **PDF/A-4 and PDF 2.0 export** (25.8) and run **veraPDF in CI**. WEAK on their side: only "b" levels — **no 1a/2a/2u/3u** (GlyphPDF's 2U/3U exceed them); GlyphPDF's optional veraPDF CLI validation mirrors their CI practice — surface it.
- **Redaction: WEAK on their side.** Whole-document rasterization at uncontrolled low resolution (tdf#128089) vs GlyphPDF's excision + sanitize + patterns + word-lists + overlay labels + transaction outcomes. LO's AutoRedact image-targets (25.8) is the one idea worth mirroring ("redact all images" as a first-class target).
- **Signatures: PARTIAL.** GAP: free visible signing of existing PDFs (7.1) and CLI signing. WEAK on their side: no PAdES profile claims, X.509-only (GPG is ODF-only), and their signed-export format was Acrobat-INcompatible until 25.8 (tdf#121133) — GlyphPDF's PAdES B-LT/B-LTA + appearance + retry UX is concretely ahead.
- **Forms: WEAK on their side** (authoring via generic form controls exists; no filling of imported PDFs, no field-level tooling, no FDF/CSV data I/O) vs GlyphPDF's 10 field types, auto-detect, undo, data I/O.
- **Comments: PARTIAL.** GAP: their 26.x threaded-comment import (IRT/State resolution) is more rigorous than many paid tools; GlyphPDF's comment manager should import PDF reply threads with resolve-state the same way. WEAK on their side: Draw comment→PDF-annotation export is undocumented.
- **Compare: WEAK on their side** — nothing. GlyphPDF's structural+text compare with fingerprints has no LibreOffice analogue at any price.
- **OCR: WEAK on their side** — nothing built in; GlyphPDF's dual-engine Tesseract+PP-OCRv5 ROVER with review workflow is a clean differentiator.
- **Accessibility: GAP (moderate).** PDF/UA export checkbox + accessibility checker + forced tagging; GlyphPDF has only a reading-order check. Their checker is Writer-centric — a Draw-class a11y checker for page content is open space.
- **Import/export formats: GAP (breadth).** Visio/CorelDRAW/Publisher/PageMaker/QuarkXPress import and a free cross-platform story; irrelevant to PDF-native workflows but relevant to marketing "which tool opens my old files".
- **Cost/trust: GAP.** Free, MPL 2.0, zero phone-home, runs everywhere — the default answer for "free PDF tool" that GlyphPDF must beat on PDF-native capability, not price.

## Sources (spec-level, with verdicts)

1. LibreOffice Help 26.2/26.8: Redaction (`text/shared/guide/redaction.html`), Automatic Redaction (`auto_redact.html`), Export as PDF (`ref_pdf_export*.xhp` set: general/security/digital-signature/universal-accessibility) — feature behavior, permission flags, X.509/TSA signing, PDF/UA checks, comments-in-margin scope: TRUE.
2. TDF Release Notes (raw wiki): 6.1 (import era), 6.3 (redaction intro + "pixellized PDF" wording + PDF/A-2, tdf#62728), 6.4 (AutoRedact + Consolidate Text tdf#118370 + PDF/A-3 commit date), 7.0 (PDF/UA + >200in pages), 7.1 (visible signatures on existing PDFs, vmiklos), 7.2 (PDFium signature verification), 7.4 (CLI JSON filter options), 7.5 (comments in margin), 7.6 (PDF 1.7 default; hybrid ODF as PDF attachment; annotation export), 24.8 (tiling patterns import; GPG UX), 25.2 (clip stroke import; GPG manager rename), 25.8 (PDF 2.0 + AES-256 + PDF/A-4 + Acrobat-compatible signatures tdf#121133 + encrypted hybrid import tdf#55425 + AutoRedact images tdf#139331), 26.2 (external links; DecomposePDF), 26.8 (veraPDF in automated tests): TRUE.
3. LibreOffice core source (master): `sd/source/filter/pdf/sdpdffilter.cxx` (page-per-graphic import; annotation types; IRT/State threading; links; password passthrough), `vcl/source/filter/ipdf/pdfread.cxx` (VectorGraphicData::Pdf from original bytes; password/interaction), `filter/source/pdf/impdialog.cxx` + `filter/uiconfig/ui/pdfgeneralpage.ui` (exact PDF version list: 1.7/2.0/A-1b/2b/3b/A-4; forms section; duplicate names), commit history of `sdpdffilter.cxx` (2018-03-14 "import PDFs as images using Pdfium"): TRUE.
4. Git history searches: "Add minimum support for PDF/A3" 2019-09-20 (→6.4); tdf#62728 PDF/A-2 2019-03 (→6.3); "initial PDF 2.0 and PDF/A-4 support" 2024-10-31 (→25.8); "enable assoc. files in PDF/A-3" tdf#160196 2025-01; "optionally explode pdf into odf elements" 2025-09-12 (→26.2): TRUE.
5. Miklos Vajna blog "pdf-convert-to" (vmiklos.hu): CLI JSON option syntax incl. PageRange/TiledWatermark/Encrypt/SignPDF examples: TRUE.
6. TDF Bug tdf#128089 (redacted export low quality, confirmed) + tdf#121133 (signed export vs Acrobat, open 2019→fixed 25.8): TRUE.
7. Sentiment (secondary corroboration only): r/libreoffice "Redact PDF in LibreOffice Draw - low resolution" (vwj10f); Ask LibreOffice 66319 + 76280 (no resolution setting); r/programming yn16ix "Draw opens PDF as an image file in its editor": MOSTLY_TRUE as sentiment quotes.
8. Negative findings (no OCR, no compare, no PAdES claims, no PDF/A u-levels, no GPG PDF signing): PARTLY_TRUE by absence across 3+ source classes each (docs, release notes, extension catalog, code).

**Confidence: High (~0.9).** Load-bearing architecture claims verified against source code and git history, not documentation alone. Residual gaps: exact Draw-side comment-export behavior (help is Writer/Calc-scoped; 7.6 annotation-export note is shared-path), DecomposePDF output quality (undocumented), and rasterization DPI constant for redacted export (bug says low; exact value not pinned).
