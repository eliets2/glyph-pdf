# Smallpdf (Web + Desktop) — Deep-Dive Spec Sheet

**Date:** 2026-09-07 · **Researcher:** research-specialist · **Mode:** deep-dive (read-only; this file is the only write)
**Target product:** Smallpdf AG (Zürich, Switzerland; founded 2013) — task-first web PDF suite + Windows desktop app + iOS/Android mobile apps + Sign.com e-signature platform (bundled since 2025).
**Purpose:** competitor input for GlyphPDF (offline-first, local-only Windows PDF workstation) — per `pdf-parity/docs/PRD.md` and the 2026-09-05 evidence ledger.

**Method & confidence:** Primary = official smallpdf.com pages fetched 2026-09-07 (pricing incl. full 46-row plan matrix JSON, /desktop, /privacy, /edit-pdf, /compress-pdf, /pdf-ocr, /pdf-to-pdfa, /redact-pdf, /sign-pdf, /accessible-pdf; /compare and /api verified 404). Secondary = 2026 hands-on reviews (TheBusinessDive 2026-04, DigitalProjectManager), competitor comparison pages (HonestPDF), Trustpilot/Reddit/Google-indexed price capture. Prices are runtime-rendered (Recurly) and could not be extracted from official HTML directly; two independent 2026 reviews + Google's index of smallpdf.com/pricing agree. **Overall confidence: High (0.85)** for feature inventory and tiers; **Medium (0.7)** for exact free-tier counts and prices.

**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per research-specialist protocol).

---

## 0. Product snapshot

| Attribute | Value | Verdict |
|---|---|---|
| Positioning | "We make PDF easy" — task-first SaaS; the original simple-tool-grid UX | TRUE (official tagline) |
| Catalog size | "30+ document management tools" (pricing hero) vs "40+ PDF tools" (FAQs); footer lists ~50 tool slugs | PARTLY_TRUE (marketing count inconsistent; real catalog ≈ 40–50 single-purpose tools) |
| Architecture | Cloud-processing by default (EU servers: Hetzner Germany; Cloudflare US for auth/edge); some tools now advertise in-browser (WASM) processing | TRUE (privacy page names Hetzner + Cloudflare) / browser-claim PARTLY_TRUE (vendor marketing, not independently verifiable) |
| Users | "1.7 billion people since 2013" (visitors, not users) | MOSTLY_TRUE as a cumulative-visit marketing number |
| Certifications | ISO/IEC 27001 (annually audited), GDPR, CCPA, Swiss nFADP | TRUE (official, repeated on every tool page) |
| File retention | Logged-in: deleted within 1 hour unless saved to Smallpdf Drive; logged-out: "within a reasonable period" (unspecified); saved files deleted ≤14 days after user deletes them | TRUE (official privacy notice — note the logged-out promise is vaguer than the marketing "1 hour") |
| Pricing model | Freemium + 7-day Pro trial; daily-metered free tier; subscription Pro/Team/Business | TRUE |

---

## 1. Viewing & navigation

| Feature | Sub-capabilities | Surface | Free tier | Pro ($10–15/mo) | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF Reader | Open, read, print, share; "fastest PDF reader" marketing | Web + Desktop + Mobile | Daily download limit | Unlimited | Y (web); Desktop claims offline | TRUE (official /desktop, /pdf-reader). Desktop offline for reader = TRUE (official FAQ: "download the Smallpdf desktop app to view PDFs offline") |
| Page navigation | Scroll/page modes, basic viewer controls | Web + Desktop | Daily limit | Unlimited | Y | Minimal reader; reviewers score reading features lowest (Business Dive 3.5/5 "lacks advanced PDF reading") — MOSTLY_TRUE |
| Thumbnail / page overview | Page grid in Organize mode (drag to reorder) | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official Organize tool) |
| Bookmarks/outline pane | — | — | **Absent** | — | — | No outline/bookmark navigation documented anywhere official — absence TRUE |
| Two-page / presentation / dark reader modes | — | — | **Absent** | — | — | Absence TRUE (not in official tool list or reviews) |
| Full-text search | Browser/app find | Web | UNVERIFIABLE | — | Y | Not documented officially; UNVERIFIABLE |
| Jump-to-page / history | — | — | UNVERIFIABLE | — | Y | Not documented |

