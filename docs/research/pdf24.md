# Spec Sheet: PDF24 (Creator 11 + Web Tools) — Deep-Dive for GlyphPDF

**Date:** 2026-09-07
**Requested by:** GlyphPDF parity program (research-specialist protocol)
**Research question:** What does the genuinely-free German offline toolkit actually ship (desktop Creator + web Tools), where are its feature gaps (compress quality, edit depth, OCR quality, redaction), failure modes (install/bundleware history, update mechanism), and loved free-tier workflows — so GlyphPDF can rank build priorities while staying offline-first/local-only?
**Method:** Primary-source extraction via curl + full-text mining of: pdf24.org (home, /en/creator product page + FAQ), tools.pdf24.org (all-tools catalog, /compress-pdf, /ocr-pdf, /redact-pdf, /sign-pdf, /edit-pdf, /compare-pdf), creator.pdf24.org/manual/11/ (full v11 manual: installer switches, 14 registry-setting sections, PDF printer ports, pdf24.exe/Toolbox/Ocr/DocTool/Reader CLIs, FAQ), creator.pdf24.org/changelog/en.html (634 KB, full version history through 11.30.1), German Wikipedia (PDF24 Creator). Secondary: web searches (Reddit r/de_EDV, r/sysadmin, r/software, r/BuyFromEu, Chocolatey package comments, PDF24 help forums, TechRadar, HonestPDF privacy review). Absence claims graded against the full all-tools catalog + manual TOC + changelog, not search-engine recall.
**Benchmark baseline:** GlyphPDF ledger `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` §9/§27/§28.

**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per protocol).

---

## 0. Product snapshot

| Dimension | State | Verdict |
|---|---|---|
| Company | Geek Software GmbH (Germany), lead developer Stefan Ziegler; developed in Germany since 2006; proprietary freeware, written in C++ | TRUE (de.wikipedia infobox + official site footer "© 2026 Geek Software GmbH") |
| The one-line model | 100% free, no paid tier, no feature gates, free for private AND commercial use — funded by ads on the web tools and the paid online fax service; the desktop Creator is the ad-free offline twin of the web catalog | TRUE (FAQ verbatim: "can be used completely free of charge and without any restrictions. Companies, authorities and agencies can also use PDF24 Creator free of charge"; web tool pages: "100% free thanks to advertising") |
| Version / cadence | Creator 11.30.1 (2026-04-21/24) is current; changelog shows a very active cadence (multiple releases/month); parallel **9.20.0 LTS branch** = same codebase minus Toolbox/OpenJDK/WebView2, still supports Windows 7 | TRUE (creator page + changelog) |
| Platform | Windows only (10/11 for 11.x; Win 7 via 9.x). EXE (Inno) + MSI + x64/arm64/x86 + Microsoft Store. Citrix/Terminal Server supported since v2.6.2 (2009) | TRUE (creator FAQ + manual + dewiki) |
| UI languages | 34 | TRUE (dewiki, cited to official docs) |
| Installed architecture (v11) | Bundle of private instances: **Ghostscript 10.07.0, QPDF 12.3.2, Tesseract 5.5.2, OpenJDK 17.0.18.8 (PDFBox for Toolbox), WebView2 runtime, own "pdflib", PDFium in the Reader** — all shipped/bundled, no system dependencies | TRUE (changelog 11.30.0 component-update entries + manual: "app used to apply PDF security … gs or qpdf, qpdf is used by default because 256 bit security is supported"; Reader = "PDFium updated to the latest version") |
| Components | PDF Creator/Assistant (drag-drop assembly), **PDF Printer(s)** (virtual printers + assistant + profiles), **PDF Reader** (tabbed, PDFium), **Toolbox** (WebView2 UI, the tool catalog), **File Tools** (batch + Explorer context menu), **DocTool** CLI, **pdf24-Ocr.exe**, **Tray icon + pdf24.exe backend service**, Shell File Tools DLL, Output Profile Manager | TRUE (manual §2–§14 component/registry sections) |
| Adoption | >10 million downloads on Chip.de alone (rank 3, PDF category, Feb 2026); TechRadar "best free PDF editor overall"; r/de_EDV: "best freeware on the market. I use it on over 5000 clients" | MOSTLY_TRUE (dewiki for Chip; TechRadar/Reddit single-platform quotes, directionally corroborated by 134k ratings on the creator page at 4.88/5) |
| Scale of catalog | ~70 tools in the shared catalog (24 have web versions; the rest run inside the desktop Toolbox or via profiles/printer), grouped: Create, Invoices, Edit, Organize, Optimize & repair, Security & privacy, View & check, Convert to PDF (~27 input formats), Convert from PDF, Convert images | TRUE (tools.pdf24.org/en/all-tools enumerated 2026-09-07) |

**Reputation framing:** No Nitro-style license scandal, no Classic-OpenCandy-style third-party bundleware found. The criticisms are domestic: default-printer takeover, autostart backend/service, Assistant popups, promotional banner, installer/upgrade friction, and — the big one — a 2026 bug where a "local" toolbox tool uploaded files to the cloud (see §3 Failure modes).

---

## 1. Spec tables by domain

