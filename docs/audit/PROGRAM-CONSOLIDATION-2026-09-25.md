# PROGRAM CONSOLIDATION — the complete remaining-work inventory (2026-09-25 program, written 2026-09-29)

**Purpose.** ONE consolidated file covering every goal, feature, fix, and finding across the
program's deliverables: what is DONE vs what REMAINS (code / decisions / process / backlog),
plus the takeover checklist. Written for Claude, who verifies and merges PR #2 next.

- **Basis:** PR head `12da4e2f` (`review/consolidated-parity`, PR #2) — every code claim below
  was re-checked at this tip (`git merge-base --is-ancestor` / tree greps), not just quoted.
- **Authoritative sources:** `CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` (STATUS: FINAL),
  `SECURITY-QUALITY-REVIEW-parity-glm.md` §11, `PGR-STATUS-2026-09-24.md` (Phase C snapshot,
  superseded for PGR-23/42-46 by FIXALL), `FIXALL-PROGRESS-2026-09-25.md` (CP1-CP9),
  `CONSOLIDATION-LEDGER-2026-09-25.md` (0 unexplained), `CONSOLIDATED-REPORT-2026-09-20.md`
  (+ addendum A2/A3 filled), `UNREVIEWED-CODE-MAP-2026-09-23.md`,
  `CONSOLIDATION-ACTION-PLAN-2026-09-25.md` + `CONSOLIDATION-CRITIQUE-2026-09-25.md`
  (branch docs — land them before execution, see §4), `RESIDUAL-PLANS-2026-09-21.md` +
  the residual-exec lane FINAL handoff, `FEATURE-PLANS-2026-09-21.md`,
  `docs/research/RESEARCH-BACKLOG-2026-09-10.md`, `CODEX-REVIEW-2026-09-25.md` +
  `CODEX-VERIFICATION-2026-09-25.md` (D:/prompts/claude-system/).
- **Program tallies:** PGR findings **43 of 46 fixed** (PGR-33 open-as-is by owner decision;
  PGR-40/41 deferred with pins). Codex CX **17 of 17 fixed**. Claude finds N1/N2/INV-1 **all
  fixed**. Gates E0-E6 **PASS** (E2: serial 185/185 = 100%; `-j6` 184/186 with both failures
  named and evidenced).

---

## 1. DONE — complete, verified or implemented-awaiting-review

Verification vocabulary: **verified** = independently re-proven (R14 / SWEEP-W2B / W2C
protocol, or FIXALL ×3-on-PR-head re-verification); **green** = implemented + suite-passing
on the PR head (×3 where recorded); most of the program remains *implemented-awaiting-review*
in ledger vocabulary — the 2026-09-25 FIXALL session's own ×3 gates are the newest evidence
layer (see the honest note at the end of this section).

### 1.1 PGR findings (all 46 dispositioned; 43 fixed)

| ID | One-line | Fix commit (PR) | Verification |
|---|---|---|---|
| PGR-01 | CID `/W` OOB heap write + DoS | kMaxCid cap (pre-Phase-C line fix, twinned in squash) | report §3 |
| PGR-02 | `#else` compile break (non-RapidOCR) | hoisted decl (line fix) | report §3 |
| PGR-03 | Excel duplicate cell refs → invalid XLSX | SEP13:3 (line fix) | report §3 |
| PGR-04 | Secret AEAD not AAD-bound to entry | SEP13:5 `0x03`/`0x04` formats (line fix) | TestSecretStore |
| PGR-05 | Unbounded Ollama response buffering (DoS) | SEP13:6 64 MiB cap (line fix) | TestOllamaProvider |
| PGR-06 | Encrypted-doc rollback drops password → lock-out | `0b06214a` | TestEngineSave 22P/0F |
| PGR-07 | `RedactOperation` accumulating leak | deleteLater (line fix) | report §3 |
| PGR-08 | Post-sign re-validation fail-open | SEP13:4 (line fix; superseded by PGR-21 fix) | TestSignatureRealCrypto |
| PGR-09 | Proof false-PASS rotated/offset pages | L8 shared transform (line fix) | TestRedactionProof |
| PGR-10 | Proof false-PASS unextractable text | unverifiable-can't-certify `862f9d5c` | TestRedactionProof 28P; Sep13Lead 11P; RedactTransaction 42P |
| PGR-11 | Overlay-label Y ignores MediaBox origin | L8 transform (line fix) | report §3 |
| PGR-12 | Batch merge reports success, no file written | publish-after-Save (line fix) | TestMergeSuccess / Sep13LeadBatchMerge |
| PGR-13 | Proof attribution misses annotation text | L7 sweep (line fix) | TestSep13LeadRedactionProof 11P/0F |
| PGR-14/15 | Read-only/edit-policy bypass (page ops / inline Replace) | `mutationBlocked()` gates (line fix) | report §3 |
| PGR-16 | CSV formula injection (conversion export) | `17e759f5` + plain-number exemption `75427aed` | TestConversionExtraction (21P/0F ×3) |
| PGR-17 | CSV formula injection (comments export) | `7d6a1d83`; unified onto `csvFormulaSafeCell` in `0aa5c2de` | TestCommentsReview 10P/0F ×3 |
| PGR-18 | Unescaped HTML into OCR overlay | `0ce53c76` | TestOcrReviewLifecycle 34P/0F |
| PGR-19 | Invalid calibration leaves stale scale | `e233e58b` | TestMeasureCore 22P/0F |
| PGR-20 | Legacy secret-blob DPAPI-binding gap | `2d893a21` | TestSecretStore (23P/0F ×3) |
| PGR-21 | **CRITICAL** in-place re-sign deletes only copy | candidate transaction `5cec76cb` | TestSignatureRealCrypto 26→28P/0F/1skip |
| PGR-22 | Save-path re-seat use-after-free | local buffer + both-or-nothing `6ed13c52` | TestEngineSave 22P/0F |
| PGR-23 | Proof certifies nested compressed containers | `6841247d` (nested-PDF recursion + decoded streams + PDFium depth-3; ZIP/OOXML/GZIP/7z/RAR/XZ/BZIP2 + unparseable-PDF refusal); pins `06ce3ae4`; parallel WIP `68bc917e` SUPERSEDED | TestRedactionProof 35P/0F ×3; Sep13Lead 11P/0F ×3; fail-before/NC/pass-after evidence |
| PGR-24 | Signing-candidate temp-file leak | M3 cleanupCandidate (line fix) | TestSignatureRealCrypto |
| PGR-25 | Secret-store lost-update race | `QLockFile` `99f8d3a6` | TestSecretStore |
| PGR-26 | `CRED_PERSIST_ENTERPRISE` roams credentials | per-machine `37561af9` | TestSecretStore |
| PGR-27 | Checked redo applied the edit twice | §6b fix (squash) | TestCheckedMutationCoverage |
| PGR-28 | Eye Care use-after-free | §6b fix (squash) | TestViewingModes |
| PGR-29 | Image move/resize/rotate never worked | §6b byte-exact operand replace (squash); nested-CTM completion = CX-02 | byte-exact pins; TestImageAppearance |
| PGR-30 | `listImages` placement reported wrong | §6b operand convention (squash) | §6b |
| PGR-31 | Image rotation pivoted wrong | §6b fix (squash) | 30° rotate + undo pin |
| PGR-32 | Refused commit could crash app | `catch (std::exception&)` (squash) | §6b |
| PGR-33 | `editTextInline` `Tf` dead code | — (owner: open-as-is, archived, do not enable) | n/a |
| PGR-34 | UX-flow harness race (3 of 11 flows) | Phase B pick | TestSweepW3UxFlows green |
| PGR-35 | Form disclosures rendered as rich text (spoof/beacon) | `40c9c382` (source `155f3bb7`) | TestFormJsAdversarial 24P/0F/1skip ×3 |
| PGR-36 | `AFSimple_Calculate` inherited-name ops | `abe351cb` (source `dc3240e9`) | TestFormJsCalc 49P/0F ×3 |
| PGR-37 | NaN/Infinity `event.value` wiped `/V` | `b5c63cbc` (source `77b57a7e`) | TestFormJsCalc 49P/0F ×3 |
| PGR-38 | `__proto__` field vanishes from snapshot | `82f309ab` (source `f6e1953c`) | adversarial snapshot pin ×3 |
| PGR-39 | Shim leaked 9 helpers as globals | `82f309ab` (IIFE) | surface pin ×3 |
| PGR-40 | quickjs-ng CPU-deadline bypass | **deferred** — pin coordinated `77db5be5` (0.15.1, hash-verified); disclosed skip auto-arms (`nativeSparseArrayScansAbideTheDeadline`); fix NOT in any MSYS2 release (A/B-probed) | decision item §3 |
| PGR-41 | Cascade cross-event tamper window | **deferred** — design decision (fresh-runtime-per-event); pinned test flips deliberately | decision item §3 |
| PGR-42 | Batch cross-file output-path collisions overwrite | `f90b4c71` (pick of pgr-d2 `39058fa9`) — NOW ON PR | TestPgr35BatchCollision 4P ×3 |
| PGR-43 | Stale signing-progress panel cross-document replay | `1d59f241` (pick of `3fd91495`) | TestPgr36StaleSigningPanel 3P ×3 |
| PGR-44 | **CRITICAL** pattern/replace excision space-law regression | `6600a429` (pick of `abe093aa`) — **FOLDED, not remaining** | TestPgr37PageSpaceLaw 9P ×3 + redaction cluster ×3 |
| PGR-45 | Batch progress/ETA skewed by mid-run list edits | `ddef6a8f` (pick of `958bd7c0`) | TestBatchMode 17P ×3 |
| PGR-46 | PatternRedactor mixed-space flip → mark-all marks off-target on rotated/offset pages | `99dd7b67` (pick of redaction-lane `a76b728d`) — **FOLDED, not remaining** | TestRedactMarkAll 21P/0F ×3; evidence `docs/audit/evidence-pgr46/` (18P/3F → 21P/0F) |

Round-2 triage items also landed: T1 OCR review-state re-entrancy `95352e53`
(TestOcrReviewLifecycle 34P), T3 compare anchor memoization `9e9e987a`
(TestSep13LeadComparePerf ratio 3.7x), T7 AiOptions maxTokens/temperature `9344eae5`
(TestOllamaProvider 63P), T6 secret-store zeroization `6722356f` (TestSecretStore).

### 1.2 Codex findings CX-01..17 (17 of 17 fixed; all confirmed by Claude at `0aa5c2de` first)

| ID | One-line | Fix commit | Verification (×3 on PR head unless noted) |
|---|---|---|---|
| CX-01 | Tag Document destroyed inline images | `838f71c7` | TestAccessibilityTagger painting-preservation invariant + PDFium backstop; 20P/0F/1skip |
| CX-02 | Image transforms double-apply outer CTM | `0bc692df` | TestImageAppearance desired × base⁻¹ pins (6 coefficients, reflection); in 47P/0F |
| CX-03 | Office conversion destroyed sibling files | `220b5f2b` (pick of `5cadce52`) | TestConversionExtraction 21P/0F; evidence `evidence-cx03/` |
| CX-04 | Repeat Tag Document deadlock | `3b2eb8c7` | TestAccessibilityPanel refusal pins; 16P/0F solo — post-fix contention defect logged (§2) |
| CX-05 | Invalid committed field values | `cabf7c15` | TestFormSafety 14P/0F; evidence `evidence-cx05/` |
| CX-06 | Stale migration re-wrap race | `f6ef7d68` | TestSecretStore 23P/0F; evidence `evidence-cx06/` |
| CX-07 | Form XObject MCIDs bare ints | `c7d261b6` | /MCR /Pg /Stm /MCID validator pins |
| CX-08 | Inline-image early EI | `175a1a12` | exact-extent pins (AHx/A85/Flate ±/L); evidence `evidence-cx08/` |
| CX-09 | Restack crosses BDC/BMC/EMC | `da295c34` | restack refusal pins; `evidence-cx09/` |
| CX-10 | Opacity wrapper blocked later edits | `1a8f6380` | gs-only-wrapper pins; `evidence-cx10/` |
| CX-11 | Matrix edit moved shared-block siblings | `c524f8ef` | SharedBlock pins; `evidence-cx11/` |
| CX-12 | deleteImage substring search | `a2397a94` | span-removal + reparse-gate pins; `evidence-cx12/` |
| CX-13 | Fuzz workflow not real | `92101f6c` (+`48a140f8`) | provisioned like ci.yml; oracle campaign + broken-harness NC in `evidence-cx13/` |
| CX-14 | CI discarded failure text | `38e6412b` | per-test txt+junit always uploaded; Fontconfig theory DISPROVED; real flake named |
| CX-15 | No sanitizer gate | `4e463a8c` + engine find `e0f72ce4` | ubuntu content-spans-sanitizer (ASan+UBSan) green on CI; gate found + fixed a real ExtGState-wrap corruption |
| CX-16 | Test bound ref into temporary QList | `1b42e166` | TestDynamicStamps 10P/0F (inspection — no ASan on UCRT64) |
| CX-17 | Vacuous OCR prefs test | `462212b4` | TestOcrPreprocessPrefs 5P/0F + non-vacuity negative control |

Small items: PoDoFo CI pin `712fb0e8` + E4 wording `eb3c181b`.

### 1.3 Claude find-and-fix items

| ID | One-line | Fix | Verification |
|---|---|---|---|
| N1 | Same-name image placements: edits hit the first occurrence only | `2ead0b17` (pick of `d63ed76e`; occurrence-index addressing end-to-end) | TestImageAppearance 41P/4F → **47P/0F** ×3; `evidence-n1/` |
| N2 (P0) | Blank document viewer (opaque `SignatureBadgeOverlay` without Q_OBJECT under theme QSS) | `2ed100fc`/`f9e44c6e`/`36b99c27` (picks of fix/p0-blank-viewer `4540e8fa`/`ee73520f`/`86f2cf34`) | composited-viewer grab pin; thumbnails announce page 1; paper-white renderPage; suites ×3 |
| INV-1 | Unsigned-incremental catalog allowlist let `/OpenAction` appends pass as Valid | `64baba6a` (DSS-only allowlist) | TestSignatureRealCrypto 28P/0F/1skip ×3; fail-before "got: Valid" captured; one CI flake of the no-over-block pin recorded (§2) |
| Phase-0 repair | 12 duplicate cherry-picks + missing include broke the build | `fbb40295` | blob pins hold; build green since |
| Ponytail (partial) | CommentsWidget CSV unification + CompareWidget probe collapse | `0aa5c2de` | TestCommentsReview 10P, TestCompareEntry 26P, TestCompareIntegration 8P ×3; §5 leftovers → backlog |

### 1.4 Features (implemented; ledger status implemented-awaiting-review unless noted)

| Feature | Carriers | Status |
|---|---|---|
| send-for-signing P1 (S4S-1..5) | `57cca6d`×3/`97c59a1`/`bc0ade4` (pre-squash; content in `1991d9c1`) | landed; TestSendForSigning green |
| form-JS P1+P2 (quickjs-ng Option A; Calculate/Format/Keystroke/Validate; R05 deadline) | `baf031e`/`86f8637`/`ac3698f`/`77bc50b`/`8afccc5`/`bcd34eb` era + §10 lane fixes | P2 Keystroke **verified (R14)**; TestFormJsCalc 49P, TestFormKeystroke 9P, adversarial 24P ×3 |
| accessibility P1 (checker) + P2 (auto-tagging structure-tree engine) | T2-4 P1 lane + AccessibilityTagger (+ CX-01/04/07 hardening) | TestAccessibilityTagger 20P/0F/1skip ×3; design plan `FEATURE-PLANS` §2 executed |
| batch presets P1 + P2 (U1-U7: bates ordered lane, rename/stop, import/export, measured-bytes report, hot-folder ingest, manager/editor) | R26 lane + `ca7deeda`/`dd8d20bb`/`3950e4b3`/`d96bace7`/`a2772901`/`341f5538`/`fde3ba46`/`32498fb8` | TestBatchPresets 15P + TestBatchPresetsP2 33P ×3; U1-U7 reviewed as new code; one LOW residual (§2) |
| printable summaries (ReviewSummaryWriter + WinAnsi-safe boundary) | W1-04 `0441192` era | **verified (W2B)** — hostile-set survival + re-extraction |
| N17 cert-encrypt UI + N18 certify selector + FU-1 wiring | `d423ba3`/`e6e2276`/`15c0d11` (pre-squash; tree-verified at tip) | TestCertEncryptPicker 12/12, TestCertifySelector 14/14, NCs captured |
| R24 machine policy + wiring (+ R24(c) touchpoint honesty `69fc6fb` verified) | `8ea3876`/`c766623`/`60212ca` + W1..3 | **verified subset (W2B)**; F1 enforcement structurally closed by `9e9818e9` (below) |
| T2-2 redaction Find&Replace page-space law (both ends) | ported `b09256ee` (of residual-exec `9b2b2727`) | TestFindReplace 30P/0F, TestPgr37PageSpaceLaw 9P ×3; PORTED-not-superseded with fail-before evidence |
| M1 per-mark accounting / M2 save-first prompt / M3 CSV plain-number exemption | `3fb0b8c2` / `673d19a0` / `75427aed` | pins present and passing ×3 (M1 pin at TestRedactionProof.cpp:1382; flow7c "never certifies" retained) |
| July-era ports (Night Mode + Eye-Care UAF `6020ea8b`; image restack/opacity/rotate `e552df26`; letter/line spacing `3d8bca5d`; TestOfficeExport `e6497dbe`; checked-redo `ab82e81d`) | squash + follow-ups | landed; TestViewingModes/TestImageAppearance/TestTextEditStyle/TestOfficeExport green |
| AM1 B1 SigningLabels seam / AM2 dead-include sweep | `5c02b01d`/`791115bb` (merge `26c9a415`) | merged; post-merge gate 173/173; awaiting review |
| W1-05 structural close (PolicyController admin-owner gate, `State::UntrustedOwner`) | `9e9818e9` (fold of residual-exec `9ff2b136`) | TestPolicyWiring 12/12, TestPolicyController 13/13, W2BProbeSummaryPolicy 10/10 — closes §2.6 residual 1 |
| Sweep-legacy residual fixes: extractLinks through the law (`47b2998b`), exportToImage range refusal (`fbfa75a6`), R14ProbeBatchSkip registration (`05e1b8cc`), matrix CSV repair (`ec9d4f2b`), archaeo dispositions (`7c70b881`), fuzz yml provisioning (`48a140f8`, superseded by CX-13's fuller fix) | feat/residual-exec folds | per-item suites green (FIXALL-PROGRESS Phase 1 table) |
| Resolved-at-tip confirmations: runIntersects glyph-band (3×fs headroom replaced by real ink band — the "3×fs precision item"), `signatureFieldAnchors` GetRectRaw + law (§2.6 residual 4), F2b-D1 preset-store mkpath | carriers inside squash `1991d9c1` | W2CProbeRotate270 7/7; TestRotate270PageSpace 8/8; TestBatchPresets pin present |

### 1.5 Sweeps W1/W2/W3

- **W1 (attack wave): 15/15 confirmed findings fixed AND verified** (W1-01..05, F1-F6,
  FZ-1..4) by SWEEP-W2B (16 negative controls); F5 rotated caveat lifted by W2C. 7 adversary
  hypotheses refuted with pins; W1-H1/H2/H3 recorded (backlog).
- **W2 (verification wave):** W2B verified 15 W1 + SL2/SL3; W2C verified W2B-1 root fix,
  flipped SL1 to verified, lifted F5 caveat, verified L5/L8 on rot270+offset. FU-2
  candidates-dir interference fixed (`RESOURCE_LOCK`, 171/171 patched run). Suite audit:
  170/170 registration, flake census, TestModeStripPins coverage gap closed.
- **Soak catch:** San-UAF (`AssertMutable` UAF) fixed `ed04426e` with fail-before + NC;
  XMP-survivor flake root-caused + fixed as SF-2 (`NoMetadataUpdate`). Both
  implemented-awaiting-review, family green.
- **W3 (system wave):** emergence EM-1..E-6 fixed (`645f4994`/`3325ea19`/`ee2cf661`/
  `cee2777c`/`9463f6c0`/`c1552c26`) and verified PRESENT at HEAD (09-25 ledger feature-
  presence); ri-fix present; archaeologist (0 deletions, dispositions recorded), architect
  (zero cycles; 13 upward edges; B1+dead-include executed), research (corpus reconciled),
  UX (flows run; friction fixes folded), UI lens (landed) — documented.
- **Performance baseline** (quiet-gated; cold start 150 ms, compare 807 ms, 50-page
  paginate 1.3 s) + **48 h soak VERDICT PASS for `b17106a`** (48.12 h, 719/719 app cycles
  clean, 100 % of failing-test events classified) + **deploy validation + dependency
  closure PASS** (objdump walk; SBOM hashes) — all recorded in CONSOLIDATED-REPORT §5-§6.

### 1.6 Infrastructure

- **CI:** Build step green on every run; content-spans-sanitizer job green; per-test
  txt+junit artifacts always uploaded (CX-14); fuzz workflow provisioned (CX-13); PoDoFo
  source pin `712fb0e8`; License Guard; purge + linearity gates (E3: 0 merges, main
  ancestor, no CLAUDE.md/SECURITY.md).
- **Gates E0-E6 at the FINAL head (handoff §6):** E0 hygiene PASS; E1 fresh Release build
  945/945 steps 0 errors; E2 `-j6` 184/186 (both failures named+evidenced) and **serial
  185/185 = 100 %**, 32 touched suites ×3 green; E3 PASS; E4 secret scan 0 matches /
  166,613 added lines; E5 ledger 0 unexplained (105 `-x` trailers, exactly the 12 Phase-0
  revert sources duplicated); E6 pushed + PR body updated.
- **Phase-0 blob acceptance** (handoff §2): all five pins hold; the three moved files carry
  named, tested reasons.
- **Line reconciliation** (LINE-RECONCILIATION-EXECUTION): `feat/consolidated` `eb0efa21` =
  pg + consolidate/all + consolidated-parity@`f4750af5` + main, 29/29 conflicts resolved,
  zero-loss proofs ×4, build+ctest verified; ff'd into `feat/parity-glm` @ `195e4309`.
- **Consolidation ledgers:** 09-24 (legacy branches, 0 unexplained) + 09-25 (fixall era,
  §1-§5, 0 unexplained; `68bc917e` SUPERSEDED(code)+PORTED(tests)).

**Honest note.** "DONE" above = implemented + gate-tested; a large fraction of the program
(the 2026-09-05 ledger's ~276 awaiting rows on the PR) is *implemented-awaiting-review* in
the strict ledger vocabulary — the CX/PGR/fixall waves carry per-fix fail-before/NC/pass-
after evidence and ×3 suite gates, but the full W2-protocol independent review of every row
has not been run and is NOT a merge blocker per the handoff. Consolidation preserves
unreviewed code; it does not bless it.

---

## 2. REMAINING — code fixes needed (open findings requiring code changes)

Checked-first items from the task brief that are NOT remaining (verified at the tip):
**PGR-44 CRITICAL is folded** (`6600a429` — the pgr-d2 fixes landed with CP9 re-
verification); the **runIntersects 3×fs precision item is resolved** (glyph-band ink
extents, carrier inside `1991d9c1`); the **PatternRedactor mark flip is fixed** (PGR-46,
`99dd7b67`); the W1-05 machine-policy enforcement residual is closed (`9e9818e9`).

| # | Item | What's needed | Owner lane | Effort |
|---|---|---|---|---|
| 2.1 | **TestAccessibilityPanel contention SEGFAULT** (`repeatApplyRefusedWhileTagRuns`) — the only crash-class item; CI -j4 3/3 runs red since the images lane (runs 36486557489/36486624682/36490155523: 12 PASS then crash entering this test); local 3/6 under 6 parallel copies; solo and ×3 always green (16P/0F). Hazard candidates: the test's by-reference runner captures vs the CX-04 cancel-only destructor on a contention-blown wait | Reproducer: 6 parallel `TestAccessibilityPanel.exe` under load. Fix the test's runner captures (by-value) or the cancel path's wait handling; prove ×3 under the 6-way load repro; re-run full `-j6` | accessibility/test lane | S-M (half-day incl. evidence) |
| 2.2 | **3 CSV sinks still lack formula-lead neutralization** (PGR-16/17's OWASP class): `src/modes/MeasureMode.cpp` `measureCsvEscape` (quoting only; fields include the annotation /Contents snapshot — PDF-derived text), `src/modes/BatchMode.cpp` `runReportCsv` (quoting only), `src/core/ErrorInfo.cpp` `ErrorLog::exportCsv` (quoting only; technicalDetails can embed PDF-derived text) | Route all three through `ConversionManager::csvFormulaSafeCell` (the single contract, incl. the plain-number exemption); extend each suite's injection pins + NC | conversion/cleanup lane | S (one pass, three sites) |
| 2.3 | **OLE compound signature residual**: superseded PGR-23 WIP's `\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1` is NOT in `6841247d`'s container refusal list — legacy .doc/.xls attachments are swept as plain payload | Add to the refusal list **with its own fail-before pin** (R7: no port without fail-before evidence) | redaction lane | S |
| 2.4 | **TestSignatureRealCrypto CI flake** — INV-1 pin `testOwnBltDssRevisionNotDowngraded` got `ValidWithUnsignedChanges` once on CI (run 36486624682, artifact captured); green everywhere else | Root-cause the DSS-only allowlist fixture's time/environment dependence; pin the fixture clock | signature lane | S-M |
| 2.5 | **TestWelcomeRoutes flake** — `imagesRouteProducesAndOpensTheOutput` red on CI (empty output pre-CX-14; captured since); Fontconfig theory disproved; solo green 20P/0F | Root-cause from the captured artifact text; fix or quarantine with evidence | UI/test lane | M |
| 2.6 | **TestSweepW3UxFlows parallel flake** — red on 2 CI runs; green solo and ×3 locally | Same treatment; flow7 assertion itself already re-pinned honestly (`cee36259`) | UX/test lane | M |
| 2.7 | **batch-presets LOW residual** — `exportTo` remove-then-copy window (a crash between remove and copy loses the target) | Copy-to-temp-then-rename (or SafeSave commit idiom) + pin | presets lane | S |

Severity context: 2.1 is crash-class (test process, not the app). 2.2 is the same
HIGH-class injection family as PGR-16/17 but with app-derived-plus-PDF-derived field
sources — treat as MEDIUM until triaged. 2.3-2.6 keep CI's Test step from being fully
green (see §4/§6: merge-with-explained-reds is the recorded posture; the owner accepts it
or lands these first).

---

## 3. REMAINING — decisions needed from the owner

| # | Decision | Options | Recommendation |
|---|---|---|---|
| 3.1 | **D1 — `forms/scriptPolicy` allowlist key** (form-JS P3 prerequisite): add a new R24 managed key `"ask" \| "never"` (fail-closed floor, no `"always"` ever) mirroring `signing/ocspNetworkPolicy` | (a) add the key now with P3; (b) refuse document-level scripts entirely (P3b never lands); (c) defer P3 | (a) — the plan is designed, the consent-dialog idiom is landed (R24-W2), sanitize keeps stripping all script entry points either way |
| 3.2 | **D2 — `gpformjs-worker` as a distribution surface** (form-JS P3a): ship a script-only quickjs worker exe (stdio JSON + watchdog + job object + `EngineLost`) | (a) approve worker exe; (b) keep in-process execution with the disclosed R05 residual | (a) — it is the standing R05/JS-01 review residual ("in-process is not an OS sandbox"); packaging cost is one exe + handshake pins |
| 3.3 | **D3 — keystroke-over-IPC vs disclosed in-process keystroke** for the worker | decided by a P95 ≤ 30 ms latency budget at implementation time | measure first; the plan's own criterion |
| 3.4 | **PGR-40 (HIGH) — quickjs-ng CPU-deadline bypass** (native sparse-array scans never poll the interrupt handler; one `/AA` expression = unkillable UI freeze per save/keystroke; fix NOT in 0.15.1 either, A/B-probed) | (a) **vendor-vs-wait**: keep the 0.15.1 pin with the auto-arming disclosed skip and wait for an upstream/MSYS2 fix; (b) patch quickjs-ng in-vendor-tree and carry the patch; (c) accept permanently with disclosure | (a) short-term (already the shipped posture — pin coordinated `77db5be5`, probe auto-arms) + schedule (b) if upstream stalls; the suite pin makes adoption automatic when a fixed package lands |
| 3.5 | **PGR-41 (LOW) — cascade cross-event tamper window** (a script ending without a committed write leaves its sandbox rewiring in place for the next event) | (a) fresh runtime per event (kills the window; per-event startup cost); (b) keep the platform-inherent window, disclosed + deliberately-flipping pin | (b) now (Acrobat/pdf.js share the semantics; the pin documents it); revisit (a) if a hostile-script story lands |
| 3.6 | **R4-3 — July-era archive branches** (editing/viewing/redaction/security/ocr-parity + accessibility-parity, 30-35 patch-unique commits each, superseded duplicates, unique features already ported) | (a) confirm do-not-merge/archive-only, tag + bundle, deletion deferred to explicit sign-off; (b) attempt merges (35-39 conflicts each, re-imports the purge files) | (a) — three plans concur (09-20 §4.3, line-recon §1, action-plan §2.3b) |
| 3.7 | Action-plan carry-over questions (§5 Q10-12): **one-branch end state** (main only vs main+survivor ref — plan default: main-only, survivor ref deleted after PR #2 records merged); **FOLD-2 `feat/ocr-verify-finereader`** (largest unmerged feature, B1-B15, 468-path merge forecast — default ARCHIVE + a dedicated port lane later); **re-soak timing** (run the R25b 48 h re-soak at the consolidated head before or after the deletion pass) | per item | main-only per the owner's "one branch" reading; ocr-verify archive; re-soak after merge, before deletions (the soak needs no branches) |

---

## 4. REMAINING — process steps (non-code work)

The consolidation endgame per `CONSOLIDATION-ACTION-PLAN-2026-09-25.md` (survivor =
`review/consolidated-parity` @ `12da4e2f`, end state `main := T` by fast-forward), with the
critique's binding fixes applied. **Lands first:** the action plan + critique documents are
on `feat/consolidation-action-plan` / `feat/consolidation-critique`, not yet on the PR —
cherry-pick both (docs-only) before executing.

1. **Pre-merge fold closes (blocking "0 unexplained"):**
   - Disposition `feat/fixall-redaction`'s `5461b72d` (a parallel N1 implementation, 19 min
     before the FINAL handoff; the critique measured **62 image-surface lines the PR
     lacks** vs `2ead0b17`): written content diff → fold / supersede / owner-decide, then
     re-run E5 accounting. Until then the honest claim is "0 unexplained + 1 open item".
   - **Ledger union proof (critique A4):** prove the PR's `CURRENT-EVIDENCE-LEDGER` (438
     rows / 276 awaiting) is a row-union superset of pg's (392/265) and consolidate/all's
     (358/244), or land missing rows on the survivor first. Never `-X ours` on the ledger.
   - **pg-side −888 lines** (222-file pg↔PR tree delta): closed-set attribution — every
     pg-side hunk traces to a named pick or documented supersession (line-recon §3.2 method,
     inverted).
   - **Unreviewed-map re-run at T** (critique A10): publish `UNREVIEWED-CODE-MAP-<date>-at-T.md`
     from the `feat/unreviewed-map` scripts; no review-coverage claims.
   - **FOLD-1 `ar/prompt-1`** (5 June UAF/data-loss fixes): per-fix supersession review
     D1-D5 against the PR tree (D2 proven superseded by EC06; D3 likely; D1/D4/D5 re-derive);
     cherry-pick `-x` survivors with pins, ×3 suites; every fix gets PICKED or SUPERSEDED
     with file:line evidence.
2. **Phase 0 — safety net (blocking everything):** pause the 06:01 continuity automation;
   freeze lanes at named tips; `git bundle create pdf-archive-final-<date>.bundle --all` +
   verify + SHA-256 recorded; create the missing `archive/*` tags (26+ tips incl. **the
   critical `archive/line/parity-glm` → `195e4309`**, fold-lane tips per critique E4, July
   siblings) and **push all archive tags**; **restore drill** (`git clone` the bundle into
   scratch, `rev-parse` every pinned tip, blob spot-checks, transcript committed) — never
   rehearsed; census re-run (any drift ⇒ abort).
3. **Stage-2 execution** (script `consolidation-action-draft.sh`, DRYRUN default,
   `CONSENT`-gated, fail-closed `delete_proven()`), with critique §7.2's E1-E11 fixes:
   local-tip pins (not origin), checked-out-in-worktree refusal + worktree-release → local
   `-d` → remote delete ordering, dirty-`D:/pdf/pdf` abort, no `tag -f`, no-op containment
   checks repaired, phase-2 owner gate present, per-class proofs (not bare `rev-list`), FOLD
   order: Phase 1 FOLD-1 → Phase 2 FOLD-2 (owner gate, default skip) → **Phase 3 merge:
   `git push origin review/consolidated-parity:main` (FF-only, never force)** → Phase 4
   gates at main (E1 fresh build + serial ctest, sanitizer green, E0-E5 re-run, CI
   re-checked) → Phase 5 deletion pass (worktree-released fold-lane branches → commit-
   contained → united-line set after PR #2 shows merged → archive classes; HOLD set —
   `feat/soak-48h-resume`, `test/view-parity-baseline` — skipped) → Phase 6 end-state proofs
   (`ls-remote` = main only; tags intact; bundle re-verified).
4. **Docs ground-truth commit:** land plan + critique + this file; update
   `PGR-STATUS`/ledger rows for the FIXALL-era changes (PGR-23/42-46 now-on-PR); record the
   Night-Mode residual as closed by the 09-24 ledger (critique A9); correct the inherited
   claims (critique A15/E1-E3: 585-commit unpushed pg tail, not 176; `claude/modest-
   mccarthy-riuo2o` downgraded to ARCHIVE-CONTAINED — its AUDIT-2026-06-16 blob diverges
   579+/661- from the PR's; "0 unexplained" is era-scoped).
5. **Cleanup (after deletions):** retire the 5 gemini-subagent worktrees + `pdf-base7d`
   (export baseline flow7 logs first); **quarantine the 5 antigravity worktrees** — each
   holds ~703 staged files incl. `tests/fixtures/signing/ca.key`, `signer.key`,
   `test_signer.p12`; forbid add/commit/stash/clean there and surface the staged keys to
   the owner as a security item; disposition `D:/pdf/pdf`'s dirty tracked mod
   (`PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` + uncommitted SECURITY-REVIEW copy —
   consolidation-relevant, not noise); fast-forward local `main` to T.
6. **Final build + verification** at merged main (Claude's independent rebuild per handoff
   §8: fresh Release configure + 945-step build, serial ctest 100 % expected, `-jN` expect
   the Panel race, E3/E4/E5 re-run, CI re-checked — Test step reds must be exactly the four
   named items with artifacts).

---

## 5. BACKLOG (post-merge; not blocking)

- **Ponytail §5 leftovers:** `EncryptedFileSecretStore::mutateSecrets` extraction (PGR-25-
  locked critical path), `FormJsSandbox::jsStringLiteral` → QJsonDocument escaping
  (PGR-38-adjacent load-bearing), `SignatureManager` bare-scope unwrap, moving the evidence
  transcripts; plus the presets `exportTo` window (§2.7) and CX-13's workflow_dispatch
  acceptance re-dispatch (evidence is currently the lane's own record).
- **Performance R1-R8** (PERF-BASELINE): warm/cold start split, office-PDF corpus,
  interactive + cancel latency, frame pacing, GPU paths, multi-monitor; F5 observation
  (16x render where a 64 Mpx guard was expected — view-path scoping); re-probe the
  redact-apply median variance before quoting either number; per-job memory budget (F2).
- **Accessibility P3:** tables/lists/ActualText/multi-column/outline-derived headings,
  merge-into-already-tagged, OCR-first scanned pages; PDF/UA conformance remains
  disclaimed.
- **Presets P3:** two-phase bates pipeline (ordered-lane optimization), `ocr` /
  `rotate-pages` / `convert` terminal ops, editor drag-reorder.
- **Form-JS P3** (the §3 decisions themselves): worker process, OpenAction/doc-level
  scripts + consent UX, `/AA` lifecycle probe-reporting, MOTW.
- **Research backlog not started:** T2-5 (batch split / password-strip), N5 reverse
  wire-up, N4 offline-degraded-validation wording, N38 GPO/ADMX/MSI/license tail, Tier-3
  pool; feature-matrix CSV id-uniqueness is NOT a contract (field-count pin only).
- **Residual-exec deferred set (all STAND):** P2 TSA test seam (`TimestampTransport`
  injectable transport — unblocks end-to-end B-T honesty tests; PGR-21/22 lanes are done,
  so it is no longer off-limits), P5c AP-stream BBox aspect on /Rotate, P5d PdfPageOps
  SafeSave candidate (design ready), P5g negative-/Rotate full grep audit, P7a /NM dedup
  two-lineage experiment, P7b `proofFailsOnXmpSurvivor` 50x+50x triage, P8 adversary
  hypotheses W1-H1/H2/H3 (H2 mechanism confirmed at `SendForSigningController.cpp:182/202`),
  P10-D1 **models bootstrap gap** (still the top devops finding —
  `bootstrap-vendor-deps.sh` has no models step; fresh clone cannot deploy), D3/D4 dead
  scripts (contested — migration-doc update first), B1 release-grade VCRT, P11 cleanup
  batch (TestLaneScheduler wall-clock bound, TestSignatureValidation merge-or-retire +
  tautology :132, TestEngineSave real-store QSettings, candidates-dir per-suite root),
  P14 perf tooling (tools/perf still lane-only), P15 soak/consolidation residuals
  (R25b re-soak at the consolidated head).
- **Architecture steps 3-9** (sequenced, not executed): BatchPresetPanel, HotFolderController
  (needs a TestHotFolder characterization pin FIRST), OpenRouteCoordinator (S4 seam pilot),
  UpdatePromptController, DocumentRecovery, ctor split, WelcomeTaskRouter; MainWindow
  service seam (S4); B2-family OcspConsent/OcspNetworkPolicyKey split into core.
- **The awaiting ledger itself:** ~276 implemented-awaiting-review rows on the PR (the
  unreviewed-map's 222 + fixall-era rows) — awaiting ≠ reviewed; the at-T map (§4.1) keeps
  the inventory honest.

---

## 6. HANDOFF TO CLAUDE — what to do on takeover

Order matters; the code is DONE and gated — what remains is accounting closure, the merge,
and the safety net.

1. **Read, in order:** `CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` (§9) → this file →
   the action plan + critique (after landing them on the PR) → the ledgers.
2. **Pre-merge closes (§4.1):** disposition `5461b72d`; ledger union proof; land the
   plan/critique docs; re-run E5. These are the only things between the current "0
   unexplained as of ledger §5 + 1 open item" and a clean merge claim.
3. **Verify the gates independently** (handoff §8 has exact commands): fresh Release build
   (expect 945/945, 0 errors); serial ctest (expect 100 %); `-j6` (expect exactly
   TestAccessibilityPanel + one flake red); E3 purge/linearity; E4 secret scan; E5
   accounting (after the closes); CI on the pushed head (Build + sanitizer green; Test
   reds = the four named items, artifacts attached). Each §4-row item re-checks against its
   commit + pin + evidence path.
4. **Take the §2 owner items** — the Panel contention segfault first (the only crash).
5. **Phase 0 safety net, then merge:** final `--all` bundle + SHA + missing archive tags +
   **push the tags** + restore drill; then merge PR #2 into `main` **keeping its commits**
   (FF push `review/consolidated-parity:main`; never force; PR #2 records merged).
6. **Post-merge:** gates at main's new head; deletion pass (fail-closed, per-class proofs,
   worktree-released first; HOLD set untouched); refresh the bundle one last time and
   re-verify the SHA; clean the worktrees (§4.5, incl. the staged-signing-keys quarantine).
7. **Schedule:** R25b re-soak at merged main; the §2/§3 queues to their lanes.

---

*Companion handoffs: `.context/program-consolidation-wip.md` (this lane's WIP);
`CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` §8 (gate reproduction) and §9 (takeover).*
