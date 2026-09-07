# Research Report: Foxit PDF Editor (Pro/Standard, current releases) — spec sheet vs GlyphPDF

**Date:** 2026-09-07
**Requested by:** GlyphPDF competitive-parity program
**Research question:** Feature-by-feature specification of Foxit PDF Editor's desktop capabilities, edition gating (Standard vs Pro vs Perpetual), cloud/subscription dependencies, version drift 2022–2026 (especially the perpetual→subscription shift), recurring failure modes, and loved workflows — with a per-domain GlyphPDF delta for mechanical diffing against the other competitor spec sheets.
**Method:** 15+ distinct web queries (official KB/version history/press, reddit r/pdf·r/sysadmin·r/software, G2/Capterra/Trustpilot/BBB patterns, PDF Association member news) + direct extraction of Foxit's official *PDF Products Comparison* datasheet PDF (v2026.2/v14, 72-page master) and the official version-history page. Claims graded: TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE.

**Edition legend (current lineup, per official datasheet):**
- **R** = Foxit PDF Reader (free) — shown only where it bounds a feature floor
- **Std** = Foxit PDF Editor (subscription, ~$129.99/yr) — the "Standard" tier
- **Pro** = Foxit PDF Editor+ (subscription, ~$159.99/yr) — the "Pro" tier
- **Perp** = Foxit PDF Editor Perpetual License (v14.0, $209.99 one-time) — feature-rich but feature-frozen under the Aug-2025 N-1 policy (security fixes only; new features subscription-only)

Datasheet caveat: the official comparison PDF's check glyphs did not survive text extraction; where edition attribution rests on qualifier text ("Windows only", "Pro only" release-note tags, legacy Standard/Business KB) it is graded; where it rests on column-position inference alone it is marked "(inferred)". Release notes with explicit "(Pro only)" tags are the strongest gating evidence.

---

## 1. Spec tables by domain

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Multi-tab viewing | Tab groups (horizontal/vertical), drag tab out to new window | All desktop incl. R | N | Tab-out behavior in v9.x release notes | TRUE |
| Thumbnails & bookmarks | View, navigate, manage | All incl. R | N | | TRUE |
| Text Viewer / Magnifier / Loupe | Pan & zoom inspector | Editor tiers (magnifier in R on macOS per qualifier) | N | GlyphPDF has magnifier only inside OCR Verify | TRUE |
| Reflow view + Reflow editing | Text reflow to window | Std/Pro/Perp, Windows only | N | Flagship "paragraph editing with automatic reflow and reformat" row; signature Foxit edit capability | TRUE |
| Reverse view | Right-to-left document reading | All incl. R | N | | TRUE |
| Night mode / Full screen / AutoScroll | Reading modes | All incl. R | N | | TRUE |
| Ruler, guides, grids; line-weights toggle | Object positioning aids | Editor tiers (line weights also in R) | N | | TRUE |
| Read aloud / Smart Podcast | TTS; 2025.1 wider voices; 2026.2 split into Read Aloud (selection+highlight) and podcast-style generation | All incl. R (podcast = AI tiers) | Y (AI assistant tier) | Read Aloud offline-capable local TTS not disclosed | TRUE |
| Word count | Document statistics | All desktop | N | | TRUE |
| PDF portfolios | Create (blank/webpage/scanner/folders/clipboard), view, edit; component rename/description; OCR a portfolio; sort; portfolio→PDF export; password/cert protection | Editor tiers; portfolio creation from all PC sources = Std/Pro/Perp Windows | N | v12.0 enhancements verified; legacy Pro tag on multimedia→PDF conversion | TRUE |
| 3D content | Display PRC/U3D; add U3D/PRC object; glTF view/navigate/measure/comment (v14/2023.3); 3D canvas ops; 3D measurement incl. radius + snap | Pro-tier for authoring/measure; display broader | N | Heavy print/CAD audience feature | TRUE |
| Advanced search | Search bookmarks+comments+all opened docs; pattern (regex) search; proximity search; stemming; diacritic/Asian-width ignores; search cache | Editor tiers (R: search only; index search in editors) | N | Proximity/stemming from PhantomPDF 10 release notes; pattern search row in datasheet | TRUE |
| Full-text index (catalog) | Create embedded index per PDF; catalog index + scheduled XML updates via Task Scheduler | Editor tiers; R search-only | N | v9.6/v10 release notes | TRUE |
| Custom page labels | From bookmarks or selected page regions; per-range labels (bidding workflows) | Editor tiers | N | 2026.2 release notes; relevant to GlyphPDF §9.9-b groundwork | TRUE |
| Named positions | Saved navigation anchors | Editor tiers | N | Datasheet row | TRUE |
| Overprint preview | Correct print-production rendering | Std/Pro/Perp Windows | N | Added 2026.1 | TRUE |

