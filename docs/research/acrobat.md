# Spec Sheet: Adobe Acrobat (Pro / Standard, current DC + Acrobat 2024) — desktop capabilities

**Date:** 2026-09-07
**Requested by:** coordinator (GlyphPDF parity research)
**Research question:** What desktop capabilities does Adobe Acrobat ship, per domain, with edition gating, version history, cloud dependence, real-behavior notes — as a machine-comparable spec sheet against GlyphPDF's offline-first baseline (ledger 2026-09-05 + PRD §27).
**Format:** Coordinator override — feature-by-feature spec tables, one row per feature. Verdict grades per research-specialist scale (TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE). "Since" = approximate release; Adobe does not publish a single canonical version map, so approximation is graded.

Edition legend: **All** = free Reader + paid; **Paid** = Standard + Pro; **Pro** = Pro only; **Sign** = Acrobat Sign add-on/subscription; **Cloud** = requires Adobe cloud service (anti-feature for GlyphPDF).

---

## 1. Viewing & navigation

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes (real behavior, limits, quirks) |
|---|---|---|---|---|---|---|
| View modes | Single page, continuous, two-page, two-page scroll, rotate | All | Acrobat 3+ | N | TRUE | helpx viewing basics; stable for decades |
| Multi-document tabs | Open many PDFs in tabs, split view | All | DC 2015 | N | TRUE | Tabbed UI since DC; split-window in later DC builds |
| Navigation panes | Thumbnails, bookmarks, attachments, comments, layers, content | All | Acrobat 3+ | N | TRUE | Panes dockable; layers pane see §2 |
| Advanced search | All PDFs in a folder / disk; criteria (proximity, stem); index-aware via .pdx | All | Acrobat 5+ | N | TRUE | Searches without index too (slow); .pdx makes it fast — see Print/Index rows |
| Dark mode / theme | Full dark UI incl. ribbon | All | 2023–2024 builds | N | MOSTLY_TRUE | Shipped across the New Acrobat rollout; was a top user request for years |
| Star / pin files | Star important files on Home | All | Acrobat 2024 / DC 2024 | N | TRUE | Listed in Adobe "New features summary — Acrobat Pro 2024" |
| Read aloud | Built-in TTS reading of document | All | Acrobat 7+ | N | MOSTLY_TRUE | Basic SAPI-based; not a screen-reader replacement |
| Liquid Mode | AI reflow of PDFs for mobile reading | Reader/mobile | 2020 | Y | TRUE | Cloud AI service; desktop parity absent — anti-feature for offline products |
| OCG layers panel | Show/hide, reorder, layer properties, "apply print overrides" | All (view) / Pro (some edits) | Acrobat 6 (2003) | N | MOSTLY_TRUE | Reorder-by-drag documented but community-reported flaky ("can't move OCGs" thread); no first-class "new layer" UI button — layers created via authoring or JS (OCG API) |
| Geospatial viewing | Find location, mark location, measure on map PDFs | Pro | Acrobat 9 (2008) | N | TRUE | helpx "geospatial-pdfs"; known quirk: measuring tool auto-switches to "Geospatial Distance" and blocks manual scale — recurring community complaint |
| Snapshot / magnifier | Raster snapshot of region; loupe | All | old | N | MOSTLY_TRUE | Long-standing utility tools |
| Accessibility of viewer | Screen-reader (NVDA/JAWS) support, high-contrast, keyboard | All | old | N | TRUE | Reader is the world's most screen-reader-tested PDF viewer |

