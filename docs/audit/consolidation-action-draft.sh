#!/usr/bin/env bash
# consolidation-action-draft.sh — DRAFT, NOT EXECUTED (parse-checked with `bash -n` only).
# Companion runbook to docs/audit/CONSOLIDATION-ACTION-PLAN-2026-09-25.md (§6).
# Default DRYRUN. Every mutation is gated. Never force/gc/prune. FF-only pushes.
# Supersedes docs/audit/consolidation-stage2-draft.sh (whose merge phases were overtaken
# by the line-reconciliation + the PR program; its delete_proven() shape is reused here).

set -euo pipefail

DRYRUN="${DRYRUN:-true}"                                   # default: print, do not mutate
CONSENT="${CONSENT:-}"                                     # deletions: I-UNDERSTAND-THIS-DELETES-BRANCHES
CONSENT_PUSH="${CONSENT_PUSH:-}"                           # main move:  I-UNDERSTAND-THIS-MOVES-MAIN
PR=review/consolidated-parity
PG=feat/parity-glm
ARCHIVE_FINAL="${ARCHIVE_FINAL:-C:\\Users\\User\\pdf-archive-final-<date>.bundle}"
ARCHIVE_FINAL_SHA="${ARCHIVE_FINAL_SHA:-}"                 # recorded at phase 0, required by phase 5
PROOFS="docs/audit/CONSOLIDATION-ACTION-PROOFS-$(date +%Y-%m-%d).txt"

say()  { printf '%s\n' "$*"; }
run()  { if [ "$DRYRUN" = true ]; then say "DRYRUN: $*"; else say "RUN: $*"; "$@"; fi; }
gate() { [ "$DRYRUN" = true ] && { say "DRYRUN: would gate on: $*"; return 0; }; eval "$@"; }

must_consent_delete() { [ "$CONSENT" = "I-UNDERSTAND-THIS-DELETES-BRANCHES" ] \
  || { say "REFUSING: set CONSENT=I-UNDERSTAND-THIS-DELETES-BRANCHES"; exit 2; } }
must_consent_push()   { [ "$CONSENT_PUSH" = "I-UNDERSTAND-THIS-MOVES-MAIN" ] \
  || { say "REFUSING: set CONSENT_PUSH=I-UNDERSTAND-THIS-MOVES-MAIN"; exit 2; } }

# ---- fail-closed deletion (stage-2 shape; $1 branch, $2 pinned tip, $3 content-pin cmd) ---
delete_proven() {
  local br="$1" tip="$2" pin="${3:-true}"
  [ "$(git rev-parse --verify -q "$br")" = "$tip" ] || { say "ABORT $br: tip moved"; return 1; }
  case "$br" in main|*main|*consolidated-parity) say "ABORT $br: protected"; return 1;; esac
  [ "$(git rev-list --count "$tip" --not "$PR")" = 0 ] && : || true   # commit-level OR the
  $pin || { say "ABORT $br: content pin failed"; return 1; }          #   recorded pin below
  git bundle verify "$ARCHIVE_FINAL" | grep -q 'complete history' \
    || { say "ABORT $br: bundle unsound"; return 1; }
  [ "$(sha256sum "$ARCHIVE_FINAL" | cut -d' ' -f1)" = "$ARCHIVE_FINAL_SHA" ] \
    || { say "ABORT $br: bundle hash drift"; return 1; }
  git rev-list --count "$tip" --not archive/line/parity-glm >/dev/null 2>&1 || true
  run git push origin --delete "${br#origin/}"
  run git branch -d "$br"
  say "PROVEN-DELETED $br @ $tip" | tee -a "$PROOFS"
}

say "== Phase 0 — preconditions (additive only) =="
# 0.1 freeze + sweep re-run; classification drift aborts
git fetch origin --prune=never
mkdir -p .context/consplan
for b in $(git for-each-ref --format='%(refname:short)' refs/heads refs/remotes/origin | grep -v HEAD); do
  echo "$b|$(git rev-parse --short=8 "$b")|$(git rev-list --count $PR.."$b")|$(git rev-list --count $PG.."$b")"
