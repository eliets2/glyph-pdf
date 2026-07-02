# GlyphPDF — Domain Implementation Prompt: Search & Navigation (§9.15)

**Domain key:** `search` | **Priority mix:** 1×P0 (M-effort), 2×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2B, filtered to §9.15

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose real regex-in-UI find/replace already beats Acrobat (which requires JavaScript for the same capability). You are picking up the Search & Navigation domain, whose most trust-eroding bug is that the Match Case / Whole Words / Use Regex checkboxes are silently dropped by the actual search path — checking a box and having it visibly ignored destroys confidence in the whole feature. (One Wave 1A item — wiring the dead thumbnail zoom +/- buttons — is being handled by a separate agent right now; do not touch or duplicate that.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.15 Search & Navigation in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.15 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Search & Navigation backlog (Wave 1B + Wave 2B items — excluding the Wave 1A thumbnail zoom +/- button fix handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fix (M-effort P0):**
1. **Wire Match Case, Whole Words, and Use Regex checkboxes into document-text search.** Fix `searchDocument()` so these parameters actually filter results instead of being silently dropped, and consolidate onto the existing `PdfiumBackend::searchText()` path for regex. Rationale: an easily-discovered gap — checking "Match Case" and seeing it ignored destroys trust in the whole search feature.

**Wave 2B — Feature-parity additions (M-effort P1):**
2. **Move thumbnail rendering off the GUI thread with proper locking on `RenderCache`.** Rationale: a pre-identified, already-diagnosed internal audit finding that causes UI jank on large documents.
3. **Add minimal automated test coverage for search/thumbnail/bookmark/jump-to-page wiring.** Rationale: the thumbnail click-to-navigate bug was a real, shipped dead-signal defect caught only by manual audit.

Confirm exact current file/class/function names by reading `src/ui/FindBar.{h,cpp}`, the `searchDocument()` implementation (search `src/` for it — likely in `PdfViewerWidget.cpp` or a dedicated search controller), `src/engines/pdfium/PdfiumBackend.{h,cpp}` (`searchText`), `src/engines/RenderCache.{h,cpp}`, `src/ui/ThumbnailSidebar.{h,cpp}`, and `src/ui/BookmarkPanel.{h,cpp}` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 3 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/search-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A thumbnail zoom button fix first, since item 2 here also touches thumbnail rendering (`RenderCache`/`ThumbnailSidebar`).
- **Incremental commits per logical group** — one commit for wiring Match Case/Whole Words/Use Regex into `searchDocument()` and consolidating onto `PdfiumBackend::searchText()`, one for moving thumbnail rendering off the GUI thread with `RenderCache` locking, one for the new search/thumbnail/bookmark/jump-to-page test coverage. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path** — item 3 explicitly calls for this (search/thumbnail/bookmark/jump-to-page wiring), and must specifically include a regression test proving each search-modifier checkbox (Match Case, Whole Words, Use Regex) actually changes the result set, not just that the UI accepts the click.
- **Thread-safety care for item 2:** moving thumbnail rendering off the GUI thread touches `RenderCache`, which is presumably read from multiple places (main view + sidebar). Add explicit locking (e.g. `QMutex`/`QReadWriteLock` as appropriate) and verify no torn-read/race conditions with a stress test if feasible (repeated rapid scroll/resize during thumbnail generation).
- **Do not touch other domains' files if avoidable.** Stay within `src/ui/FindBar*`, the search-controller code, `src/engines/pdfium/PdfiumBackend*` (search-specific methods only), `src/engines/RenderCache*`, `src/ui/ThumbnailSidebar*`, `src/ui/BookmarkPanel*`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Search & Navigation (§9.15) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Wire Match Case/Whole Words/Use Regex into searchDocument() — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Thumbnail rendering off GUI thread with RenderCache locking — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Test coverage for search/thumbnail/bookmark/jump-to-page wiring — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
