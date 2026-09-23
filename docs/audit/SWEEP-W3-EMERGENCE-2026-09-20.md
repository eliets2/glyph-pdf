# SWEEP W3 — EMERGENCE ENGINE: Composition-Matrix Audit (2026-09-20)

Lane: `feat/sweep-w3-emergence` (base = mainline tip `ec9f16f6`, everything merged:
s4s, presets, policy, accessibility, summaries, rotate-270 law, W1 fixes).
Lens: EMERGENT system behavior — feature interactions no single component designs
for. Analysis + repro-probe lane: production code review-only; probes = existing
pinned suites run at this tip + code-path evidence. Fixes are NOT in this lane —
confirmed defects below are fix-lane inputs, ranked by user impact.

Verdict vocabulary: CONFIRMED-INTERACTION-DEFECT / SAFE-BY-CONSTRUCTION /
SAFE-BY-PIN / INCONCLUSIVE.

---

## 0. Composition matrix (verdicts at a glance)

| # | Interaction | Verdict |
|---|-------------|---------|
| 1a | Batch **preset** step × machine policy (AI/OCR step under policy) | SAFE-BY-CONSTRUCTION (schema has no such step) |
| 1b | Batch **classic OCR op** × `ocr/allowNetworkDownload` policy | **CONFIRMED-INTERACTION-DEFECT (rank 2)** |
| 1c | Policy changed mid-batch (load-once snapshot) | SAFE-BY-PIN (whole run under one snapshot; user-pref half live-reads — minor wrinkle) |
| 2a | Signing sidecar × /Rotate 270 — stale `preparedSha256` re-open UX | SAFE-BY-PIN (designed re-confirm loop, honest dialog) |
| 2b | Sidecar lazy placement of a stored anchor rect, post-fix build | SAFE-BY-PIN (rotate-270 law pins) |
| 2c | Pre-fix-created unsigned field + post-fix fill (cross-version replay) | CONFIRMED-INTERACTION-DEFECT (rank 4, narrow window) |
| 2d | `signatureFieldAnchors` rotation-imperfect read × badge anchoring | CONFIRMED-INTERACTION-DEFECT (rank 3, display-only; W2B-1 known residual, consumer now pinned) |
| 3 | OCR skip path × sanitize/CollectGarbage wrapper lifetimes | SAFE-BY-CONSTRUCTION (+ E-2 pins) |
| 4 | Accessibility scan × printable summary (MRC doc) | SAFE-BY-CONSTRUCTION / NOT-COMPOSABLE (the summary renders review comments only) |
| 4b | (adjacent) sanitize/`strip-metadata`/redact × applied a11y fixes | Designed-but-undisclosed: sanitizer strips /Alt, /ActualText, /E — later scans re-report coherently |
| 5 | Policy never-network × TSA-requiring surfaces | Refusal fires everywhere (SAFE); wording misdirects under policy — **CONFIRMED-INTERACTION-DEFECT (rank 5)** |
| 6 | form-JS keystroke × read-only document (EditPolicy asymmetry) | **CONFIRMED-INTERACTION-DEFECT (rank 1)** — asymmetry exists but INVERTED from the briefed design |
| 7 | Cross-session (2 instances) × SafeSave candidate commit | CONFIRMED-INTERACTION-DEFECT (rank 6, narrow interleaving); both-displaying case fails honestly instead; sidecar leg SAFE-BY-PIN |

---

## 1. Batch preset × machine policy (matrix item 1)

### 1a. A preset carrying an AI/OCR step — SAFE-BY-CONSTRUCTION

The preset schema vocabulary is closed and contains **no AI/OCR op**
(`src/core/BatchPreset.cpp:28-37` `knownOps()` = compress, strip-metadata,
pdfa-export, pdfa-check, watermark, redact). A preset file carrying an
`ocr`/`ai` step is rejected at LOAD with the full supported-ops whyNot —
`validateSteps` (`BatchPreset.cpp:418-421`) and `validateStepParams`
(`:401`) — never at run time, never silently. `batchPresetStepCapability`
(`BatchPreset.cpp:769-803`) additionally answers UnavailableBuild for any op
that is not in the vocabulary, so even a store written by a future/foreign
build is disclosed, not executed. There is therefore no pre-flight surface
that could need a *policy* whyNot for an AI/OCR step: such a step cannot
reach the runner. The only policy-gated OCR surface is the classic batch
OCR op (1b) and the interactive path.

