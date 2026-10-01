"""Generate the GlyphPDF guides section (static HTML, one skeleton for all guides).

Run from the worktree root:  python docs/guides/_generate_guides.py
 Writes: docs/guides/index.html + one page per guide. Idempotent.
"""
import io
import os

DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)))

HEADER = """<a class="skip-link" href="#main">Skip to content</a>
  <header class="site-header">
    <div class="wrap header-row">
      <a class="brand" href="{root}index.html"><span class="g">Glyph</span>PDF</a>
      <nav class="site-nav" aria-label="Main">
        <ul>
          <li><a href="{root}index.html">Home</a></li>
          <li><a href="{root}features/">Features</a></li>
          <li><a href="{guides_index}"{cur_guides}>Guides</a></li>
          <li><a href="{root}download/">Download</a></li>
          <li><a href="{root}security/">Security</a></li>
        </ul>
      </nav>
    </div>
  </header>"""

FOOTER = """<footer class="site-footer">
    <div class="wrap">
      <p>GlyphPDF is open source under <a href="https://github.com/eliets2/glyph-pdf/blob/main/LICENSE">Apache-2.0</a> &middot; <a href="https://github.com/eliets2/glyph-pdf">github.com/eliets2/glyph-pdf</a></p>
      <p class="fine">No telemetry. No cloud. Everything runs on your machine.</p>
    </div>
  </footer>"""

BANNER = """<div class="notice" role="note"><strong>Work in progress.</strong> This guide is being prepared for the upcoming redesigned interface. The steps below are placeholders and will be completed when the new UI ships.</div>"""


def chrome(root, guides_index, cur_guides):
    return HEADER.format(root=root, guides_index=guides_index, cur_guides=cur_guides)


def guide_page(g):
    root = "../"
    steps = "\n".join(
        f'        <li><strong>Step {i + 1}</strong> &mdash; [to be captured from the redesigned UI: {hint}]</li>'
        for i, hint in enumerate(g["steps"]))
    tips = "\n".join(f'        <li>{t}</li>' for t in g["tips"])
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{g['title']} — GlyphPDF Guides</title>
  <meta name="description" content="{g['meta']}">
  <link rel="stylesheet" href="../assets/site.css">
</head>
<body>
  {chrome(root, "index.html", ' aria-current="page"')}
  <main id="main">
    <div class="wrap page">
      {BANNER}
      <h1 class="page-title">{g['title']}</h1>
      <p class="lede">{g['lede']}</p>
      <p class="status-line">Status: parity scorecard {g['sec']} verified {g['score']}/10. Claims sourced from README.md, CHANGELOG.md and docs/audit/PARITY-SCORECARD-2026-09-30.md.</p>

      <h2>How to</h2>
      <ol class="steps" role="list">
{steps}
      </ol>

      <figure class="shot pending">
        <div class="shot-box" aria-hidden="true">Screenshot placeholder &mdash; {g['shot']}</div>
        <figcaption>Screenshot pending &mdash; will be captured from the redesigned interface.</figcaption>
      </figure>

      <h2>Tips</h2>
      <ul class="tips" role="list">
{tips}
      </ul>

      <p><a href="index.html">&larr; All guides</a> &middot; <a href="../features/#{g['anchor']}">Feature details</a> &middot; <a href="../download/">Download GlyphPDF</a></p>
    </div>
  </main>
  {FOOTER}
</body>
</html>
"""


def guides_index(guides):
    root = ""
    cards = []
    for g in guides:
        cards.append(f"""        <section class="card">
          <h3><a href="{g['slug']}.html">{g['title']}</a></h3>
          <p>{g['short']}</p>
          <a class="more" href="{g['slug']}.html">Read the guide &rarr;</a>
        </section>""")
    cards_html = "\n".join(cards)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Guides — GlyphPDF</title>
  <meta name="description" content="GlyphPDF guides: viewing, editing, annotations, OCR, conversion, forms, e-signatures, redaction, page management, compare, security, batch, compression, accessibility and search.">
  <link rel="stylesheet" href="../assets/site.css">
</head>
<body>
  {chrome("../", "index.html", ' aria-current="page"')}
  <main id="main">
    <div class="wrap page">
      {BANNER}
      <h1 class="page-title">Guides</h1>
      <p class="lede">One guide per major area of GlyphPDF. Each one explains what the feature does today; the step-by-step walkthroughs and screenshots are placeholders that will be completed once the redesigned interface ships.</p>

      <div class="grid">
{cards_html}
      </div>

      <p><a href="../features/">Feature details &rarr;</a> &middot; <a href="../download/">Download GlyphPDF</a></p>
    </div>
  </main>
  {FOOTER}
</body>
</html>
"""


