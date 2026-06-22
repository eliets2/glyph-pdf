// fuzz/harnesses/harness_djot.cpp
//
// PRIORITY 3 libFuzzer harness for the Lua djot parser entry point
// (src/pdfws_djot/LuaDjotCodec.cpp). Pure C++/Lua path, no Qt required.
//
// Targets:
//   - pathological backtracking / large-alloc DoS in djot.lua
//   - lua_tostring null-deref on non-string Lua error objects
//     (LuaDjotCodec.cpp:550 and :568:  std::string err = lua_tostring(L,-1);
//      returns NULL when the error value is a table/nil -> std::string(NULL) UB)
//
// Build (REQUIRES clang+compiler-rt; gated install — see fuzz/MANIFEST.md):
//   clang++ -std=c++17 -g -O1 -fno-omit-frame-pointer \
//     -fsanitize=fuzzer,address,undefined \
//     -DDJOT_LIB_PATH='"<abs>/third_party/djot"' \
//     -I src -I src/core -I src/core/interfaces -I src/pdfws_djot \
//     -I src/docmodel -I third_party/lua-5.4/src \
//     fuzz/harnesses/harness_djot.cpp \
//     build/src/pdfws_djot/libpdfws_djot.a \
//     build/src/docmodel/libdocmodel.a \
//     build/third_party/libliblua.a \
//     -o fuzz/bin/djot_fuzzer
//
// NOTE: the engine archive was built with g++ (ucrt64). Mixing a clang-built
// harness with g++-built archives can fail to link due to libstdc++ ABI/name
// differences across compilers. If link fails, rebuild liblua + pdfws_djot +
// docmodel with the SAME clang (see fuzz/build_clang/build_djot_clang.sh).
// This file is the harness; the build script documents the full recipe.

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include "LuaDjotCodec.h"
#include "docmodel/SemanticDocument.h"

#ifndef DJOT_LIB_PATH
#define DJOT_LIB_PATH "third_party/djot"
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Bound input so DoS exploration stays a *finding signal*, not a hang that
    // wedges the campaign. libFuzzer's own -timeout flag is the real guard;
    // this is a belt-and-suspenders cap.
    if (size > (1u << 20)) return 0; // 1 MiB

    std::string djot(reinterpret_cast<const char*>(data), size);

    // Fresh codec per input -> fresh lua_State per parse (matches production).
    LuaDjotCodec codec(DJOT_LIB_PATH);
    try {
        auto doc = codec.djotToDocument(djot);
        (void)doc; // SemanticDocument destroyed at scope exit -> leak check
    } catch (const std::exception&) {
        // Expected for malformed input. The bug class we hunt (null-deref in
        // lua_tostring) crashes BEFORE the throw, so ASan/UBSan catch it; a
        // clean throw here is correct behavior, not a finding.
    } catch (...) {
        // Non-std exception: still not a crash; swallow.
    }
    return 0;
}
