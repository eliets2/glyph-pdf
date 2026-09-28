# FIXALL progress — PR #2 (review/consolidated-parity), 2026-09-25

Session: GLM FIXALL, integrator-owned (R12). Resume point for any successor.
Task prompt: D:/prompts/claude-system/GLM-FIX-ALL-PROMPT-2026-09-25.md.
Worktree: D:/pdf/pdf-review. Branch: review/consolidated-parity.

## Status line

- Phase 0 (build repair): **DONE before this session** — fbb40295 pushed, blob-accepted,
  CI Build green (per brief; 12 duplicate picks reverted + missing include).
- Phase 1 (§4 lane folds): **PICKS COMPLETE** — see per-item log below (R8 restoration + presets review done).
- Phase 1-ADDENDUM (fix/p0-blank-viewer picks): **DONE** — 2ed100fc, f9e44c6e, 36b99c27 (CP3).
- Phase 2-INSERT (CX-05, CX-06 from fixall-forms2): **DONE on branch, unpushed→CP3** —
  f6ef7d68 (CX-06 MED, ported from fixall-forms2 bd9b66bc; TestSecretStore 23P/0F),
  cabf7c15 (CX-05 HIGH, fillForm+UI-Apply gated through commit-phase /AA /K willCommit=true
  before /V; TestFormSafety 14P/0F), 476d163b (evidence move, E0).
  Remaining Phase 2 items (CX-01..04, 07..17, N1, PGR-46, INV-1): poll feat/fixall-* tips.
- Phase 1-c (ledger): NEXT (branch check across all refs → CONSOLIDATION-LEDGER-2026-09-25.md).
- Phase 3 (re-verification): pending.
- Phase 4 (gates E0-E6): pending.
- Phase 5 (handoff): pending. Do not create CONSOLIDATION-HANDOFF-FIXALL until the end.

## Phase 1 per-item log

