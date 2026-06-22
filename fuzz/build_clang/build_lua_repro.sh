#!/usr/bin/env bash
# fuzz/build_clang/build_lua_repro.sh
# Build the deterministic lua_tostring null-deref reproducer with g++ (no clang
# needed). This is the recipe that produced the EVIDENCE in fuzz/findings/.
set -eu
ROOT="/c/Users/User/Projects/pdf"; cd "$ROOT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p fuzz/bin
g++ -std=c++17 -g -O0 fuzz/harnesses/repro_lua_tostring.cpp \
    build/third_party/libliblua.a -I third_party/lua-5.4/src \
    -o fuzz/bin/repro_lua_tostring.exe
echo "OK -> fuzz/bin/repro_lua_tostring.exe"
echo "Repro:  ./fuzz/bin/repro_lua_tostring.exe table   # crashes (NULL -> std::string)"
echo "Control:./fuzz/bin/repro_lua_tostring.exe string  # survives"
