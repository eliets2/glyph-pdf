# GlyphPDF application and architecture review

**The main remaining architecture problem is inconsistent ownership of the open document.** The viewer, editing backend, undo history, annotation cache, dirty flag and task panels can describe different documents. This produces confirmed cross-document edits and annotation leakage, failed operations that still advance UI state, and a close action that proceeds after Save fails.

Review started 7 September and resumed 8 September 2026. The independently executed source/build baseline is **`0caa45e7d0751caaa36a54a085c41211a422f019`**, archived under the historical directory `work/parity-95676e6`. The new-commits reviewer checked **`b58b91054ca7a573a09c56d950588ac4f966df42`** and confirmed that the lifecycle blocks underlying ARC01–ARC07 are unchanged. Its only relevant MainWindow change resets the tool on redaction exit; that correctly addresses N07 but does not address these findings. No production checkout, installed application or user document was modified by this review.

## Scope and evidence

This assignment owns `src/app`, `src/shell`, `src/modes`, `src/ui`, `src/util` and top-level application files. The inventory contains **143 production files, including 141 C++ files with 31,780 lines**. Coverage is deliberately explicit: 21 files received focused deep review of lifecycle/caller paths, 11 received targeted review, 109 received an automated structural scan only, and two non-C++ assets were inventoried. “Deep” describes the investigated paths, not an assertion that every line in those files has been proven correct. The complete per-file manifest appears below and in the evidence JSON. Engine/command details were traced only where required to establish a production caller; the engine reviewer owns their broader review.

The new probe instantiates the real `MainWindow`, `PdfViewerWidget`, `DocumentSession`, `QUndoStack`, `PdfEditorEngine`, Home and Pages controllers, and PDF/A panel. It runs offscreen against two generated PDFs and an isolated INI profile. It links the existing baseline libraries without rebuilding or changing them. The final run exits **0** and records observed defects rather than asserting that they are acceptable behavior. No full-suite rerun was needed for this review-only probe. Prior reports' 107/107 CTest results remain historical evidence, not a substitute for these missing integration cases.

Evidence files are `work/team-architecture-review/probe.cpp`, `compile.py`, `probe.log`, `build.log`, `coverage.json` and `structural-scan.txt`; the probe also includes the existing own-fixture PDF writer from `work/parity_probe.cpp`. Two exploratory runs stopped at an unhandled Save error modal and were killed by the 30-second harness timeout; the final run has a bounded modal driver and completes. The final log is the evidence used below. No sanitizer, external application, full UI walkthrough or complete document corpus is claimed.

## Prioritized findings

### ARC01 — P1: undo history survives document changes and can mutate the next document

**Location:** `src/GpMainWindow.cpp:520–525`; `src/shell/controllers/HomeController.cpp:102–105`; cross-boundary example `src/commands/RotatePageCommand.h:22–29`.

`openDocument()` loads the next file and changes the shared `DocumentSession` path without clearing its shared `QUndoStack`. Commands such as `RotatePageCommand` consult `m_doc->path()` at execution time rather than keeping the identity of the document that created them. Once the editing engine is loaded for B, undoing a command created for A therefore operates on B. Command merging also compares page/type rather than document identity.

**Reproduction:** the real window opens A; the shared editor is initialized for A and a real rotation command changes A from 0 to 90 degrees. Opening B leaves history count 1. After initializing the same editor for B, undo changes B from 0 to 270 degrees. The extra explicit editor initialization models existing text/metadata flows that initialize the backend; the probe does not claim to have driven those dialogs. An ordinary sequence of editing B and then undoing back into A's retained history reaches the same ownership defect.

**Repair:** make a successful document switch clear the single-document history and invalidate document-specific state at one existing session boundary. Bind commands to an immutable session identity/revision so a stale command cannot execute against a different path. Do not let an unsuccessful open destroy the current state. Before switching away from dirty work, route through the same Save/Discard/Cancel decision as closing; the current open path silently clears the dirty flag.

**Acceptance:** edit A, switch to B, assert history is empty and Undo cannot change B; test A→B→A, identical page counts, failed/canceled open, and edits to B after the switch. Check actual saved PDFs, not only the undo index.

### ARC02 — P1: annotations from A are displayed and automatically saved into B

**Location:** `src/ui/PdfViewerWidget.cpp:291–299`, `586–595`, `196–201`, `568–583`.

