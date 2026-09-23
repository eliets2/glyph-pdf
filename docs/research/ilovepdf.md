# Spec Sheet: iLovePDF (Web + Desktop) — Deep-Dive for GlyphPDF

**Date:** 2026-09-08
**Requested by:** GlyphPDF parity program (research-specialist protocol)
**Research question:** What does the freemium task-grid consumer leader actually ship (web + desktop + mobile), what are its feature gaps, failure modes, and loved workflows — so GlyphPDF can rank build priorities while staying offline-first/local-only?
**Method:** Primary-source extraction via curl of ilovepdf.com live pages — sitemap.xml (1,415 URLs; 25 canonical EN tool pages enumerated), /pricing (full per-tool free/premium cap matrix scraped verbatim), /desktop, /edit-pdf, /ocr-pdf (full language selector mined), /redact-pdf, /compare-pdf, /sign-pdf, /pdf-forms, /split_pdf, /convert-pdf-to-pdfa, /help/faq, /help/security, /help/privacy, /blog/pdf-web-or-desktop (official platform-comparison post). Absence/presence claims verified against the live sitemap + leaked site navigation, not search-engine recall. Secondary: WebSearch corroboration (Trustpilot, G2, Microsoft Store listing, reviews). Raw evidence snapshots retained as `_ilp_*.html` in this directory.
**Benchmark baseline:** `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` (§27 status, §28 roadmap).

**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per protocol).

---

## 0. Product snapshot

