#!/usr/bin/env bash
# fuzz/build_clang/build_djot_clang.sh
#
# Build the P3 djot libFuzzer harness (harness_djot.cpp) WITH clang + libFuzzer
# + ASan/UBSan.
#
# INF06 contract (2026-09-07 infrastructure review): this script must FAIL —
# never exit 0 — when the harness cannot be built. A green CI result has to
# mean "built, ran, and survived". A deliberately unavailable clang must be
# represented by SKIPPING the CI job (a job-level `if:` condition), not by a
# zero exit code here.
#
# The repository root is derived from THIS SCRIPT's location, so the script
# works from any checkout/worktree and any working directory (it used to
# hardcode /c/Users/User/Projects/pdf, which silently targeted the wrong tree
# and could not exist on the Ubuntu CI runner at all).
#
# Provision clang on MSYS2 ucrt64:
#   pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-compiler-rt
# (GATED: ~250 MB download + install. Get owner approval first.)
#
# IMPORTANT ABI NOTE: liblua/pdfws_djot/docmodel in build/ were compiled with
# g++. libFuzzer needs clang. Mixing g++ and clang libstdc++ objects can fail to
# link. This script therefore REBUILDS the three needed libs with clang into
# fuzz/build_clang/ so the whole chain is one compiler -- the correct approach
# per the MSan/instrumentation rule (whole-chain consistency).
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# ucrt64 toolchain when run from an MSYS2 environment; harmless elsewhere.
[ -d /c/msys64/ucrt64/bin ] && export PATH="/c/msys64/ucrt64/bin:$PATH"
CLANGXX="${CLANGXX:-clang++}"
CLANG="${CLANG:-clang}"
OUT="fuzz/build_clang"
mkdir -p "$OUT/obj" fuzz/bin

if ! command -v "$CLANGXX" >/dev/null 2>&1; then
  echo "ERROR: clang++ ('$CLANGXX') not found — refusing to report success (INF06)." >&2
  echo "  Provision with: pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-compiler-rt" >&2
  echo "  (Ubuntu runners: sudo apt-get install clang)" >&2
  echo "  To skip this target deliberately, gate the CI JOB with a condition — do not" >&2
  echo "  turn this script's failure into a green result." >&2
  exit 1
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  echo "ERROR: clang ('$CLANG') not found — refusing to report success (INF06)." >&2
  exit 1
fi

SAN="-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g -O1"
INC="-Isrc -Isrc/pdfws_djot -Isrc/core/interfaces -Isrc/docmodel -Ithird_party/lua-5.4/src"

echo "[1] rebuild liblua (C) with clang"
for f in third_party/lua-5.4/src/*.c; do
  b=$(basename "$f" .c)
  [ "$b" = "lua" ] && continue   # skip standalone interpreter main
  [ "$b" = "luac" ] && continue
  "$CLANG" $SAN -w -c "$f" -o "$OUT/obj/lua_$b.o"
done

echo "[2] rebuild docmodel + pdfws_djot (C++) with clang"
for f in $(find src/docmodel src/pdfws_djot -name '*.cpp'); do
  b=$(echo "$f" | tr '/' '_')
  "$CLANGXX" -std=c++17 $SAN $INC -c "$f" -o "$OUT/obj/${b%.cpp}.o"
done

echo "[3] build + link the fuzzer"
"$CLANGXX" -std=c++17 $SAN -fsanitize=fuzzer $INC \
  -DDJOT_LIB_PATH="\"$ROOT/third_party/djot\"" \
  fuzz/harnesses/harness_djot.cpp "$OUT"/obj/*.o \
  -o fuzz/bin/djot_fuzzer

# INF06: a build that produces no executable must not exit 0.
if [ ! -x fuzz/bin/djot_fuzzer ]; then
  echo "ERROR: fuzz/bin/djot_fuzzer missing after link — build did not produce the harness." >&2
  exit 1
fi

echo "OK -> fuzz/bin/djot_fuzzer"
echo "Run:  ASAN_OPTIONS=abort_on_error=1:allocator_may_return_null=1 \\"
echo "      ./fuzz/bin/djot_fuzzer -dict=fuzz/dict/djot.dict -max_len=65536 fuzz/corpus/djot/"
