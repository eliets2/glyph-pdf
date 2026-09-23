# SWEEP-BACKEND — Internal API Surface Audit (backend-specialist doctrine, applied honestly)

**Date:** 2026-09-22 (audit dated 2026-09-21 per sweep plan)
**Branch:** `feat/sweep-backend` (created from `feat/parity-glm`)
**Base commit:** `26c9a415` — "Merge branch 'feat/modularity-moves' into feat/parity-glm".
NOTE: the sweep brief said "mainline tip ec9f16f6" — that value was stale; `git merge-base
--is-ancestor ec9f16f6 26c9a415` verified ec9f16f6 IS an ancestor of the current
feat/parity-glm tip, so the branch was cut from the live tip.
**Auditor:** backend-specialist (role file written for Node.js/Supabase/REST; transportable
doctrine applied to the C++17/Qt6 desktop stack — mapping in Appendix A).
**Mode:** AUDIT-ONLY. Zero `src/`/`tests/` edits. Every hardening item below is PLAN-ONLY.
**Worktree note:** the brief's worktree path (`...\Projects\pdf-clean-notess`) is the
junction to this repo's BUILD directory (`D:\pdf\pdf-clean-notess`, CMakeCache
`PdfWorkstation_SOURCE_DIR:...pdf-clean`); the audit ran in the real worktree
`C:\Users\User\Projects\pdf-clean`. Recorded because doctrine honesty starts with the facts.

---

## 0. Executive summary

The audit tested five surfaces — the desktop analogues of a backend API review: engine
seams as API contracts, the command dispatch surface, secret persistence, DoS/rate-size
limits, and security-event logging.

**Verdict per surface:**

| Surface | Verdict | Headline |
|---|---|---|
| 1. Interface seams | GOOD with gaps | Bounds checks explicit on some mutators, exception-reliant on others; options-struct ranges (opacity/jpegQuality/digitCount) documented but unenforced; unclamped opacity reaches PDF ExtGState |
| 2. Command dispatch | **2 HIGH + 1 MEDIUM** | ARC07 single gate holds for ribbon/menu/keyboard, but two in-place mutation slots bypass it entirely, and one direct `activate()` call skips the registry |
| 3. Secrets/persistence | **STRONG** | v3 DPAPI/GCM entry-identity binding, never-silent-fail, atomic+verified writes, M1 clear-on-release verified; 3 minor hygiene items |
| 4. Rate/size limits | STRONG with 1 gap | Ollama (SECFIX-5/SEP13:6/R04) and update-manifest (B-03) hardening verified in code; manifest body itself is read uncapped; no batch-list count cap |
| 5. Security logging | **DOCTRINE GAP** | No durable structured security-event sink (1 `qInfo` in all of src); refusals are scattered qWarnings; one refusal logs a full URL including its user-info |

The fleet's repeated claims that survived adversarial re-verification: the EC02/G04
document-identity guards, the ARC07 gate *for registry routes*, the undo-stack clear on
every identity change, the SEP13:5 v3 secret-store format, SECFIX-5 egress caps, and the
B-03 update-manifest refusal path. The claims that did NOT fully hold: EditPolicy's
commented coverage of "PagesMode reorder" (one of three reorder routes is ungated), and
"mutation dispatch crosses the single gate" (three bypass routes found).

---

## 1. Surface 1 — Interface seams as API contracts (`src/core/interfaces/*.h`)

Ten seam headers audited: IPdfEditorEngine (+ its 9 AR-10 role interfaces incl. IEncryptor),
IFormManager, ISignatureManager, IConversionEngine, IOcrEngine, IPdfDocument, IPdfRenderer,
IPdfSearcher, IPdfWriter, IToolController.

**What is already right (verified):**
- Role decomposition (AR-10 D1) keeps callers on narrow seams; aggregate `IPdfEditorEngine`
  is copy/assign-deleted (`IPdfEditorEngine.h:441-442`) — capability objects are
  non-copyable identities, a real capability-contract property.
