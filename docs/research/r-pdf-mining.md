# Research Report: r/pdf Community Mining for GlyphPDF

**Date:** 2026-09-07
**Requested by:** GlyphPDF 17-agent research wave (community-signal agent)
**Research question:** What can GlyphPDF learn from reddit.com/r/pdf — pain points with existing tools, feature demand, tool-switching triggers, praise patterns, and vertical/compliance signals — mapped against GlyphPDF's offline-first, local-only Windows architecture (C++17/Qt6; capabilities per PRD v1.3.1; audit baseline avg parity ~4.9/10)?

**Method & limitations:** 16 distinct WebSearch queries (`site:reddit.com/r/pdf` plus targeted adjacent-subreddit probes: r/sysadmin, r/foia, r/hipaa, r/paralegal, r/Archivists, r/therapists, r/HowToHack, r/digitalforensics, r/Revu, r/Acrobat, r/Adobe). Direct thread fetches (WebFetch + curl to old.reddit JSON) were blocked by Reddit (403/empty), so quotes come from search-index snippets captured per thread — treat exact wording as snippet-grade, thread existence + theme as verified. Thread frequency = count of distinct threads surfaced per theme across queries; it is a demand signal, not a census. Verdicts per research-specialist 6-level scale; Reddit is low-tier corroboration, so nothing is graded above MOSTLY_TRUE on r/pdf evidence alone.

---

## Executive Summary

