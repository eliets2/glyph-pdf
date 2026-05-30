# .context/ — Per-Prompt Working Memos

This directory holds working memos written by executor agents during M2-M8 prompt execution. They are NOT authoritative — the vault notes at `C:\Users\User\.claude\memory\projects\glyphpdf\` are the authoritative record.

## Purpose

Each prompt execution creates 1-2 files here before doing any work:
- `<prompt-id>-source-analysis.md` — source code snapshot + what exists vs. what needs to be built
- `<prompt-id>-vault-summary.md` — what the vault says about this prompt's dependencies + prior sessions

These memos save ~5,000-10,000 tokens in subsequent session continuation by not re-reading source files that were already analyzed.

## Naming convention

- `m2p1-source-analysis.md` → M2-PROMPT-1 source analysis
- `m2p1-vault-summary.md` → M2-PROMPT-1 vault summary
- Suffix `-a`, `-b` when multiple passes were done: `m2p4-source-analysis-a.md`, `m2p4-source-analysis-b.md`

## Lifecycle

These files are **ephemeral working state**. They become stale as soon as the prompt they document is committed. Don't cite them as authoritative — use `git log`, `git show`, and vault notes instead.

Tracked in git so that partially-executed sessions can be resumed without re-deriving context. The memos + walkthrough files together form the "why" layer between task intent and commit diff.

## Coverage (current)

Files present cover M2-P2 through M3-P5. M4 prompts have no .context files (subagent-executed prompts didn't produce them).
