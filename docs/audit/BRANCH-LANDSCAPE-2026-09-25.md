# Branch Landscape Survey — 2026-09-25 (captured 2026-09-29)

READ-ONLY reconnaissance for the endgame consolidation. Repo: `C:/Users/User/Projects/pdf-clean`
(= `D:/pdf/pdf-clean`), shared git dir `D:/pdf/pdf/.git`. Remote: `origin = https://github.com/eliets2/glyph-pdf.git`.
Surveyor: explorer agent (branch `feat/explorer-branches`). No ref, worktree, or file outside this deliverable and
`.context/explorer-branches-wip.md` was touched. Raw data: `%LOCALAPPDATA%/Temp/explorer-survey/`.

## Topline

| Metric | Value |
|---|---|
| Local branches | 158 |
| Remote branches (origin) | 89 real (+ `origin/HEAD` symref = 90 rows) |
| Tags | 102 (88 lightweight + 14 annotated; 89 of them `archive/*`) |
| Stashes | 7 (2026-07-02 .. 2026-09-14) |
| Worktrees | 22 (1 main + 21 linked) |
| D: free space | 386 GB of 895 GB (57% used) — no immediate pressure |
| PR #2 | OPEN, MERGEABLE, head `review/consolidated-parity@12da4e2f`, base `main` |

**Divergence caveat:** `review/consolidated-parity` (`12da4e2f`) folded lanes via **cherry-pick (`-x`), not merge**,
so source-branch tips remain "ahead" even when their *content* is fully on the PR. The ahead/behind columns below
measure commit objects, not content. Only `review/consolidated-parity` itself has 0 commits outside the base.

## 1. Biggest surprises

1. **Local `main` is ahead of `origin/main` by 2 unpushed commits** (`2b715f47` docs(audit) 09-13 findings;
   `703fa34e` feat(copy) §9.8+§9.9). Harmless for the endgame: **local `main` is fully contained in the PR
   head** (`review/consolidated-parity` is strictly ahead of `main`, 0/581) — merging PR #2 carries both
   unpushed commits to origin/main; no separate main push needed.
2. **`feat/parity-glm` is +585 commits vs its origin twin** (local `195e4309` 2026-09-23 vs remote `26c9a415`
   2026-09-22) — the largest unpushed body of work after the PR branch.
3. **5 antigravity subagent worktrees each hold ~703 files STAGED** (`git add -A` accident): 698×
   `vcpkg_installed/**` build artifacts, `dist/GlyphPDF-1.0.0-x64.msi` + `.wixpdb`, `graphify-out`, and —
   security-relevant — **private test signing keys staged** (`tests/fixtures/signing/ca.key`, `signer.key`,
   `test_signer.p12`). Do not commit; unstage-and-clean during consolidation.
4. **150 of 158 local branches contain commits not on `review/consolidated-parity`** (median 675, max
   1023 ahead) — a cherry-pick artifact: their *content* is folded, their commit objects are not. The ledger
   (`docs/audit/CONSOLIDATION-LEDGER-2026-09-25.md`, 0 unexplained) is the authoritative fold record.
   The other **8 are fully contained in the base and are zero-loss deletions**: `main`, `feat/fixall-ci`,
   `feat/fixall-tagging`, `feature/m4-forms`, `msys2-migration-backup-pre`, `subagent-Forms-Specialist-self-26881e20`,
   `subagent-View-Specialist-for-Rendering-Modes-view-specialist-ecb2f058`, plus the base itself.
5. **89 `archive/*` tags + 1 backup branch + 1 pre-purge bundle (70.4 MB, sha256 `960817c3…c75dc`) form the
   safety net** — but the bundle is from 2026-06-10 (pre-purge): it predates the entire consolidation era and
   does not cover the fixall session. The handoff's "refresh the all-refs backup bundle" step is still pending.
6. **Main worktree `D:/pdf/pdf` is dirty**: tracked edit to `docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`
   + 17 untracked files incl. an uncommitted `docs/audit/SECURITY-QUALITY-REVIEW-parity-glm.md`.
7. Only two remote twins are truly *diverged* (local rewrote history away from remote): `origin/audit-remediation`
   and `origin/feat/annotation-eraser`. Everything else is even, strictly ahead, or has no local twin.

## 2. Local branches (158)

Base for divergence: `review/consolidated-parity` `12da4e2f`. Columns: **Behind** = commits only on base
(branch is behind base), **Ahead** = commits only on branch. Files = tracked files at tip.