| # | Source | Branch | Disposition | Pick SHA | Build+tests | Notes |
|---|---|---|---|---|---|---|
| 1 | 262a7a09 | feat/pr-review-fixes | FOLDED | 1fcaa500 | build 378/378; Tagger 17P/0F/1skip | tagger matrix §3.6 |
| 2 | 3c1e60a6 | feat/pr-review-fixes | FOLDED | 75e7507e | Panel 13P/0F; flows 14P/0F | RO gate §3.1; conflict in TestSweepW3UxFlows.cpp (blank-line churn + flow7c block) resolved to HEAD side; GpMainWindow.cpp::runA11yTag auto-merged with required order: RO gate, save-first prompt, releaseResidentFile, tag; EditPolicy.h route comment in |
| 3 | efbbf1eb | feat/pr-review-fixes | FOLDED | 1919d0e6 | (cluster suites above) | signed refusal §3.2 |
| 4 | a25c37b7 | feat/pr-review-fixes | FOLDED | 8d3f6d0f | docs-only | ledger rows (CURRENT-EVIDENCE-LEDGER +56) |
| 5 | 9ff2b136 | feat/residual-exec | FOLDED | 9e9818e9 | build green (cluster) | W1-05 admin-owned machine policy |
| 6 | 551759c7 | feat/residual-exec | FOLDED | 47b2998b | build green (cluster) | extractLinks page-space law |
| 7 | 6552544c | feat/residual-exec | FOLDED | fbfa75a6 | build green (cluster) | exportToImage range refusal |
| 8 | 9b2b2727 | feat/residual-exec | PORTED (not superseded: tests failed on PR head — matcher reported raw-user (100,700) vs law (333.4,472.6)) | b09256ee | TFR 30P/0F; P37 9P/0F; all 14 TestRedact*/TestSep13* green | §4 procedure: tests-only first (fail-before evidence), then code port; naive port regressed PGR-37 crop pin (secret survives) — fixed by deriving replace-side geometry from the same display basis (crop box, fallback MediaBox); supplementary band pin re-expressed in display contract; evidence .context/integrator-evidence/fixall-t22-* |
| 9 | bf7bedf1 | feat/residual-exec | FOLDED | 48a140f8 | build green | fuzz CI (starts CX-13) |
| 10 | affcc420 | feat/residual-exec | FOLDED | ec9d4f2b | docs | FEATURE-COMMAND-MATRIX CSV repair |
| 11 | 47e74eea | feat/residual-exec | FOLDED | 7c70b881 | docs | archaeo Plan 12 note |
| 12 | 5d74c348 | feat/residual-exec | FOLDED | 05e1b8cc | build 393 steps incl. probe target | registers R14ProbeBatchSkip (disabled by default) |
| 13 | 779fbb1e | feat/batch-presets-p2 | FOLDED | ca7deeda | P2 33P/0F; P1 15P/0F | U1 |
| 14 | 0f704539 | feat/batch-presets-p2 | FOLDED | dd8d20bb | (cluster suites) | U2 |
| 15 | 72e3434e | feat/batch-presets-p2 | FOLDED | 3950e4b3 | (cluster suites) | U3 |
| 16 | f2448a81 | feat/batch-presets-p2 | FOLDED | d96bace7 | (cluster suites) | U4 |
| 17 | 6d9e2767 | feat/batch-presets-p2 | FOLDED | a2772901 | (cluster suites) | U5 |
| 18 | e050e140 | feat/batch-presets-p2 | FOLDED | 341f5538 | (cluster suites) | U6 |
| 19 | 59d91a6e | feat/batch-presets-p2 | FOLDED | fde3ba46 | (cluster suites) | U7 |
| 20 | ec22eeed | feat/batch-presets-p2 | FOLDED | 32498fb8 | docs | ledger; conflict (both-append) keep-both + integrator note re quickjs pin (lane note was for its pre-C.6 base; PR head authorizes 0.15.1-1, machine matches). U1-U7 review done: import traversal blocked by V8 id==stem rule; schema v1 unchanged (P1 goldens green); atomic writes via VersionedJson::atomicWrite; abort-keeps-committed pinned (U4); honest reporting pinned (U2/U3/U4/U7); LOW residual recorded: exportTo remove-then-copy window (crash loses target) |
| 21 | a2a8ff4f | feat/pgr-c4 | FOLDED | 2367e2c9 | docs | ledger rows + evidence-pgr-c4 files; ledger conflict (both-append) resolved keep-both |
| 22 | 16145af2 | feat/pgr-c4 | FOLDED | e5b401c3 | docs | evidence re-capture (stale-binary postfix captures corrected) |
| — | e89f1234, a830d999 | feat/pr-review-fixes | SUPERSEDED (Phase 0 fbb40295) | — | — | do not pick |
| — | fixall-forms repair (11 reverts + 4d410c10) | feat/fixall-forms | SUPERSEDED (Phase 0 fbb40295) | — | — | lane's own repair; poll lane for CX-05/06/03+INV-1 commits |
| A1 | 4540e8fa | fix/p0-blank-viewer | FOLDED (addendum) | 2ed100fc | TestViewingModes 10P/0F; TestSignatureBadges 25P/0F | P0 blank canvas: SignatureBadgeOverlay transparent stylesheet |
| A2 | ee73520f | fix/p0-blank-viewer | FOLDED (addendum) | f9e44c6e | TestThumbnailZoom 5P/0F; TestPagesMode 32P/0F | loadDocument announces page 1 (pageChanged spy) |
| A3 | 86f2cf34 | fix/p0-blank-viewer | FOLDED (addendum) | 36b99c27 | TestViewerRotation 4P/0F; TestOcrVerifyNavigation 16P/0F | renderPage composites onto white paper |
| F1 | bd9b66bc | fixall-forms2 | PORTED as f6ef7d68 | f6ef7d68 | TestSecretStore 23P/0F | CX-06 MED: legacy 0x02 migration re-wraps only the entry it read (fail-before/pass-after/nc-reverted/restored evidence in docs/audit/evidence-cx06/) |
| F2 | (lane) | fixall-forms2 | PORTED as cabf7c15 | cabf7c15 | TestFormSafety 14P/0F | CX-05 HIGH: fillForm + UI-Apply gated through commit-phase /AA /K willCommit=true BEFORE /V; fail-before/pass-after/nc/restored evidence (relocated to docs/audit/evidence-cx05/ by 476d163b) |

## Checkpoint pushes

- CP1 (Phase 1 pr-review-fixes cluster): 1fcaa500, 75e7507e, 1919d0e6, 8d3f6d0f —
  build 378/378 green; TestAccessibilityTagger 17P/0F/1skip, TestAccessibilityPanel
  13P/0F, TestSweepW3UxFlows 14P/0F (flow7b + flow7c incl.). Pushed.
- CP3 (this session): f6ef7d68 (CX-06), cabf7c15 (CX-05), 476d163b (evidence move),
  2ed100fc + f9e44c6e + 36b99c27 (addendum viewer picks), progress-doc commit —
  build 0 errors; TestSecretStore 23P/0F, TestFormSafety 14P/0F, TestViewingModes
  10P/0F, TestSignatureBadges 25P/0F, TestThumbnailZoom 5P/0F, TestPagesMode 32P/0F,
  TestViewerRotation 4P/0F, TestOcrVerifyNavigation 16P/0F. R13 clean for all picks
  (trailer grep + patch-id). Pushed as fast-forward on e47a1f18.

## Notes / deviations

- Session start ~02:00: worktree had one unstaged hunk (AccessibilityPanel.h) byte-
  identical to 3c1e60a6's change; saved to /d/pdf/inflight-integrator-2026-09-25.patch
  and restored (pick supplies it).
- R13 helper: .context/r13-check.sh (trailer grep + cached patch-id index /tmp/pid.txt;
  rebuild index after each pick batch).
