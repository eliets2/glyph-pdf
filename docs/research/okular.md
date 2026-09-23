# Spec Sheet: Okular (KDE) — Deep-Dive for GlyphPDF

**Date:** 2026-09-07
**Requested by:** GlyphPDF parity program (research-specialist protocol)
**Research question:** What does KDE's free viewer/annotator actually ship across viewing, annotation, forms, signatures, redaction, OCR, editing, and platforms — how good is the annotation persistence (their known strength), did they add redaction and is it real excision, what is the digital-signature timeline/quality, how mature is the Windows port, and what do Okular users keep asking for that is missing (edit text? OCR?) — so GlyphPDF can rank build priorities while staying offline-first/local-only.
**Method:** Primary-source extraction via curl + full-text mining of: okular.kde.org (home incl. release-news block, /download, /faq, /formats), the complete current Okular handbook (doc/index.docbook, master @ Sept 2026, handbook date 2026-02-23 for 26.04), the **full Okular master source tree** (KDE GitHub mirror, pushed 2026-09-07 — downloaded and grepped end-to-end for redact/OCR/compare/PAdES/LTV/DSS/FDF/XFA/timestamp), KDE Gear changelogs 20.12–26.04 (per-commit entries), KDE Gear announcements 21.12/22.04/23.08, NLnet project page (signature funding), KDE Bugzilla (quicksearch exports for redaction/OCR/edit-text/Windows), Wikipedia (Windows Store history, annotation-persistence timeline), and community threads (r/kde, r/linuxquestions, KDE Discuss). Absence claims are graded against the full source tree + handbook + changelog corpus, not search-engine recall.
**Benchmark baseline:** GlyphPDF ledger `CURRENT-EVIDENCE-LEDGER-2026-09-05.md` + `PRD.md` §9.

**Verdict scale:** TRUE / MOSTLY_TRUE / PARTLY_TRUE / MOSTLY_FALSE / FALSE / UNVERIFIABLE (per protocol).

---

## 0. Product snapshot

