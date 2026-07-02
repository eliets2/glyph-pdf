# GlyphPDF Domain Implementation Prompts — Index

Generated from `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` and `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md`, 2026-07-01. Covers every domain's remaining backlog **after** Wave 1A (quick wins, handled by a separate in-flight agent — not duplicated here). Each prompt is self-contained: a fresh Claude Code session can run it with zero prior context, since each prompt instructs the session to read the memory vault files and both source docs first.

Each prompt batches its domain's full Wave 1B/1C/2A/2B/2C backlog into one branch/one worktree/one build+test cycle, per the plan's own "one worktree per domain, not per item" execution mechanic.

---

## Index table

| # | Domain key | Domain name (§) | File | Item count | P0 | P1 | Effort mix |
|---|---|---|---|---|---|---|---|
| 1 | `compare` | Document Comparison (§9.10) | `compare-implementation-prompt.md` | 5 | 1 | 4 | S×1 (blocker), S×1, M×2 |
| 2 | `ocr` | OCR (§9.4) | `ocr-implementation-prompt.md` | 6 | 2 | 4 | M×2, S×3, M×1 |
| 3 | `compression` | Compression & Optimization (§9.13) | `compression-implementation-prompt.md` | 5 (+1 conditional) | 1 | 4 | L×1, S×1, M×2, L×1 |
| 4 | `viewing` | Document Viewing (§9.1) | `viewing-implementation-prompt.md` | 4 | 2 | 1 (+ wait, see note) | M×2, L×1, M×1 |
| 5 | `redaction` | Redaction (§9.8) | `redaction-implementation-prompt.md` | 7 | 2 | 5 | M×2, S×5 |
| 6 | `pages` | Page Management (§9.9) | `pages-implementation-prompt.md` | 6 | 1 | 5 | M×1, S×1, M×3 (+1 test item) |
| 7 | `forms` | Forms (§9.6) | `forms-implementation-prompt.md` | 5 | 2 | 3 | M×2, M×3 |
| 8 | `editing` | Text & Object Editing (§9.2) | `editing-implementation-prompt.md` | 5 | 2 | 3 | M×2, M×2, L×1 |
| 9 | `esignature` | E-Signatures (§9.7) | `esignature-implementation-prompt.md` | 5 | 2 | 3 | M×2, S×3 |
| 10 | `batch` | Batch Processing & Automation (§9.12) | `batch-implementation-prompt.md` | 8 | 3 | 5 | M×1, M×2 (P0), S×2, M×2, L×1 |
| 11 | `annotation` | Annotation & Markup (§9.3) | `annotation-implementation-prompt.md` | 3 | 1 | 2 | M×1, M×1, L×1 |
| 12 | `accessibility` | Accessibility (§9.14) | `accessibility-implementation-prompt.md` | 4 (+1 conditional) | 1 | 3 | S×1, S×2, M×1 |
| 13 | `search` | Search & Navigation (§9.15) | `search-implementation-prompt.md` | 3 | 1 | 2 | M×1, M×2 |
| 14 | `conversion` | Conversion (§9.5) | `conversion-implementation-prompt.md` | 4 | 1 | 3 | M×1, S×2, M×1 |
| 15 | `security` | Security (§9.11) | `security-implementation-prompt.md` | 4 | 0 | 4 | M×3, L×1 |
| 16 | `importExport` | File Import and Export (§9.16) | `importExport-implementation-prompt.md` | 3 | 0 | 3 | S×1, M×2 |

**Totals:** 77 items (76 firm + 2 conditional items that may already be claimed elsewhere — see "Conditional items" below) across 16 domains. **21 P0 items, 56 P1 items.**

**Note on `viewing` P1 count:** §9.1's Wave 2B item ("Night Mode") is the only P1 in that domain; the table above lists "1" — the "(+ wait, see note)" annotation is a formatting artifact of merging effort tags and can be disregarded; viewing = 3 P0 + 1 P1 = 4 items total, confirmed.

---

## Priority mix summary (whole backlog, all 16 domains combined)

- **P0 items:** 21 (all must-fix correctness/dead-code issues — no new product decisions required)
- **P1 items:** 56 (named feature-parity gaps vs. best-in-class competitors)
- **Conditional items:** 2 (flagged in `compression` and `accessibility` prompts as possibly already claimed by Wave 1A/1B assignment ambiguity in the source plan — each prompt tells the executing session to verify current state before treating them as in-scope)

