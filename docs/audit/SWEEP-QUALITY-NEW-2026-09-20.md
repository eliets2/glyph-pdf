# SWEEP-QUALITY-NEW — modularity & code-quality audit of the 2026-09-14→20 waves (2026-09-20)

- **Branch:** `feat/sweep-quality-new` (cut from `feat/parity-glm` @ `8f62a17`)
- **Lane:** END-PHASE sweep, modularity-and-quality audit-fix. Scope: the NEW commits
  (OCR-skip/SEP13 fixes, redaction-proof, forms keystroke tier, N17/N18, R24 policy layer,
  batch-presets, send-for-signing, printable summaries, accessibility).
- **Security lens:** owned by the W1 trio on the same code — this doc is modularity/quality only.
- **Fix authority honored:** files owned by active lanes (BatchMode.cpp, SigningRequest*,
  SignatureManager.cpp, RedactOperation, summary writer, PolicyController, redaction internals,
  Capability.cpp, OCRMode.cpp, ConversionManager.cpp, formjs/*, FormManager core,
  SecurityController.cpp, TextMatchFinder.*, FindReplaceDialog.*, EditController.*) are
  REPORT-ONLY here. Fixes land only in unowned new-module files, new shared modules, and tests.
- **Verdict legend:** FIXED (fixed in this lane's commits) · PROPOSED (post-sweep lane; exact
  target given) · OK (audited, no violation) · N/A (owned file — report only).

## 1. Module boundaries of the new code

Dependency law: `ui → controllers (shell) → core/engines`; never sideways or upward.
Layer map verified by include scan at the tip: `src/engines/*` never includes
`modes|ui|shell`; `src/core → engines` occurs (PageLabels, RedactionProof, Capability,
BatchPreset, SigningRequestRunner) and matches the pre-existing convention
(core may call engine public APIs).

| ID | Module | Verdict | Evidence / finding |
|----|--------|---------|--------------------|
| B1 | `src/core/SigningRequestRunner.cpp` | **N/A (VIOLATION — PROPOSED)** | Includes `shell/controllers/SecurityController.h` (line 7) for the pure static `attainedLevelLabel` (used at line 214). **core → shell/controllers is an upward dependency** — the only upward include in the whole new-code set. Fix for its owner (W1-fix): move `attainedLevelLabel` to a layer-neutral seam (e.g. a free function beside `SignOutcome` in `core/` or onto `ISignatureManager`), then drop the include. One-line comment in the file acknowledges the reuse but not the direction. |
| B2 | `src/ui/RecipientPickerDialog.cpp` | **PROPOSED** | A UI dialog performs engine-domain work: OpenSSL X.509 parsing (`d2i_X509`/`PEM_read_bio_X509`/SHA-256 fingerprint, lines 22–81). It is the only UI file in the tree that parses certificates (engines/PdfEditorEngine, engines/SignatureManager are the other two). Proposal: move `RecipientCertInfo::fromFile` into a small engine seam (`engines/CertificateInfo.{h,cpp}`) and keep the dialog's static as a one-line delegating wrapper (public/test surface unchanged). Both files are unowned — deferred here only for scope discipline; see §Fixes. |
| B3 | `SendForSigningController`, `CertEncryptController` | OK | Include `GpMainWindow.h` for `statusBar()` — this is the established shell-controller idiom (all nine controllers do). Downward deps are on engine public APIs (`ISignatureManager`, `IPdfEditorEngine`) plus `SecurityController` statics (controllers layer reading its own layer — allowed). |
| B4 | `SendForSigningController` / `SigningRequestRunner` `dynamic_cast<SignatureManager*>` | PROPOSED (owner: W1-fix) | Controllers and the core runner dynamic_cast to the CONCRETE `SignatureManager` because the `ISignatureManager` seam lacks the needed members (e.g. `takePendingAppearanceImage` is a static on the concrete class; the fill flow needs the concrete `runFillStep`). The seam exists but is incomplete. Adding the missing members to `ISignatureManager` removes three dynamic_casts; touches the interface + owned engine file → post-sweep. |
| B5 | `AccessibilityChecker` / `AccessibilityFixes` | OK | Pure engines: PoDoFo public API + `SafeSave` primitives only. `AccessibilityPanel` (modes) is well-seamed: the fix runner is INJECTED (`setFixRunner`), the shell owns the FormManager `/TU` route; no panel→FormManager direct dependency. |
| B6 | `BatchPreset` (model/codec/store) | OK | `src/core` placement correct; depends only on `Capability` + `engines/PatternRedactor.h` public static (same core→engine convention as RedactionProof). Store is dir-injectable (test seam). |
| B7 | `PolicyController` / `SupportBundle` / `NetworkTouchpoints` | OK | `core` → `core` only. Note (seam honesty, no violation): `SupportBundle::buildFromSettings` reaches `PolicyController::instance()` singleton internally despite the header's "pure" claim; making policy state an input of `SupportBundleInput` would restore purity — owner-neutral, both files unowned, deferred (behavioral pin risk outweighted benefit this sweep). |
| B8 | `CertEncryptController` | OK | Uses `IPdfEditorEngine::encryptWithCertificate`/`recipientCount` through the interface — model citizen. Multi-recipient warning exposed as a testable static. |
| B9 | `ReviewSummaryWriter` | OK (read-only file) | Pure-static, UI-free, SafeSave transactions; CommentsWidget (caller) carries the proof-pack wiring. No boundary violation found. |
| B10 | `PageSpaceTransform.h` | OK | New shared core seam; both redaction (L5/L8) and `SignatureFieldCreator` consume `PageSpace::viewerToUser` — the "one page-space law" is respected, no local re-derivation found in the new code. |
| B11 | ToolId/RibbonModel/TaskNav wiring | OK | `CertEncrypt` + `PrepareSigningRequest` follow the append-only ordinal rule; `toolIdToString`/`toolIdFromString` aliases present; ribbon rows + controller fixture enumeration extended (dbda375). Consistent with the certEncrypt precedent. |

## 2. Duplication across the new surfaces

| ID | Finding | Verdict | Evidence / fix |
|----|---------|---------|----------------|
| D1 | **Three hand-rolled versioned-JSON surfaces** — `SigningRequestModel` (magic key + schemaVersion + QSaveFile write with mkpath), `BatchPreset` (envelope schemaVersion + kind + size cap + QSaveFile store writes), `PolicyController` (schemaVersion + bounded keys, read-only). The shared pattern is the fail-closed versioned envelope + atomic QSaveFile commit; the diagnostics differ per surface and are byte-pinned by TestBatchPresets/TestSendForSigning, so a full envelope-parser unification would either change pinned messages or be so generic it carries no logic. | PARTLY FIXED | **Extracted** `src/core/VersionedJson.{h,cpp}` (new core module): canonical atomic versioned-file writer (`atomicWrite`: QSaveFile open → write → checked commit, with the exact error-string contract). `BatchPresetStore::save`/`rename` had a literally duplicated 10-line QSaveFile block (BatchPreset.cpp:815-824 vs 837-846) — both now delegate (one canonical implementation, zero behavior change; TestBatchPresets pins the messages). **PROPOSED for owners:** `SigningRequestModel::save` delegates to the same helper after its `mkpath` (keeps its Text-flag + messages — owner W1-fix); `PolicyController` read path could reuse an `VersionedJson::readObject` envelope helper when its diagnostics need to stay stable (owner W1-fix). |
| D2 | `resolve()` + `stringAt()` PoDoFo dict helpers duplicated between `AccessibilityChecker.cpp` and `AccessibilityFixes.cpp` (identical bodies; `resolve` also appears in engines/podofo/PoDoFoBackend.cpp). | FIXED | Extracted into `src/engines/PoFoDictRead.h` (new tiny shared internal header); both Accessibility engines now include it. PoDoFoBackend (not mine this sweep) left as-is — noted for post-sweep. |
| D3 | SafeSave candidate transaction hand-rolled in 13+ files (makeUniqueCandidate → ScopedFileHandleCoordination → load/mutate/save → validate → commitFileToDestination, candidate removal on every refusal). New modules (AccessibilityFixes, SignatureFieldCreator, ReviewSummaryWriter, SigningRequestRunner) follow the shape faithfully and honestly. | PROPOSED | A `SafeSave::Transaction` RAII runner (callbacks produce per-module messages; the runner owns reserve/drop/commit) would remove ~40 lines per site. NOT done here: 2 of the 4 new-code sites are owned files, and a partial extraction of only my two sites would fork the idiom instead of unifying it. Post-sweep lane should convert ALL sites in one sweep (list: FormManager, SignatureManager, RedactOperation, BatchMode, PagesMode, PoDoFoBackend, PageLabels, PagesController, FormsController, PdfEditorEngine, SigningRequestRunner, ReviewSummaryWriter, AccessibilityFixes, SignatureFieldCreator). |
| D4 | Dialog-honesty idiom drift: disclosure/error label styling. | OK | New dialogs did NOT drift against each other: `SigningRequestDialog` + `SigningProgressPanel` both use the `color: #777;` disclosure style; `RecipientPickerDialog` error red `#b00020` matches `SignatureDialog`'s existing error red. Hardcoded colors instead of `GpTheme` tokens is PRE-EXISTING tree-wide debt (EncryptionDialog, MeasureMode, FormFieldPropertiesPanel…), not introduced by these waves — recorded, no fix (would sweep owned files). |
| D5 | `SupportBundle::capIdName()` CapId→name switch. | OK | No second CapId→string mapping exists in the tree (checked); single source. |
| D6 | Versioned-JSON readers' size-cap + handshake ordering (cap → parse → magic/kind → version) is textually similar between BatchPreset::parse and SigningRequestModel::fromJson but with different message vocabularies (both pinned). | PROPOSED | Folded into the D1 owner note: a shared `VersionedJson::Envelope` (magic/schema/kind + cap) with injectable message templates can unify the order-of-checks law once both owners can re-word pins. Until then the LAW is documented here: cap first, parse second, magic/kind third, version fourth, shape last. |

## 3. God-file containment — extraction proposals (post-sweep lane; NOT executed here)

`GpMainWindow.cpp` 1643 lines; `BatchMode.cpp` 2686 lines (+ .h 376). Both are active
surfaces (owned) — proposals only, with exact ranges at tip `8f62a17`+this branch.

### GpMainWindow.cpp → target module `shell/OpenRouteCoordinator.{h,cpp}`
- Lines 1348–1510: `routeForFile` (1348–1376), `planDrop` (1377–1410), `runConversion`
  (1411–1442), `convertAndOpenOffice` (1443–1481), `convertAndOpenImages`
  (1482–1510). Dependencies: `AppContext`, `ConversionManager` (engine public API),
  `TempFileManager`, `DocumentSession`, QMessageBox/QProgressDialog. These five
  functions are one cohesive "how does a dropped/opened file become a document"
  policy; MainWindow keeps only thin delegating slots.
- Lines 1546–1629: `initUpdateChecker` → target `shell/controllers/UpdatePromptController`
  (ninth-controller pattern; owns banner label + UpdateChecker connections +
  policy-locked channel). Dependencies: `UpdateChecker`, `QSettings` (policy-gated keys
  `update/checkOnStartup`, `update/channel`), `PolicyController`.
- Lines 623–689: `recoverDocument` → join `TempFileManager`-adjacent
  `core/DocumentRecovery` helper (pure file moves + dialog-free logic), MainWindow keeps
  the user prompt.
- Lines 104–610 (constructor, ~507 lines): split into `buildShell()`,
  `buildWelcomeConnections()`, `buildModeRegistry()` private methods first (no new
  module), THEN extract the welcome-intent block (731–776 `startWelcomeTask`/
  `applyWelcomeTask`) into `shell/WelcomeTaskRouter`.

### BatchMode.cpp → three extractions, in this order
1. **`modes/BatchPresetPanel.{h,cpp}`** — lines 2164–2686 region (`buildPresetPanel`
   2184, `refreshPresetPicker` 2249, `onPresetSelected` 2296,
   `presetStepsDisplayText` 2320, `presetRunBlocker` 2344,
   `onSaveAsPresetClicked` 2455, `onRenamePresetClicked` 2455ff,
   `onDeletePresetClicked` 2504, the `*ForTest` seams 2525–2570). Dependencies:
   `BatchPreset`/`BatchPresetStore` (core), `batchPresetStepCapability`,
   `GpTheme`, QLineEdit/QComboBox wiring. BatchMode keeps composition + the
   `selectPresetForTest` forwarding shims so TestBatchPresets is untouched.
2. **`modes/BatchPresetRun.{h,cpp}`** — lines 981–1170 (the per-file preset candidate
   chain): free functions `runPresetMutatingStep` (already file-local at ~1016),
   `runPresetCheckStep`, and the per-file chain driver. Dependencies:
   `PdfEditorEngine` public API, `BatchPresetStep`, `veraPDF` probe runner,
   `QMutex` (engine serialization). Pure engine-side; zero UI — easiest, do first.
3. **`modes/HotFolderController.{h,cpp}`** — lines 788–907 (`hotFileKey` 792,
   `buildHotFolderSection` 797, `onToggleHotFolder` 830, `onHotFolderChanged` 883)
   owning the QFileSystemWatcher + debounce. Dependencies: QFileSystemWatcher,
   `resolveOutputPath` callback, settings keys.

## 4. Naming / consistency / standards

| ID | Check | Verdict |
|----|-------|---------|
| N1 | Commit format (`type(scope): summary`, lowercase, present tense) | OK — all window commits comply (`feat(a11y):`, `feat(signing):`, `fix(redaction):`, `test(ribbon):`, `docs(audit):`…). Legacy `fix(engine) SEP13 M1:` style keeps the type(scope) shape; fine. |
| N2 | File placement (controllers in `shell/controllers/`, engines in `src/engines/`, panels in `src/modes/`, dialogs in `src/ui/`, models/stores in `src/core/`) | OK — every new module sits in the layer its name claims; no misplacements. |
| N3 | Test idiom (settings isolation, offscreen, RUN_SERIAL class) | OK with one gap → FIXED: all new suites register `QT_QPA_PLATFORM=offscreen` + TIMEOUT + LABELS; settings isolation via hermetic INI (`TestPolicyController`, `TestSupportBundle`, `TestBatchPresets` dir injection). **Gap:** `TestSendForSigning` counts the SHARED `%TEMP%/glyphpdf-candidates` dir (`leftoverCandidates`, lines 180/731) but was not RUN_SERIAL — under parallel ctest any concurrent SafeSave suite perturbs the count. Same FU-2 seam class as TestPrintableSummary/TestAccessibilityFixes (process-global commit fault + shared candidate dir). FIXED here (test-properties only). |
| N4 | Stray TODO/FIXME/XXX/HACK in the new files | OK — zero hits across all 33 new files. Dead code: none found (AccessibilityFixes' `SetFieldTu` branch that reserves-then-refuses a candidate is a deliberate honest guard, documented, not dead). |
| N5 | SPDX headers | OK — every new file carries `SPDX-License-Identifier: Apache-2.0`; tests carry MIT where that is the tests convention (TestSendForSigning) — matches the existing tests/ split. |
| N6 | Namespace discipline | OK — `gp::` for new core/engines/shell modules; `SigningRequestModel`/`SigningRequestRunner` namespace-free matches the AnnotationSerializer/DocumentSession idiom (comment documents the choice). |

## 5. Missing pins (most load-bearing gaps)

| ID | Gap | Verdict |
|----|-----|---------|
| P1 | `PolicyController::defaultPolicyPath()` env seam `GLYPHPDF_POLICY_PATH` — the production default path AND the designated test seam — is referenced by NO test. | FIXED: pin added to TestPolicyController (qputenv → `defaultPolicyPath()` returns it; clear → machine default shape). Test-only; PolicyController.cpp untouched. |
| P2 | `TestSupportBundle` is not hermetic against the MACHINE's real `%GenericDataLocation%/GlyphPDF/policy.json`: `buildFromSettings` → `ensureLoaded()` after `resetForTesting()` reads whatever policy exists on the runner machine (bundle policy section becomes machine-dependent). | FIXED: `initTestCase` now points `GLYPHPDF_POLICY_PATH` at a controlled non-existent path inside the suite temp dir (pinned deterministic `NoPolicy` section), restoring hermeticity without touching production code. |
| P3 | The composed `/TU` route — `readFieldRequiredFlag` + `FormManager::setFieldMetadata` preserving the Required bit — is only half-pinned: TestAccessibilityFixes pins the reader, TestAccessibilityPanel pins via an injected FAKE runner; no test exercises the real composition end to end. | PROPOSED (pin drafted in §Fixes notes; the composition lives in GpMainWindow.cpp — owned — so the honest pin is a panel+real-runner integration test; left to the a11y owner lane to avoid pinning a moving owned file). |
| P4 | `VersionedJson::atomicWrite` (new module) | Covered transitively by TestBatchPresets save/rename/rename-validate pins (delegation keeps every message identical); dedicated negative pins (unwritable dir) already exist for the store path. OK after migration. |
| P5 | `NetworkTouchpoints::enumerate` state derivation | OK — pinned by TestNetworkDisclosure (found during enumeration; not in the new-file list because it predates/parallel-tracks the wave). |

## Suites run before/after (this lane's contract: zero behavior change)

Filled in §Fixes-log at the bottom after the runs (kept in .context/sweep-quality-new-wip.md too).

## Fixes-log

- `0c1aeab` refactor(core,engines) — D1/D2/N3: `VersionedJson::atomicWrite` (new core
  module) behind `BatchPresetStore::save`/`rename` (identical message contract);
  `PoFoDictRead.h` shared `resolve`/`stringAt` for AccessibilityChecker/AccessibilityFixes;
  TestSendForSigning `RUN_SERIAL` registration.
- `288c281` test(policy,support) — P1/P2 pins + this doc.

### Suites before → after (offscreen, `-o txt`, serial; raw outputs in `.context/results/`)

| Suite | Before | After |
|-------|--------|-------|
| TestBatchPresets | 14 passed, 0 failed | 14 passed, 0 failed |
| TestAccessibilityChecker | 10 / 0 | 10 / 0 |
| TestAccessibilityFixes | 9 / 0 | 9 / 0 |
| TestSupportBundle | 9 / 0 | 9 / 0 |
| TestPolicyController | 12 / 0 | **13** / 0 (new env-seam pin) |
| TestSendForSigning | 12 / 0 | 12 / 0 |
| TestAccessibilityPanel (adjacent) | — | 7 / 0 |
| TestBatchMode (adjacent, store consumer) | — | 17 / 0 |

Zero behavior change on every pre-existing suite; the only delta is the added P1 pin.
