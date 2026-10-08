# STAGE-2 CONSOLIDATION EXEC — FINAL STATE — 2026-09-30, head 061fea2d (pushed)

STATUS: COMPLETE. The consolidated branch `review/consolidated-parity` is PUSHED
to origin @ 061fea2d — a pure fast-forward candidate over origin/main fae7c9b3:
main + 5 picks (ui-redesign x3, xmp 1.7b images-route, xmp 1.7a staged-seed pin)
+ the completed execution record. Code tree byte-identical to the gated tree.

## Result

- All 9 post-consolidation local branches dispositioned (see the in-tree
  docs/audit/CONSOLIDATION-STAGE2-EXECUTED-2026-09-25.md — authoritative):
  k-fixes contained; ole/prodfix/ci-inv folded patch-eq in main;
  secfix 1.2/1.6 SUPERSEDED (equivalents in main) with its orphaned 1.6 pin
  rescued and folded; ui-redesign + xmp partially PICKED (6 picks, rebased
  chain: db5e1f3f dd40faed 9f1fef4d a753dcc3 f97a43a3); audit/sweep-all was
  LIVE at stage start (HOLD) — the parallel integrator has since folded its
  Wave-1 (docs-only) into main.
- FOUND + FIXED: origin/main tip 7eb5c67b did not COMPILE (mangled 1.7c test
  union). Repaired 22b4dea7/85abab41/7e73cc1c (on main via the parallel
  integrator's salvage of this lane's commits; identical content on this
  branch via the rebase base).
- Gates at the final code: fresh Release rebuild 1065/1065 targets green
  (cache reconfigured after the integrator's cleanup swept CMakeCache.txt);
  9 touched suites green; THREE full serial offscreen ctest runs:
  serial2 188/188 = 100% (pre-rebase code), serial3 188/188 = 100% (final
  code), serial1 186/188 with both reds proven load-flakes (solo x3 green).
- E3: 0 merge commits over main; purge intact; FF-candidate over origin/main.
- Safety net: 9 stage-freeze annotated tags created + pushed (archive tag
  parity 263=263); stage bundle pdf-archive-final.bundle SHA-256
  dc4db675db744919f3cbb8b66a9f1d345bdda6951030797b57060dda673d99e7 (498 refs,
  restore-drilled); post-stage refresh bundle below.
- The parallel integrator (another lane) salvaged this lane's 3 unique commits
  + the pre-gate doc draft onto main mid-flight; this branch was rebased twice
  (73830ee4, then fae7c9b3) and the completed doc supersedes the salvaged draft.
- ZERO deletions (local or origin), no force, no gc/prune, no stash; main was
  never pushed by this lane; the D:/pdf/pdf worktree never touched.

## Hazards recorded (owner/later passes)

- The actor that deregistered pdf-secfix/prodfix worktrees ALSO deleted their
  working-tree dotfiles (.gitignore/.gitattributes/.github/*), .context/
  (incl. the preserved secfix pin patch — content safe: the pin is committed),
  and build-sec/CMakeCache.txt. Worktree metadata was manually rebuilt
  (gitdir/commondir ../..) and dotfiles restored from HEAD.
- build-sec binaries on disk correspond to the final code (rebuilt this run).
- Office route still carries the Cancel/canceled-wiring shape removed from the
  images route (owner item; do not fix blind).
- Post-stage refresh bundle: D:/pdf/pdf-archive-final-2026-09-30.bundle,
  verify complete-history, 41,513,859 bytes, SHA-256
  4766c3c3dc00aed3d00a3b95ce4e5f7b73abe345d697c05681fe4ebd74981942 (covers the
  pushed head 061fea2d, main fae7c9b3, every tag).
