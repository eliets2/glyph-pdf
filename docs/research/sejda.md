# Spec Sheet: Sejda PDF (Web + Desktop) — Deep-Dive for GlyphPDF

**Date:** 2026-09-08
**Requested by:** GlyphPDF parity program (research-specialist protocol)
**Research question:** What does the task-focused market leader in task-first UX actually ship (web + desktop), what are its feature gaps, failure modes, and loved workflows — so GlyphPDF can rank build priorities while staying offline-first/local-only?
**Method:** Primary-source extraction via curl + full-text JS mining of sejda.com (home, /pricing, /upgrade, /desktop, /desktop-release-notes, /privacy, /sitemap.xml = all 51 canonical EN tool pages, /pdf-editor, /sign-pdf, /pdf-forms, /workflows, /encrypt-pdf, /developers, /help, /ocr-pdf + its page JS, /merge-pdf, /pdf-to-word, /word-to-pdf, /n-up-pdf, /split-pdf-by-size). Client-side JS strings were mined for the complete free-cap dialog matrix, the full OCR language map, and the font list. Secondary: targeted web searches (Reddit r/pdf, r/software, Trustpilot, PDNob/MobiSystems reviews, GitHub API for torakiki/sejda). Absence claims verified by enumerating the live sitemap, not by search-engine recall.
**Benchmark baseline:** GlyphPDF ledger `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` (§27 status, §28 roadmap).

**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per protocol).

---

## 0. Product snapshot

| Dimension | State | Verdict |
|---|---|---|
| Company | Sejda BV, Amsterdam ("Made in Amsterdam. © Sejda BV 2013-present"); bootstrapped, no VC narrative; Crunchbase lists Amsterdam NL | TRUE (site footer, primary) |
| Products | (1) sejda.com web service — ~35 task tools, cloud processing; (2) Sejda PDF Desktop — Win 64-bit + portable, Mac Intel/Silicon, Linux .deb/.rpm, Chromebook guide; (3) open-source Java SDK `torakiki/sejda` (AGPL-3.0, 548 stars, pushed 2026-08-23) — the engine lineage behind the service; (4) G Suite / Google Drive marketplace app; (5) HTML-to-PDF API + embeddable edit/fill links | TRUE (primary + GitHub API) |
| Web tool inventory | 35 tools in 8 task groups: Merge (3), Split (6), Edit & Sign (4), Compress (1), Security (4), Convert from PDF (5), Convert to PDF (3), Other (15), Scans (2), Automate (1) — see sitemap enumeration in Sources | TRUE (live sitemap, 2026-09-08) |
| Desktop release cadence | Latest listed desktop release **7.7.0 (May 2024)**; notes only reach back through 7.6.12/7.6.8/7.6.7/7.6.6/7.6.0/7.5.6/7.5.4 — no release note in ~2+ years as of Sept 2026 | TRUE (release-notes page) — absence of notes ≠ proof of zero updates (auto-update exists), but cadence claim stands |
| Reputation | Trustpilot 4.3/5 (102 reviews); recommended on r/pdf as the easy free editor; no license-reversal or support-collapse scandal (contrast: Nitro) | MOSTLY_TRUE (single review platform, 102 reviews = small sample) |
| Open-source engine | Sejda SDK is AGPL-3.0 Java (PDFBox-based); commercial web/desktop products are closed | TRUE (GitHub license field) |

**The one-line model:** Sejda sells *tasks, not seats of an editor* — every tool is one URL, one screen, one verb; the free tier is metered per task (hourly on web, daily on desktop), and paying removes caps rather than unlocking features. Pricing page: "no watermarks… no signup" as free-tier pillars.

---

## 1. Spec tables by domain

Legend: **Surface** W = web (cloud processing), D = desktop (local processing). **Cloud?** = does the task require upload/network in its default path. Free-tier limits quoted are the exact per-tool strings surfaced by Sejda's own UI (mined from tool-page footers and cap dialogs).

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Standalone viewer/reading modes | — | Neither | — | — | **Absent.** No continuous/two-page/presentation modes, no dark reading mode, no bookmark pane. Sejda has no "open and read" surface at all; the editor is the viewer. Opposite pole from GlyphPDF §9.1. TRUE (no such tool in sitemap/help KB) |
| Open password-protected PDFs | Password prompt per file; "use this password for all files" batch option | W+D | Free: any count within task caps | W: Y / D: N | Shared dialog across all tools |
| Owner-password permission lift | Detects permission-restricted docs; lists the 6 permission bits (copy, edit contents, organize pages, fill forms, print, sign); asks for owner password | W+D | — | W: Y / D: N | Honest permission disclosure UI — same spirit as GlyphPDF CapabilityRegistry whyNot |
| Page organization (view-adjacent) | Organize: visual thumbnail grid, drag reorder, rotate, delete; Rotate tool persists rotation | W+D | Free web: 200 pp/50 MB; free D: 200 pp/50 MB | W: Y / D: N | "Visual Combine & Reorder renamed Organize" (changelog note on merge page) |
| Recents | Recent files list via connected Google Drive ("Google Drive > Sejda PDF") | W | 1 Drive file/hour free | Y | Cloud-account-tethered, not local MRU |
| Bookmarks/outline navigation panel | — | Neither | — | — | Create Bookmarks tool exists (generation), but no navigation pane. TRUE absent |
| UI localization | 29 UI languages (en de fr ro nl no pt it es pl sv fi ja id hu th vi tr ko cs zh tw ar da hi he el ru uk) | W+D | — | — | Desktop LOCALE installer property supports en/es/de/fr/it/pt (subset). TRUE (mined from JS bundle + help KB) |

