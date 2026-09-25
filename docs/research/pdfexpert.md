# Research Report: PDF Expert (Readdle) — Deep-Dive Spec Sheet for GlyphPDF

**Date:** 2026-09-07
**Requested by:** GlyphPDF parity program (research-specialist protocol)
**Research question:** What does PDF Expert (Readdle) ship per domain, what are its failure modes and loved workflows, and where are the feature gaps vs GlyphPDF — so GlyphPDF can rank build priorities while staying offline-first/local-only?
**Method:** 12 distinct web searches; 30+ primary-source page extractions via curl (pdfexpert.com home/pricing/all 12 Mac feature pages/iOS features/Copilot; the complete Readdle help-center article set for PDF Expert — redaction, OCR, forms, signing, passwords, conversion, measurement, smart search, annotation summary, licensing, business FAQ, trust page; Macworld full review). Reddit/forum sentiment via search snippets (thread bodies blocked to bots). Wikipedia/cross-checks for ownership.
**Benchmark baseline:** GlyphPDF ledger `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` (v1.3.1 status §27, roadmap §28).

---

## CRITICAL PREMISE CORRECTIONS (read first)

The tasking assumed "PDF Expert (Readdle, now part of All-Arrow; Windows release 2024+)". Both premises FAIL verification:

1. **There is NO Windows release. Never was.** Readdle's live help center (fetched 2026-09-07): *"PDF Expert is not available for Windows for now. Currently, we are not planning to develop a Windows version of the app as our developers are focused on the improvements for iOS and macOS versions."* `pdfexpert.com/windows` returns 404. There is no Microsoft Store listing (the "PDF Reader Pro — Editing Expert" Store app is a different vendor). Macworld (2026) and TechRadar reviews independently list "unavailable on Android and Windows" as a top con. PDF Expert's ONLY Windows-facing surface is a browser page (copilot.pdfexpert.com) marketed as: *"PDF Expert isn't available on Windows — but you can still work smarter with PDFs directly in your browser."* **Verdict on "Windows release 2024+": FALSE.**
   - Implication: the "Windows-port maturity complaints" research focus is moot — there is no port to complain about. All "Platform" columns below read **Mac / iOS / Web**; Windows is N/A everywhere.
2. **No "All-Arrow" ownership exists.** No M&A record found in any news, tracker, or Readdle's own properties. Readdle's home page (fetched today): *"PDF Expert is a part of Readdle, a privately held company behind popular productivity apps such as Spark email, Scanner Pro, Documents, and Calendars… 194 million people worldwide downloaded our apps."* Readdle's trust page self-describes as an independent company with ISO/IEC 27001:2022 certification on Google Cloud infrastructure. **Verdict on "part of All-Arrow": FALSE** (no such entity found; contrary primary evidence of private independence as of 2026-09-07).

**Strategic consequence:** the design-first competitor concedes the ENTIRE Windows market. GlyphPDF has no direct Windows threat from PDF Expert; its value is as a UX benchmark and a donor of copyable patterns — and its omissions (compare, batch, PAdES, PDF/A, forms creation, redaction safety) are exactly GlyphPDF's build list.

---

## Product Snapshot (verified state, Sept 2026)

| Dimension | State | Verdict |
|---|---|---|
| Platforms | macOS (PDF Expert 3), iOS/iPadOS (iPhone/iPad/Vision Pro), browser (Copilot web only). No Windows, no Android | TRUE (help center + site + Macworld/TechRadar) |
| Current Mac version line | PDF Expert 3 (account-based, no license codes); PDF Expert 2 (legacy, license code, Mac-only, still activatable in PE3 for PE2-level features) | TRUE (official "PE2 vs PE3" article) |
| Pricing (official page, 2026-09-07) | Essential $49.99/yr ($0.96/wk billed annually) · Pro $79.99/yr ("best value") · Lifetime $139.99 one-time (Mac only) · 30% education discount on annual · 7-day free trial (no payment details per Macworld) · 30-day money-back guarantee | TRUE (official pricing page; Macworld corroborates $79.99/$139.99 + student/educator discount) |
| Ownership | Readdle, privately held, independent; products: PDF Expert, Spark, Documents, Scanner Pro, Calendars, Fluix (enterprise) | TRUE (official site) |
| Positioning | "The go-to PDF editor for iPhone, iPad and Mac"; design-first, Apple-ecosystem-native; Apple design award recognition ("App of the Year Runner-up by Apple", Red Dot 2023/2025 mentions across Readdle) | TRUE (official; awards framing as marketed) |
| Store ratings | ~4.5 Mac App Store (TechRadar), ~4.6 App Store/Capterra in aggregator snippets | MOSTLY_TRUE (reviewer-cited numbers; spot values drift over time) |
| Services posture | Cloud where it matters: Readdle account, Copilot (OpenAI via Readdle servers), cloud-storage sync, Business Manager (Stripe). Core editing/OCR/conversion marketed as on-device | TRUE for services; MOSTLY_TRUE for "conversion is fully on-device" (official RTF page contrasts itself with cloud converters; third-party corroboration; depth undocumented) |

---

## 1. SPEC SHEET — one row per feature

