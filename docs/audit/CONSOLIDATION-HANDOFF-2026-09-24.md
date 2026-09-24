# Consolidation handoff — 2026-09-24 (Phases C/D/E/F close-out)

Single-writer integrator run (R12), branch `review/consolidated-parity` (PR #2),
worktree `D:/pdf/pdf-review`. Predecessor: `.context/integrator-wip.md`
(Phases A/B close-out). This file is the final handoff per §8. **The run stops
here: nothing is merged, no branch deleted, `main` untouched.**

## 1. Final state

- **Final PR head: the handoff commit itself** — see `git log --oneline -1`
  (parent `6f0ab646` docs row-append; before that `77db5be5`, the C.6 pick).
- 24 commits added this run on top of Phase B's `7d4d8d08`: 22 cherry-picks
  (all with `-x` source trailers) + docs commits (`83289757` Phase C/D tables,
  the ledger row-append, this handoff). History **linear**:
  `git rev-list --merges origin/main..HEAD` = 0;
  `git merge-base --is-ancestor origin/main HEAD` = true; purge intact (no
  `CLAUDE.md`/`SECURITY.md` in any tree or message in `origin/main..HEAD`).
- Phase B summary (prior run, unchanged): 14-branch disposition ledger
  `docs/audit/CONSOLIDATION-LEDGER-2026-09-24.md`, **0 unexplained commits**;
  FOLDED 2 (accessibility-p2, consolidated), PRESENT 6, SUPERSEDED 3,
  ARCHIVE-ONLY 3; recovery's 12 commits individually dispositioned.

## 2. Gate record (E0–E6)

| Gate | Result |
|------|--------|
| E0 hygiene | **PASS** — no new tracked build logs (only pre-existing `fuzz/findings/*` campaign logs, kept per brief); lane evidence transcripts under `docs/audit/evidence-pgr-c4/`, `docs/audit/evidence-formjs-2026-09-23/` are findings evidence |
| E1 fresh Release build | **PASS** — `build-final` configured + built from scratch, 1024/1024 targets, exit 0 |
| E2 tests | **PASS with disposition** — see §4 below; touched suites 3× green (18/18 × 3) |
| E3 purge | **PASS** — trees + log clean, ancestry OK, 0 merges |
| E4 secret scan | **PASS** — 149,044 added lines scanned for AKIA / `ghp_` / `github_pat_` / `sk-` / `xox?-` / AIza patterns: **0 matches** |
| E5 ledger | **PASS** — 22/22 commits accounted (21 brief-listed picks + docs); ledger rows appended to `CURRENT-EVIDENCE-LEDGER`; per-ID table `docs/audit/PGR-STATUS-2026-09-24.md` |
| E6 push + PR | Pushed fast-forward after every branch integration (R3); PR #2 body updated via `gh pr edit 2 --body-file`; CI checked (guards green; `build-and-test` recorded per-run below) |

### E2 detail (this machine, UCRT64, offscreen, 2026-09-23/24)

- Full `ctest -j6` (build-final): **172/179**, 7 failures → classified:
  5 transient-load (TestWelcomeRoutes, TestFormJsCalc, TestRedactMarkAll,
  TestSecretStore timeout, TestReadOnlyGate) — all pass on individual rerun;
  TestCommandBinding transient (passes solo, 11P, and on baseline);
  TestSweepW3UxFlows flow7 — **pre-existing, reproduced identically on the
  `7d4d8d08` baseline** (below).
- Full serial ctest (build-final), run 1: **175/179** (TestEncryption 0.18s,
  TestSecretStore 4.93s, TestCommandBinding 1.19s all pass solo immediately
  after); run 2: **177/179** (flow7 + one TestFormJsCalc load flake that passes
  solo 49P seconds later and 3× in the loop below).
- **Post-C.6 re-gate on the final tree (quickjs 0.15.1, pin enforced 0.15.1):**
  full serial `ctest` = **178/179** — the single failure is the dispositioned
  flow7 (see baseline experiment below); touched suites achieved **3
  consecutive green runs (18/18 = 100% each)** on this tree; both build dirs
  rebuilt 885/885 against 0.15.1 with zero golden drift.
- **Touched suites, 3 consecutive green runs: 18/18 = 100%, three times**
  (TestSignatureRealCrypto 26P/0F/1skip, TestEngineSave 22P, TestSecretStore
  21P, TestConversionExtraction 18P, TestCommentsReview 10P, TestOcrReview-
  Lifecycle 34P, TestMeasureCore 22P, TestRedactionProof 28P, TestSep13Lead-
  RedactionProof 11P, TestRedactTransaction 42P, TestSep13LeadComparePerf 4P,
  TestCompareEntry 26P, TestCompareIntegration 8P, TestOllamaProvider 63P,
  TestOcrVerifyNavigation 16P, TestFormJsAdversarial 24P/0F/1skip disclosed,
  TestFormJsCalc 49P, TestFormKeystroke 9P).
- **Baseline experiment** (instrument from the line-reconciliation precedent):
  detached worktree `D:/pdf/pdf-base7d` @ `7d4d8d08` (pre-Phase-C PR head),
  fresh `build-base`. `TestSweepW3UxFlows::flow7_accessibility_scan_fix_rescan`
  fails there with the **identical assertion** (`disc.contains("Detection
  only") && disc.contains("never certifies")` FALSE, 11P/1F); TestReadOnlyGate's
  `readExpiryDate(dest).isValid()` flake reproduces there too; TestCommandBinding
  passes on both sides. **Conclusion: zero Phase C test regressions; the
  residual batch failures are machine-profile/load flakes of the same class the
  line-reconciliation run dispositioned against `ef371ad0`.**
- CI: PR #2 checks green at push time for guards; `build-and-test` recorded
  per-push (interruptible machine-local runs are independent of it).

## 3. Phase C — every PGR ID (authoritative long form: `docs/audit/PGR-STATUS-2026-09-24.md`)

Picks by branch (order as landed; formjs applied in **branch order** — see
deviation D1):

| Branch | Picks (PR SHA ← source) | Fixes | Gate |
|---|---|---|---|
| feat/pgr-critical-fixes | `5cec76cb`←8f07066c, `6ed13c52`←29365772, `0b06214a`←a690d7f4 | PGR-21 CRITICAL, PGR-22, PGR-06 | build 222/222; TestSignatureRealCrypto 26P/0F/1skip; TestEngineSave 22P/0F; pushed |
| feat/pgr-c3 | `2d893a21`←d71d07e7, `99f8d3a6`←fafea21b, `37561af9`←5ff0d618 | PGR-20, PGR-25, PGR-26 | TestSecretStore 21P/0F; pushed |
| feat/pgr-c2 | `17e759f5`←8779f18b, `7d6a1d83`←cc2d19e1, `0ce53c76`←deff3877, `e233e58b`←552c615f | PGR-16, PGR-17, PGR-18, PGR-19 | ConvExtract 18P, CommentsReview 10P, OcrReviewLifecycle 32P, MeasureCore 22P; pushed |
| feat/pgr-c4 | `862f9d5c`←eede8b0f, `95352e53`←a5addfbb, `9e9e987a`←280a17ff, `9344eae5`←632b93ac, `6722356f`←9f0ddb63 | PGR-10 + triage T1/T3/T7/T6 | family 247P/0F across 10 suites; pushed |
| feat/formjs-review | `a851464f`←697e7dcf, `40c9c382`←155f3bb7, `abe351cb`←dc3240e9, `b5c63cbc`←77b57a7e, `82f309ab`←f6e1953c, `8bf27032`←433b77e7 | adversarial suite + PGR-35…39 + threat model/§10 | Adversarial 24P/0F/1skip, FormJsCalc 49P, FormKeystroke 9P; pushed |
| feat/pgr40-quickjs-bump (C.6, coordinator-directed) | `77db5be5`←020c0734 | quickjs-ng pin coordination 0.15.0 → 0.15.1 (PGR-40 stays DEFERRED — check lane proved the fix is not in 0.15.1 via same-machine A/B probe) | both build dirs rebuilt 885/885; staged `libqjs-0.dll` sha256 `cc92ba7e…` = 0.15.1-1 hash-verified; FormJsCalc 49P, Adversarial 24P/0F/1skip (pin skip-arms), Keystroke 9P; pushed |

Status table (ID → status → fix SHA on PR → test):

| ID | Status | Fix on PR | Test |
|----|--------|-----------|------|
| PGR-01…05, 07, 08, 09, 11…15, 24, 27…32 | Fixed (on parity-glm line, pre-Phase C; twinned) | in PR ancestry (report §3/§6b evidence) | report §3 + §6b named suites |
| PGR-06 | **Fixed** | `0b06214a` | TestEngineSave 22P |
| PGR-10 | **Fixed** (unverifiable verdict) | `862f9d5c` | TestRedactionProof 28P; Sep13LeadRedactionProof 11P; RedactTransaction 42P |
| PGR-16 | **Fixed** | `17e759f5` | TestConversionExtraction 18P |
| PGR-17 | **Fixed** | `7d6a1d83` | TestCommentsReview 10P |
| PGR-18 | **Fixed** | `0ce53c76` | TestOcrReviewLifecycle 34P |
| PGR-19 | **Fixed** | `e233e58b` | TestMeasureCore 22P |
| PGR-20 | **Fixed** | `2d893a21` | TestSecretStore 21P |
| PGR-21 | **Fixed** (CRITICAL) | `5cec76cb` | TestSignatureRealCrypto 26P |
| PGR-22 | **Fixed** | `6ed13c52` | TestEngineSave 22P |
| PGR-23 | **OPEN** (HIGH) — no fix in any lane list | — | — |
| PGR-25 | **Fixed** | `99f8d3a6` | TestSecretStore 21P |
| PGR-26 | **Fixed** | `37561af9` | TestSecretStore 21P |
| PGR-33 | **Open as-is** (owner decision, archived) | — | — |
| PGR-34 | Fixed (Phase B bring-in) | Phase B pick | TestSweepW3UxFlows (flows green modulo the flake class) |
| PGR-35 | **Fixed** (formjs) | `40c9c382` | TestFormJsAdversarial (`pgr35PanelDisclosures…`) + TestFormKeystroke 9P |
| PGR-36 | **Fixed** (formjs) | `abe351cb` | TestFormJsCalc 49P + adversarial |
| PGR-37 | **Fixed** (formjs) | `b5c63cbc` | TestFormJsCalc 49P |
| PGR-38 + PGR-39 | **Fixed** (formjs) | `82f309ab` | TestFormJsAdversarial snapshot + surface pins |
| PGR-40 | **Deferred** (quickjs-ng bump > 0.15.0 — owner-deliberate) | — | `nativeSparseArrayScansAbideTheDeadline` (disclosed skip, auto-arms) |
| PGR-41 | **Deferred** (fresh-runtime-per-event = design decision) | — | deliberately-flipping pin |
| PGR-42…45 | **Fixed on `feat/pgr-d2` — NOT on PR** | off-PR: `39058fa9`, `3fd91495`, `abe093aa`, `958bd7c0` | lane-run: TestPgr35BatchCollision 4P, TestPgr36StaleSigningPanel 3P, TestPgr37PageSpaceLaw 9P, TestBatchMode 17P |
| Triage T1/T3/T7/T6 | **Fixed** | `95352e53` / `9e9e987a` / `9344eae5` / `6722356f` | OcrReviewLifecycle 34P / Sep13LeadComparePerf 4P / OllamaProvider 63P / SecretStore 21P (T6 inspection-verified + suite-equivalence NC) |

## 4. Phase D

- Formjs lane: §10 present in the report at the tip (came with `8bf27032`):
  PGR-35…39 fixed with attack stories; **PGR-40 deferred** (upstream added
  interrupt checks after 0.15.0; MSYS2 still ships the 0.15.0-authorized
  package); **PGR-41 deferred** (platform-inherent; Acrobat/pdf.js share it).
  Adversarial suite 24 tests; refuted-attack record kept (ReDoS polled, no host
  reach via Function/eval/async, no promise pumping).
- D2 lane: findings added as report **§10b**, renumbered **PGR-42…45**
  (collision with the formjs lane's PGR-35…38 — see deviation D4). All four
  fixes are test-backed on `feat/pgr-d2` with fail-before evidence. **Not
  cherry-picked** (Phase D brief asked for documentation; the branch is not in
  the Phase C pick list). Deferred residual recorded in D2 messages:
  TestBatchOcrSkipText fails identically on the unchanged baseline
  (pre-existing flake). Any further D2 residuals were not committed to the
  branch — evidence gap flagged.

## 5. Deviations (all documentation/process-level, none code)

- **D1 formjs pick order** — the brief listed fixes then "adversarial suite +
  docs"; the fix commits modify the suite file, so the suite (`697e7dcf`) had
  to land FIRST (branch order). Same commit set, same tree.
- **D2 cherry-pick conflict** — `9f0ddb63` (T6 zeroization) vs the PGR-20/25
  `readSecret` migration: resolved as union (migration block kept, `plain`
  made scrubable, scrub call kept). Build + TestSecretStore green after.
- **D3 quickjs-ng package coordination (two-step, closed)** — mid-run, the
  MSYS2 package (0.15.1-1) no longer matched the then-enforced 0.15.0 pin and
  the guard blocked the first C.5 re-configure. To finish Phase C on the
  authorized pin, 0.15.0-1 was restored from the local pacman cache. The
  coordinator then directed taking the PGR-40 check lane's deliberate bump
  (`020c0734` → `77db5be5`): enforced `GLYPHPDF_QUICKJS_PIN` = 0.15.1,
  package reinstalled to 0.15.1-1 from cache, staged `libqjs-0.dll` sha256
  `cc92ba7e…` hash-verified against the lane's record, zero golden drift
  (FormJsCalc 49P, Adversarial 24P/0F/1skip, Keystroke 9P). **PGR-40 stays
  deferred**: the lane's A/B probe proves 0.15.1 still never polls the
  interrupt handler in native sparse-array scans. **Operational gotcha:**
  `GLYPHPDF_QUICKJS_PIN` is a cached CMake variable — after the bump, stale
  build dirs keep enforcing 0.15.0 unless reconfigured with
  `-DGLYPHPDF_QUICKJS_PIN=0.15.1`.
- **D4 PGR-42…45 renumbering** — editorial; fixes on `feat/pgr-d2` keep their
  original IDs/messages; cross-referenced by SHA in §10b + PGR-STATUS.
- **D5 c4 docs commits not picked** — `a2a8ff4f` (ledger rows) and `16145af2`
  (evidence re-capture) were not in the brief's 5-SHA list; their content is
  folded into the ledger's Phase C section and PGR-STATUS. Left on
  `feat/pgr-c4`.
- **D6 E2 100% qualification** — full-batch runs cannot reach 100% on this
  machine today (load flakes + pre-existing flow7). Disposition protocol per
  the line-reconciliation precedent, strengthened with a same-session baseline
  build at `7d4d8d08` reproducing flow7 identically and the ReadOnlyGate flake.

## 6. Owner-decision items

1. **PGR-44 (CRITICAL, off-PR)** — integrate `feat/pgr-d2` (4 fixes + 3 test
   files) or supersede. Highest-risk unlanded item (redaction excision space
   law: rot90/270 + offset-origin secret survival).
2. **PGR-23 (HIGH, open on PR)** — redaction proof certifies nested compressed
   containers; no lane was scoped to fix it.
3. **PGR-40** — still deferred after the pin coordination: no MSYS2-carried
   release carries `js_poll_interrupts` in the array builtins. Re-triage when
   a carrying release lands (the adversarial pin auto-arms; note the
   faster-engine hazard recorded in the pin comment — an unfixed engine fast
   enough to finish the probe under ~140 ms FAILS the pin by design).
4. **PGR-41** — fresh-JS-runtime-per-event design decision.
5. **flow7 residual** — `TestSweepW3UxFlows::flow7` fails deterministically on
   this machine at BOTH `7d4d8d08` and the tip (a11y honesty-disclosure
   capture); not a Phase C regression, but it needs an a11y/ux lane look
   (capture robustness or wording), and it caps any local full-suite run at
   178/179 today.
6. PR merge itself — per brief, **not** done.

## 7. Unverified items

- D2 fixes: lane-run test numbers only (code not landed here).
- veraPDF 15/15 real-CLI evidence still lives only in `pdf-featplans`
  (untracked `tools-verapdf/` bundle) — carried over from the prior run.
- T6 zeroization: inspection-verified + suite-equivalence NC (no behavioral
  pin can observe zeroization without reading freed memory — disclosed).
- CI `build-and-test` runs are GitHub-hosted; local machine flakes do not
  affect them, but final green status should be re-checked at merge time.

## 8. Exact gate reproduction commands

```bash
# PATH preamble (every command; Git Bash would otherwise shadow ucrt64 DLLs)
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"

# E1 — fresh Release build (fresh dir)
cd /d/pdf/pdf-review
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-final --parallel 6          # 1024/1024 targets, exit 0

# E2 — full runs (offscreen), then touched-suite 3x
cd build-final
QT_QPA_PLATFORM=offscreen ctest -j6             # see §2 disposition
QT_QPA_PLATFORM=offscreen ctest                 # serial
REG="^TestSignatureRealCrypto$|^TestEngineSave$|^TestSecretStore$|^TestConversionExtraction$|^TestCommentsReview$|^TestOcrReviewLifecycle$|^TestMeasureCore$|^TestRedactionProof$|^TestSep13LeadRedactionProof$|^TestRedactTransaction$|^TestSep13LeadComparePerf$|^TestCompareEntry$|^TestCompareIntegration$|^TestOllamaProvider$|^TestOcrVerifyNavigation$|^TestFormJsAdversarial$|^TestFormJsCalc$|^TestFormKeystroke$"
for i in 1 2 3; do QT_QPA_PLATFORM=offscreen ctest -R "$REG"; done   # 18/18 x3

# E3 — purge + linearity
git merge-base --is-ancestor origin/main HEAD && echo OK
git rev-list --merges origin/main..HEAD | wc -l          # 0
git log origin/main..HEAD --format=%H | while read c; do
  git ls-tree -r --name-only $c | grep -iE '^(CLAUDE|SECURITY)\.md$'; done  # empty

# E4 — secret scan of added lines
git diff origin/main..HEAD --unified=0 | grep -E '^\+' |
  grep -E 'AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{20,}|github_pat_|sk-[A-Za-z0-9]{20,}|xox[bpars]-|AIza'  # empty

# E5 — accounting: 22 commits, 21 with -x trailers, all in ledger/PGR-STATUS
git log --oneline 7d4d8d08..HEAD | wc -l
git log --format=%B 7d4d8d08..HEAD | grep -c "cherry picked from commit"

# Baseline (disposition) — kept for inspection:
#   worktree D:/pdf/pdf-base7d (detached @ 7d4d8d08), build dir build-base
#   NOTE: fresh worktrees need the vendored podofo copied in
#   (cp -r third_party/podofo/install <worktree>/third_party/podofo/)
#   and pdfium.dll + onnxruntime.dll staged into the build dir root.
```

## 9. Residuals for the next (post-merge or follow-up) run

- **PGR-46 (new, coordinator item 2)** — PatternRedactor
  `extractCharsFromOpenDoc` has the same mixed-space flip the T2-2 fix
  corrected: RedactMode **mark-all** places viewer marks from those rects, so
  pattern-redaction marks on rotated/offset pages land away from the matched
  text. Recorded, not fixed (redaction lane / owner surface; shared surface
  with the redaction proof path). Evidence: `9b2b2727` commit message; refs
  RESIDUAL-PLANS-2026-09-21 Plan 5(b) + CONSOLIDATED-REPORT §2.6.
- Integrate or close `feat/pgr-d2` (PGR-42…45) — CRITICAL PGR-44 first.
- PGR-23 fix lane; PGR-40 upstream re-triage (pin auto-arms); PGR-41 design item.
- flow7 capture robustness (a11y/ux lane).
- Refresh the `--all` bundle + SHA-256 after the FINAL merge batch, before any
  ref deletion (carried from the prior run).
- Environment: build machines must run quickjs-ng 0.15.1-1 to match the
  enforced pin (reinstall from the pacman cache if drifted; verify staged
  `libqjs-0.dll` sha256 `cc92ba7e…`); stale build dirs need
  `-DGLYPHPDF_QUICKJS_PIN=0.15.1`; fresh worktrees need the vendored podofo +
  pdfium/onnxruntime DLL staging copied in (see §8).
- Local-only out-of-scope (unchanged): consolidate/all @ 95dccb23,
  feat/erase-ox-auto @ faa6cf10.
