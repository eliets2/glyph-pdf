# evidence-djot-depth-budget — R7 package for the CX-13 djot fix

Lane: engine-fix (feat/djot-depth-budget, base = main @ 5b4989fc).
Finding: docs/audit/evidence-fuzz-djot-finding-2026-09-30/ — heap-buffer-overflow
WRITE past the lua stack region in `finishrawget` (lapi.c:718), reached via
`pushChild` under `collectInlineText`'s unbounded C recursion in
LuaDjotCodec.cpp; `walkInline` recursed the same way.

## Fix shape (as applied)

1. **Depth budget** `kMaxAstDepth = 256`, threaded through every recursive
   descent in LuaDjotCodec.cpp — decode walk (`collectInlineText`,
   `walkInline`, `walkInlineChildren`, `walkList`, `walkBlock`,
   `walkSection` — i.e. blockquote/div containment, emphasis/strong nesting,
   list items, section nesting) AND the encode emitter (`emitInline`,
   `emitInlines`, `emitSection`). Beyond the budget the codec REFUSES with
   the honest, specific error
   `djot: nesting deeper than the supported budget (256)` — fail-closed at
   the codec boundary (djotToDocument/documentToDjot throw; callers get a
   decode failure, never a partial document, never silent truncation).
   A LUAI_MAXSTACK/C-call "stack overflow" error surfaced by djot.parse
   under its pcall is mapped to the same refusal path.
   **Budget rationale:** 256 is far above any real-world djot document
   (real block/heading/list nesting is single digits to low tens; djot
   heading levels cap at 6, so section nesting from the parser is ≤ 6) and
   bounds the C recursion to a few hundred small frames (~100 KB host stack)
   and the Lua-stack creep to ~256 slots — trivially inside LUAI_MAXSTACK.
   The fuzz bomb sits at 2000+ levels: three orders of magnitude past
   legitimate content.
2. **Per-level `lua_checkstack` reservation** (`kWalkSlotsPerLevel = 8` at
   every recursion level). This is load-bearing, not gold-plating: the Lua C
   API does not grow the stack implicitly — `lua_rawgeti`/`lua_rawget` push
   via `api_incr_top`, whose overflow check compiles out in release builds —
   and a one-shot up-front reservation can evaporate because the GC shrinks
   stacks larger than ~3x their in-use window (`luaD_shrinkstack`, called
   from lgc.c:644). Reserving per level re-establishes headroom as the walk
   descends. Budget alone would only have lowered the crash threshold.
3. **RAII `lua_close` guard** in `djotToDocument` so the refusal path (and
   any other throw) cannot leak the lua_State.

Wrapper-level only; no vendored (lua/djot) source touched.

## Crash-input note (correction to the finding's abridged description)

The finding doc describes the artifact as "4001 bytes, all `>`". The
preserved artifact's actual bytes are `"> "` (marker + space) repeated 2000
times plus a final `x` — `od -c` shows `>   >   > ... x`. SHA-1 of the
constructed `"> " * 2000 + "x"` is `5d1a4a36f7777001ea587b3c0ce864d311484c40`,
byte-identical to the artifact (libFuzzer names crash units by input SHA-1).
The pin uses this exact construction. A bare `>`-run parses as one flat
paragraph of literal text (shallow, legal) — the pin asserts it still decodes
(no-false-refusal guard).

## Evidence — per R7 pin

The R7 pins live in tests/TestDjotFuzz.cpp (TestDjotFuzz suite, serial):

- **(a) `testDepthBudgetRefusalOnNestingBomb`** — feeds the byte-exact CX-13
  crash input (plus a deep-emphasis bomb for the `walkInline` vector) through
  the public `djotToDocument` API; asserts the canonical refusal message,
  bounded time (<30 s wall; observed ~10 ms), no crash.
- **(b) `testShallowMixedNestingGolden`** — a 40-level mixed
  blockquote/list/emphasis document decodes to a canonical fingerprint
  (`doc{S(;b0(0:deep strong text- list emph item;))}`) captured on base
  5b4989fc pre-change; proves shallow nesting still decodes byte-identically.
- **(c) `testEncodeRefusesOverDeepNesting`** — synthetic 300-deep section
  chain and 300-deep emphasis chain through the public `documentToDjot` API
  must refuse with the same error.

| Check | Result | Log |
|---|---|---|
| Base build (pre-change) | 3/3 djot suites green (TestOcrDjotMapper, TestDjotRoundtrip, TestDjotFuzz) | (baseline run, 2026-09-30) |
| Fail-before pin (a), final pin vs base codec | **CRASH — access violation 0xc0000005** on the exact CX-13 input, reproduced locally without ASan | `fail-before-pin-a-crash.log` |
| Fail-before pin (b) on base | PASS (golden captured here, locked to base behavior) | — |
| Fail-before pin (c) on base | **FAIL** — encode of 300-deep chain succeeds, no refusal | `fail-before-pin-c-encode.log` |
| NC pin (a), budget check disabled (scoped `depth > 1<<30` edit, checkstack kept) | **FAIL ×2 vectors** — "decode must refuse", decodes cleanly in 25 ms | `nc-budget-disabled-pin-a.log` |
| NC pin (c), budget disabled | **FAIL** — no refusal | `nc-budget-disabled-pin-c.log` |
| Pass-after | **4/4 suites green, 3 consecutive serial runs** (TestDjotFuzz, TestDjotRoundtrip, TestOcrDjotMapper, TestAnnotationDjot) | `pass-after-run1.log`, `pass-after-run2.log`, `pass-after-run3.log` |

The NC also demonstrates the two halves of the fix are independent: with the
budget disabled but the per-level checkstack kept, the previously-crashing
2000-deep walk completes safely — the memory-safety half — while the pin
still fails because the refusal is missing — the contract half.

Fail-before runs used a scoped revert (`git checkout 5b4989fc --
src/pdfws_djot/LuaDjotCodec.cpp`) with the final pins in place; the NC used a
scoped two-site edit of the budget comparison, both restored and re-verified
green afterwards. Local toolchain is g++/UCRT64 without libasan; the ASan
confirmation remains the CI fuzz job's to make (it must go GREEN again on
this fix — the CX-13 job stays unwaived).

## Honesty notes

- The fail-before crash (0xc0000005) is the local NO-ASan reproduction; the
  CI failure mode was the ASan heap-buffer-overflow in finishrawget. Same
  root cause (unchecked Lua-stack creep under unbounded C recursion), two
  observers.
- `walkList`'s nested-list content drop and other pre-existing decode
  fidelity gaps are NOT addressed here (out of lane scope; behavior
  unchanged by this fix — pin (b)'s golden pins the flattened output).
- DjotToRichTextXhtml.cpp was audited for recursion: it is a line-based
  parser with no unbounded self-recursion over hostile trees; no change.
- No vendor (third_party) files modified. No CLAUDE.md/SECURITY.md added.