### 1.2 Editing (text / object / page)

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Paragraph editing w/ auto-reflow | Add/delete text, font/alignment/bullets/numbering, move/resize paragraphs, defined text-block width w/ reflow (v12.1) | Editor tiers | N | Foxit's flagship differentiator vs GlyphPDF inline text edit | TRUE |
| Object editing | Move/resize/rotate/flip/cut-copy-paste/delete images & graphics; edit image in external app (e.g. Paint); export pages as one long image | Editor tiers | N | | TRUE |
| Advanced object editing | Shading objects, clipping effects, text→path conversion | Pro/Perp (inferred) | N | Datasheet "Advanced editing" row | MOSTLY_TRUE |
| Hyperlinks & bookmarks | Add/edit/delete/manage; custom actions (open file, sound, web); cross-reference links; recognize/remove web links; export/import bookmarks | Editor tiers | N | | TRUE |
| Auto-create bookmarks from text | By text style or search string, preset properties | Pro only | N | Explicit "(Pro only)" in v13 release notes | TRUE |
| Headers/footers, backgrounds, watermarks | Single + batch across files; tiling watermarks; dynamic username watermark | Editor tiers (batch add across files = Std/Pro/Perp) | N | Batch row verified PhantomPDF 10; tiling/username v12.0 | TRUE |
| Bates numbering | Standard Bates; Bates from DMS files (Business/Pro, v10) | Editor tiers | N | | TRUE |
| Page management | Insert, delete, extract, reverse, rearrange, move, swap, **interleave merge** (odd/even scans, v12), duplicate, replace, split, rotate, resize, crop; split one page into multiple (v13); rearrange by bookmark order; insert pages into other files | Editor tiers | N | Full list from datasheet row + release notes | TRUE |
| Web/HTML into PDF | Insert web pages into PDF (v12); HTML→PDF with Media Style + scaling (v9.7) | Editor tiers | N (network at capture time) | | TRUE |
| Articles | Navigational path across columns/pages | Editor tiers | N | | TRUE |
| Link & join text | Text block linking | Editor tiers | N | Datasheet row | TRUE |
| Search & replace | Text replacement with prev/next shortcuts, highlight color (v12.1) | Editor tiers | N | GlyphPDF gap (PRD §9.15) | TRUE |
| Spell check | Whole-document typo correction | Editor tiers | N | | TRUE |
| Layers | Import as layer, merge/flatten, layer properties, delete/reorder, keep layers when combining | Editor tiers | N | | TRUE |
| Calculator + tape | Accounting calculator annotating PDFs; tape editor recalculation (v13) | Pro only | N | Explicit "(Pro only)" tags v12/v13 | TRUE |
| Tickmarks | Symbol/number/letter audit stamps | Pro only | N | Explicit "(Pro only)" 2024.2/v14 | TRUE |
| XMP metadata | Save/import document metadata as XMP | Editor tiers Windows | N | v14/2024.4 | TRUE |

### 1.3 OCR

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Scanned→searchable/editable | Whole-document OCR, searchable + editable output | Editor tiers (Std included in current lineup; historically Pro/Business) | N | Legacy Standard/Business KB + v13 notes tag only partial OCR as Pro; current-Std inclusion = (inferred) | MOSTLY_TRUE |
| Partial-page OCR | Recognize a selected region instead of whole page/document | Pro only | N | Explicit "(Pro only)" v13 notes | TRUE |
| OCR suspects workflow | "Find All Suspect" panel; batch mark Not-Text / edit results | Editor tiers (Perp qualifier: "Quick recognition only") | N | PhantomPDF 10; Perp limitation from datasheet qualifier | TRUE |
| OCR of portfolios | Batch OCR files inside a portfolio | Editor tiers | N | PhantomPDF 10 | TRUE |
| Engine/accuracy updates | 2025.2/v14: digits, checkboxes, handwriting, signatures; upgraded engine security (14.0.7); Vietnamese/Indonesian (2024.3); HKSCS | Editor tiers | N | | TRUE |
| MRC/high compression | High compression on scanner creation + optimization | Editor tiers | N | Datasheet row; complements GlyphPDF MRC lane | TRUE |

### 1.4 Forms (creation, filling, calculation, JavaScript)

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Form field recognition + designer | Auto-detect + assistant; fields: text, button, dropdown, checkbox, radio, **barcode, date, image**, digital signature | Editor tiers | N | Barcode/image/date = legacy Pro-era additions (PhantomPDF 10) | TRUE |
| Draw/edit form controls | Full designer incl. barcode/date/image | Editor tiers | N | | TRUE |
| Fill & save forms | Incl. **XFA forms**; flat-form fill (Fill & Sign); intelligent flat-form area detection; auto-completion | All incl. R | N | XFA fill incl. Reader; **edit static XFA** = Editor tiers | TRUE |
| Edit static XFA | Convert/edit static XFA forms | Editor tiers | N | | TRUE |
| Form data I/O | Import/export; reset; **combine form data (PDF/PPDF/FDF/XFDF/XML) → one CSV** | All desktop | N | | TRUE |
| JavaScript | Form/document JS execution; external JS editor (legacy) | Editor tiers | N | Also the attack surface behind Foxit's Safe Reading Mode | TRUE |
| Page templates | Reusable page templates | Editor tiers | N | | TRUE |
| Rich text in fields | Bold/italic/superscript while filling (v12.1) | Editor tiers | N | | TRUE |

