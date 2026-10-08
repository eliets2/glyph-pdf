# GlyphPDF engine, core and command quality review

**The older code contains source-destruction and document-identity defects that the recent package tests do not address.** The highest priority findings are a same-file engine save that truncates a valid PDF, autosave writing the wrong document into another document's recovery file, and image undo deleting a page after its restore step fails.

Review started 7 September and completed after resumption on **8 September 2026**. This agent reviewed the fixed **`0caa45e7d0751caaa36a54a085c41211a422f019`** snapshot in `work/parity-95676e6`; the directory name is historical. The matching existing build supplied production static libraries. The new-commits reviewer independently confirmed the source segments behind EC01–EC04 and EC06 are unchanged at **`b58b91054ca7a573a09c56d950588ac4f966df42`**. The team's final report controls dispositions of newly changed code.

This is a review, not an implementation. No repository production file, branch, shared build graph, installed app or private PDF was changed. Tests used generated PDFs, a fake credential string in a review-owned file, offscreen Qt, and isolated executables. No OS credential-vault record was created or read. The fake secret-store probe used the application's default key derivation but a custom output path.

## New actionable findings

### EC01 — P1: ordinary same-file engine saving can destroy the source PDF

**Runtime reproduced using the real `PdfEditorEngine` and PoDoFo backend.** At `src/engines/podofo/PoDoFoBackend.cpp:222–237`, `saveDocument` sends the destination directly to `d->document->Save(...)`. That document can still have lazily loaded objects backed by the same source file. Opening that path for output truncates it before later reads complete. `writeUpdate` delegates unsigned documents to this function at lines 338–340, so structural mutators such as `rotatePage` at 578–588 reach the same defect. `PdfEditorEngine::saveDocument` delegates there at line 214.

The probe creates a valid, two-page PDF of **7,008 bytes**, loads it for editing, and performs these independent cases:

| Real-engine operation | Return | Saved result |
|---|---|---|
| Save to a distinct path | `true` | PDFium reopens two pages |
| Save to the loaded source path | `false` | Source becomes **0 bytes**, PDFium cannot open it |
| Rotate page 0 by 90 degrees at the loaded path | `false` | Source becomes **0 bytes**, PDFium cannot open it |

The backend reports `InvalidNumber` while trying to read an object/generation number after truncation. The evidence preserves each original fixture as `.before` and records its SHA-256. This is separate from the already repaired **FormManager** safe-save boundary and from the redaction transaction's candidate-commit fixes. Those repairs do not cover this shared engine save.

**Minimal repair:** implement the existing candidate/validate/checked-commit pattern at the common engine save boundary, reusing `SafeSave`. Fully serialize to a distinct candidate before touching the destination. Ensure the source stream and Windows handle lifecycle permit replacement, and restore a usable backend from the committed document. A failed replacement must preserve source bytes and leave the current edit state recoverable. Keep signed incremental updates as a distinct validated contract; do not route them through an unsigned full rewrite. Review the delete-then-copy fallbacks in `PdfEditorEngine.cpp:195–200` and `PoDoFoBackend.cpp:350–357` while fixing this shared boundary, rather than adding caller-specific temporary files.

**Acceptance:** real save/rotate/delete/crop/reorder operations on generated PDFs with lazy font/content objects; same-path and distinct-path outputs; injected serialization/commit failure; existing destination preservation; reopen/content/page-count checks. Record source and output hashes. Check GUI-held file handles separately: this probe exercises the engine directly, and a viewer lock can change a destructive failure into an access-denied failure. It does not establish that every installed-app Save path truncates documents.

### EC02 — P1: autosave can write document B into document A's recovery file

**Deterministic real-engine reproduction.** `AutosaveManager.cpp:84–91` captures the editor's current path and derives A's temporary/final autosave names. The queued worker at lines 135–148 later calls `editor->saveDocument(tmpAutosavePath)` on the shared, mutable editor. The editor may have loaded B meanwhile. Its internal mutex protects each call, but not the identity relationship between the earlier path read and the later save. Completion at lines 103–118 also timestamps whichever `DocumentSession` is current.

