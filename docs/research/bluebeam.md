# Spec Sheet: Bluebeam Revu (Revu 21 subscription line; calendar-2024 releases 21.1–21.3; legacy Revu 20)

**Date:** 2026-09-07
**Requested by:** coordinator (GlyphPDF parity research)
**Research question:** What does Bluebeam Revu (2024/21 releases) ship per domain — especially the Markups list ("crown jewel"), AEC workflows (Sets, punch, legends), batch tooling — with plan/edition gating, cloud dependence, real-behavior quirks, and the 2023–24 subscription-transition fallout — as a machine-comparable spec sheet against GlyphPDF's offline-first baseline (ledger 2026-09-05 + PRD §27).
**Format:** Coordinator override — feature-by-feature spec tables, one row per feature. Verdict grades per research-specialist scale (TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE). Most rows are graded from primary sources (official Revu 21 user manual, Revu 20 online help, official release notes, official pricing/FAQ, official product-comparison PDF).

## Version-naming correction (load-bearing)

| Claim | Verdict | Evidence |
|---|---|---|
| There is NO product called "Revu 2024". Calendar-2024 releases were **Revu 21.1 (Apr 9, 2024), 21.2 (Jun 25, 2024), 21.3 (Sep 24, 2024)**. | TRUE | official Revu 21 release notes |
| There are NO "Revu Prime" / "Revu Complete" / "Revu CAD" desktop editions. Revu 20 and earlier = **Standard / CAD / eXtreme** (perpetual). Revu 21 = subscription plans: launched as a single "Revu 21" plan (~$400/yr, late-buy-by-phone era), later restructured to **Basics / Core / Complete** (+ **Max** AI tier, intro $590). "Prime" existed only as **Studio Prime**, a retired cloud/API add-on (integrations API EOL 2025-06-30, full EOL 2025-12-31, replaced by Org Admin + Bluebeam Public API). | MOSTLY_TRUE (tier-restructure exact month not pinned; all other elements TRUE) | bluebeam.com/pricing; revu-20-eol.html; Symetri product-comparison PDF; community.bluebeam.com Studio Prime EOL threads |
| Timeline: Revu 21 first listed in official release notes 2022-09-20 (sign-in, Tool Chest Anywhere, 2023 plugins). Perpetual sales ended (Revu 20 last sale 2023-09-30). Revu 20 EOS 2026-07-31, EOL 2026-12-31 (critical patches only until then). | TRUE | release notes; revu-20-eol.html; pricing-page FAQ |

Edition legend for tables: **B** = Basics $260/yr · **C** = Core $330/yr · **CP** = Complete $440/yr · **M** = Max $590/yr (annual billing only, per user; all plans include Revu desktop + web/mobile, BBID sign-in, up to 5 devices). Legacy Revu 20: **Std / CAD / Xt** (Xt = eXtreme). "Cloud?" = needs Bluebeam cloud service beyond the mandatory subscription/license check.

---

## 1. Viewing & navigation

| Feature | Sub-capabilities | Edition | Cloud? | Verdict | Notes (real behavior, limits, quirks) |
|---|---|---|---|---|---|
| Multi-doc tabs & torn-off windows | Open many PDFs in tabs; drag tab out to its own window/monitor; multiple Revu instances | All | N | TRUE | Symetri comparison lists both; multi-instance is an explicit feature row |
| Sets navigation (see §9) | Multi-file "virtual document" navigation without merging | C+ (create/modify; view in B) | N | TRUE | revu21 manual: "open and view Sets" in Basics, "create/modify" gated |
| Bookmarks & page labels | Manual + **automatic creation from PDF content** (titles/region text); TOC from bookmarks | Auto: C+ (Std had manual) | N | TRUE | subscription-features page splits manual vs automatic; auto-generation is the differentiator |
| VisualSearch | Search for graphical symbols (find all instances of a drawn symbol) | C+ (Xt in Revu 20) | N | TRUE | Symetri feature row "Search for symbols with VisualSearch" |
| Search panel | Full-text incl. OCR'd scans, markups, bookmarks/attachments; multi-file/folder search | All | N | TRUE | Search over scanned PDFs requires OCR text layer first (§3) |
| Dark Mode | Dark rendering of any file ("turning any file into a dark color scheme") | All (21.3+) | N | TRUE | Added 2024-09-24 in Revu 21.3 — arrived years after competitors |
| Preferences search | Search settings in Preferences dialog | All (21.1+) | N | TRUE | 21.1 release note |
| Profiles & workspaces | Save panel/toolbar layouts; profiles shareable; custom columns template rides a Profile | All | N | TRUE | Profile mechanics referenced across help; "My Tools duplicated" fix 21.6.1 shows live quirks |
| Keyboard shortcuts | Fully re-mappable per command; Tool Chest tools auto-take number-key shortcuts (punch keys, §11) | All | N | TRUE | keyboard-shortcuts.html; community confirms My Tools auto-assign |
| Markup Selection Cycle | Cycle through stacked/overlapping markups to select the right one | All (21.4+, Jan 2025) | N | TRUE | release notes; big win for dense CAD sheets |
| Hide individual markups | Temporarily hide one obscured markup without Hide-All mode | All (21.2+) | N | TRUE | 21.2 release note; classic Hide Markups (whole doc) predates it |
| Split/synchronized views | Split workspace into multiple views of same/different docs | All | N | PARTLY_TRUE | Split views exist (release notes reference switching views); deep split-sync behavior not re-verified from primary docs |
| 3D PDF viewing | View 3D PDFs; mark up 3D views | View: all; mark up: C+ | N | TRUE | subscription-features: "Markup and manipulate 3D PDFs — view only" for lower tiers |
| WebTab / embedded browser | Open links in in-app browser tab | All (21.8+ era) | N | MOSTLY_TRUE | 21.8/21.11 release notes discuss web-tab links; exact intro version unpinned |
| GoTo markup navigation | Click row in Markups list → view jumps to markup; arrow-key stepping through markups | All | N | TRUE | Revu 20 help (Markups List) |

