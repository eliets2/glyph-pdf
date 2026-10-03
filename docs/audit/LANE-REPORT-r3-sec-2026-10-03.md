# LANE REPORT — r3-sec (round-3 security fixes)

- **Date:** 2026-10-03
- **Lane:** GlyphPDF round-3 fix lane (security)
- **Worktree:** `D:/pdf/pdf-r3-sec` — branch `feat/r3-sec`, base `f6d9ca1f`
- **Context reports:** `D:/pdf/verification/crossmodel/security-auditor.md` (finding 2), `D:/pdf/verification/native-adversary-wave2b.md` (F-6)
- **Status:** COMPLETE — 2/2 missioned findings fixed, R7 evidence complete, closing full serial gate **201 passed / 0 failed** (R14ProbeBatchSkip "Not Run (Disabled)", the documented prior state)
- **Commits:** fix(r3-sec) — runtime 7-Zip integrity verification + watcher fan-out backstop (code + pins); docs(audit) — this report + `docs/audit/evidence-r3-sec/`

## 1. What was fixed

### Security finding 2 (M, CWE-494) — the 7-Zip SHA-256 pin is configure-time only (`CMakeLists.txt`, `src/engines/SafeSave.cpp`, `src/shell/controllers/HomeController.cpp`)

The auditor's finding, verbatim in threat terms: the `file(SHA256)` +
`FATAL_ERROR` gate protects the BUILD HOST, not the installed artifact. After
staging/deployment nothing re-verified the bytes — yet the resolved `7z.exe`
receives the document bytes AND the package password (the M-1 stdin
contract). "A corrupted copy fails at 7z process start" is not a security
control; a malicious binary does not fail.

Fix:

- **Pins travel into the binary.** CMake passes the already-computed
  `GLYPHPDF_7ZIP_EXE_SHA256` / `GLYPHPDF_7ZIP_DLL_SHA256` as compile
  definitions on `pdfws_engines` (the target that compiles SafeSave) exactly
  when the staged bundle passed the configure-time gate. Non-staged trees
  (non-Windows configure) compile no pins; the resolver then says so honestly
  via qWarning and keeps the historical behavior — unreachable on shipped
  Windows builds, where the CMake gate FATALs without the bundle.