**GlyphPDF delta (viewing):** bidirectional —
- GlyphPDF → Smallpdf: GlyphPDF has two-page + presentation modes, dark mode, bookmarks/attachments panes, search shortcuts (PRD §9.1 all done) that Smallpdf's viewer simply lacks; Smallpdf's viewer is its weakest surface. *Implication:* GlyphPDF can win "power reader" claims outright.
- Smallpdf → GlyphPDF: none of substance here — Smallpdf adds no viewing capability GlyphPDF lacks. *Recommendation:* keep ledger §9.1 as-is; market "real reader" vs Smallpdf's minimal one.

## 2. Editing (text / object / page)

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Add text boxes / whiteout-style annotation | Insert text, choose font, size, color, alignment | Web + Desktop | Free with daily limit | Unlimited | Y | TRUE (official Edit FAQ: free tier "add text, images, highlights, drawings") |
| **Direct text editing ("Edit Text")** | Edit existing PDF text in place via text-box model; double-click to edit, re-font/resize/recolor | Web + Desktop | **Pro-only** | ✅ | Y | TRUE (official FAQ: "Direct PDF text editing requires a Pro subscription"). Text-box model, not paragraph reflow — MOSTLY_TRUE (review-observed; no official reflow claim) |
| Image insertion | Insert JPG/PNG, move, resize, opacity | Web + Desktop | Daily limit | Unlimited | Y | TRUE/MOSTLY_TRUE (official page mentions images; opacity per Business Dive hands-on) |
| Shapes & drawing | Shapes, freehand draw | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official) |
| Page ops: Organize | Reorder (drag), merge, split, extract, delete, rotate pages | Web + Desktop | Daily limit | Unlimited (batch/Show Pages Pro-only) | Y | TRUE (official matrix) |
| Number Pages | Add/replace page numbers | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official + desktop page) |
| Crop PDF | Crop pages | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official matrix) |
| Object layering/rotation of arbitrary objects, advanced layout tools | — | — | **Absent** | — | — | Absence MOSTLY_TRUE (no official capability; text-box editor is deliberately shallow) |
| Undo history across sessions | — | — | UNVERIFIABLE | — | Y | Not documented |

**GlyphPDF delta (editing):** bidirectional —
- GlyphPDF → Smallpdf: GlyphPDF's inline native-text editing with layout-preservation warnings, object move/resize/rotate/layer, image edit-in-place (PRD §9.2 ✅) exceeds Smallpdf's text-box depth; Smallpdf gates even basic existing-text editing behind Pro.
- Smallpdf → GlyphPDF: nothing structural. *Opportunity:* GlyphPDF's honesty-first capability registry (U08) can explicitly advertise "edits existing text objects, not overlays" — a differentiator Smallpdf's FAQ dances around.

## 3. OCR

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF OCR (searchable layer) | Scanned PDF → searchable/selectable text; drag-drop, no settings | Web (Desktop via suite) | Daily limit | Unlimited | Y (official says "run OCR in your browser" — vendor claim) | TRUE feature; browser-processing claim PARTLY_TRUE |
| OCR to editable Office | OCR'd doc → editable Word/Excel/PPT | Web | **Pro-only** | ✅ | Y | TRUE (official matrix row: "OCR (PDF to Word, Excel, PPT)" ❌ free) |
| Language support | "Many different languages" (no count published); translation hand-off to Translate PDF | Web | Same limits | — | Y | Count UNVERIFIABLE; TRUE that multilingual |
| Preprocessing controls (deskew / denoise / binarize / orientation) | — | — | **Absent** | — | — | Absence TRUE — official FAQ: "Results depend on clarity, contrast, and resolution of the original scan" (i.e., no cleanup stage offered) |
| OCR review / correction UI | — | — | **Absent** | — | — | Absence MOSTLY_TRUE (no review screen documented anywhere) |
| Output modes (searchable-only / editable / text-under-image / high-accuracy) | — | — | **Absent** (single implicit mode) | — | — | Absence TRUE |

**GlyphPDF delta (OCR):** bidirectional —
- GlyphPDF → Smallpdf: dual-engine ROVER ensemble (Tesseract 5 + PP-OCRv5/RapidOCR + PP-DocLayout), 1-bit binarization, deskew, orientation detection, confidence scoring, and the OCR-Verify review screen (ledger F05/F10/F11/F04, U03) are all absent at Smallpdf. Smallpdf OCR is a zero-control black box.
- Smallpdf → GlyphPDF: task-first framing — "make scanned PDF searchable" as a single drag-drop task with no preprocessing decisions (good default behavior), and OCR→Word hand-off as a chained task. *Recommendation:* keep GlyphPDF's advanced controls under progressive disclosure (PRD §10), default to a one-click "searchable PDF" preset like Smallpdf's.

