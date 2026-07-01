# GlyphPDF — Domain Implementation Prompt: Document Comparison (§9.10)

**Domain key:** `compare` | **Priority mix:** 1×P0 (S-effort — but critical unlock), 1×P1 (S-effort), 2×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B, filtered to §9.10

**Note on effort tags:** the audit's own P0 list tags the item-1 entry-point wiring as S-effort, but the plan's Wave table places it at 1B (M-effort tier) alongside a change-type filter — read both source docs' framing before starting; the "wire the entry point" work itself is small, but do it as its own commit regardless of tier ambiguity.

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the Document Comparison domain — scored 3.0/10, the worst-scored domain after Compression, for a single reason: `CompareMode::compareFiles()` is a fully-implemented, already-unit-tested Myers-diff comparison engine that has ZERO reachable UI entry point. Every menu route today lands on a permanently empty placeholder screen. This is the audit's own top recommended first fix across the entire project (see plan's "Suggested first three" section) because it unlocks the most user-visible value for the least effort.
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.10 Document Comparison in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics (note this domain is explicitly called out as the #1 highest-value-per-effort fix in the whole plan), then re-read §9.10 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch. Note: this domain has NO Wave 1A items — everything below is your full scope, and the entry-point wiring (item 1) is the single highest-priority item in this entire prompt.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's entire remaining Document Comparison backlog (Wave 1B + Wave 2A + Wave 2B items) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes:**
1. **Wire `CompareMode::compareFiles()` into a real menu entry (the blocker).** Add a real "Compare Documents…" action calling the already-implemented, already-tested comparison engine. Rationale: this is the single blocking defect in the entire feature — every menu route today lands on a permanently empty placeholder screen despite a working, unit-tested Myers-diff engine underneath. **Do this first, as its own commit, before anything else in this domain.**
2. **Add a change-type filter to the CHANGES tree and text-diff panel.** Gate what's already computed (text/move/pixel/page-move tags) behind filter toggles. Rationale: every best-in-class tool treats noise suppression as core UX; this is display-layer work on data already produced.

**Wave 2A — Quick parity win (S-effort P1):**
3. **Generate explicit rows for pages added/removed entirely when page counts differ.** Rationale: content on trailing pages of a longer document is currently silently never diffed or reported.

**Wave 2B — Feature-parity additions (M-effort P1):**
4. **Add a progress bar with page-by-page status and a Cancel button.** Rationale: large legal/contract PDFs can take a long time with no way to cancel today.
5. **Add automated integration tests for `DiffEngine::compare` end-to-end.** Rationale: only the isolated Myers algorithm is tested — the PDF-to-page-diff integration and report builders are completely unverified.

Confirm exact current file/class/function names by reading `src/modes/CompareMode.{h,cpp}` (`compareFiles`), `src/modes/ModeController.{h,cpp}` (how modes get wired into menus/navigation — this is likely where the missing entry point needs to be added), `src/engines/DiffEngine.{h,cpp}`, `src/engines/MyersDiff.{h,cpp}`, and `src/ui/CompareWidget.{h,cpp}` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 5 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/compare-parity`, based off `audit-remediation` (or `main` if that no longer exists). This domain has no Wave 1A dependency to rebase onto specifically, but still check the latest integration branch state before starting.
- **Sequence within this domain:** land item 1 (the entry-point wiring) FIRST and get it built/tested/committed before moving to the other items — it is both the lowest-risk and highest-value item, and everything else in this domain is easier to verify once the feature is actually reachable end-to-end in the running app.
- **Incremental commits per logical group** — one commit for the menu entry point wiring, one for the change-type filter, one for added/removed-page rows, one for progress bar + Cancel, one for the new DiffEngine integration tests. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestDiffEngine.cpp` (currently 12 tests: Myers LCS + move detection + legal-doc scenario) — extend this file for item 5, do not replace it.
- **Add tests for any newly-covered path** — item 5 explicitly calls for `DiffEngine::compare` end-to-end integration tests (the PDF-to-page-diff pipeline and report builders, not just the isolated Myers algorithm already covered). Also add a smoke-level test confirming the menu entry point actually launches `CompareMode` with a real file-picker (a UI-reachability regression test, so this exact bug can never silently reappear).
- **Do not touch other domains' files if avoidable.** Stay within `src/modes/CompareMode*`, `src/engines/DiffEngine*`, `src/engines/MyersDiff*`, `src/ui/CompareWidget*`, `tests/TestDiffEngine.cpp`, and whatever menu/navigation registration file needs the new entry point (likely `src/shell/MenuBar.cpp` or `src/modes/ModeController.cpp` — touch the minimum necessary there).
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Document Comparison (§9.10) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Wire CompareMode::compareFiles() into a real menu entry (the blocker) — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Change-type filter on CHANGES tree/text-diff panel — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Explicit rows for added/removed pages on count mismatch — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Progress bar with page-by-page status + Cancel — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] DiffEngine::compare end-to-end integration tests — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line, explicitly confirm TestDiffEngine still passes; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
