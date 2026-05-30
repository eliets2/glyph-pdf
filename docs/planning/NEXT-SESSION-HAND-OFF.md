# Next Session Hand-Off

**Generated:** 2026-05-30 (consolidated catchup session)
**Project:** GlyphPDF v1.0.0 at `C:\Users\User\Projects\pdf`

---

## Current State

| Item | Value |
|------|-------|
| **Branch** | `main` (merged from feature/m4-djot-foundation in this catchup) |
| **HEAD** | see `git log -1` — post-catchup |
| **ctest targets** | 23 (up from 21; +TestPatternRedact +TestDjotRoundtrip) |
| **Working tree** | clean |
| **Sprints done** | M1 ✅ · MSYS2 ✅ · M2 (5/5) ✅ · M3 (5/5) ✅ · M4 (6/7) ✅ |
| **M4-P6** | NOT executed (skeletal prompt; D4 prune-recents not implemented) |
| **M4-P7 encode** | STUBBED (`LuaDjotCodec.cpp:54` TODO; blocks M5-P4) |

---

## Recommended Next Prompt: M5-PROMPT-3 (Office→PDF import + complete OOSS)

**Why M5-P3 first:**

| Candidate | Safe? | Why / Blocker |
|-----------|-------|--------------|
| M4-P6 Edge fixes | ❌ | Prompt skeletal (missing 6 of 7 H sections); D4 prune-recents not done |
| M5-P1 RapidOCR real | ❌ | Wrong models on disk (`PP-OCRv4` in `ppocrv5/`); see `models/ppocrv5/STATUS.md` |
| M5-P2 LayoutEnsemble | ❌ | Needs M5-P1 |
| **M5-P3 Office→PDF import** | **✅** | Independent of M5-P1/P2; only needs `soffice` (LibreOffice) installed |
| M5-P4 OCR→Djot mapping | ❌ | Needs `LuaDjotCodec::documentToDjot` encode (currently stubbed) + M5-P2 |

**What M5-P3 delivers:** `HAS_LIBREOFFICE` CMake option; `convertOfficeToPdf()` via `soffice --headless --convert-to pdf` subprocess; `imagesToPdf()` for rasterized image sequences; complete OOSS (Office Open Subdoc Support) so the Convert→Office path works end-to-end (currently dead code that existed since Antigravity sessions).

---

## Pre-Flight for M5-P3

1. `git status --short` → empty
2. `git log -1` → expected post-catchup HEAD
3. `ctest --output-on-failure -j4 --repeat-until-fail 3` → 23/23 pass, 3/3 runs
4. `where soffice` — check if LibreOffice is installed. If not:
   - Option A: install LibreOffice → `soffice` in PATH → proceed
   - Option B: note that M5-P3 will compile with `HAS_LIBREOFFICE=OFF` (feature disabled) + ship the CMake option; execution of the feature deferred until LibreOffice is present
5. Read PHASE 0 vault notes (see `SESSION_BRIEF_NEXT.md` for the list)
6. Verify M5-P3 section in `docs/planning/MONTHS-2-8-PROMPTS.md` has full 7-H structure

---

## What's Blocked and Why

| Blocked | Blocker | How to Unblock |
|---------|---------|----------------|
| M4-P6 full execution | Prompt skeletal; D4 (prune recents) not implemented | Expand to full 7-H first (separate 30-min session) |
| M5-P1 RapidOCR real PP-OCRv5 | PP-OCRv4 weights in `models/ppocrv5/` | Download PP-OCRv5 ONNX from PaddleOCR releases (see STATUS.md) OR amend prompt to v4 |
| M5-P2 LayoutEnsemble | M5-P1 not done | Do M5-P1 first |
| M5-P4 OCR→Djot mapping | `LuaDjotCodec::documentToDjot` encode stub + M5-P2 | Implement encode (standalone ~2-4h work) → then M5-P1 → M5-P2 → M5-P4 |
| M6-P4 Djot annotation rich text | M4-P7 encode + M3-P5 (✅) | Same as M5-P4 encode blocker |

---

## Cross-Links

- `SESSION_BRIEF_NEXT.md` — full session brief with pre-flight + vault workflow
- `CHANGELOG.md` Known Issues section — M4-P6 D4 + PP-OCRv5 model mismatch + encode stub documented
- `models/ppocrv5/STATUS.md` — PP-OCRv5 model situation + download instructions
- `docs/planning/MONTHS-2-8-PROMPTS.md` — M5-P3 section (verify 7-H before executing)
- `docs/planning/walkthroughs/` — 11 walkthroughs now (M3-P1 through M3-P5, M4-P1 through M4-P5, M4-P7)
- Vault `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — commit-by-commit map (update after M5-P3)