`loadDocument()` changes `m_filePath` but does not clear the annotation list. If B has no `.ann` file, `loadAnnotations()` returns without replacing the old list. A pending debounce subsequently serializes the retained A annotations using B's path. This can move private comments or visible signature graphics into an unrelated document; it is more serious than a stale visual badge.

**Reproduction:** add the generated comment `ONLY_A_SECRET_NOTE` to A, immediately open clean B without a sidecar, and wait for the existing debounce. B's in-memory annotations contain the note, and the new B sidecar contains its text. Both are confirmed by the real widget/window probe.

**Repair:** before a document identity change, capture/flush or cancel the old document's pending annotation work according to the chosen save policy. Reset all document-specific overlay state, then load the new list with an explicit empty default for a missing sidecar. Associate each pending save with immutable source identity and a generation; do not let a stale timer resolve a new mutable path. Preserve annotations during a reload of the same revision rather than treating every reload as a new document. Use the existing serializer and a checked atomic sidecar commit; avoid introducing another annotation store.

**Acceptance:** A with comments/signature/redaction marks → B without sidecar produces an empty B; B with its own sidecar contains only B's records. Include a switch inside the debounce interval, save/reload races, failed open, and shutdown with a pending write. Inspect both sidecars and exported PDFs.

### ARC03 — P1: choosing Save while closing still closes after Save fails

**Location:** `src/GpMainWindow.cpp:809–812`; `src/shell/controllers/HomeController.cpp:117–203`.

`closeEvent()` dispatches the void Save action, then unconditionally accepts the close. A failed save displays its own modal, but dismissing that modal does not stop closing. This discards the user's opportunity to choose a different destination or retry.

**Reproduction:** with a dirty generated PDF open, the actual Save path fails with `embedAnnotations: cannot remove original for in-place replace`. The modal driver selects Save in the real close prompt and dismisses the resulting failure dialog. The resulting close event is accepted while `DocumentSession::isDirty()` remains true. No injected production stub was required. The engine reviewer is investigating the separate backend replacement defect; even after it is fixed, disk/permission errors still require correct close behavior.

**Repair:** expose a small explicit Save result (`Saved`, `Canceled`, `Failed`) from the existing Home save operation. Have close and document-switch paths proceed only on `Saved` or the user's explicit Discard choice. Return failure on every guard/engine/write failure. Avoid using “save initiated” as proof of persistence.

**Acceptance:** successful save permits close; canceled destination, permission error, failed commit and backend refusal keep the window/document open. Verify history and dirty state remain available for retry.

### ARC04 — P2: dirty state has multiple disconnected authorities

**Location:** `src/ui/PdfViewerWidget.cpp:193–207`; `src/shell/controllers/HomeController.cpp:189`; `src/GpMainWindow.cpp:198–217`; `src/engines/DocumentSession.cpp:34–45`.

Annotation changes start a sidecar timer but do not mark `DocumentSession` dirty. Successful Save marks only the undo stack clean; there is no `cleanChanged` wiring that clears the session flag. The title, unsaved indicator, autosave decisions and close prompt read the session flag, so they need not describe whether edits have been embedded into the PDF.

**Evidence:** immediately after a real annotation-list edit, the session is still clean. The successful-save branch's missing session update is source-confirmed. The probe's attempted Save operations encountered the separate backend replacement failure, so it does **not** claim to demonstrate a successful save followed by a stale dirty flag.

**Repair:** keep one explicit session dirty policy for embedded document changes and annotation edits. Connect real mutation events to it; establish the clean baseline only after a checked successful save. Do not conflate sidecar persistence with PDF persistence. If undo-stack clean state participates, account separately for mutations that do not use the stack and avoid resetting dirty state merely by assigning a path.

**Acceptance:** add/edit/delete an annotation, undo/redo it, Save, fail Save, and close. The title, status, session and history must agree with whether the output PDF includes the edit. Include non-command mutations such as tab-order changes.

### ARC05 — P1: a successful Open does not initialize the editing backend

**Location:** `src/GpMainWindow.cpp:515–525`; `src/app/Bootstrapper.cpp:34–40`; `src/shell/controllers/PagesController.cpp:44–63`; `src/engines/PdfEditorEngine.cpp:61–65`, `73–114`.

Open loads Qt's viewing document and updates the path, but the shared `PdfEditorEngine` remains default-constructed with no backend. A number of editing/save callers assume that engine already represents the open document. Other callers initialize it ad hoc, so behavior depends on which tool was used first. This also leaves pathless engine queries and task panels vulnerable to a previously loaded document after switching.

