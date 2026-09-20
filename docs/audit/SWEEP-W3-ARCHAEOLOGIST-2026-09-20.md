# SWEEP-W3 ARCHAEOLOGIST AUDIT — 2026-09-20 (dead-weight candidate evidence base)

Lane: code-archaeologist (W3 ponytail sweep). Branch `feat/sweep-w3-archaeo` @ `b17106a`
(== `feat/parity-glm` tip; `feat/sweep-w2-testing` content already merged into mainline —
verified `git merge-base --is-ancestor`). Scope: the fresh dead-weight candidate list at the
CURRENT tip. This lane CLASSIFIES and PROPOSES; the cleanup phase executes. Nothing was
deleted here; no git-history operations proposed (gc/prune remain program-forbidden).

Prior inputs reconciled (not re-litigated):
- `CLEANUP-LEDGER-2026-09-09.md` — its proven-safe deletions already executed; its
  structural claims re-verified at this tip where cited below (one disposition superseded, §3).
- `DELETED-BRANCH-RECOVERY-{SUMMARY,LEDGER}-2026-09-13` — honored: zero history ops.
- `SWEEP-W2-TESTING-2026-09-20.md` — all four §4 rows re-verified at this tip (§3 below).

Method (per candidate): build-graph evidence + include-reachability + string-table greps
| last touch | risk class | proposed action.
PROVEN-SAFE bar: unreachable from ALL targets AND no string-table/metadata reference AND
superseded-or-never-loaded. Anything less is NEEDS-REVIEW or KEEP-ANYWAY.

Evidence base (machine-checked at this tip):
- `build-presets/build.ninja` (configured 2026-09-20 18:25, source dir = this worktree):
  510 repo sources with obj rules — 155 `src/**` + 171 `tests/**` + tools + generated.
- Disk inventory: 157 `src/**/*.cpp`, 207 headers under `src/`+`tests/`, 172 `tests/*.cpp`.
- Transitive include-graph (BFS from all compiled sources, quote-includes resolved against
  includer dir + `src/` + `tests/`): 206/207 headers reachable.
- String-table checks: `Q_PLUGIN_METADATA`/`Q_PLUGIN` in `src/` — ZERO hits (no
  string-loaded plugin classes exist); `resources.qrc` vs `resources/` — 173/173, 1:1
  re-verified (the 09-09 claim still holds); ToolId/config-key/QSS lookups — no
  unreachable file references any (the only unreachable pair is annotation-toolbar, §2).

## 1. Candidate counts by risk class

| Risk class | Count | Items |
|---|---|---|
| PROVEN-SAFE | **0** | nothing met the triple bar (see §6 for the nearest misses and the clause each failed) |
| NEEDS-REVIEW | **2** | `tests/R14ProbeBatchSkip.cpp` (register-or-delete; this lane recommends register), `tests/TestSignatureValidation.cpp` (merge-or-retire, carried from W2 with current-tip confirmation) |
| KEEP-ANYWAY | **8** | `src/ui/AnnotationToolBar.{cpp,h}` (documented revival marker), `src/core/LibSecretStore.{cpp,h}` (platform-gated live), `docs/audit/HARDENING-2026-06-22-STATE.md`, `docs/audit/GLM-RESUME-STATE-2026-09-14.md` (watch-list docs rows), plus packaging/scripts verified clean (no rows) |

## 2. Unreferenced source files (src/) — the complete honest answer

The 09-09 ledger's structural sweep found src/ clean at `86e1bfe`. At the current tip the
build-graph diff yields exactly TWO files with no obj rule in any target, and the transitive
include graph yields exactly ONE unreachable header. Both findings, fully evidenced:

| path | evidence-of-unreferenced | last touched | risk class | proposed action |
|---|---|---|---|---|
| `src/ui/AnnotationToolBar.cpp` | No obj rule in `build-presets/build.ninja` (fresh config); absent from every CMake target source list; header included only by its own .cpp; tree-wide grep: zero code references — only docs (`COMPETITIVE-PARITY-AUDIT-2026-07-01.md` §9.3 ×2, `COMPARISON-TABLES-2026-07-01.md`:60, `SWEEP-LEGACY-2026-09-20.md`:138 names only the TEST) and `tests/TestAnnotationToolBar.cpp:4` comment "the old floating AnnotationToolBar is not compiled into the app" | born `e2fa814`; removed-from-build `6aac22c` "remove dead AnnotationToolBar from build (F9)"; retention RE-AFFIRMED `e5a5f01` 2026-08-26 | **KEEP-ANYWAY** — explicit revival marker: e5a5f01 commit message says "AnnotationToolBar kept current for future revival"; the §9.3 P0 one-authoritative-markup-surface decision is documented in the same commit and pinned by the live ribbon test | Keep. Cleanup phase may re-open ONLY with the parity lane's sign-off that the revival question is closed; deletion would also want a header-comment note pointing at e5a5f01. Not deletable on reachability evidence alone. |
| `src/ui/AnnotationToolBar.h` | The single unreachable header of 207 (transitive include-graph from every compiled source); only consumer is its own .cpp | same as above | **KEEP-ANYWAY** | Same as above (pair decision). |
| `src/core/LibSecretStore.cpp` | No obj rule **on the Windows build graph** — by design: `CMakeLists.txt:474-482` adds it to `pdfws_engines` only when `GLYPHPDF_ENABLE_LIBSECRET AND NOT WIN32 AND LIBSECRET_FOUND` (L07 Linux Secret Service backend, NATIVE-LINUX-READINESS-2026-09-10) | L07 era, 2026-09-10 | **KEEP-ANYWAY** — load-bearing platform-gated code (see watch list §7) | None. Never propose on Windows-build evidence. |
| `src/core/LibSecretStore.h` | Reachable (included by the L07-conditional compile path); same condition as above | same | **KEEP-ANYWAY** | None. |

Everything else under `src/` (155 .cpp / 206 headers) is compiled-and-reachable — verified
mechanically, not by sampling. `src/docmodel/*` and `src/pdfws_djot/*` compile under
`src/<subdir>/CMakeFiles/` (path-relative obj names) and `tests/TestAnnotationToolBar` /
`UnitTests` aggregate sources — naive "CMakeFiles/<t>.dir/src/…" greps misreport these as
orphans (see watch list §7.8).

## 3. Dead test infrastructure

W2 reconciliation first — all four SWEEP-W2 §4 rows re-verified at this tip, unchanged:
`R14ProbeBatchSkip.cpp` orphan (never registered); `R14ProbeRedactSpace.cpp` built-intentionally-
unregistered (`CMakeLists.txt:5144`, manual probe + qoffscreen deploy); `TestPdfEditorInterface.cpp`
alive (aggregated into `UnitTests`); `SweepW1SecProbe.cpp` alive (own registered exe).
Fresh machine check: 173 `add_executable` / 171 `add_test`; built-without-registration =
`PdfWorkstation` (the app), `R14ProbeRedactSpace`, `render_path_profile` — all intentional;
registered-without-own-exe = `TestUiAccessibility200` (intentional `QT_SCALE_FACTOR=2` alias).

