# Evidence — Selective Sanitize + pre-commit summary (feat/sanitize-selective)

PARITY-SCORECARD-2026-09-30 §4 item #4 (July rows 72-73, one work item).
Base: `02d1a898` (main at branch creation); rebased onto `main` = `181247b2`
(main moved twice during the lane — 38102a23 M-3 consent gate, then 181247b2
reading-order depth cap; rebase clean, full rebuild + full serial gate
re-verified on the rebased tip). Suite: `TestSanitizeSelective` (9 pins; first
run of this suite — the sanitize-adjacent neighbors run beside it in every
round).

## What landed

- **Engine** (`PoDoFoBackend`): `sanitizeDocumentContents` became ONE traversal
  (`sanitizeWalk`) with two modes — classify (mutate=false: per-category counts
  + item labels, touches nothing) and remove (mutate=true: same walk, removes
  only the selected categories, reports what actually went into `removedOut`).
  The one-argument `sanitizeDocument(path)` is exactly "all categories" — the
  all-or-nothing default is unchanged, as is the Compress strip path (it calls
  the same walk with all categories, §9.13 row 88 contract kept). The trailer
  /ID refresh is not a category and runs only in the removal pass (also why
  sanitize outputs are not byte-comparable).
- **Interface**: `IPdfEditorEngine::sanitizeClassify()` +
  `sanitizeDocument(path, selection, removedOut)`; empty selection refuses at
  the engine boundary (no output written, honest ErrorInfo).
- **UI**: `SanitizeSummaryDialog` (pre-commit summary, per-category checkboxes,
  all-checked default = today's behavior, "will remain unchanged" line, honest
  zero-checked refusal) wired into `SecurityController::sanitizeDocument`:
  analyze worker → summary dialog → selective commit worker → proof-carrying
  completion message ("Removed: …" from the post-run plan). A clean document
  refuses honestly ("nothing to remove") instead of rewriting the file.
- **Helper**: `SanitizeDocumentHelper::classify` + selective `execute`
  overload; the legacy `execute` still routes through the one-argument engine
  entry (legacy counters/behavior untouched).

## The pins (TestSanitizeSelective)

1. `classifyReportsExactPerCategoryCountsOnAllNineDirtyCategories` — fixture
   dirty in all nine categories classifies to exact counts (Metadata 4 =
   3 /Info keys + catalog XMP; Attachments 2 = legacy.doc + notes.xlsx;
   Annotations 1; FormFields 1; JavaScriptActions 5; HiddenLayers 1;
   Bookmarks 2; PrivateData 1; StructureAltText 2) and names the items.
2. `classifyWalkDoesNotMutateTheDocument` — double classify is stable; the
   source file is byte-identical (SHA-256) after two classify runs.
3. `uncheckingAttachmentsRemovesEverythingElseAndPreservesTheFiles` — selective
   run keeps both embedded payloads (decode-level read-back) and the 2-entry
   name tree; removes everything checked; the removed-report never claims the
   preserved category.
4. `uncheckingBookmarksRemovesEverythingElseAndPreservesTheOutline` — second
   axis (bookmarks kept, titles read back) proving the mapping is not
   hardcoded to one category.
5. `allCheckedSelectiveRunMatchesLegacyFullSanitizeOutcome` — legacy
   one-argument run vs all-checked selective run: both outputs classify to the
   same honest sanitized shape (zero user data anywhere; the only legal
   remainder is the save-re-stamped /Info /ModDate; hidden layers stay present
   but forced OFF — G4 hidden-stays-hidden, verified via /D /ON empty in both).
   Byte equality is not the contract (the /ID second element is randomized by
   design on every sanitize save).
6. `emptySelectionRefusedAtEngineBoundaryWithoutWritingOutput` — zero-checked
   refuses at the engine: no output file, empty removed-report, honest
   ErrorInfo naming "nothing"; the legacy one-argument path still commits.
7. `summaryDialogDefaultsToAllCheckedAndReflectsThePlan` — one checkbox per
   found category, all checked, counts + item names in the labels,
   "Will remain unchanged: …" line.
8. `summaryDialogSelectedCategoriesFollowTheCheckboxes` — the checkbox state
   is exactly the commit selection.
9. `summaryDialogRefusesZeroCheckedCommitWithHonestMessage` — zero-checked
   refusal names what would (not) happen and the way out; a clean plan refuses
   with "Nothing removable was found…".

## Fail-before (RED, 01-fail-before-TestSanitizeSelective.log)

6 passed, 5 failed — pins 1-5 RED:
- classify pins: the seam existed but the classify mode did not (empty plan).
- selective pins: the seam refused every selective commit (honest inert).
- equivalence pin: RED at the selective leg.
Refusal pins (6, 9) and the dialog pins (7, 8) are GREEN at fail-before by
construction: the dialog consumes a plan (pinned with hand-built data), and
the inert seam's refusal is already honest — they guard regressions, not the
new behavior.

## NC (scoped revert, recorded once — 05-nc-reverted-TestSanitizeSelective.log)

Walk reverted to the base region (`02d1a898` `sanitizeDocumentContents` +
original one-argument `sanitizeDocument`) with the stub seam kept (classify →
empty plan; selective → honest refusal) → 6 passed, 5 failed, same failure
modes as fail-before. Re-applied: `06-restored-after-nc-TestSanitizeSelective.log`
→ 11 passed, 0 failed.

## Pass-after ×3 (serial — 02/03/04-pass-after-serial-round{1,2,3}.summary.txt)

ctest serial, `-R "TestSanitizeSelective|TestSanitization|TestSanitizeTrailerUaf|
TestCompressStripSanitize|TestRedactSanitizeBundle"`: **100% tests passed, 0
failed out of 5**, three rounds.

## Repair notes

- None of the pre-existing pins were weakened. `TestSanitization`'s mock pins
  caught one real routing bug during the lane: the legacy helper initially
  re-routed through the new overload, so the mock's legacy counter stopped
  incrementing; the legacy `execute` now keeps the one-argument engine entry
  (RED → fixed → green in the same session, before the recorded rounds).
- Post-sanitize classification is NOT all-zero by design: the save re-stamps
  `/Info /ModDate` (1 Metadata item) and hidden layers stay present but forced
  OFF (G4). Pin 5 asserts exactly that shape for both outputs instead of a
  dishonest `empty()` check.

## Owner-surfaced items

- None new. (Pre-existing: `sanitizeDocument` still has no per-item
  preview inside a category — counts + representative labels only; Acrobat
  shows item-level checkboxes inside Metadata. The category granularity is the
  scorecard's dispatch shape.)
