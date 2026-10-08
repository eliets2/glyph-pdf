# GlyphPDF — independent review of new commits

Review started 7 September and completed 8 September 2026. **The new commits repair several concrete defects, including the previously unconfirmed E-1 text corruption. The complete branch is not accepted: split naming and synchronous redaction execution still have gaps, and important earlier findings remain.**

## Exact boundary and method

- Fetched `origin` successfully and pinned `origin/feat/parity-glm` to **`b58b91054ca7a573a09c56d950588ac4f966df42`**.
- Reviewed the new delta after **`0caa45e7d0751caaa36a54a085c41211a422f019`**: **34 files, 2,563 insertions, 351 deletions**. Later branch movement is outside this report.
- Used `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` and the previous `LATEST-QUALITY-REVIEW-2026-09-07.md` / `UPDATE-QUALITY-REVIEW-2026-09-07.md` as the finding and acceptance guide.
- Extracted the exact tracked source into `work/team-new-review/source`; source archive SHA-256: `b2c38c978265eb7f0f4b04793aa9b6f21de933fbe7a660a3bbad220a94aae573`.
- Copied the three documented untracked binary trees from the preserved baseline snapshot: `third_party/podofo/install`, `third_party/pdfium`, `onnxruntime-win-x64-1.17.3`.
- Built in a separate `work/team-new-review/build`. Did not edit the production checkout, mutate its dirty files, merge, install, or review the older installed app.

The other agents own the broader engine/application/architecture review. This report covers every changed file and follows relevant callers outside the delta; it is not a claim to have independently audited every unchanged file.

## Verification results

**Full build: 639/639 steps, exit 0.** GCC 16.1.0, Qt 6.11.0, Debug with `-g0`, existing MSYS2 toolchain and vendored dependencies. Eleven warnings remain in existing vendor/OCR/application/test code; this was not a warning-free build.

**Full CTest first run: 106/108 passed, 96.54 seconds.** `TestDjotRoundtrip` and `TestDjotFuzz` failed because their helpers resolve Djot at `applicationDirPath()/../third_party/djot`, which is outside this deliberately separate source/build layout. Staging the unchanged, pinned Djot files at that test-required location made the two targeted reruns pass, **2/2 in 0.35 seconds**. Every target consequently has a passing result, but this report does **not** describe that as one clean 108/108 run. No assertions or production code were changed to obtain the rerun.

Detailed Qt logs were captured for 26 suites. Relevant totals below include `initTestCase` and `cleanupTestCase`:

| Suite | Passed | Failed | Skipped |
|---|---:|---:|---:|
| TestExcisionCorruption | 4 | 0 | 0 |
| TestRedactTransaction | 30 | 0 | 0 |
| TestRedactMarkAll | 17 | 0 | 0 |
| TestPagesMode | 28 | 0 | 0 |
| TestSignaturePicker | 12 | 0 | 0 |
| TestSignatureSessionCache | 8 | 0 | 0 |
| TestSignatureAppearance | 14 | 0 | 0 |
| TestPageLabels | 11 | 0 | 0 |
| TestBatchOpsCoverage | 9 | 0 | 0 |
| TestCapabilityRegistry | 19 | 0 | 0 |
| TestDiffEngine | 27 | 0 | 0 |
| TestCompareEntry | 26 | 0 | 0 |
| TestRapidOcr | 3 | 0 | 3 |
| TestSignatureRealCrypto | 17 | 0 | 1 |

Captured skips are three PP-OCRv5 model-dependent checks and the revoked-certificate check requiring `GLYPH_TESTING`. This is not an assertion that no other suite can skip. No sanitizer, complete alternate dependency matrix, independent PDF/A conformance validation, external Office application round-trip, or live installed UI review was performed.

## Actionable residuals in the changed code

### NCR-01 — P2: case-only source aliases bypass split preflight and omit a part

**Location:** `src/modes/PagesMode.cpp:965–978`, `987`, `998`; final result presentation at `1093–1101`.

`makeOutputPaths` stores `QFileInfo::absoluteFilePath()` in a case-sensitive `QSet<QString>`. On the reviewed Windows filesystem, `split-source.pdf` and `SPLIT-SOURCE.pdf` address the same file, but this guard treats them as distinct. The helper is shared by preview and execution, so the preview can promise an output that is actually the open source.

**Independent trigger:** load a generated two-page `split-source.pdf`; split groups `{0}` and `{1}` into the same directory with naming pattern `SPLIT-SOURCE.pdf`. The normal `{stem}_part{n}.pdf` control produces two files and preserves the source. With the case-only alias, the first commit reports `Access is denied`, only `SPLIT-SOURCE_part2.pdf` is produced, and the source remains unchanged with two pages. The current loaded-file lock protects the source in this reproduction; **source overwrite was not reproduced and is not claimed**.

