// SPDX-License-Identifier: Apache-2.0
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
        // Djot strong is a single-asterisk pair: `*bold*`. Using `**bold**`
        // here parses back as a *nested* strong (strong>strong), which breaks
        // the inline round-trip — see TestDjotRoundtrip::testStructuralRoundtrip.
        out << '*';
        emitInlines(inl.getChildren(), out);
        out << '*';
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
    case BT::Table: {
        // Djot pipe-table syntax.
        // Structure: ContainerBlock(Table) → rows as ContainerBlock(ListItem),
        //            each row's sub-blocks = cells as TextBlock(Paragraph).
        // First row is treated as the header row; a separator is emitted after it.
        const auto& rows = block.getBlocks();
        for (size_t ri = 0; ri < rows.size(); ++ri) {
            if (!rows[ri]) continue;
            const auto& cells = rows[ri]->getBlocks();
            out << '|';
            if (cells.empty()) {
                // Row with no parsed cells — emit the row's own inlines as a single cell
                out << ' ';
                emitInlines(rows[ri]->getInlines(), out);
                out << " |";
            } else {
                for (const auto& cell : cells) {
                    if (!cell) { out << "  |"; continue; }
                    out << ' ';
                    emitInlines(cell->getInlines(), out);
                    out << " |";
                }
            }
            out << '\n';
            // Emit header separator after the first row
            if (ri == 0) {
                size_t colCount = cells.empty() ? 1 : cells.size();
                out << '|';
                for (size_t ci = 0; ci < colCount; ++ci) {
                    out << "---|";
                }
                out << '\n';
            }
        }
        out << '\n';
        break;
    }
    case BT::Figure: {
        // Figure: emit as a fenced div with figure class, inlines as caption.
        out << "::: figure\n";
        emitInlines(block.getInlines(), out);
        out << "\n:::\n\n";
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

// ===========================================================================
// Djot AST → SemanticDocument walker (decode).
//
// We walk the Lua AST table produced by djot.parse() directly (no JSON
// round-trip). The AST is the structure documented in third_party/djot/djot/
// ast.lua: a top-level "doc" node whose children are either "section" nodes
// (produced by add_sections() wrapping headings) or loose block nodes.
//
// Node access helpers: a node table has fields t (tag, string), s (text,
// string), c (children, array). We read them by raw key so the ast.lua
// metatable aliases don't matter.
// ---------------------------------------------------------------------------

// Push node.<field> (a raw table field) onto the stack. Returns the Lua type.
static int pushField(lua_State* L, int nodeIdx, const char* field)
{
    lua_pushstring(L, field);
    lua_rawget(L, nodeIdx < 0 ? nodeIdx - 1 : nodeIdx);
    return lua_type(L, -1);
}

// Read node.t (tag) as std::string ("" if absent). Leaves stack unchanged.
static std::string nodeTag(lua_State* L, int nodeIdx)
{
    std::string tag;
    if (pushField(L, nodeIdx, "t") == LUA_TSTRING) {
        size_t len = 0;
        const char* p = lua_tolstring(L, -1, &len);
        tag.assign(p, len);
    }
    lua_pop(L, 1);
    return tag;
}

// Read node.s (text) as std::string ("" if absent). Leaves stack unchanged.
static std::string nodeText(lua_State* L, int nodeIdx)
{
    std::string s;
    if (pushField(L, nodeIdx, "s") == LUA_TSTRING) {
        size_t len = 0;
        const char* p = lua_tolstring(L, -1, &len);
        s.assign(p, len);
    }
    lua_pop(L, 1);
    return s;
}

// Number of children in node.c (0 if absent).
static int childCount(lua_State* L, int nodeIdx)
{
    int n = 0;
    if (pushField(L, nodeIdx, "c") == LUA_TTABLE) {
        n = static_cast<int>(lua_rawlen(L, -1));
    }
    lua_pop(L, 1);
    return n;
}

// Push node.c[i] (1-based) onto the stack. Caller must pop. Returns false if
// node has no children table.
static bool pushChild(lua_State* L, int nodeIdx, int i)
{
    if (pushField(L, nodeIdx, "c") != LUA_TTABLE) {
        lua_pop(L, 1);
        return false;
    }
    lua_rawgeti(L, -1, i);   // push c[i]
    lua_remove(L, -2);       // remove the c table, leave c[i] on top
    return true;
}

static docmodel::Provenance djotProv()
{
    docmodel::Provenance p;
    p.tag = docmodel::ProvenanceTag::BornDjot;
    return p;
}

// Collect the concatenated plain text of an inline subtree (for verbatim/code).
static std::string collectInlineText(lua_State* L, int nodeIdx)
{
    std::string out = nodeText(L, nodeIdx);
    if (!out.empty()) return out;
    const int n = childCount(L, nodeIdx);
    for (int i = 1; i <= n; ++i) {
        if (pushChild(L, nodeIdx, i)) {
            out += collectInlineText(L, -1);
            lua_pop(L, 1);
        }
    }
    return out;
}

// Walk an inline node (top of relative index nodeIdx) → docmodel::Inline.
static std::shared_ptr<docmodel::Inline> walkInline(lua_State* L, int nodeIdx)
{
    const std::string tag = nodeTag(L, nodeIdx);

    if (tag == "str") {
        return std::make_shared<docmodel::TextInline>(nodeText(L, nodeIdx), djotProv());
    }
    if (tag == "softbreak" || tag == "hardbreak") {
        return std::make_shared<docmodel::TextInline>(std::string{" "}, djotProv());
    }
    if (tag == "verbatim" || tag == "code") {
        // Code span: content is literal text. Represent as a Code inline whose
        // single child carries the literal text (mirrors the emitter, which
        // reads getChildren()[i]->getText() for Code spans).
        std::vector<std::shared_ptr<docmodel::Inline>> kids;
        kids.push_back(std::make_shared<docmodel::TextInline>(
            collectInlineText(L, nodeIdx), djotProv()));
        return std::make_shared<docmodel::ContainerInline>(
            docmodel::Inline::Type::Code, std::move(kids), djotProv());
    }
    if (tag == "emph" || tag == "strong") {
        std::vector<std::shared_ptr<docmodel::Inline>> kids;
        const int n = childCount(L, nodeIdx);
        for (int i = 1; i <= n; ++i) {
            if (pushChild(L, nodeIdx, i)) {
                if (auto inl = walkInline(L, -1)) kids.push_back(std::move(inl));
                lua_pop(L, 1);
            }
        }
        return std::make_shared<docmodel::ContainerInline>(
            tag == "emph" ? docmodel::Inline::Type::Emph
                          : docmodel::Inline::Type::Strong,
            std::move(kids), djotProv());
    }

    // Unknown / unsupported inline: fall back to its plain text so content is
    // never silently dropped.
    const std::string fallback = collectInlineText(L, nodeIdx);
    if (!fallback.empty()) {
        return std::make_shared<docmodel::TextInline>(fallback, djotProv());
    }
    return nullptr;
}

// Walk the inline children of a block-ish node into an inline vector.
static std::vector<std::shared_ptr<docmodel::Inline>>
walkInlineChildren(lua_State* L, int nodeIdx)
{
    std::vector<std::shared_ptr<docmodel::Inline>> inlines;
    const int n = childCount(L, nodeIdx);
    for (int i = 1; i <= n; ++i) {
        if (pushChild(L, nodeIdx, i)) {
            if (auto inl = walkInline(L, -1)) inlines.push_back(std::move(inl));
            lua_pop(L, 1);
        }
    }
    return inlines;
}

// Forward decl for list-item recursion.
static std::shared_ptr<docmodel::Block> walkBlock(lua_State* L, int nodeIdx);

// Walk a "list" node → ContainerBlock(List) of ListItem TextBlocks.
static std::shared_ptr<docmodel::Block> walkList(lua_State* L, int nodeIdx)
{
    std::vector<std::shared_ptr<docmodel::Block>> items;
    const int n = childCount(L, nodeIdx);
    for (int i = 1; i <= n; ++i) {
        if (!pushChild(L, nodeIdx, i)) continue;
        if (nodeTag(L, -1) == "list_item") {
            // A list item's inline content is the concatenation of the inlines
            // of its child paragraph(s) — the emitter writes "- <inlines>".
            std::vector<std::shared_ptr<docmodel::Inline>> inlines;
            const int ic = childCount(L, -1);
            for (int j = 1; j <= ic; ++j) {
                if (pushChild(L, -1, j)) {
                    const std::string ctag = nodeTag(L, -1);
                    if (ctag == "para" || ctag == "heading") {
                        auto sub = walkInlineChildren(L, -1);
                        for (auto& s : sub) inlines.push_back(std::move(s));
                    }
                    lua_pop(L, 1);
                }
            }
            items.push_back(std::make_shared<docmodel::TextBlock>(
                docmodel::Block::Type::ListItem, std::move(inlines), djotProv()));
        }
        lua_pop(L, 1);
    }
    return std::make_shared<docmodel::ContainerBlock>(
        docmodel::Block::Type::List, std::move(items), djotProv());
}

// Walk a block node (top of stack at nodeIdx) → docmodel::Block (or nullptr).
static std::shared_ptr<docmodel::Block> walkBlock(lua_State* L, int nodeIdx)
{
    const std::string tag = nodeTag(L, nodeIdx);

    if (tag == "para") {
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, walkInlineChildren(L, nodeIdx), djotProv());
    }
    if (tag == "heading") {
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Heading, walkInlineChildren(L, nodeIdx), djotProv());
    }
    if (tag == "list") {
        return walkList(L, nodeIdx);
    }
    if (tag == "code_block" || tag == "raw_block") {
        // Code block content is stored in node.s (one string). Mirror the
        // emitter's representation: a CodeBlock ContainerBlock whose child
        // paragraphs are the lines.
        const std::string content = nodeText(L, nodeIdx);
        std::vector<std::shared_ptr<docmodel::Block>> lines;
        std::string line;
        std::istringstream iss(content);
        while (std::getline(iss, line)) {
            std::vector<std::shared_ptr<docmodel::Inline>> inl;
            inl.push_back(std::make_shared<docmodel::TextInline>(line, djotProv()));
            lines.push_back(std::make_shared<docmodel::TextBlock>(
                docmodel::Block::Type::Paragraph, std::move(inl), djotProv()));
        }
        return std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::CodeBlock, std::move(lines), djotProv());
    }
    if (tag == "blockquote" || tag == "div") {
        // Container of blocks: flatten its block children into a paragraph-less
        // pass-through is lossy, so represent as a List-less ContainerBlock is
        // not in the model. Fall back to a paragraph holding the text.
        std::vector<std::shared_ptr<docmodel::Inline>> inl;
        const std::string t = collectInlineText(L, nodeIdx);
        if (!t.empty()) inl.push_back(std::make_shared<docmodel::TextInline>(t, djotProv()));
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inl), djotProv());
    }

    // thematic_break and other block tags carry no SemanticDocument
    // representation — skip them (caller treats nullptr as "no block").
    return nullptr;
}

