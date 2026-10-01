# LANE REPORT — page-labels /P prefix (wave-2b follow-up)

- **Date:** 2026-10-01
- **Lane:** GlyphPDF implementation lane (small follow-up from the wave-2b page-labels CHECK-FIRST)
- **Worktree:** `D:/pdf/pdf-w2b-plprefix` — branch `feat/page-labels-prefix`, base `606397b1`
- **Status:** COMPLETE

## 1. CHECK-FIRST result

**Negative — /P support did NOT exist on the base.** Proof at base `606397b1`:

- `grep -rn '"/P"' src/core/PageLabels.cpp` → no hits; no `prefix` token anywhere
  in `src/core/PageLabels.{h,cpp}` (only an unrelated comment about alphabetic
  letter-cycle prefixes at PageLabels.cpp:45).
- No prefix code in any label-related UI path (`onApplyPageLabels` prompted only
  start value + style).
- `tests/TestPageLabels.cpp`'s `readNumberTree` decoded only `S`/`St` and
  ignored `P`.
- Base `606397b1` = origin/main HEAD at lane start; the writer had landed at
  `1991d9c1` exactly as the folding lane reported (one uniform range, no /P).

## 2. What was built

### Writer — `src/core/PageLabels.h` / `src/core/PageLabels.cpp`

- `PageLabelNumEntry` gains `QString prefix` (default empty), included in the
  entry `operator==`.
- `numberTreeEntries(startValue, style, pageCount, prefix = QString())` — new
  defaulted parameter; the entry carries the prefix.
- `writeNumberTree(doc, startValue, style, pageCount, prefix = QString())` —
  writes the range's `/P` as a **PDF text string** (`PdfString` from UTF-8)
  **only when the prefix is non-empty**. ISO 32000 Table 159 semantics: the
  prefix precedes the computed number. An empty prefix never produces a `/P`
  key (absent, not empty-valued).
- `writeNumberTree(pdfPath, startValue, style, prefix = QString())` — passes the
  prefix through, and the G13 SafeSave candidate **validation now also demands
  the written `/P` matches the request exactly**: present iff the prefix was
  non-empty, byte-equal when present (`FindAt(1)` on the flat `/Nums` array).
- Stale-tree replacement, explicit `/St`, and the G13 transaction shape are
  untouched. All defaulted parameters keep every pre-existing call site and the
  six shipped writer pins compiling and green unchanged.

### Reader

There is **no production reader** (PoDoFo 1.1.0 has no page-label read API —
the shipped writer test documents this). The reader in scope is the test-side
decoder `readNumberTree` in `tests/TestPageLabels.cpp`: it previously ignored
`/P`; it now decodes `/P` into `entry.prefix` (empty when the key is absent)
via `PdfObject::GetString().GetString()` (UTF-8 `std::string_view`). Round-trip
is pinned by the new pins.

### UI — `src/modes/PagesMode.cpp` `onApplyPageLabels`

- New optional **Prefix** `QLineEdit` in the Apply Page Labels dialog
  (placeholder "none", tooltip explaining the semantics: "A-" + Decimal from 1
  labels pages "A-1", "A-2", …; leave empty for no prefix).
- The field text is passed straight through to
  `writeNumberTree(candidate, startValue, style, prefixText)`; an empty field
  yields an empty `QString` and the writer omits `/P` entirely.
- Atomic boundary unchanged: SafeSave candidate → write → validated commit →
  re-point resident engine + reload.

## 3. Pins added (`tests/TestPageLabels.cpp`, following the 6 writer tests' pattern)

1. `writeNumberTree_prefix_readback` — write→re-read with prefix: doc overload,
   `{0, "R", 4, "Fig."}` round-trips through the saved file (full entry
   equality vs `numberTreeEntries(..., "Fig.")`), computed part regenerates
   unchanged ("IV","V","VI") and prefix+first computed composes to "Fig.IV".
2. `writeNumberTree_emptyPrefixNotWritten` — empty prefix: raw PoDoFo check
   that the saved range dict carries `/S` and `/St` but **no `/P` key at all**;
   decoded readback equals the default (prefix-less) expectation.