The caller shows “Split complete” whenever any output exists. It therefore treats this partial operation as complete without identifying the omitted first part. The naming repair has fixed default execution and exact-string collisions, but its “source must never be a split output” contract remains incomplete.

**Minimal repair:** make the existing output-path preflight compare filesystem identities, using canonical paths for existing targets and appropriate Windows case handling for prospective targets. Derive a distinct name before writing when an output aliases the source. Return or retain per-part failures so the existing completion dialog can distinguish complete, partial, and canceled results; do not add another split pipeline.

**Acceptance:** on Windows, same directory + upper/lowercase source filename must yield two distinct output PDFs with the correct pages, with source bytes unchanged. Include canonical-path aliases where supported. A forced failure in one of two parts must name that failed part and report partial completion.

### NCR-02 — P2: synchronous `run()` bypasses the new redaction single-run/ownership contract

**Location:** `src/engines/RedactOperation.cpp:108`, `369–394`, especially `run()` at `389–394`.

`start()` calls `tryBeginRun()` and captures a strong copy of `m_exec`; `run()` directly calls `m_exec->execute()` with neither step. The new comments explicitly promise that a second start, including a start after run, is refused. That promise is not enforced for the public synchronous entry point.

**Independent trigger:** connect `finished`, call `run()` twice, then `start()` on one operation. The probe observes **two completions after the two runs and three after start**, despite the intended one-operation/one-execution rule. A `run()` racing an already started worker can similarly enter the same mutable execution state without this guard. This probe uses a failing generated request to count executions without writing real documents.

There is also a source-level ownership gap: unlike the worker lambda, `run()` does not retain a local `shared_ptr`. A direct-connected slot deleting the operation can release the last execution-state owner while `execute()` is active. The separate owner-deletion probe returned without a crash on this build; **a synchronous UAF crash is not claimed**. Lack of a crash does not supply the missing ownership guarantee.

**Minimal repair:** copy `m_exec` into a local strong reference in `run()` and use the same one-shot gate before executing. Keep `start()` and the synchronous seam consistent, without duplicating the transaction. Audit direct signal/hook ownership if those seams remain public.

**Acceptance:** `run(); run()`, `run(); start()`, and `start(); run()` must execute once. Retain the existing async owner-destruction and cancellation tests. Add a synchronous direct-callback owner-destruction check with a memory sanitizer where supported.

### N08 remains open — overlay text exceeds the accepted small rectangle

**Location:** `src/engines/RedactOperation.cpp:208–209`.

The baseline formula is unchanged. In the independent saved-PDF probe, a nine-point-tall box has PDF top **712**, while “LABEL” reaches **714.836** according to PDFium character boxes. The operation reports completion but the glyphs extend above the black rectangle. The newer N04 field-copy repair now carries labels correctly; it does not repair placement.

**Repair:** compute the baseline from the font's actual ascent/descent, fit both dimensions with margins, and skip a label that cannot fit. Test saved glyph bounds or rendered pixels for short/narrow boxes, not only extracted text presence.

## Independent disposition of claimed fixes

“Verified” below applies only to the stated finding contract at this exact commit, not to an entire feature family or parity row.

