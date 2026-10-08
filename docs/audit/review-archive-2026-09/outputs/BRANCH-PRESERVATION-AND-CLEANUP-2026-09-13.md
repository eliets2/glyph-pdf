# GlyphPDF branch preservation and cleanup review — 2026-09-13

The reviewed application-source changes have been preserved. This session made **six local commits covering 62 file paths across six worktrees**. The active implementation owners committed the newly discovered R15 and R18 changes during the review; their source files were checked against those commits. No branch or worktree was deleted, and this review session did not push anything. Other sessions did advance and push branches.

The final graph observation is **2026-09-13T13:55:33.350883+00:00**: **66 local branches, 19 remote-tracking branches, 17 worktrees and 6 stashes**. The filesystem observation is 2026-09-13T13:55:30.039638+00:00. Original scope was 62 local branches and 15 worktrees; new work appeared during the session. These are pinned observations, not a claim that other editing sessions are paused.

## Preserved commits

| Branch | Commit made here | Preserved work |
|---|---|---|
| `main` | `2b715f47fd4141619eb0751c051929c8608d715c` | Existing September 13 audit findings; historical verification labels retained as supplied |
| `feat/parity-glm-packa` | `7e32093c67c10896a0654adf820b6092d3956083` | Pack A: Find/Replace, summaries, stamps, auto-bookmarks; unfinished WIP |
| `feat/parity-glm-quick` | `2a82738ebc3fbb1e683c22c2f7c44e49f06656ee` | Batch OCR skip-text options, worker, command matrix and tests; unfinished WIP |
| `subagent-AST-Architect-self-54b52cfc` | `d7a8ca068eeca1145138558177d88b7503b84c3a` | Historical AST scaffolding generator; not a current product feature |
| `subagent-Vendoring-Specialist-self-ca2c3f27` | `5a018ae5fb3b5d00d854c20058cc9cb12beaa6f2` | Original Djot integration scaffolding; incomplete/unbuildable WIP |
| `feature/redaction-parity` | `45bf4302b9d43b8f7d866eb2ca9adf36c8004ba0` | Redaction region, overlay and related source changes; unfinished WIP |

Each commit used an explicit file manifest, checked HEAD and file hashes at the commit boundary, and verified that unrelated staged index entries were unchanged. In particular, the existing vendor/installer/signing-fixture staging was not included. Source bytes remained unchanged by the preservation commits, apart from normal Git storage normalization.

The quick branch advanced from the reviewer’s `8cff57a` observation to `3cb1b42` before preservation. Only its CMake raw hash differed from the review manifest, because already-committed Pack A registrations had arrived; the other four reviewed file hashes matched. The preservation commit contains only the N3 delta relative to the newer parent.

Concurrent owner commits independently accounted for:

- **R15:** `b9f417c9346e4dfcd9b0a3c6693eec77ee22fd08`, four reviewed source/test paths. They matched HEAD; no duplicate reviewer commit was made.
- **R18:** `d71a4c2dde5e3fe25487602d4a431d507f3352c3`, twelve reviewed canonical source/test paths. Every raw file hash matched the review manifest. The owner also removed the typo-named duplicate panel; its content matched the canonical file and is preserved there. This review did not delete it.

## Cleanup decision

**36 local branch names** have exact ancestry in retained integration history and no blocker recorded in the cleanup CSV. Another **11 remote-tracking names** meet that ancestry check. This is a candidate list for a later cleanup operation, not authorization or execution of remote deletion.

- Full inventory, exact SHAs, current checkouts, preservation commits and blockers: [branch-cleanup.csv](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/branch-cleanup.csv).
- Filtered candidate names: [branch-cleanup-unblocked-candidates.csv](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/branch-cleanup-unblocked-candidates.csv).
- Complete branch graph review: [graph-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/graph-review.md).
- Initial-to-final ref movement and category counts: [branch-cleanup-summary.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/branch-cleanup-summary.json).

Keep both `main` and `feat/parity-glm`. At this observation they have **430 and 700 exclusive commits**, respectively. A branch absent from one target may still hold important history in the other. Neither branch is a universal deletion baseline.

Keep every branch marked with an unintegrated tip, new work, preservation work, or a checked-out worktree blocker. Source-preservation commits are not merge approval. In particular, do not discard the feature lane just because an older tip was previously merged.

Before applying the candidate list, fetch again and confirm each candidate still resolves to the exact recorded SHA, remains an ancestor of the retained target, and is not newly checked out or being edited. If a ref changed, review its new tip. Branch deletion and worktree removal are separate operations; do not run force deletion or `git clean` from this report.

## Remaining local state

The closing inventory found **no original application-source changes left uncommitted**. The worktrees are not universally clean:

| Local state | Treatment |
|---|---|
| Five legacy Gemini worktrees, each with 703 staged artifact paths | Left unchanged. These contain dependency installation outputs, installers and generated signing fixtures. Initial index/working-tree binary patches are backed up. Do not run an ordinary blanket commit on these indexes. |
| Build directories, logs, deployment backups and graphify output | Left local; excluded from source commits. They are enumerated in the inventory. |
| Two nested Djot Git repositories | Clean at the observed revisions; each has a separate verified bundle. No accidental gitlinks were added to the parent repository. |
| Duplicate Lua vendor source in legacy trees | Already present with equivalent contents in integration history; retained locally, not recommitted. |
| Ignored `.context` research, handoffs, notes and text probes | Archived locally with hashes and a later supplement; not force-added to product branches. |
| Empty `pdf-redaction/task.md` and review scratch files | Retained locally; meaningful text scratch files were archived. |
| Ignored runtime/model/vendor binary trees and other local settings | Not fully backed up by Git or the text-note archives. Retain their worktrees until a separate relocation or backup check is complete. |

The full closing filesystem inventory is [inventory-closure.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/inventory-closure.json). Ignored/untracked artifacts are why deleting a worktree is not equivalent to deleting a redundant branch name.

## Quality review limits and findings

This was a branch/history review plus source-preservation triage. It did not execute every branch’s tests or certify release readiness. WIP commits intentionally retain the pre-existing unfinished implementation. No evidence-ledger row was upgraded to verified by this session.

| Reviewed area | Findings at the reviewed snapshot | Detail |
|---|---|---|
| Pack A | Invalid range can expand to the whole document; UTF-16 match offsets can disagree with codepoint geometry; duplicate initial outline write; regex timeout does not interrupt individual matches. | [packa-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/packa-review.md) |
| Batch OCR skip-text | OCR still runs for pages later kept original; idempotency test retains the initial input and encounters overwrite handling; preferences are read but not written; byte-preservation claim exceeds text-equality evidence. | [quick-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/quick-review.md) |
| Legacy Djot / redaction | Missing CMake file, malformed headers, unfinished mapper/sandbox; redaction API mismatch, Cancel wiring and transaction concerns. Redaction diff has 23 trailing-whitespace locations. | [legacy-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/legacy-review.md) |
| R15 | Accessibility/keyboard test coverage limits and a CJK-label encoding concern remain for independent verification. | [r15-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/r15-review.md) |
| R18 | Undo-count success detection misses history truncation/undo limits; undo/redo does not feed stale-state tracking; persistence is session-only. | [r18-review.md](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/r18-review.md) |

These findings refer to the exact reviewed source snapshots. Other sessions made subsequent commits, especially on Pack A; check the current implementation before treating an older finding as still open. No build or runtime test was run as part of these preservation commits. The documentation and most tracked source whitespace checks passed; the old redaction WIP’s whitespace failure was recorded, not silently fixed.

## Backups and recovery

All backup files remain local in `C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13`. Keep this folder while reconciling the branch history. It contains historical fixtures and development evidence and has not been uploaded by this review.

- Latest complete Git history: [glyph-pdf-closure.bundle](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/glyph-pdf-closure.bundle) (**38,751,686 bytes**). `git bundle verify` passed, and every one of the **99 sealed final refs** was found at its expected SHA inside the bundle.
- Initial refs/stashes are also retained under `refs/archive/cleanup-20260913-initial/`; final graph tips, all six stashes, the six reviewer commits and the two owner commits are under `refs/archive/cleanup-20260913-closure/`. Existing branches and stash entries were not moved by creating these backup refs.
- Initial full binary patches and raw selected source snapshots: [uncommitted-state-backup.zip](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/uncommitted-state-backup.zip). ZIP CRC verification passed.
- Research, notes and probes: [local-notes-and-probes.zip](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/local-notes-and-probes.zip) plus [closure-local-notes.zip](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/closure-local-notes.zip). Both passed ZIP CRC checks. The supplement includes the R18 canonical source copies.
- Nested repositories: [djot-7708fcd5.bundle](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/djot-7708fcd5.bundle) and [djot-ca2c3f27.bundle](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/djot-ca2c3f27.bundle). Both passed Git bundle verification.
- Hashes and checks: [backup-verification.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/backup-verification.json), [closure-verification.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/closure-verification.json), [uncommitted-state-manifest.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/uncommitted-state-manifest.json), [local-notes-and-probes-manifest.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/local-notes-and-probes-manifest.json) and [closure-local-notes-manifest.json](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/branch-preservation-2026-09-13/closure-local-notes-manifest.json).

For recovery, use a new empty directory and a mirror clone of the complete bundle so the `refs/archive/` refs are retained. Create ordinary recovery branches from the desired archived SHA. Restore a worktree’s staged patch against its recorded original HEAD, then its unstaged patch, then the selected/untracked local files. Do not apply these patches over an active checkout. Some fixtures and ignored runtime trees need their separate original local copies; the bundle is not a disk image.

No branch cleanup command has been executed. The candidate CSV and verified backup state make a subsequent, separately scoped cleanup concrete and reviewable.