**Reproduction:** immediately after the real MainWindow successfully opens the generated PDF, the real editor's rotation returns false with “No document is open for editing.” Explicit `loadDocumentForEditing(A)` makes the same rotation succeed. The probe separately demonstrates that the ordinary Pages controller can mutate through a previously primed backend.

**Repair:** establish and validate both viewer and editing session at the document-open boundary, or introduce one existing-boundary `ensureEditorForCurrentSession()` used consistently before operations. Prefer the former for this single-document app, while keeping any expensive work and failure UI explicit. Publish the new document identity only once the required load succeeds. Do not leave every controller responsible for a different partial initialization sequence.

**Acceptance:** from a fresh context, Open → Rotate, Save, attachment query and PDF/A export operate on the opened PDF without another tool priming the engine. Repeat after switching to B and after an invalid/encrypted-file load failure. Assert both caller results and saved document contents.

### ARC06 — P2: the application never gives the PDF/A panel its document

**Location:** `src/GpMainWindow.cpp:699–705`; `src/modes/PdfAValidationPanel.cpp:94`, `143–146`, `158–163`.

The PDF/A navigation branch creates the panel and sets an export callback, but never calls `setDocument(path)`. That method is the panel's only production path setter and validation entry point. The real application panel consequently keeps its empty-path state even though the viewer has a PDF open. Helper/analysis tests that call `setDocument()` directly do not cover this integration.

**Reproduction:** open generated B through MainWindow, activate the PDF/A screen, and inspect its real labels: `PDFA_PANEL_SAYS_NO_DOCUMENT true`, `VIEWER_HAS_DOC true`.

**Repair:** provide the active document identity when entering the panel, and refresh/invalidate it on successful document changes. Bind export to that same input identity. Keep asynchronous validation results tied to the submitted identity so a later A result cannot populate B. The external validator may be unavailable, but that should produce the specific availability explanation rather than “No document loaded.”

**Acceptance:** enter with no file, enter with tagged/untagged PDFs, switch A→B while panel exists, and request reading-order analysis. Use a real MainWindow integration test with an injected local validation boundary; do not require a network service.

### ARC07 — P2: read-only is enforced only for tool selection, not mutations

**Location:** `src/ui/PdfViewerWidget.cpp:486–498`; `src/shell/controllers/PagesController.cpp:36–63`, `123–135`; expiry entry `src/GpMainWindow.cpp:537–548`.

`setReadOnly()` prevents selecting editing tool modes, but controller actions that directly push commands or call engines do not check it. The application's “opened in read-only mode” promise is therefore false for page actions. This is an application workflow guarantee, not a claim that metadata expiry is cryptographic access control.

**Reproduction:** after setting the real viewer read-only, dispatching `PagesController::activate(ToolId::RotateCW)` changes the generated PDF's rotation from 270 to 0 while `viewer->isReadOnly()` remains true.

**Repair:** enforce editability at the shared command/mutation dispatch boundary and reflect it in action enablement. Keep viewing, selection and permitted copy/export actions available. Do not rely solely on cursor/tool-mode gating, and do not scatter a separate policy implementation through every dialog.

**Acceptance:** in read-only state, page rotate/delete/insert/reorder, annotation/form edits, metadata changes and save-in-place must obey the same policy. Re-enabling editing restores only actions otherwise eligible.

## Architecture assessment and bounded improvement sequence

The broad layering is useful: Bootstrapper assembles shared interfaces; ToolRegistry routes actions to controllers; ModeController owns task surfaces; the viewer supplies Qt rendering/annotation presentation; engines handle document operations. Existing `SafeSave`, capability registry, serializer and session abstractions should be retained. The defects arise at their joining points, not from an inherent need to replace the framework.

| Concern | Current ownership | Required single boundary |
|---|---|---|
| Current document | Viewer path, session path and loaded editor can differ | One successful open/switch transaction publishes identity and required engine readiness |
| Undo/redo | One app-lifetime stack, commands often resolve mutable session path | History scoped to one document identity; stale commands refuse execution |
| Annotation persistence | Viewer list, delayed sidecar writer, embedded PDF save | One versioned annotation state with explicit sidecar/PDF persistence outcomes |
| Dirty/clean | Session boolean and undo clean index are disconnected | Session policy reflects actual document mutations and checked persistence |
| Operation completion | Several void controller actions plus status text | Small explicit result values for save/commit-dependent callers |
| Task panels | Lazy panels initialized independently | Common active-document notification with identity-tagged async results |
| Editability | Viewer cursor/tool gate | Shared mutation gate also used by action enablement |

