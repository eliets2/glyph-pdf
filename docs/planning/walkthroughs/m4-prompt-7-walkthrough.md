# M4-PROMPT-7 Walkthrough — WS2 Djot Interchange Foundation (Phase 1.5)

**Date:** 2026-05-30
**Result:** Foundation shipped; encode path deferred (known stub)
**Commits:** ae84ca7 (docmodel) · 2e8aacd (vendor Lua+Djot) · 9bb410c (pdfws_djot wiring) · bc00e6a (TestDjotRoundtrip)

---

## COMPLETED

### D1 — docmodel library
- `docmodel` static library at `src/docmodel/`: `SemanticDocument`, `Section`, `Block`, `TextBlock`, `ContainerBlock`, `Inline`, `TextInline`, `ProvenanceTag`, `Provenance`, `Node`.
- Represents the semantic dual-model layer (Structural PDF object graph ↔ Semantic `SemanticDocument` tree).
- Committed in ae84ca7.

### D2/D4 — Vendor Lua 5.4 + Djot reference parser
- Lua 5.4 (MIT) at `third_party/lua-5.4/` — source-only, `liblua` STATIC target in `third_party/CMakeLists.txt` (excludes interpreter + compiler mains: `lua.c`, `luac.c`).
- Djot reference parser at `third_party/djot/` — Lua files only (stripped `.git` per vendoring convention, NOT a git submodule).
- **Non-negotiable: no reimplementation of Djot grammar** — must use the reference parser (architectural constraint in `06-non-negotiables.md`).
- Committed in 2e8aacd.

### D3 — pdfws_djot library
- `pdfws_djot` static library at `src/pdfws_djot/`:
  - `IDjotCodec` — `documentToDjot()` + `djotToDocument()` interface
  - `IDjotMapper` — `pdfToDocument()` + `documentToPdf()` interface
  - `LuaDjotCodec` — implements `djotToDocument()` (decode path); `documentToDjot()` stubbed (see Known Issue)
  - `PdfStructureMapper` — implements `IDjotMapper` (structural PDF ↔ docmodel mapping)
  - `ProvenanceGuard` — enforces the hard constraint: DjotThenSave on a signed BornPDF throws `ProvenanceViolation`

### D6 — AppContext wiring
- `AppContext.h` adds `djotCodec`, `djotMapper`, `provenanceGuard` shared_ptr fields.
- `Bootstrapper.cpp` creates `LuaDjotCodec(djotPath)`, `PdfStructureMapper()`, `ProvenanceGuard()` at app startup.

### D5 — TestDjotRoundtrip (6 tests)
- Rewritten from gtest to QtTest (gtest not in MSYS2 ucrt64 packages).
- Tests:
  1. `testDecodeSimpleDjot` — parse `# Hello\n\nA paragraph.` → non-null document
  2. `testDecodeEmptyDjot` — parse `""` → no throw
  3. `testEncodeIsStubbed` — `documentToDjot(emptyDoc)` returns `""` (documents the known stub)
  4. `testProvenanceGuardThrowsForSignedBornPdf` — signed BornPDF + DjotThenSave → ProvenanceViolation
  5. `testProvenanceGuardWarnForUnsignedBornPdf` — unsigned BornPDF → allowed with warning
  6. `testProvenanceGuardAllowsSaveAsCopy` — signed BornPDF + DjotThenSaveAsCopy → allowed
  7. `testProvenanceGuardAllowsBornDjot` — BornDjot + DjotThenSave → allowed
- Registered in root CMakeLists.txt (links: pdfws_djot, docmodel, Qt6::Test).

---

## DEFERRED

| Item | Reason | Target |
|------|--------|--------|
| `LuaDjotCodec::documentToDjot` encode path | Substantial engineering (walk SemanticDocument tree, emit Djot text); deferred per D2 option (b) | Pre-M5-P4 |
| 1000-doc roundtrip property test | Encode must work first; SKIPPED until encode lands | Same |

---

## Known Issues

- `LuaDjotCodec::documentToDjot` is stubbed at `LuaDjotCodec.cpp:54` (returns `{}`). **M5-PROMPT-4 (OCR→Djot mapping) is blocked until this stub is implemented.** Tracked in CHANGELOG Known Issues.

---

## CHANGELOG confirmation

New "Djot Interchange Foundation (M4-PROMPT-7)" section added to CHANGELOG.md (d5143eb). Known Issue for encode stub added.

---

## Patterns discovered / notes

1. **ProvenanceGuard.h inline redefinition fixed**: The original ProvenanceGuard.h redefined `docmodel::ProvenanceTag` inline ("until docmodel is merged"). Now that docmodel IS merged (ae84ca7), the inline was replaced with `#include "docmodel/ProvenanceTag.h"` to prevent ODR violations when both headers are included in the same TU.
2. **gtest → QtTest rewrite**: Original TestDjotRoundtrip.cpp used Google Test which is not in MSYS2 ucrt64. Rewritten to QtTest for consistency with all other GlyphPDF test targets.
3. **Djot .git stripped**: The vendored `third_party/djot/` had its `.git` directory removed before committing (was staged as embedded git repo — corrected with `git rm --cached -f`).

---

## Follow-up: encode path closure (2026-05-30, mini-prompt)

`LuaDjotCodec::documentToDjot` was implemented as a **direct C++ tree-walker** (not via `djot.render`). The vendored `djot.lua` exposes only `parse()`, `render_html()`, `render_ast_*()` — there is no `djot.render(ast)` function that produces Djot text from an AST. The `djot-writer.lua` is a Pandoc-specific module and cannot be used standalone.

**Commits:** `d90eda2` (encode C++ emitter) · `0149733` (TestDjotRoundtrip 8 new tests)

**Design doc:** `docs/djot-encode-design.md`

**Remaining:** `djotToDocument` still creates an empty `SemanticDocument` (decode stub). Full AST walking is M5-PROMPT-4 scope.
