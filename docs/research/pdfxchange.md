# Spec Sheet: PDF-XChange Editor (Tracker Software / PDF-XChange Co Ltd)

Research date: 2026-09-07. Product state: v10.x line (10.0.0.370, 14 Jun 2023 → 10.8.6.411, 01 Sep 2026) is the
mainstream line; v11.0.0.0 (14 May 2026) / v11.0.1.0 (09 Jun 2026) superseded it — v11 was briefly pulled for
file-dialog regressions and 11.0.1 reverted to system Open/Save dialogs. This sheet covers v9→v10.8 (as tasked)
and flags v11 deltas where they matter.

**Tier model (official comparison chart):**
- **Free** = PDF-XChange Editor (free version). Runs paid features but **stamps a watermark** on documents where paid features were used (vendor-confirmed wording on product page). GRADE: TRUE.
- **Editor** = paid base license, single-user, from $62 (scraped 2026-09; MOSTLY_TRUE, prices drift).
- **Plus** = PDF-XChange Editor Plus, from $79 — adds Enhanced OCR (ABBYY), form creation, Compare Documents, dynamic stamps, placeholder tool.
- **Pro** = PDF-XChange PRO bundle, from $131 = Editor Plus + PDF-Tools + PDF-XChange Printer Standard. Watched folders / batch conversion / folder monitors live in **PDF-Tools**, not Editor.
- "Cloud?" column: Y = feature requires/reaches an external service. License-activation phone-home is listed once in the Cloud section and applies to the whole product (v9+ licensing).

Primary spec sources: official product comparison chart (160-row feature matrix), official v10+ build history
(text export), official product-page feature catalog (categorized, with free/paid counts), Tracker KB articles,
Tracker forum staff statements. Marketing pages avoided; where only marketing claims exist, rows are marked UNVERIFIABLE.

---

## 1. Viewing & navigation

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| PDF 2.0 (spec 2.0) render support | Also legacy specs | Free | N | Matrix row: Y across all products. GRADE: TRUE (official matrix). |
| Tabbed UI + tab groups | Move tabs to new window / vertical+horizontal groups (10.8), per-tab title styles | Free | N | Changelog 10.8.0. GRADE: TRUE. |
| Session save/restore | Save/open session files, relative paths supported | Free | N | Feature catalog. GRADE: TRUE. |
| Loupe tool, Pan & Zoom pane | Show/hide page boxes+guides in loupe | Free | N | Loved navigation tools. GRADE: TRUE (matrix + catalog). |
| Snapshot tool | One-click image copy, customizable snapshot dropdown (v10.0) | Free | N | Changelog 10.0.0. GRADE: TRUE. |
| Dark Page Mode + UI themes | Darkens page background, not just chrome (v10.5); theme manager | Free | N | Changelog 10.5.0. GRADE: TRUE. |
| Split page view / Spreadsheet split | Two independent views of one doc | Free | N | Catalog. GRADE: TRUE. |
| Page transitions, autoscroll, continuous zoom | Presentation-ish features | Free | N | Catalog. GRADE: TRUE. |
| 3D PDF support | Render, 3D comments + linear measurements (v9.0), cross-sections; v11 rework adds BREP models | Free (render) / Plus (annot) | N | Matrix; v11 3D plugin rework. GRADE: TRUE (v11 BREP: MOSTLY_TRUE — vendor page only). |
| PDF Portfolios | View/edit/print portfolio files; create (Editor) | Free (view) / Editor (create) | N | Matrix rows. GRADE: TRUE. |
| Embedded multimedia + RichMedia annots | Audio/video annots; audio playback list button (10.8) | Free | N | Matrix + changelog. GRADE: TRUE. |
| Browser plugins | IE/Firefox-style embedding (Chromium handled via extension) | Free | N | Matrix. GRADE: MOSTLY_TRUE (modern browser support specifics thin). |
| Autorecovery | Remembers unsaved changes for recovery | Free | N | Matrix. GRADE: TRUE. |
| Search (simple + advanced) | Full text, comments, bookmarks, Quick Launch over prefs (10.1), non-contiguous Ctrl text selection (10.7), de-hyphenation on copy (10.3) | Free (basic) / Editor (advanced) | N | Changelog. GRADE: TRUE. |
| Find & Replace text | Content text replacement (v10.3) | Editor / Pro | N | Changelog 10.3.0; matrix "Find and Replace Text" Editor Y, Free -. GRADE: TRUE. |
| Multi-monitor / per-monitor DPI, ARM64, portable build | x64/arm64, portable ZIP, terminal-server compatible | Free | N | Matrix + catalog. GRADE: TRUE. |
| Customizable ribbon/toolbars, Quick Launch | User ribbons, favorite tools, launch third-party apps | Free | N | Catalog. GRADE: TRUE. |
| Word count, accounting calculator/adding tape | Calculator tool new in v10.0; separator customization 10.4 | Free | N | Changelog. GRADE: TRUE. |