| Branch | Tip | Date | Files | Behind | Ahead | Subject (= purpose line) |
|---|---|---|---|---|---|---|
| `ar/prompt-1` | `5d999987` | 2026-06-16 | 990 | 581 | 262 | fix(ai-chat): AR-1 D5 AIChatPanel void* UAF on document-switch/list-clear |
| `audit-remediation` | `84445698` | 2026-06-22 | 1050 | 581 | 345 | release(v1.3.2.3): pin winget SHA to built MSI (2D764855...) |
| `audit/parity-glm` | `9ba3cea4` | 2026-09-10 | 1214 | 581 | 658 | Merge branch 'feat/parity-glm-infra' into feat/parity-glm |
| `backup/regex-verified-a39356e` | `a39356e7` | 2026-08-25 | 1072 | 581 | 347 | feat(search): PRD §9.15 regex + whole-word document-text find & replace |
| `cleanup/post-v1.3.2` | `4c3a87ef` | 2026-06-21 | 1010 | 581 | 300 | fix(core): ErrorLog export must verify the write succeeded |
| `consolidate/all` | `95dccb23` | 2026-09-23 | 1611 | 581 | 977 | Merge remote-tracking branch 'origin/feat/resoak-verdict' into consolidate/all |
| `feat/accessibility-p1` | `c5496ba6` | 2026-09-19 | 1308 | 581 | 808 | test(a11y): TestAccessibilityFixes RUN_SERIAL — same seam class as TestPrintableSummary (process-global commit fault + shared SafeSave candidate temp  |
| `feat/accessibility-p2` | `4e70217e` | 2026-09-23 | 1540 | 581 | 880 | docs(ledger): T2-4 accessibility auto-tagging P2 rows (engine, panel, veraPDF invocation) — implemented-awaiting-review |
| `feat/annotation-eraser` | `916a4b7d` | 2026-06-16 | 989 | 581 | 256 | feat(annotations): implement Eraser tool (PRD §9.3) |
| `feat/batch-presets-p1` | `f80033a4` | 2026-09-15 | 1285 | 581 | 780 | test(presets): R26 - add tests/TestBatchPresets.cpp (missed from f17f47f's tree) |
| `feat/batch-presets-p2` | `ec22eeed` | 2026-09-24 | 1633 | 117 | 8 | docs(ledger): R26-P2 batch-presets P2 rows (bates, rename, stop, batch-abort, report, share, dialogs) — implemented-awaiting-review |
| `feat/candidate-leak-fix` | `5c8fd087` | 2026-09-15 | 1286 | 581 | 787 | fix(signing): remove the committed SafeSave candidate on the success path — every successful sign/certify leaked one stray PDF into %TEMP%/glyphpdf-ca |
| `feat/consolidated` | `eb0efa21` | 2026-09-23 | 1623 | 142 | 1012 | docs(audit): LINE-RECONCILIATION-EXECUTION-2026-09-23 — verification complete: build 690/690, full serial ctest 176/179, every plan-3.4 per-surface su |
| `feat/consolidated-report` | `9db3ede4` | 2026-09-21 | 1534 | 581 | 877 | docs(audit): CONSOLIDATED-REPORT §7 open items + addendum slots — A1 ux batch B, A2 modularity-moves, A3 re-soak verdict, A4 verification-review outco |
| `feat/consolidation-plan` | `744e9e20` | 2026-09-20 | 1534 | 581 | 873 | docs(audit): CONSOLIDATION-PLAN-2026-09-20 — end-game origin 54->2 plan with zero-loss proofs (38 ancestor-folds, 2 ride-main, 8 real merges rehearsed |
| `feat/consolidation-stage1` | `8507573e` | 2026-09-23 | 1539 | 581 | 884 | docs(audit): stage-1 report — record final commit SHA (053f02fc) in log section |
| `feat/dispatch-gates` | `cf283923` | 2026-09-23 | 1536 | 581 | 884 | docs(ledger): SWEEP-BACKEND-2026-09-21 dispatch-gate lane rows (S2-1/S2-2/S2-3/S1-2/S1-1/S4-1) — implemented-awaiting-review |
| `feat/emergence-fixes` | `65500182` | 2026-09-21 | 1534 | 581 | 880 | docs(audit): ledger rows EM-1..EM-6 — the emergence-fix lane closes SWEEP-W3-EMERGENCE defects E-1..E-6 (implemented-awaiting-review) |
| `feat/erase-ox-auto` | `faa6cf10` | 2026-08-25 | 1072 | 581 | 347 | feat(annotations): PRD §9.3 annotation eraser tool |
| `feat/feature-plans` | `3e13cfd9` | 2026-09-22 | 1537 | 581 | 876 | docs(audit): FEATURE-PLANS-2026-09-21 addendum — the next wave's three design plans summarized (headline designs, decision requests, residuals, wave n |
| `feat/final-pgr-closers` | `68bc917e` | 2026-09-28 | 1661 | 90 | 5 | wip(pgr-23): nested-container sweep extension — recurse into attached PDF payloads (decoded streams + strings, depth cap 3), refuse undecodable contai |
| `feat/fixall-ci` | `0aa5c2de` | 2026-09-25 | 1661 | 65 | 0 | refactor(comments,compare): behavior-preserving cleanups from review §5 ponytail — (1) CommentsWidget::csvEscapeField drops its duplicated formula-lea |
| `feat/fixall-ci2` | `590c6c27` | 2026-09-28 | 1705 | 29 | 8 | docs(audit): FIXALL progress — CI/TESTS lane CP: CX-13..17 + small items complete on feat/fixall-ci2 (36251f03, 118368ee, 30e3d4b7, 789a24d6, c8c860d5 |
| `feat/fixall-forms` | `4d410c10` | 2026-09-25 | 1661 | 65 | 11 | fix(build): repair PR head 0aa5c2de — revert 12 duplicate cherry-picks (10 code + 2 docs) and add the missing DocumentSession include |
| `feat/fixall-forms2` | `5cadce52` | 2026-09-28 | 1671 | 58 | 3 | fix(conversion): CX-03 HIGH — LibreOffice converts into a fresh private temp folder, never the destination's own folder; the product is validated ther |
| `feat/fixall-images` | `d63ed76e` | 2026-09-29 | 1726 | 17 | 7 | fix(images): N1 — placements are addressed by occurrence index, not name |
| `feat/fixall-inv1` | `898d362f` | 2026-09-28 | 1687 | 29 | 1 | fix(signing): INV-1 HIGH — the unsigned-incremental catalog allowlist is DSS-only. isLegitimateIncrementalAppend allowlisted EVERY trailing /Type /Cat |
| `feat/fixall-redaction` | `5461b72d` | 2026-09-29 | 1742 | 6 | 3 | fix(images): N1 — image edits address ONE placement by (name, occurrence) |
| `feat/fixall-tagging` | `9d150be2` | 2026-09-25 | 1662 | 58 | 0 | docs(audit): FIXALL progress CP1 — pr-review-fixes cluster folded (4 picks, suites green) |
| `feat/fixall-tagging2` | `0144d056` | 2026-09-29 | 1723 | 17 | 3 | fix(tagging): CX-07 MED — Form XObject MCIDs emit /MCR /Pg /Stm /MCID instead of unresolvable bare ints |
| `feat/followups-2026-09-15` | `247e0c76` | 2026-09-15 | 1282 | 581 | 780 | docs(audit): follow-ups lane ledger — 2026-09-15 section (FU-1/FU-2/FU-3 implemented-awaiting-review); L12 flips CONFIRMED -> fixed; certify matrix ro |
| `feat/formjs-review` | `433b77e7` | 2026-09-24 | 1626 | 125 | 6 | docs(audit): formjs Phase-D threat model (attack stories) + review report §10 delta; §7/§8 formjs rows closed |
| `feat/glm-comp` | `585cc45d` | 2026-08-27 | 1102 | 581 | 405 | merge: free-model polish pass (dead-code/rename/layout cleanups) onto parity branch |
| `feat/glm-ocr` | `585cc45d` | 2026-08-27 | 1102 | 581 | 405 | merge: free-model polish pass (dead-code/rename/layout cleanups) onto parity branch |
| `feat/l7-rotate-annot` | `71891494` | 2026-09-15 | 1282 | 581 | 777 | fix(redaction): F1 — annotation attribution + excision must use the RAW /Rect under the shared PageSpace law (R14 review PARTIAL) |
| `feat/line-reconciliation` | `3811cc6a` | 2026-09-23 | 1608 | 581 | 984 | docs(audit): LINE-RECONCILIATION-PLAN-2026-09-23 — three-line content inventory, validated conflict forecast (29 files), merge order (pg <- ca <- cp,  |
| `feat/modularity-moves` | `3c411cc8` | 2026-09-20 | 1527 | 581 | 869 | docs(ledger): SWEEP-W3 moves 1+2 rows (AM1 B1 label seam 5c02b01, AM2 dead-include sweep 791115b with 2 audit-row corrections) — implemented-awaiting- |
| `feat/ocr-verify-finereader` | `f5b59e66` | 2026-08-26 | 1074 | 581 | 366 | feat(ocr-verify): B15 spell-check + verify-low-conf option toggles |
| `feat/parity-glm` | `195e4309` | 2026-09-23 | 1626 | 142 | 1023 | Merge branch 'feat/accessibility-p2' into feat/parity-glm |
| `feat/parity-glm-clean` | `352c9b1e` | 2026-09-09 | 1209 | 581 | 621 | docs(audit): CLEANUP-LEDGER-2026-09-09 — dead-file cleanup evidence and dispositions |
| `feat/parity-glm-formjs` | `2290ab51` | 2026-09-10 | 1220 | 581 | 644 | docs(ledger): form-JS Phase 1 row — implemented-awaiting-review |
| `feat/parity-glm-gate` | `7f8cf950` | 2026-09-09 | 1203 | 581 | 613 | docs(ledger): G01–G06 rows — independent quality gate repairs implemented-awaiting-review |
| `feat/parity-glm-gateC` | `74be82cf` | 2026-09-10 | 1204 | 581 | 620 | docs(ledger): G10/G11/G14/G15 + infra flake-pin rows — independent quality gate repairs implemented-awaiting-review |
| `feat/parity-glm-gateE` | `c96d2267` | 2026-09-13 | 1224 | 581 | 678 | docs(ledger): gateE rows — R02/R03/R09 implemented-awaiting-review (failed commits preserve accepted work; rotate+inline-text at the checked seams; re |
| `feat/parity-glm-infra` | `77e3b066` | 2026-09-10 | 1212 | 581 | 646 | docs(ledger): gateD lane (G16–G20, N06, D03, TestImageDedup) rows → implemented-awaiting-review |
| `feat/parity-glm-integration` | `2f755244` | 2026-09-15 | 1281 | 581 | 770 | docs(ledger): finish sep13-residual merge — union of N17/N18 rows with the residuals section (leftover conflict hunk) |
| `feat/parity-glm-linux` | `7ba5ca1f` | 2026-09-13 | 1223 | 581 | 670 | docs(ledger): native Linux lane WP-R20 + R21 rows — implemented-awaiting-review |
| `feat/parity-glm-n17n18` | `819d84a5` | 2026-09-14 | 1271 | 581 | 730 | docs(audit): N17+N18 rows — feature-command matrix + evidence ledger (2026-09-14) |
| `feat/parity-glm-packa` | `40a38cef` | 2026-09-13 | 1233 | 581 | 651 | docs(ledger): Pack A T2-2/3/6/9 rows — 127/127 orchestrator gate; per-item revert-verify assigned to R14 |
| `feat/parity-glm-packafix` | `5ae41536` | 2026-09-14 | 1256 | 581 | 711 | docs(ledger): PackA+R15 remediation rows — 8 findings closed (F1-F4, r15-F1/F2/F3) |
| `feat/parity-glm-quick` | `2b81fc28` | 2026-09-14 | 1253 | 581 | 698 | docs(audit): Q-lane ledger section — Q1-Q4 rows (implemented-awaiting-review) |
| `feat/parity-glm-r04r08r10` | `2bb2c431` | 2026-09-13 | 1229 | 581 | 685 | docs(ledger): R04/R08/R10 rows — implemented-awaiting-review (failed or canceled 7-Zip no longer destroys the existing package; print jobs honor the d |
| `feat/parity-glm-r05` | `5995b7fa` | 2026-09-13 | 1223 | 581 | 673 | docs(ledger): R05/JS-01 row — implemented-awaiting-review (9 reproduced bypasses closed by the whole-operation deadline) |
| `feat/parity-glm-r06r13` | `cbbb8ec5` | 2026-09-13 | 1224 | 581 | 681 | docs(ledger): R06/R13 rows — implemented-awaiting-review (one alignment mapping drives comparison/navigation/exports; FPDFText_GetText API terminator  |
| `feat/parity-glm-r11r12` | `83be3c20` | 2026-09-14 | 1265 | 581 | 727 | docs(audit): GLM-RESUME-STATE-2026-09-14 — integration tip, worktree/owner map, open queue |
| `feat/parity-glm-r15r17` | `3d9a7dd5` | 2026-09-13 | 1255 | 581 | 697 | docs(ledger): R15–R17 UI wave rows — implemented-awaiting-review (canonical command binding + honest discovery with 15 promoted ribbon entries; twelve |
| `feat/parity-glm-r18f` | `7d5ae676` | 2026-09-14 | 1266 | 581 | 730 | docs(audit): R18(f)/R19 finisher ledger rows — keystroke tier, R19 end-to-end pin, F1/F2/F3 verify-at-tip; all implemented-awaiting-review with scoped |
| `feat/parity-glm-r18r19` | `abc87de2` | 2026-09-13 | 1254 | 581 | 707 | feat(signing): R19 — settings-driven PAdES/TSA configuration with honest unavailable paths and attained-level disclosure |
| `feat/parity-glm-r22` | `7ceeba82` | 2026-09-15 | 1266 | 581 | 731 | docs(audit): R22 native-Linux installed-resources gate — staged-tree independence PROVEN (TestStatusBarSlim 12/12 with source resources hidden + garba |
| `feat/parity-glm-resid` | `586d6e4c` | 2026-09-13 | 1256 | 581 | 705 | Merge branch 'feat/parity-glm-sep13' into feat/parity-glm |
| `feat/parity-glm-resid2` | `9129788f` | 2026-09-14 | 1264 | 581 | 719 | fix(shell): ARC07 residual — OCR-accept export refuses in-place overwrite of a read-only document (ocrAcceptWriteBlocker pure seam; Save-As route stay |
| `feat/parity-glm-review` | `cd01e892` | 2026-09-15 | 1279 | 581 | 763 | docs(audit): R14 independent review — verdicts, ledger flips, reviewer probes |
| `feat/parity-glm-sec` | `e36e714c` | 2026-09-09 | 1190 | 581 | 587 | docs(ledger): repair-order step 4 (lane 4B) — EC04/NCR-02/N08-residual/EC06/D03-residual → implemented-awaiting-review; duplicate D02 (redaction) rows |
| `feat/parity-glm-sep13` | `6fe482c8` | 2026-09-13 | 1251 | 581 | 700 | docs(ledger): SEP13 rows — implemented-awaiting-review (CID /W domain clamp with OOB-write abort revert-verify; XLSX same-column last-write-wins with  |
| `feat/parity-glm-t2` | `21673bc0` | 2026-09-09 | 1199 | 581 | 605 | docs(ledger): T2 lane (redaction proof mode) rows → implemented-awaiting-review |
| `feat/parity-nemo-b` | `585cc45d` | 2026-08-27 | 1102 | 581 | 405 | merge: free-model polish pass (dead-code/rename/layout cleanups) onto parity branch |
| `feat/parity-ox-auto` | `f7d9aec9` | 2026-08-27 | 1102 | 581 | 392 | fix(batch): §9.12 P0 — per-file engine for editor ops ends false parallelism |
| `feat/pgr-c2` | `552c615f` | 2026-09-24 | 1622 | 125 | 4 | fix(measure): PGR-19 reset stale unitsPerPt on the pt-labelled scaleFrom path |
| `feat/pgr-c3` | `5ff0d618` | 2026-09-24 | 1622 | 125 | 3 | fix(secrets): PGR-26 — persist API-key credentials per-machine, not enterprise-roaming |
| `feat/pgr-c4` | `16145af2` | 2026-09-24 | 1646 | 125 | 7 | docs(evidence): PGR-C4 — re-capture TestSep13LeadRedactionProof/TestRedactTransaction postfix slots: the originals were taken while the background ful |
| `feat/pgr-critical-fixes` | `a690d7f4` | 2026-09-24 | 1622 | 125 | 3 | fix(engine): PGR-06 HIGH — encrypted-document rollback keeps the encryption password (reload with retained credentials; clear only on lineage drop) |
| `feat/pgr-d2` | `958bd7c0` | 2026-09-24 | 1625 | 125 | 4 | fix(batch): PGR-38 — account mid-run progress against the run's captured file total |
| `feat/pgr40-quickjs-bump` | `020c0734` | 2026-09-24 | 1635 | 117 | 3 | fix(deps): PGR-40 check lane — bump enforced quickjs-ng pin 0.15.0 -> 0.15.1 (MSYS2 0.15.1-1); FIX IS NOT IN 0.15.1 — PGR-40 stays deferred, pin still |
| `feat/pr-review-fixes` | `a25c37b7` | 2026-09-25 | 1661 | 73 | 14 | docs(ledger): PR-review fix lane 3 rows — §3 items 4/5/6, §4 M1–M3, §5 ponytail (implemented-awaiting-review); residuals: ponytail not-taken items, CI |
| `feat/printable-summaries` | `b06b2f07` | 2026-09-15 | 1299 | 581 | 802 | docs(audit): printable summaries lane ledger — 2026-09-15 section (PS-1/PS-2 implemented-awaiting-review); resolves conflict markers committed by base |
| `feat/r24-policy` | `335d1d3a` | 2026-09-15 | 1291 | 581 | 780 | docs(audit): R24 ledger section + matrix rows — policy/support-bundle/network-page rows, all implemented-awaiting-review |
| `feat/r24-wiring` | `b8909dcb` | 2026-09-20 | 1303 | 581 | 808 | docs(audit): R24 wiring closure lane - ledger section (R24-W1/W2/W3 rows, implemented-awaiting-review) |
| `feat/redaction-gaps` | `b454d071` | 2026-09-23 | 1534 | 581 | 882 | docs(ledger): redaction gap-fix lane rows — R14 probe-scoping fix + G1–G6 (implemented-awaiting-review) |
| `feat/redaction-research` | `b8d17885` | 2026-09-22 | 1534 | 581 | 874 | docs(audit): REDACTION-RESEARCH refinement — Type3 fallback-advance drift is a real under-excision vector (INCONCLUSIVE sub-case of the SAFE-BY-DESIGN |
| `feat/regex-find-replace` | `a39356e7` | 2026-08-25 | 1072 | 581 | 347 | feat(search): PRD §9.15 regex + whole-word document-text find & replace |
| `feat/regex-ox-auto` | `7d54b387` | 2026-08-25 | 1072 | 581 | 347 | feat(search): PRD §9.15 regex + whole-word document-text find & replace |
| `feat/report-addendum` | `d65b3e35` | 2026-09-23 | 1537 | 581 | 885 | docs(audit): consolidated report ADDENDUM SLOT A3 — re-soak VERDICT PASS for b17106a (exe 509da2c8…, re-hashed post-SOAK-END): SOAK END after 720 pass |
| `feat/residual-exec` | `5d74c348` | 2026-09-24 | 1629 | 117 | 8 | test(infra): Plan 11 item 3 — register R14ProbeBatchSkip as a deliberate-run probe |
| `feat/residual-plans` | `72f5ce7c` | 2026-09-22 | 1537 | 581 | 878 | docs(audit): RESIDUAL-PLANS-2026-09-21 — execution-ready fix plans for all 15 consolidated-report §2.6 residuals, verified at 26c9a415 — 11 stand-at-t |
| `feat/resoak-verdict` | `d249bf91` | 2026-09-22 | 1540 | 581 | 878 | docs(audit): RESOAK-VERDICT-2026-09-22 — 48h re-soak PASS for b17106a (720 passes, 48.12h, SOAK END + done; zero candidate-attributable recurrence; ap |
| `feat/rotate270-fix` | `9e1cde9d` | 2026-09-20 | 1530 | 581 | 865 | docs(audit): W2B-1 RESOLUTION addendum + ledger rows — page-space geometry from the raw /MediaBox (implemented-awaiting-review; SL1 partial re-submits |
| `feat/runintersects-precision` | `e620757b` | 2026-09-21 | 1535 | 581 | 874 | fix(proof): runIntersects margin from the run's real glyph band, not a 3*fs blanket (W2c residual) |
| `feat/sanitize-assert-mutable` | `ed04426e` | 2026-09-20 | 1525 | 581 | 866 | fix(sanitize): soak SegFault E-2 — trailer /Info removal orphaned the object PdfDocument::m_Info wraps; Save's CollectGarbage freed it under the wrapp |
| `feat/send-for-signing-p1` | `f621416d` | 2026-09-19 | 1299 | 581 | 794 | docs(audit): ledger + matrix rows for send-for-signing P1 — implemented-awaiting-review |
| `feat/sep13-fixes` | `e8b9a19e` | 2026-09-14 | 1273 | 581 | 747 | docs(audit): ledger rows for SEP13 M1/M3/M5 stretch fixes (implemented-awaiting-review) |
| `feat/sep13-leads` | `b0fd8296` | 2026-09-14 | 1274 | 581 | 735 | docs(ledger): SEP13 redactfix lane rows — L5/L7/L8 (+L6 wording pin) implemented-awaiting-review |
| `feat/sep13-residual` | `c7e9ecfe` | 2026-09-15 | 1275 | 581 | 763 | docs(ledger): SEP13 residuals lane rows — RES-1 (B-T wording honesty) + RES-2 (RedactOperation lifetime) implemented-awaiting-review |
| `feat/soak-48h` | `1d2e76b8` | 2026-09-15 | 1282 | 581 | 771 | docs(audit): R25 48h soak start — candidate 2f75524448ffca09b110a5517ad087a061218b1b (feat/parity-glm tip) |
| `feat/soak-48h-resume` | `0fad38c0` | 2026-09-20 | 1526 | 581 | 866 | docs(audit): R25b re-soak start — reboot-resilient 48h loop, candidate b17106a (feat/parity-glm tip), exe 509da2c8…7853; verdict doc carried forward w |
| `feat/soak-followups` | `32282d8d` | 2026-09-23 | 1537 | 581 | 879 | fix(test): SF-2 — proofFailsOnXmpSurvivor flake root-caused to the tamper helper, not the proof: PoDoFo 1.1.0 default Save stamps /Info/ModDate and, w |
| `feat/soak-verdict` | `790a7197` | 2026-09-20 | 1526 | 581 | 866 | docs(audit): SOAK-VERDICT-2026-09-20 — verdict FAIL: 48h soak killed at 4h00m21s by Windows Update planned restart (System 1074 @ 05:45:03) — 58 passe |
| `feat/sweep-backend` | `efbd8235` | 2026-09-22 | 1537 | 581 | 878 | docs(audit): SWEEP-BACKEND internal API surface audit — findings plan-only, doctrine-mapping appendix |
| `feat/sweep-legacy-fix` | `a73419ed` | 2026-09-20 | 1324 | 581 | 820 | docs(audit): SWEEP-LEGACY verdicts + ledger rows — 3 fixes (origin-class page-space law on annotation/form rects; conversion SafeSave commit; forms su |
| `feat/sweep-quality-new` | `673129e1` | 2026-09-20 | 1325 | 581 | 819 | docs(audit): SWEEP-QUALITY-NEW fixes-log — before/after suite totals (66 -> 67 passed, zero behavior change; extraction 0c1aeab, pins 288c281) |
| `feat/sweep-w1-adversary` | `8bcde898` | 2026-09-20 | 1316 | 581 | 814 | docs(adversary): SWEEP-W1 audit — 5 findings (4 CONFIRMED w/ failing repros, 1 design), 7 refutations w/ pins, 3 hypotheses w/ missing pieces |
| `feat/sweep-w1-fixes` | `e8e715f2` | 2026-09-20 | 1515 | 581 | 848 | fix(FZ-4/fuzz): reject reserved DOS device names and overlong renders in resolveNaming — extends the W1-01 containment gate |
| `feat/sweep-w1-fuzz` | `b32d8dfd` | 2026-09-20 | 1509 | 581 | 824 | fix(fuzz): generator omits empty PDF dict keys (empty /Resources made PoDoFo throw at load, masking the S3 campaign) |
| `feat/sweep-w1-security` | `f6d46dd9` | 2026-09-20 | 1322 | 581 | 819 | docs(audit): SWEEP-W1 refuted items (10 pins), disclosure-claims verdicts, inconclusive, coverage, suites, residuals |
| `feat/sweep-w2-gsd` | `125ae929` | 2026-09-23 | 1550 | 581 | 894 | docs(audit): SWEEP-W2-GSD verdicts — all 10 queue rows VERIFIED (probe 12/0 + 10 scoped NCs + final pristine sweep 190/0); ledger flips EM-1..6, sanit |
| `feat/sweep-w2-testing` | `b17106a3` | 2026-09-20 | 1524 | 581 | 865 | docs(audit): SWEEP-W2 §2.2 in-suite grids (FU-2 live-capture verbatims) + §2.3 final classification table + §3.1 patch verification 171/171 + §8 commi |
| `feat/sweep-w2-verify` | `22588a1c` | 2026-09-20 | 1325 | 581 | 821 | Merge branch 'feat/r24-wiring' into feat/parity-glm |
| `feat/sweep-w2-verify-b` | `84a19f9e` | 2026-09-20 | 1529 | 581 | 862 | docs(audit): SWEEP-W2B verify verdicts — 14 verified + SL1 partial (FINDING W2B-1: /Rotate 270 transposed rects); ledger flips |
| `feat/sweep-w2c` | `4525f0e1` | 2026-09-21 | 1535 | 581 | 873 | verify(W2C): W2B-1 rotate-270 page-space fix VERIFIED - SL1 flipped from partial (independent probe through the production edit path + scoped NC repro |
| `feat/sweep-w3-arch` | `46cae7ba` | 2026-09-20 | 1525 | 581 | 866 | docs(audit): SWEEP-W3 architect modularity audit — compile_commands-derived include graph at b17106a (0 cycles, 13 upward edges = 10 live + 3 dead, 7  |
| `feat/sweep-w3-archaeo` | `b8a5a564` | 2026-09-20 | 1525 | 581 | 867 | docs(audit): SWEEP-W3 count-table fix — KEEP-ANYWAY is 6 rows (4 src files + 2 docs rows), 8 is the total candidate-row count; no findings changed |
| `feat/sweep-w3-devops` | `fbfa6e47` | 2026-09-20 | 1525 | 581 | 866 | docs(audit): SWEEP-W3 devops — operational readiness of the R25 candidate at b17106a: deploy validation PASS (validator exit 0 + deploy exit 0, 407MB  |
| `feat/sweep-w3-emergence` | `48c2ae51` | 2026-09-21 | 1534 | 581 | 873 | docs(audit): SWEEP-W3-EMERGENCE-2026-09-20 — emergence-engine composition-matrix audit at ec9f16f (7-cell matrix with verdicts: 6 confirmed interactio |
| `feat/sweep-w3-perf` | `2ec24f41` | 2026-09-20 | 1529 | 581 | 864 | docs(audit): PERF-BASELINE-2026-09-20 FINAL — quiet-run numbers appended (gate passed) |
| `feat/sweep-w3-research` | `e97f7075` | 2026-09-20 | 1534 | 581 | 877 | docs(audit): SWEEP-W3-RESEARCH-2026-09-20 — research/tracking corpus reconciliation log at ec9f16f (16 backlog rows re-statused with landing SHAs, fin |
| `feat/sweep-w3-ui` | `164dabd4` | 2026-09-20 | 1528 | 581 | 866 | docs(audit): SWEEP-W3-UI acceptance evidence at b17106a — offscreen R17 matrix (1366x768+1920x1080 x 100/150/200%), 6/6 cells DONE; verdicts: welcome  |
| `feat/sweep-w3-ux` | `5b36715d` | 2026-09-21 | 1531 | 581 | 867 | docs(audit): SWEEP-W3 UX — batch A verdicts (flows 1,2,3,8 COMPLETABLE; preset-store first-run breaker F2b-D1 recorded) + disk-guard stop state |
| `feat/sweep-w3-ux-resume` | `3f957d72` | 2026-09-21 | 1536 | 581 | 869 | docs(audit): SWEEP-W3 UX — batch B run complete (flows 4-7 verdicts) + F2b-D1 fix evidence + run-repaired harness |
| `feat/ui-narrow-viewport` | `c467e6a7` | 2026-09-23 | 1534 | 581 | 869 | docs(audit): SWEEP-W3-UI F1-RESOLVED — $2 matrix row 3a-R (implemented-awaiting-review) + $8 addendum: before/after grab table (1366: 1687-1973 -> 136 |
| `feat/unreviewed-map` | `0bcbd6bf` | 2026-09-23 | 1537 | 581 | 878 | docs(audit): UNREVIEWED-CODE-MAP-2026-09-23 — authoritative map of implemented-awaiting-review code for the endgame consolidation (222 awaiting rows a |
| `feat/ux-defects-fixes` | `f01a4e37` | 2026-09-23 | 1552 | 581 | 874 | test(s4s)+docs(audit): drive the re-confirm channel the gate actually reads + UX fixes ledger rows |
| `feat/ux-integration-fixes` | `ef371ad0` | 2026-09-23 | 1607 | 581 | 983 | Merge branch 'feat/sweep-backend' into feat/parity-glm |
| `feat/ux-integration-fixes2` | `14d69ce9` | 2026-09-23 | 1607 | 581 | 988 | docs(ledger): ux-integration fix lane 2 rows — slot-scoped modal drivers, flow2b batchFinished wait, F2a-F1 batch-merge output naming + flow3b G2-refu |
| `feature-elevation-wave1a` | `055592df` | 2026-07-01 | 1049 | 581 | 375 | fix(export): wire the dead "linearized" export preset checkbox (Wave 1A #26) |
| `feature/accessibility-parity` | `84445698` | 2026-06-22 | 1050 | 581 | 345 | release(v1.3.2.3): pin winget SHA to built MSI (2D764855...) |
| `feature/editing-parity` | `97172fbe` | 2026-07-12 | 1050 | 581 | 380 | test(editing): add TestEditingWave1B regression coverage + fix 2 real bugs it caught |
| `feature/m4-djot-foundation` | `ab65816a` | 2026-05-30 | 849 | 582 | 15 | fix: build + test fixes uncovered by first compile (D14 catchup) |
| `feature/m4-edge` | `f9805e31` | 2026-05-30 | 678 | 583 | 2 | feat(convert): structural PPTX export with text overlay and background image |
| `feature/m4-forms` | `68c4734a` | 2026-05-30 | 642 | 582 | 0 | M4-PROMPT-4: Forms tools implementation |
| `feature/m4-security` | `f9805e31` | 2026-05-30 | 678 | 583 | 2 | feat(convert): structural PPTX export with text overlay and background image |
| `feature/ocr-parity` | `055592df` | 2026-07-01 | 1049 | 581 | 375 | fix(export): wire the dead "linearized" export preset checkbox (Wave 1A #26) |
| `feature/redaction-parity` | `45bf4302` | 2026-09-13 | 1049 | 581 | 377 | wip(redaction): preserve local region and overlay changes |
| `feature/security-parity` | `8bb20c52` | 2026-07-02 | 1076 | 581 | 376 | feat(security): implement selective sanitize and expiry guards |
| `feature/viewing-parity` | `de1fa268` | 2026-09-07 | 1090 | 581 | 380 | WIP snapshot: secure viewing-parity session state |
| `fix/p0-blank-viewer` | `86f2cf34` | 2026-09-25 | 1671 | 38 | 3 | fix(viewer): rendered pages get white paper — thumbnails, two-page spread, OCR input and snapshots showed the theme through the page |
| `fuzz/redaction-rig` | `92c08168` | 2026-06-22 | 1048 | 581 | 334 | docs(audit): hardening orchestration state file |
| `main` | `2b715f47` | 2026-09-13 | 1116 | 151 | 0 | docs(audit): preserve September 13 parity review findings |
| `msys2-migration-backup-pre` | `0c7d48ae` | 2026-05-27 | 433 | 621 | 0 | Complete Month 1 prompts and Month 2 bonus: Autosave, RenderCache concurrency, SignatureManager crypto, PDF string escaping, license sanitization. |
| `r2-1-chain1` | `7865cae1` | 2026-06-09 | 1034 | 581 | 167 | docs(R2-1): mark CHAIN-1 closed in SUMMARY.md |
| `r2-2-silent-failure` | `a6ae1bf9` | 2026-06-09 | 1041 | 581 | 178 | docs(audit): mark UX-03/04/14 + D3 CLOSED in SUMMARY.md |
| `r2-3-redaction` | `bbf2ed0f` | 2026-06-10 | 1041 | 581 | 178 | docs(R2-3): mark NF-1/NF-2/NF-4 CLOSED in SUMMARY.md |
| `r2-4-ribbon` | `e24532e1` | 2026-06-09 | 1034 | 581 | 168 | docs(R2-4): mark UX-01/UX-02 closed in SUMMARY.md |
| `r2-5-theme` | `90e9f764` | 2026-06-09 | 1033 | 581 | 167 | docs(R2-5): mark theme findings closed in SUMMARY.md |
| `r2-6-formbuilder` | `02ddc192` | 2026-06-10 | 1042 | 581 | 178 | docs(R2-6): mark C-03/UX-08 CLOSED in SUMMARY.md |
| `r2-7-tests` | `5442f105` | 2026-06-10 | 1056 | 581 | 188 | docs(R2-7): mark test integrity findings CLOSED in SUMMARY.md |
| `r3-er2-redact-guard` | `d5af7139` | 2026-06-10 | 1060 | 581 | 198 | fix(ER-2): block in-place redaction of signed documents; require unsigned copy first |
| `r3-er3-multirecip` | `77098852` | 2026-06-10 | 1060 | 581 | 198 | fix(ER-3): expose recipientCount(); warn before re-encrypting multi-recipient docs |
| `r3-er4-saveas` | `25d6b951` | 2026-06-10 | 1060 | 581 | 198 | fix(ER-4,ER-5): warn on Save As for signed docs; UntrustedChain renders amber |
| `r3-nf6-ocsp` | `83327e47` | 2026-06-10 | 1060 | 581 | 198 | fix(NF-6,ER-1): require OCSP certID match for DSS responses; document nonce limitation |
| `reconexec/snapshot-redaction-gaps-dirty` | `2adc3df6` | 2026-09-23 | 1539 | 581 | 883 | preservation: r14 probe-dirty worktree state (PoDoFoBackend G3-block probe deletion + W2 probes/diags) snapshot before line-reconciliation checkout |
| `recovery/temp-stage-push-20260909` | `d367aca1` | 2026-09-09 | 1201 | 581 | 605 | docs(research): send-for-signing implementation plan — PAdES scope, egress policy, phased design (design-only, no code) |
| `review/consolidated-parity` | `12da4e2f` | 2026-09-29 | 1742 | 0 | 0 | docs(audit): FIXALL handoff FINAL — Phases 3-5 complete (§6 re-verification all green ×3; E1 fresh build 945 steps 0 errors; E2 serial 100%, -j6 184/1 |
| `subagent-AST-Architect-self-54b52cfc` | `d7a8ca06` | 2026-09-13 | 689 | 582 | 5 | wip(docmodel): preserve original AST scaffolding generator |
| `subagent-AST-Architect-self-a1982f71` | `f9805e31` | 2026-05-30 | 678 | 583 | 2 | feat(convert): structural PPTX export with text overlay and background image |
| `subagent-Forms-Specialist-self-26881e20` | `9c6024cd` | 2026-05-30 | 642 | 583 | 0 | feat(view): implement M4-PROMPT-1 View tools (TwoPage, EyeCare, Presentation Mode) & fix reload() compile error in PagesController |
| `subagent-Vendoring-Specialist-self-7708fcd5` | `f9805e31` | 2026-05-30 | 678 | 583 | 2 | feat(convert): structural PPTX export with text overlay and background image |
| `subagent-Vendoring-Specialist-self-ca2c3f27` | `5a018ae5` | 2026-09-13 | 684 | 582 | 4 | wip(djot): preserve unfinished local codec scaffolding |
| `subagent-View-Specialist-for-Rendering-Modes-view-specialist-ecb2f058` | `3b9effd1` | 2026-05-30 | 635 | 588 | 0 | Model Updates for AnnotationItem fields |
| `test/view-parity-baseline` | `0948743b` | 2026-09-28 | 1673 | 38 | 4 | test(view-parity): characterize every view capability of the pre-redesign UI |

Distribution: ahead min 0 / median 678 / max 1023; behind-base min 0 / max 621. Authors: Elie Tanios ×150, Claude (audit) ×7, formjs-review-lane ×1.

### Family purpose guesses

| Family | Count | Purpose guess |
|---|---|---|
| `feat/` | 116 | Feature/point work (see subject) |
| `feature/` | 10 | Early feature-parity lanes (viewing/editing/forms/redaction/ocr…) — consolidated era |
| `ar/` | 1 | Audit-remediation prompt lanes (June audit AR-1..12) |
| `audit-remediation` | 1 | Audit snapshots |
| `audit/` | 1 | Audit snapshots |
| `backup/` | 1 | Safety-net backup pointer |
| `cleanup/` | 1 | Feature/point work (see subject) |
| `consolidate/` | 1 | Consolidation-era merge branch |
| `feature-elevation-wave1a` | 1 | Feature/point work (see subject) |
| `fix/` | 1 | Feature/point work (see subject) |
| `fuzz/` | 1 | Feature/point work (see subject) |
| `main` | 1 | Feature/point work (see subject) |
| `msys2-migration-backup-pre` | 1 | MSYS2 toolchain migration backup |
| `r2-1-chain1` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-2-silent-failure` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-3-redaction` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-4-ribbon` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-5-theme` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-6-formbuilder` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r2-7-tests` | 1 | R2 wave lanes (chain, silent-failure, redaction, ribbon, theme, formbuilder, tests) |
| `r3-er2-redact-guard` | 1 | Feature/point work (see subject) |
| `r3-er3-multirecip` | 1 | Feature/point work (see subject) |
| `r3-er4-saveas` | 1 | Feature/point work (see subject) |
| `r3-nf6-ocsp` | 1 | Feature/point work (see subject) |
| `reconexec/` | 1 | Feature/point work (see subject) |
| `recovery/` | 1 | Feature/point work (see subject) |
| `review/` | 1 | Feature/point work (see subject) |
| `subagent-AST-Architect-self-54b52cfc` | 1 | Antigravity subagent scratch branches |
| `subagent-AST-Architect-self-a1982f71` | 1 | Antigravity subagent scratch branches |
| `subagent-Forms-Specialist-self-26881e20` | 1 | Antigravity subagent scratch branches |
| `subagent-Vendoring-Specialist-self-7708fcd5` | 1 | Antigravity subagent scratch branches |
| `subagent-Vendoring-Specialist-self-ca2c3f27` | 1 | Antigravity subagent scratch branches |
| `subagent-View-Specialist-for-Rendering-Modes-view-specialist-ecb2f058` | 1 | Antigravity subagent scratch branches |
| `test/` | 1 | Parity verification baseline (owner: out of scope) |

## 3. Remote branches (origin — 89 real + `origin/HEAD` symref)

Twin status compares `origin/X` with local `X` (commit objects).

| Remote branch | Tip | Date | Files | Twin status | Subject |
|---|---|---|---|---|---|
| `origin/audit-remediation` | `fd74f5dc` | 2026-06-22 | 1048 | diverged:l-behind-343/l-ahead+345 | release(v1.3.2.3): pin winget SHA to built MSI (2D764855...) |
| `origin/claude/modest-mccarthy-riuo2o` | `a8f50a44` | 2026-06-16 | 970 | no-local-twin | docs(audit): add 2026-06-16 remediation prompts (AR-PROMPT-1..12) |
| `origin/feat/accessibility-p1` | `c5496ba6` | 2026-09-19 | 1308 | even | test(a11y): TestAccessibilityFixes RUN_SERIAL — same seam class as TestPrintableSummary (process-global commit fault + s |
| `origin/feat/accessibility-p2` | `4e70217e` | 2026-09-23 | 1540 | even | docs(ledger): T2-4 accessibility auto-tagging P2 rows (engine, panel, veraPDF invocation) — implemented-awaiting-review |
| `origin/feat/annotation-eraser` | `ebd022cc` | 2026-06-16 | 987 | diverged:l-behind-254/l-ahead+256 | feat(annotations): implement Eraser tool (PRD §9.3) |
| `origin/feat/batch-presets-p1` | `f80033a4` | 2026-09-15 | 1285 | even | test(presets): R26 - add tests/TestBatchPresets.cpp (missed from f17f47f's tree) |
| `origin/feat/batch-presets-p2` | `ec22eeed` | 2026-09-24 | 1633 | even | docs(ledger): R26-P2 batch-presets P2 rows (bates, rename, stop, batch-abort, report, share, dialogs) — implemented-awai |
| `origin/feat/candidate-leak-fix` | `5c8fd087` | 2026-09-15 | 1286 | even | fix(signing): remove the committed SafeSave candidate on the success path — every successful sign/certify leaked one str |
| `origin/feat/consolidated` | `eb0efa21` | 2026-09-23 | 1623 | even | docs(audit): LINE-RECONCILIATION-EXECUTION-2026-09-23 — verification complete: build 690/690, full serial ctest 176/179, |
| `origin/feat/consolidated-report` | `9db3ede4` | 2026-09-21 | 1534 | even | docs(audit): CONSOLIDATED-REPORT §7 open items + addendum slots — A1 ux batch B, A2 modularity-moves, A3 re-soak verdict |
| `origin/feat/consolidation-plan` | `744e9e20` | 2026-09-20 | 1534 | even | docs(audit): CONSOLIDATION-PLAN-2026-09-20 — end-game origin 54->2 plan with zero-loss proofs (38 ancestor-folds, 2 ride |
| `origin/feat/consolidation-stage1` | `8507573e` | 2026-09-23 | 1539 | even | docs(audit): stage-1 report — record final commit SHA (053f02fc) in log section |
| `origin/feat/dispatch-gates` | `cf283923` | 2026-09-23 | 1536 | even | docs(ledger): SWEEP-BACKEND-2026-09-21 dispatch-gate lane rows (S2-1/S2-2/S2-3/S1-2/S1-1/S4-1) — implemented-awaiting-re |
| `origin/feat/emergence-fixes` | `65500182` | 2026-09-21 | 1534 | even | docs(audit): ledger rows EM-1..EM-6 — the emergence-fix lane closes SWEEP-W3-EMERGENCE defects E-1..E-6 (implemented-awa |
| `origin/feat/feature-plans` | `3e13cfd9` | 2026-09-22 | 1537 | even | docs(audit): FEATURE-PLANS-2026-09-21 addendum — the next wave's three design plans summarized (headline designs, decisi |
| `origin/feat/followups-2026-09-15` | `247e0c76` | 2026-09-15 | 1282 | even | docs(audit): follow-ups lane ledger — 2026-09-15 section (FU-1/FU-2/FU-3 implemented-awaiting-review); L12 flips CONFIRM |
| `origin/feat/formjs-review` | `433b77e7` | 2026-09-24 | 1626 | even | docs(audit): formjs Phase-D threat model (attack stories) + review report §10 delta; §7/§8 formjs rows closed |
| `origin/feat/glm-comp` | `585cc45d` | 2026-08-27 | 1102 | even | merge: free-model polish pass (dead-code/rename/layout cleanups) onto parity branch |
| `origin/feat/glm-ocr` | `585cc45d` | 2026-08-27 | 1102 | even | merge: free-model polish pass (dead-code/rename/layout cleanups) onto parity branch |
| `origin/feat/l7-rotate-annot` | `71891494` | 2026-09-15 | 1282 | even | fix(redaction): F1 — annotation attribution + excision must use the RAW /Rect under the shared PageSpace law (R14 review |
| `origin/feat/modularity-moves` | `3c411cc8` | 2026-09-20 | 1527 | even | docs(ledger): SWEEP-W3 moves 1+2 rows (AM1 B1 label seam 5c02b01, AM2 dead-include sweep 791115b with 2 audit-row correc |
| `origin/feat/parity-glm` | `26c9a415` | 2026-09-22 | 1536 | local-ahead+585 | Merge branch 'feat/modularity-moves' into feat/parity-glm |
| `origin/feat/parity-glm-clean` | `352c9b1e` | 2026-09-09 | 1209 | even | docs(audit): CLEANUP-LEDGER-2026-09-09 — dead-file cleanup evidence and dispositions |
| `origin/feat/parity-glm-formjs` | `2290ab51` | 2026-09-10 | 1220 | even | docs(ledger): form-JS Phase 1 row — implemented-awaiting-review |
| `origin/feat/parity-glm-gate` | `7f8cf950` | 2026-09-09 | 1203 | even | docs(ledger): G01–G06 rows — independent quality gate repairs implemented-awaiting-review |
| `origin/feat/parity-glm-gateC` | `74be82cf` | 2026-09-10 | 1204 | even | docs(ledger): G10/G11/G14/G15 + infra flake-pin rows — independent quality gate repairs implemented-awaiting-review |
| `origin/feat/parity-glm-infra` | `77e3b066` | 2026-09-10 | 1212 | even | docs(ledger): gateD lane (G16–G20, N06, D03, TestImageDedup) rows → implemented-awaiting-review |
| `origin/feat/parity-glm-integration` | `2f755244` | 2026-09-15 | 1281 | even | docs(ledger): finish sep13-residual merge — union of N17/N18 rows with the residuals section (leftover conflict hunk) |
| `origin/feat/parity-glm-n17n18` | `819d84a5` | 2026-09-14 | 1271 | even | docs(audit): N17+N18 rows — feature-command matrix + evidence ledger (2026-09-14) |
| `origin/feat/parity-glm-packa` | `40a38cef` | 2026-09-13 | 1233 | even | docs(ledger): Pack A T2-2/3/6/9 rows — 127/127 orchestrator gate; per-item revert-verify assigned to R14 |
| `origin/feat/parity-glm-packafix` | `5ae41536` | 2026-09-14 | 1256 | even | docs(ledger): PackA+R15 remediation rows — 8 findings closed (F1-F4, r15-F1/F2/F3) |
| `origin/feat/parity-glm-quick` | `2b81fc28` | 2026-09-14 | 1253 | even | docs(audit): Q-lane ledger section — Q1-Q4 rows (implemented-awaiting-review) |
| `origin/feat/parity-glm-r05` | `5995b7fa` | 2026-09-13 | 1223 | even | docs(ledger): R05/JS-01 row — implemented-awaiting-review (9 reproduced bypasses closed by the whole-operation deadline) |
| `origin/feat/parity-glm-r18f` | `7d5ae676` | 2026-09-14 | 1266 | even | docs(audit): R18(f)/R19 finisher ledger rows — keystroke tier, R19 end-to-end pin, F1/F2/F3 verify-at-tip; all implement |
| `origin/feat/parity-glm-r22` | `7ceeba82` | 2026-09-15 | 1266 | even | docs(audit): R22 native-Linux installed-resources gate — staged-tree independence PROVEN (TestStatusBarSlim 12/12 with s |
| `origin/feat/parity-glm-resid2` | `9129788f` | 2026-09-14 | 1264 | even | fix(shell): ARC07 residual — OCR-accept export refuses in-place overwrite of a read-only document (ocrAcceptWriteBlocker |
| `origin/feat/parity-glm-review` | `cd01e892` | 2026-09-15 | 1279 | even | docs(audit): R14 independent review — verdicts, ledger flips, reviewer probes |
| `origin/feat/parity-glm-sec` | `e36e714c` | 2026-09-09 | 1190 | even | docs(ledger): repair-order step 4 (lane 4B) — EC04/NCR-02/N08-residual/EC06/D03-residual → implemented-awaiting-review;  |
| `origin/feat/parity-glm-t2` | `21673bc0` | 2026-09-09 | 1199 | even | docs(ledger): T2 lane (redaction proof mode) rows → implemented-awaiting-review |
| `origin/feat/pgr-c2` | `552c615f` | 2026-09-24 | 1622 | even | fix(measure): PGR-19 reset stale unitsPerPt on the pt-labelled scaleFrom path |
| `origin/feat/pgr-c4` | `16145af2` | 2026-09-24 | 1646 | even | docs(evidence): PGR-C4 — re-capture TestSep13LeadRedactionProof/TestRedactTransaction postfix slots: the originals were  |
| `origin/feat/pgr-critical-fixes` | `a690d7f4` | 2026-09-24 | 1622 | even | fix(engine): PGR-06 HIGH — encrypted-document rollback keeps the encryption password (reload with retained credentials;  |
| `origin/feat/pgr-d2` | `958bd7c0` | 2026-09-24 | 1625 | even | fix(batch): PGR-38 — account mid-run progress against the run's captured file total |
| `origin/feat/pgr40-quickjs-bump` | `020c0734` | 2026-09-24 | 1635 | even | fix(deps): PGR-40 check lane — bump enforced quickjs-ng pin 0.15.0 -> 0.15.1 (MSYS2 0.15.1-1); FIX IS NOT IN 0.15.1 — PG |
| `origin/feat/pr-review-fixes` | `a25c37b7` | 2026-09-25 | 1661 | even | docs(ledger): PR-review fix lane 3 rows — §3 items 4/5/6, §4 M1–M3, §5 ponytail (implemented-awaiting-review); residuals |
| `origin/feat/printable-summaries` | `b06b2f07` | 2026-09-15 | 1299 | even | docs(audit): printable summaries lane ledger — 2026-09-15 section (PS-1/PS-2 implemented-awaiting-review); resolves conf |
| `origin/feat/r24-policy` | `335d1d3a` | 2026-09-15 | 1291 | even | docs(audit): R24 ledger section + matrix rows — policy/support-bundle/network-page rows, all implemented-awaiting-review |
| `origin/feat/r24-wiring` | `b8909dcb` | 2026-09-20 | 1303 | even | docs(audit): R24 wiring closure lane - ledger section (R24-W1/W2/W3 rows, implemented-awaiting-review) |
| `origin/feat/redaction-gaps` | `b454d071` | 2026-09-23 | 1534 | even | docs(ledger): redaction gap-fix lane rows — R14 probe-scoping fix + G1–G6 (implemented-awaiting-review) |
| `origin/feat/redaction-research` | `b8d17885` | 2026-09-22 | 1534 | even | docs(audit): REDACTION-RESEARCH refinement — Type3 fallback-advance drift is a real under-excision vector (INCONCLUSIVE  |
| `origin/feat/report-addendum` | `d65b3e35` | 2026-09-23 | 1537 | even | docs(audit): consolidated report ADDENDUM SLOT A3 — re-soak VERDICT PASS for b17106a (exe 509da2c8…, re-hashed post-SOAK |
| `origin/feat/residual-exec` | `5d74c348` | 2026-09-24 | 1629 | even | test(infra): Plan 11 item 3 — register R14ProbeBatchSkip as a deliberate-run probe |
| `origin/feat/residual-plans` | `72f5ce7c` | 2026-09-22 | 1537 | even | docs(audit): RESIDUAL-PLANS-2026-09-21 — execution-ready fix plans for all 15 consolidated-report §2.6 residuals, verifi |
| `origin/feat/resoak-verdict` | `d249bf91` | 2026-09-22 | 1540 | even | docs(audit): RESOAK-VERDICT-2026-09-22 — 48h re-soak PASS for b17106a (720 passes, 48.12h, SOAK END + done; zero candida |
| `origin/feat/rotate270-fix` | `9e1cde9d` | 2026-09-20 | 1530 | even | docs(audit): W2B-1 RESOLUTION addendum + ledger rows — page-space geometry from the raw /MediaBox (implemented-awaiting- |
| `origin/feat/runintersects-precision` | `e620757b` | 2026-09-21 | 1535 | even | fix(proof): runIntersects margin from the run's real glyph band, not a 3*fs blanket (W2c residual) |
| `origin/feat/sanitize-assert-mutable` | `ed04426e` | 2026-09-20 | 1525 | even | fix(sanitize): soak SegFault E-2 — trailer /Info removal orphaned the object PdfDocument::m_Info wraps; Save's CollectGa |
| `origin/feat/send-for-signing-p1` | `f621416d` | 2026-09-19 | 1299 | even | docs(audit): ledger + matrix rows for send-for-signing P1 — implemented-awaiting-review |
| `origin/feat/sep13-fixes` | `e8b9a19e` | 2026-09-14 | 1273 | even | docs(audit): ledger rows for SEP13 M1/M3/M5 stretch fixes (implemented-awaiting-review) |
| `origin/feat/sep13-leads` | `b0fd8296` | 2026-09-14 | 1274 | even | docs(ledger): SEP13 redactfix lane rows — L5/L7/L8 (+L6 wording pin) implemented-awaiting-review |
| `origin/feat/sep13-residual` | `c7e9ecfe` | 2026-09-15 | 1275 | even | docs(ledger): SEP13 residuals lane rows — RES-1 (B-T wording honesty) + RES-2 (RedactOperation lifetime) implemented-awa |
| `origin/feat/soak-48h` | `1d2e76b8` | 2026-09-15 | 1282 | even | docs(audit): R25 48h soak start — candidate 2f75524448ffca09b110a5517ad087a061218b1b (feat/parity-glm tip) |
| `origin/feat/soak-verdict` | `790a7197` | 2026-09-20 | 1526 | even | docs(audit): SOAK-VERDICT-2026-09-20 — verdict FAIL: 48h soak killed at 4h00m21s by Windows Update planned restart (Syst |
| `origin/feat/sweep-backend` | `efbd8235` | 2026-09-22 | 1537 | even | docs(audit): SWEEP-BACKEND internal API surface audit — findings plan-only, doctrine-mapping appendix |
| `origin/feat/sweep-legacy-fix` | `a73419ed` | 2026-09-20 | 1324 | even | docs(audit): SWEEP-LEGACY verdicts + ledger rows — 3 fixes (origin-class page-space law on annotation/form rects; conver |
| `origin/feat/sweep-quality-new` | `673129e1` | 2026-09-20 | 1325 | even | docs(audit): SWEEP-QUALITY-NEW fixes-log — before/after suite totals (66 -> 67 passed, zero behavior change; extraction  |
| `origin/feat/sweep-w1-adversary` | `8bcde898` | 2026-09-20 | 1316 | even | docs(adversary): SWEEP-W1 audit — 5 findings (4 CONFIRMED w/ failing repros, 1 design), 7 refutations w/ pins, 3 hypothe |
| `origin/feat/sweep-w1-fixes` | `e8e715f2` | 2026-09-20 | 1515 | even | fix(FZ-4/fuzz): reject reserved DOS device names and overlong renders in resolveNaming — extends the W1-01 containment g |
| `origin/feat/sweep-w1-security` | `f6d46dd9` | 2026-09-20 | 1322 | even | docs(audit): SWEEP-W1 refuted items (10 pins), disclosure-claims verdicts, inconclusive, coverage, suites, residuals |
| `origin/feat/sweep-w2-gsd` | `125ae929` | 2026-09-23 | 1550 | even | docs(audit): SWEEP-W2-GSD verdicts — all 10 queue rows VERIFIED (probe 12/0 + 10 scoped NCs + final pristine sweep 190/0 |
| `origin/feat/sweep-w2-testing` | `b17106a3` | 2026-09-20 | 1524 | even | docs(audit): SWEEP-W2 §2.2 in-suite grids (FU-2 live-capture verbatims) + §2.3 final classification table + §3.1 patch v |
| `origin/feat/sweep-w2-verify-b` | `84a19f9e` | 2026-09-20 | 1529 | even | docs(audit): SWEEP-W2B verify verdicts — 14 verified + SL1 partial (FINDING W2B-1: /Rotate 270 transposed rects); ledger |
| `origin/feat/sweep-w3-arch` | `46cae7ba` | 2026-09-20 | 1525 | even | docs(audit): SWEEP-W3 architect modularity audit — compile_commands-derived include graph at b17106a (0 cycles, 13 upwar |
| `origin/feat/sweep-w3-archaeo` | `b8a5a564` | 2026-09-20 | 1525 | even | docs(audit): SWEEP-W3 count-table fix — KEEP-ANYWAY is 6 rows (4 src files + 2 docs rows), 8 is the total candidate-row  |
| `origin/feat/sweep-w3-devops` | `fbfa6e47` | 2026-09-20 | 1525 | even | docs(audit): SWEEP-W3 devops — operational readiness of the R25 candidate at b17106a: deploy validation PASS (validator  |
| `origin/feat/sweep-w3-emergence` | `48c2ae51` | 2026-09-21 | 1534 | even | docs(audit): SWEEP-W3-EMERGENCE-2026-09-20 — emergence-engine composition-matrix audit at ec9f16f (7-cell matrix with ve |
| `origin/feat/sweep-w3-perf` | `2ec24f41` | 2026-09-20 | 1529 | even | docs(audit): PERF-BASELINE-2026-09-20 FINAL — quiet-run numbers appended (gate passed) |
| `origin/feat/sweep-w3-research` | `e97f7075` | 2026-09-20 | 1534 | even | docs(audit): SWEEP-W3-RESEARCH-2026-09-20 — research/tracking corpus reconciliation log at ec9f16f (16 backlog rows re-s |
| `origin/feat/sweep-w3-ui` | `164dabd4` | 2026-09-20 | 1528 | even | docs(audit): SWEEP-W3-UI acceptance evidence at b17106a — offscreen R17 matrix (1366x768+1920x1080 x 100/150/200%), 6/6  |
| `origin/feat/ui-narrow-viewport` | `c467e6a7` | 2026-09-23 | 1534 | even | docs(audit): SWEEP-W3-UI F1-RESOLVED — $2 matrix row 3a-R (implemented-awaiting-review) + $8 addendum: before/after grab |
| `origin/feat/unreviewed-map` | `0bcbd6bf` | 2026-09-23 | 1537 | even | docs(audit): UNREVIEWED-CODE-MAP-2026-09-23 — authoritative map of implemented-awaiting-review code for the endgame cons |
| `origin/feat/ux-defects-fixes` | `f01a4e37` | 2026-09-23 | 1552 | even | test(s4s)+docs(audit): drive the re-confirm channel the gate actually reads + UX fixes ledger rows |
| `origin/feat/ux-integration-fixes2` | `14d69ce9` | 2026-09-23 | 1607 | even | docs(ledger): ux-integration fix lane 2 rows — slot-scoped modal drivers, flow2b batchFinished wait, F2a-F1 batch-merge  |
| `origin/feature/editing-parity` | `97172fbe` | 2026-07-12 | 1050 | even | test(editing): add TestEditingWave1B regression coverage + fix 2 real bugs it caught |
| `origin/feature/redaction-parity` | `1263df9a` | 2026-09-07 | 1049 | local-ahead+1 | WIP snapshot: secure in-flight engine-interface changes from the redaction-parity session |
| `origin/feature/viewing-parity` | `de1fa268` | 2026-09-07 | 1090 | even | WIP snapshot: secure viewing-parity session state |
| `origin/main` | `d03d6e94` | 2026-09-02 | 1115 | local-ahead+2 | feat(export): §9.16 — local-processing badge on all export completions and import cards |
| `origin/reconexec/snapshot-redaction-gaps-dirty` | `2adc3df6` | 2026-09-23 | 1539 | even | preservation: r14 probe-dirty worktree state (PoDoFoBackend G3-block probe deletion + W2 probes/diags) snapshot before l |
| `origin/review/consolidated-parity` | `12da4e2f` | 2026-09-29 | 1742 | even | docs(audit): FIXALL handoff FINAL — Phases 3-5 complete (§6 re-verification all green ×3; E1 fresh build 945 steps 0 err |

Twin status summary: even ×83, diverged ×2, no-local-twin ×1, local-ahead+585 ×1, local-ahead+1 ×1, local-ahead+2 ×1. (`origin`/HEAD → `d03d6e94` = origin/main, row omitted above.)

## 4. Worktrees (22)

| Worktree | Branch | Tip | Tracked-dirty | Untracked | Lane status | First dirty entries |
|---|---|---|---|---|---|---|
| `C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/subagent-AST-Architect-self-54b52cfc` | `subagent-AST-Architect-self-54b52cfc` | `d7a8ca06` 2026-09-13 | 703 | 1 | STALE/ACCIDENTAL — antigravity subagent scratch; ~703 staged junk files incl. signing keys | A  dist/GlyphPDF-1.0.0-x64.msi;A  dist/GlyphPDF-1.0.0-x64.wixpdb;A  tests/fixtures/signing/ca.key; |
| `C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/subagent-AST-Architect-self-a1982f71` | `subagent-AST-Architect-self-a1982f71` | `f9805e31` 2026-05-30 | 703 | 0 | STALE/ACCIDENTAL — antigravity subagent scratch; ~703 staged junk files incl. signing keys | A  dist/GlyphPDF-1.0.0-x64.msi;A  dist/GlyphPDF-1.0.0-x64.wixpdb;A  tests/fixtures/signing/ca.key; |
| `C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/subagent-Forms-Specialist-self-26881e20` | `feature/m4-forms` | `68c4734a` 2026-05-30 | 703 | 0 | STALE/ACCIDENTAL — antigravity subagent scratch; ~703 staged junk files incl. signing keys | A  dist/GlyphPDF-1.0.0-x64.msi;A  dist/GlyphPDF-1.0.0-x64.wixpdb;A  tests/fixtures/signing/ca.key; |
| `C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/subagent-Vendoring-Specialist-self-7708fcd5` | `subagent-Vendoring-Specialist-self-7708fcd5` | `f9805e31` 2026-05-30 | 703 | 2 | STALE/ACCIDENTAL — antigravity subagent scratch; ~703 staged junk files incl. signing keys | A  dist/GlyphPDF-1.0.0-x64.msi;A  dist/GlyphPDF-1.0.0-x64.wixpdb;A  tests/fixtures/signing/ca.key; |
| `C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/subagent-Vendoring-Specialist-self-ca2c3f27` | `subagent-Vendoring-Specialist-self-ca2c3f27` | `5a018ae5` 2026-09-13 | 703 | 3 | STALE/ACCIDENTAL — antigravity subagent scratch; ~703 staged junk files incl. signing keys | A  dist/GlyphPDF-1.0.0-x64.msi;A  dist/GlyphPDF-1.0.0-x64.wixpdb;A  tests/fixtures/signing/ca.key; |
| `C:/Users/User/Projects/pdf-clean` | `feat/parity-glm` | `195e4309` 2026-09-23 | 0 | 0 | ACTIVE — explorer/base session worktree (feat/parity-glm mega-lane) | CLEAN |
| `C:/Users/User/Projects/pdf-consolidate` | `consolidate/all` | `95dccb23` 2026-09-23 | 0 | 0 | FINISHED — consolidate/all fold branch | CLEAN |
| `C:/Users/User/Projects/pdf-inst` | `feat/fixall-redaction` | `5461b72d` 2026-09-29 | 0 | 0 | FINISHED (folded) — feat/fixall-redaction → PGR-46 folded as `99dd7b67` | CLEAN |
| `C:/Users/User/Projects/pdf-keyA` | `feat/fixall-images` | `d63ed76e` 2026-09-29 | 0 | 0 | FINISHED (folded) — feat/fixall-images → N1 folded as `2ead0b17` | CLEAN |
| `C:/Users/User/Projects/pdf-keyC` | `feat/fixall-ci2` | `590c6c27` 2026-09-28 | 0 | 0 | FINISHED (folded) — feat/fixall-ci2 → CX-13..17 folded | CLEAN |
| `C:/Users/User/Projects/pdf-parity` | `feat/sep13-fixes` | `e8b9a19e` 2026-09-14 | 0 | 4 | FINISHED — feat/sep13-fixes lane | ?? build-beta-reconf.log;?? build-beta-reconf2.log;?? build-beta-sep13.log; |
| `C:/Users/User/Projects/pdf-r15` | `feat/fixall-inv1` | `898d362f` 2026-09-28 | 0 | 0 | FINISHED (folded) — feat/fixall-inv1 → INV-1 folded as `64baba6a` | CLEAN |
| `C:/Users/User/Projects/pdf-r18` | `feat/pr-review-fixes` | `a25c37b7` 2026-09-25 | 0 | 55 | FINISHED (folded) — feat/pr-review-fixes lane | ?? build-uxint-harness.log;?? build-uxint2-c1.log;?? build-uxint2-c2.log; |
| `C:/Users/User/Projects/pdf-redaction` | `feature/redaction-parity` | `45bf4302` 2026-09-13 | 0 | 1 | FINISHED — feature/redaction-parity; 1 unpushed wip commit + task.md | ?? task.md; |
| `C:/Users/User/Projects/pdf-sec` | `feat/fixall-tagging2` | `0144d056` 2026-09-29 | 0 | 0 | FINISHED (folded) — feat/fixall-tagging2 lane | CLEAN |
| `C:/Users/User/Projects/pdf-worktrees/editing` | `feature/editing-parity` | `97172fbe` 2026-07-12 | 0 | 0 | FINISHED — feature/editing-parity | CLEAN |
| `C:/Users/User/Projects/pdf-worktrees/viewing` | `feature/viewing-parity` | `de1fa268` 2026-09-07 | 0 | 0 | FINISHED — feature/viewing-parity | CLEAN |
| `D:/pdf/pdf` | `main` | `2b715f47` 2026-09-13 | 1 | 17 | ACTIVE — trunk; 2 unpushed commits; dirty docs + uncommitted security review |  M docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md;?? _review_diff.txt;?? auditbuild.log; |
| `D:/pdf/pdf-base7d` | `DETACHED` | `7d4d8d08`  | 0 | 0 | SNAPSHOT — detached HEAD `7d4d8d08` | CLEAN |
| `D:/pdf/pdf-featplans` | `feat/batch-presets-p2` | `ec22eeed` 2026-09-24 | 0 | 16 | FINISHED (folded) — feat/batch-presets-p2 → U1-U7 folded | ?? build-p22.log;?? build-u4.log;?? build-u5.log; |
| `D:/pdf/pdf-review` | `review/consolidated-parity` | `12da4e2f` 2026-09-29 | 0 | 0 | ACTIVE — PR #2 head; fixall integrator; FINAL handoff 2026-09-29 lives here | CLEAN |
| `D:/pdf/pdf-verify` | `test/view-parity-baseline` | `0948743b` 2026-09-28 | 0 | 5 | PARKED — test/view-parity-baseline — owner: out of scope (09-28) | ?? build-configure.log;?? build.log;?? build2.log; |

### Dirty-state details

- **`D:/pdf/pdf` [main]** — tracked `M docs/audit/PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`; untracked:
  `_review_diff.txt`, `auditbuild.log`, `build-audit-build.log`, `build-audit/`, `build-release/`,
  `ctest-pass1..3.log`, `deploy.bak-2026-09-09/`, `deploy14.log`, `docs/audit/SECURITY-QUALITY-REVIEW-parity-glm.md`
  (uncommitted deliverable), `e1verify.log`, `rma_diag.txt`, `verify14.log`, `verify14b.log`, `wixbuild14.log`, `zipbuild14.log`.
- **5 antigravity subagent worktrees** (`C:/Users/User/.gemini/antigravity/brain/cfac9e93-1de9-4ffb-bd71-3ac33e876c6e/.system_generated/worktrees/`)
  — 700-703 staged files each: `vcpkg_installed/**` (698), `dist/GlyphPDF-1.0.0-x64.msi` + `.wixpdb`, `graphify-out/…`,
  `tests/fixtures/signing/{ca.key,signer.key,test_signer.p12}` (keys staged!). Branches `subagent-AST-Architect-self-54b52cfc`,
  `subagent-AST-Architect-self-a1982f71`, `subagent-Forms-Specialist-self-26881e20` (= `feature/m4-forms` tip `68c4734a`),
  `subagent-Vendoring-Specialist-self-7708fcd5`, `subagent-Vendoring-Specialist-self-ca2c3f27`.
- **`pdf-parity`** — 4 untracked build logs. **`pdf-r18`** — 55 untracked build logs. **`pdf-redaction`** — untracked `task.md`.
  **`pdf-featplans`** — 16 untracked build logs. **`pdf-verify`** — 5 untracked build logs.
- All other Project worktrees CLEAN (index + worktree).

## 5. Tags (102)

88 lightweight (`objecttype=commit`) + 14 annotated (`objecttype=tag`) = 102. **89 are `archive/*`**; the non-archive tags:

| Tag | Type | Tip | Date | Subject |
|---|---|---|---|---|
| `m4-catchup-complete` | annotated | `d996c961` | 2026-05-30 | M3+M4 consolidated catchup checkpoint — 23/23 ctest, clean tree, 8 walkthroughs, vault synced |
| `v1.0.0` | lightweight | `62f6e301` | 2026-06-13 | fix(packaging): bundle VC++ 2022 runtime and add portable ZIP |
| `v1.0.1` | annotated | `fcc0f503` | 2026-06-14 | GlyphPDF v1.0.1 — maintenance release (veraPDF bundle, VC++ runtime fix, runtime tool detection) |
| `v1.2.0` | lightweight | `f4987cc0` | 2026-06-15 | fix(tests): restore QEXPECT_FAIL guards in TestDjotFuzz — CI was failing |
| `v1.2.1` | lightweight | `6d17857e` | 2026-06-15 | release: v1.2.1 — fill real SHA-256 hashes after build |
| `v1.3.0` | annotated | `4cb6f651` | 2026-06-15 | GlyphPDF v1.3.0 — nine PRD-gap features (annotation, calculated form field, batch OCR/Merge/Redact,  |
| `v1.3.1` | annotated | `20e44a78` | 2026-06-16 | GlyphPDF v1.3.1 — OCR Verify cleanup, in-app update UX, branded installer wizard |
| `v1.3.2` | annotated | `b0757890` | 2026-06-21 | GlyphPDF v1.3.2 — audit remediation + unsigned release build |
| `v1.3.2.1` | annotated | `3c29f629` | 2026-06-21 | GlyphPDF v1.3.2.1 — welcome screen + update-check hang fixes |
| `v1.3.2.2` | annotated | `406b3723` | 2026-06-22 | GlyphPDF v1.3.2.2 — UI/UX sweep |
| `v1.3.2.3` | annotated | `39156637` | 2026-06-22 | GlyphPDF v1.3.2.3 — security & hardening |
| `v1.4.0` | annotated | `0245f378` | 2026-09-02 | GlyphPDF v1.4.0 - competitive-parity features (OCR, hyperlinks, redaction, compression) + security f |
| `vcpkg-build-final` | annotated | `5006b58e` | 2026-05-28 | Last build before MSYS2 migration |

`archive/*` tags (89): `annotation-eraser`, `branch/audit-remediation`, `branch/claude/modest-mccarthy-riuo2o`, `branch/feat/accessibility-p1`, `branch/feat/annotation-eraser`, `branch/feat/batch-presets-p1`, `branch/feat/candidate-leak-fix`, `branch/feat/consolidated-report`, `branch/feat/consolidation-plan`, `branch/feat/consolidation-stage1`, `branch/feat/dispatch-gates`, `branch/feat/emergence-fixes`, `branch/feat/feature-plans`, `branch/feat/followups-2026-09-15`, `branch/feat/glm-comp`, `branch/feat/glm-ocr`, `branch/feat/l7-rotate-annot`, `branch/feat/modularity-moves`, `branch/feat/parity-glm`, `branch/feat/parity-glm-clean`, `branch/feat/parity-glm-formjs`, `branch/feat/parity-glm-gate`, `branch/feat/parity-glm-gateC`, `branch/feat/parity-glm-infra`, `branch/feat/parity-glm-integration`, `branch/feat/parity-glm-n17n18`, `branch/feat/parity-glm-packa`, `branch/feat/parity-glm-packafix`, `branch/feat/parity-glm-quick`, `branch/feat/parity-glm-r05`, `branch/feat/parity-glm-r18f`, `branch/feat/parity-glm-r22`, `branch/feat/parity-glm-resid2`, `branch/feat/parity-glm-review`, `branch/feat/parity-glm-sec`, `branch/feat/parity-glm-t2`, `branch/feat/printable-summaries`, `branch/feat/r24-policy`, `branch/feat/r24-wiring`, `branch/feat/redaction-gaps`, `branch/feat/redaction-research`, `branch/feat/report-addendum`, `branch/feat/residual-plans`, `branch/feat/resoak-verdict`, `branch/feat/rotate270-fix`, `branch/feat/runintersects-precision`, `branch/feat/sanitize-assert-mutable`, `branch/feat/send-for-signing-p1`, `branch/feat/sep13-fixes`, `branch/feat/sep13-leads`, `branch/feat/sep13-residual`, `branch/feat/soak-48h`, `branch/feat/soak-verdict`, `branch/feat/sweep-backend`, `branch/feat/sweep-legacy-fix`, `branch/feat/sweep-quality-new`, `branch/feat/sweep-w1-adversary`, `branch/feat/sweep-w1-fixes`, `branch/feat/sweep-w1-security`, `branch/feat/sweep-w2-gsd`, `branch/feat/sweep-w2-testing`, `branch/feat/sweep-w2-verify-b`, `branch/feat/sweep-w3-arch`, `branch/feat/sweep-w3-archaeo`, `branch/feat/sweep-w3-devops`, `branch/feat/sweep-w3-emergence`, `branch/feat/sweep-w3-perf`, `branch/feat/sweep-w3-research`, `branch/feat/sweep-w3-ui`, `branch/feat/ui-narrow-viewport`, `branch/feat/unreviewed-map`, `branch/feat/ux-defects-fixes`, `branch/feat/ux-integration-fixes2`, `branch/feature/editing-parity`, `branch/feature/redaction-parity`, `branch/feature/viewing-parity`, `docs-v1-8017c525`, `editing-parity`, `pr-head-before-picks`, `redaction-parity`, `squash-v1-e6872ed2`, `stash-0`, `stash-1`, `stash-2`, `stash-3`, `stash-4`, `stash-5`, `stash-6`, `viewing-parity`.

## 6. Stashes (7)

| Ref | Date | On | Description |
|---|---|---|---|
| stash@{0} | 2026-09-14 | feat/parity-glm-quick | q1-test-wip |
| stash@{1} | 2026-09-13 | feat/parity-glm | r14-preexisting-model-lineendings |
| stash@{2} | 2026-09-08 | (e1-lane) | e1-lane: unstaged others |
| stash@{3} | 2026-09-05 | feat/parity-glm | WIP (ledger F06/R11 era, base abdb3aa) |
| stash@{4} | 2026-08-26 | feat/parity-ox-auto | ox-alpha WIP: §9.13 dedup TestImageDedup (FAILING, incomplete) — parked when stealth/ox-alpha 404'd |
| stash@{5} | 2026-08-25 | main | accessibility WIP (borrowed tree for regex find/replace work) |
| stash@{6} | 2026-07-02 | feature/ocr-parity | WIP (base 055592d, linearized export preset checkbox) |

## 7. Archive / backup safety net

| Artifact | Kind | Hash / Tip | Notes |
|---|---|---|---|
| `D:/pdf/pdf-backup-pre-purge-20260610.bundle` | git bundle, 73,783,706 B | sha256 `960817c38740b82fe646b7f58839c4796b440b914c3191e1892f36b6a21c75dc` | Created 2026-06-10. Heads: `audit-remediation@d012ab2f`, `feature/m4-djot-foundation@69a47e82`, `feature/m4-edge@c22153ed`, `feature/m4-forms@0f81d97d`, `feature/m4-security@c22153ed`, `main@8b924f0c`, `msys2-migration-backup-pre@bc8c5650`, `r2-1-chain1@62bb2091`, `r2-2-silent-failure@e1c19777`, `r2-3-redaction@49324fb0`, … — **pre-consolidation-era only; stale for the endgame** |
| `backup/regex-verified-a39356e` | local branch | `a39356e7` (2026-08-25) | feat(search): PRD §9.15 regex + whole-word find & replace — verify-backup pointer |
| `archive/*` tags | 89 lightweight tags | see §5 | per-branch archive pointers |
| `D:/pdf/nc-backup-u1/` | loose source dir, 232 KB | — | BatchMode/BatchPreset .cpp/.h snapshot |

**Gap:** no post-2026-06-10 bundle exists. PR #2 handoff step "refresh the all-refs backup bundle" is still TODO.

## 8. PR #2 — `review/consolidated-parity` → `main`

| Field | Value |
|---|---|
| State | OPEN, MERGEABLE, no conflicts |
| Head | `12da4e2f` (== local twin `review/consolidated-parity`, worktree `D:/pdf/pdf-review`) |
| Base | `main` (origin `d03d6e94`; local main `2b715f47` is 2 ahead — see §1.1) |
| Size | 830 files, +166,863 / −4,635, 100 commits, **0 merge commits** (linear), main is an ancestor |
| Reviews | **None** (0 reviews, 0 review comments, 0 issue comments on GitHub) |
| CI | Build ✓, content-spans-sanitizer ✓ (ASan+UBSan), both license/config guards ✓; **Test step red on 4 named items** (below); one run still `pending` at capture |
| Gates (local fixall session, 2026-09-29) | E0 hygiene PASS; E1 fresh Release build 945/945 steps; E2 full ctest 184/186, **serial 185/185 = 100%**; E3 purge/linearity PASS; E4 secret scan 0 matches in 166,613 added lines; E5 ledger 0 unexplained |

### Findings status (evidence: `docs/audit/CONSOLIDATION-HANDOFF-FIXALL-2026-09-25.md` in `D:/pdf/pdf-review`)

- **All 21 tracked findings FIXED on the PR**: CX-01..CX-17, N1 (`2ead0b17`), PGR-46 (`99dd7b67`, was the
  last CRITICAL-class open item), INV-1 (`64baba6a`). Each row of §4 of that handoff names its commit, test pin,
  and fail-before/pass-after evidence; final code SHA `2ead0b17` (only docs commits after it).
- **PGR-23**: superseded by `6841247d` (already on PR). **PGR-44/37**: `6600a429`. **PGR-42..45**: `6600a429` et al. Ledger: 0 unexplained.
- **Still open (owner items; none blocking content):**
  1. `TestAccessibilityPanel` load race — NEW defect, the only crash (segfault under -jN contention; solo 16P/0F ×3).
  2. `TestWelcomeRoutes` — known flake (solo green 20P/0F).
  3. `TestSweepW3UxFlows` — known parallel flake.
  4. `TestSignatureRealCrypto` — one CI env flake (inherited flake list).
  5. OLE compound-signature residual (`\xD0\xCF\x11\xE0…`) missing from the container refusal list (legacy .doc/.xls scanned as plain payload).
  6. batch-presets LOW residual: `exportTo` remove-then-copy window.
  7. PGR-33 untouched (owner decision), PGR-40/41 deferred, Rotate View not ported, `test/view-parity-baseline` out of scope (09-28).

**The handoff's own endgame recipe (its §9):** rebuild + full suite independently → merge PR #2 keeping its commits →
refresh the all-refs backup bundle → delete other branches/stale worktrees → take the Panel race first.

## 9. Disk pressure

**Junction map (important for cleanup):** every `C:/Users/User/Projects/pdf-*` entry is a **junction into `D:/pdf/*`**
(`pdf`→`D:/pdf/pdf`, `pdf-clean`→`D:/pdf/pdf-clean`, `pdf-clean-notess`→`D:/pdf/pdf-clean-notess`, `pdf-featplans`,
`pdf-inst`, `pdf-keyA`, `pdf-keyC`, `pdf-parity`, `pdf-r15`, `pdf-r18`, `pdf-redaction`, `pdf-sec`,
`pdf-worktrees`) — except `pdf-consolidate`, a real directory on C: (54 MB). **All real bytes live on D:.**

All `D:/pdf` directories by size (du, captured 2026-09-29; build dirs dominate each worktree):

| Rank | Directory (worktree/branch) | Size |
|---|---|---|
| 1 | `pdf-keyA` — feat/fixall-images — FINISHED (folded) | 59G |
| 2 | `pdf-inst` — feat/fixall-redaction — FINISHED (folded) | 41G |
| 3 | `pdf-r15` — feat/fixall-inv1 — FINISHED (folded) | 29G |
| 4 | `pdf-sec` — feat/fixall-tagging2 — FINISHED (folded) | 27G |
| 5 | `pdf-keyC` — feat/fixall-ci2 — FINISHED (folded) | 24G |
| 6 | `pdf-r18` — feat/pr-review-fixes — FINISHED (folded) | 22G |
| 7 | `pdf` — main trunk (+ build-audit/, build-release/, deploy.bak-…) | 20G |
| 8 | `pdf-featplans` — feat/batch-presets-p2 — FINISHED (folded) | 20G |
| 9 | `pdf-review` — review/consolidated-parity — PR #2 ACTIVE | 6.3G |
| 10 | `pdf-clean` — feat/parity-glm — ACTIVE (explorer) | 3.9G |
| 11 | `pdf-parity` — feat/sep13-fixes — FINISHED | 3.1G |
| 12 | `pdf-verify` — test/view-parity-baseline — PARKED | 1.9G |
| 13 | `pdf-base7d` — detached snapshot | 1.7G |
| 14 | `pdf-worktrees` — editing + viewing parity worktrees | 1.1G |
| 15 | `pdf-redaction` — feature/redaction-parity — FINISHED | 175M |
| 16 | `pdf-clean-notess` — orphan checkout (not a registered worktree) | 4.1M |
| 17 | `nc-backup-u1` — loose source backup | 232K |
| 18 | `pdf-backup-pre-purge-20260610.bundle` — safety-net bundle | 70M |

Sum under `D:/pdf`: **~260 GB**. The top-10 above are the requested largest directories; the six largest
(`pdf-keyA`, `pdf-inst`, `pdf-r15`, `pdf-sec`, `pdf-keyC`, `pdf-r18` — **202 GB combined**) are all FINISHED,
already-folded fixall lanes — the primary endgame deletion candidates; they are worktree-linked, so
`git worktree remove` + branch delete reclaims them.

D: free space at capture: **386 GB free of 895 GB (57% used)** — no emergency, but deleting the folded lanes
would roughly halve the repo footprint.

## 10. Survey method + residuals

- Branch metadata: `git for-each-ref`; file counts: `git ls-tree -r --name-only <b> | wc -l`; divergence:
  `git rev-list --left-right --count review/consolidated-parity...<branch>`; twin status: same command against the local twin.
- Worktree dirty state: `git --no-optional-locks status --porcelain` per worktree.
- PR data: `gh pr view 2` + `gh api repos/eliets2/glyph-pdf/pulls/2/comments` (0) + `gh pr checks 2`.
- Residuals: (a) per-branch "purpose" is the subject line verbatim (truncated at 150 chars) plus the family table above;
  (b) stash diffs not individually diffed — titles/dates only; (c) bundle head list truncated to the 10 shown by
  `git bundle list-heads | head`; (d) du measured live — sizes drift as consolidation proceeds; (e) survey captured
  2026-09-29 but the deliverable carries the requested 2026-09-25 name.