### 1.5 Comments & markup

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Full markup set | Notes, text boxes, callouts, highlight/underline/strikeout/squiggly, typewriter, stamps, lines/rects/pencil/oval/polygon/cloud/arrow/polyline | All incl. R | N | | TRUE |
| Comment management | Filter/sort by author, status, color; reply/edit from panel; statuses on others' annotations (shared review); font override for accessibility | Editor tiers (status-change in shared review) | N | v12/v12.1/PhantomPDF 10 | TRUE |
| Comment I/O + summary | Import/export FDF/XFDF; selected-comment export; **CSV summary with custom columns, types, page ranges, sorting (2025.3)**; highlighted text → TXT; tracker details → PDF (shared review) | All desktop (tracker export in review contexts) | N | GlyphPDF U07 CSV export = 2025.3 parity | TRUE |
| Stamp system | Library management; custom stamp from page; Favorite Toolbox create/import/export (2025.3/2026.2); **sequential numbering stamps** (2025.3); enterprise shared stamp sets via drive + GPO (2026.2) | Editor tiers (Toolbox Windows) | N (GPO-shared stamps offline) | Sequential stamps target healthcare/legal/versioning | TRUE |
| Tickmarks | Audit tickmark stamps incl. symbols/numbers/letters | Pro only | N | Explicit "(Pro only)" | TRUE |
| Area highlight | Highlight a region | All desktop | N | | TRUE |
| File attachment comment | Attach file to comment | All incl. R | N | | TRUE |
| Mini toolbar / styles / shortcut keys | Comment styling UX (v12.1) | Editor tiers | N | | TRUE |
| Dictionary lookup | Right-click definition via Dictionary.com | Editor tiers | Y (web service) | | TRUE |
| Evernote integration | Send PDF to OneNote/Evernote | Editor tiers Windows | Y (external service) | Datasheet row | TRUE |

### 1.6 Redaction

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Mark + apply redaction | Permanently remove content (true excision) | **All products incl. free Reader and Cloud** | N | Datasheet row spans all columns | TRUE |
| Search-and-redact profiles | Saved text-string redaction profiles; pattern sets (Canada patterns added v10); mark partial words after search | Editor tiers (profiles legacy Business-only; partial-word Windows) | N | | MOSTLY_TRUE |
| Foxit Smart Redact | AI redaction incl. non-static patterns (person names, org names, roles) | **Pro (Editor+) only** | Y (AI service) | Was a separate-purchase plug-in at v12.1; folded into Editor+ | TRUE |
| Whiteout | One-click erase-to-white | Editor tiers | N | Datasheet row | TRUE |
| Sanitize / remove hidden info | Metadata, comments, hidden data from previous saves, hidden layers, overlapping objects; optional auto-sanitize on close/email (2025.3) | Editor tiers | N | | TRUE |
| RMS redaction from Office | Create RMS-redaction-protected PDFs from Word/Excel/PPT | Pro/Perp Windows | Y (RMS infra) | | TRUE |

### 1.7 Security & encryption

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Password + certificate encryption | Permissions controls; security policies management | Editor tiers | N | | TRUE |
| Modern crypto standards | AES-GCM encrypt/decrypt per ISO/TS 32003; ISO/TS 32001-2 hash algorithms | Editor tiers (v14/2025.2) | N | Relevant to GlyphPDF OpenSSL lane | TRUE |
| AD RMS | Decryption; encryption single + batch; dynamic security watermarks; usage logging; PDF 2.0-compliant RMS files | Editor tiers (batch encryption Pro) | Y (AD RMS infra) | | TRUE |
| Microsoft Purview / MIP | Read/apply sensitivity labels, enforce encryption, UDP labels, default/mandatory labeling policies, GCC High (2026.2) | Editor tiers | Y (MIP infra) | Also v14 AIP default/mandatory labeling | TRUE |
| Double Key Encryption (DKE) | Two-key encryption w/ Azure-held key | Editor tiers (v14/2025.2) | Y (Azure) | | TRUE |
| Safe Reading Mode | Block unauthorized JS/actions/data transmissions | All incl. R | N | ZDI advisories (v8.x) motivated this | TRUE |
| Action Inspector | Analyzes JS actions on open, alerts on risk | Editor tiers | N | 2026.1 — a local, offline-compatible feature | TRUE |
| WIP support | Windows Information Protection | Editor tiers Windows | N | | TRUE |

### 1.8 Signatures (digital certs + e-sign)

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Digital signatures | Add/edit/delete/manage; custom logo/appearance; batch place on multiple files; certify + permitted actions (DocMDP) | Editor tiers (batch sign legacy Pro) | N | | TRUE |
| PAdES + LTV | PAdES-compliant signing, Long Term Validation | Editor tiers | N | v9.6 release notes | TRUE |
| Trusted timestamps | TSA timestamps on signatures/documents | Editor tiers | Y (TSA network) | | TRUE |
| EUTL verification | EU Trusted List validation incl. qualified (legal) info display | Editor tiers | Y (TSL refresh) | | TRUE |
| Modern signature crypto | ISO/TS 32001/32002 hash algorithms; CRT import; embedded-timestamp recognition | Editor tiers (v14/2026.2) | N | | TRUE |
| Trust sources | Windows Certificate Store root trust option | Editor tiers | N | v9.2 | TRUE |
| Ink signatures | Handwritten/image; style customization; multi-page same-position placement; USB writing pads | All incl. R | N | | TRUE |
| Fill & Sign | Flat (non-interactive) form fill + quick sign; symbol color customization (v13) | All incl. R | N | | TRUE |
| Foxit eSign service | Send/initiate/track e-sign workflows; bulk sending to thousands (eSign Pro/Enterprise); online forms from templates; branding | Subscription tiers; language-gated | **Y — cloud service** | Anti-feature for GlyphPDF | TRUE |
| DocuSign integration | Send/sign/save via DocuSign | Editor tiers Windows | Y (cloud) | | TRUE |
| Cloud signatures on mobile | Cross-device signing | v14/2025.2+ | Y | | TRUE |

