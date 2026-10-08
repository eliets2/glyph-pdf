# GlyphPDF — implementation companion and UI improvement plan

<!-- TEAM-CHECKPOINT-2026-09-08 -->
> **Current review checkpoint — 8 September 2026:** use [the consolidated team review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-QUALITY-REVIEW-2026-09-08.md) and [research reconciliation](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/RESEARCH-RECONCILIATION-2026-09-08.md) before executing the older package instructions below. The remote review is pinned to `b58b91054ca7a573a09c56d950588ac4f966df42`. The team reports 21 new findings, including eight P1 items; branch acceptance is withheld. Repair shared save, document identity, autosave/annotations, close-after-save and undo failures first. N01/N02/N03/N04/N05/N07/N10 and E-1 now have scoped independent acceptance; N06/N09/D02-redaction remain partial. D01/D06/N08 and earlier V/D findings still require work. The older checkpoint/status sections below describe their dated revisions and are superseded where this new review supplies a disposition.
>
> The research synthesis is a proposal, not implementation status. DocMDP certification and recipient encryption already have code paths; do not rebuild them as missing. No installed UI review or reinstallation was performed. Each new fix stays `implemented-awaiting-review` until independent acceptance.
<!-- END-TEAM-CHECKPOINT-2026-09-08 -->


Prepared 5 September 2026 for implementation with GLM Flash and subsequent independent review.

Scope updated 7 September: review and repair the code first; reinstall the newest application after package acceptance, then review the UI. Do not judge package behavior using the older installed app. The latest review fetched `origin/feat/parity-glm` and is pinned to `0caa45e`; original defect descriptions below remain historical specifications.

**Fix document safety and output correctness first. Then improve the review workflows and simplify the interface.** This file complements the July parity documents; it does not replace their historical record or claim that competitive parity has been achieved.

## 1. Baseline, evidence, and how to use this file

**Active implementation worktree:** `C:\Users\User\Projects\pdf-parity`  
**Active branch and latest reviewed snapshot:** `origin/feat/parity-glm` / `0caa45e7d0751caaa36a54a085c41211a422f019`  
Historical baseline worktree: `C:\Users\User\Projects\pdf`  
Historical baseline branch/commit: `main` / `703fa34ece32733ea3b2093da94fa1aed94e1afc`  
Origin: `https://github.com/eliets2/glyph-pdf.git`

**Progress correction:** The main checkout does not contain the recent work because implementation is in the separate `pdf-parity` worktree. The original audit remains the defect baseline; it is not an assessment that this branch made no progress. Do not switch, merge, or edit the main checkout as part of this handoff.

Read these sources before implementing:

1. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — historical feature and quality baseline.
2. `docs/audit/COMPARISON-TABLES-2026-07-01.md` — historical comparisons.
3. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` and `docs/audit/CORRECTIONS-2026-07-01.md` — intended changes and corrections.
4. [September audit](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/AUDIT-2026-09-05.md) — findings F01–F12 and reconciliation of all 16 parity domains.
5. [Audit evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/audit-evidence.zip) — reproduction sources, outputs, and verification notes. Extract it into a scratch directory when needed.
6. `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` in `pdf-parity`, then [the independent branch review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/PARITY-BRANCH-REVIEW-2026-09-05.md) — the review supersedes stale implementation statuses in the ledger and this file's original backlog.
7. [6 September code review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/CODE-REVIEW-2026-09-06.md) — historical `cf5ddc7` review with D01–D05 implementation instructions.
8. [6 September fetched remote review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md) — historical assessment at `4761443`, preceding ledger rows and detailed D06/D07 instructions.
9. [7 September detailed update review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UPDATE-QUALITY-REVIEW-2026-09-07.md) — snapshot at `95676e6`, all 14 new rows plus 38 preceding unique rows and N01–N09 implementation instructions. Its D01/D05/D06/D07 statuses are superseded by the next report.
10. [Latest review through `0caa45e`](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/LATEST-QUALITY-REVIEW-2026-09-07.md) — **current assessment**, late fixes independently reviewed, 107/107 CTest pass, D05/D07 scoped acceptance, D01/D06 residuals and N10 unregistered tests. [Latest evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/latest-quality-evidence-2026-09-07.zip).

All source paths below are relative to the repository root. Line references are discovery aids at the audited commit; inspect the current functions before editing. If HEAD has changed, reproduce the finding again and preserve unrelated changes.

Evidence labels:

| Label | Meaning |
|---|---|
| Runtime | A targeted probe reproduced the defect using the audited production code. |
| Trace | The defect follows from the implementation and its callers; no live failure was induced. |
| Live UI | The running audited build was visually inspected. |
| Proposal | A recommended future behavior, with acceptance criteria; not an existing implementation. |

The prior audit built successfully and passed all **84 CTest targets**. That result does not cover the defects reproduced by separate probes. The fresh build lacked `duckx` and `OpenXLSX`. The current stack is C++17, Qt 6, CMake/Ninja, MSYS2 UCRT64, PoDoFo, PDFium, Tesseract, RapidOCR/ONNX Runtime, qpdf, and existing Djot integration. Reuse those boundaries and dependencies. Do not introduce a framework migration, a second document model, or a generic job system to fix a narrow contract defect.

**Current implementation status at `0caa45e`:** all 12 repair packages, all eight distinct UI packages (U01–U08), and 14 July P1 follow-up rows have committed implementations. The latest isolated full-target build succeeds and CTest passes 107/107 in 38.24 seconds. D05's sanitized-output replacement and D07's initial policy/opt-out are independently accepted within their stated boundaries. D01's intended retry path is repaired, but the Security banner still uses the old partial result. D06 rejects empty/incomplete sets but still calls corrupt nonempty models usable and needs caller/refresh integration. N01–N09 remain; N10 identifies four regression functions absent from the Qt test inventory. V01–V06 and D02–D04 remain open. In particular, the commit labeled V01/V02 changes redaction tests, not the original form import/undo defects. Model-required and revoked-certificate-hook acceptance remains limited. Use row-specific review dispositions; live UI review stays deferred until code acceptance and reinstall.

The R01–R12 sections below preserve the original defects and acceptance contracts as specifications. Statements describing old broken code are historical; inspect the current implementation before editing. **Do not reimplement already-landed packages from scratch.** Use one bounded follow-up patch, run its acceptance checks, then request an independent review of the actual diff and artifacts.

### Current follow-up queue — read before starting an R package

| Review ID | Related package | Required next work |
|---|---|---|
| V01 | R01 | Fix form import's temporary-path delete/rename caller; verify the original destination and viewer/session identity. |
| V02 | R02 | Keep failed undo/redo persistence and logical history synchronized and retryable. |
| D03 | R05/R06 | Keep the landed polarity/deskew fixes; move the Qt metadata helper outside the Tesseract conditional to restore the fallback build. |
| V03 | R09/R10 | Preserve geometric runs and spreadsheet columns while retaining the new Unicode decoder. |
| V04 | R11 | Distinguish modified pages from actual structural insertions/deletions. |
| V05 | R07/R08 | Validate a real document revision; path and page count cannot detect in-place edits/replacement/reordering. |
| V06 | R01/forms | Register auto-detected fields with undo history and correct partial-success messaging. |
| D01 | U05 | Intended retry path and Redact-mode outcome repaired at `0caa45e`; finish the Security caller's recovered banner. |
| D02 | U05 | Give redaction worker state a lifetime independent of its UI owner. |
| D04 | U03 | Correct image-to-widget overlay transforms; test painted positions against hit testing. |
| D05 | U05 | Accepted for transaction/retry replacement at `0caa45e`; preserve the validated candidate and checked commit. |

The [branch review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/PARITY-BRANCH-REVIEW-2026-09-05.md) provides instructions for V items; the [previous code review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/CODE-REVIEW-2026-09-06.md) provides D01–D05. The [6 September fetched-remote review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md) adds D06/D07. The [detailed 7 September review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UPDATE-QUALITY-REVIEW-2026-09-07.md) adds N01–N09. **Apply the [latest `0caa45e` review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/LATEST-QUALITY-REVIEW-2026-09-07.md) as the status override: preserve D05/D07, finish D01/D06, and register N10's missing tests.** Retain passing implementations; reconcile duplicated U07, stale test counts, R09's incorrect fixture attribution and the reused V01/V02/D02 labels.

### New-update repairs — 7 September

Read the current review for exact triggers, evidence limits, source locations and acceptance instructions. These are follow-ups to landed code; do not reimplement the packages.

| ID / priority | Required bounded repair |
|---|---|
| N01 / P1 | Align visible Initials/Upload pages, programmatic selection, validation and accepted kinds using one mapping. Test actual visible controls. |
| N03 / P1 | Correct PDF/A-3U and existing 3B to PDF 1.7; validate artifacts for the claimed conformance rather than checking only metadata. |
| N09 / P1 | Split through a separate destination document per part while preserving the open source engine; validate and safely commit distinct outputs. The current real engine rejects all destination insertions. |
| N02 / P2 | Use PDF repeated-letter page labels, not spreadsheet base-26; compare generated number trees with PDFium at boundary values. |
| N04 / P2 | Forward overlay text through the Security controller as well as Redact mode. Test both actual callers. |
| N05 / P2 | Invalidate signature cache on actual document lifecycle changes; make checked reuse enable valid acceptance independently of empty tab inputs. |
| N06 / P2 | Capture source identity and appearance in the immutable signing request; preserve them and the existing partial file across retry. |
| N07 / P2 | Exit redaction through the existing neutral tool transition while preserving marks. Test screen and viewer tool state together. |
| N08 / P2 | Fit overlay glyphs using font bounds and internal margins; validate saved positions/pixels for minimum-height boxes. |
| N10 / P2 | Register the three capability cases and one default-sanitize case as actual Qt test slots; confirm executable inventory and explicit named-case execution. |

The current review also identifies acceptance work for user-visible reading-order heuristic wording and disclosure of sanitize's bookmark/link removal. Tests-only comments do not meet those user-facing contracts. Implementers retain **implemented-awaiting-review** until independent acceptance of the actual commit and evidence.

## 2. Order of work

P1 retains the September audit meaning: fix promptly before relying on that workflow in a release. P2 is a material issue to schedule next. Execution order also accounts for dependencies.

| Sequence | Package | Finding / domain | Dependency and completion gate |
|---|---|---|---|
| 1 | R01 Safe form writes | F01, P1; forms | Source survives every failed write; same-file round trip succeeds. |
| 2 | R02 Real form undo | F09, P1; forms | R01; old values and metadata restored after reopening. |
| 3 | R03 AI request lifetime | F02, P1; AI | Late callbacks cannot access destroyed state. |
| 4 | R04 Endpoint validation | F03, P1; AI | Literal loopback tests and existing HTTPS policy pass. |
| 5 | R05 OCR polarity | F05, P1; OCR | Paper remains light and strokes remain dark. |
| 6 | R06 OCR deskew | F10, P2; OCR | R05; actual skew corrected and coordinates round-trip. |
| 7 | R07 OCR lifecycle | F11, P2; OCR | Every terminal outcome permits the appropriate retry/review. |
| 8 | R08 Persist reviewed OCR | F04, P1; OCR | R05–R07; edited words survive PDF text extraction. |
| 9 | R09 Unicode conversion | F07, P1; conversion | Subset-font text exported as readable Unicode. |
| 10 | R10 Truthful export formats | F08, P1; conversion | R09 for content quality; format gating can land earlier. |
| 11 | R11 Structural comparison | F06, P1; compare | Added/removed pages reach UI and reports. |
| 12 | R12 Honest compression controls | F12, P2; compression | Unsupported passes cannot be selected or reported as run. |
| 13 | Q01–Q03 Evidence and maintenance | Cross-cutting | Record validation and repair narrow test/build assumptions. |
| 14 | U01–U08 UI improvements | Viewing, OCR, compare, pages, comments, export | Start small visual fixes earlier if independent; review workflows depend on their engine repairs. |

## 3. Repair packages for GLM Flash

### R01 — Save form mutations without risking the source PDF

**Problem / evidence:** F01, runtime. `addTextField(input, ..., input)` returned false with a PoDoFo error and reduced a valid 15,257-byte, text-bearing PDF to zero bytes. A separate output path succeeded. `AddFormFieldCommand` ignores the operation result.

**Read:** `src/engines/FormManager.cpp`, particularly `fillForm`, all `add*Field`/widget creation methods, `setFieldMetadata`, `removeFieldByName`, `updateFieldRect`, and `setTabOrder`; `src/commands/AddFormFieldCommand.h`; `src/core/interfaces/IFormManager.h`; `src/engines/DocumentSession.*`. Search every `doc.Save` in FormManager, including import/flatten paths.

**Implement:**

1. Put the protection at the shared form-save boundary. Serialize the complete mutation to a unique temporary PDF, then finish and close the PoDoFo writer and any source handles it owns. Reopen the candidate to confirm it is a readable PDF with the expected page count and requested form change.
2. Commit the validated bytes to the destination using an existing safe-save facility if its guarantees are sufficient. A practical Qt fallback is a bounded copy from the validated candidate into `QSaveFile`, followed by a checked `commit()`, with direct-write fallback disabled. PoDoFo must not be given `QSaveFile::fileName()` as though it were the hidden temporary path. Check open/read/write/commit failures and clean up only this operation's temporary files. Qt documents the temporary-file and commit behavior in [QSaveFile](https://doc.qt.io/qt-6/qsavefile.html).
3. Never delete the original before renaming a temporary file. The current `setTabOrder` code uses a fixed `.tmp` filename, removes the destination, ignores rename failure, and returns success. Include that path in the save-boundary repair; it is not a safe pattern to reuse.
4. Protect both same-path writes and replacement of an existing separate destination. Prefer routing every form save through the same boundary, avoiding fragile string-only path equality checks on Windows. If replacement is blocked by an open handle, report failure while retaining the original.
5. Check the result of the specific command operation. Refresh/dirty state only after successful persistence. Use the project's error-reporting path so the user can retry; do not leave a successful-looking undo entry for a failed command. Respect `QUndoStack` ownership when marking failed initial commands obsolete.

**Acceptance:** Generate a PDF with a real embedded/subset font and extractable text. Add a field to the same filename, reopen, and check both original text and field presence. Exercise other form mutators through their real shared save boundary. Inject a failure during candidate save and during final commit; the SHA-256 and bytes of the original and any pre-existing destination must remain unchanged. Failed commands must not signal a successful reload. Verify undo/redo after successful add. Keep signed-document restrictions intact.

**Keep scope narrow:** A small reusable save helper is justified; a replacement persistence framework is not. Tests should force failures deterministically rather than depend on the local user's permissions.

### R02 — Capture actual form state and make undo transactional

**Problem / evidence:** F09, trace. `EditFormFieldCommand` remembers only the original field name. Undo writes an empty tooltip and `required=false`, and skips restoring an empty original value. Redo also skips intentionally setting an empty value.

**Read:** `src/commands/EditFormFieldCommand.h`, `src/core/interfaces/IFormManager.h`, `src/engines/FormManager.cpp`, `src/shell/controllers/FormsController.cpp`, and the form properties panel used by that controller.

**Implement:**

1. Read a complete, supported property snapshot before the first mutation. Extend the existing form interface only as needed, or reuse a proven document snapshot command if one already exists. Do not synthesize old metadata from UI defaults.
2. Distinguish current value, default value, absence of a value, and an explicitly empty string. Check whether the control named `defaultVal` intends PDF `/V`, `/DV`, or both; give the UI and engine one documented meaning.
3. Apply value and metadata as one mutation on one loaded document, then one R01 commit. Two independent saves permit partial completion. Undo must use the same transactional path and preserve unrelated field flags.
4. Capture prior state once; redo after undo must not recapture the edited state as the original. Resolve missing fields and duplicate names explicitly. Use stable identity where available.
5. Rename, placeholder, and validation-regex controls currently lack a complete persistence contract. Disable unsupported editing with an explanation, or implement each as a separate tested task. Do not report them fixed merely because the panel stores text.

**Acceptance:** Start with a field that has a nonempty tooltip, `required=true`, and a nonempty value; edit all supported properties, undo, reopen, and compare. Repeat with empty values and toggled required flags. Test redo, failed save, nonexistent field, and changes to a second field. A failed edit must not partially persist metadata or advance visible success state.

### R03 — Bound AI request lifetimes and completion

**Problem / evidence:** F02, trace. `OllamaProvider::chat` queues callbacks capturing worker-local variables and a semaphore by reference. Its 20-second timeout can return while those callbacks remain pending. A crash was not induced during the audit.

**Read:** `src/engines/ai/OllamaProvider.cpp:157–174`, its header, `AiResult`, and existing provider tests/callers.

**Implement:**

1. Retain the existing `QFuture<AiResult>` contract unless a caller requires a change. A bounded solution is to create the network manager, reply, timer, and local event loop inside the worker that owns the entire request. Qt network objects need thread affinity and an event loop; they do not inherently require the GUI thread.
2. Keep every callback's captured state alive until callbacks are disconnected and the reply is finished or aborted. Ensure cleanup happens in the owning thread. Never return from a timeout while a queued closure can still reference its stack.
3. If the existing GUI-thread manager is retained instead, use explicitly owned request state, a cancellation flag/deadline checked before dispatch, and exactly one terminal result. An owning smart pointer alone does not cancel late requests or fix competing completion paths.
4. Abort pending I/O on timeout, clean up replies, and distinguish timeout, cancellation, HTTP/network error, malformed JSON, and empty response. Preserve endpoint checks and request-content restrictions.

**Acceptance:** Use a local test HTTP server and a test-configurable short deadline. Cover success, response after timeout, connection refusal, malformed response, request cancellation, and provider/window destruction while work remains. Drain queued events after each case and assert one result with no late state access. If GUI-thread dispatch remains, test timeout before the queued dispatch runs. Run a supported sanitizer where available; do not claim sanitizer coverage when only functional tests ran.

### R04 — Parse loopback addresses instead of matching prefixes

**Problem / evidence:** F03, runtime validator probe only. `http://127.audit.invalid:11434` passes the production guard. No DNS lookup or network request was made.

