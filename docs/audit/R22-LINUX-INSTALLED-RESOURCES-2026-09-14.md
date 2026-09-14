# R22 — native Linux installed-resources runtime check — 2026-09-14

Lane: R22, worktree `pdf-inst` (C:\Users\User\Projects\pdf-inst = D:\pdf\pdf-inst), branch
`feat/parity-glm-r22` (base: local `feat/parity-glm` @ 83be3c2, the merged mainline candidate).
Runtime: Docker Desktop on Windows host; container `glyphpdf-linux` = `kalilinux/kali-rolling`
(bind: C:\Users\User\Projects\pdf-inst → /work). Toolchain IN-CONTAINER: cmake 4.3.4,
ninja 1.13.2, g++ 15.3.0 (Debian), Qt **6.10.2** (kali rolling), libsecret 0.21.7,
tesseract 5.5.0, qpdf 12.4.1, OpenJPEG 2.5.4, OpenSSL 3.6.3.

## 0. Scope contract (what a container may certify)
R22 asks: do the app/runtime test binaries run from an INSTALL TREE with the SOURCE tree's
resources hidden — not just from the build tree? A container certifies toolchain/dependency
closure and resource-loading gates ONLY. Desktop gates are UNTESTED, never claimed
(§5). R22 contract source: coordinator brief + `docs/audit/GLM-RESUME-STATE-2026-09-14.md`
open item 5 (R20-tail/R22–R23 Linux desktop continuation).

## 1. Incident + deviation (recorded honestly, decided by lane under no-answer fallback)
The brief stated the local image `kalilinux/kali-rolling:latest` was pre-provisioned
(Qt 6.10.2, cmake, ninja, g++ 15.2) and that `third_party/podofo/install-linux` is tracked.
BOTH premises were false, verified in-container:
- `install-linux/` is gitignored BY DESIGN (`.gitignore:92`, L04) — provisioned by
  `scripts/bootstrap-vendor-deps.sh`, which builds PoDoFo 1.1.0 from source.
