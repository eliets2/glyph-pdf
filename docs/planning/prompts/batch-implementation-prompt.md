# GlyphPDF — Domain Implementation Prompt: Batch Processing & Automation (§9.12)

**Domain key:** `batch` | **Priority mix:** 3×P0 (S/M-effort), 2×P1 (S-effort), 2×P1 (M-effort), 1×P1 (L-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A/2B/2C, filtered to §9.12

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose hot-folder live filesystem watch with auto-run has no equivalent in Adobe/Foxit/PDF24 (only generic RPA tools come close). You are picking up the Batch Processing & Automation domain, whose worst engineering-waste finding is that an engine-wide mutex secretly serializes 5 of 7 "parallel" batch operations — threads spin up and immediately block for zero benefit. (One Wave 1A item — collapsing multi-pattern batch redaction into a single load/redact/save pass per file — is being handled by a separate agent right now; do not touch or duplicate that.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.12 Batch processing & automation in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.12 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants, and `Pattern 19: TestBatchMode flaky under parallel ctest` (RESOURCE_LOCK note) — relevant since you are changing this domain's concurrency model

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Batch Processing & Automation backlog (Wave 1B + Wave 2A + Wave 2B + Wave 2C items — excluding the Wave 1A multi-pattern-redaction-collapse fix handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Fix engine-mutex false parallelism for non-Convert batch ops.** Cap the thread pool or move to per-file engine instances so 5 of 7 "parallel" operations aren't secretly serialized behind one mutex. Rationale: pure engineering waste — threads spin up and immediately block for zero benefit.
2. **Add low-confidence-word highlighting/flagging to batch OCR output.** Surface `OcrPipeline`'s existing confidence data into the output PDF or log. Rationale: batch OCR today is pass/fail only with zero visibility into which outputs need review.
3. **Expose OCR language selection in the Batch UI.** Rationale: batch OCR is currently unusable for any non-English document set — a hard functional wall competitors don't have.

**Wave 2A — Quick parity wins (S-effort P1):**
4. **Add named redaction pattern presets (PII quick-picks).** Rationale: closes the Adobe parity gap without requiring users to hand-author SSN/phone/email/credit-card regex.
5. **Make Compress/Optimize target DPI user-configurable with named presets.** Rationale: closes a concrete, named Foxit parity gap with minimal risk.

**Wave 2B — Feature-parity additions (M-effort P1):**
6. **Move Merge onto the async QtConcurrent pipeline with real progress.** Rationale: the one batch operation that can visibly freeze the app today on large file sets.
7. **Add recursive hot-folder watching and a polling fallback.** Rationale: makes an already-differentiating feature robust for real enterprise use (network shares, nested folders).

**Wave 2C — Larger parity investment (L-effort P1):**
8. **Add automated test coverage for Merge/OCR/Redact/Compress/Watermark/Export-PDF-A** (6 of 7 production-wired batch operations currently ship with zero CI safety net).

Confirm exact current file/class/function names by reading `src/modes/BatchMode.{h,cpp}` (the `m_engineMutex`/`engineMutexPtr` mutex around line 905-969, the hot-folder watch logic, and each `Op*` handler), `src/engines/ocr/OcrPipeline.{h,cpp}` (confidence data), `src/engines/scheduling/LaneScheduler.{h,cpp}` and `PipelineStage.h` (the async pipeline pattern to reuse for Merge), and `tests/TestBatchMode.cpp` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 8 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/batch-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A multi-pattern-redaction-collapse fix first — your mutex-parallelism fix (item 1) will touch the same `BatchMode` concurrency machinery, so verify compatibility.
- **Concurrency-fix risk (item 1) is the highest-risk item in this domain.** `TestBatchMode` is already known-flaky under parallel ctest (see `CLAUDE.md` Pattern 19, mitigated today with `RESOURCE_LOCK BatchModeIO`). When you change the mutex/threading model, re-verify this test with `ctest --output-on-failure -j4 --repeat-until-fail 3` specifically, not just a single pass — a concurrency fix that "passes once" is not proof of correctness here.
- **Incremental commits per logical group** — one commit for the mutex/parallelism fix, one for low-confidence-word flagging in batch OCR, one for OCR language selection in Batch UI, one for named redaction presets, one for configurable Compress DPI presets, one for async Merge with progress, one for recursive hot-folder watching + polling fallback, one for the new Merge/OCR/Redact/Compress/Watermark/Export-PDF-A test coverage. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands, and additionally run `ctest --output-on-failure -j4 --repeat-until-fail 3` for `TestBatchMode` specifically given the concurrency changes.
- **Add tests for any newly-covered path** — item 8 is explicitly about closing this domain's test-coverage gap (Merge/OCR/Redact/Compress/Watermark/Export-PDF-A), but also add regression coverage proving the mutex fix actually achieves real parallelism (e.g. assert multiple non-Convert ops run concurrently, not serialized) and hot-folder recursive-watch behavior.
- **Do not touch other domains' files if avoidable.** Stay within `src/modes/BatchMode*`, `src/engines/scheduling/*`, the batch-specific portions of `src/engines/ocr/OcrPipeline.{h,cpp}` (read-only access to confidence data, not modifying core OCR logic — that's the OCR domain's territory), and `tests/TestBatchMode.cpp`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Batch Processing & Automation (§9.12) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Fix engine-mutex false parallelism for non-Convert ops — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Low-confidence-word flagging in batch OCR output — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] OCR language selection exposed in Batch UI — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Named redaction pattern presets (PII quick-picks) — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Configurable Compress/Optimize target DPI presets — <detail, commit hash>
6. [DONE|SKIPPED|PARTIAL] Async Merge via QtConcurrent with real progress — <detail, commit hash>
7. [DONE|SKIPPED|PARTIAL] Recursive hot-folder watching + polling fallback — <detail, commit hash>
8. [DONE|SKIPPED|PARTIAL] Test coverage for Merge/OCR/Redact/Compress/Watermark/Export-PDF-A — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; explicitly report TestBatchMode result under --repeat-until-fail 3; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