## 2. Editing (text / object / page)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Edit text in PDF | Click-to-edit paragraph text, reflow, font/size/color, bullet lists | Paid | Acrobat XI (2012, modern form) | N | MOSTLY_TRUE | Standard includes Edit PDF; reflow quality best on well-tagged/single-font runs; composite/embedded-subset fonts degrade to substitute fonts — the perennial complaint |
| Edit scanned text | OCR then edit in place | Paid | XI/DC | N | TRUE | Uses the OCR engine (§3) |
| Add text/objects | Add text box, image, shape, draw | Paid | XI/DC | N | TRUE | "Add text" tool in global bar |
| Image/object editing | Flip, crop, rotate, replace, arrange z-order, align | Paid | DC | N | MOSTLY_TRUE | helpx "modify-image" / Objects section |
| Edit in external editor | Hand off image to Photoshop/Illustrator, round-trip | Paid (needs external app) | old (TouchUp) | N | PARTLY_TRUE | helpx documents "Edit Using" handoff; requires the external app installed; hard dependency on Adobe CC for full value |
| Page organization | Insert, replace, extract (single, range, discontinuous), split (by pages/size/top-level bookmarks), delete, rotate, drag-reorder | Paid | extract discontinuous pages: 2024 | N | TRUE | Pro 2024 release notes add discontinuous-page extraction; split-by-file-size is the power feature worth noting |
| Page labels | Editing page-label ranges | — | — | N | UNVERIFIABLE | No first-class UI in Acrobat (JS-only via `doc.setPageLabels`); perceived as a gap even in Acrobat |
| Headers/footers/backgrounds/watermarks | Add/update/remove all four, saved profiles | Paid | Acrobat 6+ | N | TRUE | Update-overwrite-existing is supported; profiles persist |
| Bates numbering | Add/remove Bates with prefixes/suffixes, batch over files | Pro | Acrobat 8/9 era | N | MOSTLY_TRUE | Consistently listed Pro-only in Adobe comparison + community |
| Crop/see page boxes | Crop tool + Set Page Boxes (crop/trim/bleed/media/art) | Paid / Pro for boxes | old | N | MOSTLY_TRUE | Set Page Boxes lives under Print Production (Pro) |
| Opacity/appearance control on added content | Opacity, blend of added shapes/objects | Paid | DC | N | PARTLY_TRUE | Opacity slider exposed in appearance properties; full blend-mode UI (as in InDesign) NOT exposed — Acrobat does not offer blend modes for edits in UI |
| Flatten file | Full flatten to images | — | — | N | UNVERIFIABLE | No first-class flatten button; users resort to Print-to-PDF or JS — an Acrobat weakness worth exploiting |

## 3. OCR

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Recognize text in scans | Output forms: searchable image, searchable (MRC-ish), editable text+images; page-range selection; auto-detect scan prompt | Paid | Acrobat 3+ (modern engine 9+) | N | TRUE | Desktop engine; no cloud needed |
| OCR language coverage | Standard: ~19 languages; Pro: ~3x (CJK, Cyrillic, E. European, Thai…) | Paid (gated) | 2017/2020 era matrix, still cited | N | MOSTLY_TRUE | Adobe compare-versions page historically lists the split; exact counts vary by version — grade capped because the live matrix did not load |
| Document-language detection | Auto language select | Paid | DC | N | PARTLY_TRUE | Present in preferences; multi-language mixed docs are a documented weak spot (community reports) |
| Preprocessing | Deskew/background removal/downsample during recognition | Paid | old | N | MOSTLY_TRUE | Via scan-optimize settings ("Custom settings": remove deskew etc.) |
| Form-recognition via OCR | Detect fields on scans in Prepare Form | Pro | DC | N | MOSTLY_TRUE | Auto-detect works on scanned layouts; quality varies |
| Comparing to GlyphPDF | Acrobat OCR is single-engine; GlyphPDF ROVER ensemble (Tesseract+PP-OCRv5+RapidOCR fusion) | — | — | N | UNVERIFIABLE (relative quality) | No neutral benchmark found; do not claim superiority without one |

## 4. Forms (creation, filling, calculation, JS)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Prepare Form (authoring) | Open any PDF, add all field types | Pro | XI/DC | N | MOSTLY_TRUE | Community + comparison consensus: Standard fills, Pro authors; Adobe's marketing copy sometimes blurs this |
| Auto field detection | Content-aware placement of detected fields (rules/lines) | Pro | XI/DC | N | TRUE | Runs on open of an untagged PDF in Prepare Form; imperfect on complex tables |
| Field types | Text, checkbox, radio, list/combo, button, image field, date field (2023+), digital signature, barcode | Pro | barcode: Acrobat 9 | N | TRUE | Barcode (2D PDF417) fields are documented and need "auto calculate" preference on; date fields added in the 2023 "new forms" work |
| Calculation engine | Preset sum/product/avg/min/max + custom calculation script; calculation-order dialog | Pro | old | N | TRUE | helpx "set-calculation-fields"; order defaults to creation order and must be fixed via More > Set Field Calculation Order — classic gotcha |
| JavaScript event model | Calculate/Validate/Format/Keystroke per field; document-level JS; actions (mouse up/down etc.) | Pro (authoring), Reader executes if rights allow | Acrobat 4/5+ | N | TRUE | Full AcroJS API reference on opensource.adobe.com; the #1 power-user dependency in enterprise forms |
| Formatting/validation | Number/date formats, keystroke masks, required/readonly, rich text | Pro | old | N | TRUE | Field Properties dialog |
| Tab order | By structure, row, column, manual | Pro | XI | N | MOSTLY_TRUE | Document tools |
| Fill & fill-assist | Highlight fields, auto-complete from store | All (fill) | old | N | TRUE | Auto-complete of personal info in Reader/Acrobat |
| Reader-enabling (usage rights) | Allow Reader users to save filled forms | Paid | Acrobat 8+ | N | PARTLY_TRUE | Long-standing feature; current edition gating and limits not re-verified — needs primary-source check before building equivalent claims |
| Form data import/export | FDF/XFDF/XML/CSV (mostly via JS: `exportAsFDF` etc.) | Pro | old | N | MOSTLY_TRUE | UI exposure is minimal; scripts carry it — Acrobat's UI is weaker than it looks here |
| Distribute/track forms (Responses) | Email/link distribution, response tracking | Pro | X/DC | Y (tracking) | PARTLY_TRUE | Distribution tracker leans on Adobe services — cloud-adjacent; local PDFs remain local |
| Flatten forms | Flatten filled forms | — | — | N | UNVERIFIABLE | No clean one-click flatten in Acrobat UI (print-to-PDF or JS); opportunity |