## 4. Forms

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF Form Filler | Fill existing AcroForm fields | Web + Desktop + Mobile | Daily limit | Unlimited | Y | TRUE (official tool list) |
| Form field **creation** (build fillable forms) | — | — | **Absent** | — | — | Absence TRUE — no create-forms tool in the full ~50-tool catalog; filler only |
| Flatten PDF | Flatten forms/annotations to static content | Web | Daily limit | Unlimited | Y | TRUE (official matrix) |
| Form data import/export (FDF/CSV/XML) | — | — | **Absent** | — | — | Absence TRUE |
| Field validation / calculated fields / tab order | — | — | **Absent** | — | — | Absence TRUE |

**GlyphPDF delta (forms):** bidirectional —
- GlyphPDF → Smallpdf: full form authoring (all 10 field types incl. calculated fields, auto-detect, tab order, CSV/FDF data, safe-save transactions, compound undo — ledger F01/F09/V06) versus Smallpdf's filler+flatten only. This is a wholesale GlyphPDF win.
- Smallpdf → GlyphPDF: none. *Recommendation:* none needed; keep forms as a headline parity-over-Smallpdf item.

## 5. Comments & markup

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF Annotator | Highlight, underline, strikethrough, squiggle, text, draw, shapes, sticky notes | Web + Desktop + Mobile | Daily limit | Unlimited | Y | TRUE (official + reviews) |
| Comment threads / statuses (open/resolved/rejected) | — | — | **Absent** | — | — | Absence TRUE — no comment-management surface documented; reviewers score annotation 3/5 "fewer annotation features" (Business Dive) |
| Stamps / callouts / file attachments on comments | — | — | **Absent** | — | — | Absence MOSTLY_TRUE |
| Share PDF (link sharing) | Shareable download link for a processed file | Web | Free with account | Unlimited | Y | TRUE (official matrix: "Share — Free with an account") |
| Comments summary / review export | — | — | **Absent** | — | — | Absence TRUE |

**GlyphPDF delta (comments):** bidirectional —
- GlyphPDF → Smallpdf: comment records with filters, table view, CSV export (U07) and statuses are beyond anything Smallpdf ships.
- Smallpdf → GlyphPDF: the "Share" pattern (one click → link) is cloud-native and out of GlyphPDF's local-only scope, but an analog exists: encrypted-package export (already shipped, §9.11). *Recommendation:* skip link sharing; keep local secure-sharing.

## 6. Redaction

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Redact PDF | Mark text/areas → "permanent" removal ("not a black box; can't be viewed, copied, or recovered"); irreversible once saved | Web | Daily limit | Unlimited | Y ("Process files 100% in your browser… nothing stored on our servers" — official page) | TRUE feature; permanence claim MOSTLY_TRUE (vendor-asserted excision, no third-party verification found); browser-claim PARTLY_TRUE |
| Redaction workflow | Mark → Finish → optional watermark/password → download | Web | Same | — | Y | TRUE (official how-to) |
| Pattern redaction (emails/phones/IDs/keywords), word-lists | — | — | **Absent** | — | — | Absence MOSTLY_TRUE (none documented) |
| Metadata sanitize bundle / redaction log | — | — | **Absent** | — | — | Absence MOSTLY_TRUE (FAQ claims metadata of redacted content removed, but no sanitization report/log) |
| Preview-before-apply / undo | Marking preview yes; undo after save: "No. Once applied and saved, redaction is permanent" | Web | — | — | Y | TRUE (official FAQ) |

**GlyphPDF delta (redaction):** bidirectional —
- GlyphPDF → Smallpdf: pattern redaction with presets (Email/Phone-US/SSN), word-list import, sanitize-on-save bundle, transaction-safe excision with byte-invariance guarantees (ledger §9.8, U05, E-1 fix, D07, N04) — all absent at Smallpdf. GlyphPDF's verified byte-exact excision also answers a trust question Smallpdf can only assert.
- Smallpdf → GlyphPDF: the framing "not a black box — content eliminated" is a marketing lesson: state explicitly that GlyphPDF redaction is content-stream excision, not covering. Also Smallpdf pairs redaction with optional password/watermark as post-steps — cheap UX win. *Recommendation:* adopt the one-line trust claim in GlyphPDF's redact completion report (formatCompletionReport seam already exists, §9.13-a).

