# CX-04 evidence — repeat Tag Document can deadlock the GUI thread

Deadlock: `AccessibilityPanel::onApplyClicked` answered a repeat Apply with
`cancel() + waitForFinished()` (and the destructor waited the same way) while
the tag worker does `BlockingQueuedConnection` hops BACK to the GUI thread —
SafeSave park/restore (`GpMainWindow.cpp` hopToGui) and the save-first prompt
(`confirmSaveBeforeInPlaceWrite`). The GUI blocked on a future whose worker
blocks on the GUI: both threads deadlocked. `hopToGui`'s safety comment
assumes a modal progress dialog that keeps the GUI in its event loop — Tag
Document has none.

Fix (this commit):
- `AccessibilityPanel::onApplyClicked` refuses a repeat Apply with an honest
  status message; the GUI thread never blocks on the tag future.
- `updateTagActionState` disables Tag (and Apply) while `m_tagRunning`.
- `onTagFinished` re-enables the controls BEFORE the ARC06 identity-tie
  discard, so a document switched mid-tag cannot leave them disabled forever.
- `~AccessibilityPanel` cancels the tag watcher WITHOUT waiting (the worker
  keeps its contract and finishes on its own; a destroyed watcher just stops
  receiving signals).
- New injected `setTagStateReader` reads EditPolicy read-only state, the
  viewer path and the dirty flag ON THE GUI THREAD before submission; the
  runner now receives that snapshot (`TaggerSessionState`) by value.
  `MainWindow::runA11yTag` no longer touches `pdfViewer()` or the session on
  the worker. The signed-document check stays file-derived inside the engine
  (preflight/transaction) — it reads the FILE, not a GUI-owned object, so it
  was already thread-honest; moving a file walk onto the GUI thread would be
  a regression.

Files:
- `fail-before-repeat-apply-deadlock.txt` — with the old blocking behavior
  temporarily re-introduced (source patch, reverted before committing), the
  new `repeatApplyRefusedWhileTagRuns` test hangs inside the test function
  until the external 60 s kill (exit=124): the GUI thread is blocked in
  `waitForFinished` while the worker sits on the barrier.
- `fail-before-close-panel-deadlock.txt` — same setup, close-panel test:
  hangs in `~AccessibilityPanel`'s `waitForFinished` (exit=124).
- `pass-after-full.txt` — the fixed code: full TestAccessibilityPanel suite
  `Totals: 16 passed, 0 failed, 0 skipped` (13 baseline + the 3 new CX-04
  tests). Related lanes on the same base: TestAccessibilityTagger
  18 passed / 1 skipped, TestSweepW3UxFlows 14 passed, TestViewingModes
  10 passed.

Tests (the spec's reproduction, pinned forever):
- `repeatApplyRefusedWhileTagRuns` — runner blocked on a barrier + second
  Apply: refused with a message, Tag/Apply disabled while running, GUI keeps
  processing events (heartbeat), exactly one runner invocation and one
  completion, controls re-enable.
- `closePanelMidTagNoDeadlock` — panel destroyed mid-tag: no deadlock, the
  orphaned worker finishes, no crash.
- `switchDocumentMidTagNoDeadlock` — document switched mid-tag: no deadlock,
  stale completion discarded (ARC06), action re-enabled.
