# GlyphPDF — Domain Implementation Prompt: Page Management (§9.9)

**Domain key:** `pages` | **Priority mix:** 1×P0 (M-effort), 1×P1 (S-effort), 4×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B, filtered to §9.9

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose atomic, bijection-validated page-reorder command and direct page-resize dialog have no equivalent in Acrobat. You are picking up the Page Management domain, whose single most visible parity gap is that thumbnail-grid drag-and-drop reordering is hard-disabled — every competitor's primary interaction is dragging the thumbnail itself. (Two Wave 1A items — surfacing real merge success/failure instead of always reporting "Successfully merged", and consolidating the two parallel reorder commands into one — are being handled by a separate agent right now; do not touch or duplicate those.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.9 Page Management in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.9 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Page Management backlog (Wave 1B + Wave 2A + Wave 2B items — excluding the Wave 1A merge-error-surfacing and reorder-command-consolidation fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fix (M-effort P0):**
1. **Enable drag-and-drop reordering directly on the page thumbnail grid.** Remove `NoDragDrop` and wire drop events to the existing atomic `ReorderPermutationCommand`. Rationale: the single most visible parity gap — every competitor's primary interaction is dragging the thumbnail itself; a side-panel-only workaround will read as broken/missing.

**Wave 2A — Quick parity win (S-effort P1):**
2. **Move Merge under the Pages/Organize tool group** (or cross-link it). Rationale: merge already works; this is discoverability, not engineering — every competitor treats it as part of the same surface as reorder/delete.

**Wave 2B — Feature-parity additions (M-effort P1):**
3. **Support multiple output parts from one split-by-range operation.** Rationale: current v1.0 cap of one output part per operation is behind every named competitor.
4. **Add a distinct Page Numbering (Page Labels) mode** with numbering-style and section support. Rationale: a plain `{page}` token cannot express Roman-numeral prefaces or independent numbering sections that competitors support.
5. **Extend Bates numbering to cross-document batches.** Rationale: real Bates usage is almost always multi-document; GlyphPDF's is currently single-document only.
6. **Add unit test coverage for Bates, header/footer, crop, resize, and merge.** Rationale: five real, shipped capabilities currently have zero automated coverage.

Confirm exact current file/class/function names by reading `src/ui/ThumbnailSidebar.{h,cpp}` (the `NoDragDrop` flag), `src/commands/ReorderPermutationCommand.{h,cpp}`, `src/shell/controllers/PagesController.{h,cpp}`, `src/modes/PagesMode.{h,cpp}`, `src/ui/PageManagementDialog.{h,cpp}`, `src/ui/BatesNumberingDialog.{h,cpp}`, `src/ui/HeaderFooterDialog.{h,cpp}`, `src/commands/CropPageCommand.{h,cpp}`, and `src/ui/ResizeDialog.{h,cpp}` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 6 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/pages-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A merge-error-surfacing and reorder-command-consolidation fixes first — item 1 here (thumbnail DnD) should call the already-consolidated single reorder command, not the retired legacy one.
- **Incremental commits per logical group** — e.g. one commit for thumbnail-grid drag-and-drop, one for Merge discoverability (menu placement/cross-link), one for multi-part split-by-range, one for the Page Numbering (Labels) mode, one for cross-document Bates, one for the new Bates/header-footer/crop/resize/merge unit tests. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestPagesMode.cpp` for the existing pattern to extend for item 6.
- **Add tests for any newly-covered path** — item 6 explicitly is test-coverage work (Bates, header/footer, crop, resize, merge), but also add coverage for thumbnail drag-and-drop reorder (does it correctly invoke the atomic permutation command, not a partial/lossy reorder?), multi-part split output, Page Labels numbering-style correctness, and cross-document Bates continuity.
- **Do not touch other domains' files if avoidable.** Stay within `src/ui/ThumbnailSidebar*`, `src/commands/ReorderPermutationCommand*`, `src/shell/controllers/PagesController*`, `src/modes/PagesMode*`, `src/ui/PageManagementDialog*`, `src/ui/BatesNumberingDialog*`, `src/ui/HeaderFooterDialog*`, `src/commands/CropPageCommand*`, `src/ui/ResizeDialog*`, and `tests/TestPagesMode.cpp`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Page Management (§9.9) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Thumbnail-grid drag-and-drop reorder (remove NoDragDrop, wire ReorderPermutationCommand) — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Move Merge under Pages/Organize group (or cross-link) — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Multiple output parts from split-by-range — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Distinct Page Numbering (Labels) mode — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Cross-document Bates numbering — <detail, commit hash>
6. [DONE|SKIPPED|PARTIAL] Unit tests for Bates/header-footer/crop/resize/merge — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
