# Current Evidence Ledger — 2026-09-05

Companion to `GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md` and the historical
`COMPETITIVE-PARITY-AUDIT-2026-07-01.md`. One row per finding/package. States:
open / implementing / implemented-awaiting-review / verified / partial / unavailable / blocked-with-reason.
"Verified" requires an independent review of the acceptance evidence (the 2026-09-05 review session
verified commits through `d03d6e9` on main; everything after that is implemented-awaiting-review).

Dated corrections to stale historical claims (superseded by HEAD):
- Compare IS reachable (entry point wired, §9.10). OCR language selection exists (interactive + batch).
- Form auto-detect is a real content-aware heuristic, not three dummy fields (88d4286).
- JPEG re-encoding exists and is tested (4d8d3fb, TestCompressJpegReencode).
- Form-add undo exists despite an obsolete no-op comment (EditFormFieldCommand — see R02 for its gaps).
- README's 14-test count and the July plan's 39-test count are stale; current registration is 85 CTest
  targets on this branch (volatile — do not hardcode counts in prose going forward).

## September repair packages (F01–F12)

| ID | Parity § | User-visible acceptance | Status | Code | Regression test | Evidence | Commit | Residual limitation |
|----|----------|------------------------|--------|------|-----------------|----------|--------|---------------------|
| F01/R01 | forms | Failed form write leaves source PDF byte-identical; same-file add survives reopen | implemented-awaiting-review | FormManager.cpp save boundary, AddFormFieldCommand | TestFormSafety | runtime repro (15,257→0 bytes) | 5c8e111 | — |
| F09/R02 | forms | Undo restores original value/tooltip/required incl. empty states | implemented-awaiting-review | IFormManager readFieldSnapshot/applyFieldSnapshot, FormManager, EditFormFieldCommand | TestFormUndo | trace | 5c8e111 | rename/placeholder/regex persistence contract out of scope |
| F02/R03 | AI | Timeout/destroy cannot produce late callback access; exactly one result | implemented-awaiting-review | OllamaProvider.cpp owned worker state | TestOllamaProvider | trace | 9d53957 | no sanitizer run claimed |
| F03/R04 | AI | Loopback guard parses hosts; 127.audit.invalid rejected | implemented-awaiting-review | OllamaProvider isAllowedEndpoint/resolveEndpoint | TestOllamaProvider (45 R04 cases) | validator probe | 8133f58 | redirects disabled (ManualRedirectPolicy); no sanitizer run claimed |
| F05/R05 | OCR | 1-bit binarization keeps paper light, strokes dark | implemented-awaiting-review | OcrPreprocessor 1-bit mapping | TestOcrPreprocessor (5 fixture classes) | runtime repro | 87d98f8 | — |
| F10/R06 | OCR | Deskew estimates on 1bpp; inverse transform maps boxes back | implemented-awaiting-review | OcrPreprocessor 1bpp estimator + composed inverse transform | TestOcrPreprocessor (±3° fixtures) | runtime repro | 8e503b4 | — |
| F11/R07 | OCR | Every terminal OCR outcome leaves the panel in a recoverable state | implemented-awaiting-review | OCRMode ReviewState enum + transitionTo, EditController job generation/seams | TestOcrReviewLifecycle (15) | trace | a7b3c32 | document revision proxied by path+page count |
| F04/R08 | OCR | Reviewed word survives saved-PDF text extraction; correct page saved | implemented-awaiting-review | OcrReviewSession.h, OCRMode word inspector, EditController buildReviewedPageOcrResult, PdfEditorEngine Unicode writer | TestOcrReviewLifecycle (+9 incl. PDFium extraction) | trace | 567b906 | revision proxy by path+page count; per-word undo absent; whole-document OCR separate |
| F07/R09 | conv | Subset-font/Unicode text extracts correctly for Word/Excel/CSV | implemented-awaiting-review | PDFium-decoded extraction in ConversionManager | TestExportPathBadge (subset-font/WinAnsi/multiline/image-only) | runtime repro | 466c706 | BT row-order fix rode in with 452bfa2 |
| F08/R10 | conv | Unavailable native format fails before writing; UI+batch gating consistent | implemented-awaiting-review | ConversionManager capability queries + ConvertController gating | TestExportPathBadge (reject-before-truncate pinning) | runtime repro | 06b542d | in-house OOXML is the native path; HTML/CSV keep true names |
| F06/R11 | compare | Added/removed pages appear in tree, navigation, report | implemented-awaiting-review | DiffEngine PageChange model, CompareMode tree/filters/reports, CompareWidget anchors | TestDiffEngine (+7), TestCompareEntry (+6) | runtime repro | 83e7c8a | middle-insertion alignment explicitly open; repeated-identical tie-break documented |
| F12/R12 | compression | Unsupported passes cannot be selected or claimed run | implemented-awaiting-review | CompressDialog unsupportedPassExplanation seam + gating | TestCompressDialogHonesty (5) | trace | f2fab98 | completion-message honesty source-verified only; estimate stays an estimate by design |

