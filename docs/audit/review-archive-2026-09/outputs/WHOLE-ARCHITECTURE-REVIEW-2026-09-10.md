# GlyphPDF architecture review — 2026-09-10 snapshots

Finalized 2026-09-11 against the preserved September 10 snapshots.

## Decision and scope

The existing architecture can support a maintained desktop PDF editor without a rewrite. It is not yet ready for unrestricted corporate document editing: important failure paths can lose accepted resident edits, misrepresent history, replace a newer external file, or destroy an existing export. A controlled internal preview using disposable copies is a more defensible release posture until the acceptance gates below pass.

This is an independent **source review**, not a runtime reproduction or certification. The initial immutable snapshot was `4e2c99ae345eab8b93bbe5899df12b2a6d39d208`; findings were reconciled against the newly preserved published snapshot **`9ba3cea499f67060604ee09903f63ea1a79c30fd`**. Links below point to that published snapshot. SHA-256 comparisons confirmed the editor, PoDoFo backend, session, autosave, SafeSave, temporary-file manager, scheduler, and the two command headers discussed here are unchanged between those snapshots. Changed controller call sites were re-read and current line numbers are used. No production file was edited, no moving working tree was reviewed as if it were committed evidence, and no new build/test success is claimed.

Recent ledger rows G01–G20 remain `implemented-awaiting-review` unless independently established elsewhere. The presence of an implementation and regression names is positive evidence, but it is not a new successful run in this review. In particular, this report does not repeat the repaired encrypted same-file truncation, normal-load autosave ABA, recovery destination, crop geometry, or the repaired four-command undo problems as if their fixes were absent. The findings below identify remaining paths and composition gaps.

Severity: **P1** means a credible reachable data-integrity failure requiring a release gate; **P2** means a bounded reliability or operational gap. Every finding below is source-confirmed; predicted filesystem, Qt, or subprocess outcomes still need the named runtime tests.

## Findings

### A01 — P1: a failed user Save can discard earlier accepted resident edits

[PoDoFoBackend.cpp:227](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/podofo/PoDoFoBackend.cpp:227) clears the resident document, buffer and encryption password, then reloads the disk source. `writeUpdate` invokes that rollback on a failed same-file commit at lines 525–561. Its comment assumes the pre-mutation resident equals the source bytes. That assumption is false for this API: `editTextInline`, metadata setters and watermark methods can modify only memory.

The watermark path is a concrete desktop trigger. [WatermarkDialog.cpp:297](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/modes/WatermarkDialog.cpp:297) calls the resident-only watermark method, then marks the session for reload at line 325. The wrapper at [PdfEditorEngine.cpp:1783](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/PdfEditorEngine.cpp:1783) does not save; the backend returns after changing the document at line 4503. The session reload connection refreshes the viewer, not the editing backend (`GpMainWindow.cpp:250–252`). User Save then calls `embedAnnotations` at [HomeController.cpp:237](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/HomeController.cpp:237); that method calls `writeUpdate` at [PoDoFoBackend.cpp:3913](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/podofo/PoDoFoBackend.cpp:3913). A refused commit reloads the old disk document and drops the previously accepted watermark. A later successful Save can therefore save without it. The same mismatch applies when a later path-based edit fails after prior resident work.

This is a **composition gap in G06**, not the old rejected-crop leak: rollback now prevents rejected edits leaking forward, but can also remove earlier valid edits. The comment that a failed plain `saveDocument` retains memory does not describe the shell's `embedAnnotations → writeUpdate` Save route.

**Minimal remedy:** distinguish “save current work” from “commit one new mutation,” and retain a pre-operation resident snapshot for the latter when the resident differs from disk. Reuse the existing candidate/buffer ownership and SafeSave boundary. Restore precisely the state before the attempted mutation; a failed user Save must keep all accepted work retryable.

**Acceptance gate:** real engine + shell Save, containing an unsaved text edit or watermark, with `SafeSave::FailBeforeCommit`. Require unchanged original bytes, retained resident edit after failure, dirty session, then successful retry whose reopened PDF contains the edit. Also require a failed rotate/crop to remove only that attempted mutation while preserving prior resident work.

### A02 — P1: checked history covers only some commands; rotate and inline-text undo still lie on failure

The G08 boundary deliberately falls back to raw `QUndoStack::undo()` for legacy commands in [CheckedHistory.h:85](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/commands/CheckedHistory.h:85). Two reachable commands still use that fallback:

- [RotatePageCommand.h:16](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/commands/RotatePageCommand.h:16) ignores the engine result in redo and undo, then always calls `markReload`. It is pushed by `PagesController.cpp:311`, `318`, `334`, and `362`. After a refused rotate, history can record a change that did not happen; undo after the disk becomes writable can apply the inverse rotation to a page that never received the first rotation. A failed undo can also move the index as though restoration succeeded.
- [EditTextInlineCommand.h:27](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/commands/EditTextInlineCommand.h:27) ignores edit failure. Undo at lines 36–37 calls `insertPageFromBytes` then `deletePage` as separate writes, without checking either result. If insertion fails, deletion can remove the following real page; if insertion succeeds and deletion fails, an extra page remains. This command is pushed by `EditController.cpp:390` and `981`. G08 introduced an atomic `restorePageFromBytes` for other commands, but this sibling does not use it.

The existing G06 rotate test calls the engine directly (`TestEngineSave.cpp:540`); it does not prove command-stack truthfulness. Corrected CropPageCommand, EditFormFieldCommand, DeleteImageCommand and ReplaceImageCommand should remain credited for their narrower G08 implementation.

**Minimal remedy:** use the existing checked restoration and atomic page-restore seam for these command families; check initial mutation results before publishing history/dirty state. Do not add a second history framework. Review other legacy commands using the same pattern, but do not label an unused command as a user-facing defect merely because its code is unsafe.

**Acceptance gate:** inject failure during initial rotate, rotate undo, inline-text snapshot/restore, and redo. Assert actual saved geometry/page count/content together with stack index, clean baseline, retryability, and a truthful error. Specifically prove that a failed insertion never permits deletion of an original adjacent page.

### A03 — P1: creating an encrypted package deletes the previous destination before launching the writer

[HomeController.cpp:447](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/HomeController.cpp:447) unconditionally removes the final archive so 7-Zip starts fresh. The process is launched later at lines 492–505 and writes directly to the final path. The result handler reports errors but does not restore the previous archive.

**Trigger:** Share → Create encrypted package → choose an existing ZIP → process cannot start, process fails, input disappears, disk fills, or the app exits during packaging. The previous ZIP is already gone; the destination may be absent or partial. File-dialog overwrite consent authorizes replacing the completed result, not destruction before a result exists. This is unrelated to G01's PDF encryption-save fix.

**Minimal remedy:** have 7-Zip create a unique owned candidate, validate success/readability, then commit through the existing checked file replacement helper. Keep the previous output until commit; remove only this operation's candidate on failure. Also bound/cancel the process: `waitForFinished(-1)` at line 500 has no timeout or cancellation path.

**Acceptance gate:** simulate failure-to-start, nonzero exit, cancellation, and failed destination commit with a sentinel pre-existing archive. Its SHA-256 must remain unchanged in every failed case; successful output must contain exactly the intended input and pass encrypted archive opening with the chosen password.

### A04 — P1: successful qpdf repair does not advance the resident-load identity

Normal successful loads increment `loadId` at [PdfEditorEngine.cpp:121](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/PdfEditorEngine.cpp:121). The successful repair branch replaces the resident backend and returns true at [PdfEditorEngine.cpp:134–144](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/PdfEditorEngine.cpp:134), without incrementing it.

The G04 identity check in `saveDocumentIfCurrent(path, loadId, output)` therefore cannot distinguish an earlier resident from a repaired replacement at the same path. Autosave captures the ID at [AutosaveManager.cpp:96](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/AutosaveManager.cpp:96); the worker validates it at line 200, and final promotion is gated by `engineMatches` at lines 127–131. The session generation only gates its timestamp there, not promotion. Thus a pending save from A can accept a repaired new A under the old ID and promote its bytes over the old recovery file.

**Minimal remedy:** mint the load identity in one common successful-resident-publication path, including normal load and repair. Do not weaken the existing path-plus-ID checks.

**Acceptance gate:** extend the deterministic G04 worker-barrier test using a real malformed PDF that PoDoFo rejects and qpdf repairs. Same-path repaired reopen must change the ID; an old worker must report stale and leave the prior recovery PDF byte-identical. This review confirmed the missing increment, not a runtime repair-fixture reproduction.

### A05 — P2, corporate collaboration gate: save has no external-version conflict check

`DocumentSession` tracks in-process generation and mutation revision, but stores no version of the source bytes (`DocumentSession.h:90–97`). [HomeController.cpp:237](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/shell/controllers/HomeController.cpp:237) saves the resident document without comparing the destination to the version loaded. [SafeSave.cpp:79](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/SafeSave.cpp:79) checks candidate I/O and atomic replacement, not whether somebody else changed the destination. Source-wide watcher searches found the hot-folder watcher, not a live-document external-change guard.

**Trigger:** open a PDF, have another process or another machine replace it, then Save from the still-open resident. The checked atomic replacement can successfully overwrite the newer external version. G04's resident-load ID solves a different problem: internal stale asynchronous work after reload.

