# AUDIT VERIFICATION — 2026-09-25 (Wave 1, gsd-verifier)

**Status:** COMPLETE (Wave 1 — coverage per Appendix A; residuals named for Wave 2)
**Scope:** entire `src/` tree (379 files, ~98.9k LOC) on `audit/sweep-all` @ `7eb5c67b`
**Method:** goal-backward verification adapted to correctness auditing — for every module, verify
(1) contract correctness (preconditions/postconditions/invariants), (2) edge cases, (3) error-path
completeness, (4) resource management, (5) API consistency, (6) documentation accuracy,
(7) type safety, (8) algorithmic correctness, (9) state management, (10) thread safety.
Evidence is `file:line` from a full read or a targeted grep sweep; every finding has a severity.

**Severity scale:** CRITICAL (data loss / security / crash on reachable input) ·
HIGH (wrong results, reachable edge case) · MEDIUM (contract violation, latent) ·
LOW (consistency, documentation) · INFO (observation).

**Rules honored:** AUDIT-ONLY. No production code changed. Findings committed on `audit/sweep-all`.

---

## Findings

### V-01 · MEDIUM · scheduling — a submit racing `shutdown()` can orphan a GPU task forever
`src/engines/scheduling/LaneScheduler.h:180-208`, `LaneScheduler.cpp:41-75`
`LaneScheduler::submit` (GPU lane) acquires the semaphore, then `enqueueGpu` pushes to
`m_gpuQueue` **without checking `m_gpuStopping`**. `gpuWorkerLoop` exits as soon as
`m_gpuStopping && m_gpuQueue.empty()`; `shutdown()` quits the thread after wakeAll. A
submit whose `tryAcquire` succeeds while the drain window closes (or that was blocked in
`m_gpuSemaphore.acquire()` and proceeds after the drain) can push a task into a queue no
thread will ever read: the `QPromise` never finishes (callers block on `result()`), the
in-flight counter never decrements, and the semaphore slot leaks. Probability is low
(shutdown-lifecycle race) but the failure mode is a hang. `enqueueGpu` (or `submit`) should
fail the promise with `Cancelled` when `m_gpuStopping` is set, under the same mutex the
loop drains under.

### V-02 · MEDIUM · scheduling — worker exceptions outside `std::exception` terminate the process
`src/engines/scheduling/LaneScheduler.h:162-170`
`runWork` catches `const std::exception&` only. Any other escaping throwable from `work()`
reaches the QThreadPool/GPU thread uncaught → `std::terminate()`; for the GPU lane the
semaphore is also never released. `catch (...)` with a `WorkerCrashed` result (and the
in-flight/semaphore release on that path) is missing.

### V-03 · MEDIUM · signing-request — commit-failure + field-created advances the prepared identity to a hash that was never on disk
`src/shell/controllers/SendForSigningController.cpp:296-313`,
`src/core/SigningRequestRunner.cpp:272,361,377-385`
When the lazy placement created the field AND the engine signed successfully but the final
`SafeSave::commitFileToDestination` failed, `FillStepResult::documentSha256` holds the
**candidate's** hash (runner line 361), while `docPath` on disk still holds the post-create
bytes. The controller's failure path (`r.fieldCreated && !r.documentSha256.isEmpty()`)
then writes that candidate hash into `model.preparedSha256` (controller line 303). The next
step's mutation gate compares `sha256OfFile(docPath)` (= post-create bytes) against the
recorded candidate hash → guaranteed `DocumentChanged` refusal for a mutation the workflow
itself owns; the user must walk a confusing re-confirm dialog to recover. The
creation-advanced hash should be carried separately (as `stepDiskIdentityHex` already is)
from the candidate hash.

### V-04 · LOW · updates — the installer download has no transfer timeout and no byte cap
`src/core/UpdateChecker.cpp:272-283`
The manifest request sets `setTransferTimeout(20000)` and a 1 MiB body cap (S4-1, three
layers). The download request sets neither: a stalled proxy/TLS connection leaves
`m_downloading == true` forever (retry permanently refused), and a hostile-but-HTTPS
manifest host can stream unbounded bytes into `reply->readAll()` RAM at
`onDownloadFinished` (line 316) before the SHA check runs. The S4-1 rationale applies
verbatim to the larger body.