## 5. Comments & markup

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Core annots | Sticky note, highlight/underline/strike/squiggly, text box, callout, shapes, pencil, stamps | All | old | N | TRUE | Reader can annotate |
| Dynamic stamps | Auto date/time/user fields in stamps; custom stamp creation | All | old | N | TRUE | Custom dynamic stamp creation is JS-heavy — power-user niche |
| File/audio attachments on annots | Attach files or recorded audio to comments | Paid | old | N | TRUE | Audio comment requires mic |
| Comment pane management | Sort/filter, threaded replies, statuses (accepted/completed/cancelled/set by), search comments, print/export summary (PDF/Word) | All | statuses XI; pane DC | N | TRUE | Summary export produces a tabulated report document |
| @mentions / notify | Mention collaborators, cloud notifications | All | 2024 | Y (notify) | MOSTLY_TRUE | Mention markup exists; delivery uses Adobe services — anti-feature part is separable |
| Share for review / Send & Track | Shared cloud review, link sending, tracking views | Paid | DC 2015 | Y | TRUE | Fully cloud — anti-feature; "Send & Track" is one of Acrobat's most-used cloud glue features |
| Comment sync across devices | Annots follow the document via cloud storage | All | DC | Y | TRUE | Anti-feature offline |

## 6. Redaction

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Mark for redaction | Draw boxes over text/images; page-level mark; multiple marks reviewed before apply | Pro | Acrobat 8 (2006) | N | TRUE | Two-step mark-then-apply with preview |
| Search-and-redact | Patterns: phone, SSN, email, credit card, custom pattern; select-all matches | Pro | Acrobat 8/9 | N | TRUE | Custom pattern = regex-like; GlyphPDF parity (§9.8 word-list/pattern) exists |
| Apply redaction | Permanent content excision on apply, optional overlay text/redaction codes | Pro | 8+ | N | TRUE | Excises text streams; Adobe positions this as legally safe removal |
| Sanitize document | Remove metadata, attachments, JS, hidden layers, embedded search index, private data — one click; "Remove hidden information" granular panel | Pro | X/DC | N | MOSTLY_TRUE | Granular panel is Pro; Acrobat X introduced the single-button Sanitize |
| Batch redaction | Via Action Wizard over many files | Pro | DC | N | TRUE | Action step wraps mark+apply |
| Editing redaction marks | Reopen/edit marks before apply | Pro | DC | N | TRUE | Marks are non-destructive until applied |

## 7. Security & encryption

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Password security | Open password + permissions (print/copy/edit/signing), AES-128/AES-256 | Paid | AES-256 since Acrobat X/XI | N | TRUE | Subset of GlyphPDF §9.11 parity |
| Certificate encryption | Encrypt to recipient public keys (certificate security), multiple recipients, different permission sets per recipient | Paid | Acrobat 7 era | N | MOSTLY_TRUE | Documented "encrypt with certificate"; gating between editions not re-verified — flagged |
| FIPS mode | FIPS 140-2 compliant crypto operations only | Paid | XI | N | MOSTLY_TRUE | Government niche; mode toggle in preferences/admin templates |
| Protected Mode / sandbox | Virtualized operations, Protected View for untrusted files | All | Reader 10 (2010); Pro 2012 | N | TRUE | Major driver of Acrobat's perf complaints (see failure modes) |
| Enhanced security manager | JS/host access restrictions, privileged locations | All | X/DC | N | TRUE | Enterprise-managed via registry |
| Remove hidden info | See Redaction/sanitize row | Pro | X | N | MOSTLY_TRUE | — |
| Security envelopes | Encrypted attachment wrapper | Paid | old | N | PARTLY_TRUE | Legacy; still present; low modern relevance |
| Document restriction inspection | Show security properties, what's allowed | All | old | N | TRUE | — |