The probe occupies the global pool with a barrier, invokes the real autosave slot for dirty A, loads B into the same editor/session, then releases the worker. It reports success; **`A.pdf.autosave.pdf` reopens with text `AUTOSAVE_B`**, and B's `lastAutosave` becomes valid. No timing lottery or private document is involved.

**Minimal repair:** bind the save to a captured document identity at the engine boundary. Check that identity under the same lock that serializes saving, or save an operation-owned snapshot. A path check outside that lock leaves the race intact. Include the identity in completion and only update the matching session; stale work should terminate with a clear cancelled/stale outcome. Reuse the session revision/identity work from the architecture review rather than creating a second lifecycle registry.

**Acceptance:** use a barrier to force A→B before the worker saves; A's recovery output must never contain B. Cover close, A→B→A, save-as and same-path reload, an edit after autosave starts, and completion after a session switch. Verify both artifact content and which session receives the timestamp.

### EC03 — P1: failed image restoration still deletes the following page

**Command-boundary fault reproduced independently of EC01.** `DeleteImageCommand.h:24–26` and `ReplaceImageCommand.h:23–25` ignore `insertPageFromBytes` failure and unconditionally call `deletePage(m_page + 1)`. The intended algorithm is insert the backup, then remove the displaced edited page. If insertion fails, the page at `m_page + 1` is the original following page. The commands also announce reload regardless of either result.

The actual controller at `EditController.cpp:959–971` passes extraction results without rejecting an empty backup. There is another realistic failure route: `PoDoFoBackend.cpp:617–619` rejects a backup over 10 MB, while extraction does not enforce the same limit. A large image page can therefore be backed up successfully but rejected during undo.

The isolated fault engine deliberately returns `false` from insertion. Both real command classes then issue **one delete at index 1** and mark the session dirty. A separate real-backend run reaches the same delete path but encounters EC01 and corrupts the file; that run is **not** presented as independent proof of a successfully saved missing-second-page artifact.

**Minimal repair:** do not allow the initial destructive edit without a valid, restorable backup. Stop immediately on failed insertion. Prefer a single atomic page-replacement operation using the existing document candidate transaction, so a successful insertion followed by a failed deletion cannot leave duplicate pages either. Propagate failure to the owner of undo history; merely returning from `QUndoCommand::undo` does not prevent Qt from moving its history index. Address the original V02 history failure contract with the same mechanism.

**Acceptance:** empty, malformed and over-limit backups; insert failure; remove failure after insertion; initial edit failure; success restoring original image and both page identities. Assert page count/content, source bytes on failure, dirty/reload signals and undo-stack index rather than just backend call counts.

### EC04 — P2: the Windows encrypted-file credential fallback cannot reread its own key

**Runtime reproduced with synthetic data.** `EncryptedFileSecretStore.cpp:65–82` calls `CryptProtectData` on every `resolveKey()` invocation, then hashes the newly wrapped ciphertext as the AES key. DPAPI protection produces a fresh wrapped blob; it is not a deterministic key-derivation operation. Encryption and subsequent decryption therefore use different AES keys. There is no persisted wrapped master key or corresponding `CryptUnprotectData` path.

With a custom review-owned store path and no key override, `storeSecret("review-fake-service", "not-a-real-credential")` returns **false**, and rereading fails. An otherwise identical store with an explicitly injected fixed key material returns **true** and rereads correctly. Thus the existing test-only override hides the default Windows failure. The primary OS Credential Manager is a different path; this finding affects its fallback, not all successful vault writes.

**Minimal repair:** on Windows, persist a DPAPI-protected secret and use `CryptUnprotectData` to read it, or persist one randomly generated master key protected by DPAPI and reuse the unwrapped key. Prefer the direct platform primitive if no cross-platform blob-sharing contract requires AES. Use a versioned format/migration rule and fail clearly on protection or unprotection errors. Do not fall back to hashing public home-path/machine-identity values as a secret key; those values are identifiers, not confidential entropy.

