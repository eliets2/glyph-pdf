# Vault Audit — GlyphPDF Obsidian Branch

**Date:** 2026-06-09
**Vault path:** `C:\Users\User\.claude\memory\projects\glyphpdf\`
**Ground truth:** branch `audit-remediation`, HEAD `b0b12fb`, 33/33 ctest pass, build clean -Werror.

## Files changed

| File | Fix applied |
|---|---|
| `00-overview.md` | Bumped `last_updated` to 2026-06-09; replaced stale "Current state" section with a pointer to CLAUDE.md + accurate HEAD/test/pending block; updated "Branch contents" table to include `10-audit-2026-06-02` and `docs/audit/round2/` |
| `01-current-state.md` | Bumped `last_updated`; added STATE block (branch `audit-remediation`, HEAD `b0b12fb`, 33/33); added remediation commit map table (WP-1 through WP-8a); updated test-status header from stale "14 targets" to "33/33 current" |
| `05-prompts-index.md` | Bumped `last_updated`; changed pre-execution checklist "14/14 tests pass" → "33/33 tests pass" |
| `10-audit-2026-06-02.md` | Changed status from "Remediation IN PROGRESS" to "Remediation LARGELY COMPLETE"; updated test count from 32/32 to 33/33; added HEAD `b0b12fb`; expanded WP status entries: WP-4/C-02 ✅, WP-6 ✅, WP-7 ✅, SP-2 ✅, WP-8a ✅; documented still-pending items (SP-5b, SP-6, i18n, G-07) |

## Files preserved as-is (history, not drift)

- `07-sessions-log.md` — old session numbers are legitimate history
- `08-lessons-learned.md` — pattern numbers are legitimate history

## Most important drift resolved

The **3-way "current state" disagreement**: `00-overview.md` had a self-contained stale state block (HEAD `e09404d`, 26 ctest, May-2026 sprint summaries) contradicting `01-current-state.md` (HEAD `30ec27c`, 32 ctest) contradicting CLAUDE.md (HEAD `b0b12fb`, 33 ctest). Fixed by making CLAUDE.md the canonical live state, turning `00-overview.md`'s section into a pointer, and updating `01-current-state.md` with the correct branch/HEAD and a STATE block that now matches ground truth.
