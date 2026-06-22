# GlyphPDF Fuzzing / Verification Rig — CAMPAIGN MANIFEST

Built by `fuzz-harness-engineer`. This file is the rig's ground truth. Every
number below comes from a tool that actually ran and wrote a cited file.
Exploitability verdicts and Finding promotion belong to `native-adversary`;
this rig produces reproducible Evidence only.

Repo: `C:\Users\User\Projects\pdf`  (GlyphPDF v1.3.2.2, MSYS2 ucrt64, C++17/Qt6/PoDoFo 1.1/PDFium)
Date: 2026-06-22

## ENVIRONMENT — what is actually present (probed, not assumed)

| Tool | Status |
|------|--------|
| g++ (ucrt64) 16.1.0 | PRESENT — workhorse compiler |
| clang / clang++ | **ABSENT** (installable: `mingw-w64-ucrt-x86_64-clang`) |
| libFuzzer / compiler-rt | **ABSENT** (ships with the clang pkg) |
| AFL++ / honggfuzz | **ABSENT** |
| ASan/UBSan runtime | **ABSENT** in ucrt64 (no `-lasan`); arrives with clang/compiler-rt |
| qpdf | PRESENT 12.3.2 — used as canonicalizer for the oracles |
| mutool | ABSENT (qpdf substitutes) |
| python | PRESENT 3.14 |
| CASR | ABSENT (Linux/SYS_PTRACE; `triage_dedup.py` is the portable stand-in) |

Consequence: the **differential/property oracles run TODAY with g++ + qpdf**;
the **libFuzzer harness is scaffolded** with an exact clang build recipe behind a
gated toolchain install.

## SANDBOX CHECK

- non-root: **PASS** (uid 197609)
- docker.sock absent: **PASS** (`/var/run/docker.sock` not present)
- writes scratch/fuzz-only: **PASS** (no production `src/` edits; main `build/` untouched)
- resource limits: **N/A** (native MSYS2, not a container) — long campaigns should
  use `-rss_limit_mb` / `-timeout` (libFuzzer) or `-m`/`-t` (AFL++) when enabled.

## TARGETS

| Harness | Backend / facet | Build | Status |
|---------|-----------------|-------|--------|
| `harnesses/redaction_driver.cpp` -> `bin/redaction_driver.exe` | PoDoFoBackend via PdfEditorEngine: applyRedactions + Save/writeUpdate | g++, links built `build/lib*.a` | **BUILT + RAN** |
| `harnesses/repro_lua_tostring.cpp` -> `bin/repro_lua_tostring.exe` | vendored Lua 5.4 (mirrors LuaDjotCodec.cpp:550/568) | g++ + `libliblua.a` | **BUILT + RAN — CRASH REPRODUCED** |
| `harnesses/harness_djot.cpp` -> `bin/djot_fuzzer` | LuaDjotCodec::djotToDocument (libFuzzer) | clang+libFuzzer+ASan/UBSan | **SCAFFOLDED** (needs clang; recipe in `build_clang/build_djot_clang.sh`) |

## BUILD MATRIX

| Sanitizer build | Status | Note |
|-----------------|--------|------|
| g++ baseline (no san) | CLEAN | redaction_driver + lua repro both link & run |
| libFuzzer + ASan + UBSan | DEFERRED | clang+compiler-rt not installed; gated `pacman -S` |
| MSan | DEFERRED | needs whole-chain instrumented libc++ + PoDoFo/PDFium; out of scope this pass |
| TSan / CFI | DEFERRED | low priority (single-threaded parse path) |

Hand-link recipes (reproducible): `build_clang/build_redaction_driver.sh`,
`build_clang/build_lua_repro.sh`, `build_clang/build_djot_clang.sh`.

## CORPUS

- `corpus/pdf/` — 2 seeds (identity-Tm + rotated-Tm text), uncompressed.
- `corpus/djot/` — 6 seeds (valid, deep blockquote, unbalanced emph, lone bracket, NUL bytes).
- Dicts: `dict/pdf.dict` (PDF + content-stream operators), `dict/djot.dict`.
- Distillation (`afl-cmin`/`afl-tmin`): N/A until AFL++ present; documented for CI.

