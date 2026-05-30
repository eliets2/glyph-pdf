# M4-PROMPT-6 Walkthrough — Edge Fixes (D4 only: Prune Missing Recents)

**Date:** 2026-05-30
**Result:** Shipped — D4 (prune missing recent files) fully closed
**Commits:** cf68514 (D1+D2) · 1cb3dd5 (D3-MEMORY PreferencesDialog)
**Test count:** unchanged (24/24 — no new test targets; existing tests cover HomeController)

---

## Note on scope

Original M4-PROMPT-6 had 4 deliverables (D1-D4). Only D4 was genuinely unimplemented:
- **D1** (Strikeout/Squiggly real ToolModes): verified in-place from prior sessions ✅
- **D2** (Share via MAPI): verified in-place (`HomeController::onShare` at `HomeController.cpp:124`) ✅
- **D3** (click-to-place form fields): shipped in M3-PROMPT-1 D2 ✅
- **D4** (prune missing recent files): was NOT implemented — this prompt's scope

In PROMPT 3 of MONTHS-2-8-PROMPTS.md, D4 was re-framed as 3 sub-deliverables (D1/D2/D3-MEMORY).

---

## COMPLETED

### Sub-D1 — Click-prompt on missing file (WelcomeWidget)

**Verified in-place.** `WelcomeWidget::onRecentItemClicked` at `src/ui/WelcomeWidget.cpp:284` already:
- Checks `QFileInfo::exists(path)` before opening
- Shows `QMessageBox::question("File Not Found", ...)` with Yes/No
- Removes from `m_recentFiles` + QSettings on Yes
- Emits `removeRecentFileRequested(path)`

This was part of the original WelcomeWidget implementation — no new code needed.

**New code added:** `HomeController::removeFromRecents(path)` public method (per spec, for future
MainWindow wiring when WelcomeWidget is surfaced as the app home screen).

### Sub-D2 — "Prune Missing" menu entry

- `HomeController::pruneMissingRecents()` — iterates `QSettings("recentFiles")`, filters missing via `QFileInfo::exists()`, saves result, returns count removed.
- `MainWindow::pruneMissingRecents()` — delegates to `_home->pruneMissingRecents()`, calls `_menu->refreshRecentFiles()`, shows `_status->showMessage("Removed N missing recent file(s).")`.
- `MenuBar::refreshRecentFiles()` — new "Prune Missing Files" entry added just before "Clear Recent Files":
  - Disabled when no missing files (`missingCount == 0`)
  - Enabled with tooltip showing count when missing files exist
  - On click: calls `mainWindow->pruneMissingRecents()`
- Auto-prune on startup: in `GpMainWindow` constructor, checks `QSettings("recent/autoPrune", false)` and silently calls `_home->pruneMissingRecents()` + refreshes menu if true.

### Sub-D3 (D3-MEMORY) — PreferencesDialog auto-prune toggle

- `PreferencesDialog.h`: added `QCheckBox* _autoPrune = nullptr`
- `PreferencesDialog.cpp` General tab: added checkbox "Auto-prune missing recent files on startup" (default unchecked), persists to `QSettings("recent/autoPrune")`
- Placed after the Autosave Interval row in the General group

---

## DEFERRED

- **WelcomeWidget ↔ MainWindow wiring**: `WelcomeWidget::removeRecentFileRequested` signal is not yet connected to `HomeController::removeFromRecents()` in `GpMainWindow`. WelcomeWidget handles QSettings inline, which is functionally correct. Full wiring deferred to M6-PROMPT-6 (edge-case cleanup pass) when WelcomeWidget is surfaced as the app home screen.

---

## Outstanding work

M4-PROMPT-6 is now fully closed. All 4 original deliverables accounted for (D1/D2/D3 verified-in-place; D4 implemented in this prompt).

---

## Commits

1. `cf68514` — `feat(welcome): prompt-to-prune missing recents on WelcomeWidget click + Prune Missing menu entry + HomeController::pruneMissingRecents (M4-P6 D4 partial)`
2. `1cb3dd5` — `feat(prefs): auto-prune setting + D3-MEMORY vault refresh (M4-P6 D4 final + completion)`

---

## Lessons captured

No new patterns. Pattern 14 (architectural ambiguity): WelcomeWidget signals not yet wired to MainWindow was a known gap, documented in deferred section rather than silently skipped.
