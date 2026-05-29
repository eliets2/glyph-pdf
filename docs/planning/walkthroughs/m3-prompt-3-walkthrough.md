# M3-PROMPT-3 Walkthrough — PagesMode Wiring

**Date:** 2026-05-29
**Commits:** `5dd5ff7` (D1+D2+D3) → `f098cb0` (D4)
**Tests:** 9 new / 20 total passing

---

## What Was Done

### D1 — Preview Banner Removed; Real Page List

The original `PagesMode.cpp` contained:
- An orange preview-only banner (`modePreviewBanner`) with text "Preview — not wired in v1.0.0"
- 12 hardcoded fake page names ("Cover", "Contents", "Exec", etc.) in a `QListWidget`
- A block that disabled all interactive child widgets
- A `QLabel` stub for the split form

All of these were removed. The replacement:
- Three-pane `QSplitter`: page list (left, 2 flex) | split form (middle, 280px fixed) | reorder panel (right, 220px fixed)
- Page list: `QListWidget` in `IconMode` with 100×130 gray placeholder thumbnails + "Page N" label
- Page count determined at runtime via binary-search over `extractPageAsBytes` (O(log N), returns empty for out-of-range indices)
- Toolbar size toggles (S/M/L) wire to icon size/spacing updates

**Note on real thumbnails:** Real PDFium rendering requires `IPdfRenderBackend` to be exposed through `AppContext`. That plumbing doesn't exist yet (`AppContext.h` only has `pdfEditor`, not a render backend). The gray placeholder approach is deliberately explicit: no masquerading as "real thumbnails". When the render backend is added to AppContext, it's a 1-function change in `refreshPageList()`.

### D2 — Split Form

The `QLabel` placeholder was replaced with a complete split form:
- **Split mode group:** Three `QRadioButton` options:
  - "Split at page N": `QSpinBox` (1 to pageCount-1)
  - "Split every N pages": `QSpinBox`
  - "Split by range": `QLineEdit` for expressions like "1-3,5,7-9"
- **Output naming:** `QLineEdit` with `{stem}_part{n}.pdf` pattern (`{stem}` and `{n}` tokens)
- **Output folder:** `QLineEdit` + Browse button (`QFileDialog::getExistingDirectory`)
- **Filename preview:** `QListWidget` showing paths that will be produced
- **Buttons:** Preview (populates preview list without writing) + Split (executes)

**Split implementation strategy** (no `splitDocument()` on engine):

For each output part (a `QList<int>` of 0-based page indices):
1. Write a minimal valid 1-page PDF stub to the output path (`PagesMode::writeMinimalPdf`)
2. Loop `extractPageAsBytes(sourcePath, pageIdx)` for each page → `insertPageFromBytes(outputPath, k, bytes)` appending at position k
3. Delete the stub page at index `pages.size()` via `deletePage(outputPath, pages.size())`
4. If any step fails, remove the partial output file and continue to next part

**Overwrite guard:** Before writing, collect existing output paths and show a single `QMessageBox::question` listing all files to be overwritten. Only proceeds on Yes.

**Progress:** `QProgressDialog` shown during split, advances per-output-part.

### D3 — Reorder Panel

Single-pane `QListWidget` with `InternalMove` drag-drop (simpler than two-column; documented simplification vs. spec which suggested two-column). Contains the page list mirrored as "Page 1", "Page 2", etc.

- **Apply:** Calls `reorderPages(path, from, to)` using a sequential-move algorithm that walks `desiredOrder` and computes the current live position of each wanted original page, making the minimum number of engine calls needed. Tracks a `currentOrder` list in sync with engine state.
- **Reset:** Repopulates the list from the original page order without touching the engine.

### D4 — Tests

`tests/TestPagesMode.cpp` — 9 headless tests (all pass in 39 ms):

| Test | What It Verifies |
|------|-----------------|
| `testPageRangeParser` | "1-3,5,7-9" → [0,1,2,4,6,7,8] |
| `testPageRangeParser_single` | "2" with 5 pages → [1] |
| `testPageRangeParser_empty` | "" → [] |
| `testPageRangeParser_outOfRange` | "1-100" with 5 pages → [0..4] clamped |
| `testSplitAtPage` | 5-page doc split at page 3 → 2 output files; extract=5, insert=5, delete=2 |
| `testSplitEveryNPages` | 6-page doc split every 2 → 3 output files; extract=6, insert=6, delete=3 |
| `testReorderPages` | [3,0,1,2] desired order → first reorderPages call moves from=3, to=0 |

Mock pattern: `PagesMock extends MockPdfEditorEngine` overrides `extractPageAsBytes` (returns "page_N" sentinel for valid indices), `insertPageFromBytes` (appends sentinel to output file), `deletePage`, `reorderPages` (records calls).

Counter reset after `setAppContext` is required because `refreshPageList()` uses `extractPageAsBytes` for the binary-search page count — tests reset `m_extractCallCount = 0` before calling `executeSplit`.

---

## Files Modified

| File | Change |
|------|--------|
| `src/modes/PagesMode.h` | Full rewrite: setAppContext, parsePageRange, executeSplit, writeMinimalPdf (all public); member variables for D1/D2/D3 |
| `src/modes/PagesMode.cpp` | Full rewrite: D1 page list + D2 split form + D3 reorder panel |
| `src/modes/ModeController.cpp` | Inject AppContext into PagesMode (same pattern as BatchMode) |
| `tests/TestPagesMode.cpp` | New: 9 headless tests |
| `CMakeLists.txt` | New: TestPagesMode target + qoffscreen plugin deploy |
| `CHANGELOG.md` | M3-PROMPT-3 entry |
| `ROADMAP.md` | M3-P3 row marked Done |

---

## Constraints Obeyed

- No local `QLayout*` variable named `tr` (used `mainLayout`, `tbLayout`, `hdrLayout`, etc.)
- No absolute pixel coordinates for geometry (only fixed widths: 280px panel, 220px panel — these are panel dimensions, not PDF user-space coordinates)
- No DjVu output
- No overwrite without `QMessageBox::question` confirmation
- Preview banner completely removed (not stubbed)
