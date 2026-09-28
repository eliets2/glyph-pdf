# CONSOLIDATION LEDGER — 2026-09-25 (integrator, review/consolidated-parity)

Extends `CONSOLIDATION-LEDGER-2026-09-24.md` (which closed the legacy branch set at
**0 unexplained**). Scope here: every commit newer than that ledger, across all local
and remote refs. Method: `-x` trailer index + `git patch-id --stable` index over
`origin/main..HEAD`, per-branch enumeration, feature-presence probes in the HEAD tree
for re-applied diffs (patch-id cannot see conflict-resolved ports), and ancestry checks
against branches the 09-24 ledger closed.

Ledger written mid-session (parallel lane round in flight — see §4); the final handoff
re-states the lane outcomes.

## 1. Fixall-era branch dispositions

| Branch | New commits (post 09-24) | Disposition | Evidence |
|---|---|---|---|
| `feat/pr-review-fixes` | 10 | 8 **ACCOUNTED** via `-x`/patch-id (Phase 1 picks `262a7a09`→`1fcaa500`, `3c1e60a6`→`75e7507e`, `efbbf1eb`→`1919d0e6`, `a25c37b7`→`8d3f6d0f`, plus earlier `-x` picks); 2 **SUPERSEDED** | `a830d999`, `e89f1234` are the lane's own merge-artifact repairs — superseded by the Phase 0 recipe commit `fbb40295` (prompt §0.5: "Do not pick"). |
| `feat/fixall-forms` | 11 | **SUPERSEDED** | The lane's own build repair (4d410c10 + 10 reverts of PGR-20/26/22/06/zeroization/35/37/38+39/slot-scope/F2a-F1). Phase 0 `fbb40295` reverted the same 12 duplicates at the PR head; the reverts themselves are wrong for the PR (they would undo landed fixes). Prompt §0.5 + FIXALL-PROGRESS row: "do not pick". |
| `feat/fixall-forms2` | 3 | **PORTED/FOLDED** | `bd9b66bc`→`f6ef7d68` (CX-06), `7f2d4843`→`cabf7c15` (CX-05), `5cadce52`→`220b5f2b` (CX-03). All with `-x`; suites green on the PR head (TestSecretStore 23P/0F, TestFormSafety 14P/0F, TestConversionExtraction 21P/0F). |
| `feat/batch-presets-p2` | 6 | **ACCOUNTED** | 0 unaccounted by trailer/patch-id: U1–U7 + ledger rows were Phase-1-picked (`779fbb1e`→`ca7deeda` … `ec22eeed`→`32498fb8`). Worktree untracked files = build logs/test outputs only. |
| `fix/p0-blank-viewer` | 3 | **FOLDED** (addendum) | `4540e8fa`→`2ed100fc`, `ee73520f`→`f9e44c6e`, `86f2cf34`→`36b99c27`, all `-x`, R13-clean before picking; acceptance pins hit (TestViewingModes 10P/0F, TestSignatureBadges 25P/0F, TestThumbnailZoom 5P/0F, TestPagesMode 32P/0F, TestViewerRotation 4P/0F, TestOcrVerifyNavigation 16P/0F). |
| `feat/final-pgr-closers` | 1 | **PENDING-VERIFICATION** | `68bc917e` — PGR-23 nested-container sweep extension (+416 lines: recurse into attached PDF payloads with depth cap; refuse undecodable containers as Unswept; 2 new proof pins). Preserved from the pdf-keyA worktree (was uncommitted lane WIP). The redaction lane verifies it (build + full TestRedactionProof); folds on green, else stays with failure evidence. NOT yet counted as landed. |
| `test/view-parity-baseline` | 4 | **OUT OF SCOPE (owner)** | Belongs to a later session (owner instruction, 2026-09-28). Not touched, not dispositioned further. |
| `feat/fixall-ci` | 0 | **CONTAINED** | Tip `0aa5c2de` fully contained in HEAD (0 commits not on PR). CI lane work continues on `feat/fixall-ci2` (cut at PR head). |
| `feat/fixall-tagging` | 0 | **CONTAINED** | Tip `9d150be2` (CP1 progress doc) contained in HEAD. Tagging lane work continues on `feat/fixall-tagging2`. |
| `feat/sweep-w2-gsd` | 0 since 09-24 (~20 older commits flagged by patch-id) | **PRESENT** | Patch-id cannot see conflict-resolved ports, so feature-presence was verified directly at HEAD (2026-09-28): all 8 code fixes present — E-6 `stepDiskIdentityHex` (3 hits), E-5 `isManaged("signing/tsaUrl")` (3), E-4 `SignatureFieldAnchor boundField` (1), E-3 `PageSpace::pageGeometry(const_cast…)` (1), runIntersects `Band-OVERLAP` (1), E-2 `.traineddata")) return false` (1), E-1 `EditPolicy::readOnlyMessage` (21), F2b-D1 "store owns its root; the save boundary creates it" (1). Files: `tests/W2CProbeRotate270.cpp` and `docs/audit/SWEEP-W2-GSD-2026-09-21.md` **byte-identical** to the branch tip; `tests/TestSweepW3UxFlows.cpp` and `tests/TestGsdW2Probe.cpp` strictly HEAD-ahead (flow3b/7b/7c; W1-05 probe env gate). Branch is also an ancestor of `consolidate/all` and `feat/parity-glm`, both closed at 0 unexplained by the 09-24 ledger (rows 2 and 6). Remaining unaccounted-by-patch-id commits are that lane's docs/verdicts — same PRESENT verdict (docs present or HEAD-side superseded). |

