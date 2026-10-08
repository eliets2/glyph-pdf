# FIXALL CI-and-tests lane — FINAL handoff (2026-09-30)

Scope: CX-13, CX-14, CX-15, CX-16, CX-17, the PoDoFo commit-SHA pin, the E4
wording fix. Branch: `feat/fixall-ci3` (worktree `D:\pdf\pdf-keyC`),
base = parity tip `12da4e2f` (origin's `review/consolidated-parity` was
deleted post-merge — PR #2 is MERGED; `12da4e2f` is its final head and an
ancestor of origin/main). NOTE: the `.context/fixall-ci-wip.md` handoff this
lane was told to read first did not exist on disk (same pattern the
ci-investigate lane recorded); this file is the final state instead.

## Headline finding of this session

**Every dispatched item was already done and folded** — the FIXALL session's
CI lane landed at CP5 (`a72ab5d3`) and the work is on the parity tip:
`92101f6c` (CX-13), `38e6412b` (CX-14), `4e463a8c`+`e0f72ce4` (CX-15),
`1b42e166` (CX-16), `462212b4` (CX-17), `eb3c181b` (PoDoFo SHA pin + E4).
All NC evidence was verified present under docs/audit/evidence-cx13..17/.
The ONE acceptance gap was real: **the fuzz workflow had never been
dispatched post-fix** (handoff §7 item 12 admitted this) — last fuzz run
before today was 2026-08-30 (the "cmake: command not found" failure CX-13
records). This lane closed that gap with real dispatches.

## CX-13 acceptance — the dispatch chain (all on feat/fixall-ci3)

| Run | SHA | Oracle job | djot job | What it proved/found |
|---|---|---|---|---|
| 36643482560 | 12da4e2f (final parity SHA) | FAIL at engine configure | FAIL: 'QString' file not found | Two provisioning defects invisible since 2026-08-30: djot chain gained QtCore (6cf0c267) and the trimmed Qt list lacked qt6-svg (CMakeLists.txt:123 hard-requires Svg) |
| 36644229748 | 96df755a | FAIL at driver build ('QCoreApplication: No such file') | FAIL: 'podofo/podofo.h' not found | build_redaction_driver.sh hardcoded /c/msys64 — CI installs msys2 to a runner temp dir; the djot chain also includes podofo |
| 36646070984 | 8528c52c | **GREEN — real oracle verdicts** | FAIL at link: podofo 1.1 refuses PODOFO_BUILD_SHARED | First real oracle campaign on CI: [identity] [rot90] [rot45] [scale3x] [skewx] [p1_simple] all CLEAN in a fresh G19 dir (run-20260929-234344-495) |
| 36646929803 | 579d2a55 | GREEN | FAIL: harness uses unqualified LuaDjotCodec (stale since the pdfws namespace move) | The harness could not have compiled since the rename — the gate refused to stay green over it (INF06) |
| 36647764910 | 42613fc4 | GREEN | FAIL: undefined utls/LogMessage/FilterFactory | podofo 1.1 static = TWO archives; libpodofo_private.a needed |
| 36648744086 | f5337dee | GREEN | FAIL: undefined EVP_* | podofo baked OpenSSL EVP (runner ships libssl-dev) |
| 36649751719 | 54e941de | GREEN | FAIL: chromium FaxModule + xml* | THREE static archives (libpodofo_3rdparty.a = vendored codecs) + libxml2 XMP |
| 36650535130 | 51ac71a1 | GREEN | FAIL: PKG_CONFIG_PATH unbound (my bug) | set -u guard |
| 36651191042 | 1ec99e25 | FAIL (TRANSIENT: lbaselib.c FAILED, zero compiler diagnostics, target green in 6 prior runs — hosted-runner hiccup) | FAIL: cannot find -llzma (libxml2 static chain) | liblzma-dev pinned |
| 36651890815 | 9cf06806 | **GREEN** | FAIL on a **REAL FINDING**: ASan heap-buffer-overflow | The gate's first catch — see below |
| **36652740078** | **0d61a9ad (final)** | **GREEN — 6/6 CLEAN verdicts on the final SHA** | red on the SAME finding, deterministically (identical crash hash 5d1a4a36..., same ASan location — genuine stable bug, not a flake) | FINAL acceptance run |