## 2. Editing (text/object/page)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Add/Edit base content text | Text blocks, paragraph properties, soft returns, overtype, RTL text, 'Tab Stop' (v10.0) | Editor+ (free = watermark) | N | Matrix "Edit PDF Content" Editor Y, Free -; v9.0 improved text engine. GRADE: TRUE. |
| Text correction tool | Correct localized text without full reflow | Editor+ | N | Catalog. GRADE: MOSTLY_TRUE (catalog lists; manual detail unverified). |
| Font replacement + Repair ToUnicode CMap | Fix broken subset-font Unicode maps per element (10.1); multi-language ToUnicode repair (10.7) | Editor+ | N | Changelog. Powerful for corrupt-scan exports. GRADE: TRUE. |
| Base content vector object editing | Content pane; path tools: create/edit/manipulate shapes (v10.0), reverse sub-paths (10.1), filling rule (10.1), transform (bottom-up Y option 10.1) | Editor+ | N | Changelogs. GRADE: TRUE. |
| Select Page Region tool | Cut/delete/manipulate page content regions (10.3); select pages by aspect ratio (10.7) | Editor+ | N | Changelog. GRADE: TRUE. |
| Edit images in PDF | Replace images (incl. from clipboard 10.6), edit/manipulate images, transform selection | Editor+ | N | Catalog + matrix. GRADE: TRUE. |
| Page ops | Insert/extract/replace/delete/duplicate/move/swap/rotate/crop/resize pages; reverse order (10.8); delete empty pages + noise tolerance (10.4); normalize pages (10.3, removes redundant rotations/media-box offsets); partial cropping (10.8) | Editor+ (page ops all paid; 19 of 19 paid per catalog) | N | Changelogs + catalog "(19)(0)(19)". GRADE: TRUE. |
| Split | Split by guide lines (v9.0), split document by output file size (v9.0), extract-to-first-doc option (10.3) | Editor+ | N | Changelogs. GRADE: TRUE. |
| Overlay/underlay pages | PDF-on-PDF overlay w/ comment+form handling options (10.6), import doc as layer (v9.0) | Editor+ | N | Changelog. GRADE: TRUE. |
| Headers/footers, Bates, watermarks, backgrounds | Shrink content under H/F+Bates; watermark on-screen/printed visibility (v9.0); Bates macros | Editor+ (H/F+Bates via Editor; also PDF-Tools) | N | Catalog + matrix. GRADE: TRUE. |
| Barcodes | Add barcode to doc; generate links from barcodes (10.4); detect in region | Editor+ | N | Changelog. GRADE: TRUE. |
| Layers (OCG) | Add/edit/remove/reorder layers, import as layer, merge selected layers (10.1), flatten, parent-dependent child visibility (10.7), select layer objects | Editor+ | N | Changelogs. GRADE: TRUE. |
| Guides/rulers/grid/snap | Snap customization (10.0), guide percentage units (10.0), copy/paste guides (10.5), split pages by guides | Editor+ | N | Changelogs. GRADE: TRUE. |
| Audit Space Usage | Per-feature byte breakdown of the file (10.1) | Editor+ | N | Changelog 10.1.0. GRADE: TRUE. |
| Page labels | Set via page properties (10.3) | Editor+ | N | Changelog. GRADE: TRUE. |
| Macros system | Filename macros, Bates value macro, util.expandMacros JS API (10.6) | Editor+ | N | Changelog. GRADE: TRUE. |
| Color tools | Convert Colors: remap/colorize/B&W (10.5), Colorize Document (10.6) | Editor+ | N | Changelog. GRADE: TRUE. |
| Bookmarks automation | Generate from page text/TOC/text file/highlights (10.8), bookmark every Nth page, regex on titles, sort/merge/validate, export to HTML/TXT | Mixed (basic free; generators paid) | N | Catalog "(23)(6)(17)". GRADE: TRUE. |