## 7. Security & encryption

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Protect PDF | Set open password | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official) |
| Unlock PDF | Remove password (user must supply it) | Web + Desktop | Daily limit | Unlimited | Y | TRUE (official; Business Dive hands-on) |
| Permission restrictions (no-print/no-copy) as separate flow | — | — | UNVERIFIABLE | — | Y | Not documented as a distinct control; UNVERIFIABLE |
| Watermark PDF | Custom text watermark (visual deterrent, not encryption) | Web | Daily limit | Unlimited | Y | TRUE (official tool) |
| Encryption standard (AES-256 etc.) | — | — | UNVERIFIABLE | — | — | Vendor publishes TLS-in-transit (256-bit TLS per OCR page) but not the PDF encryption algorithm; UNVERIFIABLE |
| Certificate-based document encryption, doc expiry, secure links | Secure links exist only as Share links (no access control claims); expiry NOT offered | — | **Absent** | — | — | Absence TRUE vs PRD §9.11 items; note GlyphPDF's encrypted-ZIP package + XMP expiry has no Smallpdf analog |
| Transport/infrastructure security | TLS, ISO 27001, GDPR/CCPA/nFADP, EU processing (Hetzner), 1-hour deletion | All | — | — | Y | TRUE (official privacy) |

**GlyphPDF delta (security):** bidirectional —
- GlyphPDF → Smallpdf: AES-256 with open+permission passwords, certificate/digital-ID workflows, XMP expiry→read-only, encrypted-ZIP secure package, metadata sanitization (PRD §9.11) exceed Smallpdf's Protect/Unlock/Watermark trio.
- Smallpdf → GlyphPDF: trust signaling. Smallpdf puts ISO/GDPR/retention badges on literally every tool page and runs an annual audit. GlyphPDF's "local badge" (§9.16) is the right move; extend it to a visible "processing: this PC only" disclosure per workflow (CapabilityRegistry already enables whyNot-style wording).

## 8. Signatures (eSign)

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Sign PDF (self-signing) | Draw / type / upload signature; initials, date, text, checkboxes; "Finish & Sign"; converts Office/images to PDF for signing; no account required | Web + Desktop + Mobile | Daily limit | Unlimited | Y | TRUE (official sign page) |
| Signature reuse | Smallpdf stores your signature(s) for future convenience | Web | — | — | Y | TRUE (official privacy notice §1.9) |
| Request Signatures (multi-party eSign) | Via **Sign.com** (bundled): activity timeline/audit trail, Certificate of Completion, templates, Sign IDs, Document IDs, digital sealing, document access codes, unlimited signees, mobile signing, signing order | Web (Sign.com) | Sign.com free: **2 documents/month**; Pro includes **unlimited Sign.com Premium**; Team: Premium per member | ✅ | Y | TRUE (official pricing page Sign.com section + FAQ "Sign.com free account limited to 2 documents a month") |
| Certificate-based digital signatures (PAdES/CAdES) | Not a Smallpdf tool; Sign.com markets "digital sealing" + "better signature verifiability" (mechanism undisclosed); blog content is educational only | — | **Absent in Smallpdf proper** | — | Y | Absence TRUE for PAdES-grade local signing; digital sealing existence TRUE, its standard-compliance PARTLY_TRUE/UNVERIFIABLE |
| Validity badges / trust-chain / OCSP display | — | — | **Absent** | — | — | Absence TRUE |

**GlyphPDF delta (signatures):** bidirectional —
- Smallpdf → **GlyphPDF** (the real gap): send-for-signing, signing order, status tracking, reminders, audit trail (PRD §9.7 "not started", ledger §9.7-c/DSS/B-LTA missing-piece wording) — Smallpdf answers this with Sign.com: 2 free docs/month, then bundled into Pro. *This is Smallpdf's strongest workflow moat and GlyphPDF's largest roadmap item (v1.5.x).*
- GlyphPDF → Smallpdf: PAdES B-LT/B-LTA with trust-chain/OCSP validation, per-signature validity badges, session signature cache, initials/monogram (ledger §9.7) have no Smallpdf equivalent — Smallpdf's basic signing is a flat stamp + cryptographic sealing claim.
- *Recommendation:* when building §9.7 workflow, copy Smallpdf/Sign.com's artifact set exactly (activity timeline, completion certificate, reminders) because reviewers treat those as table stakes for "request signature".

## 9. Compare

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Document comparison (two PDFs) | — | — | **DOES NOT EXIST** | — | — | TRUE absence — `/compare` is a 404; footer "Compare" links to marketing page `/compare/adobe-vs-foxit-vs-smallpdf`; no compare tool in the full catalog or any review |

**GlyphPDF delta (compare):** Smallpdf → nothing; GlyphPDF owns this outright (DiffEngine, fingerprint alignment, change filters, reports — ledger F06/V04/CMP-align). *Opportunity:* zero competitors in the task-first SaaS tier offer comparison; this is a clean differentiation line for GlyphPDF marketing.

