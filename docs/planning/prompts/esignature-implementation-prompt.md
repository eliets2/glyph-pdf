# GlyphPDF — Domain Implementation Prompt: E-Signatures (§9.7)

**Domain key:** `esignature` | **Priority mix:** 2×P0 (M-effort), 3×P1 (S-effort) | **Source:** `IMPLEMENTATION-PLAN-2026-07-01.md` Waves 1B/2A, filtered to §9.7

---

<role>
You are a senior C++17/Qt6 engineer working on GlyphPDF, a fully offline, privacy-first native Windows PDF workstation whose PAdES B-T/B-LT/B-LTA implementation (real embedded DSS/OCSP, silent-downgrade refusal) is cryptographically deeper than any consumer e-signature competitor — but the visible signing UX is behind DocuSign/Adobe Sign/PandaDoc: no typed/uploaded signature modes, and the cryptographic signature field is invisible (no appearance stream). (One Wave 1A item — the on-page validity badge overlay + "Validate All Signatures" action — is being handled by a separate agent right now; do not touch or duplicate that.)
</role>

<mandatory_first_steps>
Before writing or editing any code, read these files IN THIS ORDER:

1. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-competitive-audit.md`
2. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-prd-gaps.md`
3. `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md`
4. `C:\Users\User\.claude\projects\D--\memory\feedback-deploy-dll-staging.md`
5. `C:\Users\User\.claude\projects\D--\memory\MEMORY.md` (index)
6. `docs/audit/COMPETITIVE-PARITY-AUDIT-2026-07-01.md` — read §9.7 E-signatures in full, and note the "Structural Advantages" section's PAdES B-T/B-LT/B-LTA callout — do not weaken or regress that cryptographic rigor while adding UX polish
7. `docs/planning/IMPLEMENTATION-PLAN-2026-07-01.md` — read in full for execution mechanics, then re-read §9.7 rows below
8. `CLAUDE.md` at the repo root — build/test commands, ctest baseline, environment constants

Do NOT re-derive findings from scratch.
</mandatory_first_steps>

<task>
Batch-fix GlyphPDF's remaining E-Signatures backlog (Wave 1B + Wave 2A items — excluding the Wave 1A validity-badge/Validate-All fix handled elsewhere) in ONE pass, on ONE branch, in your own git worktree.

### Exact item list for this domain (verbatim from the plan)

**Wave 1B — Correctness fixes (M-effort P0):**
1. **Add typed-font and image-upload modes to the existing signature tool.** Extend the current draw-only "Add Signature" tool into a 3-tab picker (Draw/Type/Upload). Rationale: the most visible gap versus every named competitor's local signing UX.
2. **Build a signature appearance/design step into `SignatureDialog` for the cryptographic signing path.** Let users attach a graphic + configurable text lines to the currently invisible, fixed-Rect signature field. Rationale: today's cryptographic signature field has no appearance stream — effectively invisible, the biggest gap versus Adobe specifically.

**Wave 2A — Quick parity wins (S-effort P1):**
3. **Add an Initials variant of the existing signature tool.** Rationale: every named competitor treats initials as a first-class sibling of the full signature; zero matches exist in the codebase today.
4. **Cache/reuse the adopted signature across placements in a session.** Rationale: competitors cache the adopted signature; GlyphPDF requires a fresh freehand stroke every time.
5. **Surface `SignOutcome` degradation as a user-visible dialog at signing time.** Rationale: exposes information the backend already computes faithfully rather than leaving it enum-only.

Confirm exact current file/class/function names by reading `src/ui/SignatureDialog.{h,cpp}`, `src/ui/SignaturesWidget.{h,cpp}`, `src/modes/SignaturesPanel.{h,cpp}`, `src/engines/SignatureManager.{h,cpp}` (for `SignOutcome` and the appearance-stream logic), and `src/commands/SignDocumentHelper.h` before starting.
</task>

<constraints>
- **Batch, don't fragment.** All 5 items belong to one domain, one branch, one worktree.
- **Branch:** `feature/esignature-parity`, based off `audit-remediation` (or `main` if that no longer exists). Rebase onto the latest integration branch to pick up the Wave 1A validity-badge fix first, since items 1-2 here also touch `SignatureDialog`/`SignaturesWidget`.
- **Non-negotiable:** do not weaken the existing PAdES B-T/B-LT/B-LTA signing correctness, the embedded DSS/OCSP handling, or the signed-document redaction hard-block while adding appearance streams / typed-signature UX. These are explicitly called out as GlyphPDF's structural competitive advantage in the audit — any appearance/UX change must be purely additive to the cryptographic path, never a substitute for it.
- **Incremental commits per logical group** — e.g. one commit for the 3-tab Draw/Type/Upload picker, one for the cryptographic-path appearance/design step, one for the Initials variant, one for signature caching/reuse, one for the `SignOutcome` user-visible dialog. Do not squash.
- **Build clean + full ctest pass** before considering anything done (currently 37-39 targets, 100% green — verify actual count at your start). Use `CLAUDE.md` §3 build/test commands. Pay special attention to `tests/TestSignatureRealCrypto.cpp` and `tests/TestSignatureValidation*.cpp` — these must stay green; extend them, don't weaken them.
- **Add tests for any newly-covered path** — typed/uploaded signature rendering into a valid appearance stream, Initials placement, signature caching across multiple placements in one session, and `SignOutcome` degradation surfacing (assert the dialog fires for each degraded enum value).
- **Do not touch other domains' files if avoidable.** Stay within `src/ui/SignatureDialog*`, `src/ui/SignaturesWidget*`, `src/modes/SignaturesPanel*`, `src/engines/SignatureManager*`, `src/commands/SignDocumentHelper.h`.
- **Do not push or merge.** Leave the branch for human review.
- Match existing C++17/Qt6 code style. No new external dependencies without flagging it in your final report.
</constraints>

<final_report_format>
```
## Domain: E-Signatures (§9.7) — Implementation Report

### Branch
<branch name, base commit hash>

### Items
1. [DONE|SKIPPED|PARTIAL] Typed-font + image-upload signature modes (3-tab picker) — <detail, commit hash>
2. [DONE|SKIPPED|PARTIAL] Signature appearance/design step for crypto signing path — <detail, commit hash>
3. [DONE|SKIPPED|PARTIAL] Initials variant — <detail, commit hash>
4. [DONE|SKIPPED|PARTIAL] Cache/reuse adopted signature across session — <detail, commit hash>
5. [DONE|SKIPPED|PARTIAL] SignOutcome degradation user-visible dialog — <detail, commit hash>

### Build status
<clean / errors — paste final build tail if errors>

### Test status
<N/N ctest targets pass — paste ctest summary line, explicitly confirm TestSignatureRealCrypto and TestSignatureValidation* still pass; note any new tests added by name>

### Deviations / follow-ups
<anything skipped, descoped, or flagged for human review, with rationale>
```
</final_report_format>