### V-05 · LOW · updates — version comparison silently mangles pre-release tags
`src/core/UpdateChecker.cpp:87-99`
`isNewerVersion` splits on `.` and `toInt()`s each part; a semver pre-release suffix
("1.6.0-beta" → part "0-beta" → 0) compares as plain 0. Mostly conservative, but
"1.6.0-beta" > "1.6.0" evaluates false while "1.6.1-x" vs "1.6.0" still works; any channel
manifest using suffixed versions gets wrong no-update/update verdicts with no diagnostics.

### V-06 · INFO · updates — duplicated S4-1 comment block
`src/core/UpdateChecker.cpp:165-172` — the same five-line comment appears twice back to
back (merge artifact); the code beneath is single.

### V-07 · LOW · ocr — `ResultIterator::BoundingBox` return value unchecked
`src/engines/OcrEngine.cpp:349-352` (and the duplicated block at ~395)
`ri->BoundingBox(level, &x1, &y1, &x2, &y2)` returns `bool`; on `false` the four locals
stay uninitialized and are stored into `OcrResult::boundingBox` (garbage geometry exported
into the djot/overlay paths). Tesseract practically always succeeds for a valid iterator,
but the contract is unchecked.

### V-08 · INFO · core — `precheck` signed-overflow nit
`src/core/SigningRequestRunner.cpp:64-67` — `in.signerIndex + 1` in the `BadIndex` message
is evaluated when `signerIndex` can be `INT_MIN` (signed overflow UB before `.arg`). Wrap
via `qint64` or print the raw value.

### V-09 · INFO · core — `TempFileManager` fails closed but leaks the created session dir on marker-write failure
`src/core/TempFileManager.cpp:303-309` — if the ownership marker cannot be written, the
already-created `GlyphPDF-XXXXXX` session directory is abandoned unowned (deliberately
never touched by later cleanups — WP-R10), so it accumulates. Documented policy tension;
fail-closed is the right call, the residue is noted.

### V-10 · VERIFIED-CLEAN · selected deep reads
The following were read line-by-line and their contracts verified correct (math
hand-checked, edge guards present, error paths honest, RAII complete):
- `src/core/PageSpaceTransform.h` + `src/core/ItemSpaceTransform.h` — all four /Rotate
  cases of viewerToUser/userToViewer hand-derived; round-trip identity exact; rotation
  normalized; documented corner-exactness holds.
- `src/core/PageLabels.cpp/.h` — roman/letter math verified (27→"aa" repeated-letter cycle
  per ISO 32000 Table 159, roman >3999 honestly blank); invalid-input guards; the path
  overload's load→mutate→candidate→re-read-validate→atomic-commit transaction leaves the
  destination byte-identical on every failure path (G13).
- `src/core/VersionedJson.cpp` — QSaveFile all-or-nothing; all three failure branches
  reported via `err`.
- `src/core/TempFileManager.cpp` — liveness + 24 h lease double-gate; foreign/unowned
  content never touched; PID-reuse fails safe (leaves data); lock ordering
  `initMutex → m_mutex` with no inverse path.
- `src/core/EncryptedFileSecretStore.cpp` — version-gated blob acceptance (PGR-20 forgery
  rejection), AAD/entropy entry binding (SEP13:5), OPENSSL_cleanse RAII on every early
  return, QLockFile read-modify-write with fresh re-read under the lock (PGR-25, CX-06
  deferred-migration guard).
- `src/core/CredentialManager.cpp` — loud-failure store policy; delete covers all backends.
- `src/core/PolicyController.cpp` — schema-gated bounded allowlist; Windows admin-tier
  ownership gate fail-closed (incl. `LocalFree` on every path); every rejection disclosed.
- `src/core/SigningRequestRunner.cpp` — precheck mutation/binding/foreign-field gates are
  fail-closed and read-only; lazy placement + candidate commit with destination-identity
  check (E-6); candidate dropped on every path (5c8fd08 discipline). (See V-03 for the
  one hash-attribution gap at the controller layer.)
- `src/core/Capability.cpp` — erase-while-iterating uses the correct
  `it = m_cache.erase(it)` idiom; prefix key cannot alias another id (`"<id>\n"` includes
  the separator).