r/pdf's signal is unusually aligned with what GlyphPDF already is. The community's four loudest recurring screams are (1) anger at Acrobat's ads/upsells/bloat (12+ threads), (2) rage at subscriptions and hunger for one-time-purchase licenses (6+ threads, the top-voted answer being PDF-XChange's perpetual model), (3) fear of uploading confidential PDFs to online tools, with users actively auditing "no upload" claims in DevTools (10+ threads), and (4) terror of "fake redaction" — black boxes that leave text copyable — with 17 threads across r/pdf and compliance-adjacent subs on true redaction and verification. Meanwhile the tools r/pdf actually loves (PDF-XChange, PDF24, NAPS2, PDF Arranger, Stirling-PDF, OCRmyPDF) are exactly the species GlyphPDF belongs to: local, lightweight, honest, perpetual. The three biggest risks the data exposes for GlyphPDF: this community *tests* claims (fake controls, inert sliders, mislabeled exports would be found and publicly shamed — GlyphPDF's audit found many such dead controls, so P0 wiring fixes are a prerequisite to any r/pdf-targeted marketing); the "local/no-upload" niche is getting crowded with indie launches (4+ "I made a local PDF editor" show-off threads recently); and GUI-tool unreliability on large files keeps pushing power users to CLI (Ghostscript/qpdf/OCRmyPDF), a trust bar GlyphPDF must clear to win them back. Single biggest opportunity: own "redaction you can verify, on files that never leave your machine" — the highest-frequency, highest-fear theme in the entire corpus, and GlyphPDF already does true excision locally; it only lacks the proof UX.

---

## Findings

### Mining Target 1 — Pain Points

#### Finding 1.1: Acrobat ads/upsells in a paid product — the single most concentrated anger
**Thread frequency:** 12+ threads (1 direct r/pdf + 11 adjacent: r/Acrobat, r/Adobe ×3, r/graphic_design, r/sysadmin ×3, r/software, r/firefox, r/opensource).
**Representative quotes:**
- r/pdf: "Anyone know how to suppress all the ads in Acrobat?" — commenters note the ads are for Adobe's own services and appear even for paying users. https://www.reddit.com/r/pdf/comments/1m9btl1/anyone_know_how_to_suppress_all_the_ads_in_acrobat/
- "Adobe Acrobat Reader is the worst piece of software ever … now it's sluggish, got all these dumb pop ups and things you need to close before seeing the full document." https://www.reddit.com/r/Acrobat/comments/1fz79ym/
- "The most bloated, buggy, piece of garbage software I have ever used. Each year, it gets worse." https://www.reddit.com/r/Adobe/comments/1tc36if/
- "Bloated and laggy and slow, plus persistent annoying popups all the time." https://www.reddit.com/r/graphic_design/comments/17y0d4d/
**Verdict:** MOSTLY_TRUE (multi-subreddit, multi-year, high-engagement corroboration).
**Implication:** An ad-free, upsell-free, fast UI is not table stakes in this market — it is a headline differentiator. GlyphPDF should put "no ads, no upsells, no account" in the first sentence of any r/pdf-facing positioning.

#### Finding 1.2: Subscription anger; the phrase-level demand is "one-time purchase"
**Thread frequency:** 6+ threads.
**Representative quotes:**
- Thread title as quote: "Any pdf editor with 1 time purchase instead of milking us until we die?" https://www.reddit.com/r/pdf/comments/1oriuc1/
- Users switching after Adobe's subscription move seek one-time-purchase OCR + editing: https://www.reddit.com/r/pdf/comments/1n7c2ad/
- "What should I use?" — user frustrated that adding an image requires a subscription: https://www.reddit.com/r/pdf/comments/14428s5/
- Scam warning: "pdfmaster.app" quietly enrolled one-time buyers into subscriptions — proof this community polices licensing honesty: https://www.reddit.com/r/pdf/comments/16k61kz/
**Verdict:** MOSTLY_TRUE.
**Implication:** Pricing model is a feature. A perpetual license (with clearly-scoped, non-expiring rights — mirroring PDF-XChange's "license never expires, only maintenance") is the r/pdf-native monetization; anything auto-renewing invites the scam-thread treatment.

#### Finding 1.3: GUI tools are unreliable on big/mixed jobs, so power users fall back to CLI
**Thread frequency:** 8 threads (compression) + 3 (merge/bates) + 7 (password ops).
**Representative quotes:**
- "Free tools keep destroying formatting when merging mixed file types… crash on large files." https://www.reddit.com/r/pdf/comments/1t5byjk/
- Benchmark thread "Compress my PDF": users compare mutool/pikepdf/qpdf because GUI compressors disappoint. https://www.reddit.com/r/pdf/comments/1oljp20/
- "Compressing PDF without server-side processing" — Stirling-PDF praised as best for confidential docs. https://www.reddit.com/r/pdf/comments/1pnqkjp/
- Batch password stripping asked for and answered with scripts/APIs: https://www.reddit.com/r/pdf/comments/1d5o7as/
**Verdict:** MOSTLY_TRUE for the complaint; PARTLY_TRUE for "CLI is the only answer" (NAPS2/PDF-XChange/Stirling also serve it).
**Implication:** The GUI-shaped hole in the market is "trusted batch operations on large files with honest progress and real results." GlyphPDF's batch/hot-folder engine targets exactly this — but the audit found its compress controls inert and batch OCR locked to English; shipping that state to this community would convert advocates into detractors.

#### Finding 1.4: Text editing "wrecks formatting" — users' baseline expectation is broken
**Thread frequency:** 7 threads.
**Representative quotes:**
- "PDF editors that don't wreck formatting when you edit text" https://www.reddit.com/r/pdf/comments/1w6d3ws/
- "Why is it that when I try to edit a PDF… every letter [moves]" — chunked-text explanation. https://www.reddit.com/r/pdf/comments/ubfskb/
- Font-subset problem on edits: https://www.reddit.com/r/pdf/comments/1pjczbr/
**Verdict:** MOSTLY_TRUE (community even correctly self-diagnoses the format's chunked-text nature).
**Implication:** Fidelity-per-edit plus an honest reflow warning is a differentiator users can feel immediately; silent reflow is the complaint, not reflow itself. PRD §9.2 "warn on reflow risk" should be surfaced as a visible, named feature.

### Mining Target 2 — Feature Requests

#### Finding 2.1: Redaction is the largest single feature demand — and the demand is for *proof*, not just removal
**Thread frequency:** 17 threads (9 r/pdf + 8 compliance-adjacent). Largest theme in corpus.
**Representative quotes:**
- "Any good offline secure pdf redaction tool?" https://www.reddit.com/r/pdf/comments/1r0fwna/
- "PDF Redaction App that runs locally?" https://www.reddit.com/r/pdf/comments/1qgdg1n/
- "I want to redact PDFs offline, is there a free option?" https://www.reddit.com/r/pdf/comments/igbslg/
- "How do you remove the black boxes on a redacted document?" — fake-redaction exposé. https://www.reddit.com/r/HowToHack/comments/1qrrvl5/
- r/paralegal: "Know — and test — to be sure your redaction efforts actually work." https://www.reddit.com/r/paralegal/comments/1g1cf4b/
- "What does proper redaction mean beyond blacking out text?" — metadata/hidden-layer risks. https://www.reddit.com/r/digitalforensics/comments/1r2x6ec/
**Verdict:** MOSTLY_TRUE.
**Implication:** GlyphPDF already does true excision locally (PRD §9.8, audit-verified) and hard-blocks redacting signed files. The unmet half is *evidence*: post-apply verification (attempt to select/search redacted terms), a what-was-removed report, and a bundled metadata sanitize pass (audit P0). "Verify your own redaction" is a feature nobody names but everybody performs by hand.

#### Finding 2.2: OCR demand is local, repeatable, and layout-preserving; language breadth assumed
**Thread frequency:** 7-8 threads.
**Representative quotes:**
- Consensus summary: "OCRmyPDF/Tesseract for local, repeatable searchable PDFs… ABBYY/Adobe when you want a polished desktop tool." https://www.reddit.com/r/pdf/comments/1sxj0w9/
- "Best OCR tool/service for low quality PDF books" — OCRmyPDF best FOSS, ABBYY accuracy king. https://www.reddit.com/r/pdf/comments/17gdvg0/
- NAPS2 recommended as free "scan + OCR like Acrobat": https://www.reddit.com/r/pdf/comments/1tflt9d/
- r/FamilyMedicine: clinician wants 250-page scanned patient records processed locally (HIPAA constraint): https://www.reddit.com/r/FamilyMedicine/comments/1kl67va/
**Verdict:** MOSTLY_TRUE.
**Implication:** GlyphPDF's dual-engine ROVER OCR is architecturally stronger than anything the community currently recommends for local use — but only after the audit's P0 fixes (language selector is UI theater; interactive OCR doesn't persist the text layer; batch OCR locked to English). The bar to beat in user minds is "OCRmyPDF, but with a GUI."

#### Finding 2.3: Forms — Acrobat's "Prepare Form" auto-detection is the named gold standard; free world has no answer
**Thread frequency:** 10-11 threads.
**Representative quotes:**
- "How can I add fillable fields to a PDF for free?" https://www.reddit.com/r/pdf/comments/1lmsbh8/
- "What's the best tool to turn a Word doc into a fillable PDF form?" — answer: Acrobat Prepare Form auto-detect. https://www.reddit.com/r/pdf/comments/1lzbn2x/
- Indie devs shipping AI auto-field detection (Instafill.ai; DullyPDF "detects input areas") — demand validated by builders: https://www.reddit.com/r/pdf/comments/1nxrnq1/ , https://www.reddit.com/r/pdf/comments/1rlacaz/
- "Saving a fillable PDF so it is still fillable" (flatten-vs-keep confusion): https://www.reddit.com/r/pdf/comments/1du2g0n/
**Verdict:** MOSTLY_TRUE.
**Implication:** GlyphPDF's auto-detect currently returns 3 hardcoded dummy fields (audit §9.6) — the exact anti-feature this demand cluster centers on. A real heuristic detector with review-before-commit would meet the strongest named expectation in the forms space; until then, forms must not be marketed on r/pdf.

#### Finding 2.4: Smaller unmet asks that fit GlyphPDF's batch pipeline
**Thread frequency:** 1-3 threads each.
- Batch split into individual pages: https://www.reddit.com/r/pdf/comments/1es1l2s/
- Batch digital signing (answered by shell-extension tools): https://www.reddit.com/r/pdf/comments/1kjr8x4/
- Desktop "send PDF for signing" (only Foxit named): https://www.reddit.com/r/pdf/comments/1jj0zyw/
- Batch password/permissions stripping: https://www.reddit.com/r/pdf/comments/1d5o7as/ , https://www.reddit.com/r/pdf/comments/jlgul2/
- Page color/inversion for reading scanned books (PDF-XChange singled out as the only one): https://www.reddit.com/r/pdf/comments/1ssbvjm/
- PDF Portfolios creation without Acrobat: https://www.reddit.com/r/pdf/comments/1sosrdk/
**Verdict:** PARTLY_TRUE (single-thread evidence each; directionally consistent with batch/ops demand in 1.3).
**Implication:** These are cheap roadmap line-items once batch ops are trusted; page-color content inversion also matches the audit's P1 night-mode gap (§9.1).

### Mining Target 3 — Tool-Switching Stories

#### Finding 3.1: The switch is Adobe → PDF-XChange (perpetual, generous free tier); Foxit is losing its "default alternative" status
**Thread frequency:** 9 threads PDF-XChange praise/comparison; 7 threads Foxit/Nitro complaints.
**Representative quotes:**
- "PDF-XChange Editor is so awesome" — ~20-year user after testing others: https://www.reddit.com/r/pdf/comments/1uf7mtm/
- License-model clarity praise: perpetual license "never expires; only maintenance expires": https://www.reddit.com/r/pdf/comments/1rs4ey4/
- Foxit decline: "the 'lightweight' PDF reader is slowly turning into Acrobat: stupid updater; bloated; slow to load." https://www.reddit.com/r/sysadmin/comments/1k4mqgq/
- Foxit enterprise: "unstable and crashes on regular basis (still a 32-bit app)." https://www.reddit.com/r/sysadmin/comments/1foeprp/
- Feature-stripping anger: "Foxit - What the hell is this?" (editing moved behind subscription): https://www.reddit.com/r/software/comments/1q6pjsf/
- Nitro: "Nitro PDF is awful. Not user friendly. Program is glitchy. It's like using Windows 07 trying to edit PDFs." https://www.reddit.com/r/pdf/comments/1fyb3ee/
- "Foxiit messed up my PDFs": https://www.reddit.com/r/pdf/comments/1dirdbm/
**Verdict:** MOSTLY_TRUE.
**Implication:** The winning wedge narrative is documented: incumbent enshittification (ads, subscription, feature-stripping) → switch to a lightweight perpetual tool that respects the user. GlyphPDF can be the *next* chapter of this exact story. The cautionary tale is equally clear: the moment updates nag, features get fenced, or the app fattens, r/pdf will write Foxit's next thread about whoever replaces Acrobat.

#### Finding 3.2: "What drove them back": nothing — they keep a browser for reading
**Thread frequency:** 3+ threads (r/sysadmin alternatives threads; browsers as default readers).
**Representative:** "Alternatives to Adobe Acrobat Reader" — consensus: browsers handle viewing; only editing needs an app. https://www.reddit.com/r/sysadmin/comments/1qdtqhl/
**Verdict:** MOSTLY_TRUE.
**Implication:** Do not spend polish budget competing with browsers on plain viewing; win on the editing/security/batch jobs browsers can't do, with open-in-instant speed as hygiene.

### Mining Target 4 — Praise Patterns (the delight bar)

#### Finding 4.1: What r/pdf genuinely loves — local, free-tier-generous, no-account, does-one-thing-well
**Thread frequency:** 10 threads ("best alternative" umbrella) + tool-specific above.
**Representative quotes:**
- "I use PDF24 Tools. Free, and works better than Adobe." https://www.reddit.com/r/pdf/comments/1ugyrdh/
- PDF Arranger praised for visual page organization (drag thumbnails): https://www.reddit.com/r/pdf/comments/1qnexcl/
- PDFgear "most PDF functions included, free for business use": https://www.reddit.com/r/pdf/comments/1c4nkhq/
- Stirling-PDF (self-hosted) "best overall PDF toolset… for confidential documents": https://www.reddit.com/r/pdf/comments/1pnqkjp/
- "Made a free PDF editor because I was tired of creating accounts" — community cheers account-free tools: https://www.reddit.com/r/pdf/comments/1uavryi/
**Verdict:** MOSTLY_TRUE.
**Implication:** The delight bar: instant open, drag-thumbnail page ops, zero-account everything, generous free tier, and visible honesty. Audit P0 "enable thumbnail-grid drag-and-drop reorder" is literally the praised interaction of the community's favorite page tool (PDF Arranger).

#### Finding 4.2: This community verifies claims — DevTools audits and redaction self-tests are normal behavior
**Thread frequency:** 3+ meta threads.
**Representative:** "Do not upload your pdfs to random third party websites" — recommends verifying "no upload" tools by going offline with DevTools Network open: https://www.reddit.com/r/pdf/comments/1o2tr2h/
**Verdict:** MOSTLY_TRUE.
**Implication:** GlyphPDF's audit found UI theater (inert sliders, hardcoded OCR language, mislabeled .docx fallback). In this community those are not minor bugs; they are the scam-thread material. Ship P0 wiring fixes before any r/pdf engagement.

### Mining Target 5 — Vertical / Compliance Signals

#### Finding 5.1: Legal — batch Bates + redaction verification are workflow-critical
**Thread frequency:** 5 Bates threads + redaction-verification threads above.
**Representative:**
- "Acrobat Pro alternatives for batch Bates Numbering": https://www.reddit.com/r/pdf/comments/1sm47um/
- Merge→Bates→split workflow ask: https://www.reddit.com/r/pdf/comments/1hqvnuf/
- Free tools "crash on large files or ruin formatting" on Bates jobs: https://www.reddit.com/r/pdf/comments/1t5byjk/
**Verdict:** PARTLY_TRUE (sparse but consistent, cross-sub corroborated by r/paralegal).
**Implication:** GlyphPDF has Bates but single-document only (PRD §9.9 gap: cross-document batch Bates). Cross-document Bates + batch split closes a named, monetizable legal workflow.

#### Finding 5.2: Healthcare — HIPAA-redaction and local OCR for patient records (adjacent subs, not r/pdf itself)
**Thread frequency:** 4 threads (r/hipaa, r/therapists, r/FamilyMedicine, r/pdf 1vk8447 mention).
**Representative:**
- "HIPAA compliant redaction for medical records?" https://www.reddit.com/r/hipaa/comments/1ozpcw3/
- "HIPAA Compliant PDF Editor" (therapist, affordable, signing for patient forms): https://www.reddit.com/r/therapists/comments/1pbgjmu/
- Clinician OCR-ing 250-page records (see 2.2).
**Verdict:** PARTLY_TRUE.
**Implication:** "Never leaves your machine" is a HIPAA talking point sales teams can use verbatim; redaction + OCR + forms + signing is the exact bundle these users name.

#### Finding 5.3: Government/FOIA — true-redaction correctness and reason codes; archivists want PDF/A at scale
**Thread frequency:** 6 threads (r/foia, r/it, r/fednews, r/Archivists ×2, r/sysadmin).
**Representative:**
- "How to perform FOIA redaction for sensitive information?" https://www.reddit.com/r/foia/comments/1q4m4xm/
- "For long-term storage… tens of thousands of scanned PDF documents… 25-50+ years": https://www.reddit.com/r/Archivists/comments/qnewr8/
**Verdict:** PARTLY_TRUE (thin on r/pdf proper; strong in specialist subs).
**Implication:** Audit P1 items "redaction reason codes/overlay text" and "audit log with pattern/category counts" map directly to FOIA practice; GlyphPDF's veraPDF-validated PDF/A writer is a real answer to the archivist thread, currently invisible.

---

## Top 10 Build/Opportunity Recommendations
Ranked by demand frequency × fit with GlyphPDF's offline-first architecture.

1. **Redaction Trust Bundle — excision + sanitize + self-verify + report.** Bundle the audit-P0 metadata sanitize into Apply; add a post-apply "Verify redaction" mode (attempt select/search for redacted terms; show what was removed incl. metadata); per-pattern audit log. Demand: 17 threads (largest theme). Fit: perfect — true excision already local and real; only proof UX missing. This is the flagship.
2. **Ship every audit-P0 "dead control" fix before any community-facing marketing.** OCR language wiring, interactive-OCR save path, compress quality/DPI sliders, compare entry point, search checkboxes, thumbnail drag-drop, real OOXML exports. Demand: meta (community punishes fake UI — threads 1o2tr2h, 16k61kz, 1g1cf4b). Fit: these fixes are the license to operate in r/pdf.
3. **Perpetual-license pricing with PDF-XChange-shaped honesty.** Non-expiring license, optional maintenance, free tier that isn't crippled, zero ads. Demand: 6+ threads with "milking us until we die" as the theme's banner. Fit: revenue-model decision, zero engineering.
4. **Real batch compression matching the Ghostscript/Smallpdf bar.** Implement actual JPEG re-encode/downsample (audit P0-L), target-size mode, honest before/after readout. Demand: 8 threads; users currently flee to CLI. Fit: strong — stays local, absorbs the qpdf/ghostscript crowd with a GUI.
5. **Legal batch pack: cross-document Bates, split-to-single-pages, batch password/permissions strip.** Demand: 5 + 1 + 7 threads respectively. Fit: perfect — all are batch-pipeline operations over the existing hot-folder engine; audit already flags cross-document Bates as the gap.
6. **"Fast and honest on large files" engineering + messaging.** Real progress, cancel, non-blocking ops, no silent merge failures (audit P0s in §9.9/§9.12), publish a large-file benchmark. Demand: recurring crash/formatting-loss complaints (1t5byjk et al.). Fit: strong differentiator vs the free-tool field.
7. **Fidelity-first text editing with visible reflow warnings + font-subset handling.** Demand: 7 threads. Fit: strong; PRD already specifies the warning; make it a named feature ("edit without wrecking the page").
8. **OCR: "OCRmyPDF with a GUI."** Wire languages (P0), persist text layer (P0), batch language selection, low-confidence flagging (P1), then market the dual-engine ROVER advantage vs the community's Tesseract default. Demand: 7-8 threads. Fit: perfect (fully local ensemble already built).
9. **Real auto-detect form fields (heuristic + review-before-commit) to meet the "Prepare Form" expectation.** Demand: 10-11 threads. Fit: strong; replaces the audit's worst stub (3 dummy fields) with the single most-requested forms capability.
10. **Say the quiet part: "no account, no upload, no ads, no telemetry" everywhere + PDF/A archival badge for archivists.** Demand: 10 privacy threads + 4-6 archival threads. Fit: costless marketing of verified architecture (audit P2 marketing items, unified).

## What GlyphPDF should NOT build (anti-recommendations)

1. **No cloud companion / upload processing / "secure link" service.** The community's most repeated advice is *don't* upload; users DevTools-audit claims. (Threads: 1o2tr2h, egk68e, 1n74r0y, 1l2181d.) PRD's local-only stance is validated — do not erode it. Verdict: MOSTLY_TRUE.
2. **No generic "AI chat with your PDF" feature.** r/pdf's AI mentions are narrowly about OCR accuracy (Textract/Vision) and auto-form-field detection — not assistants. AI bloat is precisely the enshittification pattern users flee. Verdict: PARTLY_TRUE (absence of demand; single-thread AI-interest evidence 1mjc54a).
3. **No mobile-first or web-first investment now.** Praise concentrates on Windows desktop + the rare mobile mention is secondary; browsers own casual viewing. Keep desktop the battleground. Verdict: MOSTLY_TRUE.
4. **No free-tier ambush mechanics — no ads, no nags, no late feature-stripping.** Foxit's decline threads show the community remembers and punishes exactly this; Nitro's "glitchy" thread shows unstable = dead. Verdict: MOSTLY_TRUE.
5. **Don't chase PDF Portfolios** (1 thread, niche Acrobat-ism). Verdict: PARTLY_TRUE.

## Confidence Level
**High** for theme presence and direction (multi-thread, multi-subreddit, internally consistent). **Medium** for thread-frequency precision (search-index sampling, not a full corpus crawl; Reddit blocks unauthenticated fetches, so quotes are snippet-grade). **Low** for r/pdf-specific PDF/A demand (signal lives in adjacent subs).

## Sources
- r/pdf threads: 1m9btl1, 1oriuc1, 1n7c2ad, 14428s5, 16k61kz, 19evl3m, 1r0fwna, 1qgdg1n, 1pudaqz, 1ewdrd3, 1onmqst, mlpp8m, 17eflit, igbslg, 1dwpjcf, 1sxj0w9, 17gdvg0, 1i7md7n, 1mjc54a, 14lkm94, 1t16rgf, 1tflt9d, 1lmsbh8, 1du2g0n, 1awtcyx, 1o50fve, 1kalskj, 1lzbn2x, 1nxrnq1, 1q0tqo5, 1klzud2, 1rlacaz, 1uf7mtm, lw6gaw, 1nms75g, 1ssbvjm, 1rs4ey4, 1gv9j0b, 1ei02it, 1k8sgp7, 103zq2a, 1ugyrdh, 1sja7un, 1mqhk7u, 1c4nkhq, 1s732v7, 1sosrdk, 1rq0mbw, 1s33jzs, 1fn90do, 149bx67, 1pnqkjp, yyib8e, 1oljp20, 1ndsqb7, 13xleaw, 1nctlr5, 1h1iai4, 1es1l2s, 1qnexcl, 1jikwhf, 1vfximy, 1df3y51, 1q7w3fq, 1c1jj45, 18c8937, 1icy7wr, 1m8kk4t, 1cnkleb, m0y0cp, 1jj0zyw, 1kjr8x4, 114ffl5, uice3s, ubfskb, 1slyk16, 1snrzd5, 1pjczbr, 1w6d3ws, 1lljn0l, 1r4w7rc, 1sm47um, 1hqvnuf, 1t5byjk, 1vic2lf, 1tpb2rx, 1fyb3ee, 1dirdbm, 1lsyegx, 15r1a55, upy6dl, r8c9r4, 1d5o7as, jlgul2, 1i369as, 1o2tr2h, egk68e, 1swon3q, 1w3lgc6, 1uavryi, 1p52gou, 1n74r0y, 1l2181d, 1psq6ql, 1vk8447 (all at https://www.reddit.com/r/pdf/comments/{id}/)
- Adjacent-subreddit corroboration: r/sysadmin (1jiui4v, 1m7hpjw, 1qdtqhl, 1foeprp, 1k4mqgq, 1uivfkk, 1jagm32, 14m7qux), r/Acrobat (1fz79ym), r/Adobe (1tc36if, 1aes3vr, 1ol6sjk), r/graphic_design (17y0d4d), r/software (193gzyb, 1q6pjsf), r/firefox (3btjdm), r/opensource (1bu1gdi), r/foia (1q4m4xm), r/it (1pc3cym), r/fednews (1ks2jzy), r/paralegal (1g1cf4b), r/HowToHack (1qrrvl5), r/digitalforensics (1r2x6ec), r/MacOS (1oo7vq5), r/Revu (1g1klu3), r/hipaa (1ozpcw3, 1c14nij), r/therapists (1pbgjmu), r/FamilyMedicine (1kl67va), r/Archivists (qnewr8, q7g76o), r/DataHoarder (19b73gl), r/LaTeX (t3efuu), r/complaints (16bsp7v)
- Internal baselines: C:\Users\User\Projects\pdf\PRD.md (v1.3.1 implementation status); C:\Users\User\Projects\pdf\docs\audit\COMPETITIVE-PARITY-AUDIT-2026-07-01.md (scorecard + P0/P1/P2 plan)
