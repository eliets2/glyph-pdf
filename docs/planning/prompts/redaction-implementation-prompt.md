# GlyphPDF — Domain Implementation Prompt: Redaction (§9.8)

**Domain key:** `redaction` | **Priority mix:** 2×P0 (M-effort), 5×P1 (S-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A, filtered to §9.8

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose signed-document redaction hard-block (refuses to redact a signed PDF in place — Acrobat merely invalidates the signature afterward) is a genuine structural advantage for legal/healthcare/government users. You are picking up the Redaction domain, whose manual-marking UX is currently split across two disconnected subsystems and whose black-box redaction can leave the same PII sitting in document metadata. (One Wave 1A item — wiring "Clear Marks" to actually remove placed redaction annotations — is being handled by a separate agent right now; do not touch or duplicate that.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.8 Redaction in full, including the signed-document redaction hard-block description in the Structural Advantages section — this must never regress
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics (note: §9.8 Redaction and §9.13 Compression both call into `sanitizeDocument()` — sequence carefully with that domain), then re-read §9.8 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Redaction backlog (Wave 1B + Wave 2A items — excluding the Wave 1A "Clear Marks" fix handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Expose "Mark Region"/"Mark All Occurrences" inside `RedactMode` itself** (currently split across disconnected subsystems — wire the hidden pills to the existing `AnnotationLayer` placement path used elsewhere). Rationale: manual marking is split across two disconnected subsystems, a real UX regression versus Acrobat/Foxit's one-entry-point design.
2. **Bundle an optional "Sanitize Document" pass into the redaction Apply flow.** Offer default-on metadata/hidden-content scrub alongside content-stream excision, reusing the existing `sanitizeDocument()` routine. Rationale: Acrobat bundles this by default because a black box is meaningless if the same PII sits in metadata/attachments — a compliance risk, not cosmetic.

**Wave 2A — Quick parity wins (S-effort P1):**
3. **Add a Cancel/Back control to `RedactMode`'s panel.** Rationale: removing a broken button instead of fixing it just relocates the same missing-affordance problem.
4. **Add optional overlay text/redaction reason codes on the black box.** Rationale: legal/FOIA users expect to see why something was redacted printed on the box itself.
5. **Support a multi-term/word-list import for pattern redaction.** Rationale: Foxit's import-a-text-file workflow is explicitly lower-friction than composing a delimited regex string by hand.
6. **Add per-file confirmation before BatchMode `OpRedact` silently overwrites existing outputs.** Rationale: a known, disclosed data-loss risk already flagged in GlyphPDF's own audit doc.
7. **Upgrade the opt-in audit log to record which pattern/category matched.** Rationale: a log that says "7 regions redacted" is nearly useless for compliance review, the entire point of an audit trail.

Confirm exact current file/class/function names by reading `src/modes/RedactMode.{h,cpp}` (note the `m_pillMarkRegion`/`m_pillMarkAll`/`m_clearBtn` UI elements and the O1 comment marking them as unwired), `src/engines/PatternRedactor.{h,cpp}`, `src/ui/AnnotationLayer.{h,cpp}` (the placement path to reuse), `src/engines/podofo/PoDoFoBackend.cpp` (`sanitizeDocument`, around line 1992), and `src/modes/BatchMode.cpp` (`OpRedact` handling) before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 7 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/redaction-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A "Clear Marks" fix first.
- **Sequencing with §9.13 Compression:** both domains call `sanitizeDocument()`. Coordinate scope carefully — your item 2 (bundling Sanitize into the redaction Apply flow) should call the existing `sanitizeDocument()` API as-is; do NOT modify `sanitizeDocument()`'s internals yourself if the Compression domain agent is concurrently changing them. If you need a behavior change to `sanitizeDocument()` itself, flag it explicitly in your final report rather than editing it unilaterally.
- **Non-negotiable:** do not weaken or bypass the signed-document redaction hard-block while wiring Mark Region/Mark All or bundling Sanitize — verify the hard-block still fires correctly for signed PDFs after your changes (there should be existing regression tests for this; extend, don't remove them).
- **Incremental commits per logical group** — e.g. one commit for wiring Mark Region/Mark All into `AnnotationLayer`, one for bundling Sanitize into Apply, one for Cancel/Back control, one for overlay text/reason codes, one for word-list import, one for BatchMode per-file overwrite confirmation, one for audit-log pattern/category upgrade. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestRedaction.cpp`, `tests/TestPatternRedact.cpp`, `tests/TestSanitization.cpp` — these must stay green.
- **Add tests for any newly-covered path** — Mark Region/Mark All placement round-trip, Sanitize-bundled-into-Apply behavior (metadata actually scrubbed), word-list import parsing, BatchMode overwrite-confirmation logic, and audit-log pattern/category recording.
- **Do not touch other domains' files if avoidable.** Stay within `src/modes/RedactMode*`, `src/engines/PatternRedactor*`, the redaction-specific sections of `src/modes/BatchMode.cpp`, and audit-log-related code. Avoid editing `sanitizeDocument()`'s implementation directly — call it, don't change it, unless no other option exists (flag if so).
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Redaction (§9.8) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Wire Mark Region/Mark All into RedactMode via AnnotationLayer — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Bundle optional Sanitize Document pass into redaction Apply — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Cancel/Back control on RedactMode panel — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Overlay text/reason codes on black box — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] Multi-term/word-list import for pattern redaction — <detail, commit hash>
6. [DONE|SKIPPED|PARTIAL] Per-file confirmation before BatchMode OpRedact overwrite — <detail, commit hash>
7. [DONE|SKIPPED|PARTIAL] Audit log records matched pattern/category — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line, explicitly confirm TestRedaction/TestPatternRedact/TestSanitization still pass; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any sanitizeDocument() coordination note with the Compression domain>
```
</final_report_format>