Legend: **Sub** = requires Premium subscription (Essential/Pro tier); **Cloud** = requires Readdle/network service. All rows: **Windows = N/A (no port exists)**. Platform column lists Mac / iOS / Web only.

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes (real behavior, limits, quirks) |
|---|---|---|---|---|
| Fast rendering | Smooth scrolling, instant open ("2000-page reports"), fluid zoom | Mac, iOS | N (free tier) | Speed is the brand's #1 praised trait (Macworld "well-optimized and runs swiftly") — verdict TRUE |
| Page layouts | Single page, Two-page view, horizontal or vertical scrolling, zoom-fit modes | Mac, iOS | N (basic); layout options all free | Help-center documented; two-page view a separate article |
| Split View | Two PDFs side-by-side simultaneously | Mac, iOS (iPad) | N | Marketed for translate/compare-by-eye; NOT an automated document comparison |
| Themes | Day, Sepia, Night, Auto (follows system); Dark Mode app-wide | Mac, iOS | N | iPad reading panel adds brightness slider, keep-awake, crop-mode toggle |
| iPhone Reading Mode | Reflows PDF text to single column; font-size control; brightness; themes; non-destructive ("temporary new view") | iOS (iPhone) | N | Tables/illustrations adapt to screen; loved accessibility-adjacent feature. GlyphPDF has no reflow equivalent (desktop product) |
| Read text out loud (TTS) | System speech of PDF text | Mac, iOS | N | Single help article; no voice/library controls documented |
| Tabs | Multiple open documents in tabs; Mac search can span ALL tabs | Mac, iOS (iPad tab bar) | N | Search-across-tabs is Mac-only |
| Smart Search | Match Case, Whole Words, search history, results list with jump-to | Mac, iOS | N | NO regex, NO wildcards; GlyphPDF's planned regex search exceeds it |
| Navigation | Thumbnails, bookmarks, outlines/TOC (create/edit), jump-to-page, page-number indicator, annotation browsing | Mac, iOS | N | Outline editing supported (create outlines) |
| Presentation Mode | Full-screen present; FaceTime co-presenting gimmick | Mac | N | Niche |
| Reading widgets/keep-awake | Keep device awake; crop headers/footers in reading UI | iOS | N | |
| Default PDF viewer | Register as system default (replaces Preview) | Mac | N | Part of the "replace Preview" wedge strategy |
| URL scheme automation | `PDFE` bookmarklet (Safari→app), `pdfefile:///` URI open-in-app | iOS | N | Lightweight power-user automation; no CLI/batch scripting |

### 1.2 Editing (text / objects / pages)

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Direct text editing | Edit text "just as in a Word document": fix typos, add paragraphs, font/size/color matching | Mac (strongest), iOS (edit text) | Sub | Flagship differentiator; the most-loved editing workflow in reviews — TRUE |
| Image editing | Add, replace, resize images; logos/graphs | Mac, iOS | Sub | |
| Link editing | Web links + to-page links; links on images | Mac, iOS | Sub | Help-center article covers both types |
| Redact tool | See §1.6 | Mac, iOS | Sub | |
| Organize pages | Thumbnails view: delete, insert (blank pages, append file), drag-drop reorder, rotate, extract selection as new file, copy/paste pages | Mac, iOS | Sub (page mgmt listed as Premium on iOS) | All thumbnail-driven; two-click deletes |
| Merge files | File > Merge Files; drag-drop; merge selected pages from multiple files; unlimited count | Mac, iOS | Sub | Merge-then-OCR is the ONLY multi-document OCR path (batch surrogate) |
| Split/extract | New document from selected pages; save each selected page as separate file | Mac, iOS | Sub | No range-segment naming conventions; simpler than GlyphPDF's `_part{n}` scheme |
| Page numbers, headers/footers, Bates | Add page numbers, Bates numbers, custom text | Mac | Sub | Bates exists (GlyphPDF parity confirmed); iOS not documented for Bates |
| Page resize | Page Resize tool | Mac, iOS | Sub | Article exists; GlyphPDF has resize per PRD §9.9 |
| Create new PDF | Blank PDF; From Image; From File (Office/iWork/images); scan | Mac, iOS | N (create free) / Sub for Office import | Mac to-PDF conversion of Office files REQUIRES Word or Pages installed (piggybacks macOS apps) — notable quirk, TRUE (help center) |
| Convert PDF to black & white | Convert document colors | Mac | Sub | Single article |
| Flatten | "Export as Flattened" — read-only signature and form fields | Mac, iOS | N | Whole-file flatten on save; annotation flatten implications (see §1.6 autosave risk) |

