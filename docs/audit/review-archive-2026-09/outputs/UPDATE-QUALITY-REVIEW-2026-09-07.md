# GlyphPDF update quality review — 7 September 2026

Snapshot report at `95676e6`. The [latest review at `0caa45e`](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/LATEST-QUALITY-REVIEW-2026-09-07.md) reviews the fixes that arrived during this pass and overrides the D01/D05/D06/D07 dispositions below. The N01–N09 instructions remain applicable.

**The updated branch builds and passes all 107 CTest targets, but the new work is not ready for blanket acceptance.** This review identifies nine actionable findings: eight in the new behavior and one existing split-engine integration defect exposed while checking the new split package. The earlier V01–V06 and D01–D07 findings remain open. The most urgent new acceptance failures are the swapped signature tabs, invalid PDF/A-3U version mapping, and split operations producing no output with the real engine.

`git fetch origin` succeeded. The reviewed snapshot is **`origin/feat/parity-glm` at `95676e6a36ae9aadb2c98c39d47e55d4b0a9cfdd`**. The new-update comparison is `4761443278750390c36dec0c1fe77cde775ced20..95676e6a36ae9aadb2c98c39d47e55d4b0a9cfdd`: **44 files, 3,800 insertions, 157 deletions**. The full requested tree comparison against `main` uses baseline `703fa34ece32733ea3b2093da94fa1aed94e1afc`; both exact-hash diffs are archived.

Source came from an isolated Git archive, not the older installed application or concurrent uncommitted work. No production source, repository ledger, branch, or installation was changed. The `pdf-parity` checkout had only the same six untracked entries at the beginning and end: four build directories, `rma_diag.txt`, and `tests/TestImageDedup.cpp`. Those entries were excluded.

This report reviews all 14 new ledger rows and carries forward an explicit disposition for the preceding 38 unique rows. It follows `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`. **Verified** is reserved for independently reviewed, stated acceptance contracts in the tested configuration. **Partial** means a concrete failure or missing part of the claimed contract. **Implemented-awaiting-review** is retained where required acceptance remains deferred. These are reviewer recommendations; they are not automatic edits to the ledger.

## Evidence and limits

- Fresh isolated application/test build: **635 build steps, exit 0**; GCC 16.1, Qt 6.11, CMake/Ninja, Debug with `-g0`, Tesseract enabled. The build emitted 11 warnings, so it was not warning-free.
- Used the three vendor binary trees documented in commit `05a3336`, including matching PoDoFo/PDFium headers and import libraries. A clean checkout still needs this dependency bootstrap; it is not independently reproducible from tracked source alone.
- Full CTest run: **107/107 passed on the first run, 65.28 seconds**. Detailed Qt logs were captured for 24 selected targets; logging arguments in the isolated generated test registration were then restored. Production source and test assertions were unchanged.
- Captured Qt logs show three skipped RapidOCR initialization/recognition cases because models are missing, and one skipped revoked-certificate case requiring `GLYPH_TESTING` hooks. This is a statement about captured detailed logs, not a guarantee that no other target has an internal skip. Qt totals include setup/cleanup, and CTest target counts are not counts of accepted requirements.
- New probes used production libraries, offscreen Qt widgets, generated PDFs, the repository's test certificate, and PDFium inspection of saved outputs. Existing probes were rebuilt against this exact commit. No private document was used.
- No installed-app walkthrough, full partial-LTV retry dialog exercise, sanitizer run, alternate complete dependency matrix, external Office round-trip, or veraPDF validation was performed. Live UI and reinstall remain deferred as requested.

The [evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/update-quality-evidence-2026-09-07.zip) contains the pinned ledger, both diffs, manifest, build/test logs, detailed Qt results, probe sources and outputs, and disposable fixtures. The [implementation companion](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md) points implementers to this review.

## Findings and bounded implementation instructions

All source locations below are repository-relative at `95676e6`. Inspect the current function before editing if the branch has moved. P1 means fix before relying on the affected workflow; P2 is a material correctness issue to address next. Preserve passing functionality and existing persistence/enum contracts.

### N01 — P1: Initials and Upload use opposite tab indices

**Runtime reproduced.** `src/ui/SignaturePicker.cpp:243,263` constructs the visible tabs as Draw=0, Type=1, Initials=2, Upload=3. `showTab` at lines 319–326, `updateAccept` at 388–405, and `onAccepted` at 422–438 still treat Upload as 2 and Initials as 3.