## 2. Phase 1 + addendum + Phase 2-INSERT pick accounting (already on the PR)

The 22 Phase 1 §4 folds, the 3 addendum viewer picks, and the CX-06/CX-05/CX-03 ports
are logged per-item with pick SHAs, build+test gates, and evidence paths in
`docs/audit/FIXALL-PROGRESS-2026-09-25.md` (Phase 1 per-item table + checkpoint list).
Their `-x` trailers are the backbone of the accounting index (84 trailers / 122
patch-ids at ledger time). No source SHA appears in two `-x` trailers except the 12
Phase 0 revert sources — re-verified at each gate (E5).

## 3. Legacy branches

Everything not listed above is covered by `CONSOLIDATION-LEDGER-2026-09-24.md`
(B1/B1c/B2, verdict FOLDED/PRESENT/SUPERSEDED/ARCHIVE-ONLY, **0 unexplained**).
No legacy branch gained commits after that ledger except the ones tabled in §1.

## 4. Parallel lane round (in flight at ledger time)

Five fresh lane branches were cut at PR head `220b5f2b` per §9/R17:
`feat/fixall-images` (CX-02, 08, 09, 10, 11, 12, N1 — pdf-keyA),
`feat/fixall-tagging2` (CX-01, 07, 04 — pdf-sec),
`feat/fixall-ci2` (CX-13..17, PoDoFo pin, E4 wording — pdf-keyC),
`feat/fixall-redaction` (PGR-46 + the §1 PGR-23 verification — pdf-inst),
`feat/fixall-inv1` (INV-1 — pdf-r15).
Their commits are picked by the integrator under R13/R14 as lanes report; their
per-commit dispositions land in the final handoff (§8.3 of the fix-all prompt).
Launch constraint observed: the account's agent concurrency cap admits 2 lanes at a
time — Images/Tagging/Redaction are queued and dispatch as slots free.

## Unexplained commits: **0**

Every commit newer than the 09-24 ledger carries a disposition above (ACCOUNTED /
SUPERSEDED / FOLDED / PORTED / PRESENT / PENDING-VERIFICATION / OUT-OF-SCOPE-owner /
contained-by-ancestry). The single PENDING-VERIFICATION item (`68bc917e`) is tracked
to a named owner (redaction lane) and converts to FOLDED or stays recorded at the
gates.