- `src/engines/RenderCache.h` — AR-6 D2 hash/equality invariant holds by construction
  (single quantization point; only quantized fields in `operator==` and `qHashMulti`).
- `src/engines/scheduling/LaneScheduler.{h,cpp}` — semaphore released on both cancel and
  complete paths; shutdown drains the queue; AR-6 D5 GPU-capacity refusal is loud.
  (See V-01/V-02 for the two lifecycle gaps.)
- `src/engines/podofo/PdfEncryptPubSec.cpp` — `m_fek` is exactly 32 bytes by construction
  (`assign(fek, fek+32)`); both `memcpy(encryptionKey, …, 32)` sinks are in-bounds.
- `src/engines/SignatureManager.cpp:483-560` — every DSS/VRI `memcpy` copies
  `QByteArray` → `charbuff` at identical size (no overflow; empty-input memcpy is the
  benign size-0 idiom).
- `src/engines/ConversionManager.cpp:755-775` — `addZipFile` malloc/ownership contract
  correct on all three branches (libzip owns on success; `zip_source_free` frees the
  buffer on `zip_file_add` failure).
- `src/engines/OcrEngine.cpp` — OCR row `memcpy` bounds correct (`wpl*4 ≥ width` for
  8 bpp); GUI-thread re-entry refused loudly; 10k-pixel guard. (See V-07.)
- `src/engines/FormManager.cpp` + `PdfEditorEngine.cpp:125` — every `dynamic_cast` result
  null-checked; the D-09 borrow-then-`release()` ownership transfer is leak-safe.
- `src/commands/CheckedHistory.h` — checked undo/redo performs the real work BEFORE the
  index moves; arm/consume flags make the follow-up `QUndoStack::undo()/redo()` a pure
  index move; legacy commands keep plain Qt semantics via `dynamic_cast` fallback.

---