- Documented out-of-range contracts where they exist are honest:
  `IPdfRenderer::pageSize`/`extractText` document exact fallback values
  (`IPdfRenderer.h:15-20`); `IOutlineEditor::replaceOutline` documents out-of-range
  refusal (`IPdfEditorEngine.h:379-380`); `reorderAllPages` documents permutation
  validation (`IPdfEditorEngine.h:288-290`).
- Const-correctness tracks mutability honestly (reads that need engine state are non-const
  and say why; identity/quality probes like `documentLoadId()`, `lastError()` are const).
- Deferred-writer identity guards (EC02/G04) are documented *at the seam*
  (`IPdfEditorEngine.h:158-181`) with the attack (A→B→A stale save) spelled out.

**Findings:**

**S1-1 (MEDIUM) — `IPageEditor` page-index contract is enforced inconsistently; some
mutators rely on the third-party library throwing.**
- Evidence: `PoDoFoBackend::deletePage` (`src/engines/podofo/PoDoFoBackend.cpp:1213`) and
  `rotatePage` (`:1148`) check `pageIndex < 0 || >= GetCount()` explicitly and fail
  closed. `PoDoFoBackend::insertBlankPage` (`:1164`) passes `atIndex` straight to
  `CreatePageAt(atIndex, ...)` — correctness depends on PoDoFo validating internally
  inside a `catch (...)` (`:1177`). A release-build assert or unchecked index in the
  library is a UB path, i.e. exactly the "delegated to caller/library" pattern the
  doctrine forbids at a trust boundary.
- Plan (plan-only): one sweep adding the same explicit two-line bounds guard to every
  `IPageEditor`/`IImageEditor` index-taking implementation (insertBlankPage,
  insertPageFromBytes, restorePageFromBytes, reorderPages, listImages, moveImage,
  resizeImage, rotateImage, deleteImage, cropPage, resizePage), plus a seam comment in
  `IPdfEditorEngine.h` stating the contract: "every index argument is validated by the
  implementation; out-of-range returns false and records lastError — never UB."

**S1-2 (MEDIUM) — Options-struct value ranges are documented but not enforced anywhere;
unclamped `opacity` reaches the PDF.**
- Evidence: `TextWatermarkOptions::opacity` says "0.0–1.0" in a comment
  (`IPdfEditorEngine.h:68`); `OptimizeOptions::jpegQuality` says "0–100" (`:93`);
  `BatesNumberingOptions::digitCount` has no range at all (`:50`). `PoDoFoBackend` writes
  `options.opacity` directly into ExtGState `/ca` `/CA` (`PoDoFoBackend.cpp:5142-5143,
  5287-5288`). A caller (batch preset string param, future scripting seam) passing 7.0
  writes a spec-invalid ExtGState into the document. This is the Zod-schema gap: the
  type documents the range; nothing *checks* it at the boundary.
- Plan (plan-only): clamp-or-refuse at the engine entry points that consume the structs
  (`addTextWatermark`, `addImageWatermark`, `optimizeDocument`,
  `applyBatesNumbering`): `qBound` for opacity (with an ErrorInfo::Warning when a value
  was actually clamped — honest, not silent), refuse jpegQuality outside 0–100, clamp
  digitCount to 1–12. Document the enforced ranges in the header comments.

**S1-3 (LOW) — Error-shape ambiguity: "empty means failure" contracts.**
- Evidence: `extractEmbeddedFile` "Returns an empty array if absent or on error"
  (`IPdfEditorEngine.h:203`); `extractPageAsBytes` returns `{}` on failure
  (`:233`). A legitimate zero-byte embedded file is indistinguishable from absence;
  callers cannot distinguish "empty" from "failed" except via `lastError()` which is
  cleared at the *next* call — a classic last-error race across seams.
- Plan (plan-only): for new seams prefer `std::optional<QByteArray>` / out-param `bool*`
  (the seam already uses this pattern well in `pageCropBox(..., bool *ok)`,
  `applyBatesNumbering(..., int *lastNumberOut)`); leave the legacy signatures but
  document the `lastError()`-only failure channel at each site.

