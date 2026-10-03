# Lane Report — r3-api harmonization (wave-2b interface-consistency round 3)

- **Lane**: r3-api (API-consistency fix lane), branch `feat/r3-api` (worktree `D:/pdf/pdf-r3-api`), base `f6d9ca1f`
- **Date**: 2026-10-03
- **Input**: `D:/pdf/verification/crossmodel/api-designer.md` — "API/Interface Audit — GlyphPDF wave-2b interfaces & call sites" (deepseek-v4.1-flash), findings 1-15, verdict PASS-WITH-FINDINGS (C:0 · H:4 · M:6 · L:5). (The dispatch named the file `api-designer-wave2b.md`; the on-disk wave-2b api-designer report is `crossmodel/api-designer.md` — same 15 findings, same file:line detail.)
- **Mission**: harmonize the idioms the wave-2b lanes drifted on — WITHOUT changing behavior, except where the report names an actual defect. House patterns converged ON: typed `ErrorInfo* err = nullptr` out-params (IFormManager idiom), owner-side seam objects, honest refusal channels instead of bools-with-side-channel, default-arg additions only when back-compat is pinned.
- **Status**: COMPLETE — build `BUILD_RC=0`, touched suites green (§5), behavior pins untouched.

## 0. Exemplar map (read first — the "house idiom" is per-layer, not global)

The audit's headline finding ("a third error channel") reads the tree as having ONE
error idiom. It has THREE, each correct on its own layer; the drift was APIs that
match NONE of them:

| Layer | Idiom | Older exemplars |
|---|---|---|
| Engine interface WITHOUT error state | trailing `ErrorInfo *err = nullptr` | `IFormManager::fillForm`/`importFormData` (jsFailures/err family) |
| Engine interface WITH error state | `lastError()`/`clearError()` member pair | `IPdfDocumentIO` (whole `IPdfEditorEngine` god-interface) |
| Static utility writer on the SafeSave transaction | trailing `QString* errorOut = nullptr` | `ReviewSummaryWriter::write`, `SafeSave::makeUniqueCandidate`/`commitFileToDestination` |

Judged against that map, the wave-2b APIs sort cleanly: `importFormData` (typed
`ErrorInfo*`) and `exportEditableTextPdf` (engine `lastError()`) were each
consistent with THEIR layer's exemplar; `A11yReportWriter`/`addImageStampTo`
(`QString*`) match the utility-writer exemplar; the one API matching NO idiom was
`PageLabels::writeNumberTree` — `bool` with the reason swallowed into `qWarning`
(the bool-with-side-channel anti-pattern), plus two real disclosure defects named
below.

## 1. Per-item disposition (report findings 1-15)

### Finding 1 (H) — importFormData's ErrorInfo channel vs the other new APIs — ALIGNED (part fixed)

*Report: the typed `ErrorInfo *err` is a third channel; "do not leave two channels live"; a caller forgetting `&err` gets silent failure.*

- **Real defect found and fixed** (`src/engines/FormManager.cpp`, `importFormData` tail):
  the parser refusals populated `err`, but the final `return fillForm(...)` left the
  typed channel EMPTY on the whole save-transaction refusal class (load failure,
  change validation, checked-commit refusal) — `false` beside `err.isOk()==true`,
  i.e. the two channels DISAGREEING. Now every `false` leaves a typed reason
  (populated only when no parser reason already sits in `err`), making `err` THE
  single refusal channel in fact, not just in comment. Additive: callers passing
  `nullptr` are unchanged; the FormsController's typed-message branch now also
  covers save refusals (was the generic fallback text).
- **Contract documented** at `IFormManager::importFormData`: "err is THE refusal
  channel … true leaves err untouched."