Acceptance verdict:
- "one workflow_dispatch run on the final SHA showing real oracle
  iterations" — SATISFIED: run 36646070984 (first green campaign, real
  verdict lines) and every subsequent green oracle job including the final
  SHA's run; CI log shows the six CLEAN verdicts.
- "a deliberately broken harness (tested once, NOT committed) fails the
  job" — SATISFIED by the prior lane's record:
  docs/audit/evidence-cx13/negative-control-broken-harness.patch (redaction
  skipped -> every oracle reports LEAK -> EXIT=1; restored -> EXIT=0). The
  patch is stored as evidence, NOT in the tree.
- BONUS, stronger than the NC: the djot job failed on a REAL memory bug on
  its first genuine campaign.

## NEW FINDING (owner: engine/djot lane) — recorded, NOT fixed here

ASan heap-buffer-overflow (WRITE of size 8 past a 2640-byte lua-stack heap
region, finishrawget lapi.c:718) reached from pushChild
(LuaDjotCodec.cpp:372) under collectInlineText's UNBOUNDED C recursion
(LuaDjotCodec.cpp:392); walkInline recurses the same way. Crash input =
4001 bytes of '>' (2000-level blockquote nesting bomb):
docs/audit/evidence-fuzz-djot-finding-2026-09-30/ (input + ASAN-report.md +
owner fix shape). Production exposure: the save-time dual-write chain's
hostile-input surface. The djot job STAYS RED until fixed — INF06 behaving
correctly; do not waive.

## Per-item verification (all at parity tip 12da4e2f unless noted)