| ID / package | Disposition | Evidence and remaining boundary |
|---|---|---|
| N01 — signature tab mapping | **Verified** | Visible Initials enables OK for a valid name; empty Upload disables it. Repository tests select actual page widgets, check their visible labels, accept valid upload/initials/typed/draw, and reject letter-less initials. Independent probe confirms the original swapped-gate trigger is gone. |
| N02 — alphabetic labels | **Verified for groundwork** | Independent app labels are `aa, bb` for 27/28 and the PDFium oracle reads `bb` for `/St 28`. Boundary tests cover 25–28 and 52–54. No writer/UI completion is claimed. |
| N03 — PDF/A-3 version | **Verified for version mapping** | Saved PDF/A-3U reports PDF 1.7 instead of 2.0; saved 2B/2U/3B/3U mappings are tested through the batch worker. Setting the identification/version does not establish PDF/A conformance; no veraPDF validation was run. |
| N04 — dropped overlay label | **Verified for field propagation** | Both Security and Redact mode call `redactRequestFromPlan`; it copies every relevant dialog field including overlay. The shared seam test and real Redact-mode dialog→operation→saved-PDF test pass. Security's modal itself was source-traced, not driven. N08 fit remains separate. |
| N05 — cache lifecycle and reuse gate | **Verified** | Cache is parented to `DocumentSession` and observes its actual `setPath` emission. Independent A→B→A clears it without B's picker; a freshly populated cache enables OK on empty Type with reuse checked. Repository test also checks toggling and same-document transitions. |
| N06 — retry appearance | **Verified for captured image; broader retry acceptance incomplete** | Explicit `signDocumentWithAppearance` embeds the same image in two attempts, independently checked in saved image XObjects. Security captures source path and image into its request and uses the explicit concrete entry point. The regression exercises two successful engine calls, not a real `PartialLtvMissing` modal retry or failed replacement preserving a previous partial output. The non-concrete fallback still uses the legacy helper/session route. |
| N07 — redaction exit | **Verified for tool-state contract** | Cancel changes Redact to HandTool before emitting exit; the host relay also resets the shared viewer. Repository test retains placed marks; independent ModeController probe confirms empty screen and no active Redact tool. No native UI drag walkthrough is claimed. |
| N09 — real split | **Original failure repaired; partial overall** | Real source-resident engine now creates separate candidate PDFs, reopens and validates them, and commits via SafeSave. Independent normal split returns two files/source unchanged; repository tests independently reopen correct page identities and cover failure preserving existing outputs. NCR-01 remains. |
| N10 — orphaned tests | **Verified for registration/execution** | Executable `-functions` inventories include all three `rapidModelsProbe...` functions and `defaultSanitizePolicyIsSharedAndOn`. Detailed suite logs execute them. The capability test's “Available” assertion for arbitrary payload bytes still does not prove OCR readiness (D06). |
| D02 — async redaction ownership | **Async repair verified; API residual NCR-02** | Four added async tests pass: live delivery, cancellation, repeated-start refusal with immediate destruction, and deterministic destruction during page processing. The worker retains `ExecutionState` and signal-owner detachment is guarded. Synchronous entry bypasses that guarantee. No sanitizer claim. |
| E-1 — neighbor text corruption | **Verified for the supplied same-stream regressions** | Independently compiled the new regression against preserved `0caa45e` libraries: both cases fail with `PUBLIC_KEEP_XEXX`. The pinned new build passes both, including saved-stream byte identity and independent PDFium extraction. Deep-copying the raw `PdfString` prevents lazy evaluation from changing the operand later serialized. This is stronger evidence than the earlier single-line/two-page fixtures. |
| CMP-align — middle insertion | **Verified within structural sequence scope** | Exact-fingerprint LCS tests pass for distinct insertion, repeated-page deterministic tie-break, near-twin insertion with an edit, reversal, and CompareMode structural row/filter/report propagation. Per-page token rows remain paired by index; fingerprints still truncate at 200 characters and blank/image-only pages use the prior fallback. This does not accept general compare parity or close V04. |

## Earlier findings and ledger maintenance

- **D01 remains partial:** Security calls the presenter without an effective-result output at `SecurityController.cpp:693`, then displays `bannerText(result)` from the original partial result at `710`. A successful sanitization retry can still be followed by the obsolete failure banner. Use the recovered result as Redact mode already does.
- **D06 remains partial:** the current delta only registers existing tests; readiness detection itself is unchanged. Arbitrary nonempty model files can still be reported as usable, engine/registry model resolution differs, and runtime refresh/widget-disable ownership remains incomplete. The previous review's independent corrupt-model evidence continues to apply.
- The new delta does not repair original form import/failed undo (V01/V02), short rewritten-page classification (V04), or other unchanged V/D findings. Broader engine and application failures identified by the other reviewers must be included in branch acceptance.
- Retain the preceding D05 safe sanitized-output replacement and D07 shared default-policy acceptances within their earlier boundaries.
- The ledger now contains **two D02 redaction rows**, one “open” and one “implemented-awaiting-review”; replace them with one current scoped disposition.
- E-1 was marked “fixed” by the implementation commits before this independent review. This report now supplies independent evidence for the tested contract; attach the exact review commit and logs rather than using the implementation's 108/108 claim as review evidence.
- Correct stale D06/D07 “(this commit)” placeholders and per-suite counts. Current independently observed marking/capability totals are 17 and 19, respectively, including setup/cleanup. Avoid equating a passing availability-file test with a successfully initialized engine.

## Code and architecture quality of this delta

Useful changes reuse existing boundaries: the redaction plan conversion eliminates a real caller mismatch; split reuses SafeSave and the existing page-append idiom; signature appearance is now an explicit operation input. The repaired tests increasingly inspect persisted artifacts rather than mock call counts.

Keep the remaining work small. The split validator's factory only validates candidates despite header wording saying destination engines write them; correct that description. The production `writeMinimalPdf` helper is now retained only for tests and has stale executeSplit comments; move or remove it when touching this seam. Avoid adding a third signature abstraction to address the concrete-manager cast: carry the complete signing request through the existing manager contract when that interface is next revised. The lengthy implementation comments repeat report/history text; retain concise invariants and move the historical narrative into audit documentation.

## Per-file coverage

All 34 delta files were read; groups below enumerate each one.

