#!/usr/bin/env bash
# fuzz/build_clang/build_djot_clang.sh
#
# Build the P3 djot libFuzzer harness (harness_djot.cpp) WITH clang + libFuzzer
# + ASan/UBSan. SCAFFOLDED: this environment (ucrt64) has only g++ and no
# compiler-rt/libFuzzer, so this script is the exact recipe to run once a clang
# toolchain is provisioned (see fuzz/MANIFEST.md "BUILD MATRIX").
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
ROOT="/c/Users/User/Projects/pdf"
cd "$ROOT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
CLANGXX="${CLANGXX:-clang++}"
CLANG="${CLANG:-clang}"
OUT="fuzz/build_clang"
mkdir -p "$OUT/obj" fuzz/bin

if ! command -v "$CLANGXX" >/dev/null 2>&1; then
  echo "ERROR: clang++ not found. Install:"
  echo "  pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-compiler-rt"
  echo "Then re-run. (This script is intentionally a no-op without clang.)"
  exit 0
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

echo "OK -> fuzz/bin/djot_fuzzer"
echo "Run:  ASAN_OPTIONS=abort_on_error=1:allocator_may_return_null=1 \\"
echo "      ./fuzz/bin/djot_fuzzer -dict=fuzz/dict/djot.dict -max_len=65536 fuzz/corpus/djot/"