## 10. Batch

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Batch Compress | Multiple files in one go | Web + Desktop | **Pro-only** | ✅ | Y | TRUE (official matrix) |
| Batch Convert | Multi-file conversion | Web + Desktop | **Pro-only** | ✅ | Y | TRUE (official matrix) |
| Batch PDF/A conversion | "Convert multiple PDF files into PDF/A in one batch" | Web | **Pro-only** | ✅ | Y | TRUE (official PDF/A FAQ) |
| Desktop "Batch Tools" | Batch feature marketed on desktop page | Desktop | Pro | ✅ | Partially (login; processing location ambiguous) | TRUE (official desktop page) |
| Hot folder / watched folder automation | — | — | **Absent** | — | — | Absence TRUE |
| Named multi-step preset pipelines | — | — | **Absent** | — | — | Absence TRUE |

**GlyphPDF delta (batch):** bidirectional —
- GlyphPDF → Smallpdf: hot-folder watching + OCR/merge/redact batch operations (PRD §9.12) exceed Smallpdf's compress/convert-only batch.
- Smallpdf → GlyphPDF: none beyond UX polish. *Recommendation:* none.

## 11. Print production

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Preflight / CMYK / ICC / ink checks | — | — | **Absent** | — | — | TRUE absence (no such tool in catalog) |
| Bates numbering | — | — | **Absent** | — | — | TRUE absence (Number Pages is plain numbering only) |
| Imposition / N-up / printer marks | — | — | **Absent** | — | — | TRUE absence |
| Print from reader | Desktop reader prints | Desktop | Daily limit | Unlimited | Offline-capable | TRUE |

**GlyphPDF delta:** GlyphPDF → Smallpdf: Bates numbering (§9.9 ✅) and PDF/A-graded export are print/compliance adjacent wins; page-size handling (§20 nonstandard sizes) likewise. Nothing flows the other way.

## 12. Accessibility / tagging

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Tagged-PDF authoring / reading-order check / alt-text tooling | — | — | **Absent as tools** | — | — | TRUE absence — `/accessible-pdf` is an eBook/checklist marketing guide (EAA June-2025 framing), not a tool; OCR page notes recognized text "can help screen readers… though full accessibility may require further changes" |
| Accessible viewer features | — | — | UNVERIFIABLE | — | — | No claims found |

**GlyphPDF delta:** GlyphPDF's reading-order check (§9.14) and future tag preservation/repair have **no Smallpdf counterpart** — and Smallpdf itself is marketing the EAA compliance fear (fines "up to €100,000") without a tool. *Opportunity:* EAA is a live compliance driver; GlyphPDF can ship the checking tooling Smallpdf only writes blog posts about.

