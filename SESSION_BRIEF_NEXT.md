# GlyphPDF — Next Claude Code Session Brief

**Updated:** 2026-05-30 | **Branch:** main (merged from feature/m4-djot-foundation in catchup) | **Head:** see `git log -1`
**Project root:** `C:\Users\User\Projects\pdf`
**Branch C SCOPE LOCK:** real public v1.0.0 ships ~8-9 months from M2 start (M8 target). Current `dist\GlyphPDF-1.0.0-x64.msi` is **private/internal only** — do NOT publish until M8.

**Read first:** `docs/planning/NEXT-SESSION-HAND-OFF.md` — single-page summary of current state + which prompt to run next + what's blocked.

---

## What has been built (committed + tested)

| Sprint | Commits | What it contains |
|--------|---------|-----------------|
| Sessions 1-11 | `a5d9ec9`–`9a9ef37` | Initial architecture: DI, ToolRegistry, IPdfBackend, RenderCache, PAdES B-LTA, page ops, forms, OCR pipeline, diff/conversion, batch/compare modes |
| M1 | `a6ea6aa` | B1-B11: autosave+recovery, SignatureManager hardening, content-stream injection fix, RenderCache TOCTOU, encryptWithCertificate, CMake guards, qpdf_set_static_ID removal, RapidOCR runtime gate |
| MSYS2 | `45807de`+`6e7c8aa`+`9ac0c2f` | Qt-installer+vcpkg → MSYS2 ucrt64; PoDoFo 1.1.0 vendor |
| M2 (5/5) | `42c0f46`–`c3eb22a` | Edact-Ray glyph-advance, OCR text-layer scrub, veraPDF subprocess, real-crypto E2E tests, LaneScheduler |
| M3 (5/5) | `faac7f2`–`5bc2fbe` | FormBuilderMode, BatchMode, PagesMode, RedactMode+PatternRedactor, InspectorWidget properties |
| M4 (6/7 + catchup) | `8bb8f95`–`bc00e6a` | View/Pages/Convert/Forms/Security tools; Djot foundation (docmodel + pdfws_djot + Lua + ProvenanceGuard); 8 walkthroughs; test registrations; CHANGELOG fixes |

**Test count:** 23 ctest targets. Run: `ctest --output-on-failure -j4 --repeat-until-fail 3`

---

## What is PENDING

### M4-PROMPT-6 — Edge fixes (NOT YET EXECUTED)
- D1 (Strikeout/Squiggly real ToolModes) and D2 (Share via MAPI) are **verified in-place** from prior sessions.
- D4 (Prune missing recent files) is **NOT implemented**.
- The prompt in `docs/planning/MONTHS-2-8-PROMPTS.md` is **skeletal** (missing session_metadata, role, files_to_read, verification, constraints, error_recovery). Must expand to full 7-H before executing.
- **Recommendation:** skip M4-P6 for now; execute M5-P3 (independent, clean scope) first.

### M5 — OCR ensemble + Office import
| Prompt | Status | Blocker |
|--------|--------|---------|
| M5-P1 RapidOCR real PP-OCRv5 | Not started | `models/ppocrv5/` contains PP-OCRv4 weights (see `models/ppocrv5/STATUS.md`). Download real PP-OCRv5 ONNX OR amend prompt to v4. |
| M5-P2 LayoutEnsemble | Not started | Needs M5-P1 |
| **M5-P3 Office→PDF import** | **READY — next recommended** | Independent; `HAS_LIBREOFFICE` CMake option; `soffice` subprocess; no M5-P1 dependency |
| M5-P4 OCR→Djot mapping | Not started | Blocked on `LuaDjotCodec::documentToDjot` encode stub (`LuaDjotCodec.cpp:54`) |

### WS2 Djot encode path (blocking M5-P4)
`LuaDjotCodec::documentToDjot` returns empty string (stub). Implementing it requires walking the SemanticDocument tree and emitting Djot syntax. This is standalone C++ work (~2-4 hours) that should be done as a dedicated mini-prompt before M5-P4.

---

## Vault memory workflow (read BEFORE starting any prompt)

Before executing any M2-M8 prompt, read these 6 vault notes IN ORDER (saves ~5-10K tokens):

1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 17 cross-project failure patterns
2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` — GlyphPDF specific (includes Pattern 18 tr-shadow + Pattern 19 TestBatchMode race)
3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` — architectural constraints
4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — current commit + test status (update this after any sprint)
5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` — dependency-aware order
6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` — installed tools + MSYS2 packages (don't re-query `pacman -Q`)

**The project CLAUDE.md auto-loads; the 6 vault notes don't.** They live in `C:\Users\User\.claude\memory\projects\glyphpdf\`.

---

## Pre-flight checklist for next session

Before executing any prompt:
- [ ] `git status --short` → clean (no modified, no untracked except gitignored)
- [ ] `git log -1` → expected HEAD
- [ ] `ctest --output-on-failure -j4 --repeat-until-fail 3` → 23/23 pass, 3/3 runs
- [ ] No stale STATE.md from previous context-gated session
- [ ] Disk space: ~5 GB headroom for build
- [ ] For M5-P3: verify `soffice` is accessible (`where soffice` or check LibreOffice install)

---

## How to start the next session

**Recommended next prompt: M5-PROMPT-3 (Office→PDF import + complete OOSS)**

1. Open a fresh Claude Code session at `C:\Users\User\Projects\pdf`.
2. Read PHASE 0 vault notes (listed above).
3. Open `docs/planning/MONTHS-2-8-PROMPTS.md`.
4. Find M5-PROMPT-3 section.
5. Verify it has full 7-H structure (session_metadata, role, files_to_read, deliverables, verification, constraints, error_recovery). If skeletal, expand first in a separate session.
6. Copy the prompt block and paste as first message.
7. After completion: run `ctest --output-on-failure -j4 --repeat-until-fail 3` yourself. Don't trust agent-reported success.
8. Update vault note `01-current-state.md` + `07-sessions-log.md`.

For a full rationale: see `docs/planning/NEXT-SESSION-HAND-OFF.md`.