**Read:** `isAllowedEndpoint`, `resolveEndpoint`, and request construction in `src/engines/ai/OllamaProvider.cpp`.

**Implement:** Parse the URL and require a valid, nonempty host. For HTTP, handle the exact intended `localhost` spelling separately and require any other host to parse as a literal address whose `isLoopback()` is true. Use the existing Qt Network dependency; [QHostAddress](https://doc.qt.io/qt-6/qhostaddress.html) provides address parsing and loopback classification. Preserve the existing explicit HTTPS host allowlist and opt-in override. Reject unsupported schemes and invalid URLs. Make invalid user-supplied endpoints produce an intelligible result; avoid a misleading connection to an unrelated fallback endpoint.

**Acceptance matrix:** Permit canonical `localhost`, `127.0.0.1`, another valid `127.x.x.x` literal, and `[::1]`. Reject `127.audit.invalid`, `localhost.audit.invalid`, `0.0.0.0`, a private LAN address, public HTTP hosts, missing hosts, and non-HTTP(S) schemes. Pin the policy for trailing dots, user-info, unusual numeric spellings, and IPv4-mapped IPv6 with explicit tests. Exercise both stored and directly supplied endpoints. HTTPS tests must cover allowed, denied, and explicit override configurations with isolated settings. Inspect redirect behavior; if redirects are followed, require the same policy at each destination or disable automatic redirects. No tests need an external server.

### R05 — Preserve black text on white paper through binarization

**Problem / evidence:** F05, runtime. Converting a 1-bit Leptonica image using `val ? 255 : 0` reverses polarity. White input became black. Binarization is enabled by default.

**Read:** 1-bit Pix-to-QImage conversion in `src/engines/ocr/OcrPreprocessor.cpp:56–65` and existing preprocessing tests.

**Implement:** Correct the 1-bit foreground/background mapping for the actual Leptonica representation. Keep the 8-bit and RGB paths distinct. Do not fix this by globally inverting every OCR input; that would change other image depths and photographs. Preserve output format, dimensions, and DPI metadata as appropriate.

**Acceptance:** A deterministic white page with black rectangles/text retains a light background and dark foreground at known interior pixels. Include all-white, all-black, grayscale, and colored-input conversions. Run the normal preprocessing defaults as well as the isolated binarizer. Add a recognition smoke test only where the required local model data is available and explicitly report skips.

### R06 — Estimate skew on binary data and preserve coordinate transforms

**Problem / evidence:** F10, runtime. `pixFindSkew` receives an 8-bit Pix, emits “pixs not 1 bpp”, reports angle zero, and leaves a three-degree tilted text image unchanged.

**Read:** `OcrPreprocessor::deskew`, `process`, and `PreprocessedImage::inverseTransform`; OCR callers that map word boxes back to the source.

**Implement:** After R05, produce a temporary 1-bit estimator image for Leptonica. Estimate angle/confidence there and apply the correction to the intended output image. Check the sign and origin of rotation experimentally. Compose scale, quadrant orientation, deskew rotation, and any translation caused by the new image bounds; return the inverse of the complete forward transform. Preserve sensible DPI and avoid unnecessary repeated resampling. For blank pages or unreliable estimates, return a documented no-op with valid metadata.

**Acceptance:** Use reproducible text fixtures tilted approximately +3° and −3°. Check that residual skew improves within a documented tolerance, content is not clipped, and no depth warning occurs. Map several known points and rectangle corners through the forward/inverse pipeline; require a stated tolerance in original pixels. Repeat with DPI normalization and 90° orientation enabled. Keep the no-Tesseract build behavior intentional.

### R07 — Complete the OCR lifecycle on every exit

**Problem / evidence:** F11, trace. Run is disabled before dispatch and only success restores it. Missing files/models/languages and worker errors can leave the panel stuck. Accept disables review controls before the save dialog, so cancellation also needs a defined recovery path.

**Read:** `src/modes/OCRMode.cpp:421–526`, `src/modes/OCRMode.h`, `EditController::runOcr`, `onOcrAcceptRequested`, and the wiring in `src/GpMainWindow.cpp`.

**Implement:** Add a small explicit state enum to the existing flow: idle, running, review-ready, saving, and recoverable error as needed. Define one completion path for success, empty result, validation failure, worker failure, and cancellation. Identify each job by generation and document/page identity; drop stale results after a document switch or a newer request. All widget updates belong on the UI thread. A canceled save retains review edits and re-enables Save/Accept. Failed saves retain data for retry. Destroyed panels must not receive callbacks.

**Acceptance:** In the same panel instance, trigger missing language/model data then retry successfully. Cover no document, empty recognition, worker error, cancellation, save cancellation, save error, and switching page/document mid-job. Assert both the state and user-visible controls, not just that a success signal fires. A stale completion must not re-enable saving another document's result.

### R08 — Make reviewed OCR words authoritative

**Problem / evidence:** F04, trace. The editable pane is populated but never read back; Accept exports the cached original words. The displayed current page can differ from the cached page being saved.

**Read:** `src/modes/OCRMode.*`, `src/GpMainWindow.cpp:226–230`, `src/shell/controllers/EditController.cpp` around result caching and `onOcrAcceptRequested`, `MergedOcrWord`, and the searchable-PDF writer.

**Implement in two bounded steps:**

1. Establish a review session containing source-document identity/revision, original page index, original image and transform, OCR words with stable IDs, and each word's reviewed text. The source/page metadata must travel with the words through acceptance.
2. Make the first supported correction interaction word-based. Editing a selected word updates its stable record without changing its source box. The export uses those reviewed records and Unicode text. Keep a plain-text page view as a preview unless arbitrary editing has a defined alignment strategy. Never split arbitrary edited text and zip it against the original boxes: insertion, deletion, punctuation, and changed word counts break that approach.
3. If arbitrary page-text editing is retained, design and test explicit word/line alignment, split/merge behavior, deletions, and unresolved edits before claiming it persists. Do not invent coordinates for inserted text silently. Treat this as a separate follow-on package.
4. Use the reviewed page index for both the save label and payload. Reject stale review sessions when the source changes, or preserve an explicit source snapshot. Preserve edits on canceled or failed save. Clearly describe whether the output is the current reviewed page or an entire document.
5. Connect real source-image inspection as specified in U03. Until region operations exist, label actions with their actual page-wide scope.

**Acceptance:** Recognize or seed a misspelled word, correct it, save a searchable PDF, and extract text through PDFium. The corrected text must appear and the old token must not remain in the new text layer. Check visual alignment against the source image. Test Unicode, a deleted word, a replacement containing spaces according to the chosen contract, two pages with similar words, page switches, source edits, and save cancellation. Existing accept-seam tests alone are insufficient. Whole-document OCR remains a separate item until it has its own complete acceptance test.

### R09 — Extract Unicode through PDFium for conversion

**Problem / evidence:** F07, runtime. The shared Word/Excel/CSV extractor interprets `Tj`/`TJ` bytes directly as text. A subset-font fixture exported glyph/control codes even though the existing PDFium backend extracted “Shared first page” correctly.

**Read:** `ConversionManager::Private::extractTextFromPage` and row grouping in `src/engines/ConversionManager.cpp`; `src/engines/pdfium/PdfiumBackend.*`; `IConversionEngine::TextElement`.

**Implement:** Use PDFium's decoded text path. Reuse the backend's page-text API for plain-text requirements; where conversion needs geometry, add the smallest page-text-with-boxes method to that existing boundary using the available PDFium text APIs. Own and close document/page/text handles correctly, retain page identity, and normalize geometry once. Do not write another PDF font decoder. Prefer a per-operation backend rather than sharing the live viewer's handles across threads.

Replace approximate comparisons inside `std::sort` with deterministic ordering and a separate line-clustering pass. A pairwise “close enough in Y” comparison can violate the strict ordering contract. Sort by stable numeric keys first, cluster into lines with a documented tolerance, and then order runs within each line. Preserve Unicode logical order for scripts where x-coordinate order alone is insufficient. Keep table reconstruction claims limited to the layouts actually supported.

**Acceptance:** Export subset-font, accented Latin, ligature, Arabic/RTL, mixed-direction, multiline, and rotated/cropped fixtures with known extractable text. Assert decoded content and intentional ordering, including quoting/escaping for CSV and HTML. Avoid exact whitespace comparisons where PDF extraction inserts legitimate spacing. Compare extracted expected words, paragraph/page boundaries, and tested table cells. An image-only PDF must produce an honest empty/OCR-needed result rather than fabricated text.

### R10 — Make selected format, extension, and bytes agree

**Problem / evidence:** F08, runtime. Without native Office dependencies, export reports success while writing HTML as `.docx` and CSV as `.xlsx`. The interactive warning appears after writing; batch uses the same engine.

**Read:** `ConversionManager::hasNativeWordExport`, `hasNativeExcelExport`, `exportToWord`, `exportToExcel`; `src/shell/controllers/ConvertController.cpp`; conversion dispatch in `src/modes/BatchMode.cpp`; CMake feature detection.

**Implement the dependency-free correction first:**

1. Reuse the existing capability queries before showing save filters or starting a batch item. When native export is unavailable, disable Word/Excel with an explanation and expose the existing HTML/CSV alternatives under their actual names and suffixes.
2. Make the engine reject unavailable native formats before opening or truncating an output. UI gating alone is insufficient because batch and future callers can bypass it.
3. Resolve the actual format and extension before writing. If an alternative is chosen, create the actual format's filename; do not silently change a requested `.docx` into an HTML file or overwrite a different existing target.
4. Report per-item batch status with actual format and destination. Use the same capability decision for menus, dialogs, batch, and engine dispatch. Reuse existing enums/query functions; introduce a larger capability object only if duplication remains after the narrow repair.

**Native OOXML is a separate package:** If required next, integrate the existing optional dependencies into the project's actual MSYS2/CMake packaging, verify runtime availability, and produce valid packages. Do not follow a stale instruction to edit a nonexistent `vcpkg.json`. Confirm the pinned library APIs locally before coding against them.

**Acceptance:** In a build without the libraries, unavailable native requests return failure without creating or changing the destination; HTML/CSV remain valid. In a build with each dependency, inspect DOCX/XLSX ZIP structure, required content-type/relationship parts, and document/workbook parts, then extract expected Unicode text/cell values. Exercise GUI and batch paths, suffix mismatches, failure, and existing targets. A nonempty file or ZIP magic alone is not evidence of valid OOXML.

### R11 — Represent added and removed pages in comparison

**Problem / evidence:** F06, runtime. One page versus the same page plus an appendix produced no entry for the added page because the engine compares only the minimum page count.

**Read:** `src/engines/DiffEngine.h/.cpp`, `src/ui/CompareWidget.h/.cpp`, `src/modes/CompareMode.h/.cpp`, and report generation.

**Implement:**

1. Add explicit structural changes to the existing result model: page added, page removed, page moved, with old/new page positions where applicable. Missing sides must be represented explicitly rather than by a valid page-zero sentinel.
2. Land the proven minimum repair first: include surplus pages on either side, count them as changes, set `isIdentical=false`, and expose them in the tree, navigation, and exported report. An added blank page is still a structural change.
3. Keep page alignment as a separate subtask. Reconcile insertions in the middle with the existing move detection before claiming general insert/delete identification. Use deterministic fingerprints/matching and a fallback for ambiguous or repeated pages. Avoid double-counting moves as added/removed pages.
4. Build one visible change sequence used by tree selection, next/previous, totals, and report filters. Store old/new page locations with each change. Link viewer navigation and overlays to the selected change; do not keep showing the first page's overlay.

**Acceptance:** Compare identical PDFs; a unique and a blank appended page; a trailing removal; a middle insertion; reordered pages; repeated identical pages; an image-only change; and mixed page dimensions. Reverse old/new and verify additions become removals. Reports must name the correct page and side. Filtered counts and navigation must agree. The middle-insertion case can remain explicitly partial until alignment lands; do not mark all comparison parity complete after the trailing-page fix.

### R12 — Remove unsupported compression promises

**Problem / evidence:** F12, trace. “Subset fonts” and “Remove unused objects” are checked in `CompressDialog`, while the backend acknowledges that those passes do not run.

**Read:** `src/modes/CompressDialog.cpp:149–155`, compression options and estimates, `src/engines/podofo/PoDoFoBackend.cpp` around the compression implementation.

**Implement:** Disable and uncheck those options with concise availability text, or remove them from the selectable set. Exclude them from planned/applied pass summaries. Keep implemented JPEG encoding, deduplication, sanitize reuse, and signed-document guards intact. Describe estimates as estimates; calculate final savings from the successfully committed output. Handle a larger-than-input result honestly.

**Acceptance:** A user cannot select an unsupported pass or receive a success summary claiming it ran. Existing compression tests still pass. Manually inspect the dialog in supported themes and verify the result report distinguishes predicted and measured size. No new object garbage collector or font subsetter belongs in this UI-honesty patch.

### Latest follow-ups D06 and D07

The fetched-remote review adds two bounded repairs: **D06**, truthful RapidOCR model-set availability and runtime refresh across capability consumers; and **D07**, a shared default-ON sanitization policy for both redaction entry paths. Full implementation instructions and failure/acceptance matrices are in the [current review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md). Preserve V01–V06 and D01–D05 until their independent acceptance passes. Do not mark U08 verified because its registry unit tests pass.

## 4. Code quality and parity maintenance

### Q01 — Keep a current evidence ledger beside the historical parity audit

Create or update one current ledger in `docs/audit/`; link it from the implementation plan. Each row needs a stable ID, historical parity section, concrete behavior, status, code location, regression test, artifact/manual evidence, implementing commit, review date, and residual limitation. Suggested states: open, implementing, implemented-awaiting-review, verified, partial, unavailable, blocked-with-reason. “Verified” requires an independent review of the acceptance evidence.

Seed the ledger with F01–F12 and U01–U08. Track any additional July items separately; the September audit reconciled 16 domains, not every individual sentence of the July file. Do not replace all OCR or comparison rows with a single “done”. Preserve historical conclusions and append dated corrections.

Correct stale claims already contradicted by HEAD: Compare is reachable, OCR language selection exists, auto-detect is no longer three dummy fields, JPEG re-encoding exists, and form add has an undo implementation despite an obsolete comment describing it as a no-op. README's 14-test count and the implementation plan's 39-test count need to reflect the current registration or stop hardcoding a volatile total. Do not publish a new numeric competitive score without a defined rubric and comparable test corpus.

Ledger row template:

```text
ID | Parity section | User-visible acceptance | Status | Code | Regression test
   | Artifact/manual check | Commit | Reviewed on | Residual limitation
F04 | §9.4 | Corrected OCR word survives saved-PDF extraction | open
    | OCRMode + EditController | pending | pending | — | —
    | Current one-page Accept is wired; reviewed edits are not persisted.
```

### Q02 — Make tests reproducible outside a particular build directory

Two Djot tests locate Lua fixtures through `applicationDirPath()/../third_party/djot`. Pass the fixture directory through CMake or copy declared fixtures into the test runtime directory. Verify an out-of-tree build under a differently named directory. Keep test settings in a temporary INI-backed namespace where practical rather than writing real registry-backed preferences.

The audit encountered Windows environment-name duplication/loader issues and sandbox restrictions on settings writes. Those were resolved for validation and were not classified as product defects. Consult the evidence README rather than changing application behavior to accommodate that audit environment. Distinguish missing optional dependencies/skipped cases from passing functional coverage.

Use a small CMake helper for repetitive test declarations only if touching those declarations for these fixes makes it worthwhile. Do not rewrite the entire build file as a prerequisite.

### Q03 — Repair contracts before reorganizing large files

PoDoFoBackend, SignatureManager, PdfEditorEngine, Inspector, BatchMode, and the viewer are maintenance hotspots. Extract code when a repair reveals a cohesive shared responsibility, such as safe save or page-text extraction. Keep public interfaces small and preserve controller/engine/document separation. Do not split files solely to reduce line counts.

Review ignored mutation results, worker callbacks capturing raw UI/document pointers, fake-success fallbacks, and hardcoded resource paths in the touched workflow. Fix nearby defects required for the acceptance contract; put unrelated discoveries into the ledger with evidence. Address warnings introduced by a patch. Existing vendor/test warnings need attribution rather than a claim that the whole project is warning-free.

## 5. UI review and specific implementation instructions

### Scope of this UI review

The audited build was launched after staging its local runtime DLLs into the isolated audit build directory. Its dark welcome screen and main workspace chrome were observed at a 2560 × 1440 capture size. No display-scaling matrix was performed. The welcome screen has a consistent dark/orange identity and useful named action cards. The main workspace exposes menus, a ribbon, an editing-mode strip, sidebars, specialized-screen navigation, and a status bar.

The early main-checkout visual observations below are historical. The user subsequently requested computer use as well. During the active-branch review, computer use launched the freshly built `06b542d` executable and inspected its empty-document accessibility tree. Further screenshot capture was blocked by automatic approval review because the welcome screen includes private recent-file names and paths. The user subsequently directed that all live UI acceptance be deferred until code acceptance and installation of the newest build; the earlier capture question is no longer needed for the current scope. No private PDF was opened during the branch review, and no private screenshot assets are included in this deliverable.

OCR, comparison, redaction, forms, compression, batch, theme transitions, and page-tool recommendations below rely on source inspection and engine evidence unless explicitly labeled as historical live observations. They are not a claim that every screen of the current branch was exercised interactively. Account for R07/R08's newly landed lifecycle and reviewed-word controls when implementing U03; extend them rather than replacing them with a second review model.

### U01 — Repair the welcome layout and unify surface styling

**Evidence:** Live UI plus `src/ui/WelcomeWidget.cpp`. The final action card is clipped despite the large window, and the logo, subtitle, cards, and recent list are separated by excessive vertical space. The source places six cards with 140-pixel minimum widths and gaps inside a container capped at 600 pixels. The welcome palette is hardcoded independently of `GpTheme.h` and the three QSS themes.

**Implement:** Replace the six-card horizontal row with a small responsive grid: three columns when space permits, two columns for a narrower content width, one for a very narrow window. Use real size hints and available width, not display-resolution assumptions. Keep the content's vertical size driven by its children, with one intentional stretch outside the content block. Put “Open PDF” first and show recent files directly beneath the actions. Give Import Office a real existing icon, or add the missing icon to the current icon registry; inspect the current `file-plus` lookup before changing assets. Retain keyboard-accessible card names and shortcuts.

Replace touched local color constants and inline hardcoded styles with existing theme tokens/palette roles. Keep the existing orange accent and typography identity. Use sentence case and comfortably readable labels; monospace should be reserved for values where it helps.

**Acceptance:** At 1280×720, 1440×900, and 2560×1440, every card and label is reachable without horizontal clipping. Check 100%, 150%, and 200% scaling on a test setup without changing the user's system settings. Check empty and populated recent lists, long filenames, keyboard focus, dark/light/high-contrast themes. Layout inspection is sufficient for purely visual spacing changes; add automated coverage only for real overflow/focus regressions.

### U02 — Give each task one clear navigation entry and current state

**Evidence:** Live workspace chrome and `src/shell/ScreenNav.cpp`, `ModeStrip.cpp`, `RibbonModel.cpp`, `Ribbon.cpp`, `MenuBar.cpp`, `StatusBar.cpp`, `ToolRegistry.*`, and `src/GpMainWindow.cpp`. Twelve numbered specialized screens coexist with ribbon categories and editing modes. Several concepts appear in more than one navigation layer.

**Implement in stages:** Keep existing actions and shortcuts as the behavior source. First centralize the mapping from action to screen and editing mode, ensuring that entering OCR, redaction, or forms selects the corresponding visible state consistently. Then replace numbered screen labels such as “01 OCR VERIFY” with task names. Add a compact “Tools” chooser for specialized workflows and make the ribbon collapsible while keeping the active task visible. Do not remove all navigation paths in one patch.

Reduce the default status bar to useful document state: page n of m, zoom, saved/unsaved state, and current operation. Put PDF version, selection/debug-style values, and dimensions in a details affordance where appropriate. Do not show “page 1 / 000” or document-specific controls with no document open. Preserve document-signature information where it affects available actions.

**Acceptance:** Open PDF, search, organize pages, run OCR, compare, and return to reading through documented paths with keyboard equivalents. The active label, tool behavior, and available controls agree after each transition. Switching tasks must not discard a pending review, silently save, or unexpectedly alter the document. Compare before/after steps with the same task script rather than claiming an unmeasured productivity gain.

### U03 — Turn OCR verification into a real source-and-correction workflow

**Evidence:** Source review. `OCRMode` renders recognized words as rich text in the panel labeled image/scan. The zoom pane is a text label. The region context menu currently uses an empty box for the whole page, and regional reject clears the page results. The legend says high ≥80 / medium 50–79 / low <50, while highlighting uses ≥90 / 70–89 / <70 and the low-word count uses <70.

**Reference pattern:** ABBYY documents selecting a recognized word, highlighting its source image region, showing a magnified crop, and navigating low-confidence items. Borrow that verification loop, not its visual assets or product claims. [ABBYY FineReader PDF 16: checking recognized text](https://help.abbyy.com/en-us/finereader/16/user_guide/checkingtext/).

**Implement:** After R05–R08, use the actual source-page image with positioned boxes. Selecting a word in either view selects the same stable word record; the zoom panel shows the corresponding image crop. Provide “Previous uncertain word” / “Next uncertain word”, a correction field, original recognized text, and a reviewed/unreviewed marker. Preserve source-engine provenance separately from review status. Corrections do not automatically turn a model confidence estimate into 100%.

Define one confidence classification function used by the legend, colors, counts, and navigation. For the first repair, align the legend with the current 90/70 thresholds unless a documented product decision changes all consumers. Test boundary values 49, 50, 69, 70, 79, 80, 89, and 90. Add text or icons so meaning does not depend on red/green alone.

Rename page-wide context actions immediately. Implement true regional operations only once source-coordinate mapping exists, and keep nonselected results untouched. Replace static “4×” claims with the actual magnification. Move engine/strategy details to an advanced section; keep language, page scope, progress, review count, and save result prominent. Whole-document OCR should be a separate bounded follow-up with per-page review and cancellation.

**Acceptance:** A reviewer can identify an uncertain word, inspect its source pixels, correct it, move to the next unresolved word, and save the corrected searchable output. Counts, highlighting, and navigation agree at threshold boundaries. Test pan/zoom, rotations, DPI changes, narrow layout, keyboard-only review, canceled saving, and stale sessions. Verify saved text and box alignment, not just the appearance of two panes.

### U04 — Make comparison results drive both document views

**Evidence:** Source review plus F06. The existing widget already has two PDF viewers, text differences, and navigation. `CompareMode` filters the change tree independently; `CompareWidget` builds its own text anchors. Pixel overlay selection initially uses the first diff page.

**Reference pattern:** Acrobat documents a summary of changes, side-by-side review, filters, and next/previous navigation. Use those linked review behaviors as the reference. [Adobe Acrobat: compare two PDF versions](https://helpx.adobe.com/acrobat/using/compare-documents.html).

**Implement:** After R11, show explicit Old/New filenames and page counts, an optional swap action, and a summary by change type. Selecting a change must navigate both sides to their mapped pages and show its text/area. A missing page side gets an explanatory placeholder. Reuse the single filtered change sequence for totals and next/previous. Offer linked/unlinked scrolling; map page positions when page sizes or counts differ rather than assuming equal scrollbar values. Make the lower details area resizable instead of relying solely on fixed height. Export the same selected scope and filter state described in the UI.

**Acceptance:** In a mixed change fixture, the tree, counter, next/previous, page views, overlay, and report refer to the same change. Added/removed pages remain navigable, including blank pages. A zero-results filter says no changes match the filter without claiming the files are identical. Keep structural repair and visual polish in separate reviewable patches.

### U05 — Make redaction output and partial failure explicit

**Evidence:** Source review of `src/modes/RedactMode.cpp:387+` and `src/shell/controllers/SecurityController.cpp:470+`. Marks, Apply, signed guards, and sanitization already exist. The mode path chooses a fixed `_redacted.pdf` destination after applying edits. Sanitization failure produces a warning, then the later status still uses a generic redaction-success message. The controller has a different destination flow.

**Reference pattern:** Acrobat separates marking, Apply, optional sanitization, and choosing the final filename/location. [Adobe: redact sensitive content](https://helpx.adobe.com/uk/acrobat/desktop/protect-documents/redact-pdfs/redact.html). Its hidden-information tool also separates complete and selective cleanup. [Adobe: sanitize PDFs](https://helpx.adobe.com/acrobat/desktop/protect-documents/redact-pdfs/sanitize.html).

**Implement:** Route both entry paths through one existing controller/engine operation where practical. Before mutation, show mark/page counts, actual sanitization choice, and a destination picker with normal overwrite handling. Retain the signed-file protection. Do not advertise “Save As unsigned” as a guaranteed conversion unless that operation actually removes cryptographic signatures in a tested, supported way.

Work on a disposable copy/session so cancel or failure does not leave the live document half-redacted. Publish success only after the requested output steps have completed and the destination is committed. If redaction succeeds but sanitization fails, retain a clearly labeled partial result with explicit retry/export choices; do not replace the warning with a generic completion banner. Keep marks/review state recoverable where possible. Make “marked for removal” visually different from “applied to saved output”.

**Acceptance:** Test cancel before apply, an existing destination, signed input, write failure, engine failure after one page, sanitization failure, and full success. Original bytes survive failures. Independently reopen the output, check removed text extraction and relevant metadata, and verify the displayed result matches the actual artifact. These checks validate the tested content; they do not establish universal sanitization certification.

### U06 — Improve page organization around the existing thumbnail grid

**Evidence:** Source review. `src/modes/PagesMode.cpp` already supports internal drag movement and an atomic permutation command. Do not rebuild a feature that now exists just because the July baseline listed it as missing.

**Reference pattern:** Foxit's desktop Mac manual documents a dedicated thumbnail organization view, page context actions, and insertion affordances. This is cross-platform interaction inspiration, not a claim about the current Windows edition. [Foxit PDF Editor 2024.4 for Mac: organize PDF pages](https://help.foxit.com/manuals/pdf-editor/mac/en-us/2024.4.0/Organize_PDF_Pages.html).

**Implement:** Make selected-page count and affected range visible. Provide a clear insertion indicator during drag, page-number labels that stay readable, keyboard move commands, and context actions that call the same existing commands as the toolbar. Keep destructive controls away from incidental thumbnail clicks. Restore selection and current page after undo. Add page labels/Bates numbering only as separate engine-and-UI packages with their own persistence tests.

**Acceptance:** Select nonadjacent pages, reorder, undo, redo, save a copy, and reopen to check page identities and selection behavior. Test mixed page sizes, a last-page operation, a large thumbnail set, and keyboard-only movement. Document the measured fixture/hardware for performance claims. Do not introduce new delete/merge implementations behind new buttons.

### U07 — Extend the existing comments review tools

**Evidence:** Source review of `src/ui/CommentsWidget.*`, `src/core/AnnotationTypes.h`, and `src/shell/Sidebar.cpp`. GlyphPDF already has comment status, author, and date filters, review-state labels, and reply/composer behavior. It does not need a second markup database.

**Reference pattern:** Bluebeam documents a markup table with configurable columns, status/author filters, sorting, and exportable summaries. [Bluebeam Revu 20/21: track and manage markups](https://support.bluebeam.com/revu/how-to/track-and-manage-markups-using-markups-list.html).

**Implement:** Build on the existing annotation records and filters. Add a visible active-filter summary, result count, and clear-filters action. If a table view is useful for larger reviews, expose it as another presentation of the same records with page/type/status/author columns. Preserve selected annotation and navigate to its page/geometry when selected. Add a CSV summary of the displayed selection/scope only after confirming the persisted fields. Reuse the current status model; add batch status changes only with a coherent undo command.

**Acceptance:** Filters combine predictably, clearing restores all results, and selection links to the correct annotation after page navigation. Status changes persist after save/reopen and undo correctly. Exported counts and values match the visible scope, with escaped CSV text. Test existing replies and Djot formatting so the new review presentation does not regress them. Cloud collaboration and permissions are outside this package.

### U08 — Explain capabilities, progress, and outcomes before and after work

**Evidence:** F08/F11/F12 and source review of conversion, OCR, compression, batch, and signature flows. Some capability labels disclose a fallback only after a file is written; batch and interactive settings diverge.

**Implement:** Reuse existing capability checks at the point of selection. A disabled feature should say what is unavailable and offer a valid supported alternative. Show page/file scope and destination before execution. For OCR, name language and page scope before engine internals. For exports, explain actual format and material layout limitations. For signatures, distinguish a drawn/typed graphic from a certificate-backed digital signature and its validation result.

Use the existing worker and progress patterns for long operations. Move batch merge off the UI thread with cancellation at safe file boundaries; do not add another scheduler. Pass the same preprocessing options to interactive and batch OCR, and report any intentionally unsupported batch option. Base result summaries on successful outputs, failed items, and remaining items. Keep useful retry actions and do not report an unsupported or skipped operation as completed.

**Acceptance:** Exercise one supported, unavailable, canceled, failed, and successful path in each touched workflow. An unavailable native Office format never reaches a false-success save. Failed OCR is retryable. A failed batch item does not erase prior successful results or claim all files completed. Check responsiveness with a stated large fixture; treat a sub-100-ms interaction target as a proposed goal to measure, not an achieved result.

## 6. Research boundaries and design direction

The cited official documentation was checked on **5 September 2026**. Adobe Acrobat, ABBYY FineReader, Foxit PDF Editor, and Bluebeam Revu were selected as established reference products for relevant workflows. This is a documented interaction review, not a market-share ranking, paid-edition feature audit, performance benchmark, or hands-on test of those competitors. Product versions and platform scope are identified where relevant. Recommendations are adaptations for GlyphPDF's current architecture.

The priority is a trustworthy local PDF workstation: a readable document canvas, a clear active task, edits that reach the saved artifact, visible scope, and actionable results. Retain the current brand and working engines. Defer cloud collaboration, a new AI assistant architecture, plugin marketplaces, complete Word-style layout reconstruction, and broad accessibility-conformance claims until the existing acceptance contracts are reliable.

Do not add competitor logos, copy their assets, or claim equivalent performance based on documentation. Do not repeat an unconditional “document never leaves the machine” claim across workflows with configurable remote AI providers. Capability and processing-location descriptions should match the selected operation.

## 7. Validation and independent review

### Per-patch implementation loop

1. Read the selected package, its actual source/callers, and relevant existing tests. Check repository instructions and the working-tree diff.
2. Reproduce the stated defect on disposable fixtures. Add the smallest meaningful regression for data, lifetime, or state defects; demonstrate it fails before the repair where feasible.
3. Implement the minimum complete behavior, including caller errors, undo/state changes, and artifact format where the package requires them.
4. Run focused tests, then the required project checks. Run the full suite once the patch set is ready; broaden/repeat only after further changes or unresolved failures.
5. Inspect the resulting PDF/export through an independent reader/extractor where possible. Engine return values and file sizes alone are insufficient.
6. Update the ledger with exact commit/test/artifact evidence and remaining limits. Mark implemented-awaiting-review, not verified.

Build starting point in a correctly configured MSYS2 UCRT64 developer environment:

```powershell
cmake -S . -B build-glm-review -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-glm-review --parallel 4
ctest --test-dir build-glm-review --output-on-failure
```

Use an unused build directory or inspect it before reuse. Never delete the user's existing build folders or audit logs. Record compiler/Qt versions and feature detection, especially native Office libraries and OCR model availability. Ensure runtime DLLs and declared test fixtures are discoverable. See the audit evidence notes for the normalized Windows environment and Djot-fixture workaround used during the original validation.

### Review checklist tied to real outcomes

| Area | Reviewer must independently verify |
|---|---|
| Form saves | Same-file success; failure preserves original bytes; no remove-before-commit; command results propagated. |
| Form undo | Original value, empty state, tooltip, flags, and unrelated fields survive undo/reopen. |
| Async AI | Owned state, correct thread affinity, timeout cleanup, exactly one result, no late dispatch after cancellation. |
| Endpoints | Parsed loopback restriction; existing HTTPS policy preserved; no unvalidated redirect path. |
| OCR | Correct polarity/skew; complete transform; terminal-state recovery; saved corrected text and correct page. |
| Conversion | Decoded Unicode; deterministic order; true format/suffix; capability behavior in GUI and batch. |
| Compare | Structural page changes visible; navigation/filter/report agreement; alignment limitations disclosed. |
| UI | No clipped controls; active task is clear; keyboard focus works; operation scope and outcome are truthful. |
| Regression | Existing signed guards, redaction behavior, annotation persistence, and relevant tests still pass. |
| Evidence | Test commands/results are recorded; skips and unavailable dependencies are visible; no untested “done” claims. |

For final UI review, use generated/public test documents and an isolated test profile. Cover an empty app, digital text, image-only scan, mixed-size multipage PDF, form fields, annotations, a signed fixture, and a comparison pair. Check dark/light/high-contrast themes and common window sizes/scales. A manual task walkthrough is more useful than a large suite of tests that merely mirrors widget construction.

### Copy-ready prompt: implementation

```text
Work in C:\Users\User\Projects\pdf-parity on feat/parity-glm.
Read the repository instructions, the July parity audit, the September audit,
CURRENT-EVIDENCE-LEDGER-2026-09-05.md, the implementation companion,
PARITY-BRANCH-REVIEW-2026-09-05.md, CODE-REVIEW-2026-09-06.md, and
REMOTE-PARITY-REVIEW-2026-09-06.md (the current fetched-remote assessment).
Use the newest review for current statuses. The companion and reviews are in
C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs.

Implement only V01 first, as an R01 caller follow-up. Inspect current HEAD and
existing changes. The shared safe-save repair already exists: do not replace it.
Reproduce the controller's import/temp-path problem on a disposable form and
fix persistence, viewer/session identity, and success/error reporting together.
Add controller-level acceptance coverage from V01. Preserve unrelated changes
and the other session's preprocessing/mock work. Do not touch the main checkout.

Do not mark the package verified yourself. Run its acceptance checks and the
required project checks. Report changed files, root cause, behavioral change,
commands/results, output evidence, and remaining limitations. Update the
current parity ledger to implemented-awaiting-review only when evidence exists.
Stop after this package so its diff can be independently reviewed.
```

For later patches, replace `V01` and its reproduction scope with one selected follow-up or still-open package. Check whether concurrent work has already fixed it before editing. Supply the actual location of this companion if it has not been copied into repository docs. Keep dependent packages gated on their prerequisites.

### Copy-ready prompt: independent review

```text
Review the actual diff for package <ID> against its acceptance criteria in
GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md. Do not rely on the implementer's
summary. Inspect production callers, failure paths, object/thread lifetimes,
undo behavior, and saved artifacts where relevant. Re-run the decisive test
and check the required regression results.

Report concrete defects with file/line references and severity. Separate
blocking defects from optional improvements. State any checks you could not
perform. Mark the ledger verified only if the package's complete acceptance
contract is supported by evidence; otherwise leave it partial or awaiting fixes.
```

### Copy-ready prompt: UI implementation and review

```text
After the required engine repairs pass review, implement one UI package at a
time, beginning with U01. Use the existing Qt widgets, action registry, theme
tokens, controllers, and document model. Read that package's source evidence
and official reference links; adapt the interaction to GlyphPDF.

For each patch, provide before/after views using synthetic/public PDFs and an
isolated test profile, plus a short task walkthrough. Check keyboard access,
common sizes/scales, all supported themes, and cancel/failure states. Verify
the actual saved artifact for workflows that modify or export documents.
Record what was inspected live and what was only traced in source.
Do not claim complete parity from visual similarity or from passing unit tests.
```