## 13. Import / export formats

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| PDF → Word / Excel / PowerPoint | Convert to editable Office | Web | Daily limit | Unlimited; Batch Pro | Y | TRUE (official matrix); quality praised by reviewers (Business Dive: "output when converting PDFs is excellent") — MOSTLY_TRUE |
| PDF → JPG / PNG; images → PDF | Image conversion | Web | Daily limit; Extract-Images & Adjust-Margins **Pro-only** | Unlimited | Y | TRUE (official matrix) |
| Office → PDF | Word/Excel/PPT (+ ODT/ODS/ODP OpenOffice) → PDF | Web | Daily limit | Unlimited | Y | TRUE (official catalog) |
| Other → PDF | TXT, RTF, HWP (Hangul), HTML, EPUB, ZIP, CSV, iWork Pages → PDF | Web | Daily limit | Unlimited | Y | TRUE (official catalog) — breadth here exceeds GlyphPDF import set |
| PDF → PDF/A | ISO archival PDF, user-selectable conformance level; scanned PDFs accepted | Web | Daily limit (free); batch = Pro | Unlimited | Y | TRUE (official PDF/A page). **Levels not enumerated** (GlyphPDF's named 1B/2B/2U/3B/3U is finer); no validation/veraPDF claim — UNVERIFIABLE |
| Export destinations | Device, Google Drive, Dropbox, OneDrive, Smallpdf Drive, share link | All | Account needed for some | Unlimited size (Drive) | Y | TRUE (official tool pages) |
| PDF → Markdown/EPUB export | EPUB exists only TO PDF | — | **Absent** | — | — | Absence MOSTLY_TRUE |
| Cloud storage (Smallpdf Drive) | Stored files; free size-limited, Pro unlimited | Web + Mobile | Limited size (number unpublished) | Unlimited | Y | TRUE (official matrix); size number UNVERIFIABLE |

**GlyphPDF delta (formats):** bidirectional —
- GlyphPDF → Smallpdf: honest in-house OOXML writing with capability gating (ledger F07/F08), verified PDF/A level mapping (N03 fixed), and the local-processing badge. GlyphPDF's PDF/A validation story (veraPDF wiring) is ahead of Smallpdf's unverified claim.
- Smallpdf → GlyphPDF: broader TO-PDF fan-in (HWP, iWork, EPUB, HTML, ZIP) — *recommendation:* low priority for the PRD persona set, but HTML→PDF is a cheap, commonly requested add.

## 14. AI tools (bonus domain — central to current Smallpdf pitch)

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Chat with PDF | Q&A with citations + suggested questions | Web | **4 documents/day, 20–30 prompts/day** | Unlimited | Y | TRUE (official matrix row verbatim) |
| AI PDF Summarizer | Key-point summary + follow-up questions | Web | Same caps | Unlimited | Y | TRUE |
| Translate PDF | Whole-document or summary translation, many languages | Web | Same caps | Unlimited | Y | TRUE |
| AI Question Generator | MCQ/true-false/open questions from a PDF (study use-case) | Web | Same caps | Unlimited | Y | TRUE |
| AI file-size / word limits | 50 MB file cap on BOTH tiers; 25–35k words free vs 100k words Pro | Web | 50 MB / 25–35k | 50 MB / 100k | Y | TRUE (official matrix) |

**GlyphPDF delta (AI):** GlyphPDF has a local Ollama provider with hardened boundaries (ledger F02/F03/D02-AI). Smallpdf's AI is cloud-only, 50 MB-capped, metered. *Opportunity:* "local AI over your documents — no upload, no prompt metering" is a direct counter-position; Smallpdf proves users want PDF chat (it's now a pricing-table headline).

## 15. Cloud / web–desktop split (separately marked — structural domain)

| Feature | Sub-capabilities | Surface | Free tier | Pro | Cloud? | Notes |
|---|---|---|---|---|---|---|
| Web suite (primary) | All ~40–50 tools at dedicated URLs; drag-drop → process → download | Web | Daily meter | Unlimited | **Y (default)** | TRUE |
| Windows desktop app | Wraps the same toolset; "offline access to all the PDF tools"; batch tools; fast reader | Desktop | Requires Pro for full use; login | ✅ | **Hybrid — Y/N** | PARTLY_TRUE: offline real for reader/basic ops (official FAQ), but MS Store listing requests Internet permission; conversions/compress/OCR + licensing have historically phoned home. Official marketing overstates "offline" |
| Mobile apps (iOS/Android) | Scan (camera document scanner), organize, sign, share | Mobile | Daily task limit | Unlimited | Y | TRUE (official matrix + privacy notice) |
| Chrome extension | Use Smallpdf tools on PDFs opened in the browser | Browser | Same free meter | ✅ | Y | TRUE (review-verified; official onboarding mentions extension) |
| Google Workspace / Dropbox / OneDrive integrations | Process Drive/Dropbox/OneDrive files without leaving the host app | Web | Account | ✅ | Y | TRUE (Google Workspace Marketplace listing; OneDrive "recently launched" per Business Dive) |
| Embed PDF / Developers (API) | Footer lists "Embed PDF" and "Developers" | Web | — | — | Y | Embed existence TRUE (footer); public API landing NOT found at /api (404) — API availability UNVERIFIABLE |
| Sign.com | Dedicated e-sign platform bundled with subscription | Web | 2 docs/mo | Unlimited Premium | Y | TRUE |

**Structural takeaway:** every Smallpdf surface ultimately meters against one account and one daily quota; the desktop app is a distribution wrapper, not an offline engine. There is **no local-only mode**.

---

## 16. Licensing granularity