### 1b. Classic batch OCR × the OCR download policy — CONFIRMED-INTERACTION-DEFECT (rank 2)

Mechanism (all line refs verified at ec9f16f6):

1. **Pre-flight is compile-time only.** `BatchMode::preFlightBlocker`
   (`src/modes/BatchMode.cpp:2585-2591`) queries `CapId::OcrTesseract`,
   whose probe is `#ifdef HAS_TESSERACT → Available`
   (`src/core/Capability.cpp:429-441`). It never queries
   `CapId::OcrLanguageData` (which exists — `Capability.cpp`, probe
   `probeOcrLanguageData`), and no capability probe consults
   `PolicyController` at all.
2. **Policy gate fires deep in the engine, console-only.**
   `OcrEngine::initialize` → `loadLanguageData`
   (`src/engines/OcrEngine.cpp:214-237`): when the traineddata file is
   missing and `ocr/allowNetworkDownload` (effective, policy-over-user) is
   false, the gate does `qWarning() << "OCR download disabled"; return
   false;` — no user-facing whyNot is produced anywhere.
3. **The batch worker ignores the failure.** `BatchMode.cpp:1521`:
   `capturedCtx->ocr->initialize(capturedOcrLang);` — return value
   discarded. The pipeline then runs.
4. **Two wrong outcomes, both reported as success:**
   - *Wrong-language fallback*: `OcrEngine::processImage`
     (`OcrEngine.cpp:281-282`) does `if (!d->isInitialized &&
     !initialize()) return results;` — a re-initialize with the **default
     language "eng"** (`IOcrEngine.h:10`, `OcrEngine.h:19`). If eng
     traineddata is present locally (the MSI bundles/seed it), a German
     document OCRs with English models and the file is reported
     **successful** with a wrong-language text layer.
   - *Silent non-searchable output*: if eng data is also missing,
     `processImage` returns an empty list (silent empty, not an error);
     the MRC export still writes an image-only `_ocr.pdf` and the file is
     accounted **successful** (the §9.12 P0 confidence note explicitly
     ignores empty page results, `BatchMode.cpp:1585-1592`).

The policy whyNot reaches **no** batch surface: the pre-flight said
Available, the per-file result says success, the only trace of the policy
refusal is a console qWarning.

Interactive divergence (same engine call, different discipline): the
interactive path DOES check initialize and surfaces a recoverable error —
`EditController.cpp:1086-1088` "OCR failed: Tesseract language data for
'%1' is unavailable." — but that wording is also **policy-blind** (it
never says the download is what was refused, nor that policy decided it).

Disclosure text is policy-blind too: `probeOcrLanguageData`'s Degraded
alternative promises "It will be downloaded from the tesseract-ocr
tessdata_best project on first use — this requires network access once"
(`Capability.cpp`, probe tail) — under the policy that download will never
happen, so the disclosure misleads in both interactive and batch
contexts. (The Network-Touchpoints page is the one honest surface:
`NetworkTouchpoints.cpp:148-166` reads the effective value.)

Required for the defect: policy `ocr/allowNetworkDownload=false` (or
default-off) + missing non-eng traineddata + batch OCR run of that
language. Cheap fix-shape for the fix lane (not applied here): check
`initialize`'s return at BatchMode.cpp:1521 and fail the file with a
policy-naming whyNot; optionally give OcrEngine a structured
reason-out-of-band instead of the bare `false`.

### 1c. Policy changed mid-batch — SAFE-BY-PIN (with one wrinkle)