GUIDES = [
    dict(slug="viewing-navigation", title="Viewing & Navigation", anchor="viewing", sec="§9.1", score="9.0",
         meta="Single/Continuous/Two-Page layouts, Presentation mode, rotation, hyperlinks, Night Mode and reading modes in GlyphPDF.",
         short="Layouts, zoom, rotation, hyperlink navigation, Night Mode and reading modes.",
         lede="GlyphPDF's viewer offers Single, Continuous and Two-Page layouts plus Presentation and Full Screen modes, engine-side page rotation, real hyperlink navigation (with a scheme allow-list so unsafe links are never opened), thumbnail navigation, Night Mode RGB inversion, and Dark/Light/High-Contrast themes with Eye Care and Night reading modes. Every view mode is characterized by the TestViewParity suite.",
         shot="the document viewer in Two-Page layout with the Night Mode toggle visible",
         steps=["open a document and switch between Single, Continuous and Two-Page layouts (ribbon tab and buttons to be confirmed)",
                "rotate a page and confirm the rotation is engine-side /Rotate, not just a view flip",
                "click an internal GoTo link and an external http(s) link to see navigation and the scheme allow-list",
                "toggle Night Mode / Eye Care and switch themes from Preferences",
                "enter Presentation or Full Screen mode and exit again"],
         tips=["Alt+Left / Alt+Right navigate back / forward; Ctrl+0 restores actual size; Ctrl++ / Ctrl+- zoom (README keyboard table).",
               "F11 toggles Full Screen; F1 opens the keyboard-shortcuts help at any time.",
               "Two-page mode composites annotations and search highlights onto the spread, so markup stays visible in book view."]),

    dict(slug="text-object-editing", title="Text & Object Editing", anchor="editing", sec="§9.2", score="8.5",
         meta="Inline text edits with spacing and opacity, image move/resize/rotate/restack, clipboard editing and the eraser in GlyphPDF.",
         short="Inline text edits, image move/resize/rotate/restack, clipboard and eraser tools.",
         lede="The Edit tools change the document itself: inline text editing with letter spacing, line spacing and opacity controls, plus image manipulation — move, resize, rotate by any angle, replace, delete, restack (bring to front / send to back) and opacity — all written into the PDF as real content-stream changes, with undo.",
         shot="the Edit toolbar with the inline text style controls (spacing, opacity) in use",
         steps=["enter text-edit mode on a page and change a text run's letter spacing, line spacing and opacity",
                "select an image and move, resize, and rotate it by a custom angle",
                "use the image context menu to replace an image, delete one, and restack another to front/back",
                "set image opacity from the context menu",
                "try Cut / Copy / Delete on a selection and the eraser tool on an object (in-document paste is scoped out; Copy places a snapshot on the clipboard)",
                "undo the edits with Ctrl+Z and confirm the page returns to its prior state"],
         tips=["Every image edit is an undoable command backed by a restorable page backup.",
               "The eraser works through the real object-deletion pipeline — it removes content, not pixels.",
               "Text edits currently undo via page snapshots; finer-grained undo is a named open item in the scorecard."]),

    dict(slug="annotations-markup", title="Annotations & Markup", anchor="annotations", sec="§9.3", score="8.5",
         meta="13-tool annotation surface: highlights, notes, shapes, ink, callouts and a dynamic stamp library in GlyphPDF.",
         short="Highlights, underlines, notes, shapes, pencil/ink, callouts and the dynamic stamp library.",
         lede="All markup lives on one surface — the ribbon Comment tab — with 13 tools: highlight, underline, strikeout, squiggly, notes, shapes (square, circle, line/arrow), freehand ink, callouts and stamps. Shapes and freehand persist as real PDF annotation subtypes, so they round-trip with other PDF applications. The stamp library ships built-ins (Approved, Draft, Confidential, Received, Reviewed) and supports custom stamps with dynamic placeholders substituted at apply time.",
         shot="the ribbon Comment tab with the markup tools and the stamp library dialog",
         steps=["open the Comment tab and identify the 13 markup tools",
                "add a highlight, an underline and a squiggly to text (note: they persist as drag rectangles, not QuadPoints)",
                "draw a rectangle, an ellipse and an arrow; then annotate freehand with the pencil",
                "add a note and a callout; open the Comments list to filter and review them",
                "open the stamp library, apply a built-in stamp and create a custom stamp with ${author}/${date} placeholders",
                "save and reopen to confirm annotations persist"],
         tips=["Comments have a filter, a table view and CSV export on the Review side (README: Review & Compare).",
               "Known gap, tracked in the scorecard: text-anchored markup (QuadPoints) is not implemented yet — highlights cover the dragged rectangle.",
               "Image-as-stamp import is not available yet; the signature Upload mode is the only image-to-/Stamp path today."]),

    dict(slug="ocr", title="OCR", anchor="ocr", sec="§9.4", score="8.5",
         meta="Dual-engine OCR (Tesseract + RapidOCR PP-OCRv5) with ROVER fusion, word-level review and searchable MRC PDF/A output.",
         short="Dual-engine OCR with word-level review, confidence overlay and searchable PDF/A output.",
         lede="GlyphPDF runs Tesseract and RapidOCR PP-OCRv5 in parallel over a layout-detected, preprocessed image (deskew, binarize, denoise, 0/90/180/270 orientation detection), fuses the engines' output per word with confidence-weighted ROVER, and shows a review screen with per-word confidence before you accept. Accepting exports a searchable MRC PDF/A copy with an invisible text layer. Twelve OCR languages are selectable and shared with batch OCR.",
         shot="the OCR review screen with the per-word confidence overlay and language combo",
         steps=["open a scanned document and enter the OCR mode (entry point to be confirmed from the redesigned UI)",
                "pick the OCR language from the combo and choose preprocessing options (deskew, binarize, denoise; orientation detection)",
                "run OCR and inspect the word-level review screen with its confidence overlay",
                "correct or accept reviewed words; test the Re-OCR entry (it currently acts on the whole page and says so)",
                "Accept and save the searchable MRC PDF/A output",
                "verify the saved copy is text-searchable"],
         tips=["OCR output feeds the MRC compression pipeline — Accept produces the PDF/A sandwich described in the Compression guide.",
               "Low-confidence words are flagged; in batch they are summarized per file.",
               "4 GB RAM is recommended for OCR on large documents (README: system requirements)."]),

    dict(slug="conversion-export", title="Conversion & Export", anchor="conversion", sec="§9.5 + §9.16", score="8.5",
         meta="PDF to Word, Excel, HTML, images, CSV and text — plus PDF/A, Web Optimized and Legal Archive export presets.",
         short="PDF to Word/Excel/HTML/images/CSV/text, plus PDF/A, Web-optimized and Legal Archive export presets.",
         lede="Convert PDFs to Word (.docx), Excel (.xlsx with real table columns), HTML, images, CSV and text using real OOXML writers, with a runtime badge of which engine actually ran and an honest fallback warning if a format degrades. Export presets cover High Quality PDF/A, Web Optimized (linearized via qpdf and verified) and Legal Archive. Every export carries a local-processing notice: no internet, no upload.",
         shot="the Convert panel with format choices and the export-presets panel beside it",
         steps=["open a PDF and open the Convert surface (tab location to be confirmed from the redesigned UI)",
                "export to Word and open the .docx in an Office application to check the result",
                "export to Excel and confirm table columns arrive as columns",
                "export to HTML, images, CSV and text",
                "open the Export presets panel; create or edit a preset and save with High Quality PDF/A and Web Optimized to compare",
                "note the local-processing notice and the export-engine badge in the completion message"],
         tips=["Office documents (.docx/.xlsx/.pptx/.odt) can be opened or dropped directly — optional LibreOffice import is auto-detected with an in-app download prompt if absent.",
               "PDF/A validation uses veraPDF as a subprocess (never linked in-process); the app prompts to fetch it on first use.",
               "Batch conversion covers Word/Excel/HTML/Image/Csv today — Text and PowerPoint in batch are a named open item."]),

    dict(slug="forms", title="Forms", anchor="forms", sec="§9.6", score="8.0",
         meta="AcroForm fields, fill vs fill-and-lock, auto-detect with review, and sandboxed form JavaScript in GlyphPDF.",
         short="Create, fill, lock and auto-detect AcroForm fields; sandboxed Calculate/Format/Keystroke/Validate scripts.",
         lede="GlyphPDF builds and fills AcroForm fields: text, checkboxes, radio buttons, dropdowns and date/numeric/calculated fields. Fill can leave fields editable or lock them; unsupported fields are reported in an explicit Import Incomplete dialog rather than dropped silently. Auto-detect proposes field placements from label patterns with a disclosed heuristic and review-before-commit. AcroForm Calculate, Format, Keystroke and Validate scripts run in a sandboxed quickjs-ng runtime with a CPU deadline.",
         shot="the Forms ribbon with field tools and the auto-detect preview dialog",
         steps=["open a PDF and enter the Forms builder (entry point to be confirmed from the redesigned UI)",
                "place a text field, set its tooltip and Required flag, and confirm they persist after save/reopen",
                "use Detect (auto-detect) on a label-style page; review the disclosed heuristic suggestions and commit (or undo) them",
                "fill a form from imported data, first without lock and then with fill-and-lock, and check the Import Incomplete report for unsupported fields",
                "open a form with scripts and confirm Calculate/Format behave inside the sandbox",
                "set tab order across fields"],
         tips=["A failed form write leaves the source PDF byte-identical, and field edits undo to the original value/tooltip/required state.",
               "Auto-detected fields join one compound undo — one Ctrl+Z removes a whole detection run.",
               "Known gap, tracked in the scorecard: a Digital Signature field still maps to a Text field; CSV/FDF import parsing is basic."]),

    dict(slug="e-signatures", title="E-Signatures", anchor="esignatures", sec="§9.7", score="9.0",
         meta="PAdES B-LT/B-LTA signing with visible appearances, validity badges, Validate-All and Prepare Request in GlyphPDF.",
         short="PAdES B-LT/B-LTA signing, Draw/Type/Upload/Initials picker, visible appearances, badges and Prepare Request.",
         lede="Sign documents digitally with PAdES B-LT/B-LTA signatures (DSS/VRI embedding, TSA timestamps, a certify selector) and a visible ETSI-layout signature appearance with identity, date, reason and location lines that auto-fits the box. A Draw/Type/Upload/Initials picker places each signature as a real PDF annotation. On-page validity badges anchor to the actual signature fields — including in Two-Page mode — and Validate-All summarizes every signature's state. Protect > Sign > Prepare Request packages a document for other signers.",
         shot="the signature picker (Draw/Type/Upload/Initials) and a signed page with validity badges",
         steps=["open the Sign flow from the Protect surface (exact ribbon path to be confirmed from the redesigned UI)",
                "create a signature with the picker: Draw, Type, Upload and Initials tabs",
                "place the signature and fill the visible-appearance fields (reason, location); confirm it lands inside the signed revision",
                "choose the signature level (B-LT/B-LTA) and certify or approve; note the consent gate for network (OCSP) checks",
                "validate: read the on-page badges and run Validate-All for the summary",
                "try Protect > Sign > Prepare Request to stage a signing request for another signer"],
         tips=["SHA-256 is the only signature hash; keys below 2048 bits are rejected before signing.",
               "OCSP/CRL network access is consent-gated: a never-network policy still signs, and refusals are named honestly.",
               "The session signature cache follows document switches, so a second placement reuses your signature."]),

    dict(slug="redaction", title="Redaction", anchor="redaction", sec="§9.8", score="9.0",
         meta="True content-excision redaction with Mark All, word lists, PII presets, overlay text and a sanitize-on-save bundle.",
         short="Content-stream excision redaction — never black rectangles — with patterns, word lists and proof steps.",
         lede="Redaction removes text from the content stream itself — never a black-painted overlay — so redacted content is genuinely gone. Mark Region and Mark All work in-mode with explicit page lists (an invalid range marks nothing rather than silently marking everything), a word list can be imported to build escaped pattern alternations, and named PII presets cover email, US phone and SSN. Applying runs as one transaction with a SHA-256 check that the source is unchanged, offers overlay text burn-in, and can bundle a sanitize pass (default ON) into the saved copy. A proof step refuses to certify what it cannot verify.",
         shot="the Redact mode panel with marks placed and the Apply dialog (overlay text + sanitize checkbox)",
         steps=["enter Redact mode from the Protect surface (entry point to be confirmed from the redesigned UI)",
                "mark a region on one page, then use Mark All with an explicit page range and watch an invalid range mark nothing",
                "import a word list (.txt) and review the marks it proposes before applying",
                "open the Apply dialog: add overlay text and reason, keep Sanitize on save checked (default ON)",
                "apply and confirm the output: the text is excised from the content stream, not painted over",
                "test Clear Marks and Cancel/Back — marks are kept and the tool disarms"],
         tips=["The redaction audit log (opt-in) records only the region count and an after-hash — coordinates and pre-image hashes are deliberately not stored, so the log cannot fingerprint the original.",
               "Find & Replace has a redact-all option that shares the same matcher as document search.",
               "Signed documents refuse redaction edits — the guard is tested (TestEraseSignedGuard family)."]),

    dict(slug="page-management", title="Page Management", anchor="pages", sec="§9.9", score="8.5",
         meta="Rotate, crop, resize, reorder, insert, extract, split, merge, headers/footers, Bates numbering and watermarks.",
         short="Thumbnail-grid reordering, split/merge/extract, headers & footers, Bates numbering and watermarks.",
         lede="The Pages surface handles document structure: rotate, crop, resize, reorder by dragging thumbnails on the grid, insert, extract, split by ranges into numbered part files (_part1, _part2, ...), and merge with real success/failure reporting. Headers, footers and page numbers can be stamped on, Bates numbering runs (including across documents in batch), and text or image watermarks apply with real font metrics.",
         shot="the Pages mode thumbnail grid mid-drag with the Organize ribbon group visible",
         steps=["open the Pages mode and drag thumbnails to reorder; undo with Ctrl+Z (entry point to be confirmed from the redesigned UI)",
                "rotate and crop a page; resize a page to a different paper size",
                "split by ranges such as 1-3,4-6 and confirm the numbered part files are written",
                "merge two documents and read the success/failure status honestly reported",
                "add a header/footer with page numbers; then apply a Bates numbering run",
                "apply a text watermark and an image watermark"],
         tips=["Reorder is one atomic permutation command — undo restores the whole arrangement at once.",
               "Bates numbering continues across files in a batch run with truthful failure semantics (a fault at file two keeps file one committed).",
               "Page Labels (Roman/section styles) have groundwork in place but the writer/UI is a named open item."]),

    dict(slug="document-compare", title="Document Compare", anchor="compare", sec="§9.10", score="9.0",
         meta="Two-document compare with page alignment, change-type filters, structural rows for added/removed pages, and reports.",
         short="Side-by-side document comparison with change filters and added/removed-page rows.",
         lede="Compare Documents picks two files and diffs them: a changes tree with change-type filters gates both text-diff panes, added and removed pages appear as explicit structural rows, and page alignment uses deterministic page fingerprints so mid-document insertions align correctly instead of cascading. Status totals, navigation and reports round out the review.",
         shot="the Compare view with the changes tree, filters and side-by-side panes",
         steps=["open Compare Documents from the menu (entry point to be confirmed from the redesigned UI)",
                "pick two versions of the same document and run the comparison",
                "toggle the change-type filters and watch the tree and panes update without mutating the result",
                "click a structural added/removed-page row and confirm navigation lands on it",
                "produce a compare report"],
         tips=["Ordinary text edits no longer masquerade as page removal + addition — alignment is fingerprint-based.",
               "Comparisons run locally; two contract drafts never leave the machine.",
               "Known gap, tracked in the scorecard: long comparisons are synchronous with no progress/cancel yet."]),

    dict(slug="security-encryption", title="Security & Encryption", anchor="security-signing", sec="§9.11", score="8.5",
         meta="AES-256 and certificate encryption, sanitization, expiry dates and admin machine policy in GlyphPDF.",
         short="Password/certificate encryption, document sanitization, Set Expiry Date and machine policy.",
         lede="Protect documents with AES-256 password encryption or certificate-based encryption with a multi-recipient picker. Sanitize Document removes 15+ metadata and hidden-data vectors. Set Expiry Date writes an expiry marker through a SafeSave transaction (with XMP survival) and, once expired, the whole mutating tool set — encrypt, sign, sanitize, redact-apply, permissions — flips to read-only, while Save-As/export/print stay available as a deliberate escape hatch. Administrator machine policy shows managed settings as \u201cManaged by policy\u201d.",
         shot="the Protect ribbon with Encryption, Sanitize and Set Expiry Date entries",
         steps=["open a PDF and apply password encryption (AES-256) from the Protect surface (entry point to be confirmed from the redesigned UI)",
                "apply certificate-based encryption and use the recipient picker for multiple recipients",
                "run Sanitize Document and reopen the output to see what was removed",
                "set an expiry date, save, reopen after the date, and confirm the mutating tools are gated read-only",
                "check Preferences for a policy-managed setting if your machine has one"],
         tips=["Sanitize is all-or-nothing today; a selective mode with a pre-commit summary is a named open item in the scorecard.",
               "Expiry writes are transactional — a second expiry write can no longer corrupt the file (v1.5.0 fix, pinned by tests).",
               "See the Security page on this site for the offline posture and how to verify downloads."]),

    dict(slug="batch-hot-folder", title="Batch & Hot-Folder", anchor="batch", sec="§9.12", score="8.5",
         meta="Batch presets: Bates, redaction, OCR, merge, compress, export — plus hot-folder ingest and a multi-step editor.",
         short="Multi-step batch presets, hot-folder ingest, per-step reports and truthful failure handling.",
         lede="Batch processing runs named presets — Bates numbering, redaction (including multi-pattern in a single load/find/apply/save pass), OCR with language choice and low-confidence notes, merge, compression with DPI presets, and export — chained by a multi-step preset editor with import/export of preset definitions, rename-on-conflict, stop-on-failure and a per-step measured-bytes report. Hot-folder ingest watches a directory and processes incoming files.",
         shot="the Batch mode with a multi-step preset open in the editor",
         steps=["open Batch mode (entry point to be confirmed from the redesigned UI)",
                "add files and run a single operation — merge or convert — and read the inline per-file errors",
                "open the preset manager: run the Bates lane across several documents and confirm numbering continues across files",
                "build a multi-step preset (for example OCR then export) with the editor; export and re-import the preset definition",
                "configure hot-folder ingest on a test directory and drop a file in",
                "deliberately cause a conflict to see rename-on-conflict and the explicit overwrite prompt"],
         tips=["The processed-file report measures real bytes per step, not estimates.",
               "Known gaps, tracked in the scorecard: hot-folder watching is non-recursive with no polling fallback, and batch lacks Text and PowerPoint targets.",
               "OCR in batch shares the language setting and flags low-confidence words per file in its notes."]),

    dict(slug="compression", title="Compression", anchor="compression", sec="§9.13", score="8.0",
         meta="JPEG re-encoding, image dedup, object sweep, MRC layered compression and measured before/after readouts.",
         short="Real image re-encoding, dedup, MRC layered compression and honest measured size readouts.",
         lede="Compress does real work and reports real numbers: JPEG images are decoded and re-encoded honoring your quality and DPI settings, duplicate images are deduplicated (SMask-aware) and rewired to one canonical object, unused objects are swept, and metadata stripping runs the full sanitize logic. The completion dialog reads both sizes from disk and shows the measured delta — estimates are labeled as estimates and never claim passes that did not run. The MRC layered pipeline (JBIG2 foreground + JPEG2000 background + invisible OCR text) measured 30.4× on an A4 scanned test page.",
         shot="the Compress dialog with quality/DPI controls and the measured before/after report",
         steps=["open Compress on a large PDF (entry point to be confirmed from the redesigned UI)",
                "choose a DPI preset and quality level, then run compression",
                "read the measured before/after sizes and delta in the completion report",
                "re-run with metadata strip enabled and confirm the sanitize behavior",
                "run the MRC pipeline on a scanned page (Off / Lossless / Balanced / Aggressive modes in the dialog)"],
         tips=["If the result is not smaller, the dialog says so instead of hiding it.",
               "Font subsetting is deliberately disabled with an explanation — no subsetter exists in this build yet.",
               "Signed documents refuse compression edits, and CMYK JPEGs are deliberately skipped to avoid color shifts."]),

    dict(slug="accessibility-checker", title="Accessibility Checker", anchor="accessibility", sec="§9.14", score="8.5",
         meta="Reading-order and structure checks with jump-to-issue rows, plus Tag Document for untagged PDFs.",
         short="Accessibility checker with jump-to-issue rows and Tag Document structure building.",
         lede="The accessibility checker analyzes reading order (honoring ISO 32000-2 §14.7.2 /Pg inheritance), runs off the UI thread, and lists issues with jump buttons that take you to the page. Tag Document builds a structure tree for untagged documents while preserving images and marked content, and PDF/A validation uses veraPDF (as a subprocess) with per-issue rows in the same panel.",
         shot="the accessibility/validation panel with checker rows and JUMP buttons",
         steps=["open the Accessibility / PDF-A panel (entry point to be confirmed from the redesigned UI)",
                "run the accessibility checker on an untagged document and read the issue rows",
                "use a JUMP button and confirm it lands on the flagged page",
                "run Tag Document and reopen the file to confirm the structure tree was written",
                "run a PDF/A validation pass (veraPDF is offered for download on first use)"],
         tips=["Reading-order tolerance is a named, documented constant — a displacement of two positions is not flagged, three is.",
               "The checker runs asynchronously, so the UI stays responsive on large documents.",
               "Known gaps, tracked in the scorecard: MCID-span extraction and an exportable results panel are still open."]),

    dict(slug="search", title="Search", anchor="search", sec="§9.15", score="8.5",
         meta="Document search with Match Case, Whole Words and Regex; shared matcher with redact-all; bookmarks and thumbnails.",
         short="Match Case / Whole Words / Regex search, bookmarks, thumbnails and keyboard navigation.",
         lede="Document search supports Match Case, Whole Words and Regular Expression through one shared matcher that also powers redact-all. When flags are set, navigation is page-level with an explicit inline note that highlighting is unavailable — an honest limitation, not a silent one. Bookmarks, a thumbnail sidebar with zoom controls, and full keyboard access (F6 region cycling, F1 help) complete navigation.",
         shot="the find bar with search flags open and matches marked on a page",
         steps=["press Ctrl+F and search a document (bar placement to be confirmed from the redesigned UI)",
                "enable Match Case and Whole Words and compare the result sets",
                "try a regular expression, then an invalid one, and read the reported error",
                "use Ctrl+H Find & Replace and locate the redact-all option",
                "navigate with bookmarks and the thumbnail sidebar's zoom controls"],
         tips=["Flag-filtered search navigates at page level today — the status line tells you inline highlight is unavailable.",
               "The same matcher drives redact-all, so a regex you trust in search behaves identically in redaction.",
               "F6 cycles UI regions and F1 lists every shortcut — no mouse required."]),
]


def main():
    os.makedirs(DOCS, exist_ok=True)
    for g in GUIDES:
        path = os.path.join(DOCS, g["slug"] + ".html")
        with io.open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(guide_page(g))
        print("wrote", path)
    idx = os.path.join(DOCS, "index.html")
    with io.open(idx, "w", encoding="utf-8", newline="\n") as f:
        f.write(guides_index(GUIDES))
    print("wrote", idx)


if __name__ == "__main__":
    main()
