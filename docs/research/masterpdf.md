# Spec Sheet: Master PDF Editor 5 (Code Industry Ltd)

Research date: 2026-09-07. Product state: **v5.9.99 (11 Aug 2026)**, current across Windows 7/8/10/11, Linux (glibc 2.28+, Qt 5.15+/Qt 6.4.2+ as of 5.9.98), macOS 12+ Intel/Apple Silicon. Generated to **PDF 1.7 spec**; opens dynamic-XFA files for view/fill.

**License/tier model (official, EULA §1 + purchase page):**
- **Demo / Unregistered** — full feature set, but saved output carries a **watermark**. EULA §1.2 additionally permits free non-commercial use "to view documents, fill PDF forms, comment and print documents." No technical support.
- **Licensed** — USD **$79.95** (1–9 seats), volume to $59.95 (100–500). Perpetual **for the version purchased** + 12 months email support and updates; renewal years at **50%** of then-current price. 2 activations per license (any OS), deactivate/reactivate; **offline activation supported**. Standard/Concurrent/Individual license types in EULA (§2.6.1 standard *terminates registration* at term end; §2.6.3 individual *stays registered* — contradictory drafting, quirk). No refunds except unresolvable technical defects. MSI installer; Group Policy deployment documented; Linux remote repository.
- **History quirk that still drives sentiment:** v4.x (last ~4.3.89, 2019) was fully free for non-commercial use *including editing, no watermark*; v5 (2019) converted the free tier to watermarked demo. AUR still ships a `masterpdfeditor-free` v4 package for exactly this reason.
- "Cloud?" column: every feature row is **N** — Master PDF Editor has no vendor cloud service. Product-level phone-home exists in exactly two places: online license activation (offline path available) and update checks (disableable; 5.9.99 added installer params to disable). Form *submit* actions and "send by email" go to user-configured endpoints (HTTP/FTP/mail client), not to Code Industry.

**Verification approach:** every domain below was built from the official Online Manual (manual index + ~45 page fetches), the official product/FAQ/purchase/downloads pages, the official v5 changelog (5.8.06 → 5.9.99), then cross-checked against user forums (Reddit r/sysadmin, r/linuxquestions, AUR, Manjaro, Linux Uprising, AskUbuntu) and the NDSS 2021 PDF-security paper. Absence claims ("no PAdES", "no batch") are graded on full-manual + changelog + search coverage.

Verdict scale: TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE. GRADE is on the row's Notes claim.

---

## 1. Viewing & navigation

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Multi-doc tabs | Tabbed workspace, duplicate/rename tabs, drag | Demo (views free) | N | Manual "Document Navigation". GRADE: TRUE |
| Page display modes | Fit Page / Fit Width / Facing pages (two-page) + Show Cover Page toggle, custom zoom %, rotate view | Demo | N | No continuous-scroll mode named in manual (PRD-style "continuous" not documented). GRADE: TRUE (modes), UNVERIFIABLE (absence of continuous — likely exists undocumented) |
| Page alignment view | Align pages Left/Center/Right against largest page in doc (5.9.90) | Demo | N | Manual "View > Align to Largest Page". Useful for mixed-size scan bundles. GRADE: TRUE |
| Replace Document Colors | Overrides doc colors for readability; configured in Display settings | Demo | N | Accessibility-adjacent reading aid, not tagging. GRADE: TRUE |
| Navigation panel | Thumbnails (resize; thumbnail ops = page ops), Bookmarks tree, Search panel, Comments list, Signatures tab, Layers tab, Attachments | Demo | N | Thumbnails are fully operative (delete/move via thumbnail). GRADE: TRUE |
| Previous/Next View | Browser-style back/forward through view states incl. search results | Demo | N | Manual. GRADE: TRUE |
| Search | Toolbar box + side panel; case-sensitive, whole words, include comments; F3 next; saved search history (5.9.90) | Demo | N | **No regex search, no find-and-replace** in manual/changelog. GRADE: TRUE (scope) |
| "Go to page" dialog | Type page number (5.9.80) | Demo | N | Changelog 5.9.80. GRADE: TRUE |
| Bookmarks | Create/edit/delete; bookmark actions can open files, links, reset/show-hide forms, submit, run JavaScript; hotkey fix in 5.9.99 | Demo | N | Manual "Document Navigation". GRADE: TRUE |
| Sessions | File > Sessions: save/open named sessions of open docs (5.9.50); session-open bug fixed 5.9.94 | Demo | N | Changelog + manual. GRADE: TRUE |
| Snapshot tool | Region copy to clipboard; works on XFA docs too (5.8.06) | Demo | N | Manual + changelog. GRADE: TRUE |
| Attachments panel | View/rename/add/remove/save multiple attachments (5.9.50+) | Demo | N | Manual "edit-attachment". GRADE: TRUE |
| Dark theme / UI | Themes; macOS dynamic theme switching (5.9.70); RTL interface (crash fixed 5.9.85) | Demo | N | Community calls UI "outdated" (Wondershare roundup 2026) — cosmetic debt is real. GRADE: MOSTLY_TRUE |
| Open XFA documents | Open/view/fill dynamic XFA ("Please wait..." docs render); static XFA fully editable (5.8.30+); open warning banner (5.9.85); XFA render font fixes (5.9.06, 5.9.90) | Demo | N | The known community answer for XFA files that render as "Please wait" in browsers. GRADE: TRUE |

**GlyphPDF delta (viewing):** Master PDF has tabs, view back/forward, sessions, document-color override, mixed-size page alignment view, and the XFA-open rescue — GlyphPDF has none of those except dark mode and thumbnails. Reverse delta: GlyphPDF has presentation mode + continuous scroll documented and ledger-verified overlay behavior in two-page mode; Master PDF's manual documents no presentation mode. Both fully offline.