| Plan | Price (2026) | What you get | What it unlocks vs Free | Verdict |
|---|---|---|---|---|
| **Free** | $0 | All tools "limited"; daily download limit (reported **2 tasks/day** shared across tools); AI: 4 docs + 20–30 prompts/day, 50 MB, 25–35k words; mobile with daily task limit; Sign.com free: 2 docs/month; no forced watermark on standard outputs; account prompts after limited use | Nothing further | Price TRUE. "2/day" MOSTLY_TRUE (three independent secondary sources; official page hides the number behind "Daily download limit"). No-watermark MOSTLY_TRUE (competitor watermark claim is low-credibility; not a widespread complaint) |
| **Pro** | **$15/mo monthly; ~$10/mo billed annually** (7-day free trial, cancel anytime; PayPal/Visa/Mastercard) | Unlimited access to all 30+ tools & downloads; **Edit Text; OCR→Word/Excel/PPT; Moderate+Strong compression; Batch Compress/Convert; Extract Images; Adjust Margins; Show Pages**; unlimited AI (100k words); unlimited mobile & scan; unlimited file size; **unlimited Sign.com Premium** | Everything metered or Pro-gated in Free | Price MOSTLY_TRUE (Google-indexed "$15.00/month" on smallpdf.com/pricing + Business Dive 2026-04; historic $12/mo suggests regional/promo variance — treat $12–15/mo, $9–10/mo annual as the band) |
| **Team** | ~$8/seat/mo annual; $12/seat/mo monthly (2–19 seats) | Everything in Pro per member + priority support, centralized billing, member access management, Sign.com Premium per member | Admin layer | MOSTLY_TRUE (Business Dive; seat range TRUE from official pricing UI) |
| **Business** | Custom (20+ seats) | Everything in Team + dedicated support, flexible payment | Contract layer | TRUE |
| Cancel/refund | Cancel via Profile → Plan; refund complaints exist (EU 14-day right disputed by users) | — | — | Policy TRUE; refund friction MOSTLY_TRUE (Reddit r/pdf threads; Trustpilot ~4★ across ~4,900 reviews) |

---

## 17. Task-first UX patterns (research focus — patterns worth copying or countering)

| Pattern | Evidence | Verdict |
|---|---|---|
| One tool = one URL = one task; zero-option default flow (upload → progress → download) | Site architecture; every tool page is a drop-zone + 3-step how-to | TRUE |
| Aggressive option removal (e.g., compression = Basic/Moderate/Strong presets only — no DPI/quality numbers) | Official compress page | TRUE |
| Cross-tool hand-offs ("Edit the PDF with our other tools", OCR→Word, merge→compress→sign chains documented in official blogs) | Official OCR/compress pages; blog posts on merge+compress workflow | TRUE |
| Download-time destination choice (device / Drive / Dropbox / OneDrive / link) surfaces integrations at the moment of value | Official sign page | TRUE |
| Meter made visible late: free users hit "daily limit" walls and account prompts mid-flow | Pricing matrix wording; HonestPDF critique | MOSTLY_TRUE |
| Trust badges repeated on every tool page (ISO/GDPR/TLS/1-hour deletion) | All fetched tool pages | TRUE |
| Tool rating widget ("rate this tool 4/5, 312 votes") feeding product analytics | OCR page | TRUE |

---

## 18. Failure modes (graded)

| # | Failure mode | Evidence | Verdict |
|---|---|---|---|
| 1 | **Upload-privacy model + contradictory messaging.** Every task transits or is processed on servers (Hetzner DE / Cloudflare US); yet marketing says desktop is "offline" and some tools claim "100% in your browser… nothing stored". Logged-out deletion is only "within a reasonable period" (vaguer than the advertised 1 hour). Businesses respond with DPAs; individuals worry about contracts/medical/NDAs | Official privacy notice vs official tool/desktop pages; HonestPDF structural critique | TRUE (architecture) / PARTLY_TRUE (browser-only claims) |
| 2 | **Metered free tier + subscription billing friction.** Shared 2-tasks/day pool exhausts in one real workflow; account prompts mid-task; 7-day trial auto-converts; recurring Reddit/Trustpilot complaints about renewal charges and refund denials despite 4★ overall | Pricing matrix; ihatepdf/pdftechno/gethonestpdf; Reddit r/pdf (1nwmfos, 1q86e1y); Trustpilot | MOSTLY_TRUE |
| 3 | **Compression quality & conversion depth.** Strong compression re-encodes images; Smallpdf's own blog concedes "text on image-heavy pages can become slightly blurry"; no granular DPI/quality controls (preset-only); text-box editor breaks complex formatting (no reflow engine) | Smallpdf blog (blog.smallpdf.tools scanned-PDF article); r/pdf Ghostscript-replication thread; review observations | MOSTLY_TRUE |

## 19. Loved workflows (graded)

| # | Workflow | Evidence | Verdict |
|---|---|---|---|
| 1 | **Merge → compress → email.** The canonical Smallpdf chain since 2013; r/YouShouldKnow virality; official blogs document merge-then-compress "perfect for emailing" | Reddit YSK; smallpdf.com/blog merge/compress articles; G2 praise | TRUE |
| 2 | **No-account fill & sign.** Upload → draw/type/upload signature → Finish & Sign → download; works without registration; the task most often named in positive reviews | Official sign page ("No account creation required"); G2 reviews | TRUE |
| 3 | **Quick PDF↔Office conversion + AI summarize/chat** for students/knowledge workers; AI tools are fast, cited, and now headline-priced | Business Dive hands-on (conversion "excellent", AI "massive… really enjoyed"); official AI rows | MOSTLY_TRUE |