## 3. OCR

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Engine 1: Tesseract (default) | Since v9 two engines; Tesseract bundled | Free runs it (watermark) / Editor+ clean output | N | KB352: "Since Version 9, PDF-XChange uses two OCR engines: Tesseract (Default) and ABBYY FineReader Engine". GRADE: TRUE. |
| Engine 2: "Enhanced OCR" = ABBYY FineReader Engine | Commercial-grade engine plugin | Plus/Pro (PDF-Tools: PRO licenses only) | N | Matrix "Enhanced OCR"; KB352. A commercial ABBYY engine in a $79 product is a major differentiator. GRADE: TRUE. |
| OCR scope modes | Current page/page range, selected region, multiple images via Explorer context menu | Free runs w/ watermark; clean in paid | N | Matrix rows "OCR Selected Regions/Images"; catalog "(5)(2)(3)". GRADE: TRUE. |
| Editable OCR output | OCR text layer directly editable | Plus/Pro only ("output is not directly editable unless Editor Plus or PRO") | N | Matrix row 118 literal wording. GRADE: TRUE. |
| OCR languages | Tesseract packs via in-app Resource Updater ("Add/Update languages"); uninstall languages w/o restart (10.0); legacy site packs enumerate ~100 language variants across 14 regional packs | Free/paid, packs download from Tracker | N (files download, OCR runs local — vendor statement Oct 2025 forum) | GRADE: TRUE (mechanism); exact current count UNVERIFIABLE (v8+ packs only via in-app updater). |
| OCR during Office conversion | Recognize characters when converting PDF→Word/Excel/PPT; multi-language selection (10.7) | Editor+ (converter is paid) | N | Changelog 10.7.0. GRADE: TRUE. |
| OCR preprocessing | Deskew Scanned Pages, Enhance Scanned Pages (improved 10.3, image-compression control 10.2), Correct Scans: edge detect + crop + de-warp/perspective (10.8), skip logos/stamps/graphical elements during OCR (10.3), draw lines for tables in OCR | Editor+ | N | Changelogs + catalog. GRADE: TRUE. |

## 4. Forms (creation, filling, calculation, JavaScript)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Fill forms | Fill, reset forms, highlight fields | Free | N | Standard behavior; matrix "PDF Document Permissions" free Y. GRADE: MOSTLY_TRUE (filling itself universally reported free). |
| Create/edit fillable forms | 10 field types: text, button, checkbox, radio, dropdown, list box, **date, image, barcode, digital signature** | Plus ("Using Editor Plus" in Editor column) | N | Catalog "(10)(1)(9)"; matrix. Date/image/barcode fields exceed GlyphPDF's set. GRADE: TRUE. |
| Identify Forms (auto-detect) | Detect fields in a flat PDF (v10.0); radio-group identification (10.7) | Editor+ (auto-detect listed under paid; free=watermark) | N | Changelog 10.0.0. GRADE: TRUE. |
| Forms Recognition | Recognize + convert static layout to widgets | Plus | N | Matrix row 99. GRADE: TRUE. |
| Form calculation + validation | "Use Forms to Calculate Values", "Use Forms to Validate Values" (JS-driven) | Editor+ (JS support free-base) | N | Catalog; "Add JavaScript to Documents" free-tier Y. GRADE: TRUE. |
| JavaScript | Per-document JS options, JS console, document-level JS, util.expandMacros (10.6); per-document user consent for JS (10.4) | Free | N | Changelogs. GRADE: TRUE. |
| XFA | Render/dynamic XFA forms (v11 fixes dynamic XFA crashes; date-after-2030 handling) | Free | N | Changelog 11.0.1. XFA render support implied. GRADE: MOSTLY_TRUE. |
| Form data I/O | Export to FDF; import/export with options (10.5); populate from CSV; email form data; rename duplicated fields; fields pane | Mixed (FDF export free; CSV populate paid) | N | Matrix + catalog. GRADE: TRUE. |
| Flatten forms | Via Save-as-Optimized flatten fields+comments (v11 enhancement) | Editor+ (licensed) | N | v11 product page. GRADE: MOSTLY_TRUE. |

## 5. Comments & markup

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Annotation set | 23 types: highlight/underline/strikeout, sticky note, text box, callout, caret, cloud, file attachment, line/arrow, shapes, typewriter, stamps, audio, RichMedia video/sound, links | Free (21 of 23) | N | Catalog "(23)(21)(2)". GRADE: TRUE. |
| Measurement annotations | Distance/perimeter/area, calibration, unit precision setting (10.8), line→distance and polygon→area conversions (10.0), rectangle→measurement (10.0), export measurements CSV, manage-measurements dialog w/ import/export+filter (10.5) | Free (mostly) | N | Changelogs. GRADE: TRUE. |
| Highlight Area + Free Highlight tools | Page-content highlighting beyond text (10.4) | Free | N | Changelog. GRADE: TRUE. |
| Comments pane | Redesigned 10.2: inline text edit, filter/find, replies, lock/unlock; search in replies (10.2); group by status (10.0); group/sort by color (10.3); toggle all popups (10.3); hidden-layer comment filter (10.7) | Free | N | Changelogs. GRADE: TRUE. |
| Comment summary | Summarize to PDF w/ font customization, export comments to data file, export selected comments | Free (mostly) | N | Catalog. GRADE: TRUE. |
| Stamps | Custom stamps (v10.0), dynamic stamps + JS console authoring, edit dynamic stamps in palette (10.6), stamp size memory (10.1), localize default-stamp fonts | Custom stamps free; dynamic authoring Plus | N | Matrix row 55: "Create/Edit Dynamic Stamps — Using Editor Plus". GRADE: TRUE. |
| Eraser tool | Erases freehand ink only | Free | N | Catalog. GRADE: TRUE. |
| Markup review history | Show review histories per annotation (10.7) | Free | N | Changelog. GRADE: TRUE. |
| Thin lines, fit-box-by-text, paragraph props | Display/quality niceties | Free | N | Catalog. GRADE: TRUE. |