## 2. Editing (text / object / page) — their headline strength

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Type-filtered edit modes | Edit Document (Alt+1, anything), Edit Text (Alt+2), Edit Forms (Alt+3), Edit Images, Edit Vector Images; Select Text (Alt+7); Hand | Demo (saves watermark) | N | Type filtering prevents misclicks when docs are dense. GRADE: TRUE |
| Edit existing text objects | Double-click text object, retype; font/size/color/char+word spacing/line height in Object Inspector; "text merging into blocks" improved 5.9.40; "Edit Text Elements as Blocks" toggle | Demo | N | Object-level text surgery is the r/sysadmin "what a revelation" workflow. GRADE: TRUE |
| Insert simple text | New text object anywhere (Ctrl+T) | Demo | N | Manual. GRADE: TRUE |
| Insert formatted text | Rich-text object: per-run font/color/alignment (Format section); object size follows content (no direct resize) | Demo | N | Manual "Editing Formatted Text". GRADE: TRUE |
| Known text limits | "Possible Issues with Text" page documents encoding/reflow hazards; formatted-text *form fields* explicitly unsupported ("Current version doesn't support formatted text") | — | N | Honest in-house acknowledgment of the classic PDF-reflow problem, no reflow engine. GRADE: TRUE |
| Object geometry | Exact Left/Top/Width/Height (±10,000 pt / in / mm); **math expressions in coordinate fields** — `(189.55+34.00)*2 pt` (5.9.89); rotation angle field | Demo | N | Power-user detail almost no competitor has. GRADE: TRUE |
| Transformation matrix | Direct Matrix-coefficient editing per object | Demo | N | Manual "Transfomation Matrix". Deep, esoteric, loved. GRADE: TRUE |
| Rotation handle + Z-order | On-canvas rotation handle (5.9.89); Z-reorder multiple objects at once (5.9.90) | Demo | N | Changelog. GRADE: TRUE |
| Clipping paths | View/edit/remove clipping path on objects (removal documented; not resized with object) | Demo | N | Manual "Clipping Path". Rare capability. GRADE: TRUE |
| Vector image objects | Select/edit vector objects as objects (Edit Vector Images tool); vector display fixes (5.9.96) | Demo | N | Object-granularity vector manipulation, not node/path editing like PDF-XChange 10. GRADE: TRUE (as described); PARTLY_TRUE if read as "vector path editor" — it is not |
| Images | Insert, move, resize, replace, save image to file, copy to clipboard; scanner-texture effect (5.9.84) | Demo | N | Manual "Editing Images". No in-app image editor beyond geometry. GRADE: TRUE |
| Grid + snap | Grid display (Ctrl+U), snap (Ctrl+Shift+U), configurable grid settings | Demo | N | Manual. GRADE: TRUE |
| Multi-select & align | Ctrl-click, rubber band, Ctrl+A; align left/right/top/bottom/center via inspector or Edit menu; multiple field alignment | Demo | N | Manual. GRADE: TRUE |
| Paste to multiple pages | Paste object to chosen page range in one action (Ctrl+Shift+V) | Demo | N | Stamp-a-header-across-50-pages workflow. GRADE: TRUE |
| Container content editing | "Edit container content" (Form XObject inner content) per overview | Demo | N | Overview lists it; no dedicated manual page — depth unclear. GRADE: PARTLY_TRUE |
| Page ops | Insert blank / from PDF / from images / from scanner; delete; move; **replace pages** (5.9.80); rotate; crop + page-layout editing; resize (enlarge/reduce); page properties; extract | Demo | N | Manual "Working with PDF Pages". GRADE: TRUE |
| Extract / split | Extract by range to single file / per-range files / per-page files; export bookmarks with pages; delete-after-extract split mode (explicitly non-undoable); split by page ranges (5.9.87), by page count or file size (5.9.98) | Demo | N | Manual + changelog. GRADE: TRUE |
| Merge / insert | Insert Pages: multi-file + **Add Folder**, reorder list, per-file page ranges, insert position (before/after current/first/last), import bookmarks, per-doc bookmark creation, sort on combine (5.9.70) | Demo | N | Manual "Split and Merge". GRADE: TRUE |
| Layers (OCG) | Create/nest/sort/rename layers, set object→layer (incl. default layer for new objects), show/hide/reset visibility; merge all layers during optimize; new layer management (5.9.86) | Demo | N | Manual "add-and-edit-layers". GlyphPDF has no layer tooling. GRADE: TRUE |
| Undo/redo | Documented across edits; changelog: "improvements and fixes for undoing redaction" (5.9.99) | Demo | N | Depth of undo stack not documented; Reset Forms explicitly NOT undoable. GRADE: MOSTLY_TRUE |
| Autosave & backup | Autosave interval 1–120 min (default 5, off by default) saves the live file; optional one-time .backup per save to chosen folder | Demo | N | Autosave writes the document itself (not a recovery snapshot) — trust model weaker than GlyphPDF's safe-save. GRADE: TRUE |
| Sessions save | (see Viewing) also serves edit workflows | Demo | N | — |

**GlyphPDF delta (editing):** They are 2–3 years ahead of GlyphPDF here — this is the archetype GlyphPDF's PRD §9.2 gestures at but the ledger doesn't yet evidence: type-filtered selection, math-expression geometry, transformation matrix, clipping-path removal, Z-reorder, paste-to-multiple-pages, OCG layers, vector-object selection. Reverse delta: GlyphPDF's ledger has safe-save transactional guarantees (SHA-256 source invariance, candidate/validate/commit), redaction excision with byte-identity tests, and E-1-style corruption detection — Master PDF's autosave-writes-the-file model and "save redacted file under a different name or the data will be lost" warning (manual) show no transactional layer. Master PDF also has no fingerprint-grade compare (below) and no documented content-stream-level integrity testing.

