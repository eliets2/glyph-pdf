# LANE REPORT — findings-code wave-2c round 2 (native-adversary wave-2b fixes)

- **Date:** 2026-10-02
- **Lane:** GlyphPDF findings-fix lane (wave-2c round 2, code hardening)
- **Worktree:** `D:/pdf/pdf-f-code` — branch `feat/findings-code`, base `406dfa25`
- **Verifier report:** `D:/pdf/verification/native-adversary-wave2b.md` (F-1, F-5, F-7, F-15)
- **Status:** COMPLETE — 4/4 missioned findings fixed, R7 evidence complete, full serial gate 201 passed / 0 failed (+ R14ProbeBatchSkip disabled by design, the documented prior state)
- **Commits:** `a78074d0` fix(findings-code) — code + pins; docs commit — this report + `docs/audit/evidence-findings-code/`

## 1. What was fixed

### F-15 — /P prefix written as raw UTF-8 (`src/core/PageLabels.cpp`)

ISO 32000 §7.9.2.2 requires a TEXT string to be PDFDocEncoding or UTF-16BE
with the FE FF BOM. The writer handed PoDoFo `PdfString(UTF-8)`, which emits
the raw UTF-8 bytes as a simple string: it self-roundtrips through every
UTF-8-speaking in-tree gate, but a spec-conforming viewer decodes the bytes
as PDFDocEncoding and renders mojibake.

Fix:

- ASCII-safe prefixes (≤ 0x7F — PDFDocEncoding is ASCII-identical there)
  keep the byte-identical literal form; every pre-existing pin ("Fig.",
  "App-", "pref-") is untouched and byte-compatible.
- Anything else is written as the UTF-16BE+BOM text string via
  `PdfString::FromRaw(..., /*hex=*/false)` (writer escapes literal
  metacharacters; PoDoFo re-reads the raw bytes back).
- The path overload's SafeSave readback gate now decodes /P per §7.9.2.2
  (BOM → UTF-16BE, else UTF-8) instead of blindly `fromUtf8` — the blind
  decode was exactly what let the raw-UTF-8 writer self-certify.
- Test-side `readNumberTree` decodes /P with the same spec-aware rule, so
  the suite can no longer mask the storage form.

Pin: `writeNumberTree_prefixNonAsciiIsUtf16BETextString` uses TWO oracles —
pdfium's `QPdfDocument::pageLabel` (a real third-party consumer) and the
stored string's RAW bytes carrying the BOM. Empirical finding worth keeping:
**pdfium passes pre-fix** (it leniently decodes raw UTF-8), which is why the
raw-byte oracle is the load-bearing spec check.

### F-1 — strict-UTF-8 gate refuses spec-conforming FDF (`src/engines/FormManager.cpp`)

FDF field names/values are §7.9.2.2 text strings; Acrobat exports UTF-16BE
(hex with the FE FF BOM, or literal `\376\377` octal escapes). The
strict-UTF-8 decoder refused exactly those files (the feature inverted for
its most common producer), and a BOM-less UTF-16BE payload slipped through
strict UTF-8 as NUL-padded mojibake (`"\x00H\x00i"`).

Fix: new `decodePdfTextString` used at BOTH string-value decode sites (hex
and literal): FE FF BOM → UTF-16BE (BOM stripped before the fixed-endian
decode); else even-length all-zero-high-byte payloads with at least one
non-zero low byte → BOM-less UTF-16BE; anything else → strict UTF-8. A
truncated UTF-16BE payload refuses with a typed ErrorInfo. The
container-level strict-UTF-8 gate is intentionally unchanged (the mission
scoped the fix to the string decoder), so `nonUtf8BytesAreRefused` stays
exactly as pinned.

Pins: BOM'd hex /V round-trips; BOM-less hex /V decodes (not NUL mojibake);
BOM'd octal-escaped literal /V round-trips; guard `invalidUtf8HexValueStillRefused`
(0xC3 without continuation) keeps refusing, fail-closed.

### F-5 — hot-folder identity key (`src/modes/HotFolderController.cpp`)

path+mtime is not content identity on the exact NAS shares the polling mode
targets (cp -p / archive restore / robocopy /COPYTIMES; 1–2 s timestamp
granularity): a replaced file shared the old key and was silently never
re-ingested.

