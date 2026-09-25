# Research Report: PDFgear — Deep-Dive Spec Sheet for GlyphPDF

**Date:** 2026-09-07/08
**Requested by:** GlyphPDF parity program (research-specialist protocol; deep-dive mode)
**Research question:** What does PDFgear's free model actually deliver feature-by-feature, what does its cloud AI actually do and cost in privacy terms, where does it fail, and does "free" survive contact with their monetization — so GlyphPDF can rank build priorities while staying offline-first?
**Method:** ~25 primary-source fetches via curl: pdfgear.com homepage, /pdfgear-for-windows/, /edit-pdf/, /edit-pdf-text/, /ocr-pdf/, /batch-pdf/, /create-fillable-pdf/, /secure-pdf-tools/, /sign-pdf/, /ai-pdf-editor/, /pdf-copilot/, /online-tools/, /insights/is-pdfgear-free.htm, /whats-new/ (changelog v2.1.4→2.1.20), /about-pdfgear/, /review/ (press aggregation), /privacy/ (full policy), /reddit-disinformation-statement/, /product-security-statement/, /download/; Cure53 `review-report_pdfgear.pdf` (23 pp., read in full); eSignGear homepage/pricing; Apple App Store iOS listing; Trustpilot live count. Independent reviews via search-snippet corroboration (TheBusinessDive full text fetched; Reddit/Trustpilot direct fetch blocked → snippet-tier).
**Benchmark baseline:** GlyphPDF ledger `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` §27/§28.
**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per protocol). Platform flags W/M/i/A/Web = Windows/Mac/iOS/Android/web.

---

## Executive Summary

PDFgear (PDF GEAR TECH PTE. LTD., Singapore; first release 2021) is the genuinely-free counter-argument to the subscription PDF industry: a cross-platform editor (Win/Mac/iOS/Android/web + Chrome extension) whose core desktop features — Word-like text editing, conversion, OCR, batch modes, compression, signing — are free with no watermark, no account, no file-count caps, and no ads. That UX (plus an unlimited free ChatGPT chat-with-PDF) has won it Trustpilot 4.9/6,912 and glowing mainstream-press coverage. But the free model is investor-subsidized and explicitly transitional: the vendor's own pages state AI, high-accuracy OCR, and complex conversions are the designated future paywall, payments infrastructure (Paddle) is already in the privacy policy, and the monetization wedge has landed as eSignGear — a cloud send-for-signature SaaS (account required, free tier = 2 documents/month). The AI itself is 100% cloud: marketing says "built-in GPT-3.5," the privacy policy says PDF file content is transmitted to Microsoft Azure OpenAI after in-app consent; the same statement page admits AI and mobile tasks use server-side processing. Security posture is contested: after Reddit malware allegations, a Cure53 review (Mar 2026) found "no malicious functionality" — but scoped to vendor-supplied snippets only (6 of 8 flagged behaviors, no full source), with one Info finding (MD5-only update integrity) and documented hash-forgery hijacking of the Windows default-PDF-handler plus undocumented-COM taskbar self-pinning. For GlyphPDF the opportunity is exact inversion: ship the same AI UX surface (chat, summarize, translate, rewrite, natural-language command bar driving local operations) on the hardened local Ollama lane with zero egress and verifiable local-only processing — the one thing PDFgear architecturally cannot offer.

---

## Product Snapshot (verified, Sept 2026)