| Dimension | State | Verdict |
|---|---|---|
| Product | Okular — KDE's "universal document viewer"; GPLv2+; C++/Qt6 (Qt6 port complete); descends from KPDF (2005) | TRUE (homepage "GPLv2+"; handbook "based on the code of the KPDF application"; Wikipedia) |
| Current version | 26.04 (released 2026-04-16); prior: 25.12, 25.08, 24.12. Version scheme is KDE Gear YY.MM | TRUE (homepage news block: "Thursday, 16 April 2026 — Okular 26.04 released"; Wikipedia infobox 26.04.0) |
| Cadence | 3 Gear releases/year (Apr/Aug-Dec cycle); each Okular release is described as "several minor fixes and feature enhancements" — maintenance-grade releases, no big-bang features in 25.04–26.04 | TRUE (homepage news; changelogs 25.04–26.04 mined: annotation/signature polish items) |
| Core identity | Viewer + annotator + form-filler + signature tool for PDF and ~14 other formats. NOT an editor. The Tools menu contains Browse/Zoom/Selection/Magnifier/Annotations/Digitally Sign/Speak — nothing else | TRUE (handbook Tools Menu table, read in full) |
| Render engine | Poppler for PDF; libspectre (PS), libTIFF, DjVuLibre, epub, Discount (Markdown) backends. Signature ops delegate entirely to Poppler→NSS | TRUE (handbook backends config + formats page "Main library used: poppler") |
| Distinctions | World's first software product awarded the Blue Angel ecolabel (government-backed eco-certification, 22.04 era); LaTeX math rendering inside annotation text ($$…$$); works on nearly every document format, not just PDF | TRUE (22.04 Gear announcement: "world's first computer program to be awarded an eco-certification backed by a government"; homepage; handbook LaTeX annotation note) |
| Mission premise check: "Okular added redaction" | **FALSE.** Zero redaction code, UI, or docs in the current master tree (only Romanian/Interlingue translation strings for "redactor" = "editor" match "redact"); handbook has zero redaction mentions; no changelog entry 20.12–26.04; KDE Bugzilla 489299 "Add redaction Tool" (opened 2024-06, CONFIRMED wishlist) and 452403 "Okular needs ability to sanitize and redact PDF" (2022, REPORTED) are open feature requests. The best-known community recipe is literally "black highlighter + black rectangles + flatten with Ghostscript" (bug 489299 verbatim) | **FALSE** (multi-source, primary: full-source grep + bug tracker) |
| Reputation framing | Beloved on Linux; consistently the answer to "best PDF annotator on Linux". Recurring complaints: no content editing, no OCR, no page manipulation, NSS certificate setup friction for signing, Store-version lag on Windows | TRUE (directionally; KDE Discuss #1298, r/kde threads, bug corpus) |

---

## 1. Spec tables by domain

Column key: **Verdict** grades the row's Notes claim on the protocol scale.

### 1.1 Viewing & navigation

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| View modes | Single Page; Facing Pages (two-up); Facing Pages (Center First Page); Overview (user-configured column count); Continuous mode toggle; "Open in continuous mode by default" pref | Two-page = "Facing Pages"; Overview mode is a configurable N-column grid — GlyphPDF §9.1 has single/continuous/two-page/presentation; Okular adds the Overview grid (GlyphPDF lacks an N-up browse grid) | TRUE |
| Presentation mode | Fullscreen page-per-page, auto-advance timer, loop, transitions (+random), summary page, progress circle, touch-navigation profiles, multi-monitor screen picker, pencil drawing on slides (cleared on exit), Erase Drawings | Deeper than most viewers; drawings are ephemeral (never persisted). PDFs can self-specify presentation mode on open | TRUE |
| Reading navigation | Vim-style HJKL keys, autoscroll (Shift+Up/Down, speed control), cursor wrap at screen edges (**X11 only** — a Linux-first leftover), back/forward position history (Alt+Shift+Left/Right), Go to Page, per-document "continue from last viewed page" | The back/forward jump history is genuinely good for reference-heavy reading (click citation [15], jump back) | TRUE |
| Thumbnails / TOC / sidebar | Thumbnails panel (linked to page), Content panel (TOC), Bookmarks view (multi-doc, filterable, tree, rename), Annotations view (click-to-navigate + keyboard), Layers panel (toggle OCG layers) | Layers visibility control is a pro feature GlyphPDF does not surface | TRUE |
| Search | Find bar, Find Next/Previous (F3/Shift+F3), `--find` CLI to open pre-highlighted | Plain string search only — no regex, no search-in-comments/bookmarks (GlyphPDF §9.15 leads); known open bugs about missed matches in some PDFs ("Failed to find a string occurence…", CONFIRMED) | TRUE |
| Selection tools | Text selection (copy/speak), rectangular area selection (copy image/save to file/speak), **Table Selection tool** (draw rectangle, insert row/column dividers, copy as table to clipboard) | Table-to-clipboard is a loved academic workflow; copy is plain-text/HTML to clipboard, not a structured export | TRUE |
| Trim view | Trim Margins (persistent across restarts) and Trim To Selection (per-session bounding box, 20% minimum) | Raster-free display-side cropping — read-ergonomics feature, not page surgery | TRUE |
| Tabs / session | Open in tabs (optional), switch-to-existing-tab dedupe, undo close tab, recent files | Tabbed viewing free; no workspace/session save | TRUE |
| Accessibility display modes | 8 color transforms: Invert, Paper color, Dark/Light swap, B&W with threshold+contrast, Invert Lightness, Invert Luma (sRGB Linear), Invert Luma (Symmetric), Shift Hue ±120° — each bindable to a shortcut | The most complete recoloring stack in any free PDF viewer; huge for eye-strain and low-vision users | TRUE |
| Embedded files / media | Embedded-files bar + extraction dialog; sound/movie annotations (poppler backend) | Viewing/extraction only | TRUE |
| Magnifier | Pointer-following magnifier widget (10× pixel scaling) | — | TRUE |
| Inverse search (LaTeX) | Shift+click jumps to source line in Kate/Kile/Emacs/LyX/TeXstudio/TeXiFy; pdfsync & synctex; configurable editor command | Niche but loved by academics; no GlyphPDF equivalent (and none needed for its personas — noted for completeness) | TRUE |

**GlyphPDF delta (bidirectional):** Okular is the best-in-class *reading* UX in the free tier — Overview grid, trim view, 8 color modes, presentation drawing, table-selection copy, back/forward history are cheap-to-copy ergonomics that don't conflict with GlyphPDF's offline workstation goals. Conversely, GlyphPDF's navigation-sync (ledger CMP/U04) and search depth (§9.15) have no Okular counterpart at all; Okular cannot even display two synchronized documents.

### 1.2 Editing (text / object / page)

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Edit existing text | **None** | No mechanism to modify page content exists in the codebase. The closest gesture is the **Typewriter annotation** ("Adds text on a transparent background. This is useful for filling out forms lacking integrated editable text fields" — handbook) — an overlay, not an edit. Bug "A way edit the document would be great" open; community answer is "use LibreOffice Draw" | TRUE (absence) |
| Add/delete/move objects | **None** | No object model exposed; nothing to select/move/resize beyond annotations (resize limited to Inline Note, Typewriter, Stamp, Ellipse types) | TRUE (absence) |
| Page manipulation (insert/delete/reorder/rotate/extract) | **None in-app**; view-only rotation (View > Orientation, incl. Original Orientation reset) | KDE Discuss #1298 is a standing feature request ("rearranging & deleting pages as well as merging two PDF files"). View rotation is per-session display state | TRUE (absence) |
| Split / merge | **None** | Not in Tools menu, not in source, not in handbook | TRUE (absence) |
| Watermark / headers-footers / Bates | **None** | Stamp annotation is the closest analog (see quirks in §1.5) | TRUE (absence) |
| Metadata editing | **View only** (File > Properties: title, author, creation date, fonts) | No user-facing metadata writer; no sanitization verb | TRUE |
| Undo/redo | Session-scoped, covers annotation creation/removal/property edits/moves/content edits (Ctrl+Z / Ctrl+Shift+Z) | Clean annotation-level undo — but no document-transaction safety story (GlyphPDF safe-save/U05 has none to compare against here) | TRUE |

**GlyphPDF delta:** This whole domain is GlyphPDF's home turf — inline text editing, object model, page surgery, split/merge are all categorical over Okular (whose users route to LibreOffice Draw or CLI tools for exactly these jobs, per r/linux and KDE Discuss). Nothing to import. The one borrowable idea: Okular's per-session undo covering *every* annotation op including text-content edits — GlyphPDF's annotation history should be able to match that breadth.

### 1.3 OCR

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| OCR engine | **Absent** | Zero matches for OCR/tesseract anywhere in the master tree (source grep). Bug "Integrate Ocrad into Okular for enhanced usage and increased accessibility" CONFIRMED-unimplemented; r/kde thread "Any particular reason why Okular doesn't support OCR or other PDF editing tools?" (Dec 2023) confirms user-perceived absence | TRUE (absence) |
| Scanned-document workflow | Text selection silently does nothing on image-only pages; no searchable-layer generation, no deskew, no orientation fix | Users pair Okular with external OCR (tesseract CLI, ocrmypdf) | TRUE |
| Speak (TTS) | Speak Whole Document / Current Page / pause-resume via Qt Speech (speech-dispatcher on Linux, native engines elsewhere) | The accessibility stand-in for text you can't select | TRUE |

**GlyphPDF delta:** GlyphPDF's dual-engine OCR (Tesseract+PP-OCRv5 ROVER, verify screen, confidence gating — ledger F05/F10/F11/F04, U03) is a categorical over Okular, which has nothing. Okular's TTS-speak is the only capability here GlyphPDF lacks; it is a low-cost accessibility checkbox (Qt6 Speech) worth considering for §9.14 parity with a feature users praise in reviews.

### 1.4 Forms (AcroForm support quality)

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Show/fill AcroForms | Forms bar on open ("Show Forms"), View > Show/Hide Forms, per-field rendering, field event scripts processed (FocusIn/FocusOut, mouse, format events via processScriptAction) | Fill quality is decent for ordinary AcroForms; 24.12 fixed "order of execution of events for text form fields" — still actively patching | TRUE |
| Form JavaScript | Partial: link/document/field scripts dispatched from Poppler; document-level JS execution gated by a setting ("DocumentScripts") | No full Acrobat JS engine; complex calculated/validated forms behave imperfectly. r/kde "Okular feels a bit limited… w.r.t. filling/annotating" reflects this | MOSTLY_TRUE |
| XFA | **Explicitly unsupported and flagged**: generator returns `HasUnsupportedXfaForm` when `formType() == XfaForm` | User is told the form is unsupported rather than silently mis-filling — honest, but zero capability | TRUE (absence, verified in source) |
| Save filled forms | File > Save/Save As "including all the changes (annotations, form contents, etc.)" where backend supports it | Per docbook; failures fall back to .okular archive offer | TRUE |
| Create/edit form fields | **None** | Nothing in menu/source/docs. No field palette, no auto-detect | TRUE (absence) |
| Form data import/export | **None** | No FDF/XFDF/CSV/XML anywhere in source (grep) | TRUE (absence) |
| Flatten forms | **None in-app** (community recipe: print-to-PDF or Ghostscript) | — | TRUE (absence) |
| Form font-size control | None; open CONFIRMED wishlist "When filling in forms, make it possible to set the font size of the forms" | Quirk: filled text renders at the field's baked font size | TRUE |

**GlyphPDF delta:** GlyphPDF is a full generation ahead (10 field types, calculated fields, validation, tab order, auto-detect with compound undo — V06/F01/R02 — CSV/FDF data exchange, flatten). Okular's honest XFA flag is still worth copying: its capability registry / whyNot pattern (U08) should explicitly name XFA documents as unsupported rather than failing oddly — Okular proves users respect a clean "this form type is not supported" banner.

### 1.5 Annotations (their strength)

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Tool inventory | Highlighter (Highlight/Squiggle/Underline/Strike-out types), Inline Note, Pop-up Note, Freehand, Straight Line, Arrow (with leader line + extension length), Rectangle, Ellipse, Polygon (with note), Stamp, Typewriter; **works on ALL 14 formats**, not just PDF | Graphic annotations are format-agnostic (handbook states this explicitly) — unique breadth; GlyphPDF is PDF-only | TRUE |
| Quick annotation UX | Annotation toolbar (F6) with keys 1–9 per tool position; Quick Annotations strip on the main toolbar (sliding mini-bar at right of pane) with Alt+9…Alt+0 global activation "even with the annotation toolbar hidden"; "keep tool active after use" (continuous mode) pin per tool; add current annotation to quick menu from toolbar | This is the fastest annotation loop in the free tier — keys 1-9 + Alt-9..0 mean a reviewer never touches a menu | TRUE |
| Tool configuration | Full Annotations config page: Add/Edit/Remove/Reorder tools; two same-type tools with different defaults; per-tool name/type/appearance; defaults for color, fill, opacity, font, width, leaders | "Annotation tools in Okular are highly configurable" is docbook-verbatim; configurable-annotation-toolbars test exists in autotests | TRUE |
| In-place adjustment | Right-click > Properties per annotation (color/opacity/font/author/leaders…); move via Ctrl+drag; 8 resize handles (Inline Note/Typewriter/Stamp/Ellipse only); Shift constrains lines/polygons to 15° and shapes to 1:1 | Resize limited to 4 annotation types — a real usability gap vs. any drawing tool | TRUE |
| Annotations sidebar | List view, navigate-to-annotation, copy annotation text to clipboard (23.08), icons colored by annotation color (26.04) | No threads, no statuses (open/resolved), no filters, no CSV/comment export — GlyphPDF U07 leads | TRUE |
| Author identity | Configurable author string; defaults from systemsettings account details; per-annotation author editing in Properties | Single flat author per session; no reviewer roles | TRUE |
| LaTeX in annotations | `$$code$$` in any annotation text renders via local LaTeX (latexrenderer.cpp) | Killer academic feature; requires a TeX installation | TRUE |
| Stamps | Pre-defined symbol stamps, square or rectangular placement, opacity; **custom image stamps are "experimental" and — docbook verbatim — "Custom stamps inserted in PDF documents are not visible in PDF readers other than Okular"** | A real interop defect hiding in a footnote; stamps that vanish in Acrobat are a workflow breaker for review cycles | TRUE (docbook verbatim) |
| Persistence model | Annotations live **outside the file by default**: .okular document archive carries document+annotations for collaboration ("save a document archive … easily possible to share the original document and your annotations with other Okular users"); standard in-PDF embedding since Okular 0.15 + Poppler 0.20 (Wikipedia, cited); Save/Save As embeds into PDF; if backend can't save, Okular offers archive fallback | The two-surface model (archive vs embedded) is the origin of Okular's flexibility — but also its classic gotcha: annotate, close without saving, annotations are gone | TRUE |
| Save fidelity | Docbook verbatim: "even if there are no changes to the file, the new file need not to be an exact bit-for-bit copy of the original file (e.g. can have a different SHA-1 hash)" | Full-file rewrite on save; no incremental update, no byte-preservation guarantee — the exact failure class GlyphPDF's safe-save transactions (F01, SHA-256 source-invariance tests) are built to prevent | TRUE (docbook verbatim) |
| DRM | "Obey DRM limitations" toggle; "in some configurations of Okular, this option is not available" (distro patch-outs); DRM may block annotate/edit/remove | Owner-locked documents are respected; GlyphPDF redaction-on-secured-files policy should document its stance similarly | TRUE |
| Print/export annotations | Print dialog PDF Options: print annotations toggle, Force rasterization, scale modes (scale-mode defaults configurable since 23.08); Export As: Plain Text, Document Archive; Share menu (KPurpose) | No per-annotation summary/report, no XFDF/FDF comment round-trip | TRUE |
| Quality trajectory | 25.04–26.04 changelog: unbuffered-annotation opacity parity, stamp scaling fix (bug #370382 — 9 years old!), annotation font ColorDialog fix (#512923), sidebar icon colors | Actively maintained, incrementally polished | TRUE |

**GlyphPDF delta (bidirectional):** Okular's annotation loop — configurable tool palette + 1–9/Alt-0 keyboard activation + quick strip + continuous-mode pin — is the single best annotation *throughput* design in the free tier, and GlyphPDF should copy it almost verbatim (it is UI-layer work, orthogonal to GlyphPDF's engine). Okular also wins on format breadth (annotations on EPub/DjVu/Comics) and on the .okular archive as a review-collaboration artifact. GlyphPDF wins everywhere else that matters to professional use: annotation threads/statuses/filters/CSV (U07), stamps that other readers can actually see, safe-save byte-invariance instead of silent hash churn, and the planned capability registry to explain per-document annotation limits. Note the marketing line: Okular's own handbook admits saves are not byte-stable — that is a compliance-user trust gap GlyphPDF can name.

### 1.6 Redaction

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Redaction tool | **Does not exist** (as of 26.04 / master 2026-09) | Full-tree source grep: zero redact symbols; handbook: zero mentions; changelogs 20.12–26.04: zero entries; Bug 489299 "Add redaction Tool" (2024-06-27) CONFIRMED wishlist; Bug 452403 "Okular needs ability to sanitize and redact PDF (and possibly other types of) documents" (2022) REPORTED | **FALSE** (feature premise) — absence itself graded TRUE |
| Community workaround | Set highlighter to black → highlight text → draw black rectangles over images → flatten with Ghostscript so text can't be marked (bug 489299's own wording) | I.e., an overlay with a manual, error-prone, document-wide flattening step; the bug reporter explicitly lists "easy to forget the flattening" as drawback #1 | TRUE (verbatim) |
| Black-highlighter pseudo-redaction leak behavior | Nothing removes content: an opaque black annotation leaves the underlying text fully extractable in any other viewer once flattened-or-not | Okular never claims otherwise in docs; the risk is user error, not tool deception | MOSTLY_TRUE (no dedicated leak study exists — inferred from architecture) |
| Pattern redaction / sanitize / logs / preview | **All absent** | No sanitize verb anywhere; 452403 asks for exactly this bundle | TRUE (absence) |

