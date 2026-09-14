# GLM resume state — 2026-09-15 (post-integration push)

**PUSHED: `origin/feat/parity-glm` @ `2f755244`** — the 2026-09-14/15 integration wave, final serial
gate 153/153 (one order-flake trio in run 1, green standalone + on rerun; noted, not chased).
Record branches on origin: quick (2b81fc2), resid2 (9129788), packafix (5ae4153), integration
(= mainline), r18f (7d5ae67), sep13-leads (b0fd829), sep13-fixes (e8b9a19), n17n18 (819d84a),
sep13-residual (c7e9ecf). Project physically at D:\pdf\ (junctions keep every old C: path valid).

**What this wave contains (all implemented-awaiting-review unless R14 flips):** quick-findings
Q1–Q4 (OCR skip-before-render; kept-page path was silently dead — fixed; idempotency modal;
QSettings persistence; real preservation contract; XFAIL pins /AcroForm dropped by page-copy —
OPEN engine gap); SEP13 redaction false-success fixes L5/L7/L8 (composed user-data-loss class,
severity reclassified) + L6 wording; SEP13 fixes L1 (B-T downgrade honesty) L2 (HTML injection)
L3 (Degraded reversal) L4 (cert RAII) L9/L10 (batch result honesty) L11 (OCR guards) L13+M7
(columns) + M1/M3/M5; R18(f) Keystroke /AA /K tier + R19 settings end-to-end pin; N17
cert-encryption recipient picker + N18 DocMDP certify selector (NEW parity features, engine
seams were pre-existing); residuals: signing-outcome B-T→B-B disclosure + M8 RedactOperation
lifetime. L12 stays CONFIRMED-unfixed (perf-only, probe pins 15.78× ratio).

**Open queue (execution order):**
1. R14 independent review — RUNNING (pdf-r18 @ feat/parity-glm-review): flips rows to
   verified via own probes + negative controls; deliverable docs/audit/INDEPENDENT-REVIEW.
2. R22 installed-resources Linux gate — RUNNING (pdf-inst @ feat/parity-glm-r22, container
   glyphpdf-linux rebound to pdf-inst). Desktop gates stay UNTESTED-honest.
3. R25 48h soak — RUNNING (pdf-keyC @ feat/soak-48h; detached loop → D:\soak-48h.log;
   verdict due ~2026-09-17; doc docs/audit/SOAK-48H-2026-09-15.md).
4. Follow-ups queued: N17/N18 seam wiring (SecurityController::certifyDocument still hardcodes
   certLevel=1; setExistingSignatureCount call site); suite-isolation finding (new cert tests
   suspected of order-perturbing CommandBinding/EngineSave/EncryptedPackageSafeWrite —
   standalone green, in-suite order-flake); L12 perf fix (probe-pinned); R24 policy/diagnostics;
   R26/27 (batch-presets P1 design settled; pilot external contact = USER auth).
5. END PHASE: 16-role ponytail sweep W1 (native-adversary + fuzz + security-auditor) → W2
   (gsd-verifier + guarantee-verification + testing — only they flip verified) → W3 (perf/arch/
   emergence/archaeologist/devops/ui/ux/research) → fix lanes → consolidated report + UI
   acceptance + perf baseline.

Build/test unchanged: MSYS2 UCRT64 wrapper; -j 2; QtTest -o txt; offscreen; PCH purge after
header edits/branch switches; full ctest only when ninja no-op + graphify idle; known flakes
rerun-once. Docker: plain Git Bash, no apt installs mid-lane. Quota/captcha/model-request
failures: fresh finisher + measured handoff state + git tree; dispatches retry once.

gc/prune FORBIDDEN (recovery ref + 5 unknowns open). Pilot external contact = USER auth.
