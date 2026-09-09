# Consolidated Research Backlog — 2026-09-10

One deduplicated, ranked implementation backlog mined from the entire competitor-research corpus
(19 sources in `docs/research/`), cross-checked against the feature-command matrix
(`docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv` + notes) and the evidence ledger
(`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, newest sections first: gate fixes G01–G23,
T1 measurement lane, T2 redaction-proof lane, the three implementation plans).

**Method.** Every `rec #N` list, per-domain delta line ("GlyphPDF MISSING/PARTIAL"), loved-UX
detail, failure-mode warning, and r/pdf demand cluster in all 16 competitor sheets +
r-pdf-mining + synthesis was extracted, deduplicated against the ledger/matrix (an idea already
shipped or queued is marked, never re-proposed), then ranked by (demand evidence × leverage ÷
size), seeded by each report's own recommendation ranking. Strategy line respected per
`synthesis.md` (local-only, zero-egress, honesty surfaces): every cloud-gated idea lands in §4
REJECTED, not the backlog.

**Status vocabulary.** `SHIPPED-<ref>` (ledger-verified, incl. implemented-awaiting-review),
`QUEUED-PACK-A/B/C` (the three signed design docs), `QUEUED-T2-N` (synthesis Tier-2 packs
already in the plan of record — excluded from NEW per the tasking), `QUEUED-MATRIX` (hidden
ribbon rows with a traced wire target, matrix notes §A), `NEW` (proposed here for the first
time), `REJECTED` (§4, with the demand data that proves rejecting is right).
Sizes: **S** = 1–3 h (tonight-feasible), **M** = 1–3 days, **L** = multi-day.
`synthesis T2-x/T3-x` = the capability's row in synthesis.md §2/§3.

---

## 0. Load-bearing findings (change the plan of record)

1. **T3-1 certificate encryption is engine-DONE, UI-only.** Synthesis T3-1 and acrobat.md's
   delta grade it MISSING; the send-for-signing plan (§1.1, code-verified) shows
   `IEncryptor::encryptWithCertificate` + ER-3 CMS-recipient counter already exist
   (`src/core/interfaces/IPdfEditorEngine.h:264–275`). Remaining work is a recipient-picker UI
   over a tested seam → reclassified NEW-M (row N17), not a crypto project.
2. **T2-7 DocMDP certify-vs-approve is engine-DONE, UI-only.** `certifyDocument` →
   `signDocumentImpl(certLevel)` writes /DocMDP levels 1–3 fail-loud (send-for-signing plan
   §1.1; `ISignatureManager.h:75`). `SignatureDialog` has no certify/level selector →
   reclassified NEW-M (row N18).
3. **The shipped product signs effectively B-B only, and "Timestamp document" is a dead
   command.** `setTsaUrl`/`setSignatureLevel` have zero production call sites (grep-verified in
   the send-for-signing plan §1.2); the B-T branch skips on empty TSA URL. Consequence: do not
   market B-LT/B-LTA as a *running* capability until QUEUED-PACK-C P2/P3 light it (honesty moat
   M8 applies to us).
4. **QUEUED-PACK-A's P0 prerequisite is already satisfied.** The batch-presets plan gates P1 on
   the BatchMode queued-accounting drain race; G12 (commit `52f05f7`) fixed exactly that race
   (ledger Gate B lane). The pack can start at P1.
5. **The T1 measurement lane explicitly deferred CSV export** (T1/UI disclosure: "CSV export
   deferred") — yet synthesis T1-1's own build note says PDF-XChange's measurement-CSV export
   is one of the two features that make the toolset professional-grade. It is tonight item #1.
6. **Menu-bar Stamps submenu contains three unconnected QActions** (Approved/Draft/Confidential
   — silent no-ops bypassing `actionSpecs()` and `TestMenuBarIntegrity`, matrix notes §E). Rides
   N15 (stamp library); must not ship as-is to a community that DevTools-audits dead UI
   (r-pdf-mining 4.2).

---

## 1. Backlog table (ranked: demand × leverage ÷ size)

| # | Idea | Source(s) + rec# | User-demand evidence | Size | Depends on | Status |
|---|------|------------------|----------------------|------|------------|--------|
| 1 | Redaction Proof Mode (verify + what-was-removed report + audit log) | synthesis T1-2; r-pdf-mining 2.1/5.3 | 17 r/pdf threads — largest theme in corpus | L | — | **SHIPPED** — T2 lane `15f3f1c` (RedactionProof + 20 tests, ledger waveT2) |
| 2 | Measurement toolset (distance/perimeter/area + calibration) | acrobat rec#4; pdfexpert §1.11; masterpdf §5; synthesis T1-1 | 8/16 tools; AEC buyer checklist | L | — | **SHIPPED** — T1 lane `e73a446` + G21–G23 (ledger waveT1) |
| 3 | **Measurement CSV export + manage-measurements dialog** | pdfxchange §5 (10.5/10.8); synthesis T1-1 build note | rides 8/16 measurement demand; "ship those two first" | S | landed measurement list | **NEW (N1)** |
| 4 | Form-JS execution (run-side Calculate/Validate/Format/Keystroke) | acrobat §4 rec#7; masterpdf §5 #1; synthesis T1-3 | 8/16 tools; enterprise-forms dependency | L | decision: quickjs-ng dep | **QUEUED-PACK-B** (form-js-implementation-plan.md; decision request pending) |
| 5 | Local send-for-signing package (fields/recipients/order/package/audit PDF) | nitro rec#2 (A2); smallpdf §8; ilovepdf §1.8; synthesis T1-4 | 8+/16 ship it, all cloud-gated; r/pdf 1jj0zyw; PRD §9.7 "not started" | L | — | **QUEUED-PACK-C** (send-for-signing-implementation-plan.md P1–P4) |
| 6 | Named batch presets (Action-Wizard analog) | foxit rec#1; acrobat rec#3; nitro rec#5; sejda G1; synthesis T2-1 | 6/16 tools; institutional rail (acrobat L5 state-DOT) | L | P0 drain fix | **QUEUED-PACK-A** (batch-presets-implementation-plan.md; **P0 already fixed by G12 `52f05f7`**) |
| 7 | Find & Replace + regex/search-depth pack | foxit rec#3; pdfxchange §1; updf §1; synthesis T2-2 | 5/16 tools; r/pdf editing cluster 7 threads | M | — | **QUEUED-T2-2** (PRD §9.15, v1.4 roadmap) |
| 8 | Comment statuses + printable summary document | acrobat L4; foxit §1.5; pdfxchange §5; synthesis T2-3 | 7/16 tools | M | U07 (shipped) | **QUEUED-T2-3** (v1.4 roadmap) |
| 9 | Accessibility authoring pack (tag tree, checker, auto-tag, PDF/UA) | acrobat rec#2; foxit rec#4; pdfxchange §12; synthesis T2-4 | 6/16 tools; EAA/508 driver (smallpdf §12 markets the fear) | L | PP-DocLayout (in stack) | **QUEUED-T2-4** |
| 10 | Legal batch remainder: batch split-to-single-pages + batch password/permissions strip | r-pdf-mining 1.3/5.1 (7 password + 1 split threads); synthesis T2-5 | 7 + 1 threads; merge→Bates→split workflow | M | batch engine (shipped) | **QUEUED-T2-5** — cross-doc Bates part already **SHIPPED** (§9.9-c `69cb6e4` + G03 preflight) |
| 11 | Dynamic stamps + sequential numbering + stamp library mgmt | acrobat rec#8; foxit §1.5 (2025.3); synthesis T2-6 | 6/16 tools; "cheap, loved" | M | stamp annots (shipped) | **QUEUED-T2-6** |
| 12 | OCG layers panel (toggle/reorder/properties/create) | acrobat rec#5; foxit delta; masterpdf §2; synthesis T2-8 | 5–6/16 tools; map/CAD-adjacent | M | engine set-layer-visibility op (missing; matrix A4 caveat) | **QUEUED-T2-8** |
| 13 | Auto-bookmarks from text styles/TOC | foxit rec#5; pdfxchange 10.8; synthesis T2-9 | 4–5/16 tools; "cheap build, visible value" | M | text-extraction pipeline (shipped) | **QUEUED-T2-9** |
| 14 | Print presets + N-up/booklet + saved print configs | masterpdf §11; pdfxchange §11; nitro C8; synthesis T2-10 | 5/16 tools | M | — | **QUEUED-T2-10** |
| 15 | TTS read-aloud + page-color transforms | okular §1.1/1.3 (8 transforms); r-pdf-mining 2.4 (1 thread, inversion); synthesis T2-11 | 6/16 TTS; okular stack = free-tier gold standard | M | Qt6 Speech | **QUEUED-T2-11** |
| 16 | Split-by-text/bookmarks + filename templating grammar | sejda G2/G3; synthesis T2-12 | 5–6/16 tools; mailroom/invoice flows | M | OCR text layer + outlines (shipped) | **QUEUED-T2-12** (templating grammar designed inside QUEUED-PACK-A §3.6) |
| 17 | PDF/A accessible levels (-1a/2a/3a) | pdfxchange §7; ilovepdf §1.12; abbyy §9; synthesis T2-13 | 4/16 tools; EAA/buyer checklists | M | tag writer (= T2-4) | **QUEUED-T2-13** |
| 18 | **XFA unsupported-form honesty banner** (CapabilityRegistry disclosure) | okular §1.4 (`HasUnsupportedXfaForm`); synthesis T3-3 "copy the banner meanwhile" | XFA rescue = masterpdf loved-workflow #2 (community moat) | S | `IFormManager::hasXfaForms` (exists, `IFormManager.h:51`) | **NEW (N2)** |
| 19 | **Skip-already-text idempotent batch OCR** (`-skipFilesWithText`/`-skipPagesWithText`) | pdf24 §1.3 CLI verbatim; synthesis T3-9 "trivial absorb" | r/pdf OCR cluster 7–8 threads wants repeatable local OCR | S | batch OCR lane + per-page text detection (shipped) | **NEW (N3)** |
| 20 | **OCSP-offline toggle + honest offline-validation wording** | okular §1.8 (21.12 opt-out; okular Finding 4 rec) | r/pdf upload-fear cluster 10+ threads; air-gap promise | S | SignatureManager OCSP path (shipped) | **NEW (N4)** |
| 21 | **Reverse page order (wire-up)** | pdfxchange 10.8; sejda G7; updf §2 reverse/swap-odd-even | scan/booklet workflows; matrix: "one `reorderAllPages` permutation away" (notes §B) | S | `reorderAllPages` (exists, `IPdfEditorEngine.h:256`) | **NEW (N5)** |
| 22 | **Apply-time redaction scope selector + repeat-text overlay fill** | bluebeam §6 (apply scope 1-3,5,9; Repeat Text); masterpdf §6 repeat-overlay | redaction trust demand (17 threads) polish | S | mark-all page list (shipped `fa3b957`); overlay labels (shipped §9.8-b) | **NEW (N6)** |
| 23 | **Invisible signature + saved signature-appearance presets** | masterpdf §8 (invisible sig; saved appearance settings); pdfxchange §8 (certify w/o visible) | signing parity polish; PAdES buyers | S/M | `signDocument` (shipped); appearance engine `61fac01` | **NEW (N7)** |
| 24 | **Offline NER "Smart Redact" (entity detection, local models)** | nitro rec#1 (A1); foxit rec#8; synthesis correction C13 (local AI ≠ anti) | Nitro monetizes it cloud-only; "the marquee gap GlyphPDF can close *better*" (nitro A1) | L | pattern-redaction pipeline + ONNX runtime (pp models in `models/`) | **NEW (N8)** — top L-rank |
| 25 | **Table/form-data extract-to-spreadsheet flow** | nitro rec#3 (A1 spillover); matrix `extractTables` Planned | Nitro monetizes; r/pdf data-extraction demand | M | V03 column clustering (`0420cb5`) + CSV/XLSX writers (shipped) | **NEW (N9)** |
| 26 | **Visual-overlay compare mode (red/green blend)** | ilovepdf §1.9 (Content Overlay + scroll sync); bluebeam §9 Overlay Pages | compare is a 0-competition GlyphPDF moat; overlay is the one mode we lack | M | DiffEngine + PDFium render (shipped) | **NEW (N10)** |
| 27 | **Annotation + form-field-value diff** in compare | pdfxchange 10.0 (annotation deltas); abbyy §3 (form-field values in diff) | legal review use; completes the compare story | M | annotation records + `captureFieldSnapshot` (exist) | **NEW (N11)** |
| 28 | **Delete empty pages (noise tolerance)** | pdfxchange 10.4; abbyy §1 blank-page detect R3 (+ hot-folder quarantine) | archival/batch hygiene; UPDF ships it as *cloud AI page audit* | M | page content probes + `deletePage` (exist) | **NEW (N12)** |
| 29 | **Batch form-data combine → one CSV** (across many PDFs) | foxit §1.4 ("combine form data → one CSV") | legal/office batch workflows | M | `exportFormData` CSV per file (exists, `IFormManager.h:121`) + batch loop | **NEW (N13)** |
| 30 | **Redaction reason codes (FOIA/DOD/Privacy-Act code sets + import/export)** | bluebeam §6 (DOD/FOIA presets); pdfxchange 10.8 (code sets import/export); synthesis T3-5 | r/foia 1q4m4xm; government persona (r-pdf-mining 5.3) | M | overlay-label infra (shipped §9.8-b) | **NEW (N14)** |
| 31 | **Stamp library management + wire the dead Stamps menu** | foxit §1.5 (Favorite Toolbox import/export 2025.3; GPO-shared sets 2026.2 — "offline-compatible — buildable" per foxit delta); matrix §E | stamps ship in 14+/16 tools; dead-menu defect must not ship | M | AnnotationLayer stamp mode; 3 unconnected menu QActions (matrix §E) | **NEW (N15)** |
| 32 | **Watermark/background template manager + edit-existing-watermark** | masterpdf §7 (saved templates; edit 5.9.94); updf §2 (named presets + batch reuse 2.5.5) | watermark = standard office demand; UPDF differentiates on it | M | addText/ImageWatermark (exist) | **NEW (N16)** |
| 33 | **Certificate (recipient-list) encryption UI** | acrobat §7; foxit §1.7; masterpdf §7; synthesis T3-1 | legal/privacy persona; 3/16 tools | M | **engine seam exists** — `encryptWithCertificate` (`IPdfEditorEngine.h:264–275`, ER-3 counter) | **NEW (N17)** — reclassified from MISSING (Finding 0.1) |
| 34 | **DocMDP certify-vs-approve UI (level choice in SignatureDialog)** | acrobat §8; foxit §1.8; pdfxchange §8; bluebeam §8; synthesis T2-7 | legal persona requires certify semantics; 4/16 tools | M | **engine seam exists** — `certifyDocument` levels 1–3 fail-loud (`ISignatureManager.h:75`) | **NEW (N18)** — reclassified from MISSING (Finding 0.2); do not duplicate inside QUEUED-PACK-C |
| 35 | **Quick-annotation keyboard loop** (keys 1–9/Alt-strip, continuous-mode pin, editable tool palette) | okular §1.5 + Finding 3 ("copy almost verbatim") | "fastest annotation loop in the free tier"; Persona 1/2 throughput | M | annotation toolbar + tool modes (exist); QShortcut registry (28 bindings, matrix) | **NEW (N19)** |
| 36 | **Area/region OCR + OCR-suspects batch pass** | pdfxchange §3 (selected-region OCR); foxit §1.3 (Find-All-Suspect); pdfgear §3 (loved Extract-Text); synthesis T3-4 | r/pdf OCR cluster; PDFgear's area-OCR is a "first-minute value story" | M | U03 verify screen + selection funnel (shipped) | **NEW (N20)** |
| 37 | **Auto-OCR on navigation of image-only pages** (background/ephemeral layer) | masterpdf §3 (5.9.89, "lovely convenience"); abbyy §1 background recognition | scan-reading UX; makes §9.4 visible to casual users | M | OcrPipeline + background thread + viewer page-change signal | **NEW (N21)** |
| 38 | **Compress strip-matrix expansion** (deduplicate streams, subset fonts, rasterize heavy graphics) | pdf24 §1.10 ("most granular free compressor found"); r-pdf-mining 1.3 (8 compress threads) | users flee to qpdf/Ghostscript CLI for exactly these knobs | M | `optimizeDocument` + qpdf backend (exist) | **NEW (N22)** |
| 39 | **Audit Space Usage (per-feature byte breakdown)** | pdfxchange 10.1 | M8-class honesty surface; power-user trust | M | PoDoFo object/stream walk | **NEW (N23)** |
| 40 | **Batch signing across files** (one cert, many docs, G03-style preflight) | masterpdf §10 (5.9.96); foxit §1.8; bluebeam §8 Batch Sign & Seal; r-pdf-mining 2.4 (1kjr8x4) | 1 r/pdf thread + 3 competitors | M | `signDocument` + batch engine + G03 preflight pattern (shipped) | **NEW (N24)** |
| 41 | **Repair PDF (open & recover damaged files)** | sejda G6; pdf24 §1.2; ilovepdf §1.7 ("a real GlyphPDF gap"); synthesis T3-2 | rare-in-class trust win; damaged-file open is a top support class | M | qpdf recovery modes (`src/engines/qpdf`) + PDFium fallback-open | **NEW (N25)** |
| 42 | **Grayscale / B&W document conversion** | masterpdf §7 (5.9.84); sejda §1.2; foxit 2026.1 (preflight grayscale) | print-cost standard; 3+ tools | M | render/recolor pipeline + image ops | **NEW (N26)** |
| 43 | **Word count / document statistics + saved search history + named nav positions** | foxit §1.1 (all three); masterpdf 5.9.90 (search history); pdfxchange §1 (word count) | small office asks; near-zero cost | S | FindBar + text extraction | **NEW (N27, S-cluster)** |
| 44 | **Offline NER ⇄ tiling + dynamic-username watermarks** | foxit §1.2 (v12 tiling + username watermarks) | enterprise watermark demand | S/M | `addImageWatermark` Tile position (exists) | **NEW (N28)** |
| 45 | **Interleave (odd/even) merge** | foxit §1.2 (v12); sejda Alternate&Mix; updf §2 swap-odd-even | two-sided-scan recombination; standard scan workflow | M | merge worker (batch merge shipped) | **NEW (N29)** |
| 46 | **XFDF/FDF comment + form-data round-trip (incl. annotation-only export)** | foxit §1.5; masterpdf §5 (FDF comments); bluebeam §5 (BAX/FDF) | PDF-native interchange; reviewer hand-off without the PDF | M | annotation model + FDF writer (form FDF exists) | **NEW (N30)** |
| 47 | **Form format-categories editor** (number/currency/date/percent/special) | updf §4 ("matches Acrobat-style format tabs"); acrobat §4 | forms parity polish; r/pdf forms cluster 10–11 threads | M | `addNumericField`/`addDateField` (exist) + format-string writer | **NEW (N31)** |
| 48 | **localhost MCP server over the CapabilityRegistry** | nitro rec#6 (A6); synthesis T3-10 "small surface" | Nitro Automate + Bluebeam Max validate agent demand; bytes stay local | M | CapabilityRegistry (shipped U08) + existing CLI verbs | **NEW (N32)** |
| 49 | **CLI verbs for redact/sign/watermark/encrypt** (LibreOffice-style headless option set) | libreoffice-draw delta ("single most copyable LibreOffice advantage", §10 JSON filter options) | power users script pipelines; complements QUEUED-PACK-A | M | engine seams all exist; batch CLI exists (M2) | **NEW (N33)** |
| 50 | **Sessions save/restore (named sets of open docs)** | masterpdf §1 (5.9.50); pdfxchange §1 session files; okular tabs | multi-document review workflows | S/M | Qt session/window state | **NEW (N34)** |
| 51 | **Print-appearance warning** (optional content/annot flags print differently) | pdfxchange 10.8 ("nice honesty feature") | M8 disclosure on the print path | S | print path + annotation/OCG flags | **NEW (N35)** |
| 52 | **Markup Selection Cycle** (cycle stacked markups) | bluebeam §1 (21.4) | dense-sheet selection friction | S | viewer hit-test loop | **NEW (N36)** |
| 53 | **Back/forward view history (Alt+arrows)** | okular §1.1 ("genuinely good for reference-heavy reading"); masterpdf Previous/Next View | academic/legal reading UX | S | viewer navigation stack | **NEW (N37)** |
| 54 | **Deployment trust pack: GPO/ADMX templates + documented silent MSI + offline-license statement** | foxit rec#10; nitro rec#8 (A7/B1–B3); masterpdf §10 GP; abbyy §13 playbook | "the IT wedge is silent MSI + GPO/ADMX + offline license files" (synthesis §3.11) | M | MSI pipeline (shipped INF02/INF03) | **NEW (N38)** |
| 55 | **Shell extensions: right-click convert/combine in GlyphPDF** | nitro A3 (the one local piece — "cheap, loved"); ilovepdf §1.1 ("fastest way to convert") | loved convenience; iLovePDF leads with it in marketing | M | existing conversion entry points | **NEW (N39)** |
| 56 | **Accessibility of signing: strong-verification strict mode + signed-version byte-range viewing** | masterpdf §8 (Strong verification; "Click to view this version") | anti-substitution trust; unique differentiator | M | `isLegitimateIncrementalAppend` + SignatureInfo byte ranges (exist) | **NEW (N40)** |
| 57 | Cross-format compare (Word vs its scan via in-house OOXML import → DiffEngine) | abbyy §3 ("the killer"); abbyy Top-5 #2 | legal/quality audience; ABBYY gates it Corporate-only | L | OOXML import (shipped) + DiffEngine | **NEW (N41)** |
| 58 | Full-text index (.pdx-class) + multi-folder search with saved criteria | acrobat §1 rec#9; foxit §1.1 (catalog + scheduled indexing); synthesis T3-8 | decades-old Acrobat capability; power users | L | local SQLite index | **NEW (N42)** |
| 59 | Scanner acquisition (TWAIN/WIA scan-to-PDF + scan presets) | acrobat §13; masterpdf §3; pdfgear §2; NAPS2 praise (r-pdf-mining 4.1) | r/pdf loves NAPS2; scan ingestion is the OCR entry point | L | — | **NEW (N43)** |
| 60 | XFA static fill (+ dynamic view optional) | masterpdf §4 ("the community moat"); synthesis T3-3 | government/insurance "Please wait..." rescue | L | XFA parser | **NEW (N44, persona-gated)** |
| 61 | ZUGFeRD/Factur-X e-invoice read/validate | pdf24 §1.12 (DACH moat, free); synthesis T3-11 | DACH B2B mandate; no other corpus tool ships it | L | — | **NEW (N45, persona-gated DACH)** |
| 62 | OCR customization stack (area templates, pattern training, custom languages/dictionaries) | abbyy §1 (gold standard); synthesis T3-7 | structured-doc batch OCR | L | U03 verify screen | **NEW (N46, persona-gated)** |
| 63 | Barcode fields + barcode recognition | acrobat §4; pdfxchange §2; abbyy §1; synthesis T3-6 | 5/16 tools but **zero r/pdf forms-cluster mentions** | M | — | **NEW (N47, build-on-demand)** |
| 64 | Spell check | foxit §1.2; synthesis T3-13 | "low demand" (synthesis) | S | — | **NEW (N48, low rank)** |
| 65 | PDF/A-4 + PDF 2.0 export | libreoffice-draw §3 (25.8); pdfxchange §7 | archival future-proofing; no buyer checklist demands it yet | L | PDF/A writer discipline (N03) | **NEW (N49, low rank)** |
| 66 | Preflight-lite validator + fixup presets (PDF/A structural contract + common fixups) | acrobat rec#1; nitro anti (full engine); C12 graded resolution | Pro demand real; scope graded "validation-and-fixups-lite = yes, deferred" (synthesis C12) | L | veraPDF wiring (shipped) + PoDoFo fixups | **NEW (N50, deferred-by-C12)** |
| 67 | GlyphRecovery (reconstruct mis-encoded PDF text) | abbyy §1 (claimed +36% on ~2/3 of such PDFs — UNVERIFIED vendor claim) | corrupt-scan export niche; PDFium extraction (F07) already covers much | L | — | **NEW (N51, low rank)** |
| 68 | Vector path-editing toolset / Repair ToUnicode CMap / Select Page Region / Normalize Pages | pdfxchange §2 (10.1/10.3) | pro-editing depth; MPE-adjacent "object surgery" devotion | L | content-stream ops | **NEW (N52, low rank)** — single row for the object-surgery cluster |
| 69 | External image-editor hand-off (round-trip) | acrobat rec#10 ("PARTLY_TRUE gating, TRUE feasibility") | pro edit convenience | M | image replace seam (exists) | **NEW (N53)** |
| 70 | AEC subset: Sets revision navigation + Batch Slip Sheet + VisualSearch | bluebeam §9–11; synthesis T3-12 | AEC-only; **PRD §7.2 excludes CAD** | L | — | **REJECTED (persona-excluded)** — revisit only if an AEC persona is formally added |
| 71 | Measurement follow-ups: per-viewport scales, AP-stream value captions, data-driven captions | T1/UI disclosures ("per-viewport scales deferred; AP-stream caption deferred"); bluebeam §11 captions-from-columns | completes T1-1 to Bluebeam depth | M | landed measurement lane | **NEW (N54)** |
| 72 | PDF24 reader niceties: XFA fill toggle, in-reader object move/delete, Reader→Tool handoff | pdf24 §1.1 | deployment reach, not depth | M | — | **REJECTED-scope** (viewer creep; no demand signal) |
| 73 | Secure time-boxed "week pass" licensing instrument | sejda G9/L3 ($5–7.95 one-time week passes loved) | one-off local users without subscriptions | S | pricing decision, zero engineering | **NEW (N55, non-code; recorded)** |
| 74 | "No remote deactivation / no activation counts / no telemetry" licensing promise surface | nitro B1–B3 (BBB F); pdfxchange §14 activation friction; sejda F4 | r/pdf 1.2 "one-time purchase" 6+ threads; PDF-XChange-shaped honesty (1rs4ey4) | S | — | **NEW (N56, copy/UX + zero-engineering licensing posture)** |
| 75 | LibreOffice-style threaded-comment import fidelity (IRT/State resolution) | libreoffice-draw §6 ("more rigorous than many paid tools") | comment round-trip correctness for review cycles | M | annotation model + reply nesting (shipped U07) | **NEW (N57)** |
| 76 | Header/footer "shrink content under H/F+Bates" option | pdfxchange §2 | overlapping-content correctness | S | addHeaderFooter (exists) | **NEW (N58)** |
| 77 | Excel/CSV populate of forms from a data file | pdfxchange §4 (CSV populate UX) | mailroom merges | M | `importFormData` (exists) | **NEW (N59)** |
| 78 | iLovePDF-style required-field save-validation wording + "no fields → detect automatically" fallback | ilovepdf §1.4 (mined dialog strings) | forms UX trust; auto-detect already shipped (88d4286/V06) | S | auto-detect (shipped) | **NEW (N60)** |
| 79 | Upsell-adjacent gating of shipped features ("Smart Tools" pattern) | nitro anti; synthesis §3.12 | — | — | — | **REJECTED** (the mechanism that created Nitro's r/complaints corpus) |
| 80 | PDF Portfolios | synthesis §3.8; acrobat anti; r-pdf-mining anti#5 | 1 r/pdf thread; Acrobat de-emphasizing | — | — | **REJECTED** (niche Acrobat-ism) |
| 81 | 3D PDF authoring, geospatial projection math | synthesis §3.7; acrobat anti | niche CAD; enormous cost | — | — | **REJECTED** |
| 82 | Cloud processing/upload companion; cloud AI; cloud e-sign SaaS; DRM/remote deactivation; ECM/DMS connectors; admin cloud portals; Liquid-Mode cloud reflow; PostScript/Distiller; droplets; mobile/web-first; full print-production engine; virtual printer driver | synthesis §3 (all 13); masterpdf §11 ("big lift not worth it"); abbyy §failure-2 (removed its printer 2023); nitro anti | upload fear 10+ threads; subscription anger 6+; DevTools audits (r-pdf-mining 4.2) | — | — | **REJECTED-cloud-gated** (see §4) |

Counts: 82 deduplicated rows. SHIPPED 4 (rows 1,2 + sub-references), QUEUED-PACK 3,
QUEUED-T2 11, REJECTED 8 rows (covering ~20 distinct anti-ideas), NEW 56 numbered N1–N60
(two clusters merged). Total distinct actionable ideas mined across the corpus before
deduplication: ~130; after dedup: 82.

---

## 2. NEW items — behavior spec, competitor model, engine seams, acceptance tests

Grouped S / M / L. Quotes are from the cited source report. Engine seams verified against
`src/core/interfaces/` and the matrix at HEAD.

### 2.1 S-size (tonight-feasible; see §5 for the ranked shortlist)

**N1 — Measurement CSV export (+ manage-measurements list).**
- Spec (pdfxchange §5): "export measurements CSV, manage-measurements dialog w/ import/export+filter (10.5)"; synthesis T1-1: "PDF-XChange's measurement-CSV export … ship those two first."
- Competitor: PDF-XChange (free tier). Columns per measurement: page, type, calibrated scale, value, unit, label (/Contents).
- Seams: AnnotationLayer measurement list (T1 lane, `e73a446`); CSV discipline pinned by U07 (RFC-4180).
- Accept: exporting a calibrated perimeter writes a CSV whose re-importable rows reproduce value+unit+page for every annotation on the page; empty-document export produces headers only; RFC-4180 escaping pinned like U07.

**N2 — XFA honesty banner.**
- Spec (okular §1.4): "generator returns `HasUnsupportedXfaForm` … User is told the form is unsupported rather than silently mis-filling"; synthesis T3-3: "Copy Okular's honest 'unsupported form type' banner via the CapabilityRegistry meanwhile."
- Competitor: Okular (source-verified), Master PDF (the rescue tool the banner deflects to).
- Seams: `IFormManager::hasXfaForms` (`IFormManager.h:51`) + CapabilityRegistry whyNot/alternative contract (`Capability.h:92–94`).
- Accept: opening an XFA document raises a CapId with non-empty whyNot + alternative ("fill/print in the source application"); a non-XFA doc raises nothing; registry query enforces the contract.

**N3 — Skip-already-text batch OCR.**
- Spec (pdf24 §1.3): CLI mirrors GUI switches "`-skipFilesWithText`, `-skipPagesWithText` … force OCR"; synthesis T3-9: "trivial absorb for the batch OCR lane."
- Seams: BatchMode OCR worker + PDFium per-page text extraction (already used by Bates tests).
- Accept: a mixed corpus of text and scanned PDFs with skip-file flag produces outputs only for image-only files; skip-page flag leaves text pages byte-identical inside output; force-OCR overrides both; idempotency: second run is a no-op.

**N4 — OCSP-offline toggle + honest wording.**
- Spec (okular §1.8): "OCSP network queries with an opt-out ('Support not contacting OCSP servers when validating signatures', 21.12)"; okular Finding 4: "add the OCSP-offline toggle pattern."
- Seams: SignatureManager OCSP fetch path; Settings + CapabilityRegistry disclosure of degraded validation.
- Accept: toggle ON → validation runs with zero network (no socket attempts — test asserts), signature panel states "revocation not checked (offline)"; toggle OFF restores live OCSP; persisted across restarts.

**N5 — Reverse page order (wire-up).**
- Spec: pdfxchange 10.8 "reverse page order"; matrix notes §B: "`reverse` is one `reorderAllPages` permutation away."
- Seams: `IPageEditor::reorderAllPages` (`IPdfEditorEngine.h:256`); hidden ribbon id `reverse`.
- Accept: N-page fixture reversed on disk (PDFium page-identity checks both endpoints); undo restores original permutation; read-only refusal per EditPolicy (ARC07 pattern).

**N6 — Apply-time redaction scope + repeat-text overlay.**
- Spec (bluebeam §6): "Apply scope: All pages / current / custom range (1-3, 5, 9) at apply time"; "Repeat Text (fills area with repeated text)" (also masterpdf §6: "repeat overlay text to fill the box").
- Seams: RedactRequest plan seam (N04's shared `redactRequestFromPlan`); overlay burn-in (§9.8-b + N08 metrics).
- Accept: marks on pages 1–3 apply with explicit page list where marks exist only on marked pages (extend TestRedactMarkAll page-list contract to the dialog); a wide box with repeat-text fills with repeated glyphs that still satisfy the N08 vertical/horizontal fit pins.

**N7 — Invisible signature + saved appearance presets.**
- Spec (masterpdf §8): "Invisible signature … managed in Signatures tab"; "saved appearance settings"; pdfxchange §8 "Certify w/o visible signature."
- Seams: `signDocument` appearance optional (`planSignatureAppearance`); QSettings preset precedent (ExportPresetsPanel).
- Accept: signing with appearance suppressed produces a valid signature whose widget has no /AP stream (validateSignatures still passes); saved appearance round-trips across sessions and documents.

**N27 — S-cluster: word count, saved search history, named positions, back/forward history, selection cycle, shrink-under-H/F, print-appearance warning, required-field fallback wording.**
- Specs: foxit §1.1 (word count, named positions); masterpdf 5.9.90 (search history); okular §1.1 (back/forward "click citation [15], jump back"); bluebeam §1 (21.4 Selection Cycle); pdfxchange §2 (shrink content under H/F+Bates); pdfxchange 10.8 (print-appearance warning — "Warns when optional content/annot flags/JS may print differently than displayed"); ilovepdf §1.4 (detect-automatically fallback dialog strings).
- Seams: text extraction; FindBar (regex checkbox exists, matrix §C); viewer history stack; annotation hit-testing; addHeaderFooter options; print path flags; auto-detect + V06 compound undo.
- Accept (each): single behavior pin per item in the style of TestMeasurePanelHonesty — e.g., print warning fires for a doc with an OCG layer and annots set to not-print, and does not fire on a plain document.

### 2.2 M-size

**N8 — Offline NER Smart Redact.**
- Spec (nitro A1): "Extend pattern redaction with local NLP/NER entity detection (ONNX models, same runtime as RapidOCR/PP-DocLayout) over OCR + text layers → offline Smart Redact with the same review-before-burn UX"; foxit rec#8: "local NER model to match Foxit Smart Redact (names/orgs/roles) without its cloud; slots into the existing redaction transaction + pattern presets."
- Competitor: Nitro Smart Redact (30+ PII categories, cloud-gated, Pro-only); Foxit Smart Redact (AI service, Pro-only).
- Seams: `applyPatternRedactionsMulti` + named presets (§9.12-a) + word-list import (§9.8-c) + ONNX runtime already provisioning pp_doclayout/ppocrv5; proof mode (T2) verifies the burn.
- Accept: a fixture letter containing a person name/org/address is detected by the local model and surfaced as reviewable marks; nothing egresses (loopback-guard test idiom from F03/R04); marks flow through the existing 7-stage transaction; proof pack reports the NER-derived excisions like pattern-derived ones.

**N9 — Table/form-data extract-to-spreadsheet.**
- Spec (nitro rec#3): "surface the existing V03 column-detection + form data (CSV/FDF) as one 'extract tables & form data to XLSX/CSV' flow over selections or batches. Nitro monetizes this; GlyphPDF mostly has the parts."
- Seams: `ConversionManager::deriveColumns` (V03 `0420cb5` — x-anchor clustering); in-house XLSX writer (§9.5); matrix hidden ids `extractTables`/`detectTables`.
- Accept: a 3-column ruled-table fixture extracts to XLSX with true column addressing (V03 pins); a form fixture exports field name/value pairs per page; batch mode processes a folder to one workbook with a file column.

**N10 — Visual-overlay compare mode.**
- Spec (ilovepdf §1.9): "Content Overlay: 'Overlay content from two files and display any changes in a separate color'; scroll sync"; bluebeam §9: "2+ PDFs color-layered into one PDF (red/green blend); per-layer color/opacity/blend."
- Seams: DiffEngine alignment (fingerprint LCS, CMP-align) + PDFium page raster + U04 anchors.
- Accept: two revised fixtures produce an overlay artifact where removed content renders in color A and added in color B, navigable via the existing change filter; identical documents produce a clean overlay; mode is gated behind the same entry as text compare.

**N11 — Annotation + form-field-value diff.**
- Spec (pdfxchange 10.0: compare "includes comment/annotation deltas"; abbyy §3: diffs "form-field values and Text Box annotations differences").
- Seams: annotation records + `IFormManager::captureFieldSnapshot`; DiffEngine PageChange model extension.
- Accept: fixtures differing only in one comment and one field value surface both as change rows with page anchors; report export includes the new categories; identical docs yield zero annotation diffs.

**N12 — Delete empty pages.**
- Spec (pdfxchange 10.4): "delete empty pages + noise tolerance"; abbyy §1 blank-page detect "with quarantine folder option" (hot-folder).
- Seams: page text/image probes (PDFium extraction + `listImages`) + `deletePage` + batch loop.
- Accept: a fixture with blank, near-blank (one speck below tolerance) and content pages deletes exactly the blanks at default tolerance; tolerance 0 keeps the speck page; undo restores page order; batch variant reports per-file counts.

**N13 — Batch form-data combine → single CSV.**
- Spec (foxit §1.4): "combine form data (PDF/PPDF/FDF/XFDF/XML) → one CSV."
- Seams: `exportFormData(format="CSV")` per file (`IFormManager.h:121`) + BatchMode worker + U07 CSV escaping discipline.
- Accept: N filled fixtures with identical field sets produce one CSV with one row per file (union of field names as columns, empty where absent); a corrupt file yields a failed row, not a truncated export (G12-style reconciled counts).

**N14 — Redaction reason codes.**
- Spec (bluebeam §6): "Overlay Text ('CONFIDENTIAL'), … **DOD/FOIA preset codes**"; pdfxchange 10.8: "code list editor + import/export."
- Seams: overlay-label burn-in (§9.8-b/N08) + named-preset pattern (§9.12-a) + JSON import/export idiom (annotation package, SecurityController.cpp:547).
- Accept: applying a mark with FOIA(b)(6) code burns the code as the overlay label (N08 fit pins hold); a custom code-set imports and appears as quick-pick; export round-trips byte-stable.

**N15 — Stamp library + wire the dead Stamps menu.**
- Spec (foxit §1.5): "Favorite Toolbox create/import/export (2025.3/2026.2)"; delta: "enterprise stamp sets via file share (offline-compatible — buildable)." Matrix §E: Approved/Draft/Confidential menu QActions are "UNCONNECTED … silent no-ops."
- Seams: AnnotationLayer stamp mode; menu `actionSpecs()`; QSettings/file-folder preset store.
- Accept: each menu stamp places its named stamp; a user stamp imported from a shared folder appears in the picker after restart; export/import round-trips; TestMenuBarIntegrity extended to the submenu.

**N16 — Watermark/background template manager + edit-existing.**
- Spec (masterpdf §7): "saved templates, multiple watermarks per page, **edit existing watermarks** (5.9.94), remove all"; updf §2: "named presets + batch reuse (2.5.5)."
- Seams: `addTextWatermark`/`addImageWatermark` (Tile position exists, `IPdfEditorEngine.h:81`); preset store idiom.
- Accept: a saved template reapplies identically to a second document; edit-in-place replaces the prior watermark object (not stacking); remove-all clears every watermark it created and leaves others untouched (pinned by fixture with a foreign watermark).

**N17 — Certificate-encryption UI.**
- Spec (acrobat §7): "Encrypt to recipient public keys … multiple recipients, different permission sets per recipient"; masterpdf §7 recipient-list + own-cert warning.
- Seams: **existing** `IEncryptor::encryptWithCertificate` + ER-3 CMS-recipient counter (`IPdfEditorEngine.h:264–275`); cert source = P12/Windows store paths from SignatureManager.
- Accept: encrypting to two recipients lets each recipient's cert decrypt (round-trip via the engine's own load path); wrong-cert open fails honestly; source byte-invariance on failed commit (SafeSave discipline).

**N18 — DocMDP certify UI.**
- Spec (acrobat §8): "Certify document with change-allowed levels"; foxit §1.8 "certify + permitted actions."
- Seams: **existing** `certifyDocument(certLevel 1–3)` fail-loud (`ISignatureManager.h:75`); SignatureDialog gains certify mode + level picker with plain-language permitted-change wording.
- Accept: certifying at level 1 then editing content flips validation to modified (validateSignatures + tamper evidence); level wording maps 1:1 to /DocMDP P values in a saved-artifact check; SignOutcome degradation surfaces as in §9.7-c.

**N19 — Quick-annotation keyboard loop.**
- Spec (okular Finding 3): "Keys 1–9 + Alt-9…0 quick tools, continuous-mode pin, per-tool defaults, tool palette editor … GlyphPDF should copy it almost verbatim (UI-layer)."
- Seams: annotation toolbar + ToolModes; QShortcut registry (28 bindings, matrix scope table); named-preset pattern for tool defaults (§9.12-a idiom).
- Accept: pressing 1–9 activates mapped tools without menu focus; a pinned tool stays active after placing an annotation; palette edits persist and survive restart; conflicts with existing shortcuts resolved explicitly (registry audit test).

**N20 — Region OCR + suspects pass.**
- Spec (pdfxchange §3: "OCR Selected Regions"; foxit §1.3: "'Find All Suspect' panel; batch mark Not-Text"; pdfgear §3: area-OCR "Lifewire reviewer favorite").
- Seams: U03 selection funnel + word-confidence data + ReviewState lifecycle (F11/R07); OCR preprocessor (F05/F10).
- Accept: dragging a region OCRs only that region and lands in the verify screen with correct page coordinates; suspect words list navigates source⇄result; "Not Text" removes the word from the saved layer (F04 persistence pins hold).

**N21 — Auto-OCR on navigation.**
- Spec (masterpdf §3): "auto-OCR pages that contain only images/vectors as you view/edit (5.9.89)"; abbyy §1 background recognition adds "a **temporary** text layer for search/copy; not saved into the file unless user runs Recognize."
- Seams: viewer page-change signal + OcrPipeline async + an explicit ephemeral-layer flag (never silently persisted — M8).
- Accept: opening a scan and paging runs background OCR that enables selection/copy on viewed pages; the ephemeral layer is disclosed in the status bar and never written unless the user accepts (F04 path); setting turns off cleanly.

**N22 — Compress strip-matrix expansion.**
- Spec (pdf24 §1.10): "**deduplicate streams**, **rasterize heavy graphics**, **subset embedded fonts** … original size displayed; settings remembered."
- Seams: `optimizeDocument(OptimizeOptions)` + qpdf backend (dedupe/optimize are qpdf strengths).
- Accept: a fixture with duplicate images shrinks measurably with dedupe on and byte-differs only in expected object counts; an oversize image rasterizes to the DPI cap; every pass reports measured before/after (§9.13-a readout contract); unsupported pass → R12 honesty gate.

**N23 — Audit Space Usage.**
- Spec (pdfxchange 10.1): "Per-feature byte breakdown of the file."
- Seams: PoDoFo object walk (stream lengths by category: images, fonts, content, attachments, metadata).
- Accept: a fixture's reported total equals file size within declared overhead; categories sum to total; report is exportable (CSV) with the U07 discipline.

**N24 — Batch signing.**
- Spec (masterpdf §10: "Sign multiple documents at once (5.9.96)"; bluebeam §8 Batch Sign & Seal incl. per-file page ranges + "skip docs without matching fields").
- Seams: `signDocument` + batch engine + G03 whole-batch preflight pattern (inputs exist, outputs distinct, per-file SafeSave candidates) + N06 restartable SigningRequest.
- Accept: signing 3 files with one P12 produces 3 valid signatures (validateSignatures per file), any single failure leaves the other two intact and reported (G03 contract), no file is overwritten on refusal.

**N25 — Repair PDF.**
- Spec (sejda G6): "Investigate PDFium/PoDoFo recovery modes for an 'Open & Repair' entry"; pdf24 §1.2 "Repair PDF (rebuild broken files)"; ilovepdf §1.7 flags the gap.
- Seams: qpdf recovery (`src/engines/qpdf` — qpdf's damage tolerance), PDFium lenient open, PoDoFo strict parser as the validator; explicit "recovered, lossy?" disclosure (M8).
- Accept: a truncated-xref fixture opens via repair with page count + extractable text reported; unrecoverable input fails loudly with whyNot; repaired output declares the recovery in its completion report; original file never modified.

**N26 — Grayscale/B&W conversion.**
- Spec (masterpdf §7: "Convert whole document to grayscale (5.9.84); grayscale-with-alpha; B&W — in optimize and print"; sejda §1.2; foxit 2026.1).
- Seams: optimize pipeline + image ops (recolor) + optional print-time conversion.
- Accept: converting a color fixture yields zero color operators/images (probe); alpha preserved in grayscale-with-alpha mode; B&W threshold mode produces bilevel images; signed-doc guard applies (45aa606).

**N29 — Interleave (odd/even) merge.**
- Spec (foxit §1.2: "interleave merge (odd/even scans, v12)"; sejda "Alternate & Mix").
- Seams: batch merge worker (async merge landed) + `insertPageFromBytes`.
- Accept: two N-page scans interleave to a 2N-page document in alternating order with configurable start side; unequal lengths interleave then append remainder; undo-safe via the merge worker's transactional outputs.

**N30 — XFDF/FDF comment + form-data round-trip.**
- Spec (foxit §1.5: "Import/export FDF/XFDF"; masterpdf §5: FDF export/import of comments alone; bluebeam BAX import idiom).
- Seams: annotation records + existing form FDF writer; annotation package JSON idiom (SecurityController.cpp:547) as the internal fallback.
- Accept: comments-only export imports into a second copy of the same document with geometry/author intact; XFDF round-trip preserves reply threading (IRT) and resolve state (§9.14-class fidelity, libreoffice §6 sets the bar); form XFDF import maps to fields with an unsupported-fields report (existing importFormData contract).

**N31 — Format-categories editor.**
- Spec (updf §4: "Number (decimals, separator, currency symbol + position, negatives), percentage, date, time, special formats — matches Acrobat-style format tabs").
- Seams: `addNumericField`/`addDateField` (exist) + format-string writer (Acrobat /AA F format JS or /MK level per feasibility — decide at design; format-JS ties to QUEUED-PACK-B).
- Accept: a currency field with 2 decimals + $ prefix validates sample input and renders the mask; date category rejects an invalid date with the field's error style; saved doc re-opens with the format intact (PDFium read-back).

**N32 — localhost MCP server.**
- Spec (nitro rec#6): "a localhost MCP server exposing split/merge/convert/OCR/redact/batch over the existing capability registry keeps every byte on-machine"; synthesis T3-10: "Small surface."
- Seams: CapabilityRegistry (U08) as the tool-description source of truth; existing engine ops; loopback-only binding (F03/R4 loopback-guard idiom).
- Accept: a local MCP client lists tools mirroring CapIds, executes split+redact on a fixture, and no non-loopback socket ever binds (test asserts); every tool response carries the same whyNot on refusal.

**N33 — Headless CLI verb pack.**
- Spec (libreoffice-draw delta): "a UNO-like automation surface (CLI verbs for redact/sign/watermark/version) is the single most copyable LibreOffice advantage" (their §10 JSON filter options sign+encrypt from CLI).
- Seams: engine seams (sanitize/pattern-redact/sign/watermark/encrypt/PDF-A) + SafeSave + batch CLI precedent.
- Accept: each verb runs on a fixture headlessly producing the same bytes as the interactive path (differential test), honors read-only/expiry guards, and exits non-zero with techDetail on failure (INF01 CLI discipline).

**N34 — Sessions save/restore.** masterpdf §1 (5.9.50) + pdfxchange §1: save/open named sessions of open docs. Accept: a saved session reopens the exact document set (paths + view states); a moved file reports and skips honestly.
**N35 — Print-appearance warning.** pdfxchange 10.8 verbatim (see table). Accept: warning lists the specific flags found (OCG, annot-print flags, JS) per document; plain doc prints silently.
**N36 — Markup Selection Cycle.** bluebeam §1 (21.4). Accept: repeated cycle key walks stacked annotations at one point in z-order; Esc ends; selection lands on the topmost afterwards.
**N37 — Back/forward view history.** okular §1.1. Accept: jump-to-page-50 then Back returns to the prior position including scroll; history is per-document and cleared on identity change (ARC01 discipline).
**N38 — Deployment trust pack.** foxit rec#10 / nitro rec#8: ADMX/GPO template for update/telemetry-off defaults, documented silent-MSI flags (INF02 pipeline exists), and an explicit "no remote deactivation, no activation counts, no telemetry" statement surface. Accept: policy template toggles persist and are honored at runtime; installer flags verified by the INF02 validator idiom.
**N39 — Shell extensions.** nitro A3: "Explorer context-menu 'create/convert PDF' and 'combine in GlyphPDF' shell extensions are offline, cheap, and mirror a Nitro-loved convenience"; ilovepdf §1.1 right-click conversions.
**N40 — Strong-verification strict mode + signed-version viewing.** masterpdf §8: "Strong verification mode (new sig invalidates previous ones even untouched)"; "Signatures tab > 'Click to view this version' renders the byte range as actually signed — anti-substitution check." Seams: `isLegitimateIncrementalAppend` + SignatureInfo byte ranges. Accept: strict mode flags an appended-increment document that default mode passes; viewing a signed version renders exactly the signed byte range (page-identity fixture).
**N57 — Threaded-comment import fidelity.** libreoffice-draw §6: "/IRT reply-parent resolution, /State(/StateModel=Review) collapsed to Resolved flag." Accept: a fixture with a 3-deep IRT chain + Review states imports with replies nested and resolved state displayed; round-trips out via N30.
**N58 — Shrink-under-H/F+Bates.** pdfxchange §2: "Shrink content under H/F+Bates." Accept: stamping over content optionally scales the content box down so text is never overprinted (probe both modes).
**N59 — CSV populate of forms.** pdfxchange §4: "populate from CSV." Accept: a 3-row CSV fills 3 output copies with per-row values and an unsupported-columns report.
**N60 — Required-field + no-fields fallback wording.** ilovepdf §1.4: "You cannot save this file in Fill mode because a required field is missing…" + "We could not detect any form in the PDF… Add the form fields manually." Accept: wording parity pins in the fill-save path and auto-detect-empty path (V06 messaging contract).

### 2.3 L-size

**N41 — Cross-format compare.** abbyy §3: "Any two of: scans, images, PDF, DOC(X)… — e.g., Word vs its scan"; output benchmark = "Word file in **track-changes mode** … differences **table**." Seams: in-house OOXML import (§9.5) → normalize to PDF → DiffEngine; report export already HTML/text. Accept: Word-vs-scan fixture pair surfaces the two changed clauses; output artifacts: annotated PDF + diff table; identical docs produce clean results. (ABBYY's "does not detect missing/extra spaces" KB 20523291308818 is a testable edge to beat.)
**N42 — Full-text index + saved multi-folder search.** acrobat §1 rec#9 ("local SQLite index is a natural fit"); foxit §1.1 catalog + scheduled indexing. Accept: index N folders, search returns ranked hits with snippets across files; incremental re-index on file change; saved criteria rerun.
**N43 — Scanner acquisition.** r-pdf-mining 4.1: NAPS2 loved for "scan + OCR like Acrobat"; acrobat/masterpdf/pdfgear all ship scan ingestion. Accept: WIA scan of N pages lands as one PDF with a deskew preview; scanner absent → honest unavailable capability (registry).
**N44 — XFA static fill (persona-gated).** masterpdf §4: "Full support for editing static XFA (5.8.30) … The only mainstream small editor that edits static XFA"; synthesis T3-3 warns ISO-deprecated; gate on a government/insurance persona decision. **N45 — ZUGFeRD read/validate (DACH-gated).** pdf24 §1.12. **N46 — OCR customization stack (persona-gated).** abbyy §1 area templates/pattern training/custom dictionaries; the U03 verification-ergonomics bar (zoom pane + keyboard flow + uncertain-character logging — abbyy Top-5 #1) is the target regardless.
**N47 — Barcode fields/recognition (on demand).** synthesis T3-6: "the r/pdf forms cluster never mentions barcodes; build on demand."
**N49/N50/N51/N52 — low-rank L items** (PDF/A-4/PDF 2.0 export; preflight-lite; GlyphRecovery; object-surgery cluster) kept on the board with their sources; none ranks into the next two waves.
**N54 — Measurement follow-ups.** T1/UI disclosures: "per-viewport scales deferred; AP-stream value caption deferred"; bluebeam §11: "Measurement captions can display any Markups-list column value." Accept: a viewport with its own scale reports per-viewport measurements; caption test extends the N08 metric pins.

---

## 3. UX/detail pack — small loved details that ride along with bigger items

Each is a ride-along; none justifies its own wave.

1. **Okular's 8 color transforms with per-shortcut binding** (okular §1.1) — ship inside QUEUED-T2-11 as the reference set, not just inversion.
2. **Nitro's saved print presets** (nitro C8) + **masterpdf manual-duplex refeed prompt** (masterpdf §11) — ride QUEUED-T2-10.
3. **Sejda's per-task honest-limit footers** (sejda §3.2) — the retail idiom for CapabilityRegistry disclosure; adopt wherever a task has a real limit (e.g., OCR language packs).
4. **Sejda's scanned-editing refusal modal that says what IS possible in the same breath** (sejda §1.2) — copy for every capability refusal; pairs with M8.
5. **PDF24's trust story as cautionary tale**: a "local" toolbox tool silently uploaded files (11.30.1, pdf24 §3 F1) — our local-processing badge (§9.16) should assert per-workflow locality; PDF24 F1 is the citation for why the badge exists.
6. **Smallpdf's one-line redaction trust claim** ("not a black box; can't be viewed, copied, or recovered" — smallpdf §6 recommendation) — adopt in the redaction completion report via `formatCompletionReport` (§9.13-a seam).
7. **iLovePDF's pattern-chip quick-marks** (Text/Credit Card/Phone/Email as one-tap chips — ilovepdf §1.9 delta) — a preset-picker UX for the redact toolbar; presets already shipped (§9.12-a).
8. **Bluebeam's filtered-annotation dimming + saved named filters + "Export Latest Status Only"** (bluebeam §5) — ride T2-3 (statuses/summary).
9. **Bluebeam's column template "Save to Profile … data written only when used, avoiding dirty saves"** (bluebeam §5, "elegant detail worth copying verbatim") — the persistence contract for any future custom-columns work.
10. **PDF-XChange's print-appearance warning** (10.8) — N35; the M8 pattern applied to print.
11. **Okular's annotation-strip Alt+9…0 working "even with the annotation toolbar hidden"** (okular §1.5) — the keyboard-first bar for N19.
12. **Master PDF's math expressions in coordinate fields** (`(189.55+34.00)*2 pt`, masterpdf §2, "power-user detail almost no competitor has") — cheap property-panel nicety; rides N52-adjacent editing work.
13. **Master PDF's checkbox/radio semantics documented in-app** ("same name + same export value = linked checkboxes" — masterpdf §4) — copy as help text in the field properties panel.
14. **Okular's OCSP opt-out** — N4; also copy its "good but not fully valid" distinct wording (okular §1.8) into signature badges.
15. **Nitro's "no tab switching / no dark mode / no drag pages" gap list** (nitro C8) — market the negatives we already fixed (U06 drag reorder, dark mode, tabs) rather than building more.
16. **PDF-XChange's de-hyphenation on copy** (10.3, §1) — reading nicety for extracted text; rides find/copy work.
17. **Sejda's non-recurring week pass** — N55; pricing instrument only.
18. **Bluebeam's Tool Chest number-key "punch keys"** (§11) — generalized by N19 for any tool, not just punch symbols.
19. **ABBYY's per-task TXT log with uncertain-character counts** (abbyy §10, "quality-audit touch worth copying") — ride batch OCR reporting (pairs with N3/N20).
20. **PDF24's `-skipFilesWithText` honesty** — N3; also its "skip logos/stamps during OCR" heuristic idea (pdfxchange 10.3) as a future preprocessor flag.

---

## 4. Rejected / anti-recommendations (never build)

Strategy line: local-only, zero-egress, honesty surfaces (synthesis.md). Every rejection below
carries the demand data that proves rejecting is right.

1. **Cloud processing / upload companion / secure links.** Upload fear = 10+ r/pdf threads; users DevTools-audit "no upload" claims (r-pdf-mining 4.2, 1o2tr2h); the cap walls are the documented ceiling of the species (sejda F1 3-tasks/hour; ilovepdf F3 caps that survive Premium; smallpdf F1 2-tasks/day). Sources: synthesis §3.1; all task-grid reports.
2. **Cloud AI chat/agents/AI-bloat marketing.** All 10 competitors' AI is cloud-gated; r/pdf's AI mentions are OCR accuracy and form detection, not assistants (r-pdf-mining anti#2; synthesis C13). Local AI stays a quiet capability (ledger F02/F03/D02), never the identity.
3. **Cloud e-sign SaaS / envelope backend.** 8+/16 competitors monetize it behind accounts (synthesis T1-4 evidence list); the local package (QUEUED-PACK-C) ships the workflow without the service. nitro.md: "ship the local package, not the SaaS."
4. **DRM / remote license deactivation / activation-count servers.** Nitro's deactivation letter → BBB grade F (nitro B1–B3); Foxit connectedPDF discontinued (foxit §1.14); PDF-XChange activation phone-home friction (pdfxchange §14). "We cannot turn off your software" is the marketable sentence (N56 ships the promise, not the mechanism).
5. **ECM/DMS/SharePoint/iManage/OA connector suites.** Foxit's enormous integration list = "enormous maintenance for the wrong market" (foxit §1.14 anti; synthesis §3.5).
6. **Free-tier ambush mechanics** (watermarks on save, nags, feature-stripping, review-for-gift). Master PDF's v4→v5 watermark switch still breeds a 7-year fork (masterpdf failure-1); Foxit's nag tax (foxit failure-2); UPDF's review-for-gift program (updf failure-3); PE7 paywalling (pdfexpert F1). r/pdf punishes exactly this (r-pdf-mining 3.1).
7. **3D PDF authoring + geospatial projection math + PDF Portfolios + Liquid-Mode cloud reflow + PostScript/Distiller + droplets.** acrobat.md anti list + synthesis §3.7–3.9; portfolio demand = 1 thread (r-pdf-mining anti#5).
8. **Admin cloud portals / telemetry dashboards.** nitro anti; the IT wedge is N38 (silent MSI + GPO/ADMX + offline license files).
9. **Gating shipped features behind accounts/tiers.** The "Smart Tools" pattern that converted Nitro lovers into complainants (nitro anti; synthesis §3.12); CapabilityRegistry discloses *why*, never upsells.
10. **Full print-production engine + virtual printer driver.** Nitro itself skips the engine (nitro anti); ABBYY *removed* its third-party printer in 2023 (abbyy failure-2); masterpdf.md: "the virtual printer is a big lift not worth it"; C12 graded resolution = validation-and-fixups-lite deferred (N50), separations/ink/traps/driver = no.
11. **Mobile-first / web-first investment.** Browsers own casual viewing (r-pdf-mining 3.2); desktop is the battleground.
12. **AEC Sets/Slip-Sheet/VisualSearch subset.** Persona-excluded by PRD §7.2 (no CAD) — backlog row 70; revisit only on a formal persona addition.
13. **PDF24-style viewer creep** (XFA-fill reader, in-reader object move/delete, fax/backed tray services — pdf24 §1.1/§1.7): deployment reach, not depth; no r/pdf demand signal; tray/autostart hostility is the documented anti-pattern (pdf24 F2).

---

## 5. Tonight-feasible shortlist (NEW S items, ranked — feeds the overnight waves)

Ranked by demand × leverage ÷ size; each is genuinely landable in 1–3 h by one agent on the
current tree (build gates: reuse the house pattern — real fixtures, saved-artifact oracles,
revert-verified anchors, offscreen GUI tests).

1. **N1 Measurement CSV export** — finishes synthesis T1-1's own "ship those two first"; the T1 lane deferred it explicitly. Seams all landed.
2. **N2 XFA honesty banner** — `hasXfaForms` exists; pure CapabilityRegistry disclosure; highest trust-per-line-of-code.
3. **N3 Skip-already-text batch OCR** — two flags over a landed pipeline; direct r/pdf OCR-cluster value.
4. **N5 Reverse page order wire-up** — matrix-annotated one-permutation seam; kills a hidden planned row.
5. **N4 OCSP-offline toggle** — small settings + guard change; strengthens the air-gap promise and the signature-honesty story.
6. **N6 Apply-time redaction scope + repeat-text overlay** — both ride fully landed redaction infra; closes the last bluebeam §6 UX edges.
7. **N27a Word count / document statistics** — trivial over text extraction; visible checkbox feature.
8. **N27b Saved search history** — FindBar QSettings list; masterpdf-parity nicety.
9. **N60 Required-field + no-fields fallback wording** — two message pins over shipped auto-detect/fill paths; r/pdf-visible UX trust.
10. **N35 Print-appearance warning** — M8 disclosure on the print path; small probe + dialog.

Honorable mentions (S but lower leverage): N27c named positions, N27d back/forward history,
N36 selection cycle, N58 shrink-under-H/F. Deferred from tonight: N7 (needs a design call on
appearance-preset storage), N22/N23 (M in practice), N15 (menu wiring deserves its stamp-library
wave).

---

## 6. Confidence

High for status classifications (every SHIPPED/QUEUED row cites a ledger commit or a design
doc; every engine-seam claim cites `src/core/interfaces/` or the matrix). Medium for sizes
(M/L boundaries are estimates). Inherited UNVERIFIED marks: nitro OCR language count (nitro C7),
foxit booklet/N-up (foxit §1.11 UNVERIFIABLE), pdfgear redaction depth (pdfgear §6), abbyy
GlyphRecovery +36% and 99.8% claims (abbyy §1, vendor-internal), iLovePDF server-side quality
(ilovepdf §6). No claim from a source report is upgraded by this document.
