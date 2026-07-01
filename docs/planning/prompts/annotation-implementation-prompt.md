# GlyphPDF — Domain Implementation Prompt: Annotation & Markup (§9.3)

**Domain key:** `annotation` | **Priority mix:** 1×P0 (M-effort), 1×P1 (M-effort), 1×P1 (L-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2B/2C, filtered to §9.3

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation competing with Adobe Acrobat, Foxit, and Xodo. You are picking up the Annotation & Markup domain, which contains what the competitive audit calls its single highest-severity finding across all 16 domains: shapes and freehand ink annotations silently vanish on save. (Two Wave 1A items — the false CHANGELOG "file attachment" claim fix and the annotation-toolbar consolidation — are being handled by a separate agent right now; do not touch or duplicate that.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.3 Annotation & Markup in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.3 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Annotation & Markup backlog (Wave 1B + Wave 2B + Wave 2C items) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fix (M-effort P0):**
1. **Persist Shapes and Freehand ink as their real PDF annotation subtypes.** Map DrawRectangle/Ellipse/Line/Arrow/Freehand to `/Square`, `/Circle`, `/Line`, `/Ink` in `applyAnnotationsToDoc`, and the inverse in `extractAnnotations`. Rationale: the single highest-severity finding in the whole audit — users draw shapes, save, reopen, and the shapes have silently vanished into an invisible note.

**Wave 2B — Feature-parity addition (M-effort P1):**
2. **Ship a small predefined stamp set + custom image-as-stamp import.** Rationale: today's Stamp produces only a flat gray box with text — functionally a worse callout, not a stamp.

**Wave 2C — Larger parity investment (L-effort P1):**
3. **Add QuadPoints-based text-anchored Highlight/Underline/Strikeout/Squiggly** as an alternative to free-rectangle drag. Rationale: the clearest quality gap versus literally every competitor studied, including free-tier Xodo — free-rectangle markup can silently highlight blank space or miss wrapped lines.

Confirm exact current file/class/function names by reading `src/core/AnnotationSerializer.{h,cpp}` (for `applyAnnotationsToDoc`/`extractAnnotations`), `src/core/AnnotationTypes.h`, `src/ui/AnnotationLayer.{h,cpp}`, and `src/ui/AnnotationToolBar.{h,cpp}` before starting — verify against current HEAD since the Wave 1A toolbar-consolidation agent may already have touched `AnnotationToolBar.cpp`.
</task>

<constraints>
- **Batch, don't fragment.** All 3 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/annotation-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch, especially to pick up the Wave 1A toolbar/CHANGELOG fixes first.
- **Incremental commits per logical group** — e.g. one commit for shape/ink persistence (split further into DrawRectangle/Ellipse/Line/Arrow → Freehand if that's cleaner), one for the stamp set + image-as-stamp import, one for QuadPoints text-anchored markup. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path.** Item 1 in particular needs a save→reload round-trip test proving shapes/ink survive as real `/Square`/`/Circle`/`/Line`/`/Ink` annotations (check `tests/TestAnnotationDjot.cpp` for the existing pattern to extend). Add coverage for QuadPoints text-anchoring accuracy (does it correctly wrap across lines?) and stamp placement/persistence.
- **Do not touch other domains' files if avoidable.** Stay within `src/core/AnnotationSerializer*`, `src/core/AnnotationTypes.h`, `src/ui/AnnotationLayer*`, `src/ui/AnnotationToolBar*`, and any new stamp-picker dialog you add under `src/ui/`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Annotation & Markup (§9.3) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Persist Shapes/Ink as real PDF annotation subtypes — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Predefined stamp set + image-as-stamp import — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] QuadPoints text-anchored Highlight/Underline/Strikeout/Squiggly — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
