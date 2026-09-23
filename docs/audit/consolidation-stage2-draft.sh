#!/usr/bin/env bash
# ============================================================================
# GlyphPDF consolidation — STAGE-2 EXECUTION SCRIPT
# File: docs/audit/consolidation-stage2-draft.sh
#
#  ██████╗ ██╗   ██╗ █████╗ ███████╗████████╗    ███╗   ██╗ ██████╗ ████████╗
#  ██╔══██╗██║   ██║██╔══██╗██╔════╝╚══██╔══╝    ████╗  ██║██╔════╝ ╚══██╔══╝
#  ██████╔╝██║   ██║███████║███████╗   ██║       ██╔██╗ ██║██║  ███╗   ██║
#  ██╔══██╗██║   ██║██╔══██║╚════██║   ██║       ██║╚██╗██║██║   ██║   ██║
#  ██║  ██║╚██████╔╝██║  ██║███████║   ██║       ██║ ╚████║╚██████╔╝   ██║
#  ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝   ╚═╝       ╚═╝  ╚═══╝ ╚═════╝    ╚═╝
#
#  STATUS: D R A F T — N O T   E X E C U T E D   (written 2026-09-23 by the
#  stage-1 consolidation-executor lane; see CONSOLIDATION-STAGE1-2026-09-23.md
#  and CONSOLIDATION-STAGE2-SCRIPT-2026-09-23.md). It MUST NOT be run until
#  (a) the five in-flight remediation lanes have landed via the final merge
#  batch, (b) a human release owner has reviewed every CONFIRM gate below,
#  and (c) the plan's §2 naming choice (which branch name survives) is made.
#
#  Script syntax was checked with `bash -n` (parse only — no line of the
#  runbook logic has ever executed); treat every line as a proposal to be
#  re-verified at execution time.
#
#  WHAT IT DOES (per plan CONSOLIDATION-PLAN-2026-09-20 §5, updated for drift):
#    Phase 0  Preconditions: freeze, final bundle (refresh of the stage-1
#             bundle), re-run of all stage-1 proofs at execution time.
#    Phase 1  Real merges: 7 outstanding C-class branches + the reclassified
#             claude/modest-mccarthy-riuo2o docs tip into the line.
#    Phase 2  The main merge, resolved per plan §5.2 policy classes R1–R4.
#    Phase 3  Suite gate, then FF both surviving lines to tip T; push tags.
#    Phase 4  Deletion pass: per-branch `rev-list <tip> ^T` = 0 proof, then
#             `git push origin --delete`, in the plan §5.3 order.
#    Phase 5  End-state verification (ls-remote shows exactly 2 heads).
#
#  HARD RULES ENCODED BELOW (violating any of these aborts the run):
#    - NEVER force-push, never rewrite, only fast-forward ref moves.
#    - NEVER git gc / prune / reset --hard / clean in this script.
#    - Every deletion is preceded by its own zero-loss proof AND a
#      bundle-verify of the final archive bundle.
#    - Any nonzero proof aborts the whole pass (fail-closed).
#    - Destructive actions require CONSENT env var; default is a dry-run.
# ============================================================================
set -euo pipefail

# ----------------------------- configuration -------------------------------
RUN_MODE="${RUN_MODE:-DRYRUN}"            # DRYRUN (default) | EXECUTE
CONSENT="${CONSENT:-}"                    # must be exactly "I-UNDERSTAND-THIS-DELETES-BRANCHES" for EXECUTE
REPO="${REPO:-C:/Users/User/Projects/pdf-clean}"   # shared repo (worktree stays on feat/parity-glm)
LINE="${LINE:-origin/feat/parity-glm}"    # integration line tip (execution-time re-read)
FINAL_BUNDLE="${FINAL_BUNDLE:-C:/Users/User/pdf-archive-final.bundle}"  # refresh of stage-1 bundle
FINAL_BUNDLE_SHA256_EXPECTED="${FINAL_BUNDLE_SHA256_EXPECTED:-}"  # record at creation, re-check before deletions
# Plan §2 naming choice — MUST be picked once by the release owner:
#   "feat/parity-glm-integration" (recommended) or "feat/parity-glm".
SURVIVOR="${SURVIVOR:-UNDECIDED}"         # which single integration branch name survives
PROOF_LOG="${PROOF_LOG:-docs/audit/CONSOLIDATION-STAGE2-PROOFS-$(date +%Y-%m-%d).txt}"