Legend: **Surface** — D = PDF24 Creator desktop (offline, local processing); W = PDF24 Tools web (cloud processing on German servers, files auto-deleted after 1 hour, SSL). **Cloud?** = does the feature's default path require upload/network. **Free** = verified free (all features are free on both surfaces; fax is pay-per-page).

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| PDF Reader (standalone viewer) | Tabbed multi-doc, bookmark/TOC panel (rename entries via F2/context menu; full add/delete/move "still pending" per changelog), text selection incl. forms, double-click word select, text-link recognition, snapshots, zoom tools, spacebar scroll, forward/back history | D | Yes | N | PDFium-based, positioned as "lightweight, low resource, fast start" replacement for other readers. TRUE (creator page + changelog) |
| Interactive page-object manipulation in reader | Click-select page objects (images/text) with highlight, then **move (drag-drop) or delete** them; save changes | D | Yes | N | Shipped 11.30.0 (2026-04). Lightweight object-level editing inside a *reader* — notable scope creep toward editing |
| Form filling in reader | AcroForm fields + **XFA forms support** (registry-toggleable), mailto submit | D | Yes | N | XFA support is rare in free tools. TRUE (manual Reader registry: XFA enable/disable; changelog "Support of the XFA forms") |
| Save/read helpers | Save compressed from reader; open current file in Toolbox ("transfer to Toolbox"); PDF/A and PDF/X version shown in doc info; repair path via Toolbox Repair tool | D | Yes | N | Reader↔Toolbox handoff is their version of tool chaining |
| Web viewer ("View as PDF") | Basic open/view in browser | W | Yes | Y | Thin; the real viewer is desktop-only |
| Set PDF viewer preferences | Page mode, layout, hide toolbars etc. | W (+D via profiles) | Yes | W: Y | Niche tool with no GlyphPDF equivalent |
| Two-page/presentation/dark reading modes | — | Neither | — | — | Not found in manual/changelog/product pages (Reader zoom/print/selection emphasized instead). MOSTLY_TRUE absent — GlyphPDF §9.1 (continuous/two-page/presentation, dark mode) leads here |
| Update cadence of viewer | Reader gets fixes nearly every release (print dialogs, PDFium updates, startup time) | D | Yes | N | |

**GlyphPDF delta:** GlyphPDF wins on reading ergonomics (two-page, presentation, dark mode, search shortcuts) and on viewer honesty plumbing (capability registry). PDF24 wins on deployment reach (tabbed reader that enterprises actually deploy as an Acrobat-Reader replacement, XFA filling, object-level move/delete in-reader) and on the Reader→Toolbox handoff pattern — worth copying as a "send to tool" affordance. No navigation-sync equivalent exists on their side (nothing to sync — single pane).

### 1.2 Editing (text / object / page)

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Edit existing PDF text | **NOT supported** | W+D | — | — | The Edit tool draws NEW content on an overlay over the page; official tip in the tool itself: "Convert a PDF to Word to edit the text." Editor is SVG-overlay based (svg2pdf pipeline; crop-box transformation bugs they fixed confirm the overlay architecture). GlyphPDF's inline native text edit (§9.2) is categorical over PDF24. TRUE (mined tool UI + changelog overlay entries) |
| Add content (overlay editor) | Text boxes (font family, line height, opacity, color, stroke), freehand brush (pencil/spray/square/circle/HLine/VLine/diamond shapes, brush width), images (incl. WEBP since svg2pdf update), full-size/full-width view modes, modified-flag unsaved-changes prompt | W+D | Yes | W: Y | Bold/italic + user fonts visible in toolbox; multi-language glyphs via svg2pdf. Saving runs through a Job Monitor with progress messages |
| Annotate PDF | Same overlay editor tuned for markup; color picker | W+D | Yes | W: Y | Overlay annotations, not a PDF-annot comment model |
| Fill out PDF | Field-aware fill of AcroForms | W+D | Yes | W: Y | Desktop reader also fills XFA (§1.1) |
| Create fillable PDF form | Form editor added to Toolbox **11.30.0 (2026-04)**: edit existing forms + make flat PDFs fillable | D | Yes | N | Brand-new; 11.30.1 immediately fixed it uploading files to the online service when run locally (see §3). Field-type palette breadth not yet documented — UNVERIFIABLE beyond "edit + make fillable" |
| Page organization | Merge (incl. insert blank pages, per-page mode, range selection), Split, Rearrange (drag), Remove pages, Extract pages, Rotate, Pages-per-sheet (N-up, 2-up + **3-up** added 11.30.0), **Halve PDF pages** (2-up scan splitter), Insert blank pages | W+D | Yes | W: Y | Deep and free; parity with paid tools for page surgery |
| Page furniture | Watermark (behind content option, opacity), Page numbers, PDF Overlay (letterhead/stamp under/over), Crop (rotated-page aware since 11.30.0), Change page size, Bookmark PDF (bookmark editor) | W+D | Yes | W: Y | Overlay/paper also available as an output-profile function with URL sources (SharePoint use case) |
| Metadata editor | Change PDF document information; Remove PDF metadata | W+D | Yes | W: Y | Two separate dedicated tools |
| Repair / rasterize / flatten | **Repair PDF** (rebuild broken files), Rasterize PDF, Flatten PDF | W+D (repair primarily D) | Yes | W: Y | Repair is a differentiator — GlyphPDF has no repair tool (PRD §9 edge cases handle broken files in-place, but no user-facing "repair" verb) |
| Undo/redo | Tool-local; modified-flag confirm before discard | D | Yes | N | No session-level undo stack across tools |

**GlyphPDF delta:** This is PDF24's weakest professional domain and GlyphPDF's core strength: inline existing-text editing vs overlay-only; full object model (move/resize/rotate/layer) vs add-on overlay; safe-save transactional writes vs save bugs fixed repeatedly (11.26.1 "Edit/Sign/Annotate save failures", overlay transformation misplacements). Where PDF24 beats GlyphPDF on *breadth*: Repair PDF, Rasterize, Halve/N-up (3-up), bookmark editing as a tool, crop rotated pages, page-size changer. Cheap wins worth absorbing: Repair PDF and Halve/N-up breadth.