### 1.2 Editing (text / object / page)

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Edit existing PDF text | Click existing text and retype; find-and-replace **all occurrences**; font/size/style changes | W+D | Web editor: 200 pp/50 MB, 3 tasks/hr; D: 3 tasks/day | W: Y / D: N | Web editor is explicitly labeled **BETA**. Claim "Edit existing PDF text. Annotate PDF" on tool card |
| Font handling | ~38 bundled fonts incl. Arabic (Amiri, Scheherazade New) and metric-compatible substitutes: Liberation Sans (Arial), Carlito (Calibri), Caladea (Cambria), DejaVu (Verdana/Georgia), Selawik (Segoe UI), PT Sans/Serif family, Inter, Roboto, Lato, Poppins, Noto family | W+D | — | — | Substitute-font strategy is a praised differentiator vs Acrobat garbling (see §5 Loved). Full list mined from editor page JS. TRUE |
| Add objects | Text boxes, images, shapes, whiteout (cover box), links (external URL / mailto / tel / internal page anchor), edit existing hyperlinks; "Repeat on all pages" for placed objects | W+D | Free web: 20 links/task, 5 MB/image | W: Y / D: N | Whiteout = visual white rectangle, NOT secure redaction |
| Scanned-document editing | — (explicit refusal modal) | W+D | — | — | Modal: "Changing existing text within scanned documents is not supported… you can still add new text, images, annotations. Converting scanned documents is not supported." Honest capability disclosure; no OCR-integrated editing. TRUE (mined modal string) |
| Object model depth | Move/resize placed items; no layer controls, no reflow engine, no paragraph reflow — box/line oriented | W+D | — | — | Shallower than Acrobat/Foxit reflow; fine for form-like and small fixes |
| Page manipulation | Insert/reorder/delete/rotate/duplicate pages; Organize visual grid; Delete Pages tool; Extract Pages; Split by pages / by bookmarks / in half (A3→2×A4 two-up scans) / by size / by text; Alternate & Mix (interleave odd/even from 2+ docs); Merge + reorder | W+D | Free web merge: **50 pp/50 MB**, merge ≤30 files; free D: combine ≤30 files and ≤50 pages; split ≤200 pp | W: Y / D: N | Split-by-text and split-in-half (two-up scan divider) are distinctive; split outputs support filename templating ([CURRENTPAGE], [BOOKMARK_NAME], [TEXT]) |
| Page furniture | Header & Footer (text labels), Page Numbers (position/format), Bates Numbering ("stamp multiple files at once"), Watermark (image or text) | W+D | Same doc caps | W: Y / D: N | Bates = batch-capable |
| Page geometry | Crop (trim margins, change page size), Resize (add margins/padding, change size), Flip mirror pages horizontally/vertically (marked **New**), Grayscale, N-up | W+D | 200 pp/50 MB | W: Y / D: N | Flip shipped recently on web |
| Metadata | Edit Metadata: Author, Title, Keywords, Subject, etc. | W+D | 200 pp/50 MB | W: Y / D: N | |
| Undo/redo | Per-session editor undo (web editor) | W | Session-bound | W: Y | Web session expiry deletes server-side working copies mid-edit ("Session expired… files were deleted from our servers. Please re-upload") — see Failure modes |