Fix (minimal, per the verifier's direction and the mission): **size joins
the identity key** (`path|mtime|size`) — every length-changing rewrite is a
new identity in both modes. Documented residual, by design: a rewrite
preserving BOTH size and mtime is still one ingest per session (poll tick
cadence is the feature's contract; content hashing was explicitly out of the
mission's minimal scope). The m_processed set keeps its consume-once
semantics; no other consumer reads the key format.

Pin: `pollingRewriteForgedMtimeDifferentSizeReingests` — rewrite in place
with a forged-identical mtime and different size; premise-checked (mtime
byte-equal, size differs); must re-ingest in polling mode.

### F-7 — full-tree watch walk on every fs-event (same file)

`onDirectoryChanged` walked the entire tree synchronously on EVERY event, on
the GUI thread, before arming the debounce — O(events × dirs) for any bulk
drop.

Fix: the refresh moved to the debounce fire (the verifier's primary
direction), running once per quiet window immediately before the ingest pass
— which already re-walks the same tree — so N events in one window cost one
debounced walk, and nothing walks before the debounce arms. Honest deviation
from the mission's phrasing: QFileSystemWatcher's `directoryChanged` signal
carries NO per-event cause, so "only walk when the event indicates a
directory creation" is not expressible over this API; the shipped design
achieves the finding's performance demand deterministically. Discovery of
directories created during a storm still happens (at the fire, ≤ debounce
window later) — `controllerDiscoversSubdirCreatedAfterStart` still pins it,
now after the debounce window (timing adapted to the deliberate behavior
change; assertion strength kept — this is the pin-flip class the lane rules
document, cf. subdirectoryPdfNotIngested).

Pin: new test seam `watchWalkCountForTest()`; `controllerBulkFileEventsDoNotWalkPerEvent`
drives 4 file-only events in one window → asserts 0 synchronous walks,
exactly 1 walk at the fire, 1 debounce pass, 1 delivery of all four drops.
Fully deterministic — no timing proxy needed.

## 2. R7 evidence (all runs SERIAL, offscreen, `build-rel` only)

Evidence: `docs/audit/evidence-findings-code/` (README there maps every file).

| Stage | Result |
|---|---|
| fail-before RED | `red-pagelabels.log` 23/1 (F-15, byte-oracle), `red-fillform.log` 17/3 (F-1 ×3, each the finding's named reason), `red-hotfolder.log` 23/2 (F-5 + F-7). All pre-existing pins green throughout. |
| NC once per fix | `nc.log`: all four fixes neutralized → exactly their own pins RED (TestPageLabels 23/1, TestHotFolder 23/2, TestFillFormNoOp 17/3); guards green under NC (`invalidUtf8HexValueStillRefused`); zero collateral reds. Fixes restored (grep-verified no `NC STAGE` markers), then rebuilt BUILD_RC=0. |
| pass-after ×3 SERIAL | `pass-1.log` (pre-NC), `pass-2.log`, `pass-3.log` (post-restore): 3/3 suites green each run, first-try each. |
| Closing gate | `build-full-final.log` BUILD_RC=0; `full-gate.log`: **201 passed, 0 failed** out of 201 run (202nd entry R14ProbeBatchSkip "Not Run (Disabled)" — the same disabled-by-design state as the wave-2b closing gate). |

No existing test was weakened. The only pre-existing pin modified is
`controllerDiscoversSubdirCreatedAfterStart` (added the debounce-window wait
for the F-7 behavior change; the discovery assertion itself is unchanged).

## 3. Deviations / fixes during the lane (Rule 1, recorded)

1. **Test-authoring bug caught mid-build:** the F-15 pin first called
   `writeNumberTree(path, style, pageCount, prefix)` — the path overload has
   no pageCount (attempt-1 build FAILED on it and was externally killed
   later the same run, exit 137, the documented contention class; preserved
   as `build-full-attempt1-killed.log`). Fixed to the real signature before
   RED staging; the resume built clean (`build-full-attempt2-resume.log`).
2. **Stale-binary episode:** the first TestFillFormNoOp/TestHotFolder RED
   attempt ran binaries that predated the pin edits (compiled pre-edit in
   attempt 1; the post-kill resume linked stale objs). Diagnosed via
   `-functions` not listing the new slots; force-rebuilt
   (`build-redtests.log`) and RED re-captured. The NC stage independently
   re-proves pin detection, so nothing rests on the stale run.
3. **F-7 design deviation** (mission phrasing vs API reality): debounced
   refresh instead of per-cause walking — see §1; the verifier's own primary
   fix direction is what shipped.
4. **Oracle discovery (F-15):** pdfium accepts raw-UTF-8 /P leniently, so
   `QPdfDocument::pageLabel` alone cannot police the spec; the raw-byte BOM
   oracle carries the pin (both asserted).

## 4. Build

- Configure per runbook (Release, LTO, Tesseract, libsecret, no fixtures, no
  fuzz): CONFIGURE_RC=0 (`build-configure.log`).
- Full-tree build: attempt 1 externally killed (exit 137) at 1082/1108;
  incremental resume BUILD_RC=0; fix / NC / restore stage builds all
  BUILD_RC=0; closing full-tree build BUILD_RC=0 (`build-full-final.log`).
  The one FAILED in attempt 1's log is the pin-signature compile error of
  item §3.1.

## 5. Constraints honored

Work confined to `D:/pdf/pdf-f-code`; build dir strictly in-worktree
`build-rel`; no push, no merge/rebase to main, no stash/gc, no branch or
worktree deletion; no CLAUDE.md/SECURITY.md created; no other worktree
(`D:/pdf/pdf` et al.) touched.