// Walk a "section" node → docmodel::Section. The section's first child is the
// heading (its title); remaining children are blocks or nested sections.
static std::shared_ptr<docmodel::Section> walkSection(lua_State* L, int nodeIdx)
{
    std::string title;
    std::vector<std::shared_ptr<docmodel::Block>> blocks;
    std::vector<std::shared_ptr<docmodel::Section>> subs;

    const int n = childCount(L, nodeIdx);
    for (int i = 1; i <= n; ++i) {
        if (!pushChild(L, nodeIdx, i)) continue;
        const std::string ctag = nodeTag(L, -1);
        if (i == 1 && ctag == "heading") {
            // Section heading becomes the section title.
            title = collectInlineText(L, -1);
        } else if (ctag == "section") {
            subs.push_back(walkSection(L, -1));
        } else {
            if (auto blk = walkBlock(L, -1)) blocks.push_back(std::move(blk));
        }
        lua_pop(L, 1);
    }
    return std::make_shared<docmodel::Section>(
        std::move(title), std::move(blocks), std::move(subs), djotProv());
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

    // Stack top is the parsed "doc" AST node. Walk its children into a
    // SemanticDocument. Top-level children are "section" nodes (produced by
    // ast.lua add_sections wrapping headings) and/or loose blocks. Loose
    // blocks that precede any heading are gathered into an implicit untitled
    // leading section so no content is dropped.
    std::vector<std::shared_ptr<docmodel::Section>> sections;
    std::vector<std::shared_ptr<docmodel::Block>> leadingBlocks;

    const int docIdx = lua_gettop(L);
    const int n = childCount(L, docIdx);
    for (int i = 1; i <= n; ++i) {
        if (!pushChild(L, docIdx, i)) continue;
        const std::string ctag = nodeTag(L, -1);
        if (ctag == "section") {
            sections.push_back(walkSection(L, -1));
        } else {
            if (auto blk = walkBlock(L, -1)) leadingBlocks.push_back(std::move(blk));
        }
        lua_pop(L, 1);
    }

    if (!leadingBlocks.empty()) {
        // Wrap loose pre-heading blocks in an untitled section, preserving order
        // ahead of any titled sections.
        sections.insert(sections.begin(), std::make_shared<docmodel::Section>(
            std::string{}, std::move(leadingBlocks),
            std::vector<std::shared_ptr<docmodel::Section>>{}, djotProv()));
    }

    auto doc = std::make_unique<docmodel::SemanticDocument>(
        std::move(sections), djotProv());

    lua_close(L);
    return doc;
}

} // namespace pdfws
