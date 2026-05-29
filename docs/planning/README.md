# GlyphPDF — Planning Artifacts

This folder contains the Branch C SCOPE LOCK execution artifacts. These are **working documents** (not user-facing docs) used by Claude Code sessions executing the M2-M8 sprint plan.

For the public-facing project introduction, see `/README.md`.
For the Claude Code session memory, see `/CLAUDE.md`.

## Files in this folder

| File | Purpose | Size |
|---|---|---|
| **`AUDIT-v1.0.0.md`** | Comprehensive 11-blocker audit + decision tree (Branch A/B/C analysis) + positioning + appendices A-E (architecture / UI / code-quality / gap / industry benchmark) | ~1,982 lines |
| **`M1-REMEDIATION.md`** | Month 1 remediation prompt (closes 11 release blockers). **Already executed** — commit `a6ea6aa` (May 27, 2026). Kept for reference. | ~472 lines |
| **`MSYS2-MIGRATION.md`** | MSYS2 ucrt64 build environment migration prompt. **Already executed** — commits `45807de` + `6e7c8aa` + `9ac0c2f` (May 28, 2026). Kept for reference. | ~669 lines |
| **`MONTHS-2-8-PROMPTS.md`** | All 34 launch prompts for M2-M8 Branch C execution. Each prompt is a self-contained 7-H formatted message ready to paste into a fresh Claude Code session. | ~2,229 lines |

## How to use these

1. Pick the prompt for the work you want to execute (see `MONTHS-2-8-PROMPTS.md` index).
2. Open a **fresh** Claude Code session rooted at `C:\Users\User\Projects\pdf`.
3. Copy the entire `<session_metadata>` ... `</success_criteria>` block from the prompt file.
4. Paste as the first message in the new session. Don't add commentary.
5. The session executes following the STANDARD EXECUTION PROTOCOL (analyze → plan → implement → verify → context-gate at 50% → final verification + atomic commit).
6. After completion: **independently verify** ctest pass (see `/CLAUDE.md` §7 Pattern 1).
7. Close the session. Start a fresh one for the next prompt.

## Dependency-aware execution order

See `/CLAUDE.md` §5 OR the Obsidian vault `projects/glyphpdf/05-prompts-index.md` for the full dependency graph. Short version:

- M2-PROMPT-1 → M2-PROMPT-2/3/4/5 (parallel)
- M3-PROMPT-1/2/3/4/5 (all parallel)
- M4-PROMPT-1..6 (parallel by controller) + M4-PROMPT-7 (independent — Djot foundation)
- M5-PROMPT-1 (RapidOCR real) → M5-PROMPT-2 (needs M2-PROMPT-5 + M5-PROMPT-1) → M5-PROMPT-4 (needs M4-PROMPT-7 + M5-PROMPT-2). M5-PROMPT-3 independent.
- M6 mostly parallel except PROMPT-4 (needs M4-PROMPT-7 + M3-PROMPT-5)
- M7-PROMPT-1/2 parallel; M7-PROMPT-3 needs M5 outputs + M4-PROMPT-7
- M8-PROMPT-1 → -2 → -3 (sequential)

## Related project documents

| Document | Purpose |
|---|---|
| `/CLAUDE.md` | Auto-loads in every CC session opened in this repo; comprehensive project context |
| `/SESSION_BRIEF_NEXT.md` | State-of-the-art for next CC session; quick-orient |
| `/PRD.md` | Product requirements document |
| `/ROADMAP.md` | Engineering roadmap (Sessions 1-20 + WS1/WS2/WS3 + Phase 1.5) |
| `/CHANGELOG.md` | Version history; `[1.0.0-internal]` marker + `[Unreleased]` M2-M8 work |
| `/LICENSE-3RD-PARTY.md` | Dependency license matrix |
| `/README.md` | Public-facing project intro + build instructions |
| **Obsidian vault** `C:\Users\User\.claude\memory\projects\glyphpdf\` | Rich reference notes: 00-overview, 01-current-state, 02-architecture, 03-build-environment, 04-scope-lock, 05-prompts-index, 06-non-negotiables, **07-sessions-log** (Antigravity + CC history), **08-lessons-learned** (recurring failure patterns), 09-license-policy |

## SCOPE LOCK reminder

**Real public v1.0.0 does not ship until all 34 M2-M8 prompts complete** (~8-9 months from M2 start, M8 target launch). The current `dist/GlyphPDF-1.0.0-x64.msi` is **internal-only** — NOT for public distribution under any v1.0.0 label.

See `/CLAUDE.md` §5 + `/CHANGELOG.md` `[1.0.0-internal]` warning for full context.