**Minimal remedy:** keep a source version token in the existing session/file-save boundary, compare it immediately before in-place replacement, and require conflict resolution/Save As when it changes. Include file identity and sufficiently strong content/version checks; modification time alone is not a reliable equality proof. Refresh the baseline only after a successful open or committed save. Consider cooperative file locking if stronger multi-process guarantees are required; a watcher alone is advisory.

**Acceptance gate:** two instances and an external replacement while editing; the second save must not silently overwrite the new bytes. Include same-size replacement, preserved timestamps, file rename/replacement, and supported network storage. Race behavior between check and commit must be stated and tested for the supported storage model.

### A06 — P2: startup temporary cleanup can remove another live session's files

[TempFileManager.cpp:101](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/core/TempFileManager.cpp:101) enumerates `GlyphPDF-*` and `glyph_*` in the system temporary directory. Entries older than 24 hours are recursively deleted based on their directory/file modification time at lines 102–111. No process ownership, lock, active-session marker, or tracked-entry check is used. This runs at application startup (`src/app/main.cpp:131`).

**Trigger:** keep one instance open longer than a day with temporary working artifacts, then launch another. An old directory modification time is not proof that the directory is abandoned; its child contents may still be in use. On Linux, deleting open files/directories also does not necessarily fail merely because another process is using them. The deletion rule itself is proven by source; no real user temporary directory was touched to test it.

**Minimal remedy:** create one session directory with an ownership marker and held lock, clean abandoned session directories only after obtaining their lock, and limit deletion to those verified owned directories. Reuse Qt's existing filesystem/locking facilities and per-session directory mechanism; avoid scanning broad filename prefixes as ownership proof.

**Acceptance gate:** isolated temporary root with a live locked session older than 24 hours, an abandoned owned session, an unrelated matching-name entry, and a symlink. Only the abandoned owned session may be removed. Test on Windows and Linux.

### A07 — P2, currently latent: scheduler shutdown is not a bounded safe completion contract

[LaneScheduler.cpp:37](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/scheduling/LaneScheduler.cpp:37) cancels queued work by generation, waits 30 seconds for the GPU thread, then calls `terminate()` and an unbounded wait; the CPU pool uses `waitForDone(-1)` at line 53. [LaneScheduler.h:148](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/whole-project-2026-09-10/published-source/src/engines/scheduling/LaneScheduler.h:148) checks cancellation only before executing the work function. A running function receives no cancellation context. The GPU admission path checks `m_gpuStopping` only if its immediate semaphore acquisition fails, so work submitted after shutdown with spare capacity can be queued to a stopped worker.

**Reachability limitation:** Bootstrapper constructs `ctx.scheduler`, but the production source search found no consumer wiring it into the live `OcrPipeline`/`LayoutEnsemble`. Their optional scheduler APIs and the tests use it. Therefore this is a real reusable-component contract gap, **not evidence that ordinary desktop OCR currently hangs through this scheduler**. The encrypted-package unbounded process wait is separately reachable in A03.

**Minimal remedy:** reject all submissions after shutdown begins; give work a cooperative cancellation/deadline contract; finish every admitted future on cancellation. For potentially uninterruptible native work, use the existing process boundary where available rather than terminating a thread that can hold locks. Wire this scheduler into more production work only after the shutdown contract is tested.

**Acceptance gate:** queued/running work, task submitted concurrently with shutdown, submit after shutdown, cancellation during a long job, and every promise reaching one terminal state. Assert bounded exit without `QThread::terminate()` and no dangling native resources.

## Existing strengths and repaired boundaries to preserve

The project already has useful separation: `AppContext` injects editor/OCR/forms/signature/conversion services; Bootstrapper constructs them; `DocumentSession` carries shared state; controller boundaries route user work. `IPdfEditorEngine` and related interfaces allow targeted command and shell tests. These are sufficient places for the proposed changes.

`SafeSave` is a credible common primitive: unique candidates, bounded copying to QSaveFile, checked commit, a deterministic commit-fault seam, and viewer-handle coordination. The current PoDoFo full-save path validates candidates, checks page count, retains the buffer needed for lazy loading, and applies the same candidate route to encrypted documents. G01's old bypass is absent. Reuse this work for the remaining export/transaction gaps.

The G04 normal-load identity and autosave stale checks, G05 recovery source/destination binding, G07 crop-origin snapshot, and G08 checked restoration are substantive improvements. G14's newly published pending-annotation-embed state and G15's checked Bates re-anchor are also present in the published delta; they must not be confused with A01–A04. CapabilityRegistry already provides a place to explain unavailable features, including optional model/tool readiness. Existing failure-injection, reopen/content, and command-history tests provide the right testing style.

