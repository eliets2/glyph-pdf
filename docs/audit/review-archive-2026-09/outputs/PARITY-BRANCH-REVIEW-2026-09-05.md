# GlyphPDF parity branch: progress and code-quality review

Historical snapshot report. The [fetched-remote row-by-row review at `4761443`](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/REMOTE-PARITY-REVIEW-2026-09-06.md) contains the current package dispositions and full-suite evidence.

Reviewed 5 September 2026. **Substantial implementation progress is present, but the reviewed changes need follow-up fixes before release acceptance.**

Scope updated 6 September: review code now and defer UI acceptance until the packages pass review and the newest application is reinstalled. The older installed application is not evidence of the packages' UI behavior. The build, test, and probe findings below come from the pinned source snapshot. A later check found HEAD had advanced to `cf5ddc7`; this report's findings and counts apply to `06b542d` until the newer delta is reviewed.

## Scope and correction

The active implementation worktree is `C:\Users\User\Projects\pdf-parity`, branch `feat/parity-glm`. This review is pinned to **`06b542db7256e01260e607a96bcbf62205abb79d`**. Local HEAD remained at that commit at the final repository check. The local remote-tracking reference also pointed there; no network fetch or independent verification of GitHub's live state was performed.

The original audit of `C:\Users\User\Projects\pdf` at `main` / `703fa34ece32733ea3b2093da94fa1aed94e1afc` remains a valid historical baseline. It was the wrong checkout for assessing the recent implementation progress. Statements that recent work was absent must not be applied to `pdf-parity`.

I read `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` and reconciled it with the actual source, commits, new tests, and the September repair packages, alongside the July parity documents reviewed earlier. The two endpoint trees differ in 52 files, with 9,200 insertions and 1,150 deletions; those are change-volume figures, not a quality or completion score.

The review used an isolated archive of the committed tree, with the worktree's existing PoDoFo, PDFium, and ONNX dependency trees copied into the audit workspace. Neither source checkout was edited, switched, merged, or reset. Concurrent changes in `tests/TestOcrPreprocessor.cpp`, `tests/mocks/MockFormManager.h`, and untracked `tests/TestImageDedup.cpp` were excluded, as were other sessions' build outputs.

## Verified checks

- Fresh CMake configuration and build succeeded for `PdfWorkstation` and the 12 targeted test executables below, using GCC 16.1 and Qt 6.11 in MSYS2 UCRT64. This was not a build of every test target.
- **12/12 targeted CTest suites passed in 37.11 seconds:** `TestOcrReviewLifecycle`, `TestOcrPreprocessor`, `TestControllers`, `TestCompareEntry`, `TestSignatureBadges`, `TestCompressDialogHonesty`, `TestFormSafety`, `TestFormUndo`, `TestExportPathBadge`, `TestConversionExtraction`, `TestOllamaProvider`, and `TestDiffEngine`.
- The pinned configuration registers **91** tests. The other 79 were not run in this branch review. The original main-checkout result of 84/84 is historical and must not be presented as branch-wide validation.
- The initial test attempt had runtime-DLL loader failures and an AI-test timeout. The successful run supplied the vendored runtime paths and used the required test-only Windows settings/local-server permissions. These initial environmental failures are retained in the evidence archive and are not reported as proven application regressions.
- Independent probes linked the freshly built production engine library. They confirmed a successful same-path form addition, a real parseable XLSX package, and the three behavioral failures described below. Passing repository tests do not cover these failures.

No sanitizer run, full OCR model evaluation, native duckx/OpenXLSX configuration, Word/Excel desktop round trip, installer validation, or complete competitive-parity acceptance is claimed.

## Findings and GLM Flash follow-up instructions

Source locations below refer to the pinned commit. Paths are relative to `C:\Users\User\Projects\pdf-parity`. Preserve unrelated work and implement one finding per bounded patch.

### V01 — P1: form import targets the loaded temporary path for deletion