| path | evidence | last touched | risk class | proposed action |
|---|---|---|---|---|
| `tests/R14ProbeBatchSkip.cpp` | The ONLY `tests/*.cpp` with no obj rule (172 on disk / 171 compiled); grep "ProbeBatchSkip" in CMakeLists.txt: 0 hits. Born `cd01e89` 2026-09-15 (R14 independent review, Q1/Q3/Q4 probes; header says "NOT a lane artifact"); 362 lines; W2 verified it syntax-checks clean against CURRENT headers (seams AppContext/PdfEditorEngine/BatchMode all exist); its two sibling probes both got registrations — `R14ProbeRedactSpace` (`:5144`, built-manual) and `R14ProbeSep13Fixes` (`:5162`, built+registered) | `cd01e89` 2026-09-15 | **NEEDS-REVIEW** (W2 left register-or-delete open) | **RECOMMEND: REGISTER**, via the sibling EXISTS-guard pattern + qoffscreen deploy (built-but-manual like `R14ProbeRedactSpace` — it drives OCR + QProcess and is heavy; promote to `add_test` only when a verdict re-run is wanted). Rationale: the omission looks accidental (both siblings registered), it pins independent-review verdicts Q1/Q3/Q4, and W2 already proved it compiles against current headers. If the cleanup phase instead deletes: the R14 review doc should gain a line noting the probe's retirement, because the review's evidence trail references it. |
| `tests/TestSignatureValidation.cpp` | W2 §5 TRUE-overlap finding, re-confirmed at this tip: 222 lines, 7 slots; the tautology `QVERIFY2(!sig.signerName.isEmpty() || sig.signerName.isEmpty(), …)` is STILL at line 132; own comment says "tested more strictly in TestSignatureRealCrypto" (23 slots, alive and green) | W2-era | **NEEDS-REVIEW** — merge-or-retire (W2's proposal, carried forward with current-tip confirmation; NOT a delete proposal from this lane) | Cleanup phase decides after porting the 3 unique pins W2 identified (`testValidateMissingFile`, `testSetTsaUrlPersists`, `testValidateUnsignedPdf`) into RealCrypto/Mock. |
| `tests/TestImageDedup.cpp` | **Disposition delta vs the 09-09 ledger**: it was "RETAIN unregistered" there; it is NOW `EXISTS`-guarded, registered with its own `add_test` (`CMakeLists.txt:1674-1697`, "gateD cleanup-lane finding") and has 35 obj-rule mentions in build.ninja | gateD era | not a candidate | none — alive; the 09-09 row is superseded by the gateD lane and should be read through this delta. |

Fixtures/mocks (checked; NO orphans — do not let "zero direct refs" re-surface these):
- `tests/mocks/MockPdfEditorEngine.h` — 16 including files; `MockSignatureManager.h` — 3. Alive.
- `tests/fixtures/signing/` — 25 files; every file with zero DIRECT test-name references
  (`expired.*`, `weak.*`, `revoked.crt/csr/key`, `ext.cnf`, `test_ca.srl`) is an input or
  intermediate of `generate.bat` (lines 11-38 verified: each is an openssl input or output)
  — live generator material with a documented regen path. Consumed fixtures
  (`test_input.pdf` ×13, `test_signer.p12` ×12, `test_ca.pem` ×5, …) all live.
- `tests/` top level: no non-.cpp files, no scratch debris.

## 4. Superseded documentation / design docs — zero deletion-grade candidates

112 files under `docs/`. Checked absorption candidates with inbound-reference evidence:

| path | finding | risk class |
|---|---|---|
| `docs/audit/HARDENING-2026-06-22-STATE.md` | zero inbound references; orchestration state for the June pass whose window closed (its durable half, FINDINGS, is separate). Proposal-grade superseded-state candidate. | **KEEP-ANYWAY** — it is the only record of the June pass's orchestration process and the origin of the hide-don't-delete policy; program history is evidence. |
| `docs/audit/GLM-RESUME-STATE-2026-09-14.md` | cross-referenced by `R22-LINUX-INSTALLED-RESOURCES-2026-09-14.md`; it is the program's live coordination point (names this very W3 sweep; its "shared %TEMP%/glyphpdf-candidates collision class" residual was subsequently fixed by W2's RESOURCE_LOCK patch — absorption happens forward, not backward) | **KEEP-ANYWAY** (live state doc) |
| `docs/djot-encode-design.md`, `docs/SPEC-TRACEABILITY.md`, `docs/github-setup.sh` | inbound refs from `CLAUDE.md`, `docs/planning/AUDIT-2026-06-16-REMEDIATION.md`, `CLEANUP-LEDGER-2026-09-09.md` + `docs/release/release-commands.md` respectively | KEEP (referenced) |
| `docs/research/*` (incl. `synthesis.md` vs the 16 per-competitor files) | synthesis summarizes but does not absorb; per-competitor files are research provenance | KEEP (blanket, per program policy) |
| release notes / beta / soak / security kit / planning prompts | documented consumers per `CLEANUP-LEDGER-2026-09-09.md` (re-verified: no new orphans) | KEEP |

## 5. Build/packaging remnants — zero candidates; all references verified against the tree

- `packaging/GlyphPDF.wxs` — 145-line skeleton (shortcuts + file association); payload is
  heat-harvested from `$(var.DeployDir)` at build time; only hardcoded `SourceFile` is
  `glyphpdf.ico` (exists, deployed by `deploy.ps1:292`). No stale paths.
- `packaging/deploy.ps1` — canonical deploy; installs the exe AS `GlyphPDF.exe`
  (`Copy-Item $exeSrc → GlyphPDF.exe`, line 72) — so `GlyphPDF.exe` in scripts is the
  installed name, not a stale target name; `PdfWorkstation.exe` remains the build name.
- `packaging/deploy-qt.bat` — 9-line documented forwarder to `deploy.ps1` ("canonical since R4").
- `packaging/deploy-msys2.bat` / `check-deps.bat` / `build-msi.bat` / `build_all.bat` —
  reference `build/` + `PdfWorkstation.exe`; paths valid (build dir created on demand by
  `build_all.bat:7`).
- `packaging/gen-installer-art.py` — regen SOURCE for tracked `banner.bmp`/`dialog.bmp`
  (both present, mtimes Sep 9); zero inbound refs is expected for generators (09-09 disposition, unchanged).
- `packaging/stage/` (tracked: `glyphpdf.ico` + 4 `tessdata/*.traineddata`) — NOT staging
  leftovers: consumed by `deploy.ps1`, `build-msi.bat`, `build-portable.ps1`, `deploy-qt.bat`.