| Dimension | State | Verdict |
|---|---|---|
| Company | iLovePDF, S.L. — Calle Sabino de Arana 60, 08028 Barcelona, Spain (CIF B66921552). Data controller for iLovePDF, iLoveIMG, iLoveSign, iLoveAPI | TRUE (privacy policy, primary) |
| Products | (1) iLovePDF Web (25 tools + AI suite); (2) iLovePDF Desktop for Windows (Microsoft Store) + macOS (Mac App Store), MSI/DMG via Sales; (3) iLovePDF Mobile (iOS + Android); (4) iLoveSign (separate e-signing product); (5) iLoveIMG; (6) iLoveAPI (developer automation); (7) integrations: Zapier, Make, WordPress, Google Drive/Dropbox/OneDrive/SharePoint | TRUE (site nav + desktop page + blog) |
| Scale claims | "#1 most-used online PDF software worldwide"; "16.5 million documents processed every day"; "82% cost savings for businesses switching" | UNVERIFIABLE (self-reported marketing on pricing page; no independent metric) |
| Reputation | Trustpilot ~5.0/5 on ~13,400 reviews (Trustpilot's own snippet: "iLovePDF has 5 stars! … 13,428 people"); pricing page claims "4.9 average rating"; G2 4.6/5 (582 reviews) | MOSTLY_TRUE (Trustpilot snippet is primary-via-search; G2 single platform) |
| Certifications | ISO/IEC 27001:2017 (cert "renewed in November–March 2023"), GDPR-compliant EU company, QTSP integration under eIDAS for signatures | TRUE (security page, primary) |
| Data retention | All processed files auto-deleted within 2 hours; manual delete button on download screen; **signed documents retained up to 5 years** (eIDAS custody) | TRUE (FAQ + security + privacy pages — three primary confirmations) |
| Encryption | "End-to-end encryption" claim for upload→process→return; HTTPS/CDN/DDoS; storage via "a leading data storage provider" | PARTLY_TRUE (E2E claim is marketing-grade; no crypto spec published; UNVERIFIABLE in detail) |
| Regional processing | Premium: "7 regions"; Business: "11 regions" — file-processing region options | TRUE (pricing page) — implies server processing is region-selectable, not user-local |

**The one-line model:** iLovePDF sells *unmetered access to a task grid* — every tool is one URL/one verb; the free tier is a funnel (per-tool file-count + file-size caps, ads, batch limits), and Premium ($4/mo annual, $7/mo monthly) removes caps across Web+Desktop+Mobile while adding the AI suite, digital signatures, and workflows. The desktop app exists primarily to move processing **local** ("maximum privacy") and to sell Premium.

---

## 1. Spec tables by domain

Legend: **Surface** W = web (cloud processing, upload required), D = desktop (local processing), M = mobile. **Cloud?** = does the task's default path require upload/network. Free-tier limits are the **exact values from iLovePDF's own pricing-page comparison table** (scraped 2026-09-08) unless noted. Premium = $4/mo billed annually ($48/yr) or $7/mo billed monthly; Business = custom (25+ seats).

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Standalone PDF reader | Preview, read, print; "robust PDF viewer that can help you read large PDF documents without crashing"; works without internet | D | Free (reader is the free desktop tier) | n/a | N | The reader is the entire free desktop product — tools on desktop are Premium-gated. TRUE (desktop page pricing block: "Desktop Reader — Free" vs "Desktop Tools and Reader — $7/mo") |
| Continuous/two-page/presentation modes; dark reading; bookmark pane | — | Neither | — | — | — | **Absent.** No reading-mode surface documented anywhere (desktop page, blog, help). Basic preview/read/print only. TRUE (absence across primary pages). GlyphPDF §9.1 is categorical over this |
| Open password-protected PDFs | Password prompt; desktop can auto-apply a stored password | D+W | Within task caps | Same | W: Y / D: N | Desktop "Automatically apply passwords: apply default passwords to automatically unlock protected documents or add passwords to processed files" — batch-friendly. TRUE (official blog) |
| Jump-to-page / thumbnails / search in-doc | Not documented as a product surface | Neither | — | — | — | iLovePDF has no "viewer" pages in nav/sitemap; search/within-reader features undocumented. UNVERIFIABLE (absence of evidence — tool exists only as processing surfaces) |
| Right-click shell conversions (Windows) | Right-click a PDF → choose tool → process without opening the app | D | Premium | Included | N | "The fastest way to convert and compress files" — loved convenience feature. TRUE (desktop page) |
| Recents / favorites / cloud file pickers | Google Drive, Dropbox (web tools), OneDrive/SharePoint (mobile), favorites (mobile) | W+M | Free | Same | Y | Cloud-account-tethered, no local MRU story on web. TRUE |

### 1.2 Editing (text / object / page)

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Edit **existing** PDF text | NEW "Content Editor": "Select text to edit, move, or delete the existing content"; "You can edit existing text directly in the document" | W | 1 file/task; **100 MB cap at ALL tiers** | Same caps | Y | Historically add-only; now real existing-text editing. **RTL languages not supported** ("your changes may not be applied correctly"). TRUE (tool page, mined UI strings) |
| Add text / shapes / images / comments / highlights | Toolbar-based placement; z-order control ("Reorder items to move them to the back or front"); remove-all | W (+M richer) | 1 file/task; 100 MB all tiers | Same | Y | Mobile Edit adds audio, links, highlighting, sticky notes, line thickness/opacity (blog). TRUE |
| Page organization | Organize PDF: visual page grid, reorder, rotate, delete; Extract pages; Remove pages; Rotate PDF | W+D | Organize: 5 files/100 MB free → 20 files premium; Rotate: 20 files free → 80; Remove/Extract within split/organize caps | 20/80 files, 4 GB | W: Y / D: N | Extract & Remove pages are separate nav tools (likely organize modes). TRUE (nav + pricing) |
| Split PDF | Range mode (Custom/Fixed); fixed ranges N pages per file; split by size (max MB per file) + optional compression; extract selected pages; merge-all-ranges-into-one | W+D | 1 file, 100 MB free; 10 files/4 GB premium | — | W: Y / D: N | |
| Split PDF (**Smart**) | "Select a document category and a split preset. Or choose Custom prompt to guide the split with your own instructions" — AI-guided chapter/section splitting | W | 200 pages free; "Custom" paid | Custom | Y | Distinctive AI page logic; category-driven presets (loading dynamically). TRUE (tool page + pricing row) |
| Merge PDF | Reorder before merge; up to 25 files free / 500 premium per task | W+D | 25 files/100 MB | 500 files/4 GB | W: Y / D: N | Merge is the brand's flagship (homepage hero) |
| Page furniture | Add page numbers (2 free/10 prem), Add watermark (2/10) | W+D | 100 MB / 4 GB | — | W: Y / D: N | Position/format options not enumerated in scrape; watermark = image or text (historical, PARTLY_TRUE) |
| Crop PDF | — | W | 1 file/100 MB **at all tiers** | Same | Y | Premium does NOT lift crop caps (1 file). TRUE (pricing table) |
| Metadata editing | Author, Keywords, Creator | D | Premium desktop | — | N | Desktop-exclusive feature per blog. TRUE |
| Undo/redo session depth | Not documented | W | — | — | — | UNVERIFIABLE (no docs found) |

### 1.3 OCR

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| OCR engine (web) | Server-side pipeline ("Recognising text…" progress state after upload); "high accuracy" marketing; searchable/selectable output PDF | W | **1 file, 15 MB per task** | 10 files, 4 GB | Y | Language set is the **full Tesseract tessdata catalog** (~120 entries incl. Math/Equation detection, Korean vertical, Fraktur, Middle-French/English, Cherokee, Inuktitut, Yiddish…) — strong engine fingerprint. TRUE for list (mined selector); engine identity PARTLY_TRUE (inferred from language catalog) |
| Languages | ~120 Tesseract languages+scripts; multi-select ("The accuracy of detection is increased by correctly selecting the document's languages") | W+D | Same | Same | W: Y / D: N | Full list mined from ocr-pdf page (see Sources snapshot). Broadest published set among studied competitors incl. Sejda (60) |
| OCR (desktop) | Offline OCR included in desktop tools; "Automatic tool settings … default … OCR features" | D | Premium (desktop tools gated) | Included | N | Local Tesseract-class OCR offline — the direct iLovePDF analog to GlyphPDF's offline OCR |
| Output modes | Searchable PDF only (no .txt/DOCX-OCR output documented on the OCR tool; OCR'd conversions via PDF→Word "(OCR)" variant exist as separate conversion rows) | W+D | — | — | — | Pricing table lists **"PDF to Word (OCR)"** and **"PDF to Excel (OCR)"** as distinct conversion products — OCR-then-convert pipeline. TRUE (pricing table) |
| Review/correction screen | — | Neither | — | — | — | **Absent.** No word-level verify UI anywhere; output is take-it-or-leave-it. GlyphPDF OCR Verify (U03, F11/R07-F08/R08) is categorical over iLovePDF. TRUE (absence in tool page flow) |
| Scan prep (deskew/binarize/denoise/orientation) | — | W | — | — | — | Not surfaced on OCR page. Mobile has a scanner app ("Mobile PDF Scanner… entirely on your mobile devices"). TRUE absent on web OCR |
| Pattern/honesty disclosures | No accuracy disclaimer found on tool page (contrast Sejda's honesty banner) | W | — | — | — | "High accuracy" unqualified. TRUE absent |

### 1.4 Forms (fill / create)

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Fill AcroForms | Field-aware fill; required-field save validation: "You cannot save this file in Fill mode because a required field is missing. Fill in the field or switch to Edit mode" | W | 1 file, 15 MB free → 100 MB premium | 1 file, 100 MB | Y | Note: forms size cap lifts to only 100 MB even on Premium. TRUE (pricing + tool page) |
| Auto field detection | "Detect automatically" — offers auto-detect when no fields found ("It may be a scanned or a flattened PDF… We could not detect any form in the PDF") | W | Same | Same | Y | Fallback: "Add the form fields manually" / "Edit/add manually". Parallels GlyphPDF content-aware auto-detect (88d4286, V06). TRUE (mined dialog strings) |
| Create fields | Text fields, checkboxes, radio buttons, lists ("editable and interactive text fields, checkboxes, radio buttons, and lists") | W | Same | Same | Y | **No** dropdown/list-box distinction, no calculated fields, no validation rules, no tab-order editor, no date/numeric fields documented — vs GlyphPDF's 10 field types incl. calculated (PRD §9.6). MOSTLY_TRUE (marketing summary; full widget palette not enumerable from scrape) |
| Form data import/export (CSV/FDF/XML) | — | Neither | — | — | — | Absent from tool + pricing. TRUE absent. GlyphPDF has CSV/FDF (PRD §27) |
| Flatten forms | Not a standalone tool | W | — | — | — | Flatten mentioned only as a *condition* ("It may be a … flattened PDF"). TRUE absent as tool. GlyphPDF has flatten (PRD §9.6) |

### 1.5 Comments & markup

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Annotate in Edit PDF | "adding text, shapes, comments and highlights" inside the editor | W+M | Within Edit caps (1 file/100 MB) | Same | Y | Markup lives inside Edit; no dedicated comment mode. TRUE |
| Comment management (list, threads, statuses, filters, export) | — | Neither | — | — | — | **Absent.** GlyphPDF U07 (filters, table view, CSV export) has no iLovePDF counterpart. TRUE absent |
| Mobile-only markup | Audio annotations, sticky notes, links, thickness/opacity | M | — | — | Y | Mobile Edit is richer than web Edit for markup. TRUE (official blog) |
| Stamps / measurement / audio (desktop/web) | Company Stamp exists only inside Sign tool | W (Sign) | — | — | — | No general stamp tool; no measurement. TRUE absent |

### 1.6 Redaction

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Redact PDF tool (exists — correction to prior assumption) | Pattern quick-marks: **Text, Credit Card, Phone Number, Email**; "Select and search text or pages to start redacting sensitive content"; Clear all; Accept; "Remember to review the result of your document before sending private information"; "Recognising text…" (server-side text pass) | W | 1 file, **400 MB at ALL tiers** | Same | Y | Premium does NOT lift redact caps. **"Right-to-left languages are not currently supported by this tool."** TRUE (tool page, mined UI) |
| Excision mechanics | Undisclosed — no claim about content-stream removal vs burn-in flatten; no verification report, no sanitize bundle | W | — | — | Y | Whether redacted text is truly removed from the file is **UNVERIFIABLE from primary sources** (server-side black box). GlyphPDF's excision + transaction + SHA-256 invariance + default-ON sanitize (U05, E-1, D07, N04) is verifiable-by-design — a categorical honesty advantage |
| Pattern redaction scope | 4 fixed patterns; no custom regex, no word-list import, no presets, no overlay labels | W | — | — | Y | GlyphPDF has regex + word-list import (256KB, a9a3eda) + named presets (Email/Phone-US/SSN, b43ef08) + overlay text labels (24c479d, N08). TRUE absent on iLovePDF |
| Batch redaction | — | Neither | — | — | — | 1 file per task at every tier. TRUE. GlyphPDF has batch redact (PRD §27 §9.12) |

### 1.7 Security (password / protect / hygiene)

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Protect PDF | Set open password; permission restrictions (scope not enumerated in scrape) | W+D | 2 files/100 MB free → **80 files**/4 GB premium | 80/4 GB | W: Y / D: N | Encryption algorithm not advertised (AES variant UNVERIFIABLE). Desktop can auto-apply passwords in batch. TRUE caps |
| Unlock PDF | Remove password/owner restrictions | W+D | 2 files/100 MB → 10/4 GB | 10/4 GB | W: Y / D: N | |
| Watermark as security | Text/image watermark grouped under security in nav | W+D | 2 files/100 MB → 10/4 GB | 10/4 GB | W: Y / D: N | |
| Metadata sanitize / document hygiene | No dedicated sanitizer tool | — | — | — | — | Absent. GlyphPDF sanitize bundle default-ON (b64aef2, D07) has no iLovePDF counterpart. TRUE absent |
| Repair PDF | Attempts to recover damaged files | W+D | 1 file/100 MB → 10/4 GB | 10/4 GB | W: Y / D: N | GlyphPDF has no repair tool — **a real GlyphPDF gap** worth noting |
| Regional file processing | Choose processing region: 7 regions (Premium), 11 (Business) | W | — | Included | Y | Privacy-mitigation pattern for cloud processing; honest data-flow disclosure. TRUE (pricing). GlyphPDF N/A (local-only) |

### 1.8 Signatures (Sign PDF + iLoveSign)

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Self-sign (Simple Signature) | Draw / type (color options) / upload PNG-JPG-SVG / **draw from mobile device** (QR handoff); initials; company stamp upload | W+D+M | 3 files/50 MB per task free → 5 files/50 MB premium | 5/50 MB | W: Y / D: N (sign on desktop processes locally; requests need cloud) | Sign size cap stays 50 MB at ALL tiers. TRUE (pricing + tool page) |
| Send for signature (workflow) | "Only me" vs "Several people — invite others"; signers list; required/optional fields: Signature, Initials, Name, Date, Text Input, Company Stamp; **signing order**; expiration date (default 15 days); email notifications; **reminders every N days**; per-signer language; **Multiple requests (Premium)** — each signer gets a unique separate request | W | Free tier can send requests (limited — exact monthly count not published on scraped pages) | Unlimited requests | Y | This is a full multi-party eSign workflow INSIDE the main product — and also spun out as iLoveSign (SES unlimited free self-sign; requests limited free; **AES advanced eSign: 5 free**; branding, templates, custody, audit trail). TRUE (tool page + pricing iLoveSign rows) |
| Digital Signature (cryptographic) | "A signed Certified Hash and a Qualified Timestamp is embedded to the signed documents… Certified signatures are **eIDAS, ESIGN & UETA compliant**" — QTSP-backed | W | Premium-gated | Included | Y | Cloud QTSP model vs GlyphPDF's **local PAdES B-LT/B-LTA with own cert + OCSP validation** (PRD §27). iLovePDF leads on managed/qualified signing + custody (5-yr retention); GlyphPDF leads on offline/controlled PKI. TRUE (tool page + security page) |
| Audit trail / custody / status tracking | Signature tracking (web-exclusive feature); signed-doc custody 5 years; notifications | W (iLoveSign) | Limited free | Unlimited | Y | GlyphPDF §9.7 gap confirmed by PRD (multi-party send-for-signing "not started") — **this is iLovePDF's lead** |
| Certificate-based local signing (PAdES with own cert) | — | Neither | — | — | — | No local digital-ID signing; all qualified signing is cloud QTSP. TRUE absent locally. GlyphPDF categorical here |

### 1.9 Compare

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Compare PDF tool (exists — correction to prior assumption) | Two modes: **Semantic Text** ("Compare text changes between two PDFs. Change report") and **Content Overlay** ("Overlay content from two files and display any changes in a separate color"); **scroll sync**; Old/New page navigation; live change counter | W | 2 files, **400 MB at ALL tiers** | Same | Y | Server-side (upload both files). TRUE (tool page, mined UI) |
| Structural page-change detection (added/removed/reordered pages) | Not surfaced — change report is text-change oriented; no page-tree diff UI, no filters, no exported report file | W | — | — | Y | GlyphPDF's structural fingerprints, middle-insertion alignment, change filters, HTML/text reports, page-reorder detection (CMP-align, V04, U04, §9.10-a) is deeper. MOSTLY_TRUE (absence inferred from scraped UI vocabulary; full behavior is server-side black box) |
| Overlay visual diff | Present (distinctive) | W | — | — | Y | **GlyphPDF lacks a pixel/overlay visual diff mode** — candidate parity item |
| Compare report export | "Change report" panel only; no downloadable report documented | W | — | — | Y | GlyphPDF exports HTML/text reports. TRUE absent |

### 1.10 Batch processing

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Multi-file batch per tool | Every tool accepts N files per task per the pricing matrix (free: 1–25 depending on tool; premium: mostly 10, rotate/Image→PDF 80, Protect 80, Organize 20, Merge 500) | W+D+M | "Limited" | **Unlimited** batch | W: Y / D: N | Exact free-tier batch rule beyond the published per-tool numbers: undocumented. TRUE for published matrix |
| Chained tasks | Take a processed file and jump directly into the next tool | W | Free | — | Y | Web-exclusive per official blog — the closest thing to workflows in the free funnel. TRUE |
| Workflows | Listed as a Premium feature on pricing ("Workflows" also in Business row) | W | — | Included | Y | Product surface undocumented in scrape — UNVERIFIABLE depth (likely Business-oriented; Teams blog mentions business workspaces with roles + PDF/A restrictions) |
| Desktop bulk processing | "Process documents in bulk… multiple documents at the same time"; output-folder control; automatic tool settings (default compression/PDF-A/watermark/OCR) | D | Premium (tools gated) | Included | N | **No hot-folder/watching** documented anywhere — GlyphPDF §9.12 hot folder leads. TRUE absent |
| Folder automation / hot folder / CLI | — | Neither | — | — | — | Absent from product surfaces; API covers automation server-side instead (iLoveAPI). TRUE absent |

### 1.11 Print production

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Bates numbering | — | Neither | — | — | — | **Absent** (no tool in nav/sitemap/pricing). GlyphPDF Bates (PRD §9.9) leads. TRUE absent (sitemap enumeration) |
| Imposition / N-up | — | Neither | — | — | — | Absent (contrast Sejda N-up). TRUE absent |
| Page sizes / resize / prepress color | Crop exists (trim); no page-resize/padding tool | W | 1 file, all tiers | Same | Y | No color management outputs documented. PARTLY_TRUE |
| Compression (print-adjacent) | Compress PDF: 200 MB free → 4 GB premium; 2 files free → 10 | W+D | 2/200 MB | 10/4 GB | W: Y / D: N | Compression level presets; desktop default compression settings. LOW/MEDIUM/HIGH-style presets historical (PARTLY_TRUE) |

### 1.12 Accessibility / tagging

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Tagged-PDF reading-order / accessibility checker | — | Neither | — | — | — | **Absent.** GlyphPDF §9.14 reading-order check leads. TRUE absent |
| PDF/A accessible levels | PDF/A-**1a/2a/3a** conversion (accessible conformance levels) offered alongside -b/-u | W+D | 1 file/100 MB free → 10/4 GB premium | 10/4 GB | W: Y / D: N | **iLovePDF ships 8 levels (1b/1a/2b/2u/2a/3b/3u/3a) vs GlyphPDF's 5 (1B/2B/2U/3B/3U)** — the -a levels are a parity gap. TRUE (tool page selector mined) |
| PDF/A validation | Desktop: "PDF/A Validator — View the PDF/A status of PDF files to assure that they are compliant" | D | Reader free? validator in desktop | — | N | Web tool converts but does not validate. GlyphPDF's veraPDF CLI wiring (1582f46) + E-1-residual ICC/CIDSet work is the stronger validation contract; iLovePDF's reader-visible compliance STATUS is a UX pattern worth copying. TRUE (blog + desktop page) |
| Screen-reader UI / RTL support | RTL **unsupported** in Edit Content Editor and Redact tool | W | — | — | Y | 25 UI languages incl. Arabic UI — but content tools break on RTL. TRUE (two tool-page disclosures) |

### 1.13 Import / export formats

| Feature | Sub-capabilities | Surface | Free-tier limits | Premium | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF → Office | PDF to Word, Excel, PowerPoint; **plus OCR variants** ("PDF to Word (OCR)", "PDF to Excel (OCR)" as separate products) | W+D | 1 file/15 MB → 10/4 GB | 10/4 GB | W: Y / D: N | Fidelity undisclosed; no per-engine claims. Desktop conversions run locally (offline claim covers Office conversion) |
| Office → PDF | Word, PowerPoint, Excel to PDF; JPG/PNG→PDF (Image to PDF: 20 free → 80 premium; 40 MB → 4 GB) | W+D+M | 1 file/15 MB (Office) | 10/4 GB | W: Y / D: N | |
| PDF → images | PDF to JPG (2 free → 10 premium; 25 MB → 4 GB); JPG→PDF reverse | W+D+M | 2/25 MB | 10/4 GB | Y (web) | |
| HTML → PDF | Web page capture | W only | Within caps | — | Y | Web-exclusive per blog. TRUE |
| PDF → Markdown | AI-suite export target | W | 1 file/15 MB → 200 MB premium | 200 MB | Y | New AI-era tool. TRUE (pricing) |
| PDF → PDF/A | 8 conformance levels | W+D | 1/100 MB → 10/4 GB | 10/4 GB | W: Y / D: N | Desktop converts + validates |
| Repair | Damaged-PDF recovery as import path | W+D | 1/100 MB → 10/4 GB | 10/4 GB | W: Y / D: N | **GlyphPDF gap — no repair tool** |
| Djot/HTML/CSV text exports; FDF/CSV form data | — | Neither | — | — | — | Absent. GlyphPDF conversion breadth (Word/Excel/PPT/image/text/HTML/CSV + CSV/FDF form data) leads. TRUE absent |

### 1.14 Cloud / web–desktop–mobile split

| Dimension | Web | Desktop | Mobile | Notes |
|---|---|---|---|---|
| Processing location | Cloud (upload; 2-h retention; region choice on paid) | **Local, offline** ("process heavy PDF tasks offline"; "processing your files on your computer for maximum privacy") | Cloud-integrated ("integrated cloud storage processing"); scanner "entirely on your mobile devices" | TRUE (desktop page + blog). iLovePDF's own marketing concedes the privacy argument — GlyphPDF's home turf |
| Installers | — | Microsoft Store + Mac App Store; MSI/DMG/unattended via Sales | App Store/Play | Store-only consumer path; enterprise installers sales-gated. TRUE |
| Tool parity | All 25 tools + AI suite | "most of the tools available at ilovepdf.com" (offline); AI suite + Redact/Compare web-status on desktop undocumented | Merge/split/organize + rich Edit + scanner + Create PDF (blank docs, templates: invoices, planners, CVs) | Desktop exact tool list not published — MOSTLY_TRUE on "most tools" (official wording, unenumerated) |
| Exclusive to desktop | — | PDF/A Validator status view; output folder; auto-passwords; metadata edit; automatic tool settings; right-click shell conversions; free Reader | — | TRUE (blog) |
| Exclusive to web | HTML→PDF; digital signatures + request tracking; chained tasks; Teams; account mgmt | — | — | TRUE (blog) |
| Free tier on desktop | — | PDF Reader free; tools require Premium (desktop pricing block); MS Store listing reportedly mentions "daily limit of 3 tasks" for free tool use | Limited tasks | MOSTLY_TRUE — desktop-page pricing says Reader free / tools Premium; the 3-tasks/day figure appears in the Store listing per search but the listing itself is a JS shell (not directly verified) |

---

## 2. Licensing granularity

| Tier | Price | What it unlocks (published) |
|---|---|---|
| **Free / Basic** | $0 (no account required for web; account for some features) | "Access to essential iLovePDF tools; Limited document processing"; per-tool caps: file counts (1–25/task per matrix), sizes (15 MB conversions, 100 MB general, 200 MB compress, 400 MB redact/compare); batch "Limited"; ads shown; free Split-Smart up to 200 pages; AI Summarizer/Translate up to 67 pages; iLoveSign SES self-sign unlimited; desktop = PDF Reader free |
| **Premium** | **$4/mo billed annually ($48/yr); $7/mo billed monthly**; 1–25 seats | "Full access to all tools; Unlimited document processing (per-task caps become 4 GB); Web+Mobile+Desktop; Digital Signatures (certified hash + qualified timestamp); Workflows; Ad-free; Priority support; Regional file processing (7 regions); 2,000 AI credits; batch unlimited (Merge 500, Rotate/Image→PDF 80, Protect 80, Organize 20)"; education: **full year Premium free for students/academics** |
| **Business** | Custom ("25+ users", contact sales) | All Premium + SSO + dedicated account manager + custom contracts + 11 processing regions + invoicing/bank transfer |
| **Hard caps that survive Premium** | — | Edit PDF 1 file/100 MB; Sign PDF 5 files/50 MB; Redact 1/400 MB; Compare 2/400 MB; AI Summarizer 50 MB; Crop 1 file — several tools do NOT scale with the subscription (per pricing matrix). TRUE |
| Separate products | iLoveAPI (developer, separate pricing); iLoveSign (own free/paid ladder: SES free unlimited, AES 5 free); iLoveIMG | TRUE |

**Key structural fact:** the free tier is metered by **per-task file counts and per-task file size** (published precisely per tool — unusual transparency), plus ads, plus undocumented "limited document processing" throttling ("iLovePDF may investigate any account that registers an unusual number of tasks" — Terms). No published fixed tasks/hour number exists on official pages; third-party reviews report ~1–3 tasks/day-hour variance. Verdict: caps TRUE; exact throttle UNVERIFIABLE.

---

## 3. GlyphPDF delta (bidirectional), per domain

Format: **G→i** = GlyphPDF leads (keep/leverage). **i→G** = iLovePDF leads (consider adopting).

| Domain | G→i (GlyphPDF leads) | i→G (iLovePDF leads — candidate items) |
|---|---|---|
| Viewing | Full reading surface: continuous/two-page/presentation, dark mode, bookmarks (PRD §9.1) vs basic preview reader | Right-click shell integration ("fastest way to convert"); PDF/A compliance **status** visible in a free reader; auto-password presets |
| Editing | Undo/redo discipline across complex ops; local no-upload editing at any file size (iLovePDF Edit hard-caps 100 MB/1 file even paid) | NEW Content Editor (existing-text edit/move/delete) closed the historic gap — verify parity of GlyphPDF inline text edit UX; z-order ("back/front") affordance; Split-Smart (category/prompt-driven page logic) as an AI-era pattern |
| OCR | ROVER dual-engine (Tesseract+RapidOCR+PP-DocLayout) + **Verify screen** + deskew/binarize/denoise/orientation + confidence lifecycle (F05–F11, U03) vs single-engine server OCR with no review step; GlyphPDF offline vs 15 MB/1-file free cap | Language catalog breadth (~120 Tesseract entries incl. Math/Equation detection, vertical Korean); "PDF to Word/Excel **(OCR)**" as explicit product rows — frame OCR-convert as first-class conversion |
| Forms | 10 field types incl. calculated; CSV/FDF data; undo-safe auto-detect (F01/R01, V06) | Required-field save validation wording; explicit "no fields found → detect automatically / add manually" fallback dialog (mirrors GlyphPDF auto-detect but with friendlier UX copy) |
| Comments | Full comment management: threads, filters, table view, CSV export (U07) vs none | Mobile-grade markup (audio, sticky notes) — out of scope for desktop v1; note only |
| Redaction | Content-stream excision + 7-stage transaction + SHA-256 invariance + default-ON sanitize + regex/word-list/presets + overlay labels + batch (U05, E-1, D07, N04, a9a3eda, b43ef08) vs 4 fixed patterns, black-box server burn-in, RTL-broken, 1 file/task | Pattern **taxonomy naming** (Text/Credit Card/Phone/Email chips as one-tap marks) is a clean UX pattern GlyphPDF presets can mirror |
| Security | Local-only by architecture; sanitize bundle; AES-256 | Regional-processing disclosure + ISO 27001 messaging; published per-tool cap matrix (transparency pattern → CapabilityRegistry); Repair PDF tool (**GlyphPDF gap**) |
| Signatures | Local PAdES B-LT/B-LTA, visible /AP appearance, trust-chain/OCSP validation, DSS degradation surfacing (§9.7, N05/N06) vs cloud QTSP only, no local cert signing | **Multi-party workflow: send-for-signing, signing order, reminders, expiry, notifications, status tracking, 5-yr custody, draw-from-mobile** — the single largest iLovePDF lead (also GlyphPDF's declared §9.7 roadmap gap) |
| Compare | Structural page diff (fingerprints, middle-insert, reorders), change filters, HTML/text report export, offline vs text-report-only server compare | **Content Overlay visual diff mode** with per-color change rendering + scroll sync — a concrete feature GlyphPDF lacks |
| Batch | Hot-folder automation, batch OCR/Merge/Redact, per-item pre-flight (§9.12, U08) vs per-tool multi-select only | Chained tasks (result → next tool) as a UX loop; desktop "automatic tool settings" (remember default compression/PDF-A/watermark/OCR) |
| Print production | Bates numbering, page labels (ISO Table 159 scheme), split-by-range outputs | Nothing (Bates/N-up absent on iLovePDF) |
| Accessibility | Reading-order check (§9.14, c862307) vs none | **PDF/A -a levels (1a/2a/3a)** — 3 conformance levels GlyphPDF lacks; RTL support in content tools (iLovePDF explicitly breaks; an easy GlyphPDF win to claim) |
| Import/export | In-house OOXML, JPEG re-encode, Djot, HTML/CSV, bookmark/link round-trip contract vs HTML→PDF, PDF→Markdown | HTML→PDF (web capture) tool; Repair-PDF import path |
| Platform split | One native Windows app, offline everything, no account | Chained cross-surface model (web+desktop+mobile one sub); education-free-year program; store-distributed desktop with enterprise MSI |

## 4. Failure modes (user-reported or structural)

| # | Failure mode | Evidence | Verdict |
|---|---|---|---|
| F1 | **Upload privacy wall**: every web tool requires uploading the document to iLovePDF servers (2-h retention, region processing, E2E-encryption *claim*); sensitive-doc workflows (contracts, IDs, medical) are structurally exposed; iLovePDF's own marketing sells the desktop app as the fix ("maximum privacy") — conceding the point | Security/privacy/FAQ pages; desktop page; blog | TRUE (structural, primary sources) |
| F2 | **Free-tier metering ambush**: unpublished "limited document processing" throttle + tiny conversion caps (15 MB PDF→Word) + 1 file/task on most convert tools + ads; Terms reserve right to "investigate any account that registers an unusual number of tasks"; users report hitting task walls mid-workflow | Pricing page (precise per-tool caps, but no task-rate number); Terms; third-party reviews | TRUE for caps; UNVERIFIABLE for exact throttle |
| F3 | **Premium doesn't lift everything**: Edit (1 file/100 MB), Sign (50 MB), Redact (1 file), Compare (2 files), Summarizer (50 MB) keep paid-tier caps — subscription value is uneven and the pricing matrix buries this | Pricing page comparison table | TRUE (primary) |
| F4 | **Content-tool language holes**: RTL ("may not be applied correctly" in Edit; "not currently supported" in Redact) — Arabic/Hebrew users can corrupt documents silently in Edit | Tool-page disclosures | TRUE (primary) |
| F5 | **Black-box fidelity**: conversions/redaction/OCR/compare run server-side with no verifiable mechanics, no accuracy disclaimers, no report artifacts — users can't audit what was done to a document (redaction excision quality especially) | Absence of any mechanism disclosure across pages | TRUE (absence verified); actual quality UNVERIFIABLE |

## 5. Loved workflows (what made the brand)

| # | Workflow | Evidence | Verdict |
|---|---|---|---|
| L1 | **One-verb task grid**: merge / split / compress / JPG↔PDF in <30s with zero learning curve — the default consumer answer to "I just need to combine two PDFs" | Product architecture; Trustpilot ~13.4k reviews ≈ 5.0 | TRUE (behavioral consensus, multi-source) |
| L2 | **Bureaucracy conversions**: PDF→Word/JPG round-trips for forms, applications, immigration paperwork; fill-and-sign simple PDFs free | Review platforms; product breadth | MOSTLY_TRUE (secondary corroboration) |
| L3 | **Chained tasks + right-click conversions**: process result straight into the next tool (web); right-click → convert without opening the app (desktop) | Official blog + desktop page | TRUE (primary) |

---

## 6. Confidence level

**High (≈0.85)** for: tool inventory (live sitemap + leaked nav), pricing/cap matrix (verbatim scrape of the official comparison table), platform split (official blog), redact/compare/edit/sign/forms UI capabilities (mined tool-page strings), privacy/retention/certifications (three primary pages), absence claims (sitemap enumeration). **Medium** for: desktop free-tier exact limits (Store listing not directly parseable), desktop exact tool list ("most tools" unenumerated), sign-request free quota. **Low/UNVERIFIABLE**: actual server-side quality (OCR accuracy, conversion fidelity, redaction excision correctness), scale claims, exact free-task throttle.

## Sources

Primary (ilovepdf.com, crawled 2026-09-08; snapshots `_ilp_*.html` in this directory):
- Sitemap: https://www.ilovepdf.com/sitemap.xml (1,415 URLs; 25 canonical EN tool pages)
- Pricing/cap matrix: https://www.ilovepdf.com/pricing
- Desktop: https://www.ilovepdf.com/desktop
- Tool pages: /edit-pdf, /ocr-pdf, /redact-pdf, /compare-pdf, /sign-pdf, /pdf-forms, /split_pdf, /convert-pdf-to-pdfa, /pdf-summarize
- Trust: /help/faq (2-h retention), /help/security (ISO 27001, GDPR, QTSP, 5-yr custody), /help/privacy (controller identity, Barcelona)
- Platform split: https://www.ilovepdf.com/blog/pdf-web-or-desktop

Secondary:
- Trustpilot: https://www.trustpilot.com/review/ilovepdf.com (via search snippet; direct fetch blocked)
- G2 (4.6/5, 582 reviews), Microsoft Store listing (apps.microsoft.com/detail/9nlvzbz2wz28 — "3 tasks/day" figure via search, JS shell not directly parsed), TechRadar review (edit-depth note)
- Sibling spec for format: sejda.md (this directory)
