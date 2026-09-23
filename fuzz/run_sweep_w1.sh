#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_sweep_w1.sh — W1 ponytail-sweep campaign runner (feat/sweep-w1-fuzz).
#
# Runs the five deterministic W1 drivers against their seeded corpora with a
# per-input watchdog budget, then re-runs every FINDING/TIMEOUT/crash input in
# single-seed repro mode and preserves the exact seed bytes under
# fuzz/findings/<bucket>/.
#
#   MSYSTEM=UCRT64 bash fuzz/run_sweep_w1.sh [mutsPerSeed] [budgetMs]
#
# Corpus regeneration:  python fuzz/corpus/w1/gen_fixtures.py
# Build:                cmake -B build-fz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
#                         -DGLYPHPDF_FUZZ=ON && cmake --build build-fz -j 2
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
MUTS="${1:-32}"
BUDGET="${2:-10000}"

FUZZ_BIN="$ROOT/build-fz/fuzz"
SCRATCH="$ROOT/fuzz/scratch/w1"
FINDINGS="${FINDINGS_OUT:-$ROOT/fuzz/findings/w1}"
mkdir -p "$SCRATCH" "$FINDINGS"

# Memory hygiene: ALL driver scratch (staged inputs, SafeSave candidates,
# rendered artifacts) lives under fuzz/scratch/w1 — never the system temp.
export SWEEP_SCRATCH="$(cygpath -w "$SCRATCH")"
export TMP="$(cygpath -w "$SCRATCH")"
export TEMP="$(cygpath -w "$SCRATCH")"
export TMPDIR="$(cygpath -w "$SCRATCH")"
export QT_QPA_PLATFORM=offscreen
export QT_LOGGING_RULES="*.debug=false"

# Runtime DLLs: the build dir FIRST (vendored podofo 1.1.0 + pdfium DLLs are
# staged there — the ucrt64 system libpodofo.dll must never win), then Qt/etc.
# Both msys64 roots: the build may resolve quickjs (libqjs-0.dll) from the
# D:\pdf\msys64 toolchain root while the interactive shell uses C:\msys64.
PATH="$ROOT/build-fz:$ROOT/third_party/podofo/install/bin:$ROOT/third_party/pdfium:/c/msys64/ucrt64/bin:/d/pdf/msys64/ucrt64/bin:$PATH"
export PATH

DRIVERS="fuzz_signreq fuzz_batchpreset fuzz_policy fuzz_a11y fuzz_reviewsummary"
declare -A CORPUS=(
  [fuzz_signreq]=signreq
  [fuzz_batchpreset]=batchpreset
  [fuzz_policy]=policy
  [fuzz_a11y]=a11y
  [fuzz_reviewsummary]=reviewsummary
)

total_execs=0
total_findings=0
report="$FINDINGS/CAMPAIGN-RESULTS.txt"
: > "$report"

for drv in $DRIVERS; do
  exe="$FUZZ_BIN/$drv.exe"
  corp="$ROOT/fuzz/corpus/${CORPUS[$drv]}"
  log="$SCRATCH/${drv}.log"
  if [ ! -x "$exe" ]; then
    echo "SKIP $drv (missing $exe)" | tee -a "$report"
    continue
  fi
  echo "== $drv (corpus $(ls "$corp" | wc -l) seeds x $MUTS mutants, budget ${BUDGET}ms)"
  "$exe" campaign "$corp" "$MUTS" "$BUDGET" > "$log" 2> "$log.err"
  rc=$?
  execs=$(grep -c '^EXEC ' "$log" || true)
  finds=$(grep -c 'FINDING' "$log" || true)
  total_execs=$((total_execs + execs))
  total_findings=$((total_findings + finds))
  {
    echo "== $drv rc=$rc execs=$execs finding-lines=$finds"
    grep 'FINDING\|SUMMARY\|TIMEOUT' "$log" | head -40
    [ -s "$log.err" ] && head -5 "$log.err"
  } >> "$report"

  # ── repro + seed preservation for every flagged input ───────────────────
  grep '^EXEC ' "$log" | grep 'FINDING' | head -5 | while read -r line; do
    file=$(echo "$line" | awk '{print $2}' | sed 's/\\/\//g; s|^C:|/c|')
    idx=$(echo "$line" | awk '{print $3}' | tr -d '#')
    kind=$(echo "$line" | awk '{print $4}')
    base=$(basename "$file")
    bucket="$FINDINGS/${drv}-${base%.???}-${kind//[^A-Za-z0-9_]/}"
    mkdir -p "$bucket"
    cp "$file" "$bucket/seed-source.bin" 2>/dev/null
    "$exe" materialize "$file" "$idx" "$bucket/seed.bin" > /dev/null 2>&1
    # Single-seed repro from the preserved file alone:
    "$exe" one "$bucket/seed.bin" > "$bucket/repro.log" 2>&1
    rc2=$?
    echo "$drv seed=$base idx=$idx kind=$kind repro_rc=$rc2" >> "$bucket/EVIDENCE.txt"
    cp "$line" "$bucket/original-exec-line.txt" 2>/dev/null
    if [ $rc2 -ne 0 ]; then
      echo "  REPRO-FAIL kind=$kind rc=$rc2 -> $bucket"
    else
      echo "  preserved (oracle verdict, not a crash): $bucket"
    fi
  done

  # Hard deaths (crash/timeout mid-campaign): the LAST printed EXEC line is
  # the guilty input — materialize + preserve it too.
  if [ $rc -ne 0 ]; then
    last=$(grep '^EXEC ' "$log" | tail -1)
    if [ -n "$last" ]; then
      file=$(echo "$last" | awk '{print $2}' | sed 's/\\/\//g; s|^C:|/c|')
      idx=$(echo "$last" | awk '{print $3}' | tr -d '#')
      bucket="$FINDINGS/${drv}-crash-$(basename "$file" | tr -d '.')-$idx"
      mkdir -p "$bucket"
      cp "$file" "$bucket/seed-source.bin" 2>/dev/null
      "$exe" materialize "$file" "$idx" "$bucket/seed.bin" > /dev/null 2>&1
      "$exe" one "$bucket/seed.bin" > "$bucket/repro.log" 2>&1
      rc2=$?
      echo "$drv seed=$(basename "$file") idx=$idx CAMPAIGN_DEATH rc=$rc campaign_repro_rc=$rc2" \
        >> "$bucket/EVIDENCE.txt"
      echo "$last" > "$bucket/original-exec-line.txt"
      echo "  CRASH/TIMEOUT preserved rc=$rc2 -> $bucket"
    fi
  fi
done

echo "TOTAL execs>=$total_execs findings-lines=$total_findings" | tee -a "$report"
echo "report: $report"
