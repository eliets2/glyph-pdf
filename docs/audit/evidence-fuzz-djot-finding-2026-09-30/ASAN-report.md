# djot libFuzzer finding — heap-buffer-overflow in the codec's lua stack walk

Found BY the CX-13 gate on its first real campaign (INF06 working as
designed): glyphpdf-fuzz workflow_dispatch, run 36651890815 on branch
feat/fixall-ci3, job djot-libfuzzer, 2026-09-30. The 120 s smoke campaign
crashed within seconds of corpus start.

- Harness: fuzz/bin/djot_fuzzer (clang -fsanitize=fuzzer,address,undefined,
  ASan+UBSan) over fuzz/harnesses/harness_djot.cpp -> pdfws::LuaDjotCodec
  (src/pdfws_djot/LuaDjotCodec.cpp) driving third_party/djot (lua).
- Crash input: crash-5d1a4a36f7777001ea587b3c0ce864d311484c40 — 4001 bytes,
  all `>` (0x3E): a 2000-level blockquote-nesting bomb.
- libFuzzer wrote `Test unit written to ./crash-...`; the job uploaded it as
  the djot-crashes artifact (artifact ID 11070374296, run 36651890815).

## ASan report (abridged to the load-bearing frames)

    ==4754==ERROR: AddressSanitizer: heap-buffer-overflow on address
    0x51e000002ed0 ... WRITE of size 8 ... thread T0
        #0 finishrawget third_party/lua-5.4/src/lapi.c:718:5
        #1 pdfws::(anonymous namespace)::pushChild(lua_State*, int, int)
           src/pdfws_djot/LuaDjotCodec.cpp:372:5        (lua_rawgeti)
        #2..#N pdfws::(anonymous namespace)::collectInlineText(lua_State*, int)
           src/pdfws_djot/LuaDjotCodec.cpp:392:20       (UNBOUNDED C recursion)
    0x51e000002ed0 is located 0 bytes after 2640-byte region
    [0x51e000002480,0x51e000002ed0)
    SUMMARY: AddressSanitizer: heap-buffer-overflow
    third_party/lua-5.4/src/lapi.c:718:5 in finishrawget

## Reading (for the owning lane — not fixed here, out of CI-lane scope)

collectInlineText (LuaDjotCodec.cpp:385-395) recurses in C for every level
of the djot tree with NO depth bound; on the nesting bomb the recursion
(pushed through pushChild -> lua_rawgeti -> luaD_growstack/finishrawget)
writes past a 2640-byte heap region — the Lua stack/CallInfo arena — i.e. a
missing stack-space/depth guard in the codec's C walk, not a djot.lua bug
alone. walkInline (LuaDjotCodec.cpp:400+) recurses the same way; the whole
C++ side trusts the parsed tree's shape. Production exposure: the codec runs
on hostile-input-facing surfaces (the save-time dual-write chain, M6-P4 D3);
a deep-nested payload crashes the process.

Minimal owner fix shape (NOT applied by this lane): thread an explicit
depth budget through collectInlineText/walkInline/walkBlocks (or set a
lua_sethook instruction/depth guard for the parse call), refuse beyond the
budget the same way the codec already refuses malformed input, and pin the
budget with the crash input above (ASan-verified on the CI harness — the
local MSYS2 toolchain has no libasan, so local reproduction needs the CI
job or a clang+compiler-rt install).

## Provenance

- CI: https://github.com/eliets2/glyph-pdf/actions/runs/36651890815
  (job djot-libfuzzer, step "Short fuzz run (PR smoke)")
- Branch head at finding time: 9cf06806 (feat/fixall-ci3)
- Found by: FIXALL CI-and-tests lane, CX-13 acceptance dispatch chain,
  2026-09-30. The workflow staying RED until this is fixed is the gate
  behaving correctly (INF06) — do not waive or skip the job.