## Already-landed parity work on this branch (July audit P0 items — selected rows)

| ID | § | Acceptance | Status | Commit(s) | Test |
|----|---|-----------|--------|-----------|------|
| §9.13 JPEG re-encode | compression | Quality/DPI controls re-encode DCTDecode images | implemented-awaiting-review | 4d8d3fb, 6cba489 | TestCompressJpegReencode |
| §9.13 signed-doc guard | compression | optimizeDocument refuses signed docs | implemented-awaiting-review | 45aa606 | TestOptimizeSignedGuard |
| §9.4 orientation | OCR | orientDetect rotates 0/90/180/270 scans | implemented-awaiting-review | 97f656d | TestOcrPreprocessor |
| §9.4 preprocessing prefs | OCR | Deskew/Binarize/Denoise checkboxes honored + persisted | implemented-awaiting-review | b79415a | TestOcrPreprocessPrefs |
| §9.8 page-list redaction | redaction | Mark All honors explicit page list; invalid range marks nothing | implemented-awaiting-review | fa3b957 | TestRedactMarkAll |
| §9.8 sanitize bundle | redaction | Apply flow offers default-ON sanitize of the saved copy | implemented-awaiting-review | b64aef2 | TestRedactMarkAll |
| §9.7 signature picker | signatures | Draw/Type/Upload modes persist as real PDF annots | implemented-awaiting-review | 8a278db | TestSignaturePicker |
| §9.7 validity badges | signatures | On-page per-signature state badge (view-layer only) | implemented-awaiting-review — badges now ANCHORED to real field rects via signatureFieldAnchors | 10efbd5 + badge-anchoring commit | TestSignatureBadges (15) |
| §9.7 signature appearance | signatures | Cryptographic signature renders visible /AP /N (ETSI layout, auto-fit, cert CN/date/reason/location) | implemented-awaiting-review — full-suite run was disk-blocked (62 Not Run); 13/13 own tests + signing-lane suites green | 61fac01/cec39a7 | TestSignatureAppearance (13) |
| §9.1 two-page overlays | viewing | Annotations + search highlights visible in two-page mode | implemented-awaiting-review | ef02541 | TestTwoPageOverlay |
| §9.10 change filter | compare | Change-type toggles gate the CHANGES tree | implemented-awaiting-review | 65395da | TestCompareEntry |
| §9.16 local badge | import/export | Local-processing notice on exports + import cards | implemented-awaiting-review | d03d6e9/0a93f62 | TestExportPathBadge |
| §9.5 in-house OOXML | import/export | Real .docx/.xlsx written in-house (no HTML/CSV mislabeling) | implemented-awaiting-review | 452bfa2 | TestExportPathBadge (+13) |
| §9.14 async reading order | accessibility | Check runs off the GUI thread; repaired test | verified (2026-09-05 review) | 1da4ffe | TestReadingOrderAsync |
| §9.12 batch flake | batch | TestBatchMode deterministic | verified (2026-09-05 review) | 7de331b | TestBatchMode |
| enum-bound hardening | serializers | All persisted ToolMode ordinals round-trip; bound single-sourced | implemented-awaiting-review | 05a3336 | TestAnnotationDjot |

## UI packages (U01–U08) — implementation gated on engine repairs per the plan (all landed 2026-09-07 unless noted)

