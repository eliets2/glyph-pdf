#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SWEEP-W3-PERF driver builder.
#
# Compiles the tools/perf measurement drivers with the EXACT compiler/link
# commands CMake generated for tools/render_path_profile.cpp in build-perf
# (Release + LTO, same include dirs, same libs). This lane does NOT modify
# CMakeLists.txt (production read-only) — extracting the generated commands is
# the honest way to link against the same objects the app ships with.
#
# Usage (inside MSYS2 UCRT64 shell, from anywhere):
#   /c/Users/User/Projects/pdf-sec/tools/perf/build_drivers.sh [build-dir]
#
# Outputs: <build-dir>/perf_fixtures.exe, perf_startup.exe, perf_docops.exe
# plus the qoffscreen platform plugin staged beside them (the same staging the
# CMakeLists POST_BUILD gives render_path_profile).
set -euo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="${1:-$REPO/build-perf}"
cd "$BUILD"

echo "== extracting exact commands for render_path_profile from build.ninja"
ninja -t commands render_path_profile > perf-cmds.txt

SRC_LINE=$(grep -F "tools/render_path_profile.cpp" perf-cmds.txt | grep -F -- "-o" | head -1)
if [ -z "$SRC_LINE" ]; then echo "FATAL: compile command not found"; exit 1; fi
LINK_LINE=$(grep -F -- "-o render_path_profile.exe" perf-cmds.txt | head -1)
if [ -z "$LINK_LINE" ]; then echo "FATAL: link command not found"; exit 1; fi

# ninja emits Windows backslash paths; bash -c eats backslashes as escapes.
# Convert every backslash to a forward slash FIRST — g++/ld accept them — but
# PRESERVE \" (escaped quotes inside -D macros; e.g.
# -DGLYPHPDF_QUICKJS_VERSION=\"0.15.0\" must survive as \").
norm() {
    local s="$1"
    s="${s//\\\"/@DQ@}"
    s="${s//\\//}"
    s="${s//@DQ@/\\\"}"
    printf '%s' "$s"
}

SRC_LINE=$(norm "$SRC_LINE")
LINK_LINE=$(norm "$LINK_LINE")

OLD_OBJ="CMakeFiles/render_path_profile.dir/tools/render_path_profile.cpp.obj"
OLD_SRC="C:/Users/User/Projects/pdf-sec/tools/render_path_profile.cpp"

# Bootstrapper.cpp lives in the PdfWorkstation EXECUTABLE target, not the
# static libs (the TestDocumentIdentity precedent: test targets compile it
# themselves and MUST define DJOT_LIB_DIR — its #error enforces it). The
# startup/open-app drivers call Bootstrapper::createContext(), so compile it
# once with the same injected define and slip it into every link.
BOOTSTRAP_OBJ="perf-bootstrapper.obj"
BOOT_LINE="${SRC_LINE//$OLD_OBJ/$BOOTSTRAP_OBJ}"
BOOT_LINE="${BOOT_LINE%* -c *render_path_profile.cpp}"
BOOT_LINE="$BOOT_LINE -c $REPO/src/app/Bootstrapper.cpp -DDJOT_LIB_DIR=\\\"$REPO/third_party/djot\\\""
echo "== compiling src/app/Bootstrapper.cpp (DJOT_LIB_DIR injected)"
bash -c "$BOOT_LINE"

# The link command is a cmd.exe sandwich:
#   cmd.exe /C "cd . && <linker ...> && cmd.exe /C "cd /D ... <POST_BUILD staging> ""
# ninja runs it via CreateProcess; bash cannot (quote nesting). Strip the
# wrapper and run the LINKER command directly — this script stages qoffscreen
# itself at the end (the same artifact the POST_BUILD produces).
LINK_LINE="${LINK_LINE#cmd.exe /C \"cd . && }"
LINK_LINE="${LINK_LINE%% && cmd.exe /C \"cd /D*}"
LINK_LINE="${LINK_LINE%\"}"

build_one() {
    local src="$REPO/$1" exe="$2"
    local obj="perf-$(basename "$1" .cpp).obj"

    echo "== compiling $1"
    # Swap EVERY MT/MF/-o object reference, then the trailing `-c <source>`
    # argument (last token of the command).
    local compile_cmd="${SRC_LINE//$OLD_OBJ/$obj}"
    compile_cmd="${compile_cmd%* -c *render_path_profile.cpp}"
    compile_cmd="$compile_cmd -c $src"
    bash -c "$compile_cmd"

    echo "== linking $exe"
    local link_cmd="${LINK_LINE//$OLD_OBJ/$obj}"
    # Insert the Bootstrapper object right before -o so its symbols resolve
    # in the same order the exe target links them (driver obj first).
    link_cmd="${link_cmd/ -o / $BOOTSTRAP_OBJ -o }"
    link_cmd="${link_cmd//-o render_path_profile.exe/-o $exe}"
    bash -c "$link_cmd"
}

build_one "tools/perf/perf_fixtures.cpp" "perf_fixtures.exe"
build_one "tools/perf/perf_startup.cpp" "perf_startup.exe"
build_one "tools/perf/perf_docops.cpp" "perf_docops.exe"

# Sanity: the three drivers must NOT be byte-identical (guards against the
# substitution failure mode where every build compiles the SAME source).
H1=$(sha256sum perf_fixtures.exe | cut -d' ' -f1)
H2=$(sha256sum perf_startup.exe | cut -d' ' -f1)
if [ "$H1" = "$H2" ]; then
    echo "FATAL: perf_fixtures.exe and perf_startup.exe are identical — substitution failed"
    exit 1
fi

# Stage the qoffscreen plugin beside the drivers (same as the CMake POST_BUILD).
if [ ! -f "$BUILD/platforms/qoffscreen.dll" ]; then
    mkdir -p "$BUILD/platforms"
    cp /c/msys64/ucrt64/share/qt6/plugins/platforms/qoffscreen.dll "$BUILD/platforms/" 2>/dev/null || true
fi

echo "== OK: drivers built in $BUILD"
ls -la "$BUILD"/perf_*.exe