**GlyphPDF delta (viewing).** Gap: GlyphPDF has no Sets-style multi-file virtual navigation, no VisualSearch symbol search, no auto bookmark/page-label extraction, no Selection Cycle for stacked markups. Advantage: GlyphPDF's PDF/A export, tagged-PDF reading-order check, and local-only badge have no Revu equivalent; Revu 21's sign-in wall is a hard anti-feature. Bluebeam's 2024 "new" items (Dark Mode, prefs search) show the 21.x line prioritized AEC workflow over viewer basics — an opening for a fast general-purpose viewer.

## 2. Editing (text / object / page)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|
| Edit PDF content | Erase content, Cut content, Edit text | B+ (edit tools in all plans; erase/cut listed under "edit PDFs") | N | TRUE | Erase = permanent content-stream deletion incl. hidden layer content; "won't let you paste what you remove" |
| Markups as the editing model | Custom markups, flatten, import markups, assign to layers, translate markups | B+ (translate: CP historically Xt) | N | TRUE (translate tier PARTLY_TRUE) | Revu is markup-centric; no paragraph-reflow text editor like Acrobat — text edits happen via Text Box markups or Cut/Erase |
| Page organization | Extract, delete, rotate, insert blank/existing pages; split without altering source; resize/custom page sizes | B+ | N | TRUE | subscription-features page; 21.2 added per-operation PDF units independent of Preferences |
| Combine / stitch | Combine files to one PDF; stitch sheets side-by-side into one view | Combine: B; multi-view Stitch: M | N | TRUE | Stitching (multi-view with matchlines) is a Max AI-era feature |
| Headers/footers | Add/edit | B+ | N | TRUE | feature list; recurring conversion-bug reports (21.7) show the pipeline's fragility |
| Reduce file size | Preset + custom fidelity options | B+ | N | TRUE | Known bug class: 21.11 fix "Reduce File Size could remove text from Allplan PDFs" |
| Multiply (offset copies) | Offset duplicate markups with direction/rotation/distance/count + preview | All (21.0.50+) | N | TRUE | Release notes; classic AEC grid placement |
| Convert markup types | Duplicate as different type (e.g., cloud→polygon); convert Polylength→Arc | All (21.x) | N | MOSTLY_TRUE | Feature list "convert markup types/duplicate as different types"; Arc conversion quirk fixed in 21.2 |
| Round corners | Round one/more corners of Polygon & Polyline markups | All (21.8+) | N | TRUE | Release notes |
| Sketch to Scale | Calibrated sketch markups that rescale with viewports | C+ | N | TRUE | feature list |
| AutoAlign text box sizing | Text boxes auto-size to content | All (21.11) | N | TRUE | Release notes |
| PDF/A export | — | — | — | **FALSE (feature absent)** | No PDF/A creation anywhere in Revu 21/20 feature matrix — Bluebeam ships no archiving-standard export. Direct GlyphPDF strength (1B/2B/2U/3B/3U + veraPDF wiring) |
| Paragraph text editing (reflow) | Edit native text in place like a word processor | — | — | PARTLY_TRUE (weak) | Revu's "Edit text" is limited vs Acrobat; AEC users don't buy Revu for prose editing — not a gap worth chasing |

**GlyphPDF delta (editing).** Gap: Multiply-style offset duplication, markup-type conversion, sketch-to-scale. Advantage: GlyphPDF edits text objects directly and exports PDF/A; Revu's erase/redact operate similarly at content-stream level, which validates GlyphPDF's excision architecture. Bluebeam's repeated reduce-size/print regression fixes underscore how big the CAD-format surface is — GlyphPDF should not chase CAD plugins.

## 3. OCR

| Feature | Sub-capabilities | Edition | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|
| OCR to searchable PDF | Document > OCR or Batch > OCR; add files/folders/open files/current Set; page-range selectors incl. even/odd/portrait/landscape | CP (Revu 21) / Xt (Revu 20) | N | TRUE | Also invoked by Create-PDF-from-Scanner; cannot run on digitally signed/certified PDFs |
| Language support | Multiple languages selectable per run (multi-language same PDF); third parties cite 35+ languages | CP | N | MOSTLY_TRUE | Manual confirms multi-language selection; "35+" figure is partner (Graitec) — exact matrix UNVERIFIABLE |
| Document-type optimization | Document Type: CAD Drawing vs Text Document; Optimize For: Accuracy vs Speed | CP | N | TRUE | Manual; CAD mode "tends to ignore text formatting" |
| Preprocessing | Correct Skew; Detect Orientation (90/180/270); Detect Vertical Text; Detect Text in Pictures; Skip Vector Pages; Max Vector Size filter | CP | N | TRUE | Manual — parity with GlyphPDF's deskew/orientation/binarize set |
| Page Chunk Size | Max pages sent to engine per chunk; chunk=1 recommended for large formats; re-run with 1 fixes empty results | CP | N | TRUE | Manual documents the failure mode itself — OCR silently returning nothing is a known Revu quirk |
| 64-bit requirement | OCR requires 64-bit Revu | CP | N | TRUE | support troubleshooting page |
| Engine | Single embedded engine (module updated in 21.0.30); no engine choice, no confidence scores, no review UI | CP | N | MOSTLY_TRUE | Manual + deployment guide; no user-facing confidence/review workflow anywhere in docs — the engine is a black box |