| ID | Surface | Scope | Status |
|----|---------|-------|--------|
| U01 | Welcome | Responsive card grid, theme tokens, no clipping | implemented-awaiting-review — geometric overflow/reflow tests; DPI-matrix + live re-skin checks manual |
| U07 | Comments | Active-filter summary, count, clear, table view, CSV export over existing records | implemented-awaiting-review — 7 tests incl. numeric page-sort defect fix + reply-nesting regression; CSV escaping pinned; page-level navigation only (geometry focus API is a follow-up) |
| U02 | Navigation | One clear entry per task; honest status bar | implemented-awaiting-review — TaskNav table + TaskStateSync single writer (3-layer OCR drift resolved: entry opens the verify screen everywhere), numbering gone, collapsible ribbon, slim 2-tier status bar; 4 new test files, 100/100 suite; theme pass + remaining mode-local pills deferred |
| U03 | OCR verify | Source-image review workflow; one confidence function | implemented-awaiting-review — 15 tests (confidence boundary matrix, magnifier pixel-identity, selection funnel, lifecycle-following nav); real-engine e2e run UNVERIFIED (needs GUI+models) |
| U04 | Compare | Results drive both views | implemented-awaiting-review — filter-aware anchors (CompareChangeFilter/setChangeFilter/changeCount), gated reports, linked scroll + placeholders + swap; 13 new tests; two-page-mode scroll linking deferred (PdfViewerWidget seam, badge-anchoring lane) |
| U05 | Redaction | Explicit output/failure states; one controller flow | implemented-awaiting-review — RedactOperation transaction (7 stages, 4 outcomes), SafeSave shared primitive, pre-mutation dialog; 13 transaction tests + SHA-256 source invariance; 96/96 suite |
| U06 | Pages | Selection visibility, insertion indicator, keyboard moves | implemented-awaiting-review — PLUS fixed a probe-verified defect: Qt InternalMove inserts-before-removing, so the drag snapshot captured the duplicate and the atomic permutation command never fired (drag reorder was silently dead in the live path); capture moved to rowsAboutToBeInserted + removal reconciliation. 6 new tests |
| U07 | Comments | Filter summary, count, clear action on the existing records | implemented-awaiting-review — summary+count+clear, table view of the same records, RFC-4180 CSV export, numeric page-sort defect fixed; page-level navigation only (geometry focus is a follow-up) |
| U08 | Capabilities | Pre-execution capability/scope disclosure across workflows | implemented-awaiting-review — CapabilityRegistry (CapId probes, cache/invalidate, whyNot+alternative enforced in query) in src/core/Capability, injected via AppContext/Bootstrapper; consumers: CompressDialog (registry wording + applyToWidget MRC gate), ConvertController disclosure at selection, BatchMode per-item pre-flight, SignaturePicker kind label; 16+3 tests; 101/101 suite |

## July-parity P1 follow-ups (post-September wave)