**S1-4 (LOW) — Raw-path seams without ownership statement.**
- Evidence: ~30 seam methods take `const QString &path` for BOTH the target and the
  output (`fillForm(pdfFilePath, …, outputPath)`, `setTabOrder(path, names, path)`). The
  resident-file device discipline exists but lives in prose at
  `releaseResidentFile` (`IPdfEditorEngine.h:275-283`) and
  `PoDoFoBackend.cpp:1695-1714`. Nothing in the seam contract says what happens when
  `inputPath != residentFile` (fresh parse from disk — correct) or when
  `inputPath == outputPath` (in-place SafeSave transaction). A plugin/caller that guesses
  wrong gets a partially-documented hazard.
- Plan (plan-only): one "path mutator contract" block in `IPdfEditorEngine.h`: (a)
  path-mutators resolve the resident document or parse from disk; (b) in-place writes go
  through the SafeSave transaction with rollback (G06); (c) callers holding their own
  devices must call `releaseResidentFile` first; (d) path args are never re-validated
  after capture — deferred writers must use `saveDocumentIfCurrent(path, loadId, out)`.

**S1-5 (OBSERVATION) — Const-correctness as capability contract is in good shape.** The
only seams where `const` reads mutate state internally (`getOutline`,
`validateSignatures`) are honest "reads that prime caches" cases; no const method grants
write capability. No action.

---

## 2. Surface 2 — The command dispatch surface (ToolId → ToolRegistry → controllers)

**What is already right (verified):**
- The ARC07 gate is real for every registry route: `ToolRegistry::activate` checks
  `isEnabled()` before dispatch and emits `toolRefused` (`src/shell/ToolRegistry.cpp:37-42`);
  Ribbon (`Ribbon.cpp:210`) and MenuBar (`MenuBar.cpp:480`) both obtain QActions via
  `registry->actionFor()`, so ribbon and menu share the exact dispatch path; lazy-created
  actions for unregistered ids are disabled (fail-closed, `ToolRegistry.cpp:72`).
- `EditPolicy.h` is genuinely the ONE policy implementation: `isMutatingTool` enumerates
  every mutating id in one switch; controllers' `isEnabled` are one-liners over it
  (verified in all 7 overridden controllers; `TaskNavController` returns true but only
  navigates screens — no mutation, acceptable).
- Alias table (`src/core/ToolId.cpp:165-341`): **no duplicate keys** — verified
  mechanically by extracting every string literal and `sort | uniq -d` (empty output).
  Since dispatch is by `ToolId`, an alias cannot reach a handler other than its ToolId's
  controller.
- Stale/foreign-document discipline at the shell: `beginDocument()` is called at exactly
  two sites (`GpMainWindow.cpp:644, 884`) and BOTH clear the undo stack immediately
  (`:668, :894`) — the A→B "undo commands mutate document B" class is closed at the
  boundary. Commands resolve `m_doc->path()` at execution time (verified across
  `src/commands/*.h`), and `DocumentSession::setPath` is a same-path no-op vs
  `beginDocument` minting a new identity (`DocumentSession.h:40-47`).

**Findings:**

**S2-1 (HIGH) — Read-only bypass: `PagesMode::onApplyReorder` mutates in place with no
gate.**
- Evidence: `src/modes/PagesMode.cpp:1288` — the slot checks only document presence, then
  pushes `ReorderPermutationCommand` (`:1326`), whose redo() calls
  `engine->reorderAllPages(path, …)` — an in-place write. The Apply button is wired at
  `:672`. Its sibling routes ARE gated: grid drag/keyboard moves
  (`PagesMode.cpp:1427` — with a comment citing ARC07) and page labels (`:1566`).
  `EditPolicy.h`'s comment claims "PagesMode reorder/labels … call mutationBlocked() at
  their top" — TRUE for labels and the grid route, FALSE for the list route. An expired
  (read-only per §9.11) document can be reordered through this panel.
- Plan (plan-only): add the standard block at the top of `onApplyReorder` (mirror
  `:1427-1434`); while there, add a registry-integrity test asserting every slot in
  `src/modes/*` that pushes a mutating command calls `EditPolicy::mutationBlocked` or is
  reached only through `ToolRegistry::activate` — the drift between the comment and the
  code is the finding.