## 20. Synthesis — top 5 gaps, top opportunity

**Top 5 feature gaps (Smallpdf lacks; GlyphPDF direction confirmed):**
1. Document comparison — entirely absent (GlyphPDF §9.10 ✅).
2. Form creation/authoring — filler+flatten only (GlyphPDF §9.6 ✅).
3. OCR depth — no preprocessing, no review/correction UI, no output modes (GlyphPDF §9.4 + U03 ✅).
4. Pattern redaction + sanitization + redaction logging (GlyphPDF §9.8 ✅).
5. Print-production & accessibility tooling (Bates, preflight, tagging/reading-order) — absent (GlyphPDF §9.9/§9.14 ✅).

**Reverse gaps (Smallpdf has; GlyphPDF roadmap):**
1. Multi-party eSign workflow with audit trail + completion certificate + reminders (GlyphPDF §9.7 = largest Phase-2 gap; Sign.com defines the table stakes).
2. AI document tools (chat/summarize/translate/quiz) — now a headline pricing row; GlyphPDF's local Ollama stack can counter-position.
3. Ecosystem surfaces: mobile scan+sign, Chrome extension, Google Workspace/Dropbox/OneDrive embeds, share links.
4. TO-PDF format fan-in breadth (HTML/EPUB/HWP/iWork/ZIP/CSV).
5. Compression presets as honest UX (3 named levels + measured before/after — GlyphPDF already has the readout, §9.13-a).

**Single biggest opportunity:** Smallpdf's moat is *task-first simplicity with cloud economies*; its structural ceiling is *the upload step plus the daily meter*. GlyphPDF should clone the task-first UX surface (one-task screens, preset-only controls, guided tool hand-offs, trust badges on every workflow) while inverting the architecture message: **every Smallpdf workflow, running locally, uncapped, with files that never leave the machine** — "Smallpdf-simple, offline-true". The eSign workflow remains the one area where the cloud model is genuinely hard to replicate locally; build the artifacts (timeline, certificate, order) and pair with the existing PAdES trust display.

---

## Sources

Official (fetched 2026-09-07):
- https://smallpdf.com/pricing (plan matrix JSON: 46 feature rows, AI caps, Sign.com free = 2 docs/month)
- https://smallpdf.com/download/windows and https://smallpdf.com/desktop (desktop app)
- https://smallpdf.com/privacy (retention: 1 hour / reasonable period / 14 days; Hetzner; Cloudflare; signature storage)
- https://smallpdf.com/edit-pdf (free-vs-Pro text editing split), /compress-pdf (Basic/Moderate/Strong; Pro gating), /pdf-ocr (OCR scope, browser claim), /pdf-to-pdfa (PDF/A tool), /redact-pdf (permanent redaction claims), /sign-pdf (self-sign + Sign.com), /accessible-pdf (eBook, not a tool)
- https://smallpdf.com/compare → 404 (footer Compare = /compare/adobe-vs-foxit-vs-smallpdf marketing page); /api → 404

Secondary:
- The Business Dive, "Smallpdf Review 2026" (2026-04/06) — prices ($10 annual / $15 monthly; Team $8/$12), hands-on feature depth, scores, OneDrive
- The Digital Project Manager, Smallpdf review — "from $10/month billed annually", offline framing
- HonestPDF vs Smallpdf comparison (gethonestpdf.com/vs/smallpdf) — structural upload critique, watermark/account-prompt claims (competitor source, weighted low)
- Google-indexed capture of smallpdf.com/pricing showing "Pro $15.00/month"
- Trustpilot (smallpdf.com, ~4★/4,900+) via search summary; Reddit r/pdf threads 1nwmfos (refunds) and 1q86e1y (subscription caution)
- Reddit: r/DataHoarder + r/pdf compression threads (incl. "Compressing PDFs like SmallPDF using Ghostscript"); r/SmallYoutubers Smallpdf tutorial; r/YouShouldKnow Smallpdf post (via search)
- Google Workspace Marketplace: Smallpdf add-on listing
- blog.smallpdf.tools (scanned-PDF compression blur admission); smallpdf.com/blog merge/compress/sign workflow articles (via search)
- Microsoft Store listing 9PBFK53T69ZK (desktop app, Internet-connection permission)