Implement ARC01/ARC02/ARC03/ARC05 first as small changes at existing boundaries. Then wire dirty state, PDF/A entry and read-only policy. Add only integration tests that cross the boundaries implicated by real failures. Do not replace AppContext with a new dependency framework, create another capability registry, or undertake a broad MVC rewrite as a prerequisite.

Large files such as BatchMode (1,631 lines), PdfViewerWidget (1,468), InspectorWidget (1,389), OCRMode (1,299), EditController (1,004) and MainWindow (921) are review-cost indicators, not defects by line count alone. Extract a workflow only when doing so removes an actual duplicated decision or makes an operation's input/result lifetime explicit. The highest-value first extraction is the existing open/save/session boundary, not generic helpers for layout code.

Async work should capture immutable document identity plus owned worker state, then verify identity before applying results. The annotation debounce failure is a concrete example of why a weak widget pointer alone is insufficient. Existing redaction D02 remains owned by the engine review; a repaired AI owner test does not settle it.

## Previously known findings and latest-code boundary

- **V01 form import remains source-confirmed at baseline:** `FormsController.cpp:160–166` loads the temporary output into the viewer, then deletes `viewer->filePath()` and renames the same temporary path to itself. This review does not relabel it as a new finding.
- **V06 auto-detect undo remains source-confirmed:** `FormsController.cpp:129–141` executes a stack-local command directly and discards it, while telling the user to undo. Original V02's failed form-undo history contract is handled by the command/engine reviewer.
- N01/N04/N05/N06/N07 and the redaction/signature packages have intervening fixes in the latest commits. Their final dispositions belong to the new-commits report. In particular, the new redaction-exit tool reset and real document-switch signature-cache notification should not be overwritten by this baseline report.
- The existing code-quality/GLM companion remains the detailed queue for earlier V/D/N findings. ARC01–ARC07 are complementary application integration findings, not substitutes for the ledger's independent per-package acceptance protocol.

## Per-file coverage manifest

The following categories record work actually done. Structural scan means automated enumeration of functions and selected lifecycle/IO patterns; it is **not** line-by-line review or feature verification. Non-C++ assets received inventory only. Files under the other agents' owned engine/core/command/document-model namespaces are excluded here.

<!-- Coverage table is generated from work/team-architecture-review/coverage.json. -->