### 1.3 OCR

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| OCR engine (desktop) | **Tesseract 5.5.2**, local process `pdf24-Ocr.exe` (GUI + CLI), result combined with source pages; profiles can post-process (e.g., PDF/A) | D | Yes | N (except language files) | TRUE (manual §10: "Under the hood, the app uses Tesseract") |
| Languages | **219 language entries** on the web selector (full Tesseract traineddata set incl. Fraktur, vertical CJK, Cherokee, Inuktitut…); desktop downloads traineddata **on demand**; fully-local install possible via `trainDataList.txt` in the tesseract folder | W+D | Yes | First use of a language: Y (download); W processing: Y | Counted from the live OCR page option list 2026-09-07. TRUE. GlyphPDF bundles a smaller set by default but has local engines — network-on-first-use is a real difference for air-gapped machines |
| OCR options | DPI (300 recommended), language(s) multi-select `eng+deu`, **segmentation mode** (auto/textBlock/sparseText/any Tesseract psm — 11.30.0), remove background, deskew pages, auto-rotate pages (text-based), skip files with text, skip pages with text, **remove existing text**, force OCR, combine to one file, output PDF or **PDF/A**, metadata setting | W+D | Yes | W: Y | CLI mirrors GUI switches (-skipFilesWithText, -skipPagesWithText, -deskew, -autoRotatePages, -segmentationMode). Deskew has a documented limitation: only when the page has exactly one image |
| OCR review / correction | **None** | — | — | — | No confidence scores, no word-level review screen, no editable result — run-and-pray. GlyphPDF's OCR Verify screen (§9.4, U03) and ROVER dual-engine ensemble (Tesseract 5 + RapidOCR + PP-DocLayout) are categorical quality advantages |
| OCR quality posture | Single engine (Tesseract), quality depends on preprocessing switches | D | — | — | Deskew/remove-background/auto-rotate help, but no binarize/denoise pair like GlyphPDF's OcrPreprocessor, no ensemble voting, no confidence gating |
| Batch OCR | Multi-file lists in OCR GUI; CLI for automation; error column in status list | D | Yes | N | |

**GlyphPDF delta:** GlyphPDF clearly ahead on quality machinery (dual-engine ROVER ensemble, layout model, verify screen, confidence warnings, binarize/denoise). PDF24 ahead on language coverage (219 vs bundled set), psm control, skip-already-text intelligence (a lovely batch-friendly touch — worth copying), and OCR→PDF/A one-step profiling.

### 1.4 Forms

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Fill AcroForms | Reader + "Fill out PDF" tool + toolbox; pointer cursors in form fields; mailto submit | W+D | Yes | W: Y | XFA fill supported in desktop Reader |
| Create/edit form fields | Toolbox **PDF Form Editor (11.30.0)**: edit existing forms, make flat PDFs fillable | D | Yes | N | Newest tool in the catalog; upload-to-cloud bug fixed in 11.30.1. Depth (field types, calc fields, validation, tab order) UNVERIFIABLE — not yet documented |
| Auto field detection | — | Neither | — | — | No evidence in catalog/changelog. GlyphPDF's content-aware auto-detect (ledger 88d4286) leads |
| Field types | Undocumented beyond generic; no evidence of calculated fields, validation rules, comb, tab-order editor | D | — | — | GlyphPDF ships 10 field types incl. calculated (PRD §9.6, §27) — far ahead |
| Form data import/export | — | Neither | — | — | No FDF/XFDF/CSV/XML data exchange found anywhere. GlyphPDF CSV/FDF (§27) wins |
| Flatten forms | Flatten PDF tool; compress tool also has "Flatten form" toggle | W+D | Yes | W: Y | |

**GlyphPDF delta:** Forms are effectively a greenfield for PDF24 (one month old). GlyphPDF is a generation ahead on depth (calculated fields, validation, tab order, data exchange, auto-detect, undo-verified form editing per V06/F01/R02). Watch item: PDF24 iterating fast on the form editor — their release cadence is ~weekly.

### 1.5 Comments & markup

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Markup drawing/highlighting | Annotate PDF tool (overlay ink/shapes/text) | W+D | Yes | W: Y | Visual layer only |
| Comment model (sticky notes, threads, statuses, filters, summaries, reply) | — | Neither | — | — | No comment manager anywhere; Reader only shows annotation tooltips. GlyphPDF U07 (threads, filters, table view, CSV export) has no PDF24 counterpart |
| Annotation stripping | Compress tool "Strip annotations" checkbox; Reader deletes individual objects | W+D | Yes | W: Y | Sanitization-adjacent, undocumented as privacy feature |