### 1.3 OCR

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Text recognition (OCR) | Scan & OCR tab; Recognize Text; searchable text layer enabling search/highlight/copy | **Mac 3.0+ ONLY** | Sub (PE3-exclusive premium feature) | Official help center: iOS OCR not shipped — *"our team is already working on adding this functionality"*. Marketing home page implies iOS OCR; help center contradicts — MOSTLY_TRUE on platform spread |
| Languages | ~20 languages; auto-detect main language; manual override/add multiple | Mac | Sub | Official FAQ says "choose among 20 different"; marketing says 20+ — TRUE |
| Page scope | All pages / current page / selected page ranges | Mac | Sub | |
| OCR review (typo preview) | Orange frames flag suspected OCR typos BEFORE apply; click to compare picture vs recognized text; edit + Correct; navigation between flags | Mac | Sub | Human-in-the-loop preview BEFORE committing — validates GlyphPDF's OCR Verify pattern; PDF Expert's review is pre-apply, GlyphPDF's is post-verify with save |
| Scan preprocessing | Enhance (AI: fix distortion, remove shadows, improve contrast), custom page size, unify paper format, fill margins, crop/split scan pages | Mac, iOS (Enhance) | Sub | "Enhance" is AI-marketed scan beautification; GlyphPDF's deskew/binarize/denoise/deskew-preprocessing is the honest-engineering equivalent |
| OCR input formats | PDF, JPEG, PNG (plus Word via import) | Mac | Sub | |
| Multi-document OCR | NO true batch: merge files first, then OCR the merged result | Mac | Sub | Official FAQ's own workaround — confirms no batch OCR. GlyphPDF batch-OCR is a clean gap-closer |

### 1.4 Forms

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| AcroForm fill | Text fields; checkbox; radio; numeric fields with auto-calculation; dropdowns; date/time fields; signature fields (My Signature saved vs Customer Signature one-time) | Mac, iOS | N for basic fill / Sub for some tools | iOS documents MORE field types (dropdowns, date/time) than Mac article; calculation fields both |
| Static XFA fill | Fill static XFA (LiveCycle) forms | Mac, iOS | N | **Dynamic XFA NOT supported** (official, both platforms) — TRUE |
| Flat-form filling | Type onto non-interactive forms via Text tool; Pen-drawn checkboxes | Mac, iOS | N | |
| Highlight form fields | Colored background on interactive fields (preference toggle) | Mac | N | |
| AcroForm JavaScript | Supported scripting subset documented (for-developers section) | Mac, iOS | N | Two articles: AcroForms + XFA scripting support — deeper than most viewers |
| Form FIELD CREATION | **NOT AVAILABLE** — no form-field authoring tool anywhere in official docs/marketing | — | — | "Make a fillable PDF" marketing phrasing = filling guidance only; FAQ: flat forms filled with Text tool. **Verdict: MOSTLY_TRUE that creation is absent** (absence consistent across all primary sources). GlyphPDF's AcroForm editor + auto-detect is a full tier above |
| Form data import/export | **NOT AVAILABLE** (no FDF/XFDF/CSV form-data tooling documented) | — | — | GlyphPDF CSV/FDF data export is a gap-closer |
| Form flattening | Export as Flattened (read-only fields) | Mac, iOS | N | |

### 1.5 Comments & markup

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Text markup | Highlight, underline, strikethrough; custom color palette, color-coding | Mac, iOS | N basic / richer sets Sub | |
| Notes | Sticky notes (pop-up), text boxes, typed comments anywhere | Mac, iOS | N/Sub | |
| Drawing | Pen with color/opacity/line-width; Eraser; Apple Pencil on iPad; palm rejection era UX | Mac, iOS | Sub | |
| Shapes | Arrows, circles, rectangles, polygons/polylines (construction section) | Mac, iOS | Sub | Polygon/polyline documented under construction tools |
| Stamps | Library ("Approved", "Void", "Confidential") + custom stamps | Mac, iOS | Sub | |
| Stickers | Fun sticker set (iOS-oriented) | iOS | Sub | Design-first flourish |
| Audio notes | Record voice annotations in-document | Mac, iOS | Sub | GlyphPDF lacks audio annotations |
| Annotation management | View/search/remove annotations list; Manage annotated pages; Content Selection Tool | Mac, iOS | N | |
| Annotation Summary export | Mac: export all annotations as **HTML, Text, or Markdown**; iOS: HTML + "Annotated Pages" PDF of only-annotated pages | Mac, iOS | Sub | Limits: freehand (Pen) annotations and signatures NOT included in export — TRUE (official). GlyphPDF's comments CSV export vs their HTML/MD — different shapes, both partial |
| Translation | Select text → Translate (Copilot-powered); Translation Tool article | Mac, iOS | **Cloud + Sub** | Copilot pipeline; offline impossible by design |
| PDF Copilot (AI) | Summarize doc or annotation set; List Main Points with page references + in-text highlight; Ask-PDF with cited pages; Explain (right-click); Translate; quiz generation; suggested questions | Mac 13+, iOS 16+ | **Cloud + Sub** (OpenAI via Readdle servers; pseudonymized on their backend) | Official privacy FAQ: content shared with OpenAI, not used for training, GDPR/DPA, deletion via dpo@readdle.com. Anti-feature for GlyphPDF — but its UX (page-anchored answers) is the benchmark a LOCAL Copilot should copy |