- `packaging/winget/`, `WINGET-SUBMISSION.md`, `licenses/` — release submission material, live.
- `scripts/bootstrap-vendor-deps.sh` — targets `third_party/pdfium{,_dl}` and
  `third_party/podofo{,_build}`; fetch dirs are created by the script itself (Q02 bootstrap). Live.
- `tools/render_path_profile.cpp` — built, unregistered by design; documented in
  `docs/performance/hot-path-analysis.md` + evidence ledger. `tools/clean_scanned_pdf.py` —
  no production caller, but `tests/TestCleanupCli.cpp` exercises it. Both live.
- Untracked debris in THIS checkout: **zero** (`git status --porcelain`: clean; the only
  ignored entries are regenerable build/vendor dirs: `build-linux/`, `build-packafix/`,
  `graphify-out/`, `onnxruntime-win-x64-1.17.3/` — not git state, out of a delete-proposal's scope).

## 6. Nearest-miss table — why PROVEN-SAFE count is 0

| candidate | unreachable | string-table clean | superseded/never-loaded | failed clause |
|---|---|---|---|---|
| `src/ui/AnnotationToolBar.{cpp,h}` | yes | yes | **NO** — e5a5f01 "kept current for future revival" | 3rd |
| fixture generator intermediates (§3) | n/a (never expected to be named in tests) | yes | **NO** — live generate.bat material | 3rd (they are also not code) |
| everything else | not unreachable | — | — | 1st |

## 7. Load-bearing watch list — live code that LOOKS dead (prevent false positives)

1. `src/core/LibSecretStore.{cpp,h}` — absent from every WINDOWS build; Linux-primary
   credential backend (L07). Deleting on Windows-build evidence breaks the Linux path.
2. `packaging/gen-installer-art.py` — zero inbound references BY NATURE: it writes exactly
   the tracked `banner.bmp`/`dialog.bmp`.
3. `TestUiAccessibility200` — `add_test` with no own executable: intentional env-variant
   alias of `TestUiAccessibility` (`QT_SCALE_FACTOR=2`).
4. `R14ProbeRedactSpace` — built, intentionally never `add_test`-registered (manual probe,
   deploys qoffscreen plugin, `CMakeLists.txt:5144`).
5. `render_path_profile` — built, unregistered perf tool, documented in
   `docs/performance/hot-path-analysis.md`.
6. `tools/clean_scanned_pdf.py` — no production caller; driven by `tests/TestCleanupCli.cpp`.
7. `tests/fixtures/signing/*` zero-direct-ref files — generator chain of
   `generate.bat` (§3); regen path = the .bat itself (+ `generate_revoked_ocsp.py`,
   `generate_test_input.py`).
8. `src/docmodel/*`, `src/pdfws_djot/*`, `UnitTests`-aggregated and `EXISTS`-guarded test
   sources — obj paths in `build.ninja` are relative (`src/docmodel/CMakeFiles/docmodel.dir/
   Block.cpp`), so naive target-source greps misreport them as orphans. Check obj rules, not
   CMake text, before calling anything under src/ uncompiled.
9. `packaging/stage/{glyphpdf.ico,tessdata/}` — tracked and consumed by deploy/MSI/portable
   scripts; "stage" in the path does not mean "scratch".
10. `tests/TestAnnotationToolBar` — the NAME suggests it tests the uncompiled toolbar; it
    actually pins the ribbon Comment tab as the one authoritative markup surface (13 tools).
    Its header comment is the canonical record of why the toolbar is out of the build.

## 8. Residuals for the cleanup/fix lanes

1. `tests/R14ProbeBatchSkip.cpp` — register (recommended) or delete+annotate-R14-doc (§3).
2. `tests/TestSignatureValidation.cpp` — merge-or-retire after porting 3 unique pins (W2 §5, re-confirmed here).
3. `AnnotationToolBar revival question` — parity lane should close it explicitly (keep-with-comment or delete-with-annotation); evidence base now complete for either decision.
4. W2 residuals carried (not mine to act on): TestLaneScheduler bound redesign; TestEngineSave real-store QSettings remove (H2).
5. No build was run for this lane: the build graph evidence comes from `build-presets/build.ninja`
   configured fresh at this tip (2026-09-20 18:25) — a compiled ground truth, stronger than a
   re-parse of CMake text. Honest boundary: Linux-conditional sources (LibSecretStore) were
   verified by CMake condition reading, not by a Linux build.

— code-archaeologist, SWEEP-W3. Handoff: `.context/sweep-w3-archaeo-wip.md` (gitignored, tree-local).
