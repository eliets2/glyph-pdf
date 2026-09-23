#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SWEEP-W3-UI probe builder.
#
# Compiles tools/uisweep/ui_sweep_probe.cpp with the EXACT compiler/link
# commands CMake generated for tests/TestCommandBinding.cpp in build-ui —
# the perf lane's build_drivers.sh trick (ninja -t commands). CMakeLists.txt
# stays untouched: this lane modifies production code only for P1 fixes in
# its own surfaces, which this harness is not.
#
# The probe also compiles the APP'S OWN resources.qrc (icons + theme QSS)
# via rcc — without it every icon would render as the circle fallback (the
# qrc belongs to the PdfWorkstation exe only), which would be a harness
# artifact, not product evidence.
#
# Usage (inside MSYS2 UCRT64 login shell, ANY cwd):
#   bash /d/pdf/pdf-sec/tools/uisweep/build_probe.sh [build-dir]
# Output: <build-dir>/ui_sweep_probe.exe (beside the test exes, sharing the
# staged runtime DLLs + platforms plugin).
set -euo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="${1:-$REPO/build-ui}"
cd "$BUILD"

echo "== ensure TestCommandBinding objects exist (builds only that target + deps)"
ninja -j 2 TestCommandBinding > uisweep-tcb-build.log 2>&1 || { tail -20 uisweep-tcb-build.log; exit 1; }

echo "== extracting exact commands for TestCommandBinding from build.ninja"
ninja -t commands TestCommandBinding > uisweep-cmds.txt

SRC_LINE=$(grep -F "tests/TestCommandBinding.cpp" uisweep-cmds.txt | grep -F -- "-o" | grep -F -- "-c" | head -1)
if [ -z "$SRC_LINE" ]; then echo "FATAL: compile command not found"; exit 1; fi
LINK_LINE=$(grep -F -- "-o TestCommandBinding.exe" uisweep-cmds.txt | head -1)
if [ -z "$LINK_LINE" ]; then echo "FATAL: link command not found"; exit 1; fi

# ninja emits Windows backslash paths; bash -c eats backslashes as escapes.
# Convert every backslash to a forward slash FIRST — g++/ld accept them — but
# PRESERVE \" (escaped quotes inside -D macros).
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
# Derive the worktree path the way build.ninja writes it (junction-safe —
# hardcoding either junction spelling breaks the other).
OLD_SRC="$(printf '%s' "$SRC_LINE" | grep -oE '[A-Za-z]:/[^" ]*tests/TestCommandBinding\.cpp' | head -1)"
if [ -z "$OLD_SRC" ]; then echo "FATAL: TestCommandBinding.cpp path not found in compile line"; exit 1; fi
echo "OLD_SRC=$OLD_SRC"
# ui-fix lane (2026-09-20): derive the probe source from THIS script's repo
# instead of a hardcoded worktree path, so any lane can rebuild the harness
# from its own checkout (the probe source itself is unchanged).
NEW_SRC=$(cygpath -m "$REPO/tools/uisweep/ui_sweep_probe.cpp")
NEW_OBJ="uisweep_probe.obj"

echo "== compiling the probe"
PROBE_LINE="${SRC_LINE//$OLD_OBJ/$NEW_OBJ}"
PROBE_LINE="${PROBE_LINE//$OLD_SRC/$NEW_SRC}"
bash -c "$PROBE_LINE"

echo "== compiling the AUTORCC-generated resource sources (what the app ships)"
# CMake's AUTORCC already generated the compiled-form qrc C++ for
# PdfWorkstation (icons + theme QSS + translations). Compile THOSE with the
# probe's own compile line (same Qt include dirs) — no rcc needed and the
# resource data is byte-identical to the shipped exe.
QRC_OBJS=""
n=0
for qsrc in PdfWorkstation_autogen/*/qrc_resources.cpp PdfWorkstation_autogen/*/qrc_PdfWorkstation_translations.cpp; do
    [ -f "$qsrc" ] || continue
    n=$((n+1))
    qobj="uisweep_qrc_$n.obj"
    qsrc_abs=$(cygpath -m "$(cd "$(dirname "$qsrc")" && pwd)/$(basename "$qsrc")")
    QLINE="${SRC_LINE//$OLD_OBJ/$qobj}"
    QLINE="${QLINE//$OLD_SRC/$qsrc_abs}"
    bash -c "$QLINE" || { echo "FATAL: qrc compile failed for $qsrc"; exit 1; }
    QRC_OBJS="$QRC_OBJS $qobj"
done
[ -n "$QRC_OBJS" ] || { echo "FATAL: no generated qrc sources found"; exit 1; }
echo "qrc objects:$QRC_OBJS"

echo "== linking ui_sweep_probe.exe (probe + qrc objects + TestCommandBinding's Bootstrapper.obj)"
LINK_LINE="${LINK_LINE//$OLD_OBJ/$NEW_OBJ$QRC_OBJS}"
LINK_LINE="${LINK_LINE//TestCommandBinding.exe/ui_sweep_probe.exe}"
# The real link is wrapped: cmd.exe /C "cd . && c++ ... -o X.exe ... && <POST_BUILD>...".
# bash -c mangles the cmd wrapper (UTF-16 garble); strip it and run the inner
# c++ invocation directly. The link's own args never contain ' && ', so the
# first remaining ' && ' starts the POST_BUILD chain — cut it.
INNER="${LINK_LINE#*cd . && }"
INNER="${INNER%% && *}"
bash -c "$INNER"

echo "== staging qoffscreen plugin beside the exe (same as the CMake POST_BUILD)"
EXE_DIR=$(dirname "$(ninja -t files 2>/dev/null | grep -F 'TestCommandBinding.exe' | head -1)" 2>/dev/null || echo .)
ls "$REPO/build-ui/platforms" >/dev/null 2>&1 || true
mkdir -p platforms
PLUGIN=""
for cand in "$EXE_DIR/platforms/qoffscreen.dll" "/d/pdf/pdf-sec/build-ui/platforms/qoffscreen.dll"; do
    [ -f "$cand" ] && PLUGIN="$cand" && break
done
if [ -z "$PLUGIN" ]; then
    PLUGIN=$(find /ucrt64 -name "qoffscreen.dll" 2>/dev/null | head -1)
fi
if [ -n "$PLUGIN" ] && [ -f "$PLUGIN" ]; then
    cp -uf "$PLUGIN" platforms/ 2>/dev/null || true
    echo "staged $(cygpath -w "$PLUGIN") -> $(pwd)/platforms/"
else
    echo "WARN: qoffscreen.dll not found for staging (may already exist beside exe)"
fi
ls -la ui_sweep_probe.exe
echo "BUILD OK"
