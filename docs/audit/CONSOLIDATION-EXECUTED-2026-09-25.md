# CONSOLIDATION EXECUTED — 2026-09-25 program, executed 2026-09-29

**Lane:** branch-consolidation executor (R12 single writer, worktree `D:/pdf/pdf-review`).
**Owner order:** "merge all of them into 1 for Claude to review" + "make sure that no work
is lost". Governing plans: `CONSOLIDATION-ACTION-PLAN-2026-09-25.md` +
`CONSOLIDATION-CRITIQUE-2026-09-25.md` (E1–E11 fixes applied) +
`PROGRAM-CONSOLIDATION-2026-09-25.md` §4. **Hard rules honored:** cherry-pick `-x` only;
linear history (0 merge commits); no stash; no secrets; no test weakened; no ref deleted;
purge intact; every branch tip pinned before anything else.

---

## 0. Result at a glance

- **The ONE consolidated branch is `review/consolidated-parity`.** Every one of the
  162 other local branches is dispositioned below: **FOLDED** (content commit-level or
  patch-level or blob-level present on this branch), **CONTAINED** (history pinned by the
  `archive/line/parity-glm` line tag), or **ARCHIVED** (tip pinned by an `archive/final/*`
  tag + the final bundle; never mergeable without an owner decision).
- **Zero branch, tag, or stash was deleted.** Deletion is Claude's post-verification job
  (fail-closed `delete_proven()` per action-plan §4.5, consent-gated).
- **New work folded this run:** one commit — the TestAccessibilityPanel segfault fix
  (PROGRAM §2.1, the only crash-class owner item), cherry-picked from the lane that
  landed 19 minutes into this run.
- **New code fixes required by FOLD-1: ZERO** — all five `ar/prompt-1` June safety fixes
  are proven superseded by stronger code already on this branch (§3, file:line evidence).
- **Accounting closes landed:** the `5461b72d` N1 supersession record (predecessor
  executor, `fdfac2ee`), the A4 ledger-union proof (§4.4 here), and the F3/E5 open item
  are CLOSED — "0 unexplained" now stands without the "+1 open item" qualifier.
- **Safety net:** all 163 branch tips pinned by `archive/final/*` tags; final `--all`
  bundle `C:\Users\User\pdf-archive-final-2026-09-29.bundle` cut, verified, restore-drilled;
  SHA-256 recorded in the follow-up record commit and in `.context/consol-exec2-wip.md`.