### 1.6 Redaction

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Manual redaction | Edit tab > Redact; select fragment; **Blackout** (remove content + black box) or **Erase** (remove content + empty space) | Mac, iOS | Sub | Official: "removes the selected content"; "no possibility to restore the redacted text after saving" — permanent-removal claim TRUE; content-stream-level excision rigor UNVERIFIABLE (not documented) |
| Search-based redaction | Search field in Redact sidebar; review hits; Erase one-by-one or **Erase All** for query across document | Mac | Sub | Search-term redaction exists (like GlyphPDF pattern redaction without regex/presets/word-lists) |
| iOS redaction | Edit PDF > Redact; tap text; Blackout (default) / Erase via long-press on tool | iOS | Sub | |
| **Autosave hazard** | "The app automatically permanently saves changes if you close the file or switch to another tab" — redaction committed with NO preview/burn step and no restore | Mac, iOS | — | **Officially documented failure mode.** No preview-before-commit, no transaction. Direct marketing contrast for GlyphPDF's preview-before-burn + SafeSave transactions — TRUE (official note) |
| Sanitize/metadata | No separate sanitize-metadata bundle documented under redaction | — | — | GlyphPDF's sanitize-on-save default-ON bundle exceeds it (as far as documented) |

### 1.7 Security & encryption

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Password protection | Set Password (File menu / More menu); open-password | Mac, iOS | N | "Standard Apple's encryption" (CGPDF-based); open password documented — NO separate owner/permissions password documented (restrict-print/copy not offered) |
| Change/remove password | After unlock; **only if password was originally added in PDF Expert** | Mac, iOS | N | Documented quirk/limitation |
| Password recovery | None — "you won't be able to recover your password" | Mac, iOS | N | |
| Certificate encryption / MIP | NOT AVAILABLE | — | — | No AIP/Purview/certificate security (contrast Nitro enterprise) |
| Watermarks | Not a documented dedicated watermark tool (image/text placement possible via editing) | Mac | Sub | Absence MOSTLY_TRUE; weaker than GlyphPDF §9.11 |
| Trust posture | Readdle: GDPR, ISO/IEC 27001:2022, data encrypted in transit/at rest, Google Cloud infra, Trust Center | Services | — | TRUE (official trust page); applies to cloud services, not file processing |

### 1.8 Signatures

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Create signatures | Keyboard-type (stylized), Trackpad-draw (Mac), Image upload with white-to-transparent conversion + tolerance slider; Apple Pencil/finger draw (iOS) | Mac, iOS | N to place; saved sigs sync with account | |
| Multiple saved signatures | Save several; reuse; choose color; delete; sync across Mac/iPad/iPhone via account | Mac, iOS | Cloud (account sync) | "Create on iPad with Pencil, use on Mac" is a loved continuity workflow |
| Customer Signature (kiosk) | One-time signature not saved, can't be copied to other docs — for collecting a signer-in-person | Mac, iOS | N | PDF Expert's answer to "collect signatures" — in-person, NOT remote |
| Signature placement | Click to place; right-click context menus; resize/move; one signature multiple times; remove from document | Mac, iOS | N | |
| Interactive form signing | Tap signature field to place My/Customer Signature | iOS | N | |
| **Cryptographic / digital signatures (PAdES, certificates)** | **NOT AVAILABLE.** Official FAQ: "PDF Expert provides basic electronic signature capabilities"; explicitly contrasts e-sig vs digital sig and ships only the former; validity disclaimer recommends consulting a lawyer | — | — | TRUE. No signing order, no send-for-signing, no audit trail, no OCSP/LTV. GlyphPDF's PAdES B-LT/B-LTA stack is a full professional tier PDF Expert lacks entirely |
| Signature validity display | Not applicable (no cryptographic sigs verified) | — | — | |

### 1.9 Compare

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Document comparison | **NOT AVAILABLE** | — | — | No compare feature in marketing, help center (no article), or reviews; reviewers point users to Acrobat/draftable for diffing. Split-view side-by-side is manual eyeballing only. **Verdict MOSTLY_TRUE** (consistent absence across all primary sources + reviewer behavior). GlyphPDF's fingerprint-aligned structural+text compare is a category PDF Expert does not play in |

### 1.10 Batch

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Batch conversion to PDF | iOS: multi-select files > Convert to PDF / Merge to PDF | iOS | Sub | The only documented multi-file operation |
| Batch OCR/compress/watermark/redact | **NOT AVAILABLE** (OCR batch = merge-first workaround; no batch tool UI) | — | — | MOSTLY_TRUE absence. GlyphPDF batch + hot-folder has no PDF Expert counterpart |
| Watched folders / automation | None in-app; URL schemes (iOS) and Share-sheet integrations are the only hooks; enterprise sibling Fluix (separate product) does workflow automation for businesses | — | — | Readdle routes automation demand to Fluix, not PDF Expert |

### 1.11 Print production

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Measure tab (construction kit) | **Calibrate** with intelligent scale auto-detection from drawing (or manual/known-dimension); Distance, Area, Perimeter; units mm/m/in/ft; move/resize/rotate measurement objects | Mac, iOS | Sub | Documented under "construction tools"; Macworld praises measuring. **GlyphPDF lacks any measurement tool** — its clearest single-feature gap |
| Redline tools | Construction redlining (markup for plan review) | Mac, iOS | Sub | |
| Polygon/polyline shapes | Drawing precision shapes | Mac, iOS | Sub | |
| Preflight / PDF/A / imposition / printer's marks | **NOT AVAILABLE** — no preflight, no PDF/A export (not mentioned anywhere), no print-production tooling | — | — | MOSTLY_TRUE absence. GlyphPDF PDF/A-1B/2B/2U/3B/3U exceeds it |
| Printing | Standard print + documented printing troubleshooting (Mac) | Mac | N | |

