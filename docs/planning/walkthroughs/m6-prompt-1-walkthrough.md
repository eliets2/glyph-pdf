# M6-PROMPT-1 Walkthrough — DiffEngine LCS/Myers Upgrade

**Date:** 2026-05-30
**Result:** Shipped — D1-D5 complete
**Commits:** 887823d (D1-D3) · 9f48ca7 (D4) · b30d612 (D5 tests) · 9184d30 (CHANGELOG)
**Test count:** 24 → 25 (`TestDiffEngine` added)

---

## COMPLETED

### D1 — MyersDiff LCS implementation

`src/engines/MyersDiff.h/.cpp` (NEW).

Algorithm: Myers 1986 O((N+M)D) using the standard V-array forward pass with trace
snapshots, followed by backtracking to produce the ordered edit script. Time complexity
O((N+M)*D), space O((N+M)*D) for trace storage.

`struct EditOp { Type type; QString token; int indexA; int indexB; }`
`MyersDiff::compute(a, b) → QList<EditOp>`

Edge cases: empty sequences handled as pure insert / pure delete without the
main algorithm running.

### D2 — Move detection post-pass

`MyersDiff::detectMoves(edits) → QList<MoveOperation>`

Implementation: build two maps (deleted[token]→indexA list, inserted[token]→indexB list),
then for each token in both maps greedily pair earliest delete with earliest insert.
Only pairs where `fromIndex != toIndex` are classified as moves (same-position
delete+insert = replacement, not move).

`struct MoveOperation { QString token; int fromIndex; int toIndex; }`

### D3 — DiffEngine replacement

`DiffEngine.cpp` lines 48-69 (QSet set-difference) replaced with:
1. `MyersDiff::compute(words1, words2)` → edit script
2. `MyersDiff::detectMoves(edits)` → moves
3. `textRemoved` / `textAdded` populated from non-move edits (move slots consumed first)

`PageDiff` gains `QList<MoveOperation> moves` field. Existing `textRemoved`/`textAdded`
remain populated for backward compat with `CompareMode` change counts.

### D4 — CompareWidget text diff panel + orange moves + PREV/NEXT

`CompareWidget.cpp` rewrite:
- Added `QTextBrowser* m_textDiff` + navigation bar label at bottom of widget
- `setDiffResult()` calls `buildTextDiff()` → `buildHtml()` generating color-coded HTML:
  - 🟠 Orange `↔` with underline for moves (with `[moved from pos X → Y]` annotation)
  - 🟢 Green `+` for additions
  - 🔴 Red `−` with strikethrough for deletions
  - Grey pixel diff count
- Anchor-based navigation: each change block has a unique `chg0..chgN` anchor
- `nextChange()` / `prevChange()` scroll to next/prev anchor in `m_textDiff`
- Nav label shows "change N of M | ← → to navigate"

`CompareMode.cpp`:
- Removed `setEnabled(false)` + "Coming in v1.1" from PREV/NEXT buttons
- Connected PREV/NEXT to `m_compareWidget->prevChange()` / `nextChange()`
- `onDiffFinished()` tree entries now include moves count (`↔N moved`)
- Tree items with moves highlighted in orange (`#d97c00`)

### D5 — TestDiffEngine (10 tests)

`tests/TestDiffEngine.cpp` (NEW) — all pass:
1. `testMyersEmptyBoth` — empty × empty = empty edit script
2. `testMyersEmptyA` — all inserts
3. `testMyersEmptyB` — all deletes
4. `testMyersIdentical` — all keeps
5. `testMyersOneInsert` — single insert surrounded by keeps
6. `testMyersOneDelete` — single delete
7. `testMyersComplexLCS` — ABCABBA vs CBABAC; verifies LCS invariant
8. `testMyersEditScriptOrdered` — reconstruct B from inserts+keeps = verify correctness
9. `testMoveDetectNoMoves` — pure replacement, no moves
10. `testMoveDetectSingleMove` — single token physically moved
11. `testMoveDetectParagraphReorder` — "clause3" moved to front; legal-doc scenario ✅
12. `testDiffResultHasMoveField` — `PageDiff.moves` struct smoke test

---

## DEFERRED

- **Visual overlay on PDF pages**: the current implementation adds a text diff panel
  below the viewer pair. A future enhancement could overlay colored annotations directly
  on the rendered PDF page at the word positions (requires word bbox extraction from
  PdfiumBackend). Deferred to M6-PROMPT-6 edge-case cleanup or M7-PROMPT-2 bug bash.
- **Multi-word phrase move detection**: current `detectMoves()` operates on individual
  tokens. Phrases (e.g., an entire sentence moved) are detected as multiple single-token
  moves. No practical issue for the legal/compliance persona's word-level comparison, but
  phrase-level detection would improve UX for long paragraph reorders.

---

## CHANGELOG confirmation

Removed: "DiffEngine uses per-word set-difference rather than LCS/Myers — order changes
appear as add+delete pairs, not moves. Affects legal/compliance comparison persona."

Added: M6-PROMPT-1 section under `[Unreleased]`.

---

## Lessons captured

No new patterns. Pattern 1 compliance: `ctest --repeat-until-fail 3` run after D5.