### 1.9 Compare

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Side-by-side viewing | Two documents side by side | All desktop | N | Datasheet row | TRUE |
| PDF comparison (content diff) | Text + visual compare; **cross-page text comparison** for accuracy + improved results display (2026.2) | Editor tiers (historically Business/Pro; current gating inferred) | N | Existence TRUE via 2026.2 release notes; edition gating MOSTLY_TRUE | MOSTLY_TRUE |

### 1.10 Batch / actions (custom action wizard)

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Action Wizard | Create named multi-step actions; run on files/folders; custom commands built from existing commands (v13); includes Adobe-default-equivalent actions: Save As, remove hidden info, view logs, batch conversion (PhantomPDF 10); make-accessible action (autotag, doc properties, language, PDF/UA check+fix) | **Pro only** | N | Explicit "(Pro only)" tags v13; PhantomPDF 10 scope list. Foxit's GlyphPDF-relevant killer feature | TRUE |
| Batch print | Right-click batch print; print current page of each open PDF (v12.1) | Editor tiers | N | | TRUE |
| Batch watermark/background/header-footer | Apply across multiple files at once (v10) | Editor tiers | N | | TRUE |
| Batch page-size modification | Modify page size across documents (v14/2024.3) | Editor tiers | N | | TRUE |
| Batch digital signing | Place signature on multiple files | Editor tiers (legacy Pro) | N | v9.6 | TRUE |
| Batch RMS encryption | Encrypt in batch | Pro | Y (RMS) | | TRUE |
| Batch ECM convert/OCR | Convert files from iManage etc. in batch; Bates from DMS | Pro (legacy Business) | Y (ECM) | | TRUE |
| Scheduled indexing | XML batch file + Task Scheduler for catalog index updates | Editor tiers | N | PhantomPDF 10 | TRUE |

### 1.11 Print production

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Foxit PDF Printer (virtual printer) | Print-to-PDF from any Windows app | Installed with editors | N | Verified in release notes (v8.x "PhantomPDF Printer") | TRUE |
| Print dialogs | Print w/ comments; print pages from selected bookmarks; print forms; print multiple open PDFs | All desktop | N | Datasheet rows | TRUE |
| Booklet / N-up printing | Booklet imposition, multiple pages per sheet, duplex subsets | Editor tiers | N | General knowledge only — not verified in fetched sources this session | UNVERIFIABLE |
| Preflight | Validate + fix PDF/A, PDF/E, PDF/X, PDF/UA, PDF/VT; detailed reports; Standards panel; **grayscale conversion** (2026.1) | Editor tiers (grayscale = Pro Windows per qualifiers) | N | v9.7 scope list | TRUE |
| PDF/A creation | ISO PDF/A with font embedding; PDF/A/E/X conversion | Editor tiers | N | Datasheet rows | TRUE |
| USPTO-ready PDFs | Preset creation defaults (page size, fonts) | Editor tiers | N | v10 release notes | TRUE |
| High-compression scan creation | MRC-style compression on create | Editor tiers | N | Datasheet row | TRUE |
| Export-format lockdown | GPO/Customization Wizard hide/show Save As export formats | Editor tiers (admin-configured) | N | 2026.1 | TRUE |

### 1.12 Accessibility / tagging

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Accessibility full check | Checker + report | Editor tiers | N | Datasheet row | TRUE |
| Fix failed checks | Remediate check failures | Editor tiers | N | | TRUE |
| Touch up reading order | Add/edit tags, reading-order editing | Editor tiers | N | GlyphPDF has check-only | TRUE |
| Autotag | Auto-tag document + tag report | Editor tiers | N | | TRUE |
| Make-accessible Action Wizard action | Batch accessibility (properties, autotag, language, PDF/UA check+fix) | Pro only | N | v13 release notes | TRUE |
| MathML formula tagging | AI conversion of formulas to MathML into tags; also LaTeX export | Editor tiers | Y (AI add-on) | 2026.2 | TRUE |
| Comment font override | Accessibility font/size for all comments | Editor tiers | N | PhantomPDF 10 | TRUE |

### 1.13 Import / export formats

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| Office one-step creation | Word/Excel/PPT/Outlook add-ins; tagged PDF from Office; Project/Visio (Pro, Windows) | Editor tiers | N | | TRUE |
| Browser capture | One-click PDF from IE/Edge/Chrome/Firefox (extension); HTML media-style + scaling options | Editor tiers Windows | N | | TRUE |
| DWG→3D PDF | CAD conversion | Pro Windows | N | Datasheet row | TRUE |
| PDF→Office/images/etc. | Word/Excel/PPT, XPS, HTML (single/multi-file by headings/bookmarks), images (incl. one long image), RTF/TXT, accessible text, XML 1.0; selected-area export; spreadsheet numeric/multi-worksheet options | Editor tiers (XPS not in Cloud; per-OS qualifiers) | N | | TRUE |
| Google Workspace round-trip | Save PDF as gDocs/gSlides/gSheets | Editor tiers | Y (Google) | Datasheet row | TRUE |
| Scan ingestion | Multi-file scanning, scan presets, append to existing PDF | Editor tiers Windows | N | | TRUE |
| Markdown / EPUB targets | — | **Not offered** | — | Capterra con: "lacks ePub conversion"; no Markdown target anywhere | TRUE (as a gap) |

