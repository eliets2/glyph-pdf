#!/usr/bin/env bash
# fuzz/run_oracles.sh -- one-shot driver+oracle campaign for P1/P2 (redaction).
# Builds nothing; assumes fuzz/bin/redaction_driver.exe is already linked
# (see fuzz/build_clang/build_redaction_driver.sh). Runtime DLLs come from the
# CMake build dir (override with GLYPHPDF_BUILD_DIR=<dir>).
#
#   bash fuzz/run_oracles.sh            # from anywhere; root = this script's ../
#
# INF06 contract (2026-09-07 infrastructure review):
#   - the repository root is derived from THIS script's location (the old
#     hardcoded /c/Users/User/Projects/pdf targeted the wrong checkout from
#     any other worktree and could not exist on other machines);
#   - the driver, the python interpreter and both oracle scripts are REQUIRED
#     (absent tools fail the campaign up front, not case-by-case);
#   - every case must produce an oracle JSON with a parseable CLEAN/LEAK
#     verdict — a missing or invalid oracle result is a FAILURE, never a
#     silent pass (success is not inferred from absent "LEAK" text);
#   - G19 (2026-09-09 quality gate): every DRIVER invocation must EXIT 0 as
#     well as produce its PDF — the old runner checked only output existence,
#     so six stale CLEAN PDFs from a previous campaign plus a driver that
#     always exits 9 reported ALL CLEAN and exited 0. All outputs/reports now
#     live in a fresh, owned per-run directory (no reused fixed names), so a
#     prior campaign's artifacts can never be inspected as this run's result.
#   - the exit code is 0 only when every case produced a valid verdict.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1
BUILD_DIR="${GLYPHPDF_BUILD_DIR:-$ROOT/build}"
if [ -d /c/msys64/ucrt64/bin ]; then
  export PATH="$BUILD_DIR:/c/msys64/ucrt64/bin:$PATH"
else
  export PATH="$BUILD_DIR:$PATH"
fi
PYTHON="${PYTHON:-python}"
DRV="$ROOT/fuzz/bin/redaction_driver.exe"
SC="$ROOT/fuzz/scratch"
TM_ORACLE="$ROOT/fuzz/oracle/tm_residual_oracle.py"
LEAK_ORACLE="$ROOT/fuzz/oracle/redaction_leak_oracle.py"
mkdir -p "$SC"
fail=0

# --- G19: fresh, owned per-run output/report directory ----------------------
# The unique name (start time + PID) plus a plain `mkdir` (no -p) make the
# directory fresh by construction: if anything already occupies the path the
# campaign fails up front instead of mixing evidence with a prior run.
RUNDIR="$SC/run-$(date +%Y%m%d-%H%M%S)-$$"
mkdir "$RUNDIR" || {
  echo "ERROR: fresh run directory '$RUNDIR' cannot be created - refusing to share oracle outputs with a prior run (G19)." >&2
  exit 1
}

# --- INF06: required tools/artifacts are required ---------------------------
if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "ERROR: python ('$PYTHON') not found — oracles cannot produce verdicts; failing (INF06)." >&2
  exit 1
fi
if [ ! -x "$DRV" ]; then
  echo "ERROR: redaction driver not found at $DRV — build it first:" >&2
  echo "  bash fuzz/build_clang/build_redaction_driver.sh" >&2
  exit 1
fi
if [ ! -f "$TM_ORACLE" ] || [ ! -f "$LEAK_ORACLE" ]; then
  echo "ERROR: oracle scripts missing under fuzz/oracle/ — refusing to run a gate without its oracle (INF06)." >&2
  exit 1
fi

# verdict_from <json-path>: print CLEAN / LEAK on stdout, or return non-zero
# (diagnostics on stderr) when the JSON is missing, unparseable, or carries no
# recognised verdict. NOTE: callers must set fail=1 themselves — the function
# runs in a command-substitution subshell, where assignments do not propagate.
verdict_from() {
  local json="$1" v
  if [ ! -s "$json" ]; then
    echo "ORACLE FAILED: no JSON report at $json" >&2; return 1
  fi
  v=$("$PYTHON" -c "import json,sys;d=json.load(open(sys.argv[1],encoding='utf-8'));print(d.get('verdict',''))" "$json" 2>/dev/null)
  if [ "$v" != "CLEAN" ] && [ "$v" != "LEAK" ]; then
    echo "ORACLE FAILED: no valid CLEAN/LEAK verdict in $json" >&2; return 1
  fi
  echo "$v"
}