| File | Lines | Review depth |
|---|---:|---|
| src/app/app.ico | asset | inventory-only non-C++ asset |
| src/app/app_icon.rc | asset | inventory-only non-C++ asset |
| src/app/Bootstrapper.cpp | 63 | focused lifecycle/caller deep review |
| src/app/Bootstrapper.h | 10 | focused lifecycle/caller deep review |
| src/app/main.cpp | 170 | automated structural scan only |
| src/GpMainWindow.cpp | 921 | focused lifecycle/caller deep review |
| src/GpMainWindow.h | 121 | focused lifecycle/caller deep review |
| src/modes/AIChatPanel.cpp | 154 | automated structural scan only |
| src/modes/AIChatPanel.h | 41 | automated structural scan only |
| src/modes/BatchMode.cpp | 1631 | automated structural scan only |
| src/modes/BatchMode.h | 250 | automated structural scan only |
| src/modes/CompareMode.cpp | 775 | automated structural scan only |
| src/modes/CompareMode.h | 99 | automated structural scan only |
| src/modes/CompressDialog.cpp | 628 | automated structural scan only |
| src/modes/CompressDialog.h | 73 | automated structural scan only |
| src/modes/FormBuilderMode.cpp | 593 | targeted source review |
| src/modes/FormBuilderMode.h | 108 | automated structural scan only |
| src/modes/FormFieldPropertiesPanel.cpp | 215 | targeted source review |
| src/modes/FormFieldPropertiesPanel.h | 64 | automated structural scan only |
| src/modes/ModeController.cpp | 106 | focused lifecycle/caller deep review |
| src/modes/ModeController.h | 74 | focused lifecycle/caller deep review |
| src/modes/OcrConfidence.cpp | 41 | automated structural scan only |
| src/modes/OcrConfidence.h | 40 | automated structural scan only |
| src/modes/OCRMode.cpp | 1299 | automated structural scan only |
| src/modes/OCRMode.h | 256 | automated structural scan only |
| src/modes/OcrReviewSession.h | 52 | automated structural scan only |
| src/modes/PagesMode.cpp | 1527 | targeted source review |
| src/modes/PagesMode.h | 171 | automated structural scan only |
| src/modes/PdfAValidationPanel.cpp | 591 | focused lifecycle/caller deep review |
| src/modes/PdfAValidationPanel.h | 84 | focused lifecycle/caller deep review |
| src/modes/RedactApplyDialog.cpp | 387 | automated structural scan only |
| src/modes/RedactApplyDialog.h | 108 | automated structural scan only |
| src/modes/RedactMode.cpp | 738 | targeted source review |
| src/modes/RedactMode.h | 112 | automated structural scan only |
| src/modes/SignaturesPanel.cpp | 261 | automated structural scan only |
| src/modes/SignaturesPanel.h | 65 | automated structural scan only |
| src/modes/WatermarkDialog.cpp | 333 | automated structural scan only |
| src/modes/WatermarkDialog.h | 59 | automated structural scan only |
| src/shell/controllers/ConvertController.cpp | 658 | automated structural scan only |
| src/shell/controllers/ConvertController.h | 57 | automated structural scan only |
| src/shell/controllers/EditController.cpp | 1148 | targeted source review |
| src/shell/controllers/EditController.h | 204 | automated structural scan only |
| src/shell/controllers/FormsController.cpp | 197 | focused lifecycle/caller deep review |
| src/shell/controllers/FormsController.h | 35 | focused lifecycle/caller deep review |
| src/shell/controllers/HomeController.cpp | 747 | focused lifecycle/caller deep review |
| src/shell/controllers/HomeController.h | 60 | focused lifecycle/caller deep review |
| src/shell/controllers/PagesController.cpp | 215 | focused lifecycle/caller deep review |
| src/shell/controllers/PagesController.h | 45 | focused lifecycle/caller deep review |
| src/shell/controllers/SecurityController.cpp | 877 | targeted source review |
| src/shell/controllers/SecurityController.h | 70 | automated structural scan only |
| src/shell/controllers/ViewController.cpp | 194 | automated structural scan only |
| src/shell/controllers/ViewController.h | 42 | automated structural scan only |
| src/shell/MenuBar.cpp | 484 | automated structural scan only |
| src/shell/MenuBar.h | 53 | automated structural scan only |
| src/shell/ModeStrip.cpp | 286 | automated structural scan only |
| src/shell/ModeStrip.h | 63 | automated structural scan only |
| src/shell/Ribbon.cpp | 307 | automated structural scan only |
| src/shell/Ribbon.h | 64 | automated structural scan only |
| src/shell/RibbonModel.cpp | 124 | automated structural scan only |
| src/shell/RibbonModel.h | 37 | automated structural scan only |
| src/shell/ScreenNav.cpp | 55 | automated structural scan only |
| src/shell/ScreenNav.h | 26 | automated structural scan only |
| src/shell/Sidebar.cpp | 324 | focused lifecycle/caller deep review |
| src/shell/Sidebar.h | 57 | focused lifecycle/caller deep review |
| src/shell/StatusBar.cpp | 259 | focused lifecycle/caller deep review |
| src/shell/StatusBar.h | 79 | focused lifecycle/caller deep review |
| src/shell/TaskNav.cpp | 99 | automated structural scan only |
| src/shell/TaskNav.h | 67 | automated structural scan only |
| src/shell/TaskStateSync.cpp | 40 | automated structural scan only |
| src/shell/TaskStateSync.h | 47 | automated structural scan only |
| src/shell/ToolRegistry.cpp | 65 | automated structural scan only |
| src/shell/ToolRegistry.h | 47 | automated structural scan only |
| src/ui/AnnotationLayer.cpp | 787 | targeted source review |
| src/ui/AnnotationLayer.h | 94 | focused lifecycle/caller deep review |
| src/ui/AnnotationToolBar.cpp | 139 | automated structural scan only |
| src/ui/AnnotationToolBar.h | 40 | automated structural scan only |
| src/ui/BatesNumberingDialog.cpp | 175 | automated structural scan only |
| src/ui/BatesNumberingDialog.h | 47 | automated structural scan only |
| src/ui/BookmarkPanel.cpp | 102 | automated structural scan only |
| src/ui/BookmarkPanel.h | 39 | automated structural scan only |
| src/ui/CommentsWidget.cpp | 962 | targeted source review |
| src/ui/CommentsWidget.h | 125 | automated structural scan only |
| src/ui/CompareWidget.cpp | 532 | automated structural scan only |
| src/ui/CompareWidget.h | 178 | automated structural scan only |
| src/ui/DocumentPropertiesWidget.cpp | 120 | automated structural scan only |
| src/ui/DocumentPropertiesWidget.h | 39 | automated structural scan only |
| src/ui/EditAnnotationCommand.cpp | 8 | targeted source review |
| src/ui/EditAnnotationCommand.h | 43 | automated structural scan only |
| src/ui/EditToolBar.cpp | 148 | automated structural scan only |
| src/ui/EditToolBar.h | 44 | automated structural scan only |
| src/ui/EncryptionDialog.cpp | 177 | automated structural scan only |
| src/ui/EncryptionDialog.h | 35 | automated structural scan only |
| src/ui/ErrorDialog.cpp | 198 | automated structural scan only |
| src/ui/ErrorDialog.h | 58 | automated structural scan only |
| src/ui/ExportPresetsPanel.cpp | 239 | automated structural scan only |
| src/ui/ExportPresetsPanel.h | 64 | automated structural scan only |
| src/ui/FindBar.cpp | 192 | automated structural scan only |
| src/ui/FindBar.h | 64 | automated structural scan only |
| src/ui/HeaderFooterDialog.cpp | 59 | automated structural scan only |
| src/ui/HeaderFooterDialog.h | 25 | automated structural scan only |
| src/ui/InspectorWidget.cpp | 1389 | automated structural scan only |
| src/ui/InspectorWidget.h | 101 | automated structural scan only |
| src/ui/MetadataDialog.cpp | 48 | automated structural scan only |
| src/ui/MetadataDialog.h | 29 | automated structural scan only |
| src/ui/OcrScanCanvas.cpp | 159 | automated structural scan only |
| src/ui/OcrScanCanvas.h | 58 | automated structural scan only |
| src/ui/OcrWordMagnifier.cpp | 126 | automated structural scan only |
| src/ui/OcrWordMagnifier.h | 58 | automated structural scan only |
| src/ui/PageManagementDialog.cpp | 85 | automated structural scan only |
| src/ui/PageManagementDialog.h | 46 | automated structural scan only |
| src/ui/PageSetupDialog.cpp | 189 | automated structural scan only |
| src/ui/PageSetupDialog.h | 60 | automated structural scan only |
| src/ui/PdfViewerWidget.cpp | 1468 | focused lifecycle/caller deep review |
| src/ui/PdfViewerWidget.h | 264 | focused lifecycle/caller deep review |
| src/ui/PermissionsDialog.cpp | 124 | automated structural scan only |
| src/ui/PermissionsDialog.h | 36 | automated structural scan only |
| src/ui/PreferencesDialog.cpp | 485 | targeted source review |
| src/ui/PreferencesDialog.h | 49 | automated structural scan only |
| src/ui/RecoveryDialog.cpp | 91 | automated structural scan only |
| src/ui/RecoveryDialog.h | 27 | automated structural scan only |
| src/ui/ResizeDialog.cpp | 68 | automated structural scan only |
| src/ui/ResizeDialog.h | 26 | automated structural scan only |
| src/ui/ShortcutHelpDialog.cpp | 98 | automated structural scan only |
| src/ui/ShortcutHelpDialog.h | 13 | automated structural scan only |
| src/ui/SignatureDialog.cpp | 312 | automated structural scan only |
| src/ui/SignatureDialog.h | 56 | automated structural scan only |
| src/ui/SignaturePicker.cpp | 453 | targeted source review |
| src/ui/SignaturePicker.h | 165 | automated structural scan only |
| src/ui/SignaturesWidget.cpp | 105 | automated structural scan only |
| src/ui/SignaturesWidget.h | 30 | automated structural scan only |
| src/ui/ThumbnailSidebar.cpp | 494 | automated structural scan only |
| src/ui/ThumbnailSidebar.h | 67 | automated structural scan only |
| src/ui/UpdateDialog.cpp | 125 | automated structural scan only |
| src/ui/UpdateDialog.h | 32 | automated structural scan only |
| src/ui/WelcomeWidget.cpp | 423 | automated structural scan only |
| src/ui/WelcomeWidget.h | 58 | automated structural scan only |
| src/util/Badge.cpp | 32 | automated structural scan only |
| src/util/Badge.h | 17 | automated structural scan only |
| src/util/GpTheme.h | 80 | automated structural scan only |
| src/util/Icons.cpp | 75 | automated structural scan only |
| src/util/Icons.h | 22 | automated structural scan only |
| src/util/Slider.cpp | 33 | automated structural scan only |
| src/util/Slider.h | 19 | automated structural scan only |
