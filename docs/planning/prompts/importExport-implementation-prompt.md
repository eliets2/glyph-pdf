# GlyphPDF — Domain Implementation Prompt: File Import and Export (§9.16)

**Domain key:** `importExport` | **Priority mix:** 1×P1 (S-effort), 2×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 2A/2B, filtered to §9.16

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the File Import and Export domain, whose most embarrassing gap is that Office/image import is hidden behind separate menus instead of being unified into File > Open with drag-and-drop, the way both Acrobat and Foxit treat opening a non-PDF file as first-class. (Two Wave 1A items — adding a runtime badge showing which export path was actually used (real OOXML vs fallback), and removing/wiring up the dead "linearized" export preset checkbox — are being handled by a separate agent right now; do not touch or duplicate those.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.16 File import and export in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.16 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining File Import and Export backlog (Wave 2A + Wave 2B items — excluding the Wave 1A export-path-badge and linearized-checkbox fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 2A — Quick parity win (S-effort P1):**
1. **Add automated round-trip tests for bookmark and hyperlink preservation on Save.** Rationale: currently rests entirely on an implicit side effect with zero dedicated test coverage, while a sibling redaction path is known to deliberately strip Outlines.

**Wave 2B — Feature-parity additions (M-effort P1):**
2. **Unify Office/image import into the main Open flow and add drag-and-drop.** Rationale: both best-in-class products treat opening a non-PDF file as first-class, not a hidden secondary menu.
3. **Surface OCR output-mode choice and document-language picker in the OCR/scan-import UI.** Rationale: the underlying capability likely already exists internally; this may be primarily a UI-exposure task.

Confirm exact current file/class/function names by reading `src/GpMainWindow.{h,cpp}` (the main Open/File menu flow and any existing drag-and-drop handling), `src/engines/conversion/DjvuImporter.{h,cpp}` and related Office-import code, `src/engines/ConversionManager.{h,cpp}`, `src/modes/OCRMode.{h,cpp}` (for the scan-import UI), and `tests/TestOfficeImport.cpp` before starting. Note: item 3 overlaps conceptually with the OCR domain's own backlog (`docs/planning/prompts/ocr-implementation-prompt.md` covers an `OutputMode` choice in Wave 2B for OCR generally) — if that domain's agent has already landed an `OutputMode` enum/mechanism, reuse it here for the import-specific UI surface rather than inventing a second one.
</task>

<constraints>
- **Batch, don't fragment.** All 3 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/importExport-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A export-path-badge and linearized-checkbox fixes first.
- **Coordinate with the OCR domain on item 3** — check whether `feature/ocr-parity` has landed an `OutputMode` (searchable image vs editable text) mechanism before building your own; reuse it rather than duplicating. If it hasn't landed yet, build the minimal UI-exposure piece here and note in your final report that it should be reconciled with the OCR domain's mechanism once both branches integrate.
- **Incremental commits per logical group** — one commit for bookmark/hyperlink round-trip tests, one for unifying Office/image import into the main Open flow with drag-and-drop, one for surfacing OCR output-mode/language picker in the scan-import UI. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestOfficeImport.cpp` (currently 5 tests: 3 active without LibreOffice, 2 QSKIP when soffice absent) — extend this file for item 1 and item 2's new import paths, respecting the existing QSKIP-when-soffice-absent convention.
- **Add tests for any newly-covered path** — item 1 explicitly calls for bookmark/hyperlink round-trip tests on Save (verify against the redaction path's known Outline-stripping behavior — this unified test should make clear whether that stripping is intentional-and-documented or a regression to flag). Also add coverage for drag-and-drop file-type detection (PDF vs Office vs image) routing to the correct import path.
- **Do not touch other domains' files if avoidable.** Stay within `src/GpMainWindow*` (Open/drag-and-drop flow only), Office/image import engine files, `src/modes/OCRMode*` (import-surfacing UI only, not core OCR engine logic), and `tests/TestOfficeImport.cpp`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: File Import and Export (§9.16) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Bookmark/hyperlink round-trip tests on Save — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Unify Office/image import into main Open flow + drag-and-drop — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] OCR output-mode/language picker in scan-import UI — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any reconciliation note with the OCR domain's OutputMode mechanism>
```
</final_report_format>
