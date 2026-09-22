#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ui-fix lane: build tools/uisweep/minwidth_probe.cpp with the EXACT
# compiler/link commands CMake generated for tests/TestCommandBinding.cpp
# (same ninja -t commands trick as build_probe.sh, worktree-relative).
#
# Usage (MSYS2 UCRT64 login shell): bash tools/uisweep/build_minwidth.sh [build-dir]
set -euo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="${1:-$REPO/build-uif}"
cd "$BUILD"

echo "== ensure TestCommandBinding objects exist"
ninja -j 2 TestCommandBinding > minwidth-tcb-build.log 2>&1 || { tail -20 minwidth-tcb-build.log; exit 1; }

ninja -t commands TestCommandBinding > minwidth-cmds.txt
SRC_LINE=$(grep -F "tests/TestCommandBinding.cpp" minwidth-cmds.txt | grep -F -- "-o" | grep -F -- "-c" | head -1)
[ -n "$SRC_LINE" ] || { echo "FATAL: compile command not found"; exit 1; }
LINK_LINE=$(grep -F -- "-o TestCommandBinding.exe" minwidth-cmds.txt | head -1)
[ -n "$LINK_LINE" ] || { echo "FATAL: link command not found"; exit 1; }

norm() {
    local s="$1"
    s="${s//\\\"/@DQ@}"
    s="${s//\\//}"
    s="${s//@DQ@/\\\"}"
    printf '%s' "$s"
}
SRC_LINE=$(norm "$SRC_LINE")
LINK_LINE=$(norm "$LINK_LINE")

OLD_OBJ="CMakeFiles/TestCommandBinding.dir/tests/TestCommandBinding.cpp.obj"
OLD_SRC="$(printf '%s' "$SRC_LINE" | grep -oE '[A-Za-z]:/[^" ]*tests/TestCommandBinding\.cpp' | head -1)"
[ -n "$OLD_SRC" ] || { echo "FATAL: TestCommandBinding.cpp path not found"; exit 1; }
NEW_SRC=$(cygpath -m "$REPO/tools/uisweep/minwidth_probe.cpp")
NEW_OBJ="minwidth_probe.obj"

PROBE_LINE="${SRC_LINE//$OLD_OBJ/$NEW_OBJ}"
PROBE_LINE="${PROBE_LINE//$OLD_SRC/$NEW_SRC}"
bash -c "$PROBE_LINE"

# qrc resources (icons + QSS) — same as the sweep probe, so the chrome
# stylesheets render as shipped.
QRC_OBJS=""
n=0
for qsrc in PdfWorkstation_autogen/*/qrc_resources.cpp PdfWorkstation_autogen/*/qrc_PdfWorkstation_translations.cpp; do
    [ -f "$qsrc" ] || continue
    n=$((n+1))
    qobj="minwidth_qrc_$n.obj"
    qsrc_abs=$(cygpath -m "$(cd "$(dirname "$qsrc")" && pwd)/$(basename "$qsrc")")
    QLINE="${SRC_LINE//$OLD_OBJ/$qobj}"
    QLINE="${QLINE//$OLD_SRC/$qsrc_abs}"
    bash -c "$QLINE" || { echo "FATAL: qrc compile failed for $qsrc"; exit 1; }
    QRC_OBJS="$QRC_OBJS $qobj"
done
[ -n "$QRC_OBJS" ] || { echo "FATAL: no generated qrc sources found"; exit 1; }

LINK_LINE="${LINK_LINE//$OLD_OBJ/$NEW_OBJ$QRC_OBJS}"
LINK_LINE="${LINK_LINE//TestCommandBinding.exe/minwidth_probe.exe}"
INNER="${LINK_LINE#*cd . && }"
INNER="${INNER%% && *}"
bash -c "$INNER"

mkdir -p platforms
PLUGIN=""
for cand in "$(dirname "$(ninja -t files 2>/dev/null | grep -F 'TestCommandBinding.exe' | head -1)")/platforms/qoffscreen.dll" /d/pdf/pdf-sec/build-ui/platforms/qoffscreen.dll; do
    [ -f "$cand" ] && PLUGIN="$cand" && break
done
[ -n "$PLUGIN" ] || PLUGIN=$(find /ucrt64 -name "qoffscreen.dll" 2>/dev/null | head -1)
if [ -n "$PLUGIN" ] && [ -f "$PLUGIN" ]; then
    cp -uf "$PLUGIN" platforms/ 2>/dev/null || true
    echo "staged qoffscreen from $PLUGIN"
fi
ls -la minwidth_probe.exe
echo "BUILD OK"