### 1.14 ConnectedPDF / collaboration / deployment — ANTI-FEATURES for GlyphPDF (cloud/connect surface)

| Feature | Sub-capabilities | Edition | Cloud/subscription? | Notes | Verdict |
|---|---|---|---|---|---|
| connectedPDF | Document identity via cloud service; track who opened/when/where; version registration; one-click DRM protection; owner messages; Connected Review | **DISCONTINUED — service no longer active** in current Editor | Y (was) | Official KB: "ConnectedPDF was discontinued, the service is no longer active"; GPO key remains as legacy. Market validation that cloud-tethered PDF identity failed | TRUE |
| Shared Review | Review via **email, network folder, or SharePoint workspace**; Tracker for review status; publish/sync comments; Reader participation; tracker→PDF export | Editor tiers (Std/Pro/Perp) | N-partial (network-folder mode is LAN/offline-compatible) | The one collaboration feature that can work air-gapped | TRUE |
| Real-time collaboration | Multi-user comments on shared files, review management | Subscription tiers | Y | 2023.3 | TRUE |
| Cloud Documents → Foxit DMS | Unlimited storage, sharing, RBAC, version history, tracking | Subscription tiers | Y | 2026.1.1 | TRUE |
| ECM integrations | SharePoint, iManage 9/10, ndOffice (NetDocuments), OpenText eDOCS DM, Epona DMSforLegal, Alfresco, ShareFile, Worldox, Enterprise Connect, Documentum, Egnyte + Google Drive/OneDrive/Box/Dropbox; open/check-out/edit/check-in; content-type metadata; save-as-version | Editor tiers (full list = Pro/Perp Windows; subsets elsewhere) | Y (DMS infra) | "OA plug-ins" per the brief = China-market OA integrations (泛微/致远-class); international equivalent is this ECM set | TRUE (ECM list) / PARTLY_TRUE (OA plug-ins = China edition) |
| Office/Teams/SharePoint/browser add-ins | Teams app, SharePoint add-in, Office 365 add-in, Outlook add-in, Chrome/Edge extensions, Asana/Box/Gmail cloud apps | Editor tiers (business-gated for Teams/SharePoint) | Y | Datasheet plugin page | TRUE |
| AI Assistant / Foxit Workspace / MCP | Chat w/ docs/images; agentic multi-step; multiple LLMs; **MCP connectors (Salesforce, Jira, ERP)** — "first MCP Host in PDF industry"; skills; generate artifacts; translation; image generation | Subscription tiers; AI add-on ~$49.99/yr (not available on perpetual) | **Y — cloud** (BYOM + SLMs = on-device option, 2026.2) | BYOM/SLM worth watching as the only offline-compatible piece | TRUE |
| Admin Console | Cloud or on-prem license mgmt; SSO/AD; user groups; license assign; audit logs | Std/Pro/Perp + cloud-hosted variants | Y (service) | Legacy separate-orderable service (v10) now datasheet-gated | TRUE |
| Foxit Update Server | Internal update distribution; approve/push/schedule; no external server | Editor tiers | Y (service; LAN-distributed) | | TRUE |
| GPO/ADMX + Customization Tool + SCUP + Intune | Lock settings, default viewer, support links, disable self-signed IDs, trusted-app list, IRM versions, domain login whitelist/blacklist (v14), shared template locations (v14), hide export formats (2026.1), no-admin upgrade (v14) | Editor tiers Windows | N | Fully offline enterprise controls — cheap to emulate | TRUE |

### 1.15 Version granularity: the 2022–2026 licensing shift (context for every row above)

| Date | Event | Verdict |
|---|---|---|
| Jun 2022 (v12.0) | Last era before subscription split; Pro-only tags appear (calculator, Smart Redact plug-in separate purchase) | TRUE |
| May 2023 (2023.1) | **Subscription suites introduced** (Individuals/Teams/Education bundling desktop+cloud+eSign); note: "perpetual packages will be delivered in future release schedule" | TRUE |
| Sep 2023 (v13.0 / 2023.2) | Same codebase split: perpetual v13 line vs subscription 2023.x line — two tracks begin | TRUE |
| Aug 2024 (2024.3) / Sep 2025 (v14.0) | Binder, tickmarks land on both tracks | TRUE |
| **Aug 5–13, 2025** | **Perpetual support policy effective: N-1 support; v11/v12 discontinued; perpetual versions get security fixes, no active feature development; version split hardened (perpetual = 13/14, subscription = 2025+); Foxit blog publicly denies "phasing out perpetual" while KB policy freezes feature work** | TRUE |
| Aug 2025 (v14/2025.2) | Native **64-bit** Windows installers (all prior releases 32-bit); MCP Host; DKE; ISO/TS 32001-2/32003 crypto | TRUE |
| Dec 2025 – Sep 2026 (2025.3–2026.2) | AI agent + Workspace, measurement expansion (volume/polylength/diameter/radius/angle), cross-page compare, Action Inspector, Foxit DMS, custom page labels, enterprise stamps | TRUE |