`PolicyController::ensureLoaded()` is one-shot per process
(`PolicyController.cpp`, `m_loadedOnce`), and there are **no** explicit
`load()` call sites in production code — policy values are a load-once
snapshot shared by pre-flight, engine gates, and the network-touchpoints
page alike. A mid-batch policy-file change therefore cannot split the
batch: every file runs under the same policy (coherence, not staleness —
and the batch pre-flight cannot go stale relative to the worker because
both read the same frozen instance). Capability probes are memoized and
never consulted policy anyway.

Wrinkle (minor, noted for the record): the **user-preference half** of
every `effectiveValue` call is re-read live (`QSettings()` constructed at
each decision point), so a mid-run user toggle of a non-managed key
(e.g. re-enabling OCR download in Preferences while a batch runs) DOES
split the batch. Policy keys cannot split; user keys can. Asymmetry is
inherent to the precedence design, not a defect; worth one sentence in
the batch docs if it ever matters.

---

## 2. Signing-request sidecar × /Rotate 270 (matrix item 2)

### 2a. Stale `preparedSha256` re-open UX — SAFE-BY-PIN (designed, honest)

A sidecar prepared before the W2B-1 fix, re-opened after the document was
mutated (e.g. rotated), hits `SigningRequestRunner::precheck`'s
DocumentChanged refusal (`src/core/SigningRequestRunner.cpp:95-107`) and
the controller's re-confirm loop
(`src/shell/controllers/SendForSigningController.cpp:182-202`). What the
user sees, exactly:

- A `QMessageBox::question` titled **"Document Changed Since
  Preparation"** whose body is `mutationRefusalMessage`
  (`src/core/SigningRequestModel.cpp:64-73`): names the file, the
  expected and found SHA-256 prefixes (16 hex chars each), and states
  "Signing steps are refused until you review and re-confirm the request
  against the changed document.", followed by "Re-confirm this signing
  request against the document as it is now?" — Yes/No, **No default**.
- Yes → the reconfirmation is recorded out of band (SWEEP-W1 F3:
  `userReconfirmedSha256` set only in the controller after the user's
  Yes; the sidecar's `reconfirmedSha256` is display/record and never
  gates) and the sidecar is rewritten with the new hash + `preparedUtc`.
- No → the step is refused; nothing changed.

This is designed behavior, pinned by
`TestSendForSigning::mutationRefusalAndReconfirmPin`.

### 2b. Post-fix lazy placement of a stored anchor rect — SAFE-BY-PIN

The sidecar stores the anchor as viewer-convention rect
(`SigningRequestModel.h:58-60`), authored from user-entered dialog
coordinates (`src/ui/SigningRequestDialog.cpp:144, 298`), NOT from any
engine geometry read. Fields are created **lazily at fill time**
(SigningRequestDialog disclosure, `SigningRequestRunner.cpp:198-227`):
`spec.viewerRect = stepEntry.anchorRect` → `SignatureFieldCreator`, which
post-879c171 maps viewer→raw through `PageSpace::pageGeometry`
(GetMediaBoxRaw) and stores the rect VERBATIM (the raw-rect discipline;
the F5 containment runs against the same raw box). A pre-fix sidecar
replayed on a post-fix build therefore places its not-yet-created fields
**through the fixed code** — the stored viewer rect is re-resolved
correctly on rot-270/offset pages. Pinned by `TestRotate270PageSpace`
(7/7 at the law's landing: embed/round-trip/form-create/F5 literals on
rot 0/90/180/270 + offset shapes).

### 2c. Cross-version replay trap — CONFIRMED-INTERACTION-DEFECT (rank 4, narrow)

The one dangerous composition: a field physically created by a **pre-fix**
build (whose CreateField double-transformed view-space rects on rotated
pages — the second W2B-1 defect) that is still UNSIGNED when a **post-fix**
build fills the step. `precheck` only checks the field's existence and
signedness by name (`SigningRequestRunner.cpp:114-130`); the lazy-placement
branch is skipped (field exists), and the signature is placed into the
existing widget with its corrupted /Rect — silently, because the document
bytes are exactly the prepared bytes (the corruption happened during
preparation/earlier fill attempt, so `preparedSha256` matches and no
mutation refusal fires). Concrete path into that state: pre-fix fill attempt
where lazy creation succeeded but signing failed (the runner then publishes
the post-create hash — `SigningRequestRunner.cpp:221-226` — making the
retry legitimately hash-clean); the retry lands post-fix. Window is narrow
(same document, crossing the fix boundary mid-workflow), impact is a
misplaced visible signature with no detection. Fix-shape suggestion for the
owners: on lazy-fill, verify the existing bound field's /Rect against
`anchorRect` (viewer re-projection) and warn/refuse on divergence.

