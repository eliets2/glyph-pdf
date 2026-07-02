# GlyphPDF — Domain Implementation Prompt: OCR (§9.4)

**Domain key:** `ocr` | **Priority mix:** 2×P0 (M-effort), 3×P1 (S-effort), 1×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B, filtered to §9.4

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose dual-engine ROVER OCR ensemble (Tesseract 5 + RapidOCR/PP-OCRv5, IoU word-level merge, 3 voting strategies, real ONNX RT-DETR layout detector) is architecturally ahead of Adobe and ABBYY — but the audit found the interactive OCR path silently fails to save its own output. (Wave 1A — wiring the language selector into `EditController::runOcr()` — is being handled by a separate agent right now; do not touch or duplicate that specific fix, but your work depends on it landing.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.4 OCR in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics (note the explicit sequencing guidance: §9.2 Editing and §9.4 OCR both touch `EditController` — coordinate/sequence, don't parallelize blindly), then re-read §9.4 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining OCR backlog (Wave 1B + Wave 2A + Wave 2B items — excluding the Wave 1A language-selector wiring handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Make the interactive "Run OCR → Accept" flow call `exportMrcPdfA`** so the searchable text layer is actually saved. Route Accept through the same production-quality, veraPDF-validated writer already used by Batch Mode. Rationale: the primary interactive OCR path currently does not persist searchable text at all — the PRD's headline OCR claim silently fails for most users.
2. **Implement page-level orientation detection (0/90/180/270).** Implement real detection (e.g. Tesseract OSD, already linked) behind the existing but dead `orientDetect` flag. Rationale: flag and code comment already acknowledge the gap; competitors treat this as always-on baseline preprocessing.

**Wave 2A — Quick parity wins (S-effort P1):**
3. **Make "Re-OCR this region" actually region-scoped** (currently re-runs the whole page). Rationale: a visible, clickable context-menu item silently does the wrong, more expensive thing.
4. **Surface which binarization/deskew path is active in the OCR Verify UI.** Rationale: a build without Leptonica silently gets materially worse preprocessing with no way to know why.
5. **Add automated test coverage for language pass-through and orientation correction.** Rationale: zero test coverage exists for either sub-feature today.

**Wave 2B — Feature-parity addition (M-effort P1):**
6. **Add a lightweight `OutputMode` choice (searchable image vs editable text).** Rationale: closes the PRD's explicitly-called-out zero-matching-code gap using components already built.

Confirm exact current file/class/function names by reading `src/shell/controllers/EditController.{h,cpp}` (`runOcr`), `src/engines/mrc/MrcPageProcessor.{h,cpp}` (`exportMrcPdfA`), `src/engines/ocr/OcrPipeline.{h,cpp}`, `src/engines/ocr/OcrPreprocessor.{h,cpp}` (binarization/deskew/orientation), `src/modes/OCRMode.{h,cpp}`, and `src/engines/OcrEngine.{h,cpp}` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 6 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/ocr-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A language-selector fix first — item 1 (routing Accept through `exportMrcPdfA`) should use the already-language-correct OCR call, not hardcoded "eng".
- **Sequencing with §9.2 Editing:** both domains touch `EditController`. If the editing-domain agent's branch has already merged, rebase cleanly on top of it. If it's still in flight, keep your `EditController::runOcr()` changes narrowly scoped to OCR-Accept/export logic and flag any conflict area explicitly in your final report so a human can reconcile the two branches.
- **Incremental commits per logical group** — e.g. one commit for exportMrcPdfA routing, one for orientation detection, one for region-scoped re-OCR, one for binarization/deskew surfacing, one for the new tests, one for OutputMode choice. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path** — this domain explicitly calls out zero coverage for language pass-through and orientation correction (item 5 above); also add coverage for the Accept→exportMrcPdfA save-persistence path (the single most severe bug in this domain) and region-scoped re-OCR behavior.
- **Do not touch other domains' files if avoidable.** Stay within `src/engines/ocr/*`, `src/engines/mrc/*`, `src/engines/OcrEngine*`, `src/modes/OCRMode*`, and the OCR-specific portions of `EditController`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: OCR (§9.4) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Route interactive Accept through exportMrcPdfA — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Page-level orientation detection (Tesseract OSD) — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Region-scoped Re-OCR — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Surface active binarization/deskew path in Verify UI — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Test coverage for language pass-through + orientation — <detail, commit hash>
6. [DONE|SKIPPED|PARTIAL] OutputMode choice (searchable image vs editable text) — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any EditController merge friction with the Editing domain>
```
</final_report_format>