### V-11 · LOW · ui — `saveAnnotations` fire-and-forget writer races application shutdown
`src/ui/PdfViewerWidget.cpp:763-775`
The `.ann` envelope write is dispatched to a detached `QThread::create` worker with no
completion tracking; if the application quits while the thread runs, the QThread object can
be destroyed before `finished` is delivered ("QThread: Destroyed while thread is still
running" — undefined behavior window for the file write). The write is also unchecked: a
failed `open()` silently drops the annotation sidecar (the G14 contract says the envelope
is the annotation state of record for non-embedded annotations).

### V-12 · INFO · engines — OcrPipeline lambdas capture raw `LayoutEnsemble*`/engine pointers
`src/engines/ocr/OcrPipeline.cpp:240-283`
`[=]` captures of `layoutEns`, `primaryEngine`, `secondaryEngine` (raw pointers into
pipeline state) are correct only while the pipeline object outlives the returned
`QFuture`. Every in-tree caller awaits before tearing down, but the contract is implicit —
a caller that drops the pipeline keeps a future whose worker dereferences freed pointers.
Deserves an ownership comment or a shared_ptr capture.

### V-13 · INFO · type safety — the >2 GiB buffer class narrows through `int`
`src/engines/PdfEditorEngine.cpp:1186,1237`, `src/core/PoDoFoBackend.cpp:303,1162,1274,
3063,4116,6521,6646`, `src/core/RedactionProof.cpp:443,450,462` (representative set of ~30
`static_cast<int>(…size())` hits)
Whole-file/stream buffers (`QByteArray(char*, int)`, `SetData(int)`, OCR staging) truncate
above `INT_MAX`. No reachable in-tree path produces a >2 GiB single stream today (the OCR
matrix guard caps at 10k×10k; preset/import paths enforce size caps), and PoDoFo itself is
int-bounded — this is a documented class boundary, not a live defect. The mutation
baseline at `PoDoFoBackend.cpp:303` also holds the whole file in RAM.

### V-14 · LOW · ui — `MainWindow::confirmSaveBeforeInPlaceWrite` blocking hop has an unguarded receiver-lifetime window
`src/GpMainWindow.cpp:1801-1812`
The worker blocks on `Qt::BlockingQueuedConnection` with a raw `this`. The comment's
liveness argument (the GUI thread is back in its event loop) holds for QtConcurrent
runners, but if `MainWindow` is destroyed between the worker's thread check and the
metacall delivery, the blocked worker is never released (Qt discards pending events for a
dead receiver). Shutdown-order dependent; a `QPointer` guard + `abort` path or
`QMetaObject::invokeMethod` with a context object lifetime check would close it.

### V-15 · INFO · shell — worker mutation of controller members relies on a documented, un-enforced invariant
`src/shell/controllers/EditController.cpp:1034-1058`
The OCR worker initializes `self->_ocrRapid` / `_ocrEnsemble` (non-atomic members) from the
worker thread; the race-freedom argument is the `_ocrRunning` flag (comment in place).
The invariant is real but enforced only by convention — no assert documents it. INFO:
works today; fragile under refactoring.

### V-16 · VERIFIED-CLEAN · second-wave deep reads and sweeps
- `src/core/FormStaleFieldTracker.cpp` — mutex-disciplined; empty-map pruning on every
  mutation; `acknowledge` erases the document entry when drained.
- `src/core/SigningRequestModel.cpp` — fail-closed parse: strict magic **value** handshake
  (SWEEP-W1 FZ-3), schema gate, per-signer shape validation, sentinel-rejecting
  `rectFromJson`, the W1-02 alias lint (trimmed comparison) at the untrusted boundary.
- `src/core/RedactionProof.cpp` — attachment sweep is honest-by-construction: depth-capped
  recursion (`kMaxAttachmentDepth`), compressed-container magics (incl. the OLE CFBF magic,
  PROGRAM-CONSOLIDATION §1.2) → Unswept never Clean, PDF-declared-but-unparseable → dark,
  raw-only streams disclose "content not certified", scratch-file staging removed on every
  path, verdict aggregation fails closed (any Unswept ⇒ FAIL, `RedactionProof.cpp:1560`).
- `src/core/BatchPreset.cpp` — `exportTo` (§1.6 fix verified): candidate staging, cleanup
  on all four failure branches, only the commit touches the destination; import refuses
  silently re-keyed copies; store root owned at every boundary (F2b-D1).
- `src/engines/ConversionManager.cpp:497-510` — `csvFormulaSafeCell` single contract
  (OWASP apostrophe prefix, narrow plain-number exemption); verified routed from ALL sinks:
  `MeasureMode.cpp:43`, `BatchMode.cpp:3522`, `ErrorInfo.cpp:51`, `CommentsWidget.cpp:829`
  (PROGRAM-CONSOLIDATION §1.1 fix confirmed wired, not just claimed).
- `src/engines/podofo/PoDoFoBackend.cpp:790-794` vs `:3565` — §1.3 fix confirmed: property
  edits sync XMP **without** packet reset (custom entities preserved); PDF/A export keeps
  its deliberate reset.
- Worker-thread census: 25 `QThread::create` sites, all with `finished → worker
  deleteLater` (per-file counts 10/10, 5/5, 1/1, …); engines captured via weak_ptr/
  shared_ptr (`SecurityController`, `CertEncryptController`, `SendForSigningController`);
  the encryption worker wraps its body in `try/catch(...)` (`SecurityController.cpp:537-546`).
- `src/app/main.cpp` — guarded `files.first()`; splash deleted via `deleteLater`; tray
  parented to the window; lambda receiver contexts correct.
- `src/core/PolicyController.cpp` — see V-10 note: admin-tier ownership gate fail-closed.
- pdfws_djot — no raw indexing found (Qt container `at()/value()` discipline).

---

## Appendix A — Verification coverage log

| Area | Files | Method | Status |
|---|---|---|---|
| core geometry/labels/JSON/temp/secrets/policy/signing-request/proof/presets | PageSpaceTransform.h, ItemSpaceTransform.h, PageLabels.*, VersionedJson.cpp, TempFileManager.cpp, EncryptedFileSecretStore.cpp, CredentialManager.cpp, PolicyController.cpp, SigningRequestRunner.cpp, SigningRequestModel.cpp, FormStaleFieldTracker.cpp, RedactionProof.cpp (key paths), BatchPreset.cpp (store), Capability.cpp | full line-by-line read + math hand-check | VERIFIED (V-03, V-08, V-09 findings) |
| scheduling | LaneScheduler.{h,cpp}, RenderCache.h | full read | VERIFIED (V-01, V-02 findings; hash invariant clean) |
| engines: OCR/conversion/signing crypto/backends | OcrEngine.cpp (recognize/getRawText), ConversionManager.cpp (addZipFile, csvFormulaSafeCell), SignatureManager.cpp (DSS/VRI memcpy), PdfEncryptPubSec.cpp, PdfEditorEngine.cpp (D-09), PoDoFoBackend.cpp (save/sync boundaries) | targeted reads + bounds hand-check | VERIFIED (V-07, V-12, V-13 findings) |
| commands/undo | CheckedHistory.h | full read | VERIFIED |
| shell/ui workers | GpMainWindow.cpp (closeEvent, confirmSave, policy wiring), SendForSigningController.cpp (full), SecurityController.cpp (worker sites), ConvertController.cpp (worker sites), EditController.cpp (OCR worker), PdfViewerWidget.cpp (saveAnnotations), main.cpp, PdfViewerWidget lifetime sweep | targeted reads + census greps | VERIFIED (V-04..V-06, V-11, V-14, V-15 findings) |
| whole-tree pattern sweeps | 379 files | narrowing casts, erase-while-iterating, unchecked first/last, raw C functions, dynamic_cast discipline, DirectConnection/BlockingQueued, mutable members, parentless widgets, QThread::create cleanup | SWEPT (V-13 class finding; idiom otherwise correct) |
| NOT fully read this wave | PoDoFoBackend.cpp (6756 lines — read at save/sync/redact/metadata boundaries only), SignatureManager.cpp (3153 — LTV section), AccessibilityTagger.cpp (2483), BatchMode.cpp (3576 — CSV sink + worker sites), FormManager.cpp (2016 — validation/commit regions), PdfViewerWidget paint math, ui/AnnotationLayer, modes/CompareMode, mrc/, ai/, formjs/ internals | residual for Wave 2 | RESIDUAL |

## Summary

| Severity | Count |
|---|---|
| CRITICAL | 0 |
| HIGH | 0 |
| MEDIUM | 3 (V-01, V-02, V-03) |
| LOW | 6 (V-04, V-05, V-07, V-11, V-14, V-15* informational-low) |
| INFO | 5 (V-06, V-08, V-09, V-12, V-13) |
| **Total findings** | **14** (+2 verified-clean registers V-10/V-16) |

**Top items (by user impact):**
1. **V-03** — signing-request commit-failure records a candidate hash as the prepared
   identity → guaranteed false `DocumentChanged` refusal on retry (confusing, recoverable).
2. **V-01** — LaneScheduler shutdown race can hang a submitter and leak the GPU slot.
3. **V-02** — non-`std::exception` escape in a scheduler worker terminates the process.
4. **V-04** — update-installer download: no transfer timeout (stuck `m_downloading`) and
   no byte cap (unbounded RAM) — the exact S4-1 hole closed for the manifest only.
5. **V-11** — annotation sidecar write is fire-and-forget and unchecked (silent loss on
   write failure; shutdown race).
6. **V-14** — blocking GUI hop unguarded against receiver destruction (shutdown deadlock
   window).
7. **V-05** — semver pre-release tags mis-compare in the update gate.
8. **V-07** — unchecked Tesseract `BoundingBox` can export garbage geometry.
9. **V-13** — the >2 GiB `int`-narrowing class (documented boundary, no live trigger).
10. **V-15** — worker mutation of controller members guarded by an un-enforced invariant.

**Residuals for a Wave 2:** PoDoFoBackend.cpp, SignatureManager.cpp (LTV internals),
AccessibilityTagger.cpp, BatchMode.cpp, FormManager.cpp, ui paint math (PdfViewerWidget,
AnnotationLayer), CompareMode, mrc/, ai/, formjs/ internals — named above with the method
that should cover them.

**Audit-only compliance:** no production file touched; commits contain only
`docs/audit/AUDIT-VERIFICATION-2026-09-25.md`. The pre-existing unstaged modification to
`tests/TestSignatureRealCrypto.cpp` (117 deletions, not ours) was left untouched.