**S2-2 (HIGH) — Read-only bypass: `FormBuilderMode::onTabOrderApplyClicked`.**
- Evidence: `src/modes/FormBuilderMode.cpp:469` — checks document presence only, then
  `setTabOrder(path, orderedNames, path)` (`:488`) — an in-place write to the same path.
  Wired at `:215`. `grep isReadOnly src/modes/FormBuilderMode.cpp` = 0 hits; the mode
  consults the shared gate nowhere. Ribbon route for the same operation
  (`ToolId::Tabs`) IS gated (it is in `isMutatingTool`), so the panel button is a
  privilege escalation relative to the ribbon for the identical operation.
- Plan (plan-only): same gate block as S2-1; additionally decide the mode-level posture
  (either FormBuilderMode's mutation controls disable on `readOnlyChanged`, or every
  mutating slot gates individually — the second matches the existing ARC07 pattern and
  costs one guard per slot).

**S2-3 (MEDIUM) — Direct controller activation bypasses the registry gate.**
- Evidence: `src/GpMainWindow.cpp:1138` — `SignaturesPanel::placeSignatureRequested` →
  `_security->activate(ToolId::Sign)` calls the controller directly, skipping
  `ToolRegistry::activate` where the ARC07 refusal lives. The comment at `:1136` claims
  activate "also guards on an open document" — true for *existence* only
  (`SecurityController.cpp:371-375`); there is no read-only check inside
  `SecurityController::activate` or `signDocument()` (`:545`). Severity is tempered
  because `signDocument()` writes to a user-picked NEW file (copy-shaped, like SaveAs
  which deliberately stays available) — but the invariant "mutating dispatch crosses the
  single gate" is broken, and the safety of this route rests on the sign flow's internal
  details (exactly the scattering ARC07 removed). Same class, benign today because the
  ids are non-mutating: `:239`, `:241`, `:243`, `:751`.
- Plan (plan-only): route the panel signal through
  `_toolRegistry->activateFromString("sign")` (or `actionFor(ToolId::Sign)->trigger()`)
  so the gate and `toolRefused` telemetry apply; correct the `:1136` comment. Optional
  belt: make `SecurityController::activate` assert-not-refused for mutating ids in debug
  builds.

**S2-4 (LOW) — Intent-misroute aliases.**
- Evidence: `src/core/ToolId.cpp` — `"export"`→ExportData and `"import"`→ImportData
  (`:285-286`), `"delete"`→DeleteSelection (`:238`), `"regex"`→FindReplace (`:316`, NOT
  RegexRedact despite the name), `"bookmarks"`→PaneBookmarks (`:323`). These are
  single-owner today (no dup keys) but any future `add()` that reuses one of these
  generic strings silently re-binds it (QHash::insert last-wins, no warning).
- Plan (plan-only): a one-shot unit test that (a) re-runs the duplicate-key extraction
  against the compiled table via `toolIdFromString` for every alias literal, and (b)
  pins the canonical mapping of the generic aliases above so a collision fails the test
  instead of silently re-binding dispatch.

**S2-5 (LOW) — Duplicate controller registration is warning-only.**
- Evidence: `ToolRegistry.cpp:16-19` — a second controller claiming an already-owned
  ToolId is skipped with `qWarning`; the first registration silently wins and the new
  controller's tool is dead.
- Plan (plan-only): qFatal/Q_ASSERT in debug; the existing `TestRibbonIntegrity` /
  `TestControllers` suite is the natural home for a "every ToolId in ToolId::COUNT has
  exactly one controller or none" assertion.

**S2-6 (OBSERVATION, POSITIVE) — Deferred-writer identity guards.** EC02 + G04
(`IPdfEditorEngine.h:158-181`) and EditController's snapshot-load-id checks
(`EditController.cpp:798, 848`) cover the stale/foreign-document matrix for async writers;
`TestDocumentIdentity`, `TestHistoryIntegrity`, `TestReadOnlyGate` are the regression
net. No gap found in this lineage.

---

## 3. Surface 3 — Persistence / secret storage (the desktop auth/token checklist)

