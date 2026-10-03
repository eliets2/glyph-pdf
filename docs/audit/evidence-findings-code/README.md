# R7 evidence — findings-code wave-2c round 2 (finished 2026-10-02)

Mission: fix lane for the native-adversary wave-2b findings F-15 (/P text
string), F-1 (UTF-16BE FDF values), F-5 (hot-folder identity key) and F-7
(per-event full-tree walk). Base `406dfa25`, branch `feat/findings-code`,
worktree `D:/pdf/pdf-f-code`, build dir `build-rel` (in-worktree only).

All test runs SERIAL (ctest default single-process), offscreen, from
`build-rel`. Per behavior change: fail-before RED, NC once (fix neutralized
→ its pins RED, guards green), pass-after ×3 SERIAL.

## Files

| File | What it is |
|---|---|
| `build-configure.log` | Runbook configure (Release, LTO, Tesseract, libsecret, no fixtures, no fuzz). CONFIGURE_RC=0. |
| `build-full-attempt1-killed.log` | Full-tree build attempt 1: externally killed (exit 137, the prior lanes' documented class) at [1082/1108]; its one FAILED entry is the F-15 pin's wrong signature (`writeNumberTree(path, style, pageCount, prefix)` — the path overload has no pageCount), caught mid-build and fixed before RED staging. Preserved verbatim. |
| `build-full-attempt2-resume.log` | Incremental resume after the kill: BUILD_RC=0, 0 FAILED. |
| `build-redtests.log` | Forced rebuild of `TestFillFormNoOp`/`TestHotFolder` (their binaries predated the pin edits — diagnosed via `-functions` showing the new slots missing; see "Stale-binary episode" below). BUILD_RC=0. |
| `red-pagelabels.log` | **F-15 fail-before RED**: 23 passed, 1 failed — `writeNumberTree_prefixNonAsciiIsUtf16BETextString` fails at the byte-level oracle (stored /P lacks the UTF-16BE BOM). Notably Oracle 1 (pdfium `QPdfDocument::pageLabel`) PASSES pre-fix: pdfium leniently decodes the raw UTF-8 bytes, which is exactly why the spec-byte oracle is load-bearing. |
| `red-fillform.log` | **F-1 fail-before RED**: 17 passed, 3 failed, each for the finding's named reason — BOM'd hex /V refused ("FDF hex string value: invalid UTF-8 byte sequence"), BOM-less hex /V imported as NUL-padded mojibake (`\x00H\x00i`), BOM'd octal-escaped literal refused. Guard `invalidUtf8HexValueStillRefused` green pre-fix. |
| `red-hotfolder.log` | **F-5 + F-7 fail-before RED**: 23 passed, 2 failed — `pollingRewriteForgedMtimeDifferentSizeReingests` (rewrite with forged-identical mtime + different size never re-ingests) and `controllerBulkFileEventsDoNotWalkPerEvent` (4 file events = 4 synchronous full-tree walks). |
| `build-fix.log` | Fix stage build: BUILD_RC=0, 0 FAILED. |
| `pass-1.log` | **Pass-after ×1** (pre-NC): 3/3 suites green. |
| `build-nc.log` | NC stage build (all four fixes neutralized): BUILD_RC=0. |
| `nc.log` | **NC once per fix**: TestPageLabels 23/1 (F-15 neutralization caught by its pin), TestHotFolder 23/2 (F-5 + F-7 neutralizations caught), TestFillFormNoOp 17/3 (F-1 neutralization caught by all three pins). Guards stay green under NC (`invalidUtf8HexValueStillRefused` passes). No collateral reds. |
| `build-restore.log` | Fixes restored (all `NC STAGE` markers removed, verified by grep): BUILD_RC=0. |
| `pass-2.log` | **Pass-after ×2** (post-restore): 3/3 suites green. |
| `pass-3.log` | **Pass-after ×3** (serial): 3/3 suites green. |
| `build-full-final.log` | Closing full-tree incremental build: BUILD_RC=0, 0 FAILED. |
| `full-gate.log` | Closing full serial ctest gate (all suites). |

## F-15 oracle note (why two oracles)

The finding's trap is that a raw-UTF-8 /P self-roundtrips through any UTF-8
decoder — including PoDoFo's `GetString()` and this suite's own
`readNumberTree`. The pin therefore asserts (a) pdfium's spec read
(`QPdfDocument::pageLabel`) AND (b) the stored string's RAW bytes carrying
the FE FF BOM. (b) is the load-bearing, spec-literal check; (a) documents
that lenient consumers mask the noncompliance (it passed pre-fix). PoDoFo's
`GetRawData()` on a loaded string returns the unevaluated file bytes, so (b)
reads the stored form directly.

## Stale-binary episode (documented, not silent)

The first `TestFillFormNoOp` RED attempt reported RC=0: the binary predated
the pin edits (attempt-1 compiled the file before the edits landed; the
post-kill resume linked the stale obj — exe mtime newer than source, ninja
saw no change). Diagnosed by `TestFillFormNoOp.exe -functions` not listing
the new slots; both targets force-rebuilt (`build-redtests.log`), the RED
re-captured (`red-fillform.log`, real refusal reasons). The NC stage
independently re-proves pin detection, so the staged evidence does not rest
on the stale run.

## Residuals (by design, documented in code comments)

- F-5: a rewrite preserving BOTH size and mtime is still one ingest per
  session (poll tick cadence is the feature's contract; content hashing was
  explicitly out of the mission's minimal scope).
- F-7: the refresh runs once per debounce fire; QFileSystemWatcher's signal
  carries no per-event cause, so "walk only on directory creation" is not
  expressible over this API — the verifier's primary direction ("move the
  refresh inside the debounce fire") is what shipped.
- F-1: PDFDocEncoding high-bit bytes (0x80–0xFF outside UTF-8) still refuse;
  the mission scoped the fix to the UTF-16BE forms.
