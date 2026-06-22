// fuzz/harnesses/repro_lua_tostring.cpp
//
// Minimal, DETERMINISTIC reproducer for the lua_tostring null-deref class at
// LuaDjotCodec.cpp:550 and :568.  It uses the SAME vendored Lua 5.4 the product
// links. It reproduces the exact unsafe construct:
//
//      std::string err = lua_tostring(L, -1);
//
// when lua_pcall fails with a NON-STRING error value (a table). For such a
// value lua_tostring returns NULL, and std::string(NULL) is undefined behavior
// (libstdc++ dereferences and calls strlen(NULL) -> SIGSEGV).
//
// This is APPARATUS proving the code pattern is unsafe; it does not touch the
// product binary. Build with the vendored liblua only.
//
// Build (ucrt64, no clang needed):
//   g++ -std=c++17 -g -O0 fuzz/harnesses/repro_lua_tostring.cpp \
//       build/third_party/libliblua.a -I third_party/lua-5.4/src \
//       -o fuzz/bin/repro_lua_tostring.exe
//
// Two sub-cases via argv[1]:
//   string   -> error("msg")  : lua_tostring returns the message (SAFE path)
//   table    -> error({})     : lua_tostring returns NULL (UNSAFE -> crash)

#include <cstdio>
#include <string>
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "table";
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // Build a chunk that raises the chosen error value, mirroring how djot.lua
    // could raise a non-string error that propagates through lua_pcall.
    std::string chunk = (std::string(mode) == "string")
        ? "error('a string error')"
        : "error(setmetatable({}, {}))";   // a table with NO __tostring -> lua_tostring == NULL

    luaL_loadstring(L, chunk.c_str());
    int rc = lua_pcall(L, 0, 0, 0);
    std::printf("[repro] mode=%s pcall_rc=%d (LUA_OK=%d)\n", mode, rc, LUA_OK);

    if (rc != LUA_OK) {
        const char* raw = lua_tostring(L, -1);   // <-- the exact product call
        std::printf("[repro] lua_tostring(L,-1) = %p\n", (const void*)raw);
        // Exact product construct from LuaDjotCodec.cpp:550/568:
        std::string err = lua_tostring(L, -1);    // UB if raw == NULL
        std::printf("[repro] std::string err = \"%s\" (len=%zu)\n",
                    err.c_str(), err.size());
    }
    lua_close(L);
    std::printf("[repro] survived\n");
    return 0;
}
