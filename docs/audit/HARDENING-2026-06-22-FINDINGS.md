# GlyphPDF Hardening — 2026-06-22 Findings

## Security fixes applied

Branch: `fuzz/redaction-rig` (one commit ahead of `main`; carries the fuzz rig +
reproductions used to verify F-02 and the Lua crash). All commits local-only.
Build: `cmake --build build`. Suite: `QT_QPA_PLATFORM=offscreen ctest -j4` —
**100% / 39** after every fix (TestLaneScheduler and TestBatchMode are known
parallel-repeat flakes; both pass single-pass).

| # | ID | File | Commit |
|---|----|------|--------|
| 1 | F-02 | `src/engines/podofo/PoDoFoBackend.cpp` | `1972901` |
| 2 | lua_tostring null-deref | `src/pdfws_djot/LuaDjotCodec.cpp` | `1ca69dd` |
| 3 | M-1 (Lua DoS) | `src/pdfws_djot/LuaDjotCodec.cpp` | `1ca69dd` |
| 4 | F-05 | `src/engines/podofo/PoDoFoBackend.cpp` + `tests/TestRedaction.cpp` | `05d303a` |
| 5 | H-1 | `src/engines/VeraPdfValidator.cpp` | `830770e` |
| 6 | M-3 | `src/engines/PatternRedactor.cpp` | `92af05d` |

---

### 1. F-02 — redaction ignored the text-matrix linear part (CONFIRMED leak)

- **File/lines:** `PoDoFoBackend.cpp` `redactCanvasRecursively`
  - text-matrix state added at `:1220` (`RedactCtm tm; RedactCtm tlm; double penX;`)
  - intersection rewritten at `:1272` (`isIntersectingSpan(penStart, penEnd)` + `mapTextToDevice`)
  - matrix maintenance in `BT` / `Tm` (`:1356`) / `Td` / `TD` / `T*` / `'` / `"` handlers
- **What changed:** Previously `Tm a b c d e f` was collapsed to the scalar
  translation (e,f) into `textX`/`textY`; the linear part (a,b,c,d) was discarded
  and the span was mapped through the CTM only — modeling rotated/scaled/skewed
  text as a horizontal span at the wrong device location, so it never intersected
  the redaction rect. Now a full text matrix `Tm` and text-line matrix `Tlm` are
  tracked per PDF 9.4.2, plus a horizontal pen offset; each baseline endpoint maps
  to device as `[t 0 1] × Tm × CTM` (full affine) and the axis-aligned bbox of the
  transformed endpoints is tested.
- **Verification (oracle):** rebuilt driver
  (`fuzz/build_clang/build_redaction_driver.sh`) + `fuzz/run_oracles.sh`:
  - `identity → CLEAN`, `rot90 → CLEAN`, `rot45 → CLEAN`, `scale3x → CLEAN`,
    `skewx → CLEAN` (the four non-identity cases were `LEAK` before the fix; the
    originally-confirmed 90° `VERTLEAK` case now scrubs — no residual `Tj`).
  - ctest 39/39 green.

### 2. lua_tostring null-deref (CONFIRMED crash)

- **File/lines:** `LuaDjotCodec.cpp` `:625` (require path) and `:644` (parse path).
- **What changed:** `std::string err = lua_tostring(L,-1)` → `const char* e =
  lua_tostring(L,-1); std::string err = e ? e : "unknown lua error";` at both
  sites. `lua_tostring` returns NULL for a non-string error value with no
  `__tostring`, and `std::string(NULL)` aborts under hardened libstdc++.
- **Verification:** repro `fuzz/findings/lua_tostring_nullderef/` reproduces the
  old crash (exit 127). A product-linked driver (built ad-hoc, then removed)
  pointed the real `djotToDocument` at a `djot.lua` raising
  `error(setmetatable({},{}))`: it now **throws cleanly** ("failed to require
  djot: unknown lua error") instead of aborting; normal djot still parses. ctest
  djot/codec/roundtrip + full 39/39 green.

### 3. M-1 — Lua djot parse DoS (no instruction/memory cap)

- **File/lines:** `LuaDjotCodec.cpp` — bounded allocator `boundedAlloc` (`:92`),
  state created via `lua_newstate(boundedAlloc, &mem)` (`:594`), instruction-count
  hook installed via `lua_sethook(..., LUA_MASKCOUNT, kHookInterval)` (`:604`).
- **What changed:** A `LUA_MASKCOUNT` hook raises a Lua error once a per-call
  instruction budget is exhausted (surfaced as a normal parse failure via the
  surrounding `lua_pcall`), and a custom `lua_Alloc` enforces a 256 MiB live-bytes
  ceiling so a pathological input cannot hang the VM or OOM the process. The budget
  guard is cleared on every exit path (including exceptions) via a small RAII guard.
- **Verification:** djot round-trip / codec tests pass (normal parse finishes well
  within budget); full 39/39 green.

### 4. F-05 — redaction audit sidecar leaked regions + pre-redaction hash

- **File/lines:** `PoDoFoBackend.cpp` `applyRedactions` — opt-in flag at `:1679`,
  rewritten log block at `:1811`+; test `tests/TestRedaction.cpp::testAuditLogSidecar`.
- **What changed:** The log was written unconditionally to
  `<currentFile>.redaction-log.json` beside the source with exact region
  coordinates and `before_sha256` (a hash of the UN-redacted file) under the
  source's default ACL. Now the audit log is **opt-in and OFF by default**
  (`GLYPHPDF_REDACTION_AUDIT=1`); when enabled it is written to the per-user
  `AppDataLocation` (never beside the source) and records only the region COUNT,
  the after-hash, page, filename and timestamp — **no `before_sha256`, no
  coordinates**. The pre-redaction hash is no longer even computed.
- **Test change (strengthened, not weakened):** `testAuditLogSidecar` previously
  asserted the insecure behavior (sidecar present, contains `before_sha256`); it
  now asserts (a) default-off → no sidecar beside source, (b) opt-in → log in
  app-data with neither `before_sha256` nor `regions`.
- **Verification:** TestRedaction + full 39/39 green.

### 5. H-1 — veraPDF cmd.exe metachar injection

- **File/lines:** `VeraPdfValidator.cpp` `:64`–`:78` (the path-validation block;
  forbidden set at `:72`).
- **What changed:** When the CLI is a `.bat`/`.cmd` it runs via
  `cmd.exe /c call <bat> ... <pdfPath>`, and the old blocklist only rejected
  `& | < >` — missing `%VAR%` expansion, the `^` escape, and `( ) "`. Replaced with
  a **strict allowlist** rejecting any `pdfPath` containing `% ^ " & | < > ( )`.
  exitCode / waitForStarted / 30 s timeout handling unchanged.
- **Verification:** TestVeraPdf + full 39/39 green.

### 6. M-3 — custom-regex ReDoS

- **File/lines:** `PatternRedactor.cpp` `findMatches` `:258`–`:300`
  (`kMaxRegexInput` at `:271`, time guard in the match loop).
- **What changed:** A user-supplied custom regex run via `globalMatch` over full
  page text on the UI path could catastrophically backtrack. Now the input is
  capped at 256 KiB (truncated with a warning) and the match loop aborts after a
  1500 ms wall-clock budget, returning partial results. **Documented residual:** a
  single PCRE2 match step is not interruptible mid-step, so the input cap is what
  bounds that case; a fully cancellable worker thread is out of scope for this
  minimal hardening. Built-in named patterns are linear and unaffected.
- **Verification:** TestPatternRedact + full 39/39 green.

---

All six fixes built clean and left the suite at 100% / 39. No `main` push performed.