## 3. OCR

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| OCR engine | Tesseract; 4.1 (5.8.18, Nov 2021) → Tesseract 5 (5.9.06, Oct 2022); multithreaded since 5.9.50 | Demo | N | Fully local; .traineddata files loaded from configurable directory. GRADE: TRUE |
| Languages | Bundled install dialog + **any custom Tesseract .traineddata** dropped into the data dir; multi-language select (advise minimal set) | Demo | N | Manual "ocr-pdf". GRADE: TRUE |
| Output modes | **Searchable Text** (invisible layer under image) or **Editable Text** (real text in front, image covered with background color) — i.e., scan→editable-document conversion, not just search layer | Demo | N | Manual. The Editable mode is a real differentiator vs search-layer-only OCR. GRADE: TRUE |
| Recognized-text font | Choose font family for recognized text (default Helvetica) to match source | Demo | N | Manual. GRADE: TRUE |
| Page range | Current/all/range in OCR dialog | Demo | N | Manual + 5.9.40 changelog. GRADE: TRUE |
| Deskew | Auto-straighten during OCR; separate deskew in Optimize Scanned Pages; fixes through 5.9.99 | Demo | N | Manual. No orientation (90/180/270) detection documented. GRADE: TRUE (deskew), MOSTLY_TRUE (orientation absence) |
| Confidence threshold | "Minimal confidence level" numeric; below threshold → **force manual text editing** dialog showing original image fragment + recognized text per fragment, with Yes / Yes to All / Not Text / Cancel | Demo | N | Manual. Fragment-level review loop exists but is a modal drill, not GlyphPDF's verify screen; no dual-engine fusion. GRADE: TRUE |
| Automatic recognition on navigation | Settings > Text Recognition: auto-OCR pages that contain only images/vectors as you view/edit (5.9.89), single preselected language | Demo | N | Changelog + manual. Lovely convenience feature GlyphPDF lacks. GRADE: TRUE |
| Background fill handling | Detects background fill color for OCR on color and B/W docs (5.9.20, 5.9.06) | Demo | N | Changelog. GRADE: TRUE (claim), UNVERIFIABLE (quality) |
| Scan to PDF | Scanner integration; device list persistence (5.9.99), faster scanner reselect Linux (5.9.96), page-size fixes; blank-page detection during scan (5.9.50) | Demo | N | Manual + changelog. GRADE: TRUE |
| Convert to Scanned Pages | Applies scanned look ("scanner texture effect", 5.9.84) | Demo | N | Manual. Novel anti-forensic-ish utility; GlyphPDF has nothing similar. GRADE: TRUE |
| Optimize Scanned Pages | Adaptive compression: JPEG/ZIP for color+gray; ZIP/CCITT G4/JBIG2 for B/W; quality slider; deskew + **remove background** filters | Demo | N | Manual. MRC-style pipeline is implicit (JBIG2+G4+downsample) but never named MRC; no size estimate documented. GRADE: TRUE (features), PARTLY_TRUE (any "MRC" labeling) |
| No batch OCR | No multi-file OCR anywhere in manual/changelog (only single-doc page ranges) | — | N | Absence across all official sources. GRADE: MOSTLY_TRUE (absence) |

**GlyphPDF delta (OCR):** GlyphPDF's ROVER ensemble (Tesseract 5 + RapidOCR + PP-DocLayout fusion), orientation detection (0/90/180/270), 1-bit binarization pipeline, review lifecycle, word-level verified overlay geometry, whole-document vs page scoping, batch OCR + hot folder are all absent from Master PDF — GlyphPDF is clearly ahead on accuracy infrastructure and process honesty. Reverse delta: Master PDF has the navigation-triggered auto-OCR convenience, the "Editable Text" scan-to-document mode, recognized-text font control, and scan acquisition itself (scanner support), which the ledger doesn't list for GlyphPDF (its OCR input path is files).

## 4. Forms (AcroForm + XFA, JavaScript) — their other headline strength

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Field creation | Link, Push Button, Check Box, Radio Button, Combo Box, List Box, Text Field, Signature field — drawn or click-placed; multi-select for aligned batch editing in Object Inspector | Demo (watermark on save) | N | Manual "Create and Edit Interactive PDF Forms". No barcode/date-only/custom calculated-field *types* (calculation is JS-driven). GRADE: TRUE |
| Field properties | Name, tooltip, orientation; visible/hidden/visible-not-printable/printable-not-visible; read-only; required (except list/button); **Locked** flag; auto-size font; border styles (solid/dashed/beveled/inset/underline) | Demo | N | Manual "PDF Form Properties". GRADE: TRUE |
| Text field options | Alignment, default value, multiline, password, scrollable, spell-check, character limit, **split into N combed cells**, new date formats (5.9.84); formatted-text-in-field unsupported (documented) | Demo | N | Manual. Combed fields are a real AcroForm depth marker. GRADE: TRUE |
| Choice fields | Sort items, multiple selection, commit-value-immediately, custom export values, user-custom combo text | Demo | N | Manual. GRADE: TRUE |
| Buttons | Behavior (invert/push/outline/none), Up/Down/Rollover labels, images on buttons (overview) | Demo | N | Manual + overview. GRADE: TRUE (labels), MOSTLY_TRUE (button images — overview bullet only) |
| Checkbox/radio semantics | Same name + same export value = linked checkboxes; same name + different export values = radio behavior; documented explicitly | Demo | N | Rare, correct AcroForm education in docs. GRADE: TRUE |
| Tab order editor | Forms > Edit Tab Order | Demo | N | Manual. GlyphPDF has tab order (PRD §9.6) — parity. GRADE: TRUE |
| Field recognition | "Recognize form fields" (overview); Forms menu auto-detect | Demo | N | Overview bullet; no manual page detailing the heuristic quality. GRADE: PARTLY_TRUE |
| JavaScript — field actions | Triggers: Mouse Down/Up/Enter/Exit, On Receive/Lose Focus; multiple actions per trigger; **Run a JavaScript** action | Demo | N | Manual "Actions tab". GRADE: TRUE |
| JavaScript — automation | "Automatic calculation and data validation with JavaScript is supported" (product page); dedicated **JavaScript settings tab**; new high-perf JS engine (5.8.46); "Extended JavaScript API support" (5.8.18, 5.9.50) | Demo | N | GRADE TRUE that JS calc/validation is claimed; UNVERIFIABLE on exact API surface — Code Industry publishes **no JS API reference** (no object/method list), so portability of Acrobat-authored scripts is untested territory |
| JavaScript — document/page events | JS on document open, before/after save, before close, before/after print; per-page before/after view (overview) | Demo | N | Overview "Edit Properties" bullets — document-level scripting most editors omit. GRADE: MOSTLY_TRUE (overview-level only, no manual page) |
| Submit / distribute | Submit-form action: FDF format to local file, FTP, or HTTP(S); email submission explicitly unsupported; "send files by FTP/HTTP(s)/email with JavaScript" via scripting | Demo | N | Manual. FDF-only submit is dated (no XFDF/HTML submit documented). GRADE: TRUE |
| Form data import/export | Export/import form data as **FDF**; 5.9.96 "wider range of file formats" for comments+forms exchange (formats unnamed) | Demo | N | Manual says FDF; changelog 5.9.96 implies more — unspecified. GRADE: TRUE (FDF), UNVERIFIABLE (added formats) |
| Static XFA editing | **"Full support for editing static XFA"** (5.8.30); option to disable automatic conversion of static XFA → standard PDF (5.9.40); compatibility improvements 5.9.70/5.9.90 | Demo | N | Changelog. The only mainstream small editor that edits static XFA. GRADE: TRUE |
| Dynamic XFA | Open, view, fill, print; snapshot (5.8.06); warning banner on open (5.9.85) — **not** editable as XFA | Demo | N | Manual "opening-and-saving": dynamic XFA = open and view. GRADE: TRUE |
| Fill & reset | Hand-tool filling, required-field highlighting, highlight color config, Reset Forms (not undoable), save filled doc | Demo (fill+save = the free EULA use) | N | Manual "Fill PDF Form Fields". GRADE: TRUE |