### 1.3 OCR

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| OCR engine (web) | **Tesseract.js 5.1.1 (WASM, runs in the browser)** for the interactive per-page "quick single page mode"; server-side pipeline for whole-document OCR | W | Free: **10 pages/task, ≤50 MB, 3 tasks/hr**; paid web: ≤**100 pages** (hard web cap even for PRO) | Quick mode: N (client-side); full-doc: Y | Architectural nugget: the quick mode never uploads page pixels for recognition — canvas → WASM Tesseract in-browser, word bboxes + baselines drawn back. TRUE (mined from ocr-pdf.min.js) |
| OCR engine (desktop) | Tesseract-based; language packs **downloaded on demand** (release notes 7.5.6: "downloads language data… over system proxy") | D | Free D: 10 pages/task; paid: no page limit | N (except pack download) | Desktop OCR has no page cap when paid |
| Languages | **60 languages** (mined complete map): Afrikaans, Azerbaijani, Belarusian, Bengali, Bulgarian, Catalan, Chinese (Simplified chi_sim), Traditional Chinese (chi_tra), Cherokee, Czech, Danish, German, Greek, English, English (Old), Esperanto, Estonian, Basque, Persian (Farsi), Finnish, French, Frankish, French (Old), Galician, Ancient Greek, Hebrew, Hindi, Croatian, Hungarian, Indonesian, Icelandic, Italian, Japanese, Kannada, Korean, Latvian, Lithuanian, Malayalam, Macedonian, Maltese, Malay, Dutch, Norwegian, Polish, Portuguese, Romanian, Russian, Slovak, Slovenian, Spanish, Old Spanish, Albanian, Serbian, Swahili, Swedish, Tamil, Telugu, Thai, Turkish, Ukrainian, Vietnamese | W+D | 1 language per pass (selector synced across pages) | — | Subset of Tesseract's full set (no Arabic, no Devanagari beyond Hindi, no_CJK extras beyond chi_sim/tra). GlyphPDF's Tesseract+RapidOCR stack covers overlapping set incl. CJK; Arabic absence is notable. TRUE (mined option map) |
| Output formats | Searchable PDF (invisible text overlay "at the correct locations") and/or plain .txt; both at once supported | W+D | — | — | |
| Review/correction | Per-page editable text result (contenteditable `pre` + copy/download buttons); word bbox overlay; "Review the results before using" warning banner | W (quick mode) | — | — | **No document-level review screen** like GlyphPDF's OCR Verify (§9.4); review is per-page on web only |
| Scan prep | Deskew tool ("automatically straighten scanned pages", automatic); **no** despeckle/binarize/dedicated orientation tool | W+D | 200 pp/50 MB | W: Y / D: N | GlyphPDF has binarize+denoise+orientation (ledger F05/R05, §9.4); Sejda does not surface them |
| Accuracy honesty | "Unfortunately we can't guarantee 100% accuracy… best-effort approach"; per-task warning: "Characters may be incorrect" | W | — | — | Honesty pattern worth mirroring |

### 1.4 Forms (fill / create)

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Fill AcroForms | Field-aware filling; "Fill out PDF forms easily, **even if no form inputs are present**" (free-text placement over flat forms) | W+D | 200 pp/50 MB | W: Y / D: N | |
| Create form fields | Text fields (unique-name enforcement with error message, max length, **"divide into boxes"** = comb field, mandatory flag), radio groups (group name, options one-per-line, allow multiple selections → checkbox behavior), "Repeat on all pages"; created inside the same visual editor (Create Forms = pdf-forms tool) | W+D | 200 pp/50 MB | W: Y / D: N | Field palette mined from page JS. No list box, no dropdown (only radio/"multiple selections"), no date picker, no calculated fields, no validation rules, no tab-order editor — vs GlyphPDF's 10 field types incl. calculated (PRD §9.6). PARTLY_TRUE on exact widget set (palette strings show text/radio/checkbox; absence of others inferred from palette, not docs) |
| Form data import/export (FDF/XFDF/CSV/XML) | — | Neither | — | — | Absent. GlyphPDF has CSV/FDF (PRD §27). TRUE absent (no tool, no help KB entry) |
| Flatten forms | Flatten tool: "Makes fillable PDFs read-only. Print & scan in one step" | W+D | 200 pp/50 MB | W: Y / D: N | |
| Auto field detection | — | Neither | — | — | No auto-detect field placement (GlyphPDF has content-aware auto-detect, ledger 88d4286). TRUE absent |

### 1.5 Comments & markup

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Annotate while editing | Highlight, strikethrough, shapes, notes added in the editor ("Annotate PDF") | W+D | 200 pp/50 MB | W: Y / D: N | No underline/squiggly-specific tooling advertised; markup lives inside the editor, not a comment mode |
| Comment management | — | Neither | — | — | No comment list/threads/statuses/filters/summaries (GlyphPDF U07 ships these). TRUE absent |
| Remove annotations | Batch remove highlights, strikeouts "or any other annotations" (marked **New**) | W+D | 200 pp/50 MB | W: Y / D: N | Batch-capable annotation stripping — useful sanitization-adjacent feature |
| Stamps / audio / measurement | — | Neither | — | — | Absent |

### 1.6 Redaction

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Content redaction (excision) | **ABSENT — no redact tool exists** | — | — | — | Verified by full sitemap enumeration (no /redact-* URL among 51 canonical pages) + help KB. Closest proxies: editor **Whiteout** (white rectangle drawn over content — content remains extractable underneath) and Remove annotations. Neither removes text streams. TRUE (primary enumeration) |
| Implication | Sejda cannot serve redaction compliance at all; its whiteout is the classic "draw a box, get sued" trap | | | | GlyphPDF's excision + transaction + sanitize bundle (U05, E-1 fix) is a categorical capability Sejda lacks entirely |

