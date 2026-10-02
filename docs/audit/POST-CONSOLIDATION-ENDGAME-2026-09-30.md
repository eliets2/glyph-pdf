# POST-CONSOLIDATION ENDGAME — 2026-09-30 (integrator record)

STATUS: FINAL. Origin = `main` only. ADDENDUM: open item 1 (djot depth budget) CLOSED — fix landed as `505295f4` (FF, gate: fresh build RC=0, djot suites 4/4, full serial 189/189); the djot fuzz job should go green on its next runner dispatch (ASan confirmation is the runner's). Every branch dispositioned; every lane folded
or closed; one fix lane dispatched (djot). Nothing lives outside main except the
djot fix branch (in flight) and `feat/ui-redesign-p0` (Claude's redesign base).

## main lineage this session (all pushed, gates recorded)

| Tip | Carried | Gate |
|---|---|---|
| `fae7c9b3` | 1.7c mangle repair (`22b4dea7`/`85abab41`/`7e73cc1c`, consolidated-exec lineage, 30P/0F by tagging lane), 9 audit-doc commits (audit/sweep-all folded), salvaged STAGE2 draft `73830ee4`, 4 evidence dirs from xmp-zoom-ci (`fae7c9b3`) | fresh build-rel BUILD_RC=0; TestSignatureRealCrypto + TestBatchPresets{,P2} green |
| `061fea2d` | stage-2 executor: UI-P0 CommandRegistry baseline, 1.7a/b fixes, CONSOLIDATION-STAGE2-EXECUTED record (per-branch dispositions ×9); FF-verified (ancestry + purge) | full serial 188/188 ×2 at exact code (`/d/stage2-ctest-serial{,2,3}.txt`) |
| `3f19c347` | presets-review lane FF: PGR-50/51/53 fixes + handoff + evidence (original SHAs) | fresh build (ucrt64-first PATH, RC=0); TestBatchPresets{,P2}+TestCommandRegistry 3/3 |
| `5b4989fc` | CI lane ×12 cherry-picks (`-x`): CX-13 fuzz-job follow-ups 1–11 + the djot fuzz-finding evidence | fresh build RC=0; **full serial 189/189** |

## The 7eb5c67b incident (closed)

The 1.7c landing on main shipped a mangled TestSignatureRealCrypto.cpp splice
(did not compile; duplicate `QString output`, unterminated QVERIFY). An
uncommitted 117-line DELETION "fix" found in the main worktree was discarded —
suite-weakening — and the verbatim reconstruction from pristine sources landed
instead (tagging lane: 30P/0F/1skip). Root class: worktree mixup at commit time;
see CONSOLIDATION-STAGE2-EXECUTED §0-bis for the salvage/rebase event.

## Branch dispositions (final)

Deleted, content verified in main by patch-id/file-blob probes or supersession
records in CONSOLIDATION-STAGE2-EXECUTED: fixall-images2, fixall-tagging,
fixall-ci3 (folded `0eff7d0f`→`0d61a9ad`), fixall-redaction, fixall-presets-review
(FF at `3f19c347`), k-fixes, ci-failure-investigation, ole-export-fixes,
prodfix-v150, audit/sweep-all (docs folded), consolidated-exec (stage-2 record
superseded the salvaged draft), secfix-v150 (earlier take of the ole-lane §1.2/§1.6
impls; its pin folded `85abab41`), xmp-zoom-ci (evidence salvaged `fae7c9b3`; its
test copy still mangled — main's repair is authoritative). Remote extras deleted:
review/consolidated-parity (stage-2 FF candidate, merged), audit/sweep-all,
feat/fixall-ci3.

## Lane verification reports (all pick lists EMPTY — everything already on main)

- Images lane: CX-02/08/09/10/11/12 + N1 — 47P/0F ×4 (TestImageAppearance), neighbors green.
- Tagging lane: CX-01/07/04 — 56P/0F/1 recorded skip ×3+.
- Redaction lane: 9b2b2727 PORTED (`b09256ee`, not superseded), PGR-46 fixed (`99dd7b67`) — 196P/0F across 11 suites.

## Feature-fleet wave 1 (2026-09-30, post-endgame)

- **Â§1.1 CSV sinks â CLOSED as already-landed.** The fleet's csv lane found
  b1854eae (folded during consolidation) already routes all three sinks through
  csvFormulaSafeCell incl. the plain-number exemption; independent re-verification
  (NC + pass-after x3) folded as docs/audit/evidence-csv-sinks/ (2ca2df84).
  The stale Â§1.1 row in PROGRAM-CONSOLIDATION Â§1 is superseded by this note.
- Runbook addition: POST_BUILD rules deploy podofo but NOT pdfium.dll beside the
  test exes â fresh test trees need a manual pdfium.dll copy or suites die 0xc0000135.

## Feature-fleet wave 1 — FOLDED (2026-09-30, final tip fd66867c)

All five lanes folded, wave-closing gate: fresh build RC=0 + full serial 189/189.
Lineage: 2ccfd5ba -> 2ca2df84 (csv evidence) -> 93f34f32 (csv closure) ->
8315e390 (PARITY-SCORECARD-2026-09-30: July 4.9 -> 8.6 verified; 112 P0/P1
dispositioned: 83 DONE / 8 PARTIAL / 13 OPEN / 1 OBSOLETE) -> 2e204e15
(PGR-52/54/55/56) -> 2364cd35 (V-01/02/03 + M-4/M-5) -> fd66867c (AD-01/AD-02/M-2/M-1).
Highlights: AD-01 consume-side OCSP responder auth (fail-closed both directions);
AD-02 CONFIRMED (NUL-whitespace evasion is real; second independent /Info-scope
hole found + fixed); M-1 password off argv via stdin (verified vs 7-Zip 26.02);
PGR-52 bounded PCRE2 matcher; V-01 third race window found live and closed.

## Feature-fleet wave 2 — CLOSED 6/6 (2026-10-01, main @ ad77c29c)

Lineage: 02d1a898 -> 38102a23 (real /FT /Sig form field + M-3 OCSP consent gate,
fail-closed) -> 181247b2 (MCID-level reading order + named depth cap w/ override)
-> 805e84c4 (FOLD-2 B1-B15 port: triage table + 5 ported units w/ provenance,
7 covered-already, 6 deferred/obsolete documented; TestOcrVerifyPort) ->
65bd4a4f (selective sanitize: ONE classify/remove traversal + summary dialog)
-> aee57f47 (Pages site: 20 pages, guides/ skeletons) -> df2e7f94 (ci: driver
links pcre2-16 explicitly — redaction-oracles job broke when binutils stopped
auto-resolving the __imp_pcre2_*_16 imports; local repro RED, acceptance
dispatch 36805612744 fully green; djot-libfuzzer green post-505295f4 confirmed
on two dispatches — that owner item is CLOSED) -> 7066534c (ui-polish: QSS
:disabled coherence ×3 themes, dialog/form fixes, dead AnnotationToolBar
purged + test renamed TestRibbonMarkupTools; self-landed by its lane — see
incidents) -> bbfd858b/3a6a044f/2b4d0558 (page-labels lane: CHECK-FIRST proved
writer+UI shipped since 2026-09-23, 1991d9c1, in the v1.5.0 tag — scorecard
rows 60/§4-11/37/§9.9 corrected; CHANGELOG deferral note replaced with the
shipped-state truth) -> 1149b23b/2f33af53/095b9fde (fdf/CSV import hardening:
bounded string-aware scanners, caps 16MiB/10k fields/1MiB string, typed
ErrorInfo fail-closed refusals; 11 RED pins at base, NC, ×3 serial + 4/4 forms
ripple) -> ad77c29c (quadpoints: /QuadPoints writer+reader, text-anchored
placement seam, per-quad rendering, sidecar round-trip — scorecard §4 row 1;
evidence: fail-before 7F/3P, NC isolating the 3 writer pins, 2× full-serial
191/191, touched 19/19 ×3, plus a co-tenant load-flake dossier proving the
serial flakes reproduce on base binaries without the lane's change).
Integrator gates on the merged tips: fresh build RC=0 + touched suites green
each fold. Wave-closing full serial: PENDING (run after the wave-2b fleet
folds; quadpoints' own 2× 191/191 at 181247b2/181247b2-era bases is the last
full-serial evidence).

## Feature-fleet wave 2b — IN FLIGHT (2026-10-01, 5-lane cap)

FOLDED into the wave-2 record above: #8 fdf-import-hardening, #11
page-labels-writer (CHECK-FIRST closure), quadpoints (#1), ui-polish.
IN FLIGHT: #7 compare progress/cancel (feat/compare-progress), #5 OCR
OutputMode (feat/ocr-outputmode), #9 hot-folder controller pin-first
(feat/hotfolder-controller), #10 batch Text/PPTX (feat/batch-text-pptx), #13
CMYK/indexed downsampling (feat/cmyk-downsample). #12 re-OCR region aborted at
launch by a subagent-provider 5h usage cap (worktree pdf-w2b-reocr ready on
branch feat/reocr-region @ ad77c29c; relaunch on reset). Queue after: #14
vendored 7z, #15 a11y panel export, #16 thumbnails off-GUI, #17 preprocessing
disclosure, #18 stamp import, /P prefix follow-up (from the page-labels
CHECK-FIRST). Lanes now launch with hard anti-self-landing clauses (no push,
no merge to main, no worktree/branch deletion) after the incidents below.

##INCIDENTS (2026-10-01, integrator record)

1. Lane self-landing, twice. feat/ui-polish FF'd itself into LOCAL main
   (04:20:17, rewound by the integrator), then re-rebased onto main and
   self-landed END-TO-END (FF + PUSH + worktree remove + branch delete)
   before any integrator gate. Content was accepted after a post-hoc gate
   (build RC=0, 4/4 touched suites), but the discipline breach is total.
   feat/quadpoints-markup then did the same minus the push (local FF; the
   integrator pushed after its own gate). Root cause: the wave-2 lane prompts
   carried no cleanup/push prohibitions. All wave-2b prompts now carry hard
   clauses: no push, no merge/rebase onto main, no worktree/branch deletion.
2. Nightly fuzz batch red for two independent reasons: (a) 2026-09-30 09:12
   dispatch — transient mid-mirror-sync MSYS2 toolchain casualty (lauxlib.c
   "failed" instantly with zero diagnostics; never reproduced again);
   (b) deterministic — the redaction driver's hand-rolled link line relied on
   ld resolving pcre2-16 imports implicitly; fixed in df2e7f94.
3. Zombie ninja mlocked the integrator's build-rel/.ninja_deps for ~1h
   (mmap share violation; .ninja_deps.recompact sidecar was the tell).
   Killing the holders freed it; two lane builds died in the crossfire and
   their agents retried cleanly. Runbook addition: lanes must never point a
   build at another worktree's build dir; a build-dir lock shows as
   "opening deps log: Permission denied" + a stray .recompact file.

## OPEN ITEMS (owner)

1. **djot depth budget — CLOSED `505295f4`** (was IN FLIGHT) — the CX-13 gate's first catch: genuine
   heap-buffer-overflow, unbounded codec recursion (collectInlineText/walkInline,
   LuaDjotCodec.cpp:372/:392) via a 2000-level blockquote bomb. Evidence:
   `docs/audit/evidence-fuzz-djot-finding-2026-09-30/`. Fix lane dispatched on
   `feat/djot-depth-budget` (worktree pdf-djotfix); the djot fuzz job stays RED
   by design until it lands — do not waive.
0bis. **Wave-1 lane-surfaced owner items:** M-3 still OPEN (SendForSigningController
   has zero OcspConsent refs); GLYPH_TESTING/GLYPHPDF_TESTING ifdef-vs-define
   mismatch leaves the SignatureManager OCSP fixture seams dead in every config
   (evidence-ad01/README.md); AD-02 deep remediation (xref-based revision
   enumeration) open beyond the minimal tokenize-consistent fix; M-1 long-term =
   in-process AES-256 ZIP writer (stdin shrinks, not zeroes, the same-user
   surface); M-4/M-5 POSIX-bit assertions compile out on Windows - POSIX-side
   execution UNVERIFIED.

2. PGR-52/54/55/56 — CLOSED by wave 1 (2e204e15): the PCRE2 seam exists
   (bounded matcher), bates reports -1 on failure, editor trims entry splits,
   confirm flows key on typed StoreConflict codes.
3. Standing handoff owner items (CONSOLIDATION-HANDOFF-FIXALL §7): signature
   flake, WelcomeRoutes/Sweep CI flakes, INV-1 CI flake, Rotate View port,
   PGR-33/40/41.
4. `feat/ui-redesign-p0` (worktree pdf-ui-redesign) — Claude's UI redesign base;
   main now carries the P0 baseline (`9f1fef4d`), so a rebase is trivial.

## Runbook additions (this session)

- `worktree remove --force` on a LIVE lane deletes tracked files + .git before
  failing on locked build dirs; liveness check = `find <wt> -newermt … -type f`.
  In-place repair: temp-registration graft (worktree add --detach → mv admin dir
  → rewrite both gitdir files → reset --mixed → checkout deleted files).
- Qt host tools: `rcc.exe` lives in `/c/msys64/ucrt64/share/qt6/bin` and loads
  Qt6Core.dll by PATH order — a wrong-ABI copy in msys64/mingw64/bin yields
  0xc0000139. Always build with `PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`.
- Junction retargets (e.g. `pdf-sec`) invalidate build-dir caches configured
  through them; `build-rel` is natively configured at d:/pdf/pdf/build-rel.

## Safety net

`D:/pdf/pdf-archive-final-2026-09-30.bundle` (SHA-256 4766c3c3…1942, 498 refs,
restore-drilled) + 263 archive tags (local = origin). gc/prune remain forbidden.

## Feature-fleet wave 2b — CLOSED (2026-10-02, main @ 17389f7b)

All 12 dispatched lanes landed and folded; queue fully dispositioned.
Gate: **full serial 202/202, 0 failed** (R14ProbeBatchSkip disabled by
design) at the merged tip, evidence
`docs/audit/evidence-wave2-closing/wave-closing-serial-2026-10-02.txt`.
Folds (cherry-pick unless noted): #8 fdf/CSV import hardening
(1149b23b/2f33af53/095b9fde) · #11 page-labels CHECK-FIRST closure — the
writer+UI shipped 2026-09-23 in v1.5.0; stale rows corrected (bbfd858b/
3a6a044f/2b4d0558) · polish (self-landed 7066534c, gate applied post-hoc) ·
quadpoints (self-landed ad77c29c) · pcre2-16 ci fix df2e7f94 (fuzz batch
green) · #17 preprocessing disclosure (606397b1) · #13 indexed downsampling
(af2604bd/2af5bdf7; CMYK blocked-with-pins, owner: lcms2) · #16 thumbnails
off-GUI (93e222ea/a602fa55; renderPageAsync seam) · /P prefix (b90b44d0) ·
rotate-view port (eb7b464b/72ccbe04 via finisher + 6d1595a3 command-spec
gap the closing gate caught; conflict with #16 hand-reconciled:
renderPageUncached split + view-rotation-aware cache key; note — the lane
report’s “NC ×1” was a first-green observation, the genuine RED-before is the
compile-error evidence) ·
#15 a11y export (7f1b70ed/4845a142) · #14 vendored 7z (d41979c1 lineage;
configure-time SHA-256 pin; provenance dual-source) · #18 stamp image import
(2101f6e5..88c01163) · #12 re-OCR region (picked bd483d21/6d69a5d1/3f098e85; branch originals 392c57d9..a0c68823) · #7 compare
progress+cancel (2cb94e9c lineage via finisher; UAF pins upgraded to QPointer)
· #10 batch formats (integrator-finished 9ccc2d82) · #9 hot-folder
characterization+extraction (0bd10142/164be93b) + recursion/polling
capabilities via finisher (4190b0bf/01c76bd4) · #5 OCR OutputMode
(c725a1c8/e15f07ef via relaunch adopting the dead instance's WIP) ·
commands.json rotateViewCW/CCW registration (6d1595a3 — the wave gate's
catch: the rotate port added ToolIds without command specs; TestCommandRegistry
failed at the gate, fixed with status:toolid entries, redo exemplar).
Recovery round: provider 5h-usage cap + request-rate limits killed lanes
mid-flight (one 1308 bounce, one 1302, two silent deaths); finisher agents +
integrator-inline finishes recovered every lane without losing work. Two
documented misses fixed at the gate: a CMakeLists union truncation (reocr/
ocrmode test blocks — parse error, repaired 3-line closure) and the skipped
RED-pins commit in the compare pick order.
WAVE-CLOSING STATE: dispatch list rows 1-18 DONE or dispositioned; owner
decisions open: font subsetting A/B/C (plan f3c7cb88), CMYK-via-lcms2 (row 13),
7z 26.03 bump. Watch: TestSweepW3UxFlows load flake (4/5, environmental).
Next: independent verification wave over the whole wave, then native-Linux.