**GlyphPDF delta (OCR).** Gap: none material — GlyphPDF's dual-engine Tesseract+PP-OCRv5 ROVER ensemble, per-word confidence, and OCR Verify review screen exceed Revu's black-box, no-review OCR. Revu's edge is integration: OCR feeds AutoMark page-region reading, Sets auto-matching, and Batch Link (all require "text detection"), i.e., OCR as plumbing for sheet intelligence rather than an end in itself. Verdict: GlyphPDF is ahead on engine; behind on downstream integration.

## 4. Forms

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Create forms (manual) | Place text box, radio, check box, list box, dropdown, button, digital-signature fields | CP/M only | N | TRUE |
| Auto Create Form Fields | Scan content layer, auto-detect text fields, checkboxes, radio, date-formatted, signature fields (by shape + text clues like "Signature" label); auto-names fields from nearby text; engages Edit Form mode; offers OCR fallback when nothing detected | CP/M only (Xt in 20) | N | TRUE |
| Field behavior | JS field events (calculate/validate/format), appearance customization, validation & input formats, unique-name model | CP/M | N | TRUE |
| Fill forms | Fill, tab order, JS-driven actions | B+ (fill) | N | MOSTLY_TRUE |
| Merge form data | Merge FDF-style data across forms | CP/M | N | MOSTLY_TRUE |
| Flatten completed forms | Flatten markups incl. form content | B+ | N | MOSTLY_TRUE |
| Auto-create reliability | "Automatically Create Form Fields produced an error and failed" (21.6.1) / "did not detect form fields" — twice-fixed in 2025 | CP/M | N | TRUE | Release-note-confirmed fragility |

**GlyphPDF delta (forms).** Gap: small — GlyphPDF already has all 10 field types + calculated fields + auto-detect; Revu adds nothing conceptually except the OCR-fallback loop and signature-field auto-detection by adjacent label text (worth stealing: label-adjacency typing). Advantage: GlyphPDF form edits ride safe-save transactions with real undo; Revu's form auto-detect has shipped regression bugs two releases running. Revu forms creation being Complete-only shows even "basic" authoring is upsell ammo.

## 5. Markups list (crown jewel — capture precision matters)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict | Notes |
|---|---|---|---|---|---|
| Automatic tracking | Every markup auto-tracked with author, date(s), color, comments; one row per markup; selecting a row jumps the view; Up/Down arrow steps through markups | B+ | N | TRUE | Revu 20 help |
| Built-in columns | Subject (icon+text, editable), Page Label, Page Index, Lock, Status, Checkmark, Author, Date, Creation Date, Color, X, Y, Document Width/Height, Comments (incl. replies), Wall Area, Length, Area, Volume, Count, Measurement, Label, Sequence, Capture, Depth, Rise/Drop, Slope, Layer, Legend, Height/Width (real-world size), Space, Unit, 3D View, Center Coordinates (21.0.50+) | B+ | N | TRUE | Full list captured from help; ~30+ columns |
| Column show/hide/reorder | Columns menu toggles; Manage Columns dialog sets display order (up/down); drag headers in list; hidden columns keep storing data | B+ | N | TRUE | |
| Custom columns — 6 types | Checkmark; Choice (CSV import: item/subject/numeric value; currency/percent formats; allow-custom-text toggle); Date (formats; default none/current/custom); Number (min/max/default, totals); Text (multiline, default); **Formula** (variables = measurement + custom columns; constants e/pi; functions acos..tan; operators + − * / ^ % negation; parentheses; currency/percent/decimals; Include-In-Totals feeds section dividers) | Formula & takeoff columns: CP (per Symetri "Add formulas to custom columns") / Xt | N | TRUE | Measurement column = unit-free numeric of whatever measure type — that's what formulas consume |
| Column template across documents | "Save to Profile" applies custom columns to ALL PDFs opened/created (data written only when used, avoiding dirty saves) | B+ | N | TRUE | Elegant detail worth copying verbatim |
| Sorting | Click header to sort, click again to reverse; secondary sort by creation date on Subject/Page; sorted rows GROUP under collapsible Subject section headings | B+ | N | TRUE | Section grouping is the visual signature of the list |
| Filter List | Filter row above columns; filter by ANY column; multi-select values per column (OR within column); AND across columns; filtered-out markups DIM on the PDF (dim % configurable via "Filtered Annotation Dim" preference); filter headers turn orange | B+ | N | TRUE | |
| Custom Filter dialog | Per-column rules (equals / does not equal / before / after / greater / less …), chained AND/OR conditions, any number, multiple columns simultaneously | B+ | N | TRUE | Date columns get before/after; numeric get comparisons |
| Dynamic date filters | "Past 7 days" style relative date picks in the filter row | B+ | N | TRUE | Revu 21 how-to |
| Search filter | Free-text across all columns (clears other filters when typed) | B+ | N | TRUE | |
| Saved Filters | Name + save current filter sets (standard + custom); reuse on any PDF; asterisk marks modified-engaged set; custom-column filters only reusable where same name+type column exists; delete with confirm | B+ | N | TRUE | |
| Statuses | Set Status per markup; defaults: Accepted, Rejected, Completed, Cancelled, None | B+ | N | TRUE | |
| Custom Status models | Manage Status dialog: Models (preloaded Review + Migration, unalterable) contain States; each state has name + optional auto-color (recolors the markup) + optional auto-text (fills grouped Text Box when status set) | B+ (custom statuses) | N | TRUE | 21.2 added status KEYBOARD shortcuts; custom statuses listed as Core-tier feature row in Symetri matrix (grading: B+ with C listed — PARTLY_TRUE on tier) |
| Export Latest Status Only | Summary can export only latest status, omitting audit history | All (21.2+) | N | TRUE | Release note; the audit history otherwise bloats summaries |
| Reply threads | Reply creates indented row under markup; shown in Comments column; replies allowed on locked markups | B+ | N | TRUE | |
| Checkmark column | Turn the list into a checklist; clear-all command | B+ | N | TRUE | |
| Lock from list | Lock/unlock markup from the row; locked = no geometry edits, but status/replies still allowed | B+ | N | TRUE | |
| Grouping under Subject | Sorted lists collect under collapsible Subject sections | B+ | N | TRUE | |
| Markup Summary export | Publish summary as separate PDF **or appended to current PDF**; Columns tab picks/reorders report columns; Filter-and-Sort tab applies same filter/sort machinery (multi-level "sort by… then by…"); export as CSV or XML | CSV/XML: CP (Basics: PDF only) | N | TRUE | The report inherits the list's configuration — same engine, three output formats |
| Batch Summary | Run the summary across multiple files | CP | N | TRUE | Symetri row + subscription-features page |
| Spaces-based summaries | Summaries scoped by pre-defined Spaces (regions) | CP | N | TRUE | "Track and generate reports on markups located in pre-defined customized regions" (Symetri) |
| Markup import/export | Import markups from PDF, XML, **BAX** (Bluebeam XML; double-click import; rename .bax→.xml); export to BAX or FDF; FDF import from other apps | B+ | N | TRUE | BAX also imports into Excel |
| Capture (media in markups) | Embed photos (B) / 360° photos + videos (CP); Capture Summary report; export media to folder; mark up embedded images (21.6) | tiered | N | TRUE | Capture column shows indicator; double-click previews |
| Legends from list | Legend column ties markup to a Markup Legend (§11) | CP | N | TRUE | |
| Custom captions on measurements | Measurement captions can display any Markups-list column value (21.0.50+) | CP | N | TRUE | Release notes — measurement text becomes data-driven |
| Known quirks | Custom-column CSV import broke on non-English delimiters (fixed 21.2); formula decimal-char bug (fixed 21.2); Studio Session reports mislabeled Page Label (21.4); custom columns clipped in Properties panel (21.0.50) | — | N | TRUE | Release notes |

