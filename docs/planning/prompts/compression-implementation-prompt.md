# GlyphPDF — Domain Implementation Prompt: Compression & Optimization (§9.13)

**Domain key:** `compression` | **Priority mix:** 1×P0 (L-effort, the big lift), 1×P1 (S-effort), 2×P1 (M-effort), 1×P1 (L-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1C/2A/2B/2C, filtered to §9.13

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the Compression & Optimization domain — the single worst-scored domain in the entire audit at 2.5/10, because the Quality/DPI controls are UI theater: no real JPEG re-encoding path exists for the DCTDecode streams that dominate real-world PDFs. (Two Wave 1A items — reusing the existing `sanitizeDocument()` in the Compress flow instead of a thin subset, and fixing/removing the size-estimate heuristics that don't match the write path — are being handled by a separate agent right now; do not touch or duplicate those.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.13 Compression and optimization in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics (note: §9.8 Redaction and §9.13 Compression both call into `sanitizeDocument()` — sequence carefully with that domain, and note this domain's L-effort JPEG re-encoding item is explicitly one of the plan's "suggested first three" highest-value fixes project-wide), then re-read §9.13 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Compression & Optimization backlog (Wave 1C + Wave 2A + Wave 2B + Wave 2C items — excluding the Wave 1A sanitizeDocument-reuse and size-estimate-heuristic fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1C — Big lift (L-effort P0):**
1. **Real JPEG re-encoding for the Quality/DPI controls.** Implement an actual decode/downsample/re-encode path for `DCTDecode`/JPEG streams. Rationale: the worst quality gap of any domain audited — a visible Quality spinner currently does nothing to most real-world PDFs, which are dominated by JPEG images. This is the single highest-value item in this entire domain — prioritize it.

(Note: "Finish the duplicate-image dedup rewiring" — completing the XObject-reference rewiring + object removal "last mile" — is also listed as Wave 1B M-effort in the plan; confirm during your read of the plan doc whether it has already been picked up by another wave/agent before you start, since the plan text places it in 1B while this prompt's remaining-scope filter treats Wave 1A/1B items assigned elsewhere as out of scope. If unclaimed, treat it as in-scope for this pass and add it as item 1b.)

**Wave 2A — Quick parity win (S-effort P1):**
2. **Add a real post-compression before/after size readout.** Rationale: every competitor shows the real result, not just a prediction.

**Wave 2B — Feature-parity additions (M-effort P1):**
3. **Implement unused/unreferenced object removal (mark-and-sweep).** Rationale: currently a pure UI placebo with a flat 5% fudge-factor estimate and no actual logic.
4. **Extend downsampling coverage to Gray/CMYK/indexed raw streams.** Rationale: incremental, bounded extension of code that already works for one narrow case.

**Wave 2C — Larger parity investment (L-effort P1):**
5. **Implement real font subsetting for the existing "Subset fonts" checkbox.** Rationale: converts an inert checkbox into Adobe-parity functionality with high payoff on CJK/large-Unicode documents.

Confirm exact current file/class/function names by reading `src/modes/CompressDialog.{h,cpp}`, `src/engines/PdfEditorEngine.{h,cpp}` (compression/optimize entry points), `src/engines/podofo/PoDoFoBackend.{h,cpp}` (stream/XObject handling, downsampling), and check for an existing JPEG codec dependency (libjpeg-turbo is already an MSYS2 dependency per `CLAUDE.md` §3 — confirm it's linked and available for the re-encode path) before starting.
</task>

<constraints>
- **Batch, don't fragment.** All items above belong to one domain, one branch, one worktree.
- **Branch:** `feature/compression-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A `sanitizeDocument()`-reuse and size-estimate fixes first — item 2 (real post-compression size readout) must reflect what the write path actually produces after those Wave 1A fixes land, not the old inflated estimate.
- **Sequencing with §9.8 Redaction:** both domains call `sanitizeDocument()`. Do not modify `sanitizeDocument()`'s internals yourself if the Redaction domain agent is concurrently changing them — call the existing API, and if you need a behavior change to it, flag it explicitly in your final report rather than editing it unilaterally.
- **Item 1 (real JPEG re-encoding) is a substantial, correctness-critical change.** Be conservative: decode → downsample (respecting the user's DPI target) → re-encode at the user's quality setting, verify the resulting PDF still renders correctly (visually and via PDFium) before considering it done. Do not ship a re-encode path that silently corrupts images — that would be a worse regression than the current no-op.
- **Incremental commits per logical group** — one commit for real JPEG re-encoding, one for the dedup "last mile" (if in scope, see note above), one for the real post-compression size readout, one for mark-and-sweep unused-object removal, one for Gray/CMYK/indexed downsampling, one for font subsetting. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path** — this domain has essentially zero prior test coverage on the real compression logic per the audit, so add coverage for: JPEG re-encoding correctness (file size actually shrinks at lower quality, image still decodes/renders), post-compression size-readout accuracy (matches actual output file size), mark-and-sweep object removal (unreferenced objects actually gone, referenced ones untouched), Gray/CMYK/indexed downsampling, and font subsetting (subset font still renders all used glyphs correctly, file size shrinks).
- **Do not touch other domains' files if avoidable.** Stay within `src/modes/CompressDialog*`, the compression/optimize-specific sections of `src/engines/PdfEditorEngine*` and `src/engines/podofo/PoDoFoBackend*`. Avoid editing `sanitizeDocument()`'s implementation directly.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report (libjpeg-turbo should already be available per the project's MSYS2 dependency list — confirm rather than assume).
</constraints>

<final_report_format>
```
## Domain: Compression & Optimization (§9.13) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Real JPEG re-encoding for Quality/DPI controls — <detail, commit hash>
1b. [DONE|SKIPPED|PARTIAL|N/A-already-handled] Duplicate-image dedup rewiring (XObject rewiring + object removal) — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Real post-compression before/after size readout — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Unused/unreferenced object removal (mark-and-sweep) — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Downsampling extended to Gray/CMYK/indexed streams — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Real font subsetting for "Subset fonts" checkbox — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any sanitizeDocument() coordination note with the Redaction domain, and confirmation of whether item 1b was already claimed by another agent>
```
</final_report_format>