## ORACLE

- Backends compared: single-backend property oracles (qpdf as canonicalizer).
  Cross-backend differential (PoDoFo vs PDFium vs qpdf) is scaffolded conceptually
  but the three-way diff is NOT YET wired (next pass).
- Benign-divergence baseline: `oracle/benign_divergences.json` (operand
  re-serialization order, Qt->PDF rect flip, Edact-Ray numeric-TJ gap, fresh-doc
  single-revision) — these are filtered so they never escalate.
- `oracle/redaction_leak_oracle.py` (P1/F-01/F-03), `oracle/tm_residual_oracle.py`
  (P2/F-02), `oracle/triage_dedup.py` (CASR-style bucketing).

## COVERAGE

**Not yet measured.** Source-based `llvm-cov` requires the clang build
(`-fprofile-instr-generate -fcoverage-mapping`), which is deferred with the
libFuzzer toolchain. No coverage number is claimed.

## THROUGHPUT

**Not applicable yet** — no continuous libFuzzer/AFL++ loop ran (no fuzzer
toolchain). The oracle matrix is a deterministic finite suite, not a throughput
campaign. No execs/sec is claimed.

## RESULTS (what actually happened)

| Priority | Target | Result |
|----------|--------|--------|
| P2 / **F-02** | Tm linear part ignored | **CONFIRMED — reproducible residual extractable text** (see EVIDENCE f02-tm-residual) |
| P3 | lua_tostring null-deref | **CONFIRMED — deterministic crash** (see EVIDENCE lua-tostring-nullderef) |
| P1 / F-01 | plain Save orphaned objects | **NOT REPRODUCED** — plain Save scrubbed cleanly with correct geometry (verdict CLEAN) |
| P1 / F-03 | incremental SaveUpdate keeps original revision | **NOT PROVEN here** — writeUpdate on a fresh in-memory doc yields a single clean revision; needs a doc with a pre-existing on-disk revision (signed fixture). See "NOT YET PROVEN". |
| P3 | OCR/MRC NUL/paren injection; malformed /Rect | **NOT YET EXERCISED** — needs libFuzzer build or a dedicated content-injection driver (scaffolded path). |

### NOT YET PROVEN (honest gaps — do not treat as cleared)
- **F-01/F-03 on SIGNED docs.** The ER-2 guard (PdfEditorEngine.cpp:1238-1243)
  refuses redaction+incremental-save on signed docs; proving/refuting F-03
  requires a signed fixture + a multi-revision input. Build: extend
  `redaction_driver` with a `signed-incr` mode that loads a pre-signed fixture
  from `tests/fixtures/signing`, then run `redaction_leak_oracle.py` expecting
  `xref_revisions >= 2` and asserting the original revision lacks the secret.
- **PDFium / qpdf differential.** Three-way text/structure diff with the
  `benign_divergences.json` ignore-list.
- **Coverage measurement** (needs clang build).

## REPRODUCE

```bash
cd /c/Users/User/Projects/pdf && export PATH="/c/msys64/ucrt64/bin:$PATH"
# P2/F-02 + P1 matrix (runtime DLLs come from build/):
bash fuzz/build_clang/build_redaction_driver.sh
PATH="$PWD/build:$PATH" bash fuzz/run_oracles.sh
# P3 lua crash:
bash fuzz/build_clang/build_lua_repro.sh
./fuzz/bin/repro_lua_tostring.exe table     # crashes; 'string' is the control
# P3 djot libFuzzer (after: pacman -S mingw-w64-ucrt-x86_64-clang compiler-rt):
bash fuzz/build_clang/build_djot_clang.sh
```

## WIRING INTO THE BUILD (owner action — we do not edit root files)
Add one guarded line to the root `CMakeLists.txt`:
```cmake
if(GLYPHPDF_FUZZ) add_subdirectory(fuzz) endif()
```
Then: `cmake -S . -B fuzz/build -G Ninja -DGLYPHPDF_FUZZ=ON <main -D flags>`.
Never reuse the main `build/` dir.
