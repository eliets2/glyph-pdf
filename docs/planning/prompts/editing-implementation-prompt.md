# GlyphPDF — Domain Implementation Prompt: Text & Object Editing (§9.2)

**Domain key:** `editing` | **Priority mix:** 2×P0 (M-effort), 2×P1 (M-effort), 1×P1 (L-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2B/2C, filtered to §9.2

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation that competes with Adobe Acrobat, Foxit, and PDFelement on feature depth while beating all of them structurally on zero-upload local processing. You are picking up the Text & Object Editing domain — the worst-scored core-editing domain in the competitive audit at 3.5/10, driven by an explicitly-disabled Edit menu and fully-built-but-unreachable image commands (rotate/delete/replace are handled by a separate Wave 1A agent already — your scope starts one level up).
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.2 Text & Object Editing in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.2 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch. Note: Wave 1A (already running in a separate agent, DO NOT touch or duplicate) covers: wiring Image Rotate to the UI, and wiring Image Delete/Image Replace to the UI, via `RotateImageCommand`/`DeleteImageCommand`/`ReplaceImageCommand`. Your scope below assumes those UI hooks exist or are in flight — build alongside them, don't collide with them.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Text & Object Editing backlog (Wave 1B + Wave 2B + Wave 2C items — excluding the Wave 1A image rotate/delete/replace wiring handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Ship a minimal Cut/Copy/Delete for the object the `EditController` already selects.** Delete key removes the selected object; Copy places a raster snapshot on the system clipboard, without full in-document paste. Rationale: the Edit menu currently shows a user-facing "not shipped" tooltip — the most visible admitted gap in the whole feature area.
2. **Replace the eraser placeholder with a real erase using the existing `deleteObjectAt` pipeline.** Wire a mouse-press handler in Erase mode to the already-correct `PoDoFoBackend::deleteObjectAt`. Rationale: turns two admitted dead features into one shipped capability via a single UI-wiring task.

**Wave 2B — Feature-parity additions (M-effort P1):**
3. **Add opacity control to the inline text-edit and image-edit toolbars.** Reuse the existing `ExtGState`-writing code already built for watermarks. Rationale: zero opacity control exists outside watermarks despite already owning the needed PDF-graphics machinery.
4. **Add letter-spacing and line-spacing controls to `EditToolBar`.** Rationale: PDFelement ships this as a natural sibling to the existing font/size/align controls.

**Wave 2C — Larger parity investment (L-effort P1):**
5. **Add basic z-order (bring-to-front/send-to-back) for images.** Rationale: every competitor treats z-order as baseline, even Adobe's limited images-only version.

Confirm exact current file/class/function names by reading `src/shell/controllers/EditController.{h,cpp}`, `src/ui/EditToolBar.{h,cpp}`, `src/engines/podofo/PoDoFoBackend.{h,cpp}` (for `deleteObjectAt`), and `src/commands/EditTextInlineCommand.h` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 5 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/editing-parity`, based off `audit-remediation` (or `main` if that no longer exists — check first). Rebase onto the latest integration branch if it has moved since Wave 1A landed.
- **Sequencing note from the plan:** §9.2 Editing and §9.4 OCR both touch `EditController` — if the OCR domain agent is working concurrently, expect merge friction there specifically; keep your `EditController` changes narrowly scoped to editing-selection/clipboard/erase logic and avoid touching OCR-related methods in that file.
- **Incremental commits per logical group** — e.g. one commit for Cut/Copy/Delete, one for the real eraser, one for opacity control, one for letter/line-spacing, one for z-order. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path**, particularly: clipboard copy/delete round-trip, eraser invoking `deleteObjectAt` correctly, and z-order persistence across save/reload.
- **Do not touch other domains' files if avoidable.** Stay within `src/shell/controllers/EditController*`, `src/ui/EditToolBar*`, `src/commands/EditTextInlineCommand.h` and directly related image/object-selection code. If you need to touch `EditController.cpp` in a way that risks colliding with OCR wiring, isolate that change into its own clearly-labeled commit so a human can resolve conflicts easily.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Text & Object Editing (§9.2) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Minimal Cut/Copy/Delete for selected object — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Real eraser via deleteObjectAt — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Opacity control on text/image-edit toolbars — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Letter-spacing/line-spacing controls — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Basic z-order for images — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale — especially any EditController merge friction with the OCR domain>
```
</final_report_format>