### 1.7 Security (password / permissions / sign)

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Password protect | Open password + permission restrictions (modify / print / copy checkboxes); recommends 16+ char passwords | W+D | 200 pp/50 MB | W: Y / D: N | Encryption algorithm not advertised (UNVERIFIABLE from site) |
| Unlock / remove security | Remove restrictions + password (owner-password path) | W+D | 200 pp/50 MB | W: Y / D: N | |
| Permission model disclosure | 6 permission bits surfaced: content copying, editing contents, organizing pages, fill forms, printing, signing | W+D | — | — | |
| Fill & Sign (self-signing) | Draw / type / upload-image signature; place anywhere; works without form fields | W+D | 200 pp/50 MB | W: Y / D: N | BETA label. **No cryptographic signing** — image/draw/type only |
| Certificate-based / PAdES signing | **ABSENT** | — | — | — | No digital-ID, no certificate signature anywhere in tool set or KB. GlyphPDF's PAdES B-LT/B-LTA is categorical parity-over-Sejda. TRUE (sitemap + KB absence) |
| Send-for-signing (3-party workflow) | **Not a signing workflow.** Closest: "Direct links" — generate a URL that opens the doc in Sejda's editor for anyone to **fill & sign** and email it back (`returnEmail` param); shareable on website/email | W | — | Y | No recipients list, no signing order, no reminders, no status tracking, no audit trail, no identity verification. Confirmed by search: Sejda's own sign-pdf copy — "a link that opens the document with Sejda, allowing others to fill & sign the form and send it back to you by email." TRUE |
| Watermark | Image or text watermark | W+D | 200 pp/50 MB | W: Y / D: N | Security-grouped in their nav |
| Metadata sanitize / document hygiene | No dedicated sanitizer; Remove annotations (New) strips markup; metadata editing exists (can blank fields manually) | W+D | — | — | No one-click sanitize bundle (GlyphPDF §9.8 sanitize default-ON). TRUE absent |

### 1.8 Compare

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Document comparison | **ABSENT — no compare tool exists** | — | — | — | Verified by full sitemap enumeration (no compare/diff URL) + no help KB entry + absent from all reviews consulted. TRUE (primary enumeration) |
| Implication | Legal/compliance users must leave Sejda for document comparison. GlyphPDF's compare (structural fingerprints, middle-insertion alignment, filters, reports — ledger CMP-align, V04) is categorical over Sejda | | | | |