## 8. Signatures (digital certs, e-sign, LTV)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Fill & Sign (self) | Draw/type/image signature, initials, save signature, place on any PDF | All (free Reader included) | XI/DC | N | TRUE | The single most-consumed Acrobat feature class |
| Certificate-based digital signing | AATL/EUTL-trusted certs, local self-signed IDs, custom appearance, reason/location | Paid (Reader signs if rights/cert workflow allows) | old | N | TRUE | Signature Panel shows chain + document changes |
| Validation infrastructure | OCSP/CRL revocation checking, AATL/EUTL trust lists | All | X/DC | N (network fetch only) | TRUE | Trust lists update over network but signing is local |
| LTV / DSS | Embed revocation info for long-term validation; timestamp via TSA | Paid | Acrobat X (2010) | N (TSA optional network) | TRUE | Adobe reader-enables LTV; PAdES-compatible profiles |
| Author/certification signatures (DocMDP) | Certify document with change-allowed levels | Paid | Acrobat 7 | N | TRUE | Distinct certify-vs-approve semantics |
| Request e-signatures (Acrobat Sign) | Send to recipients, signing order, reminders, status, audit trail, templates | Sign add-on; limited quota in Acrobat plans | DC 2015+ (EchoSign 2011) | Y | TRUE | Acrobat Sign = 150 transactions/user/year on Teams plans (helpx transaction-limits); individuals marketed "unlimited"; fully cloud — anti-feature |
| In-app signing agreement file | Sign-in-required for Sign features | Sign | — | Y | TRUE | Sign requires Adobe ID — confirms cloud boundary |

## 9. Compare

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Compare Files | Old-vs-new side-by-side, change list, filter (text/images/annotations/formatting), report export | Pro | Acrobat XI Pro (2012); modernized DC 2015 | N | TRUE | helpx title literally "(Acrobat Pro)" |
| Compare scans | OCR-assisted comparison of scanned docs | Pro | DC | N | MOSTLY_TRUE | Documented; quality depends on OCR |
| Overlay comparison | Graphical overlay mode | Pro | DC | N | MOSTLY_TRUE | Alternative diff presentation |
| Reliability | — | — | — | — | PARTLY_TRUE (negative) | Multiple Adobe-community "Compare Files not working" threads (~13K views) — a parity-reliability opening for GlyphPDF's tested DiffEngine |

## 10. Batch / actions (Action Wizard, hot folders)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Action Wizard | Multi-step wizard actions across file sets/folders; pause-and-prompt steps; default actions (Make Accessible, Create PDF/A, OCR scans, Print) | Pro | Acrobat 9 ("Actions"; was Batch Processing ≤8) | N | TRUE | Institutional reliance documented (state DOT scanning manuals built on it) |
| Custom actions/commands | User-defined action chains; Custom Commands in toolbar | Pro | DC | N | TRUE | Commands = single-click versions of actions |
| Preflight droplets | Saved drag-and-drop icons that run a preflight profile+fixups on dropped files | Pro | Acrobat 8 | N | TRUE | helpx "automating-document-analysis-droplets-or" |
| Hot folders / watched folders | NOT built in | — | — | N | TRUE (absence) | No native hot-folder; third-party plug-ins (EverMap AutoBatch etc.) sell what Acrobat lacks |
| CLI / headless | NOT built in (IAC COM automation is the workaround) | — | — | N | TRUE (absence) | Stack Overflow/SO threads confirm no supported command line for actions — GlyphPDF CLI/hot-folder directly beats this |
| Batch OCR/convert/print/protect | As action steps | Pro | DC | N | TRUE | — |

## 11. Print production (preflight, color, marks)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Preflight | Hundreds of profiles; checks (fonts, images, color spaces, boxes, ICC, PDF/X-A-E-VT compliance); reports | Pro | Acrobat 6 Professional (2003) | N | TRUE | helpx "(Acrobat Pro)"; the industry-standard local PDF validator |
| Fixups | One-click repairs (embed/convert fonts, convert colors, flatten transparency, set boxes, add printer marks); custom profiles + fixup chaining | Pro | 6/8 | N | TRUE | Fixup droplets (§10) reuse profiles |
| Output Preview | Separation preview, color-space warnings, total-area-coverage, object inspector, overprint preview | Pro | Acrobat 9 | N | TRUE | helpx "previewing-output-acrobat-pro" |
| Ink Manager | Remap/alias spot inks in preview and output | Pro | 9 | N | TRUE | Lives inside Output Preview |
| Transparency flattener + preview | Flatten styles + preview artifacts | Pro | 6+ | N | MOSTLY_TRUE | Flattener presets; rarely touched by non-print users |
| Trap presets | Trap adjustments | Pro | Acrobat 9 | N | MOSTLY_TRUE | Legacy niche |
| PDF/A creation & validation | Via preflight profiles / save-as standards | Pro | Acrobat 8 | N | MOSTLY_TRUE | PDF/A creation exists in Standard save flows in some versions — gating not fully verified |
| Distiller | PostScript→PDF with job options | Paid (ships with Acrobat) | always | N | TRUE | Separate app bundled; Ghostscript-class workflow |