## 6. Redaction

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Redact Documents (mark + apply) | Redaction marks w/ preview then excision | Editor+ (Free shows -, i.e., paid-only) | N | Matrix row 133. GRADE: TRUE. |
| Find and Redact | Search-term driven marking; font size options for redaction labels (10.3) | Editor+ | N | Matrix row 90 + changelog. GRADE: TRUE. |
| Redaction Code Sets | Quick-pick code lists (FOIA/Privacy-Act style), code list editor + import/export (10.8) | Editor+ | N | Changelog 10.8.0. GRADE: TRUE. |
| Overlay-style redaction | Blurred or pixelated overlay redaction (listed under Edit Documents; v10.x-era addition) | Editor+ | N | Catalog. NOTE: blur/pixelate "redaction" is destructive-to-verify — cosmetic overlay, not excision. GRADE: MOSTLY_TRUE (feature exists; security semantics unclear). |
| Redact form field content | Apply redactions to form-field content (10.1) | Editor+ | N | Changelog. GRADE: TRUE. |
| Sanitize Documents | Strip metadata/hidden data | Editor+ (Free -) | N | Matrix row 138. GRADE: TRUE. |
| Regex/pattern auto-redaction | NOT FOUND as a user feature | — | — | Tracker has no advertised email/SSN pattern-redaction presets (searches + catalog). GAP. GRADE: PARTLY_TRUE (absence claim — negative evidence). |

## 7. Security & encryption

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Encryption | 40/128-bit RC4 + 128/256-bit AES | Free (apply? read) / paid apply | N | Matrix row 124. GRADE: TRUE (read support free; apply path paid — exact split UNVERIFIABLE). |
| Permissions | Restrict print/copy/edit/extract; password strength indicator | Same | N | Matrix + catalog. GRADE: TRUE. |
| Security policies | Create/import/export reusable policy sets | Free-base Y | N | Matrix row 83. GRADE: TRUE. |
| Azure Purview labels | Apply/change/remove sensitivity labels (10.4) | Editor | **Y (Microsoft cloud)** | Changelog. Anti-feature for offline use. GRADE: TRUE. |
| FileOpen security | Open FileOpen-protected docs | Free | N (FileOpen does its own network) | Matrix row 89. GRADE: TRUE. |
| PDF/A conformance output | ISO PDF/A 1a/1b/2a/2b/2u/3a/3b/3u | Editor (Free Y per matrix row 102 — free w/ watermark) | N | Matrix. Wider level set than GlyphPDF's 1B/2B/2U/3B/3U (adds the 'a' accessible variants). GRADE: TRUE. |
| PDF/X output | Convert to PDF/X | Editor | N | Catalog. GRADE: TRUE (which PDF/X flavors UNVERIFIABLE). |

