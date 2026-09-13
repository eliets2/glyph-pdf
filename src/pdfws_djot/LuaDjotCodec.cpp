#include "IDjotCodec.h"
#include "DjotBundle.h"
#include <lua.hpp>
#include <stdexcept>

namespace pdfws {

class LuaDjotCodec : public IDjotCodec {
public:
    LuaDjotCodec() {
        L = luaL_newstate();
        if (!L) throw std::runtime_error("Failed to create Lua state");
        luaL_openlibs(L);

        // Preload modules
        auto preload = [](lua_State* L, const char* name, const unsigned char* code, size_t len) {
            lua_getglobal(L, "package");
            lua_getfield(L, -1, "preload");
            
            // Push closure
            lua_pushlstring(L, (const char*)code, len);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                size_t len;
                const char* code = lua_tolstring(L, lua_upvalueindex(1), &len);
                if (luaL_loadbuffer(L, code, len, lua_tostring(L, 1)) != LUA_OK) {
                    return lua_error(L);
                }
                lua_pushvalue(L, 1); // push module name as argument
                lua_call(L, 1, 1);
                return 1;
            }, 1);
            
            lua_setfield(L, -2, name);
            lua_pop(L, 2); // pop package and preload
        };

        preload(L, "djot", src_djot_lua, sizeof(src_djot_lua) - 1);
        preload(L, "djot.ast", src_djot_ast_lua, sizeof(src_djot_ast_lua) - 1);
        preload(L, "djot.attributes", src_djot_attributes_lua, sizeof(src_djot_attributes_lua) - 1);
        preload(L, "djot.block", src_djot_block_lua, sizeof(src_djot_block_lua) - 1);
        preload(L, "djot.filter", src_djot_filter_lua, sizeof(src_djot_filter_lua) - 1);
        preload(L, "djot.html", src_djot_html_lua, sizeof(src_djot_html_lua) - 1);
        preload(L, "djot.inline", src_djot_inline_lua, sizeof(src_djot_inline_lua) - 1);
        preload(L, "djot.json", src_djot_json_lua, sizeof(src_djot_json_lua) - 1);

        // require("djot") so it is cached before we sandbox
        lua_getglobal(L, "require");
        lua_pushstring(L, "djot");
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            lua_close(L);
            throw std::runtime_error("Failed to load djot: " + err);
        }
        lua_setglobal(L, "djot_module"); // save for later

        // Sandbox: disable io, os, loadfile, require, dofile
        const char* to_remove[] = {"io", "os", "loadfile", "require", "dofile", "load", "loadstring"};
        for (const char* g : to_remove) {
            lua_pushnil(L);
            lua_setglobal(L, g);
        }
    }

    ~LuaDjotCodec() override {
        if (L) lua_close(L);
    }

    std::string DecodeToAst(const std::string& djotText) override {
        // We want to call djot.render_ast_json(djot.parse(djotText))
        lua_getglobal(L, "djot_module");
        lua_getfield(L, -1, "parse");
        lua_pushlstring(L, djotText.c_str(), djotText.size());
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            lua_pop(L, 2); // pop error and module
            throw std::runtime_error("djot.parse error: " + err);
        }

        lua_getfield(L, -2, "render_ast_json");
        lua_pushvalue(L, -2); // push the doc ast
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            lua_pop(L, 3); // pop error, doc, module
            throw std::runtime_error("djot.render_ast_json error: " + err);
        }

        std::string result = lua_tostring(L, -1);
        lua_pop(L, 3); // pop result, doc, module
        return result;
    }

private:
    lua_State* L;
};

// Expose factory if needed, or caller can just include and instantiate
} // namespace pdfws