**GlyphPDF delta (Markups list).** This is the single largest structural gap between GlyphPDF and Revu. GlyphPDF's U07 Comments panel (filter summary, count, clear, table view, CSV export) is a comment viewer; Revu's Markups list is a **queryable production database** living next to the drawing. Concretely missing in GlyphPDF: per-markup row model with ~30 typed columns; user-defined custom columns with a formula engine and section totals; filter row with per-column multi-select + AND/OR custom rules + saved named filters; dimming of filtered-out annotations on canvas; status models with auto-recolor/auto-text and keyboard status entry; subject-grouped collapsible sorting; summary publishing that reuses the exact column/filter/sort configuration into PDF/CSV/XML (and batch across files); BAX markup interchange. Conversely GlyphPDF leads on: RFC-4180-pinned CSV export, page-sort defect fixes under test, and the local-only guarantee (Revu's list is also the telemetry surface for Studio). Recommendation in §Opportunity.

## 6. Redaction

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Redaction tool | Two-step: Mark for Redaction (drag over text / box over image) then Apply Redactions; content "completely deleted from the PDF's content stream" | CP/M only ("Complete and Max plans include redaction tools") | N | TRUE |
| Apply scope | All pages / current / custom range (1-3, 5, 9) at apply time | CP/M | N | TRUE |
| Appearance | Outline color, Redact fill color, Overlay Text ("CONFIDENTIAL"), custom text, **DOD/FOIA preset codes**, Autosize Text, Repeat Text (fills area with repeated text) | CP/M | N | TRUE |
| Advanced control | Choose redact text / images / both | CP/M | N | TRUE |
| Extra data removal | Redaction Options at apply: remove additional data/metadata (community-documented: hyperlinks, bookmarks, markups, form fields) | CP/M | N | MOSTLY_TRUE (official page confirms "additional data or metadata"; exact list from secondary source) |
| Reusable redaction tools | Save configured redaction (appearance + overlay text) to Tool Chest | CP/M | N | TRUE |
| Erase Content | Permanent content-stream deletion with NO visible mark; rectangle or polygon; removes even content hidden on other layers; destroys unflatten-ability of that page's flattened markups | B+ | N | TRUE |
| Cut Content | Remove-and-keep-for-paste counterpart | B+ | N | TRUE |
| Guards | Cannot redact/erase digitally signed or certified PDFs; warning dialog skippable | CP/M | N | TRUE |

**GlyphPDF delta (redaction).** Near-parity and architecturally identical (content-stream excision; RedactOperation 7-stage transaction). Revu adds: DOD/FOIA coded overlay text, repeat-text fill, and redaction-tool presets in a tool palette. GlyphPDF adds: default-ON sanitize bundle with visible contract, word-list pattern redaction, and SHA-256 source-invariance tests. Gap worth closing: preset redaction codes and "apply across range at burn time" (GlyphPDF has mark-all page list; Revu's apply-time scope selection is a small UX edge). Verdict: GlyphPDF at parity-plus for safety, minus polish codes.

## 7. Security & permissions

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Password protection | Open + permission passwords; set printing/copying/modification restrictions | B+ | N | TRUE |
| Certificate security | None in Revu (no certificate-encryption of documents); file exchange security via Studio instead | — | — | MOSTLY_FALSE (as a feature) — absent from all matrices |
| MIP support | Open Microsoft Information Protection-protected PDFs read-only within the org | All (21.0.30+) | Y (MIP service) | TRUE |
| Vitrium/DRM | Open Vitrium-protected files (blank-file bug fixed 21.10) | All | Y | MOSTLY_TRUE |
| FileOpen DRM | Opens FileOpen-encrypted PDFs (fixed 21.0.45) | All | Y | MOSTLY_TRUE |
| Metadata sanitization | No standalone "sanitize document" dialog; extra-data stripping exists only inside Apply Redactions | CP/M | N | PARTLY_TRUE |
| BEAst profile | Swedish BEAst 3.0 compliance profile included (21.0.50) | All | N | TRUE |

**GlyphPDF delta (security).** GlyphPDF is ahead across the board: AES-256 with explicit permissions, watermarks, XMP expiry, encrypted-ZIP secure package, metadata sanitization as first-class default-ON flow. Revu's model assumes trust comes from signing + Studio, not from encryption UI. No gap to close; this is a marketing differentiator ("your redaction and sanitization don't require a Complete plan").

## 8. Signatures

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Digital IDs | Create self-signed PKCS#12 (.pfx) in-app; create in Windows Certificate Store; import P12; auto-detect eTokens/USB dongles/Windows store; password management; export public cert; Trusted Identities store | B+ | N | TRUE |
| Sign & certify | Sign signature fields; Document Certification with Permitted-Changes options (allow markups/form-fill/other signatures); Reason/Location/Contact metadata; customized signature appearance | B+ | N | TRUE |
| Signature tracking | Track all signatures/certificates on doc; validation status | B+ | N | TRUE |
| Batch Sign & Seal | Apply cert/signature + professional seal + date across a batch; page-range per file; saved batches; skip docs without matching fields | CP/M (Xt) | N | TRUE |
| Third-party CAs | Works with DigiCert document-signing certs etc.; Entrust sign-in supported (double-prompt bug fixed 21.6) | B+ | N | MOSTLY_TRUE |
| Interop caveat | Adobe-issued digital IDs not directly portable; import issuing CA as trusted identity instead | — | N | MOSTLY_TRUE (community-corroborated) |
| Known placement bug | Batch Sign & Seal put signature fields at wrong locations on odd page sizes (community threads; preview-resize fix 21.7) | CP/M | N | TRUE |
| E-sign (send/track) | Not a Revu feature — no send-for-signature, no signing-order, no reminders (that space belongs to Studio partners/GoCanvas Task Link, §13) | — | Y | TRUE (absence) |
| PAdES B-LT/B-LTA | Not advertised anywhere in Revu docs/matrix | — | — | UNVERIFIABLE → treat as absent |

**GlyphPDF delta (signatures).** GlyphPDF leads technically: PAdES B-LT/B-LTA with DSS, OCSP validation, trust-chain display, visible appearance generation — none of which Revu advertises. Revu leads operationally: Batch Sign & Seal (certify+seal+date across hundreds of sheets with saved batches) is a real AEC deadline feature GlyphPDF lacks; its documented placement bugs are a warning to test per-page-size. Gap worth closing: batch sign with per-file placement + seal stamping.

## 9. Compare (Sets / Compare / Overlay)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Compare Documents | Two-PDF raster comparison producing difference MARKUPS (reviewable/filterable in Markups list); best for scanned/raster | C+ ("Overlay/compare drawings" in Core) | N | TRUE |
| Advanced Comparison Options | Presets: Printed (same printer), Printed (different printer), Scanned; custom types savable; Grid Size, Pixel Density, Color Sensitivity, Pixel Proximity Allowance, DPI (72 default/144 fine), Margin, Include Markups, Include Flattened Markups; difference-markup Subject naming/color/opacity/width/lock/cloud | C+ | N | TRUE |
| Overlay Pages | 2+ PDFs color-layered into one PDF (red/green blend); per-layer color/opacity/blend/background; Select Region per layer; page ranges per layer | C+ | N | TRUE |
| Alignment methods | Page Align (stack); Manual Align (3 anchor points, same order); **Auto Align** — AI point-matching for different-size/scale sheets (NEW 21.1, "up to 8x faster" in 21.2) | C+ | N | TRUE |
| Batch Compare / Batch Overlay | Compare/overlay groups of PDFs | CP | N | TRUE |
| Sets | Open a folder of drawings as one ordered virtual document incl. revisions/addenda, no merging; works around per-file signatures/security; Sort By (Page Label / File Name+Label / File Name+Index), alphabetic vs alphanumeric ordering (documented with numeric-aware examples), Stack Multi-Page Documents, Stack Revisions By, Revision Filter (Auto-detect naming strategy / Wildcard / Off), Categories (Off/Auto/Manual), Tags (Sheet Number/Revision tags drive sorting) | Create/modify: C+; view: B | N | TRUE |
| Sets auto-matching | Auto-matching aligns current sheets with revisions by file name/page label/page region; used by Batch Slip Sheet and compare-on-Sets | C+ | N | TRUE |
| Document health / Smart Review (AI) | Missing/blank sheet detection, duplicate numbers, tag & gridline validation, door-tag mismatch checks (21.10) | M | Y (AI) | TRUE |

**GlyphPDF delta (compare).** Fundamental architectural divergence: Revu compares RASTERIZED pixels (grid/pixel-density model) and never detects structural page changes — no added/removed/reordered page events, no text diff, no report of structural deltas. GlyphPDF's DiffEngine (page fingerprints, LCS alignment, PageAdded/PageRemoved/ALIGNED-MODIFIED, change filters, HTML/text reports) detects exactly what Revu structurally cannot. Revu compensates operationally: Auto Align handles scale/rotation-offset drawings (GlyphPDF's alignment assumes same geometry — a real gap for scanned AEC revisions), Sets-as-revision-rack + Batch Slip Sheet solve "which page replaced which" outside the diff. Recommendation: keep structural diff as the flagship; add an optional image-alignment pre-pass (Auto Align equivalent) and a Sets-like revision-stack navigation.

## 10. Batch (Batch Slide/"Transform" equivalents)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Batch OCR | OCR over files/folders/Sets | CP | N | TRUE |
| Batch Link® | Auto-create navigational hyperlinks across doc groups; search-term sources: Filename / Page Region (AutoMark) / manual CSV import; destination rules; saved configurations; runs on Sets; unopened files modified in place (not checked in) | CP/M | N | TRUE |
| Batch Slip Sheet | Insert revisions / replace pages across many docs; Match Pages By: File Name+Page Index / Page Label / Page Region (AutoMark) / Manual Correlation; unmatched pages dropped (except manual) | CP/M | N | TRUE |
| Batch Compare / Batch Overlay | See §9 | CP | N | TRUE |
| Batch Sign & Seal | See §8 | CP/M | N | TRUE |
| Batch Stamp (apply stamps) | Batch apply stamps across files | B+ | N | TRUE |
| Batch process hyperlinks | Update/verify hyperlinks across sets | CP | N | MOSTLY_TRUE |
| Apply scale to multiple sheets | Batch viewport/scale application | CP | N | TRUE |
| Scripting commands | Revu scripting for automation | CP (Xt) | N | TRUE |
| Hot folder / watched folder | **Absent** — no folder-watching anywhere in Revu; automation = manual batch dialogs + scripting | — | — | FALSE (feature absent) |
| Batch Summary | Markups summary across multiple files | CP | N | TRUE |

**GlyphPDF delta (batch).** Revu's batch set is deep but dialog-driven and Complete-gated; its superpowers are AutoMark page-region reading and Slip Sheet revision matching — neither exists in GlyphPDF. GlyphPDF's hot-folder automation has NO Revu counterpart (real differentiator for reprographics/IT shops). Gap worth closing: batch slip-sheet (replace-by-matching) and batch hyperlink generation; keep hot-folder as the anti-subscription headline.

## 11. Punch / AEC workflows (punch keys, legends, Tool Chest, Spaces, takeoffs)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Tool Chest | Reusable tools; Tool Sets (categories); Reuse-on-place mode; Properties mode; Sequences; export/import/share .btx tool sets; network-drive shared sets (Add = live link, Import = copy); "Show In All Profiles" | B+ | N | TRUE |
| My Tools auto-shortcuts | Tools in chest take number-key "punch keys" for rapid placement while walking a punch list | B+ | N | TRUE |
| Punch tool sets | Bluebeam-published free .btx sets (Punch, Punch Cleaning, Punch Electrical, …); custom punch sets; punch workflow = Spaces + punch template + room snapshots on sketch pages | B+ | N | TRUE |
| Punch Key CSV import | Import punch-key assignments from CSV (regional-delimiter bug fixed 21.2; new Punch Key options 21.6) | B+ | N | TRUE |
| Markup Legends | Dynamic on-page table of markups: **Tool Set Legends** (auto-include everything placed from a tool set, even before placement) and **Ad-hoc Legends** (from selection; match by type+appearance); auto-update quantities as markups come and go; Source Pages scope (all/current/multi-page); Apply-to-All-Pages; dynamic copy across docs; static Snapshot copy; savable to Tool Chest; configurable columns = any Markups-list data (takeoff totals, open-issue counts) | CP (Xt) | N | TRUE |
| Legend limitation | Legends become static Snapshots inside Studio Sessions; auto-update resumes on removal | CP | Y | TRUE |
| Spaces | Named regions (rooms/zones/disciplines) applied to pages; markups auto-report their Space; Spaces feed filtering, summaries, punch workflows | C+ | N | TRUE |
| Measurement toolset | Length, Area, Perimeter, Polynength (Rise/Drop, Slope), Volume, Angle, Count (with Resume Count), Wall Area; Viewports = multiple scales per PDF; preset scales; calibrate tool sets to scale; custom measurement captions from any column | B: length+area only; C+: advanced | N | TRUE |
| Dynamic Fill | Section off drawing regions by filling; generates Area/Perimeter fill markups + measurements + Spaces | CP | N | TRUE |
| Quantity Link | Live measure-markup → Excel worksheet linkage (real-time quantity takeoffs) | CP/M | N | TRUE |
| Sketch to Scale | Calibrated polygons/polylines/rects/ellipses that rescale per viewport | C+ | N | TRUE |
| Custom hatch patterns | Create/import .PAT hatches; apply as markup fills | C+ | N | MOSTLY_TRUE |
| Custom line styles | Manage/create line styles for tools | C+ | N | MOSTLY_TRUE |
| Status color coding | Custom statuses recolor markups on set — the visual backbone of punch review (open vs closed at a glance) | B+ | N | TRUE |
| Stamps & dynamic stamps | Dynamic/interactive stamps; batch stamp apply | B+ | N | TRUE |

**GlyphPDF delta (punch/AEC).** This domain is Revu's moat and GlyphPDF's white space. Missing in GlyphPDF: Tool Chest (reusable, shareable tool libraries with reuse/properties modes), punch-key rapid placement, Spaces, markup legends with live quantities, Dynamic Fill, Quantity Link, scale-calibrated tool sets. Loved-workflow evidence (community/training corpus) centers exactly here. Highest-leverage subset to build: Tool Chest + punch symbols + custom statuses + Legends (they compound each other; all four ride the Markups-list data model in §5).

## 12. Import / export & creation

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Bluebeam PDF Printer | Any Windows file → PDF (or TIF/JPG/BMP/PSD/PNG/PCL/GIF/EMF/WMF) | B+ | N | TRUE |
| Office plugins | One-click + batch PDF from Word/Excel/PowerPoint/Outlook; recurring formatting regressions documented (21.2–21.7 fixes) | B+ | N | TRUE |
| CAD plugins | One-click + batch 2D PDF from AutoCAD/Revit/SolidWorks; 3D PDF from AutoCAD/Revit/Navisworks; Revit Rooms→Spaces; AutoCAD sheet-set import; SHX import; hyperlink transfer; Connected Sessions in Revit (M) | C+ (2025 versions: plugin support lands 6–12 months after Autodesk releases) | N | TRUE |
| Export to Office | Export scanned PDFs as editable Word/Excel/PowerPoint | C+ | N | MOSTLY_TRUE |
| Stapler | Batch PDF-creation wizard incl. .txt quirks and output-path bugs (release notes 21.8/21.9) | B+ | N | MOSTLY_TRUE |
| Markup interchange | BAX/FDF/XML import-export (see §5) | B+ | N | TRUE |
| Capture media export | Export embedded markup media to folder | B+ | N | TRUE |
| Create from Scanner/Camera | With OCR dialog auto-invoked | B+ | N | TRUE |
| PDF/A, tagged PDF, accessibility export | Not present | — | — | FALSE (absent) |

**GlyphPDF delta (import/export).** Revu's creation pipeline is plugin-heavy and Windows-printer-based, with a visible bug tail every release; GlyphPDF's in-house OOXML conversion + tracked-temp import routing is cleaner but has no CAD story (fine — CAD plugins are out of scope per PRD §7.2). No PDF/A or accessibility features in Revu at all: both stay GlyphPDF flagship exclusives.

## 13. Studio (cloud collaboration — ANTI-FEATURE for GlyphPDF, marked separately)

| Feature | Sub-capabilities | Edition | Cloud? | Verdict |
|---|---|---|---|---|
| Studio Sessions | Real-time markups on shared PDFs; all participants see each other's markups live; session reports; free participation with BBID (no plan) though limited toolset (21.0.50 widened free participants' tools) | Host: C+; participate: free BBID | Y | TRUE |
| Studio Projects | Centralized cloud file store; check-out/check-in; revoke checkout; offline sync of Project files; Project search incl. markup results (21.7/21.8, US/EN only) | Host: C+; view/upload: free | Y | TRUE |
| Studio in all plans | Every paid plan includes Studio + unlimited centralized cloud storage | All | Y | TRUE |
| Studio Express / legacy servers | Regional servers (US/DE/AU/SE/UK); service-status dashboard published | All | Y | TRUE |
| Task Link (GoCanvas) | Dispatch field tasks from a markup to mobile crews; paid Bluebeam + GoCanvas subscriptions (21.8+) | M-era add-on | Y | TRUE |
| MCP / AI ("Max") | "Use Claude MCP to perform tasks in Revu", AI drawing review, Smart Overlay with match scores, Smart Review checks | M | Y | TRUE |
| Tool Chest Anywhere | Sync Tool Chest to web/mobile via login | All | Y | TRUE |
| **ANTI-FEATURE VERDICT** | Studio is the retention lock: AEC teams stay on subscription because sessions/projects live in Bluebeam's cloud. GlyphPDF's local-only posture cannot and should not clone it; the addressable users are exactly those alienated by it (see Failure modes) | — | Y | TRUE |