## 8. Signatures (digital certs, self-sign)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Digital signatures (cert-based) | Sign w/ Windows cert store; Digital IDs Manager to organize certs from multiple sources/registers + create new (10.7) | Free (validate) / Editor (apply) | N (activation separate) | Matrix row 71 + changelog 10.7. GRADE: TRUE. |
| Certify documents | Certify w/o visible signature (catalog) | Editor | N | Matrix row 34. GRADE: TRUE. |
| Timestamp (TSA) | Timestamp documents; sign-save automation | Editor | N (TSA is an external service by definition) | Matrix row 150. GRADE: TRUE. |
| LTV | Long-Term Validation embedding | Editor | N | Catalog. GRADE: TRUE. |
| Non-CryptoAPI signing path | Compose signatures without Windows CryptoAPI (10.5) | Editor | N | Changelog. GRADE: TRUE. |
| Self-signed certificates | Create in-app; Group Policy to block creation (10.1) + GPO for cert-files usage (10.6) | Editor | N | Changelogs. GRADE: TRUE. |
| Sign on multiple pages | One signature placement across page ranges (10.3); signatures to multiple pages | Editor | N | Changelog. GRADE: TRUE. |
| Appearance | Signature templates; webcam/phone image capture (DroidCam etc.) for signature images (10.0); export/import signatures+initials w/ merge (10.6); type-your-signature; custom fonts | Editor | N | Changelogs. GRADE: TRUE. |
| PAdES baseline conformance (B-B/B-T/B-LT/B-LTA labels) | Not advertised as PAdES-conformant anywhere found | — | — | LTV+timestamp present (functionally near B-LT), but no PAdES profile claim. GRADE: UNVERIFIABLE. |
| FreeSign Tool | New v11 quick sign/initials tool | Free | N | Product page v11 list. No cloud evidence found. GRADE: MOSTLY_TRUE. |
| Placeholder Tool | Mark where signatures/initials go (v11) | Plus | N | Product page. GRADE: TRUE. |
| DocuSign integration | Send to DocuSign | Editor | **Y** | Matrix row 73. Anti-feature. GRADE: TRUE. |

## 9. Compare

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Compare Documents | Two-doc compare, RibbonUI > Review (added v9.2.359, Nov 2021) | **Plus/Pro only** (Editor column: "Using Editor Plus"; Free -) | N | Changelog + matrix row 41. GRADE: TRUE. |
| Annotation changes in compare | Compare includes comment/annotation deltas (10.0) | Plus | N | Changelog. GlyphPDF diff is content/structural only — gap. GRADE: TRUE. |
| Diff filtering | CaseSensitive/WholeWord/RegExp filters in CompareDocs:Diffs pane (9.5); Formatting category toggle (9.5) | Plus | N | Changelogs. GRADE: TRUE. |
| UX | Scroll-sync of left/right views (9.5 improvements), drag-drop files into dialog (10.0) | Plus | N | Changelogs. GRADE: TRUE. |
| Compare report export | Not found as an export-to-file feature | — | — | No evidence of HTML/text report output. GAP vs GlyphPDF reports. GRADE: PARTLY_TRUE (negative evidence). |
| Page reorder/middle-insert alignment handling | No documentation found on alignment algorithm | — | — | UNVERIFIABLE. |

## 10. Batch / actions (sequences)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Watched folders (hot folder) | "Watched Folders: Set Custom Action for Files Added to Specified Folders"; Create Folder Monitors | **PDF-Tools / PRO only** — NOT in Editor | N | Matrix rows 60/158. GlyphPDF's in-Editor hot folder exceeds Editor tier. GRADE: TRUE. |
| Batch conversion | PDF-Tools / Printer Standard | Tools/Printer/Pro | N | Matrix row 31. GRADE: TRUE. |
| Customized tools (user action sequences) | Build custom tools from operations; job profiles + saved settings | PDF-Tools (+ job profiles in Editor per row 64) | N | Matrix rows 52/64. GRADE: TRUE. |
| Batch Link | Batch hyperlink creation (10.8) | Editor | N | Changelog. GRADE: TRUE. |
| Command line | Editor supports CLI options (row 40); Tools/Printer richer automation | Free-base Y | N | Matrix. GRADE: TRUE. |
| Email automation | Send automated emails SMTP/MAPI | Printer Standard/Tools/Pro | N (SMTP) | Matrix row 143. GRADE: TRUE. |
| File-name rules via macro/JS | User-defined naming control | Tools/Printer | N | Matrix row 154. GRADE: TRUE. |

## 11. Print production (preflight, N-up, booklet)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| N-up, booklet, brochure printing | Multi-pages-per-sheet; don't-upscale option (10.5) | Free/Editor (printer drivers add more) | N | Matrix row 115. GRADE: TRUE. |
| Print-appearance warning | Warns when optional content/annot flags/JS may print differently than displayed (10.8) | Editor | N | Changelog. Nice honesty feature. GRADE: TRUE. |
| Printer-driver isolation | Isolate drivers from app process for stability (10.6) | Editor | N | Changelog. GRADE: TRUE. |
| PDF printer products | Lite (free) / Standard $58: 2400 DPI, JBIG2/JPEG2000/CCITT compression, H/F, overlays, letter/append/prepend modes, Office add-in, TOC-link conversion | Separate products (in PRO bundle) | N | Matrix. GRADE: TRUE. |
| Preflight (Acrobat-style profile checker) | NOT present | — | — | Only PDF/UA accessibility checker + PDF/X conversion exist; no general preflight rule-profile engine found. GAP. GRADE: PARTLY_TRUE (negative evidence, 3 searches). |
| Job profiles / paper settings / mirrored print | Custom forms, scaling | Printer Standard | N | Matrix. GRADE: TRUE. |