Signed incremental saving is an explicit residual contract, not a solved atomic transaction: `PoDoFoBackend.cpp:572–588` removes/copies a different destination then appends via `SaveUpdate`, while same-file append can partially write before reporting failure. The ledger itself acknowledges the incremental boundary. A corporate release should preserve signed original byte ranges while staging a complete candidate revision before final replacement where feasible; at minimum, characterize failure behavior and ensure the existing destination is not removed before staging succeeds. This review did not run a signed append fault reproduction or audit signature validity semantics.

## Architectural target using the existing design

Keep the present engine/controller/session split. Make four contracts explicit and consistent:

1. **One published document identity:** session path, recovery input, generation, resident load ID, content revision and external source version have named meanings. Every successful load publishes once; asynchronous jobs carry that identity; only a checked committed revision becomes clean.
2. **One transaction rule:** an operation either produces a validated candidate and commits, or leaves both the previous destination and previous accepted resident state intact. Existing SafeSave, PoDoFo ownership, and the atomic restore seam implement most of this already. Treat signed append as an explicit specialized case with its own tested guarantees.
3. **One truthful command result:** history position, dirty state, displayed result and stored artifact agree. Extend CheckedHistory and result checking to remaining reachable legacy commands, including redo behavior, instead of creating a new undo system.
4. **One job lifetime rule:** a job owns its candidate paths, knows its document identity, has cancellation/deadline behavior, and produces exactly one terminal outcome before any dependent service is destroyed. Use the existing scheduler/process abstractions only where they meet that contract.

Do not start by splitting every large source file or replacing Qt/PoDoFo/PDFium. Centralizing invariants at these already-existing boundaries yields more risk reduction than cosmetic file decomposition. Extract duplicated writer/admission logic only when the same tested helper can serve actual callers.

## Linux adaptation implications

The Qt interfaces, CMake targets, portable core libraries and process-based helpers are a workable starting point. A Linux build should not be treated as proof of Linux release readiness. The architecture findings above cross platforms; Linux adds specific correctness checks:

- Centralize file identity. The backend currently uses unconditional `Qt::CaseInsensitive` comparisons for same-file decisions (`PoDoFoBackend.cpp:322`, `527`, `571`, `2917`). On a case-sensitive filesystem, `A.pdf` and `a.pdf` can be different files. In the signed different-destination branch, treating them as equal can skip staging source bytes before append. Use native identity/canonical paths with a policy suitable for the actual filesystem; test symlinks, case-distinct paths and aliases. This is a source-observed portability defect pattern, not a tested Linux failure.
- Replace filename-prefix/age cleanup with locked session ownership before exposing long-running Linux sessions. Test process interruption, open-file deletion behavior, temporary storage permissions, and abandoned candidates in an isolated root.
- Implement and test the native secret store, certificate/trust-store integration, print dialogs, file associations, desktop portals and external tool discovery on the selected Linux target. Report unsupported capabilities through CapabilityRegistry. The package flow currently checks `7z` and then Windows-specific installation paths and wording; availability and failure handling need a Linux path.
- Publish one reproducible Linux build/configuration with the required feature closure and run the same failure/artifact gates there. Include real fonts, non-ASCII paths/text, display scaling, GUI/offscreen distinctions, and an explicit optional OCR/model matrix. Platform details and packaging are separate specialist-review concerns; no distribution support claim is made here.

## Release gates and order

| Order | Gate | Evidence required |
|---|---|---|
| 1 | Preserve accepted work on failed saves/mutations; complete rotate/text command history | Real-engine failure injection and successful retry, reopened artifacts, stable source hashes, truthful stack/session state (A01/A02) |
| 2 | Finish safe output coverage and repaired-load identity | Existing export preserved on every failure; actual repaired-PDF ABA fixture; signed-output failure characterization (A03/A04 and signed residual) |
| 3 | Corporate document concurrency and session ownership | Multi-instance/external-writer conflicts; live/stale temp session tests; crash/restart recovery against the final artifact (A05/A06) |
| 4 | Job lifetime and Linux acceptance | Reachable process cancellation; bounded scheduler contract before production adoption; platform identity and packaging tests (A07/Linux) |
| 5 | Independent release evidence | Fixed commit/build identity, exact enabled capabilities, no stale ledger claims, test logs and skips, packaged-binary artifact checks, and a small manual user workflow matrix |

A broad passing unit suite is valuable but cannot replace these failing-operation and final-artifact checks. Resolve the transaction/identity contracts before investing in broad structural refactoring or asserting corporate readiness.