Pricing anchor (verdict TRUE, official pricing page): Editor $129.99/user/yr; Editor+ $159.99/user/yr; $10.99/mo; Perpetual Pro $209.99 one-time; AI add-on $49.99/yr (subscription only). Community pricing datapoint: "$129/yr vs Adobe $204" (r/sysadmin, MOSTLY_TRUE).

---

## 2. GlyphPDF delta (per domain, for mechanical diffing)

Legend: **GAP** = Foxit ships, GlyphPDF lacks; **ANTI** = Foxit cloud/connect feature GlyphPDF should NOT build; **PARITY** = covered by ledger/PRD.

- **Viewing & navigation** — GAP: portfolios; full-text index/catalog + scheduled indexing; regex/pattern + proximity + stemming search; search across open docs; custom page labels (§9.9-b groundwork → finish); named positions; ruler/guides; reverse view; word count; overprint preview. ANTI: cloud unified search.
- **Editing** — GAP: paragraph-reflow editing engine; auto-bookmarks from text styles/patterns; auto-bookmarks via AI (skip — AI); cross-reference links; articles; link-and-join text; search & replace; spell check; layer import/merge/flatten/properties; interleave (odd/even) merge; split-one-page-into-many; batch page-size; tickmarks; calculator tape; tiling + dynamic-username watermarks; XMP metadata save/import; external image-object edit; long-image export. PARITY: text edit, object ops, bookmarks/links, Bates, headers/footers/watermarks (basic), crop/resize/rotate/split/merge.
- **OCR** — GAP: partial-region OCR; OCR-suspects batch panel (GlyphPDF OCR Verify ≈ parity); portfolio OCR (n/a if no portfolios). PARITY+: ROVER dual-engine likely exceeds Foxit single-engine; MRC lane already exists. Foxit 2025.x handwriting/signature/checkbox recognition = benchmark to match with PP-OCRv5.
- **Forms** — GAP: XFA static fill/edit; barcode/image field types; page templates; JS execution (deliberate skip = security stance; but a JS *analysis* analog exists in §Security); form-data combine→CSV across many PDFs; rich-text field filling. PARITY: 10 field types incl. calculated, tab order, flatten, CSV/FDF, auto-detect + compound undo (V06).
- **Comments & markup** — GAP: sequential stamps; Favorite Toolbox import/export; area highlight; comment statuses (open/resolved/rejected — PRD v1.4 roadmap); XFDF import/export; comment font override; enterprise stamp sets via file share (offline-compatible — buildable). PARITY: U07 CSV export/filters/table, stamps, threads roadmap.
- **Redaction** — PARITY+ (excision + patterns + presets + sanitize + transaction). GAP: whiteout one-click; partial-word marking; AI entity redaction (could be done OFFLINE with local NER — the Smart Redact feature re-interpreted offline-first). ANTI: cloud Smart Redact.
- **Security & encryption** — GAP: AES-GCM (ISO/TS 32003); ISO/TS 32001-2 hashes; Action-Inspector-style JS risk scan; security-policy presets. ANTI: AD RMS/MIP/DKE/WIP (enterprise cloud identity). Note: MIP *label display* is the only piece with plausible offline value.
- **Signatures** — GAP: batch signing across files; certify/DocMDP permitted-actions (not in ledger); CRT import; DocuSign/eSign/cloud signing = ANTI. PARITY: PAdES B-LT/B-LTA, OCSP, appearance, validity badges, initials, session cache.
- **Compare** — PARITY (structural + fingerprints + Myers-LCS + reports). GAP: cross-page text comparison (Foxit 2026.2 benchmark), side-by-side linked scrolling in two-page mode (deferred U04 seam).
- **Batch/actions** — **GAP (biggest): Action-Wizard analog — named, saved, multi-step batch pipelines with composable commands; batch print; batch watermark across files; batch sign.** PARITY: hot-folder, batch convert/OCR/merge/redact/compress/watermark. PRD v1.5 "batch preset workflows" = exactly this — elevate priority.
- **Print production** — **GAP (whole domain thin): virtual PDF printer; booklet/N-up; print-with-comments/bookmark pages; Preflight-style validation+fixup beyond PDF/A; USPTO-style presets.** PARITY: PDF/A export all levels.
- **Accessibility** — GAP: full check+report, fixups, autotag, reading-order touch-up editing, tag preservation on export (PRD v1.4 roadmap), PDF/UA validation in batch. PARITY: reading-order check (§9.14).
- **Import/export** — GAP: Office COM add-ins (heavy), browser capture, DWG→3D, XPS/RTF/XML targets, selected-area export, scan presets. PARITY+: in-house OOXML (better than HTML mislabel); Foxit has NO Markdown/EPUB — GlyphPDF v1.5 roadmap differentiates here.
- **ConnectedPDF/collaboration/deployment** — ANTI: connectedPDF (dead), cloud DMS, real-time collab, eSign, MCP/AI, Admin Console, Update Server. BUILDABLE-OFFLINE: GPO/ADMX + MST-style installer customization; shared stamp/template locations; shared-review-on-network-folder (LAN, no cloud) + comment XFDF round-trip.

---

## 3. Failure modes (Foxit) — what GlyphPDF should not replicate