| Files | Review performed |
|---|---|
| `CMakeLists.txt` | New E-1 target, linkage, offscreen deployment, registration, timeout; full configure/build and executable test run. |
| `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` | New rows compared to baseline reports, actual source/callers and executed logs; duplicate/stale statuses above. |
| `src/GpMainWindow.cpp` | Redaction-exit relay and shared-viewer reset; checked broader lifecycle blocks unchanged for other reviewers. |
| `src/core/PageLabels.cpp`, `src/core/PageLabels.h` | Repeated-letter scheme, boundaries, documented scope; independent PDFium oracle. |
| `src/engines/DiffEngine.cpp`, `src/engines/DiffEngine.h` | Exact/fuzzy alignment, repeated-page tie-break, empty/short fingerprints, structural vs index-paired output distinction. |
| `src/engines/RedactOperation.cpp`, `src/engines/RedactOperation.h` | Worker ownership, emission guard, cancellation, public run/start, candidate/commit and unchanged overlay geometry. |
| `src/engines/SignatureManager.cpp`, `src/engines/SignatureManager.h` | Explicit appearance API, pending slot consumption, sign/certify paths, retry boundary and saved image resources. |
| `src/engines/podofo/PdfPageOps.cpp`, `src/engines/podofo/PdfPageOps.h` | AppendDocumentPages ownership, new destination assembly, one-page input behavior, caller validation. |
| `src/engines/podofo/PoDoFoBackend.cpp` | Raw-string deep copy and evaluated-string fallback; complete PDF/A mapping; confirmed other backend segments unchanged. |
| `src/modes/PagesMode.cpp`, `src/modes/PagesMode.h` | Preview/output naming, actual source extraction, candidate assembly, validation/commit, partial-result behavior; independent real split probe. |
| `src/modes/RedactApplyDialog.cpp`, `src/modes/RedactApplyDialog.h` | Shared field mapping and unchanged result/recovery presenter; both production callers traced. |
| `src/modes/RedactMode.cpp` | Exit tool reset/mark retention, plan conversion, operation invocation; repository real-dialog saved-output test. |
| `src/shell/controllers/SecurityController.cpp` | Signing request capture/worker/retry dispatch, source identity and fallback, shared redaction conversion, unresolved effective-result banner. |
| `src/ui/SignaturePicker.cpp`, `src/ui/SignaturePicker.h` | Visible page/kind dispatch, upload gate, cache lifecycle connection, checkbox enablement and accepted result. |
| `tests/TestBatchOpsCoverage.cpp` | Saved metadata/version assertions and actual batch flow; conformance limits. |
| `tests/TestCapabilityRegistry.cpp` | MOC registration and payload-file semantics; executable inventory and detailed run. |
| `tests/TestCompareEntry.cpp`, `tests/TestDiffEngine.cpp` | Middle insertion, near-twin, duplicate/reversal, tree/filter/report coverage and intentionally unaligned token rows. |
| `tests/TestExcisionCorruption.cpp` | Fixture geometry, glyph-byte assertions, independent extractor; compiled against both old and new engines, failing before/passing after. |
| `tests/TestPageLabels.cpp` | Corrected boundary expectations against independent oracle. |
| `tests/TestPagesMode.cpp` | Replacement of mocks with real PDF outputs, form/group integration, preview correspondence and preservation failures; missing Windows alias case. |
| `tests/TestRedactMarkAll.cpp` | Four formerly/currently added slots, caller overlay test, shared request fields, Cancel state and marks, default policy. |
| `tests/TestRedactTransaction.cpp` | Four async lifetime/cancel tests, thread/queued-delivery scope, missing synchronous guard tests. |
| `tests/TestSignatureAppearance.cpp` | Explicit image reuse and legacy-slot separation; successful-engine retry versus partial-outcome controller boundary. |
| `tests/TestSignaturePicker.cpp` | Visible identity and control-hosting assertions, valid/invalid upload, initials/typed/draw outputs. |
| `tests/TestSignatureSessionCache.cpp` | Actual DocumentSession A→B→A lifecycle, dirty/reload preservation, checkbox gating and accepted cached content. |

## Evidence

The companion archive `TEAM-NEW-COMMITS-EVIDENCE-2026-09-07.zip` contains the pinned delta/log/ledger, manifest, build and first-run logs, detailed Qt logs, targeted Djot rerun, executable inventories, independent probe source/runner/output, and the E-1 pre-fix failing control. Generated fixture PDFs are included. Large binaries/build products and vendor trees remain in the isolated work directory instead of the deliverable archive.

The next implementation pass should repair NCR-01/NCR-02 and N08, finish D01/D06 and the broader reviewers' prioritized failures, then obtain independent acceptance at a new exact commit. Reinstallation and live UI review remain a subsequent user-authorized phase.