**What is already right (verified in code):**
- **At-rest formats (SEP13:5 v3):** `EncryptedFileSecretStore` writes 0x04 (DPAPI with the
  SERVICE NAME as optional entropy) or 0x03 (AES-256-GCM with the service name as AAD)
  — every blob is cryptographically bound to its entry identity, so the v2 (0x02,
  constant-description DPAPI) blob-swap attack fails loudly (`EncryptedFileSecretStore.h:17-51`,
  `.cpp:111-152`). Legacy formats remain readable for migration; v2 is never written.
- **Never-silent-fail:** `storeSecret` returns false without writing on any encrypt/open/
  short-write/commit failure (`EncryptedFileSecretStore.cpp:313-359`), then verifies
  re-readability before returning true (`:361`). `ISecretStore` documents the contract
  and `CredentialManager` implements loud fallbacks (Windows cred vault → libsecret →
  labelled encrypted file; a locked keyring is a LOUD failure, `CredentialManager.cpp:97-112`).
- **No plaintext secrets in settings:** `grep QSettings … | grep -iE password|apikey|secret|token`
  across src = zero hits. AI keys go through CredentialManager.
- **Signing-request sidecar is password-free:** `SigningRequestModel` (`SigningRequestModel.h:33-90`)
  persists names, field bindings, hashes, and engine-attested outcomes only; the
  fail-closed schema handshake (magic key + schemaVersion) refuses anything else, and the
  SWEEP-W1 F3 honesty rule keeps authorization OUT of the unsigned JSON.
- **Memory hygiene (M1 verified):** `PoDoFoBackend` clears `encryptionPassword` on
  resident-file release (`PoDoFoBackend.cpp:1714`, the M1 rule), on lineage changes
  (`:450`, `:3239`, G01) and on failed load (`:405`); the private key buffer is cleansed
  (`OPENSSL_cleanse`, `SignatureManager.cpp:1458`). The signing password is held only in
  the dialog, the `SigningRequest` value copy inside the sign worker/retry lambdas, and
  `SignatureManager` call parameters — no member cache, no persistence, no session
  retention feature exists.

**Findings:**

**S3-1 (LOW) — The fallback store file gets default permissions.**
- Evidence: `EncryptedFileSecretStore.cpp:337-344` — `QSaveFile` write with no explicit
  restrictive ACL/attribute step; protection relies on the user-profile's inherited ACLs
  (reasonable on Windows, weaker in odd multi-user setups; on non-Windows no chmod 0600
  equivalent).
- Plan (plan-only): after mkpath/before write, apply the tightest ACL the platform offers
  (owner-only), and document in `EncryptedFileSecretStore.h` that the file inherits
  profile ACLs today.

**S3-2 (LOW) — Retry lambda retains the certificate password until the dialog dies.**
- Evidence: `SecurityController.cpp:228-249` — `req` (containing `pwd`) is captured by
  value in the worker lambda and the `finished` handler lambda; for a PartialLtvMissing
  retry prompt the copy lives until that dialog/handler is destroyed. Not a persistence
  leak, but longer than needed.
- Plan (plan-only): on final outcome (Success/Failed/no-retry-offered), clear `req.pwd`
  in the captured copy before the handler returns; document that the retry window is the
  only retention window.

**S3-3 (OBSERVATION) — `deleteSecret` is not secure erase.** The JSON store is rewritten
atomically but the old ciphertext remains in freed blocks/backups. Desktop-acceptable;
document in the header so nobody assumes otherwise.

**S3-4 (DOCTRINE NOTE, no action) — QString cannot be zeroized** (implicit sharing, UTF-16
copies). The M1/G01 lineage-clear rule is the project's chosen mitigation and is applied
consistently at all four engine password sites; true wipe semantics would require
QByteArray-backed secret plumbing end-to-end. Recorded as a known, documented limitation —
the honest ceiling for this stack.

---

## 4. Surface 4 — Rate/size limiting (the DoS-surface review)

