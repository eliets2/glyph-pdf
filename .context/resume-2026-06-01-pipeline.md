# Orchestrator Resume Checkpoint — M5–M8 Pipeline

**Written:** 2026-06-01 · **HEAD at checkpoint:** `af43881` (M6-P3 AI backend) · **Tree:** clean except `docs/planning/MONTHS-2-8-PROMPTS.md` (uncommitted prompt-source rewrite)

## Why this exists
Pipeline launch was blocked by an **account usage limit** (resets **5:20pm Asia/Beirut**). Two background sub-agents (Track A P12, Track B P13) were spawned and **died instantly on the limit — zero work done**. This file preserves the pre-flight + decisions so a post-reset / fresh session resumes in one paste.

## Decisions locked (from user, 2026-06-01)
- **Scope:** drive the whole pipeline ("all the prompts").
- **Decision A (PP-OCRv5 weights):** RESOLVED — *agents acquire/install/wire the weights* (user authorized 2026-06-01). An acquisition agent fetches genuine PP-OCRv5 **mobile** ONNX (det+rec+dict+orientation-cls) into `models/ppocrv5/` and writes `PROVENANCE.md` (replaces the 3 fake 10,000,000-byte dummy files). The ROVER/RapidOCR **code wiring** = PROMPT 4 (M5-P1) + 5 + 6, executed in Track A order AFTER PROMPT 11 (do not race the running PROMPT 12). ROVER merge already exists at `src/engines/ocr/OcrPipeline.cpp` (RoverVote, word-level IoU>0.5); RapidOcrEngine currently loads 3 dummy ONNX → fails → ROVER falls back to Tesseract-only. No rec-dict handling and no models→runtime deploy rule yet (PROMPT 4 closes both).

## Pre-flight verdict
- ✅ Build gate green at `af43881` — the `retrieveKey`→`readKey` break is fixed; AI subsystem committed.
- ✅ PROMPT 12 and PROMPT 13 are full, executable 7-H prompts (verified in `docs/planning/MONTHS-2-8-PROMPTS.md`: P10 @1219, P11 @1292, P12 @1368, P13 @1464).
- ⚠️ `MONTHS-2-8-PROMPTS.md` is uncommitted — consider committing it as the source-of-truth before executing from it.

## Execution model (orchestrator + project rules)
- **Track A is sequential** (one prompt per FRESH session), each gated by a real MSYS2 ucrt64 build + `ctest` before moving on. No `git push`; local per-deliverable commits only. "Don't trust agent-reported success — run ctest."
- **Track B interleaves** (non-engineering, D1+D2 only this round).
- Build: `export PATH="/c/msys64/ucrt64/bin:$PATH"; cd build && cmake .. -G Ninja && cmake --build . --parallel 8; QT_QPA_PLATFORM=offscreen ctest --output-on-failure`

## Runnable-now order (until Decision A clears)
1. **Track A → PROMPT 12** (M6-P4 Djot annotation rich-text) — `MONTHS-2-8-PROMPTS.md` lines 1368–1461. Deps satisfied (PROMPT 2 encode done). 7 deliverables D1–D7.
2. **Track A → PROMPT 10** (M6-P5 Comment threading depth) — lines 1219–1291.
3. **Track A → PROMPT 11** (M6-P6 Edge-case cleanup) — lines 1292–1367.
4. **— Decision A gate —** wait for user's real PP-OCRv5 weights, then PROMPT 4 → 5 → 6 → 15 → 14 → 16 → 17 → 18.
- **Track B → PROMPT 13 D1+D2** (auditor shortlist + auditor kit, new files under `docs/security/`) — lines 1464–1545. Independent; safe to run anytime.

## How to resume after 5:20pm reset
Open a FRESH Claude Code session at `C:\Users\User\Projects\pdf`, read PROMPT 12's PHASE 0 vault notes (6 notes under `C:\Users\User\.claude\memory\projects\glyphpdf\` + `knowledge\agent-execution-anti-patterns.md` + `infrastructure\system-environment.md`), then execute PROMPT 12 D1–D7, build+ctest, local-commit per deliverable, no push. Repeat per prompt.
