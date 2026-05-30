#include "LuaDjotCodec.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <stdexcept>
#include <sstream>
#include <vector>

namespace pdfws {

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
// Djot emitter — C++ tree-walker producing Djot syntax from SemanticDocument.
//
// Design rationale: the vendored djot.lua reference parser (djot.lua) exposes
// only parse() + render_html/render_ast functions.  There is no djot.render()
// that converts a Lua AST table back to Djot text.  The djot-writer.lua is a
// pandoc-specific writer (requires pandoc.layout) and cannot be used
// standalone.  Therefore encode is implemented as a direct C++ emitter that
// walks the SemanticDocument tree, as recommended in the error-recovery
// section of M5-PROMPT-4 (mini-prompt follow-up).
// ---------------------------------------------------------------------------

namespace {

// Escape Djot special characters inside plain text spans.
// In Djot, the following characters begin markup when they appear inline:
// \ * _ ` [ ] { } ^ ~ < >
// A backslash before any of them suppresses the special meaning.
static std::string escapeDjotText(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\\': case '*': case '_': case '`':
        case '[':  case ']': case '{': case '}':
        case '^':  case '~': case '<': case '>':
            out += '\\';
            break;
        default:
            break;
        }
        out += static_cast<char>(c);
    }
    return out;
}

static void emitInlines(const std::vector<std::shared_ptr<docmodel::Inline>>& inlines,
                        std::ostringstream& out);

static void emitInline(const docmodel::Inline& inl, std::ostringstream& out)
{
    using T = docmodel::Inline::Type;
    switch (inl.getType()) {
    case T::Text:
        out << escapeDjotText(inl.getText());
        break;
    case T::Emph:
        out << '_';
        emitInlines(inl.getChildren(), out);
        out << '_';
        break;
    case T::Strong:
        out << "**";
        emitInlines(inl.getChildren(), out);
        out << "**";
        break;
    case T::Code:
        out << '`';
        // Code spans: content is literal; backtick in content is unsupported
        // in this minimal emitter (no multi-backtick delimiters).
        for (const auto& ch : inl.getChildren()) {
            out << ch->getText();
        }
        out << '`';
        break;
    }
}

static void emitInlines(const std::vector<std::shared_ptr<docmodel::Inline>>& inlines,
                        std::ostringstream& out)
{
    for (const auto& inl : inlines) {
        if (inl) emitInline(*inl, out);
    }
}

static void emitBlock(const docmodel::Block& block, int headingLevel,
                      std::ostringstream& out)
{
    using BT = docmodel::Block::Type;
    switch (block.getType()) {
    case BT::Paragraph:
        emitInlines(block.getInlines(), out);
        out << "\n\n";
        break;
    case BT::Heading: {
        // Heading blocks inside a section inherit level from call site.
        const int lvl = headingLevel > 0 ? headingLevel : 1;
        out << std::string(static_cast<size_t>(lvl), '#') << ' ';
        emitInlines(block.getInlines(), out);
        out << "\n\n";
        break;
    }
    case BT::List:
        for (const auto& item : block.getBlocks()) {
            if (!item) continue;
            out << "- ";
            if (item->getType() == BT::ListItem) {
                emitInlines(item->getInlines(), out);
            } else {
                // Fallback: treat child block as paragraph content
                emitInlines(item->getInlines(), out);
            }
            out << '\n';
        }
        out << '\n';
        break;
    case BT::ListItem:
        emitInlines(block.getInlines(), out);
        out << '\n';
        break;
    case BT::CodeBlock: {
        out << "```\n";
        // Code block content: child blocks are treated as lines
        for (const auto& child : block.getBlocks()) {
            if (!child) continue;
            emitInlines(child->getInlines(), out);
            out << '\n';
        }
        out << "```\n\n";
        break;
    }
    }
}

static void emitSection(const docmodel::Section& section, int depth,
                        std::ostringstream& out)
{
    // Emit section title as a heading (depth ≥ 1).
    if (!section.getTitle().empty()) {
        out << std::string(static_cast<size_t>(depth), '#') << ' '
            << escapeDjotText(section.getTitle()) << "\n\n";
    }
    // Emit top-level blocks at depth+1 so any Heading blocks they contain
    // are subordinate to the section heading.
    for (const auto& block : section.getBlocks()) {
        if (block) emitBlock(*block, depth + 1, out);
    }
    // Emit nested sections at depth+1
    for (const auto& sub : section.getSubsections()) {
        if (sub) emitSection(*sub, depth + 1, out);
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// LuaDjotCodec implementation
// ---------------------------------------------------------------------------

LuaDjotCodec::LuaDjotCodec(const std::string& djotLibPath)
    : m_djotLibPath(djotLibPath)
{
}

LuaDjotCodec::~LuaDjotCodec() = default;

std::string LuaDjotCodec::documentToDjot(const docmodel::SemanticDocument& doc)
{
    std::ostringstream out;
    for (const auto& section : doc.getSections()) {
        if (section) emitSection(*section, 1, out);
    }
    return out.str();
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
