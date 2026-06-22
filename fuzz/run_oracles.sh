#!/usr/bin/env bash
# fuzz/run_oracles.sh -- one-shot driver+oracle campaign for P1/P2 (redaction).
# Builds nothing; assumes fuzz/bin/redaction_driver.exe is already linked
# (see fuzz/build_clang/build_redaction_driver.sh). Runtime DLLs come from build/.
#
#   cd /c/Users/User/Projects/pdf && bash fuzz/run_oracles.sh
#
set -u
ROOT="/c/Users/User/Projects/pdf"
cd "$ROOT" || exit 1
export PATH="$ROOT/build:/c/msys64/ucrt64/bin:$PATH"
DRV="$ROOT/fuzz/bin/redaction_driver.exe"
SC="$ROOT/fuzz/scratch"
LEAK="python $ROOT/fuzz/oracle/redaction_leak_oracle.py"
TM="python $ROOT/fuzz/oracle/tm_residual_oracle.py"
mkdir -p "$SC"
fail=0

run() { # name mode-args... -- secret -- rect-args...
  echo "=== $1 ==="
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
  out="$SC/case_${name}.pdf"
  "$DRV" tm "$out" "$secret" $a $b $cc $d $e $f $rx $ry $rw $rh >/dev/null 2>&1
  if [ ! -f "$out" ]; then echo "[$name] DRIVER FAILED"; fail=1; continue; fi
  res=$($TM --pdf "$out" --secret "$secret" --json "$SC/case_${name}.json" 2>/dev/null)
  v=$(echo "$res" | grep -o '"verdict": *"[A-Z]*"' | head -1)
  echo "[$name] Tm=$a $b $cc $d $e $f  -> $v"
  echo "$res" | grep -q '"verdict": "LEAK"' && { echo "    !! RESIDUAL SECRET in $out"; }
done

echo
echo "=== P1/F-01: orphaned-object leak across revisions (plain Save path) ==="
"$DRV" simple "$SC/p1_simple.pdf" "P1SECRET" >/dev/null 2>&1
$LEAK --pdf "$SC/p1_simple.pdf" --secret "P1SECRET" --json "$SC/p1_simple.json" 2>&1 | grep -E 'verdict|hits' | sed 's/^/    /'

echo
echo "Done. JSON reports in $SC/case_*.json and $SC/p1_*.json"
exit $fail