---

## Failure modes (transition-era evidence)

1. **Subscription-only conversion + price shock.** Perpetual sales ended (Revu 20 last sale 2023-09-30); $400/yr single-plan launch vs ~$275/yr old perpetual+maintenance effective cost; Basics/Core/Complete restructure re-priced the ladder ($260/$330/$440, Max $590). Verdict: TRUE (official pricing + community cost breakdowns). r/msp literally titled a thread "Price Gouging + increase."
2. **Migration coercion mechanics.** Maintenance customers' ~10%/yr price escalation toward full price; perpetual licenses cannot be reactivated/transferred once EOL passes ("PSA: Bluebeam EOL Ramifications"); Revu 21's best features (redaction, forms creation, batch, OCR, advanced measurement, Compare/Overlay) gated behind Complete — capabilities loyal eXtreme owners already had. Verdict: TRUE (multiple independent threads + official EOL page).
3. **Feature regression at launch, drip-fed back.** Revu 21.0 shipped without several Revu 20 eXtreme capabilities; Compare/Overlay returned overhauled in 21.1, Auto Align in 21.1/21.2, batch/form/script features across 21.x; OCR module patched in 21.0.30. Verdict: MOSTLY_TRUE (release-note-backed for the returns; the exact launch-gap inventory rests on partner comparison guides).
4. **Quality churn on the 21.x line.** 2024–26 release notes carry repeated regressions: Reduce-File-Size deleting text, form auto-detect failing twice, Word-plugin misalignment, Batch Sign & Seal field placement, OneDrive/SharePoint path bugs, ARM64 rendering. Verdict: TRUE (primary release notes).
5. **EOL cliff pressure.** Revu 20 EOS 2026-07-31 / EOL 2026-12-31 forces the subscription decision for the entire installed perpetual base right now. Verdict: TRUE (official).