## 12. Accessibility / tagging

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Accessibility Checker | Full rule tree (tags, alt text, contrast, language, tab order), per-rule results, report | Pro | Acrobat 9 (2008) | N | TRUE | helpx title "(Acrobat Pro)" |
| Reading Order tool (ex-TouchUp) | Visual zone editing of reading order, table editor, tag-as (H1/P/Figure…) | Pro | old (TOCR); current tool DC | N | TRUE | helpx "(Acrobat Pro)" |
| Tags/Content panels | Edit tag tree, add/edit/reorder tags, alt text, child tags, tag comments | Pro | Acrobat 6/7 | N | TRUE | helpx "editing-document-structure-content-tags" |
| Auto-tag (AI) | One-click AI tagging of untagged PDFs | Paid (edition split not verified) | 2022–2023 | Y (AI inference) | PARTLY_TRUE | Documented feature; cloud inference component and edition gating unverified — Adobe pages fetched did not disambiguate |
| Make Accessible (Guided Action) | Step-by-step wizard action for remediation | Pro | XI (as Action); renamed "Guided Actions" ~2024 | N | MOSTLY_TRUE | Renaming observed in 2025 tutorials |
| Set document language/title/display | Document properties for AT | Paid | old | N | TRUE | Also settable in Standard |
| Screen-reader ecosystem | NVDA/JAWS reading of tagged PDFs | All | — | N | TRUE | Depends on tags surviving edits — Acrobat's own edits can strip tags (known industry pain) |

## 13. Import/export formats

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Create PDF | From Office (COM add-ins), images, clipboard, scanner (WIA/TWAIN, both editions) | Paid | old | N | TRUE | Ribbon add-ins for Word/Excel/PPT |
| Export to Word/Excel/RTF/TXT | Layout-preserving conversion | Paid | XI/DC (was ExportPDF service) | N | TRUE | Desktop engine; fidelity complaints exist but broadly works |
| Export to PowerPoint | PPTX export | Pro (commonly listed) | DC | N | PARTLY_TRUE | Listed Pro-only in comparison summaries; not re-verified against live matrix |
| Export images / HTML / EPS-PS | PNG/JPEG/TIFF; HTML (legacy); PostScript/EPS via Distiller/print | Paid | old | N | MOSTLY_TRUE | HTML export aged poorly |
| Markdown/EPUB export | NOT offered | — | — | N | TRUE (absence) | Acrobat has neither — GlyphPDF roadmap target is differentiation, not parity |
| PDF/A/E/X creation | Via standards profiles | Pro (mostly) | 8+ | N | MOSTLY_TRUE | See §11 |
| PDF Optimizer / Reduce file size | Full optimizer (image downsample, font/stream settings, discard objects) Pro; simple Reduce File Size in Standard | Pro / Paid | 6+ / XI | N | MOSTLY_TRUE | Two-tier structure confirmed by UI; exact split per version varies |
| Form data export | FDF/XFDF/CSV/XML via JS | Pro | old | N | MOSTLY_TRUE | See §4 |

## 14. Cloud / services (marked as anti-features for GlyphPDF)

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Document Cloud storage | 100GB, device sync, cloud file links | All | DC 2015 | Y | TRUE | Anti-feature |
| Share for review / comments sync | Cloud-hosted shared reviews | Paid | DC 2015 | Y | TRUE | Anti-feature |
| Acrobat Sign integration | See §8 | Sign | 2015+ | Y | TRUE | Anti-feature (offline-first conflict) |
| AI Assistant | Chat with doc, summarize, cross-doc queries, suggested questions; add-on subscription | Add-on (~$4.99/mo individual, UNVERIFIED price) | 2024 | Y | PARTLY_TRUE | Existence and 2024 GA well-corroborated; exact current pricing unverified. GlyphPDF's local Ollama/LLM provider is the offline counter-move |
| Web/online tools | Convert/OCR/compress in browser, free-with-limits | Free/All | 2020s | Y | TRUE | Anti-feature |
| Acrobat Studio | AI-workspace successor branding (PDF Spaces etc.) | Subscription | 2025 | Y | UNVERIFIABLE | Seen in secondary results only; details not verified from primary sources |
| Integrations | Teams/SharePoint/Google Drive/Box/Dropbox hooks | Paid | DC | Y | TRUE | Anti-feature |