- The image is VANILLA (187 MB, pulled 5 months ago). The R20/R21 toolchain lived in the
  OLD container's writable layer (`/usr/bin/cmake`, `/usr/lib/x86_64-linux-gnu/cmake/Qt6`
  — confirmed read-only via `pdf-clean/build-linux/CMakeCache.txt`). The brief-mandated
  `docker rm glyphpdf-linux` destroyed it. `kali-pentest:tooled-2026-07-04` (9.69 GB) has
  NO cmake/ninja/g++/Qt6 either. No provision script or package list survives anywhere
  in the repo (only fuzz-CI's `apt-get install clang`).
- AskUserQuestion returned no answer → lane best judgment: MINIMAL container-local
  `apt-get install` of the build toolchain (the exact class of provisioning R20/R21 did).
  `apt-get update` hit no captcha (the feared failure mode did not materialize). Host
  Windows environment untouched. **DEVIATION FROM THE LETTER OF THE BRIEF — recorded
  here and in `.context/r22-wip.md`.** Additional package beyond the R20 set:
  `libopenjp2-7-dev` (OpenJPEG2 is REQUIRED for MRC, CMakeLists:524; Debian ships
  OpenJPEGConfig.cmake 2.5.4, config path hit).

## 2. Linux build-gate defects found at the merged tip (4, all fixed on this branch)
The mainline candidate 83be3c2 did NOT compile on a Linux engine-less configuration.
Each is a Windows-lane blind spot — the same defect class R20-tail/SEP13:2 documented:

| # | Defect | Fix | Commit |
|---|--------|-----|--------|
| 1 | `src/engines/TextMatchFinder.cpp` `#else // !HAS_PDFIUM` stub of `findMatches` was not given the `MatchBudget* budget` parameter packa-F4 (88d5686) added to the declaration + HAS_PDFIUM branch → "no declaration matches", hard fail at 137/807 | stub updated + `Q_UNUSED(budget)`; provenance comment | 58e3914 |
| 2 | `tools/render_path_profile.cpp` included `<windows.h>/<psapi.h>` unconditionally and its CMake target (≈4528) is not WIN32-gated → fatal at 540/671 | only `peakWorkingSetBytes()` was Windows-specific: `_WIN32`-gated includes + POSIX branch reading `VmHWM` from `/proc/self/status` (honest PeakWorkingSetSize analogue; samples machine-dependent by design, never cross-OS comparable) | 8a6e4e4 |
| 3 | 12 newer test/tool POST_BUILD blocks hardcoded `platforms/qoffscreen.dll` instead of the L05 `${GLYPH_QPA_PLUGIN}` seam (TestBatchOcrSkipText, TestXfaHonestyBanner, TestFindReplace, TestReviewSummary, TestDynamicStamps, TestAutoBookmarks, …) — a regression AGAINST the R20 L05 contract; every plugin deploy failed on Linux | replaced with `${GLYPH_QPA_PLUGIN}` (12 lines) | 8a6e4e4 |
| 4 | TestXfaHonestyBanner ran an UNGUARDED podofo-DLL copy inside its plugin-deploy `custom_command`: on Linux `PODOFO_VENDOR_DLL` is the empty string, so `cmake -E copy_if_different` collapsed to ONE argument → usage error → build fail (the exact L05-mangle class the ledger records for TestBatchOpsCoverage) | split into a separate `if(WIN32)`-wrapped `add_custom_command` (the pattern every other podofo block uses) | 8a6e4e4 |

After fixes: `cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Debug` configures clean;
`ninja -j 2` full Debug build exits 0. Feature summary EXACTLY the honest R20 profile:
HAS_PDFIUM FALSE, HAS_TESSERACT TRUE, HAS_RAPIDOCR FALSE, HAS_QPDF TRUE, HAS_QUICKJS
FALSE, libsecret ON, OpenJPEG2 on, LinguistTools on. 142 ctest tests registered.

### 2b. Infrastructure hazard (NOT fixed here — flagged for the devops/infra lane)
The provisioned `third_party/podofo/install-linux` tree (copied READ-ONLY from
pdf-clean) was missing BOTH required symlinks — `libpodofo.so` AND the SONAME link
`libpodofo.so.4` (SONAME verified via readelf). Consequences: CMakeLists' vendored-prefix
guard checks only `lib/libpodofo.so` — with the symlink gone the guard fails (fixed here
by recreating it), and even with configure passing, EVERY test binary fails at exec with
`libpodofo.so.4: cannot open shared object file` (fixed here by recreating it). Something
stripped symlinks from that tree AFTER pdf-clean's configure (its cache references the
vendored prefix). Recommendation: bootstrap script must create both links; strengthen the
CMake guard to check the SONAME link; investigate what stripped them (a Windows-side
tree copy/sync is the likely culprit — symlinks do not survive FAT/ZIP/some sync tools).

## 3. THE R22 CHECK (deliverable)

### 3a. Install tree staging — honest note first
`cmake --install build-linux --prefix /tmp/r22stage` **installs NOTHING** (exit 0): the
only install rule is `install(SCRIPT ${deploy_script})` from
`qt_generate_deploy_app_script`, and with no `install(TARGETS PdfWorkstation …)` the
generated deploy script has no installed target to act on → silent no-op. `packaging/` is
Windows-only (WiX/MSI/ps1). THEREFORE the staged tree was constructed MANUALLY per the
runtime layout (bin/ + bin/../resources + platforms/ + qt.conf), which is the layout a
real Linux package would produce. **A Linux install/packaging rule remains OPEN WORK for
the R23/release lane** — without it there is no honest "clean-machine Linux install" gate.

Staged layout:
```
/tmp/r22stage/bin/PdfWorkstation TestStatusBarSlim TestRibbonCollapse
                       TestPagesMode TestOcrReviewLifecycle platforms/ qt.conf
/tmp/r22stage/resources/          # theme_dark/highcontrast/light.qss, icons, branding…
```
(TestRapidOcr is not built on this profile — target is guarded behind HAS_RAPIDOCR=OFF.)