**GlyphPDF delta (forms):** The gap cuts hardest here. Master PDF has what GlyphPDF's PRD §9.6 explicitly lists as gaps: JavaScript calculation/validation (GlyphPDF's "calculated field" is in-house, not scriptable), XFA (GlyphPDF: none — not even fill), FDF round-trip (GlyphPDF has CSV/FDF but no XML), combed fields, locked fields, commit-immediately choice fields, and document/page-level script events. Reverse delta: GlyphPDF's ledger brings what Master PDF lacks — form-write transactional safety with byte-identical failure guarantee (F01), field-level undo incl. empty-state snapshots (F09/R02), compound undo of auto-detected placements (V06), and honest capability disclosure (U08). Master PDF documents no form-write safety story at all, and Reset Forms is documented as non-undoable.

## 5. Comments & markup

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Sticky notes | With author header; **others may answer notes** (replies) | Demo | N | Manual "Comments Menu". GRADE: TRUE |
| Typewriter / Callout / Label | Typewriter text at cursor; callout with arrow (5.9.80); graphic labels (check mark, circle, right arrow) | Demo | N | Manual. GRADE: TRUE |
| Text markup | Highlight, Strikeout, Underline. **No squiggly** documented | Demo | N | Manual. Absence noted. GRADE: TRUE (tools), MOSTLY_TRUE (squiggly absence) |
| Drawing tools | Arrow, Line, Rectangle, Ellipse, Pencil, Brush, Polygon, **Cloud** (5.9.40) | Demo | N | Manual. No eraser tool documented. GRADE: TRUE |
| Stamps | Create/use custom stamps ("seal impression" placement) | Demo | N | Manual + product page. GRADE: TRUE |
| Measurement tools | Distance, Area, Perimeter; feet unit + scaling-factor accuracy (5.9.61); live distance readout (5.9.50) | Demo | N | Manual. **GlyphPDF has no measurement tools anywhere in PRD/ledger** — genuine gap. GRADE: TRUE |
| Comments list | Side panel; **filter by type, reviewer, or status**; show/hide all; delete all; comments hidden by default when printing | Demo | N | Manual. Status filtering exists but no documented set-status/resolved workflow. GRADE: MOSTLY_TRUE |
| Print/show control | Per 5.9.98: hide or display comments both on screen and in print | Demo | N | Changelog. GRADE: TRUE |
| Comment data import/export | FDF export/import of comments alone (send comments without the PDF); wider formats 5.9.96 (unspecified) | Demo | N | Manual "Comments Menu". GlyphPDF exports CSV of comments; FDF is the PDF-native interchange. GRADE: TRUE (FDF) |
| Object Inspector for comments | Full geometry/style/matrix editing of comment objects incl. color/opacity fixes (5.9.50) | Demo | N | Manual/changelog. Comments are first-class objects — deeper than GlyphPDF's annotation model. GRADE: TRUE |
| File attachment as comment | Yes | Demo | N | Manual. GRADE: TRUE |
| No review summaries | No status workflow (open/resolved/rejected), no review-summary report in manual | — | N | Absence. GlyphPDF v1.4 plans the same; parity gap for both. GRADE: MOSTLY_TRUE (absence) |

**GlyphPDF delta (comments):** Master PDF is ahead on measurement tools, FDF comment interchange, object-inspector geometry for comments, and screen/print visibility toggles. GlyphPDF is ahead on squiggly markup, ledger-verified CSV export with RFC-4180 pinning (U07), numeric page-sort fix, and the planned threads/status/review-summary UI (v1.4). Both lack a resolved-status loop today.

## 6. Redaction

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Mark for Redaction | Crosshair manual area marking, multiple areas | Demo (watermark) | N | Manual "Redacting PDF Documents". GRADE: TRUE |
| Search and Redact | Keyword search → checkable result list → "Mark Checked Results for Redaction"; per-result document preview | Demo | N | Manual. Keyword-only — **no regex/pattern redaction (email/phone/SSN), no word-list import**. GRADE: TRUE |
| Apply Redactions | Permanently removes data; colored boxes remain; **object borders visible during redact for coverage checking** (5.9.98); works over forms (fix 5.9.98) | Demo | N | Manual + changelog. GRADE: TRUE |
| Overlay text | Per-mark text with font/size/color/alignment; auto-size to area; **repeat overlay text** to fill the box | Demo | N | Manual. GlyphPDF added overlay labels only in §9.8-b/9.12-a — parity now, but Master PDF's repeat-text option is richer. GRADE: TRUE |
| Redaction properties | Default fill color (Settings > Redaction); per-area style: fill/stroke color+opacity, line width, geometry, transformation matrix, clipping path | Demo | N | Manual. Unusual depth — per-mark matrix editing. GRADE: TRUE |
| Undo redaction | "Improvements and fixes for undoing redaction" (5.9.99) implies marks/apply are undoable within session | Demo | N | Changelog. Mechanism undocumented. GRADE: PARTLY_TRUE |
| Excision guarantee | Manual warns: redaction permanently removes data — **save under a different file name or the redacted data will be lost from the original**; no sanitize bundle (metadata/attachment scrub) documented | — | N | The warning implies in-place application risk; no verified-removal report, no log. GlyphPDF's 7-stage RedactOperation + SHA-256 source-invariance + default-ON sanitize is materially safer. GRADE: TRUE (warning), MOSTLY_TRUE (sanitize absence) |
| No batch redaction | Absent from manual/changelog | — | N | Absence. GlyphPDF has batch redact (PRD §9.8). GRADE: MOSTLY_TRUE (absence) |

**GlyphPDF delta (redaction):** GlyphPDF ahead: regex/pattern + word-list redaction, presets (Email/Phone/SSN), page-list mark-all, sanitize bundle, transactional apply with failure recovery and cancel/disarm semantics (N07), byte-exact excision tested to the hex level (E-1). Master PDF ahead on: per-mark visual styling depth (matrix/clipping), repeat-overlay-text, and during-redaction object-border visibility for coverage auditing. Neither has a redaction log; GlyphPDF's PRD asks for one too.