### 1.12 Accessibility / tagging

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Tagged-PDF authoring/validation | **NOT AVAILABLE** — no tag editor, no accessibility checker, no reading-order tool documented | — | — | MOSTLY_TRUE absence. GlyphPDF reading-order check (§9.14) is ahead |
| Reading aids | Reflow Reading Mode (iPhone), TTS read-aloud, themes, zoom modes — serving reading accessibility via UX rather than PDF/UA compliance | Mac, iOS | N | Design-first substitute for true accessibility |
| Keyboard support | Keyboard shortcuts article (Mac); full keyboard access idioms | Mac | N | |

### 1.13 Import / export

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| Export PDF → Word | .docx (Word 2007) and .doc (Word 97) options | Mac, iOS | Sub (iOS explicitly subscription-gated) | Export tab; multi-page OK |
| Export PDF → Excel / PowerPoint / Text | .xlsx/.pptx/.txt | Mac, iOS | Sub | |
| Export PDF → Image | PNG or JPG; one image per page | Mac, iOS | Sub | |
| Convert to PDF | Word, Excel, PowerPoint, Pages/Numbers/Keynote (iWork), JPG/PNG; Mac REQUIRES Word/Pages installed for Office import | Mac, iOS | N create / Sub advanced | HTML→PDF via Safari share sheet (iOS) |
| Compression | File > Reduce File Size; **Document Quality presets: Lossless / High / Medium / Low** with guidance (print vs email) | Mac, iOS | N | Preset-based; no measured before/after readout documented (GlyphPDF's §9.13-a delta readout exceeds); no MRC claims |
| Page selection on export | Convert selected pages/ranges only | Mac | Sub | |
| Preserve on save | Bookmarks/annotations persist on ordinary save; no explicit round-trip contract published (GlyphPDF's §9.16-a characterization pins what PDF Expert leaves implicit) | Mac, iOS | N | |
| Export targets MISSING | No HTML/CSV/Markdown document export, no EPUB, no OOXML-native pipeline claims | — | — | GlyphPDF conversion surface is wider |

### 1.14 Cloud & services (ANTI-FEATURES for GlyphPDF — marked separately)

| Feature | Sub-capabilities | Platform | Cloud/subscription? | Notes |
|---|---|---|---|---|
| **Readdle account** | One account across Mac+iOS; required for cross-platform subscription, signature sync, Copilot | All | **Cloud (account required for cross-platform)** | Anti-feature: account-gated licensing |
| **PDF Copilot** | AI chat/summarize/translate/explain/quiz; content sent to OpenAI **via Readdle servers**; pseudonymization on their backend; no model training; GDPR/DPA; deletion on request | Mac, iOS, Web | **Cloud + Sub** | Anti-feature for GlyphPDF; NOTE the UX is the benchmark — page-anchored, annotation-aware, suggested questions. A local (Ollama/ONNX) equivalent inside GlyphPDF's existing AI lane would match the UX with zero egress |
| **Web Copilot for Windows** | copilot.pdfexpert.com — the only PDF Expert surface a Windows user can touch | Web | **Cloud + Sub** | Explicitly positioned: "PDF Expert isn't available on Windows — but you can still work smarter with PDFs directly in your browser" |
| Cloud storage sync | iCloud, Dropbox, Google Drive, OneDrive + sync-folder from cloud storage | Mac, iOS | **Cloud** | Files-first sync |
| Network file access | SMB (Mac/PC), SFTP (Mac sync), WebDAV, Time Capsule, Wi-Fi Transfer, USB cable transfer | Mac, iOS | N (network, not cloud service) | Genuinely useful for local-only workflows — a pattern GlyphPDF could echo (network-drive support per PRD §18) |
| Share via link | Share files from cloud via a link | iOS | **Cloud** | |
| PDF Expert for Business | Business Manager admin panel: 1–50 seats self-serve (Sales beyond); annual per-seat via Stripe; roles (Member/Admin/Super Admin); seat auto-revoke at 4th device (earliest revoked); prorated seat changes; 30-day full refund incl. renewals; 14-day team-deletion grace; **no AI/Copilot at launch** | Mac, iOS | **Cloud (admin) + Sub** | Apple-only fleet management ("every device your crews use" — construction vertical marketing). GlyphPDF's answer stays MSI + portable ZIP + offline licensing |
| Widgets / Handoff-era features | iOS widgets; formerly Readdle Transfer (Handoff-style continuity) — REMOVED in PDF Expert 7 transition per user reports | iOS | N | Feature removal during subscription era — see failure modes |

---

## 2. LICENSING GRANULARITY & USER SENTIMENT

### 2.1 Current model (official pricing page + license FAQ + business FAQ, fetched 2026-09-07)

| Plan | Price | Scope | Includes | Excludes |
|---|---|---|---|---|
| Free tier | $0 | Mac + iOS | Viewer; basic annotation; free "basic version for new users" on iOS | Everything Premium |
| Essential (annual) | $49.99/yr ($0.96/wk framing) | iPhone + iPad + Mac (one account) | Big updates, Copilot AI, Translate, Convert to Word/Excel/PPT, OCR, Sign, Pro annotation, Text/image/link editing, priority support | (Pro-tier extras — precise delta between Essential and Pro not extractable from page markup; verdict UNVERIFIABLE on exact Pro delta) |
| Pro (annual) | $79.99/yr ($1.53/wk framing) | iPhone + iPad + Mac | "Best value" bundle | — |
| Lifetime | $139.99 one-time | **Mac ONLY** | Current Mac edition; continuous support and bug fixes; *"Additional new features may come at an extra cost"* | Future major updates, iOS/iPadOS use, **AI/Copilot integration** |
| Education | 30% off annual (official site); Macworld reports "students and educators 50% discount" | — | — | Conflicting discount figures: official page says 30%, Macworld says 50% — PARTLY_TRUE each; needs checkout-time verification |
| Business | Annual per-seat (EUR in EU / USD elsewhere), 1–50 seats self-serve via Stripe-backed Business Manager | Mac + iOS per member | Full Premium features incl. OCR + conversion; 3 devices per license (4th activation auto-revokes earliest); 30-day full refund incl. renewals; prorated seat adds; no immediate refund on seat reduction; Super Admin/Admin/Member roles; 14-day deletion grace | AI/Copilot (not at launch); SSO/SCIM not mentioned |
| Legacy PE2 | License code, up to 3 Macs, manual Deactivate flow (support-assisted reset) | Mac only | Activates **PE2-level** features inside PE3 free — paid-up owners keep old capability set but not PE3 exclusives (OCR, Enhance, Export are PE3-exclusive premium) | Cross-platform, PE3 premium tools |

### 2.2 Sentiment (graded)

- **Subscription-transition backlash is real and recurring.** PDF Expert 7 (iOS, 2019) moved paid-up owners to free+subscription; r/ipad thread "Updating to PDF Expert 7 — losing functionality?" records core features removed (users specifically mourn Readdle Transfer/Handoff) and Readdle's App Store developer replies ("PDF Expert 7 inherits all the features you've unlocked") — **MOSTLY_TRUE** (multiple independent threads + developer responses).
- **The pattern repeated brand-wide:** r/macapps on Spark's subscription pivot: *"Huge backlash on their socials. They did the same with PDF Expert and then brought some features back and minimized the upgrade prompts."* r/ios: *"Now you're losing several paid Documents subscribers, PDF Expert, etc."* — **MOSTLY_TRUE** (consistent multi-subreddit corroboration).
- **Paywall placement resentment:** MPU Talk thread "PDF Expert has gone subscription" — even post-update, annotation-summary export remained behind the paywall; nag-screen complaints (mitigated on iPhone) — **PARTLY_TRUE** (single-forum, plausible, matches Readdle pattern).
- **Lifetime-tier resentment:** r/apple discussion: lifetime = "$300 for a lifetime subscription with no updates beyond whatever version" framing; Macworld confirms lifetime excludes future major updates, mobile, and AI — the lifetime SKU reads as an exit ramp, not an ownership tier — **TRUE** (official terms corroborate sentiment).
- **Positive sentiment is equally strong on the Apple side:** Macworld (2026): "We wholeheartedly recommend it… I can't think of a single way Readdle could meaningfully improve the user interface"; PCWorld: "our pick for Mac, iPad, and iPhone users"; TechRadar: "robust and easy-to-use solution for managing business documents"; home-page press bar: "the most Mac-like and straightforward to use of the major PDF apps" — **TRUE** (published reviewer verdicts).
- **No Windows constituency at all:** Windows users asking for PDF Expert on r/windows/r/macapps are routed to Foxit/UPDF/PDFgear etc. There are no "Windows port" complaints because there is no port — **TRUE**.

---

## 3. GLYPHPDF DELTA PER DOMAIN

| Domain | PDF Expert has, GlyphPDF lacks | GlyphPDF has, PDF Expert lacks (differentiation to press) | Anti-features to refuse |
|---|---|---|---|
| Viewing & navigation | Reflow Reading Mode; TTS read-aloud; search across all open tabs; presentation mode + FaceTime co-present; default-viewer registration; iOS widgets/URL schemes | Two-page overlay correctness; presentation mode exists; honest status bar; TaskNav single-entry UX | — |
| Editing | Scan Enhance AI (shadow/distortion); unify page size/margin fill; convert-to-B&W; page resize parity exists both sides | In-house OOXML export; excision-grade text ops; SafeSave byte-identity guarantees; overlay-label redaction polish | — |
| OCR | AI-marketed "Enhance" scan beautifier | **Batch OCR** (PE has none); dual-engine ROVER ensemble vs single OCR; post-save verified text layer (F04/R08); 1bpp deskew/binarize engineering vs AI marketing | — |
| Forms | Static-XFA fill; iOS dropdown/date fields; AcroForm JavaScript subset docs; kiosk Customer Signature | **Form-field creation** (PE has zero authoring); auto-detect placement with compound undo; CSV/FDF data I/O; calculated fields | — |
| Comments & markup | Audio annotations; annotation-summary export to HTML/Markdown; stickers; Content Selection Tool | Comment table view + RFC-4180 CSV export; reply nesting; filter summaries; Djot-verified annotation persistence | Translation + Copilot as shipped (cloud) — build local equivalents |
| Redaction | Search-term Erase All (parity-ish with pattern redaction) | **Preview-before-commit; transactional 7-stage RedactOperation; sanitize default-ON; overlay labels; byte-adjacent-stream corruption guard (E-1 fix); page-list mark-all; word-list import; named presets** — PE's documented autosave-commits-redaction is GlyphPDF's anti-pattern poster child | Autosave-destructive semantics |
| Security & encryption | — | Permissions/owner-password handling; AES-256; XMP expiry; encrypted-ZIP secure package; watermark tooling | Cloud trust surface |
| Signatures | Cross-device signature sync UX (account-based); image-to-transparent-signature slider | **Entire PAdES/certificate tier**: B-LT/B-LTA, trust-chain + OCSP validation, validity badges anchored to field rects, appearance generation, SignOutcome degradation surfacing, initials + session cache | Send-for-signing cloud SaaS (both absent; GlyphPDF plans local signing packages) |
| Compare | — (absent entirely) | **The whole domain**: structural page fingerprints, middle-insertion alignment, ALIGNED-MODIFIED third stage, change filters, reports, linked scroll | — |
| Batch | — (multi-file convert-to-PDF only) | **The whole domain**: batch convert/OCR/merge/redact/compress + hot-folder + per-item pre-flight | — |
| Print production | **Measure tab: auto-scale calibrate + distance/area/perimeter + redlines + polygon/polyline** (clearest single-feature gap); AEC vertical marketing | PDF/A-1B/2B/2U/3B/3U export with version asserts; compression MRC gating + measured delta readout | — |
| Accessibility/tagging | Reading aids as UX (reflow, TTS) | Tagged-PDF reading-order check with named tolerance; async checker | — |
| Import/export | Word 97 (.doc) target; .docx/.doc option pair; scan-to-PDF iOS pipeline | In-house OOXML (true .docx/.xlsx, not mislabeled HTML/CSV); HTML/CSV/text targets; local-processing badges; bookmark/hyperlink round-trip contract; compression presets incl. Lossless parity | — |
| Cloud/services | Readdle account, Copilot, cloud sync, Business Manager, share-links | **Entire local-first stack**; opportunity: localhost MCP server over CapabilityRegistry (Nitro validated agent demand; PE has nothing) | ALL of §1.14 by design |

---

## 4. FINDINGS — failure modes & loved workflows

### Failure modes (with verdicts)

- **F1. Subscription-model trust erosion.** PE7 (2019) stripped paid features into subscriptions; Spark repeated the pattern in 2022 and users explicitly cite PDF Expert as precedent; the lifetime SKU excludes major updates, iOS, and AI ("additional new features may come at an extra cost"). Implication: licensing model is a durable brand wound across Readdle's catalog. — **MOSTLY_TRUE** (multi-thread + official terms; no single source suffices for TRUE).
- **F2. Destructive autosave semantics.** Official redaction docs: changes are *automatically permanently saved* on file close or tab switch, with "no possibility to restore" — a professional-irreversible operation with no preview/burn boundary and no undo. Implication: design-first UX can coexist with unsafe commit semantics; GlyphPDF's preview+transaction design is a demonstrable safety differentiator to market. — **TRUE** (official documentation).
- **F3. Platform ceiling + professional vacuum.** No Windows/Android at all; no compare, no batch, no PAdES/certificate signing, no PDF/A, no form authoring, no accessibility tooling, dynamic XFA unsupported. Implication: PDF Expert wins consumers on one ecosystem and cedes every professional/checklist buying criterion; Readdle routes automation demand to its separate enterprise product (Fluix). — **TRUE** for the absent features (consistent absence across all primary sources, corroborated by reviewers); graded per-item MOSTLY_TRUE where based on absence-of-evidence.

### Loved workflows (worth copying into a Windows idiom)

- **L1. Task-centric, design-first shell.** Tools tab with color-coded task shortcuts; a single toolbar whose center buttons are user-reorderable; custom toolsets and Home Tab on iOS; "most Mac-like" reputation; Macworld unable to name a UI improvement. Copy as: GlyphPDF TaskNav/ribbon continuity, capability-disclosed tool tiles, user-orderable tool strip. — **TRUE** (official + reviewer consensus).
- **L2. Word-like direct text editing + thumbnail-driven page craft.** Edit paragraphs in place with font matching; drag-drop pages, extract/merge by thumbnail gesture. This is the editing muscle reviewers actually test and praise. — **TRUE**.
- **L3. Reading as a first-class mode.** iPhone Reading Mode reflow (non-destructive), Day/Sepia/Night/Auto themes, split view, fluid 2000-page scrolling, search across tabs, TTS. Copy as: desktop reflow-ish focus mode + honest themes + cross-tab search. — **TRUE** (official docs; reviewer praise for smoothness).

---

## 5. COMPACT SUMMARY (for coordinator)

**Top 5 feature gaps (PDF Expert ships; GlyphPDF lacks):**
1. Measurement/construction kit — auto-scale Calibrate, Distance/Area/Perimeter, mm/m/in/ft, redlines, polygon/polyline (Sub; Mac+iOS) — TRUE.
2. PDF Copilot AI UX — summarize/ask/translate/explain/quiz with page-anchored highlights (Cloud/OpenAI; Mac 13+/iOS 16+) — TRUE as shipped; build LOCAL equivalent in the existing Ollama/ONNX lane.
3. Reading-mode depth — iPhone reflow (non-destructive), TTS read-aloud, Day/Sepia/Night/Auto themes, split view — TRUE.
4. Static-XFA form fill + kiosk Customer Signature (one-time, unsaved) + iOS dropdown/date/signature-field breadth — TRUE (official).
5. Apple-continuity conveniences — cross-device signature sync, network file access (SMB/SFTP/WebDAV), URL schemes, default-viewer registration, annotation-summary export to HTML/Markdown — TRUE.

**Top 3 failure modes:**
1. Subscription-transition trust erosion (PE7 paywalling + Spark echo; lifetime excludes updates/AI/iOS) — MOSTLY_TRUE.
2. Destructive autosave: redactions permanently committed on tab-switch with no preview/undo (officially documented) — TRUE.
3. Platform ceiling + professional vacuum: Apple-only, no compare/batch/PAdES/PDF-A/forms-authoring/accessibility; automation demand routed to separate product Fluix — TRUE (per-item absence MOSTLY_TRUE).

**Top 3 loved workflows:**
1. Task-centric color-coded Tools launcher + user-reorderable toolbar ("most Mac-like PDF app").
2. Word-like in-place text editing with font matching + thumbnail-driven page organization.
3. Reading-first experience: fluid 2000-page scrolling, reflow Reading Mode, split view, cross-tab search.

**Single biggest opportunity:** PDF Expert permanently abandoned Windows to be Apple's design champion — GlyphPDF can own "PDF Expert's soul on Windows": adopt the task-centric design-first shell and reading depth, invert its cloud anti-features into local-first equivalents (local Copilot, offline conversion, transactional redaction vs PE's autosave hazard), and ship the professional tier PE has never offered (compare, batch/hot-folder, PAdES, PDF/A, form authoring). The redaction-autosave contrast alone is a marketable safety story.

---

## 6. Sources

Official (primary, fetched 2026-09-07 unless noted):
- https://support.readdle.com/pdfexpert/en_US/billing-subscription/is-pdf-expert-available-on-windows (no Windows — decisive)
- https://pdfexpert.com/ · /pricing · /features · /ios/features · /pdf-copilot
- Feature pages: /features/{ocr-pdf-mac, pdf-edit-mac, organize-pdf-mac, pdf-sign-mac, convert-pdf-mac, combine-pdf-mac, reduce-pdf-mac, pdf-extract-mac, pdf-fill-mac, pdf-reader-mac, pdf-annotate-mac, translate-pdf}
- Help center: /edit-pdfs/remove-sensitive-content · /scan-ocr/convert-scanned-documents-into-text-ocr · /fill-and-sign-pdfs/supported-forms-and-fields · /fill-and-sign-pdfs/sign-pdf-documents · /managing-files-and-folders/password-protect-pdfs · /file-conversion/{convert-files, export-to-other-formats, convert-pdfs-to-images-word-excel-powerpoint-text-files} · /construction-tools/scale-and-measure-drawings · /annotate-pdfs/export-annotation-summary · /reading-pdfs/{use-smart-search, reading-mode, read-pdf-text-out-loud} · /billing-subscription/{the-pdf-expert-license-faq, how-to-get-pdf-expert, the-difference-between-pdf-expert-2-and-pdf-expert-3} · /pdf-expert-for-business/pdf-expert-for-business-faq · /privacy-security/data-and-security-in-pdf-expert · /for-developers/url-schemes
- https://copilot.pdfexpert.com/ (Windows-facing web surface)

Reviews & sentiment (secondary):
- Macworld review: https://www.macworld.com/article/2405333/pdf-expert-review.html (full text; 2026)
- TechRadar: https://www.techradar.com/reviews/readdle-pdf-expert
- r/ipad "Updating to PDF Expert 7 — losing functionality?": https://www.reddit.com/r/ipad/comments/cn61al/
- r/macapps Spark-backlash thread citing PDF Expert precedent: https://www.reddit.com/r/macapps/comments/xvg0cp/
- r/ios Readdle subscription thread: https://www.reddit.com/r/ios/comments/y31yj5/
- MPU Talk "PDF Expert has gone subscription": https://talk.macpowerusers.com/t/pdf-expert-has-gone-subscription/13193
- r/apple lifetime-tier discussion: https://www.reddit.com/r/apple/comments/1grj5f7/
- Readdle company: https://readdle.com/ (privately held; product family)

**Verdict recap:** Windows release = FALSE · All-Arrow ownership = FALSE · redaction permanence = TRUE (with excision-rigor UNVERIFIABLE) · OCR Mac-only = TRUE · iOS OCR absent = TRUE (official; contradicts marketing implication) · dynamic-XFA unsupported = TRUE · no crypto signatures = TRUE · compare/batch/forms-authoring/accessibility/PDF-A absent = MOSTLY_TRUE each (absence-based) · pricing figures = TRUE · Essential-vs-Pro exact delta = UNVERIFIABLE · on-device conversion = MOSTLY_TRUE · Copilot cloud pipeline = TRUE · App Store rating 4.5–4.6 = MOSTLY_TRUE.