**Evidence: source trace, with successful engine import confirmed separately.** `src/shell/controllers/FormsController.cpp:160–167` creates `original.pdf.tmp`, calls `viewer->loadDocument(outputPath)`, then uses `viewer->filePath()` as the removal and rename destination. `src/ui/PdfViewerWidget.cpp:291–299` assigns that new path immediately. Consequently, removal targets the imported temporary output and the rename uses that same path as source and destination. The original filename is never correctly replaced. Windows handle sharing determines whether removal succeeds; neither failure is checked, and the controller reports success regardless. This is a remaining caller defect, not evidence that the new shared FormManager transaction fails.

**Implement:** Capture the original path before import. Route the operation through the new safe-save boundary, preferably importing directly to the original destination through that boundary. If a candidate is still needed, validate and commit it before changing viewer/document identity. Remove the controller's delete-before-rename sequence. Check persistence and reload separately; preserve a successfully saved file if reload fails, and describe that outcome accurately. Reconcile the viewer and `DocumentSession` paths after success.

**Acceptance:** Import a known value into a synthetic existing form. Reopen the original filename and assert that value persisted, the viewer/session still reference the intended document, and original text/fields remain. Force save failure and assert original bytes, paths, and success notifications are unchanged. Exercise reload failure without deleting the saved result. Test the controller flow, not only `FormManager::importFormData`.

### V02 — P1: failed form undo advances history without restoring the file

**Evidence: independent runtime probe.** At `src/commands/EditFormFieldCommand.h:104–112`, a failed `applyFieldSnapshot` logs and returns. `QUndoStack` still moves its index. The probe pushed a successful edit, injected the existing `SaveFault::Commit`, then called `undo()`:

```text
UNDO_BEFORE 1 VALUE "Changed"
UNDO_AFTER_COMMIT_FAILURE 0 CAN_UNDO false VALUE "Changed"
```

The disk transaction correctly preserves the edited file, but the history now claims the edit was undone and offers no Undo retry. The existing `TestFormUndo` passes because its failure coverage does not establish this traversal contract.

**Implement:** Make the history transition and fallible persistence agree through a small, tested failure-aware boundary used by every undo entry point. A `QUndoCommand::undo()` return cannot veto the stack transition. Stage/validate persistence before committing the logical transition, or provide an explicit rollback protocol that cannot execute the mutation twice or recurse through signals. Report the failure to the user and preserve a retryable history state. Do not mark the failed undo obsolete as a substitute for restoring consistency. Check redo traversal and add-field undo for the same contract.

**Acceptance:** On a one-command and a multiple-command stack, fail an undo commit and verify file contents, stack index, clean state, action availability, and visible error. Remove the fault and retry successfully. Repeat for redo failure. Confirm normal undo/redo still restores absent versus empty values, tooltip, required flag, and unrelated fields.

### V03 — P1: the new text-run grouping collapses table columns

**Evidence: independent runtime probe and extracted XLSX XML.** `src/engines/pdfium/PdfiumBackend.cpp:338–366` groups characters using line breaks and baseline distance, without preserving separate horizontal cells or changes in font runs. `ConversionManager.cpp:744–759` writes one Excel cell per resulting element.

A synthetic PDF has headers `Name` and `Amount` at x=72 and x=300 on one baseline, with `Alice` and `125` beneath them. Both exports return success, but CSV contains:

```csv
"Name Amount"
"Alice 125"
```

The XLSX sheet has only A1=`Name Amount` and A2=`Alice 125`; there are no B-column cells. Unicode decoding is improved, but geometry needed by spreadsheet export has been discarded before the writer sees it.

**Implement:** Keep PDFium Unicode decoding. Preserve geometric runs or word boxes across horizontal gaps/text-object boundaries and font changes at the backend boundary. Separate line grouping from column inference. Derive cells consistently across rows using retained geometry, with a documented tolerance; avoid splitting prose at every ordinary space. Preserve logical reading order for text/Word consumers. Retain the R09 font-decoding fixtures.

**Acceptance:** Assert a real 2×2 XLSX cell grid and two CSV columns for this fixture, with exact cell text. Add a wider gap, multiword cell, empty interior cell, and mixed-font same-line case. Check extracted Unicode and top-to-bottom ordering remain correct. Keep the original glyph-code bug fixed while restoring useful geometry.