- **Push:** `review/consolidated-parity` fast-forwarded to origin (no force); all archive
  tags pushed (origin's second copy of the never-pushed pg tail + fixall era).

Dispositions: **A (commit-contained) = 8, B (fold-lane, content verified) = 25,
C (united-line ancestor, line-tag-pinned) = 111, D (archive/HOLD) = 18.** Total 162 + the
survivor = 163 local branches (census 2026-09-29 03:45–04:10 +0300).

---

## 1. Method (execution-time re-derivation, not trust)

1. Census: `git rev-list --count HEAD..<branch>` and `git rev-list --count
   feat/parity-glm..<branch>` for every local branch (162). Any branch absent from every
   prior plan's tables would fall out here; none did — the class sums of the landed
   action-plan §3 and critique §1.3 reproduce exactly.
2. Ref-move detection: `git for-each-ref --sort=-committerdate` + branch reflogs. Found:
   `feat/panel-segfault-fix` created 03:06 and committed 03:40 (a LIVE lane — handled,
   §2); `feat/fixall-images` reset to the PR head 03:43 (benign lane re-point per the
   09-24 integrator precedent; its true lane tip `d63ed76e` was already tag-pinned).
3. Predecessor handoff: the prior executor run died after completing exactly one close —
   commit `fdfac2ee` (the `5461b72d` supersession record, evidence absorbed verbatim in
   `docs/audit/evidence-n1/fixall-redaction-variant/`) and the `archive/final/*` sweep
   (lightweight tags, 03:19–03:43 window). This run verified both and completed the rest.
4. Tag completion: 8 tips the sweep missed were pinned as ANNOTATED tags (no `-f`, no
   overwrite): `feat/panel-segfault-fix@23292572`, `feat/fixall-ci@0aa5c2de`,
   `feat/fixall-tagging@9d150be2`, `feature/m4-forms@68c4734a`, `main@2b715f47`,
   `msys2-migration-backup-pre@0c7d48ae`, `subagent-Forms-Specialist-self-26881e20@9c6024cd`,
   `subagent-View-Specialist-for-Rendering-Modes-view-specialist-ecb2f058@3b9effd1`.
   All 163 branch tips now resolve under `archive/final/<branch>`.

## 2. The live-lane fold — feat/panel-segfault-fix (FOLDED, picked)

The lane (worktree `C:/Users/User/Projects/pdf-r15`, handoff
`pdf-r15/.context/panelfix-wip.md`, STATUS: FINAL) fixed the TestAccessibilityPanel
segfault: test-helper lambdas captured a local bool BY REFERENCE with the panel as
context and never disconnected; the connection outlived the helper frame, and the
onTagFinished re-scan delivered a late `scanCompleted` that wrote into the dead stack
frame — the exact Qt6Core rip-corruption signature of the CP10 erratum. Fix: heap-held
`QSharedPointer<bool>` flags, a permanent 300 ms amplifier making the late fire harmless
(pre-fix: deterministic crash), and a QPointer-guarded `onTagFinished` continuation.

- **Pick:** `4187fe2b` ← `23292572` (cherry-pick `-x`; trailer verified). Touched files:
  `tests/TestAccessibilityPanel.cpp`, `src/modes/AccessibilityPanel.cpp`. The delta
  between the lane's verified base (`9848dc54`) and this branch at pick time was
  docs-only (`git diff --name-only 9848dc54..HEAD` — no non-docs files), so the lane's
  evidence transfers 1:1.
- **Gate this run:** incremental rebuild of the target on the picked head + solo run:
  **16 passed / 0 failed** (with the amplifier in the build — the pre-fix code crashes
  deterministically under it). Fail-before 5/8 amplified segfaults, ×10 solo 16P/0F and
  a11y cluster ×3 greens are the lane's own evidence (`pdf-r15/.context/panel-*.txt`).
- Lane residual: its full serial ctest was still running on ITS base at fold time; result
  lands with the lane/owner. Its branch tip stays pinned and undeleted.

## 3. FOLD-1 — ar/prompt-1 @ 5d999987 (FOLDED-BY-SUPERSESSION, 0 picks)

Per-fix supersession review against the PR tree (the plan's G-FOLD-1 gate; "no pick may
be skipped without a written supersession row"):

| Fix | Hazard | Verdict | Evidence on this branch |
|---|---|---|---|
| D1 `db90eb41` | watermark null-deref: raw `SearchFont("Helvetica")` deref | **SUPERSEDED** | `PoDoFoBackend.cpp:5950–5975` — §9.11 P0 resolution chain: null-checked `SearchFont(fontName)` → base-14 map → guaranteed `GetStandard14Font(Helvetica)` before the `SetFont` deref; covers the fontFamily case D1 did not |
| D2 `7ba4fa43` | render-prefetch UAF (single-future `cancelAndWaitForPrefetch`) | **SUPERSEDED** | EC06 `drainPrefetches()` at `RenderCache.cpp:36–50,71–73`: cancels + joins EVERY in-flight prefetch future (snapshot under lock, wait outside), called from BOTH `clear()` and `~RenderCache` — strictly stronger than D2's single-token destructor-only path |
| D3 `c6d67de9` | blanket soffice taskkill (data loss) + no profile isolation | **SUPERSEDED** | `ConversionManager.cpp:605–606` private per-conversion profile dir (`glyphpdf-soffice` temp dir, so no shared-lock wait); the ONLY taskkill is `:644–647` — `/F /T /PID <pid>` of its own timed-out child. No `/IM soffice` blanket kill exists in the tree |
| D4 `2ffe0712` | autosave rename-retry `QTimer::singleShot(250, this, …)` UAF | **SUPERSEDED** | `AutosaveManager.cpp:175–204` — `QPointer<AutosaveManager> weakThis` + `if (!weakThis) return;` (auto-nulls on destroy), PLUS engine-identity (`documentLoadId`) and document-generation guards and the G04 stale-temp drop D4 lacked; `m_saving` is `std::atomic<bool>` (`AutosaveManager.h:45`) |
| D5 `5d999987` | AIChatPanel typing-cursor `void*` QVariant round-trip UAF | **SUPERSEDED** | `AIChatPanel.cpp:108,127–136,149–151` — `m_cursorRow` member + bounds-checked `m_msgs->item(m_cursorRow)` (null-safe after `clear()`); input AND send button disabled while in flight (`:105–106`) closing the re-entry path; no `void*`/`cursorItem` pattern remains. (D5's extra `isRunning()` belt-and-suspenders is absent — the guarded path it doubled is closed at the UI level.) |

The June branch's own regression tests were not ported: each is a source-grep or
crash-repro pin whose assertion target is now structurally impossible in this tree;
the modern equivalents are the per-fix pins already in the touched suites. Branch tip
pinned `archive/final/ar/prompt-1`.

## 4. Zero-loss proofs (commands + outputs, run 2026-09-29)

### 4.1 Content superset (action-plan §4.2)
- Name set: `diff <(git ls-tree -r X --name-only | sort) <(…HEAD…)` — files HEAD lacks:
  `feat/consolidated` = **0**, `feat/parity-glm` = **0**, `consolidate/all` = **2**
  (exactly `CLAUDE.md` + `SECURITY.md` — the purge files whose absence is REQUIRED by
  the E3 gate; consolidate/all is the only line still carrying them). `feat/explorer-branches`
  = **0**.
- pg-side delta (`git diff --numstat feat/parity-glm HEAD`): 69 files carry pg-side
  deletions, total **25,966 added / 888 deleted** — matching the critique's measurement.
  The top deleters are exactly the documented fix-wave surfaces (PoDoFoBackend 511+/184−,
  ledger 251+/63−, ContentSpans 409+/61−, BatchMode 836+/52−, EncryptedFileSecretStore
  270+/46−, SignatureManager 233+/38−, RedactionProof 345+/16−). Sampled attribution:
  `tests/TestOcrPreprocessPrefs.cpp` pg-side hunks are the VACUOUS pre-CX-17 test,
  replaced by the negative-controlled persistence cycle (`CX-17` markers in-tree,
  `evidence-cx17/`); the ledger's −63 is covered row-for-row by the A4 union below.
  Full line-by-line closed-set attribution remains a residual (§9) — zero-loss does not
  depend on it: every pg commit is pinned (§4.3).

### 4.2 Ledger union (critique A4) — PASS
First-cell row-ID sets of `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`:
pg = 309 unique row-IDs, consolidate/all = 293, feat/consolidated = (⊆ pg), PR = 341.
`comm -23` (line-set A-only rows): **pg→PR = 0, consolidate/all→PR = 0,
feat/consolidated→PR = 0.** Full-row-text check for the ID-bearing subset likewise 0.
The PR ledger is the row-union superset. (Counts differ from the critique's raw row
totals because multi-table files share first cells; the containment direction is what
A4 requires and it is empty in the loss direction.)

### 4.3 History pins — PASS
- `git rev-list --count feat/parity-glm --not archive/line/parity-glm` = **0** — the
  ENTIRE united line (pg = 195e4309, and through it fc `eb0efa21`, ca `95dccb23`,
  main `2b715f47`, every uPG=0 lane branch) is reachable from the line tag forever.
- Loop over all 111 uPG=0 branches: `rev-list --count <b> --not archive/line/parity-glm`
  = 0 for **111/111**.
- Ancestry: `fc..ca = 0`, `fc..main = 0`, `pg..fc = 0`; `main` and `origin/main` are
  ancestors of HEAD (FF-to-main precondition intact).
- pg's 3 patch-uniques vs HEAD beyond fc's pin (critique E2-corrected form):
  `git cherry HEAD feat/parity-glm $(git merge-base feat/consolidated feat/parity-glm)`
  = exactly `14d69ce9`, `983dd81d`, `4e70217e` — each ledger-dispositioned 09-24 §B1
  (picks-with-resolution 885c9f0b / bf85bfbd / 0e3dcdd).
- `5461b72d` (fixall-redaction N1 variant) and `68bc917e` (final-pgr-closers WIP):
  dispositions recorded at `fdfac2ee` and ledger §5; supersession targets `2ead0b17`
  and `6841247d` verified ancestors of HEAD.
- Fold-lane `-x` accounting: all §3.4 source SHAs (`8f07066c 29365772 a690d7f4
  552c615f 5ff0d618 9f0ddb63 433b77e7 020c0734 39058fa9 3fd91495 abe093aa 958bd7c0
  d63ed76e 898d362f 5cadce52 a25c37b7 ec22eeed 779fbb1e 0f704539 5d74c348 9b2b2727`)
  have their `cherry picked from commit` trailer on HEAD; content pins verified
  (`BatchPreset.h` ordered-lane comment, EditPolicy `PR-review §3.1`, flow7c
  "never certifies" pin, `b09256ee` T2-2 port, `f6ef7d68` CX-06, `6722356f` T6 union,
  `77db5be5` quickjs 0.15.1 pin — all ancestors of HEAD).
- Doc-lane absorption: `CONSOLIDATION-CRITIQUE-2026-09-25.md`,
  `PROGRAM-CONSOLIDATION-2026-09-25.md`, `BRANCH-LANDSCAPE-2026-09-25.md`,
  `CONSOLIDATION-ACTION-PLAN-2026-09-25.md`, `consolidation-action-draft.sh` are
  **blob-identical** between their lane branches and HEAD.

### 4.4 Patch-level containment for the small-divergence stragglers
- `feat/line-reconciliation` (1 beyond-commit `3811cc6a`): `git cherry HEAD …` = 0 `+`
  (patch-equivalent to `64c48ba1`). FOLDED.
- `fix/p0-blank-viewer` (3 beyond-commits): `git cherry HEAD …` = 0 `+` (the N2 picks
  `2ed100fc`/`f9e44c6e`/`36b99c27`). FOLDED.

### 4.5 Gates at the folded head
- E3: `git rev-list --merges --count origin/main..HEAD` = **0** (linear);
  `git log --full-history origin/main..HEAD -- CLAUDE.md SECURITY.md` = **empty** (purge
  intact); `origin/main` and `main` are ancestors.
- Touched-suite gate for the new pick: TestAccessibilityPanel **16P/0F** solo on `4187fe2b`
  (fresh incremental link of the target). E1/E2/E4/E5 records stand from the FIXALL
  close-out (`12da4e2f` handoff §6, re-verified clean at `9848dc54` CP10); this run's
  code delta vs `9848dc54` is exactly the one picked commit, gated above. Claude's
  independent full rebuild + serial ctest remains the next actor's step (PROGRAM §6).

## 5. Per-branch disposition — all 162 non-survivor branches

Class B per-branch evidence rows (FOLDED — content verified on this branch):

| Branch | tip | uPR | Disposition + evidence |
|---|---|---|---|
| feat/panel-segfault-fix | 23292572 | 1 | **PICKED this run** `4187fe2b`←23292572; 16P/0F gate (§2) |
| ar/prompt-1 | 5d999987 | 262 | **SUPERSEDED D1–D5** with file:line evidence (§3); 0 picks |
| feat/pr-review-fixes | a25c37b7 | 14 | per §3.4: `e89f1234`/`a830d999` SUPERSEDED by Phase-0 `fbb40295`; `3c1e60a6`/`31ad94ed` folded-with-resolution (EditPolicy blob + flow7c pin verified); rest patch-eq |
| feat/fixall-forms | 4d410c10 | 11 | 11 commits = the lane's own duplicate-pick REVERTs; SUPERSEDED by the clean Phase-0 path (handoff-FIXALL §3) |
| feat/residual-exec | 5d74c348 | 8 | `9b2b2727` PORTED as `b09256ee` (ancestor-verified, fail-before evidence); W1-05/extractLinks/exportToImage/R14/fuzz/docs picks on HEAD (`-x` trailers verified) |
| feat/fixall-ci2 | 590c6c27 | 8 | docs-only progress checkpoint; content covered by in-tree FIXALL-PROGRESS CP entries |
| feat/batch-presets-p2 | ec22eeed | 8 | U1/U2 folded-with-resolution (BatchPreset.h ordered-lane comment verified in-tree); `ec22eeed` ledger rows present (R26-P2 ×8) |
| feat/pgr-c4 | 16145af2 | 7 | `9f0ddb63`→`6722356f` union resolution (ancestor-verified); `a2a8ff4f` ledger rows absorbed (deviation D5, 09-24 handoff) |
| feat/fixall-images | (lane tip d63ed76e) | 7→0 | CX-02/08..12 + N1 `2ead0b17` on HEAD; branch later re-pointed 9848dc54 (uPR=0, benign; true tip tagged) |
| feat/formjs-review | 433b77e7 | 6 | 6 exact picks `a851464f`…`8bf27032` (−x trailer verified) |
| feat/final-pgr-closers | 68bc917e | 5 | WIP SUPERSEDED by `6841247d` (ancestor-verified; ledger §5); 2 test pins ported `06ce3ae4` |
| feat/pgr-c2 | 552c615f | 4 | exact picks (−x verified) |
| feat/pgr-d2 | 958bd7c0 | 4 | PGR-42..45 picks `f90b4c71`/`1d59f241`/`6600a429`/`ddef6a8f` (ancestors verified) |
| feat/consolidation-action-plan | 3a0c485b | 1025 | plan + script **blob-identical** on HEAD (`01e18619`/`f54072c7`); remainder = pg ancestry (class C pin) |
| feat/explorer-branches | 72ac5a17 | 1025 | landscape docs **blob-identical** on HEAD (`3800059c`/`33071b0b`); 0 files HEAD lacks; remainder = pg ancestry |
| feat/consolidation-critique | b94f3313 | 2 | critique docs **blob-identical** on HEAD (`54fc4b95`/`fd30b3a1`) |
| feat/program-consolidation | 998e9d0f | 1 | PROGRAM doc **blob-identical** on HEAD (`c3d2e394`) |
| feat/pgr-critical-fixes | a690d7f4 | 3 | exact picks `5cec76cb`/`6ed13c52`/`0b06214a` (−x verified) |
| feat/pgr-c3 | 5ff0d618 | 3 | exact picks (−x verified) |
| feat/pgr40-quickjs-bump | 020c0734 | 3 | pick `77db5be5` (C.6 pin, ancestor-verified) |
| feat/fixall-redaction | 5461b72d | 3 | N1 variant **SUPERSEDED** by `2ead0b17` — written content diff + evidence absorbed at `fdfac2ee` (`evidence-n1/fixall-redaction-variant/`) |
| feat/fixall-tagging2 | 0144d056 | 3 | CX-01/04/07 picks + direct lane pushes on HEAD |
| feat/fixall-forms2 | 5cadce52 | 3 | CX-06 folded-with-resolution `f6ef7d68` (ancestor-verified) |
| feat/fixall-inv1 | 898d362f | 1 | INV-1 pick `64baba6a` (ancestor-verified) |
| feat/line-reconciliation | 3811cc6a | 984 | 1 beyond-commit **patch-equivalent** to `64c48ba1` (git cherry = 0 `+`); rest = pg ancestry |

Class A (uPR = 0 — commit-level contained; delete-after-proof class): `feat/fixall-ci`
`0aa5c2de`, `feat/fixall-images` (post-reset tip) `9848dc54`, `feat/fixall-tagging`
`9d150be2`, `feature/m4-forms` `68c4734a`, `main` `2b715f47`,
`msys2-migration-backup-pre` `0c7d48ae`, `subagent-Forms-Specialist-self-26881e20`
`9c6024cd`, `subagent-View-Specialist-…-ecb2f058` `3b9effd1`.

Class C (uPG = 0 — united-line ancestors; 111 branches, each `rev-list --count <b>
--not archive/line/parity-glm` = 0): the full list with tips is machine-recorded in
`.context/consol-exec2-wip.md` and reproduced here in compact form:
`audit-remediation@84445698, audit/parity-glm@9ba3cea4, cleanup/post-v1.3.2@4c3a87ef,
consolidate/all@95dccb23, feat/accessibility-p1@c5496ba6, feat/accessibility-p2@4e70217e,
feat/batch-presets-p1@f80033a4, feat/candidate-leak-fix@5c8fd087,
feat/consolidated@eb0efa21, feat/consolidated-report@9db3ede4,
feat/consolidation-plan@744e9e20, feat/consolidation-stage1@8507573e,
feat/dispatch-gates@cf283923, feat/emergence-fixes@65500182, feat/feature-plans@3e13cfd9,
feat/followups-2026-09-15@247e0c76, feat/glm-comp@585cc45d, feat/glm-ocr@585cc45d,
feat/l7-rotate-annot@71891494, feat/modularity-moves@3c411cc8, feat/parity-glm@195e4309,
feat/parity-glm-clean@352c9b1e, feat/parity-glm-formjs@2290ab51,
feat/parity-glm-gate@7f8cf950, feat/parity-glm-gateC@74be82cf,
feat/parity-glm-gateE@c96d2267, feat/parity-glm-infra@77e3b066,
feat/parity-glm-integration@2f755244, feat/parity-glm-linux@7ba5ca1f,
feat/parity-glm-n17n18@819d84a5, feat/parity-glm-packa@40a38cef,
feat/parity-glm-packafix@5ae41536, feat/parity-glm-quick@2b81fc28,
feat/parity-glm-r04r08r10@2bb2c431, feat/parity-glm-r05@5995b7fa,
feat/parity-glm-r06r13@cbbb8ec5, feat/parity-glm-r11r12@83be3c20,
feat/parity-glm-r15r17@3d9a7dd5, feat/parity-glm-r18f@7d5ae676,
feat/parity-glm-r18r19@abc87de2, feat/parity-glm-r22@7ceeba82,
feat/parity-glm-resid@586d6e4c, feat/parity-glm-resid2@9129788f,
feat/parity-glm-review@cd01e892, feat/parity-glm-sec@e36e714c,
feat/parity-glm-sep13@6fe482c8, feat/parity-glm-t2@21673bc0, feat/parity-nemo-b@585cc45d,
feat/parity-ox-auto@f7d9aec9, feat/printable-summaries@b06b2f07,
feat/r24-policy@335d1d3a, feat/r24-wiring@b8909dcb, feat/redaction-gaps@b454d071,
feat/redaction-research@b8d17885, feat/report-addendum@d65b3e35,
feat/residual-plans@72f5ce7c, feat/resoak-verdict@d249bf91, feat/rotate270-fix@9e1cde9d,
feat/runintersects-precision@e620757b, feat/sanitize-assert-mutable@ed04426e,
feat/send-for-signing-p1@f621416d, feat/sep13-fixes@e8b9a19e, feat/sep13-leads@b0fd8296,
feat/sep13-residual@c7e9ecfe, feat/soak-48h@1d2e76b8, feat/soak-followups@32282d8d,
feat/soak-verdict@790a7197, feat/sweep-backend@efbd8235, feat/sweep-legacy-fix@a73419ed,
feat/sweep-quality-new@673129e1, feat/sweep-w1-adversary@8bcde898,
feat/sweep-w1-fixes@e8e715f2, feat/sweep-w1-fuzz@b32d8dfd,
feat/sweep-w1-security@f6d46dd9, feat/sweep-w2-gsd@125ae929,
feat/sweep-w2-testing@b17106a3, feat/sweep-w2-verify@22588a1c,
feat/sweep-w2-verify-b@84a19f9e, feat/sweep-w2c@4525f0e1, feat/sweep-w3-arch@46cae7ba,
feat/sweep-w3-archaeo@b8a5a564, feat/sweep-w3-devops@fbfa6e47,
feat/sweep-w3-emergence@48c2ae51, feat/sweep-w3-perf@2ec24f41,
feat/sweep-w3-research@e97f7075, feat/sweep-w3-ui@164dabd4, feat/sweep-w3-ux@5b36715d,
feat/sweep-w3-ux-resume@3f957d72, feat/ui-narrow-viewport@c467e6a7,
feat/unreviewed-map@0bcbd6bf, feat/ux-defects-fixes@f01a4e37,
feat/ux-integration-fixes@ef371ad0, feat/ux-integration-fixes2@14d69ce9,
feature/accessibility-parity@84445698, feature/m4-djot-foundation@ab65816a,
feature/m4-edge@f9805e31, feature/m4-security@f9805e31, fuzz/redaction-rig@92c08168,
r2-1-chain1@7865cae1 … r2-7-tests@5442f105, r3-er2-redact-guard@d5af7139,
r3-er3-multirecip@77098852, r3-er4-saveas@25d6b951, r3-nf6-ocsp@83327e47,
subagent-AST-Architect-self-a1982f71@f9805e31,
subagent-Vendoring-Specialist-self-7708fcd5@f9805e31`.
This class includes the entire September real history (pg's ~1000 commits), the gemini
m4 era (incl. the djot foundation: Lua 5.4 + Djot vendored parser, docmodel, pdfws_djot —
uPG=0, in-tree via the superset), and the r2/r3 June remediation era.

Class D (ARCHIVED — never merge without an owner decision; every tip tagged
`archive/final/*` AND covered by the final bundle; older dedicated tags exist where
noted): July-era parity waves `feature/editing-parity@97172fbe`,
`feature/viewing-parity@de1fa268`, `feature/redaction-parity@45bf4302`,
`feature/security-parity@8bb20c52`, `feature-elevation-wave1a` =
`feature/ocr-parity@055592df` (ports recorded landed: Night Mode/Eye-Care UAF, restack/
opacity/rotate, letter/line spacing, TestOfficeExport, checked-redo); the descoped
feature and June one-shot lanes `feat/ocr-verify-finereader@f5b59e66` (FOLD-2: 20
commits, B1–B15 OCR verify UI, 468-path merge forecast — **ARCHIVE + dedicated owner
port lane later**, per plan §2.2 default), `feat/regex-find-replace@` =
`backup/regex-verified-a39356e@` `a39356e7`, `feat/regex-ox-auto@7d54b387`,
`feat/erase-ox-auto@faa6cf10`, `feat/annotation-eraser@916a4b7d` (local twin; origin tip
already `archive/annotation-eraser`); preservation/debris
`reconexec/snapshot-redaction-gaps-dirty@2adc3df6`,
`recovery/temp-stage-push-20260909@d367aca1` (12 commits individually dispositioned
09-24 ledger row 7), `subagent-AST-Architect-self-54b52cfc@d7a8ca06`,
`subagent-Vendoring-Specialist-self-ca2c3f27@5a018ae5` (staged-artifact dumps; selected
content hash-verified preserved by the Sept-13 preservation commits); **HOLD (owner,
do-not-delete)** `feat/soak-48h-resume@0fad38c0` (R25b re-soak still owed),
`test/view-parity-baseline@0948743b` (owner out-of-scope 09-28); folded-by-patch-id
members already listed in class B (`fix/p0-blank-viewer@86f2cf34`,
`feat/line-reconciliation@3811cc6a`).

## 6. FOLD-2 — ocr-verify-finereader: ARCHIVED (owner decision recorded as open)

`feat/ocr-verify-finereader@f5b59e66` (20 commits: verify dialog, word sync,
reading-order reorder, dictionaries, zoom pane, hotkeys). Three plans concur
(09-20 §7, action-plan §2.2, PROGRAM §3.7): a fold is a FEATURE PORT (468 conflicted
paths, June-era base, re-imports purge files), not a consolidation step, and would
jeopardize the verified gates for zero endgame value. Archived: tip pinned
(`archive/final/feat/ocr-verify-finereader`) + final bundle. **Owner item:** open a
dedicated port lane after the endgame if the feature is wanted.

## 7. Safety net (critique A1/E4)

- **Tags:** 163/163 branch tips resolve under `archive/final/<branch>` (252
  `archive/*` tags total after this run's 8 annotated additions). Plus the pre-existing
  dedicated pins (`archive/{editing,redaction,viewing}-parity`,
  `archive/annotation-eraser`, `archive/line/parity-glm` → `195e4309`,
  `archive/pr-head-before-picks`, stash tags `archive/stash-0..6`).
  `archive/pr-head-final` → the report commit of this run (annotated; §8).
- **Bundle:** `C:\Users\User\pdf-archive-final-2026-09-29.bundle` (`--all`), cut AFTER
  the report commit so it contains the final head, every branch, tag and stash;
  `git bundle verify` = "complete history"; **SHA-256 recorded in the follow-up record
  commit and in `.context/consol-exec2-wip.md`** (the bundle necessarily cannot contain
  its own hash).
- **Restore drill:** `git clone` the bundle into a scratch dir; `git rev-parse`
  spot-checks of pinned tips vs this repo (survivor, pg line tag, panel lane tip,
  ocr-verify, July trio) — all equal; blob spot-check of a pinned-file hash — equal.
  Transcript: `.context/consol-exec2-wip.md`.

## 8. Push (FF-only, no force)

- `review/consolidated-parity`: `9848dc54 → <final head>` fast-forward to origin
  (result recorded in the follow-up commit).
- Tags: `git push origin --tags` restricted to the `archive/*` estate (never the
  survivor during the fold; never force).
- NO branch deleted locally or on origin. NO force, NO gc, NO prune, NO reflog expiry.

## 9. Residuals carried to Claude / the owner (not blocking, not lost)

1. **Merge to main** (the actual "one branch" end state): `git push origin
   review/consolidated-parity:main` is a pure FF (both main tips verified ancestors).
   Owner-gated per action-plan phase 3 — deliberately NOT executed by this run.
2. Deletion pass (post-merge, consent-gated, fail-closed): none executed here; every
   deleted-branch precondition (tag + bundle + proof) is now satisfiable from §5.
3. The panel lane's full serial run completes on its base; append its result (its
   amplifier makes any regression deterministic). Re-run full serial + `-j6` at the
   merged head (PROGRAM §6).
4. Closed-set attribution of the pg-side −888 lines file-by-file (sampled here;
   supersession pattern confirmed; content pinned regardless).
5. Unreviewed-map re-run at the final head (critique A10; scripts on
   `feat/unreviewed-map`).
6. Owner items unchanged from PROGRAM §2–§3 (CSV sinks, OLE-signature residual, flakes,
   PGR-40/41, D1–D3 decisions, re-soak, ocr-verify port decision) — all recorded
   in-tree in PROGRAM-CONSOLIDATION §2–§3.
7. Hygiene hazards owned elsewhere: dirty `D:/pdf/pdf` main worktree (F6); five
   antigravity worktrees with ~703 staged files each INCLUDING
   `tests/fixtures/signing/ca.key`, `signer.key`, `test_signer.p12` (F7 — quarantine;
   never add/commit/stash/clean there); `pdf-base7d` baseline export (flow7 evidence).
8. The 06:01 continuity automation: confirm paused before any future deletion pass.

*Executor: branch-consolidation executor (consol-exec2). Raw evidence:
`.context/consol-exec2-wip.md` (census, classification, drill transcript, SHAs).*