## 7. Security & encryption (incl. certificates)

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Password encryption | Open password + permission password; permission checkboxes for print/edit/etc.; honest manual note that permission passwords don't encrypt data | Demo | N | Manual. GRADE: TRUE |
| Cipher choices | **128-bit RC4 or 256-bit AES** (both password and certificate encryption) | Demo | N | Manual. No AES-128; RC4 still offered — a compliance liability GlyphPDF (AES-256 only) doesn't carry. GRADE: TRUE |
| Certificate encryption | Recipient-list encryption: add recipient certs (public-key) from Certificate Manager; permissions; own-cert warning | Demo | N | Manual "Securing PDF with a Certificate". **GlyphPDF has no certificate-based document encryption** (PRD §9.11: passwords/watermarks/sanitize/expiry only) — real gap. GRADE: TRUE |
| Certificate Manager | Import/manage certificates incl. self-signed creation guidance (OpenSSL); macOS 15 Certificate Manager breakage fixed late (5.9.94); PKCS#11 provider path for tokens (Linux) | Demo | N | Manual + changelog. GRADE: TRUE |
| Watermarks | Text or image (incl. PDF-page-as-watermark), rotation/opacity/scale, position offsets, page ranges, preview, saved templates, multiple watermarks per page, **edit existing watermarks** (5.9.94), remove all | Demo | N | Manual. GlyphPDF has watermarks (PRD §9.11) but no documented template manager/edit-in-place. GRADE: TRUE |
| Backgrounds | Same engine as watermarks; add/edit (5.9.94) | Demo | N | Manual. GlyphPDF: backgrounds not in ledger. GRADE: TRUE |
| Grayscale / B&W conversion | Convert whole document to grayscale (5.9.84); grayscale-with-alpha; B&W — in optimize and print | Demo | N | Manual + changelog. Print-production leaning; GlyphPDF lacks it. GRADE: TRUE |
| Metadata | Document Properties editing (author/title/etc.); "Remove unused elements" in optimize touches metadata indirectly; **no dedicated sanitize/scrub bundle** | Demo | N | Manual. GlyphPDF's default-ON sanitize is ahead. GRADE: MOSTLY_TRUE |
| No secure-sharing/expiry | No XMP expiry, no encrypted-ZIP package (GlyphPDF has both) | — | N | Absence. GRADE: MOSTLY_TRUE (absence) |

**GlyphPDF delta (security):** Master PDF ahead: certificate (public-key) document encryption, PKCS#11 token plumbing, RC4-128 legacy-compat ciphers, watermark/background template management, document grayscale conversion. GlyphPDF ahead: AES-256-only posture (no weak cipher), sanitize bundle with default-ON honesty (D07 fix), XMP document expiry, encrypted-ZIP secure package.

## 8. Signatures

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Certificate signing | Create signature field (Forms > Signature), sign with cert from system store (Win/mac) or Certificate Manager (Linux); USB tokens via PKCS#11 (Linux driver path setting); cryptographic USB token support Linux (5.9.70); token+password signing fixes (5.9.97) | Demo (watermark) | N | Manual "Signing PDF with a Certificate". Requires user-supplied certificate — no vendor signing service (offline). GRADE: TRUE |
| Signature appearance | Show/hide text components (Name, E-mail, Date/Time, Signed By), date format, custom formatted text, border (round, color), image w/ stretch; signature preview; **saved appearance settings**; reason (predefined or custom) + location | Demo | N | Manual. Rich appearance editor — roughly GlyphPDF §9.7 parity plus saved-settings reuse (GlyphPDF: session cache §9.7-b). GRADE: TRUE |
| Lock after signing | Prohibit form/content changes after signing | Demo | N | Manual. GRADE: TRUE |
| Invisible signature | Forms > Invisible Signature; managed in Signatures tab; deletable | Demo | N | Manual + 5.9.84. GlyphPDF ledger has no invisible-signature row. GRADE: TRUE |
| Batch signing | **Sign multiple documents at once** (5.9.96) | Demo | N | Changelog. GlyphPDF: no batch signing. GRADE: TRUE |
| Validation | Signature Properties: VALID / INVALID / UNKNOWN with trust explanation; make-cert-trusted instructions per OS; **Strong verification mode** (new sig invalidates previous ones even untouched); verification improvements (5.9.98) | Demo | N | Manual. No OCSP/CRL/LTV language anywhere in manual. GRADE: TRUE (validation), MOSTLY_TRUE (no OCSP/LTV — absence across manual+search) |
| Signed-version viewing | Signatures tab > "Click to view this version" renders the byte range as actually signed — anti-substitution check | Demo | N | Manual. Sophisticated touch; not in GlyphPDF ledger. GRADE: TRUE |
| Initials | Inserting Initials (typed/drawn/image styles), default initials (5.9.94), transparent-background image initials (5.9.99); placement fixes (5.9.86/5.9.99) | Demo | N | Manual + changelog. Roughly GlyphPDF §9.7-a parity. GRADE: TRUE |
| No PAdES/TSA | **No mention of PAdES, B-LT/B-LTA, timestamp authorities, or LTV** anywhere in the official manual, changelog, or product pages | — | N | Absence across all official sources. GlyphPDF's PAdES B-LT/B-LTA + DSS degradation surfacing (§9.7-c) is a compliance tier above. GRADE: MOSTLY_TRUE (absence — strong coverage) |
| Signing safety | Manual: "Signing should be done on the final version... if modified after signing, signature becomes invalid"; plain Save of signed doc invalidates signature (opening-and-saving note); delete-page-then-sign bug fixed 5.9.97; sig-invalidated-on-save bug fixed 5.9.89 | — | N | No incremental-update preservation story comparable to GlyphPDF's safe-save. GRADE: TRUE |

**GlyphPDF delta (signatures):** GlyphPDF ahead: PAdES B-LT/B-LTA, trust-chain/OCSP validation (per PRD §27), visible-signature ETSI layout with auto-fit (§9.7), SignOutcome degradation + retry (§9.7-c), signature anchoring to real field rects. Master PDF ahead: PKCS#11/USB-token signing, batch multi-document signing, invisible signatures, strong-verification strict mode, signed-version anti-substitution viewing, saved appearance presets.

