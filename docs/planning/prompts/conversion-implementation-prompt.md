# GlyphPDF — Domain Implementation Prompt: Conversion (§9.5)

**Domain key:** `conversion` | **Priority mix:** 1×P0 (M-effort), 2×P1 (S-effort), 1×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B, filtered to §9.5

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the Conversion domain, which has the single biggest quality gap in the whole audit versus every competitor: GlyphPDF silently ships a mislabeled HTML file as ".docx" when its real OOXML dependencies aren't linked in the build. (Two Wave 1A items — automated tests for the real-OOXML code paths, and fixing PPTX text-overlay opacity — are being handled by a separate agent right now; do not touch or duplicate those.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.5 Conversion in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.5 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants, and the vcpkg/MSYS2 dependency-management conventions for this project

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Conversion backlog (Wave 1B + Wave 2A + Wave 2B items — excluding the Wave 1A OOXML-test and PPTX-opacity fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fix (M-effort P0):**
1. **Land real OOXML dependencies (OpenXLSX + duckx) in the shipped build.** Add to `vcpkg.json` (or the equivalent MSYS2/CMake dependency mechanism this project actually uses — verify current state first, since the project migrated to MSYS2 ucrt64), verify `HAS_DUCKX`/`HAS_OPENXLSX` branches are taken, and never mislabel fallback output with the wrong extension. Rationale: single biggest quality gap versus every competitor — shipping mislabeled HTML as ".docx" causes Office repair prompts and destroys trust.

**Wave 2A — Quick parity wins (S-effort P1):**
2. **Bring the batch format list to parity with the single-document `ConvertController`.** Rationale: PowerPoint and Text exist in one path but not the other — an easily-noticed inconsistency.
3. **Make batch Compress use the full DPI+quality preset pairing instead of hardcoded DPI=150.** Rationale: batch users silently get different, non-adjustable results than the single-doc path.

**Wave 2B — Feature-parity addition (M-effort P1):**
4. **Add an explicit OCR-mode toggle to PDF→Word/Excel/Text/CSV export dialogs.** Rationale: Smallpdf/iLovePDF both expose this as first-class UX; GlyphPDF already has a working OCR pipeline to wire in.

Confirm exact current file/class/function names by reading `src/engines/ConversionManager.{h,cpp}`, `src/shell/controllers/ConvertController.{h,cpp}`, `src/core/interfaces/IConversionEngine.h`, and the CMake dependency declarations (search for `HAS_DUCKX`/`HAS_OPENXLSX` preprocessor guards) before starting. Also check `src/modes/BatchMode.cpp` for the batch-format-list and hardcoded-DPI=150 locations.
</task>

<constraints>
- **Batch, don't fragment.** All 4 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/conversion-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A OOXML-test-coverage and PPTX-opacity fixes first — item 1 here should build directly on that groundwork, not duplicate it.
- **Incremental commits per logical group** — e.g. one commit for landing real OOXML deps + verifying branch coverage, one for batch/single-doc format-list parity, one for batch DPI+quality preset parity, one for the OCR-mode toggle in export dialogs. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Since item 1 changes what gets linked into the build, be extra careful to rebuild from a clean CMake cache (`cmake -B build -G Ninja` re-run) and confirm `HAS_DUCKX`/`HAS_OPENXLSX` are actually true in the configured build, not just assumed.
- **Add tests for any newly-covered path** — beyond the Wave 1A OOXML test coverage, add regression coverage proving the export never silently falls back to a mislabeled extension (i.e. if OOXML libs are unavailable, the file extension and a user-visible warning must match reality), plus coverage for batch format-list parity and the new OCR-mode toggle.
- **Do not touch other domains' files if avoidable.** Stay within `src/engines/ConversionManager*`, `src/shell/controllers/ConvertController*`, `src/core/interfaces/IConversionEngine.h`, the conversion-specific sections of `src/modes/BatchMode.cpp`, and CMake/vcpkg dependency manifests.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. Since this domain explicitly adds new external dependencies (OpenXLSX, duckx), document exactly what was added/verified and why in your final report — this is expected, not a deviation to flag apologetically, but it must be clearly stated for a human reviewing licensing/supply-chain impact (see `LICENSE-3RD-PARTY.md` convention in this repo).
</constraints>

<final_report_format>
```
## Domain: Conversion (§9.5) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Land real OOXML deps (OpenXLSX + duckx), verify branches taken — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Batch format list parity with ConvertController — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Batch Compress full DPI+quality preset parity — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] OCR-mode toggle on export dialogs — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors; explicitly confirm HAS_DUCKX/HAS_OPENXLSX status>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any new dependency/licensing note>
```
</final_report_format>