### 2d. `signatureFieldAnchors` residual × badge anchoring — CONFIRMED-INTERACTION-DEFECT (rank 3, display-only)

W2B-1 recorded the residual: `SignatureManager::signatureFieldAnchors`
(`src/engines/SignatureManager.cpp:2105-2128`) reads
`widget->GetRect()` and flips Y with `page->GetMediaBox().Height` — the
rotation-ADJUSTED height (W/H swapped on 90/270), so on any rotated,
non-square page the viewer Y is computed from the wrong dimension. This
lane pinned the consumer set: `SigningRequestDialog` and
`SigningRequestRunner` consume **only `fieldName`** (rect unused — the
sidecar is not polluted), but `SignaturesPanel.cpp:207-217` consumes
`a.rect` to anchor the signature **badges** painted over the viewer. On
rot-90/270 pages the badges paint at the wrong Y (wrong-flip height).
Signature validity is unaffected; the overlay lies. This is the
"re-audit requested" consumer the W2B-1 lane asked for: confirmed as
display-only, but user-visible on every rotated-page signature. Fix-shape:
compute the flip from the same `PageSpace::pageGeometry` the law
introduced (one function, sibling of the fixed `fieldRectFromDisplay` in
`FormManager.cpp:63-68`).

---

## 3. OCR skip × sanitize/CollectGarbage lifetimes (matrix item 3) — SAFE-BY-CONSTRUCTION

The briefed composition ("skip-text files and sanitize-step files in one
preset pipeline") cannot occur, and the lifetimes cannot alias:

1. **One op per batch run.** `OpOCR` and `OpPresetPipeline` are mutually
   exclusive arms of the same worker switch (`BatchMode.h:337-351`,
   `BatchMode.cpp:1451/1472`). A preset pipeline cannot contain the OCR
   skip path (no OCR step in the schema, §1a) and the OCR op cannot
   contain sanitize steps.
2. **Fresh engine per chain step.** `runPresetChain` constructs a new
   `PdfEditorEngine` per step, self-contained load/save
   (`BatchMode.cpp:1126-1131`) — no loaded document is shared across
   steps or files, so sanitize's in-place dict scrubs (E-2) never
   coexist with another step's wrapper on the same object.
3. **The skip path's kept-page extractor is per-file fresh** with its own
   full load (`BatchMode.cpp:1611-1618`), function-local unique_ptr.
4. **The only shared engine in batch** (`capturedCtx->pdfEditor`) is used
   exclusively by the OCR op for `exportMrcPdfA`, which is self-contained
   (takes page images + results; builds its own MRC processor; no
   dependence on, or mutation of, a previously loaded document state) —
   it never runs sanitizeDocument, so the CollectGarbage/free-wrapper
   hazard class E-2 closed (`PoDoFoBackend.cpp:3254-3368`: /Info and
   /Outlines cleared IN PLACE precisely so Save's CollectGarbage cannot
   free a cached wrapper) has no second actor in batch.

E-2's own regression pins (`TestSanitization`,
`TestSanitizeTrailerUaf`) cover the double-save-on-one-loaded-document
composition that motivated the in-place discipline.

---

## 4. Accessibility scan × printable summary (matrix item 4) — NOT-COMPOSABLE