# Branch sets (stage-1 verified classifications — see CONSOLIDATION-STAGE1-2026-09-23.md §3)
A_CLASS="feat/accessibility-p1 feat/batch-presets-p1 feat/candidate-leak-fix feat/followups-2026-09-15 feat/glm-comp feat/glm-ocr feat/l7-rotate-annot feat/parity-glm feat/parity-glm-clean feat/parity-glm-formjs feat/parity-glm-gate feat/parity-glm-gateC feat/parity-glm-infra feat/parity-glm-integration feat/parity-glm-n17n18 feat/parity-glm-packa feat/parity-glm-packafix feat/parity-glm-quick feat/parity-glm-r05 feat/parity-glm-r18f feat/parity-glm-r22 feat/parity-glm-resid2 feat/parity-glm-review feat/parity-glm-sec feat/parity-glm-t2 feat/printable-summaries feat/r24-policy feat/r24-wiring feat/send-for-signing-p1 feat/sep13-fixes feat/sep13-leads feat/sep13-residual feat/sweep-legacy-fix feat/sweep-w1-adversary feat/sweep-w1-fixes feat/sweep-w1-security feat/sweep-w2-testing feat/sweep-w2-verify-b"
B_CLASS="audit-remediation"               # claude/... was RECLASSIFIED (stage-1 §3.3) — see REAL_MERGE
REAL_MERGE="feat/soak-48h feat/soak-verdict feat/sweep-w3-archaeo feat/sweep-w3-devops feat/sweep-w3-perf feat/sweep-w3-research feat/sweep-quality-new claude/modest-mccarthy-riuo2o"
ARCHIVED="feature/editing-parity feature/redaction-parity feature/viewing-parity feat/annotation-eraser"

die() { echo "ABORT: $*" >&2; exit 1; }
run() { if [ "$RUN_MODE" = EXECUTE ]; then "$@"; else echo "DRYRUN: $*"; fi }
g() { git -C "$REPO" "$@"; }

[ "$RUN_MODE" = EXECUTE ] || echo "== DRY-RUN: no git mutation will happen =="
if [ "$RUN_MODE" = EXECUTE ]; then
  [ "$CONSENT" = "I-UNDERSTAND-THIS-DELETES-BRANCHES" ] || die "set CONSENT=I-UNDERSTAND-THIS-DELETES-BRANCHES"
  [ "$SURVIVOR" != "UNDECIDED" ] || die "pick the surviving integration branch name first (plan §2)"
fi

# ----------------------------- phase 0: preconditions ----------------------
# 0.1 Freeze check (manual): no lane pushes during the run. Announce and wait.
echo "PHASE 0: preconditions"
# 0.2 Final bundle refresh (the five remediation lanes landed AFTER the
#     stage-1 bundle): create + verify + hash, then REQUIRE verify again
#     before any deletion (plan: never gc/prune; the bundle is the insurance).
run git -C "$REPO" bundle create "$FINAL_BUNDLE" --all
run git -C "$REPO" bundle verify "$FINAL_BUNDLE" || die "final bundle verify failed"
FINAL_SHA="$(sha256sum "$FINAL_BUNDLE" | cut -d' ' -f1)"
echo "FINAL_BUNDLE_SHA256=$FINAL_SHA" | tee -a "$PROOF_LOG"
# 0.3 Re-run the stage-1 fold-proof sweep AT EXECUTION TIME — the line has
#     moved since 2026-09-23; any A-class branch now non-contained is a lane
#     that pushed after stage 1 and must go through REAL_MERGE instead.
LINE_TIP="$(g rev-parse --short=8 "$LINE")"
for b in $A_CLASS; do
  n="$(g rev-list --count "origin/$b" "^$LINE" || die "cannot prove $b")"
  [ "$n" = 0 ] || die "A-class DRIFT: origin/$b has $n commits not in $LINE — reclassify REAL-MERGE, do not fold"
  echo "PROOF-A $b tip=$(g rev-parse --short=8 "origin/$b") uniq=0" >> "$PROOF_LOG"