## 12. Accessibility / tagging

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Tags structure editing | Create/modify tag structure (v9.0); Tags Pane create/view/edit (v11 catalog) | Free | N | Changelog v9.0 + catalog. GRADE: TRUE. |
| Accessibility checker + report | PDF/UA checker (matrix), document accessibility check + report (v9.0) | Free | N | Matrix row 3 + changelog. GRADE: TRUE. |
| Reading order | Determine reading order of documents (v11 item) | Free | N | Catalog. GRADE: MOSTLY_TRUE (v11 catalog; manual depth unverified). |
| Alt text | Set alternate text for content | Free | N | Catalog. GRADE: TRUE. |
| Screen-reader/UI automation | UIA support for app UI + document content (v9.0) | Free | N | Changelog v9.0. GRADE: TRUE. |
| RTL UI + RTL page layout | Interface and layout modes | Free | N | Catalog. GRADE: TRUE. |

## 13. Import / export formats

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| Export PDF→Office | Word (bookmarks carried 10.5; page thumbnails 10.4), Excel, PPT; password-protect exported Office docs (10.5); OCR-aware (10.7) | Editor+ | N | Matrix rows 80-82 + changelogs. GRADE: TRUE. |
| Export PDF→text/images | Plain text, BMP/JPEG/TIFF etc., WebP (v11-era), snapshots | Editor+ | N | Matrix + catalog. GRADE: TRUE. |
| Import→PDF | Word/Excel/PPT/Publisher/Visio (via printers/add-in or direct), images, CSV table, Markdown, RTF, TXT, XPS, email .msg/.eml (10.2), clipboard, scan/TWAIN, webcam (10.0), web page URLs (Chromium-based converter, 10.0), blank new | Mixed; most paid | N (web-page fetch = network) | Catalog + changelogs. GRADE: TRUE. |
| PDF/A + PDF/X conversion | See §7 | Editor | N | GRADE: TRUE. |
| Metadata | Advanced metadata dialog, metadata templates, import/export metadata, XMP (TDM Reservation page 10.7) | Free | N | Catalog + changelog. GRADE: TRUE. |
| Links/bookmarks export | Export links to CSV (10.0); bookmarks to HTML/TXT | Editor+ (bookmarks export), free partial | N | Changelog. GRADE: TRUE. |
| Export/import settings | Full settings files, presets merge on import (10.4) | Free | N | Catalog. GRADE: TRUE. |

## 14. Cloud / services & telemetry — ANTI-FEATURE DOMAIN (cloud is a disqualifier for GlyphPDF)

| Feature | Sub-capabilities | Tier | Cloud? | Notes |
|---|---|---|---|---|
| License activation | v9+ licensing: keys must be **activated online on each new installation**; activation-count check against Tracker servers; key-activation **proxy app** for air-gapped networks (KB556); deregister machines via web account | All paid | **Y (mandatory for activation)** | KB554/556; Tracker staff forum post Feb 2025 confirms activation connect + activation counting. GRADE: TRUE. |
| Maintenance-gated updates | "Perpetual" licenses keep working at the last version available during maintenance; updates/new versions require active maintenance; RU/CN purchasers post-2022 are **blocked from site/updates** | All | **Y** | Tracker staff forum Feb 2025. r/pdf thread "Perpetual licenses that self-destruct" = PARTLY_TRUE (overstated: license persists at frozen version). |
| 2025-2026 licensing regressions | Auto-updates moved file paths and triggered **license-loss/watermark** reports for orgs; default-PDF-app breakage | All | Y (update/license servers) | r/sysadmin 1tedg2v (title + snippets: "users report Editor loses license and sta[rts watermarking]"). GRADE: MOSTLY_TRUE (thread content; full thread not fetchable). |
| In-app telemetry of document content | No evidence of document-content telemetry found in 6+ searches (changelog silent, privacy policy web-focused, staff claims local processing incl. OCR) | — | — | The oft-repeated "Tracker telemetry scandal" claim: **UNVERIFIABLE** — could not locate a primary source (no HN/gHacks/forum record surfaced). Documented phone-home = activation/updater only. Vendor staff statement Oct 2025: "processes files entirely on your device… no PII sent to external servers" = MOSTLY_TRUE (vendor self-claim). |
| Website analytics | Google Analytics, MixPanel, Singular on pdf-xchange.com (not the app) | — | Y (web only) | Privacy policy. GRADE: TRUE. |
| Cloud storage integrations | Google Drive, OneDrive (incl. Business), Dropbox, Box, SharePoint, WebDAV Places (10.2); **search files in cloud storage** (v11); document preview for WebDAV (10.3) | Free/Editor | **Y** | Matrix + catalog + KB490. GRADE: TRUE. |
| AI Assistant (v11) | Analyze/interact with PDF via **OpenAI/Anthropic/Google models; user supplies API key** — document content leaves the machine | Free (BYOK) | **Y** | Product page v11: "your data will not be processed by any AI models unless you provide an AI account API key". GRADE: TRUE. Core anti-feature; also a market signal. |
| Translate Documents | Catalog item; engine/provider not documented | — | **Presumed Y** | Catalog. GRADE: UNVERIFIABLE. |
| Updater | Auto-updater (disable via KB470); **Updater LPE CVE-2026-2040** fixed 10.7.3.401 | All | Y | Security bulletin. GRADE: TRUE. |