The "printable summary" surface is `ReviewSummaryWriter`
(`src/engines/ReviewSummaryWriter.h`): a printable document built from
**review comment records** (`AnnotationItem`), rendered by
`CommentsWidget::exportReviewSummaryPdf` (`src/ui/CommentsWidget.cpp:883`).
Accessibility findings (`A11yReport`/`A11yFinding`,
`src/engines/AccessibilityChecker.h`) have no path into it: the
AccessibilityPanel's surface is "Run Check" + per-finding Apply/FIX only —
no export, no summary, no print. Misattribution is impossible because the
two data planes never meet.

Coherence of the scan itself on an MRC-re-encoded (scanned) document: the
image /Alt finding is a struct-tree/resource-walk check
(`AccessibilityChecker.cpp:62-96, 324`) — a full-page MRC background
image without a struct-tree /Alt IS reported as an image-alt finding on
its page, which is the truth; attribution is page-scoped and correct.

Adjacent interaction worth the owners' attention (designed, undisclosed):
`sanitizeDocumentContents` strips `/Alt`, `/ActualText`, `/E` from the
whole StructTreeRoot (`PoDoFoBackend.cpp:3318-3351`) and removes
/MarkInfo — the privacy sanitizer deliberately deletes accessibility
metadata. Consequence: a `strip-metadata` or `redact` step (both end in
sanitizeDocument) silently **undoes applied accessibility fixes**; the
next scan re-reports the same findings (coherent, not misattributed).
The sanitizer's UI/docs disclose what it removes in general terms; the
specific "your /Alt fixes are gone" consequence is not called out
anywhere user-facing. Doc-only suggestion for the fix lane.

---

## 5. Policy never-network × TSA/OCSP surfaces (matrix item 5)

**Every surface refuses up front — verified end to end:**

- One production reader of signing settings:
  `SecurityController::readSigningConfig`
  (`src/shell/controllers/SecurityController.cpp:107-125`) resolves
  `signing/tsaUrl` + `signing/padesLevel` through
  `PolicyController::effectiveValue`. Both the sign/certify dispatch
  (`SecurityController.cpp:198`) and the send-for-signing sidecar flow
  (`SendForSigningController.cpp:138-141`) run
  `signingPreflightRefusal(cfg.level, cfg.tsaUrl)` BEFORE any dispatch;
  the same statics are reused by the sidecar lane by design.
- OCSP: consent is per-document with a global "never"
  (`signing/ocspNetworkPolicy`); a refused/never decision refuses the
  B-LT/B-LTA dispatch before any network attempt
  (`SecurityController.cpp:223-241`) with a reason that names the way
  out (B-T/B-B need no OCSP). Note the OCSP key is deliberately NOT in
  the policy allowlist (`knownKeysImpl` — 6 keys, PolicyController.cpp);
  a machine policy cannot manage OCSP consent, only the user can. The
  `effectiveValue` read in NetworkTouchpoints on that key is therefore a
  passthrough (harmless; page shows the user value — correct).
- AI: policy-managed empty `ai/ollamaEndpoint` disables chat with a
  whyNot naming the policy (pinned by `TestPolicyWiring`).
- Preset leg: nothing in a batch preset can require TSA (no signing op,
  §1a) — SAFE-BY-CONSTRUCTION.

**CONFIRMED-INTERACTION-DEFECT (rank 5, honesty):** the TSA refusal text
is policy-blind and self-contradicting under policy. When policy empties
`signing/tsaUrl` while the user has a URL configured,
`signingPreflightRefusal` (`SecurityController.cpp:128-145`) says
"…requires a timestamp authority (TSA) URL, which is not configured. Set
it under Preferences → Security → Signing…" — but the Preferences widget
is disabled with a "Managed by policy" badge for exactly that key. The
user is directed into a dead end, and the refusal never mentions that
policy decided. Every other policy surface (Preferences, support bundle,
network page) discloses the override; the refusal path is the one that
doesn't. Fix-shape: one clause — when `isManaged("signing/tsaUrl")`,
append "This setting is managed by machine policy (see Preferences)."
The refusal is otherwise correct (no network attempted, document
untouched).

---

## 6. form-JS keystroke × read-only documents (matrix item 6) — CONFIRMED-INTERACTION-DEFECT (rank 1)

**The briefed asymmetry does not exist; the real one is inverted and
accidental.**