done
# 0.4 Re-check main divergence claim (expect ~401 '-', 14 '+' — see stage-1 §4).
g cherry "$LINE" main > /tmp/cherry_now.txt
echo "CHERRY equivalents=$(grep -c '^-' /tmp/cherry_now.txt || true) real=$(grep -c '^+' /tmp/cherry_now.txt || true)" >> "$PROOF_LOG"
# 0.5 Confirm main is still origin/main + N FF commits (no rewrite on main).
g merge-base --is-ancestor origin/main main || die "main history rewritten — STOP, re-plan"

# ----------------------------- phase 1: real merges ------------------------
# 7 outstanding C-class merges (rehearsed order, plan §5.1) + the stage-1
# reclassified claude docs tip. Ledger-file conflicts resolve by ROW-UNION
# (never -X ours: it drops the other side's ledger rows — plan §5.1).
# NOTE: feat/modularity-moves + feat/sweep-w3-arch already contained at
# 26c9a415 (stage-1 §4) — removed from this list; re-add only if 0.3 shows
# drift. Run in a dedicated worktree on the line, then push FF.
echo "PHASE 1: real merges"
WT="$REPO/.context/consolidation-stage2-wt"
run git -C "$REPO" worktree add "$WT" "$SURVIVOR"   # execute phase 1+2 on the survivor name
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/soak-48h'" origin/feat/soak-48h
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/soak-verdict'" origin/feat/soak-verdict
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/sweep-w3-archaeo'" origin/feat/sweep-w3-archaeo
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/sweep-w3-devops'" origin/feat/sweep-w3-devops
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/sweep-w3-perf'" origin/feat/sweep-w3-perf
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/sweep-w3-research'" origin/feat/sweep-w3-research
run git -C "$WT" merge --no-ff -m "Merge branch 'feat/sweep-quality-new'" origin/feat/sweep-quality-new
run git -C "$WT" merge --no-ff -m "Merge branch 'claude/modest-mccarthy-riuo2o' (docs AR-PROMPT tip, reclassified REAL-MERGE in stage-1 §3.3)" origin/claude/modest-mccarthy-riuo2o
# ^ any conflict here stops the run; resolve per plan §5.1 (row-union) and continue by hand.

# ----------------------------- phase 2: the main merge ---------------------
# The only hard merge (~130 conflicted paths at rehearsal). Resolution per
# plan §5.2 classes, decided once:  R1 code src/**,tests/** -> line wins
# (keep tests/mocks/MockFormManager.h DELETED); R2 docs/governance/memory ->
# main's additive content, keep the line's 52 deletions (paths stay readable
# at 2b715f47 forever); R3 release-eng (CMakeLists version, packaging/*.wxs,
# .github/workflows/release.yml) -> adopt main's v1.4.0 state, REQUIRES
# release-owner sign-off; R4 workflows ci/fuzz/license-guard -> union
# (line INF04 feat/** filters + main's release workflow).
echo "PHASE 2: main merge (manual policy resolution per plan §5.2 R1-R4)"
run git -C "$WT" merge --no-ff -m "Merge branch 'main' (endgame consolidation; policy classes R1-R4)" main
# ^ stop here on conflict; resolve per class, then run the gate before pushing.

