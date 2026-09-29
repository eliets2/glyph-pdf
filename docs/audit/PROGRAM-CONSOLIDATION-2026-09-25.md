# PROGRAM CONSOLIDATION — remaining work (2026-09-25 program; state at v1.5.0, 2026-09-29)

**What this file is.** The single list of work that **remains** after the consolidation program:
- landed on `main` (PR #2, cherry-picks only, linear history, purge intact);
- shipped as **v1.5.0**.

Nothing below is done. Completed work is not repeated here; its record is:

| Record | Covers |
|---|---|
| `CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` | The fix-all session: CX-01…17, N1, INV-1, and the E0–E6 gates with reproduction commands |
| `PGR-STATUS-2026-09-24.md`, `SECURITY-QUALITY-REVIEW-parity-glm.md` | PGR-01…46: per-ID status, carriers, pins |
| `CONSOLIDATION-EXECUTED-2026-09-25.md`, `CONSOLIDATION-BUNDLE-RECORD-2026-09-29.md` | The census and disposition of all 163 branches, the archive tags, the verified `--all` bundle and its restore drill |
| `CONSOLIDATION-LEDGER-2026-09-25.md` | Accounting ("0 unexplained") |
| `CHANGELOG.md` [1.5.0], `docs/release/release-notes-v1.5.0.md` | What shipped, including the release-verification fixes (K5 deadlock, the tag-run completion signal, the Set Expiry Date data loss, test isolation) |

**Parity records** (kept in `docs/audit/`):
- `PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` — the 2026-09-13 parity-line review findings;
- `SECURITY-QUALITY-REVIEW-parity-glm.md` — the security and quality review of the parity line, with per-finding status;
- `PGR-STATUS-2026-09-24.md` — the per-ID status table;
- `COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — the July competitive-parity audit;
- `evidence-pgr*/` — fail-before and pass-after transcripts.

Archived work stays reachable through its `archive/final/<branch>` tag and the bundle. Nothing was deleted without a pin.

---

## 1. Code fixes

| # | Item | What's needed | Effort |
|---|---|---|---|
| 1.1 | **3 CSV sinks lack formula-lead neutralization** (the PGR-16/17 class): `MeasureMode.cpp` `measureCsvEscape`, `BatchMode.cpp` `runReportCsv`, `ErrorInfo.cpp` `ErrorLog::exportCsv`. They quote fields, but some fields carry PDF-derived text. | Route all three through `ConversionManager::csvFormulaSafeCell`, including the plain-number exemption. Extend each suite's injection pins and add a negative control. | S |
| 1.2 | **OLE compound-document signature** `D0 CF 11 E0 A1 B1 1A E1` is missing from the container refusal list of `6841247d` (PGR-23). Legacy .doc/.xls attachments are swept as plain payload. | Add it to the list, with its own fail-before pin. | S |
| 1.3 | **Document-property edits may drop custom XMP** (found by code reading while fixing Set Expiry Date; not yet pinned). `PoDoFoBackend::setMetadata` calls `SyncXMPMetadata(true)`, which rebuilds the packet. An expiry date set earlier would then vanish when the title or author is edited. | Pin first: set an expiry, edit the title, read the expiry back. Then sync without reset. PDF/A export resets on purpose; keep that path. | S |
| 1.4 | **Zoom after Fit stays in fit mode.** Zoom In/Out after Fit Width or Fit Page does not leave fit mode. It is pinned as the expected failure `TestViewParity::zoomInAfterFitLeavesFitMode`. | An explicit zoom leaves fit mode; flip the expected failure to a pass. | S |
| 1.5 | **The rest of the file-handle coordinator.** K5 (the autosave/GUI deadlock) is fixed: undisplayed destinations never take the blocking hop. Still open: K1–K4, and a bounded hop for commits to the displayed file. | ADR-UI-03 step 2b. It is scheduled as UI-redesign Phase 0 (§6). | M |
| 1.6 | **Batch-presets `exportTo`** removes the target, then copies. A crash between the two steps loses the target. | Use the SafeSave commit idiom (candidate, then atomic replace), with a pin. | S |
| 1.7 | **Test flakes:**<br>• TestSignatureRealCrypto: the INV-1 pin `testOwnBltDssRevisionNotDowngraded` failed once on CI.<br>• TestWelcomeRoutes `imagesRouteProducesAndOpensTheOutput` fails on CI.<br>• TestSweepW3UxFlows fails on CI under parallel runs.<br>All three are green solo and ×3 locally. | Root-cause each from its captured CI artifacts (CX-14 uploads them). Fix it, or quarantine it with evidence. | S–M each |

## 2. Decisions for the owner

| # | Decision | Options | Recommendation |
|---|---|---|---|
| 2.1 | **D1 — the `forms/scriptPolicy` key** (a form-JS P3 prerequisite): a new R24 managed key, `"ask" \| "never"`. It has a fail-closed floor and never allows `"always"`. | (a) add it with P3<br>(b) refuse document-level scripts entirely<br>(c) defer P3 | (a) |
| 2.2 | **D2 — the `gpformjs-worker` executable** (form-JS P3a): a script-only quickjs worker using stdio JSON, a watchdog, a job object and `EngineLost`. | (a) ship the worker<br>(b) keep in-process execution with the disclosed R05 residual | (a). In-process execution is not an OS sandbox. |
| 2.3 | **D3 — keystroke handling.** Keystrokes over IPC, or keep keystroke handling in-process. | Decide by a P95 ≤ 30 ms latency budget. | Measure first. |
| 2.4 | **PGR-40 (HIGH) — the quickjs-ng CPU-deadline bypass.** Native sparse-array scans never poll the interrupt handler, so one `/AA` expression freezes the UI. The fix is not in 0.15.1. | (a) keep the pin and its auto-arming skip, and wait<br>(b) patch quickjs-ng in the vendor tree<br>(c) accept permanently | (a) now, then (b) if upstream stalls. It is disclosed in the v1.5.0 release notes. |
| 2.5 | **PGR-41 (LOW) — cascade cross-event tamper window.** | (a) a fresh runtime per event<br>(b) keep it disclosed and pinned | (b). Revisit if a hostile-script scenario lands. |
| 2.6 | **FOLD-2 — `feat/ocr-verify-finereader`** (B1–B15, the largest unmerged feature). It is archived as `archive/final/feat/ocr-verify-finereader`. | (a) a dedicated port lane<br>(b) drop it | (a), when OCR work resumes |
| 2.7 | **Code signing** (an EV or OV certificate). It unblocks three things: the in-app updater (`docs/updates/latest.json` is held at 1.3.1 because the updater refuses unsigned MSIs), the winget submission, and the SmartScreen warning. | Buy one or keep shipping unsigned | Buy one, when feasible |

## 3. Process

1. **R25b re-soak at the v1.5.0 head.**
   - The 48 h window of 2026-09-20…22 ran on an earlier head.
   - Read its verdict per the RESOAK protocol and run again at the release head.
   - `archive/final/feat/soak-48h-resume` holds the harness line.
2. **Closed-set attribution of the pg-side −888 lines, file by file.**
   - CONSOLIDATION-EXECUTED §4 sampled them and confirmed the supersession pattern.
   - The content is pinned regardless.
3. **Re-run the unreviewed-code map at the v1.5.0 head** (critique A10).
   - The scripts are on `archive/final/feat/unreviewed-map`.
   - Publish `UNREVIEWED-CODE-MAP-<date>-at-v1.5.0.md`. It makes no review-coverage claims.
4. **The awaiting ledger:** about 276 rows are *implemented-awaiting-review*. Independent review (W2 protocol) is still owed. "Awaiting" does not mean "reviewed".
5. **Package manifests.** winget, scoop and chocolatey are still at pre-1.4 versions. Update the SHA-256 and ProductCode when the project submits, after signing.
6. **Archived WIP scaffolding** (unfinished, never merged):
   - `archive/final/subagent-AST-Architect-self-54b52cfc`: the docmodel AST generator;
   - `archive/final/subagent-Vendoring-Specialist-self-ca2c3f27`: the djot codec scaffolding.

   Both sit on the pre-purge history line: port by patch only, never by merge.

## 4. Backlog (not blocking)

- **Ponytail §5 leftovers:**
  - the `EncryptedFileSecretStore::mutateSecrets` extraction (the PGR-25-locked critical path);
  - `FormJsSandbox::jsStringLiteral` → QJsonDocument escaping;
  - the `SignatureManager` bare-scope unwrap;
  - moving the evidence transcripts;
  - CX-13's `workflow_dispatch` acceptance re-dispatch.
- **Performance R1–R8** (PERF-BASELINE):
  - a warm/cold start split;
  - an Office-PDF corpus;
  - interactive and cancel latency;
  - frame pacing;
  - GPU paths;
  - multi-monitor;
  - the F5 observation (a 16x render where a 64 Mpx guard was expected);
  - the redact-apply median variance;
  - a per-job memory budget (F2).
- **Accessibility P3:**
  - tables, lists, ActualText, multi-column and outline-derived headings;
  - merging into already-tagged documents;
  - OCR-first scanned pages.

  PDF/UA conformance remains disclaimed.
- **Presets P3:**
  - a two-phase Bates pipeline;
  - `ocr` / `rotate-pages` / `convert` terminal ops;
  - editor drag-reorder.
- **Form-JS P3** (behind the decisions in §2):
  - the worker process;
  - OpenAction and document-level scripts, with a consent UX;
  - `/AA` lifecycle probe-reporting;
  - MOTW.
- **Research backlog not started:**
  - T2-5 (batch split / password strip);
  - N5 reverse wire-up;
  - N4 offline-degraded-validation wording;
  - the N38 GPO/ADMX/MSI/license tail;
  - the Tier-3 pool.
- **The residual-exec deferred set:**
  - P2 TSA test seam (an injectable `TimestampTransport`);
  - P5c AP-stream BBox aspect on /Rotate;
  - P5d PdfPageOps SafeSave candidate;
  - P5g the negative-/Rotate audit;
  - P7a the /NM dedup experiment;
  - P7b `proofFailsOnXmpSurvivor` triage;
  - P8 hypotheses W1-H1/H2/H3 (the H2 mechanism is confirmed at `SendForSigningController.cpp:182/202`);
  - **P10-D1, the models bootstrap gap:** `bootstrap-vendor-deps.sh` has no models step, so a fresh clone cannot deploy;
  - D3/D4 dead scripts;
  - B1 release-grade VCRT;
  - the P11 cleanup batch;
  - P14 perf tooling.
- **Architecture steps 3–9** (sequenced, not executed):
  - BatchPresetPanel;
  - HotFolderController (it needs a TestHotFolder characterization pin first);
  - OpenRouteCoordinator (the S4 seam pilot);
  - UpdatePromptController;
  - DocumentRecovery;
  - the ctor split;
  - WelcomeTaskRouter;
  - the MainWindow service seam;
  - the OcspConsent / OcspNetworkPolicyKey split into core.

## 5. Build and test hygiene (learned during the v1.5.0 verification)

- **Git Bash:** put `C:\msys64\ucrt64\bin` first on PATH. Git's `/mingw64/bin` DLLs break `rcc` during configure (`0xc0000139`). The README now says so.
- **The per-test temp roots already stop suites sharing temp state.** Test seams must not write into the shared build directory either. The CX-03 fake converter did, and it poisoned TestOfficeImport; it now plants on a private PATH.

## 6. Next program — the UI redesign

The redesign starts from the **v1.5.0** tag:
- the approved merge plan, with the co-designer's sign-off;
- the Phase M HTML reference prototype;
- the implementation handoff pack: ADR-UI-03 document ownership, the command-ID map, the theme system, and the Qt acceptance-test plan.

These live in the owner's design workspace, outside this repository.

`TestViewParity` is the no-regression lock for every view mode:
- Single, Continuous and Two-Page;
- Presentation and Full Screen;
- the zoom set;
- themes;
- Eye Care and Night Mode;
- RTL;
- panes;
- task screens.

Phase 0 of the redesign carries item 1.5.