**Caps verified holding in code (the brief's "verify the caps hold"):**
- **Ollama (SECFIX-5, SEP13:6, R04):** per-message cap 32 KiB and aggregate egress cap
  96 KiB with honest trimming logs (`OllamaProvider.cpp:193-231`); response cap via
  `setReadBufferSize` PLUS explicit abort when the body crosses it, bounded
  4 KiB–1 GiB, default 64 MiB (`:237-249, :308-316`); timeout `qBound(50, setting,
  600000)` (`:233`); endpoint allowlist rejects non-HTTP(S) schemes, non-loopback
  HTTP hosts, user-info URLs, and empty hosts (`:44-70`, R04).
- **Update manifest (B-03):** HTTPS-only manifest URL even against tampered QSettings
  (`UpdateChecker.cpp:38-39, 58-60`); `NoLessSafeRedirectPolicy` (`:123-124`);
  20 s transfer timeout (`:128`); the manifest's `downloadUrl` must be exactly https AND
  `sha256` non-empty or the manifest is refused before anything downloads
  (`:173-191`) — the msiexec-on-attacker-bytes path stays closed.
- **Batch preset JSON:** `kMaxSteps=16`, `kMaxFileBytes=256 KiB`, `kMaxStringParamBytes=1 MiB`
  (`BatchPreset.h:81-83`), enforced with structured refusals (`BatchPreset.cpp:408-411,
  519`), fail-closed schema handshake (`:546-560`).
- **Misc bounded inputs:** image dimensions capped at 10000 px in conversion
  (`ConversionManager.cpp:1392`) and OCR (`OcrEngine.cpp:274, 334`); render-cache memory
  thresholds (`RenderCache.h:23-24`); load of >500 MB warns honestly and continues by
  design (`PdfEditorEngine.cpp` loadDocumentForEditing, D5) — a disclosed UX decision,
  not a silent OOM.

**Findings:**

**S4-1 (MEDIUM-LOW) — The manifest response body is read uncapped.**
- Evidence: `UpdateChecker.cpp:143` — `reply->readAll()` with no size check. The 20 s
  transfer timeout bounds duration, not bytes: a fast hostile host (the manifest URL is a
  user/company setting) can push hundreds of MB–GB of JSON into RAM within the window
  before parse rejects it.
- Plan (plan-only): refuse when `Content-Length` (if present) exceeds ~1 MiB, and
  additionally abort streaming reads past that bound; manifests are tiny — the honest
  cap costs nothing.

**S4-2 (LOW) — No batch file-list count cap.**
- Evidence: `src/modes/BatchMode.cpp` — no `kMax` on `m_filesToProcess`; 100k enqueued
  files = unbounded runtime and log growth (sequential processing keeps memory bounded).
- Plan (plan-only): soft warning above a documented threshold (e.g., 500) with an
  explicit continue; no hard cap (legit bulk users exist).

**S4-3 (LOW) — `kMaxPageCount = 10000` exists only in the test file.**
- Evidence: `tests/TestResourceLimits.cpp:13` declares the constant and the comment
  admits the test validates "current safe behavior" when the engine limit is undefined;
  grep finds no page-count enforcement in `src/engines/`. The engine relies on PoDoFo's
  own structure limits; a 100k-page document loads and the cap silently doesn't exist.
- Plan (plan-only): either enforce a page-count ceiling with an honest ErrorInfo in
  `loadDocumentForEditing`, or change the test's constant comment to say the value is a
  test-local probe, not an enforced engine limit — today the naming misleads.

**S4-4 (VERIFICATION NOTE) — The brief's "preset/sidecar JSON covered by fuzz" is only
partly accurate.** The only fuzz harness is `tests/TestDjotFuzz.cpp` (djot sidecar).
Preset JSON has structured corruption/handshake tests (`TestBatchPresets.cpp:520-524`)
and the caps above, but no property-style fuzz run. The caps are real and enforced; the
*fuzz* claim should be recorded as "handshake + corruption tests, no fuzz harness."

---

## 5. Surface 5 — Structured security logging

**What the project has instead of a log stream (verified):**
- **Redacted support bundle** (the standard the brief cites): built from an allowlist of
  non-sensitive inputs, machine-policy VALUES and URL settings excluded, recents as
  counts, path usernames scrubbed as defense in depth (`SupportBundle.h:9-30`,
  `SupportBundle.cpp:90-132`).
- **Per-document audit artifacts by construction:** the signing-request sidecar records
  what the engine actually did per signer (signed field, attained PAdES level, validation
  summary — `SigningRequestModel.h:53-64`); the redaction proof pack
  (`RedactionProof.h`, T1-2) is machine-verifiable redaction evidence.
- **Network touchpoint disclosure** (`NetworkTouchpoints.h`): every touchpoint's enabled
  state + consent key, policy-effective values, gaps disclosed not papered over.
- **Refusal logging exists but is scattered qWarnings:** R04 endpoint blocks
  (`OllamaProvider.cpp:46-68`), update-manifest rejections (`UpdateChecker.cpp:177-188`),
  secret-store failures (`EncryptedFileSecretStore.cpp:131-351`), TSA degradations
  (`SignatureManager.cpp:632-1765`). None log secret payloads.

**Findings:**

**S5-1 (MEDIUM, doctrine gap) — No durable, structured security-event trail.**
- Evidence: exactly one `qInfo()` in all of `src/` (grep count = 1); `PolicyController`
  (machine-policy enforcement — a textbook security event) logs nothing and discloses
  only through the UI status line/enforcement notes (`PolicyController.h:94-111`). Sign,
  encrypt, redact-apply, policy-override and network-refusal events live in transient
  status-bar messages or stderr qWarnings, which no support artifact captures (the
  support bundle is settings-only by design).
- Plan (plan-only, two honest options — pick one deliberately): (a) a minimal
  append-only JSONL security-event sink under AppData (event id, outcome, document
  fingerprint, timestamp — never payloads, scrub discipline reused from SupportBundle),
  opt-in via settings with the Network-page disclosure treatment; or (b) write the
  scope-cut down: single-user desktop app, per-document artifacts are the audit trail —
  and record that decision in the role-file mapping so the doctrine gap is a decision,
  not an oversight.

**S5-2 (LOW) — A refusal log line can embed user-info credentials.**
- Evidence: `OllamaProvider.cpp:56-59` — the R04 branch that rejects an endpoint WITH
  user-info logs `url.toString()` — i.e. the exact string containing whatever the user
  (or a malicious config import) embedded, `user:pass@host`, lands in the log. The
  sibling branches (`:46`, `:68`) also log full URLs. The project's own SupportBundle
  doctrine says diagnostics carry no credential-shaped payloads.
- Plan (plan-only): log `scheme + host + "…"`, never the full URL, in all three R04
  refusal branches (the user-info branch needs only "contained user-info", which is the
  finding).

**S5-3 (OBSERVATION) — Policy enforcement is disclosed, not logged.** `PolicyController`
honesty surfaces (status line, enforcement notes, NetworkTouchpoints integration) are
user-visible but ephemeral. If S5-1 option (a) is adopted, policy-override is the first
event to record; if (b), the disclosure surfaces are the recorded mechanism.

---

## 6. What was NOT done (honest residuals)

- **No build, no probe requiring one.** Every finding above is established by reading
  code (file:line) and mechanical greps; no claim needed a compiled probe. /d had 43G
  free at setup (below the ~50G guard), reinforcing the no-build choice.
- **No dynamic verification of the two HIGH bypasses** (S2-1, S2-2) — the code paths are
  unambiguous (slot → command → engine in-place write, zero gate callsites in the file
  region), but a UI-driven repro was out of scope for an audit-only sweep. The
  remediation test in the S2 plan closes this.
- **LibSecretStore.cpp reviewed only in summary** (Windows is the audit's primary
  platform; the libsecret path's loud-failure contract is verified at the
  CredentialManager call site, `CredentialManager.cpp:97-105`).
- **Command/undo command classes** spot-checked (AddFormField, DeleteFormField, DeletePage,
  ReorderPermutation, EditFormField) rather than exhaustively enumerated.

---

## Appendix A — Doctrine mapping (role-file checklist → GlyphPDF)

The role file (`backend-specialist`) prescribes a Node.js/Supabase/REST stack. This
appendix records, per checklist item, APPLIED (with the desktop analogue), ADAPTED, or
SKIPPED (and why) — the audit's value is the doctrine applied honestly, not keyword
matching.

| Role-file item | Status | Desktop analogue / where it was applied |
|---|---|---|
| "Every API route is an attack surface" | **APPLIED** | Engine seam headers treated as untrusted-caller contracts (§1); dispatch slots as routes (§2) |
| Input validation at every entry (Zod) | **APPLIED/ADAPTED** | Seam boundary checks instead of schemas: S1-1 (index bounds), S1-2 (options ranges) — the gaps ARE the finding |
| Auth middleware on protected routes | **ADAPTED** | ARC07 single gate + EditPolicy as the authz-at-the-boundary analogue; S2-1/2/3 are the "unprotected route" findings |
| Rate limiting on mutation endpoints | **APPLIED/ADAPTED** | SECFIX-5 egress caps, SEP13:6 response cap, batch preset caps, S4-1 manifest body cap gap |
| CORS / security headers | **SKIPPED** | No HTTP server surface; nearest analogue = R04 endpoint allowlist (egress, not ingress) + B-03 https enforcement — verified holding |
| RLS on user-facing tables | **SKIPPED** | No multi-tenant store; nearest analogue = the read-only session gate (EditPolicy) + document identity guards (EC02/G04), audited in §2 |
| Secrets from env, validated at startup | **ADAPTED** | Secrets from OS vault/DPAPI store with never-silent-fail contract (§3); PolicyController machine keys loaded once per process (`ensureLoaded`) |
| Passwords hashed (bcrypt) | **SKIPPED** | No credential database; passwords are unlock secrets for P12/PDF crypto — at-rest binding (v3 entropy/AAD) is the analogue (§3) |
| Never log passwords/tokens/PII | **APPLIED** | Verified by grep (clean) — then S5-2 found one refusal branch that can embed user-info credentials; SupportBundle scrub is the standard |
| Structured logging of significant actions | **PARTIAL/GAP** | S5-1: the project substitutes per-document honest artifacts (sidecar, proof pack) + scattered qWarnings; no durable event sink — decision needed |
| Error responses never expose stack traces | **ADAPTED** | `ErrorInfo` carries user-facing message + separate technical detail; support bundle excludes probe detail (can carry paths) |
| Sandbox path shares prod code path | **APPLIED** | `IOcrEngine::isMockImplementation`, SignatureManager mock-vs-real signing share the dispatch seam; mock/test stores use the same `ISecretStore` contract |
| Money-field race condition | **SKIPPED** | No ledger; nearest analogue = save-transaction identity guards (EC02/G04) and single-commit mutation contracts (G06/G08) — audited, holding |
| Hold-transaction-across-external-call | **APPLIED** | Sign flow: engine lock held across TSA/OCSP network I/O is the analogue — the sign worker runs off the GUI thread with weak refs and a bounded timeout; noted, no defect found |
| Third-party APIs mocked before live creds | **APPLIED (existing)** | TestSignatureValidationMock, TestOllamaProvider mock endpoints |

**Files central to this audit:** `src/core/interfaces/IPdfEditorEngine.h`,
`ISignatureManager.h`, `IFormManager.h`, `IToolController.h`, `src/shell/EditPolicy.h`,
`src/shell/ToolRegistry.cpp`, `src/core/ToolId.cpp`, `src/engines/DocumentSession.h`,
`src/modes/PagesMode.cpp`, `src/modes/FormBuilderMode.cpp`,
`src/shell/controllers/SecurityController.cpp`, `src/GpMainWindow.cpp`,
`src/core/EncryptedFileSecretStore.{h,cpp}`, `src/core/CredentialManager.cpp`,
`src/core/SigningRequestModel.h`, `src/engines/ai/OllamaProvider.cpp`,
`src/core/UpdateChecker.cpp`, `src/core/BatchPreset.{h,cpp}`,
`src/core/SupportBundle.{h,cpp}`, `src/core/PolicyController.{h,cpp}`,
`src/engines/podofo/PoDoFoBackend.cpp`.
