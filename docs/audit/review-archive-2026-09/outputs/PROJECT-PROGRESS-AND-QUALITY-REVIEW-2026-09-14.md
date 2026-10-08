# GlyphPDF progress and quality review — 2026-09-14

The project has advanced, but the latest work is not ready for a release-quality sign-off. Form execution and signing configuration have progressed, several earlier defects have committed corrections, and recovery evidence is better. Important fixes remain outside the pushed integration branch, the batch OCR findings remain open, and this review found new correctness problems.

## Baseline and scope

Compared the last handoff observation, **2026-09-13 14:49:21 UTC**, with fresh Git state after fetching `origin`. The new inventory was captured at **2026-09-13 22:52:19 UTC / September 14 in Beirut**. Source assessments use pinned commit IDs, not moving branch names. The September 13 preservation report remains the historical baseline for local-file recovery.

| Area | Current observation |
|---|---|
| Latest fetched `origin/feat/parity-glm` | `54d5bcdbe177a009695d813028c23f52f857887a`, available in `C:/Users/User/Projects/pdf-clean` |
| Local `feat/parity-glm` | Still `586d6e4`, checked out in `pdf-r15`; it has not followed the remote's latest 13 commits |
| `main` | Still `2b715f4`; this is not where recent product work is being integrated |
| Pending Pack A fixes | `feat/parity-glm-packafix` at `5ae4153`, in `pdf-inst`; six commits absent from the fetched integration tip |
| Pending OCR permission fix | `feat/parity-glm-resid2` at `9129788`, in `pdf-sec`; one commit beyond integration |
| Branch/worktree inventory | 70 local branches, 19 remote-tracking branches, 17 worktrees, six stashes |
| Change since handoff | Four additional local branches; no baseline ref or registered worktree is missing |
| Local application source | No uncommitted original application-source change found in the inspected worktrees. Build output, logs, vendor copies and the five historical staged-artifact indexes remain local |

There are **25 newly reachable commit objects** compared with the earlier branch inventory: **13 new commits and 12 recovered historical staging-chain commits**. The latter are recovered history, not new feature implementation. The remote's 13-commit advance also includes work previously present on other local branches; it should not be counted as 13 newly implemented packages.

Exact inventory and commit subjects: [snapshot.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/progress-2026-09-14/snapshot.json) and [newly-reachable-commits.txt](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/progress-2026-09-14/newly-reachable-commits.txt).

## What progressed

| Workstream | Progress | Acceptance status |
|---|---|---|
| Form JavaScript | New no-engine build evidence; Validate execution on the fill/import text-field path; configure-time QuickJS version checks; clearer session-only stale-state scope | Implemented in the integration history. Validation does not cover all interactive edits; current changes still need fixes below |
| Form history | Removed undo-stack count arithmetic; successful apply and restore now feed the stale-field tracker | Addresses the original mechanism, but the replacement retains invalid result pointers on later Redo |
| Signing | Preferences now configure TSA URL and PAdES level; controller consumes settings and preserves them for retry; empty-TSA preflight and attained-level messages added | Real production wiring, but attained-level messages can overstate the resulting signature |
| Pack A | Added invalid-scope state at the controller boundary, UTF-16-to-glyph mapping, single ownership of the first outline write, and search debounce/cooperative budget | Committed only on `packafix`. Three useful source corrections; responsiveness remains partial, with additional defects in the pending branch |
| UI verification | Save/Find & Replace checks now deliver key events; corrected the CJK test literal and overstated export coverage | Committed on `packafix`, not yet included in the fetched integration tip. The earlier twelve welcome routes and ribbon expansion were already present at the previous handoff |
| OCR permissions | Added an OCR-accept guard against overwriting a read-only source while preserving Save As | Committed on `resid2`, pending integration and independent acceptance |
| Cleanup/recovery | Added deletion recovery ledger/summary and created `recovery/temp-stage-push-20260909` at `d367aca` | Historical objects now have a durable branch reference. Local-file recovery unknowns remain |

The original Find/Replace invalid-range click path deserves a correction to a blanket “still unfixed” claim: the integrated dialog already refuses malformed/out-of-document ranges. The pending F1 commit strengthens controller-level invalid-state handling and disabled-button behavior. Its scope is narrower than implementing that protection from scratch.

## Current quality blockers

### R14-P01 — P1: form Redo can write through expired stack variables

At integrated `54d5bcd`, the properties panel creates local `jsFailures` and `applyResult`, then gives their addresses to an undo command retained by the undo stack. Later Redo writes through both pointers after the panel method has returned. This is a source-confirmed stack-use-after-return path; a runtime crash was not reproduced in this review.

Evidence: [FormFieldPropertiesPanel.cpp:247](C:/Users/User/Projects/pdf-clean/src/modes/FormFieldPropertiesPanel.cpp:247), [EditFormFieldCommand.h:204](C:/Users/User/Projects/pdf-clean/src/commands/EditFormFieldCommand.h:204). The regression test traverses a command without the transient sinks and misses this case. Replace the borrowed result storage with a lifetime-safe result handoff, then test actual panel Apply → return → Undo → Redo.

### R14-P02 — P1: signing can claim a timestamped level after TSA failure