| ID | § | Item | Status | Commit |
|----|---|------|--------|--------|
| §9.8-a | §9.8 | Cancel/Back control in RedactMode (exitRequested contract, marks kept) | implemented-awaiting-review | bcef6ad |
| §9.8-b | §9.8 | Optional overlay text on burn-in boxes (7pt white, centered, skip-too-small; candidate swap fix for in-place save corruption) | implemented-awaiting-review | 24c479d |
| §9.8-c | §9.8 | Word-list import for pattern redaction (256KB cap, escaped alternation, review-before-mark) | implemented-awaiting-review | a9a3eda |
| §9.12-a | §9.12 | Named redaction presets (Email/Phone-US/SSN checkboxes) + configurable Optimize DPI (Low/Medium/High presets, clamped 36-600) | implemented-awaiting-review | b43ef08 |
| §9.14-a | §9.14 | Reading-order tolerance named (kReadingOrderSlotTolerance=2) with honest heuristic framing + boundary tests (displacement 2 not flagged, 3 flagged) | implemented-awaiting-review | c862307 |
| §9.16-a | §9.16 | Bookmark/hyperlink round-trip contract pinned: plain saves preserve outlines+links; sanitize stripping is intentional-and-visible, never silent | implemented-awaiting-review (tests-only characterization) | 28e8df6 |
| §9.16-b | §9.16 | Unified open/drag-drop routing for Office and image files (routeForFile/planDrop seams, drop matrix pinned, conversion output tracked-temp; cards remain as explicit entry points) | implemented-awaiting-review — 6 routing tests; File>Open filter text and QDropEvent synthesis documented residuals | 74a840e |
| §9.7-a | §9.7 | Initials variant (monogram seam, 4th picker tab, typed-placement reuse) | implemented-awaiting-review | be47cce → 8dae489 |
| §9.7-b | §9.7 | Session signature cache (reuse-last checkbox, cleared on document switch) | implemented-awaiting-review | 706a60c (merged c2e127b) |
| §9.7-c | §9.7 | SignOutcome degradation surfaced at signing time (DSS/B-LTA missing-piece wording, Retry) | implemented-awaiting-review | 22a7b66 (merged c2e127b) |
| §9.9-a | §9.9 | Split: one output per comma-separated range segment (<stem>_part{n}.pdf) | implemented-awaiting-review (parsePageRangeSegments seam; overlap duplicates tested; single-segment unchanged) | c7fc7ef |
| §9.9-b | §9.9 | Page Labels: writer + catalog /PageLabels number tree (writeNumberTree, SafeSave candidate/commit, grid context-menu entry) — groundwork seam d2f4484 consumed end-to-end | implemented-awaiting-review — writer 16/16 tests (decimal/roman readback, replace-existing, /St offset, invalid-args); viewer read API absent in PoDoFo 1.1 (tree decoded directly); multi-range trees + dirty-doc refusal documented | 32c7780 |
| §9.9-c | §9.9 | Bates numbering continuity across documents (3-arg applyBatesNumbering overload returning lastNumberOut; batch UI list; per-file <stem>_bated.pdf outputs; counter continuity contract) | implemented-awaiting-review — 6 tests (2-doc + 3-page continuity via PDFium text extraction, empty-range counter safety, engine-interface overload); batch output name fixed, synchronous batch loop documented | 69cb6e4 |
| §9.12-b | §9.12 | exportPdfA maps PDF/A-2U/3-U for real (combo levels were silently downgraded to 1B) | implemented-awaiting-review | 0364f48 |
| CMP-align | compare | Middle-insertion alignment via deterministic page fingerprints | implemented-awaiting-review — exact-fingerprint LCS (tie-break pinned: earliest unmatched doc2 occurrence surfaces as PageAdded), fuzzy pass retained as fallback for fingerprint-less pages; ledger correction: the blanket trailing-chain claim did NOT reproduce for distinct-text insertions — the genuine defect was near-twin pages (Jaccard ≥ 0.80 mispairing), now pinned | 808965b |
| §9.13-a | §9.13 | Measured before/after size readout (formatCompletionReport seam; delta + not-smaller note; larger-than-input fallthrough bug caught pre-commit) | implemented-awaiting-review — dialog wiring source-verified (modal not headlessly drivable) | a21ecc8 |
| §9.10-a | §9.10 | End-to-end compare integration tests on real DiffEngine+Widget+Mode (identical/edit/added-page/reports/filters; alignment-similarity and PDFium-NUL behaviors pinned as designed) | implemented-awaiting-review (tests-only) | bc27cf2 |

## Review-pass findings (REMOTE-PARITY-REVIEW-2026-09-06, at 4761443)

| ID | Finding | Status | Commit |
|----|---------|--------|--------|
| D06 | Capability probe reported RapidOCR Available on a zero-byte detector stub | implemented-awaiting-review — probeRapidModelsIn demands non-empty detector+recognizer+vocabulary, classifier optional/disclosed; applyToWidget reversibility; 3 dir-fixture tests | (this commit) |
| D07 | Security redaction entry path defaulted sanitize OFF (contradicted the default-ON contract; Redact mode defaulted ON) | implemented-awaiting-review — shared kDefaultSanitizeOn policy seeded into both entry paths, static_assert guard | (this commit) |
| D03 | Qt-only preprocessing failed to compile — carryResolution hidden by HAS_TESSERACT | implemented-awaiting-review — carryResolution moved to the Qt-only anonymous namespace; both-config syntax proof (+fsyntax-only) and DPI/polarity tests | 603d984 |
| parseJson | VeraPdfValidator::parseJson was schema-stale (real CLI → isValid=false, violations=[] for EVERY document) | implemented-awaiting-review — parseJson rebuilt to real veraPDF 1.26–1.30 schema (validationResult array, per-profile integer counts, taskException); offline fixtures + CLI cross-check | 1582f46 |
| E-1-residual | PDF/A conformance validation was unverified; exported artifacts lacked DestOutputProfile ICC + complete CIDSet; veraPDF CLI wired as optional integration | implemented-awaiting-review — 500-byte sRGB ICC v2.1 DestOutputProfile attached at every level (veraPDF-verified: 1b profile-free FAILED 6.2.3.3-3, ISO 19005-1 permits but does not exempt); complete CIDSet streams derived from /W; all five flavours FULLY CONFORMANT under veraPDF 1.30.2 (0 failed rules, mismatch control kept); parseJson schema defect fixed separately (1582f46) | 0fe5953 |
| U02-theme | Navigation/status surfaces inherit default styling (visual pass unwritten) | implemented-awaiting-review — U02 THEME PASS QSS sections in all three themes (ribbon tab row/collapsed label/chevron, Tools chooser, status details popup), GpTheme tokens only; pixel guard test proves bg2/bg1 token consumption on live widgets after applyTheme | 9ff9e7c |
| N08 | Overlay label glyphs extended outside the minimum-height black box (3.046pt at 9pt min) | implemented-awaiting-review — metric-derived minimum (ascent+descent=6.566pt @7pt Helvetica), extent-centered baseline, SetPrecision(8); skip-below contract kept | 9a912b9 |