## 9. Compare

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Document comparison | **Added 5.9.94 (Sep 2025)** — the newest major feature; View > Compare or Document > Compare; old/new file pickers (open docs or browse), swap | Demo | N | Changelog + manual. Feature is <1 year old. GRADE: TRUE |
| Text-based diff | "Comparison is performed based on the textual content" — additions green, deletions red, modifications blue | Demo | N | Manual. No structural/page-event diff (reorder/insert/delete detection as page events) documented. GRADE: TRUE |
| Page ranges | Independent range per document | Demo | N | Manual. GRADE: TRUE |
| Shift tolerance | "Compare with Shift Tolerance" ignores whole-block moves on a page to focus on content changes | Demo | N | Manual. A crude alignment knob; no fingerprint alignment claim. GRADE: TRUE |
| Output format | Generates a **new PDF** ("The Result of Comparison.pdf"): old left / new right side-by-side, changes highlighted + per-change comment annotations; comments listed in nav panel; savable | Demo | N | Manual. No HTML/text report export. GRADE: TRUE |
| No page-reorder detection | No structural page-change tree, no alignment reporting in manual | — | N | Absence. GlyphPDF's DiffEngine PageChange model + fingerprint LCS alignment + change filters + HTML/text reports (ledger F06, CMP-align, V04) is a generation ahead. GRADE: MOSTLY_TRUE (absence) |

**GlyphPDF delta (compare):** GlyphPDF leads decisively — structural added/removed/moved page detection with fingerprint alignment, near-twin disambiguation, filter-aware anchors driving both views, linked scrolling, and report export. Master PDF leads only on the dead-simple side-by-side *visual artifact* (a shareable PDF a human can attach to an email) and the shift-tolerance knob.

## 10. Batch & automation

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Headers/footers/Bates to multiple files | Document > Header and Footer > Add to Multiple Files: Add Files/Directory/Open Files, reorder, sequential Bates across files, save-and-close option | Demo | N | Manual "headers-footers-pdf" + "bates-numbering". The one real multi-file pipeline. GRADE: TRUE |
| Batch signing | Sign multiple documents at once (5.9.96) | Demo | N | Changelog. GRADE: TRUE |
| Merge from folder | Insert Pages > Add Folder (all PDFs/images in a directory) with per-file ranges | Demo | N | Manual "split-merge". GRADE: TRUE |
| Group Policy deployment | MSI + GP installation walkthrough; installer CLI params for language/dir/update-checks (5.9.99); Linux remote repository | Demo | N | Manual + changelog. IT-friendly, no license server documented. GRADE: TRUE |
| No general batch | No batch convert / batch OCR / batch watermark / batch redact / batch compress / hot folder anywhere in manual or changelog | — | N | Absence across all official sources. GlyphPDF's batch + hot-folder (PRD §9.12, ledger U08 pre-flight) is far ahead. GRADE: MOSTLY_TRUE (absence) |
| Virtual PDF printer (automation-adjacent) | Print-to-PDF from any app into MPE (Windows only); output-folder selection (5.9.80); wrong-format bug fixed 5.9.94 | Demo | N | Manual. GlyphPDF has no printer driver. GRADE: TRUE |

**GlyphPDF delta (batch):** GlyphPDF is far ahead (convert/OCR/compress/watermark/redact/merge batch + hot folder + capability pre-flight). Master PDF's counterweights: folder-level merge input, multi-file Bates continuity across documents, batch signing, and MSI/GP enterprise deployment polish that GlyphPDF's ledger doesn't evidence.

## 11. Print production

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Print dialog | Page ranges incl. current view; copies+collate; grayscale; preview with page/spread navigation | Demo | N | Manual. GRADE: TRUE |
| Duplex | Auto, long-edge, short-edge, **manual duplex with refeed prompt** (5.9.84) | Demo | N | Manual. GRADE: TRUE |
| Booklet printing | Booklet mode (5.9.89) | Demo | N | Changelog. GlyphPDF: nothing in ledger/PRD. GRADE: TRUE |
| Paper handling | Choose paper source by paper size (5.9.96); mixed-size page printing enhancements (5.9.99); reverse page order (5.9.82); fractional scale values (5.9.90); print presets (5.9.96) | Demo | N | Changelog. GRADE: TRUE |
| Comments/forms print control | Print document only / with annotations options; comments show/hide in print (5.9.98) | Demo | N | Manual. GRADE: TRUE |
| CUPS/macOS print fixes | CUPS 2.4.14+ orientation fix (5.9.99) | Demo | N | Changelog. GRADE: TRUE |
| Bates/page numbers/date | Via headers-footers engine (see above) | Demo | N | Manual. GlyphPDF parity (§9.9). GRADE: TRUE |

**GlyphPDF delta (print production):** Master PDF ahead across the board: virtual PDF printer driver, booklet, manual duplex, per-size paper source, print presets, grayscale output. GlyphPDF's ledger evidences no print pipeline at all beyond the OS default. If legal/office users are a target persona (PRD Persona 3/4), Bates-continuity and booklet are cheap wins; the virtual printer is a big lift not worth it.

## 12. Accessibility / tagging

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Tagged-PDF authoring/checking | **None.** No tag tree editor, no reading-order check, no alt-text tool, no screen-reader support statements anywhere in manual | — | N | Absence across all official sources. GRADE: MOSTLY_TRUE (absence) |
| PDF/A "a-level" conversion | Offers PDF/A-1a/2a/3a conversion claiming structure preservation | Demo | N | Manual claims "preserves semantic structure/accessibility" — no validation report, no named verifier. Treat structure claims skeptically. GRADE: PARTLY_TRUE |
| Reading aids | Replace Document Colors; UI language selection; RTL UI (buggy — crash fixed 5.9.85) | Demo | N | Manual/changelog. GRADE: TRUE |