**GlyphPDF delta (and premise correction):** The mission premise "they added it — grade whether it does real content excision or overlay" is graded **FALSE** — Okular has shipped no redaction through 26.04 (April 2026), and the open wishlist bugs prove demand without delivery. This makes redaction a clean GlyphPDF differentiator versus the KDE free stack: content-stream excision with transaction stages, sanitize bundle default-ON, overlay-label burn-in, word-list/pattern marking (U05, §9.8, N04/N07/D01 repairs) has *no* counterpart to parity-check against — there is nothing on their side to match. Watch item: bug 489299 is CONFIRMED and aligned with the NLnet-funded signature push, so Okular could land a first redaction implementation within a year or two; grade today's capability as zero.

### 1.7 Security — certificate (digital) signing

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Signing timeline | Verification tiers arrived via Poppler (0.51 base info, 0.68 reason/location, 0.73 certificate info — handbook); **creation** requires Poppler ≥ 21.01 and appeared in Okular in the 21.04–21.12 window (21.12 changelog already carries signing fixes: "Fix issues when cancelling while adding a digital signature"; Burrows walkthrough Apr 2022; NLnet funded the improvement project) | So: signing is a ~2021–2022 arrival, still receiving UX rework in 24.12 ("Rework UX for adding digital signature", bug #443403) — "added recently" is fair for creation, unfair for verification | MOSTLY_TRUE |
| Signing workflow | Tools > Digitally Sign… (top-level hamburger entry since 23.08); draw the signature rectangle; reason/location metadata (23.08); background image behind signature (23.08; all image types 24.12); certificate picker from NSS store; unsigned signature fields can be signed (22.04); signature placeholders (renamed from "unsigned signature", 25.08) | Alert when you try to sign with no valid certificate (22.04 highlight). Appearance = background image + auto info line; **font size of the signature display not adjustable** (bug 443403 remained open after 23.08 improvements) | TRUE |
| Certificate sources | Poppler→NSS chain (docbook order): Firefox user store → /etc/pki/nssdb → $HOME/.pki/nssdb; custom store path configurable in PDF Backend page (with certificate list UI) | **No Windows/macOS system certificate store integration** — on Windows you are still in NSS-certutil territory (Burrows: "properly configuring Okular to sign documents can be challenging"). No PKCS#11/smartcard mention in docs or source | TRUE |
| Signature type/level | Generic PKCS#7 (adbe.pkcs7) signing via Poppler. **No PAdES** (zero PAdES/B-T/B-LT hits in source), **no LTV/DSS augmentation** (zero hits), **no RFC3161 timestamping** (no timestamp-sign path in poppler generator) | Verdict grades: "Okular signs but produces no PAdES-baseline or LTV-enabled signatures" — the exact ladder GlyphPDF's §9.7 (ETSI layout, DSS/B-LT degradation surfacing via SignOutcome) climbs | TRUE (source-verified absence) |
| Signature appearance rendering | Cryptographic signature widgets visible on-page; ensure-not-rotated/displaced fixes (26.04-era); signature pane shows signature type (25.08) | No appearance templates like GlyphPDF's /AP ETSI layout with CN/date/reason (ledger 61fac01) — Okular's look is background-image + plain text, with a known oversized-font quirk | MOSTLY_TRUE |

**GlyphPDF delta:** Okular signs; GlyphPDF *certifies*. The delta ladder: NSS certutil ceremony vs GlyphPDF cert/DIGID flows; no PAdES levels vs B-T/B-LT with missing-piece wording (22a7b66); no timestamp vs signed timestamps; fixed-size awkward appearance vs auto-fit ETSI /AP; and Okular's signing UX was only de-roughed in 24.12. Nothing in Okular's signing stack is ahead of GlyphPDF's except raw maturity-in-the-wild (five years of Poppler interop testing against real-world signatures).

### 1.8 Signature verification

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Signature panel | Signatures bar on open → Signature panel: per-signature validity, "detect any modifications since the moment the document was signed", certificate details (homepage wording + handbook) | The homepage leads with this as "First-class Signature Support" | TRUE |
| Validation mechanics | Poppler/NSS chain validation; OCSP network queries with an opt-out ("PDF: Support not contacting OCSP servers when validating signatures", 21.12 — relevant to GlyphPDF's offline-first posture); expired-certificate signatures highlighted (21.12); "good but not fully valid" states handled with distinct wording (26.04-era); plural verification of multiple signatures (25.08) | CRL/OCSP present through Poppler; no revocation-info *embedding* into the file (no DSS) — validation is live-at-open only | TRUE |
| Health-certificate quirk | "Don't color health certificates without a verified signature green" (21.12) — EU digital COVID certificates rendered in Okular got special-cased | Amusing interop scar; harmless | TRUE |
| Trust display | Signature pane + per-certificate inspection; no document-level certification-policy display (no doc-MDP enforcement UI found) | GlyphPDF's on-page per-signature badges anchored to field rects (10efbd5 lane) exceed the panel-only model | MOSTLY_TRUE |

**GlyphPDF delta:** Okular's verification is genuinely trustworthy (Poppler does the crypto) and offline-tunable, but it is read-only validation with live revocation checks and no LTV story: a signed PDF validated today gives no path to validate in 5 years without network. GlyphPDF's DSS/B-LT augmentation, on-page badges, and SignOutcome degradation messaging are a full level above. Copyable idea: the OCSP opt-out switch as an explicit offline-mode affordance.

### 1.9 Document comparison

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Compare two documents | **Absent** | No compare code path in source grep, no feature in any changelog, no bug request surfaced in the corpus (users pair documents in tabs manually — tabs don't even synchronize scroll) | TRUE (absence) |

**GlyphPDF delta:** Compare (DiffEngine, fingerprints, middle-insertion alignment, V04/V06-class fixes, reports) is pure white space against Okular. Even Okular's tab mode cannot do side-by-side synchronized viewing — worth a sentence in GlyphPDF positioning.

### 1.10 Batch processing & automation

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Batch operations | **None** (no batch UI, no presets, no hot folder) | — | TRUE (absence) |
| CLI surface | `okular file.pdf`, `-p page`, `--presentation`, `--print`, `--print-and-exit`, `--unique` (single instance), `--noraise`, `--find string`, `--editor-cmd`, `document#named_destination` | A viewer's CLI (open/print), not a processing CLI; `--unique` + `-p` is a known power-user pattern for jumping to pages from editors | TRUE |
| Watch/reload | "Reload document on file change" preference — auto-reload if the open file mutates on disk | Hand-crafted adjacency to hot-folder workflows, viewing-side only | TRUE |

**GlyphPDF delta:** Batch/hot-folder (§9.12) has zero Okular counterpart; nothing to borrow except the `--find`/`--presentation` style of scriptable entry points and the file-change reload toggle.

### 1.11 Print production

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Printing | Print dialog with PDF Options: print-annotations toggle, Force rasterization, scale modes (only when rasterization forced); default scaling mode preference (23.08); Print Preview | Solid reader-grade printing; rasterization escape hatch for font problems | TRUE |
| PDF/A or print-production output | **None** | No PDF/A export, no color management, no preflight, no overprint/ink tools anywhere in source or docs | TRUE (absence) |

**GlyphPDF delta:** PDF/A export (1B–3U, veraPDF-validated lane) is pure GlyphPDF territory; Okular users who need PDF/A route through other tools.

### 1.12 Accessibility & tagging

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Reading accessibility | 8 color transforms, TTS engine/voice selection, link borders, keyboard-only operation (fully menu/shortcut navigable), magnifier, trim view | Strong *consumption* accessibility | TRUE |
| Screen reader / UI accessibility | Qt/KDE accessibility frameworks; Blue Angel eco/efficiency certification includes accessibility criteria | — | MOSTLY_TRUE |
| PDF tagging | **None**: no tag tree inspection, no reading-order checker, no role map display; text-extraction order bugs exist ("Okular does not respect logical text order in PDF while selecting text" — REPORTED) | Okular renders tagged PDFs via Poppler but offers no tagging surface. GlyphPDF §9.14 (reading-order checks, async checker) is ahead | TRUE (absence) |

**GlyphPDF delta:** GlyphPDF leads on structure-level accessibility (tag-aware checks, preserve-tags exports); Okular leads on perceptual accessibility (colors, TTS, magnifier, keys). The TTS gap is the one cheap absorb.

### 1.13 Import / export formats

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Open formats | PDF, PostScript, TIFF, DjVu, Images (JPEG/PNG/GIF/TIFF/WebP), DVI, XPS, OpenDocument Text (ODT), FictionBook, ComicBook (CBR/CBZ), Plucker, EPub, Fax, Mobipocket, Markdown, plus printer output | 16-ish backends; per-format capability matrix published on the formats page (annotations/forms/printing/text-export per backend). Note: Ubuntu's okular-extra-backends historically omits EPub due to licensing (FAQ) | TRUE |
| Export | Plain Text (any doc), Document Archive (.okular), images of pages via selection tool; Share menu via KPurpose | **No Office/HTML/CSV conversion, no PDF→Word** — conversion is delegated to other KDE/external tools. Non-PDF formats can't be saved back to themselves in place (archive fallback) | TRUE |
| Save/convert between formats | Import PostScript as PDF exists in menu history (commented out in current docbook — treat as removed/unavailable in 26.04) | PostScript→PDF conversion appears disabled in the current handbook menu listing | PARTLY_TRUE (stale doc artifacts; feature state unclear) |

**GlyphPDF delta:** Okular's breadth is *input* breadth (EPub/Comics/DjVu) — irrelevant to GlyphPDF's PDF-workstation mission. GlyphPDF's in-house OOXML/CSV export lane has no Okular analog; nothing to borrow except the published per-format capability matrix (exactly the pattern GlyphPDF's capability registry formalizes).

### 1.14 Platform availability (Windows port quality)

| Feature | Sub-capabilities | Notes (real behavior, limits, quirks) | Verdict |
|---|---|---|---|
| Linux | First-class: all distros, Flathub (stable + KDE nightly repo), Snap | The canonical platform | TRUE |
| Windows | **Microsoft Store is the recommended install** ("tested by our developers… seamless updates", download page); plus **untested nightly .exe installers** from KDE's binary factory (Windows, macOS ARM, macOS x86_64) | In Store since **September 2019** (Wikipedia, cited to the Store listing). Wikipedia: the Store version "is often not the most recent version", and older versions are not available for download | TRUE |
| Store-version lag & update quirks | Community + Wikipedia report Store builds lagging stable (e.g., 23.08.1 era examples); open bug "[Windows Store] Filename extension associations and updates resetting them" | Update-path friction is the recurring Windows complaint | MOSTLY_TRUE |
| Windows feature parity | Full feature set compiles (Qt6/KF6 cross-platform); speech requires a Windows TTS engine; NSS-based signing has **no Windows certificate-store bridge** — Windows users must build an NSS database manually | The signing path is the clearest Linux-first seam: docbook's cert-store chain is Firefox//etc/pki/nssdb/~/.pki — no CryptoAPI/DPAPI mention anywhere | TRUE |
| macOS | Nightly (untested) installers only; no stable channel, not in the Mac App Store | "macOS (ARM)" + "macOS (Intel)" nightlies | TRUE |
| Android / mobile | Okular mobile (Kirigami/QML app) ships in-tree incl. signature viewing UI (22.04 changelog "Mobile: Add Signature Viewing UI"); NLnet describes Okular on "Android, postmarketOS and pureOS" | Real codebase (mobile/app QML incl. Signatures.qml), but the mobile app is a lighter viewer; desktop-grade annotation/signing UX is desktop-first | MOSTLY_TRUE |
| Port engineering quality | Same codebase, Craft build system (.craft.ini in tree), CI-built Windows binaries, KDE-supported | Not an afterthought port (Store version is the *recommended* Windows install), but release-channel lag + NSS signing friction + no old-version downloads keep it below commercial Windows polish | MOSTLY_TRUE |

**GlyphPDF delta (bidirectional):** Directly relevant: Okular proves a Qt6 document app can be a credible Windows citizen via the Store + nightly installers, with the Store channel buying update trust — and equally proves the failure modes GlyphPDF should design out: channel lag, association resets after updates, no pinned old versions, and Linux-era security plumbing (NSS) that ignores the Windows certificate store. GlyphPDF's Windows-native signing (CNG/PKCS#11 lane) is a structural advantage over a Poppler/NSS stack; if GlyphPDF ever ships a Store presence, the association-reset bug report is the pre-written test plan.

---

## 2. Cross-cutting findings (finding → implication → recommendation)

### Finding 1: The redaction premise is false — and that is an opportunity, not a stale brief.
Full-source grep (master, pushed 2026-09-07), handbook, and 20.12–26.04 changelogs contain zero redaction; open wishlist bugs 489299 (2024) and 452403 (2022) show demand without implementation. The documented community recipe is black-highlighter + Ghostscript flatten.
**Implication:** Against the KDE free stack, GlyphPDF's redaction (excision + transactions + sanitize + pattern marking) has no competitor feature to match — it is differentiation, not parity. Against the *market*, Okular's users are demonstrably asking for exactly what GlyphPDF already ships.
**Recommendation:** Keep U05 hardening on the roadmap; add a "why free tools fail at redaction" capability-registry disclosure (Okular's own bug tracker provides the citation).

### Finding 2: Okular's save model is architechitecturally antithetical to GlyphPDF's safe-save.
Docbook verbatim: saves are not byte-identical even with no changes; annotations default to a sidecar archive; custom stamps are admitted to be invisible in other readers.
**Implication:** Okular optimizes for reviewer convenience and accepts silent file mutation; GlyphPDF's SHA-256-invariance-tested SafeSave (U05) targets the compliance buyer.
**Recommendation:** Position copy: "Okular tells you in its own manual that a saved file is a different file. GlyphPDF proves source invariance in CI."

### Finding 3: The annotation loop (keyboard-first, configurable, cross-format) is the thing to copy.
Keys 1–9 + Alt-9…0 quick tools, continuous-mode pin, per-tool defaults, tool palette editor, LaTeX math, .okular archive for review handoff.
**Implication:** Annotation throughput is UI-layer and cheap to absorb; it directly serves GlyphPDF Persona 1/2 (students, office workers) journeys.
**Recommendation:** Port the quick-annotation strip + per-tool continuous pin into GlyphPDF's annotation mode; make annotation tool palettes user-editable (stores as presets, matching the named-redaction-preset pattern from §9.12-a).

### Finding 4: Signing is real but pre-PAdES, NSS-bound, and Windows-hostile.
Signing since ~21.04–21.12 via Poppler/NSS; reason/location + background image (23.08); UX rework (24.12); no PAdES/LTV/DSS/timestamps (source-verified); certificate access via Firefox/NSS stores only.
**Implication:** GlyphPDF's B-T/B-LT + DSS + ETSI appearance stack has no free-KDE rival; but Okular's five years of Poppler interop set the bar for "verifies real-world signatures without complaint."
**Recommendation:** InGlyphPDF: add the OCSP-offline toggle pattern to the security settings; keep surfacing SignOutcome degradation wording (22a7b66) as a differentiator.

### Finding 5: What Okular users ask for is GlyphPDF's feature list.
Edit text ("A way edit the document would be great"; LibreOffice Draw referrals), OCR (Ocrad request; r/kde 2023 thread), page manipulation/merge (KDE Discuss #1298), redaction (489299), form font-size control, stamp interop fix.
**Implication:** The KDE audience's unmet needs map 1:1 onto PRD §9.2/§9.4/§9.9/§9.8 — evidence of demand from a live, vocally frustrated user base.
**Recommendation:** Marketing should name these workflows; positioning can borrow Okular's trust halo ("everything KDE's viewer can't edit, done safely and offline").

---

## 3. Compact summary (as requested)

**Top 5 feature gaps in Okular (vs GlyphPDF):**
1. **Redaction — none at all** (FALSE premise; open bugs 489299/452403; workaround leaves text extractable). TRUE.
2. **Content editing — none** (annotation-only; Typewriter overlay is the closest; users routed to LibreOffice Draw). TRUE.
3. **OCR — none** (zero source presence; Ocrad wishlist bug). TRUE.
4. **Page management — none** (no merge/split/reorder/delete; view-rotation only; Discuss #1298). TRUE.
5. **PAdES/LTV/timestamps — none** (generic PKCS#7 via NSS only; no DSS augmentation; live-revocation-only validation). TRUE.

**Top 3 failure modes (real-world):**
1. **Silent mutation on save:** Save/Save As is explicitly not byte-stable (docbook), full rewrite; combined with the default sidecar annotation model, users lose annotations by closing without saving, or share files believing black highlighter = redaction. TRUE/MOSTLY_TRUE.
2. **Windows channel friction:** Store version lags stable, old versions unavailable, extension associations reset after updates (open bug), nightly .exe channel untested. MOSTLY_TRUE.
3. **Stamp/annotation interop:** custom stamps not visible in other PDF readers (docbook-admitted experimental defect); resize restricted to 4 annotation types; complex form-JS forms misbehave (partial script dispatch). MOSTLY_TRUE.

**Top 3 loved workflows:**
1. **Keyboard-speed reviewing:** F6 toolbar, keys 1–9, Alt-9…0 quick annotations, continuous-mode pin — the fastest markup loop in the free tier. TRUE.
2. **Academic reading:** LaTeX-rendering annotations, table-selection copy, inverse search (synctex/pdfsync), Overview mode, trim view, back/forward history. TRUE.
3. **Archive-based review collaboration:** .okular document archive shares original + annotations with other Okular users without touching the source file. TRUE.

**Single biggest opportunity for GlyphPDF:** Position as the "edit/safe-redact/offline-sign" completion of the Okular workflow — the exact five capabilities Okular's own tracker shows users begging for (edit text, OCR, redact, page surgery, PAdES signing) are all shipped in GlyphPDF with transactional safe-save guarantees Okular's architecture cannot promise. Secondary absorb: copy the quick-annotation keyboard loop and the OCSP-offline toggle verbatim; both are cheap and visible.

---

## 4. Confidence level

**High (0.85+)** for absence claims (redaction, OCR, editing, compare, batch, PAdES/LTV, XFA) — each verified against the complete current source tree, not docs or reviews. **High** for annotation/form/signature behavior — handbook (dated 2026-02-23, version 26.04) read in full plus per-commit changelogs. **Medium** for Windows Store price/port community claims (Store page is JS-rendered; price graded from the listing's own crawl title, free, MOSTLY_TRUE) and for Android distribution specifics (in-tree QML app + NLnet statement, Play Store listing not independently verified). The historical signature-verification introduction version is pinned only to Poppler-gated tiers (0.51/0.68/0.73) rather than an exact Okular release — graded accordingly.

## Sources

Primary:
- https://okular.kde.org/ (homepage: version news block 24.12–26.04; feature claims; Blue Angel)
- https://okular.kde.org/download/ (platform matrix, Store recommendation, nightly installers)
- https://okular.kde.org/formats/ (backend/format capability table)
- https://okular.kde.org/faq/ (EPub/Ubuntu packaging, speech service, poppler-data)
- Okular handbook, current master docbook (handbook date 2026-02-23, ver 26.04): https://raw.githubusercontent.com/KDE/okular/master/doc/index.docbook (via GitHub mirror)
- Okular master source tree (grep corpus; mirror pushed 2026-09-07): https://github.com/KDE/okular (formfields.cpp, generator_pdf.cpp, formwidgets.cpp, pdfsignatureutils.*, conf/okular.kcfg, mobile/app/ui/Signatures.qml)
- KDE Gear announcements: https://kde.org/announcements/gear/22.04.0/ (welcome screen, no-certificate alert, Blue Angel first), https://kde.org/announcements/gear/23.08.0/ (signature metadata/background image, print scaling, copy annotation text)
- KDE Gear full changelogs 20.12/21.04/21.08/21.12/22.04/22.08/23.04/24.12/25.04/25.08/26.04: https://kde.org/announcements/changelogs/gear/<ver>/ (signing fixes, OCSP opt-out, quick-annotation minibar, signature UX rework #443403, form event ordering, stamp scaling #370382)
- KDE Bugzilla: https://bugs.kde.org/show_bug.cgi?id=489299 (Add redaction Tool — verbatim workaround), 452403 (sanitize+redact request); quicksearch exports for OCR/edit-text/Windows
- NLnet Foundation: https://nlnet.nl/project/Okular/ (funded signature improvement project; platform statement incl. Android)
- Wikipedia: https://en.wikipedia.org/wiki/Okular (Windows Store since Sept 2019, Store-lag note, annotation persistence since 0.15/Poppler 0.20, infobox 26.04.0)
- Microsoft Store listing: https://apps.microsoft.com/detail/9n41msq1wnm8 ("Free download and install on Windows" crawl title)

Secondary (community, corroboration only):
- https://gregbur.me/2022/04/28/deep-dive-digitally-signing-pdfs-with-okular/ (NSS certutil walkthrough; config difficulty)
- https://www.reddit.com/r/kde/comments/183qxsk/ (no OCR/editing), https://www.reddit.com/r/linuxquestions/comments/ty9rwx/okular_without_ocr/, https://www.reddit.com/r/kde/comments/wa2rfn/ (form-filling limits), https://www.reddit.com/r/linux/comments/g8bcsm/ (LibreOffice Draw for editing)
- https://discuss.kde.org/t/feature-request-modifying-pdf-pages-with-okular/1298 (page manipulation request)
- https://bugs.kde.org/show_bug.cgi?id=443403 (signature display font size, 23.08 improvements)
