# GlyphPDF — Domain Implementation Prompt: Security (§9.11)

**Domain key:** `security` | **Priority mix:** 3×P1 (M-effort), 1×P1 (L-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Wave 2B/2C, filtered to §9.11

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation. You are picking up the Security domain — scored 6.5/10, the best-scoring domain in the audit, but with a real enforcement gap: an expired document can still be freely re-encrypted, sanitized, or redacted through menus that never check the expiry flag. (Three Wave 1A items — fixing `addTextWatermark` to honor `fontFamily` instead of hardcoded Helvetica, replacing the watermark text-centering heuristic with real font metrics, and wiring `setExpiryDate` into the UI via a date picker — are being handled by a separate agent right now; do not touch or duplicate those. Your work in this domain assumes `setExpiryDate` is now reachable from the UI.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.11 Security in full
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.11 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining Security backlog (Wave 2B + Wave 2C items — this domain has no Wave 1B items; excluding the Wave 1A watermark-fontFamily, watermark-centering, and setExpiryDate-UI-wiring fixes handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 2B — Feature-parity additions (M-effort P1):**
1. **Add a "Selective remove" checkbox mode to Sanitize Document.** Rationale: both Acrobat and Foxit present exactly this Remove-All-vs-Selective pattern; GlyphPDF's underlying routine already covers an equal or broader category set.
2. **Show a pre-commit summary/count of what Sanitize Document found and will remove.** Rationale: builds trust in an operation that is otherwise a black box, cheap given `sanitizeDocument` already walks every category.
3. **Deepen expiry enforcement beyond the viewer tool-mode flag** — gate re-encrypt/sanitize/redact/save-as behind the existing `isExpired()`/`isReadOnly()` check. Rationale: an expired document can currently still be freely re-encrypted, sanitized, or redacted through menus that never check the flag.

**Wave 2C — Larger parity investment (L-effort P1):**
4. **Bundle a static/vendored 7z library** instead of shelling to system-installed `7z.exe`. Rationale: the one place GlyphPDF's own feature contradicts its "no external dependency" pitch.

Confirm exact current file/class/function names by reading `src/shell/controllers/SecurityController.{h,cpp}` (`sanitizeDocument`, around line 275), `src/engines/podofo/PoDoFoBackend.{h,cpp}` (`sanitizeDocument`, `isExpired`, `isReadOnly`), `src/ui/EncryptionDialog.{h,cpp}`, `src/ui/PermissionsDialog.{h,cpp}`, `src/ui/DocumentPropertiesWidget.{h,cpp}`, and search for the 7z shell-out call (likely in a compression/archive-related helper or `SecurityController`) before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 4 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/security-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A watermark-fontFamily/centering and setExpiryDate-UI fixes first — item 3 here (deepening expiry enforcement) directly depends on `setExpiryDate` being reachable from the UI.
- **Incremental commits per logical group** — one commit for the Selective-remove Sanitize mode, one for the pre-commit summary/count, one for deepened expiry enforcement across re-encrypt/sanitize/redact/save-as, one for vendoring the 7z library. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Check `tests/TestSanitization.cpp`, `tests/TestEncryption.cpp` — these must stay green and should be extended.
- **Add tests for any newly-covered path** — Selective-remove category selection correctness, pre-commit summary accuracy (does the count match what's actually removed?), and expiry-enforcement gating (attempt re-encrypt/sanitize/redact/save-as on an expired document and confirm each is correctly blocked or warned).
- **7z vendoring (item 4) is the highest-risk item** — this changes a build/packaging dependency. Before starting, check `LICENSE-3RD-PARTY.md` and the packaging notes referenced in your memory files (`glyphpdf-packaging.md`) for any existing constraints on adding vendored binaries, since packaging/signing is an active release concern for this project. If vendoring cleanly within this pass isn't achievable, it is acceptable to mark this item PARTIAL/SKIPPED with a clear rationale and a concrete recommended next step — do not rush a fragile packaging change.
- **Do not touch other domains' files if avoidable.** Stay within `src/shell/controllers/SecurityController*`, `src/ui/EncryptionDialog*`, `src/ui/PermissionsDialog*`, `src/ui/DocumentPropertiesWidget*`, and the 7z-related packaging/build code.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report — item 4 explicitly introduces one (a vendored 7z lib) and this is expected; document exactly what was added and its license.
</constraints>

<final_report_format>
```
## Domain: Security (§9.11) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Selective-remove mode for Sanitize Document — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Pre-commit summary/count for Sanitize Document — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Deepen expiry enforcement (re-encrypt/sanitize/redact/save-as) — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Vendor static 7z library instead of shelling to system 7z.exe — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line, explicitly confirm TestSanitization/TestEncryption still pass; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review — especially any packaging/licensing note for the 7z vendoring>
```
</final_report_format>