**GlyphPDF delta (accessibility):** GlyphPDF's tagged-PDF reading-order check (§9.14, async, tolerance-pinned) has no Master PDF counterpart; that's a clean differentiator. Master PDF counter-leads only via PDF/A a-level *conversion breadth* (1a/2a/3a vs GlyphPDF's 1B/2B/2U/3B/3U — GlyphPDF deliberately ships no a-levels; N03 showed version-correctness discipline GlyphPDF should advertise).

## 13. Import / export / conversion

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| PDF open/save | PDF 1.7 generation; opens other-editor files, encrypted docs, dynamic XFA view; recent-files Start page; **extended-length paths Windows** (5.9.94) | Demo | N | Manual. GRADE: TRUE |
| PDF/A conversion | **All 8 levels: 1a/1b/2a/2b/2u/3a/3b/3u** via File > Export > Convert to PDF/A; fixes for 2 (5.9.94), version/size side effects (5.9.89), enhancements 5.9.96/5.9.98 | Demo | N | Manual. Breadth exceeds GlyphPDF (1B/2B/2U/3B/3U); no conformance-report artifact documented. GRADE: TRUE (levels), PARTLY_TRUE (quality/validation depth) |
| Export images | JPEG/TIFF/BMP/PNG; page range; TIFF compression choice (5 methods) | Demo | N | Manual. GRADE: TRUE |
| Export text | TXT, all pages or per-page files, page range; bug fixes 5.9.50/5.9.70/5.9.87 | Demo | N | Manual. GRADE: TRUE |
| Export CSV | Tables → CSV (5.9.40) | Demo | N | Manual/changelog. GRADE: TRUE |
| Export Excel | XLSX: worksheet per table / per page / single; export non-table content option; table structure clustering ("similar structure same sheet"); export-all-content (5.9.84); improvements 5.9.87 | Demo | N | Manual. Column-model details undocumented — likely row-run heuristics like GlyphPDF's pre-V03 state. GRADE: TRUE (options), UNVERIFIABLE (table-detection quality) |
| Export Word | DOCX: page range, "fill vertical space with empty lines" layout heuristic, auto-open | Demo | N | Manual. Simple layout heuristic, no layout-preservation claims. GRADE: TRUE |
| Import | Existing PDFs, images, scanner; no Office→PDF import (use the virtual printer on Windows) | Demo | N | Manual. GlyphPDF imports Office natively — ahead. GRADE: TRUE |
| Optimization on save | Save Optimized As: remove unused elements, flatten forms, merge layers, **font embedding/substitution**, grayscale/alpha-gray/B&W, DPI downsampling, ZIP/JPEG (quality slider) / CCITT G4 / JBIG2; honest "may damage images" warning; no before/after size estimate documented | Demo | N | Manual. Font-substitution-on-optimize is a real production feature GlyphPDF lacks; GlyphPDF's measured size readout (§9.13-a) is ahead of MPE's nothing. GRADE: TRUE |
| Bookmarks/links/comments preservation | Export/import pages with bookmarks; bookmark export on extract; comment/form FDF round-trip; **no explicit preservation contract** (GlyphPDF pins byte-level preservation contracts in tests §9.16-a) | Demo | N | Manual. GRADE: MOSTLY_TRUE |

**GlyphPDF delta (import/export):** Roughly balanced. Master PDF ahead: PDF/A a-level breadth (1a/2a/3a), font embedding/substitution during optimize, TIFF compression menus, per-table Excel worksheet modes, FDF comment exchange. GlyphPDF ahead: Office→PDF import (in-house OOXML writer, V03 column clustering), PPT target, HTML/Markdown-class targets (HTML done; Markdown/EPUB planned), measured compression readout, export-path honesty gating (F08), local-processing disclosure badges (§9.16).

## 14. Cloud / services (architectural comparison)

| Feature | Sub-capabilities | License | Cloud? | Notes |
|---|---|---|---|---|
| Vendor cloud services | **None.** No account, no storage, no e-sign service, no AI API | — | N | Whole-catalog review: no service endpoints except activation + update check. GRADE: TRUE |
| License activation phone-home | Online activation (2 devices, any OS); **offline activation path documented**; deactivation flow; email-based code reset | Licensed | Y (activation only) | Manual "on-line/off-line activation". Offline activation makes it air-gap-deployable — closest licensing model to GlyphPDF's local-only stance. GRADE: TRUE |
| Update checker | Settings > Update; check frequency throttled to quarterly (5.9.50); disable via installer param (5.9.99); Network settings tab (proxy) | Demo | Y (update check) | Changelog/settings. GRADE: TRUE |
| Document data egress | Only user-initiated: Submit-form (HTTP/FTP/file), send-via-email GUI, FTP/HTTP via JS | Demo | Per user action | Never to Code Industry. GRADE: TRUE |
| Vendor identity | Code Industry Ltd; English+Russian site (code-industry.ru exists); payments via resellers; email-only support (English); EULA §11.1 reserves the right to stop development/support at any time | — | N | Site/FAQ verified; company domicile not officially published — community discusses Russian origin (Privacy Guides) — UNVERIFIABLE as a claim, relevant to procurement. GRADE: TRUE (structure), UNVERIFIABLE (domicile) |

**GlyphPDF delta (cloud/services):** Architecturally the closest cousin in the entire market — both local-only, offline-capable, no accounts. GlyphPDF can claim a stricter posture (zero phone-home by design vs MPE's activation/update pings, even if an offline path exists). Master PDF's one cloud-adjacent advantage is none; its offline activation is a pattern GlyphPDF's licensing could copy.

---

## Failure modes (small-vendor risk inventory)

1. **Free-tier erosion created lasting distrust.** v4 (≤4.3.89, 2019) was free for non-commercial use including editing with no watermark; v5 made every unregistered save watermarked. Community maintains a v4 fork package (AUR `masterpdfeditor-free`) 7 years later; "watermarking of PDFs upon any edits truly renders the program useless" (r/linuxmasterrace). Verdict: TRUE (multi-source: Linux Uprising 2019, AUR, AskUbuntu, Manjaro).
2. **Release-quality whiplash on a thin team.** Changelog shows critical regressions shipped then hotfixed: 5.9.94 → 5.9.95 "critical issue on some Windows systems" (8 days); 5.9.81 → 5.9.82 "critical issue when minimizing main window"; 5.9.87 → 5.9.88 comment corruption tied to localization. Platform adaptation lags OS releases (macOS 15 Certificate Manager broken until 5.9.94; Linux Qt6 support only Apr 2026). Verdict: TRUE (changelog-derived).
3. **Support and continuity are single-channel, with no external accountability.** Support = one email address, English, no public forum/bug tracker/issue tracker; no refunds except unresolvable defects; EULA §11.1 explicitly reserves the right to stop development and updates at any time. NDSS 2021 ("Processing Dangerous Paths") additionally flagged that Master PDF Editor "silently writes to or overwrites arbitrary files" when opening malicious PDFs — no published post-fix statement found. Verdict: TRUE (EULA/purchase pages), MOSTLY_TRUE (NDSS finding as current behavior — paper is 2021, fix status unverified).

(Secondary, lower-weight: crash reports on Linux packaging threads — MOSTLY_TRUE; "outdated UI" — MOSTLY_TRUE as cosmetic consensus.)

## Loved workflows (from user communities)

1. **Form-field and text-object surgery.** "After struggling with about half a dozen tools I happened on Master PDF. What a revelation. You can select and move form fields and text" (r/sysadmin, 2022). The type-filtered edit modes + object inspector are the draw. Verdict: TRUE (primary thread + repeated Linux-forum endorsements).
2. **The XFA rescue.** First recommendation whenever someone has a government/insurance XFA form rendering "Please wait..." in a browser/reader — opens, fills, prints (AskUbuntu, Wondershare 2026 roundup "best XFA reader for Linux"). Verdict: MOSTLY_TRUE (multiple independent recommendation threads).
3. **Perpetual-license local editing on Linux (and signing/initials on scanned docs).** One of the only serious native-Linux commercial PDF editors; single payment; insert text/signature-image into scans; email-from-GUI; runs on modest hardware. Verdict: MOSTLY_TRUE (Linux Uprising, Linux Mint forums, r/linuxquestions).

## Top 5 feature gaps (what Master PDF has that GlyphPDF lacks — priority order)

1. **JavaScript forms model** — per-field actions on 6 triggers, calculation/validation via JS, document-open/save/print and page-view script events, dedicated JS settings tab. GlyphPDF has a fixed "calculated field" and no scripting. (TRUE, official docs)
2. **XFA: static-XFA editing + dynamic-XFA fill/view** — GlyphPDF has nothing XFA; this is MPE's community moat. (TRUE)
3. **Object-level editing depth** — type-filtered edit modes, transformation-matrix editing, clipping-path removal, math-expression coordinates, Z-reorder, paste-to-multiple-pages, OCG layers. (TRUE)
4. **Certificate-based document encryption + PKCS#11 token signing + batch signing + invisible signatures.** (TRUE)
5. **Print-production bundle** — virtual PDF printer, booklet, manual duplex, paper-source-by-size, presets, multi-file Bates continuity. (TRUE)

(Honorable mentions: measurement tools; navigation-triggered auto-OCR; scan acquisition + "convert to scanned pages"; PDF/A a-levels; watermark/background template manager.)

## Top 3 failure modes (exploitable weaknesses)

1. **Watermarked free tier / licensing distrust** — GlyphPDF's free story can win the Linux/power-user crowd that still resents the v4→v5 switch.
2. **No transactional safety net** — autosave writes the live file, redaction warns you to keep a copy yourself, plain save invalidates signatures, Reset Forms is non-undoable, NDSS flagged silent file overwrites. GlyphPDF's safe-save/SHA-256/undo-stack evidence base is a marketing-grade differentiator for exactly MPE's power-user audience.
3. **Thin support surface + slow platform adaptation + no accountability channels** — email-only, EULA walk-away clause, hotfix cadence, macOS/Qt6 lag. A competitor with published regression discipline (GlyphPDF's ledger-grade testing) can target professional/enterprise buyers MPE courts via MSI/GP but can't truly serve.

## Top 3 loved workflows (to replicate)

1. Form-field/text object manipulation with type-filtered modes and a real object inspector (PRD §10 "right panel: object properties" should rise to this bar).
2. One-click rescue of XFA and "Please wait..." documents (fill + print offline).
3. Cheap perpetual license + fully local operation on any of 3 OSes — the trust economics, not just features.

## Single biggest opportunity

**Own "Master PDF Editor's power, with GlyphPDF's guarantees":** the closest architectural cousin has left a trust vacuum around its deepest features — object surgery, form/JS editing, redaction, and signing all operate on the live file with no transactional safety, no preservation contracts, and watermark-based licensing. GlyphPDF can replicate the 20% of MPE capabilities that drive its devotion (object-level editing, field-level form authoring with calculated fields and tab order, XFA *fill*, FDF/data round-trip) and wrap them in the ledger's already-built safe-save transactions, undo snapshots, capability disclosure, and honest free tier — i.e., beat the small vendor on exactly the axis (verifiability) a small vendor structurally cannot match, without racing it on breadth (print driver, batch signing).

## Sources

Official (high tier):
- Product page — https://code-industry.net/masterpdfeditor/ (feature claims, demo watermark, virtual printer)
- Online Manual index + ~45 pages — https://code-industry.net/masterpdfeditor-help/ (overview, forms, pdf-forms-properties, fill-pdf-forms, ocr-pdf, redacting-pdf-document, compare-files, digital_signatures, invisible-signature, inserting-initials, encryption-pdfs-password, encryption-pdfs-certificate, objects, edit-pdf-text, editing-formatted-text, editing-modes, working-with-pdf-pages, split-merge, bates-numbering, headers-footers, watermark, background, add-and-edit-layers, convert-to-pdfa, optimizing-and-saving, optimize-scanned-pages, comment-pdf-files, comments-menu, document-navigation, pdf-files-view, print-pdf-files, opening-and-saving, autosave, backup-saving, export-to-word/excel, virtual_pdf_printer, license_agreement, technical-support, masterpdfeditor-settings)
- Version history "What is new in Master PDF Editor 5" — https://code-industry.net/what-is-new-in-master-pdf-editor-5/ (5.8.06–5.9.99, dates, feature first-appearance)
- FAQ (activation/demo/renewal) — https://code-industry.net/faq/, /faq-demo-version/, /faq-license-renewal/
- Purchase/pricing — https://code-industry.net/purchase/, /purchase-masterpdfeditor/
- Downloads — https://code-industry.net/downloads/ (v5.9.99 current)

Community/reviews (corroboration tier):
- r/sysadmin "PDF editing - praise for Master PDF" — https://www.reddit.com/r/sysadmin/comments/t5p7iu/
- r/linuxquestions "Good PDF editor?" (crash complaint, nags) — https://www.reddit.com/r/linuxquestions/comments/8oscc2/
- Linux Uprising, "Master PDF Editor 4 free-to-use version" (v4/v5 watermark split) — https://www.linuxuprising.com/2019/04/download-master-pdf-editor-4-for-linux.html
- AUR masterpdfeditor-free — https://aur.archlinux.org/packages/masterpdfeditor-free
- AskUbuntu (XFA/Please-wait rescue; v4 installs on 24.04) — https://askubuntu.com/questions/1557541/
- Wondershare comparison (XFA-on-Linux praise, watermark/UI critiques) — https://pdf.wondershare.com/pdf-software-comparison/master-pdf-editor.html
- NDSS 2021, "Processing Dangerous Paths" (arbitrary file write finding) — https://www.ndss-symposium.org/wp-content/uploads/ndss2021_1B-2_23109_paper.pdf
- Void Linux packaging crash issue — https://github.com/void-linux/void-packages/issues/22869