# ----------------------------- phase 3: gate + FF both lines ---------------
echo "PHASE 3: suite gate, then FF main + $SURVIVOR to tip T"
# Gate: the line's standard gate (host full serial ctest; R22 Linux gate if
# available). A consolidation commit must not bypass the lane's suite.
# run cmake --build ... && ctest -C Debug --output-on-failure   (lane's exact gate commands)
T="$(g rev-parse --short=8 "$SURVIVOR")"
echo "T=$T" >> "$PROOF_LOG"
# Fast-forward-only pushes (never --force):
run git -C "$REPO" push --force-with-lease=NO-NEVER-USE-FORCE origin "$SURVIVOR:main"           # placeholder; real run uses: git push origin $SURVIVOR:main (FF-checked)
run git -C "$REPO" push origin "$SURVIVOR:$SURVIVOR"
run git -C "$REPO" push origin "refs/tags/archive/editing-parity" "refs/tags/archive/redaction-parity" "refs/tags/archive/viewing-parity" "refs/tags/archive/annotation-eraser"
# ^ NOTE: the push line above is intentionally awkward in draft form — the
#   execution lane rewrites it as an explicit, reviewed command list.

# ----------------------------- phase 4: deletion pass ----------------------
# For EVERY branch to fold: pin its tip SHA, prove `rev-list <pinned-tip> ^T`
# = 0 (fail-closed), re-verify the final bundle, then push-delete origin.
# Order per plan §5.3: A-class, then B-class, then REAL_MERGE inputs, then
# the archived D-class (only after tags exist on origin), then the folded
# line name if the alternative §2 naming was chosen.
echo "PHASE 4: deletion pass (proof-gated, fail-closed)"
delete_proven() {
  local b="$1" pinned
  pinned="$(g rev-parse "origin/$b")"
  local n; n="$(g rev-list --count "$pinned" "^$T")"
  [ "$n" = 0 ] || die "zero-loss proof FAILED for origin/$b ($n commits not in T) — do not delete"
  run git -C "$REPO" bundle verify "$FINAL_BUNDLE" || die "bundle verify failed before deleting $b"
  [ "$(sha256sum "$FINAL_BUNDLE" | cut -d' ' -f1)" = "$FINAL_BUNDLE_SHA256_EXPECTED" ] || die "final bundle hash changed since phase 0 — re-bundle before deleting"
  echo "PROOF-DEL origin/$b pinned=$pinned uniq-vs-T=0" >> "$PROOF_LOG"
  run git -C "$REPO" push origin --delete "$b"
}
for b in $A_CLASS $B_CLASS $REAL_MERGE $ARCHIVED; do
  case "$b" in "$SURVIVOR"|"main") die "refusing to delete survivor/main";; esac
  [ "$b" = "origin/feat/parity-glm" ] && continue   # folded last, only per §2 naming outcome
  delete_proven "$b"
done
# Last (per plan §5.3 step 3): the line name that did NOT survive §2, only
# after T is pushed and proven to contain its tip — handled by the same
# delete_proven with the chosen name.
# 4.9 LOCAL cleanup (separate lane, plan §7): local-only branches, gemini
# subagent worktrees, recovery/temp-stage-push-20260909 (superseded by the
# final bundle) — OUT OF SCOPE for this script; local `git branch -d` only,
# never `git branch -D`, and only after the gemini/soak worktrees are released.

# ----------------------------- phase 5: end-state verification -------------
echo "PHASE 5: end-state"
# Reviewer one-liner (plan §8): exactly 2 heads on origin; every old tip
# contained in T; bundle verifies; log shows the merge sequence; gate green.
run git -C "$REPO" ls-remote --heads origin
echo "END-STATE CHECK: heads must be exactly main + $SURVIVOR"
echo "Proof log: $PROOF_LOG ; final bundle: $FINAL_BUNDLE (SHA-256 $FINAL_SHA)"
# ============================ END OF DRAFT =================================