Ground truth at ec9f16f6:

1. **Read-only = expired documents.** The session flag is set only by the
   expiry feature (`GpMainWindow.cpp:664, 920`); the viewer mirrors it
   (`:325-335`) and tool enablement re-syncs from the same signal. ARC07's
   "ONE read-only policy" (`src/shell/EditPolicy.h`) gates two channel
   families: tool dispatch (`toolRefusedByReadOnly`, consumed by every
   controller's `isEnabled`) and direct mutation entries
   (`mutationBlocked`, consumed at PagesMode/EditController/
   PagesController/HomeController direct slots).
2. **There is NO on-open Calculate.** The calculate cascade runs only
   INSIDE committed form-save transactions
   (`FormManager.cpp:92-106`, `runInTransactionCalculateCascade`;
   Acrobat-order /V wiring at `FormManager.cpp:598` is the fillForm
   transaction). Viewing never computes; doc-level/OpenAction JS is
   explicitly not landed (FormJsRunner.h P3 hooks absent per the R26/P3
   scope). The matrix premise "Calculate on open still runs (it must)" is
   false — there is nothing to protect on the viewing path because the
   run-side cascade is already confined to mutations.
3. **The keystroke tier has no EditPolicy gate.**
   `FormFieldPropertiesPanel::onDefaultTextChanged`
   (`src/modes/FormFieldPropertiesPanel.cpp:201-262`) calls
   `FormManager::runKeystrokeEvent` (`FormManager.cpp:497-514`) — which
   loads a throwaway `PdfMemDocument`, runs the /AA /K script in the
   quickjs-ng sandbox, returns a display outcome; nothing persists. The
   panel and its host `FormBuilderMode` contain **zero**
   `isReadOnly`/`EditPolicy` references; Form Builder's controls are
   enabled on `hasDoc` alone (`FormBuilderMode.cpp:241-244`). The
   matrix premise "the panel gate blocks user typing on read-only" is
   also false — there is no such gate.
4. **Worse: the panel's Apply PERSISTS on a read-only document.**
   `onApplyClicked` (`FormFieldPropertiesPanel.cpp:321-357`) pushes
   `EditFormFieldCommand` → `applyFieldSnapshot`
   (`FormManager.cpp:307`) → `runFormSaveTransaction` → SafeSave
   candidate → **atomic commit to the document path**. No gate anywhere
   in that chain consults the session's read-only flag (verified: no
   `EditPolicy`/`mutationBlocked`/`isReadOnly` in
   FormFieldPropertiesPanel, FormBuilderMode, EditFormFieldCommand.h,
   FormManager). The apply also runs the calculate cascade and saves —
   i.e. exactly the "computed + persisted" composition ARC07's policy
   exists to refuse on an expired document.

Consequences, ranked: (a) an **expired document's form field properties
can be edited and saved** through the panel — a hole in the expiry
read-only contract (the one read-only policy, bypassed by a UI surface
that predates the policy or was never added to its consumer list);
(b) typing in the panel executes the field's /AA /K script on a read-only
document (in-memory, sandboxed, egress-verb-blocked, non-persisting —
execution still happens where the session says the document is closed
for anything but viewing); (c) honesty: the user types, sees "Adjusted
by the field's keystroke script", and can Apply — the UI affords a write
that the session forbids, with no refusal message ever shown
(`EditPolicy::readOnlyMessage()` never fires on this path).

Fix-shape (root-cause, one boundary): put the gate at the panel's two
entries — `onDefaultTextChanged` (early-return + status text) and
`onApplyClicked` (`EditPolicy::mutationBlocked` before the push, showing
`readOnlyMessage()`), and add Form Builder to the direct-entry consumer
list in EditPolicy.h's comment. Optionally, one engine-side assertion in
the command's caller for defense in depth. (Fix lane's call; not applied
here.)

---

## 7. Cross-session: two instances, one document (matrix item 7)

Composition of the FU-2 candidate-dir family and SafeSave under real
(two-process) usage:

- **Candidates collide-safe.** `%TEMP%\glyphpdf-candidates` is shared by
  every process, but candidates are `QTemporaryFile`-unique
  (`SafeSave::makeUniqueCandidate`) — two instances never pick the same
  candidate name. The FU-2 flake class is a TEST-assert artifact
  (absolute-zero leftover scans vs other suites' in-flight candidates,
  since fenced by `ctest RESOURCE_LOCK GlyphpdfCandidates`, commit
  `ae636a5a`), not a production hazard. The FU-4 success-path leak is
  fixed in this tree (`SignatureManager.cpp:1830-1837` remove after
  `candidateCommitted=true`; `:1933` for timestamps).
- **Commit is atomic but UNCONDITIONAL.** `commitFileToDestination`
  (`SafeSave.cpp:207-259`) has no destination-identity precondition (no
  mtime/size/hash compare) — it replaces whatever is on disk with the
  validated candidate. The handle coordinator is per-process
  (`SafeSave.h:102-118`, installed once per MainWindow).
- **Both instances displaying the file: mutual honest failure.** The
  documented Windows behavior that motivated the coordinator — the
  viewer holds the destination handle without delete-share for the whole
  session — means each instance's atomic rename fails with access-denied
  while the OTHER instance displays the file. Data-safe (original
  byte-identical, error surfaced), UX-confusing, no corruption.
