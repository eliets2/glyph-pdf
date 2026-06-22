# GlyphPDF Hardening Audit — Orchestration State (2026-06-22)

**Purpose:** durable handoff so this multi-agent hardening pass can be CONTINUED
autonomously (e.g. by a scheduled task at 12:30 after a session-limit reset) with
zero human intervention. Read this top-to-bottom and continue from "NEXT ACTIONS".

## Context
- Repo: `C:\Users\User\Projects\pdf`, branch `main`, version **v1.3.2.2** (released).
- Suite baseline: **100% pass, 39 ctest targets, single-pass.** Must stay green.
- Env (Windows MSYS2 ucrt64): prefix every shell command with
  `cd /c/Users/User/Projects/pdf && export PATH="/c/msys64/ucrt64/bin:$PATH" && ...`
  Build: `cmake --build build` (pre-configured Release/Ninja — never reconfigure/delete).
  Test: `cd /c/Users/User/Projects/pdf/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4`.
  `TestBatchMode` is a known parallel-repeat flake (RUN_SERIAL) — must pass single-pass.
- Commit on `main`, atomic, trailer `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
  Push main + merge→push `audit-remediation`. Hide-don't-delete for unfinished UI surfaces.

## WAVE 1 — read-only audits (dispatched 2026-06-22, parallel, no edits)
Each returns a prioritized findings list with file:line evidence. Agent IDs are
THIS-session-only; their transcripts are at
`C:\Users\User\AppData\Local\Temp\claude\D--\3478146a-7bb8-46fb-b517-184ae164a1ba\tasks\<id>.output`
(may not survive a new session — if a dimension's findings are unavailable, RE-DISPATCH a
fresh read-only audit agent for it):
- native-adversary (`ad3dfc346a4e43299`) — adversarial security (parser/redaction/PKI/subprocess).
- cso / security-auditor (`a5a31c09cddaecfd4`) — security hygiene (secrets, input-validation, supply-chain, CI, LLM/SSRF).
- code-archaeologist (`af701b9f166db9ed7`) — DEAD CODE (zero-ref proofs) + spaghetti/tech-debt + arch health.
- performance-optimizer (`af37884d9a2e96e68`) — speed (UI-thread blocking, render/cache hotspots, hot loops, startup).
- frontend-specialist UI (`a1b3d7f33cdce9ffc`) — fresh wiring/dead-control audit.
- frontend-specialist UX (`a4668ffdeed9e34a8`) — fresh label-truth/flows/a11y audit.

As each completes, persist its findings into `docs/audit/HARDENING-2026-06-22-FINDINGS.md`
(create it) so they survive across sessions.

## NEXT ACTIONS (continue here)
1. Collect all six Wave-1 findings (from the transcripts if present, else re-dispatch that audit).
   Write/append them to `docs/audit/HARDENING-2026-06-22-FINDINGS.md`.
2. Synthesize a de-duplicated MASTER findings list, prioritized 🔴/🟠/🟡 across all dimensions.
3. WAVE 2 (serial — one writer at a time on the shared checkout/build):
   a. Run `emergence-engine` (read-only) over the consolidated findings + arch for emergent system risk.
   b. Apply high-value fixes in order: provably-dead-code removal → 🔴 security hardening →
      perf wins → remaining UI/UX wiring/label fixes. Each: smallest correct diff, `cmake --build build`,
      `ctest -j4` 100%/39, atomic commit. NEVER weaken tests; hide-don't-delete for unfinished UI.
   c. `fuzz-harness-engineer` — stand up the fuzzing rig in the isolated `fuzz/` subtree (writes only there).
   d. `devops-engineer` — CI/build hardening from the findings.
4. After fixes land green: consider a v1.3.2.3 build (same unsigned flow: bump CMake/wxs/build-msi.ps1/winget
   version + NEW ProductCode, build MSI -SkipSigning, SHA, GitHub release, push). See
   `C:\Users\User\.claude\projects\D--\memory\glyphpdf-packaging.md` for the exact release recipe + caveats
   (unsigned; latest.json stays 1.3.1; AllowSameVersionUpgrades handles in-place upgrade).
5. `cost-optimizer` is SEPARATE scope (audits the Claude-session agent/MCP setup, not the app) — optional.

## Guardrails
- Read-only audits may run in parallel; WRITE/build agents must run SEQUENTIALLY (one shared build/ dir).
- Verify every fix with build + ctest before committing. Independent-verify agent claims (don't trust reports).
- A blocked item honestly reported beats a faked green.