| # | Failure mode | Evidence | Verdict |
|---|---|---|---|
| 1 | **Perpetual→subscription squeeze**: Aug-2025 N-1 policy; v11/v12 killed; perpetual = security-fixes-only; v14 upgrade requires *new license purchase*; official blog denial contradicted by own KB | Foxit KB policy article; Foxit blog; KB upgrade article; r/sysadmin "Foxit is phasing out perpetual licenses"; Spiceworks; schneider.im analysis | TRUE |
| 2 | **Reader-era nag/bloat/telemetry**: default-app popups, updater nagging, bundled toolbar/add-on attempts, "slowly turning into Acrobat" — concentrated in free Reader; paid Editor materially less affected | r/geek, r/software, r/pdf default-app thread, BleepingComputer, SuperUser | TRUE (Reader) / MOSTLY_TRUE (Editor) |
| 3 | **Support/billing friction**: perpetual activation loss ("reactivate every few weeks"), 14-day refund window, BBB complaint about update prompts that force new-license purchase, subscription billing complaints (CASRAI), Trustpilot mixed support experiences | r/sysadmin, BBB, Trustpilot, CASRAI, official refund policy | MOSTLY_TRUE |
| 4 | **connectedPDF sunset**: marquee cloud feature discontinued, stranding workflows built on it | Foxit KB preferences article | TRUE |

## 4. Loved workflows (what users choose Foxit for)

| # | Loved for | Evidence | Verdict |
|---|---|---|---|
| 1 | **Speed/lightweight vs Adobe**: snappier scrolling, faster open, faster OCR and search, lower resource use | r/sysadmin PSA thread, r/software reader comparison, Capterra pros ("lightweight, fast to open… doesn't eat resources like Adobe") | TRUE |
| 2 | **Price/value**: ~$129.99/yr vs Adobe ~$204; $209.99 perpetual; 40–60% cheaper across reviews | Foxit pricing page; r/sysadmin pricing thread; CheckThat.ai aggregate | TRUE |
| 3 | **Enterprise deployability**: GPO templates, licensing tools, "better GPO support" — IT admins cite it as the Acrobat replacement that deploys cleanly | r/sysadmin "Foxit!" thread, datasheet deployment rows | TRUE |
| 4 | **Modern Office-like UI** (2023 skin/title-bar rework) | v13 release notes; G2 ease-of-use 500+ mentions; Capterra counterpoint "UI is clunky" | PARTLY_TRUE (mixed) |

## 5. Top 10 build recommendations for GlyphPDF (ranked)

1. **Named multi-step batch pipelines (Action-Wizard analog)** — pure-local; composable commands (convert/compress/redact/watermark/OCR/rename) saved as reusable named actions run on folders. Closes the single largest desktop gap; extends the existing hot-folder and PRD v1.5 "batch presets" item.
2. **Print production pack** — virtual PDF printer + print dialog with N-up/booklet/duplex/print-with-comments/bookmark-range printing. Whole domain absent from ledger; high office demand (Foxit rows above).
3. **Search depth** — regex search, find-and-replace (already PRD §9.15 gap + v1.4 roadmap), proximity/stemming options, search across all open documents, embedded full-text index + scheduled re-index. All offline.
4. **Accessibility checker + autotag + fixups** — extend §9.14 reading-order check to full-check report, tag editing/touch-up, PDF/UA validation; batch make-accessible. Foxit gates this heavily; a free-of-charge honest checker is a differentiator.
5. **Auto-bookmarks from text styles/patterns** — Foxit Pro-only; GlyphPDF already has the text-extraction pipeline; cheap build, visible value.
6. **Region OCR + suspects batch panel** — partial-page OCR and a "fix all suspects" pass on top of the existing OCR Verify screen (U03).
7. **Page-management extensions** — interleave (odd/even) merge, split-one-page-into-grid, batch page-size, tiling watermark, dynamic-username watermark, XMP metadata save/import.
8. **Offline entity redaction** — local NER model to match Foxit Smart Redact (names/orgs/roles) without its cloud; slots into the existing redaction transaction + pattern presets.
9. **LAN shared review + comment XFDF round-trip** — shared review via network folder is the one Foxit collaboration feature that is air-gap-compatible; pair with XFDF import/export and comment statuses (PRD v1.4 comments roadmap).
10. **Enterprise-friendly offline controls** — ADMX/GPO template + installer customization (MST-style), shared stamp/template folders, export-format lockdown. Cheap to emulate; the exact reason IT admins pick Foxit.

## 6. Anti-recommendations (do NOT build)

- **Cloud document tracking / DRM (connectedPDF or successor)** — the service was discontinued; built-in obsolescence and trust damage.
- **AI chat/agent, MCP connectors, Workspace, cloud DMS, real-time collaboration, eSign service** — network-tethered; contradicts GlyphPDF's privacy-first stance; Foxit's AI add-on monetization is also the most churn-prone part of its lineup. (Watch their 2026.2 BYOM/SLM local-AI option as the sole data point that local AI is viable — a future offline AI lane could counter-position.)
- **ECM/OA plug-in suites (SharePoint/iManage/NetDocuments/eDOCS/China-OA)** — enormous maintenance surface, wrong target market for a local-only workstation.
- **Admin Console / Update Server cloud services** — replace with plain GPO/ADMX + signed installers.
- **Free-tier nagging, bundled offers, default-app popups, forced major-version paid upgrades** — the documented Foxit reputation tax; GlyphPDF's trust model is the counter-position.

## 7. Sources (spec-level)

