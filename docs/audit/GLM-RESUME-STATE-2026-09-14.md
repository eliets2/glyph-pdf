# GLM resume state — 2026-09-14

Integration candidate: `feat/parity-glm` (this tree, pdf-clean) at the merge of
`feat/parity-glm-packafix` (8/8 remediation findings) into origin tip `54d5bcd` + resid2.
Worktrees: pdf-parity = quick-wins (N3 preserved in `2a82738`; 4 quick-review findings open —
skip-before-render, idempotency modal, settings persistence, real page-preservation contract);
pdf-inst = packafix (8/8 merged here); pdf-sec = resid2 (ARC07 gate, merged here); pdf-clean =
integration + this doc; pdf-r15/pdf-r18 = idle; pdf/keyA/keyC/redaction/worktrees = foreign/legacy.

Landed (implemented-awaiting-review unless noted): repair-prompt steps 1–5; G01–G23; form-JS
P1 + R05/JS-01 deadline (9 bypasses); R02/R03/R09; R04/R08/R10; R06/R13; R11/R12; Pack A
(T2-2/3/6/9); R15–R17 UI wave; SEP13 hardening (6 confirmed); Linux R20/R21 compile-level;
cleanup pass; deletion audit (6 safe/1 recovered/5 unknown/0 loss — gc/prune FORBIDDEN until
recovery ref `recovery/temp-stage-push-20260909` and the 5 unknowns resolve); research backlog;
feature-command matrix (300×21).

Open (in execution order):
1. Quick-findings finisher (4 findings on the N3 WIP, `quick-review.md`).
2. packafix-F4 re-verify + r15 encoding recheck at this merged tip (full serial ctest).
3. R14 independent review finisher (dead reviewer's probes in pdf-inst `.context/review-r14/`).
4. R15–R19 residuals: R18(f) Keystroke /AA /K (WIP-handoff), R19 end-to-end settings verify.
5. R20-tail/R22–R23 Linux desktop continuation (container `glyphpdf-linux` up+provisioned;
   desktop gates stay honestly unclaimed from a container).
6. R24 policy/diagnostics; R25 release evidence + 48h soak (start early).
7. R26/R27 expansions + pilot (pilot contact = user authorization).
8. END: 16-role ponytail sweep (W1 native-adversary+fuzz+security → W2 gsd-verifier+
   guarantee+testing → W3 perf+arch+emergence+archaeologist+devops+ui+ux+research) →
   fix lanes → consolidated report + UI acceptance + perf baseline.

Build/test: MSYS2 UCRT64 wrapper; -j 2 always; QtTest -o file.txt,txt; offscreen for GUI;
PCH gch purge after header edits/branch switches; full ctest only when ninja no-op AND
graphify log idle; known flakes rerun-once: TestOllamaProvider/TestBatchMode/TestLaneScheduler/
TestReadOnlyGate/TestBatchOpsCoverage + TestEngineSave×TestRedactTransaction parallel interference.
Quota windows ([1308]/[1310]) kill subagents — coordinator builds/tests/commits directly;
captcha kills dispatches probabilistically — retry once.
