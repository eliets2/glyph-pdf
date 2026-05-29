#include "LuaDjotCodec.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <stdexcept>
#include <fstream>
#include <sstream>

namespace pdfws {

// ---------------------------------------------------------------------------
// Helper: read an entire file into a string
// ---------------------------------------------------------------------------
static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        throw std::runtime_error("LuaDjotCodec: cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ---------------------------------------------------------------------------
// Sandbox: remove dangerous globals before running any user code
// ---------------------------------------------------------------------------
static void sandboxLuaState(lua_State* L) {
    // Remove dangerous modules / functions
    const char* blocked[] = {
        "io", "os", "loadfile", "dofile", "require",
        "debug", "package", nullptr
    };
    for (int i = 0; blocked[i]; ++i) {
        lua_pushnil(L);
        lua_setglobal(L, blocked[i]);
    }
}

// ---------------------------------------------------------------------------
// LuaDjotCodec implementation
// ---------------------------------------------------------------------------

LuaDjotCodec::LuaDjotCodec(const std::string& djotLibPath)
    : m_djotLibPath(djotLibPath)
{
}

LuaDjotCodec::~LuaDjotCodec() = default;

std::string LuaDjotCodec::documentToDjot(const docmodel::SemanticDocument& /*doc*/) {
    // TODO: implement once docmodel → djot serialisation is written
    return {};
}

std::unique_ptr<docmodel::SemanticDocument> LuaDjotCodec::djotToDocument(const std::string& djotText) {
    // Create a fresh Lua state for each parse (simple & safe)
    lua_State* L = luaL_newstate();
    if (!L)
        throw std::runtime_error("LuaDjotCodec: could not create Lua state");

    luaL_openlibs(L);
    sandboxLuaState(L);

    // Load the djot.lua library
    std::string djotLuaSrc = readFile(m_djotLibPath + "/djot.lua");
    if (luaL_dostring(L, djotLuaSrc.c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: failed to load djot.lua: " + err);
    }

    // Call djot.parse(text) to get AST
    lua_getglobal(L, "djot");
    if (!lua_istable(L, -1)) {
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: djot global is not a table after loading");
    }

    lua_getfield(L, -1, "parse");
    if (!lua_isfunction(L, -1)) {
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: djot.parse is not a function");
    }

    lua_pushstring(L, djotText.c_str());
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: djot.parse failed: " + err);
    }

    // For now, produce an empty document. Full AST walking will be
    // implemented once the docmodel types are stable.
    auto doc = std::make_unique<docmodel::SemanticDocument>();
    
    lua_close(L);
    return doc;
}

} // namespace pdfws