## Latest-review findings (LATEST-QUALITY-REVIEW-2026-09-07, N01–N10 at 0caa45e)

| ID | Finding | Status | Commit |
|----|---------|--------|--------|
| N03 | PDF/A-3U (and L3B) wrote PDF 2.0; PDF/A-3 is PDF 1.7-based | implemented-awaiting-review — whole switch audited, version asserts added for 2B/2U/3B/3U | f9f3a95 |
| N02 | Alphabetic page labels used spreadsheet scheme (28=ab); ISO 32000 Table 159 repeats the letter (28=bb) | implemented-awaiting-review — letters() corrected, tests re-pinned over review boundaries 1/25-28/52-54 | c764f73 |
| N01 | Visible Initials/Upload tabs dispatched opposite kinds | implemented-awaiting-review — tab→kind resolved via page widgets, never indices; upload/monogram gates pinned | 47d2fe1 |
| N05 | Session cache A→B→A retained per-document signature; reuse gate left OK disabled | implemented-awaiting-review — cache rides dirtyChanged on real path change; reuse+cache opens the OK gate | 1d4daca |
| N06 | Signing retry lost the appearance image (consume-once slot) | implemented-awaiting-review — explicit signDocumentWithAppearance entry points; request captures source+image once, both attempts embed | f8704aa |
| N09 | Real split execution produced no files (source-resident engine + AR-4 guard rejected every write) | implemented-awaiting-review — writeDocumentFromPages + SafeSave candidate/validate/commit; real end-to-end test drives the production caller | 6ee8eb7 |
| N10 | Four regression checks were never moc-registered (3 CapabilityRegistry D06 slots + D07 policy slot defined outside slots sections) | implemented-awaiting-review — moved into private slots, mutation-proven to execute (19/19 + 14/14 totals) | 866321d |
| N04 | Security entry dropped the overlay label (request built by hand, 5 of 6 fields) | implemented-awaiting-review — shared redactRequestFromPlan seam now carries all 6 fields; both entry paths consume it; e2e label burn-in pinned | a7a1bf0 |
| N07 | Redaction exit left the marking tool armed | implemented-awaiting-review — Cancel disarms Redact→HandTool before exitRequested; GpMainWindow relay resets the shared viewer too; marks kept | 61e51da |

## PARITY-BRANCH-REVIEW findings (V01–V06)

