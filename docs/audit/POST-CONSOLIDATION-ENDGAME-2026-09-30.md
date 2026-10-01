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

## Feature-fleet wave 2 — 4 of 6 lanes FOLDED (2026-10-01, main @ aee57f47)

Lineage: 02d1a898 -> 38102a23 (real /FT /Sig form field + M-3 OCSP consent gate,
fail-closed) -> 181247b2 (MCID-level reading order + named depth cap w/ override)
-> 805e84c4 (FOLD-2 B1-B15 port: triage table + 5 ported units w/ provenance,
7 covered-already, 6 deferred/obsolete documented; TestOcrVerifyPort) ->
65bd4a4f (selective sanitize: ONE classify/remove traversal + summary dialog)
-> aee57f47 (Pages site: 20 pages, guides/ skeletons w/ placeholder steps +
pending-screenshot blocks for the redesigned UI, all claims repo-sourced).
Per-lane gates green; wave-closing full serial PENDING (last full: 191/191 at
805e84c4 by the port lane; suite count now 191 targets).
IN FLIGHT: feat/quadpoints-markup (scorecard #1), feat/ui-polish (glitch/QSS
sweep + dead AnnotationToolBar delete). Fold per the standard protocol.
NOTE: GitHub Pages serves main:/docs -> the guides site is LIVE at
https://eliets2.github.io/glyph-pdf/ and rebuilds on every main push.

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