**Acceptance:** test the actual default path implementation with a temporary store, multiple reads, a new store instance and a new process; corruption and failed DPAPI operations must be explicit failures. Keep the injected-key crypto tests as separate low-level checks. Do not use real user API keys in tests.

### EC05 — P2: the crop command exposes an Undo action that does not restore the crop

**Source/caller trace and real-command history probe.** `CropPageCommand.cpp:10–14` explicitly implements undo as a reload only. The live `PagesController.cpp:209` pushes this command onto `QUndoStack`. The probe uses the real command with a recording crop backend: after push and undo, the stack index is **0**, the backend has received only **one** crop write, and the original rectangle is **not restored**. This is an incomplete user contract, not merely missing test coverage.

**Minimal repair:** capture the effective original CropBox before mutation, then restore it through the same safe mutation boundary. Preserve whether a box was inherited or explicitly present if that distinction affects the writer. Reject the command if its snapshot or initial mutation fails. Reuse the document identity/history failure handling rather than treating crop as a special history system.

**Acceptance:** crop→undo→redo using a saved/reopened PDF with nondefault and inherited boxes, failure paths, and document switches. Check geometry and history together. This probe did not independently render the crop UI.

### EC06 — P3: superseded render prefetch work is not drained by `clear`

**Library contract reproduced; no current production prefetch caller found.** `RenderCache::prefetchViewport` replaces its sole `m_prefetchFuture` at line 297 while the previous worker can still be inside `renderer->renderPage`. Cancellation is cooperative between renders. `clear()` at lines 56–58 waits only for the newest future, although each worker captures a raw renderer pointer.

The probe blocks the first renderer, completes a second prefetch, then calls `clear()`. It returns with **one old render still in flight**. The probe releases and joins that worker before destruction, so no use-after-free or process crash was deliberately induced. The source scan found `ThumbnailSidebar` using `RenderCache`, but **no production call to `prefetchViewport`**; this is not evidence of a current viewer crash.

**Minimal repair before activating this API:** retain/drain every in-flight prefetch, or serialize replacement so a prior render completes before its renderer can be retired. Give the renderer an ownership contract covering each active job. A cancellation token alone cannot interrupt a render already executing. If prefetch is unused, leave it disabled until this contract is tested rather than adding a new scheduling subsystem.

## Existing findings and architecture assessment

The [previous review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/LATEST-QUALITY-REVIEW-2026-09-07.md) and its linked V/D/N evidence were read, together with the parity evidence ledger. This pass does not silently promote those rows to verified. In particular, the field-edit undo at `EditFormFieldCommand.h:104–110` still logs a failed restore and returns while Qt advances history; this remains the original **V02**. **EC03** broadens that family with a distinct destructive second operation. The original **V01** form import is a controller-boundary issue reviewed by the architecture lane.

R09's conversion extraction fix does not automatically certify the separate raw-token text reader in `PdfStructureMapper`, or every export layout. Signature, OCR, conversion, redaction and comparison files received risk scans and selected call traces, not a new conformance/security certification. Baseline N02/N03/N06 and redaction/diff/split findings overlap the new-commit lane; use that lane's current dispositions. Earlier runtime acceptances for bounded FormManager safe save and repaired redaction candidate commits are retained within their stated scope.

The main architectural issue is **inconsistent transaction and identity boundaries**, rather than a shortage of interfaces. Forms already have a candidate/validate/commit helper while the shared PDF engine still writes loaded sources directly. Jobs own managers but not necessarily the document identity they operate on. Commands frequently ignore boolean failure while advancing visible history. Several small commands are marked undoable without retaining enough state for a true inverse; `DeleteFormFieldCommand` additionally documents recreating every deleted field as an empty text field, which deserves a separate type-preserving snapshot check before acceptance.