| ID | Finding | Status | Commit |
|----|---------|--------|--------|
| V03 | Text-run grouping collapsed table columns (runs split only on line breaks) | implemented-awaiting-review — PdfiumBackend splits runs on >1em gaps, wide space glyphs, font changes; ConversionManager deriveColumns() clusters x-anchors into true columns (CSV interior gaps, XLSX true-column addressing); 13-test suite | 0420cb5 |
| V05 | OCR revision validation accepted same-path, same-count changes | implemented-awaiting-review — DocumentSession::mutationRevision() advanced at every mutation/undo/redo boundary; review session captures dispatch-time revision; mismatch → Stale/reject; -1 legacy fallback | ff8c1b9 |
| V04 | Ordinary text edits became structural page changes (alignment leftovers below the Jaccard 0.80 floor surfaced as PageRemoved+PageAdded on top of the text diff; Apple→Orange one-page probe = 2 structural changes) | implemented-awaiting-review — third alignment stage: leftover pairs align in order as ALIGNED MODIFIED pages, structurally silent (content lives in result.pages page rows, so tree/filters/navigation/reports agree); one-sided remainder stays PageRemoved/PageAdded; exact/fuzzy anchors untouched (middle insertions, duplicates, reorders keep their pins); insertionWithTextEdit… re-pinned (old assertions encoded the below-floor remove+add policy V04 overturns); new pins onePageTextEdit… + middlePageRewrite…; revert-verified stash→3 fail→pop→29 pass | 250bbad |
| V06 | Auto-detected fields described as undoable but bypassing history (commands redone and discarded; "undo ... as needed" + "document unchanged" claims false) | implemented-awaiting-review — AutoDetectPlacement seam (pdfws_commands): direct redo() per placement (succeeded() readable per ownership rules), successes join ONE compound on the shared AppContext stack (single Undo removes all, Redo re-places), failures never join and are never dereferenced post-push, counts from reloadRequested; messaging rebuilt (partial names counts+Undo, only total failure may claim unchanged); TestFormSafety compound-undo/reopen + injected-failure-after-success + messaging pins (12/12); mutation-verified | 0696036 |
| D04 (CODE-REVIEW-2026-09-06) | OCR word overlays offset from the displayed scan (paint subtracted the letterboxed origin before scaling; wordIdAt used the correct inverse — overlays and clicks disagreed for any aspect mismatch) | implemented-awaiting-review — paint = imgRect.topLeft() + scale×imagePos (exact inverse of wordIdAt); pen widths stay device-pixel (2px selection ring intentional); pixel-probe pins for horizontal+vertical letterboxing, resize/zoom, selection ring, removed-word styling, click-on-painted-box consistency; revert-verified stash→2 fail→pop→pass | c84e55e |
| D02 (redaction) | RedactOperation worker state ownership | implemented-awaiting-review | 3c3be82 |
| D01 | Retry-sanitize empty destination path | implemented-awaiting-review (recovery repair) | 8bd01d5 |

## CODE-REVIEW-2026-09-06 findings (D01–D05, redaction + AI boundaries)

| ID | Finding | Status | Commit |
|----|---------|--------|--------|
| D02 (AI) | OllamaProvider caller-boundary lifetime: no late callback into destroyed owner, exactly one result per request, retry on same instance | implemented-awaiting-review — 3 caller-boundary pins (PanelLikeOwner harness mirroring AIChatPanel), mutation-tested | 4d45769 |
| D02 (redaction) | RedactOperation worker does not own the state it uses (QPointer re-check; UI-parented destructor uncoordinated) | **open** — next dispatch to the redaction lane (files now free) | — |
| D01 | Retry Sanitize receives an empty sanitizedDestination after partial failure | implemented-awaiting-review — RedactResult::intendedSanitizedDestination travels with the result; presenter Retry targets the intended path and repairs the banner | 8bd01d5 |
| D02 (redaction) | RedactOperation worker does not own the state it uses | implemented-awaiting-review — shared ExecutionState (request/cancel/seams) held by worker shared_ptr; UI destruction neither crashes nor cancels in-flight runs; pre-fix UAF crash reproduced deterministically (0xc0000005 in maybeSignalConnected on the worker) | 3c3be82 |

## Newly discovered engine defect (exposed by U05 fixtures)

| ID | Surface | Finding | Status | Evidence |
|----|---------|---------|--------|----------|
| E-1 | redaction excision backend | Redacting one line corrupts a byte-ADJACENT same-stream Tj (observed 'PUBLIC_KEEP_TEXT' -> 'PUBLIC_KEEP_XEXX') | **fixed** — root cause: PdfString::GetString() forces lazy glyph-encoding evaluation IN PLACE on the COW-shared operand; the stream rebuild re-emitted the transcoded (corrupted) form. getEncodedStringWidth now evaluates a deep raw copy (FromRaw), so un-redacted operators re-emit byte-exact; Edact-Ray gap-width property preserved | 2c5a0d6 — TestExcisionCorruption (same-page + 4-op stream, hex-level byte-identity asserts); 108/108 |

## Known test-infrastructure facts (Q02)

- A fresh checkout/worktree needs three untracked binary trees to build and run:
  `third_party/podofo/install/bin`, `third_party/pdfium/bin`, `onnxruntime-win-x64-1.17.3` —
  without them CMake silently falls back to MSYS2 podofo 0.10.4 (API mismatch) and tests exit
  0xc0000135. Documented in 05a3336's commit message; a bootstrap script remains open work.
- Full-suite runs raced twice with the post-commit graphify rebuild hook (transient failures
  resolved on rerun). Gate rule: ctest only when `cmake --build` is a no-op / hook log idle.