- **Resolution re-verifies.** `SafeSave::locateSevenZip` gained an optional
  `integrityError` out-param (default `nullptr` — existing call sites
  unchanged). When the app-owned pair exists, BOTH files are hashed
  (SHA-256, 64 KiB chunks) and compared against the compiled-in pins; any
  mismatch — tampered OR stale OR unreadable (an unreadable staged binary is
  a failure, not a pass) — returns EMPTY with an honest user-presentable
  disclosure ("…failed its integrity check — the SHA-256 of the staged
  …does not match the value pinned for this build. The bundled tool will not
  be launched with your document or your package password…"). There is
  deliberately NO fallback leg (the wave-2b F-02 removal stands): a refused
  pair never degrades to a system 7z.exe.
- **Same disclosure channel as absence.** `HomeController::createEncryptedPackage`
  passes `&integrityError` and surfaces the message through the same
  `QMessageBox::warning` channel the absence path uses; absence (empty
  error) vs integrity failure (populated error) are disclosed differently.
- **Cached per session.** Verdicts (pass AND fail) are cached in a
  mutex-guarded `QHash` keyed by the resolved native path: the hash runs once
  per first use, not on every package operation, and a pair that passed
  cannot silently flip mid-session. ~5 MB of hashing once, zero per package.

Pins (TestSevenZipBundle, 7 → 9 test functions):

- `tamperedBundledPairIsRefusedWithIntegrityDisclosure` — the mission's
  byte-flipped fixture (real 7z.exe, one bit flipped at offset 0x200) is
  REFUSED with the integrity message; the symmetric 7z.dll leg too; junk
  bytes (the old fake-pair fixture) equally refused; absence stays
  distinguishable from tamper.
- `goodBundledPairPassesRuntimeVerification` — byte-exact copies resolve
  normally with no integrity error.
- `integrityCheckRunsOncePerSession` — the mission's "check runs once per
  session", pinned behaviorally: first use verifies clean; a control leg
  proves the SAME tamper is detected on a fresh path (cache miss); then the
  already-verified path turns hostile and still resolves (cached verdict —
  no re-hash, no flip-flop).
- `resolverPrefersAppOwnedBinary` keeps its name and assertion; its fixture
  now stages byte-exact committed-bundle copies (see §3.1).
- `resolverDisclosesAbsenceHonestly` gained one assertion (absence must not
  arrive as an integrity failure).

### Adversary F-6 (M) — watcher fan-out: silent exhaustion with no backstop (`src/modes/HotFolderController.{h,cpp}`, `src/modes/BatchMode.cpp`)

`watchSubdirectories()` fed every directory under the root to a single
`QFileSystemWatcher` and deliberately swallowed `addPaths` failures ("that is
what polling is for" — but polling was NOT running in watcher mode). Beyond
the OS watch budget the failures begin: subtrees become PERMANENTLY
unwatched, a drop there raises no event ever, and `isWatching()` keeps
reporting healthy as long as the root stays watched.

Fix:

- **One choke point.** `addWatchPaths()` is the single path into the native
  watch (root in `start()` and every refresh batch in `watchSubdirectories()`);
  its return value is exactly what the OS refused — the failures F-6 exploits.
- **Backstop.** Any refusal calls `engagePollingBackstop()`: the EXISTING
  polling machinery (`kPollIntervalMs` scan timer; the tick's ingest scan
  walks the whole root, so every unwatched subtree is covered) is engaged for
  the session. Watch+poll coexistence is already deduped by the shared
  processed set — no double delivery.
- **Disclosure.** Once per degraded subtree (the refresh re-learns the same
  losses every pass and must not spam): a `qWarning` from the controller plus
  the new `setWatchDegradedHandler` channel, wired in `BatchMode::ensureHotFolder`
  to its log ("Hot folder: N subdirector(ies) could not be watched (OS watch
  limit) — polling fallback engaged for the unwatched subtrees.").
- **Accounting.** `m_unwatched` / `m_watchFailures` track the degradation;
  `stop()` clears it (a fresh start begins healthy). A refused ROOT degrades
  the whole watch to polling (the "or the whole root" leg).

Pins (TestHotFolder, 25 → 29 test functions; all deterministic — the OS cap
is simulated, not waited for):

- `controllerWatchCapEngagesPollingForDegradedSubtree` — real-exhaustion
  shape: start-time adds succeed, later subtrees are refused; the capped
  subtree is unwatched, polling is engaged, the disclosure NAMES the degraded
  subtree, the counter records the refusal — and a drop into the un-watched
  subtree is still delivered by the engaged poll (ceiling wait on the tick,
  same discipline as the existing polling pins).
- `controllerWatchCapDisclosureFiresOncePerSubtree` — two refresh passes
  re-learning the same loss: disclosure fires once, the counter keeps
  cumulative accounting.
- `controllerRootWatchFailureDegradesToPolling` — refused root → whole-watch
  polling, disclosed.
- `batchModeDisclosesWatchDegradationInLog` — end-to-end BatchMode wiring:
  the degradation surfaces in the hot-folder log and on BatchMode's own
  controller.

## 2. R7 evidence (all runs SERIAL, `build-rel` only)

Evidence: `docs/audit/evidence-r3-sec/` (README there maps every file).

| Stage | Result |
|---|---|
| fail-before RED | `red-sevenzipbundle.log` 7/2 (fix 1: byte-flipped exe RESOLVES pre-fix; cache control leg red) — all pre-existing pins green. `red-hotfolder.log` 25/4 (fix 2: the four F-6 pins, no polling engagement, no disclosure) — all 25 pre-existing pins green. |
| NC once per fix | `nc.log`: fix 1 neutralized → exactly its two pins RED (`goodBundledPair`, `resolverPrefers`, absence, hash-pin, end-to-end guards green); fix 2 neutralized → exactly the four F-6 pins RED (all 25 pre-existing green). Zero collateral reds. Fixes restored (`grep "NC STAGE"` → 0), rebuild BUILD_RC=0. |
| pass-after ×3 SERIAL | `pass-1.log`, `pass-2.log`, `pass-3.log`: 2/2 suites green each run (TestSevenZipBundle 9/9, TestHotFolder 29/29). |
| Closing gate | `build-final.log` BUILD_RC=0; `full-gate.log`: **201 passed, 0 failed** out of 201 run (R14ProbeBatchSkip "Not Run (Disabled)" — the same disabled-by-design state as prior closing gates). |

## 3. Deviations / episodes (Rule 1, recorded)

1. **`resolverPrefersAppOwnedBinary` fixture swap (deliberate, not a
   weakening).** The pin's assertion is unchanged — the app-owned directory
   wins. Its fixture changed from a fake tool pair to byte-exact copies of
   the committed bundle, because the resolution CONTRACT now includes runtime
   verification: post-fix, a fake pair resolving would pin INSECURE
   behavior. The fake-pair fixture was not thrown away — it is the junk-bytes
   leg of the new tamper pin. The pin passes both pre-fix (no check ran) and
   post-fix (check passes), so the RED evidence does not rest on it.
2. **Disclosure-wording episode caught by the pin (mid-pass-after).** The
   first fix-stage `pass-1` run failed ONE assertion: the disclosure said
   "the 7-Zip tools … failed THEIR integrity check" while the pin (and the
   mission text) requires "failed ITS integrity check". The refusal itself
   worked; the message was aligned to the mission's exact phrasing
   (singular), rebuilt (BUILD_RC=0, in `build-fix.log` segment 3), and all
   subsequent runs green. The stale first `pass-1.log` was overwritten by the
   clean run — the episode is documented here instead.
3. **Two external kills, exit 137 (the prior lanes' documented contention
   class).** Stage-A full build killed at [994/1108] (0 FAILED up to the
   kill) — resumed clean (`build-stageA-resume.log`). Fix-stage build killed
   mid-relink — resumed clean (segment 2 of `build-fix.log`). Runbook
   permits incremental retry after transient failure; both recorded.
4. **Caching semantics, stated honestly.** The verdict cache keys on the
   resolved path and holds for the session: a bundle that turns hostile
   AFTER a clean first use is not re-detected until restart. That is the
   mission's "cache the check result per session" — pinned as a property
   (`integrityCheckRunsOncePerSession`), with the refusal path (first-use
   detection) carrying the security load. The Windows-registry-scale
   alternative (hash on every launch of every package operation) was
   rejected as the mission explicitly requires caching.

## 4. Build

- Configure per runbook: CONFIGURE_RC=0 (`build-configure.log`), plus one
  clean reconfigure to pick up the new compile definitions
  (`build-fix-reconf.log`).
- Stage-A full build: attempt 1 externally killed (exit 137) at 994/1108;
  resume exit 0. Fix / NC / restore stage builds all BUILD_RC=0; closing
  full-tree build BUILD_RC=0 (`build-final.log`).
- Full serial gate: 305.8 s wall, **201/201 passed, 0 failed**
  (`full-gate.log`).

## 5. Constraints honored

Work confined to `D:/pdf/pdf-r3-sec` (verified via `git worktree list` —
`D:/pdf/pdf` and sibling worktrees untouched); build dir strictly in-worktree
`build-rel`; no push, no merge/rebase to main, no stash/gc, no branch or
worktree deletion; no CLAUDE.md/SECURITY.md created; DLLs already provisioned
in `third_party/` (configure gate green throughout — the vendored 7-Zip pin
was never violated).