Keep the positive foundations: `SafeSave`, explicit outcomes, the backend path-divergence guard, PDFium-decoded conversion text, bounded subprocess paths, typed OCR results, and simple document-model containers. Do not remove the path guard to make split or autosave appear to work. Consolidate save behavior in the existing engine and identity in the existing session. Add one meaningful failure test at each repaired production boundary.

Large files make these contracts difficult to see: the reviewed snapshot has approximately 4,000 lines in `PoDoFoBackend.cpp`, 2,500 in `SignatureManager.cpp`, 1,800 in `PdfEditorEngine.cpp`, and 1,500 in `FormManager.cpp`. Extract a bounded helper only when a repair needs one or removes duplication. A framework rewrite, new registry or broad service layer is not justified by these findings.

## Evidence, coverage and limits

The independent executable was compiled against the fixed baseline's existing production libraries. The final probe run exited **0**; its output deliberately records failing application contracts, so process success is not feature acceptance. The agent did not rerun or alter the shared CTest suite. The prior team's 107/107 result is historical context, not new proof from this lane. No sanitizer, full optional-dependency build matrix, actual OCR model inference, external Office/veraPDF validation, certificate revocation service, updater installation or live installed UI review was performed.

The [engine evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/team-engine-review-evidence-2026-09-07.zip) contains the probe and runner, build/probe logs, generated before/after PDFs, inventory with hashes, structural scan output and this report. The manifest below enumerates **every file in the assigned production directories**, with an explicit depth label. A scanned file is not independently accepted and is not a claim of line-by-line review.

Depth labels: **T** = selected high-risk functions and caller/ownership contract traced (not every line); **R** = complete small implementation/header read with its stated API contract; **S** = inventory plus structural risk scan only. Build metadata and the Djot version marker are enumerated as supporting files. No vendored implementation was treated as owned project code.

Inventory: 152 files, 28,616 lines including comments, headers and supporting metadata.