### 1.9 Batch / automation

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Multi-file processing | "Processing multiple files at once" — applies to most task tools | W+D | Free: **single file per task** (web) / "convert files one by one" (desktop); paid: multiple | W: Y / D: N | The single biggest free-vs-paid behavioral split |
| Workflows (tool chains) | NEW (BETA): "Execute series of tasks on PDF documents. Configure tool chains to process PDFs" — create, **export/import** workflow definitions, launch | W | Free: **1 workflow** | Y | Named multi-step pipelines exist on web only, not desktop. Export/import implies shareable JSON configs. No hot folder, no folder watching, no schedule |
| Output naming rules | Filename templating keywords: [CURRENTPAGE###], [TIMESTAMP], [FILENUMBER13], [BASENAME], [BOOKMARK_NAME/_STRICT], [TEXT], [TEXT1], [TEXT2] | W+D | — | — | Excellent, documentation-grade feature (help KB). Pairs with Split-by-text ("1-Invoice 3456789.pdf") and Rename-from-page-region tools |
| Bates in batch | "Bates stamp multiple files at once" | W+D | Single file free | W: Y / D: N | |
| Hot folder / watched directory / CLI | — (commercial product) | Neither | — | — | Absent from product. Note: the AGPL `sejda-console` (open-source sibling) provides CLI task processing for technical users — separate project, not the product |
| Rename tool | Change filename based on text found in page regions ([TEXT1], [TEXT2] from selected areas) | W+D | Free: 5 files/task | W: Y / D: N | Distinctive invoice/mailroom-oriented tool |

### 1.10 Print production

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| N-up imposition | "N-up & PDF Imposition": A5 plan as 4-up on A3, A4 2-up on A3 — grid sheet composition | W+D | 200 pp/50 MB | W: Y / D: N | True imposition-lite; **no booklet/signature imposition** (saddle-stitch ordering), no crop/registration marks, no color separations |
| Split in half (two-up scans) | A3 two-page-layout scan → double A4 | W+D | 200 pp/50 MB | W: Y / D: N | Inverse imposition — loved for digitized books (see §5) |
| Grayscale | Convert text+images to grayscale | W+D | 200 pp/50 MB | W: Y / D: N | Print-cost tool |
| Flatten | "Print & scan in one step" flattening | W+D | 200 pp/50 MB | W: Y / D: N | |
| Prepress depth (marks, traps, output intents, PDF/X) | — | Neither | — | — | All absent. TRUE |

### 1.11 Accessibility / tagging

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Tagged-PDF authoring/repair, reading order, PDF/UA | **ABSENT — nothing** | — | — | — | No accessibility tooling anywhere in product or KB. TRUE (enumeration + KB). GlyphPDF's reading-order check (§9.14) has no Sejda counterpart |
| UI accessibility gestures | Web fonts include **Atkinson Hyperlegible** (low-vision-optimized typeface) in their font assets | W | — | — | Small signal of UI-level legibility care, not document accessibility. PARTLY_TRUE (asset present; intent inferred) |

### 1.12 Import / export formats

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| PDF → Word | PDF to DOC (Word); scans explicitly unsupported (honest modal) | W+D | Free: **20 pp/conversion** (web dialogs) or 50 pp/50 MB (tool footer); paid 2000+ pp | W: Y / D: N | Web cap strings show both "20 pages per conversion" and "50 pages per conversion" — per-tool variation. DOC not DOCX advertised |
| PDF → Excel / CSV | Table extraction to Excel or CSV ("Extract table data from PDF") | W+D | Free: 20-50 pp | W: Y / D: N | |
| PDF → PowerPoint | PDF to PPT | W | Free: 20-50 pp | Y | Web tool (desktop set markets Word/Excel/images) |
| PDF → images | JPG, PNG or TIFF per page; 150/220 DPI options; delivered as ZIP | W+D | Free images ≤5 MB | W: Y / D: N | |
| PDF → text | Plain text extraction (also the OCR text output) | W+D | 200 pp/50 MB | W: Y / D: N | |
| Images → PDF | JPG to PDF (multiple images, reorder) | W+D | Images ≤5 MB | W: Y / D: N | |
| Word → PDF | .docx → PDF | W+D | 200 pp/50 MB | W: Y / D: N | .docx only — no PPTX→PDF, no ODT, no Markdown/EPUB (GlyphPDF gap list §9.5 also lacks these) |
| HTML/web → PDF | HTML to PDF tool (URL or HTML) + public **HTML-to-PDF API** | W | Save-as-PDF links: 10 uses/hour/visitor, free | Y | Also API product |
| Extract images / text | Extract embedded images; extract text | W+D | 200 pp/50 MB | W: Y / D: N | |
| Repair | "Recover data from a corrupted or damaged PDF document" | W+D | 200 pp/50 MB | W: Y / D: N | Rare feature in this price class |
| PDF/A export, HTML export from PDF, EPUB/Markdown | — | Neither | — | — | **No PDF/A anywhere** (archival users unserved). TRUE absent. GlyphPDF ships PDF/A 1B-3U |

### 1.13 Cloud / web–desktop split (their architecture, marked separately)

| Feature | Sub-capabilities | Surface | Limits (free tier) | Cloud? | Notes |
|---|---|---|---|---|---|
| Web service processing | Files upload over SSL, processed server-side, download result; "Files stay private. Automatically deleted after 2 hours"; session expiry deletes working copies | W | Hourly caps (below) | Y | Core business. Privacy policy effective Jan 6, 2026 |
| Desktop = local twin | "Sejda Desktop offers the same features as the PDF cloud service, only files are processed on your computer… peace of mind privacy, perfectly suitable for business" | D | 3 tasks/**day**, 50 MB/200 pp, compress 100 MB, OCR 10 pp, images 5 MB, combine 30 files/50 pp, convert one-by-one | N | Marketing explicitly weaponizes localness as the escape hatch from its own cloud — proof of demand for GlyphPDF's default posture |
| Cloud connectors | Upload from Dropbox, Google Drive, OneDrive, URL; Google Drive recents; G Suite marketplace app | W | 1 Drive file/hr free | Y | |
| Embeddable task links | `pdf-editor?files=[{downloadUrl…}]&returnEmail=…` — websites can deep-link users into edit/fill&sign tasks; Save-as-PDF links | W | Free | Y | Task-embedded-at-point-of-need pattern |
| APIs | HTML-to-PDF API (documented options/response codes); open-source AGPL SDK for self-building | W | — | Y | No general REST PDF API advertised for the product tools (UNVERIFIABLE whether one exists privately) |
| Desktop telemetry/licensing footprint | Licensing contacts server (transmits hostname, username, IP); **error reporting can include hostname, username, OS, file names used, edits performed, log output** — opt-out available; update check (disable via UPDATE_CHECK=false) | D | — | Partial | From Sejda's own privacy policy. A privacy wrinkle for a "local" app — GlyphPDF's no-phone-home stance is a clean differentiator |
| Enterprise deployment | msiexec volume deploy: `LICENSE_KEY=…`, `LOCALE=…`, `UPDATE_CHECK=false`, `DISABLED_FEATURES="edit.whiteout"` (feature flags), `EULA_ACCEPTED=…`; Citrix and Terminal Services compatible; pre-activated volume keys | D | — | Partial (key activation) | Documented, cmd-line-first enterprise story — worth copying the installer-property pattern |

---

## 2. Licensing granularity

| Tier | Price (seen 2026-09-08) | What it unlocks | Verdict |
|---|---|---|---|
| Free web | $0, **no signup** | All 35 tools, metered: **3 tasks/hour** (tool-page footers + cap dialog) — though the /pricing page says "5 tasks per hour" (site inconsistent between 3 and 5/hr; MOSTLY_TRUE, discrepancy primary-sourced); ≤50 MB & per-tool page caps (OCR 10 pp; merge 50 pp; conversions 20-50 pp; split 200 pp); ≤30 files/hr uploads (pricing page says 50); 1 image ≤5 MB; **1 file per task**; 1 workflow | TRUE caps; the 3-vs-5/hr and 30-vs-50 uploads inconsistencies are REAL (both strings live today) |
| Free desktop | $0 | Same toolset locally: **3 tasks/day**, docs ≤50 MB/200 pp, compress ≤100 MB, OCR ≤10 pp, images ≤5 MB, combine ≤30 files/50 pp, conversion one-file-at-a-time | TRUE (desktop page table, exact match to coordinator's brief: "3 tasks/day, 200 pages/50 MB") |
| Web Week Pass | **$5 one-time** (7 days) — the /pricing page showed $3.99 | Unlimited tasks, no page/hour limits, OCR ≤100 pp, multi-file, ≤21 min/task, uploads ≤**500 MB**, email support. Desktop NOT included | TRUE (upgrade page); two live price points seen — regional/AB variance, flag as MOSTLY_TRUE on exact figure |
| Web Monthly | **$7.50/user/mo** (recurring) — /pricing page showed $5.99 | Same as week pass, web only | MOSTLY_TRUE (two live figures) |
| Desktop Week Pass | **$7.95 one-time, 7 days**, explicitly non-recurring, downgrades to free after 7 days | All desktop limits removed for a week; web not included | TRUE (upgrade + desktop FAQ support answer) |
| Desktop+Web Annual | **$63/user/yr** (recurring; "save 30% over monthly") | Both surfaces unlimited | TRUE |
| Volume discounts | 10% (2-4), 20% (5-24; also "20% for 10 users" on upgrade), 40% (25-49), 60% (50+) | Team pricing UI with live seat-count slider | TRUE |
| Perpetual licenses | **Discontinued**: FAQ — "No, we don't offer perpetual licenses anymore"; old perpetual holders blocked from updates ("Sorry, this update is not available with your perpetual license") | Subscription or term-pass only | TRUE (primary FAQ) — same direction of travel as Nitro but without the deactivation scandal |
| Enterprise/edu | Volume key MSI deploys, Citrix/TS support, dedicated Education ("Sejda for Education", /teachers) and G Suite offerings | Deployment & admin conveniences, not extra features | TRUE |

**What paying actually buys:** removal of quantity caps (tasks/hour-day, file size, page count, files-per-task) and one *feature* gate (multi-file processing; extra workflows). No feature-tier matrix beyond that — every tool is usable in free, which is the core of the brand's trust.

---

## 3. Task-first UX patterns (the reference GlyphPDF should mine)

1. **Home = verb grid, not document shell.** "Sejda helps with your PDF tasks" — 35 one-screen tools in 8 task groups (Merge / Split / Edit & Sign / Compress / Security / Convert / Other / Scans / Automate). No file-open-first flow anywhere. One URL per task = shareable, bookmarkable, SEO-dominant.
2. **Every tool page states its own free cap in the footer** ("Free service for documents up to 10 pages or 50 MB and 3 tasks per hour" on OCR; "50 pages…" on merge). Cap disclosure is per-task, pre-upload, honest. This is the retail version of GlyphPDF's CapabilityRegistry pre-execution disclosure (U08).
3. **Honest capability refusals as modals, not failures.** The scanned-editing modal explains what IS possible in the same breath ("you can still add new text, images…"). OCR warns "Review the results before using."
4. **Zero-signup free tier** — email enters only at download/share time. Friction budget spent on caps, not accounts.
5. **Desktop promo embedded in every web tool** ("Rather work offline? Try Sejda Desktop") — privacy- sensitive routing between surfaces.
6. **Task embedding:** direct links (`?files=[…]&returnEmail=`), Save-as-PDF buttons, G Drive integration — tasks travel to where the work is.
7. **BETA labels kept for years** on the editor — humility signaling (compare: marketing-overclaiming competitors).
8. **Tool-chains (Workflows) with export/import** + filename templating keywords — automation treated as a first-class task, not a settings pane.

---

## 4. GlyphPDF delta (bidirectional, per domain)

### 4.1 Sejda ships it; GlyphPDF lacks it (build candidates)

| # | Sejda capability | GlyphPDF state (PRD/ledger) | Recommendation | Verdict Sejda ships |
|---|---|---|---|---|
| G1 | **Web Workflows: named tool-chain pipelines with export/import** (§1.9) | Batch has hot-folder + per-op batch, but named reusable multi-step presets are PRD §28 v1.5 item ("Batch preset workflows") | Build — adopt the export/import (shareable JSON) idea; ours stays local/hot-folder-driven | TRUE |
| G2 | **Output filename templating keywords** ([CURRENTPAGE###], [FILENUMBER13], [TEXT], [BOOKMARK_NAME]) across split/rename/batch | Partial: split has `_part{n}` (§9.9-a); no keyword grammar | Cheap, high-leverage: one keyword engine reused by split/rename/batch | TRUE |
| G3 | **Split-by-text and split-by-bookmarks** (document segmentation by content) | Split by ranges/size; no text/bookmark segmentation | Mid-term; our OCR text layer + outlines already provide the inputs | TRUE |
| G4 | **Rename-from-page-region** (filename from invoice number etc.) | Absent | Niche but loved in mailroom/invoice flows; pairs with OCR | TRUE |
| G5 | **Direct fill&sign links with returnEmail** (3-party form completion loop) | §9.7 send-for-signing not started | Do NOT clone the cloud link; build the local equivalent (signing package + email) per Nitro finding A2 | TRUE |
| G6 | **Repair corrupted PDF tool** | Safe-save transactions protect writes; no recovery/open-repair path | Investigate PDFium/PoDoFo recovery modes for an "Open & Repair" entry | TRUE |
| G7 | **Flip (mirror) pages H/V** | Absent (rotate exists) | Trivial add to page ops | TRUE |
| G8 | **N-up imposition** for printing | Absent | Small print-utility win; booklet ordering could leapfrog | TRUE |
| G9 | **WebWeek/desktop week-pass pricing** ($5-7.95 one-time, 7 days) | No pricing model shipped | Pricing-design insight only: a cheap time-boxed pass fits one-off local users without subscriptions | TRUE |
| G10 | **In-browser WASM OCR quick mode** (client-side Tesseract.js, word bbox overlay) | N/A as architecture (we are the desktop), but the *quick single-page OCR with instant editable text* interaction is adoptable in the OCR verify screen | Pattern adoption only | TRUE |

### 4.2 GlyphPDF ships it; Sejda lacks it (our differentiation, verify marketing claims against this)

| # | GlyphPDF capability | Sejda state | Verdict |
|---|---|---|---|
| D1 | **Redaction: content-stream excision + transaction + sanitize bundle + pattern presets + word-list import** (U05, E-1, §9.8) | No redaction at all; whiteout is cosmetic | TRUE |
| D2 | **Compare: structural + text diff, fingerprint alignment, filters, reports** (§9.10, CMP-align, V04) | No compare tool | TRUE |
| D3 | **PAdES B-LT/B-LTA certificate signing + trust-chain validation** (§9.7) | Image/draw/type signatures only | TRUE |
| D4 | **PDF/A export 1B/2B/2U/3B/3U** | No PDF/A support anywhere | TRUE |
| D5 | **Accessibility: tagged reading-order check** (§9.14) | Nothing | TRUE |
| D6 | **Offline everything: no caps, no upload, no telemetry** (§14, §28 out-of-scope cloud) | Desktop is local but metered 3 tasks/day + phones home for licensing/error reports | TRUE |
| D7 | **Dual-engine OCR (Tesseract 5 + RapidOCR + PP-DocLayout, ROVER fusion) with deskew/binarize/denoise/orientation + review screen** (§9.4, F05-F11) | Single Tesseract engine, 60 langs, no image-preprocessing suite, no doc-level review | TRUE |
| D8 | **Forms: 10 field types incl. calculated, CSV/FDF data, auto-detect placement** (§9.6) | Text/radio/checkbox palette, no calc fields, no data import/export, no auto-detect | MOSTLY_TRUE (their full palette not fully enumerable from static JS) |
| D9 | **True viewer: single/continuous/two-page/presentation, dark mode, bookmarks** (§9.1) | No reader surface at all | TRUE |
| D10 | **Comment review UI (threads/filters/status/CSV)** (U07) | Remove-annotations only | TRUE |

### 4.3 Where Sejda is simply *better* (adopt, don't beat)

- **Task-first IA.** Their verb grid + one-URL-per-task is the cleanest in the market; GlyphPDF's own U02 TaskNav ("one clear entry per task") should go further: name tools as tasks, state the capability contract on entry (Sejda's footer caps = our registry disclosure).
- **Per-task honest limits before upload** — pre-execution disclosure everywhere, not just where we probed.
- **Filename templating grammar** (G2) and **workflow export/import** (G1).
- **Non-recurring week pass** as a pricing instrument for a local tool.

---

## 5. Failure modes (recurring, graded)

| # | Failure mode | Evidence | Verdict |
|---|---|---|---|
| F1 | **Cap-wall interrupts work mid-task.** Free users hit "You reached your free limit of 3 tasks per hour… break for 00:59:00" during editing; desktop analog is 3 tasks/DAY. The single most-cited complaint; competitors build marketing pages around it ("Tired of Sejda's 3-tasks-per-hour limit?"). Counters are per-hour (web) so power bursts die fast | r/pdf thread (quote mined), OneClickPDF comparison page, cap dialogs | TRUE |
| F2 | **Cloud exposure + ephemeral sessions.** Sensitive docs upload by default; sessions expire deleting server-side working copies mid-edit ("Session expired… please re-upload"); 100-page OCR cap even for paying web users; 21-min task ceiling; upload-failure toast exists in the happy path UI. Desktop exists as the escape hatch but is itself capped free | Tool-page strings; privacy policy (files auto-deleted after 2 h); OCR modal | TRUE |
| F3 | **Desktop stagnation & drift.** Latest published desktop release 7.7.0, May 2024 (~2+ years without a release note); editor labeled BETA for years; scanned-doc editing refused outright. Users needing depth (redaction, compare, signing workflow, PDF/A) hit a wall and must switch tools | Release-notes page; BETA labels; sitemap absences | TRUE (cadence); PARTLY_TRUE if unnoted silent auto-updates ship, but published cadence is the user-visible fact |
| F4 | **"Local" desktop is not fully private.** Licensing contacts home (hostname, username, IP) and opt-out-able error reporting may include file names + edits performed — a real cost for the privacy-motivated desktop buyer (their own marketing: "peace of mind privacy") | Sejda privacy policy, primary | TRUE |

## 6. Loved workflows (what users praise)

| # | Workflow | Evidence | Verdict |
|---|---|---|---|
| L1 | **Zero-friction one-off tasks** — merge/split/compress/fill&sign without signup, in seconds, from any browser; free tier is genuinely usable ("pretty liberal per-use limits", "the quality has been good") | r/pdf; Trustpilot 4.3; multiple review sites | MOSTLY_TRUE (positive consensus, small review base) |
| L2 | **Edit-text font handling that doesn't garble** — metric-compatible substitute fonts keep layout stable; users contrast with Acrobat ("keeps things super smooth") where font mismatch breaks the page | Reddit via search summary (low-tier corroboration, single thread) + the shipped 38-font substitute list (primary) | PARTLY_TRUE (mechanism TRUE from primary; praise single-source) |
| L3 | **Desktop week-pass + local batch** — pay $7.95 once for a week of unlimited local processing (multi-file convert/OCR) for a big job, then lapse; recommended for confidential docs ("very good - with daily limits for free") | r/software quote; desktop FAQ (non-renewal is officially clarified); MobiSystems review praising offline+batch desktop | MOSTLY_TRUE |
| L4 | **Two-up scan handling** — split-in-half (A3→2×A4) + deskew + OCR + rename-from-text is a quiet favorite pipeline for digitized books/courier docs | Feature prominence in nav/marketing; review mentions. Inference from tooling, not a quote | PARTLY_TRUE |

---

## 7. Executive synthesis

**Finding → Implication → Recommendation**

1. **Sejda's task grid is its moat; its caps are its ceiling.** The verb-first UX (35 tools, one screen each, honest per-tool limits, zero signup) is the best-in-class task-first pattern — but every cap (3/hr web, 3/day desktop, 50 MB, 100-page OCR, 1 file free) exists because cloud processing has marginal cost. A local-first product has none. → **Recommendation:** adopt the task-grid IA wholesale in U02/TaskNav and pair it with "no caps, no upload" messaging; disclose per-task limits (registry) exactly where Sejda prints its caps. This inverts their business model into our positioning.
2. **Sejda is categorical-ly absent in exactly GlyphPDF's five flagship pillars** — redaction, compare, PAdES, PDF/A, accessibility — and refuses scanned-text editing outright. → **Recommendation:** market against the task-tool category: "task tools stop where the workstation starts." Keep our depth features listed in one capability matrix page (their "Suggest a missing feature" wall is where we win).
3. **Their automation layer (Workflows + filename keywords + multi-file) is the most copyable high-value surface.** → **Recommendation:** build named preset pipelines with export/import and a filename keyword grammar in v1.5 as planned (PRD §28), and stretch to split-by-text/bookmark segmentation.
4. **Desktop trust is fragile elsewhere; ours is structural.** Sejda's desktop phones home for licensing/error reports and its release cadence has stalled since May 2024; Nitro (prior report) deactivates licenses. → **Recommendation:** never phone home; sign a licensing promise ("cannot be disabled remotely, no telemetry") into the product and marketing.

**Single biggest opportunity:** a **task-first home grid with per-task capability/limit disclosure and no metered caps** — Sejda proved users prefer tasks over monolithic editors, and its cloud economics force the caps users resent most. GlyphPDF can ship the identical interaction model with the one advantage Sejda structurally cannot match locally.

**Confidence:** High on all spec rows and caps (mined from live primary pages/JS today, 2026-09-08). Medium on community sentiment (Trustpilot sample small; Reddit threads partially seen via search summaries). Flagged inline: 3-vs-5 tasks/hour and 30-vs-50 uploads/hour inconsistencies are real live-site contradictions; price points $5.99/$7.50 and $3.99/$5 both seen on live pages (variance noted, exact current figure MOSTLY_TRUE).

## Sources

Primary (fetched/mined 2026-09-08):
- https://www.sejda.com/ (task grid) · /pricing · /upgrade (2026 plan matrix) · /desktop (free-tier table) · /desktop-release-notes (7.7.0 May 2024 latest) · /privacy (policy eff. Jan 6 2026) · /sitemap.xml (all 51 canonical EN URLs = absence verification) · /pdf-editor (fonts, links, fields, BETA) · /sign-pdf (fill&sign + direct links) · /pdf-forms (field palette) · /workflows (tool chains, 1 free) · /encrypt-pdf · /developers (embed links, APIs) · /help (filename keywords, enterprise MSI properties) · /ocr-pdf + /js/pages/ocr-pdf.min.js?v=22 (60-language map, Tesseract.js 5.1.1, caps dialogs, accuracy warnings) · /merge-pdf · /pdf-to-word · /word-to-pdf · /n-up-pdf · /split-pdf-by-size
- GitHub API: https://github.com/torakiki/sejda (AGPL-3.0, 548 stars, pushed 2026-08-23)

Secondary:
- Reddit r/pdf Sejda thread (cap quote): https://www.reddit.com/r/pdf/comments/1ggnaab/sejda/
- r/software recommendation quote: https://www.reddit.com/r/software/comments/1r2pjsm/why_is_it_so_difficult_to_just_edit_and_sign_a/
- Trustpilot: https://www.trustpilot.com/review/www.sejda.com (4.3/5, 102 reviews)
- MobiSystems review: https://mobisystems.com/en-eu/blog/industry-insights/should-you-use-sejda-pdf-pros-and-cons-of-using-the-web-based-pdf-editor
- Competitor anti-Sejda positioning: https://www.oneclickpdf.net/compare/sejda-alternative
- Crunchbase: https://www.crunchbase.com/organization/sejda-pdf