A nonempty but failing TSA URL passes the new preflight. The signing engine can downgrade a failed signature-timestamp attempt to an untimestamped result while the controller still labels it as attaining the requested timestamped level. This is source-confirmed; no live TSA request was made here.

Evidence: [SignatureManager.cpp:1383](C:/Users/User/Projects/pdf-clean/src/engines/SignatureManager.cpp:1383), [SecurityController.cpp:126](C:/Users/User/Projects/pdf-clean/src/shell/controllers/SecurityController.cpp:126). Record actual signature-timestamp success in the outcome and derive disclosure from the produced artifact. Add a failed-TSA B-T test and correct the higher-level fallback expectations.

### R14-P03 — P2: the pending Pack A branch breaks the no-PDFium build

The header adds a fourth `MatchBudget*` argument, but the no-PDFium implementation still defines only three arguments. This was **independently reproduced with the installed compiler**:

- Integrated `54d5bcd`: syntax-only compile passes, exit 0.
- Pending `5ae4153`: fails, exit 1, with a declaration/definition mismatch.
- An isolated probe copy with only the missing fallback parameter corrected: passes, exit 0.

No repository fix was applied. Evidence: [TextMatchFinder.cpp:221](C:/Users/User/Projects/pdf-inst/src/engines/TextMatchFinder.cpp:221) and [compiler commands/results](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/progress-2026-09-14/probes/textmatch-no-pdfium/results.json). Fix this before integration; reduced configurations matter for the Linux effort as well.

### Other findings that keep acceptance open

- **R14-P04:** Validate is run by `fillForm`, but the properties-panel current-value path uses `applyFieldSnapshot` and writes directly. The same rejected value can therefore be accepted through a different user-facing route. Define a shared user-edit validation boundary with explicit undo/restore semantics.
- **R14-P05:** Search remains synchronous. Its deadline can return an ordinary partial match list with no incomplete status; Count and Replace All can describe that subset as complete. Debounce reduces calls but does not make individual matching interruptible or expose cancellation correctly.
- **R14-P06:** The Validate caller budget does not include all separately timed setup/snapshot operations. The event is bounded; the broader single-deadline claim is not established.

Detailed evidence, fixes and test gaps: [forms/signing review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/progress-2026-09-14/forms-signing-review.md) and [Pack A/batch review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/progress-2026-09-14/packa-batch-review.md).

## Earlier work still incomplete

All four batch OCR findings from the preservation review remain in the integrated source: text pages are OCRed before being retained as original, the idempotency test retains the first input, skip preferences have no writers, and the byte-preservation claim is supported only by text equality using blank “image-only” fixtures. The quick branch's only new commit was deletion-audit documentation; its OCR source did not change.

Linux has no new lane-tip progress since the last handoff. The existing ledger describes a Linux build and incomplete headless testing, with native PDFium availability, desktop/Wayland/X11 behavior, printing, portals, IME and live Secret Service verification still outstanding. This is development progress, not evidence of a ready native Linux release. Form Keystroke/document-open execution and the previously recorded manual accessibility/UI checks also remain outstanding.

## Verification and cleanup evidence

The implementation-owner logs include a 14/14 stale-disclosure run, a 24/24 Find/Replace run, and an earlier 138/138 R15/R17 suite. These are useful records, but they do not establish acceptance of the latest combined source. The 138-test run predates the newest integrations. Pack A's own ledger correctly retains `implemented-awaiting-review`.

The F2/F4 negative-control evidence needs improvement: the ledger describes failure to compile old source against the new MatchBudget API, which does not demonstrate that the old behavior fails the regression. An earlier F2 prefix log actually reports passing geometry tests and does not identify its exact baseline SHA. Keep the API compatible while reverting the targeted behavior to obtain a meaningful failing control.

This review ran the narrow compiler check described above and inspected source plus saved logs. It did **not** run a full build, full suite, installed-app UI pass or remote service test. It made no application-source edits, commits, merges or deletions, and did not promote ledger rows to verified.

The recovery ledger reports six committed-history categories preserved, one recovered chain, five unresolved evidence categories and no confirmed loss. It still cannot establish the untracked/ignored state of the previously deleted `pdf-stage` and `pdf-sig` directories. No confirmed loss is not proof that nothing was lost. Also, the summary's instruction to create the staging recovery ref is now stale: that ref exists. Update the ledger to distinguish the completed recovery action from remaining unknowns, and extend backup coverage to subsequently created commits before any further cleanup.

## Recommended next sequence

1. Fix the form-command lifetime bug and false signing-level disclosure in the current integration line; cover the actual user paths.
2. Fix the pending matcher fallback and incomplete-search outcome. Re-run focused and reduced-configuration checks, then integrate the reviewed Pack A and OCR permission changes.
3. Complete interactive Validate coverage and the four batch OCR repairs. Reconcile each old finding with a specific current fix and independent result.
4. Run one complete gate against the final combined commit; retain the native UI and Linux desktop checks as explicit separate gates.
5. Refresh the deletion/recovery ledger and backups. Do not remove active or incompletely accounted-for worktrees based on the old candidate list.

The project is moving forward, but the immediate priority is making the newly implemented behavior safe and integrating its fixes, rather than declaring the packages complete from commit messages or prior green runs.
