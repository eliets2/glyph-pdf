#include "LuaDjotCodec.h"
#include "docmodel/SemanticDocument.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <vector>

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
// Restrict package.path so Lua can only require() from the djot library dir.
// Must be called BEFORE loading djot.lua so submodule requires succeed.
// ---------------------------------------------------------------------------
static void restrictPackagePath(lua_State* L, const std::string& djotLibPath) {
    // Set package.path to only the djot directory so require() can't reach
    // arbitrary filesystem paths. Pattern: `dir/?.lua;dir/djot/?.lua`
    std::string path = djotLibPath + "/?.lua;" + djotLibPath + "/djot/?.lua";
    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_pushstring(L, path.c_str());
        lua_setfield(L, -2, "path");
        lua_pushstring(L, "");  // clear cpath (no C extensions allowed)
        lua_setfield(L, -2, "cpath");
    }
    lua_pop(L, 1);
}

// ---------------------------------------------------------------------------
// Sandbox: remove dangerous globals AFTER djot library is loaded.
// Keep require functional during library loading; remove io/os/debug after.
// ---------------------------------------------------------------------------
static void sandboxLuaState(lua_State* L) {
    // Remove dangerous modules / functions. NOTE: require is kept during
    // library loading but package.path is restricted to the djot directory.
    const char* blocked[] = {
        "io", "os", "loadfile", "dofile", "debug", nullptr
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
    // Restrict package.path BEFORE loading djot.lua so that submodule
    // require() calls work but can only reach the vendored djot directory.
    restrictPackagePath(L, m_djotLibPath);
    sandboxLuaState(L);

    // Load the djot library via require() so it registers in package.loaded
    // and submodule requires work correctly.
    lua_getglobal(L, "require");
    if (!lua_isfunction(L, -1)) {
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: require not available");
    }
    lua_pushstring(L, "djot");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_close(L);
        throw std::runtime_error("LuaDjotCodec: failed to require djot: " + err);
    }
    // Stack top is the djot module table; assign to global for convenience
    lua_pushvalue(L, -1);
    lua_setglobal(L, "djot");

    // Call djot.parse(text) to get AST
    lua_getfield(L, -1, "parse");
    lua_remove(L, -2);  // remove module table, keep parse function
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
    docmodel::Provenance prov;
    auto doc = std::make_unique<docmodel::SemanticDocument>(
        std::vector<std::shared_ptr<docmodel::Section>>{}, prov);
    
    lua_close(L);
    return doc;
}

} // namespace pdfws
