# GLM handoff prompt — continue GlyphPDF implementation (2026-09-14)

You are the implementation coordinator continuing GlyphPDF's parity/quality program. Everything below is verified state as of 2026-09-14 02:00 MEDT. Re-verify refs before acting; nothing needs re-implementing. Launch agents in bounded waves (max 3 workers + coordinator; concurrency cap is ~2 during quota-storm windows — retry a bounced dispatch after a delay, it is probabilistic).

## 1. Standing mission, authorizations, and hard rules

- Mission: complete GlyphPDF (C++17/Qt6, MSYS2 UCRT64, PoDoFo 1.1.0 + PDFium + quickjs-ng) against the whole-project review program (`outputs\GLM-RESUME-WHOLE-PROJECT-PROMPT-2026-09-13.md`, `WHOLE-PROJECT-IMPLEMENTATION-BACKLOG-2026-09-10.csv` WP-R01–R27), the SEP13 findings (`C:\Users\User\Projects\pdf\docs\audit\PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` — READ-ONLY, in the main checkout), the UI/performance phase (`GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md`, `UI-BUTTON-REVIEW-2026-09-09.md`), the cleanup safeguards (`GLM-DEAD-FILE-AND-TEST-CLEANUP-PROMPT-2026-09-09.md`), and the fix/preservation handoff (`GLM-FIX-FINDINGS-AND-SAFE-BRANCH-CLEANUP-2026-09-13.md`).
- Standing authorizations: quickjs-ng via MSYS2 pacman (Option A, recorded — does NOT extend to other deps); push to GitHub after each verified wave; Windows installers only after gates; pilot/external contact requires explicit user authorization.
- HARD RULES: never reset/clean/force/gc/prune (recovery ref `recovery/temp-stage-push-20260909` + 5 unknowns exist — gc stays forbidden); never touch `C:\Users\User\Projects\pdf` (main checkout + user's installed app at `pdf\deploy\`, backup at `pdf\deploy.bak-2026-09-09\`); never push without the coordinator's independent gate; `verified` is reserved for independent review; handoff file `.context\<lane>-wip.md` from the first action of every lane; one owner per shared file (GpMainWindow, RibbonModel/ToolRegistry/ToolId, DocumentSession, FormManager, CMake, SecurityController).
- Build/test protocol: MSYS2 UCRT64 wrapper `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -lc '...'`; ALWAYS `-j 2`; QtTest `-o file.txt,txt` (piped stdout is empty); GUI tests `QT_QPA_PLATFORM=offscreen` (never pop modals on the user's desktop); PCH hazard → delete `build-*/CMakeFiles/{pdfws_engines,pdfws_commands,pdfws_ui}.dir/cmake_pch.hxx.gch` after PCH-header edits or branch switches; full `ctest` only when ninja is a no-op AND `tail C:/Users/User/.cache/graphify-rebuild.log` is idle; known flakes (rerun once, note, don't chase): TestOllamaProvider/TestBatchMode/TestLaneScheduler/TestReadOnlyGate/TestBatchOpsCoverage + TestEngineSave×TestRedactTransaction parallel-fault interference.
- Subagents fail on: quota windows ([1308]/[1310], resets ~5h), concurrency cap (~2–3), captcha during apt/pip — all absorbed by: fresh finisher + measured state from `.context\<lane>-wip.md` + git tree; coordinator builds/tests/commits directly during quota windows (Bash has no LLM quota). Captcha-killed dispatches: retry once, works.

## 2. Current state (verified 2026-09-14 02:00)

Origin `feat/parity-glm` @ `54d5bcd` — everything below is LANDED, gated (125–139/139 across waves), and pushed. DO NOT REDO:
- 2026-09-08 repair prompt steps 1–5 (EC01/INF01, ARC05/01/02+EC02+GUI-handle, ARC03/04+EC03/05+V01/V02/V06, ARC06/07+NCR-01/02+EC04+EC06+D06+N08+V03/V04/V05, INF02–INF06+Q02); quality gate G01–G23 (incl. G05 integration fix); form-JS Phase 1 + R05/JS-01 deadline (9 bypasses); R02/R03/R09 (gateE); R04/R08/R10; R06/R13; R11/R12; Linux R20/R21 compile-level; Pack A (T2-2/3/6/9 under the WP-R07 real-replacement bar); R15–R17 UI wave (command identity, 12 welcome routes, keyboard/RTL/CJK/DPI); SEP13 hardening (CID /W, #else break, XLSX dup refs, sign fail-open, AEAD AAD, Ollama cap, PPTX refuted); cleanup pass; deletion audit (proven-safe 6 / recovered 1 / unknown 5 / confirmed-loss 0; recovery ref created); research backlog (82 ranked rows) + feature-command matrix (300×21).

Worktrees (all clean, all committed):
- `pdf-parity` = feat/parity-glm-quick @ `bc2743f` — quick-wins: N1/N2 committed; N3 WIP preserved in `2a82738`; **4 quick-review findings OPEN** (see §4-Q).
- `pdf-inst` = feat/parity-glm-packafix @ `5ae4153` — 8/8 remediation findings DONE (F1–F4 + r15-F1/F2/F3), ledger rows committed. **UNMERGED into mainline.**
- `pdf-sec` = feat/parity-glm-resid2 @ `9129788` — ARC07 OCR-accept read-only gate DONE (ocrAcceptWriteBlocker seam + 5 pins, TestOcrReviewLifecycle 30/30). **UNMERGED into mainline.**
- `pdf-clean` = feat/parity-glm-r11r12 @ `54d5bcd` (= origin tip; integration branch `feat/parity-glm-integration` also @ 54d5bcd and already pushed). Build-clean is warm at this tip (125/125 flake-green).
- `pdf-r15` holds the local `feat/parity-glm` ref @ `586d6e4` (BEHIND origin — release it: `git checkout --detach` there, then re-point the branch).
- `pdf-r18` = feat/parity-glm-r18r19 @ `abc87de` — **MERGED into origin tip** (branch may be deleted after verification).
- Docker container `glyphpdf-linux` up; kali-rolling fully provisioned (Qt 6.10.2, cmake 4.3.4, ninja, g++ 15.2; podofo 1.1.0 built in `third_party/podofo/install-linux` on the `feat/parity-glm-linux` branch @ `7ba5ca1`, merged).

## 3. INTEGRATE FIRST (before new work)

In `pdf-clean`: release the ref (`git -C C:\Users\User\Projects\pdf-r15 checkout --detach origin/feat/parity-glm`), then `git checkout feat/parity-glm && git merge --ff-only origin/feat/parity-glm`, then `git merge feat/parity-glm-resid2` and `git merge feat/parity-glm-packafix` (expect ledger/CMake conflicts → union of sections/blocks; GpMainWindow was already union-merged once — re-check). Purge PCH gch files, full serial `ctest` at the merged tip (expect ~140+ targets green; known flakes may appear — rerun once each, note), then `git push origin feat/parity-glm`. Also push `feat/parity-glm-resid2` and `feat/parity-glm-packafix` as record branches.

## 4. Remaining work, in order (with the exact open findings)

**Q-lane — quick-findings finisher** (pdf-parity @ feat/parity-glm-quick, building on `2a82738` N3 WIP; specs in `work\branch-preservation-2026-09-13\quick-review.md`):
1. Skip decision must run BEFORE render/OCR (currently renders+OCRs every page then discards — BatchMode.cpp ~1257–1275); page-indexed results; a mixed 2-page fixture asserts exactly ONE OCR call.
2. Idempotency test modal hang (TestBatchOcrSkipText.cpp:303 + addFilePaths append semantics): fresh harness containing only the first output, or explicit list reset before run two; test must finish unattended.
3. The three advertised skip-options are never persisted (no QSettings writer): wire persistence OR correct the matrix claim; isolate test settings.
4. Page-preservation contract: state the REAL guarantee (extracted-text equality ≠ byte identity) and add artifact checks (annotations/form/resources/geometry) with real image-containing fixtures.

**Remediation lane — 3 findings remain** (pdf-inst @ feat/parity-glm-packafix; handoff `.context\packafix-wip.md` has coordinator state; specs in `work\branch-preservation-2026-09-13\packa-review.md` + `r15-review.md`):
1. packa-F2 remainder is DONE; packa-F4 remainder is DONE — what REMAINS: (a) `packa-review` F3 double-write was fixed in bee2c02 — re-verify at merge; (b) r15 encoding recheck (likely closed by CJK fromUtf8 fix — confirm and ledger-note); (c) re-run the full a11y suite at both scales after merge.
(If the merge in §3 surfaces new conflicts in TextMatchFinder/FindReplaceDialog, resolve as union and re-run TestFindReplace 29/29 + TestUiAccessibility 11/11 & 12/12.)

**R14 — independent review finisher** (pdf-inst @ feat/parity-glm-review; dead reviewer's probes survive in `.context\review-r14\` — r14-r02r09-probe.cpp, r14-run-f
fsmith-probe.py, r14-run-formjs-probe.py, etc. — inspect, adapt, continue). Verify EVERY package from the merged tip: G01–G23, R02–R13, form-JS P1+R05, Linux R20/R21-compile, Pack A, R15–R19 (when landed). Protocol: implementer claim → falsifiable contract → its suite → YOUR independent probe (adapt preserved probes from `work\whole-project-2026-09-10` + `work\gate-2026-09-09`) → negative control (scratch disable of the fix → test must fail) → independent read path for persistence → verdict verified/partial/rejected with exact SHA+commands+skips. Deliver `docs/audit/INDEPENDENT-REVIEW-2026-09-14.md` + ledger flips.

**R15–R19 continuation** (after quick lane and remediation merge): R18(f) Keystroke /AA /K (WIP-handoff from r18r19 — Qt line-edit layer, AFMergeChange; larger, budget honestly); R19 verify the Preferences→SecurityController settings surface end-to-end at the merged tip.

**R20-tail/R22–R23 — native Linux desktop continuation** (Docker `glyphpdf-linux` is up and provisioned — apt is captcha-risky, everything needed is installed): desktop gates remain UNTESTED (Wayland/X11, printing, portals, IME, keyring runtime, clean-machine install) — a container run cannot claim them; record honestly. R22: installed-resources + source-tree-hidden runtime check inside the container is doable (install-tree staging + `GLYPHPDF_SOURCE_RESOURCE_DIR` independent test).

**R24 — managed policy + privacy-safe diagnostics**: machine policy overrides user prefs (visible in UI), redacted support bundle (no PDF text/secrets), network behavior explicit. R25 — release evidence: exact-candidate Release full suite + optional configs, SBOM/runtime hashes, clean-machine checks, 48h soak (wall-clock — start it early in a lane).

**R26/R27 — expansions + pilot**: batch presets (P0 done via G12; design `docs/research/batch-presets-implementation-plan.md`), printable review summaries (Pack A delivered the writer — extend), local signing requests (design `send-for-signing-implementation-plan.md`), accessibility authoring (T2-4). Pilot external contact = USER authorization required; do not invent results.

**SEP13 leads → ponytail W1**: timestamp downgrade to B-B reported as Success (SignatureManager ~1352); HTML font-name injection (ConversionManager ~358); applyToWidget Degraded branch never reverses (Capability ~191); cert leaks on error paths (~1273/~1522); redaction-proof rotate/MediaBox origin false-PASS (~693) + extraction-recall (~905) + annotation/form-text attribution (~819) + overlay Y origin (RedactOperation ~222); BatchMode phantom result (~1439) + cancelled-merge successes (~1408); OCRMode ReOcr/Reject guards (~707/~617); CompareMode filter perf (~392); deriveColumns misassignment (~238); + the medium/low list in the findings doc.

**END PHASE (user-mandated) — 16-role ponytail sweep** after all lanes merge, in 3 waves (roles from `C:\Users\User\.claude\agents\<role>\<role>.md`): W1 native-adversary + fuzz-harness-engineer + security-auditor (adversarial reproduction incl. SEP13 leads confirmation); W2 gsd-verifier + guarantee-verification-engine + testing-specialist (the ONLY reviewers who may flip rows to verified); W3 performance-optimizer + solution-architect + emergence-engine + code-archaeologist + devops-engineer + ui-specialist + ux-specialist + research-specialist. Fix lanes for sweep findings; then consolidated report + UI acceptance evidence + performance baseline. backend-specialist and orchestrator run throughout.

## 5. Known decisions still open with the user

- WP-R26/27 pilot external contact (needs explicit authorization).
- Linux desktop acceptance environment beyond the Docker container (physical/VM Linux desktop for Wayland/X11/printing/keyring gates).
- Nothing else — quickjs-ng (Option A) is authorized and landed; P2/P3 and worker-process isolation are recorded recommendations, not blockers.

## 6. Evidence rules (unchanged, binding)

Separate implementation / independent-review / release states. Current-candidate tests only (full SHA + dirty scope + enabled capabilities + skips). Pre-fix failure and post-fix pass on the same fixture. Real saved artifacts via independent read paths (podofo/PDFium/qpdf). Negative controls must fail. Never weaken tests, never hide failures as skips, never claim unrun gates. One commit per coherent finding with its matrix row; ledger rows `implemented-awaiting-review`; `verified` only via the independent protocol. Update `docs/audit/GLM-RESUME-STATE-2026-09-14.md` after each bounded package.

## 7. First three commands for the new session

1. `git fetch origin` in every worktree; verify origin/feat/parity-glm = 54d5bcd or newer and check each worktree tip against §2.
2. Read `docs/audit/GLM-RESUME-STATE-2026-09-14.md` (committed at the integration tip) + `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` newest sections.
3. Execute §3 INTEGRATE FIRST, then dispatch the Q-lane + Remediation + R14 finishers in parallel (2–3 workers), and continue down §4.

## 8. REQUIRED-READING FILE INDEX (absolute paths — read before acting)

Review workspace root: `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\outputs\`
- `GLM-HANDOFF-CONTINUE-2026-09-14.md` (this handoff)
- `GLM-RESUME-WHOLE-PROJECT-PROMPT-2026-09-13.md` — the program definition (waves, roles, rules)
- `WHOLE-PROJECT-READINESS-REVIEW-2026-09-10.md` — readiness verdict, corporate gates, UX priorities, perf targets (PROPOSED, not measured)
- `WHOLE-ARCHITECTURE-REVIEW-2026-09-10.md` — A01–A07 (all closed by gateE/R09/R10 — verify at merge)
- `PERFORMANCE-AND-SCRIPT-REVIEW-2026-09-10.md` — PERF-01–05 (R06/R11/R12/R13 closed) + JS-01 (R05 closed) + form-JS release conditions (R18 remainder)
- `WHOLE-PRODUCT-AND-PLAN-REVIEW-2026-09-10.md` — PP01–PP07 (R07/R08/R15–R19 scope)
- `NATIVE-LINUX-READINESS-2026-09-10.md` — L01–L13: REQUIRED for the R20-tail/R22–R23 Linux desktop continuation
- `WHOLE-PROJECT-IMPLEMENTATION-BACKLOG-2026-09-10.csv` — WP-R01–R27 acceptance gates
- `WHOLE-PROJECT-REVIEW-VERIFICATION-2026-09-10.json` — what the reviewers checked vs didn't
- `QUALITY-GATE-2026-09-09.md` + `QUALITY-GATE-VERIFICATION-2026-09-09.json` + `quality-gate-evidence-2026-09-09.zip` — G01–G23 baseline (closed; probes are the comparison standard)
- `GLM-FLASH-REPAIR-PROMPT-2026-09-08.md`, `GLM-REPAIR-PROMPT-2026-09-09.md` — historical repair contracts (steps 1–5 closed)
- `GLM-MULTI-AGENT-UI-AND-QUALITY-PROMPT-2026-09-09.md` + `UI-BUTTON-REVIEW-2026-09-09.md` — UI phase detail (R15–R17 landed; use for R15-residual/ponytail-ui)
- `GLM-DEAD-FILE-AND-TEST-CLEANUP-PROMPT-2026-09-09.md` — cleanup safeguards (cleanup pass landed)
- `GLM-FIX-FINDINGS-AND-SAFE-BRANCH-CLEANUP-2026-09-13.md` — §4 deletion-audit duties (FULFILLED — ledger in docs/audit/) + §5 lane findings (Pack A 4, quick 4, R15 4, R18 3 — remediation/residual state per §4 of this handoff)

Preserved evidence/probe roots (COPY probes to scratch; never modify):
- `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\whole-project-2026-09-10\` — compare/perf/formjs probes + baseline/verification JSONs
- `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\gate-2026-09-09\` — G-gate probes + verify.py
- `C:\Users\User\Documents\Codex\2026-09-05\read-c-users-user-projects-pdf\work\branch-preservation-2026-09-13\` — packa/quick/r15/r18/legacy reviews + candidates JSONs + deletion-audit inputs (inventories, bundles: closure bundle SHA-256 `1c63ed3e…cc596` verified) + `work\ui-prompt-2026-09-09\` (UI probe + command-inventory)

Main checkout (READ-ONLY — never modify): `C:\Users\User\Projects\pdf\docs\audit\PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` — SEP13 findings (6 confirmed FIXED at tip; LEADS list = ponytail W1 input)

In-repo required reading (feat/parity-glm @ current tip):
- `docs/audit/GLM-RESUME-STATE-2026-09-14.md` — the live resume-state doc (KEEP UPDATED after each package)
- `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md` — every finding's status; newest sections first
- `docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv` + `-NOTES-.md` — 300×21 command ground truth (append rows with every feature)
- `docs/audit/DELETED-BRANCH-RECOVERY-LEDGER-2026-09-13.csv` + `-SUMMARY-.md` — deletion audit (gc/prune stays forbidden)
- `docs/research/RESEARCH-BACKLOG-2026-09-10.md` — 82 ranked research items (N1–N60 + Tier-2/3 tails; quick-wins items N1–N35 in flight; the rest = R26/27 expansion pool)
- `docs/research/batch-presets-implementation-plan.md` — batch-presets P1 (design settled; P0 done via G12)
- `docs/research/send-for-signing-implementation-plan.md` — PAdES phases (P1 scope = R26)
- `docs/research/form-js-implementation-plan.md` — form-JS P2/P3 scope (P1+R05 landed)
- `docs/research/synthesis.md` — competitive synthesis + moat framing
- `.context/<lane>-wip.md` — ACTIVE lane handoffs (quick-wins-wip.md = N3→N60 state; packafix-wip.md = coordinator state; review-r14/ = dead reviewer's probes to reuse)
- `.context/evidence-2026-09-08/` — all lane evidence logs

Agent fleet root (read `<role>\<role>.md` before assigning): `C:\Users\User\.claude\agents\` — orchestrator, backend-specialist, gsd-debugger, gsd-executor, gsd-planner, gsd-roadmapper, code-archaeologist, solution-architect, security-auditor, native-adversary, fuzz-harness-engineer, performance-optimizer, testing-specialist, guarantee-verification-engine, devops-engineer, ui-specialist, ux-specialist, emergence-engine, research-specialist.