## 15. Version granularity — v9 → v10 → v10.8 (official build history)

- **v9.0.350 (Jan 2021):** accessibility wave (tag editing, a11y checker+report, UIA/screen-reader), layers overhaul (default layer, drag content to layers, export selected layers), Split-by-Size, Split-Pages-by-Guides, Crop-to-White-Margins, Recompress Images, text-engine upgrades (paragraph props, RTL, soft returns), 3D comments+measurements.
- **v9.2.359 (Nov 2021):** **Compare Documents introduced** (Plus/Pro).
- **v10.0.0 (Jun 2023):** custom stamps, Identify Forms, Web-Page-to-PDF converter, path/shape editing toolset, calculator/adding-tape, compare now includes annotation changes, webcam/phone image capture, export links to CSV, OCR language uninstall.
- **10.1 (Sep 2023):** Repair ToUnicode CMap, Audit Space Usage, merge layers, redact form-field content, GPO to block self-signed certs.
- **10.2 (Jan 2024):** comments pane redesign (inline edit/filter/replies), email (.msg/.eml)→PDF, WebDAV Places, page margins.
- **10.3 (Apr 2024):** **Find and Replace**, Normalize Pages, page labels via properties, Select Page Region tool, scan edge-detect+crop+de-warp, OCR skips logos/stamps, annotation color grouping in summaries.
- **10.4 (Sep 2024):** Highlight Area/Free Highlight, links from barcodes, **Azure Purview labels**, delete-empty-pages noise tolerance, per-document JS consent.
- **10.5 (Jan 2025):** **Dark Page Mode**, themes, signatures without CryptoAPI, Colorize/Remap/B&W, Office-export passwords, bookmarks→Word.
- **10.6 (May 2025):** replace images from clipboard, dynamic stamp editing, printer-driver isolation, util.expandMacros JS, GPO cert controls.
- **10.7 (Aug 2025):** **Digital IDs Manager**, multi-language ToUnicode/Office-converter OCR, non-contiguous text selection, layer visibility dependency, markup review histories.
- **10.8 (Dec 2025):** **redaction code sets (+import/export)**, partial cropping, reverse page order, Batch Link, Correct Scans (perspective), print-appearance warning, bookmarks from highlights.
- **v11.0.0 (May 2026):** **AI Assistant (BYOK cloud)**, FreeSign, Placeholder tool, BREP 3D, cloud-file search, new Open/Save dialogs — the new dialogs caused regressions; **11.0.1 (Jun 2026) reverted to system dialogs**; 10.8.6.411 (Sep 2026) remains the maintenance line for v10 (libheif/OpenSSL updates).

## 16. GlyphPDF delta (per domain; gap = PDF-XChange has it, offline-first workstation does not)