## 15. Deployment / licensing

| Feature | Sub-capabilities | Edition | Since ver. | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|---|
| Individual subscriptions | Standard ~$12.99–14.99/mo; Pro ~$19.99–23.99/mo (annual-paid-monthly vs monthly billing swings price); annual-prepaid discounts | — | — | Sign-up cloud | MOSTLY_TRUE | Figures from PCWorld/comparison pages + Adobe snippets; exact current numbers fluctuate — verify at purchase time before quoting publicly |
| Acrobat 2024 (Classic) term license | Pro/Standard desktop, 3-year term, paid upfront, runs offline for the term, no cloud/AI/mobile perks | One-time purchase | 2024 | N (activation may need one online pass) | TRUE | helpx FAQ "faq-acrobat-pro-2024": "3-year term license, non-renewing… use it offline for the entire term". Successor to Acrobat 2020 perpetual (discontinued 2024; support ended 2025). Marketing answer to subscription anger — NOTE: "Acrobat Pro 2024" = separate Classic track, no updates after term |
| Feature-restricted licenses | Offline-validated licenses for secure/air-gapped enterprises | Enterprise | old | N | MOSTLY_TRUE | Sold in multi-year terms via resellers; relevant precedent for GlyphPDF's zero-cloud stance |
| Admin/deployment | GPO/registry preference templates, SCCM/Intune packages, UI customization wizard | Enterprise | old | N | MOSTLY_TRUE | Customization Wizard is a long-standing IT tool |
| Subscription anger context | ETF = 50% of remaining monthly payments on annual-paid-monthly; hidden-fee + hard-cancel FTC/DOJ complaint | — | FTC case June 2024 | — | TRUE | FTC press release verified (quotes captured): "Adobe trapped customers into year-long subscriptions through hidden early termination fees and numerous cancellation hurdles"; DOJ filed; ROSCA violations alleged; executives named |
| FTC outcome | Settlement with $1M civil penalty, ETF disclosure/cancel-flow remedies | — | June 2025 | — | PARTLY_TRUE | Settlement figure widely reported; primary settlement press release not retrieved (rate limits) — verify before publishing |

---

## Failure-mode findings (with frequency signals)

| # | Failure mode | Evidence (grade) | Signal strength |
|---|---|---|---|
| F1 | Subscription/ETF anger as product identity | FTC complaint Jun 2024 (TRUE, primary quote), $1M settlement Jun 2025 (PARTLY_TRUE); r/sysadmin "is almost every company paying for Adobe Acrobat" | High — regulatory + persistent community threads |
| F2 | Performance bloat / RAM | r/Adobe "Genuinely impressed by how bloated Adobe Acrobat feels in 2026", "Acrobat is Painfully Slow"; community "90% of 32GB RAM managing pages", slow on i7/16GB SSD (MOSTLY_TRUE, multiple independent threads) | High |
| F3 | Forced "New Acrobat" UI churn breaking workflows | r/sysadmin outages thread (toolbar disappears; fix = View > Disable New Acrobat); r/Adobe "Where are all my tools?", page numbers gone; update "broke the ability to digitally sign documents" (MOSTLY_TRUE, thread titles corroborated) | High — enterprise-visible |
| F4 | Update-triggered regressions in core features | Community threads: "Compare Files feature not working" (~13K views), "suddenly worse at combining files" (PARTLY_TRUE, anecdotal but multiple) | Medium |
| F5 | Resentment at paywalling basics (edit/organize/merge behind subscription) | r/Adobe "everyone uses a pdf editor for [X] is a 'premium' paid feature"; FTC-adjacent pricing resentment (MOSTLY_TRUE) | High — the one-price contrast sells itself |
| F6 | OCR weakness on mixed/multi-language docs | r/pdf reports (LOW-tier corroboration) | Low-Medium |

## Loved-workflow findings (parity-table ammo / UX patterns)

| # | Workflow | Evidence (grade) | Build implication |
|---|---|---|---|
| L1 | Combine Files with thumbnail preview + rearrange before merge | helpx official; regression thread proves reliance (TRUE) | Keep merge UI visual, preview-first, drag-order — parity exists; ensure preview-thumbnail speed beats Acrobat's regression |
| L2 | Organize Pages drag/reorder/extract (incl. discontinuous extract, 2024) | helpx + release notes (TRUE) | GlyphPDF U06 parity + keyboard moves already lands here |
| L3 | Fill & Sign daily driver | helpx; most-consumed feature class (MOSTLY_TRUE) | Session signature cache + initials (§9.7-a/b) already matches the pattern |
| L4 | Comment review + summary report | helpx (TRUE) | U07 comments table/CSV is close; add printable summary document |
| L5 | Action Wizard + Preflight as institutional rails (gov DOT scanning manuals; print pros) | WV DOH manual, helpx (TRUE) | Named multi-step batch presets (v1.5) is the entry; preflight-lite extends it |
| L6 | "Industry standard" fidelity trust | r/NoStupidQuestions, r/Adobe threads (MOSTLY_TRUE — sentiment) | Marketing moat, not a feature; counter with compatibility-test transparency |