| Dimension | State | Verdict |
|---|---|---|
| Company | PDF GEAR TECH PTE. LTD., 91 Bencoolen Street, Singapore; "In 2021, we built PDFgear" (eSignGear story page); TheBusinessDive says launched 2022 | TRUE (entity + 2021); 2022 date PARTLY_TRUE (source conflict) |
| Current version | Windows 2.1.20 (28 Aug 2026); major feature drops v2.1.8 (Aug 2024), v2.1.13 (Oct 2025), v2.1.16–2.1.20 (May–Aug 2026, eSign) | TRUE (official changelog) |
| Platforms | Windows 10/11; macOS (website + App Store); iOS App Store (4.7★, 8.5K ratings); Android Google Play; 30+ browser-local online tools; Chrome extension | TRUE (official + store listings) |
| Engine posture | Core desktop features local/offline ("all compression tools and most converters in the desktop app now work completely offline"); AI + "certain tasks on mobile apps" use server-side processing (vendor's own admission); online conversion/compression historically Amazon-cloud processed, "deleted after completion" | TRUE (disinformation-statement + is-pdfgear-free page) |
| Pricing | Desktop/online = free (no account, watermark, caps, ads); eSignGear = account-gated SaaS, free 2 docs/month, Premium unlimited (price UNVERIFIABLE — JS-gated, Stripe checkout) | TRUE on structure; Premium price UNVERIFIABLE |
| Trust signals | Trustpilot 4.9/5, 6,912 reviews (live fetch); press: DigitalTrends, PCWorld, TechRadar, Macworld, 9to5Mac, XDA, Lifewire, gHacks; TheBusinessDive 4.1/5 (privacy-debate docked); Reddit spyware war 2025–2026; Cure53 scoped-clear Mar 2026 | MIXED — see Failure Modes |
| Review-count drift | Same site shows "4.7/5 (366 Reviews)", "4.9 (10,000+ Reviews)", "4.9 Trustpilot 2170 Reviews", "3000+ reviews" on different pages | TRUE (drift verified); treat self-cited counts as unaudited |

---

## 1. Viewing & Navigation

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Open/read PDFs | No file-size limit on desktop; recent-file management via new File menu (2.1.13) | W/M/i/A | N | Free | Reddit: app startup slow, ~3–5s to open even small files (PARTLY_TRUE, snippet) |
| Page view | Single/double-page view; zoom | W/M/i | N | Free | TheBusinessDive hands-on (TRUE) |
| Auto-scroll reading | Mouse-wheel auto page scroll (2.1.8) | W | N | Free | Changelog + TBD (TRUE) |
| Slideshow mode | Read pages as slides | W/M | N | Free | TheBusinessDive (MOSTLY_TRUE — single reviewer) |
| Reading comfort | 3 UI themes, 5 background colors, dark mode (2.1.4, enhanced 2.1.10) | W/M | N | Free | TBD: "maximum eye comfort" (MOSTLY_TRUE) |
| Reader (TTS) | "Read aloud texts for better accessibility" tool (2.1.8) | W | N | Free | Changelog (TRUE); depth/voices undocumented |
| Bookmarks | Panel, drag-drop reorder, sub-bookmark delete prompts (2.1.8) | W/M | N | Free | Changelog (TRUE) |
| Attachments | View/manage PDF attachments (2.1.13) | W | N | Free | Changelog (TRUE); creation of file-attachment annots undocumented |
| Thumbnails | Redesigned thumbnail panel, page management (2.1.13) | W/M/i | N | Free | Changelog (TRUE) |
| Text search | Present across app; depth (regex, comments search) undocumented | W/M/i | N | Free | Existence MOSTLY_TRUE; capabilities UNVERIFIABLE |
| Shell integration | Default-PDF-handler registration via **UserChoice hash-forgery** (bypasses Win8+ consent protection); undocumented COM taskbar self-pin; "Received Files Reminder" download-folder watcher (default OFF) | W | N | Free | Cure53 report (TRUE) — aggressive installer behavior, flag for privacy-conscious users; online editor caps 100 MB (TRUE) |

## 2. Editing (Text / Object / Page)

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Edit existing text | In-place select/replace/insert/delete/move text "like Word," no text-box insertion; font/typeface/size/style/symbol changes; sub-8pt sizes (2.1.8); formatting retained | W/M | N | Free | Official + TBD "smooth, no bugs" (TRUE); missing per TBD: text alignment, spacing controls (MOSTLY_TRUE) |
| Text erase | Direct text erasure tool (2.1.13) | W | N | Free | Changelog (TRUE) — GlyphPDF has no equivalent |
| Add text | Text boxes with font/size/color/alignment | W/M/i/A | N | Free | Official (TRUE) |
| Image editing | Add, replace, extract, rotate, delete images (2.1.4); image add/edit inside PDF (2.1.13); resize/reposition | W/M/i | N | Free | Changelog + TBD (TRUE) |
| Links | Edit links (homepage: "edit text, images, shapes, signatures, links, and form fields") | W/M | N | Free | Homepage tagline (MOSTLY_TRUE — not feature-page documented) |
| Watermarks | Add/remove watermarks | W/M | N | Free | edit-pdf-text page (TRUE) |
| Page numbers | Header/footer page numbers: page ranges, alignment, margin control | W/M | N | Free | Official (TRUE) |
| Page organize | Insert, delete, rotate, crop (multi-page crop + preview 2.1.13), extract, reorder, duplicate; resize pages (2.1.13); page-size property in inches (2.1.10); blank-PDF creation (2.1.8) | W/M/i | N | Free | Changelog + TBD (TRUE) |
| Split/merge | Split PDF; merge with multiple page ranges, drag-reorder file list (2.1.8) | W/M/i/A | N | Free | Changelog (TRUE) |
| Scanner integration | Create PDF from scanner; insert pages from scanner (2.1.13) | W | N | Free | Changelog (TRUE) |
| Reflow warnings / reflow-risk UX | Not documented | — | — | — | Absent (UNVERIFIABLE, no official row) — GlyphPDF PRD §9.2 "warn on reflow risk" has no PDFgear counterpart |

## 3. OCR

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| OCR to searchable PDF | "New OCR feature to turn scanned documents into searchable text" (2.1.13, Oct 2025); before that, scanned→searchable PDF was admitted as not-yet-shipped ("strenuously working on it… rolling out any time soon" — page never updated) | W | N | Free | Changelog TRUE; **vendor doc drift between /ocr-pdf/ page and changelog TRUE** |
| OCR during conversion | Scanned→editable Word/TXT/RTF via OCR toggle in converter; batch OCR via PDF-to-Word tool with OCR enabled | W/M | N | Free | Official batch page (TRUE); processing locality for OCR UNVERIFIABLE (server-side possible per "high computing power" admission) |
| Area OCR ("Extract Text") | Drag-select any page region → text box to copy; 10+ languages | W/M | N | Free | Lifewire reviewer favorite (TRUE) |
| OCR languages | 30+ languages (conversion OCR); 10+ (area OCR) | W/M | N | Free | Official (TRUE) |
| Accuracy claims | "98.6% accuracy rate" | — | — | — | UNVERIFIABLE (vendor claim, no benchmark); TBD hands-on: "mostly accurate… but not perfect… struggles with complex layouts or tables" (MOSTLY_TRUE) |
| Preprocessing (deskew/denoise/binarize/orientation) | Not documented anywhere | — | — | — | Absent (MOSTLY_FALSE that PDFgear matches GlyphPDF's preprocessor) |
| Review-before-save (verify screen) | Not documented | — | — | — | Absent — GlyphPDF OCR Verify has no counterpart (MOSTLY_FALSE) |
| Stability | "Keeps crashing on massive scanned files" — freezes during OCR/table extraction on large scans | W | N | Free | Reddit snippet (PARTLY_TRUE, single thread + no counter-evidence) |
| OCR language selection UX | Not documented for the new OCR tool | — | — | — | UNVERIFIABLE |

## 4. Forms

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Fill forms | Text insertion, checkmarks/crossmarks, built-in + custom stamps | W/M/i/A | N | Free | Official (TRUE) |
| Create fields (desktop) | Fillable text boxes, images; form-field highlighting for interaction (2.1.8); **signature fields** (2.1.13) | W/M | N | Free | Changelog (TRUE) |
| Create fields (online) | Text, checkboxes, radio buttons, dropdowns, list boxes in browser-local Form Creator; start from blank PDF | Web | N | Free, 100 MB | Official (TRUE) |
| Field properties/validation | Tooltip/required/format categories/calculated fields — **not documented** | — | — | — | Absent/UNVERIFIABLE — TBD: "complex form creation… limited"; GlyphPDF's 10 field types incl. calculated field exceed (MOSTLY_FALSE) |
| Auto-detect fields in flat PDFs | Not documented | — | — | — | Absent vs GlyphPDF content-aware heuristic (MOSTLY_FALSE) |
| Form data import/export | Not documented | — | — | — | Absent vs GlyphPDF CSV/FDF (MOSTLY_FALSE) |
| Flatten forms | Flatten PDF tool (all layers); flatten signature to prevent edits | W/M/Web | N | Free | Official (TRUE) |

## 5. Comments & Markup

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Text markup | Highlight (text + area), underline, strikethrough | W/M/i/A | N | Free | Official (TRUE) |
| Notes | Sticky notes, comments, text boxes, text callouts | W/M/i/A | N | Free | Official (TRUE) |
| Shapes & drawing | Rectangles, circles, lines, arrows; freehand ink + ink eraser | W/M/i/A | N | Free | Official (TRUE) |
| Stamps | Built-in + **custom stamp management** (redesigned 2.1.13) | W/M/i/A | N | Free | Changelog (TRUE) |
| Comment management | Filter comments (2.1.8); copy annotated text via right-click (2.1.8) | W | N | Free | Changelog (TRUE) |
| Reply threads / statuses / review summary | Not documented | — | — | — | Absent (MOSTLY_FALSE) — matches GlyphPDF's own v1.4 roadmap gap |
| Comment attachments / summary export | Not documented | — | — | — | Absent (UNVERIFIABLE) — GlyphPDF U07 CSV export ahead |
| Measurement tools | Not documented | — | — | — | Absent (MOSTLY_FALSE) |
| Screenshot tool | Screenshot with prompts (2.1.8) | W | N | Free | Changelog (TRUE) |

## 6. Redaction

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Redact tool | "Redact sensitive content securely with the new Redact tool" (2.1.13, Oct 2025); "How to Redact a PDF for Free" article markets safe permanent removal | W | N | Free | Existence TRUE (changelog); sub-capabilities (search-and-redact, patterns, overlay labels, excision guarantees) UNVERIFIABLE — no dedicated feature page, no content-stream-excision claim |
| Pattern/regex redaction | Not documented | — | — | — | Absent vs GlyphPDF regex presets + word lists (MOSTLY_FALSE) |
| Sanitize / metadata removal | Not documented as a distinct feature | — | — | — | Absent/UNVERIFIABLE |
| Redaction integrity engineering | Not documented | — | — | — | Absent — GlyphPDF's excision corruption fix + SHA-256 source-invariance pins have no counterpart; a year-old "redact" tool with no published integrity contract is a trust gap GlyphPDF can own |

## 7. Security & Encryption

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Open password | Set/remove document-open password | W/M | N | Free | Official + TBD (TRUE) |
| Permissions password | **Missing**: TBD — "I missed… who can print it. Also, some other editors provide more options on what type of AES PDF protection method" | W/M | N | — | MOSTLY_TRUE (single thorough reviewer + no official permission-row anywhere) — GlyphPDF permission controls + AES-256 exceed |
| Unlock PDF | Online tool removes password from PDFs | Web | Y (server upload implied) | Free | Official online-tools list (TRUE); processing locality for unlock UNVERIFIABLE |
| Certificate encryption | Not documented | — | — | — | Absent (UNVERIFIABLE) |
| Secure sharing links / expiry | Not documented (out of scope for local-first GlyphPDF too) | — | — | — | Absent |
| Marketing compliance claim | "Complies with eIDAS, GDPR, ISO 27001, ESIGN Act" (sign-pdf FAQ) | — | — | — | PARTLY_TRUE at best — no certification artifacts found; ISO 27001 claim UNVERIFIABLE |
| Update integrity | **MD5-only** checksum for downloaded updaters (PGR-01-001, Info) | W | N | Free | Cure53 (TRUE) — MitM-replaceable updater; still unfixed per report |

## 8. Signatures

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Electronic signature | Draw (mouse/stylus), type, upload image; signatures + initials auto-saved for reuse; drag-drop placement; one-click multi-page placement; flatten-after-sign | W/M/i/A | N | Free | Official (TRUE) |
| Certificate-based digital signing | "Apply certificate-based digital signatures and add new signature fields" (2.1.13); "eSign feature… sign PDFs with digital certificates" (2.1.16) | W/M | N | Free | Changelog (TRUE that it exists); depth (PAdES levels, AATL trust chain, LTV, TSA) UNVERIFIABLE — no feature page; GlyphPDF PAdES B-LT/B-LTA + OCSP trust-chain validation likely exceeds |
| Signature validation | "How to validate digital signatures" article; Trust Center page | W | N | Free | Existence TRUE; mechanics UNVERIFIABLE |
| Send-for-signing (multi-party) | **eSignGear** (separate product, 2026): upload → assign signees → place fields → digital-certificate seal, tamper evidence, Document ID, signing activity records, tracking, notifications; rolled into PDFgear as Secure eSign + Request Signature (2.1.16–2.1.20, May–Aug 2026, now multilingual) | Web/W | **Y — account + cloud upload required** | Freemium: 2 documents/month free; Premium unlimited (price UNVERIFIABLE; Stripe checkout; 14-day annual refund) | Official eSignGear pages + changelog (TRUE); THE monetization wedge — anti-feature for GlyphPDF by design |

## 9. Compare

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Document comparison | **No compare feature exists** — no tool page, no sitemap entry, no changelog row, no reviewer mention | — | — | — | Absent (MOSTLY_FALSE that PDFgear covers compare; high confidence — checked sitemap, changelog, two reviews). GlyphPDF's structural fingerprints + filters + reports are uncontested here |

## 10. Batch

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Batch convert | PDF↔60+ claimed formats; no file-count limit | W/M | N (mostly local now; "most converters" offline — some server-side) | Free | Official (TRUE); format count PARTLY_TRUE (vendor claim) |
| Batch compress | Per-file compression-mode selection | W/M | N | Free | Official (TRUE) |
| Batch merge | JPEG/PNG/PDFs → one | W/M | N | Free | Official (TRUE) |
| Batch print | Multi-PDF print, custom settings; enhanced multi-print UI (2.1.13) | W | N | Free | Changelog (TRUE) — GlyphPDF lacks |
| Batch OCR | Via converter with OCR enabled | W/M | N | Free | Official (TRUE) |
| Hot folder / watched folder | Not documented | — | — | — | Absent vs GlyphPDF hot-folder (MOSTLY_FALSE) |
| Named preset pipelines | Not documented | — | — | — | Absent (both products; GlyphPDF roadmap §28) |
| Batch watermark/redact/encrypt | Not documented as batch tools | — | — | — | Absent vs GlyphPDF batch OCR/Merge/Redact (MOSTLY_FALSE) |

## 11. Print Production

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| Print dialog | Printer/copies/range; save print configuration (2.1.8); right-click print (2.1.8); redesigned multi-print UI + expanded print modes (2.1.13) | W/M | N | Free | Changelog (TRUE) |
| Print quality complaints | Fit option cuts off margins; zoom reduction distorts content | W | N | Free | Reddit snippets (PARTLY_TRUE) |
| Booklet/imposition | Only how-to articles pointing at Acrobat | — | — | — | Absent (TRUE) |
| Preflight | Not documented | — | — | — | Absent |
| Flatten | Flatten PDF (all layers); Flatten Annotations UI (2.1.13) | W/M/Web | N | Free | Changelog (TRUE) |
| Compression | Compress tool, "reduce by 90% or without quality loss" claim; Compress-to-100KB / 300KB / 500KB target tools; low mode ≈50% per TBD, little quality loss | W/M/Web | N (local) | Free | TRUE on existence; 90% claim PARTLY_TRUE (vendor); no before/after measured-size readout documented — GlyphPDF's formatCompletionReport honesty exceeds |
| PDF/A-E-X export | Not documented | — | — | — | Absent vs GlyphPDF PDF/A 1B/2B/2U/3B/3U (MOSTLY_FALSE) |

## 12. Accessibility & Tagging

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| TTS read-aloud | Reader tool (2.1.8) | W | N | Free | Changelog (TRUE); quality undocumented |
| Tag authoring/editing | Not documented | — | — | — | Absent (MOSTLY_FALSE) |
| Accessibility checker / reading order | Not documented | — | — | — | Absent — GlyphPDF reading-order check uncontested (MOSTLY_FALSE) |
| UI localization | Multilingual site (13 locales) + "multilingual support for secure eSign" (2.1.20); app UI languages undocumented | All | N | Free | PARTLY_TRUE on app-level localization breadth |

## 13. Import / Export (Create & Convert)

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| PDF → Office | Word, Excel (multi-sheet or single-sheet choice, 2.1.8), PPT/PPTX; OCR on scans; TBD: 5–10s, no bugs observed | W/M/i/A | Mixed — "most converters… work completely offline" per Mar 2026 statement; AI/Excel converter and some paths server-side | Free | TRUE on existence; per-format locality UNVERIFIABLE — honest-locality disclosure is a GlyphPDF differentiator |
| PDF → data/images | TXT, RTF, XML, HTML, CSV (batch list), PNG/JPG (+transparent PNG), long-image, PSD | W/M/Web | Mixed | Free | Official (TRUE); PSD/RTF/XML/HEIC targets exceed GlyphPDF's set |
| Office/images → PDF | Word, Excel, PPT, TXT, RTF, PSD, HEIC, JPG, PNG, webpage-to-PDF | W/M/i/A/Web | Mixed | Free | Official (TRUE) |
| Create PDF | Blank PDF (2.1.8), scanner (2.1.13), images | W/M | N | Free | Changelog (TRUE) |
| Online tools | 30+ browser-local tools (edit, read, merge, split, extract, crop, rotate, delete/add pages, highlight, annotate, flatten, forms, sign); conversions/compress/unlock server-side; 100 MB cap in browser editor; no sign-up | Web | N for local tools; Y (file egress) for server-side ones | Free | Official (TRUE) — privacy split explicitly marketed |
| Fidelity claims | "Without formatting issues/loss" throughout | — | — | — | MOSTLY_TRUE (TBD satisfied; Reddit reports conversion failures on some files — PARTLY_TRUE) |
| Markdown/EPUB | Not documented | — | — | — | Absent (same gap as GlyphPDF roadmap) |

## 14. AI Features — CLOUD ANTI-FEATURES (the contrast domain; every row egresses file content)

| Feature | Sub-capabilities | Platform | Cloud/account? | Free vs paid | Notes (verdict) |
|---|---|---|---|---|---|
| PDFgear Chatbot | Chat with PDF: summarize (manuals/textbooks/contracts), Q&A over document content, multi-document chat (desktop), chat in any language; "no limit to number of pages or size" vs ChatPDF (XDA) | W/M/i/A | **Y — cloud** | Free, currently unlimited | Official (TRUE); free-unlimited status TRUE as of Sept 2026 |
| PDFgear Copilot | Natural-language command of app operations: "compress this PDF," "delete page 10," "convert to PPT," add password — drives UI actions incl. batch; smart secondary confirmation for ambiguous commands; can be disabled (2.1.6) | W/M | **Y — cloud** | Free | Official (TRUE) — this is an agent-style workflow driver whose brain is remote |
| AI text tools | Rewrite/paraphrase, proofread, revise PDF content | W/M | Y | Free | Official (TRUE) |
| Translation | Translate PDF file content to any language (dedicated translation tool, 2.1.13); large-PDF translation marketing pages | W/M | Y | Free | Official (TRUE) |
| Model & egress reality | Marketing FAQ: "powered by GPT-3.5" / "built-in GPT"; privacy policy §6: features "powered by… Microsoft Azure OpenAI Service"; transmits "**the content of PDF files you upload**"; **consent-gated**: "We only send your data to Microsoft Azure OpenAI Service after you provide explicit consent within the app" | All | **Y** | Free | TRUE (policy text fetched); GPT-3.5 vs Azure OpenAI model drift TRUE (marketing ≠ policy); vendor claims "we never store user data… enterprise-level encryption" UNVERIFIABLE |
| Data-handling admission | "Advanced features such as AI and certain tasks on mobile apps still use secure server-side processing when high computing power or licensed SDKs are required"; files "processed once… automatically deleted"; older page: conversion on "our Amazon Cloud computing server" | All | Y | Free | TRUE (vendor's own words) — the egress is on the record; deletion claim UNVERIFIABLE |
| AI quotas/caps | None found for desktop AI (contrast: UPDF's 100-questions caps); mobile-server tasks may have undocumented limits | All | — | Free | UNVERIFIABLE (absence of documented caps) |
| What the AI does NOT do | No local/offline AI mode; no on-device model; no semantic search beyond chat; no AI page-audit/creative studio (unlike UPDF) | — | — | — | TRUE per all official pages — the AI is chat + NL-commands + text tools, nothing runs locally |

---

## 15. Licensing Granularity — what's actually free vs gated

**Structure (verified from official pages + policy + eSignGear):**

| Surface | Price | Account | Catch |
|---|---|---|---|
| Desktop Win/Mac | Free, all features | **No account** | None today: no watermark, no file caps, no ads, no IAP (official FAQ + press + Trustpilot corpus agree — TRUE) |
| iOS / Android apps | Free, all functions | No | iOS listing: "free use for all functions without any limits like trial watermark, page count" (TRUE); mobile server-tasks = file egress (TRUE, vendor admission) |
| Online tools | Free | No sign-up | 100 MB browser-editor cap; server-side processing for conversion/compress/unlock (TRUE) |
| AI (Chatbot/Copilot/text tools) | Free, unlimited currently | No | **Consent gate**: PDF content egresses to Azure OpenAI after in-app consent (TRUE, policy); investor-subsidized ("We've secured investment to cover… technology like the ChatGPT API" — TRUE) |
| eSignGear / Secure eSign / Request Signature | Freemium SaaS | **Account required** (login, Stripe checkout) | Free = **2 documents/month** (search-snippet corroborated — PARTLY_TRUE; Premium price not publicly rendered — UNVERIFIABLE); annual plan 14-day refund |
| Future paywall (declared) | — | — | Vendor's own words: "there will be a fee for some advanced options. Paid options may include AI-driven tools requiring cloud computing and special PDF conversion features"; changelog trajectory (eSign 2026) shows account infrastructure already built (TRUE that it's declared; timing UNVERIFIABLE) |

**Does "free" survive monetization?** Yes for the classic desktop editor, for now — three years of consistent no-cap behavior plus live Trustpilot corpus. But the sustainability story is explicitly venture-subsidized, the paywall targets (AI, high-accuracy OCR, complex conversions) are declared, payment rails (Paddle) and accounts (eSign) are already plumbed, and the most durable free thing — local, private processing — is being quietly eroded by cloud AI defaults and server-side conversion. The honest reading: free-until-investors-want-ROI, with eSign the first toll booth.

## 16. Failure Modes (graded)

1. **Privacy/trust deficit is structural, not FUD-free.** (a) AI egress: PDF file content → Azure OpenAI (policy, TRUE). (b) Server-side processing for AI + mobile + some conversions (vendor admission, TRUE). (c) Installer hijacks Windows default-PDF-handler via UserChoice hash-forgery and self-pins via undocumented COM — "mirror defense evasion patterns" in Cure53's own words (TRUE). (d) MD5-only update integrity (Cure53 Info finding, TRUE). (e) Cure53's clean verdict is explicitly snippet-scoped: "cannot definitively prove the presence or absence of malicious functionality within the application as a whole" (TRUE — the caveats are in the report PDF itself). (f) Website privacy policy permits third-party advertising trackers — in tension with "we don't display ads" product claims (TRUE on policy text; app vs site distinction PARTLY_TRUE). Reddit's "spyware/griftware" allegations are the un-evidenced extreme (MOSTLY_FALSE as stated); the documented behaviors above are the real, citable core.
2. **Quality ceiling on heavy documents.** Crashes on massive scanned files during OCR/table extraction; conversion failures reported; ~3–5s open times on small files; print margin/distortion issues; OCR weak on complex layouts/tables (all Reddit/TBD, PARTLY_TRUE each — snippet/single-reviewer tier, but mutually independent). No preprocessing, no review screen, no measured-compression readout to compensate.
3. **Trust-communication drift.** GPT-3.5 (marketing) vs Azure OpenAI (policy); "processes files locally without storing any data" (FAQ) vs the same page's Amazon-cloud conversion admission; OCR page stale vs changelog; four different self-cited review counts on one site (all TRUE, primary-source). The vendor fights Reddit with a "disinformation statement" + legal-threat language while its own pages contain the contradictions users cite.
4. **Free-model time bomb.** Declared paywall plan + Paddle + accounts + eSign SaaS (TRUE). Anything AI-touched will be the first to gate.
5. **Feature thinness in professional domains.** No permissions-password/AES choice, no compare, no PDF/A flavors, no form validation/calculation, no patterns in redaction, no accessibility tagging (absences graded per-domain above; MOSTLY_FALSE that PDFgear covers professional workflows).

## 17. Loved Workflows (graded)

1. **"Free, actually free" Word-like editing + conversion** — the headline draw: edit existing PDF text like Word, convert to/from Office, merge/split/organize, zero watermark/account/caps. DigitalTrends: "free, offline, no-strings-attached… it's excellent"; 9to5Mac: "no catches like watermarks, page limits, or in-app purchases"; r/macapps: "Solution for 99% of PDFs needs, 100% free" (TRUE that users love it; "offline" part now MOSTLY_TRUE given cloud admissions).
2. **Unlimited free chat-with-PDF for study/research** — students and researchers use the ChatGPT integration to summarize and interrogate long documents (Mushtaq Bilal PhD Twitter: "totally FREE"; MakeUseOf: Copilot "scour PDFs for answers"; XDA: no page/size caps vs ChatPDF) (MOSTLY_TRUE — reviewer + influencer tier).
3. **Area OCR "Extract Text" + quick page ops** — highlight any region to copy text from scans; one-minute-first-use stories for extracting articles from medical journals (Lifewire + App Store reviews, MOSTLY_TRUE).
4. **Lightweight full-toolkit single app** — TBD: "super performance even when working with larger files," fast conversions, short learning curve from Acrobat (MOSTLY_TRUE; contradicted on open-speed by Reddit — both graded).

## 18. GlyphPDF Delta (per domain — bidirectional; GAP = PDFgear ships, GlyphPDF lacks)

- **Viewing/navigation:** GAP: TTS read-aloud, auto-scroll, reading themes/background colors, area-highlight UX polish. GlyphPDF ADVANTAGE: annotation/search overlays in two-page mode (§9.1), honest status bar, no file-association hijacking — verifiable local-only posture.
- **Editing:** GAP: direct **text-erase tool**, scanner integration, blank-PDF-from-scratch, multi-page crop preview. Near-parity: in-place text edit, image edit, page resize, page numbers. GlyphPDF ADVANTAGE: reflow-risk warnings (PRD §9.2), undo/redo depth.
- **OCR:** NO GAP that matters — GlyphPDF's dual-engine ROVER (Tesseract+RapidOCR+PP-DocLayout), preprocessing (binarize/deskew/denoise/orientation), and OCR Verify review screen exceed PDFgear's single-engine OCR with no preprocessing and crash reports on big scans. Micro-GAP: area-OCR "Extract Text" one-liner UX is worth copying.
- **Forms:** NO GAP — GlyphPDF's 10 field types incl. calculated fields, validation, CSV/FDF data, auto-detect + compound undo exceed PDFgear's text/checkbox/radio/dropdown/signature-field set.
- **Comments/markup:** Micro-GAP: custom-stamp management, screenshot tool. Parity race: threads/filters/summary export (GlyphPDF U07) vs PDFgear's filter-only. GlyphPDF ADVANTAGE once U07 lands: statuses, CSV export, reply nesting.
- **Redaction:** NO GAP — GlyphPDF regex presets, word lists, overlay labels, content-stream excision with byte-identity tests, transaction operation, sanitize bundle exceed a year-old un-documented-depth redact tool. This is a marquee trust domain.
- **Security:** NO GAP — GlyphPDF AES-256 + permission restrictions + metadata sanitize exceed PDFgear's open-password-only. Plus PDFgear's MD5 updater and handler-hijack are anti-patterns GlyphPDF can explicitly contrast.
- **Signatures:** GAP (workflow): send-for-signing / multi-party / tracking / reminders — PDFgear has it but only via account-gated cloud SaaS (2 docs/month). GlyphPDF ADVANTAGE (tech): local PAdES B-LT/B-LTA + OCSP trust-chain validation vs PDFgear's un-documented-depth cert signing. Strategic call: the *cloud* signing workflow is out of scope by design; a *local* signature-request package (encrypted ZIP + instructions) is the privacy-preserving analog.
- **Compare:** NO GAP — PDFgear has nothing; GlyphPDF's structural fingerprint alignment + filters + reports is a clean win to market.
- **Batch:** GAP: **batch print**; larger no-cap conversion breadth. GlyphPDF ADVANTAGE: hot-folder automation, batch redact — neither exists in PDFgear.
- **Print production:** GAP: saved print presets, duplex UX polish. GlyphPDF ADVANTAGE: PDF/A 1B–3U export, measured before/after compression readout, signed-doc guards, MRC honesty gate.
- **Accessibility:** GAP: TTS reader. GlyphPDF ADVANTAGE: reading-order check; tagging preservation on export remains GlyphPDF's own roadmap gap (§9.14) — neither ships full tagging.
- **Import/export:** GAP: PPT/PPTX both ways, RTF/XML/PSD/HEIC targets, 30+ browser-local mini-tools as a distribution channel. GlyphPDF ADVANTAGE: in-house OOXML with honest naming, tracked-temp conversion outputs, per-format locality disclosure; add: PDF/A.
- **Cloud/AI (anti-feature inversion):** THE OPPORTUNITY — every PDFgear AI row (chat, summarize, translate, rewrite, proofread, NL command bar driving operations incl. batch) is remote-brained with PDF-content egress to Azure OpenAI. GlyphPDF's hardened local Ollama lane (F02 timeout/lifetime, F03/R04 loopback-guard, D02 caller-boundary pins) can deliver the identical UX surface at zero egress: chat-with-PDF, page-aware summarize, translate, rewrite on selected text, and a natural-language command bar mapping to local operations (compress, delete page N, export). Same convenience, opposite trust model, and a capability-registry disclosure ("this answer never left your machine") that PDFgear architecturally cannot make.

---

## 19. Compact Summary (per mission)

**Top 5 feature gaps (PDFgear ships, GlyphPDF lacks):**
1. AI assistant UX surface — chat-with-PDF, summarize/translate/rewrite, natural-language command bar driving app operations (cloud; invert locally)
2. Conversion breadth — PPT/PPTX both ways, RTF/XML/PSD/HEIC, plus 30+ zero-install browser tools as a funnel
3. Direct text-erase tool (2.1.13) — one-click text removal without redaction ceremony
4. TTS read-aloud + auto-scroll + reading comfort themes (accessibility + long-read UX)
5. Batch print (and saved print presets); plus scanner-to-PDF integration

**Top 3 failure modes:**
1. Privacy/trust: Azure OpenAI egress of PDF content (consent-gated), server-side conversion/mobile/AI processing, UserChoice handler hijack + COM self-pin, MD5 updater, Cure53 verdict scoped to snippets — a verifiable-local competitor owns this contrast
2. Quality ceiling on heavy docs: OCR crashes on large scans, weak on tables, conversion failures, slow opens, print margin bugs — no preprocessing/review/verification layer
3. Free-model endgame: declared paywall on AI/high-accuracy OCR/complex conversions; Paddle rails + eSign accounts (2 docs/month free) already live — "free" is a funded land-grab, not a covenant

**Top 3 loved workflows:**
1. Actually-free Word-like text editing + conversion + merge/split/organize, no watermark/account/caps (press + Trustpilot + Reddit consensus)
2. Unlimited free chat-with-PDF for study/research (students, PhD Twitter, MakeUseOf, XDA)
3. Area-OCR Extract Text and quick page ops — first-minute value stories

**Single biggest opportunity:** Ship the local AI twin — PDFgear's exact Copilot/Chatbot UX (chat with the document, summarize, translate, rewrite, and a natural-language command bar that drives real local operations) running entirely on GlyphPDF's hardened offline Ollama lane, surfaced with capability-registry proof that nothing left the machine. It neutralizes PDFgear's only marquee differentiator at its stated future paywall point, converts its documented privacy controversy into GlyphPDF's positioning ("verifiably local: no egress, no handler hijacking, no server-side anything"), and is buildable now that F02/F03/R04/D02 hardened the provider boundary.

## 20. Sources

**Official (high tier, directly fetched):**
- Homepage + platform pages: https://www.pdfgear.com/ , /pdfgear-for-windows/ , /download/ , /online-tools/
- Feature pages: /edit-pdf/ , /edit-pdf-text/ , /ocr-pdf/ , /batch-pdf/ , /create-fillable-pdf/ , /secure-pdf-tools/ , /sign-pdf/ , /read-pdf/
- AI pages: /pdf-copilot/ , /ai-pdf-editor/
- Monetization/free-model: /insights/is-pdfgear-free.htm , /about-pdfgear/
- Changelog: /whats-new/ (v2.1.4 Jan 2024 → v2.1.20 Aug 2026)
- Policy/statements: /privacy/ (Azure OpenAI §6, Paddle, trackers), /reddit-disinformation-statement/ (Mar 2026; server-side admission; Cure53 engagement), /product-security-statement/
- Press aggregation: /review/ (DigitalTrends, PCWorld, TechRadar, Macworld, 9to5Mac, XDA, Lifewire, gHacks, MakeUseOf, Geekflare, GeeksforGeeks quotes)
- eSignGear: https://www.esigngear.com/ (+ /pricing, JS-rendered; free-tier 2 docs/month via search snippet — PARTLY_TRUE)
- iOS listing: App Store id6465897558 (4.7★/8.5K, feature list)
- Trustpilot: https://www.trustpilot.com/review/pdfgear.com (4.9/5, 6,912 — live fetch)

**Independent security (high tier, fetched in full):**
- Cure53, "Pentest-Report PDFgear Desktop App & Codebase 02.-03.2026" (Mar 19, 2026, 23 pp.): https://cure53.de/review-report_pdfgear.pdf

**Independent reviews (medium tier):**
- TheBusinessDive, "My Honest PDFgear Review 2026" (Mar/Jun 2026, 4.1/5, full text fetched): https://thebusinessdive.com/pdfgear-review

**Community / review platforms (low-medium tier, snippet-corroborated; direct fetch blocked):**
- Reddit: r/software 1lm1prp ("Beware… spyware, malware, or… griftware"), 1r3fwfc ("Is it okay to keep using Pdfgear?"), r/microsoft 1p3afp1, r/PDFgear (1sx45z1 crashes, 1dymo0w conversion failure + Preview hijack, 1f44tba slow opens, 11rqpzj lounge print issues), r/macapps z9jwxd (positive)
- Linus Tech Tips forum: topic 1628220 ("Dangers of Reddit Recommended Software")

**Confidence:** High (≥85%) on feature existence, version history, pricing structure, AI egress mechanics, and the Cure53 findings (primary sources, multi-page corroborated). Medium on failure-mode weighting (Reddit snippet tier, mutually independent threads) and on eSignGear Premium price (gated). Vendor quality/fidelity/compliance claims (98.6% OCR, 90% compression, ISO 27001, "never store user data") graded UNVERIFIABLE or PARTLY_TRUE above. Known gaps: Mac/Android depth not independently exercised; redaction sub-capabilities and cert-signing depth undocumented by the vendor — treated as thin rather than assumed.