The independent probe selects the actual tab labeled Initials, enters “Jane Doe,” and finds OK disabled. Switching to Upload without uploading anything enables OK and accepts `Kind::Initials`. An Upload tool entry can also select the visible Initials page because `showTab` shares the stale mapping. This regresses the previously accepted picker entry flow even though underlying graphic persistence remains implemented.

**Why the tests miss it:** new tests use `showTab(Initials)` and find/edit the initials child even when that child belongs to a hidden page. The helper and assertions share the same wrong index assumption.

**Implement:** use one mapping from kind to the actual page widget, or consistent named indices in construction and all dispatch paths. Do not add a second dialog or change persisted `ToolMode` ordinals. Gate acceptance on a valid resulting image for Initials; a nonempty input containing no usable initials must not accept a null image.

**Acceptance:** select each visible tab by its displayed page and interact with only its visible controls. A valid initials name enables OK and produces Initials; a valid upload enables OK and produces Upload; empty/unreadable input stays disabled. Assert both the current visible page and accepted kind when testing `showTab`. Save/reopen each graphic. Keep Draw/Type and serialization checks.

### N03 — P1: PDF/A-3U is written with PDF 2.0

**Runtime reproduced and standards cross-checked.** `src/engines/podofo/PoDoFoBackend.cpp:1911–1914` maps the new level 5 case to `PdfALevel::L3U` and `PdfVersion::V2_0`. A real export returns success; reopening confirms L3U metadata and PDF version 2.0. The older 3B branch at 1901–1903 has the same version mistake.