| File | Lines | Depth |
|---|---:|:---:|
| src/commands/AddFormFieldCommand.h | 85 | R |
| src/commands/CropPageCommand.cpp | 24 | T |
| src/commands/CropPageCommand.h | 25 | R |
| src/commands/DeleteFormFieldCommand.h | 74 | R |
| src/commands/DeleteImageCommand.h | 36 | T |
| src/commands/DeletePageCommand.h | 38 | T |
| src/commands/EditFormFieldCommand.h | 137 | T |
| src/commands/EditTextInlineCommand.h | 58 | S |
| src/commands/EncryptDocumentHelper.h | 31 | S |
| src/commands/InsertPageCommand.h | 32 | S |
| src/commands/MoveFormFieldCommand.h | 65 | R |
| src/commands/MoveImageCommand.h | 40 | R |
| src/commands/ReorderPermutationCommand.cpp | 40 | R |
| src/commands/ReorderPermutationCommand.h | 39 | R |
| src/commands/ReplaceImageCommand.h | 36 | T |
| src/commands/ResizeFormFieldCommand.h | 65 | R |
| src/commands/ResizeImageCommand.h | 33 | R |
| src/commands/RotateImageCommand.h | 40 | R |
| src/commands/RotatePageCommand.h | 43 | T |
| src/commands/SanitizeDocumentHelper.h | 34 | R |
| src/commands/SetMetadataCommand.h | 39 | R |
| src/commands/SignDocumentHelper.h | 23 | R |
| src/core/AnnotationSerializer.cpp | 124 | S |
| src/core/AnnotationSerializer.h | 15 | S |
| src/core/AnnotationTypes.h | 60 | S |
| src/core/AppContext.h | 48 | S |
| src/core/Capability.cpp | 620 | S |
| src/core/Capability.h | 133 | S |
| src/core/CredentialManager.cpp | 122 | T |
| src/core/CredentialManager.h | 21 | S |
| src/core/EncryptedFileSecretStore.cpp | 260 | T |
| src/core/EncryptedFileSecretStore.h | 55 | S |
| src/core/ErrorInfo.cpp | 80 | S |
| src/core/ErrorInfo.h | 94 | S |
| src/core/ISecretStore.h | 51 | S |
| src/core/ImageTypes.h | 17 | S |
| src/core/OcrTypes.h | 46 | S |
| src/core/PageLabels.cpp | 112 | S |
| src/core/PageLabels.h | 75 | S |
| src/core/PdfEnums.h | 55 | S |
| src/core/TempFileManager.cpp | 145 | R |
| src/core/TempFileManager.h | 53 | S |
| src/core/ToolId.cpp | 291 | S |
| src/core/ToolId.h | 158 | S |
| src/core/UpdateChecker.cpp | 436 | R |
| src/core/UpdateChecker.h | 127 | S |
| src/core/interfaces/IConversionEngine.h | 19 | S |
| src/core/interfaces/IFormManager.h | 129 | S |
| src/core/interfaces/IOcrEngine.h | 19 | S |
| src/core/interfaces/IPdfDocument.h | 15 | S |
| src/core/interfaces/IPdfEditorEngine.h | 289 | S |
| src/core/interfaces/IPdfRenderer.h | 22 | R |
| src/core/interfaces/IPdfSearcher.h | 12 | S |
| src/core/interfaces/IPdfWriter.h | 11 | S |
| src/core/interfaces/ISignatureManager.h | 109 | S |
| src/core/interfaces/IToolController.h | 26 | S |
| src/docmodel/Block.cpp | 23 | R |
| src/docmodel/Block.h | 49 | R |
| src/docmodel/CMakeLists.txt | 27 | R |
| src/docmodel/DocumentFuzzer.cpp | 97 | R |
| src/docmodel/DocumentFuzzer.h | 14 | R |
| src/docmodel/Inline.cpp | 21 | R |
| src/docmodel/Inline.h | 47 | R |
| src/docmodel/ProvenanceTag.h | 42 | R |
| src/docmodel/SemanticDocument.cpp | 19 | R |
| src/docmodel/SemanticDocument.h | 35 | R |
| src/engines/AutosaveManager.cpp | 150 | T |
| src/engines/AutosaveManager.h | 42 | R |
| src/engines/BackendRouter.cpp | 33 | R |
| src/engines/BackendRouter.h | 34 | S |
| src/engines/ConversionManager.cpp | 1243 | S |
| src/engines/ConversionManager.h | 89 | S |
| src/engines/DiffEngine.cpp | 264 | S |
| src/engines/DiffEngine.h | 56 | S |
| src/engines/DocumentSession.cpp | 72 | R |
| src/engines/DocumentSession.h | 35 | S |
| src/engines/FormManager.cpp | 1516 | T |
| src/engines/FormManager.h | 76 | S |
| src/engines/MyersDiff.cpp | 197 | S |
| src/engines/MyersDiff.h | 43 | S |
| src/engines/OcrEngine.cpp | 354 | S |
| src/engines/OcrEngine.h | 31 | S |
| src/engines/PatternRedactor.cpp | 382 | S |
| src/engines/PatternRedactor.h | 73 | S |
| src/engines/PdfEditorEngine.cpp | 1766 | T |
| src/engines/PdfEditorEngine.h | 117 | S |
| src/engines/RedactOperation.cpp | 524 | S |
| src/engines/RedactOperation.h | 146 | S |
| src/engines/RenderCache.cpp | 462 | T |
| src/engines/RenderCache.h | 175 | R |
| src/engines/SafeSave.cpp | 104 | R |
| src/engines/SafeSave.h | 46 | S |
| src/engines/SignatureManager.cpp | 2498 | T |
| src/engines/SignatureManager.h | 123 | S |
| src/engines/VeraPdfValidator.cpp | 165 | R |
| src/engines/VeraPdfValidator.h | 47 | S |
| src/engines/ai/IAiProvider.h | 55 | S |
| src/engines/ai/OllamaProvider.cpp | 344 | S |
| src/engines/ai/OllamaProvider.h | 21 | S |
| src/engines/conversion/DjvuImporter.cpp | 206 | R |
| src/engines/conversion/DjvuImporter.h | 69 | S |
| src/engines/mrc/MrcPageProcessor.cpp | 620 | S |
| src/engines/mrc/MrcPageProcessor.h | 144 | S |
| src/engines/ocr/ILayoutDetector.h | 68 | S |
| src/engines/ocr/LayoutEnsemble.cpp | 243 | S |
| src/engines/ocr/LayoutEnsemble.h | 66 | S |
| src/engines/ocr/OcrDjotMapper.cpp | 425 | S |
| src/engines/ocr/OcrDjotMapper.h | 68 | S |
| src/engines/ocr/OcrPipeline.cpp | 415 | T |
| src/engines/ocr/OcrPipeline.h | 110 | S |
| src/engines/ocr/OcrPreprocessor.cpp | 397 | S |
| src/engines/ocr/OcrPreprocessor.h | 55 | S |
| src/engines/ocr/PpDocLayoutDetector.cpp | 366 | S |
| src/engines/ocr/PpDocLayoutDetector.h | 47 | S |
| src/engines/ocr/PpOcrDecoder.cpp | 582 | S |
| src/engines/ocr/PpOcrDecoder.h | 112 | S |
| src/engines/ocr/RapidOcrEngine.cpp | 163 | S |
| src/engines/ocr/RapidOcrEngine.h | 33 | S |
| src/engines/ocr/SuryaDetector.cpp | 30 | S |
| src/engines/ocr/SuryaDetector.h | 47 | S |
| src/engines/pdfium/PdfiumBackend.cpp | 423 | S |
| src/engines/pdfium/PdfiumBackend.h | 72 | R |
| src/engines/pdfium/PdfiumEnvironment.cpp | 27 | R |
| src/engines/pdfium/PdfiumEnvironment.h | 18 | S |
| src/engines/podofo/GlyphAdvanceCalculator.cpp | 80 | S |
| src/engines/podofo/GlyphAdvanceCalculator.h | 36 | S |
| src/engines/podofo/PdfEncryptPubSec.cpp | 318 | S |
| src/engines/podofo/PdfEncryptPubSec.h | 37 | S |
| src/engines/podofo/PdfPageOps.cpp | 132 | S |
| src/engines/podofo/PdfPageOps.h | 39 | S |
| src/engines/podofo/PdfStringEscape.cpp | 93 | S |
| src/engines/podofo/PdfStringEscape.h | 14 | S |
| src/engines/podofo/PoDoFoBackend.cpp | 4179 | T |
| src/engines/podofo/PoDoFoBackend.h | 106 | S |
| src/engines/qpdf/QpdfBackend.cpp | 107 | R |
| src/engines/qpdf/QpdfBackend.h | 13 | S |
| src/engines/scheduling/ILaneScheduler.h | 69 | S |
| src/engines/scheduling/LaneScheduler.cpp | 107 | R |
| src/engines/scheduling/LaneScheduler.h | 215 | T |
| src/engines/scheduling/PipelineStage.h | 132 | S |
| src/pdfws_djot/CMakeLists.txt | 29 | S |
| src/pdfws_djot/DJOT_SPEC_VERSION | 8 | S |
| src/pdfws_djot/DjotToRichTextXhtml.cpp | 327 | S |
| src/pdfws_djot/DjotToRichTextXhtml.h | 51 | S |
| src/pdfws_djot/IDjotCodec.h | 23 | S |
| src/pdfws_djot/IDjotMapper.h | 46 | S |
| src/pdfws_djot/LuaDjotCodec.cpp | 686 | S |
| src/pdfws_djot/LuaDjotCodec.h | 23 | S |
| src/pdfws_djot/PdfStructureMapper.cpp | 242 | R |
| src/pdfws_djot/PdfStructureMapper.h | 21 | R |
| src/pdfws_djot/ProvenanceGuard.cpp | 64 | R |
| src/pdfws_djot/ProvenanceGuard.h | 70 | R |
