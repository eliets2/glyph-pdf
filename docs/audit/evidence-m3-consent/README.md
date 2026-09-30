# M-3 evidence — OCSP consent gate on the send-for-signing lane (feat/sig-form-consent)

Base: 02d1a898 (main; branch tip at delivery: see the lane report). Suite:
TestSendForSigning (17 slots after this lane).

Finding: AUDIT-SECURITY-2026-09-25.md §58 — M-3 (MEDIUM, CWE-862):
`SendForSigningController.cpp:168-197` copied `cfg.level`/`cfg.tsaUrl` into
`FillStepInput` and dispatched `SigningRequestRunner::runFillStep` with ZERO
`OcspConsent` references — a user (or org policy
`signing/ocspNetworkPolicy="never"`) who opted out of OCSP egress still emitted
the AIA responder request whenever signing happened through the send-for-signing
workflow, while `NetworkTouchpoints` claimed the touchpoint's state was the
effective policy value. Contrast `SecurityController.cpp:223-238`, where the
main sign path refuses B-LT/B-LTA dispatch without consent.

Fix — both halves of the audit's suggested shape, coherently:
1. GUI-thread consent (mirrors the main path exactly): in
   `SendForSigningController::runSignStep`, after the cert dialog, at
   `cfg.level >= PAdESLevel::B_LT` the controller calls
   `gp::OcspConsent::obtain(_mainWindow, docPath)` (the SAME per-document
   dialog + global never-network switch + remember-for-document store the main
   path uses). A denied decision refuses BEFORE any dispatch with
   `OcspConsent::refusalReason` — no signature attempted, no silent downgrade.
2. Shared fail-closed choke point: the decision travels as data in
   `FillStepInput::ocspEgressConsented` (default FALSE — never asked means
   never granted) and `SigningRequestRunner::precheck` — which re-runs inside
   `runFillStep`, so EVERY dispatch lane shares it — refuses
   `requestedLevel >= B_LT` without recorded consent with a new
   `StepRefusal::OcspConsentDenied` whose message discloses exactly what was
   skipped (the OCSP responder contact), that nothing was signed, and the way
   out (allow the check, or sign B-T/B-B which need no OCSP). The runner stays
   GUI-free (the ui-side consent enum never crosses the layer boundary).

Pins (TestSendForSigning, GUI-free, through the real fixtures):
1. `ocspConsentDeniedRefusesBeforeEngineDispatch` — B_LT input with the
   fail-closed default: `runFillStep` refuses in precheck with
   `attempted=false`, `outcome=NotRun` (the engine — whose signDocumentImpl
   hosts the ONLY OCSP transport call site — is never invoked: the
   transport-never-invoked seam is the engine dispatch seam itself),
   document bytes byte-identical, no signature after, and the refusal names
   OCSP + consent + "Nothing was signed" + the B-T way out.
2. `ocspConsentGrantedDispatchesEngineAndCommits` — the SAME step with
   consent recorded: dispatches, commits, `sig_A` carries the only signature,
   integrity intact (a consent gate, not a level ban — no over-blocking).
3. `ocspConsentNeverNeededAtOfflineLevels` — B_B (the offline level) with no
   consent recorded still runs: the gate is scoped to OCSP-needing levels.
(TestOcspConsent itself is untouched — its machinery is consumed, not
changed; it re-runs green in the final gate.)

- fail-before (the gate scoped out — `false &&` on the choke-point condition,
  the M-3 "no gate" shape): RED — 16 passed / 1 failed; the failed pin is
  exactly `ocspConsentDeniedRefusesBeforeEngineDispatch`
  (`!refused.attempted` FALSE — a consentless B_LT step dispatched the engine).
- NC (same scoped revert, distinct run): 16 passed / 1 failed — the failure
  is isolated to the consent pin; nothing else moved.
- pass-after ×3 serial (gate restored; full suite): see
  m3-pass-after-{1,2,3}.log — 17 passed / 0 failed every run.
