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

## OPEN ITEMS (owner)

1. **djot depth budget — CLOSED `505295f4`** (was IN FLIGHT) — the CX-13 gate's first catch: genuine
   heap-buffer-overflow, unbounded codec recursion (collectInlineText/walkInline,
   LuaDjotCodec.cpp:372/:392) via a 2000-level blockquote bomb. Evidence:
   `docs/audit/evidence-fuzz-djot-finding-2026-09-30/`. Fix lane dispatched on
   `feat/djot-depth-budget` (worktree pdf-djotfix); the djot fuzz job stays RED
   by design until it lands — do not waive.
2. PGR-52 (per-match timeout needs a PCRE2 seam), PGR-54 (failed bates step
   reporting), PGR-55 (untrimmed redact-entry splits), PGR-56 (confirm flows
   keyed on error strings) — recorded in the presets-review handoff on main.
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
