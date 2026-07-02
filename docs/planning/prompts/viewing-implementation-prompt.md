# GlyphPDF — Domain Implementation Prompt: Document Viewing (§9.1)

**Domain key:** `viewing` | **Priority mix:** 2×P0 (M-effort), 1×P0 (L-effort), 1×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/1C/2B, filtered to §9.1

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation that competes head-on with Adobe Acrobat, Foxit, and Nitro on feature depth while beating all of them structurally on zero-upload, zero-retention local processing. You are picking up the Document Viewing domain from a competitive-parity audit — this domain scored 4.5/10, dragged down by a page-rotation bug and dead click-navigation.
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index — skim for anything else relevant to viewing/rendering work)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.1 Document Viewing in full (scorecard row, structural-advantages mentions, and every §9.1 item in the P0/P1/P2 lists)
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics (build/test gate, worktree-per-domain convention, cross-domain overlap notes), then re-read the §9.1 rows in Wave 1B/1C/2B tables below
8. `CLAUDE.md` at the repo root — build/test commands, current ctest baseline, environment constants

Do NOT re-derive findings from scratch — they are already fully logged in the audit doc. Your job is to implement the fixes, not rediscover them.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's entire remaining Document Viewing backlog (Wave 1B + Wave 1C + Wave 2B items only — Wave 1A quick wins for this domain, if any, are being handled by a separate agent right now; there are none for §9.1 in Wave 1A, so this is the domain's full remaining scope) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Fix page-rotation to rotate the actual rendered bitmap, not just the annotation overlay.** Apply rotation to the real page render (`QPdfDocumentRenderOptions::setRotation` or a `QTransform` on the resulting `QImage`), keeping the `AnnotationLayer` in sync. Rationale: current behavior rotates only an invisible-until-annotated overlay while the page stays upright — a shipping correctness bug, not a missing feature.
2. **Add real hyperlink (URI + internal GoTo) click-navigation.** Wire `QPdfLinkModel` or parse `/Annots` `Link` entries into `PdfViewerWidget` with `QDesktopServices::openUrl` / go-to-page navigation and a hover cursor change. Rationale: zero code path exists today for this PRD-in-scope, table-stakes capability every competitor ships.

**Wave 1C — Big lift (L-effort P0):**
3. **Make two-page mode a true continuous live layout instead of a static spread image pair.** Keep `QPdfView`/`AnnotationLayer` active in two-page mode instead of static `QLabel` images; at minimum restore annotation visibility and search-highlight sync. Rationale: switching to two-page view today silently hides annotations/comments with no warning — a trust/correctness gap, not a cosmetic one.

**Wave 2B — Feature-parity addition (M-effort P1):**
4. **Add real content-level Night Mode (color inversion)**, distinct from chrome Dark Mode and any Eye Care tint. Rationale: "Dark Mode" currently overpromises — pages stay glaring white while only chrome darkens.

Confirm exact current file/class/function names yourself by reading `src/ui/PdfViewerWidget.{h,cpp}`, `src/ui/AnnotationLayer.{h,cpp}`, `src/shell/controllers/ViewController.{h,cpp}` before starting — the audit and plan name the right area but you must verify against current HEAD, since other domains' fixes may land in parallel.
</task>

<constraints>
- **Batch, don't fragment.** All 4 items above belong to one domain, one branch, one worktree. Do not split this into multiple sessions or hand off partial work.
- **Branch:** create your work off `audit-remediation` (or `main` if `audit-remediation` no longer exists at your start time — check first), named `feature/viewing-parity`. Rebase onto latest integration branch if it has moved.
- **Incremental commits per logical group** — e.g. one commit for the rotation fix, one for hyperlink navigation, one for two-page mode, one for Night Mode. Do not squash into a single mega-commit; a future reviewer needs to see the fix-by-fix diff.
- **Build clean + full ctest pass before considering anything done.** Currently 37-39 ctest targets, 100% green (verify actual count at your start — it may have grown from parallel domain work). Use the build/test commands in `CLAUDE.md` §3 (MSYS2 ucrt64 toolchain, `cmake --build build --parallel 8`, `ctest --output-on-failure -j4`). Never trust a stale test-result file — confirm the result file's mtime is newer than your last source edit.
- **Add tests for any newly-covered path.** In particular: page-rotation correctness (rotate 90/180/270 and confirm the rendered bitmap orientation, not just an annotation-layer flag), hyperlink click-navigation (URI + internal GoTo), and two-page mode annotation-visibility parity with single-page mode. These paths currently have no or inadequate coverage per the audit.
- **Do not touch other domains' files if avoidable.** Two known cross-domain overlaps exist elsewhere in the plan (§9.2 Editing ↔ §9.4 OCR both touch `EditController`; §9.8 Redaction ↔ §9.13 Compression both call `sanitizeDocument()`) — neither overlaps this domain, but if you find yourself editing `EditController.cpp` or files outside `src/ui/PdfViewerWidget*`, `src/ui/AnnotationLayer*`, `src/shell/controllers/ViewController*`, stop and reconsider scope.
- **Do not push or merge.** Leave the branch for human review. Do not open a PR unless explicitly asked.
- Match existing C++17/Qt6 code style in the surrounding files. No new external dependencies without flagging it prominently in your final report.
</constraints>

<final_report_format>
Produce a final report in this exact structure (matches the Wave 1A agent's report format):

```
## Domain: Document Viewing (§9.1) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Fix page-rotation to rotate actual bitmap — <1-2 sentence detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Add hyperlink click-navigation — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Two-page mode true continuous live layout — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Real content-level Night Mode — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything you skipped, descoped, or flagged for human review, with rationale>
```

If an item is SKIPPED or PARTIAL, state the blocking reason precisely (e.g. missing dependency, ambiguous spec, conflicting in-flight change from another domain) — do not silently drop it.
</final_report_format>