### 3b. Method — independence from the SOURCE tree
Inside the container, against /work: `mv /work/resources /work/resources.r22-hidden;
mv /work/models /work/models.r22-hidden` (both restored afterwards, §3d). With the source
resources gone, ALL THREE G20-era resolution paths into the source tree are dead:
1. embedded Qt resource `:/resources/theme_X.qss` — NOT linked into test binaries
   (resources.qrc belongs to PdfWorkstation);
2. the CMake-injected compile definition `GLYPHPDF_SOURCE_RESOURCE_DIR="/work/resources"`
   (TestStatusBarSlim only, CMakeLists:2926) — the hidden dir;
3. the legacy `applicationDirPath()/../resources` heuristic in the BUILD tree — hidden.
The staged pass can then only come from the STAGED tree (`bin/../resources`) or the
embedded app resources. `GLYPHPDF_SOURCE_RESOURCE_DIR` is a compile-time definition, not
an env var, so "unset" is simulated by the hiding itself and "set-to-garbage" by an
explicitly recompiled variant (3c).

### 3c. Results (offscreen, serial, cwd outside /work; evidence in
`.context/r22-scratch/evidence/`)

| Run | Result | Meaning |
|-----|--------|---------|
| baseline: build-tree subset, nothing hidden | TestStatusBarSlim 12/12, TestRibbonCollapse 31/31, TestPagesMode 12/12, TestOcrReviewLifecycle 27/30 | source-fallback behaviour intact; the 3 reds are the pdfium-stub extraction class (`HAS_PDFIUM=OFF`; R21 precedent) |
| STAGED, source `resources/`+`models/` HIDDEN | **TestStatusBarSlim 12/12 PASS**, TestRibbonCollapse 31/31, TestPagesMode 12/12, TestOcrReviewLifecycle 27/30 (identical reds) | the G20 test now resolves the theme sheet from the STAGED tree (`bin/../resources`); models/-hiding changes nothing (engine-less probe never touches models/ — `Capability.cpp` #else) |
| STAGED, GARBAGE injection | recompiled TestStatusBarSlim TU with `GLYPHPDF_SOURCE_RESOURCE_DIR="/r22-garbage-nonexistent"` (exact ninja command via `ninja -t commands`, define swapped, relinked) → **12/12 PASS** | no hidden dependency on the injected path OR the source tree, even with a garbage define |
| CONTRAST control: build-tree TestStatusBarSlim, same hidden source | **FAIL** `cannot load theme sheet for mode 0` | proves the hiding was real and the build tree genuinely depends on the source tree — the staged pass is not vacuous |
| NEGATIVE control: staged `resources/` also hidden | **FAIL** `cannot load theme sheet for mode 0` (honest error, then staged copy restored) | proves the staged pass came from the staged resources — no third fallback lurking |
| App smoke: staged `PdfWorkstation`, offscreen, source hidden | exit 124 = alive until the 8 s timeout; stderr only the benign offscreen `propagateSizeHints()` note | app launches from the install layout; theme sheets load via embedded `:/…` (GpTheme.h returns qrc paths; MainWindow::applyTheme + app/main.cpp read them) — launch proof, NOT a UI gate |

### 3d. Restoration
`mv` back both dirs; spot-check `resources/theme_dark.qss` + `models/ppocrv5` present;
`git status --porcelain` **empty** (both dirs are tracked and byte-identical). Evidence
copied to `.context/r22-scratch/evidence/` (container /tmp is ephemeral).

## 4. Full serial ctest at the merged tip (142 tests) — FIRST EVER at this tip
Result: **109 passed, 33 failed + 1 timeout (77%)** — exit 8. Full log:
`.context/r22-scratch/r22-fullctest.log`. This is exactly the resume-state open item 2
("full serial ctest at this merged tip") — the tip had never had one; this run HANDS the
integration lane its data, it does not conclude it. Sampled classification (house
rerun-once applied to the known-flake names):

- **L03/pdfium-stub boundary class (VERIFIED in source — `extractedText`/read-back via
  `PdfiumBackend`, which is a stub at HAS_PDFIUM=OFF → empty text):** TestEngineSave
  (8 reds, helper at tests/TestEngineSave.cpp:124 literally "PDFium text extraction — the
  honest content check"), TestRedactTransaction (8; rerun-once identical), TestReadOnlyGate
  (1; rerun-once identical), TestOcrReviewLifecycle (3). SUSPECTED same class (not
  individually verified): TestCompareEntry/Integration, TestConversionExtraction,
  TestTextExtractionCoords, TestRedactionProof, TestRedactMarkAll, TestFindReplace,
  TestBatchOpsCoverage (7/9 on rerun; 1 red), TestBatchOcrSkipText (timeout — 60 s cap;
  suspect 9p I/O + stub extraction).
- **Container-environment class:** TestTempOwnership (10/11; the one red spawns a real
  process, `waitForStarted` fails in the minimal container).
- **Real signals needing integration-lane triage (NOT pdfium-explained):** TestDiffEngine
  (`pageCount1: 0` — the PoDoFo-side diff sees no pages in a freshly written base doc),
  TestCapabilityRegistry::rapidModelsProbeRealSetIsAvailableWithoutClassifier (green at
  the R20 tip, red here — merge-era change or fixture/root semantics), TestUiAccessibility
  (+200; keyboard delivery on offscreen — the packafix evidence was MSYS2/Windows),
  SmokeTest, TestWelcomeRoutes, TestRecoverySave, TestBatesBatchSafety/CrossDoc,
  TestExcisionCorruption, TestFormSafety, TestPptxOverlayAlpha, TestExportPathBadge,
  TestCheckedMutationCoverage, TestHistoryIntegrity, TestPersistenceOutcomes.
- Known-flake names that were GREEN this run: TestBatchMode, TestLaneScheduler,
  TestOllamaProvider (not in the red list).

The R22 core (§3c) is independent of this run: all five suites in the staged run behaved
identically before/after the models/-and-resources hiding.

## 5. Honest UNTESTED list (a container cannot certify these — never claimed)
- Wayland/X11 windowing, real GPU/compositor rendering, display scaling.
- Printing (CUPS absent in container), xdg-desktop portals, MIME/default-app integration.
- Input methods / IME / CJK candidate windows.
- Secret Service keyring RUNTIME (no gnome-keyring/kwallet daemon): positive
  store/read/delete round-trip, lock/unlock, cross-process — R21's boundary unchanged.
- Clean-machine desktop INSTALL experience (no Linux packaging exists — §3a).
- Form-JS execution (HAS_QUICKJS=FALSE — `libqjs-dev` NOT installed, same as R20).
- Pdfium rendering/extraction and RapidOCR/ONNX on Linux (native artifacts unprovisioned
  BY DESIGN — L03 open; the 3 TestOcrReviewLifecycle reds live exactly here).
- Windows side untouched: this lane never built or ran Windows binaries.

## 6. Ledger
R22 row appended to the 2026-09-14 section of
`docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` — wording: implemented/verified-here,
still **implemented-awaiting-review**; NEVER a global "verified" claim.

## 7. Artifact map
- Branch commits: 58e3914 (findMatches stub), 8a6e4e4 (build gate: QPA seam + podofo
  guard + tool port), 875c1c4 (case-alias FS-semantics pin) — on `feat/parity-glm-r22`.
- Handoff: `.context/r22-wip.md` (incident, plan, findings, final state).
- Evidence: `.context/r22-scratch/evidence/*.txt|log|sh` (staged/garbage/negative/
  contrast runs, app smoke, the exact recompile+relink commands).
- This doc: `docs/audit/R22-LINUX-INSTALLED-RESOURCES-2026-09-14.md`.