- **The dangerous interleaving: stale-memory overwrite.** If instance B's
  commit runs while B's viewer handle is released (B parked the document
  for its own commit; or B's session holds no live handle at that
  moment), B's candidate — serialized from B's **stale in-memory**
  document — replaces the file and **silently drops instance A's
  signature** (or any of A's writes). No detection exists on the
  plain-save path (no precondition, no warning surface). CONFIRMED by
  construction; the window is the release/restore scope plus any
  closed-viewer-but-live-session state.
- **The sidecar flow is designed cross-session-safe.** The fill path
  re-reads the CURRENT disk bytes every step
  (`SigningRequestRunner::precheck` hashes `docPath` from disk,
  `:95-107`): after A signs, B's next step refuses with
  DocumentChanged and the re-confirm loop (§2a). Residual TOCTOU
  between precheck-hash and commit exists but is seconds-wide and
  user-mediated (dialog between), vs. the plain-save path's unguarded
  write.

Verdict: CONFIRMED-INTERACTION-DEFECT (rank 6, narrow interleaving,
silent signature loss as worst case) + SAFE-BY-PIN for the sidecar leg.
Fix-shape suggestion for the save owners: record (size, mtime) or the
loaded-bytes hash at session load and make `commitFileToDestination`
callers verify it before commit (a precondition parameter, not a
SafeSave API break), failing with "the document changed on disk —
reload before saving".

---

## 8. Probe evidence (run at this tip)

Build: fresh `build-emg`, Ninja, Debug, UCRT64, `-j 2`. Probes = the
existing suites that pin each SAFE half of the matrix, run at
ec9f16f6+W3-lane-commits. (Results recorded after the runs complete —
see section 8.1.)

### 8.1 Results — 9/9 PASSED (offscreen, serial, `-j 1`, 2026-09-20)

```
TestSanitization ......... Passed (0.19 s)   — E-2 in-place /Info discipline (M3)
TestSanitizeTrailerUaf ... Passed (0.06 s)   — CollectGarbage/UAF regression pin (M3)
TestSendForSigning ....... Passed (0.43 s)   — sidecar handshake, mutationRefusalAndReconfirmPin,
                                               candidatesDirClean (M2a, M7)
TestBatchOcrLanguage ..... Passed (0.08 s)   — language-data probe behavior (M1)
TestBatchOcrSkipText ..... Passed (46.9 s)   — skip-files/skip-pages mixed assembly (M3)
TestFormJsCalc ........... Passed (13.6 s)   — calculate cascade confined to committed
                                               transactions (M6: no on-open compute)
TestPolicyController ..... Passed (0.11 s)   — load-once snapshot, effectiveValue (M1c)
TestPolicyWiring ......... Passed (0.08 s)   — per-key enforcement pins incl.
                                               policy-empty AI endpoint whyNot (M5)
TestRotate270PageSpace ... Passed (0.22 s)   — rotate-270 law + F5 + raw-rect pins (M2b)
100% tests passed, 0 tests failed out of 9
```

Every SAFE verdict above is pinned at this tip by at least one of these
suites; every CONFIRMED defect is the ABSENCE of a gate on a cited code
path (E-1: no EditPolicy consumer in the form panel chain; E-2: unchecked
`initialize` at BatchMode.cpp:1521 + policy-blind probes; E-3: the
GetMediaBox().Height flip at SignatureManager.cpp:2118-2121 feeding
SignaturesPanel; E-4: name-only binding gate at SigningRequestRunner.cpp:
114-130; E-5: static refusal wording at SecurityController.cpp:128-145;
E-6: no identity precondition at SafeSave.cpp:207-259).

Build note for the next lane: a fresh configure (C:/msys64 toolchain this
time, vs D:/pdf/msys64 for earlier lanes) did not stage
`pdfium.dll`/`onnxruntime.dll` into the build dir until
`cmake --build build-emg --target stage_runtime_dlls` was run explicitly;
without them every test exits 0xc0000135 under ctest. One command, no
source impact.

---

## 9. Confirmed defects, ranked by user impact (fix-lane inputs)

1. **E-1 (M6): Expired/read-only document is writable via Form Builder →
   field properties panel Apply; /K scripts also execute on read-only
   docs.** Mechanism §6. Impact: expiry contract violation + unintended
   JS execution; UI affords a forbidden write with no refusal message.
2. **E-2 (M1b): Machine policy blocks the OCR download silently; batch
   reports wrong-language OCR or a non-searchable "_ocr.pdf" as
   success.** Mechanism §1b. Impact: silent wrong output under policy;
   policy whyNot never reaches any user surface; interactive/batch
   discipline divergence on the same engine call.
3. **E-3 (M2d): Signature badges paint at the wrong position on
   rot-90/270 pages** (rotation-imperfect `signatureFieldAnchors` flip
   feeding `SignaturesPanel` anchoring). Display-only; the W2B-1
   requested re-audit, closed by this lane: consumers are badges only.
4. **E-4 (M2c): Cross-version replay trap — a pre-fix-created unsigned
   field is filled post-fix with a silently corrupted /Rect** (mutation
   gate cannot see it; hash matches). Narrow window.
5. **E-5 (M5): TSA refusal under a policy-managed empty tsaUrl directs
   the user to a Preferences setting they cannot change** and never
   names the policy. Honesty gap in one message; refusal behavior
   itself correct everywhere.
6. **E-6 (M7): Two-instance stale-memory commit can silently drop the
   other instance's signature** (no destination-identity precondition
   on commit; both-displaying case fails honestly instead; sidecar flow
   guarded).

Not defects, recorded for the consolidated report: preset × AI/OCR
policy (schema-closed, §1a); mid-batch policy split (impossible for
policy keys, §1c); skip×sanitize lifetimes (§3); a11y×summary
misattribution (planes never meet, §4); sanitize undoing /Alt fixes
(designed, suggest disclosure, §4 adjacent); OCSP not policy-manageable
(deliberate bounded allowlist, §5).

## 10. Residuals / handoff notes

- Build dir `build-emg` left in place (fresh, Debug) for the next lane;
  prior lanes' build-s4s/build-w2b/build-w2b1 untouched. Untracked
  sibling-lane files carried over untouched.
- Nothing pushed; production sources untouched (analysis + docs +
  evidence only). Probes = existing suites (no new test files needed —
  every SAFE half already had a pin at this tip; every CONFIRMED defect
  is a missing gate, provable by the code paths cited above).
- E-1 is the highest-value fix and is small (two panel entry points +
  one EditPolicy consumer-list line). E-2 is two-line-shaped (check
  initialize's return in the batch worker; name the policy in the
  interactive wording). E-3 is one geometry function swap to
  `PageSpace::pageGeometry`. E-5 is one conditional clause.