## Loved workflows (what AEC teams standardize on)

1. **Markups list as the review database** — filter by author/status/subject, dim handled markups on canvas, custom columns for punch metadata (assignee/due date/cost), CSV/XML summaries into Excel for closeout packages. TRUE (help + community workflows).
2. **Tool Chest + punch keys + Spaces** — standardized symbol libraries shared via .btx across the company, number-key rapid placement while walking the site, Space-tagged markups so every punch item reports its room. TRUE.
3. **Sets + Batch Slip Sheet + Overlay/Compare with Auto Align** — revision rack of a full drawing set as one navigable doc, one-shot replacement of superseded sheets, red/green or markup-based change detection for reviewers. TRUE.
4. (honorable mention) **Studio Sessions** for submittal cycles, and **Quantity Link takeoffs** for estimators. MOSTLY_TRUE.

## Top-5 feature gaps (Revu → GlyphPDF build list, ranked by AEC pull)

1. **Markups-list production core** (§5): typed columns, custom columns + formula/totals, filter row + saved filters, status models with auto-color/keyboard, subject-grouped sort, summary publishing (PDF/CSV/XML reusing the config). — the gold standard, fully specified above.
2. **Tool Chest + punch keys + punch tool sets** (§11): shareable .btx-style libraries, number-key placement, published punch symbol sets.
3. **Markup Legends with live quantities** (§11): dynamic on-page summary tables bound to the list.
4. **Sets + Batch Slip Sheet (revision matching)** (§9/§10): virtual multi-file navigation with revision stacking + page-region (AutoMark) matching.
5. **Batch Sign & Seal + Batch Link** (§8/§10): batch certify/seal/date; AutoMark-driven hyperlink generation.

