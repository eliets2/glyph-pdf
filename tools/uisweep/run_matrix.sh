#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SWEEP-W3-UI matrix runner: the R17 matrix (1366x768 + 1920x1080) x
# (QT_SCALE_FACTOR 1.0 / 1.5 / 2.0), one probe process per cell, six runs.
# Each run writes PNGs + audit-<tag>.json + rc-<tag>.txt into
# <repo>/.context/ui-sweep/<tag>/.
#
# Usage (MSYS2 UCRT64 login shell):
#   bash /d/pdf/pdf-sec/tools/uisweep/run_matrix.sh [build-dir]
set -uo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="${1:-$REPO/build-ui}"
PROBE="$BUILD/ui_sweep_probe.exe"
OUT="$REPO/.context/ui-sweep"
FIXTURES="$REPO/tests/fixtures/signing"

mkdir -p "$OUT"
echo "MATRIX BEGIN $(date -Is)" > "$OUT/matrix.log"

run_cell() {
    local tag="$1" w="$2" h="$3" scale="$4"
    local dir="$OUT/$tag"
    rm -rf "$dir"           # our own evidence dir only
    mkdir -p "$dir/profile"
    echo "== cell $tag (${w}x${h} @ ${scale})" | tee -a "$OUT/matrix.log"
    ( cd "$dir" && \
      QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR="$scale" \
      "$PROBE" --out "$dir" --tag "$tag" --width "$w" --height "$h" --fixtures "$FIXTURES" \
    ) >> "$OUT/matrix.log" 2>&1
    local rc=$?
    echo "rc=$rc tag=$tag" | tee -a "$OUT/matrix.log"
    echo $rc
}

TOTAL=0; FAILS=0
for cell in "1366 768 1.0 small-100" "1366 768 1.5 small-150" "1366 768 2.0 small-200" \
            "1920 1080 1.0 large-100" "1920 1080 1.5 large-150" "1920 1080 2.0 large-200"; do
    # shellcheck disable=SC2086
    set -- $cell
    rc=$(run_cell "$4" "$1" "$2" "$3") || true
    TOTAL=$((TOTAL+1))
    [ "$rc" != "0" ] && FAILS=$((FAILS+1))
done

echo "MATRIX END total=$TOTAL rcFailures=$FAILS $(date -Is)" | tee -a "$OUT/matrix.log"
grep -l "DONE" "$OUT"/rc-*.txt 2>/dev/null | wc -l
