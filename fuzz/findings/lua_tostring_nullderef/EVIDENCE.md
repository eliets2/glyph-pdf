# EVIDENCE lua-tostring-nullderef  (HANDOFF to native-adversary)

Type:            crash (null-pointer / process abort) — DoS class
Harness/target:  fuzz/bin/repro_lua_tostring.exe (deterministic reproducer) mirroring
                 src/pdfws_djot/LuaDjotCodec.cpp:550 and :568
Sanitizer:       no ASan available; libstdc++ hardened std::string throws std::logic_error
                 ("basic_string: construction from null is not valid") -> std::terminate -> abort.
                 First user-code frame (gdb): repro_lua_tostring.cpp:52
                   std::basic_string(__s=0x0)   <-- construction from NULL
                 (mirror of  `std::string err = lua_tostring(L, -1);`  at LuaDjotCodec.cpp:550/568)

Root cause:      lua_tostring(L,-1) returns NULL when the Lua error VALUE on the stack is not a
                 string and has no __tostring metamethod (e.g. a raised table). The product code
                 constructs std::string directly from that pointer:
                   if (lua_pcall(...) != LUA_OK) { std::string err = lua_tostring(L, -1); ... }
                 -> std::string(NULL): UB. On this libstdc++ it aborts; on non-hardened libstdc++
                    it is a raw strlen(NULL) SIGSEGV.

Minimized input: min_input.lua  ->  `error(setmetatable({}, {}))`
                 (a non-string error value with no __tostring)
Crash log:       repro_stdout.txt, backtrace.txt, crash_san.txt
Triage bucket:   buckets.json  (severity_hint: LOW null-deref/DoS)

Repro command:
  cd /c/Users/User/Projects/pdf && export PATH="/c/msys64/ucrt64/bin:$PATH"
  bash fuzz/build_clang/build_lua_repro.sh
  ./fuzz/bin/repro_lua_tostring.exe table     # crashes (exit 127)
  ./fuzz/bin/repro_lua_tostring.exe string    # control: survives (lua_tostring non-NULL)

Triage notes (OBSERVATIONS only):
  - Class: denial of service (crash on non-string Lua error). Not a memory-write primitive.
  - Reachability from djotToDocument(untrusted text): the two unsafe sites are on the
    require("djot") path (:550) and the djot.parse(text) path (:568). The reproducer proves
    the *construct* is unsafe; whether djot.lua/Lua-internals can surface a non-string error to
    those exact lua_pcall sites (e.g. LUA_ERRERR during error handling, or a library bug
    raising a table) is a REACHABILITY question for native-adversary. The defensive fix is
    independent of reachability and cheap.
  - Suggested fix direction (NOT applied): guard both sites, e.g.
       const char* m = lua_tostring(L, -1);
       std::string err = m ? m : "<non-string Lua error>";

Environment: g++ 16.1.0 (ucrt64), vendored Lua 5.4 (third_party/lua-5.4), libstdc++ 16.1.0,
             GlyphPDF v1.3.2.2, uid 197609.