- Viewing & navigation: GAP — 3D PDF viewing/annotation, portfolios, session files, spreadsheet split, loupe-class nav tools.
- Editing: GAP — vector path-editing toolset, Repair ToUnicode CMap, Select Page Region, Normalize Pages, Find & Replace in content, Audit Space Usage.
- OCR: PARTIAL — parity on local engines + preprocessing (deskew/deskew-class), but gaps: in-app second commercial-grade engine (ABBYY — not buildable, note as benchmark), OCR-skip-logos heuristic, perspective "Correct Scans", OCR during Office export.
- Forms: GAP — date/image/barcode field types, XFA rendering, forms recognition, CSV populate UX; parity on auto-detect + calc + JS.
- Comments & markup: GAP — measurement suite (calibrate/CSV export), dynamic stamps authoring, Highlight Area/Free Highlight, review history display.
- Redaction: GAP — redaction code sets + import/export, blurred/pixelated overlay mode (copy as "visual-only, non-security" mode or skip), find-and-redact label sizing; parity on excision+sanitize; Tracker has NO pattern/regex preset redaction (our differentiator).
- Security & encryption: parity on AES-256/permissions/PDF/A (they add 1a/2a/3a accessible variants — GAP), security-policy import/export minor gap; Azure Purview = anti-feature, skip.
- Signatures: PARTIAL — parity on PAdES-style LTV/TSA (they don't even claim PAdES labels — our conformance is a differentiator); gaps: Digital IDs Manager UX, multi-page single signing, non-CryptoAPI signing, placeholder workflow.
- Compare: PARTIAL — their compare is Plus-only, no report export found (our HTML/text reports win), but they diff **annotations** too (gap); alignment internals undocumented.
- Batch/actions: GAP — user-defined tool sequences (custom tools), CLI on the editor itself, macro/JS file-naming rules; our hot-folder beats Editor-tier (theirs requires PDF-Tools).
- Print production: GAP — N-up/booklet printing, print-appearance-difference warning, driver isolation; no real preflight anywhere (opportunity).
- Accessibility/tagging: GAP (largest) — tag structure editing, PDF/UA checker+report, alt text, UIA; we have only a reading-order check.
- Import/export: GAP — direct Office export w/ OCR + bookmarks, email/web/markdown import, PDF/X, PDF/A "a"-levels.
- Cloud/telemetry: INTENTIONAL ABSENCE — AI Assistant (BYOK), DocuSign, cloud storage/search, mandatory activation phone-home, geo-blocking. Our zero-phone-home stance is the headline contrast; still needs an offline activation story superior to their proxy app.

## 17. Sources (spec-level; with per-area verdicts)

1. PDF-XChange Products Comparison Chart (official 160-row matrix, parsed 2026-09-07) — https://www.pdf-xchange.com/pdf-xchange-products-comparison-chart — tier mapping: TRUE.
2. PDF-XChange Editor full build history text export (v8→11.0.1) — https://www.pdf-xchange.com/product/pdf-xchange-editor/history (+ /history/download-text-file) — v9→v10.8 feature dating: TRUE.
3. PDF-XChange Editor product page feature catalog (categorized, free/paid counts, v11 notes incl. AI Assistant data policy) — https://www.pdf-xchange.com/product/pdf-xchange-editor — TRUE; v11 BREP/FreeSign detail MOSTLY_TRUE.
4. KB352 "How do I Manage OCR language packs" (Tesseract + ABBYY since v9) — https://www.pdf-xchange.com/knowledgebase/352-... — TRUE.
5. OCR language packs page (~100 variants, legacy listing; v8+ via in-app updater) — https://www.pdf-xchange.com/pdf-xchange-ocr — MOSTLY_TRUE for current counts.
6. KB554/556 (v9 licensing setup; key-activation proxy for offline networks) — https://www.pdf-xchange.com/knowledgebase/554-... , /556-... — activation phone-home: TRUE.
7. Tracker staff forum statements: license activation/geo-block thread (t=45495, Feb 2025); local-processing/OCR claim (t=47928, Oct 2025) — forum.pdf-xchange.com — TRUE (as vendor statements), local-processing claim MOSTLY_TRUE.
8. Security bulletins page (CVE-2025-58113, CVE-2025-64085/64086, CVE-2026-2040 updater LPE, JBIG2 fix 10.0.0, COM hijacking 10.8.3) — https://www.pdf-xchange.com/support/security-bulletins.html — TRUE.
9. Privacy policy (GA/MixPanel/Singular web analytics; Google Drive plugin data) — https://www.pdf-xchange.com/privacy-policy — TRUE (web-scoped).
10. r/sysadmin 1tedg2v "If you have PDF-XChange Editor, please be careful with their new…" (license-loss/watermark regressions) — MOSTLY_TRUE (snippets; thread unfetchable). r/pdf 1qjszmv "perpetual licenses self-destruct" — PARTLY_TRUE (overstated vs KB554 reality).
11. Sentiment: Reddit r/software 10sf8bi ("most powerful free PDF tool… only option if you need to OCR" — MOSTLY_TRUE as sentiment quote), 1lfwgpd, r/pdf 1unjz9m; G2 vendor 4.6/5 (148 reviews, g2.com/sellers/pdf-xchange-co-ltd-tracker-software) and Editor 4.3/5 (38 reviews, per third-party roundup) — MOSTLY_TRUE; Capterra 170838 (dated-interface criticism) — MOSTLY_TRUE; PCWorld review (free tier watermark caveat) — TRUE-consistent with vendor wording.
12. "Tracker telemetry controversy" (as briefed): UNVERIFIABLE — no primary record found across 6 targeted queries (HN, gHacks-class press, Tracker forum, changelog). Documented reality = activation/update phone-home + v9 licensing friction + v11 cloud additions. Graded accordingly and not repeated as fact in the tables above.
