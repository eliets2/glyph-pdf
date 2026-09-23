# SWEEP-W3 ARCHITECT AUDIT — 2026-09-20 (solution-architect lane: modularity)

Lane: solution-architect (W3 ponytail sweep). Branch `feat/sweep-w3-arch` @ base `b17106a`
(== `feat/parity-glm` tip). REVIEW + DESIGN ONLY: no production edits; deliverables are this
audit and the validated refactor roadmap. Handoff: `.context/sweep-w3-arch-wip.md`.

Prior inputs built on (not re-done): `SWEEP-QUALITY-NEW-2026-09-20.md` (boundaries B1–B11,
duplication D1–D6, god-file proposals), `SWEEP-W3-ARCHAEOLOGIST-2026-09-20.md` (dead-weight
evidence base + load-bearing watch list), `SWEEP-W2-TESTING-2026-09-20.md` (suite layering,
FU-2 candidate-dir law, RESOURCE_LOCK map). Both W3 sibling docs live only on
`feat/sweep-w3-archaeo` (288c281 / 7985a10); they were read from git, not merged here.

## 0. Method and evidence base (machine-checked, not grep-guessed)

- Fresh configure at this tip: `cmake -B build-arch -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (MSYS2 UCRT64) — configure log verifies
  **"Using vendored podofo 1.1.0 from …/third_party/podofo/install"**; 581 compile entries.
- Include graph derived from `build-arch/compile_commands.json` (per-file `-I` sets honored;
  quote-includes resolved includer-dir-first, then include dirs, then unique-basename): all
  `src/**` compiled units scanned, headers transitively closed — **359 files scanned, 204
  headers reachable**. Analyzer + raw output kept in `.context/sweep-w3-arch-scratch/`
  (`incgraph.py`, `inccycles.py`, `incgraph-out.txt`, `cycles-out.txt`).
- Cycles: Tarjan SCC over the full file-level include graph — **0 cyclic clusters** (no SCC
  larger than 1 file). The tree is a DAG today; the audit below is about *direction discipline
  and seams*, not about cycle-breaking.
- Symbol-level verification: every cross-layer include was checked for the symbols it is
  actually used for (three of them are dead includes — §2.2). No build was run: the include
  evidence is from the generated compiler argument lists, which is the same ground the
  compiler sees. Honest boundary: dead-include deletions still require one compile check when
  executed (fix-lane step, §2.2).

## 1. Module map and the layer law

Total `src/` ≈ 88k LOC (engines counted recursively). Layers as they exist on disk:

| Layer (top → bottom) | Files | LOC | Contents |
|---|---:|---:|---|
| `src/app` | 3 | 269 | main(), Bootstrapper (composition root wiring) |
| `src/GpMainWindow.{h,cpp}` (src-root) | 2 | 1,905 | the main window class `gp::MainWindow` — **loose at src root, in no layer directory** |
| `src/shell/controllers` | 20 | 6,469 | the ten IToolController controllers |
| `src/shell` | 21 | 3,044 | MenuBar, Ribbon, ModeStrip, StatusBar, Sidebar, TaskNav, ScreenNav, RibbonModel, ToolRegistry, TaskStateSync, EditPolicy |
| `src/modes` | 35 | 14,160 | mode panels incl. BatchMode.cpp (2,685) |
| `src/ui` | 80 | 16,359 | dialogs, viewer widget, canvases, panels |
| `src/commands` | 26 | 1,978 | undo commands + CheckedHistory |
| `src/core` (+`core/interfaces`) | 47+10 | 9,913+863 | models, stores, runners, policy + the 10 I* seams |
| `src/docmodel` | 9 | 338 | document model leaf |
| `src/engines` | 42+subdirs | 31,036 | PoDoFo/Pdfium/QPdf backends, SignatureManager, FormManager, ConversionManager, OCR, SafeSave |
| `src/pdfws_djot` | 10 | 1,543 | djot converter leaf |
| `src/util` | 7 | 278 | leaf |

**The layer law** (as the code and quality-new §1 state it, now measured):

```
app → src-root(GpMainWindow) → shell/controllers → shell → modes → ui → commands → core → engines
                                                              (core→engines is the sanctioned
                                                               "core may call engine public APIs"
                                                               convention, 9 edges — all healthy)
```

Downward includes are legal; upward and (between peers) sideways-toward-shell are violations.
Measured matrix (unique include edges between layers, intra-layer elided) is in
`.context/sweep-w3-arch-scratch/incgraph-out.txt`. Healthy bulk: ui→core 30, ui→engines 16,
modes→engines 26, modes→core 19, controllers→core 26, controllers→core/interfaces 23,
controllers→ui 32, controllers→commands 19, engines→core/interfaces 17, engines→core 16,
commands→core/interfaces 22, commands→engines 22.

## 2. Violations — every upward/sideways edge named (13 edges / 12 files)

### 2.1 Live upward dependencies (10 edges in 10 files)

| # | Edge | Evidence | What is actually used | Severity |
|---|---|---|---|---|
| V1 | **core → shell/controllers** | `src/core/SigningRequestRunner.cpp:7` → `shell/controllers/SecurityController.h` | pure static `SecurityController::attainedLevelLabel` at `:266` (decl `SecurityController.h:85`) | **the B1 finding, confirmed at this tip** (quality-new reported use at :214; drifted). Design in §5 |
| V2 | **core → ui** | `src/core/NetworkTouchpoints.cpp:17` → `ui/OcspConsentDialog.h` | **nothing — zero symbols** (see §2.2) | dead include, trivial fix |
| V3 | **commands → ui** | `src/commands/SetOutlineCommand.h:9` → `ui/PdfViewerWidget.h` | `QPointer<PdfViewerWidget> m_viewer` member (`:65`, ctor `:27`) — an undo command holding a widget pointer | real coupling; undo command should observe a signal/interface, not the widget |
| V4 | **ui → src-root** | `src/ui/PdfViewerWidget.cpp:4` → `GpMainWindow.h` | `qobject_cast<gp::MainWindow*>` at `:1371` (statusBar + openDocument) and `:1626` (statusBar) | real child→parent reach-up |
| V5 | **ui → src-root** | `src/ui/PreferencesDialog.cpp:8` → `GpMainWindow.h` | casts at `:700` (autosave restart via `appContext()`) and `:724` (support-bundle open-document count/capabilities) | real reach-up into app context |
| V6 | **modes → src-root** | `src/modes/PdfAValidationPanel.cpp:3` → `GpMainWindow.h` | parent-walk cast at `:56` → `pdfViewer()->goToPage()` | real reach-up |
| V7 | **shell → src-root** | `src/shell/MenuBar.cpp:5` → `GpMainWindow.h` | casts at `:176`, `:491` (recent-files refresh) | reach-up inside the shell itself |
| V8 | **shell → src-root** | `src/shell/StatusBar.cpp:20` → `GpMainWindow.h` | cast at `:210` → `pdfViewer()` for page-size facts | real reach-up |
| V9 | **ui → modes** | `src/ui/OcrScanCanvas.h:7` → `modes/OcrReviewSession.h`; `OcrScanCanvas.cpp:6` → `modes/OcrConfidence.h` | the canvas consumes the OCR review-session types | real type dependency: a ui widget is built on a modes-layer model. Fix by relocation, not indirection: `OcrScanCanvas` belongs beside the session (modes), or `OcrReviewSession` belongs in core/docmodel |
| V10 | **modes → shell** | `src/modes/PagesMode.cpp:31` → `shell/EditPolicy.h` | `EditPolicy` — a pure policy header parked in `shell/` | placement smell, not a runtime inversion: EditPolicy is layer-neutral policy; it should live in core (then the edge becomes downward). Same for `shell → modes` below |

### 2.2 Dead upward includes (3 edges) — Rung-1 deletions, run a compile check each

| Evidence | Note |
|---|---|
| `src/core/NetworkTouchpoints.cpp:17` → `ui/OcspConsentDialog.h` | zero `OcspConsent*`/`OcspConsentDecision` symbol use in the file (the real user of the pure `OcspConsent` class is `SecurityController.cpp:223-228`, a legal downward call). Deleting it removes the **core → ui** edge class entirely. Note the header itself mixes a QDialog with the pure `OcspConsent` policy class (`OcspConsentDialog.h:37,67`) — splitting the pure half into core is the durable shape (B2-family, design-only note) |
| `src/ui/PdfViewerWidget.cpp:5` → `shell/StatusBar.h` | zero `StatusBar` symbol use (the `statusBar()` calls at :1373/:1628 are `QMainWindow::statusBar()` reached via the MainWindow cast, V4). Removes the **ui → shell** edge class entirely |
| `src/shell/Sidebar.cpp:3` → `GpMainWindow.h` | zero `MainWindow` symbol use. Shrinks shell→src-root to the two live reach-ups |

### 2.3 Cross-cutting seam breaches (concrete types where an I* seam exists)

| ID | Breach | Evidence |
|---|---|---|
| S1 | `ISignatureManager` exists, but the CONCRETE `SignatureManager` is used through 6 non-wiring files in 3 layers | `core/SigningRequestRunner.h:134` (`runFillStep(SignatureManager&, …)` — the core header itself names the concrete type) + `SigningRequestRunner.cpp:5`; `SecurityController.cpp:268` (`dynamic_cast<SignatureManager*>`) + statics `:601,:1092`; `SendForSigningController.cpp:57,109,171,224` (4 dynamic_casts) + static `:164`; ui dialogs `SignatureDialog.cpp:113,345` (statics `planSignatureAppearance`/`setPendingAppearanceImage`), `SigningProgressPanel.cpp:11` + `SigningRequestDialog.cpp:26` (ctors take `SignatureManager*`). This is quality-new B4, now quantified tree-wide: **7 dynamic_casts to concrete managers** (adding `ConvertController.cpp:142,210` → `ConversionManager*`) |
| S2 | `IConversionEngine` exists, but `core/Capability.cpp:5` includes `ConversionManager.h` for the static `locateSoffice()` (`:287`) | core→engines direction is the sanctioned convention; going through the concrete type while the seam exists is the incompleteness. Same pattern class as S1 |
| S3 | `IPdfEditorEngine` mostly respected; concrete `PdfEditorEngine.h` reached by `modes/BatchMode.cpp`, `modes/PagesMode.cpp`, `shell/controllers/SecurityController.cpp` | engine-facing mode code holds the concrete engine (batch workers construct `PdfEditorEngine` directly). Acceptable while modes construct engines, but it is what makes the BatchPresetRun chain (§4) un-extractable from the panel today |
| S4 | **No seam exists at all for MainWindow services** — the concrete window is reached by child→parent `qobject_cast` at **8 sites in 6 files** (V4–V8: PdfViewerWidget ×2, PreferencesDialog ×2, MenuBar ×2, StatusBar ×1, PdfAValidationPanel ×1) for exactly four capabilities: `statusBar()->showMessage`, `openDocument`, `appContext()`, `pdfViewer()` | This is the parent class of every god-file pressure on GpMainWindow: children reach up, so the window must expose everything. Highest-leverage seam gap in the app |
| S5 | Placement anomalies | `src/GpMainWindow.{h,cpp}` loose at src root (belongs in `src/shell/` or its own module dir); `shell/EditPolicy.h` is layer-neutral policy in a chrome directory (V10); `MenuBar.cpp:4 → modes/PagesMode.h` for one static string `PagesMode::localFirstClaim()` (`:260`) — a text claim living on a mode class |

Count summary: **10 live upward edges + 3 dead upward includes; 0 cycles; 7 cross-layer
dynamic_casts; 8 MainWindow reach-up sites; 3 of 10 interfaces (ISignatureManager,
IConversionEngine, IPdfEditorEngine) have concrete-through-seam consumers; 7 of 10 interfaces
are cleanly consumed** (IFormManager, IOcrEngine, IPdfDocument, IPdfRenderer, IPdfSearcher,
IPdfWriter, IToolController — concrete includes only from wiring (app/Bootstrapper) or self).

## 3. Interface seam inventory

| Interface (`src/core/interfaces/`) | Implementors | Seam health |
|---|---|---|
| `IToolController` | all 10 controllers (HomeController…SendForSigningController) | clean — consumed by shell via the interface |
| `IPdfEditorEngine` (+ 9 role sub-interfaces: IPdfDocumentIO, IPageEditor, IImageEditor, ITextReplacer, IRedactor, IOutlineEditor, IEncryptor, IExporter, ISignatureAware) | PdfEditorEngine | good; 3 concrete consumers (S3) |
| `ISignatureManager` (defines PAdESLevel :34, SignatureOutcomeDetail :56) | SignatureManager | weakest: 6 non-wiring concrete consumers (S1) |
| `IConversionEngine` | ConversionManager | 3 concrete consumers (S1/S2) |
| `IFormManager` | FormManager | clean (concrete only in Bootstrapper/self) |
| `IOcrEngine` | OCR engines | clean |
| `IPdfDocument` / `IPdfSearcher` / `IPdfWriter` | PoDoFo/Pdfium backends | clean |
| `IPdfRenderer` | render backend + `ThumbnailRenderer` (src/ui/ThumbnailSidebar.cpp — a ui-side implementor, legitimate: the seam abstracts the renderer the sidebar drives) | clean |

## 4. God-file extractions — validated, corrected, sequenced

Baseline sizes at this tip: `GpMainWindow.cpp` **1,673** lines (quality-new measured 1,643 at
its tip; +30 drift), `BatchMode.cpp` **2,685** (+ .h 376). All quality-new line windows were
re-anchored; drift is noted per step. Sequenced dependencies-first (each step only depends on
steps above it). Risk classes: LOW = mechanical move with file-local logic; MEDIUM = widget/
wiring rewiring; HIGH = touches many call sites or unpinned behavior.

| Step | Extraction | Validated window (this tip) | Target module + layer | Interface exposed | Risk | Pin guarding it (first if missing) |
|---|---|---|---|---|---|---|
| 1 | **B1 label seam** (§5) | `SecurityController.cpp:155-183` body; use `SigningRequestRunner.cpp:266` | `core/SigningLabels.{h,cpp}` | free `QString gp::attainedLevelLabel(PAdESLevel, const SignatureOutcomeDetail&)` | LOW | existing: TestSignatureRealCrypto, TestSignatureBadges, SweepW1SecProbe pin the label text |
| 2 | **BatchPresetRun** — the per-file preset candidate chain | anon namespace `BatchMode.cpp:996-1169`: `pdfaLevelCode`, `pdfaConformance`, `runPresetMutatingStep` (:1016), `runPresetCheckStep` (:1061), `runPresetChain` (:1090, sole caller :1460) | **`engines/BatchPresetRunner.{h,cpp}`** | `bool runPresetChain(inputPath, outputPath, const BatchPreset&, QMutex* engineMutex, QString* techDetail)` (+ the two step fns as internals) | LOW | existing: TestBatchPresets + TestSweepW1PresetAdversary (both RESOURCE_LOCK-protected per W2 §3.1) — **VALIDATED as "do first"**, with one correction below |
| 3 | **BatchPresetPanel** | `BatchMode.cpp:2171-2570` (`s_presetStoreDirForTest` :2171 through the ForTest block ending :2562+, incl. `buildPresetPanel` :2183, `refreshPresetPicker` :2248, `onPresetSelected` :2295, `presetStepsDisplayText` :2319, `presetRunBlocker` :2343, save/rename/delete :2454/:2476/:2503) + the 6 m_preset* members (`BatchMode.h:297-302`) | `modes/BatchPresetPanel.{h,cpp}` | panel widget owning picker/labels/out-dir + the capture/delete API; **BatchMode keeps every `*ForTest` shim as a one-line forward** so TestBatchPresets is untouched | MEDIUM | existing: TestBatchPresets drives through the ForTest seams |
| 4 | **HotFolderController** | `BatchMode.cpp:788-930` (`hotFileKey` :792, `buildHotFolderSection` :797, `onToggleHotFolder` :830, `onHotFolderChanged` :883) + `m_hotFolder*` members (`BatchMode.h:364-369`, incl. watcher + debounce timer) | `modes/HotFolderController.{h,cpp}` | small controller owning QFileSystemWatcher + debounce; emits `batchRequested(paths)`; consumes a `resolveOutputPath` callback | MEDIUM | **NONE EXIST — add `TestHotFolder` FIRST** (characterization pin on current code: toggle persists the setting; simulated `onHotFolderChanged` → debounce → per-file run/resolve; settings-isolated, offscreen, RUN_SERIAL). Unpinned extraction of watcher lifecycle is exactly how regressions ship |
| 5 | **OpenRouteCoordinator** | `GpMainWindow.cpp:1349-1510` (`routeForFile` :1349, `planDrop` :1378, `runConversion` :1412, `convertAndOpenOffice` :1444, `convertAndOpenImages` :1483; quality-new's 1348–1510 re-validated ±1) | `shell/OpenRouteCoordinator.{h,cpp}` | keep `routeForFile`/`planDrop` as pure statics (move them, keep one-line static shims on `gp::MainWindow` for TestOpenRouting); coordinator owns the conversion runners and **emits** `requestOpenDocument(path)` / `showStatus(msg)` instead of casting to MainWindow (pilot for S4) | MEDIUM | existing: TestOpenRouting (routing matrix), TestWelcomeRoutes (office/images routes end-to-end :460/:480), TestBatchOpsCoverage (office tool probe) |
| 6 | **UpdatePromptController** | `GpMainWindow.cpp:1547-1659` (`startupUpdateCheckEnabled` :1547, `startupUpdateChannel` :1560, `initUpdateChecker` :1572; **drifted +26 lines** from quality-new's 1546–1629 — the two policy helpers belong to the same concern) | `shell/controllers/UpdatePromptController.{h,cpp}` (tenth controller, IToolController not required — it owns banner + UpdateChecker connections) | banner label + policy-locked channel/check-on-startup | LOW-MEDIUM | existing: TestUpdateChecker + TestPolicyWiring |
| 7 | **DocumentRecovery helper** | `GpMainWindow.cpp:624-689` (`recoverDocument`) | `core/DocumentRecovery.{h,cpp}` (pure file-move logic beside TempFileManager) | static recovery plan/apply, dialog-free; MainWindow keeps only the user prompt | LOW-MEDIUM | existing: TestRecoverySave |
| 8 | **ctor split** (no new module) | `GpMainWindow.cpp:105-611` (~506 lines) into `buildShell()` / `buildWelcomeConnections()` / `buildModeRegistry()` private methods | same file | — | MEDIUM (mechanical but large blast radius) | full-suite offscreen run (the shell is exercised by ~40 suites) |
| 9 | **WelcomeTaskRouter** | `GpMainWindow.cpp:734-776` (`startWelcomeTask`/`applyWelcomeTask`) | `shell/WelcomeTaskRouter.{h,cpp}` | task-id → screen/mode intent mapping | MEDIUM | existing: TestWelcomeRoutes (12 route slots :275-:547) |

**Validation verdicts on quality-new §3:** every line window re-anchored and confirmed
(±1 line in BatchMode, +26 drift in the update-checker window, correctly absorbed in step 6).
"BatchPresetRun do first" is **VALIDATED by dependency evidence**: the chain is an anonymous
namespace of 5 free functions with zero UI, zero BatchMode-member, zero AppContext
dependencies — its entire dependency set is `PdfEditorEngine&`, `SafeSave`, `PdfAConformance`
(from `engines/VeraPdfValidator.h`), `QPdfDocument`, `QMutex`, and core `BatchPreset` types.
**One correction: target placement.** quality-new proposed `modes/BatchPresetRun.{h,cpp}`;
the dependency set is engines-layer, so the module belongs at **`engines/BatchPresetRunner`**
— placing engine logic under `modes/` would mislocate it (the exact N2 discipline quality-new
praised elsewhere) and keep a modes→modes sideways edge instead of a clean modes→engines
downward call. Priority stands; placement corrected.

## 5. The B1 fix design (core → shell/controllers)

**Problem.** `src/core/SigningRequestRunner.cpp:7` includes
`shell/controllers/SecurityController.h` for the pure static
`attainedLevelLabel(PAdESLevel, const SignatureOutcomeDetail&)` (use at `:266`). It is the
only live core→shell edge in the app.

**Why the seam lands in core.** The function is pure and its ENTIRE dependency surface —
`PAdESLevel` (`ISignatureManager.h:34`) and `SignatureOutcomeDetail` (`ISignatureManager.h:56`)
— already lives in `src/core/interfaces/`. A label derived only from core types belongs in
core; controllers reaching it are then making a legal downward call. (Putting it on
`ISignatureManager` as a member was considered and rejected: an engine seam is the wrong
cohesion for a user-facing label, and engines would have to carry a string concern they do
not use.)

**Design — new module `src/core/SigningLabels.{h,cpp}`:**
1. Add `core/SigningLabels.{h,cpp}`: `namespace gp { QString attainedLevelLabel(PAdESLevel
   requested, const SignatureOutcomeDetail& detail); }` — body moved **verbatim** from
   `SecurityController.cpp:155-183` (including the SEP13/SWEEP-W1 F2 honesty comments). No
   behavior change possible: same bytes decide the same strings.
2. `SecurityController::attainedLevelLabel` becomes a one-line delegating static (or a
   `using` alias at class scope): all 7 in-shell call sites (`SecurityController.cpp:142,310,
   671,679` and SendForSigning-side users) and any test referencing the static stay
   source-compatible. Single definition, zero duplication — the R19c "single definition"
   law the code comments already claim.
3. `SigningRequestRunner.cpp`: swap include `:7` to `core/SigningLabels.h`; call site `:266`
   becomes `gp::attainedLevelLabel(...)`. The core→shell edge ceases to exist.
4. Compile + run the three label-pinning suites (TestSignatureRealCrypto, TestSignatureBadges,
   SweepW1SecProbe — all green per W2 grids).

Effort: **S** (half a day including the suite run). Risk: LOW. This is move #1 of the roadmap.

## 6. Architecture statement and the top-5 moves

**Statement.** GlyphPDF is a layered Qt monolith with a genuinely clean skeleton: zero include
cycles, 7/10 interfaces cleanly consumed, engines and core/document layers leafward-correct,
and a sanctioned core→engines convention that is not abused. The modularity debt is
concentrated in exactly three places: (1) the concrete `gp::MainWindow` acts as an
uninterfaced service hub that child widgets reach up into (8 cast sites — this, not laziness,
is why GpMainWindow.cpp grew to 1,673 lines), (2) the signing seam is incomplete, forcing 7
dynamic_casts and concrete-include consumers through 3 layers, and (3) a tail of 13 upward
include edges (10 live, 3 dead) — one per boundary, each individually small, all fixable with
named, pinned, sub-day-to-two-day moves.

**Top-5 ranked by defect-prevention value ÷ effort** (effort honestly sized: S ≤ ½ day, M = 1–2 days, L = multi-day):

| Rank | Move | Prevents | Effort |
|---|---|---|---|
| 1 | **B1 seam** (§5): `core/SigningLabels`, delegate, drop the include | the only live core→shell upward dependency; kills the class of "core borrows a shell helper" before it gets a second instance | **S** |
| 2 | **Dead-include sweep** (§2.2): delete `NetworkTouchpoints.cpp:17`, `PdfViewerWidget.cpp:5`, `Sidebar.cpp:3` includes, one compile check each | removes the entire core→ui and ui→shell edge classes at zero runtime risk; also un-blocks honest layer metrics (a fitness function on the layer matrix becomes trivially enforceable in CI afterwards) | **S** |
| 3 | **engines/BatchPresetRunner extraction** (§4 step 2) | engine logic trapped in the 2,685-line modes god file; makes the candidate chain testable without the widget and frees BatchMode for the panel/hot-folder splits | **S–M** |
| 4 | **MainWindow service seam (S4)**: narrow interface or signal set for the four capabilities children actually need (`showStatus`, `requestOpenDocument`, `appContext`, `pdfViewer`), piloted by OpenRouteCoordinator (§4 step 5), then migrate the 8 cast sites | the god-file *generator*: every future "widget reaches the window" feature adds surface to GpMainWindow today; the seam caps that permanently and is the precondition for shrinking the 506-line ctor | **L** (M for the pilot + interface; L across all 6 consumer files) |
| 5 | **Complete the signing seam (S1/S2)**: add the missing members to `ISignatureManager` (`takePendingAppearanceImage`, appearance-plan statics, fill-step support) and `IConversionEngine` (`locateSoffice`), then delete the 7 dynamic_casts and the ui/core concrete includes | silent concrete-type coupling that breaks the moment SignatureManager gains a mock/alt backend (W2's MockSignatureManager exists precisely because it doesn't); removes B4 class tree-wide | **M** |

Not top-5 but recorded: V3 (`SetOutlineCommand` holding `QPointer<PdfViewerWidget>` —
convert to a view-model signal, M) and V9 (`OcrScanCanvas` relocation beside
`OcrReviewSession`, S) are the two remaining live upward edges after moves 1–3.

## 7. Residuals and method boundaries

1. No build was executed in this lane (configure + compile_commands.json only; include
   evidence is compiler-argument-derived). The dead-include deletions of §2.2 each need one
   compile check when executed.
2. Linux-conditional sources (LibSecretStore per archaeologist §7.1) are outside Windows
   include-graph evidence, unchanged from the archaeologist boundary.
3. `shell/EditPolicy.h` relocation (V10) and `PagesMode::localFirstClaim()` text-home (S5)
   are placement proposals needing an owner lane; both are pure moves with existing pins.
4. TestLaneScheduler bound redesign, TestSignatureValidation merge-or-retire, R14ProbeBatchSkip
   register-or-delete, TestEngineSave QSettings (H2) — carried untouched from W2/archaeologist.
5. The extraction sequence (§4) is sequenced but NOT executed here; each step names its pin.
   Steps 4 and 5 of §4 interact with the S4 seam design — execute step 5's pilot before
   migrating remaining cast sites.

— solution-architect, SWEEP-W3. Handoff: `.context/sweep-w3-arch-wip.md`; evidence scratch:
`.context/sweep-w3-arch-scratch/` (both tree-local, gitignored).