---

## GlyphPDF delta (per domain — gaps vs offline-first workstation)

Baseline = PRD §27 + ledger 2026-09-05 (viewer, inline text/image edit, dual-engine OCR, AcroForm w/ calculated fields + auto-detect, annotations, redaction excision + patterns + batch, AES-256 + watermarks + sanitize, PAdES B-LT/B-LTA local signing + validation, compare w/ fingerprints + reports, batch + hot-folder, PDF/A-1B/2B/2U/3B/3U export, reading-order check, capability registry).

- **Viewing/nav:** GAP — advanced multi-folder search with saved criteria; OCG layers panel (show/hide/reorder/properties); geospatial measure view. Have parity elsewhere; dark mode/modes/tabs done.
- **Editing:** GAP — object hand-off to external image editor; page-box (trim/bleed) editing UI; full flatten command (Acrobat lacks it too — opportunity); page labels UI (groundwork landed §9.9-b).
- **OCR:** NO GAP — parity on outputs/preprocessing; language coverage breadth is our lever (Acrobat gates languages by edition; we bundle open models).
- **Forms:** GAP — barcode (PDF417) fields; JS event model (Calculate/Validate/Format/Keystroke); calculation-order UI; one-click form flatten (opportunity); XML/FDF UI exposure. Auto-detect + calculated fields + undo already landed.
- **Comments:** SMALL GAPS — dynamic stamps (auto date/user), printable comment-summary document, annot statuses UI (roadmap v1.4).
- **Redaction:** NO GAP — mark/search-pattern/apply/sanitize/batch all landed; keep excision-corruption tests (E-1) as the trust story.
- **Security:** GAP — certificate (public-key recipient) encryption; FIPS-mode packaging (niche gov).
- **Signatures:** GAP — certify-vs-approve (DocMDP) distinction in local signing; LTV embedding exists (B-LT/A). E-sign workflow (send/track) intentionally out of offline scope.
- **Compare:** NO GAP — parity + our tested alignment beats Acrobat's reliability reputation.
- **Batch/actions:** GAP — named multi-step Action-Wizard-style presets (roadmap v1.5) and saved drag-drop style batch. WE BEAT — native hot-folder + CLI (Acrobat has neither natively).
- **Print production:** GAP (largest single domain) — preflight-style validation profiles + fixups, output preview (separations/overprint), ink/box handling, printer marks. Pure-engine, offline-feasible.
- **Accessibility:** GAP — full checker rule tree + visual reading-order editor + tags-panel repair + one-click auto-tag (local model feasible via layout detector already in OCR stack).
- **Import/export:** NO MAJOR GAP — Office/image/scanner both ways; PPT gated claim unverified; we add Markdown/EPUB (Acrobat lacks).
- **Cloud/services:** ALL ANTI — replicate none; local AI assistant via existing Ollama provider is the counter to AI Assistant.
- **Deployment/licensing:** CONTRAST — one-price perpetual vs subscription + ETF/FTC baggage; Acrobat 2024 term license validates demand for a no-cloud SKU but is time-limited — our perpetual is strictly stronger positioning. Consider MSI/GPO-registry preference templates for IT parity.

## Top 10 build recommendations (demand x offline fit)

1. **Preflight-lite validator + fixup presets** (PDF/A & structural validation, common fixups: embed fonts, flatten transparency, set boxes) — Pro-gated in Acrobat, deep pro demand, pure C++/Qt engine work on existing PoDoFo/PDFium stack. (MOSTLY_TRUE demand)
2. **Accessibility remediation suite** (full checker rules, visual reading-order zone editor, tag-tree panel edit, local auto-tag using PP-DocLayout) — 508/EAA compliance demand, builds on landed reading-order check + OCR layout model. (TRUE feature set, TRUE feasibility)
3. **Named multi-step batch presets (Action Wizard analog) over hot-folder/CLI** — institutional demand proven; we add native CLI/headless Acrobat never had. (TRUE)
4. **Measurement toolset** (distance/perimeter/area annots, scale calibration, unit snap) — AEC/construction daily need; Qt overlay + CALS-scale storage; offline trivial. (TRUE)
5. **OCG layers panel** (toggle/reorder/properties, create-layer-from-selection via OCG/OCMD writes) — map/CAD-adjacent demand; engine work is moderate. (MOSTLY_TRUE)
6. **Certificate encryption** (encrypt-to-recipient-list with per-recipient permissions) — legal/privacy demand; OpenSSL already in stack. (MOSTLY_TRUE)
7. **Form JS event model + calculation-order UI** (Calculate/Validate/Format/Keystroke; sandboxed JS interpreter for existing AcroForms) — needed to run the enormous installed base of JS forms; highest-effort item here, scope to execution-first (run) before authoring. (TRUE demand, scope risk flagged)
8. **Dynamic stamps + comment summary document** — cheap, loved, closes comments domain. (TRUE)
9. **Advanced search across folders + saved searches (.pdx-index aware)** — parity with a decades-old Acrobat capability we lack; local SQLite index is a natural fit. (TRUE)
10. **Object touch-up hand-off** (edit image in external editor, round-trip in place) — pro edit demand; simple process plumbing on existing image-edit path. (PARTLY_TRUE gating, TRUE feasibility)

