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
| §9.7 validity badges | signatures | On-page per-signature state badge (view-layer only) | implemented-awaiting-review | 10efbd5 | TestSignatureBadges |
| §9.7 signature appearance | signatures | Cryptographic signature renders visible /AP /N (ETSI layout, auto-fit, cert CN/date/reason/location) | implemented-awaiting-review — full-suite run was disk-blocked (62 Not Run); 13/13 own tests + signing-lane suites green | 61fac01/cec39a7 | TestSignatureAppearance (13) |
| §9.1 two-page overlays | viewing | Annotations + search highlights visible in two-page mode | implemented-awaiting-review | ef02541 | TestTwoPageOverlay |
| §9.10 change filter | compare | Change-type toggles gate the CHANGES tree | implemented-awaiting-review | 65395da | TestCompareEntry |
| §9.16 local badge | import/export | Local-processing notice on exports + import cards | implemented-awaiting-review | d03d6e9/0a93f62 | TestExportPathBadge |
| §9.5 in-house OOXML | import/export | Real .docx/.xlsx written in-house (no HTML/CSV mislabeling) | implemented-awaiting-review | 452bfa2 | TestExportPathBadge (+13) |
| §9.14 async reading order | accessibility | Check runs off the GUI thread; repaired test | verified (2026-09-05 review) | 1da4ffe | TestReadingOrderAsync |
| §9.12 batch flake | batch | TestBatchMode deterministic | verified (2026-09-05 review) | 7de331b | TestBatchMode |
| enum-bound hardening | serializers | All persisted ToolMode ordinals round-trip; bound single-sourced | implemented-awaiting-review | 05a3336 | TestAnnotationDjot |

## UI packages (U01–U08) — all open; implementation gated on engine repairs per the plan

| ID | Surface | Scope | Status |
|----|---------|-------|--------|
| U01 | Welcome | Responsive card grid, theme tokens, no clipping | implemented-awaiting-review — geometric overflow/reflow tests; DPI-matrix + live re-skin checks manual |
| U07 | Comments | Active-filter summary, count, clear, table view, CSV export over existing records | implemented-awaiting-review — 7 tests incl. numeric page-sort defect fix + reply-nesting regression; CSV escaping pinned; page-level navigation only (geometry focus API is a follow-up) |
| U02 | Navigation | One clear entry per task; honest status bar | implemented-awaiting-review — TaskNav table + TaskStateSync single writer (3-layer OCR drift resolved: entry opens the verify screen everywhere), numbering gone, collapsible ribbon, slim 2-tier status bar; 4 new test files, 100/100 suite; theme pass + remaining mode-local pills deferred |
| U03 | OCR verify | Source-image review workflow; one confidence function | implemented-awaiting-review — 15 tests (confidence boundary matrix, magnifier pixel-identity, selection funnel, lifecycle-following nav); real-engine e2e run UNVERIFIED (needs GUI+models) |
| U04 | Compare | Results drive both views | implemented-awaiting-review — filter-aware anchors (CompareChangeFilter/setChangeFilter/changeCount), gated reports, linked scroll + placeholders + swap; 13 new tests; two-page-mode scroll linking deferred (PdfViewerWidget seam, badge-anchoring lane) |
| U05 | Redaction | Explicit output/failure states; one controller flow | implemented-awaiting-review — RedactOperation transaction (7 stages, 4 outcomes), SafeSave shared primitive, pre-mutation dialog; 13 transaction tests + SHA-256 source invariance; 96/96 suite |
| U06 | Pages | Selection visibility, insertion indicator, keyboard moves | implemented-awaiting-review — PLUS fixed a probe-verified defect: Qt InternalMove inserts-before-removing, so the drag snapshot captured the duplicate and the atomic permutation command never fired (drag reorder was silently dead in the live path); capture moved to rowsAboutToBeInserted + removal reconciliation. 6 new tests |
| U07 | Comments | Filter summary, count, clear action on the existing records | open |
| U08 | Capabilities | Pre-execution capability/scope disclosure across workflows | implemented-awaiting-review — CapabilityRegistry (CapId probes, cache/invalidate, whyNot+alternative enforced in query) in src/core/Capability, injected via AppContext/Bootstrapper; consumers: CompressDialog (registry wording + applyToWidget MRC gate), ConvertController disclosure at selection, BatchMode per-item pre-flight, SignaturePicker kind label; 16+3 tests; 101/101 suite |

## Newly discovered engine defect (exposed by U05 fixtures)

| ID | Surface | Finding | Status | Evidence |
|----|---------|---------|--------|----------|
| E-1 | redaction excision backend | Redacting one line corrupts a byte-ADJACENT same-stream Tj: the following line's glyph bytes are overwritten (observed 'PUBLIC_KEEP_TEXT' -> 'PUBLIC_KEEP_XEXX' — the two 0x54 'T' bytes became 0x58 — while a line 150pt away in the same stream was also hit; a second-page line survives). Both the mark-based and operation paths share PoDoFoBackend::applyRedactions/redactCanvasRecursively, so this is PRE-EXISTING (all prior fixtures were single-line and could not see it) | open — engine fix needed in redactCanvasRecursively text surgery; U05 fixture pins per-PAGE survival honestly | TestRedactMarkAll two-page fixture, 436aaa8 |

## Known test-infrastructure facts (Q02)

- A fresh checkout/worktree needs three untracked binary trees to build and run:
  `third_party/podofo/install/bin`, `third_party/pdfium/bin`, `onnxruntime-win-x64-1.17.3` —
  without them CMake silently falls back to MSYS2 podofo 0.10.4 (API mismatch) and tests exit
  0xc0000135. Documented in 05a3336's commit message; a bootstrap script remains open work.
- Full-suite runs raced twice with the post-commit graphify rebuild hook (transient failures
  resolved on rerun). Gate rule: ctest only when `cmake --build` is a no-op / hook log idle.
