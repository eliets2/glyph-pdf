# GlyphPDF — Domain Implementation Prompt: Forms (§9.6)

**Domain key:** `forms` | **Priority mix:** 2×P0 (M-effort), 3×P1 (M-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2B, filtered to §9.6

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation competing with Adobe Acrobat and Foxit on forms workflows. You are picking up the Forms domain, whose worst quality gap is a form auto-detect feature that silently places 3 hardcoded dummy fields regardless of actual document content while reporting success. (Three Wave 1A items — fixing `fillForm`'s unintended `SetReadOnly` side effect, making Radio/PushButton no-op visible instead of a silent `qDebug` skip, and moving Calculated field into the standard Forms ribbon/menu — are being handled by a separate agent right now; do not touch or duplicate those.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.6 Forms in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.6 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Forms backlog (Wave 1B + Wave 2B items — excluding the Wave 1A `SetReadOnly`/Radio-PushButton/Calculated-field fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Persist Required flag and Tooltip as real PDF metadata.** Wire the existing UI checkbox/textbox to actually write the `/Ff` required bit and `/TU`. Rationale: UI already collects this data and users believe it's saved; also has a WCAG 2.1 government-compliance angle since April 2026.
2. **Give auto-detect a real content-aware heuristic, or clearly gate/relabel it as experimental.** Replace the stub returning 3 hardcoded dummy fields with a real text/underline/box heuristic and review-before-commit, or relabel as a demo feature. Rationale: the worst quality gap in the section — the feature silently places fake fields regardless of document content and reports success.

**Wave 2B — Feature-parity additions (M-effort P1):**
3. **Move tab order off the `/CO` array onto a spec-correct mechanism**, and resolve the collision with calculated fields. Rationale: a real spec-conformance bug — no conforming viewer honors `/CO` for tab order, and it silently clobbers genuine calculation order.
4. **Add a real Digital Signature field type distinct from Text.** Rationale: Signature currently hard-maps to a plain text box — a functional mismatch a customer notices immediately.
5. **Harden the CSV/FDF import parser to match the export side's rigor.** Rationale: the export path was already hardened against a real audit finding; import handling the same formats was not.

Confirm exact current file/class/function names by reading `src/engines/FormManager.{h,cpp}` (`fillForm`, `autoDetectFields` — note the current stub at line ~459), `src/core/interfaces/IFormManager.h`, `src/modes/FormBuilderMode.{h,cpp}`, `src/modes/FormFieldPropertiesPanel.{h,cpp}`, and `src/commands/{Add,Delete,Edit,Move,Resize}FormFieldCommand.h` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 5 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/forms-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A `SetReadOnly`/Radio-PushButton/Calculated-field fixes first.
- **Incremental commits per logical group** — e.g. one commit for Required/Tooltip metadata persistence, one for the auto-detect heuristic (or honest relabel), one for tab-order mechanism + calculated-field collision fix, one for the Digital Signature field type, one for CSV/FDF import hardening. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands.
- **Add tests for any newly-covered path** — in particular: Required/Tooltip round-trip through save/reload, auto-detect's new heuristic against real (non-dummy) sample documents (see `tests/TestFormBuilder.cpp`, `tests/TestFormPersistence.cpp` for existing patterns to extend), tab-order/calculated-field interaction, and CSV/FDF import parity with the already-hardened export path (check `tests/fixtures` for existing CSV/FDF samples to reuse).
- **Decision point on auto-detect (item 2):** you have two acceptable paths — implement a real text/underline/box heuristic, OR clearly gate/relabel the feature as experimental (e.g. an "Experimental" badge + confirmation dialog before committing detected fields). Pick whichever is achievable at solid quality within this pass; do not ship a heuristic that's barely better than the dummy stub. State which path you took and why in your final report.
- **Do not touch other domains' files if avoidable.** Stay within `src/engines/FormManager*`, `src/core/interfaces/IFormManager.h`, `src/modes/FormBuilderMode*`, `src/modes/FormFieldPropertiesPanel*`, and the `*FormFieldCommand.h` command headers.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: Forms (§9.6) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Persist Required flag + Tooltip as real PDF metadata — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Auto-detect: real heuristic OR honest experimental relabel (state which) — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Tab order off /CO array + calculated-field collision fix — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Real Digital Signature field type — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] CSV/FDF import parser hardening — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