### V04 — P2: ordinary text edits become structural page changes

**Evidence: independent runtime probe.** `src/engines/DiffEngine.cpp:232–245` treats every page left unmatched by the similarity-based alignment as removed/added. A one-page PDF containing `Apple` compared with the same one-page layout containing `Orange` yields:

```text
ONE_PAGE_TEXT_EDIT 1 1 STRUCTURAL_CHANGES 2
STRUCTURAL 1 0 -1
STRUCTURAL 0 -1 0
```

Those are `PageRemoved` and `PageAdded`, in addition to the normal text difference. Low text similarity is not proof that the page structure changed. The new structural sequence propagates this classification into user-visible results and reports.

**Implement:** Represent aligned modified-page pairs separately from unmatched insertions/deletions. Use sequence context and an explicit substitution policy. Preserve exact/fuzzy anchors for real insertions and moves; do not solve this by pairing every same-numbered page, which would break middle insertions. Keep ambiguous alignment limitations explicit.

**Acceptance:** A one-page content edit reports content changes without structural add/remove. Appended and removed pages still report one structural change each. Add middle insertion plus edited neighbor, reorder, and repeated-page fixtures. Check tree totals, filters, navigation, and exported reports agree on classification.

### V05 — P1: OCR revision validation accepts same-path, same-count changes

**Evidence: source trace; limitation acknowledged in the ledger.** `src/shell/controllers/EditController.cpp:456–489` classifies completion using job generation, path, and page index. `ocrSessionIsExportable` at lines 497–515 checks validity, path, and page count. Neither establishes that the reviewed page is still the same revision. A replacement, reorder, content edit, or redaction can preserve path and page count. The stored review image and words can therefore be accepted after the source has changed.

**Implement:** Capture a document-session identity and mutation revision when the OCR page snapshot is created. Advance the revision at the existing successful mutation/reload boundaries, including undo/redo and page changes that alter document contents. Validate the captured revision at completion and export, while preserving the explicit reviewed-page identity when merely navigating to another page. If immutable historical review export is intentionally supported, label and isolate it as that operation instead of treating it as current-document validation.

**Acceptance:** Run OCR, then replace or reorder a page without changing page count; reject stale completion/export or require a new run. Repeat with an in-place text/redaction edit and same-path reopen. Ordinary page navigation must retain the explicitly labeled reviewed page under the chosen policy. Confirm corrections/deletions still survive independent saved-PDF extraction.

### V06 — P2: auto-detected fields are described as undoable but bypass history

**Evidence: source trace.** `src/shell/controllers/FormsController.cpp:129–141` constructs each `AddFormFieldCommand` on the stack, calls `redo()` directly, and discards it instead of registering it with the application's undo stack. The success message tells the user to undo as needed. The partial-failure message also says the document is unchanged even when some fields were successfully placed.

**Implement:** Use the existing undo stack and a suitable compound operation for the successful placements. Follow its ownership rules: a failed obsolete command may be deleted by `push`, so do not dereference it afterward. Report actual success/failure counts and ensure partial success remains recoverable. If the operation must remain non-undoable temporarily, disclose that before execution and remove the inaccurate promise.

**Acceptance:** Detect and add multiple synthetic fields, undo once under the chosen grouping policy, and reopen to verify removal. Inject failure after at least one success; keep accurate counts, no unchanged-document claim, and working undo for the successful portion.

## Package status reconciled with the ledger

“Targeted checks passed” below means the stated tests and inspected behavior have supporting evidence. It does not certify the whole parity domain or every original acceptance case.