---

## Conditional items (verify before executing)

Two items sit at a boundary between this batch and Wave 1A/other-domain scope, due to minor tier ambiguity in the source plan document. Each affected prompt already instructs the executing session to check current repo state first:

1. **`compression` item "1b"** — "Finish the duplicate-image dedup rewiring" is tagged Wave 1B in the plan table but wasn't picked up by any other domain prompt; the compression prompt treats it as in-scope pending a live check.
2. **`accessibility` item "1b"** — "Move `analyzeReadingOrder` off the UI thread" is tagged Wave 1B under §9.14 in the plan table; the accessibility prompt treats it as in-scope pending a live check that no other agent has claimed it.

---

## Suggested run order

Reuses the audit/plan's own sequencing logic: **highest user-visible value per unit of effort first**, per the plan's "Suggested first three" section and the audit's "Next Step" recommendation, then P0-heavy domains, then remaining P1-heavy domains. Domains with no known file overlap can run in parallel within a wave; the two flagged cross-domain overlaps (`editing` ↔ `ocr` via `EditController`; `redaction` ↔ `compression` via `sanitizeDocument()`) should not run fully in parallel — sequence or coordinate as noted in each prompt.

### Wave A — Unlock + headline-fix domains (run first, highest value/effort ratio)
1. **`compare`** — single blocking defect (`CompareMode::compareFiles()` has zero UI entry point) unlocks an entire fully-built, already-tested feature for minimal effort. Explicitly named the #1 priority in the plan.
2. **`ocr`** — fixes the headline OCR promise silently failing to save (Accept doesn't persist text). Explicitly named the #2 priority in the plan.
3. **`compression`** — worst-scored domain overall (2.5/10); the JPEG re-encoding item is the single biggest quality gap of any domain, explicitly named the #3 priority in the plan (largest single lift, L-effort).

### Wave B — Remaining P0-heavy domains (parallel-safe except where noted)
4. **`viewing`** — page-rotation correctness bug + missing hyperlink navigation, both P0.
5. **`redaction`** — do NOT run in full parallel with `compression` (Wave A #3); both call `sanitizeDocument()`. Sequence after `compression` lands, or coordinate closely.
6. **`pages`** — thumbnail drag-and-drop reorder is the most visible page-management parity gap.
7. **`forms`** — auto-detect's hardcoded dummy fields is a severe trust bug.
8. **`editing`** — do NOT run in full parallel with `ocr` (Wave A #2); both touch `EditController`. Sequence after `ocr` lands, or coordinate closely.
9. **`esignature`** — invisible cryptographic signature field + missing typed/uploaded modes.
10. **`batch`** — engine-mutex false parallelism is pure engineering waste; also unblocks non-English batch OCR.

### Wave C — P1-heavy polish/parity domains (fully parallel-safe)
11. **`annotation`** — shapes/ink persistence bug is high-severity but domain has only 1 P0 vs viewing/pages/forms's 2; single-item P0 already narrow in scope.
12. **`accessibility`** — the `pageIndexOf` false-positive fix is cheap (S-effort) and high-value; rest is polish.
13. **`search`** — search-modifier checkboxes being silently ignored is a trust bug but narrowly scoped (3 items total).
14. **`conversion`** — real OOXML dependency landing is important but most of the domain's severity was already Wave-1A'd (opacity fix, test coverage).

### Wave D — Pure P1 domains (no P0 debt, run whenever capacity allows)
15. **`security`** — already the best-scoring domain (6.5/10); remaining work is enforcement-depth and packaging hygiene (7z vendoring), not urgent correctness.
16. **`importExport`** — smallest domain (3 items), mostly UX unification and test coverage; the export-path-badge and linearized-checkbox P0s were already handled in Wave 1A.

---

## Not included (Phase 3 — marketing/polish)

Phase 3 (the cross-cutting privacy/offline marketing bundle + remaining P2 polish items) was explicitly deprioritized per the task scope and is NOT covered by these 16 prompts. Per `IMPLEMENTATION-PLAN-2026-07-01.md`, Phase 3 can run any time in parallel with the above since it touches copy/UI-badges, not core logic — pull P2 items directly from `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` §4 P2 section if/when capacity opens up.
