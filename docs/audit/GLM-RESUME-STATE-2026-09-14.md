# GLM resume state — 2026-09-15 (post-integration push)

**PUSHED: `origin/feat/parity-glm` @ `6f156d6`** — integration wave + same-day follow-through:
R22 Linux gate (install-tree independence PROVEN + 4 build-gate defects fixed), R14 independent
review (21 rows verified; its one PARTIAL F1 — rotated-annotation attribution — FIXED @ 7189149),
follow-ups (N17/N18 seam wiring; cert-suite isolation; L12 perf FIXED, probe now a regression
guard), candidate-leak fix (signing success-path candidate removal + a defeated-mid-try-cleanup
device-lifetime defect), batch-presets P1 (R26 flagship: versioned JSON presets, capability-honest
transactional pipeline), R24 (managed policy with visible overrides, redaction-by-construction
support bundle, explicit network-touchpoint page). Record branches on origin: quick, resid2,
packafix, integration, r18f, sep13-leads, sep13-fixes, n17n18, sep13-residual, r22, review,
candidate-leak-fix, followups-2026-09-15, l7-rotate-annot, soak-48h, batch-presets-p1, r24-policy.
Gates: 153-155 targets, green (known order-flakes rerun-once: TestReadOnlyGate/TestBatchMode
standalone-green after in-suite red). Project at D:\pdf\ (junctions keep old C: paths valid).

**Open queue:**
1. RUNNING: send-for-signing P1 (pdf-keyA @ feat/send-for-signing-p1 — the last Tier-1 moat
   gap: multi-signer workflow, versioned sidecar requests, no network per plan scope cuts);
   printable summaries (pdf-sec @ feat/printable-summaries — small R27 tail).
2. R25 48h soak — RUNNING detached (D:\soak-48h.log, started 2026-09-15T01:44+03, verdict
   ~2026-09-17 01:44; doc SOAK-48H-2026-09-15.md; watch item: one-off failing test differs
   per pass — a RECURRING single test would be a candidate finding).
3. END PHASE: 16-role ponytail sweep W1 (native-adversary + fuzz + security-auditor) → W2
   (gsd-verifier + guarantee-verification + testing) → W3 (perf/arch/emergence/archaeologist/
   devops/ui/ux/research) → fix lanes → consolidated report + UI acceptance + perf baseline.
4. Known residuals: R24's 4 policy keys enforcement-pending (wiring points documented);
   OCSP consent-switch gap disclosed; SuiteIsolation: ctest -j shared %TEMP%/glyphpdf-candidates
   collision class (RUN_SERIAL/RESOURCE grouping candidates); accessibility authoring T2-4
   (last big Tier-2 feature, not started); Linux 33-fail triage list (dominant pdfium-stub
   class) in feat/parity-glm-r22's R22 doc; pilot external contact = USER auth.

Build/test unchanged: MSYS2 UCRT64 wrapper; -j 2; QtTest -o txt; offscreen; PCH purge after
header edits/branch switches; full ctest only when ninja no-op + graphify idle; known flakes
rerun-once. Docker: plain Git Bash, no apt installs mid-lane. Quota/captcha/model-request
failures: fresh finisher + measured handoff state + git tree; dispatches retry once.

gc/prune FORBIDDEN (recovery ref + 5 unknowns open). Pilot external contact = USER auth.