- Foxit *PDF Products Comparison* datasheet (official edition-gating matrix, v2026.2/v14): https://cdn01.foxitsoftware.com/pub/foxit/datasheet/general/en_us/pdf-products-comparison.pdf — local evidence copy: `C:\Users\User\Projects\pdf\.context\research\_foxit_comparison.pdf` (extracted text `_comparison.txt`)
- Foxit PDF Editor Version History & Release Notes (official): https://www.foxit.com/pdf-editor/version-history.html (local extract `_tmp_vh.txt`)
- Foxit KB: Perpetual License Support Policy (effective Aug 13, 2025): https://kb.foxit.com/s/articles/Foxit-PDF-Editor-Perpetual-License-Support-Policy
- Foxit KB: v11/v12 discontinuation / upgrade to v14: https://kb.foxit.com/s/articles/4410630911764 and https://kb.foxit.com/s/articles/Steps-to-Upgrade-to-Foxit-PDF-Editor-Version-14-on-Windows
- Foxit KB: ConnectedPDF discontinued (Preferences/Wizard article): https://kb.foxit.com/s/articles/ (Wizard Section 8: Preferences)
- Foxit Blog: Perpetual License Support Policy clarification: https://www.foxit.com/blog/foxits-perpetual-license-support-policy-what-you-need-to-know/
- PDF Association (member news, Aug 22 2025): Foxit 2025.2 and v14 release highlights (MCP Host, 64-bit, DKE, N-1): https://pdfa.org/foxit-20252-and-v14-release-highlights-faster-smarter-more-secure/
- Foxit press: v2025.3 Smart Command: https://www.foxit.com/company/press/11200.html
- Foxit pricing page: https://www.foxit.com/pdf-editor/pricing/
- Foxit user guide: Shared Review: https://www.foxit.com/resource-hub/user-guide/foxit-pdf-editor-for-windows/shared-review/
- Foxit user guide: Add custom commands to custom actions: https://www.foxit.com/resource-hub/user-guide/foxit-pdf-editor-for-windows/add-custom-commands-to-custom-actions/
- Foxit KB: Standard vs Business (legacy edition split): https://kb.foxit.com/s/articles/360040658511
- Foxit refund policy: https://www.foxit.com/support/refund-policy/
- Reddit: r/sysadmin "Foxit is phasing out perpetual licenses": https://www.reddit.com/r/sysadmin/comments/1l7rfeh/foxit_is_phasing_out_perpetual_licenses/
- Reddit: r/sysadmin "Foxit!" (deployment praise): https://www.reddit.com/r/sysadmin/comments/1k4mqgq/foxit/
- Reddit: r/sysadmin "PSA: Foxit working well… replace Acrobat Pro and Docusign": https://www.reddit.com/r/sysadmin/comments/1qx5922/psa_foxit_working_well_for_us_to_replace_acrobat/
- Reddit: r/sysadmin pricing thread: https://www.reddit.com/r/sysadmin/comments/1sqshzs/cheapest_adobe_reader_subscription_to_just_edit/
- Reddit: r/software reader comparison: https://www.reddit.com/r/software/comments/10sf8bi/comparison_of_pdf_readers_adobe_foxit_pdfxchange/
- Reddit: r/geek "Foxit… slowly turning into Acrobat": https://www.reddit.com/r/geek/comments/aavad/ ; r/software "creepier & weirder": https://www.reddit.com/r/software/comments/2w4obq/ ; r/pdf default-app popup: https://www.reddit.com/r/pdf/comments/16fpa4b/
- BleepingComputer: "Beware of Foxit PDF Reader!": https://www.bleepingcomputer.com/forums/t/525069/ ; SuperUser: "Is Foxit reader adware?": https://superuser.com/questions/186893/
- Spiceworks: perpetual licensing discussion: https://community.spiceworks.com/t/foxit-to-discontinue-perpetual-licensing-for-pdf-editors/966716
- Reviews: Capterra https://www.capterra.com/p/249044/Foxit-PDF-Editor/reviews/ ; G2 https://www.g2.com/products/foxit-pdf-editor/reviews ; Trustpilot https://www.trustpilot.com/review/foxit.com ; BBB https://www.bbb.org/us/ca/fremont/profile/computer-software/foxit-software-company-1116-294986/complaints ; CASRAI https://casrai.org/guides/foxit-pdf-editor-review ; RedactifyAI (Smart Redact = Editor+ gating) https://www.redactifyai.com/blog/redactifyai-vs-foxit-pdf-editor/ ; schneider.im perpetual analysis https://www.schneider.im/foxit-perpetual-license-end-of-active-development/

## Confidence Level

**High (≈0.85)** for feature existence, version timeline, licensing shift, and pricing — anchored on the official datasheet PDF, official release notes, and official KB, each corroborated by independent community sources. **Medium** for exact Standard-vs-Pro checkmark placement on ~15% of datasheet rows (check glyphs lost in extraction; those rows marked "inferred"), for print-dialog booklet/N-up specifics (UNVERIFIABLE this session), and for the historical OA-plug-in framing (China-edition specific, PARTLY_TRUE). Community sentiment claims are graded no higher than the corroboration count supports (low-tier sources used as corroboration only).

**Known gaps:** Foxit Admin Guide (full deployment PDF) not pulled; user-guide measurement chapter not pulled (measurement feature set sourced from datasheet + 2026.2 release notes); no live probe of the installed product.
