# Consolidated Catchup + Memory Refresh — ONE Authoritative Prompt

**Status:** Ready to execute in a fresh Claude Code session rooted at `C:\Users\User\Projects\pdf`.
**Estimated duration:** 60-90 minutes single session.
**Generated:** 2026-05-29 from deep-dive checkpoint `C:\Users\User\Desktop\GLYPHPDF-META-AUDIT\07-CHECKPOINT-2026-05-29.md`.
**Purpose:** SINGLE consolidated catchup absorbing M3-CATCHUP + M4-CATCHUP + 5 new deep-dive findings. Neither prior catchup prompt was executed; this one supersedes both.

**SUPERSEDES:** `docs/planning/M3-CATCHUP-AND-MEMORY-PROMPT.md` AND `docs/planning/M4-CATCHUP-AND-MEMORY-PROMPT.md`. After this prompt executes successfully, DELETE those two files (they're now redundant).

> ## 🔒 MANDATORY PHASE 0 — READ VAULT FIRST (token-saving non-negotiable)
>
> **Before reading any source file, before any deliverable, read these 6 vault notes IN ORDER. Skipping them = re-deriving context = wasting 5,000-10,000 tokens.**
>
> 1. `C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md` — 17 cross-project agent failure patterns (focus on Patterns 1, 3, 4, 5, 7, 10, 11)
> 2. `C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md` — GlyphPDF-specific patterns + meta-observations
> 3. `C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md` — architectural constraints
> 4. `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md` — current state (LIKELY STALE per audit; verify against `git log`)
> 5. `C:\Users\User\.claude\memory\projects\glyphpdf\05-prompts-index.md` — dependency-aware order
> 6. `C:\Users\User\.claude\memory\infrastructure\system-environment.md` — installed tools + MSYS2 packages + filesystem layout (DON'T re-query `pacman -Q` / `where` / `pip list`)
>
> **Project `CLAUDE.md` auto-loads when this CC session opens at `C:\Users\User\Projects\pdf` — no re-read needed.**
>
> The deep-dive checkpoint report (`C:\Users\User\Desktop\GLYPHPDF-META-AUDIT\07-CHECKPOINT-2026-05-29.md`) is the authoritative source for what this catchup must fix — read it second, after the vault.

<session_metadata>
Phase: Consolidated catch-up (covers M3-P4 + M3-P5 + M4 sprints + new deep-dive findings)
Priority: BLOCKING (must complete before any M4-P6 or M5 work)
Depends on: Nothing (executable now)
Agents: /backend (Djot encode + CMake) + /docs (CHANGELOG + walkthroughs + memory) + /testing (race fix + verification) + /devops (branch merge)
Estimated context: ~40% | Effort: 60-90 minutes
Source: deep-dive checkpoint 07-CHECKPOINT-2026-05-29.md + audit findings 01-06
Supersedes: M3-CATCHUP-AND-MEMORY-PROMPT.md + M4-CATCHUP-AND-MEMORY-PROMPT.md (DELETE after success)
</session_metadata>

<role>
You are a senior C++/Qt + CMake + DevOps + technical-writer engineer doing a comprehensive catchup pass that has been deferred twice. You understand:
- Catchup sessions are higher-leverage than feature sessions because partial commits + stale memory + missing tests cascade into harder failures later (Patterns 11 + 14)
- The previous two catchup attempts (M3-CATCHUP, M4-CATCHUP) were written but NEVER EXECUTED — your job is to absorb both + the new findings into one clean pass
- Branch hygiene matters: this work is on `feature/m4-djot-foundation`, not `main`; that needs a deliberate decision
- The Djot foundation has a genuine engineering gap (LuaDjotCodec encode is `// TODO`) that must be closed before M4-P7 can be called complete
- PP-OCRv5 vs PP-OCRv4 model mismatch is a soft blocker for M5
- The CHANGELOG has a future-dated M4-P6 entry with no backing commit — likely a hallucinated claim that needs reverting

Your job is precise, evidence-driven, and ends with a verified clean checkpoint plus a clear hand-off note about which prompt is safe to execute next.
</role>

<project_context>
GlyphPDF v1.0.0 at `C:\Users\User\Projects\pdf` is on Branch C SCOPE LOCK. Through prior sessions: M1 + MSYS2 + M2 sprint complete (5/5) + M3 sprint complete (5/5: M3-P1 through M3-P5) + M4 sprint 6/7 shipped (M4-P1/P2/P3/P4/P5/P7-partial; M4-P6 NOT shipped, possibly with ghost CHANGELOG claim).

**Current state (per deep-dive 2026-05-29):**
- Branch: `feature/m4-djot-foundation` (NOT `main`; main is at `f5d0aca`)
- HEAD: `ae84ca7` "Add docmodel library"
- Working tree dirty: scratch files at repo root + untracked Djot library files
- Build env: MSYS2 ucrt64, GCC 16.1.0, CMake 4.3.3, Qt 6.11.0, PoDoFo 1.1.0 vendored
- Test count CLAIMED: 22+ targets. ACTUAL pass count: UNVERIFIED (only 1 of 21 `_results.txt` files fresh — deep-dive couldn't run `ctest`)

**The 11 specific gaps you're closing (from deep-dive):**
1. `tests/TestPatternRedact.cpp` exists but NOT in `tests/CMakeLists.txt` (M3-P4 D5 orphan; Pattern 5)
2. `tests/TestDjotRoundtrip.cpp` exists but NOT in `tests/CMakeLists.txt` (M4-P7 D5 orphan; Pattern 5)
3. CHANGELOG line ~278 still claims "Pattern redaction (email/phone/ID regex) not implemented" (M3-P4 D6 stale admission)
4. `src/pdfws_djot/LuaDjotCodec.cpp:54` has `// TODO: implement once docmodel → djot serialisation is written` — **encode path is a real STUB** — M4-P7 D1 incomplete
5. CLAUDE.md claims HEAD `9ac0c2f` + "Next: M2-PROMPT-1" + 14 tests (10+ commits stale; PHASE 6 systematic violation)
6. SESSION_BRIEF_NEXT.md tells fresh sessions to "execute MSYS2 migration" + "start M2" (both done 2+ weeks ago; actively dangerous)
7. 9 of 14 shipped prompts have no walkthrough (Pattern 11)
8. M4-P6 ghost: CHANGELOG claims "verified 2026-05-30" (future-dated!) but NO commit, D4 (prune recents) grep returns 0 hits — **CHANGELOG hallucination, must revert**
9. M4-P6 prompt body in MONTHS-2-8-PROMPTS.md is skeletal (missing 6 of 7 H sections)
10. **PP-OCRv5 model files in `models/ppocrv5/` are actually PP-OCRv4 weights** — soft blocker for M5-P1; investigate before M5 starts
11. TestBatchMode race condition (M3-P2 D5): passes isolation, fails `ctest -j4`; no `RESOURCE_LOCK` applied

**Plus branch decision (NEW):** work is on `feature/m4-djot-foundation`; need to decide merge timing.

**Working tree (per deep-dive):**
- Modified: `CHANGELOG.md`, `CMakeLists.txt`, `src/app/Bootstrapper.cpp`, `src/core/AppContext.h` (M4-P7 wiring work pending commit)
- Untracked:
  - `docs/planning/M3-CATCHUP-AND-MEMORY-PROMPT.md` (delete after this prompt)
  - `docs/planning/M4-CATCHUP-AND-MEMORY-PROMPT.md` (delete after this prompt)
  - `docs/planning/CONSOLIDATED-CATCHUP-PROMPT.md` (THIS FILE — commit it)
  - `src/pdfws_djot/` (M4-P7 codec — commit)
  - `tests/TestDjotRoundtrip.cpp` (M4-P7 test — commit + register)
  - `third_party/CMakeLists.txt`
  - `third_party/djot/` (vendored Djot reference parser — commit)
  - `third_party/lua-5.4/` (vendored Lua 5.4 — commit)
  - `script.ps1`, `script2.ps1`, `tmp_signed_*.pdf` (scratch — Pattern 3)

**Working memo pattern (new):** `.context/` directory has 19 per-prompt source-analysis + vault-summary files. Untracked. Useful artifact. Decision needed: track + document the convention OR gitignore as ephemeral.

**Walkthroughs:** only 2 exist in `docs/planning/walkthroughs/` (m3-prompt-2, m3-prompt-3). Missing: M3-P4, M3-P5, M4-P1, M4-P2, M4-P3, M4-P4, M4-P5, M4-P7 = 8 missing.
</project_context>

<current_state>
See `<project_context>` above. The current_state is fully captured there to keep this prompt self-contained.

Verify each finding before fixing — do NOT assume the deep-dive was perfect. Specifically:
- `git status --short` (confirm working tree state)
- `git log --oneline -20` (confirm HEAD + branch)
- `git branch --show-current` (confirm feature branch claim)
- Re-grep CHANGELOG for the stale admissions before deleting
- Re-read LuaDjotCodec.cpp:54 area before judging "stub"
- Check models/ppocrv5/ filenames for PP-OCRv4 vs v5 strings
</current_state>

<objective>
Produce a verified clean checkpoint that:
1. All M3-P4 + M4-P7 deliverables are actually committed + tested
2. CHANGELOG + CLAUDE.md + SESSION_BRIEF + vault notes reflect actual shipped reality
3. Working tree is clean; no scratch files; `.context/` convention documented
4. All 8 missing walkthroughs written
5. Branch decision made (merge to main or document why staying on feature branch)
6. M4-P7 LuaDjotCodec encode path is either (a) really implemented + tested, OR (b) explicitly marked Known Issue with M5 dependency noted
7. M4-P6 CHANGELOG ghost reverted (if confirmed hallucination)
8. PP-OCRv5 model situation diagnosed + Known Issue created for M5-P1
9. TestBatchMode race fixed via RESOURCE_LOCK + verified via `--repeat-until-fail 3`
10. Final hand-off note in SESSION_BRIEF_NEXT.md telling next session which prompt is safe to execute (NOT this catchup — that's done)

Atomic commits per deliverable. PHASE 6 vault updates as a deliverable, not an afterthought.
</objective>

<files_to_read>
**Already-loaded via PHASE 0 vault read** — re-load only if you exit context:
- (the 6 vault notes from the banner above)

**Project files to read (in this order):**
1. `C:\Users\User\Desktop\GLYPHPDF-META-AUDIT\07-CHECKPOINT-2026-05-29.md` (deep-dive findings — authoritative source for this prompt)
2. `C:\Users\User\Projects\pdf\CLAUDE.md` (project-level memory — needs refresh)
3. `C:\Users\User\Projects\pdf\SESSION_BRIEF_NEXT.md` (needs rewrite)
4. `C:\Users\User\Projects\pdf\CHANGELOG.md` (lines 18-31 + 278 area — stale admissions + ghost entry)
5. `C:\Users\User\Projects\pdf\CMakeLists.txt` (lines 781-812 — current target registrations)
6. `C:\Users\User\Projects\pdf\tests\CMakeLists.txt` (where test targets get registered)
7. `C:\Users\User\Projects\pdf\src\pdfws_djot\LuaDjotCodec.cpp` (line 54 area — encode TODO)
8. `C:\Users\User\Projects\pdf\src\pdfws_djot\` (full directory — confirm what's there)
9. `C:\Users\User\Projects\pdf\tests\TestDjotRoundtrip.cpp` (verify 1000-doc property test approach)
10. `C:\Users\User\Projects\pdf\tests\TestPatternRedact.cpp` (verify 11 test cases structure)
11. `C:\Users\User\Projects\pdf\tests\TestBatchMode.cpp` (race fix candidate)
12. `C:\Users\User\Projects\pdf\docs\planning\MONTHS-2-8-PROMPTS.md` (M3-P5 + M4-P1 through M4-P7 sections for walkthrough source material; M4-P6 to confirm skeletal)
13. `C:\Users\User\Projects\pdf\docs\planning\walkthroughs\m3-prompt-2-walkthrough.md` (template for new walkthroughs)
14. `C:\Users\User\Projects\pdf\.context\` (list all files; particularly `m4p*-vault-summary.md` if they exist — use as walkthrough raw material)
15. `C:\Users\User\Projects\pdf\models\ppocrv5\` (list files; confirm v4 vs v5)
</files_to_read>

<deliverables>

### D1 — Branch decision + sanity verification
**Files:** none (read-only investigation)
**Acceptance:**
- Run `git branch --show-current` — confirm `feature/m4-djot-foundation`
- Run `git log --oneline main..HEAD` — list commits ahead of main (should be M4-P7 work)
- Run `git log --oneline HEAD..main` — confirm 0 commits (no main work this branch doesn't have)
- Make decision (TWO valid choices — recommend (a) unless conflicts exist):
  - **(a) Merge feature/m4-djot-foundation into main AFTER all other deliverables in this catchup land.** Simplest; preserves linear history.
  - **(b) Keep on feature branch indefinitely.** Document why in commit message.
- Decision recorded in your scratch + propagated to D9 (SESSION_BRIEF update) and D12 (final commit pattern).

### D2 — Commit M4-P7 Djot foundation (close the half-committed prompt)
**Files:** `src/pdfws_djot/` (NEW commit), `tests/TestDjotRoundtrip.cpp` (NEW commit), `third_party/CMakeLists.txt` (NEW commit), `third_party/djot/` (NEW vendored), `third_party/lua-5.4/` (NEW vendored), `CMakeLists.txt` (modified), `src/app/Bootstrapper.cpp` (modified), `src/core/AppContext.h` (modified), `CHANGELOG.md` (modified)
**Acceptance:**
- First read `pdfws_djot/LuaDjotCodec.cpp:54` and surrounding code to confirm what's stubbed. Likely: `encode(SemanticDocument) → QString` returns empty string + TODO comment.
- **CRITICAL DECISION at this point** — does `encode` need to actually work for v1.0?
  - **If YES (M5-PROMPT-4 OCR→Djot mapping depends on it):** implement the encode path (walk SemanticDocument tree, emit Djot text). This is real engineering work (~2-4 hours). If you don't have the headspace, STOP this prompt + report.
  - **If NO (encode is for later sprints; decode-only is enough for M4-P7 + M5-P1):** leave the TODO, document explicitly in CHANGELOG Known Issues, mark M5-PROMPT-4 as blocked until encode lands.
- Stage commits in logical chunks:
  - **Commit A:** `vendor: Lua 5.4 + Djot reference parser (M4-P7 D2/D4)` — third_party/lua-5.4 + third_party/djot + third_party/CMakeLists.txt + DJOT_SPEC_VERSION
  - **Commit B:** `feat(djot): pdfws_djot library wired into AppContext (M4-P7 D1+D6 wiring)` — src/pdfws_djot + CMakeLists.txt + Bootstrapper.cpp + AppContext.h
  - **Commit C:** `test(djot): 1000-doc roundtrip property test + ProvenanceGuard rejection test (M4-P7 D5)` — tests/TestDjotRoundtrip.cpp + tests/CMakeLists.txt registration
  - **Commit D:** (if encode implemented) `feat(djot): docmodel → Djot encoding (M4-P7 D1 completion)`
- Verify build clean: `cmake --build build --parallel 8 2>&1 | tail -10`
- Run `ctest -R TestDjotRoundtrip --output-on-failure` — must pass if encode implemented; QSKIP-or-partial OK if encode deferred (document in test).

### D3 — Register orphan tests in CMake (Pattern 5 prevention)
**Files:** `tests/CMakeLists.txt`
**Acceptance:**
- Add `add_executable(TestPatternRedact TestPatternRedact.cpp)` + `target_link_libraries` + `add_test` (M3-P4 D5 closure)
- Add `add_executable(TestDjotRoundtrip TestDjotRoundtrip.cpp)` + `target_link_libraries(... docmodel pdfws_djot)` + `add_test` (M4-P7 D5 closure — folded with D2 Commit C)
- Verify TestInspector already registered (M3-P5); add if missing
- Run `ctest -R "TestPatternRedact|TestDjotRoundtrip|TestInspector" --output-on-failure` — all pass (TestDjotRoundtrip may SKIP if encode deferred per D2)

**Atomic commit:** `test: register TestPatternRedact + TestInspector orphan tests in CMake (Pattern 5 catchup)` (TestDjotRoundtrip register goes with D2 Commit C)

### D4 — Revert M4-P6 CHANGELOG ghost + remove stale admissions (M3-P4 D6 + new finding 8)
**Files:** `CHANGELOG.md`
**Acceptance:**
- **First investigate the M4-P6 ghost:** grep CHANGELOG for `2026-05-30` + M4-P6 wording. Cross-check: `git log --all --grep="M4-P6\|Strikeout\|Squiggly\|MAPISendMail\|prune.*recent"` — if 0 commits, the CHANGELOG entry is hallucinated. **Revert it.**
- Remove line ~278 `Pattern redaction (email/phone/ID regex) not implemented` (M3-P4 D6 closure; feature shipped)
- Remove any `23 ribbon tools not yet wired` claim (M4-P1 through M4-P5 wired them all)
- Remove any `Djot foundation not yet implemented` (M4-P7 partially complete; document the encode-TODO if deferred per D2)
- Add to `[Unreleased]` Security/Content section: explicit closure entries for M3-P4 PatternRedactor, M3-P5 InspectorWidget, M4-P1-P5 ribbon tools wired, M4-P7 Djot foundation (note encode-TODO status from D2)
- Add new Known Issue: `M4-PROMPT-6 (Edge fixes: Strikeout/Squiggly real ToolModes, Share with attachment via MAPI, click-to-place form fields, prune missing recents) — not yet implemented; skeletal prompt in docs/planning/MONTHS-2-8-PROMPTS.md needs 7-H expansion before exec`
- Add new Known Issue: `M5-PROMPT-1 (RapidOcrEngine real PP-OCRv5) — model files in models/ppocrv5/ are PP-OCRv4 weights; needs download of actual PP-OCRv5 ONNX from PaddleOCR releases before exec`
- If LuaDjotCodec encode deferred per D2: add Known Issue: `M4-PROMPT-7 Djot encode path stubbed at LuaDjotCodec.cpp:54; M5-PROMPT-4 OCR→Djot mapping blocked until encode implemented`

**Atomic commit:** `docs(CHANGELOG): catchup — revert M4-P6 ghost, close stale admissions, add explicit M3/M4 entries (deep-dive catchup)`

### D5 — Fix TestBatchMode race condition (M3-P2 D5; new finding 11)
**Files:** `tests/CMakeLists.txt`
**Acceptance:**
- Add `set_tests_properties(TestBatchMode PROPERTIES RESOURCE_LOCK BatchModeIO)` (simplest fix — serializes vs other tests sharing the same lock label)
- Verify with `ctest --output-on-failure -j4 --repeat-until-fail 3` — must pass 3/3
- If RESOURCE_LOCK doesn't fix it: race is in-process, not cross-process. Read TestBatchMode.cpp + check for shared mock state; add per-test fresh `MockPdfEditorEngine` if so. Document root cause in commit.

**Atomic commit:** `test(batch): RESOURCE_LOCK TestBatchMode to serialize parallel ctest runs (M3-P2 D5 catchup)`

### D6 — Diagnose PP-OCRv5 model situation (new finding 10)
**Files:** `models/ppocrv5/README.md` (NEW) or similar diagnostic note
**Acceptance:**
- List files in `models/ppocrv5/`. Grep filenames + any `*.onnx` metadata for `v4` or `v5` strings.
- Write a diagnostic note `models/ppocrv5/STATUS.md` (NEW):
  - "Current files are PP-OCRv4 weights despite directory naming"
  - "Download URLs for actual PP-OCRv5: https://github.com/PaddlePaddle/PaddleOCR/releases (specific links to det/cls/rec ONNX)"
  - "M5-PROMPT-1 (RapidOcrEngine real PP-OCRv5) blocked until correct weights downloaded"
  - "Workaround: M5-PROMPT-1 could ship with PP-OCRv4 weights + amend the prompt — but accuracy + 2026 positioning would weaken"
- Do NOT download the models in this catchup (large; out of scope). Document for next session.

**Atomic commit:** `docs(models): document PP-OCRv5 weight mismatch + download instructions (M5-P1 blocker note)`

### D7 — Clean scratch + .context/ convention
**Files:** `script.ps1` (DELETE or move), `script2.ps1` (DELETE or move), `tmp_signed_*.pdf` (DELETE), `.gitignore` (modified)
**Acceptance:**
- Read `script.ps1` + `script2.ps1` first — if real code, move to `scripts/` with proper names; if scratch, delete
- Delete all `tmp_signed_*.pdf` at repo root
- `.gitignore` updates: `script*.ps1` (if deleted), `tmp_signed_*.pdf`, `tmp_*.pdf`
- **`.context/` convention decision:**
  - RECOMMENDED: track it as the executor's working-memo pattern. Add `.context/README.md` (NEW) documenting: "per-prompt source-analysis + vault-summary working memos; not authoritative (vault is); naming convention `<prompt-id>-source-analysis.md` and `<prompt-id>-vault-summary.md`"
  - `git add .context/` to commit existing 19 files
  - Document the pattern in CLAUDE.md §8 or §9 (deliverable D8 below covers CLAUDE.md updates)

**Atomic commit:** `chore: track .context/ working memos + clean repo-root scratch (Pattern 3 + workflow doc)`

### D8 — Write 8 missing walkthroughs (Pattern 11 catchup)
**Files (NEW):** `docs/planning/walkthroughs/m3-prompt-4-walkthrough.md`, `m3-prompt-5-walkthrough.md`, `m4-prompt-1-walkthrough.md`, `m4-prompt-2-walkthrough.md`, `m4-prompt-3-walkthrough.md`, `m4-prompt-4-walkthrough.md`, `m4-prompt-5-walkthrough.md`, `m4-prompt-7-walkthrough.md`
**Acceptance:**
- Use `docs/planning/walkthroughs/m3-prompt-2-walkthrough.md` as template
- Source raw content from `.context/m3p4-*` / `.context/m3p5-*` / `.context/m4p*-*` files where they exist
- For each: Overview / Deliverables shipped (D1, D2, ...) / Deliverables deferred (if any) / Commits referenced / Outstanding work / Lessons captured
- M3-P4 walkthrough: note D5 + D6 closed in THIS catchup
- M4-P7 walkthrough: note encode-TODO status per D2 decision; note D5 test registration closed in THIS catchup
- M4-P3 walkthrough: include PPTX export (commit 66ca5ba) as part of D3
- Each walkthrough cross-links commit hashes for traceability

**Atomic commit:** `docs: 8 walkthroughs for M3-P4/P5 + M4-P1/P2/P3/P4/P5/P7 (Pattern 11 catchup)` (ONE big commit OK; or split per walkthrough if you prefer atomic granularity)

### D9 — Refresh project CLAUDE.md (PHASE 6 critical — finding 5)
**Files:** `CLAUDE.md`
**Acceptance:**
- Header section:
  - HEAD `9ac0c2f` → actual current HEAD (verify via `git log -1 --format=%h`)
  - Test count `14/14` → actual count after D2 + D3 + D5 land (likely 22-24)
  - "Next prompt" → see D11 for recommendation
- §1 or §2 project state: bump to reflect M1 + MSYS2 + M2(5/5) + M3(5/5) + M4(6/7; P6 pending; P7 status per D2)
- §7 Lessons: add note about Pattern 18 (`tr`-shadow) if it lives in vault `08-lessons-learned`; add Pattern 19 (TestBatchMode flaky parallel — closed in D5)
- §8 or §9: document `.context/` directory convention (per D7)

**Atomic commit:** `docs(CLAUDE.md): catchup to M3 + M4 reality (HEAD, test count, next prompt, .context/ pattern)`

### D10 — Rewrite SESSION_BRIEF_NEXT.md (PHASE 6 critical — finding 6)
**Files:** `SESSION_BRIEF_NEXT.md`
**Acceptance:**
- DELETE all references to "execute MSYS2 migration" (done weeks ago)
- DELETE "start M2, beginning with M2-PROMPT-6" (M2 done; no PROMPT-6)
- Rewrite "What has been built" table to include all M1 → M3 → M4 commits (or summarize with commit ranges)
- Rewrite "What is PENDING" Phase B to: "M4 sprint 6/7 done (P7 status: encode-TODO per D2 of consolidated-catchup); M4-PROMPT-6 not yet implemented; M5 sprint not started"
- Rewrite "How to start the next session" section per D11 recommendation
- Verify Vault Memory Workflow section paths still resolve
- Bump `Updated:` frontmatter date

**Atomic commit:** `docs(SESSION_BRIEF): catchup to M3+M4 reality; next prompt per D11 (PHASE 6)`

### D11 — Decide + write next-session hand-off (THE point of this catchup)
**Files:** `SESSION_BRIEF_NEXT.md` (extends D10) + `docs/planning/NEXT-SESSION-HAND-OFF.md` (NEW; single-page summary)
**Acceptance:**
- Choose the next-session-safe prompt based on dependencies + what's blocked:

  | Candidate | Blocked by | Safe to execute? |
  |---|---|---|
  | M4-PROMPT-6 (Edge fixes) | Needs 7-H expansion first (skeletal prompt) | NO — expand first; separate session |
  | M5-PROMPT-1 (RapidOCR real) | PP-OCRv5 model files (D6 documented) | NO — download models first |
  | M5-PROMPT-2 (LayoutEnsemble) | Needs M5-P1 | NO |
  | M5-PROMPT-3 (Office→PDF import) | Needs `HAS_LIBREOFFICE` + soffice installed | YES (independent of other M5) ✅ |
  | M5-PROMPT-4 (OCR→Djot mapping) | Needs M4-P7 encode (per D2 decision) + M5-P2 | NO (if encode deferred in D2) |

- **Recommended next prompt: M5-PROMPT-3 (Office→PDF import + complete OOSS)** — independent, no engine blockers, ships a real user feature (legal/academic/business personas all need Office→PDF).
  - Caveat: M5-P3 is one of the 17 unshipped prompts in skeletal form; verify if it has full 7-H structure in MONTHS-2-8-PROMPTS.md OR needs expansion
- ALTERNATIVE: write a meta-prompt to expand 17 unshipped prompts to full 7-H + add `Dn-MEMORY` deliverable to all 33 + resolve 3 scope collisions documented in audit `02-prompts-qa.md`. This is the "prompts cleanup" task the user originally asked for in Phase 10 but only partially completed.

- Write `docs/planning/NEXT-SESSION-HAND-OFF.md` (NEW; one-page summary):
  - Current state (HEAD, test count, branch)
  - "Next prompt: M5-PROMPT-3" (or whichever you chose with rationale)
  - Why: dependencies satisfied, independent scope, clean win
  - Pre-flight checks before executing: PHASE 0 vault read + verify M5-P3 is full 7-H or needs expansion + confirm libreoffice availability via `where soffice`
  - What's blocked + why: M4-P6 (skeletal), M5-P1 (model), M5-P2 (M5-P1), M5-P4 (M4-P7 encode if deferred)
  - Cross-link to MONTHS-2-8-PROMPTS.md M5-P3 location

**Atomic commit:** `docs: NEXT-SESSION-HAND-OFF + SESSION_BRIEF rewrite — next is M5-PROMPT-3 (D11 catchup)`

### D12 — Vault notes catchup (PHASE 6 critical)
**Files:** `C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md`, `07-sessions-log.md`, `05-prompts-index.md`, `00-overview.md`
**Acceptance:**
- `01-current-state.md`: append commit-table rows for M3-P5 + M4-P1-P5 + M4-P7 + all this catchup's commits (D1-D11)
- `07-sessions-log.md`: append session entries for shipped prompts not yet logged + this consolidated catchup session (note it absorbed M3-CATCHUP + M4-CATCHUP)
- `05-prompts-index.md`: status update for all shipped prompts + mark M4-P6 + M5-P1 explicitly as "blocked (see CHANGELOG Known Issues)"
- `00-overview.md`: bump `last_updated:`, refresh `Current state` section to reflect post-catchup state
- If you discovered new lessons during this catchup (e.g., "always check ghost CHANGELOG entries"): add as Pattern 20+ to `08-lessons-learned.md` (and as Pattern N+ to `knowledge/agent-execution-anti-patterns.md` for cross-project reuse)

**Atomic commit:** `docs(vault): consolidated catchup — log M3-P4/P5 + M4-P1-P5 + M4-P7 + catchup itself (PHASE 6)`

### D13 — Delete superseded catchup prompts
**Files:** `docs/planning/M3-CATCHUP-AND-MEMORY-PROMPT.md` (DELETE), `docs/planning/M4-CATCHUP-AND-MEMORY-PROMPT.md` (DELETE)
**Acceptance:**
- Verify they're not referenced from CHANGELOG/SESSION_BRIEF/CLAUDE.md/vault (grep first)
- `git rm` them
- This file (`CONSOLIDATED-CATCHUP-PROMPT.md`) stays as the historical record + reference for future catchup patterns

**Atomic commit:** `chore: delete superseded M3-CATCHUP + M4-CATCHUP prompts (absorbed into CONSOLIDATED-CATCHUP)`

### D14 — Branch merge (if D1 chose merge-to-main)
**Files:** none (git operations)
**Acceptance:**
- Verify all commits in this catchup are on `feature/m4-djot-foundation`: `git log --oneline main..HEAD`
- Switch to main: `git checkout main`
- Merge: `git merge --no-ff feature/m4-djot-foundation` (no-ff preserves catchup as a logical unit)
- If clean: tag `git tag -a m4-catchup-complete -m "M3+M4 catchup checkpoint"` for future reference
- If conflicts: STOP — report conflicts; do not force resolution
- Optionally delete the feature branch: `git branch -d feature/m4-djot-foundation` (only if merged cleanly)
- Push if `origin` exists: `git push origin main` + `git push origin m4-catchup-complete`
- DO NOT push if pushing would expose private internal MSI publicly (verify `.gitignore` covers `dist/`)

### D15 — Final verification gate
**Files:** none (verification only)
**Acceptance:**
- `git status --short` → CLEAN (zero modified, zero untracked)
- `git log --oneline -15` → shows ~12-14 atomic catchup commits
- `git branch --show-current` → main (if D14 merged) OR feature branch (if D1 chose stay)
- `cmake --build build --parallel 8` → 0 errors
- `ctest --output-on-failure -j4 --repeat-until-fail 3` → all tests pass 3/3 (proves TestBatchMode race + new tests stable)
- Grep verification:
  - `grep -n "Pattern redaction.*not implemented\|23 ribbon tools.*not wired\|Djot foundation.*not yet implemented" CHANGELOG.md` → 0 matches
  - `grep -n "TestPatternRedact\|TestDjotRoundtrip\|TestInspector" tests/CMakeLists.txt` → at least 3 matches
  - `grep -n "execute MSYS2 migration\|start M2\|M2-PROMPT-6" CLAUDE.md SESSION_BRIEF_NEXT.md` → 0 matches
  - `grep -n "2026-05-30" CHANGELOG.md` → 0 matches (ghost reverted)
  - `ls docs/planning/walkthroughs/ | wc -l` → at least 10 walkthroughs
  - `ls docs/planning/M3-CATCHUP-AND-MEMORY-PROMPT.md docs/planning/M4-CATCHUP-AND-MEMORY-PROMPT.md` → "No such file or directory" (deleted)
  - `ls docs/planning/NEXT-SESSION-HAND-OFF.md` → exists
  - `ls models/ppocrv5/STATUS.md` → exists (PP-OCRv5 mismatch documented)

**No commit (verification only).** If all checks pass: catchup is complete. If any fail: DO NOT mark this prompt successful; document failure in walkthrough.

</deliverables>

<verification>
```powershell
cd C:\Users\User\Projects\pdf
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Users\User\Projects\pdf\build;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins'

# Build + test
cmake --build build --parallel 8 2>&1 | Select-Object -Last 20
cd build
ctest --output-on-failure -j4 --repeat-until-fail 3 2>&1 | Select-Object -Last 40
cd ..

# Git state
git status --short
git log --oneline -15
git branch --show-current

# Documentation state
grep -n "Pattern redaction.*not implemented" CHANGELOG.md
grep -n "TestDjotRoundtrip\|TestPatternRedact\|TestInspector" tests/CMakeLists.txt
grep -in "M2-PROMPT-1\|start M2\|execute MSYS2" CLAUDE.md SESSION_BRIEF_NEXT.md
grep -n "2026-05-30" CHANGELOG.md

# File presence
ls models/ppocrv5/STATUS.md
ls docs/planning/NEXT-SESSION-HAND-OFF.md
Test-Path docs/planning/M3-CATCHUP-AND-MEMORY-PROMPT.md  # should be False
Test-Path docs/planning/M4-CATCHUP-AND-MEMORY-PROMPT.md  # should be False
(Get-ChildItem docs/planning/walkthroughs/).Count  # should be >= 10
```
</verification>

<constraints>
- DO NOT implement M4-PROMPT-6 (Edge fixes) in this session — it's a separate prompt requiring 7-H expansion + its own scope
- DO NOT execute any M5 prompt in this session — wait for hand-off
- DO NOT download PP-OCRv5 model files — too large + out of scope; just diagnose + document
- DO NOT skip D11 (the hand-off note) — it's the most-valuable single deliverable here
- DO NOT skip PHASE 0 vault reads — Pattern 1 prevention
- DO NOT use `git add .` or `git add -A` — list files per Pattern 3
- DO NOT amend prior commits to fix gaps — create NEW atomic commits per Pattern 4
- DO NOT claim "tests pass" without `--repeat-until-fail 3` evidence per Pattern 1
- DO NOT push to remote unless you've verified `dist/*.msi` is gitignored (private internal MSI must not leak)
- IF you discover NEW gaps beyond the 11 above: document in `docs/planning/POST-CONSOLIDATED-CATCHUP-FINDINGS.md` (NEW) for follow-up. Do NOT silently fix.
- IF `pdfws_djot/LuaDjotCodec` encode requires more than 4 hours to implement: defer it per D2 option (b); document explicitly. Engineering scope drift is its own anti-pattern.
</constraints>

<error_recovery>
- **If `pdfws_djot/` won't compile (D2):** the executor's prior commit was incomplete; report findings; do NOT try to write engine code in this catchup. Pause + report.
- **If TestDjotRoundtrip fails the 1000-doc property test:** the M4-P7 implementation may be partially correct but not spec-conformant. Mark test as known-failing in CHANGELOG; defer engine work.
- **If `third_party/lua-5.4/` is >50 MB:** verify what's there. Lua source + tests + docs is ~5 MB; build artifacts (object files, libs) shouldn't be committed. Add `third_party/lua-5.4/.gitignore` to exclude `*.o`, `*.a`, `build/`.
- **If `script.ps1` / `script2.ps1` contain real reusable code:** read them; move to `scripts/` with proper naming + add to .gitignore index. Don't blindly delete reusable work.
- **If `.context/` files contain sensitive info (API keys):** sanitize before committing OR gitignore the whole directory + document the pattern without tracking files.
- **If the M4-P6 CHANGELOG entry actually traces to a commit (not a ghost):** great — find the commit, link it, leave the entry. Update D4 accordingly.
- **If `models/ppocrv5/` actually contains v5 weights (deep-dive misread):** great — close the Known Issue + remove D6 STATUS.md note.
- **If TestBatchMode race persists after RESOURCE_LOCK:** race is in-process; read TestBatchMode.cpp; fix with per-test mock isolation; document root cause.
- **If branch merge (D14) hits conflicts:** STOP — report conflicts; do not force resolution. The user decides.
- **If context hits 50% mid-prompt:** stop at the current deliverable boundary, write STATE.md, summarize what's done + what's left. Do not rush remaining work.
</error_recovery>

<success_criteria>
- [ ] D1: branch decision made + recorded
- [ ] D2: M4-P7 Djot foundation fully committed (3-4 atomic commits); encode TODO either closed or explicitly deferred + documented
- [ ] D3: TestPatternRedact + TestInspector registered + passing (TestDjotRoundtrip per D2)
- [ ] D4: CHANGELOG ghost reverted; stale admissions removed; new Known Issues added for M4-P6 + M5-P1 (+ encode if deferred)
- [ ] D5: TestBatchMode race fixed; `ctest --repeat-until-fail 3` passes 3/3
- [ ] D6: PP-OCRv5 model mismatch diagnosed + STATUS.md committed
- [ ] D7: scratch cleaned; .context/ tracked + documented
- [ ] D8: 8 missing walkthroughs written (M3-P4/P5 + M4-P1/P2/P3/P4/P5/P7)
- [ ] D9: CLAUDE.md reflects HEAD + test count + .context/ pattern + lesson updates
- [ ] D10: SESSION_BRIEF_NEXT.md drops stale instructions; points per D11
- [ ] D11: NEXT-SESSION-HAND-OFF.md exists; recommends next prompt with rationale
- [ ] D12: Vault notes (01-current-state + 07-sessions-log + 05-prompts-index + 00-overview) updated
- [ ] D13: M3-CATCHUP + M4-CATCHUP prompts deleted
- [ ] D14: (if chose merge) branch merged to main; tag applied
- [ ] D15: full verification suite passes; working tree clean; ctest 3/3

**After this prompt:** the project is at a verified clean checkpoint. The user (or next session) can confidently execute the prompt named in NEXT-SESSION-HAND-OFF.md without risk of redoing committed work or building on stale state.
</success_criteria>