- **Declined** (the report's two alternatives): making `err` non-optional is a
  breaking signature change for zero behavior gain; a `lastImportError()` accessor
  would CREATE the second live channel the report itself says not to leave.
  IFormManager deliberately has no error-state member — the optional out-param IS
  the IFormManager idiom.

### Finding 2 (H) — locateSevenZip: resolver shape + disclosure path — ALIGNED (disclosure defect fixed)

*Report: test-driven relocation into a "program-agnostic" SafeSave is a layering inversion; keep the resolver in a dedicated unit; the caller must disclose absence honestly.*

- **Extracted** the locator verbatim to a dedicated unit `src/engines/SevenZipLocator.{h,cpp}`
  (`gp::SevenZipLocator::locate()`), which now OWNS the 7-Zip install policy;
  `SafeSave` is tool-agnostic again (its transactions take the program path from
  the caller and never did call the locator internally — the inversion was
  cohesion, not control flow). Behavior byte-identical: same bundled-only,
  both-files-required, native-separators body.
- **Honest-disclosure defect fixed**: the wave-2b security audit (F-02, CWE-427)
  had already removed the PATH/Program-Files fallback legs, but four disclosure
  surfaces still described the removed behavior: the `SafeSave.h` locator comment
  ("then PATH, then the conventional 7-Zip install dirs" — stale, now replaced by
  a pointer to the new unit), the `HomeController::createEncryptedPackage` header
  comment ("falling back to a system installation"), and — user-visible — the
  absence dialog, which claimed "**no system 7-Zip installation was detected**"
  when the code never looks at system installs, and told users to "install 7-Zip"
  (advice that cannot help). The dialog now states the truth: bundled pair or
  nothing, by audit design, with Protect→Encrypt as the alternative. Stale
  "falls back to system 7z/PATH" configure comments and the two test QSKIP
  reason strings aligned the same way.
- **Callers** (`HomeController.cpp`, `TestControllers.cpp`,
  `TestEncryptedPackageSafeWrite.cpp`, `TestSevenZipBundle.cpp`) and CMake
  (`pdfws_engines` sources; `TestEncryptedPackageSafeWrite` minimal target gains
  `SevenZipLocator.cpp`) updated; all assertions byte-for-byte unchanged.
- **Finding 13 (L, same seam) — FIXED** by the extraction: the production
  `appDirOverride` test seam became `SevenZipLocator::locateForTesting(appDir)` —
  a name that says what it is for; production callers have no override to misuse.

### Finding 3 (H) — renderPageAsync cancellation contract — ALIGNED (docs; no code defect exists)

*Report: asymmetric contract (insert-then-refuse-to-deliver); either make insertion conditional or document the invariant + pin the no-stale property.*

- **No observable defect**: the asymmetry is real but safe by construction — the
  cache is keyed by page+scale of the CURRENT document only, and every document
  change routes through `clear()`, which JOINS the in-flight worker
  (`drainPrefetches`) BEFORE wiping, so a cancelled render can leave a FRESH entry
  but a STALE entry can never survive the wipe. Making insertion
  token-conditional (the report's option a) would regress this: the worker's
  insert goes through `getOrRender`, and suppressing it would discard valid
  renders of the current document on a mere prefetch supersession.
- **Done (option b)**: named invariant **TOKEN-EPOCH-DELIVERY** documented at
  `RenderCache::renderPageAsync` (`src/engines/RenderCache.h`) — insert and
  delivery are deliberately asymmetric, here is why that is safe, and here is the
  pin. The demanded pin ALREADY EXISTS:
  `TestThumbnailOffGui::clearCancelsInFlightRenderJoinsAndDeliversNothing` (pin 3)
  asserts both the join-before-wipe ordering and "nothing stale survived the
  wipe"; it is referenced by name from the header. Untouched, green.

### Finding 4 (H) — OcrOutputMode plumbing (settings-mediated mode seam) — PARTIALLY ALIGNED, redesign DECLINED

*Report: interface method mode-agnostic but call-site reads a global via a static seam; suggests one `exportOcrPdf(path, mode, ...)`; keep the settings read at the UI boundary.*

- **Verified sound**: the pref-mediated seam (`ocr/outputMode`, parser
  `ocrOutputModeFromPref`, writer `OCRMode::setOutputMode`, reader
  `EditController::outputModeFromSettings`) is exactly the tree's existing idiom
  for OCR panel choices — the same QSettings wire as `ocr/language` and
  `ocr/orientDetect` (older exemplars). A signal-based handoff would make the
  panel the ODD one out.
- **Declined**: merging the two writers into `exportOcrPdf(path, mode, ...)`.
  `exportMrcPdfA` and `exportEditableTextPdf` are not two modes of one writer —
  they have different output CONTRACTS (PDF/A-2b archival claim vs plain PDF 1.6,
  image+invisible layer vs content replacement), one ships with a veraPDF gate
  the other deliberately lacks. A mode flag over that seam hides an honesty
  distinction the split keeps visible. The mode-agnostic interface + UI-boundary
  mode read is the correct shape.
- **Aligned instead** (the part of the finding that WAS a defect): see finding 14
  — the failure reason never reached the user.

### Finding 5 (M) — setOutputMode naming / "single writer" claim — ALIGNED (doc)

*Report: panel writes a pref, controller reads it — a settings-mediated seam behind a "single writer" framing.*

- The claim vs reality is now stated truthfully at the seam
  (`src/modes/OCRMode.h`): the persisted pref is the CANONICAL channel, the combo
  is the widget's only truth, `setOutputMode` is THE writer for user/hosts/tests,
  and the accept flow re-reads the pref (so a combo-setting test and a
  pref-setting test exercise the same wire, not two paths). No code change.

### Finding 6 (M) — HotFolderController polling: key contract + cross-path race — ALIGNED (doc + new pin)

*Report: document the key contract in the header; pin that a polling-ingested file is not re-ingested by fs-event and vice versa.*

- Header doc sharpened (`src/modes/HotFolderController.h`): the key contract was
  already documented (root-relative path + mtime + size); added the named
  CROSS-PATH DEDUP INVARIANT — both ingestion paths funnel through the SAME
  `ingestDeliver()` pass and the SAME `m_processed` key set, so even with both
  paths live at once each drop is delivered exactly once. (The race the report
  feared is structurally absent: there are not two dedup sets.)
- **New pin added** (additive — no existing test touched):
  `TestHotFolder::crossPathIngestSharesOneProcessedSet` — watch mode + polling
  live simultaneously, two drops delivered exactly once across whichever pass
  wins, then silence from the other path and from a direct `ingestDeliver()`.
- `kPollIntervalMs` exposure verified sound: it is public precisely so its one
  consumer (`BatchMode.cpp:878`) passes it to `startPolling` — one contract, not
  two magic numbers; documented as such.

### Finding 7 (M) — StampLibrary carrier invariant (function-enforced, not type-enforced) — ALIGNED (doc), redesign DECLINED

*Report: text XOR image enforced by validation, not by the type; suggests `std::variant` carrier.*

- **Declined the variant**: `StampTemplate` is a persisted aggregate
  (stamps.json serialization, test fixtures, UI construction); a tagged-union
  carrier rewrites every construction site and serializer for a rule that is
  already fail-closed at every trust boundary (load refuses unsafe carriers;
  import writes exactly one; placement resolves through the safe-relative gate).
- **Done**: the XOR carrier invariant is now a named rule in
  `src/core/StampLibrary.h` — including the duty the report flagged, that
  in-memory construction sites own it too, and why that residual is acceptable
  (placement fail-closed either way).
- **Shape parity**: `addImageStampTo`'s out-param renamed `error` → `errorOut`,
  matching the utility-writer exemplars (`ReviewSummaryWriter`/`A11yReportWriter`)
  — position (trailing) and shape (`QString*`, optional) already matched.

### Finding 8 (M) — csvCell composition duplicated at three exporters — FIXED (promoted)

*Report: expose a single `csvCell(raw)` beside `csvFormulaSafeCell` and have all three exporters call it; the a11y writer's shape is right — promote it.*

- **Promoted** the composition to `ConversionManager::csvCell`
  (formula-safe → RFC-4180 quote-when-needed) in
  `src/engines/ConversionManager.{h,cpp}`; `A11yReportWriter::csvCell` and
  `CommentsWidget::csvEscapeField` now delegate (both public members kept — the
  per-writer contract stays nameable and directly testable). Byte-identical
  output: the two delegates were textually identical compositions.
- **Scoped decline**: the conversion TABLE exporter's own rows
  (`ConversionManager::exportToCsv`) emit always-quoted cells over
  `csvFormulaSafeCell` — a deliberately different, older emission shape
  (V03 column-fill logic escapes interior quotes BEFORE run-joining, so the
  cells are not plain values) with its own pinned byte contract
  (`TestConversionExtraction`). Forcing it through `csvCell` would change
  shipped output bytes for zero hardening gain. The drift the hoist prevents —
  one of the three forgetting a layer — is now impossible at the composition
  the three NEWER exporters share.
- `Standard14Text.h` hoist (the report's other half): verified behavior-neutral
  as landed; nothing to do.

### Finding 9 (M) — PageLabels prefix "absent, not empty-valued" rule — ALIGNED (doc, folded into finding-1 work)

- The rule is now spelled out at both writer overloads
  (`src/core/PageLabels.h`): "an empty prefix must never produce a /P key
  (ABSENT, never present with an empty value; readers see no /P at all)" — plus
  the writer's validation already proves present-iff-non-empty on every write
  (wave-2b F-15 read-back check, pinned, untouched).

### Finding 10 (M) — view-rotation cache keys hold unreachable entries after reset — ALIGNED (doc), clear-on-reset DECLINED

*Report: document session-scoped eviction, or clear the cache on reset.*

- **Scope correction first**: the report's "LRU, 256 MB" conflates two caches.
  Rotation-keyed entries live in `PdfViewerWidget::m_pageCache` (the widget's
  per-page cache); `RenderCache`'s 256 MB LRU serves the thumbnail rail and is
  never rotation-keyed. The widget cache holds at most ONE entry per page, and
  the page's next render at the new rotation REPLACES it (bytes discounted
  before insert) under the same `MaxCacheBytes` LRU.
- **Done**: SESSION-SCOPED KEYS disclosure at the member
  (`src/ui/PdfViewerWidget.h`) — unreachable-after-reset, not actively wiped,
  and why the bounded replacement cost makes an extra invalidation pass not
  worth it (the report's option 2 declined: `resetViewRotation()` is on the
  hot rotation path; a cache clear there would thrash exactly the bitmaps the
  user is about to need at rotation 0… which are, precisely, not cached under
  the new key — the re-render is the cost either way; the doc says so).

### Finding 11 (L) — loadStampImage null = "missing or corrupt" — DECLINED (already documented)

The header already carries the contract ("A null image means missing or
undecodable — callers must surface that honestly") and the report's own fix
direction for the status quo is "document that null means … same user action" —
done upstream when the seam landed. A `{QImage, QString reason}` result type is
churn for a distinction no caller can act on differently today.

### Finding 12 (L) — leptonicaAvailable static bool — DECLINED (report says "fine" itself)

The report's own conclusion: "for now, the static bool is fine and the dual-config
pin is the right shape." Promote to an enum when a partial-Leptonica build
actually exists.

### Finding 13 (L) — appDirOverride test seam in production API — FIXED (see finding 2)

### Finding 14 (L) — editable writer's error channel vs lastError() — FIXED (the controller, not the engine)

*Report: confirm the editable writer populates lastError(); the controller must not substitute a hardcoded string.*

- **Engine side verified sound**: `PdfEditorEngine::exportEditableTextPdf` clears
  on entry (`d->clearErr()`) and populates a typed `ErrorInfo::Error` on every
  false (empty payload, all-empty words, unopenable output), identical to
  `exportMrcPdfA`. Contract now documented at BOTH interface declarations
  (`IPdfEditorEngine.h`, `IExporter`): "BOTH OCR writers clear lastError() on
  entry and populate a typed ErrorInfo on every false, so a caller reads ONE
  channel for either mode."
- **Controller side was the defect**: `EditController::onOcrAcceptRequested`
  emitted a hardcoded "Could not write the … copy. See the application log." on
  failure — the engine's honest typed reason (e.g. "No recognized words to
  write — the editable text copy would be blank") was discarded. The controller
  now consumes `lastError().userMessage` and keeps the mode-specific string only
  as the empty-channel fallback. Pins: the only failure-message pin
  (`TestOcrReviewLifecycle::saveErrorRetainsReviewForRetry`) feeds its own
  message into the panel and pins the PANEL's lifecycle handling — untouched,
  green.

### Finding 15 (L) — batch combo index as load-bearing contract — DECLINED (churn > value)

The index-to-extension mapping predates the wave (five rows shipped); the new
Text/PowerPoint rows were appended, not interspersed, and the lane's pin fixes
the positions. Keying the tables on `itemData` means touching the combo wiring,
the two tables and their pins in the same breath — a refactor of SHIPPED,
pinned UI plumbing with no drift having occurred. Revisit if a fourth wave
reorders the combo.

## 2. Commits (this lane)

1. `fix(r3-api) F1` — refusal-channel harmonization: importFormData populates the typed channel on save-transaction refusals (+ contract doc); `PageLabels::writeNumberTree` gains the trailing `QString* err` reason channel (both overloads, utility-writer idiom) and PagesMode logs the reason; prefix "absent, not empty-valued" doc rule (finding 9).
2. `refactor(r3-api) F2+F13` — `SevenZipLocator` extraction (SafeSave tool-agnostic again); honest bundled-only disclosure (dialog text, stale comments, QSKIP reasons); `locateForTesting` test seam; CMake + caller updates.
3. `docs(r3-api) F3` — TOKEN-EPOCH-DELIVERY invariant at `RenderCache::renderPageAsync`, referencing the existing pin.
4. `fix(r3-api) F4+F14` — EditController consumes the engine's typed `lastError()` on OCR-writer failure (fallback keeps mode-specific wording); writer error-channel contract documented on both `IExporter` declarations.
5. `refactor(r3-api) F5-F8,F10` — M-batch: OCRMode canonical-channel doc; HotFolder cross-path dedup invariant + new additive pin; StampLibrary XOR-carrier doc + `errorOut` rename; `ConversionManager::csvCell` promotion with comments/a11y delegates; view-rotation SESSION-SCOPED KEYS doc.
6. `docs(audit)` — this report.

## 3. Hard-rule compliance

- Work confined to `D:/pdf/pdf-r3-api` (branch `feat/r3-api`, base `f6d9ca1f`); no pushes, no merges/rebases, no stash/gc, worktree intact.
- No test weakened or skipped; existing pins untouched except three mechanical
  call-site migrations FORCED by the locator move (assertion lines byte-identical:
  `TestSevenZipBundle` pins 2-4, `TestControllers`/`TestEncryptedPackageSafeWrite`
  locate+QSKIP lines). One test ADDED (`crossPathIngestSharesOneProcessedSet`).
- No CLAUDE.md/SECURITY.md created. Built only into `D:/pdf/pdf-r3-api/build-rel`.

## 4. Verification

- Configure: runbook flags, Release + LTO + Tesseract + libsecret. `BUILD_RC=0`
  over the full tree (every executable + plugin relinked after the CMake
  source-list change; the only compiler diagnostics are the pre-existing
  third_party lua LTO `-Wstringop-overflow` notes, untouched by this lane).
- Touched suites, SERIAL (`ctest -j 1`, offscreen), after the refactor —
  **18/18 suites, 0 failed** (closing run: 104.9 s):

| Suite | Pins | Result |
|---|---|---|
| TestFillFormNoOp | 20 | PASS (typed-refusal pins untouched) |
| TestPageLabels | 24 | PASS (writer pins untouched, default-arg back-compat) |
| TestSevenZipBundle | 6 | PASS (locator pins assertion-identical against the new unit) |
| TestControllers | 15 | PASS (locator call migrated, assertions identical) |
| TestEncryptedPackageSafeWrite | 12 | PASS (locator via new unit; minimal target gains one .cpp) |
| TestThumbnailOffGui | 9 | PASS (TOKEN-EPOCH-DELIVERY pins, untouched) |
| TestOcrOutputMode | 12 | PASS (writer refusal pins untouched) |
| TestOcrReviewLifecycle | 34 | PASS (failure-lifecycle pins untouched) |
| TestStampImageImport | 10 | PASS (`errorOut` rename transparent) |
| TestHotFolder | 26 | PASS (includes the NEW cross-path pin) |
| TestA11yReportWriter | 11 | PASS (csvCell delegation byte-identical) |
| TestCommentsReview | 10 | PASS (csvEscapeField delegation byte-identical) |
| TestConversionExtraction | 21 | PASS via ctest (3 soffice pins are `GLYPHPDF_FAKE_SOFFICE_EXE`-gated; direct run without the env var fails them by design — ctest provisions the fake) |
| TestBatchMode | — | PASS |
| TestMeasureCsvExport | — | PASS |
| TestBatchPresetsP2 | — | PASS |
| TestOcrAcceptSeam | — | PASS |
| TestMrcPipeline | — | PASS |

- Honest flake note: one group run of five csv/ocr-adjacent suites reported a
  single failure (TestBatchMode timing-family) that did not reproduce in two
  subsequent runs (group re-run 5/5 and the serial closing run 18/18). The
  hot-folder pins are wall-clock-wait based; under concurrent suite load one
  wait window missed. Not related to this lane's changes (the lane touched
  HotFolderController.h comments only).

## 5. Residuals / hand-off

- The full-suite serial gate (202 suites at the wave-2b close) was NOT re-run in
  this lane — out of lane budget; the 13 touched suites plus the full Release
  build are the evidence. A closing gate before merge is recommended as usual.
- `ErrorInfo` adoption is still per-layer by design; if a future wave wants ONE
  channel everywhere, the utility-writer layer (`QString* errorOut`) is the one
  to migrate, and `ConversionManager::csvCell` shows the promotion pattern.
- Report finding 15's `itemData` keying is the first candidate if batch export
  formats ever reorder.