## Anti-recommendations (not worth building)

- **PDF Portfolios** — legacy container format, declining relevance, Acrobat itself de-emphasized it in the new UI; low demand signals. (UNVERIFIABLE on current Acrobat UI status; anti on demand)
- **3D PDF authoring (U3D/PRC)** — niche CAD workflow, enormous format/viewer cost. (TRUE cost)
- **Geospatial projection math** — niche; view/measure-on-georeferenced files only if a customer appears. (MOSTLY_TRUE)
- **Acrobat-Sign-style cloud e-sign workflow** — contradicts offline-first; local PAdES + audit trail is the differentiator, not the clone. (TRUE)
- **Cloud AI assistant** — anti; local LLM provider already the answer. (TRUE)
- **PostScript/Distiller ingestion** — dead workflow for the target user; skip. (MOSTLY_TRUE)
- **Liquid-Mode-style reflow** — cloud AI in Acrobat; low desktop value. (TRUE)
- **Droplet .exe artifacts** — supersede with CLI + presets (simpler, scriptable). (Design call)

## Confidence Level

Medium-High on feature inventory and gating (Adobe helpx page titles + official comparison-page snippets + Adobe Sign helpx corroborate most rows; several rows graded MOSTLY_TRUE/PARTLY_TRUE because Adobe's live comparison matrix and some helpx pages would not load under tool rate limits). High on failure modes (FTC primary source). Gaps flagged inline; rows UNVERIFIABLE must not be quoted externally without primary-source confirmation: Acrobat Studio details, AI Assistant pricing, FTC settlement terms, PPT-export gating, PDF/A creation gating in Standard, Reader-enabling rights gating, portfolios-in-new-UI status.

## Sources (spec-level)

- Adobe Help Center (helpx.adobe.com): Analyzing documents with the Preflight tool (Acrobat Pro); Previewing output (Acrobat Pro); Automating document analysis with droplets or preflight actions; Action Wizard (acrobat-pro slug); Create and verify PDF accessibility (Acrobat Pro); Reading Order tool (Acrobat Pro); Edit document structure with the Content and Tags panels; Grids, guides, and measurements in PDFs; Geospatial PDFs (overview, calculate distance/area, customize measurements); PDF layers overview; Creating PDF indexes (Catalog); Configure form field calculations; Add and test barcode fields; Field properties/formatting; Rich Media (audio/video/3D); Compare two versions of a PDF file (Acrobat Pro); Modify images in PDFs; Combine files; OCR supported languages; Acrobat Pro 2024 FAQ; New features summary — Acrobat Pro 2024; Acrobat Sign transaction limits.
- Adobe.com: Acrobat pricing / compare-versions pages (Standard vs Pro matrix; Acrobat Pro 2024 desktop-only term-license copy).
- Adobe Open Source SDK docs: Acrobat JavaScript API reference; JavaScript contexts; Interapplication Communication (IAC/OLE).
- Federal Trade Commission press release, June 2024: "FTC Takes Action Against Adobe and Executives for Hiding Fees, Preventing Consumers from Easily Cancelling Software Subscriptions" (primary; direct quotes retrieved).
- User-evidence (secondary corroboration): r/sysadmin (Adobe outages / New Acrobat toolbar; digital-signing breakage; Acrobat license cost threads), r/Adobe (bloat 2026, painfully slow, missing tools, page numbers), r/Acrobat (combine-files regression), r/pdf, r/graphic_design ("Acrobat Pro such utter garbage"), Adobe Community forums (RAM 90%, compare-files not working, OCG reorder, geospatial auto-switch, redaction-in-Standard), HHS Section 508 tagging guide, WebAIM PDF accessibility review.
