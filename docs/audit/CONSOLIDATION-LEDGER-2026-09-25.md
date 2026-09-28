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

## 5. Gates-era delta (2026-09-29, Phases 3-5) — closes §4's lane round

| Item | Disposition | Evidence |
|---|---|---|
| `68bc917e` (was PENDING-VERIFICATION) | **SUPERSEDED** (code) + **PORTED** (tests only) | Parallel implementation of PGR-23; PR's `6841247d` already landed the same fix with a superset of its container refusals (ZIP/OOXML/GZIP/7z/RAR/XZ/BZIP2 + unparseable-PDF; nested-PDF recursion with decoded streams + PDFium text extraction, depth 3) and fail-before/NC/pass-after evidence. Only the WIP's 2 test pins were ported — `06ce3ae4` (tests-only, both pass with NO WIP code: TestRedactionProof 35P/0F ×3 on the PR head). The WIP's one non-subset content item, the OLE compound signature `\xD0\xCF\x11\xE0`, is recorded as a residual (not ported without its own fail-before evidence — R7). |
| `feat/final-pgr-closers` 4 fix commits | **PRESENT** (byte-identical patch-ids, already on the PR) | `5e81a0fa`≡`6600a429` (PGR-37), `629b5749`≡`f90b4c71` (PGR-35), `4aa8cb81`≡`1d59f241` (PGR-36), `9aa576bf`≡`ddef6a8f` (PGR-38). Re-picking them would violate R13. |
| `feat/fixall-redaction` `a76b728d` (PGR-46 CRITICAL-class) | **FOLDED** → `99dd7b67` (`-x`, R13-clean) | Fail-before TestRedactMarkAll 18P/3F, pass-after 21P/0F ×3 on the PR head (plus TestPatternRedact 15P, TestPgr37PageSpaceLaw 9P, redaction cluster green ×3). Evidence: `docs/audit/evidence-pgr46/`. |
| `feat/fixall-redaction` `efd3f8ae` (PGR-23 pins port) | **FOLDED** → `06ce3ae4` (`-x`, R13-clean) | See `68bc917e` row; TestRedactionProof 35P/0F ×3. |
| N1 (occurrence-index addressing), finished on `feat/fixall-images` | **FOLDED** → `2ead0b17` (`-x`, R13-clean) | Source `d63ed76e` ("placements are addressed by occurrence index, not name"). Fold race with a concurrent lane pick resolved by squashing the two partial applications into ONE commit whose union diff was first proven byte-identical to the source diff (content applied exactly once; 23 files, 704+/158−). Fail-before TestImageAppearance 41P/4F, pass-after 47P/0F (evidence `docs/audit/evidence-n1/`); 3× green on the PR head recorded in the Phase 3 log. Process deviation recorded: lane agents cherry-picked into the integrator worktree/branch concurrently (see handoff §Deviations). |
| Lane-side docs checkpoints (`590c6c27` on `feat/fixall-ci2`, siblings on other lane branches) | **LANE-SIDE CHECKPOINTS** | Content carried by the integrator's progress-doc commits on the PR (CP5-CP8). Not picked (docs-only, lane-local). |
| `72069bd8` (consolidate/all et al., 2026-09-21) | **Pre-cutoff** | Predates the 09-24 ledger (its subject matter); that ledger's branch verdicts (0 unexplained) cover the consolidated-era universe. Not re-dispositioned. |

E5 re-verified at the gates head: no source SHA appears in two `-x` trailers in
`origin/main..HEAD` except exactly the 12 Phase 0 revert sources.

Unexplained commits (fixall era, all refs): **0**.