PDF/A-3 is based on PDF 1.7; PDF/A-4 is the PDF 2.0 family. Merely writing a PDF/A identifier does not make the document conformant. [PDF Association standards overview](https://pdfa.org/pdf-standards/), [veraPDF validation documentation](https://docs.verapdf.org/cli/validation/).

**Why the tests miss it:** the new batch test at `tests/TestBatchOpsCoverage.cpp:404–430` exercises 2U and checks metadata. It does not exercise 3U or validate actual conformance.

**Implement:** correct both 3B and 3U to the appropriate PDF 1.7 version and make the complete supported mapping explicit in one table/switch. Validate the exported artifact, including the requested conformance level. Font embedding, Unicode mapping, output intents and other requirements must be met or reported as unavailable/failed; do not label an XMP-only declaration as a validated conversion.

**Acceptance:** exercise every selectable PDF/A level through its real caller; reopen and assert header/catalog version and conformance metadata, then run a suitable validator on representative positive and negative fixtures. Record validator availability and results. The current review did not run veraPDF.

### N09 — P1: real split execution fails across source/destination documents

**Runtime reproduced; existing shared-path defect exposed by the new package.** `src/modes/PagesMode.cpp:1073–1085` extracts pages from the open source and inserts them into each output using the same `m_ctx->pdfEditor`. `PoDoFoBackend::Private::resolveDocument` at lines 163–174 deliberately rejects another path while a document is resident. `insertPageFromBytes` reaches that guard at line 626 and swallows the exception into `false`.

With a valid generated two-page source loaded in the real `PdfEditorEngine`, splitting `{{0},{1}}` with the default `{stem}_part{n}.pdf` pattern produces an empty path list. Both insertions fail. A separate engine loaded on the destination successfully inserts the same extracted bytes. The same control succeeds with the split stub loaded into that destination engine. This localizes the failure to engine ownership/path handling, not bad source data. The stub has inaccurate xref offsets, but PoDoFo repairs and loads it in this configuration; it was not the cause of this failure.

**Why the tests miss it:** `tests/TestPagesMode.cpp:857–926` calls the real mode with `PagesMock`, whose insert method appends sentinel text. It does not apply the real backend's document-identity guard or reopen actual split PDFs. Calling this “end-to-end” overstates the tested boundary.

**Implement:** retain the source engine and its unsaved state for extraction; use a separate destination document/engine per part, with explicit ownership. Do not remove the anti-divergence guard or switch the user's active editor repeatedly to output paths. Build each part in a candidate, check insertion and stub-removal results, validate page count/content, and commit through the existing safe-save boundary. Generate stubs with the PDF library or correct offsets rather than relying on parser recovery.

**Additional source-level acceptance trap:** `makeOutputName` only replaces `{n}` if it exists. `onSplit:1001–1018` checks pre-existing files but not duplicate generated paths. A pattern such as `collision.pdf` produces the same destination for multiple groups; after the engine defect is fixed, direct repeated writes can overwrite earlier parts. This overwrite was **not** runtime reproduced because execution currently fails earlier. Preflight normalized distinct paths and source/output collisions before writing; require a numbering token or derive unique names reflected in the preview.

**Acceptance:** use the real editor with a real open document. Split multiple, overlapping and single-page segments; reopen each distinct result and verify ordered text/page identities and counts. Confirm the open source bytes and unsaved state survive. Test insertion/delete/commit failures, cancellation, existing destinations, names without `{n}`, and source collisions. Existing outputs must survive failure, and partial output must not be described as a complete split.

### N02 — P2: alphabetic page labels use spreadsheet numbering

**Runtime reproduced against an independent decoder.** `src/core/PageLabels.cpp:37–47` implements bijective base-26: 27=`aa`, 28=`ab`. PDF `/S /a` labels use repeated letters: 27=`aa`, 28=`bb`, 52=`zz`, 53=`aaa`. On a generated PDF with `/PageLabels /Nums [0 << /S /a /St 28 >>]`, the shipped PDFium returns `bb`; the app helper returns `ab`. PDFium's independently maintained implementation uses that repeated-character rule. [PDFium page-label implementation](https://pdfium.googlesource.com/pdfium.git/+/refs/heads/chromium/6823/core/fpdfdoc/cpdf_pagelabel.cpp).

**Why the tests miss it:** `tests/TestPageLabels.cpp:69–78` encodes the spreadsheet expectation. The header/comment also describes the wrong rule. Writer/UI work is explicitly deferred, so this is a faulty groundwork contract rather than a claim of a currently broken page-label dialog.

**Implement:** use the PDF repeated-letter rule for both cases, update documentation and test expectations, and keep the small pure helper. Do not build a new labeling subsystem to fix this calculation.

**Acceptance:** cross-check labels at 1, 25–28, 52–54 and nondefault starts/prefixes against PDFium reading actual number-tree entries. Ensure `labelsFor` and `numberTreeEntries` agree; do not use one new helper as the sole oracle for the other.

### N04 — P2: the Security redaction caller drops overlay text

**Source/caller trace.** `src/shell/controllers/SecurityController.cpp:626–631` copies the dialog's selected request fields but omits `request.overlayText = chosen.overlayText`. The dedicated Redact mode at `src/modes/RedactMode.cpp:500–505` forwards it. The same confirmation dialog can therefore accept a label that silently disappears depending on entry point.

**Implement:** forward the field in both production paths. A small shared plan-to-request conversion is reasonable if it removes this duplication; retain the existing operation rather than adding a second flow. Keep the separate D07 sanitize-default repair in view.

**Acceptance:** create a plan through each real entry path, supply an overlay label, capture the dispatched request, and inspect the saved PDF for that label. Empty labels must remain optional. A test that constructs `RedactRequest` directly cannot catch this omission. This review did not drive the Security modal end to end.

### N05 — P2: signature reuse is not tied to the real document lifecycle

**Caller trace plus runtime notification-sequence probe.** The helper in `src/shell/controllers/EditController.cpp:51–63` stores its cache under the persistent `DocumentSession`. Its only `noteDocument` call is immediately before opening the picker at line 161. `DocumentSession::setPath:14–21` and the main-window open flow do not notify that cache when the document actually changes.

Store a signature on A, open B without opening its picker, return to A, and reopen the picker: the cache sees A twice and retains the signature despite intervening document switches. The probe reproduces this exact notification sequence. The tests instead call `noteDocument(B)` directly, so they test invalidation mechanics while bypassing the missing production notification.

There is also an acceptance-gate mismatch: a populated cache with the default checked “reuse” option still leaves OK disabled on an empty Type page. `updateAccept` ignores the cache, and checkbox changes do not update that gate, while `onAccepted` gives reuse precedence.

**Implement:** invalidate on actual document/session replacement or close, with a clear identity policy, using the existing lifecycle boundary. Include checked, valid cache reuse in acceptance gating and refresh it when the checkbox changes. Show the cached preview or otherwise make clear which graphic will be placed. Avoid a global or persisted signature cache.

**Acceptance:** exercise A→B→A through the production lifecycle without opening B's picker; confirm clearing. Cover close/reopen, same-document repeated placement, reuse checked/unchecked, empty active controls, and a new graphic replacing the previous one. Preserve the chosen per-document privacy/scope contract.

### N06 — P2: Retry Signing loses the selected appearance image

**Source trace with engine-repeat reproduction, not a full partial-LTV UI retry.** The new `SecurityController::SigningRequest` at lines 55–63 contains certificate/password/output/reason/location, but no appearance image or immutable source identity. The retry path at line 140 reuses that request. `src/engines/SignatureManager.cpp:1223–1225` consumes the pending image from a one-use slot; the dialog populates it only on initial acceptance.

The probe signs a generated PDF twice with the same ordinary B-B signing arguments after setting a test image once. Both outcomes are Success. The first saved document contains an image XObject; the second does not. The controller's new retry uses precisely this repeated call without restoring the image. This establishes the lost-input mechanism; the full DSS/B-LTA degradation dialog path remains untested.

**Implement:** capture a complete immutable request including source identity and appearance, and pass appearance explicitly per operation. Reuse those values on retry; avoid reseeding a shared consume-once slot. The worker currently locks shared managers for its execution, which is useful, but reading a mutable session path inside the worker is not an immutable input snapshot. Preserve the existing partial signed output until a replacement has been validated and committed.

**Acceptance:** inject a partial outcome followed by retry, inspect both saved appearance streams/images, and confirm the input identity remains the selected document. Test retry failure preserving the usable partial file, changed session state, and certified as well as signed flows. Keep the new accurate missing-piece wording.

### N07 — P2: Exit redaction leaves the marking tool armed

**Source trace plus offscreen production-widget reproduction.** `src/GpMainWindow.cpp:289–292` handles the new exit signal by switching to an empty screen and showing a status message. The viewer tool is not reset. `ModeController::setScreen` changes the screen, and task-state synchronization does not neutralize the viewer tool.

The probe sets the real mode/viewer to Redact, clicks `redactBtnCancel`, and uses the same exit-to-empty-screen relay as the main window. The screen becomes empty while `viewer->toolMode()` remains Redact. Thus a control presented as “Exit redaction” can leave subsequent document drags creating marks. No installed application was operated.

**Implement:** use the existing neutral selection/navigation tool transition when exiting and synchronize the visible task/tool state. Preserve existing marks as specified; exiting should not apply or discard them.

**Acceptance:** create a mark, exit, verify the mark remains and the redaction tool is inactive; a subsequent ordinary drag must not create a new redaction. Re-entering should restore the expected review state. Check status and tool affordances alongside the screen signal, rather than asserting only signal emission.

### N08 — P2: overlay text spills above the smallest accepted redaction box

**Saved-PDF geometry reproduced.** `src/engines/RedactOperation.cpp:129–134` accepts boxes as short as 9 points and computes baseline as `pdfY + height/2 + fontSize*0.35`. In PDF's upward Y coordinates, this positive baseline offset lifts the glyphs too far. A real 9-point box has top Y=712; PDFium reports the saved “LABEL” glyph top at 714.836, approximately 2.84 points outside the box. White text outside the black fill can disappear into the page.

**Implement:** center using actual font ascent/descent or glyph bounds, with a small internal margin. Skip boxes that cannot fit both width and height. Preserve the landed sibling-candidate save fix; it avoids unsafe in-place rewriting of the loaded PDF.

**Acceptance:** inspect actual glyph bounds or rendered pixels for 9-point, 10-point, tall and narrow boxes, including descenders and non-ASCII labels supported by the chosen font. Every painted label must fit inside its box. Text-extraction presence alone does not verify centering or readability.

## Independent disposition of the 14 new ledger rows

| Ledger row | Disposition | Evidence and remaining work |
|---|---|---|
| §9.8-a — Cancel/Back | **Partial** | Exit signal and screen wiring exist; N07 leaves the viewer tool armed. |
| §9.8-b — overlay text | **Partial** | Saved label and sibling-candidate write work; N04 drops the field from one caller and N08 fails the fit contract. |
| §9.8-c — word-list import | **Verified for code contract** | Trimming, deduplication, escaped alternation, size cap and review-before-mark flow are present; `TestRedactMarkAll` passes. No automatic mark/apply on import. Live file-picker ergonomics remain deferred. |
| §9.12-a — batch presets and DPI | **Verified for tested propagation** | Named presets use stable keys; selected preset/DPI values reach batch options. `TestBatchMode` and real-operation `TestBatchOpsCoverage` pass. This does not accept every freeform regex or compression result. |
| §9.14-a — reading-order threshold | **Partial** | Named tolerance 2 and actual tagged-PDF boundary tests pass. Honest heuristic framing is in source comments, while user results still say “Reading order: OK” (`PdfAValidationPanel.cpp:562,566`). Add user-visible scope wording; do not imply a full accessibility certification. |
| §9.16-a — bookmark/link preservation | **Partial for full row; preservation contract verified** | Real backend tests show plain saves preserve bookmarks/links and sanitize removes outlines/URI links while retaining internal GoTo. The tests-only commit does not establish the claimed user-visible disclosure of stripping; show this consequence before sanitize and verify the caller. |
| §9.7-a — Initials | **Partial** | Monogram helper exists; N01 breaks the visible tabs and their accepted kinds. |
| §9.7-b — session cache | **Partial** | Cache mechanics pass tests, but N05 misses real switches and reuse acceptance. |
| §9.7-c — signing degradation/Retry | **Partial** | Missing-piece details and partial-success wording are useful; N06 means Retry does not preserve the complete request. Full failure/retry acceptance remains pending. |
| §9.9-a — split by segment | **Partial** | Parser correctly produces separate groups; the real output path fails (N09). New mocked tests cannot establish saved-PDF correctness. |
| §9.9-b — page-label groundwork | **Partial** | Small pure helper and explicit deferred writer/UI scope are appropriate; N02 has an incorrect PDF label rule. |
| §9.12-b — PDF/A mappings | **Partial** | 2U metadata mapping is exercised; 3U is incompatible with its selected PDF version (N03). Actual conformance remains unvalidated. |
| §9.13-a — measured compression result | **Verified for calculation/caller contract** | `formatCompletionReport` reports measured before/after/delta and honestly handles equal/larger output; the completion caller supplies those values. `TestCompressDialogHonesty` passes. Modal presentation and themes remain in R12/live UI acceptance. |
| §9.10-a — compare integration | **Partial for feature acceptance** | Real engine/widget/mode tests cover identical docs, a sufficiently similar token edit, trailing pages, filters and reports. V04 still reproduces on a short completely changed page; production `DiffEngine` was not changed by this tests-only commit. |

The three narrowly verified rows above do not accept entire redaction, batch or compression workflows. Retain the row-specific boundaries in the repository ledger.

## Earlier rows rechecked on this snapshot

The full suite reran, the prior independent probes were rebuilt, and relevant source/caller changes were inspected. Existing detailed repair instructions remain in the [5 September branch review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/PARITY-BRANCH-REVIEW-2026-09-05.md), [6 September code review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/CODE-REVIEW-2026-09-06.md), and [preceding fetched-branch review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md). Historical line numbers may have moved; this table is the current status.

| September row | Disposition | Current evidence/boundary |
|---|---|---|
| F01/R01 — safe form writes | **Partial** | Engine same-file save works; V01 import controller still reloads/deletes/self-renames its temporary path. |
| F09/R02 — form undo | **Partial** | Snapshot operations pass. Failed undo still moves history index 1→0 without reverting the value (V02); auto-detect bypasses history (V06). |
| F02/R03 — AI lifetime | **Verified, scoped as before** | Owned worker state and local-server timeout/cancel/destroy tests pass. No sanitizer claim. |
| F03/R04 — endpoint policy | **Verified, scoped as before** | Parsed host policy and explicit redirect handling retain passing policy matrices. |
| F05/R05 — OCR polarity | **Partial** | Enabled polarity fixtures pass; no-Tesseract compilation still fails (D03). |
| F10/R06 — deskew/coordinates | **Partial** | Enabled transform/deskew fixtures pass; D03 prevents optional fallback acceptance. |
| F11/R07 — OCR lifecycle | **Partial** | State/generation work remains; real revision identity is missing (V05), model/UI acceptance pending. |
| F04/R08 — reviewed OCR output | **Partial** | Unicode/page-specific saved output tests pass; V05 can authorize stale review after same-count mutations. |
| F07/R09 — conversion extraction | **Partial** | Decoded Unicode fixtures pass; independent two-column CSV/XLSX output still merges columns (V03). Broader script/layout corpus remains incomplete. |
| F08/R10 — format honesty | **Partial** | Real OOXML and reject-before-write tests pass; configuration/caller acceptance and runtime capability recovery remain incomplete. |
| F06/R11 — structural comparison | **Partial** | Added/removed page support exists; Apple→Orange still emits removal plus addition (V04). New integration tests do not repair alignment. |
| F12/R12 — compression honesty | **Implemented-awaiting-review** | Control/result code tests pass, strengthened by measured reporting. Required live dialog/theme checks remain deferred. |

| Selected July row | Disposition | Current evidence/boundary |
|---|---|---|
| §9.13 JPEG re-encode | **Verified for tested encoder contract** | Quality/DPI/filter/mask/dimension checks continue to pass. Not arbitrary color-space fidelity certification. |
| §9.13 signed-document optimize guard | **Verified** | Real signing fixture and reject-before-output test pass. |
| §9.4 orientation | **Partial** | Enabled Roman-text fixtures pass; inconclusive/universal-script scope and D03 remain. |
| §9.4 preprocessing preferences | **Verified for propagation** | Defaults, persistence and both dispatch callers remain covered. |
| §9.8 page-list redaction | **Verified for page selection** | Explicit/invalid list behavior passes; excludes complete excision/recovery acceptance. |
| §9.8 sanitize bundle | **Partial** | D01 retry, D05 safe replacement and D07 default mismatch remain. |
| §9.7 signature picker | **Partial — regression from prior scoped acceptance** | Graphic persistence tests still pass, but N01 breaks visible Initials/Upload selection and N05 affects reuse. |
| §9.7 validity badges | **Partial** | Actual field anchoring and offscreen tests pass; crop/rotation/multipage/live placement acceptance remains. |
| §9.7 signature appearance | **Partial** | Real appearance/integrity tests pass; rendering corpus remains incomplete and N06 affects retry images. |
| §9.1 two-page overlays | **Verified for tested pixels** | Existing annotation/search overlay pixel tests pass; broader theme/DPI walkthrough pending. |
| §9.10 change filter | **Verified for filtering** | Projection, navigation and reports pass. Underlying classification remains V04. |
| §9.16 local-processing badge | **Implemented-awaiting-review** | Source/tests present; live readability pending. Claims must remain scoped to the actual operation. |
| §9.5 in-house OOXML | **Verified for real package identity** | Correct package structures continue to pass; spreadsheet geometry remains V03. |
| §9.14 asynchronous reading order | **Verified for worker dispatch** | Nonblocking/delivery/no-document tests pass. This does not certify accessibility correctness. |
| §9.12 batch-test determinism | **Partial** | Current first full run passes. One green run does not identify or prove repair of the preceding intermittent failure. |
| Enum-bound serialization | **Verified** | Persisted ordinal bounds/round-trips pass; preserve them when fixing N01. |

| UI/investigation row | Disposition | Current evidence/boundary |
|---|---|---|
| U01 Welcome | **Implemented-awaiting-review** | Geometry tests pass; supported DPI/theme/live clipping checks deferred. |
| U02 Navigation | **Implemented-awaiting-review** | Central task/nav/status tests pass; live walkthrough deferred. N07 adds a concrete tool-state integration repair. |
| U03 OCR verify | **Partial** | D04 painted overlay coordinates and V05 review identity remain; real OCR models missing. |
| U04 Compare | **Partial** | Linked views/filter/report features remain; V04 classification and live/two-page linkage acceptance pending. |
| U05 Redaction | **Partial** | D01/D02/D05/D07 persist; N04/N07/N08 affect new controls and output. |
| U06 Pages | **Implemented-awaiting-review for its selection/reorder scope** | Reorder/history tests pass; live indicators/focus pending. Split is separately blocked by N09. |
| U07 Comments — both duplicate entries | **Implemented-awaiting-review** | Summary/filter/table/CSV tests pass; live geometry/focus remains pending. Consolidate duplicate rows. |
| U08 Capabilities | **Partial** | Zero-byte detector still yields Available for RapidOCR and ensemble (D06); refresh/recovery integration remains incomplete. |
| E-1 same-stream redaction corruption | **Open — not independently reproduced** | Both regenerated independent fixtures preserve `PUBLIC_KEEP_TEXT` again. Obtain the exact failing fixture/rect/stream; do not infer a fix or declare the report disproved. |
| Q02 reproducible builds/tests | **Partial** | Declared-vendor isolated build and 107/107 run succeed. Bootstrap, diagnostic output and prior intermittent-failure explanation remain open. |

### Existing findings remain actionable

| IDs | Recheck at `95676e6` |
|---|---|
| V01, V06 | Relevant `FormsController` source is unchanged; the temporary-file caller and stack-local auto-detect command patterns remain. |
| V02 | Rebuilt commit-failure probe reproduces incorrect undo history. |
| V03 | Rebuilt probe produces combined CSV strings and an XLSX with only A-column cells for the two-column fixture. |
| V04 | Rebuilt single-page Apple→Orange probe reports two structural changes. |
| V05 | Updated EditController still validates generation/path/page count rather than an actual document revision. |
| D01 | Partial result has an empty sanitized destination. Retry with the presenter's path fails; retry with the original requested path succeeds. |
| D02 | Worker still dereferences a `QPointer` to its UI-owned operation without retaining its lifetime; destructor/caller ownership was traced, not crash-tested. |
| D03 | Separate Qt-only syntax compilation still fails at the two `carryResolution` call sites. The normal Tesseract build cannot cover this. |
| D04 | Rebuilt offscreen canvas probe still misses the expected overlay pixel location. |
| D05 | Sanitization still receives the final destination and has direct-write/remove-before-rename paths outside the validated atomic replacement boundary. |
| D06 | Detector-only/empty-model probe still reports RapidOCR and ensemble Available; unavailable→available widget recovery remains disabled. |
| D07 | Security caller still sets `plan.sanitize = false`; dedicated Redact mode retains the default-on choice. |

## Code-quality assessment and next implementation sequence

The branch has made concrete progress: decoded extraction, native package writers, shared operation/state types, named limits, explicit partial outcomes and targeted PDF tests are useful foundations. The main quality problem is **tests that stop before the production boundary where the feature can fail**. N01 mirrors an index assumption; N05 bypasses lifecycle wiring; N04 constructs requests instead of checking callers; N09 substitutes a mock for the backend; N03 checks a self-declared metadata value; N08 checks text presence rather than fit. More test count alone will not address those gaps.

The smallest effective repair strategy is to stabilize those existing boundaries:

1. Fix the P1 document/lifetime/output failures already recorded (V01/V02/V05/D01/D02/D03/D05) and new N01/N03/N09 before relying on these workflows. Use focused patches; no framework migration or general job-system rewrite is warranted.
2. Repair N04/N05/N06/N07/N08 through actual callers and lifecycle events. Correct N02 before adding a label writer or UI. Retain passing parser, graphic writer, candidate-save and outcome-wording code.
3. Address V03/V04/V06/D04/D06/D07 with their existing handoff instructions. The new compare tests are useful coverage but do not supersede the short changed-page reproducer.
4. At each patch, add the smallest meaningful regression that crosses the failing boundary and verifies the saved result or actual visible control state. Run the affected suites plus the necessary optional-configuration check; use a full suite at integration. Do not add tests that simply reproduce the implementation's calculation.
5. Reconcile the ledger: remove duplicate U07, replace the stale “all open” heading, correct R09's fixture attribution to `TestConversionExtraction`, and date/remove stale 85-test and disk-blocked claims. Replace blanket commit-level “verified” wording with row-specific acceptance evidence. Keep E-1 separate until an exact reproducer is supplied.
6. Implementers record the exact commit, behavior changed, checks executed, artifacts, and residual limitations as **implemented-awaiting-review**. An independent review may promote only the stated accepted contract. The new rows requiring heuristic/sanitize disclosure need user-facing wording, not comments alone.
7. Once code acceptance is complete, reinstall the exact accepted build, record its identity, and perform the companion's industry-informed UI scenarios and computer-use walkthrough. Current offscreen widget probes do not replace that review.

Use the [updated GLM Flash companion](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md) together with this report and the [reproduction archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/update-quality-evidence-2026-09-07.zip).