| Package | Observed progress at `06b542d` | Review disposition |
|---|---|---|
| R01 | Shared candidate validation + checked safe commit; same-path addition succeeds; FormSafety passes | Engine repair supported; caller completion blocked by V01/V06. |
| R02 | Real before/after snapshots; empty-value handling; FormUndo passes | Partial acceptance; V02 fails independent review. |
| R03 | Network manager, event loop, deadline, cancellation and completion state owned within the worker | Targeted lifetime tests pass; no sanitizer claim. |
| R04 | Parsed host/loopback checks and manual redirect policy | Targeted endpoint tests pass. |
| R05 | Polarity repair absent from the pinned committed tree | Open. Concurrent preprocessing tests are not proof of a landed repair. |
| R06 | Deskew repair absent from the pinned committed tree | Open; depends on correct binary conversion and transform verification. |
| R07 | Explicit recoverable lifecycle and job-generation guards | Targeted lifecycle tests pass; revision handling still needs V05. |
| R08 | Reviewed-word model and Unicode export writer; corrected/deleted-word tests | Partial acceptance; V05 remains, along with ledger-disclosed limits. |
| R09 | Commit `466c706` uses PDFium Unicode; extraction suite passes | Implemented, despite stale “open” row; V03 blocks full acceptance. |
| R10 | Commit `06b542d` reconciles capability queries with built-in OOXML writers | Basic truthful-format checks pass; update stale gating row. Spreadsheet fidelity remains V03. |
| R11 | Structural model, UI/report wiring, new comparison tests | Implemented; V04 blocks full acceptance. Middle-insertion alignment also remains disclosed. |
| R12 | Unsupported compression passes disabled with explanations | Targeted honesty tests pass; complete live completion flow not exercised. |
| U01–U08 | Ledger marks all eight UI packages open | Still planned work; do not infer completion from R-package commits. |

There is implementation in **10 of 12 September repair packages**, with R05/R06 still open. That is an implementation count, not an “83% complete” acceptance score. The new ledger is useful, but R09/R10 and the test count need updating. Its statement about all commits through a historical hash being verified is too broad unless it links to specific independent acceptance evidence; a passing suite alone does not verify an entire commit or parity area.

## UI review and industry-informed direction

Computer use launched the freshly built pinned executable and inspected its empty-document accessibility tree. It confirmed the six welcome actions and recent-file list. Further screenshot capture was blocked by automatic approval review because the welcome screen exposes private recent-file names and paths. No private PDFs were opened or exported, and no screenshot assets are included here. This branch review does not claim a completed visual walkthrough.

Source inspection still shows the welcome layout constraint (`WelcomeWidget.cpp:45`, minimum card size 140×100; line 119, container maximum width 600), which conflicts with fitting six cards on one row. Retain U01's responsive grid and keyboard/theme checks. R08's word inspector is useful progress, but U03's source-image review remains separate work. Comparison still needs trustworthy classification before U04's linked navigation can be accepted.

The companion contains detailed UI packages and official-source research: [Adobe comparison](https://helpx.adobe.com/acrobat/using/compare-documents.html) for summaries and change navigation; [ABBYY verification](https://help.abbyy.com/en-us/finereader/16/user_guide/checkingtext/) for reviewing recognized text against the source image; [Bluebeam's Markups List](https://support.bluebeam.com/revu/how-to/track-and-manage-markups-using-markups-list.html) for review status/filtering; and [Foxit PDF Editor for Mac 2024.4 page organization](https://help.foxit.com/manuals/pdf-editor/mac/en-us/2024.4.0/Organize_PDF_Pages.html) for page selection and organization. These are documented interaction references, not rankings or claims of equivalent feature quality.

Prioritize V01/V02 and R05/R06, then V03/V05 and comparison classification. U01 can proceed independently; gate saved-artifact review workflows on their engine fixes. Keep existing Qt widgets, controllers, and engines. Avoid a broad refactor while these specific contracts remain unresolved.

## Deliverables and reproduction

- [Updated implementation and UI companion](GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md): original repair acceptance criteria, current-worktree correction, and revised GLM handoff.
- [Branch review evidence](parity-review-evidence.zip): setup/build/test helpers, final test log, initial environment failures, standalone probe source and output, synthetic PDFs/CSV/XLSX, and reproduction notes.

Run probe/build helpers only in the isolated audit workspace described in the archive README, or adapt their paths to a new isolated workspace. They are review tools, not files to drop blindly into the application's build graph.