# --- P2/F-02 matrix: each row is "name secret a b c d e f rx ry rw rh" -------
# Identity control (expect CLEAN), then non-identity linear parts (rotation,
# scale, skew). Rect chosen to cover the TRUE glyph path; if redaction ignores
# the Tm linear part the secret survives -> LEAK.
declare -a CASES=(
  "identity    IDENT    1 0 0 1 100 700   60 70 400 60"
  "rot90       ROT90    0 1 -1 0 100 700  90 7  40 65"
  "rot45       ROT45    0.707 0.707 -0.707 0.707 100 600  90 60 120 130"
  "scale3x     SCALE3   3 0 0 1 100 700   60 70 400 60"
  "skewx       SKEWX    1 0 1 1 100 700   60 50 400 90"
)

for c in "${CASES[@]}"; do
  set -- $c
  name=$1; secret=$2; a=$3;b=$4;cc=$5;d=$6;e=$7;f=$8; rx=$9;ry=${10};rw=${11};rh=${12}
  out="$RUNDIR/case_${name}.pdf"
  json="$RUNDIR/case_${name}.json"
  # G19: the driver's EXIT STATUS is part of the evidence. A driver that
  # fails must fail the case even if a same-named PDF from a prior campaign
  # (now impossible — outputs live in the fresh RUNDIR) exists on disk.
  "$DRV" tm "$out" "$secret" $a $b $cc $d $e $f $rx $ry $rw $rh >/dev/null 2>&1
  drv_rc=$?
  if [ $drv_rc -ne 0 ] || [ ! -f "$out" ]; then
    echo "[$name] DRIVER FAILED (rc=$drv_rc, pdf produced: $([ -f "$out" ] && echo yes || echo no))"
    fail=1
    continue
  fi
  # A failing oracle must be visible as a failure even though its output is
  # consumed here: verdict_from returns non-zero on missing/invalid results.
  res=$("$PYTHON" "$TM_ORACLE" --pdf "$out" --secret "$secret" --json "$json" 2>"$RUNDIR/case_${name}.oracle.err")
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "[$name] ORACLE FAILED (tm_residual_oracle rc=$rc)"; fail=1; continue
  fi
  if ! v=$(verdict_from "$json"); then
    echo "[$name] ORACLE FAILED (invalid/missing oracle result — see stderr)" >&2
    fail=1
    continue
  fi
  echo "[$name] Tm=$a $b $cc $d $e $f  -> $v"
  [ "$v" = "LEAK" ] && { echo "    !! RESIDUAL SECRET in $out"; }
done

echo
echo "=== P1/F-01: orphaned-object leak across revisions (plain Save path) ==="
# G19: driver exit status + fresh output required here too.
"$DRV" simple "$RUNDIR/p1_simple.pdf" "P1SECRET" >/dev/null 2>&1
drv_rc=$?
if [ $drv_rc -ne 0 ] || [ ! -f "$RUNDIR/p1_simple.pdf" ]; then
  echo "[p1_simple] DRIVER FAILED (rc=$drv_rc, pdf produced: $([ -f "$RUNDIR/p1_simple.pdf" ] && echo yes || echo no))"
  fail=1
else
  "$PYTHON" "$LEAK_ORACLE" --pdf "$RUNDIR/p1_simple.pdf" --secret "P1SECRET" --json "$RUNDIR/p1_simple.json" >"$RUNDIR/p1_simple.oracle.out" 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "[p1_simple] ORACLE FAILED (redaction_leak_oracle rc=$rc)"; fail=1
  elif ! v=$(verdict_from "$RUNDIR/p1_simple.json"); then
    echo "[p1_simple] ORACLE FAILED (invalid/missing oracle result — see stderr)"; fail=1
  else
    sed 's/^/    /' "$RUNDIR/p1_simple.oracle.out" | grep -E 'verdict|hits' || true
    echo "[p1_simple] -> $v"
    [ "$v" = "LEAK" ] && { echo "    !! RESIDUAL SECRET in $RUNDIR/p1_simple.pdf"; }
  fi
fi

produced=$(ls "$RUNDIR"/case_*.json "$RUNDIR"/p1_simple.json 2>/dev/null | wc -l)
if [ "$produced" -lt 6 ]; then
  echo "ERROR: expected 6 oracle reports (5 matrix + 1 P1), got $produced — failing (INF06)." >&2
  fail=1
fi

echo
echo "Done. JSON reports in $RUNDIR (fresh per-run directory, G19)."
exit $fail