3. `writeNumberTree_prefixStyleComposition` — "App-" + Decimal from 7 via the
   path overload: entry `{0,"D",7,"App-"}` round-trips; composed labels
   "App-7"…"App-10".
4. `writeNumberTreePrefixSafeSaveRoundTrip` — full PagesMode staged flow
   (content-bearing fixture → `makeUniqueCandidate` → copy → label with
   "pref-" + LowercaseRoman → commit): committed original carries
   `{0,"r",1,"pref-"}` and the page content survives.

## 4. R7 evidence (all runs SERIAL, single process, offscreen, third_party DLL dirs prepended)

Evidence: `docs/audit/evidence-page-labels-prefix/` (see its README for the
file-by-file map).

- **fail-before RED** (`test-red.log`): built the API surface WITHOUT the /P
  emission (BUILD_RC=0) → the 3 prefix-writing pins failed for the right
  reason (written tree prefix "" ≠ expected "Fig."/"App-"/"pref-");
  `writeNumberTree_emptyPrefixNotWritten` was green **by design** — it is a
  guard pin (a writer that never writes /P trivially omits it; its job is NC
  detection of an always-write regression, cf. the dual-pin pattern). All 20
  pre-existing pins stayed green, proving the API extension is behavior-neutral.
  Totals: 20 passed, 3 failed.
- **NC once** (`test-nc.log`): commented the emission out, rebuilt → the same
  3 pins went RED again; 2 of them through the file overload's new
  /P-matches-request validation gate ("returned FALSE"), proving that gate
  catches a non-emitting writer. No false alarms anywhere else. Totals:
  20 passed, 3 failed. Emission restored.
- **pass-after ×3 serial** (`test-pass-{1,2,3}.log`): 23 passed, 0 failed,
  RC=0 — three consecutive runs, first-try each.
- No existing test was weakened; the six shipped writer pins are untouched and
  green throughout.

## 5. Deviations / fixes during the lane (Rule 1, recorded)

1. Test-authoring bug caught in RED staging: the raw catalog check asserted
   `labels->IsReference()`, but PoDoFo 1.1 `FindKey` may return the resolved
   dictionary; fixed to accept both shapes (mirrors the production
   validation), and the recorded RED run is post-fix.
2. Wrong expectation inside the new `writeNumberTree_prefix_readback` pin
   itself (Roman startValue 4 → "IV","V","VI", not "I","II","III"); the
   structural /P round-trip assert had already passed. Fixed; superseded log
   kept as `test-green-1.log`.

## 6. Build

- Configure: per runbook (Release, LTO, Tesseract, libsecret, no fixtures, no
  fuzz) — CONFIGURE_RC=0.
- Targeted builds (TestPageLabels + PdfWorkstation) at each R7 stage:
  BUILD_RC=0 every time (`build-red.log`, `build-green.log`, `build-pass.log`,
  `build-nc.log`).
- Full-tree build: attempt 1 (`-k 0 -j 2`) killed externally (exit 137, 0
  FAILED entries, died in LTO linking — 10 lanes share this machine); preserved
  as `build-full-attempt1-killed.log`. Attempt 2 (incremental resume) got to
  the last ~33 steps, was killed externally again (exit 137) and logged 2
  transient LTO link failures (`lto-wrapper failed` on TestTextEditStyle.exe
  and TestCheckedMutationCoverage.exe — the exact runbook transient-LTO class);
  preserved as `build-full-attempt2-killed.log`. Attempt 3 (incremental
  resume): **BUILD_RC=0** — both previously failing exes linked, all targets
  built (`build-full.log`, cumulative with attempt 2's tail; the 2 FAILED
  lines in it belong to attempt 2).

## 7. Constraints honored

No push, no merge to main, no branch/worktree deletion, no rebase, no stash;
all work confined to `D:/pdf/pdf-w2b-plprefix`; build dir strictly
`build-rel` inside the worktree; no CLAUDE.md/SECURITY.md created.
