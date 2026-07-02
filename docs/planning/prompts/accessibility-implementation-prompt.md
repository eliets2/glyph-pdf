# GlyphPDF — Domain Implementation Prompt: Accessibility (§9.14)

**Domain key:** `accessibility` | **Priority mix:** 1×P0 (S-effort), 2×P1 (S-effort), 1×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B, filtered to §9.14

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the Accessibility domain, tied for the worst-scored domain in the audit at 3.0/10: the reading-order checker has a false-positive bug on normal, correctly-tagged PDFs because it doesn't walk up the page-inheritance chain per ISO 32000-2 §14.7.2, coding a missing `/Pg` as page=-1 instead of resolving it correctly.
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.14 Accessibility in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.14 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch. Note: this domain has NO Wave 1A items — everything below is your full scope.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's entire remaining Accessibility backlog (Wave 1B + Wave 2A + Wave 2B items) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fix (S-effort P0):**
1. **Fix page-inheritance walk-up in `pageIndexOf`** per ISO 32000-2 §14.7.2, instead of coding missing `/Pg` as page=-1. Rationale: single highest-value fix — eliminates a documented false-positive class on perfectly-ordered, real-world tagged PDFs.

**Wave 2A — Quick parity wins (S-effort P1):**
2. **Replace the ">2 position slots" heuristic with a named, documented threshold** and honest framing. Rationale: near-zero-effort change that stops implying algorithmic authority the tool cannot back up; matches how the actual PDF/UA standard treats this checkpoint as human-only.
3. **Add automated unit tests for `analyzeReadingOrder`/`collectStructElems`.** Rationale: cheap insurance against regressions when the P0 fix above lands, since it touches this exact code.

**Wave 2B — Feature-parity addition (M-effort P1):**
4. **Add a persistent, exportable results panel.** Rationale: turns a one-time popup glance into a compliance-workflow artifact for professional/government users.

**Note:** this prompt intentionally excludes two P0/P1 items from the source docs that belong to OTHER domains' Wave 1A scope handled elsewhere: "Add jump-to-page/highlight-on-click for each reported mismatch" (Wave 1A) and "Move `analyzeReadingOrder` off the UI thread" (Wave 1B, listed under §9.14 in the plan but tagged as a separate item — verify current status: if it has not been picked up elsewhere, treat the UI-thread move as in-scope for this pass and add it as item 1b, since it is a correctness/performance fix in the same code area as item 1).

Confirm exact current file/class/function names by reading the accessibility/reading-order checker source — likely `src/modes/PdfAValidationPanel.{h,cpp}` or a dedicated accessibility panel/engine file (search for `analyzeReadingOrder`, `collectStructElems`, `pageIndexOf` across `src/` to locate the exact file, since it may not be obviously named) before starting.
</task>

<constraints>
- **Batch, don't fragment.** All items above belong to one domain, one branch, one worktree.
- **Branch:** `feature/accessibility-parity`, based off `audit-remediation` (or `main` if that no longer exists). This domain has no Wave 1A dependency, but check the latest integration branch state before starting.
- **Sequence within this domain:** land item 1 (the `pageIndexOf` inheritance walk-up fix) first — items 2-4 either touch the same reading-order code path or depend on it being correct first (e.g. the new unit tests in item 3 should cover the fixed behavior, not the buggy one).
- **Incremental commits per logical group** — one commit for the `pageIndexOf` page-inheritance fix (plus the UI-thread move if in scope, see note above), one for the named/documented threshold replacing the ">2 position slots" heuristic, one for the new `analyzeReadingOrder`/`collectStructElems` unit tests, one for the persistent exportable results panel. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestVeraPdf.cpp` and any existing accessibility-related test file for the pattern to extend.
- **Add tests for any newly-covered path** — item 3 explicitly calls for this (`analyzeReadingOrder`/`collectStructElems` unit tests), and must specifically include a regression test proving the page-inheritance walk-up fix resolves the documented false-positive class on correctly-tagged PDFs (i.e. a fixture PDF that previously triggered a false positive should now pass cleanly).
- **Do not touch other domains' files if avoidable.** Stay narrowly within the reading-order/accessibility checker's own source files and its results-panel UI.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Accessibility (§9.14) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Fix page-inheritance walk-up in pageIndexOf (ISO 32000-2 §14.7.2) — <detail, commit hash>
1b. [DONE|SKIPPED|PARTIAL|N/A-already-handled] Move analyzeReadingOrder off UI thread — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Named/documented threshold replacing ">2 position slots" heuristic — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Unit tests for analyzeReadingOrder/collectStructElems — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Persistent, exportable results panel — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name, and confirm the false-positive regression fixture now passes>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