## Single biggest opportunity

**"The Markups list, minus the meter."** Bluebeam just locked its entire AEC installed base (Revu 20 EOL Dec 2026) into a $440/yr Complete plan to keep the features they already owned, behind a sign-in, with Studio gravity on top. A local-only GlyphPDF that implements the Markups-list core + Tool Chest/punch + Legends — BAX-compatible interchange so shops can carry their existing tool sets and markup data over — addresses the exact resentment documented across r/Revu, r/estimators, and Spiceworks, with GlyphPDF's PDF/A, PAdES, structural compare, dual-engine OCR, and hot-folder as untaxed bonuses no Revu tier offers at any price. Verdict on the opportunity framing: the resentment is TRUE (multi-community corroboration); the conversion potential is an inference, not a fact.

## Confidence

High (>0.85) for: current pricing/tiers, edition gating, Markups-list behavior, redaction, OCR options, Compare/Overlay/Sets mechanics, batch tools, forms, signatures, release timeline, sentiment. Medium for: Revu 21 launch-tier naming/history, OCR language count, a few "MOSTLY_TRUE" edition splits taken from the Symetri/Bluebeam comparison PDF whose checkmark cells don't extract as text. All such rows are graded in-line.

## Sources

Primary (official):
- Revu 21 release notes — https://support.bluebeam.com/revu/resources/revu-21-release-notes.html
- Bluebeam pricing (Basics/Core/Complete/Max, Revu 20 EOL FAQ) — https://www.bluebeam.com/pricing/
- Subscription features matrix — https://support.bluebeam.com/revu/subscription/subscription-features.html
- Subscription FAQ (plans, 5 devices, Revu 20 patches, languages) — https://support.bluebeam.com/revu/subscription/onboarding/bluebeam-subscription-faq.html
- Revu 21 user manual: Markups List & filters — https://support.bluebeam.com/revu/how-to/track-and-manage-markups-using-markups-list.html · https://support.bluebeam.com/revu/how-to/manage-and-review-markups-with-filter-list.html · https://support.bluebeam.com/user-manual/menus/tools/create-forms.html · https://support.bluebeam.com/user-manual/menus/tools/form-fields.html · https://support.bluebeam.com/user-manual/menus/document/ocr.html · https://support.bluebeam.com/user-manual/menus/document/compare-documents.html · https://support.bluebeam.com/user-manual/menus/document/overlay-pages.html · https://support.bluebeam.com/user-manual/menus/edit/redaction.html · https://support.bluebeam.com/user-manual/menus/edit/erase-content.html · https://support.bluebeam.com/user-manual/menus/window/create-edit-sets.html · https://support.bluebeam.com/user-manual/menus/batch/sign-seal.html · https://support.bluebeam.com/user-manual/menus/batch/slip-sheet.html · https://support.bluebeam.com/user-manual/menus/batch/link.html · https://support.bluebeam.com/user-manual/menus/tools/manage-self-signed-ids.html · https://support.bluebeam.com/user-manual/menus/window/punch-tool-set.html
- Revu 20 online help: Markups List, filters, custom columns, custom status, legend — https://support.bluebeam.com/online-help/revu20/Content/RevuHelp/Menus/Window/Panels/Markups/Markups-List--MTV.htm (+ Markups-List-Filters--MTV.htm, Custom-Columns--MT.htm, Custom-Status--T.htm, Unsorted/Markups-Legend--T.htm)
- Revu 20 EOL — https://support.bluebeam.com/revu/resources/revu-20-eol.html
- Product Comparison Guide (Revu 21 plans vs Revu 20 editions, official PDF) — https://www.symetri.dk/media/suqdelli/product-comparison-guide-bluebeam-revu-21-vs-bluebeam-revu-20-5.pdf
- Compare vs Overlay feature page — https://support.bluebeam.com/revu/features/compare-documents-vs-overlay-pages.html

Secondary (corroboration, graded in-line):
- Studio Prime EOL — https://support.bluebeam.com/studio/resources/studio-prime-eol-announcement.html · https://community.bluebeam.com/discussion/5299/about-studio-prime-end-of-life-eol
- OCR 35+ languages claim — https://graitec.com/us/tech-resources/how-to-use-ocr-in-bluebeam/
- Punch workflow guidance — https://novedge.com/blogs/design-news/bluebeam-tip-efficient-punch-list-management-in-bluebeam-revu-key-tools-and-techniques
- Sentiment: https://www.reddit.com/r/Revu/comments/xi9fw9/ · /r/Revu 13qtib6 (10%/yr escalation) · /r/Revu 1gkgfn5 (9% increase) · /r/msp xkd7d0 (price gouging) · /r/estimators 1jtsrsq · https://community.bluebeam.com/discussion/363 · https://community.spiceworks.com/t/bluebeam-switch-to-subscription-model-huge-price-increase/936575/ · https://community.spiceworks.com/t/psa-bluebeam-eol-ramifications/938143
- DigiCert signing interop — https://knowledge.digicert.com/solution/bluebeam-revu-sign-pdf-with-digicert-document-signing-certificate