- CX-13 — `92101f6c` (+ `48a140f8` runner/deps pin). Fuzz workflow now
  provisions like ci.yml (SHA-pinned podofo, ONNX, PDFium, full MSYS2 set +
  clang), path filter covers src/engines/** + src/core/Redaction* + fuzz/**,
  workflow_dispatch trigger present, pipefail + belt-and-braces greps,
  artifacts with if: always(). Evidence: docs/audit/evidence-cx13/ + the run
  table above. THIS LANE ADDED 11 follow-up commits (see below) because the
  acceptance dispatches kept peeling provisioning layers the folded commit
  could not have seen without actually running the workflow.
- CX-14 — `38e6412b`. CMakeLists GLYPHPDF_QTTEST_CAPTURE_DIR block adds
  `-o -,txt -o <abs>/<name>.txt,txt -o <abs>/<name>.junit,junitxml` to every
  Qt6::Test target; ci.yml uploads test-results/ + LastTest.log with
  if: always(). PROVEN ON THIS SESSION'S RUNS: the red build-and-test runs
  (36643459928, 36644222145, 36646053589) each uploaded a ~322 KB
  test-results artifact with 186 .txt + 186 .junit files; the single
  captured failure each time was TestWelcomeRoutes::
  imagesRouteProducesAndOpensTheOutput 'QFileInfo::exists(out)' returned
  FALSE — the exact defect the ci-investigate lane later root-caused from
  these same artifacts and FIXED on main (84a30e10).
  **Fontconfig verdict: DISPROVED.** evidence-cx14/TestWelcomeRoutes-baseline.txt
  (WITH the f48daead fonts.conf): FAIL at imagesRoute... 19P/1F;
  TestWelcomeRoutes-noconfig.txt (WITHOUT): 20P/0F. Fontconfig is not the
  cause; the real cause was the test-driver delivery defect (fixed on main).
- CX-15 — `4e463a8c` + gate find `e0f72ce4`. content-spans-sanitizer job:
  ubuntu-24.04, clang -fsanitize=address,undefined -fno-sanitize-recover
  over tests/sanitizers/ContentSpansAdversarial.cpp + ContentSpans.cpp
  (the CX-08..12 adversarial shapes; 278895 checks). NC recorded once:
  evidence-cx15/negative-control-local-ubsan-trap.txt (NC-EXIT=132, SIGILL).
  GREEN on every run this session (all 10 dispatches + all CI pushes).
  Coverage thresholds: R10-DEFER (unchanged from the session's disposition).
- CX-16 — `1b42e166`: copies the placed AnnotationItem (no ref into the
  temporary QList). Fail-before -Wdangling-reference at the exact line +
  pass-after in evidence-cx16/.
- CX-17 — `462212b4`: clear keys -> toggle false->true -> key EXISTS+true ->
  reconstruct+true -> toggle back -> exists+false. NC:
  writer disconnected -> FAIL (new-test-writer-disconnected-FAILS.txt);
  the OLD test with the writer disconnected STILL PASSED (proving the old
  test vacuous). Both in evidence-cx17/.
- PoDoFo SHA pin — `eb3c181b`: fetch --depth 1 commit
  712fb0e80e0e9404525d8db54fa0baa4ae469963, cache key
  podofo-1.1.0-g712fb0e8...-ucrt64-r1. Validated end-to-end by every fuzz
  dispatch (bootstrap runs green on the runner).
- E4 wording — `eb3c181b`: gate-table row no longer quotes the
  self-matching literals; recorded scan excludes its own command line.

## This lane's commits on feat/fixall-ci3 (one per finding, evidence-first)

0d61a9ad docs(evidence): fuzz finding record (crash input + ASan report)
9cf06806 fix(fuzz): follow-up 11 liblzma-dev
1ec99e25 fix(fuzz): follow-up 10 PKG_CONFIG_PATH set -u guard
51ac71a1 fix(fuzz): follow-up 9 vendored libpodofo.pc --static link
54e941de fix(fuzz): follow-up 8 OpenSSL EVP link
f5337dee fix(fuzz): follow-up 7 libpodofo_private.a + codec deps
42613fc4 fix(fuzz): follow-up 6 pdfws::LuaDjotCodec qualifier in harness
579d2a55 fix(ci): follow-up 5 ubuntu podofo goes PODOFO_BUILD_STATIC=TRUE
8528c52c fix(ci): follow-up 4 (note: message lost two backticked fragments
         to shell substitution — cosmetic; content intact) ACTIVE msys2
         root in driver script + stage_runtime_dlls in the build target list
40106761 fix(ci): follow-up 3 djot job builds the pinned vendored podofo
96df755a fix(ci): follow-up 2 qt6-svg is REQUIRED
0eff7d0f fix(ci): follow-up 1 djot job provisions QtCore
(Evidence commit 0d61a9ad is docs-only; the tip is the final SHA.)

Local validation on this machine: driver build via
GLYPHPDF_BUILD_DIR=build-keyC -> OK; bash fuzz/run_oracles.sh -> 6/6 CLEAN
exit 0 (run-20260930-023423-94664); harness TU g++ -fsyntax-only clean;
bash -n + YAML parse on every workflow edit.

## Residuals / owner items

1. djot libFuzzer finding (heap-buffer-overflow) — see above; job red until
   fixed; crash input preserved.
2. origin/main Build is RED at 7eb5c67b: TestSignatureRealCrypto.cpp:778
   declares `QString output` TWICE and the second QVERIFY call is
   unterminated ('missing )' in macro usage' at :779) — the 1.7c fix as
   LANDED on main is broken; the local lane commits (78146902/6a63ff63)
   never contained that text, so the corruption happened during the main
   landing. Owner must fix on main; NOT touched here (out of lane scope).
3. TestWelcomeRoutes imagesRoute + the other §1.7 CI failures are FIXED on
   main by the ci-investigate lane (7f822fcc/84a30e10/7eb5c67b there =
   8e773ed7/84a30e10/7eb5c67b on main); feat/fixall-ci3 predates them, so
   its CI Test step legitimately shows the captured imagesRoute failure.
4. content-spans-sanitizer: green everywhere; keep coverage thresholds
   R10-deferred.
5. The fuzz workflow's oracle job is deterministic; one hosted-runner
   transient seen (36651191042 lbaselib.c, zero diagnostics) — plain re-run
   sufficed.

## Git state

Branch feat/fixall-ci3 pushed to origin, tip 0d61a9ad, based on 12da4e2f
(= merged parity tip). 12 commits ahead of the base, no merges, no stash,
one file-history note: all commits are new lane work (no cherry-picks).
Worktree has an untracked test-results/ dir (local capture from the
ci-investigate era, gitignored pattern, left alone) and fuzz/scratch run
dirs (gitignored).