### 1.6 Redaction

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Redact/blacken tool | Manual drawing of black boxes (stroke width, brush/spray/square shapes, color); pages then made "really safe" | W+D | Yes | W: Y (D: N) | W version = draw strokes in browser UI |
| Security mechanism | **Pages are rendered to an image and the image replaces the page** — content below black boxes becomes irretrievable; a pluggable "security provider" (`blackenSecurity.provider` registry) picks the best image format per page; DPI + JPEG quality configurable (`blackenSecurity.dpi`, `blackenSecurity.jpgQuality`) | D | Yes | N | TRUE (changelog verbatim: "the pages are rendered as an image and then replaced with the actual page. This then makes it impossible to make the redacted content visible again") |
| Scrubbing of redacted pages | Redaction also **removes page annotations** and **deletes page resources** (may contain hidden images/overlays) on redacted pages | D | Yes | N | Changelog verbatim — genuine defense-in-depth thinking |
| Pattern redaction (emails/phones/IDs/keywords) | — | Neither | — | — | Absent. GlyphPDF pattern redaction + word-list import (§9.8, 9.8-c) leads |
| Redaction log / preview-before-burn / selective text excision | — | Neither | — | — | Absent. And because the whole PAGE is rasterized, **all remaining text on a redacted page loses selectability/searchability** (it's now an image) — a real usability cost vs GlyphPDF's surgical content-stream excision (U05, E-1 fix) that preserves surrounding text byte-exact |
| Compliance story | No redaction codes/PRF (FDA-style) support | — | — | — | Absent |

**GlyphPDF delta:** Both are "safe by destruction," but differently: PDF24 guarantees the redacted page can't leak (rasterization) at the cost of nuking the page's text layer and incurring image quality loss (registry-tunable DPI/JPEG quality, default undocumented); GlyphPDF excises only the marked spans and keeps the rest live text with transaction safety + sanitize bundle + logs. GlyphPDF's approach is what legal/compliance buyers need; PDF24's is what casual users get for free. Marketing angle: "redact one line, keep the other 400 lines searchable — PDF24 rasterizes the whole page."

### 1.7 Security (encrypt / sign)

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Password protection | Open + owner passwords, permission bits; applied via QPDF **AES-256 by default** (gs path legacy, removed 11.7.0) | D (profiles/print/save) + W Protect tool | Yes | W: Y | TRUE (manual: "qpdf is used by default because 256 bit security is supported") |
| Unlock / remove protection | Unlock PDF tool; remembers already-unlocked files to skip re-unlock | W+D | Yes | W: Y | |
| Visual signing | Draw / upload image / camera signatures, place and scale; stroke+fill options; images signable | W+D | Yes | W: Y | Web tool UI mined: Draw/Upload/Camera |
| Certificate-based (cryptographic) signing | **Exists on desktop**: self-signed certificate creation tool + Windows certificate manager integration + signature-card (smartcard) support via Windows middleware; signing wired into editor/save ("signature" among save-as profile options: "Options for PDF meta data, password security, … enclosures, signature") | D | Yes | N | MOSTLY_TRUE (older changelog documents the feature set verbatim incl. signature cards; current cert-creation lockdown keys NoSelfSignedCertCreation / NoCertManagerAccess exist for enterprises; exact current UI surface in 11.30 not visually verified). **No evidence of PAdES B-LT/B-LTA, LTV, DSS, or timestamp-authority integration** — GlyphPDF's PAdES long-term validation (§9.7, §27) is a generation ahead |
| Metadata hygiene | Remove PDF metadata tool; strip metadata in compress | W+D | Yes | W: Y | No one-click full sanitize bundle (GlyphPDF default-ON sanitize, D07 contract) |
| E-invoice security-adjacent | ZUGFeRD/XRechnur EN16931 validation (XRechnung-only field requirements enforced) | D+CLI | Yes | N | See §1.10/1.13 |
| Fax | PDF24 Fax send/receive (printer target, DocTool `-fax`) | D+W | **Paid per page** | Y | The only paid product; confirms the ad-funded-not-freemium model |

**GlyphPDF delta:** Cryptographic parity is closer than expected — PDF24 does have real certificate signing incl. smartcards, free. But no long-term validation story (no LTV/DSS/timestamp chain surfacing anywhere) where GlyphPDF ships B-LT/B-LTA with OCSP trust-chain validation and degradation honesty (22a7b66). No XMP expiry, no encrypted-ZIP secure package (GlyphPDF v1.3.0). No cloud-dependency traps on GlyphPDF side at all.

### 1.8 Compare

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Compare PDFs | Two files in, textual diff mode: **same / new / removed text** highlighting in browser; auto-converts any Office input first | **W only** | Yes | Y | TRUE (tool page: single "Mode: Textual" control). No desktop compare tool found in catalog/manual/changelog — MOSTLY_TRUE absent on desktop |
| Structural comparison (added/removed/reordered pages), visual pixel diff, reports, navigation sync | — | Neither | — | — | Absent everywhere. GlyphPDF compare (structural fingerprints w/ middle-insertion alignment, Myers-LCS text diff, HTML/text reports, filters, linked views — ledger CMP-align/V04/83e*) has no competition here at all in the free segment |

### 1.9 Batch / automation

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| File Tools app | Batch UI over common tasks: join, apply profiles, lock/unlock, convert, split, compress, send | D | Yes | N | Explorer context-menu extension (Shell DLL) routes files into tasks |
| Output profiles (the automation backbone) | Default profiles (low/medium/good/high/best), user profiles, machine-wide profiles (HKLM "all/"); profile params overridable per call (-profileParam); format options: PDF, PDF/A, PDF/X, images, security, watermark, page numbers, underlay/overlay, attachments, signature | D | Yes | N | This is PDF24's equivalent of batch presets — mature since the printer era |
| DocTool CLI | 18 commands: -convertTo/From, -applyProfile, -join, -splitByPage, -extractPages, auto-rotate, -compress, -crop, web-optimize, HTTP upload, email, fax, ShellExecute print, clipboard capture, TWAIN capture, screen capture, e-invoice | D | Yes | N | Scriptable backbone; shell extension built on it |
| Toolbox CLI | -processJob (JSON job files), -htmlToPdf (printBackground/orientation/paperSize/viewport/margins), -createInvoice | D | Yes | N | |
| OCR CLI | See §1.3 | D | Yes | N | |
| Hot folder / watched directory | — | Neither | — | — | No evidence in manual (18 DocTool commands enumerated), changelog, or registry sections. GlyphPDF hot-folder watching (§9.12 v1.3.0) wins |
| Named multi-step pipelines | Partial via profiles (single profile = many post-process steps) | D | Yes | N | One profile can chain compress+encrypt+letterhead+signature, but not multi-tool pipelines (OCR then redact) — GlyphPDF batch presets (v1.5 roadmap) should note the profile model as prior art |

**GlyphPDF delta:** PDF24's automation is broader in surface (CLI everywhere, context menu, profiles) but has no hot folder and no per-job multi-step pipelines. GlyphPDF's batch + hot folder beats it for enterprise drop-processing; PDF24 beats it on scriptable surface area and install-base interoperability.

### 1.10 Print production

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Virtual PDF printers | Multiple printers for different tasks, auto-save, profiles, printer assistant (save/email/fax/continue-with), "digital letter paper" (stationery) | D | Yes | N | The original product; universal convert-anything-printable |
| PDF/A export | Save-as/profiles: **PDF/A-1, PDF/A-2, PDF/A-3** (PDF/A-3 added early, "in the Save As dialog"); created **via Ghostscript** (ColorConversionStrategy auto-fix; umlaut metadata patch contributed upstream); OCR can output PDF/A directly; "PDF to PDF/A" dedicated tool | D+W | Yes | W: Y | Level letters (1B/2B/2U/3B/3U) not specified in docs — PARTLY_TRUE on variants. **No validation step** (no veraPDF equivalent). GlyphPDF ships 5 named levels + veraPDF CLI validation wiring (E-1-residual work) |
| PDF/X export | PDF/X listed among output formats; custom ICC handling (ICC bug fixes in changelog) | D | Yes | N | Depth (which X flavors, output intents UI) UNVERIFIABLE from docs |
| Web-optimize (linearize) | Dedicated tool + DocTool command + compress checkbox | W+D | Yes | W: Y | |
| Compression | **Most granular free compressor found:** DPI + image quality %, remove thumbnails, **deduplicate streams**, **rasterize heavy graphics**, **subset embedded fonts**, **modern image compression**, convert to grayscale, **maximum Flate compression**, flatten form, **strip output intents / attachments / annotations / outline / metadata / structural information**; original size displayed; filename suffix; settings remembered (registry); handles hundreds of files | W+D | Yes | W: Y | Option matrix mined verbatim from compress-pdf tool page. Pipeline = Ghostscript + own pdflib. No before/after size *estimate* (applies then reports) — GlyphPDF's §9.13-a measured readout + honest-estimate pattern is comparable; PDF24's strip-depth is deeper than GlyphPDF's current dialog |
| Preflight / ink coverage / imposition | — | Neither | — | — | Absent. No true print-production preflight in either product; PDF24 covers more of the output side (PDF/X, ICC) |

### 1.11 Accessibility / tagging

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Tagged-PDF authoring/repair, reading order, PDF/UA | — | Neither | — | — | No evidence anywhere (catalog "Optimize & repair" is file repair; compress "strip structural information" actually *removes* tags). TRUE-absent by enumeration. GlyphPDF's reading-order check (§9.14) and tag-preservation roadmap lead; low bar |
| UI accessibility | Registry mention of an "accessibility module" in the svg2pdf pipeline (a crash fix), standard Windows controls elsewhere; WebView2-based toolbox UI | D | — | — | No screen-reader labeling claims — UNVERIFIABLE/absent |

### 1.12 Import / export (conversion)

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| To PDF | ~27 input formats: Word (doc/docx), Excel, PowerPoint (+Publisher!), OpenDocument (ODT/ODS/ODP/ODG), RTF, Text, **EPUB, Markdown (11.30.0)**, images (JPG/PNG/WEBP/HEIC/SVG/TIFF), webpage (HTML), camera capture, screen capture, TWAIN scanner | W+D | Yes | W: Y; D: N for images/HTML/e-invoice; Office conversion on D can use **Office Automation APIs (11.8+)** when Office installed, else engine path | TRUE (all-tools + manual + changelog). Breadth exceeds GlyphPDF (which lacks EPUB/Markdown targets inbound? — GlyphPDF converts Office/images/scans in, exports Office out; Markdown/EPUB are v1.5 roadmap *outbound*) |
| From PDF | PDF to Word/PPT/Excel (office family), ODF, RTF, EPUB, HTML, **Markdown**, Text, SVG, JPG/PNG/images | W+D | Yes | W: Y; D: N | Toolbox "PDF Converter" offline engine; column-aware extraction not claimed |
| Image-to-image | HEIC↔JPG/PNG, WEBP↔JPG/PNG | W | Yes | Y | Genuinely free utility bread-and-butter |
| E-invoice suite (DACH moat) | **Create invoice (visual), Create electronic invoice (ZUGFeRD EN16931 / XRechnung, JSON+CLI), PDF invoice → e-invoice, XML e-invoice → PDF (visual layer), e-invoice import (CrossIndustryInvoice XML + hybrid ZUGFeRD; UBL not yet)**; validator with XRechnung-only field rules; BT-field coverage broad and growing weekly | D+CLI | Yes | N | TRUE (changelog 11.22–11.30 is dense with BT-xx additions). This is a *legal-compliance* feature (German B2B e-invoicing mandate) that no other free tool ships this completely — and GlyphPDF has nothing here |
| QR code, job-application PDF, "Create PDF with camera" | Quirky long-tail tools | W+D | Yes | W: Y | Catalog breadth as marketing |

**GlyphPDF delta:** PDF24 converts more formats in more directions for free, period. GlyphPDF's edge is honesty about conversions (capability registry disclosure, reject-before-truncate, true OOXML writer — F08/R10) vs PDF24's engine-soup quality (Office via COM vs engine vs PDFBox — fidelity varies by path). GlyphPDF should not chase the full format matrix; it should consider one DACH-relevant wedge (read/validate ZUGFeRD) only if a target persona needs it.

### 1.13 Cloud / web tools (the W surface)

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| 24 web tools | Merge, Split, Compress, Edit, Sign, Converter, Images↔PDF, Extract images/pages, Protect, Unlock, Rotate, Remove/Extract/Rearrange pages, Webpage-to-PDF, OCR, Watermark, Page numbers, Overlay, **Compare**, Web-optimize, Redact, Create | W | Yes | Y | German servers, files deleted after 1 hour, no registration, no limits, ad-funded. TRUE (every tool page carries the identical trust block) |
| Desktop↔web parity | Toolbox = offline twin of the catalog; some web tools (Compare) have no desktop counterpart; some desktop things (printer, profiles, e-invoice CLI) have no web counterpart | W+D | Yes | — | Parity is asymmetric in both directions |
| Chrome Extension | Browser integration for web tools | W | Yes | Y | |

### 1.14 Install / update mechanism

| Feature | Sub-capabilities | Surface | Free | Cloud? | Notes |
|---|---|---|---|---|---|
| Installers | EXE (Inno: `/VERYSILENT /SILENT /NOUPDATE /COMPONENTS=pdfPrinter …`) and MSI (`AUTOUPDATE=[Yes|No]`, `DESKTOPICONS`, `FAXPRINTER`, transforms) + x64/arm64/x86 + Microsoft Store | D | Yes | N | Enterprise-deployable; 9.x branch exists specifically to shed Toolbox/OpenJDK/WebView2 payload for smaller installs and Win7 |
| Update mechanism | `UpdateMode` HKLM DWORD: 0 = auto, 1 = notify, 2 = off; MSI **auto-update disabled by default** ("The feature is disabled by default"); EXE can /NOUPDATE; update mode preserved across upgrades (11.30-era fix); launcher update-check button hideable by policy; version-pinned downloads offered ("download a specific PDF24 Creator version") | D | Yes | Update checks: Y | Manual verbatim. Mechanism is transparent and policy-friendly — the criticism historically was *default EXE behavior* and update nagging, not telemetry |
| In-app promotion | Self-promotional banner (replaceable via `banner.png` / LogoClickUrl registry); online-feature references can be disabled via registry keys (help forum-documented, "under-documented" per staff) | D | — | Clicks: Y | No third-party ad network in the desktop app (HonestPDF + absence of any ad-sdk trace in docs) — MOSTLY_TRUE |
| Autostart components | pdf24.exe backend (Windows service "PDF24"), tray icon, Assistant popup after printing — each with documented disable paths (FAQ: "How to stop/remove the PDF24 autostart entry?"; forum: stop backend service) | D | — | — | Defaults favor visibility of the vendor; sysadmins disable them fleet-wide |

---

## 2. Research-focus deep dives

### 2.1 Compress quality
- Pipeline: Ghostscript (+ own pdflib) with the strip/degrade matrix in §1.10. No published benchmarks found; reputation is "good enough, very tunable" — PARTLY_TRUE (no benchmark located). Verdict: **UNVERIFIABLE** on absolute quality; **TRUE** on control granularity (richest free option set, including deduplicate-streams and subset-fonts that GlyphPDF lacks).
- Failure texture from changelog: compression historically broke on marked content (slow/low ratio — fixed by removing markings), failed when the F12 console was open, wrong Ghostscript argument — a pipeline built on fragile glue that they patch constantly.
- Signed-doc guard: PDF24 has none surfaced (GlyphPDF refuses signed docs by design — 45aa606). PDF24 would happily compress a signed PDF and invalidate signatures (inference from docs — UNVERIFIABLE, flagged as a test candidate).

### 2.2 Edit depth
- Overlay-only editing (§1.2). No existing-text rewrite, no object reflow, no paragraph handling. All "editing" = new SVG-derived overlay + optional page rasterization in redact. Save-path bugs (11.26.1) and crop-box misplacement history confirm how different their model is from true content-stream editing. Verdict: TRUE.
- Consequence: GlyphPDF's §9.2 (inline text edit, image edit, move/resize/rotate) has **no free competitor in PDF24**; closest free rivals are elsewhere (LibreOffice Draw, Sejda).

### 2.3 OCR quality
- Single-engine Tesseract 5.5.2 with good pre/post switches and psm control, but no review UI, no confidence output, no layout model, no ensemble. Verdict on "PDF24 OCR is weaker machinery than GlyphPDF's ROVER dual-engine + verify": **MOSTLY_TRUE** (architecture verified on both sides; no head-to-head benchmark exists).
- Their strengths to steal: `-skipFilesWithText` / `-skipPagesWithText` (idempotent batch OCR), OCR→PDF/A profile chaining, 219-language on-demand catalog with local-cache escape hatch.

### 2.4 Redaction
- Verdict "PDF24 redaction is secure but destructive": **TRUE** (their own changelog describes render-to-image replacement + resource/annotation scrubbing). Quality cost: page DPI/JPEG quality via registry only, whole page loses text layer. No patterns, no preview contract, no log, no marked-content workflow (draw boxes → apply). GlyphPDF's excision + transaction + sanitize + overlay-label + word-list patterns is a category above for compliance use.

---

## 3. Failure modes (with verdicts)

| # | Failure mode | Evidence | Verdict |
|---|---|---|---|
| F1 | **Offline product that phones home**: 11.30.1 (2026-04) fixed the *PDF Form Editor uploading the local file to the PDF24 online service* "even though this is unnecessary when the tool is run from the PDF24 Creator toolbox… this uploading behavior was incorrect". Also OCR language downloads on demand, update checks, online-feature links in UI, fax/cloud tools mixed into the same shell | Changelog 11.30.1 verbatim | TRUE — the single most useful trust-differentiator for GlyphPDF |
| F2 | **Install footprint & hostilities**: installer historically set PDF24 printer as **default printer** (11.30 added restore-of-previous-default — implicit admission), autostart backend service + tray + Assistant popups needing documented removal, WebView2/OpenJDK payload so large a whole LTS branch (9.x) exists to avoid it, Explorer-shell DLL blocks updates (manual admits Explorer restart needed), Chocolatey-reported MSI upgrade failures (10.8.0 era) | Changelog, manual, Chocolatey comments, help forums | MOSTLY_TRUE (each item individually evidenced; "hostile" is the aggregate read) |
| F3 | **Update friction**: nag/notify defaults on the consumer EXE path, update-mode resetting across upgrades (fixed only recently), update button that "could not be hidden" until a fix, and enterprises pinning versions manually | Changelog ("Update check button could not be hidden", "Update mode preserved during upgrades"), manual UpdateMode | MOSTLY_TRUE |
| F4 | **Ghostscript CVE exposure window**: bundled GS carried CVE-2023-36664 until they shipped 10.01.2 — a reminder that their engine soup inherits upstream CVEs and fixes land on their release cadence, not upstream's | Changelog verbatim | TRUE (as an event); recurrence risk MOSTLY_TRUE |
| F5 | **Save-corruption class bugs** in toolbox editors (11.26.1: Edit/Sign/Annotate save failures; 2139-era: certain files errored; overlay misplacement from crop-box handling) — no transactional safe-save semantics documented anywhere | Changelog | TRUE (events); systemic claim MOSTLY_TRUE |
| F6 | No sandbox/roadmap for compliance: no redaction logs, no PDF/A validation, no long-term signature validation — free tools stop where compliance starts (analysis) | Docs absence | MOSTLY_TRUE |

**No classic bundleware found:** no third-party sponsored offers in the installer in any changelog entry, review, or forum thread consulted. The "bundleware history" in the mission brief resolves to *self-promotion + system takeovers* (default printer, autostart, banner), not adware bundling. Verdict: MOSTLY_TRUE (absence across many sources).

---

## 4. Loved workflows (free-tier UX)

| # | Workflow | Evidence | Verdict |
|---|---|---|---|
| L1 | **Print-to-PDF with profiles/assistant** — "print anything from any app to PDF24", auto-save profiles, letterhead, then the Assistant offers save/email/continue-with-compress. The universal converter workflow is the product's origin story and still its stickiest | Creator page, manual §6, Dewiki | TRUE (breadth); "loved" MOSTLY_TRUE (review consensus) |
| L2 | **One free toolbox with no paywall anxiety** — merge/split/compress/OCR/sign/redact unlimited, no watermarks, no signup, offline; the repeated Reddit/TechRadar/134k-ratings theme ("never want to use any other tool again" is literally their rating prompt) | TechRadar, r/software, r/BuyFromEU, tool pages | TRUE |
| L3 | **Enterprise-free deployment** — "I use it on over 5000 clients" (r/de_EDV), MSI + Citrix + Win7 LTS + version-pinned downloads; IT deploys it *because* it's genuinely free commercially | Reddit quote, manual | MOSTLY_TRUE (single-platform quote, corroborated by Citrix FAQ + manual) |
| L4 | Reader-as-Acrobat-Reader-replacement + ZUGFeRD e-invoicing for German SMBs | Product pages, changelog density | PARTLY_TRUE |

---

## 5. GlyphPDF delta — bidirectional summary per domain

| Domain | PDF24 beats GlyphPDF | GlyphPDF beats PDF24 |
|---|---|---|
| Viewing | Deployable tabbed Reader (XFA, in-reader object move/delete), Reader→Toolbox handoff | Two-page/presentation/dark modes, search shortcuts, navigation sync |
| Editing | Repair/Rasterize/Flatten/Halve/N-up/breadcrumb breadth; page-size tool | **Inline existing-text editing**, object model, transactional safe-save, undo contracts |
| OCR | 219 languages on demand + local cache, psm control, skip-text intelligence, OCR→PDF/A | **ROVER ensemble quality**, verify screen + confidence, binarize/denoise/orientation, layout model |
| Forms | Momentum + XFA fill in reader | Everything else: 10 field types, calculated fields, auto-detect, CSV/FDF, undo-verified edits |
| Comments | — (parity: none/none at their layer) | Threads/filters/CSV summaries (U07) |
| Redaction | Resource+annotation scrubbing mindset (worth mirroring as disclosure) | **Surgical excision keeping text live**, patterns, logs, transactions, sanitize bundle |
| Security | Smartcard/cert signing free; QPDF AES-256; enterprise lockdown keys | **PAdES B-LT/B-LTA + LTV/OCSP + degradation honesty**, XMP expiry, encrypted-ZIP package, sanitize default |
| Compare | — | Entire domain (structural fingerprints, reports, filters, linked views) |
| Batch | CLI surface area (DocTool 18 cmds, JSON jobs), profiles, context menu | **Hot folder**, multi-step batch (OCR/redact/merge), capability pre-flight per item |
| Print prod | PDF printer + profiles + PDF/X + deeper compress strip matrix + web-optimize | Signed-doc guard, honest estimates, MRC gate, PDF/A level+validation (veraPDF) |
| A11y | — | Reading-order check; roadmap tag preservation |
| Import/export | ~27 in / 15 out formats, e-invoice ZUGFeRD suite, webpage/camera/screen | True-name OOXML writer, reject-before-truncate honesty, local-badge UX |
| Trust | — | Local-only enforced and *verifiable* (capability registry) vs F1 upload bug |

---

## 6. Sources (primary unless noted)

- https://www.pdf24.org/en/ (home; version 11.30.1; "100% Free of spyware") — fetched 2026-09-07
- https://tools.pdf24.org/en/creator (product page, FAQ verbatim on free/commercial/offline/Citrix; EXE+MSI+arm64+Store; 9.20.0 LTS)
- https://tools.pdf24.org/en/all-tools (full catalog enumeration)
- https://tools.pdf24.org/en/compress-pdf (compression option matrix, verbatim trust block)
- https://tools.pdf24.org/en/ocr-pdf (219-language list counted; OCR options; cloud-processing statements)
- https://tools.pdf24.org/en/redact-pdf, /en/sign-pdf, /en/edit-pdf, /en/compare-pdf (tool UIs/statements)
- https://creator.pdf24.org/manual/11/ (v11 manual: installer switches, UpdateMode, qpdf/AES-256, Ocr CLI + trainDataList, DocTool 18 commands, Toolbox CLI, registry sections, FAQ)
- https://creator.pdf24.org/changelog/en.html (full history through 11.30.1: blackenSecurity provider, form-editor upload fix, component versions, GS CVE-2023-36664, save-failure fixes, default-printer restore, e-invoice BT-xx)
- https://de.wikipedia.org/wiki/PDF24 (company, since 2006, C++, freeware license, 34 languages, Chip >10M downloads, Citrix since 2009)
- Secondary: https://www.techradar.com/best/free-pdf-editor ; https://www.reddit.com/r/de_EDV/comments/1icvx4m/ ("5000 clients") ; https://www.reddit.com/r/BuyFromEU/comments/1l4orw0/ ; https://community.chocolatey.org/packages/pdf24/10.8.0 (MSI upgrade failures) ; https://help.pdf24.org/en/forums/topic/avoid-autostart-of-pdf24/ and /pdf24-registry-keys/ (autostart, update keys) ; https://www.gethonestpdf.com/blog/is-pdf24-safe-2026 (desktop ad-free read) ; https://apps.microsoft.com/detail/xpfd51h3vqzfm0 (Store listing)
- Raw mining artifacts retained in `C:\Users\User\Projects\pdf\.context\research\_p24\` (tool pages, changelog.txt, manual11.txt)

**Confidence:** High (~0.85) on feature inventory, redaction mechanics, OCR/engines, install/update mechanics, pricing model — all primary-source verbatim. Medium on: exact form-editor depth (too new), PDF/A level variants (B/U unspecified), current certificate-signing UI surface (documented via changelog/registry, not screenshots), absolute compress/OCR quality (no benchmarks exist).

---

## 7. Compact summary

**Top 5 feature gaps (PDF24 gaps = GlyphPDF opportunities):**
1. **Desktop compare** — web-only, textual-only; GlyphPDF's structural+text compare with reports has zero free competition. (TRUE/MOSTLY_TRUE)
2. **Inline text editing** — overlay-only editor, "convert to Word" deflection; GlyphPDF edits native text. (TRUE)
3. **OCR quality machinery** — single Tesseract, no review UI/confidence; GlyphPDF dual-engine ROVER + verify screen. (MOSTLY_TRUE)
4. **Compliance redaction** — whole-page rasterization (page loses text layer), no patterns/logs/preview; GlyphPDF excision+transaction+sanitize. (TRUE)
5. **PAdES long-term validation** — free cert signing incl. smartcards exists, but no B-LT/B-LTA/LTV/timestamp story; hot folder also absent. (MOSTLY_TRUE)

**Top 3 failure modes:**
1. **Offline product uploading local files** (11.30.1 form-editor bug) — the trust wedge GlyphPDF's capability registry should exploit. (TRUE)
2. **Install hostilities** — default-printer takeover history, autostart backend service + Assistant popups, WebView2/OpenJDK bloat (whole 9.x LTS branch exists to shed it), Explorer-blocking shell DLL. (MOSTLY_TRUE)
3. **Glue-pipeline fragility** — constant save-failure/crop-box/Ghostscript-CVE patches on a multi-engine stack with no transactional save semantics. (TRUE as events)

**Top 3 loved workflows:**
1. Print-to-PDF universal converter with profiles + Assistant chaining. (TRUE)
2. Zero-paywall toolbox: every task free, offline, no watermark/signup — "100% free thanks to advertising". (TRUE)
3. Enterprise-free mass deployment (MSI/Citrix/Win7-LTS; "5000 clients" sysadmin quote). (MOSTLY_TRUE)

**Single biggest opportunity:** Position GlyphPDF as *the verifiable local workstation*: PDF24 owns "free and broad" and just demonstrated (form-editor upload bug) that its offline guarantee can silently fail — while its redaction rasterizes pages, its editor can't touch existing text, and nothing validates PDF/A or long-term signatures. GlyphPDF's excision redaction with live text + PAdES B-LT/B-LTA + desktop compare + hot folder + capability-registry honesty is exactly the compliance-grade layer PDF24 will never ship for free. Second-order: copy PDF24's cheap wins (Repair PDF, skip-pages-with-text OCR, compress strip matrix, Reader→Tool handoff, 3-up/halve) — and note their free ZUGFeRD/XRechnung e-invoice suite as the DACH-market wedge GlyphPDF currently concedes by default.
