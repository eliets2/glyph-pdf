# Unreviewed-Code Map — 2026-09-23

**Purpose.** Authoritative map of where every UNREVIEWED (implemented-awaiting-review) change sits,
produced for the endgame consolidation (fold origin's branches into main + the integration line).
Audit + map only — no consolidation executed here.

- Branch of this audit: `feat/unreviewed-map` (from `feat/parity-glm`)
- Integration-line ("mainline") tip checked against: **26c9a415** ("Merge branch 'feat/modularity-moves' into feat/parity-glm")
- Source of the row set: `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` @ 26c9a415
  (+ the gsd-branch copy of the same ledger, see §6, for in-flight rows)
- Method: full parse of every ledger table row (275 rows), status classification per cell and per
  section prose ("All rows implemented-awaiting-review"), carrying-commit resolution (Commit column,
  falling back to row SHAs and lane-branch history), `git merge-base --is-ancestor` of every carrying
  commit vs 26c9a415, and file derivation via `git show <sha> --name-only`.
  Parsed row inventory + scripts: `.context/unrevmap-scratch/` (ledger_rows.json, file_map.json,
  files_by_commit.json, ledger_gsd_rows.json).

## 1. Headline numbers

| Bucket | Count | Meaning |
|---|---|---|
| **implemented-awaiting-review rows** | **222** | implemented, never independently reviewed (221 table rows + E-1 recorded as "fixed") — THE map's core |
| verified rows | 43 | flipped by the passes in §3 |
| partial | 2 | both are the SAME finding W2B-1/SL1 (sweep-legacy row + W2B verdict row) — resolved by the W2B-1 fix lane |
| deferred / reconciled / consolidated / refuted | 4 / 2 / 1 / 1 | bookkeeping rows, not code awaiting review |
| **awaiting rows NOT reachable from 26c9a415** | **0** | zero branch-only loss at the tip (proof in §4) |
| distinct carrying commits | 254 | all resolve, all ancestors of 26c9a415 |
| distinct files touched by awaiting rows | 348 | file-level map in §5 |

## 2. The awaiting set (222 rows) — where it sits

All rows below are implemented-awaiting-review at 26c9a415. Carrying commit = ledger Commit column
(∅ = unnamed in the ledger; resolved carrier listed, found via the lane's docs commits / branch history).

| Ledger section (lane) | Rows | Carrying commits (representative) |
|---|---|---|
| September repair packages F01–F12 (R01–R12) | 12 | 5c8e111, 9d53957, 8133f58, 87d98f8, 8e503b4, a7b3c32, 567b906, 466c706, 06b542d, 83e7c8a, f2fab98 |
| July-audit P0 selected rows | 14 | 4d8d3fb, 45aa606, 97f656d, b79415a, fa3b957, b64aef2, 8a278db, 10efbd5, 61fac01, ef02541, 65395da, 05a3336, 0a93f62, 452bfa2 |
| UI packages U01–U08 (landed 2026-09-05/07, commit column absent) | 9 | 34a11ef1 (U01), 10b983c5+9ff9e7c5 (U02), 9ca2547 (U03), 11569ff (U04), 436aaa8 (U05), a09468a (U06), 968d242 (U07), 3dc976b+07bf8c5 (U08) — carriers located via the same-day `docs(ledger): Uxx` commits |
| July-parity P1 follow-ups | 17 | bcef6ad, 24c479d, a9a3eda, b43ef08, c862307, 28e8df6, 74a840e, be47cce, 706a60c, 22a7b66, c7fc7ef, 32c7780, 69cb6e4, 0364f48, 808965b, a21ecc8, bc27cf2 |
| REMOTE-PARITY-REVIEW findings (D03/D06/D07/parseJson/E-1-residual/U02-theme/N08) | 7 | 9435b1be (D06+D07), 603d984 (D03), 1582f46, 0fe5953, 9ff9e7c, 9a912b9+04f0f0c |
| LATEST-QUALITY-REVIEW N01–N10 | 9 | f9f3a95, c764f73, 47d2fe1, 1d4daca, f8704aa, 6ee8eb7, 866321d, a7a1bf0, 61e51da |
| PARITY-BRANCH-REVIEW V01–V06 | 7 | (per-row; e.g. V06 = 0696036, V03/V04/V05 = 0420cb5/250bbad/ff8c1b9) |
| CODE-REVIEW-2026-09-06 D01/D02 | 2 | 8bd01d5, 4d45769, 3c3be82 |
| E-1 excision corruption (recorded "fixed") | 1 | 2c5a0d6 |
| 2026-09-08 review-cycle repairs (steps 1–5: EC/ARC/INF/NCR/Q02) | 31 | 934801e (EC01), 5c3f1d1, 75b77db, ARC/4A-lane 3b27317, 4B-lane d897691, 18a27a9+aee33e5+e03273e (gateE), INF02–06 bc14f3c/3b3ec1d/699a89a/b2d9906+48a9552/9024124, Q02 e006a52/f96efe7 |
| T2 lane (Redaction Proof Mode) + T1 lane (Measurement) | 6 | 15f3f1c+2703667 (T2), T1 lane commits |
| Gate B / G01–G20 gate lanes (incl. G05 merge repair) | 28 | gate-lane commit series (G07/G08/G12/G13, G01–G06, G10/G11/G14/G15, G16–G20 gateD) |
| form-JS Phase 1 (feat/parity-glm-formjs) + R05/JS-01 deadline fix | 2 | formjs + r05 lane commits |
| native Linux lane (WP-R20 + R21) + R22 installed-resources | 6 | 9b902895, 3388d620, 0183e59 (R20-tail), 84b11eaa, 9e4c6d9 (R21), 58e3914+8a6e4e4+875c1c4 (R22) |
| R06/R13, R04/R08/R10, R11/R12 lanes | 11 | r06r13 commits, e0f3085+c10b20c (WP-R10), c92a2cc+caece3d (R11/R12) |
| Pack A Tier-2 features | 4 | 7e32093 (+test commit) |
| R15–R17 UI wave, SEP13 hardening, packafix | 16 | r15r17/sep13/packafix lane series |
| sep13-leads (L lanes), sep13-fixes, sep13-residual | 8 | sep13 lane series (fixes; verified subset in §3) |
| R18(f)/R19, R24 policy + wiring, printable summaries | 14 | bcd34eb (R18f), 8ea3876/c766623/60212ca (R24), 03f4606/d574530/cbd599b (R24-W1..3), printable-summaries commits |
| send-for-signing P1 (S4S-1..5) | 5 | 57cca6d ×3, 97c59a1, bc0ade4 |
| follow-ups lane (FU-1..4), batch-presets P1, T2-4 accessibility P1 | 8 | 15c0d112, 5074da6, 4d0b606, 5c8fd08 (FU-4 root fix), batch-presets commits, accessibility-p1 commits |
| F1 rotate-annot repair (R14 PARTIAL resolution) | 1 | 7189149 |
| sanitize-crash lane E-2 | 1 | ed04426 |
| W2B-1 fix lane (/Rotate 270) — awaiting at tip | 3 | 879c171 + 28b5c48 |
| SWEEP-W3 architect moves (AM1, AM2) | 2 | 5c02b01, 791115b |

## 3. Verification overlay — the 43 verified rows (who flipped what)

| Verifying pass | Rows flipped | Where recorded |
|---|---|---|
| 2026-09-05 review session (bar: "verified commits through d03d6e9 on main") | 2 (§9.14 async reading order 1da4ffe, §9.12 batch flake 7de331b) | ledger "Already-landed" section |
| **R14 independent review 2026-09-14** (INDEPENDENT-REVIEW-2026-09-14.md) | 21 (+1 verified-with-caveat, +1 verified-at-tip r18-review F1/F2/F3) — Q-lane 4, sep13-leads 3+1caveat, sep13-fixes 11, F1-repair 3 | ledger flip arrows "implemented-awaiting-review → **verified** (R14 …)" |
| **SWEEP-W2B 2026-09-20** (feat/sweep-w2-verify-b @ 2d29a16) | 19 — W1-01..05, F1–F6, FZ-1..4 (15) + SL2, SL3 + sweep-legacy verdict rows; SL1 = PARTIAL (FINDING W2B-1) | ledger SWEEP-W2B table + docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md |
| SWEEP-W2-TESTING 2026-09-20 | 0 row flips — it is the testing/hygiene grid (flakes, coverage, harness); its deliverables (TestModeStripPins 0ef416d, FU-2 RESOURCE_LOCK ae636a5) are merged at the tip and not rowed | docs/audit/SWEEP-W2-TESTING-2026-09-20.md |
| **SWEEP-W2C 2026-09-20/21** | ⚠ **BRANCH-ONLY (feat/sweep-w2-gsd)**: flips the 3 tip-awaiting W2B-1 rows + SL1 (both copies) to verified, lifts the F5 rotated-page caveat, verifies SEP13 L5/L8 offset-270 — via independent probe W2CProbeRotate270 7/7 (4525f0e1) | gsd-side ledger + docs/audit/SWEEP-W2C-2026-09-20.md |

**Effective pending set after the final merge batch** = 222 − 3 (W2B-1 rows already verified branch-only
by W2C) = **219**, plus the 8 gsd-only awaiting rows of §6 land with the same batch.

## 4. Ancestry cross-check — the zero-loss proof (tip side)

Every one of the 222 awaiting rows' carrying commits (254 distinct, every one resolving in this repo)
satisfies `git merge-base --is-ancestor <sha> 26c9a415`. **Zero awaiting rows are branch-only; zero
carrying SHAs failed to resolve.**

Two rows initially flagged BRANCH-ONLY by raw SHA matching, investigated and resolved as NOT lost:

1. **§9.16 local badge — cell "d03d6e9/0a93f62".** d03d6e9 lives only on `main` (the 2026-09-05
   review's verified-through commit); the integration line carries the same feature via **0a93f62**
   (2026-09-03, verified ancestor). Both lines have the content; the consolidation merge of main will
   bring d03d6e9's side (WelcomeWidget.cpp) — expect a benign duplicate-carrier situation, not a loss.
   Context: `main` and `feat/parity-glm` diverged at 1669f70d — main holds 428 commits not on the
   integration line, the integration line 877 not on main. **The consolidation cannot fast-forward;
   both sides must be merged.**
2. **§9.7-a initials variant — cell "be47cce → 8dae489".** be47cce IS an ancestor of the tip.
   8dae489 is a rebased-away twin (parent d309d9df) contained in NO branch; its extra diff is later
   re-landed work, and the initials content at the tip is be47cce + N01's 47d2fe1. Stale rebase
   artifact — nothing to recover.

Residual honesty note: for rows whose ledger Commit cell is empty or "(this commit)", the carrier was
reconstructed (UI packages via same-day docs commits; D06/D07, E-1, E-2, FU-4, F1, R20-tail, R21,
W2B-1, AM1 via lane branch history). All reconstructed carriers verified ancestors. The 4 "deferred"
rows (N06/D01/D04/NCR-01 residuals → lane 4A) are open work items, not unreviewed code; the step-4
sections that consumed them are themselves awaiting rows already counted.

## 5. File-level map (awaiting rows → files)

348 distinct files. Top surfaces (rows referencing the file; commit = distinct carrying commits):

| File | Rows | Commits | What review still owes here |
|---|---|---|---|
| CMakeLists.txt | 92 | 80 | per-target registration correctness (every lane added if(EXISTS) test blocks) |
| src/GpMainWindow.cpp | 37 | 25 | shell wiring: relay/status-bar/policy seams (U02, R24-W1, ARC06/07, N07) |
| src/engines/podofo/PoDoFoBackend.cpp | 29 | 21 | engine save/sanitize/excision/page-space contracts (EC01, E-1/E-2, W2B-1, WP-R02/R09) |
| src/shell/controllers/EditController.cpp | 25 | 17 | OCR/compare/signature flows (R07/R08, U03/U04, N01, §9.7) |
| src/ui/PdfViewerWidget.cpp | 21 | 13 | viewer overlays, two-page, badge anchoring, status-bar seam |
| src/GpMainWindow.h / src/engines/PdfEditorEngine.{cpp,h} / IPdfEditorEngine.h | 19/18/18/15 | — | interface seams carried by the same engine-repair wave |
| src/modes/RedactMode.cpp | 13 | 9 | redaction entry/exit/overlay/sanitize contracts (N04/N07, §9.8-a/b/c) |
| src/shell/controllers/SecurityController.cpp | 13 | 10 | signing/policy/OCSP/redaction entry (R24, S4S, D01 residual wording) |
| tests/TestRedactMarkAll.cpp | 12 | 11 | redaction regression estate |
| src/ui/FindReplaceDialog.cpp | 11 | 7 | T2-2 replace pipeline |
| src/engines/SignatureManager.cpp | 10 | 7 | signing flow, candidate leak (FU-4), anchors law (EM-3 branch-only) |

By directory: src/engines 105 rows / 57 files; src/shell 64/39; src/core 62/40; src/ui 56/41;
src/modes 53/29; src/commands 19/12; tests/ 130+ rows across ~90 suites; packaging/.github/fuzz/
tools/scripts ~20 rows. Full per-file table: `.context/unrevmap-scratch/file_map.json`.

**What a reviewer still owes (bar per the ledger's own definition):** every awaiting row has
self-run tests + (mostly) negative controls, but NO independent review of the acceptance evidence.
Concretely owed per group: an R14-style independent probe + NC re-run for the September F/U/N/V/D/ARC/EC/G
rows; the W2B protocol (own-seam probe + NC at the fix's named base) for everything after 2026-09-20;
the W2B table's residuals that stand even post-verification (F2 embed branch scoped to probe+source,
W1-05 enforcement-posture, SignatureManager pre-existing rotation-imperfect read, R14ProbeRedactSpace
annotation-only attribution disclosure) are re-audit requests, not flips.

## 6. In-flight overlay — the six RUNNING lanes

| Branch | Branch-only commits vs 26c9a415 | Content pending the final merge batch | Status of its ledger rows |
|---|---|---|---|
| feat/sweep-w2-gsd | **20** (aggregates feat/emergence-fixes, feat/runintersects-precision, feat/sweep-w2c, feat/sweep-w3-ux — all contained in gsd, none in parity-glm) | **EM-1..EM-6** fixes (645f4994, 3325ea19, ee2cf661, cee2777c, 9463f6c0, c1552c26), **W2C verification** (4525f0e1; SL1/W2B-1 flips above), **ri-fix** runIntersects precision (87c4acbc) + its 2 new awaiting residual rows, GSD verifier probes (26f767f0, 242ee8f4), SWEEP-W3-EMERGENCE doc (48c2ae51) | gsd-side ledger rows: **8 awaiting** (EM-1..6, W2C precision residual, ri-fix) + 5 W2C-verified flips + 1 carried residual — ALL branch-only until this lane lands |
| feat/ux-defects-fixes | 6 | UX fixes **F2b-D1** (72069bd8 preset-store mkpath), **F4d-D1** (422389e4 signing-request in-place commits), **F6-F1** (1e741568 PubSec open-failure naming) + TestSweepW3UxFlows harness (c1daaae5) + SWEEP-W3 UX docs — no ledger rows yet (rows expected with the merge batch) | none in any ledger yet — flag as unrowed awaiting code |
| feat/redaction-gaps | 3 | REDACTION-RESEARCH-2026-09-21.md (544-line deep audit; Type3 under-excision vector INCONCLUSIVE-disclosed) + R14 composed-slot probe-scoping test fix (198e3bf2) | no new ledger rows; research findings feed future rows |
| feat/ui-narrow-viewport | 1 | docs-only: SWEEP-W3-UI-2026-09-20.md + R17-matrix acceptance evidence (164dabd4) | n/a (docs) |
| feat/soak-followups | 0 — branch created AT 26c9a415 | nothing committed; any in-flight work is uncommitted in its (busy, untouchable) worktree | none |
| feat/dispatch-gates | 0 — branch parked AT 26c9a415 | nothing committed (same situation) | none |

**Loud flag (the only branch-only work in the repo):** the feat/sweep-w2-gsd aggregate
(EM fixes + W2C verification evidence + ri-fix + 8 pending ledger rows) and the
feat/ux-defects-fixes / feat/redaction-gaps code. All four branches are expected to land via the
final merge batch — the consolidation MUST include them or this content is lost, because it exists
on no other ref. Nothing here is pushed; all four are local branches (verify with
`git branch --no-merged feat/parity-glm` at consolidation time).

## 7. Loss-risk statement (what the consolidation inherits)

1. Every awaiting row documented in the tip ledger (222) carries content that is an ancestor of
   26c9a415 — the integration line alone already contains 100% of the documented unreviewed work.
2. The only content NOT reachable from 26c9a415 sits on four local branches
   (feat/sweep-w2-gsd, feat/ux-defects-fixes, feat/redaction-gaps, feat/ui-narrow-viewport), all
   enumerated commit-by-commit in §6 — merge those four (gsd first, it aggregates three sub-lanes)
   and the map is complete with zero loss.
3. main's diverged 428 commits (incl. d03d6e9) are content-duplicated on the integration line where
   they overlap; the merge is still required for main-side-only work, but no awaiting row depends on
   main-only content.
4. Handoffs (.context/*-wip.md) are untracked/ignored — they live only inside busy worktrees and are
   NOT recoverable from git; this document intentionally carries everything the consolidation needs
   so it does not depend on any handoff file.

## 8. Residuals of this map

- Row → commit is 1:many (lanes committed per-defect; some rows share commits, e.g. 57cca6d ×3,
  7e32093 ×4); file attribution therefore over-counts rows per commit partner, never under-counts.
- The W2B/W2C verified verdicts rest on the verifying lanes' own committed probes; re-running them is
  part of review, not of this map.
- feat/soak-followups and feat/dispatch-gates may hold uncommitted work invisible to git from here;
  their lanes must self-commit before the final merge batch (standard lane-close procedure).
- Counts are a snapshot at 26c9a415 + the four branch tips as of 2026-09-23; any lane landing
  before consolidation shifts the awaiting count by its row count (expected: 222 → 219 + 8 = 227
  awaiting + unrowed UX fixes after the merge batch, before review begins).
