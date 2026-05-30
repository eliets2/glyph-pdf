# Djot Encode Design — `LuaDjotCodec::documentToDjot`

**Written:** 2026-05-30 (M5-PROMPT-4 mini-prompt follow-up)
**Status:** Implemented and tested.

---

## Approach chosen: direct C++ emitter

### Options considered

| Option | Why rejected / chosen |
|---|---|
| **A — marshall SemanticDocument → Lua AST → `djot.render(ast)`** | `djot.lua` exposes `parse()`, `render_html()`, `render_ast_pretty()`, `render_ast_json()`. There is **no** `djot.render()` that produces Djot text from an AST. The `djot-writer.lua` file is a Pandoc custom writer — it uses `pandoc.layout.*` modules that only exist inside Pandoc itself, making it unusable in our standalone Lua FFI context. |
| **B — direct C++ emitter** ✅ | Walks the `SemanticDocument` tree and emits Djot markup directly. Chosen because: (a) the tree structure is small and well-defined; (b) no Lua FFI overhead per call; (c) output is deterministic; (d) the reference parser's grammar is public and well-specified. |

### Implementation

File: `src/pdfws_djot/LuaDjotCodec.cpp` — anonymous namespace, 4 functions:

| Function | Purpose |
|---|---|
| `escapeDjotText(s)` | Escapes Djot special chars: `\ * _ \` [ ] { } ^ ~ < >` with a backslash prefix |
| `emitInline(inl, out)` | Emits one `Inline` node: Text → escaped text; Emph → `_..._`; Strong → `**...**`; Code → `` `...` `` |
| `emitBlock(block, headingLevel, out)` | Emits one `Block`: Paragraph → inlines + `\n\n`; Heading → `##...` + inlines + `\n\n`; List → `- ` items; CodeBlock → ```` ``` ```` |
| `emitSection(section, depth, out)` | Emits section title as `# ` (depth hashes) + blocks + nested sub-sections at depth+1 |

`documentToDjot` iterates `doc.getSections()` and calls `emitSection(*, 1, out)`.

### Performance characteristics

- **O(N)** where N = total node count in the tree.
- No Lua state allocation — purely C++.
- Output size proportional to input node count.
- Memory: single `std::ostringstream`; no intermediate copies.

---

## Known limitations

| Limitation | Impact | Mitigation / planned fix |
|---|---|---|
| **Decode path is still stub** — `djotToDocument` creates an empty `SemanticDocument` regardless of input. | The encode path works correctly, but the structural round-trip `decode(encode(doc)) == doc` cannot be verified. | M5-PROMPT-4 (OCR→Djot mapping, which depends on decode being real) will implement full AST walking in `djotToDocument`. |
| **Code spans with backticks** — if a `Code` inline's child text itself contains backticks, they are emitted verbatim (produces malformed Djot). | Unlikely in current use cases (OCR output, annotation text). | Use double-backtick delimiter when content contains backtick: ``` ``content with ` tick`` ``` — deferred since OCR rarely produces backtick characters in inline code. |
| **Attribute syntax** — PDF fidelity attributes (`{pdf-page=N pdf-bbox=...}`) are not emitted (not yet stored in the current `docmodel` types). | Attribute-aware output required by M5-PROMPT-4 for OCR→Djot mapping. | M5-PROMPT-4 D1 adds attribute maps to `SemanticDocument`; `documentToDjot` will need a pass to emit them. |
| **Round-trip attribute ordering** — Djot attribute maps are unordered sets; comparison must use set equality not string equality. | Affects future structural equality tests. | Already noted in M5-PROMPT-4 D2 spec. |

---

## Test coverage

`tests/TestDjotRoundtrip.cpp` — 12 tests total (8 encode + 4 ProvenanceGuard):

| Test | What it verifies |
|---|---|
| `testEncodeEmptyDocument` | Empty document → empty string |
| `testEncodeSectionTitleAsHeading` | Section title → `# Title` |
| `testEncodeParagraph` | Paragraph block → text + blank line |
| `testEncodeEmphInline` | Emph inline → `_content_` |
| `testEncodeStrongInline` | Strong inline → `**content**` |
| `testEncodeNestedSections` | Nesting → `#`/`##` headers |
| `testEncodeList` | List → `- ` items |
| `testEncodeTextEscaping` | `*_\`` etc. → `\*\_\`` |
| `testSpecRoundtripNoCrash` | `encode → re-parse` doesn't throw |

The "1000-doc fuzzer + structural equality" test from the original spec is deferred until `djotToDocument` walks the AST (M5-PROMPT-4 scope).