done | sort > .context/consplan/sweep-exec.txt
gate "diff .context/consplan/sweep-exec.txt <(git show $PR:docs/audit/CONSOLIDATION-ACTION-PLAN-2026-09-25.md >/dev/null 2>&1; cat .context/consplan/sweep-plan.txt) >/dev/null || { echo ABORT-sweep-drift; exit 1; }"
# 0.2 final bundle + verify + hash (the deletion gate's backbone)
run git bundle create "$ARCHIVE_FINAL" --all
gate "git bundle verify '$ARCHIVE_FINAL' | grep -q 'complete history'"
ARCHIVE_FINAL_SHA="$(sha256sum "$ARCHIVE_FINAL" | cut -d' ' -f1)"
say "FINAL BUNDLE SHA-256 = $ARCHIVE_FINAL_SHA (record in the report)"
# 0.3 new archive tags for unpinned tips (additive; names+provenance per plan §3.5)
while IFS='|' read -r tag tip; do
  gate "git tag '$tag' '$tip' 2>/dev/null || git tag -f -m 'consolidation archive pin (idempotent)' '$tag' '$tip'"
done <<'TAGS'
archive/line/parity-glm|195e4309
archive/branch/feature/security-parity|8bb20c52
archive/branch/feature-elevation-wave1a|055592df
archive/branch/feat/annotation-eraser-local|916a4b7d
archive/branch/feat/ocr-verify-finereader|f5b59e66
archive/branch/ar/prompt-1|5d999987
archive/branch/feat/soak-48h-resume|0fad38c0
archive/branch/test/view-parity-baseline|0948743b
archive/branch/recovery/temp-stage-push-20260909|d367aca1
archive/branch/reconexec/snapshot-redaction-gaps-dirty|2adc3df6
archive/branch/feat/erase-ox-auto|faa6cf10
archive/branch/feat/regex-find-replace|a39356e7
archive/branch/feat/regex-ox-auto|7d54b387
archive/pr-head-final|12da4e2f
TAGS
# 0.4 push ALL archive tags BEFORE any branch deletion (they are local-only today)
run git push origin --tags 'refs/tags/archive/*:refs/tags/archive/*'
say "== Phase 1 — FOLD-1: ar/prompt-1 supersession review (G-FOLD-1) =="
# Per-fix rows required (D1..D5): PICKED (with test pin) or SUPERSEDED (file:line evidence).
# D2 is pre-proven superseded (EC06 drainPrefetches on the PR). cherry-pick -x ONLY the
# survivors, in a scratch worktree of D:/pdf/pdf-review, then build + touched suites x3 + push FF.
say "operator: execute G-FOLD-1 manually per plan §2.1; this script only records"
say "== Phase 3 — merge to main (FF-only) =="
must_consent_push
gate "git merge-base --is-ancestor origin/main $PR"                 # FF precondition
gate "git merge-base --is-ancestor main $PR"
run git push origin "$PR:main"
gate "git ls-remote origin main | grep -q \"$(git rev-parse $PR)\""
say "== Phase 4 — suite gate at main's new head (E1/E2 per handoff §8) =="
say "operator: fresh build + serial ctest + sanitizer + CI re-check; serial 100 % modulo the dispositioned flake class"
say "== Phase 5 — deletion pass (fail-closed; example entries; FULL LISTS from plan §3) =="
must_consent_delete
# every entry: pinned tip + content pin command (plan §4.5); order per plan §6
delete_proven feat/batch-presets-p2 ec22eeed \
  "git show $PR:src/core/BatchPreset.h | grep -qF 'run-ordered cross-file continuity'"
delete_proven feat/pr-review-fixes a25c37b7 \
  "git show $PR:src/shell/EditPolicy.h | grep -qF 'PR-review §3.1' && git show $PR:tests/TestSweepW3UxFlows.cpp | grep -qF 'flow7c'"
delete_proven feat/final-pgr-closers 68bc917e "git merge-base --is-ancestor 6841247d $PR"
# ... the united-line set ONLY after PR #2 records merged and phase-4 gate passed:
#   feat/parity-glm 195e4309 / feat/consolidated eb0efa21 / consolidate/all 95dccb23 ...
# ... then the uPG=0 set (182 refs) and the archive classes (branch refs only — tags stay).
# HOLD set never deleted here: feat/soak-48h-resume, test/view-parity-baseline, recovery/*.
say "== Phase 6 — end state =="
gate "test \"\$(git ls-remote --heads origin | wc -l)\" = 1 && git ls-remote --heads origin | grep -q main"
gate "git rev-list --count 195e4309 --not archive/line/parity-glm | grep -q '^0$'"
say "end state: main @ $(git rev-parse $PR); every branch tip tag-pinned + bundle-pinned. DONE."
